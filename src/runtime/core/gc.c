#include "runtime/core/gc.h"
#include "runtime/core/object.h"
#include "runtime/core/vm.h"
#include "utils/allocators.h"
#include "utils/logger.h"
#include "utils/platform_compat.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <assert.h>

// Default GC configuration
static const GCConfig default_config = {
    .heap_grow_factor = 2,
    .min_heap_size = 1024 * 1024,     // 1MB
    .max_heap_size = 0,               // Unlimited
    .gc_threshold = 1024 * 1024,      // 1MB
    .incremental = false,
    .incremental_step_size = 1024,    // 1KB worth of work
    .stress_test = false,
    .verbose = false
};

// Gray stack operations
static void gray_stack_init(GrayStack* stack) {
    stack->capacity = 128;
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_VM);
    stack->items = MEM_ALLOC(alloc, stack->capacity * sizeof(GCObjectHeader*));
    stack->count = 0;
}

static void gray_stack_free(GrayStack* stack) {
    if (stack->items) {
        Allocator* alloc = allocators_get(ALLOC_SYSTEM_VM);
        SLANG_MEM_FREE(alloc, stack->items, stack->capacity * sizeof(GCObjectHeader*));
        stack->items = NULL;
    }
    stack->count = 0;
    stack->capacity = 0;
}

static void gray_stack_push(GrayStack* stack, GCObjectHeader* obj) {
    if (stack->count >= stack->capacity) {
        size_t old_capacity = stack->capacity;
        stack->capacity *= 2;
        Allocator* alloc = allocators_get(ALLOC_SYSTEM_VM);
        stack->items = MEM_REALLOC(alloc, stack->items, 
                                  old_capacity * sizeof(GCObjectHeader*),
                                  stack->capacity * sizeof(GCObjectHeader*));
    }
    stack->items[stack->count++] = obj;
}

static GCObjectHeader* gray_stack_pop(GrayStack* stack) {
    if (stack->count == 0) return NULL;
    return stack->items[--stack->count];
}

// Create garbage collector
GarbageCollector* gc_create(VM* vm, const GCConfig* config) {
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_VM);
    GarbageCollector* gc = MEM_NEW(alloc, GarbageCollector);
    if (!gc) return NULL;
    
    // Initialize with config
    gc->config = config ? *config : default_config;
    gc->vm = vm;
    
    // Initialize object tracking
    gc->all_objects = NULL;
    gc->object_count = 0;
    gray_stack_init(&gc->gray_stack);
    
    // Initialize memory tracking
    gc->bytes_allocated = 0;
    gc->bytes_allocated_since_gc = 0;
    gc->next_gc_threshold = gc->config.gc_threshold;
    
    // Initialize GC state
    gc->phase = GC_PHASE_NONE;
    gc->is_collecting = false;
    gc->sweep_cursor = NULL;
    
    // Initialize statistics
    memset(&gc->stats, 0, sizeof(GCStats));
    
    if (gc->config.verbose) {
        LOG_INFO(LOG_MODULE_GC, "Created garbage collector with threshold %zu bytes", 
                 gc->config.gc_threshold);
    }
    
    return gc;
}

// Destroy garbage collector
void gc_destroy(GarbageCollector* gc) {
    if (!gc) return;
    
    // Collect everything one last time
    gc_collect(gc);
    
    // Free remaining objects (should be none after collection)
    GCObjectHeader* obj = gc->all_objects;
    while (obj) {
        GCObjectHeader* next = obj->next;
        Allocator* alloc = allocators_get(ALLOC_SYSTEM_OBJECTS);
        
        // Destroy the object
        if (obj->object) {
            object_destroy((Object*)obj->object);
        }
        
        // Free the header
        SLANG_MEM_FREE(alloc, obj, sizeof(GCObjectHeader));
        obj = next;
    }
    
    // Free gray stack
    gray_stack_free(&gc->gray_stack);
    
    // Free GC itself
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_VM);
    SLANG_MEM_FREE(alloc, gc, sizeof(GarbageCollector));
}

// Allocate memory with GC tracking
void* gc_alloc(GarbageCollector* gc, size_t size, const char* tag) {
    // Check if we need to collect
    if (gc->config.stress_test || gc_should_collect(gc)) {
        gc_collect(gc);
    }
    
    // Allocate object
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_OBJECTS);
    void* object = MEM_ALLOC_TAGGED(alloc, size, tag);
    if (!object) {
        // Try collecting and retry
        gc_collect(gc);
        object = MEM_ALLOC_TAGGED(alloc, size, tag);
        if (!object) return NULL;
    }
    
    // Track the object
    gc_track_object(gc, object, size);
    
    return object;
}

// Track an object in the GC
void gc_track_object(GarbageCollector* gc, void* object, size_t size) {
    if (!object) return;
    
    // Create header
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_OBJECTS);
    GCObjectHeader* header = MEM_NEW(alloc, GCObjectHeader);
    if (!header) return;
    
    // Initialize header
    header->object = object;
    header->size = size;
    header->color = GC_WHITE;
    header->is_pinned = false;
    header->prev = NULL;
    header->next = gc->all_objects;
    
    // Add to list
    if (gc->all_objects) {
        gc->all_objects->prev = header;
    }
    gc->all_objects = header;
    gc->object_count++;
    
    // Update memory tracking
    gc->bytes_allocated += size;
    gc->bytes_allocated_since_gc += size;
    
    // Update statistics
    gc->stats.total_allocated += size;
    gc->stats.current_allocated += size;
    if (gc->stats.current_allocated > gc->stats.peak_allocated) {
        gc->stats.peak_allocated = gc->stats.current_allocated;
    }
    
    if (gc->config.verbose) {
        LOG_DEBUG(LOG_MODULE_GC, "Tracked object %p (size %zu, total %zu bytes)", 
                  object, size, gc->bytes_allocated);
    }
}

// Untrack an object (manual free)
void gc_untrack_object(GarbageCollector* gc, void* object) {
    if (!object) return;
    
    // Find the header
    GCObjectHeader* header = gc->all_objects;
    while (header) {
        if (header->object == object) {
            // Remove from list
            if (header->prev) {
                header->prev->next = header->next;
            } else {
                gc->all_objects = header->next;
            }
            if (header->next) {
                header->next->prev = header->prev;
            }
            
            // Update tracking
            gc->bytes_allocated -= header->size;
            gc->stats.current_allocated -= header->size;
            gc->object_count--;
            
            // Free header
            Allocator* alloc = allocators_get(ALLOC_SYSTEM_OBJECTS);
            SLANG_MEM_FREE(alloc, header, sizeof(GCObjectHeader));
            
            return;
        }
        header = header->next;
    }
}

// Find object header
static GCObjectHeader* find_header(GarbageCollector* gc, void* object) {
    GCObjectHeader* header = gc->all_objects;
    while (header) {
        if (header->object == object) {
            return header;
        }
        header = header->next;
    }
    return NULL;
}

// Mark a single object
static void mark_object(GarbageCollector* gc, void* ptr) {
    if (!ptr) return;
    
    GCObjectHeader* header = find_header(gc, ptr);
    if (!header) return;
    
    // Already marked?
    if (header->color != GC_WHITE) return;
    
    // Mark as gray
    header->color = GC_GRAY;
    gray_stack_push(&gc->gray_stack, header);
    
    if (gc->config.verbose) {
        LOG_DEBUG(LOG_MODULE_GC, "Marked object %p as gray", ptr);
    }
}

// Mark a value
static void mark_value(GarbageCollector* gc, TaggedValue value) {
    switch (value.type) {
        case VAL_OBJECT:
            mark_object(gc, AS_OBJECT(value));
            break;
        case VAL_STRING:
            // Strings are in string pool, not GC managed
            break;
        case VAL_CLOSURE:
            mark_object(gc, AS_CLOSURE(value));
            break;
        case VAL_FUNCTION:
            mark_object(gc, AS_FUNCTION(value));
            break;
        default:
            // Primitive values don't need marking
            break;
    }
}

// Process gray objects
static void process_gray_objects(GarbageCollector* gc) {
    while (gc->gray_stack.count > 0) {
        GCObjectHeader* header = gray_stack_pop(&gc->gray_stack);
        if (!header) continue;
        
        // Mark as black
        header->color = GC_BLACK;
        
        // Mark children based on object type
        Object* obj = (Object*)header->object;
        if (!obj) continue;
        
        // Mark object properties
        ObjectProperty* prop = obj->properties;
        while (prop) {
            if (prop->value) {
                mark_value(gc, *prop->value);
            }
            prop = prop->next;
        }
        
        // Mark prototype
        if (obj->prototype) {
            mark_object(gc, obj->prototype);
        }
        
        // Arrays are handled through their properties (indices as keys)
        
        if (gc->config.verbose) {
            LOG_DEBUG(LOG_MODULE_GC, "Marked object %p as black", obj);
        }
    }
}

// Mark roots
static void mark_roots(GarbageCollector* gc) {
    if (!gc->vm) return;
    
    // Mark VM stack
    for (TaggedValue* slot = gc->vm->stack; slot < gc->vm->stack_top; slot++) {
        mark_value(gc, *slot);
    }
    
    // Mark globals
    for (size_t i = 0; i < gc->vm->globals.count; i++) {
        mark_value(gc, gc->vm->globals.values[i]);
    }
    
    // Mark call frames
    for (int i = 0; i < gc->vm->frame_count; i++) {
        CallFrame* frame = &gc->vm->frames[i];
        if (frame->closure) {
            mark_object(gc, frame->closure);
        }
    }
    
    // Mark open upvalues
    for (Upvalue* upvalue = gc->vm->open_upvalues; upvalue != NULL; upvalue = upvalue->next) {
        mark_object(gc, upvalue);
    }
    
    if (gc->config.verbose) {
        LOG_DEBUG(LOG_MODULE_GC, "Marked %zu roots", gc->gray_stack.count);
    }
}

// Sweep unmarked objects
static size_t sweep(GarbageCollector* gc) {
    GCObjectHeader* header = gc->all_objects;
    GCObjectHeader* previous = NULL;
    size_t freed_count = 0;
    size_t freed_bytes = 0;
    
    while (header) {
        if (header->color == GC_WHITE && !header->is_pinned) {
            // This object is garbage
            GCObjectHeader* garbage = header;
            
            // Unlink from list
            if (previous) {
                previous->next = header->next;
            } else {
                gc->all_objects = header->next;
            }
            if (header->next) {
                header->next->prev = previous;
            }
            
            // Move to next before freeing
            header = header->next;
            
            // Update stats
            freed_count++;
            freed_bytes += garbage->size;
            gc->bytes_allocated -= garbage->size;
            gc->stats.current_allocated -= garbage->size;
            gc->stats.total_freed += garbage->size;
            gc->object_count--;
            
            if (gc->config.verbose) {
                LOG_DEBUG(LOG_MODULE_GC, "Sweeping object %p (size %zu)", 
                         garbage->object, garbage->size);
            }
            
            // Destroy the object
            if (garbage->object) {
                object_destroy((Object*)garbage->object);
            }
            
            // Free the header
            Allocator* alloc = allocators_get(ALLOC_SYSTEM_OBJECTS);
            SLANG_MEM_FREE(alloc, garbage, sizeof(GCObjectHeader));
        } else {
            // Reset color for next GC
            header->color = GC_WHITE;
            previous = header;
            header = header->next;
        }
    }
    
    gc->stats.objects_freed += freed_count;
    
    if (gc->config.verbose) {
        LOG_INFO(LOG_MODULE_GC, "Swept %zu objects (%zu bytes)", freed_count, freed_bytes);
    }
    
    return freed_bytes;
}

// Perform garbage collection
void gc_collect(GarbageCollector* gc) {
    if (gc->is_collecting) return;
    
    clock_t start_time = clock();
    gc->is_collecting = true;
    gc->stats.collections++;
    
    if (gc->config.verbose) {
        LOG_INFO(LOG_MODULE_GC, "Starting collection #%zu (allocated: %zu bytes)", 
                 gc->stats.collections, gc->bytes_allocated);
    }
    
    // Mark phase
    mark_roots(gc);
    process_gray_objects(gc);
    
    // Sweep phase
    size_t freed = sweep(gc);
    
    // Update threshold
    gc->bytes_allocated_since_gc = 0;
    gc->next_gc_threshold = gc->bytes_allocated * gc->config.heap_grow_factor;
    if (gc->next_gc_threshold < gc->config.min_heap_size) {
        gc->next_gc_threshold = gc->config.min_heap_size;
    }
    if (gc->config.max_heap_size > 0 && gc->next_gc_threshold > gc->config.max_heap_size) {
        gc->next_gc_threshold = gc->config.max_heap_size;
    }
    
    gc->is_collecting = false;
    
    // Update timing
    clock_t end_time = clock();
    double elapsed = ((double)(end_time - start_time)) / CLOCKS_PER_SEC * 1000.0;
    gc->stats.last_gc_time = elapsed;
    gc->stats.total_gc_time += elapsed;
    
    if (gc->config.verbose) {
        LOG_INFO(LOG_MODULE_GC, "Collection complete: freed %zu bytes in %.2f ms (remaining: %zu bytes)", 
                 freed, elapsed, gc->bytes_allocated);
    }
}

// Should we collect?
bool gc_should_collect(GarbageCollector* gc) {
    return gc->bytes_allocated_since_gc > gc->next_gc_threshold;
}

// Pin an object (prevent collection)
void gc_pin_object(GarbageCollector* gc, void* object) {
    GCObjectHeader* header = find_header(gc, object);
    if (header) {
        header->is_pinned = true;
    }
}

// Unpin an object
void gc_unpin_object(GarbageCollector* gc, void* object) {
    GCObjectHeader* header = find_header(gc, object);
    if (header) {
        header->is_pinned = false;
    }
}

// Write barrier for incremental GC
void gc_write_barrier(GarbageCollector* gc, void* object, void* value) {
    if (!gc->config.incremental) return;
    if (!gc->is_collecting) return;
    
    GCObjectHeader* obj_header = find_header(gc, object);
    GCObjectHeader* val_header = find_header(gc, value);
    
    // If we're writing a white object into a black object during marking,
    // we need to gray the black object again
    if (obj_header && val_header && 
        obj_header->color == GC_BLACK && 
        val_header->color == GC_WHITE) {
        obj_header->color = GC_GRAY;
        gray_stack_push(&gc->gray_stack, obj_header);
    }
}

// Get statistics
GCStats gc_get_stats(GarbageCollector* gc) {
    return gc->stats;
}

// Print statistics
void gc_print_stats(GarbageCollector* gc) {
    printf("\n=== Garbage Collector Statistics ===\n");
    printf("Collections:         %llu\n", (unsigned long long)gc->stats.collections);
    printf("Total allocated:     %llu bytes\n", (unsigned long long)gc->stats.total_allocated);
    printf("Total freed:         %llu bytes\n", (unsigned long long)gc->stats.total_freed);
    printf("Current allocated:   %llu bytes\n", (unsigned long long)gc->stats.current_allocated);
    printf("Peak allocated:      %llu bytes\n", (unsigned long long)gc->stats.peak_allocated);
    printf("Objects freed:       %llu\n", (unsigned long long)gc->stats.objects_freed);
    printf("Total GC time:       %.2f ms\n", gc->stats.total_gc_time);
    if (gc->stats.collections > 0) {
        printf("Average GC time:     %.2f ms\n", 
               gc->stats.total_gc_time / gc->stats.collections);
    }
    printf("Last GC time:        %.2f ms\n", gc->stats.last_gc_time);
    printf("Current threshold:   %llu bytes\n", (unsigned long long)gc->next_gc_threshold);
    printf("Live objects:        %llu\n", (unsigned long long)gc->object_count);
    printf("===================================\n");
}

// Set verbose mode
void gc_set_verbose(GarbageCollector* gc, bool verbose) {
    gc->config.verbose = verbose;
}

// Set collection threshold
void gc_set_threshold(GarbageCollector* gc, size_t threshold) {
    gc->config.gc_threshold = threshold;
    gc->next_gc_threshold = threshold;
}

// Set incremental mode
void gc_set_incremental(GarbageCollector* gc, bool incremental) {
    gc->config.incremental = incremental;
}

// Perform incremental GC work
void gc_incremental_step(GarbageCollector* gc, size_t work_units) {
    if (!gc || gc->is_collecting || !gc->config.incremental) return;
    
    // Start a new incremental cycle if needed
    if (gc->phase == GC_PHASE_NONE && gc_should_collect(gc)) {
        gc->is_collecting = true;
        gc->stats.collections++;
        gc->phase = GC_PHASE_MARK_ROOTS;
        
        if (gc->config.verbose) {
            LOG_INFO(LOG_MODULE_GC, "Starting incremental collection #%zu", 
                     gc->stats.collections);
        }
    }
    
    size_t work_done = 0;
    clock_t start_time = clock();
    
    while (work_done < work_units && gc->phase != GC_PHASE_NONE) {
        switch (gc->phase) {
            case GC_PHASE_MARK_ROOTS:
                mark_roots(gc);
                gc->phase = GC_PHASE_MARK;
                work_done += 100; // Arbitrary cost for marking roots
                break;
                
            case GC_PHASE_MARK:
                // Process some gray objects
                while (work_done < work_units && gc->gray_stack.count > 0) {
                    GCObjectHeader* header = gray_stack_pop(&gc->gray_stack);
                    if (header) {
                        // Mark children (simplified - real implementation in process_gray_objects)
                        header->color = GC_BLACK;
                        work_done += 10; // Arbitrary cost per object
                    }
                }
                
                if (gc->gray_stack.count == 0) {
                    gc->phase = GC_PHASE_SWEEP;
                    gc->sweep_cursor = gc->all_objects;
                }
                break;
                
            case GC_PHASE_SWEEP:
                // Sweep some objects
                size_t swept = 0;
                while (work_done < work_units && gc->sweep_cursor) {
                    GCObjectHeader* next = gc->sweep_cursor->next;
                    if (gc->sweep_cursor->color == GC_WHITE && !gc->sweep_cursor->is_pinned) {
                        // Free this object
                        size_t freed = gc->sweep_cursor->size;
                        Object* obj = (Object*)gc->sweep_cursor->object;
                        if (obj) {
                            object_destroy(obj);
                        }
                        gc_untrack_object(gc, gc->sweep_cursor->object);
                        gc->stats.objects_freed++;
                        gc->stats.total_freed += freed;
                        swept += freed;
                    } else {
                        // Reset color for next cycle
                        gc->sweep_cursor->color = GC_WHITE;
                    }
                    gc->sweep_cursor = next;
                    work_done += 5; // Arbitrary cost per object
                }
                
                if (!gc->sweep_cursor) {
                    // Collection complete
                    gc->phase = GC_PHASE_NONE;
                    gc->is_collecting = false;
                    gc->bytes_allocated_since_gc = 0;
                    gc->next_gc_threshold = gc->bytes_allocated * gc->config.heap_grow_factor;
                    
                    clock_t end_time = clock();
                    double elapsed = ((double)(end_time - start_time)) / CLOCKS_PER_SEC * 1000.0;
                    gc->stats.last_gc_time = elapsed;
                    gc->stats.total_gc_time += elapsed;
                    
                    if (gc->config.verbose) {
                        LOG_INFO(LOG_MODULE_GC, "Incremental collection complete: freed %zu bytes", 
                                 swept);
                    }
                }
                break;
                
            default:
                gc->phase = GC_PHASE_NONE;
                break;
        }
    }
}

// Root management functions
void gc_add_root(GarbageCollector* gc, TaggedValue* root) {
    if (!gc || !root) return;
    
    // This would require a root set data structure
    // For now, roots are discovered dynamically from the VM
    // This function is a placeholder for future explicit root management
    LOG_WARN(LOG_MODULE_GC, "gc_add_root not fully implemented - roots are managed by VM");
}

void gc_remove_root(GarbageCollector* gc, TaggedValue* root) {
    if (!gc || !root) return;
    
    // This would require a root set data structure
    // For now, roots are discovered dynamically from the VM
    // This function is a placeholder for future explicit root management
    LOG_WARN(LOG_MODULE_GC, "gc_remove_root not fully implemented - roots are managed by VM");
}

// Temporary root stack for protecting values during allocation
void gc_push_temp_root(GarbageCollector* gc, TaggedValue value) {
    if (!gc) return;
    
    // This would require a temporary root stack
    // For now, we rely on the VM stack for protection
    // A full implementation would maintain a separate stack of temporary roots
    LOG_WARN(LOG_MODULE_GC, "gc_push_temp_root not fully implemented - use VM stack for protection");
}

void gc_pop_temp_root(GarbageCollector* gc) {
    if (!gc) return;
    
    // This would require a temporary root stack
    // For now, we rely on the VM stack for protection
    LOG_WARN(LOG_MODULE_GC, "gc_pop_temp_root not fully implemented - use VM stack for protection");
}
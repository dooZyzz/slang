#ifndef GC_H
#define GC_H

#include "runtime/core/object.h"
#include "runtime/core/vm.h"
#include <stdbool.h>
#include <stddef.h>

// GC colors for tri-color marking
typedef enum {
    GC_WHITE = 0,  // Unvisited/garbage (candidate for collection)
    GC_GRAY  = 1,  // Visited but children not yet visited
    GC_BLACK = 2   // Visited and all children visited
} GCColor;

// GC statistics
typedef struct {
    size_t total_allocated;
    size_t total_freed;
    size_t current_allocated;
    size_t peak_allocated;
    size_t collections;
    size_t objects_freed;
    double total_gc_time;
    double last_gc_time;
} GCStats;

// GC configuration
typedef struct {
    size_t heap_grow_factor;      // How much to grow heap (default 2)
    size_t min_heap_size;         // Minimum heap size
    size_t max_heap_size;         // Maximum heap size (0 = unlimited)
    size_t gc_threshold;          // Bytes allocated before triggering GC
    bool incremental;             // Enable incremental GC
    size_t incremental_step_size; // Work done per incremental step
    bool stress_test;             // Force GC on every allocation
    bool verbose;                 // Print GC debug info
} GCConfig;

// GC phase for incremental collection
typedef enum {
    GC_PHASE_NONE,
    GC_PHASE_MARK_ROOTS,
    GC_PHASE_MARK,
    GC_PHASE_SWEEP
} GCPhase;

// Forward declaration
typedef struct GarbageCollector GarbageCollector;

// Object header for GC metadata
typedef struct GCObjectHeader {
    struct GCObjectHeader* next;  // Next in allocation list
    struct GCObjectHeader* prev;  // Previous in allocation list
    GCColor color;                // Current color
    bool is_pinned;              // Object cannot be collected
    size_t size;                 // Size of allocation
    void* object;                // Pointer to actual object
} GCObjectHeader;

// Gray object stack for marking
typedef struct GrayStack {
    GCObjectHeader** items;
    size_t count;
    size_t capacity;
} GrayStack;

// Garbage collector structure
struct GarbageCollector {
    // Object tracking
    GCObjectHeader* all_objects;   // All allocated objects
    size_t object_count;           // Number of objects
    GrayStack gray_stack;          // Gray objects to process
    
    // Memory tracking
    size_t bytes_allocated;        // Current bytes allocated
    size_t bytes_allocated_since_gc; // Bytes since last GC
    size_t next_gc_threshold;      // When to trigger next GC
    
    // GC state
    GCPhase phase;                 // Current GC phase
    bool is_collecting;            // GC in progress
    GCObjectHeader* sweep_cursor;  // Current position in sweep
    
    // Configuration
    GCConfig config;
    
    // Statistics
    GCStats stats;
    
    // VM reference
    VM* vm;
};

// GC creation and destruction
GarbageCollector* gc_create(VM* vm, const GCConfig* config);
void gc_destroy(GarbageCollector* gc);

// Object allocation and tracking
void* gc_alloc(GarbageCollector* gc, size_t size, const char* tag);
void gc_track_object(GarbageCollector* gc, void* object, size_t size);
void gc_untrack_object(GarbageCollector* gc, void* object);

// Manual GC control
void gc_collect(GarbageCollector* gc);
bool gc_should_collect(GarbageCollector* gc);
void gc_incremental_step(GarbageCollector* gc, size_t work_units);

// Root management
void gc_add_root(GarbageCollector* gc, TaggedValue* root);
void gc_remove_root(GarbageCollector* gc, TaggedValue* root);
void gc_push_temp_root(GarbageCollector* gc, TaggedValue value);
void gc_pop_temp_root(GarbageCollector* gc);

// Object pinning (prevent collection)
void gc_pin_object(GarbageCollector* gc, void* object);
void gc_unpin_object(GarbageCollector* gc, void* object);

// Write barrier for incremental GC
void gc_write_barrier(GarbageCollector* gc, void* object, void* value);

// Statistics and debugging
GCStats gc_get_stats(GarbageCollector* gc);
void gc_print_stats(GarbageCollector* gc);
void gc_set_verbose(GarbageCollector* gc, bool verbose);

// Configuration
void gc_set_threshold(GarbageCollector* gc, size_t threshold);
void gc_set_incremental(GarbageCollector* gc, bool incremental);

// Helper macros
#define GC_ALLOC(gc, type, tag) ((type*)gc_alloc(gc, sizeof(type), tag))
#define GC_ALLOC_ARRAY(gc, type, count, tag) ((type*)gc_alloc(gc, sizeof(type) * (count), tag))

// Write barrier macro
#define GC_WRITE_BARRIER(gc, obj, field, value) do { \
    (obj)->field = (value); \
    gc_write_barrier(gc, obj, value); \
} while(0)

#endif // GC_H
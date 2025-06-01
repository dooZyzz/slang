#include "runtime/modules/extensions/module_inspect.h"
#include "runtime/modules/loader/module_loader.h"
#include "runtime/modules/loader/module_cache.h"
#include "runtime/core/vm.h"
#include "utils/hash_map.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <fnmatch.h>

/**
 * Implementation of module metadata inspection API.
 */

// Module state tracking (for statistics)
typedef struct ModuleMetrics {
    clock_t load_start;
    clock_t load_end;
    clock_t init_start;
    clock_t init_end;
    size_t access_count;
    size_t export_lookups;
    size_t cache_hits;
    size_t cache_misses;
} ModuleMetrics;

// Store metrics in a hash map (module path -> metrics)
static HashMap* g_module_metrics = NULL;

// Initialize metrics tracking
static void ensure_metrics_initialized() {
    if (!g_module_metrics) {
        g_module_metrics = hash_map_create();
    }
}

// Get or create metrics for a module
static ModuleMetrics* get_module_metrics(const char* module_path) {
    ensure_metrics_initialized();
    
    ModuleMetrics* metrics = (ModuleMetrics*)hash_map_get(g_module_metrics, module_path);
    if (!metrics) {
        metrics = calloc(1, sizeof(ModuleMetrics));
        hash_map_put(g_module_metrics, module_path, metrics);
    }
    return metrics;
}

// Get module information
ModuleInfo* module_get_info(Module* module) {
    if (!module) return NULL;
    
    ModuleInfo* info = calloc(1, sizeof(ModuleInfo));
    info->path = module->path;
    info->absolute_path = module->absolute_path;
    info->version = module->version;
    info->description = NULL; // TODO: Get from metadata
    info->type = module->is_native ? "native" : "library";
    info->state = module->state;
    info->is_native = module->is_native;
    info->is_lazy = (module->chunk != NULL);
    info->export_count = module->exports.count;
    info->global_count = module->globals.count;
    
    // Estimate memory usage
    info->memory_usage = sizeof(Module);
    info->memory_usage += module->exports.capacity * (sizeof(char*) + sizeof(TaggedValue) + sizeof(uint8_t));
    info->memory_usage += module->globals.capacity * (sizeof(char*) + sizeof(TaggedValue));
    if (module->chunk) {
        info->memory_usage += sizeof(Chunk) + module->chunk->count + 
                             module->chunk->constants.count * sizeof(TaggedValue);
    }
    
    return info;
}

// Free module information
void module_info_free(ModuleInfo* info) {
    if (info) {
        // Note: strings are owned by the module, don't free them
        free(info);
    }
}

// Helper structure for collecting modules
typedef struct {
    Module** modules;
    size_t count;
    size_t capacity;
} ModuleCollector;

// Iterator callback for collecting modules
static void collect_module(const char* name, Module* module, void* user_data) {
    ModuleCollector* collector = (ModuleCollector*)user_data;
    
    if (collector->count >= collector->capacity) {
        collector->capacity = collector->capacity ? collector->capacity * 2 : 8;
        collector->modules = realloc(collector->modules, 
                                   collector->capacity * sizeof(Module*));
    }
    
    collector->modules[collector->count++] = module;
}

// Get all loaded modules
Module** module_get_all_loaded(ModuleLoader* loader, size_t* count) {
    if (!loader || !count) return NULL;
    
    ModuleCollector collector = {0};
    module_cache_iterate(loader->cache, collect_module, &collector);
    
    *count = collector.count;
    return collector.modules;
}

// Get value type name
const char* value_type_to_string(ValueType type) {
    switch (type) {
        case VAL_NIL: return "nil";
        case VAL_BOOL: return "bool";
        case VAL_NUMBER: return "number";
        case VAL_STRING: return "string";
        case VAL_FUNCTION: return "function";
        case VAL_CLOSURE: return "closure";
        case VAL_NATIVE: return "native";
        case VAL_OBJECT: return "object";
        case VAL_STRUCT: return "struct";
        default: return "unknown";
    }
}

// Get export information
ExportInfo* module_get_exports(Module* module, size_t* count) {
    if (!module || !count) return NULL;
    
    *count = module->exports.count;
    if (*count == 0) return NULL;
    
    ExportInfo* exports = calloc(*count, sizeof(ExportInfo));
    
    for (size_t i = 0; i < *count; i++) {
        exports[i].name = module->exports.names[i];
        exports[i].visibility = module->exports.visibility[i];
        
        TaggedValue value = module->exports.values[i];
        exports[i].type = value.type;
        exports[i].type_name = value_type_to_string(value.type);
        
        // Determine if constant (simple heuristic: uppercase name)
        exports[i].is_constant = (exports[i].name[0] >= 'A' && exports[i].name[0] <= 'Z');
        
        // Function-specific info
        if (IS_FUNCTION(value)) {
            exports[i].is_function = true;
            Function* func = AS_FUNCTION(value);
            exports[i].function.arity = func->arity;
            exports[i].function.is_native = false;
            exports[i].function.is_closure = false;
            exports[i].function.module = func->module ? func->module->path : NULL;
        } else if (IS_CLOSURE(value)) {
            exports[i].is_function = true;
            Closure* closure = AS_CLOSURE(value);
            exports[i].function.arity = closure->function->arity;
            exports[i].function.is_native = false;
            exports[i].function.is_closure = true;
            exports[i].function.module = closure->function->module ? 
                                       closure->function->module->path : NULL;
        } else if (IS_NATIVE(value)) {
            exports[i].is_function = true;
            exports[i].function.arity = -1; // Unknown for native functions
            exports[i].function.is_native = true;
            exports[i].function.is_closure = false;
            exports[i].function.module = module->path;
        }
    }
    
    return exports;
}

// Get specific export info
ExportInfo* module_get_export_info(Module* module, const char* export_name) {
    if (!module || !export_name) return NULL;
    
    for (size_t i = 0; i < module->exports.count; i++) {
        if (strcmp(module->exports.names[i], export_name) == 0) {
            size_t count = 1;
            ExportInfo* exports = module_get_exports(module, &count);
            if (exports) {
                ExportInfo* result = malloc(sizeof(ExportInfo));
                *result = exports[i];
                free(exports);
                return result;
            }
        }
    }
    
    return NULL;
}

// Free export information
void module_exports_free(ExportInfo* exports, size_t count) {
    if (exports) {
        free(exports);
    }
}

// Get module dependencies (placeholder - needs package metadata)
DependencyInfo* module_get_dependencies(Module* module, size_t* count) {
    if (!module || !count) return NULL;
    
    // TODO: Implement when package metadata is available
    *count = 0;
    return NULL;
}

// Free dependency information
void module_dependencies_free(DependencyInfo* deps, size_t count) {
    if (deps) {
        free(deps);
    }
}

// Get module statistics
ModuleStats* module_get_stats(Module* module) {
    if (!module) return NULL;
    
    ModuleStats* stats = calloc(1, sizeof(ModuleStats));
    ModuleMetrics* metrics = get_module_metrics(module->path);
    
    if (metrics->load_end > metrics->load_start) {
        stats->load_time_ms = (metrics->load_end - metrics->load_start) * 1000 / CLOCKS_PER_SEC;
    }
    if (metrics->init_end > metrics->init_start) {
        stats->init_time_ms = (metrics->init_end - metrics->init_start) * 1000 / CLOCKS_PER_SEC;
    }
    
    stats->access_count = metrics->access_count;
    stats->export_lookups = metrics->export_lookups;
    stats->cache_hits = metrics->cache_hits;
    stats->cache_misses = metrics->cache_misses;
    
    return stats;
}

// Free module statistics
void module_stats_free(ModuleStats* stats) {
    if (stats) {
        free(stats);
    }
}

// Pattern search callback context
typedef struct {
    const char* pattern;
    ModuleCollector collector;
} PatternSearchContext;

// Iterator callback for pattern matching
static void pattern_match_module(const char* name, Module* module, void* user_data) {
    PatternSearchContext* ctx = (PatternSearchContext*)user_data;
    
    if (fnmatch(ctx->pattern, name, 0) == 0) {
        collect_module(name, module, &ctx->collector);
    }
}

// Find modules by pattern
Module** module_find_by_pattern(ModuleLoader* loader, const char* pattern, size_t* count) {
    if (!loader || !pattern || !count) return NULL;
    
    PatternSearchContext ctx = {
        .pattern = pattern,
        .collector = {0}
    };
    
    module_cache_iterate(loader->cache, pattern_match_module, &ctx);
    
    *count = ctx.collector.count;
    return ctx.collector.modules;
}

// Export search callback context
typedef struct {
    const char* symbol_name;
    ModuleCollector collector;
} ExportSearchContext;

// Iterator callback for export searching
static void export_search_module(const char* name, Module* module, void* user_data) {
    ExportSearchContext* ctx = (ExportSearchContext*)user_data;
    
    // Check if module exports the symbol
    if (module_has_export(module, ctx->symbol_name)) {
        collect_module(name, module, &ctx->collector);
    }
}

// Find modules by export
Module** module_find_by_export(ModuleLoader* loader, const char* symbol_name, size_t* count) {
    if (!loader || !symbol_name || !count) return NULL;
    
    ExportSearchContext ctx = {
        .symbol_name = symbol_name,
        .collector = {0}
    };
    
    module_cache_iterate(loader->cache, export_search_module, &ctx);
    
    *count = ctx.collector.count;
    return ctx.collector.modules;
}

// Serialize module to JSON
char* module_to_json(Module* module, bool include_exports, bool include_stats) {
    if (!module) return NULL;
    
    // Start with a reasonable size
    size_t capacity = 1024;
    char* json = malloc(capacity);
    size_t len = 0;
    
    // Helper macro to append to JSON string
    #define APPEND(fmt, ...) do { \
        int written = snprintf(json + len, capacity - len, fmt, ##__VA_ARGS__); \
        if (written >= 0 && (size_t)written >= capacity - len) { \
            capacity *= 2; \
            json = realloc(json, capacity); \
            written = snprintf(json + len, capacity - len, fmt, ##__VA_ARGS__); \
        } \
        if (written > 0) len += written; \
    } while(0)
    
    APPEND("{");
    APPEND("\"path\":\"%s\",", module->path);
    APPEND("\"absolute_path\":\"%s\",", module->absolute_path ? module->absolute_path : "");
    APPEND("\"version\":\"%s\",", module->version ? module->version : "");
    APPEND("\"state\":\"%s\",", module_state_to_string(module->state));
    APPEND("\"is_native\":%s,", module->is_native ? "true" : "false");
    APPEND("\"is_lazy\":%s,", module->chunk ? "true" : "false");
    APPEND("\"export_count\":%zu,", module->exports.count);
    APPEND("\"global_count\":%zu", module->globals.count);
    
    if (include_exports && module->exports.count > 0) {
        APPEND(",\"exports\":[");
        for (size_t i = 0; i < module->exports.count; i++) {
            if (i > 0) APPEND(",");
            APPEND("{\"name\":\"%s\",", module->exports.names[i]);
            APPEND("\"type\":\"%s\",", value_type_to_string(module->exports.values[i].type));
            APPEND("\"visibility\":%d}", module->exports.visibility[i]);
        }
        APPEND("]");
    }
    
    if (include_stats) {
        ModuleStats* stats = module_get_stats(module);
        APPEND(",\"stats\":{");
        APPEND("\"load_time_ms\":%zu,", stats->load_time_ms);
        APPEND("\"init_time_ms\":%zu,", stats->init_time_ms);
        APPEND("\"access_count\":%zu,", stats->access_count);
        APPEND("\"export_lookups\":%zu", stats->export_lookups);
        APPEND("}");
        module_stats_free(stats);
    }
    
    APPEND("}");
    
    #undef APPEND
    
    return json;
}

// Serialize all modules to JSON
char* module_loader_to_json(ModuleLoader* loader) {
    if (!loader) return NULL;
    
    size_t capacity = 1024;
    char* json = malloc(capacity);
    size_t len = 0;
    
    #define APPEND(fmt, ...) do { \
        int written = snprintf(json + len, capacity - len, fmt, ##__VA_ARGS__); \
        if (written >= 0 && (size_t)written >= capacity - len) { \
            capacity *= 2; \
            json = realloc(json, capacity); \
            written = snprintf(json + len, capacity - len, fmt, ##__VA_ARGS__); \
        } \
        if (written > 0) len += written; \
    } while(0)
    
    size_t module_count;
    Module** modules = module_get_all_loaded(loader, &module_count);
    
    APPEND("{\"modules\":[");
    
    for (size_t i = 0; i < module_count; i++) {
        if (i > 0) APPEND(",");
        char* module_json = module_to_json(modules[i], false, false);
        if (module_json) {
            APPEND("%s", module_json);
            free(module_json);
        }
    }
    
    APPEND("],\"count\":%zu}", module_count);
    
    if (modules) free(modules);
    
    #undef APPEND
    
    return json;
}

// Get module state string
const char* module_state_to_string(ModuleState state) {
    switch (state) {
        case MODULE_STATE_UNLOADED: return "unloaded";
        case MODULE_STATE_LOADING: return "loading";
        case MODULE_STATE_LOADED: return "loaded";
        case MODULE_STATE_ERROR: return "error";
        default: return "unknown";
    }
}

// Check module capability
bool module_has_capability(Module* module, const char* capability) {
    if (!module || !capability) return false;
    
    if (strcmp(capability, "native") == 0) {
        return module->is_native;
    } else if (strcmp(capability, "lazy") == 0) {
        return module->chunk != NULL;
    } else if (strcmp(capability, "async") == 0) {
        // TODO: Check for async support
        return false;
    } else if (strcmp(capability, "exports") == 0) {
        return module->exports.count > 0;
    }
    
    return false;
}

// Print module information
void module_print_info(Module* module, bool verbose) {
    if (!module) return;
    
    fprintf(stderr, "Module: %s\n", module->path);
    fprintf(stderr, "  State: %s\n", module_state_to_string(module->state));
    fprintf(stderr, "  Type: %s\n", module->is_native ? "native" : "script");
    if (module->version) {
        fprintf(stderr, "  Version: %s\n", module->version);
    }
    fprintf(stderr, "  Exports: %zu\n", module->exports.count);
    
    if (verbose && module->exports.count > 0) {
        fprintf(stderr, "  Export list:\n");
        for (size_t i = 0; i < module->exports.count; i++) {
            fprintf(stderr, "    - %s (%s)\n", 
                   module->exports.names[i],
                   value_type_to_string(module->exports.values[i].type));
        }
    }
    
    if (verbose) {
        ModuleInfo* info = module_get_info(module);
        fprintf(stderr, "  Memory usage: %zu bytes\n", info->memory_usage);
        module_info_free(info);
        
        ModuleStats* stats = module_get_stats(module);
        if (stats->load_time_ms > 0) {
            fprintf(stderr, "  Load time: %zu ms\n", stats->load_time_ms);
        }
        module_stats_free(stats);
    }
}

// Print dependency tree (placeholder)
void module_print_dependency_tree(Module* module, int max_depth) {
    if (!module) return;
    
    fprintf(stderr, "%s\n", module->path);
    
    // Get dependency information
    size_t dep_count;
    DependencyInfo* deps = module_get_dependencies(module, &dep_count);
    
    // Print dependencies recursively
    if (max_depth > 0 && deps && dep_count > 0) {
        fprintf(stderr, "  Dependencies:\n");
        
        for (size_t i = 0; i < dep_count; i++) {
            fprintf(stderr, "    - %s", deps[i].name);
            if (deps[i].version) {
                fprintf(stderr, " (%s)", deps[i].version);
            }
            fprintf(stderr, "\n");
        }
        
        free(deps);
    } else if (max_depth > 0) {
        fprintf(stderr, "  No dependencies\n");
    }
}

// Track module access for statistics
void module_track_access(Module* module) {
    if (!module) return;
    
    ModuleMetrics* metrics = get_module_metrics(module->path);
    metrics->access_count++;
}

// Track export lookup
void module_track_export_lookup(Module* module, bool hit) {
    if (!module) return;
    
    ModuleMetrics* metrics = get_module_metrics(module->path);
    metrics->export_lookups++;
    if (hit) {
        metrics->cache_hits++;
    } else {
        metrics->cache_misses++;
    }
}

// Track module load timing
void module_track_load_start(Module* module) {
    if (!module) return;
    
    ModuleMetrics* metrics = get_module_metrics(module->path);
    metrics->load_start = clock();
}

void module_track_load_end(Module* module) {
    if (!module) return;
    
    ModuleMetrics* metrics = get_module_metrics(module->path);
    metrics->load_end = clock();
}

// Track module init timing
void module_track_init_start(Module* module) {
    if (!module) return;
    
    ModuleMetrics* metrics = get_module_metrics(module->path);
    metrics->init_start = clock();
}

void module_track_init_end(Module* module) {
    if (!module) return;
    
    ModuleMetrics* metrics = get_module_metrics(module->path);
    metrics->init_end = clock();
}
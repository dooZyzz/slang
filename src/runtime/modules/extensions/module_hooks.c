#include "runtime/modules/extensions/module_hooks.h"
#include "utils/hash_map.h"
#include "runtime/core/vm.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>

/**
 * Implementation of module lifecycle hooks.
 */

// Hook storage entry
typedef struct HookEntry {
    char* module_name;
    ModuleHooks hooks;
    struct HookEntry* next;
} HookEntry;

// Global hook entry
typedef struct GlobalHookEntry {
    int id;
    int priority;
    GlobalModuleHooks hooks;
    struct GlobalHookEntry* next;
} GlobalHookEntry;

// Hook system state
static struct {
    HashMap* module_hooks;          // Module name -> ModuleHooks
    GlobalHookEntry* global_hooks;  // Sorted list of global hooks
    int next_global_id;
    pthread_mutex_t lock;
    
    // Statistics
    size_t total_hooks;
    size_t executions;
    size_t failures;
} g_hook_system = {0};

// Initialize hooks system
void module_hooks_init(void) {
    g_hook_system.module_hooks = hash_map_create();
    g_hook_system.global_hooks = NULL;
    g_hook_system.next_global_id = 1;
    pthread_mutex_init(&g_hook_system.lock, NULL);
    g_hook_system.total_hooks = 0;
    g_hook_system.executions = 0;
    g_hook_system.failures = 0;
}

// Cleanup hooks system
void module_hooks_cleanup(void) {
    pthread_mutex_lock(&g_hook_system.lock);
    
    // Free module-specific hooks
    if (g_hook_system.module_hooks) {
        // Note: hash_map_destroy will free the entries
        hash_map_destroy(g_hook_system.module_hooks);
        g_hook_system.module_hooks = NULL;
    }
    
    // Free global hooks
    GlobalHookEntry* entry = g_hook_system.global_hooks;
    while (entry) {
        GlobalHookEntry* next = entry->next;
        free(entry);
        entry = next;
    }
    g_hook_system.global_hooks = NULL;
    
    pthread_mutex_unlock(&g_hook_system.lock);
    pthread_mutex_destroy(&g_hook_system.lock);
}

// Set hooks for a specific module
bool module_set_hooks(const char* module_name, const ModuleHooks* hooks) {
    if (!module_name || !hooks) return false;
    
    pthread_mutex_lock(&g_hook_system.lock);
    
    // Create hook entry
    HookEntry* entry = malloc(sizeof(HookEntry));
    entry->module_name = strdup(module_name);
    entry->hooks = *hooks;
    entry->next = NULL;
    
    // Store in hash map
    hash_map_put(g_hook_system.module_hooks, module_name, entry);
    g_hook_system.total_hooks++;
    
    pthread_mutex_unlock(&g_hook_system.lock);
    return true;
}

// Get hooks for a module
const ModuleHooks* module_get_hooks(const char* module_name) {
    if (!module_name) return NULL;
    
    pthread_mutex_lock(&g_hook_system.lock);
    HookEntry* entry = (HookEntry*)hash_map_get(g_hook_system.module_hooks, module_name);
    pthread_mutex_unlock(&g_hook_system.lock);
    
    return entry ? &entry->hooks : NULL;
}

// Remove hooks for a module
void module_remove_hooks(const char* module_name) {
    if (!module_name) return;
    
    pthread_mutex_lock(&g_hook_system.lock);
    
    HookEntry* entry = (HookEntry*)hash_map_get(g_hook_system.module_hooks, module_name);
    if (entry) {
        free(entry->module_name);
        free(entry);
        hash_map_remove(g_hook_system.module_hooks, module_name);
        g_hook_system.total_hooks--;
    }
    
    pthread_mutex_unlock(&g_hook_system.lock);
}

// Register global hooks
int module_register_global_hooks(const GlobalModuleHooks* hooks, int priority) {
    if (!hooks) return -1;
    
    pthread_mutex_lock(&g_hook_system.lock);
    
    // Create new entry
    GlobalHookEntry* new_entry = malloc(sizeof(GlobalHookEntry));
    new_entry->id = g_hook_system.next_global_id++;
    new_entry->priority = priority;
    new_entry->hooks = *hooks;
    new_entry->next = NULL;
    
    // Insert in priority order (lower priority first)
    GlobalHookEntry** current = &g_hook_system.global_hooks;
    while (*current && (*current)->priority <= priority) {
        current = &(*current)->next;
    }
    new_entry->next = *current;
    *current = new_entry;
    
    g_hook_system.total_hooks++;
    int id = new_entry->id;
    
    pthread_mutex_unlock(&g_hook_system.lock);
    return id;
}

// Unregister global hooks
void module_unregister_global_hooks(int hook_id) {
    pthread_mutex_lock(&g_hook_system.lock);
    
    GlobalHookEntry** current = &g_hook_system.global_hooks;
    while (*current) {
        if ((*current)->id == hook_id) {
            GlobalHookEntry* to_remove = *current;
            *current = to_remove->next;
            free(to_remove);
            g_hook_system.total_hooks--;
            break;
        }
        current = &(*current)->next;
    }
    
    pthread_mutex_unlock(&g_hook_system.lock);
}

// Execute initialization hooks
bool module_execute_init_hooks(Module* module, VM* vm) {
    if (!module) return true;
    
    bool success = true;
    pthread_mutex_lock(&g_hook_system.lock);
    g_hook_system.executions++;
    
    // Execute global before_init hooks
    GlobalHookEntry* global = g_hook_system.global_hooks;
    while (global && success) {
        if (global->hooks.before_init) {
            if (!global->hooks.should_apply || 
                global->hooks.should_apply(module->path, global->hooks.user_data)) {
                success = global->hooks.before_init(module, vm);
            }
        }
        global = global->next;
    }
    
    // Execute module-specific init hook
    if (success) {
        HookEntry* entry = (HookEntry*)hash_map_get(g_hook_system.module_hooks, module->path);
        if (entry && entry->hooks.on_init) {
            success = entry->hooks.on_init(module, vm);
        }
    }
    
    // Execute global after_init hooks
    global = g_hook_system.global_hooks;
    while (global && success) {
        if (global->hooks.after_init) {
            if (!global->hooks.should_apply || 
                global->hooks.should_apply(module->path, global->hooks.user_data)) {
                success = global->hooks.after_init(module, vm);
            }
        }
        global = global->next;
    }
    
    if (!success) {
        g_hook_system.failures++;
    }
    
    pthread_mutex_unlock(&g_hook_system.lock);
    return success;
}

// Execute first-use hooks
void module_execute_first_use_hooks(Module* module, VM* vm) {
    if (!module) return;
    
    pthread_mutex_lock(&g_hook_system.lock);
    g_hook_system.executions++;
    
    HookEntry* entry = (HookEntry*)hash_map_get(g_hook_system.module_hooks, module->path);
    if (entry && entry->hooks.on_first_use) {
        entry->hooks.on_first_use(module, vm);
    }
    
    pthread_mutex_unlock(&g_hook_system.lock);
}

// Execute unload hooks
void module_execute_unload_hooks(Module* module, VM* vm) {
    if (!module) return;
    
    pthread_mutex_lock(&g_hook_system.lock);
    g_hook_system.executions++;
    
    // Execute global before_unload hooks
    GlobalHookEntry* global = g_hook_system.global_hooks;
    while (global) {
        if (global->hooks.before_unload) {
            if (!global->hooks.should_apply || 
                global->hooks.should_apply(module->path, global->hooks.user_data)) {
                global->hooks.before_unload(module, vm);
            }
        }
        global = global->next;
    }
    
    // Execute module-specific unload hook
    HookEntry* entry = (HookEntry*)hash_map_get(g_hook_system.module_hooks, module->path);
    if (entry && entry->hooks.on_unload) {
        entry->hooks.on_unload(module, vm);
    }
    
    // Execute global after_unload hooks
    global = g_hook_system.global_hooks;
    while (global) {
        if (global->hooks.after_unload) {
            if (!global->hooks.should_apply || 
                global->hooks.should_apply(module->path, global->hooks.user_data)) {
                global->hooks.after_unload(module, vm);
            }
        }
        global = global->next;
    }
    
    pthread_mutex_unlock(&g_hook_system.lock);
}

// Execute error hooks
void module_execute_error_hooks(Module* module, VM* vm, const char* error) {
    if (!module) return;
    
    pthread_mutex_lock(&g_hook_system.lock);
    g_hook_system.executions++;
    
    HookEntry* entry = (HookEntry*)hash_map_get(g_hook_system.module_hooks, module->path);
    if (entry && entry->hooks.on_error) {
        entry->hooks.on_error(module, vm, error);
    }
    
    pthread_mutex_unlock(&g_hook_system.lock);
}

// SwiftLang script hooks implementation
typedef struct {
    char* function_name;
    bool is_init_hook;
} ScriptHookData;

static bool script_init_hook(Module* module, VM* vm) {
    ScriptHookData* data = (ScriptHookData*)module_get_hooks(module->path)->user_data;
    if (!data || !data->function_name) return true;
    
    // Look up the function in the module
    TaggedValue func_val = module_get_export(module, data->function_name);
    if (!IS_FUNCTION(func_val)) {
        fprintf(stderr, "Module init hook: function '%s' not found\n", data->function_name);
        return false;
    }
    
    // Call the function
    vm_push(vm, func_val);
    TaggedValue result = vm_call_value(vm, func_val, 0, NULL);
    if (IS_NIL(result)) {
        fprintf(stderr, "Module init hook: function '%s' failed\n", data->function_name);
        return false;
    }
    
    // Check return value (should be bool)
    if (!IS_BOOL(result)) {
        fprintf(stderr, "Module init hook: function '%s' must return bool\n", data->function_name);
        return false;
    }
    
    return AS_BOOL(result);
}

static void script_unload_hook(Module* module, VM* vm) {
    ScriptHookData* data = (ScriptHookData*)module_get_hooks(module->path)->user_data;
    if (!data || !data->function_name) return;
    
    // Look up the function in the module
    TaggedValue func_val = module_get_export(module, data->function_name);
    if (!IS_FUNCTION(func_val)) {
        fprintf(stderr, "Module unload hook: function '%s' not found\n", data->function_name);
        return;
    }
    
    // Call the function
    vm_push(vm, func_val);
    vm_call_value(vm, func_val, 0, NULL);
}

// Set SwiftLang init hook
bool module_set_script_init_hook(const char* module_name, const char* init_function_name) {
    if (!module_name || !init_function_name) return false;
    
    ScriptHookData* data = malloc(sizeof(ScriptHookData));
    data->function_name = strdup(init_function_name);
    data->is_init_hook = true;
    
    ModuleHooks hooks = {0};
    hooks.on_init = script_init_hook;
    hooks.user_data = data;
    
    return module_set_hooks(module_name, &hooks);
}

// Set SwiftLang unload hook
bool module_set_script_unload_hook(const char* module_name, const char* unload_function_name) {
    if (!module_name || !unload_function_name) return false;
    
    ScriptHookData* data = malloc(sizeof(ScriptHookData));
    data->function_name = strdup(unload_function_name);
    data->is_init_hook = false;
    
    ModuleHooks hooks = {0};
    hooks.on_unload = script_unload_hook;
    hooks.user_data = data;
    
    return module_set_hooks(module_name, &hooks);
}

// Get hook statistics
void module_hooks_stats(size_t* total_hooks, size_t* executions, size_t* failures) {
    pthread_mutex_lock(&g_hook_system.lock);
    
    if (total_hooks) *total_hooks = g_hook_system.total_hooks;
    if (executions) *executions = g_hook_system.executions;
    if (failures) *failures = g_hook_system.failures;
    
    pthread_mutex_unlock(&g_hook_system.lock);
}
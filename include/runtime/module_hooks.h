#ifndef MODULE_HOOKS_H
#define MODULE_HOOKS_H

#include "runtime/module.h"
#include "vm/vm.h"
#include <stdbool.h>

/**
 * Module lifecycle hooks for initialization and cleanup.
 * 
 * Hooks are called at specific points in the module lifecycle:
 * - onInit: Called after module is loaded and exports are set up
 * - onFirstUse: Called before first access to module (lazy init)
 * - onUnload: Called before module is unloaded
 * - onError: Called if module loading fails
 */

// Hook function types
typedef bool (*ModuleInitHook)(Module* module, VM* vm);
typedef void (*ModuleUnloadHook)(Module* module, VM* vm);
typedef void (*ModuleErrorHook)(Module* module, VM* vm, const char* error);
typedef void (*ModuleFirstUseHook)(Module* module, VM* vm);

// Module hook configuration
typedef struct ModuleHooks {
    ModuleInitHook on_init;           // Called after loading
    ModuleFirstUseHook on_first_use;  // Called on first access (lazy)
    ModuleUnloadHook on_unload;       // Called before unloading
    ModuleErrorHook on_error;         // Called on error
    
    // User data for hooks
    void* user_data;
} ModuleHooks;

// Global hook registration (affects all modules)
typedef struct GlobalModuleHooks {
    ModuleInitHook before_init;      // Called before module init
    ModuleInitHook after_init;       // Called after module init
    ModuleUnloadHook before_unload;  // Called before module unload
    ModuleUnloadHook after_unload;   // Called after module unload
    
    // Module filtering
    bool (*should_apply)(const char* module_name, void* user_data);
    void* user_data;
} GlobalModuleHooks;

// Hook management functions

/**
 * Set hooks for a specific module.
 * Must be called before the module is loaded.
 * 
 * @param module_name Name of the module
 * @param hooks Hook configuration
 * @return true if hooks were set successfully
 */
bool module_set_hooks(const char* module_name, const ModuleHooks* hooks);

/**
 * Get hooks for a module.
 * 
 * @param module_name Name of the module
 * @return Module hooks or NULL if none set
 */
const ModuleHooks* module_get_hooks(const char* module_name);

/**
 * Remove hooks for a module.
 * 
 * @param module_name Name of the module
 */
void module_remove_hooks(const char* module_name);

/**
 * Register global hooks that apply to all modules.
 * Multiple global hooks can be registered.
 * 
 * @param hooks Global hook configuration
 * @param priority Hook priority (lower executes first)
 * @return Hook ID for later removal
 */
int module_register_global_hooks(const GlobalModuleHooks* hooks, int priority);

/**
 * Unregister global hooks.
 * 
 * @param hook_id ID returned by register_global_hooks
 */
void module_unregister_global_hooks(int hook_id);

// Hook execution (called by module system)

/**
 * Execute initialization hooks for a module.
 * Called by the module system after loading.
 * 
 * @param module The loaded module
 * @param vm Current VM instance
 * @return true if all hooks succeeded
 */
bool module_execute_init_hooks(Module* module, VM* vm);

/**
 * Execute first-use hooks for a module.
 * Called on first access to a lazy-loaded module.
 * 
 * @param module The module being accessed
 * @param vm Current VM instance
 */
void module_execute_first_use_hooks(Module* module, VM* vm);

/**
 * Execute unload hooks for a module.
 * Called before module is unloaded.
 * 
 * @param module The module being unloaded
 * @param vm Current VM instance
 */
void module_execute_unload_hooks(Module* module, VM* vm);

/**
 * Execute error hooks for a module.
 * Called when module loading fails.
 * 
 * @param module The module that failed
 * @param vm Current VM instance
 * @param error Error message
 */
void module_execute_error_hooks(Module* module, VM* vm, const char* error);

// SwiftLang script hooks

/**
 * Set a SwiftLang function as a module init hook.
 * The function should accept no arguments and return bool.
 * 
 * @param module_name Name of the module
 * @param init_function_name Name of the init function
 * @return true if hook was set successfully
 */
bool module_set_script_init_hook(const char* module_name, const char* init_function_name);

/**
 * Set a SwiftLang function as a module unload hook.
 * The function should accept no arguments.
 * 
 * @param module_name Name of the module
 * @param unload_function_name Name of the unload function
 * @return true if hook was set successfully
 */
bool module_set_script_unload_hook(const char* module_name, const char* unload_function_name);

// Utility functions

/**
 * Initialize the module hooks system.
 * Called once at startup.
 */
void module_hooks_init(void);

/**
 * Cleanup the module hooks system.
 * Called at shutdown.
 */
void module_hooks_cleanup(void);

/**
 * Get statistics about hook usage.
 * 
 * @param total_hooks Total number of registered hooks
 * @param executions Total hook executions
 * @param failures Total hook failures
 */
void module_hooks_stats(size_t* total_hooks, size_t* executions, size_t* failures);

#endif // MODULE_HOOKS_H
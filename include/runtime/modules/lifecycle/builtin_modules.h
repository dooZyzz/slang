#ifndef BUILTIN_MODULES_H
#define BUILTIN_MODULES_H

#include "runtime/core/vm.h"
#include <stdbool.h>

// Builtin module registry
typedef struct {
    const char* name;
    void (*init_func)(void);
    struct {
        const char** names;
        NativeFn* functions;
        size_t count;
    } exports;
} BuiltinModule;

// Initialize all builtin modules
void builtin_modules_init(void);

// Register a builtin module
void builtin_module_register(const char* name, void (*init_func)(void));

// Get exports from a builtin module
bool builtin_module_get_export(const char* module_name, const char* export_name, 
                              TaggedValue* out_value);

// Get all exports from a builtin module
bool builtin_module_get_all_exports(const char* module_name, 
                                   const char*** names, 
                                   TaggedValue** values, 
                                   size_t* count);

// Standard library module initializers
void builtin_math_init(void);
void builtin_io_init(void);
void builtin_string_init(void);
void builtin_array_init(void);

#endif

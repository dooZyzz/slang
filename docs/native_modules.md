# Native Module Development Guide

## Overview

Native modules allow you to extend SwiftLang with C/C++ code for performance-critical operations or system integration. Native modules are dynamically loaded shared libraries (.so on Linux, .dylib on macOS) that export functions accessible from SwiftLang code.

## Module Structure

A native module consists of:
- A `module.json` manifest file
- One or more C/C++ source files
- Optionally, Swift wrapper files

### Example Directory Structure

```
modules/time/
├── module.json          # Module manifest
├── src/
│   ├── time.swift      # Optional Swift wrapper
│   └── time_native.c   # Native implementation
└── time_native.so      # Compiled shared library
```

## Module Manifest

The `module.json` file describes the module and its exports:

```json
{
  "name": "time",
  "version": "1.0.0",
  "description": "Native time module for SwiftLang",
  "type": "native",
  "main": "src/time.swift",
  "native": {
    "library": "time_native"
  },
  "exports": [
    {
      "name": "now",
      "type": "function",
      "native": "native_time_now"
    },
    {
      "name": "format",
      "type": "function",
      "native": "native_time_format"
    }
  ]
}
```

## Native Module Implementation

### Basic Structure

```c
#include <stdbool.h>

// SwiftLang value types
typedef enum {
    VAL_BOOL,
    VAL_NIL,
    VAL_NUMBER,
    VAL_STRING,
    VAL_OBJECT,
    VAL_FUNCTION,
    VAL_CLOSURE,
    VAL_NATIVE,
    VAL_STRUCT,
    VAL_MODULE
} ValueType;

typedef union {
    bool boolean;
    double number;
    char* string;
    void* object;
    void* function;
    void* closure;
    void* native;
    void* module;
} Value;

typedef struct {
    ValueType type;
    Value as;
} TaggedValue;

// Value creation macros
#define NUMBER_VAL(value) ((TaggedValue){VAL_NUMBER, {.number = value}})
#define STRING_VAL(value) ((TaggedValue){VAL_STRING, {.string = value}})
#define BOOL_VAL(value) ((TaggedValue){VAL_BOOL, {.boolean = value}})
#define NIL_VAL ((TaggedValue){VAL_NIL, {.number = 0}})
#define NATIVE_VAL(fn) ((TaggedValue){VAL_NATIVE, {.native = fn}})

// Forward declarations
typedef struct Module Module;
extern void module_export(Module* module, const char* name, TaggedValue value);
```

### Native Function Signature

All native functions must have this signature:

```c
static TaggedValue function_name(int arg_count, TaggedValue* args) {
    // Function implementation
}
```

### Module Initialization

Every native module must export a `swiftlang_module_init` function:

```c
bool swiftlang_module_init(Module* module) {
    // Register your functions
    module_export(module, "functionName", NATIVE_VAL(function_name));
    return true;
}
```

## Complete Example

Here's a complete native module implementing time functions:

```c
// time_native.c
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

// [Include type definitions from above]

// Get current timestamp
static TaggedValue native_time_now(int arg_count, TaggedValue* args) {
    (void)arg_count;  // Unused
    (void)args;       // Unused
    
    time_t current_time = time(NULL);
    return NUMBER_VAL((double)current_time);
}

// Format current time as string
static TaggedValue native_time_format(int arg_count, TaggedValue* args) {
    time_t current_time = time(NULL);
    struct tm* time_info = localtime(&current_time);
    
    static char buffer[64];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", time_info);
    
    return STRING_VAL(buffer);
}

// Sleep for specified seconds
static TaggedValue native_time_sleep(int arg_count, TaggedValue* args) {
    if (arg_count < 1 || args[0].type != VAL_NUMBER) {
        return NIL_VAL;
    }
    
    int seconds = (int)args[0].as.number;
    sleep(seconds);
    
    return NUMBER_VAL((double)seconds);
}

// Module initialization
bool swiftlang_module_init(Module* module) {
    module_export(module, "now", NATIVE_VAL(native_time_now));
    module_export(module, "format", NATIVE_VAL(native_time_format));
    module_export(module, "sleep", NATIVE_VAL(native_time_sleep));
    return true;
}
```

## Building Native Modules

### Linux
```bash
gcc -shared -fPIC -o module_name.so module_name.c -ldl
```

### macOS
```bash
gcc -shared -fPIC -o module_name.dylib module_name.c
```

## Using Native Modules

### Import with $ prefix
```swift
// Import specific functions from native module
import { now, format } from $time

let timestamp = now()
print("Time: ${format()}")
```

### Import entire module
```swift
// Import entire native module
import $time

let timestamp = time.now()
print("Time: ${time.format()}")
```

### Import with @ prefix (local modules)
```swift
// Import from local module directory
import { now, format } from @time
```

## Module Search Paths

Native modules are searched in the following order:

1. Current directory
2. `./modules` directory
3. Paths specified with `-M` flag
4. Paths in `SWIFTLANG_MODULE_PATH` environment variable
5. System paths (`/usr/local/lib/swiftlang/modules`)
6. User paths (`~/.swiftlang/modules`)

### Environment Variable

Set multiple search paths using colon-separated values (semicolon on Windows):

```bash
export SWIFTLANG_MODULE_PATH=/path/to/modules:/another/path
```

## Best Practices

1. **Error Handling**: Always validate arguments before use
2. **Memory Management**: Be careful with string allocations
3. **Thread Safety**: Native functions should be thread-safe
4. **Documentation**: Document expected arguments and return values
5. **Versioning**: Use semantic versioning in module.json

## Debugging

Enable debug output to see module loading details:

```bash
SWIFTLANG_DEBUG=1 swiftlang your_script.swift
```

## Common Issues

### Module not found
- Check that the module is in a search path
- Verify the shared library has the correct name
- Use `SWIFTLANG_DEBUG=1` to see where modules are being searched

### Undefined symbols
- Ensure the main executable was built with `-Wl,-export-dynamic`
- Check that all required functions are properly exported

### Wrong function signature
- Native functions must match the exact signature
- Return types must be valid TaggedValue instances
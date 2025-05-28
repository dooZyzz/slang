# Module System Demo

This example demonstrates the SwiftLang module system with proper `.swiftmodule` archives.

## What This Example Shows

1. **Module Building**: Modules are compiled into `.swiftmodule` archives using `swiftlang build`
2. **Module Imports**: Using `import` statements to load compiled modules
3. **Module Exports**: How to export functions, structs, and variables from a module
4. **Namespace Access**: Accessing module members via dot notation (e.g., `hello.greet()`)

## Modules Used

### hello
A simple greeting module that exports:
- `greet(name)` - Function to greet someone
- `MESSAGE` - A constant string

### time  
A time utilities module that exports:
- `Time` - Struct for time representation
- `Duration` - Struct for time intervals
- `Timer` - Struct for timing operations
- `sleep(ms)` - Function to sleep (mocked)

## Running the Example

1. Build the modules:
```bash
cd modules/hello && ../../cmake-build-debug/swiftlang build
cd modules/time && ../../cmake-build-debug/swiftlang build
```

2. Run the example:
```bash
cd examples/09-module-demo
../../cmake-build-debug/swiftlang run simple.swift
```

## Key Implementation Details

- Modules are built into ZIP archives containing bytecode and metadata
- The `module.json` file defines module structure with `sources` array
- Exports are captured during module execution and stored in the module object
- The VM sets `current_module` context during module loading for proper export handling
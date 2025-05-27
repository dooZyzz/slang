# SwiftLang Modules Example

This example demonstrates SwiftLang's module system including:
- Creating modules with exports
- Importing entire modules
- Selective imports
- Public vs private module members
- Module organization

## Project Structure

```
02-modules/
├── module.json      # Module configuration
├── main.swift       # Main entry point
├── math_utils.swift # Math utility module
└── string_utils.swift # String utility module
```

## Building and Running

### Current Status

⚠️ **Multi-file module imports are not working yet.** The import system cannot resolve functions from other files at runtime.

### Working Version (Single File)

Use the simplified single-file version that demonstrates the concepts:

```bash
# From project root
./cmake-build-debug/swiftlang examples/02-modules/main_simple.swift

# Or navigate to the directory
cd examples/02-modules
../../cmake-build-debug/swiftlang main_simple.swift
```

### Not Working Yet (Multi-File)

The following commands will create build artifacts but fail at runtime:

```bash
# This builds successfully but...
cd examples/02-modules
../../cmake-build-debug/swiftlang build

# ...this fails with "Undefined variable 'square'" errors
../../cmake-build-debug/swiftlang run
```

The error occurs because the VM cannot resolve imports from other files.

## Module System Features

### Exporting from a Module

```swift
// Export functions
export func square(x: Number) -> Number {
    return x * x
}

// Export constants
export let PI = 3.14159

// Private function (not exported)
func helper() {
    // Only accessible within this module
}
```

### Importing Modules

```swift
// Import all exports
import math_utils

// Selective imports
import { join, repeat } from string_utils

// Use imported items
let result = square(5)
```

### Module Resolution

SwiftLang looks for modules in the following order:
1. Current directory
2. Module search paths
3. Built-in modules

## Key Concepts

1. **Exports**: Use `export` keyword to make functions, constants, or types available to other modules
2. **Imports**: Use `import` to bring exported items into scope
3. **Privacy**: Items without `export` are private to the module
4. **Selective Imports**: Import only what you need with destructuring syntax

## Expected Output

```
Square of 5: 25
Cube of 3: 27
Factorial of 5: 120
PI: 3.14159265359
E: 2.71828182846
Sum of even numbers: 30
Joined: Hello, SwiftLang, World
Stars: *****
```

## Common Patterns

### Utility Modules
Group related functionality into separate modules:
- `math_utils.swift` - Mathematical operations
- `string_utils.swift` - String manipulation
- `array_utils.swift` - Array operations

### Namespace Management
Use selective imports to avoid namespace pollution:
```swift
import { specific, functions } from large_module
```

### Module Testing
Create test files that import and verify module functionality:
```swift
import math_utils
assert(square(5) == 25)
```
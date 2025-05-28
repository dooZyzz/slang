# SwiftLang Module System Documentation

## Overview

The SwiftLang module system provides a powerful and flexible way to organize, share, and reuse code. Modules can be compiled into efficient `.swiftmodule` archives for fast loading and distribution.

## Module Types

### 1. Source Modules
- Single `.swift` files or directories with `module.json`
- Compiled on-demand and cached
- Example: `math.swift` or `utils/` directory

### 2. Compiled Modules (.swiftmodule)
- ZIP archives containing bytecode and metadata
- Pre-compiled for instant loading
- Built using `swiftlang build` command

### 3. Native Modules
- Dynamic libraries (.dylib/.so) with C API
- High-performance system interfaces
- Seamless integration with SwiftLang

## Import Syntax

### Basic Import
```swift
import modulename
// Access as: modulename.function()
```

### Import with Alias
```swift
import modulename as alias
// Access as: alias.function()
```

### Specific Imports
```swift
import {func1, func2, const1} from modulename
// Direct access: func1(), const1
```

### Import with Aliases
```swift
import {func1 as f1, const1 as c1} from modulename
// Access as: f1(), c1
```

### Import All as Namespace
```swift
import * as namespace from modulename
// Access as: namespace.function()
```

### Import All to Current Scope
```swift
import * from modulename
// Direct access to all exports: function(), constant
```

## Export Syntax

### Named Exports
```swift
func myFunction() { ... }
let MY_CONSTANT = 42

export {myFunction}
export {MY_CONSTANT}
```

### Multiple Exports
```swift
export {func1, func2, const1, const2}
```

## Module Structure

### module.json
```json
{
    "name": "mymodule",
    "version": "1.0.0",
    "description": "Module description",
    "main": "main.swift",
    "type": "module",
    "sources": ["main.swift", "utils.swift"],
    "dependencies": {
        "othermodule": "^1.0.0"
    }
}
```

### Directory Structure
```
mymodule/
├── module.json
├── src/
│   ├── main.swift
│   └── utils.swift
└── build/
    └── mymodule.swiftmodule
```

## Building Modules

### Build Command
```bash
# In module directory
swiftlang build

# Output: build/modulename.swiftmodule
```

### Module Archive Format
- ZIP archive containing:
  - `module.json` - Module metadata
  - `bytecode/*.swiftbc` - Compiled bytecode files

## Module Resolution

### Search Order
1. Current directory
2. Module search paths (`-M` flag or `SWIFTLANG_MODULE_PATH`)
3. `modules/` subdirectory
4. System module paths
5. Global cache (`~/.swiftlang/cache/`)

### Local Imports
```swift
import @./utils/helper  // Relative to current file
import @/lib/common     // Relative to project root
```

## Best Practices

### 1. Module Organization
- Keep modules focused on a single purpose
- Use clear, descriptive names
- Follow semantic versioning

### 2. Export Management
- Only export public APIs
- Use consistent naming conventions
- Document exported functions

### 3. Dependency Management
- Minimize dependencies
- Use specific version constraints
- Avoid circular dependencies

### 4. Performance
- Pre-compile modules for distribution
- Use native modules for performance-critical code
- Leverage module caching

## Advanced Features

### Circular Dependency Detection
The module system automatically detects and prevents circular dependencies:
```
Error: Circular dependency detected: module 'A' is already being loaded
```

### Module Caching
- Compiled modules are cached in `~/.swiftlang/cache/`
- Cache key includes module name and content hash
- Automatic cache invalidation on source changes

### Module Context
- Each module maintains its own global scope
- Functions retain module context when exported
- Module state is preserved between calls

## Example: Complete Module

### math_utils/module.json
```json
{
    "name": "math_utils",
    "version": "2.0.0",
    "description": "Advanced math utilities",
    "main": "src/main.swift",
    "type": "module",
    "sources": [
        "src/main.swift",
        "src/trigonometry.swift",
        "src/statistics.swift"
    ]
}
```

### math_utils/src/main.swift
```swift
// Re-export from submodules
import {sin, cos, tan} from @./trigonometry
import {mean, median, stddev} from @./statistics

// Additional utilities
func degrees_to_radians(degrees) {
    return degrees * PI / 180
}

const PI = 3.14159265359
const E = 2.71828182846

// Export everything
export {
    sin, cos, tan,
    mean, median, stddev,
    degrees_to_radians,
    PI, E
}
```

### Usage
```swift
import math_utils as math

let angle = math.degrees_to_radians(90)
let sine = math.sin(angle)
print("sin(90°) = ${sine}")
```

## Troubleshooting

### Module Not Found
- Check module is in search path
- Verify module.json exists and is valid
- Ensure module is built (for .swiftmodule)

### Export Not Found
- Verify function/constant is exported
- Check for typos in import statements
- Ensure module compiled successfully

### Circular Dependencies
- Review module import structure
- Consider extracting shared code to separate module
- Use dependency injection patterns

## Future Enhancements

- Package registry for module distribution
- Module versioning and resolution
- Hot module reloading for development
- Module documentation generation
- Type definitions for modules
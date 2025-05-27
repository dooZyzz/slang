# SwiftLang Documentation

## Quick Start

```bash
# Build SwiftLang
mkdir cmake-build-debug && cd cmake-build-debug
cmake .. && make -j8

# Run a program
./swiftlang examples/01-basics/main.swift

# Start REPL
./swiftlang repl

# Create new project
./swiftlang new myapp
cd myapp
../swiftlang run
```

## Documentation

- [Language Guide](language.md) - Learn SwiftLang syntax and features
- [CLI Reference](cli.md) - Command-line interface documentation
- [Development](development.md) - For contributors

## Current Status

SwiftLang is ~45% complete. Core features work, but some advanced features are still in development.

### Working ✅
- Basic types, variables, functions
- Control flow (if/else, loops)
- Arrays with map/filter/reduce
- Closures and first-class functions
- String interpolation
- Objects with prototypes
- REPL and file execution

### In Progress 🚧
- Module system (imports don't work yet)
- Extension functions
- Struct syntax
- Type annotations

See [examples/](../examples/) for working code samples.
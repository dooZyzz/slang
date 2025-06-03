# SwiftLang Test Project

This project demonstrates all working features of the SwiftLang programming language.

## Working Features

### 1. Core Language Features
- ✅ Variables (`let` and `var`)
- ✅ Basic types (string, number, boolean, nil)
- ✅ Functions with parameters and return values
- ✅ Recursive functions
- ✅ Closures with parameter capture
- ✅ Higher-order functions
- ✅ Nested functions and closures

### 2. Control Flow
- ✅ If/else statements
- ✅ While loops
- ✅ For-in loops (partial - has runtime issues)
- ✅ Break and continue statements

### 3. Operators
- ✅ Arithmetic: `+`, `-`, `*`, `/`, `%`
- ✅ Comparison: `>`, `<`, `>=`, `<=`, `==`, `!=`
- ✅ Logical: `&&`, `||`, `!`
- ✅ Bitwise: `&`, `|`, `^`, `~`, `<<`, `>>`
- ✅ String concatenation with `+`

### 4. Data Structures
- ✅ Arrays: `[1, 2, 3]`
- ✅ Array methods: `map`, `filter`, `reduce`, `first`, `last`, `length`
- ✅ Objects: `{ key: value }` (parsing only)
- ✅ Structs with fields
- ✅ Extensions for adding methods to types

### 5. Module System
- ✅ Module imports: `import { name } from module`
- ✅ Module exports: `export func`, `export let`
- ✅ Native module support: `import { func } from $native`
- ✅ Module-level variables and functions
- ✅ Cross-module closure captures

### 6. String Features
- ✅ String interpolation: `"Hello, ${name}!"`
- ✅ Multi-line string concatenation
- ✅ Escape sequences

### 7. Advanced Features
- ✅ Function composition
- ✅ Closure captures (upvalues)
- ✅ Lexical scoping
- ✅ First-class functions
- ✅ Anonymous functions (closures)

## Known Limitations

1. **Parser Issues:**
   - Empty closures `{}` parse as empty objects
   - Ternary operator `? :` not implemented
   - Optional chaining `?.` not implemented
   - Nil coalescing `??` not implemented

2. **Runtime Issues:**
   - Some array operations missing VM implementation
   - For-in loops have issues with iterators
   - Objects cannot be created at runtime
   - No mutable closure captures

3. **Module System:**
   - Struct exports not supported
   - Module build system incomplete
   - No circular import detection

## Test Files

- `working_features.swift` - Demonstrates all working features
- `test_native.swift` - Tests native module integration
- `simple_math.swift` - Example module with exports
- `final_test.swift` - Comprehensive integration test

## Running Tests

```bash
# Run a test file
../build/swift_like_lang working_features.swift

# Test native modules (requires time.so)
../build/swift_like_lang test_native.swift

# Test module imports
../build/swift_like_lang test_simple_modules.swift
```

## Building Native Modules

```bash
cd ../modules/time
gcc -shared -fPIC -o time.so src/time_native.c -I../../include
```
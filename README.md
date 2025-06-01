# 🚀 SwiftLang - A Modern Swift-like Programming Language

<p align="center">
  <img src="https://img.shields.io/badge/Language-Swift--like-orange.svg" alt="Language">
  <img src="https://img.shields.io/badge/License-MIT-blue.svg" alt="License">
  <img src="https://img.shields.io/badge/Status-In%20Development-yellow.svg" alt="Status">
  <img src="https://img.shields.io/badge/Platform-macOS%20|%20Linux-lightgrey.svg" alt="Platform">
</p>

A modern, expressive programming language that combines the best features of Swift, Kotlin, and JavaScript. Features a clean syntax, powerful module system with package management, extension functions, native C interop, and functional programming capabilities. Built with C for performance and portability.

## ✨ Features

### 🎯 Core Language Features

- **Swift-like Syntax** - Clean, expressive syntax inspired by Swift
- **Module System** - Organize code with import/export
- **Extension Functions** - Kotlin-style extensions to any type including structs
- **Structs** - Value types with fields and extension methods
- **Closures** - First-class functions with full struct property/method access
- **Type Inference** - Dynamic typing with optional annotations
- **String Interpolation** - `"Hello, ${name}!"`
- **Array Operations** - Built-in `map`, `filter`, `reduce`

### 📦 Project Structure

```
.
├── CMakeLists.txt       # Build configuration
├── examples/            # Example programs (01-basics through 06-stdlib)
├── include/             # Header files
├── modules/             # Built-in modules (math, glfw, opengl)
├── src/                 # Source code
└── tests/               # Unit tests
```

## 🚀 Quick Start

### Building from Source

```bash
# Clone the repository
git clone https://github.com/yourusername/swiftlang.git
cd swiftlang

# Create build directory
mkdir cmake-build-debug && cd cmake-build-debug

# Build with CMake
cmake ..
make -j8

# Run the REPL
./swift_like_lang repl

# Run a program
./swift_like_lang run ../examples/01-basics/main.swift

# Initialize a new module
./swift_like_lang init myproject
cd myproject

# Add dependencies
../swift_like_lang add time
../swift_like_lang add math

# Build the module (creates .swiftmodule)
../swift_like_lang build

# Bundle multiple modules
../swift_like_lang bundle -o myapp.swb module1.swiftmodule module2.swiftmodule

# Install a module
../swift_like_lang install somemodule.swiftmodule
```

### Hello World

```swift
// hello_world.swift
print("Hello, World!")

// With string interpolation
let name = "SwiftLang"
print("Welcome to $name!")
```

## 📚 Language Guide

### Variables and Constants
```swift
var mutable = 10
mutable = 20  // OK

let immutable = 42
// immutable = 0  // Error!
```

### Arrays and Higher-Order Functions
```swift
let numbers = [1, 2, 3, 4, 5]

// Transform with map
let squared = numbers.map({ x in x * x })
print(squared)  // [1, 4, 9, 16, 25]

// Filter elements
let evens = numbers.filter({ x in x % 2 == 0 })
print(evens)  // [2, 4]

// Reduce to single value
let sum = numbers.reduce({ acc, x in acc + x }, 0)
print(sum)  // 15
```

### Closures and Functions
```swift
// Function declaration
func add(a: Int, b: Int) -> Int {
    return a + b
}

// Closure with type inference
let multiply = { x, y in x * y }

// Higher-order function
func twice(f: (Int) -> Int, x: Int) -> Int {
    return f(f(x))
}

let result = twice({ x in x + 1 }, 5)  // 7
```

### Structs and Extension Methods
```swift
// Define a struct with fields
struct Point {
    var x: Number
    var y: Number
}

// Add extension methods
func Point.distanceFromOrigin() {
    return Math.sqrt(this.x * this.x + this.y * this.y)
}

func Point.toString() {
    return "(${this.x}, ${this.y})"
}

// Use structs
let p = Point(x: 3, y: 4)
print(p.toString())           // "(3, 4)"
print(p.distanceFromOrigin()) // 5

// Structs work seamlessly with closures
let points = [Point(x: 1, y: 2), Point(x: 3, y: 4)]
let distances = points.map({ p in p.distanceFromOrigin() })
print(distances)  // [2.236..., 5]
```

### Object-Oriented Programming
```swift
// Object literal
let person = {
    name: "Alice",
    age: 30,
    greet: { print("Hi, I'm ${this.name}!") }
}

person.greet()
```

### Control Flow
```swift
// If/else
if age >= 18 {
    print("Adult")
} else {
    print("Minor")
}

// Loops
for i in [1, 2, 3] {
    print(i)
}

// Java-style for loop
for (var i = 0; i < 10; i++) {
    print(i)
}

// While loop
var count = 0
while count < 5 {
    print(count)
    count++
}
```

### Module System
```swift
// math_utils.swift
export func add(a, b) {
    return a + b
}

export func multiply(a, b) {
    return a * b
}

// main.swift
import { add, multiply } from "./math_utils"

print(add(2, 3))        // 5
print(multiply(4, 5))   // 20

// Import everything
import * as math from "./math_utils"
print(math.add(1, 2))   // 3
```

### Native C Interop
```swift
// time/src/time_native.c
double time_native_getCurrentTime() {
    return (double)time(NULL);
}

// time/src/time.swift
native func getCurrentTime() -> Number

export func formatTime(timestamp) {
    return "Time: $timestamp"
}

// main.swift
import { getCurrentTime, formatTime } from "time"

let now = getCurrentTime()
print(formatTime(now))
```

## 🏗️ Architecture

The language is implemented in clean, modular C:

- **Lexer** - Token scanning with support for UTF-8
- **Parser** - Recursive descent parser generating AST
- **Semantic Analyzer** - Type checking and validation
- **Compiler** - Bytecode generation
- **Virtual Machine** - Stack-based bytecode interpreter
- **Garbage Collector** - Mark-and-sweep collector
- **Standard Library** - Built-in types and functions

## 🧪 Testing

Comprehensive test suite with 100+ unit tests:

```bash
# Run all tests
make test

# Run specific test suite
./array_hof_unit_standalone
```

## 📊 Implementation Status

~75% Complete - Core language and module system fully operational!

### ✅ Implemented
- Basic types (numbers, strings, booleans, arrays, objects)
- Variables, constants, and scoping
- All operators (arithmetic, logical, bitwise)
- Control flow (if/else, while, for-in, for-loop)
- Functions and closures with capture
- Array methods (map, filter, reduce) with full closure support
- String interpolation (Kotlin-style with `$var` and `${expr}`)
- Prototype-based objects
- Structs with fields and constructor syntax
- Extension methods for structs (Kotlin-style)
- Full closure support for struct properties and methods
- Module system with multi-file support
- Package management (init, bundle, install, publish)
- Native C interop with automatic bindings
- Memory-efficient custom allocator system
- Garbage collection

### 🚧 In Progress
- Type annotations and type checking
- Class syntax with inheritance
- Pattern matching for enums

### 📋 Planned
- Enums and pattern matching
- Error handling (try/catch)
- Async/await
- Standard library expansion
- Development tools (debugger, LSP)

See [docs/](docs/) for full documentation.

## 🤝 Contributing

Contributions are welcome! Please feel free to submit a Pull Request. For major changes, please open an issue first to discuss what you would like to change.

### Development Setup

1. Fork the repository
2. Create your feature branch (`git checkout -b feature/amazing-feature`)
3. Write tests for your changes
4. Ensure all tests pass (`make test`)
5. Commit your changes (`git commit -m 'Add amazing feature'`)
6. Push to the branch (`git push origin feature/amazing-feature`)
7. Open a Pull Request

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🙏 Acknowledgments

- Inspired by [Swift](https://swift.org/) programming language
- VM design influenced by [Crafting Interpreters](https://craftinginterpreters.com/)
- Test framework inspired by modern C testing practices

---

<p align="center">
  Made with ❤️ by the SwiftLang team
</p>
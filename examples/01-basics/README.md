# SwiftLang Basics Example

This example demonstrates the fundamental features of SwiftLang including:
- Variables and constants
- Basic data types
- Functions
- Control flow (if/else)
- Loops (for, while)
- Arrays
- String interpolation

## Building and Running

### Using the CLI

From the project root directory:

```bash
# Run directly
./cmake-build-debug/swiftlang run examples/01-basics/main.swift

# Or navigate to the example directory
cd examples/01-basics
../../cmake-build-debug/swiftlang run main.swift

# Run with debug output to see the compilation process
../../cmake-build-debug/swiftlang run --debug-ast main.swift
```

## Code Overview

### Variables and Constants

```swift
let greeting = "Hello!"  // Immutable constant
var counter = 0         // Mutable variable
```

### Functions

```swift
func add(a: Number, b: Number) -> Number {
    return a + b
}
```

### Control Flow

```swift
if condition {
    // code
} else {
    // code
}
```

### Loops

```swift
// For-in loop
for item in array {
    print(item)
}

// While loop
while condition {
    // code
}
```

### Arrays

```swift
let fruits = ["apple", "banana"]
fruits.push("orange")
print(fruits.length)
```

### String Interpolation

```swift
let name = "World"
print("Hello, ${name}!")
```

## Expected Output

```
Hello, SwiftLang!
Number: 42
Pi: 3.14159
Ready: true
5 + 3 = 8
Hello, World!
Result is greater than 5

Counting from 1 to 5:
  Count: 1
  Count: 2
  Count: 3
  Count: 4
  Count: 5

While loop:
  Countdown: 3
  Countdown: 2
  Countdown: 1

Fruits:
  - apple
  - banana
  - orange
Number of fruits: 3
After adding grape: 4 fruits

Welcome to SwiftLang version 1!
```
# SwiftLang Standard Library Example

This example demonstrates the usage of SwiftLang's standard library:
- Math module functions
- Array operations and utilities
- String manipulation
- I/O operations
- Type checking and assertions
- Practical data processing

## Building and Running

```bash
cd examples/06-stdlib
../../cmake-build-debug/swiftlang run

# With verbose output
../../cmake-build-debug/swiftlang run --verbose
```

## Standard Library Modules

### Math Module

```swift
import math

sin(angle)     // Sine function
cos(angle)     // Cosine function
sqrt(x)        // Square root
pow(x, y)      // x to the power of y
abs(x)         // Absolute value
max(a, b)      // Maximum of two values
min(a, b)      // Minimum of two values
```

### Array Module

```swift
import array

range(end)        // Creates [0, 1, ..., end-1]
range(start, end) // Creates [start, start+1, ..., end-1]
```

### I/O Module

```swift
import io

print(...)        // Output to stdout
readLine(prompt)  // Read line from stdin
```

## Built-in Functions

### Type Operations

```swift
typeof(value)     // Returns type as string
assert(condition) // Throws if condition is false
```

### Array Methods

```swift
array.length           // Array size
array.push(item)      // Add to end
array.pop()           // Remove from end
array.map(fn)         // Transform elements
array.filter(fn)      // Select elements
array.reduce(init, fn) // Combine elements
```

### String Methods

```swift
string.length         // String length
string.toUpperCase()  // Convert to uppercase
string.toLowerCase()  // Convert to lowercase
string[index]         // Character at index
```

## Expected Output

```
=== Math Module ===
Sin(PI/2): 1
Cos(0): 1
Sqrt(16): 4
Pow(2, 8): 256
Abs(-42): 42
Max(10, 20): 20
Min(10, 20): 10

=== Array Operations ===
Range 1-10: [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
Doubled: [2, 4, 6, 8, 10, 12, 14, 16, 18, 20]
Evens: [2, 4, 6, 8, 10]
Sum: 55

=== String Operations ===
Text: Hello, SwiftLang!
Length: 17
Uppercase: HELLO, SWIFTLANG!
Lowercase: hello, swiftlang!
Welcome Developer to SwiftLang v1

=== I/O Operations ===
This is standard output

=== Nil Handling ===
Maybe value: nil
Definite value: 42
Nil coalescing result: 100

=== Type Checking ===
Type of 42: number
Type of 'hello': string
Type of true: boolean
Type of [1,2,3]: array
Type of nil: nil

=== Assertions ===
Basic math assertion passed
Array length assertion passed

=== Practical Example: Grade Processing ===
Grades: [85, 92, 78, 95, 88, 76, 90]
Average: 86.28571428571429
Passing grades (>=80): [85, 92, 95, 88, 90]
Pass rate: 71.42857142857143 %
Letter grades: ["B", "A", "C", "A", "B", "C", "A"]
```

## Common Patterns

### Data Transformation Pipeline

```swift
let result = data
    .filter((x) => x > 0)
    .map((x) => x * 2)
    .reduce(0, (sum, x) => sum + x)
```

### Safe Nil Handling

```swift
let value = getUserInput() ?? "default"
```

### Mathematical Computations

```swift
import math

func distance(x1, y1, x2, y2) {
    let dx = x2 - x1
    let dy = y2 - y1
    return sqrt(dx * dx + dy * dy)
}
```

### Validation with Assertions

```swift
func divide(a, b) {
    assert(b != 0)
    return a / b
}
```

## Available Modules

- **math** - Mathematical functions
- **array** - Array utilities
- **string** - String manipulation
- **io** - Input/output operations
- **fs** - File system operations (native)
- **net** - Networking (native)
- **sys** - System operations (native)
# SwiftLang Language Guide

SwiftLang combines Swift-like syntax with dynamic typing and functional programming features.

## Basic Types

```swift
// Numbers
let integer = 42
let float = 3.14

// Strings  
let name = "SwiftLang"

// Booleans
let ready = true
let done = false

// Nil
let nothing = nil

// Arrays
let numbers = [1, 2, 3, 4, 5]

// Objects
let person = {
    name: "Alice",
    age: 30
}
```

## Variables and Constants

```swift
// Mutable variable
var count = 0
count = count + 1

// Immutable constant
let pi = 3.14159
// pi = 3.14  // Error! Cannot reassign constant
```

## String Interpolation

```swift
let name = "World"
print("Hello, ${name}!")  // Use ${} for interpolation

let x = 5
print("The answer is ${x * 2}")

// For literal $ in strings, place it outside interpolation
print("Price: $" + price)
```

## Functions

```swift
// Function declaration
func greet(name) {
    print("Hello, ${name}!")
}

// With return value
func add(a, b) {
    return a + b
}

// With typed parameters (optional)
func multiply(x: Number, y: Number) -> Number {
    return x * y
}

// Call functions
greet("Alice")
let sum = add(5, 3)
```

## Closures

```swift
// Closure syntax with 'in'
let double = { x in x * x }
print(double(5))  // 25

// Arrow function syntax
let triple = (x) => x * 3
let add = (a, b) => a + b

// Closures as arguments
let numbers = [1, 2, 3, 4, 5]
let squared = numbers.map({ x in x * x })
let doubled = numbers.map((x) => x * 2)

// Multi-line closures
numbers.forEach({ num in
    print("Number: ${num}")
    print("Squared: ${num * num}")
})
```

## Control Flow

```swift
// If/else
if age >= 18 {
    print("Adult")
} else {
    print("Minor")
}

// While loop
while count < 10 {
    print(count)
    count = count + 1  // Note: ++ operator not supported
}

// For-in loop with arrays
for item in items {
    print(item)
}

// For-in loop with range
for i in 0..<10 {
    print(i)
}
```

## Structs

Structs provide value-type data structures with fields and methods.

```swift
// Struct definition
struct Point {
    var x: Number  // Fields must use 'var' or 'let'
    var y: Number
}

// Create struct instance (use parentheses, not braces)
let p = Point(x: 10, y: 20)

// Access fields
print("X: ${p.x}, Y: ${p.y}")

// Struct with nested structs
struct Person {
    var name: String
    var age: Number
}

struct Employee {
    var person: Person
    var salary: Number
}

let emp = Employee(
    person: Person(name: "Alice", age: 30),
    salary: 50000
)

// Structs work seamlessly with closures and array methods
let points = [Point(x: 1, y: 2), Point(x: 3, y: 4), Point(x: 5, y: 6)]

// Access struct properties in closures
let xValues = points.map({ p in p.x })
print(xValues)  // [1, 3, 5]

// Filter based on struct properties
let farPoints = points.filter({ p in p.x > 2 })
print(farPoints.length)  // 2
```

## Extension Methods

Add methods to existing types using Kotlin-style syntax:

```swift
// Extend built-in types
func Number.squared() {
    return this * this
}

func String.shout() {
    return this + "!"
}

// Use extensions
let n = 5
print(n.squared())  // 25

let msg = "Hello"
print(msg.shout())  // Hello!

// Extend structs
struct Dog {
    var name: String
    var loudness: Number
}

func Dog.bark() {
    // Extension methods can access all struct fields via 'this'
    let barks = "Woof " * this.loudness
    print("${this.name} says: ${barks}!")
}

func Dog.description() {
    return "${this.name} (loudness: ${this.loudness})"
}

let dog = Dog(name: "Buddy", loudness: 3)
dog.bark()  // Buddy says: Woof Woof Woof!

// Extension methods can take parameters
func Point.translate(dx, dy) {
    return Point(x: this.x + dx, y: this.y + dy)
}

func Point.distanceTo(other) {
    let dx = this.x - other.x
    let dy = this.y - other.y
    return Math.sqrt(dx * dx + dy * dy)
}

// Method chaining
let p1 = Point(x: 1, y: 1)
let p2 = p1.translate(2, 3).translate(-1, -1)

// Extension methods work in closures
let dogs = [Dog(name: "Rex", loudness: 2), Dog(name: "Max", loudness: 4)]
let descriptions = dogs.map({ d in d.description() })
print(descriptions)  // [Rex (loudness: 2), Max (loudness: 4)]

// Can even call methods inside closures
dogs.map({ d in d.bark() })  // Each dog barks!
```

## Arrays

```swift
// Create arrays
let numbers = [1, 2, 3, 4, 5]
let mixed = [1, "two", true, nil]

// Access elements
let first = numbers[0]
let last = numbers[numbers.length - 1]

// Array methods (via extensions)
func Array.isEmpty() {
    return this.length == 0
}

func Array.first() {
    if this.length > 0 {
        return this[0]
    }
    return nil
}

// Built-in array methods
let doubled = numbers.map({ x in x * 2 })
let filtered = numbers.filter({ x in x > 2 })
let sum = numbers.reduce(0, { acc, x in acc + x })
```

## Objects

Objects are dynamic dictionaries with prototype-based inheritance.

```swift
// Create object
let obj = {
    name: "Example",
    value: 42,
    method: func() {
        print("Hello from ${this.name}")
    }
}

// Access properties
print(obj.name)
obj.value = 100

// Call methods
obj.method()

// Add properties dynamically
obj.newProp = "Dynamic"
```

## Modules

```swift
// Import modules
import math
import { sin, cos } from math
import utils as u

// Use imported functions
let angle = math.PI / 2
let result = sin(angle)

// Export from modules
export func myFunction() {
    // ...
}

export let constant = 42
```

## Error Handling

```swift
// Currently, errors are runtime errors
// Future versions may add try/catch support

// Check for errors manually
func divide(a, b) {
    if b == 0 {
        print("Error: Division by zero")
        return nil
    }
    return a / b
}

let result = divide(10, 0)
if result == nil {
    print("Operation failed")
}
```

## Comments

```swift
// Single-line comment

/*
   Multi-line
   comment
*/
```

## Best Practices

1. **Use `let` for constants, `var` for variables**
   ```swift
   let pi = 3.14159  // Won't change
   var count = 0     // Will be modified
   ```

2. **Prefer string interpolation over concatenation**
   ```swift
   // Good
   print("Hello, ${name}!")
   
   // Less readable
   print("Hello, " + name + "!")
   ```

3. **Use meaningful names**
   ```swift
   // Good
   func calculateArea(width, height) {
       return width * height
   }
   
   // Bad
   func calc(w, h) {
       return w * h
   }
   ```

4. **Keep functions small and focused**
   ```swift
   // Each function does one thing
   func validateInput(input) { ... }
   func processData(data) { ... }
   func saveResult(result) { ... }
   ```

## Known Limitations

1. **Struct Syntax**
   - Fields must use `var` or `let` keywords
   - Initialization uses parentheses: `Point(x: 1, y: 2)`
   - No struct inheritance

2. **String Interpolation**
   - Use `${}` syntax, not `$variable`
   - For literal `$`, place outside interpolation

3. **Operators**
   - No `++` or `--` operators (use `+= 1` or `-= 1`)
   - No ternary operator (use if/else)

4. **Type System**
   - Currently dynamically typed
   - Type annotations are parsed but not enforced
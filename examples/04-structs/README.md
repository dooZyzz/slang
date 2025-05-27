# Structs Example

This example demonstrates SwiftLang's struct feature for creating value-type data structures.

## What are Structs?

Structs are value types that bundle data and functionality together. They are:
- Lightweight data containers
- Passed by value (copied when assigned)
- Can have fields and methods (via extensions)
- Similar to structs in Swift, Rust, and C

## Syntax

### Struct Definition
```swift
struct StructName {
    var field1: Type  // Fields MUST use 'var' or 'let'
    var field2: Type
}
```

### Creating Instances
```swift
// Use parentheses, NOT curly braces
let instance = StructName(field1: value1, field2: value2)

// Multi-line initialization
let instance = StructName(
    field1: value1,
    field2: value2
)
```

### Adding Methods via Extensions
```swift
func StructName.methodName(params) {
    // Use 'this' to access instance fields
    return this.field1 + this.field2
}
```

## Examples in this Directory

### main.swift
Comprehensive struct demonstrations including:
- Basic struct definition and usage
- Extension methods for structs
- Struct composition (structs containing other structs)
- Arrays of structs
- Method chaining
- Real-world examples (BankAccount, Employee, etc.)
- **NEW**: Structs with closures and array methods (map, filter)
- **NEW**: String interpolation working in closures
- **NEW**: Complex nested property access in closures
- **NEW**: Method calls from within closures

### simple.swift
Minimal example with Point and Person structs.

## Running the Examples

```bash
# From project root
./cmake-build-debug/swiftlang run examples/04-structs/main.swift
./cmake-build-debug/swiftlang run examples/04-structs/simple.swift
```

## Key Features

### 1. Field Declaration
Fields MUST be declared with `var` (mutable) or `let` (immutable):
```swift
struct Point {
    var x: Number  // Mutable field
    var y: Number  // Mutable field
}

struct Config {
    let version: String  // Immutable field
    var debug: Boolean   // Mutable field
}
```

### 2. Field Access
Access fields using dot notation:
```swift
let p = Point(x: 10, y: 20)
print(p.x)  // 10
p.x = 30    // OK if declared with 'var'
```

### 3. Extension Methods
Add behavior to structs using Kotlin-style syntax:
```swift
struct Point {
    var x: Number
    var y: Number
}

func Point.distanceFromOrigin() {
    return this.x * this.x + this.y * this.y
}

func Point.display() {
    print("Point(${this.x}, ${this.y})")
}

// Usage
let p = Point(x: 3, y: 4)
p.display()                    // Point(3, 4)
print(p.distanceFromOrigin())  // 25
```

### 4. Struct Composition
Structs can contain other structs:
```swift
struct Address {
    var street: String
    var city: String
}

struct Person {
    var name: String
    var address: Address
}

let person = Person(
    name: "Alice",
    address: Address(street: "123 Main", city: "NYC")
)

print(person.address.city)  // NYC
```

### 5. Method Chaining
Return new instances for fluent APIs:
```swift
func Point.translate(dx, dy) {
    return Point(x: this.x + dx, y: this.y + dy)
}

func Point.scale(factor) {
    return Point(x: this.x * factor, y: this.y * factor)
}

// Chain multiple operations
let p2 = p.translate(1, 1).scale(2).translate(-2, -2)
```

### 6. Structs with Closures and Array Methods
Structs work seamlessly with closures and functional programming:
```swift
struct Dog {
    var name: String
    var loudness: Number
}

func Dog.description() {
    return "${this.name} (loudness: ${this.loudness})"
}

let dogs = [
    Dog(name: "Rex", loudness: 2),
    Dog(name: "Max", loudness: 4),
    Dog(name: "Tiny", loudness: 1)
]

// Map struct properties
let names = dogs.map({ d in d.name })
print(names)  // [Rex, Max, Tiny]

// Filter based on properties
let loudDogs = dogs.filter({ d in d.loudness > 2 })
print("Loud dogs: ${loudDogs.length}")  // Loud dogs: 1

// Call methods in closures
let descriptions = dogs.map({ d in d.description() })
print(descriptions)  // [Rex (loudness: 2), Max (loudness: 4), Tiny (loudness: 1)]

// String interpolation works in closures
let announcements = dogs.map({ d in 
    "${d.name} barks ${d.loudness} times!"
})
```

## Working Example

```swift
// Define a struct
struct Rectangle {
    var width: Number
    var height: Number
}

// Add methods via extensions
func Rectangle.area() {
    return this.width * this.height
}

func Rectangle.perimeter() {
    return 2 * (this.width + this.height)
}

func Rectangle.isSquare() {
    return this.width == this.height
}

func Rectangle.display() {
    print("Rectangle ${this.width}x${this.height}")
}

// Usage
let rect = Rectangle(width: 10, height: 5)
rect.display()                  // Rectangle 10x5
print("Area:", rect.area())     // Area: 50
print("Square?", rect.isSquare()) // Square? false
```

## Common Patterns

### Builder Pattern
```swift
struct Config {
    var host: String
    var port: Number
    var debug: Boolean
}

func Config.withHost(host) {
    return Config(host: host, port: this.port, debug: this.debug)
}

func Config.withPort(port) {
    return Config(host: this.host, port: port, debug: this.debug)
}

// Usage
let config = Config(host: "localhost", port: 80, debug: false)
    .withPort(8080)
    .withHost("example.com")
```

### Data Validation
```swift
struct Email {
    var address: String
}

func Email.isValid() {
    // Simple validation
    return this.address.length > 0 && 
           this.address.contains("@") &&
           this.address.contains(".")
}

let email = Email(address: "user@example.com")
if email.isValid() {
    print("Valid email")
}
```

## Important Notes

### Syntax Differences from Swift

1. **Field declarations require `var`/`let`**:
   ```swift
   // SwiftLang - REQUIRED
   struct Point {
       var x: Number
       var y: Number
   }
   
   // NOT ALLOWED (will error)
   struct Point {
       x: Number
       y: Number
   }
   ```

2. **Initialization uses parentheses**:
   ```swift
   // Correct
   let p = Point(x: 10, y: 20)
   
   // WRONG (will error)
   let p = Point { x: 10, y: 20 }
   ```

3. **Methods via extensions only**:
   ```swift
   // Correct - extension method
   func Point.distance() { ... }
   
   // NOT SUPPORTED - methods inside struct
   struct Point {
       func distance() { ... }  // Won't work
   }
   ```

4. **String interpolation uses `${}`**:
   ```swift
   print("Point at ${p.x}, ${p.y}")  // Correct
   print("Point at $p.x, $p.y")      // Wrong
   ```

## Best Practices

1. **Use structs for simple data**: Points, colors, configurations
2. **Keep structs small**: Prefer composition over large structs
3. **Use meaningful names**: `Point` not `p`, `isValid()` not `check()`
4. **Add validation methods**: Ensure data integrity
5. **Return new instances**: Structs are values, don't mutate in-place for transformations

## Limitations

- No struct inheritance
- No private fields (all fields are public)
- No custom initializers (always uses field names)
- No default field values (must provide all values)
- No methods inside struct body (use extensions)
- Fields must use `var` or `let` keywords

## Troubleshooting

**Error: "Only variable declarations are allowed in structs"**
- Make sure all fields use `var` or `let`

**Error: "Expect expression" with struct initialization**
- Use parentheses `()` not braces `{}` for initialization

**Error: "Unknown property"**
- Check field names are spelled correctly
- Ensure struct instance is created properly

**String interpolation issues**
- Use `${}` not `$` for interpolation
- For literal `$`, put it outside: `"Price: $" + amount`
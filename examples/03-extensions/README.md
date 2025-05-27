# Extension Methods Example

This example demonstrates SwiftLang's extension method feature, which allows you to add methods to existing types using Kotlin-style syntax.

## What are Extension Methods?

Extension methods let you add new functionality to existing types without modifying their original definition. This is particularly useful for:
- Adding utility methods to built-in types (Number, String, Array)
- Adding methods to structs defined elsewhere
- Creating a more fluent, readable API

## Syntax

```swift
// Kotlin-style syntax (what SwiftLang uses)
func TypeName.methodName(parameters) {
    // Use 'this' to refer to the instance
    return this.someProperty
}

// Usage
instance.methodName(args)
```

## Examples in this Directory

### main.swift
Full demonstration of extension methods including:
- Number extensions: `squared()`, `cubed()`, `isEven()`, `isOdd()`, `abs()`, `times()`
- String extensions: `shout()`, `whisper()`, `repeat()`
- Array extensions: `isEmpty()`, `first()`, `last()`
- Struct extensions for Point, Rectangle, and Dog
- Method chaining examples

### simple.swift
Minimal example showing basic Number and String extensions.

## Running the Examples

```bash
# From project root
./cmake-build-debug/swiftlang run examples/03-extensions/main.swift
./cmake-build-debug/swiftlang run examples/03-extensions/simple.swift
```

## Key Concepts

1. **The `this` keyword**: Inside an extension method, `this` refers to the instance the method is called on.
   ```swift
   func String.shout() {
       return this + "!"
   }
   ```

2. **Method chaining**: Extension methods return values, allowing you to chain multiple calls:
   ```swift
   let result = point.translate(2, 3).scale(2).translate(-1, -1)
   ```

3. **Extending primitives**: You can add methods to Number, String, and Array types:
   ```swift
   let n = 5
   print(n.squared())  // 25
   ```

4. **Struct extensions**: Add methods to your custom structs:
   ```swift
   struct Dog {
       var name: String
   }
   
   func Dog.bark() {
       print("${this.name} says: Woof!")
   }
   ```

## Working Examples

### Number Extensions
```swift
func Number.squared() {
    return this * this
}

func Number.times(action) {
    var i = 0
    while i < this {
        action(i)
        i = i + 1
    }
}

// Usage
print(5.squared())  // 25
3.times({ i in print("Count: ${i}") })
```

### String Extensions
```swift
func String.repeat(times) {
    var result = ""
    var i = 0
    while i < times {
        result = result + this
        i = i + 1
    }
    return result
}

// Usage
print("Hi".repeat(3))  // HiHiHi
```

### Struct Extensions
```swift
struct Point {
    var x: Number
    var y: Number
}

func Point.distanceSquared() {
    return this.x * this.x + this.y * this.y
}

func Point.translate(dx, dy) {
    return Point(x: this.x + dx, y: this.y + dy)
}

// Usage
let p = Point(x: 3, y: 4)
print(p.distanceSquared())  // 25
let p2 = p.translate(1, 1)  // Point(x: 4, y: 5)
```

## Implementation Notes

- Extension methods are resolved at compile time based on the type name
- Methods are added to the type's prototype in the VM
- Built-in types (Number, String, Array) have their own prototypes
- Each struct type gets its own prototype for methods
- The compiler automatically adds `this` as the first parameter for extension methods

## Common Patterns

### Fluent API
```swift
func Rectangle.withWidth(w) {
    return Rectangle(width: w, height: this.height)
}

func Rectangle.withHeight(h) {
    return Rectangle(width: this.width, height: h)
}

// Usage
let rect = Rectangle(width: 10, height: 20)
    .withWidth(15)
    .withHeight(30)
```

### Utility Methods
```swift
func Array.isEmpty() {
    return this.length == 0
}

func String.shout() {
    return this + "!"
}
```

## Limitations

- Cannot add properties, only methods
- Cannot override existing methods
- Type must exist before adding extensions
- No access to private fields (all struct fields are public)
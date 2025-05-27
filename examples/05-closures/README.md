# SwiftLang Closures Example

This example demonstrates closures and functional programming in SwiftLang:
- Closure syntax and types
- Higher-order functions
- Array methods (map, filter, reduce)
- Lexical scope and capturing
- Practical closure usage

## Building and Running

```bash
cd examples/05-closures
../../cmake-build-debug/swiftlang run

# With debug output
../../cmake-build-debug/swiftlang run --debug-trace
```

## Closure Syntax

### Basic Closure

```swift
let add = (a: Number, b: Number) -> Number => {
    return a + b
}
```

### Single Expression (Implicit Return)

```swift
let multiply = (a: Number, b: Number) -> Number => a * b
```

### Anonymous Closures

```swift
calculate(10, 5, (x, y) => x + y)
```

## Higher-Order Functions

Functions that take or return other functions:

```swift
func calculate(a: Number, b: Number, operation: (Number, Number) -> Number) -> Number {
    return operation(a, b)
}
```

## Array Methods

### map - Transform Elements

```swift
let doubled = [1, 2, 3].map((x) => x * 2)
// Result: [2, 4, 6]
```

### filter - Select Elements

```swift
let evens = [1, 2, 3, 4].filter((x) => x % 2 == 0)
// Result: [2, 4]
```

### reduce - Combine Elements

```swift
let sum = [1, 2, 3, 4].reduce(0, (acc, x) => acc + x)
// Result: 10
```

## Closure Capturing

Closures capture variables from their enclosing scope:

```swift
func makeCounter() -> () -> Number {
    var count = 0
    return () => {
        count = count + 1
        return count
    }
}
```

## Expected Output

```
Add closure: 5 + 3 = 8
Multiply closure: 4 * 7 = 28
Double of 6 = 12
Calculate with add: 10 + 5 = 15
Calculate with multiply: 10 * 5 = 50
Calculate with subtract: 10 - 5 = 5

Original: [1, 2, 3, 4, 5]
Doubled: [2, 4, 6, 8, 10]
Even numbers: [2, 4]
Sum of numbers: 15

Counters:
Counter1: 1
Counter1: 2
Counter2: 1
Counter1: 3
Counter2: 2

Original words: ["banana", "apple", "cherry", "date"]
Sorted words: ["apple", "banana", "cherry", "date"]

Sum of squares of even numbers from 1-10: 220
```

## Common Patterns

### Callback Functions

```swift
func fetchData(callback: (String) -> Void) {
    // Simulate async operation
    let data = "Result"
    callback(data)
}

fetchData((result) => {
    print("Got:", result)
})
```

### Function Composition

```swift
let addOne = (x) => x + 1
let double = (x) => x * 2
let addOneThenDouble = (x) => double(addOne(x))
```

### Partial Application

```swift
func multiply(x: Number) -> (Number) -> Number {
    return (y) => x * y
}

let multiplyBy2 = multiply(2)
print(multiplyBy2(5))  // 10
```
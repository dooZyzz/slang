// Closures and Higher-Order Functions in SwiftLang

// Basic closure syntax
let add = (a: Number, b: Number) -> Number => {
    return a + b
}

print("Add closure: 5 + 3 =", add(5, 3))

// Single expression closures (implicit return)
let multiply = (a: Number, b: Number) -> Number => a * b
print("Multiply closure: 4 * 7 =", multiply(4, 7))

// Closures with single parameter
let double = (x: Number) -> Number => x * 2
print("Double of 6 =", double(6))

// Closures as function parameters
func calculate(a: Number, b: Number, operation: (Number, Number) -> Number) -> Number {
    return operation(a, b)
}

let result1 = calculate(10, 5, add)
let result2 = calculate(10, 5, multiply)
print("Calculate with add: 10 + 5 =", result1)
print("Calculate with multiply: 10 * 5 =", result2)

// Anonymous closures
let result3 = calculate(10, 5, (x, y) => x - y)
print("Calculate with subtract: 10 - 5 =", result3)

// Higher-order functions with arrays
let numbers = [1, 2, 3, 4, 5]

// Map - transform each element
let doubled = numbers.map((x) => x * 2)
print("\nOriginal:", numbers)
print("Doubled:", doubled)

// Filter - select elements matching condition
let evens = numbers.filter((x) => x % 2 == 0)
print("Even numbers:", evens)

// Reduce - combine elements into single value
let sum = numbers.reduce(0, (acc, x) => acc + x)
print("Sum of numbers:", sum)

// Closure capturing variables (lexical scope)
func makeCounter() -> () -> Number {
    var count = 0
    return () => {
        count = count + 1
        return count
    }
}

let counter1 = makeCounter()
let counter2 = makeCounter()

print("\nCounters:")
print("Counter1:", counter1())  // 1
print("Counter1:", counter1())  // 2
print("Counter2:", counter2())  // 1
print("Counter1:", counter1())  // 3
print("Counter2:", counter2())  // 2

// Practical example: Array sorting
let words = ["banana", "apple", "cherry", "date"]
let sorted = words.sort((a, b) => {
    // Simple string comparison (alphabetical)
    if a < b { return -1 }
    if a > b { return 1 }
    return 0
})
print("\nOriginal words:", words)
print("Sorted words:", sorted)

// Chaining operations
let result = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
    .filter((x) => x % 2 == 0)    // Get even numbers
    .map((x) => x * x)             // Square them
    .reduce(0, (acc, x) => acc + x) // Sum them up

print("\nSum of squares of even numbers from 1-10:", result)
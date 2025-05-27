// Standard Library Usage in SwiftLang

// Math module
import math

print("=== Math Module ===")
print("Sin(PI/2):", sin(PI/2))
print("Cos(0):", cos(0))
print("Sqrt(16):", sqrt(16))
print("Pow(2, 8):", pow(2, 8))
print("Abs(-42):", abs(-42))
print("Max(10, 20):", max(10, 20))
print("Min(10, 20):", min(10, 20))

// Array module and built-in array functions
import array

print("\n=== Array Operations ===")
let numbers = range(1, 11)  // [1, 2, 3, ..., 10]
print("Range 1-10:", numbers)

// Array methods (built-in)
let doubled = numbers.map((x) => x * 2)
print("Doubled:", doubled)

let evens = numbers.filter((x) => x % 2 == 0)
print("Evens:", evens)

let sum = numbers.reduce(0, (acc, x) => acc + x)
print("Sum:", sum)

// String operations (built-in)
print("\n=== String Operations ===")
let text = "Hello, SwiftLang!"
print("Text:", text)
print("Length:", text.length)
print("Uppercase:", text.toUpperCase())
print("Lowercase:", text.toLowerCase())

// String interpolation
let name = "Developer"
let version = 1.0
print("Welcome ${name} to SwiftLang v${version}")

// I/O operations
import io

print("\n=== I/O Operations ===")
print("This is standard output")
// readLine() would read from stdin
// let input = readLine("Enter your name: ")

// Working with nil/null values
print("\n=== Nil Handling ===")
let maybeValue = nil
let definiteValue = 42

print("Maybe value:", maybeValue)
print("Definite value:", definiteValue)

// Nil coalescing
let result = maybeValue ?? 100
print("Nil coalescing result:", result)

// Type checking
print("\n=== Type Checking ===")
print("Type of 42:", typeof(42))
print("Type of 'hello':", typeof("hello"))
print("Type of true:", typeof(true))
print("Type of [1,2,3]:", typeof([1,2,3]))
print("Type of nil:", typeof(nil))

// Assertions
print("\n=== Assertions ===")
assert(1 + 1 == 2)
print("Basic math assertion passed")

assert(evens.length == 5)
print("Array length assertion passed")

// Time operations (if available)
// import time
// let now = time.now()
// print("Current time:", now)

// JSON operations (future feature)
// import json
// let data = { name: "SwiftLang", version: 1.0 }
// let jsonStr = json.stringify(data)
// print("JSON:", jsonStr)

// Practical example: Data processing
print("\n=== Practical Example: Grade Processing ===")
let grades = [85, 92, 78, 95, 88, 76, 90]

let average = grades.reduce(0, (sum, grade) => sum + grade) / grades.length
print("Grades:", grades)
print("Average:", average)

let passingGrades = grades.filter((g) => g >= 80)
print("Passing grades (>=80):", passingGrades)
print("Pass rate:", (passingGrades.length / grades.length) * 100, "%")

let letterGrades = grades.map((g) => {
    if g >= 90 { return "A" }
    if g >= 80 { return "B" }
    if g >= 70 { return "C" }
    return "F"
})
print("Letter grades:", letterGrades)
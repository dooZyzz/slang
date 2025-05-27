// Basic SwiftLang Features Demo

// Variables and Constants
let greeting = "Hello, SwiftLang!"
var counter = 0

print(greeting)

// Basic Types
let number = 42
let pi = 3.14159
let isReady = true
let nothing = nil

print("Number:", number)
print("Pi:", pi)
print("Ready:", isReady)

// Functions
func add(a: Number, b: Number) -> Number {
    return a + b
}

func greet(name: String) {
    print("Hello, " + name + "!")
}

// Function calls
let result = add(5, 3)
print("5 + 3 =", result)

greet("World")

// Control Flow
if result > 5 {
    print("Result is greater than 5")
} else {
    print("Result is 5 or less")
}

// Loops
print("\nCounting from 1 to 5:")
for i in [1, 2, 3, 4, 5] {
    print("  Count:", i)
}

// While loop
print("\nWhile loop:")
counter = 3
while counter > 0 {
    print("  Countdown:", counter)
    counter = counter - 1
}

// Arrays
let fruits = ["apple", "banana", "orange"]
print("\nFruits:")
for fruit in fruits {
    print("  -", fruit)
}

// Array methods
print("Number of fruits:", fruits.length)
fruits.push("grape")
print("After adding grape:", fruits.length, "fruits")

// String interpolation
let name = "SwiftLang"
let version = 1.0
print("\nWelcome to ${name} version ${version}!")
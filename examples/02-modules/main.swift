// main.swift - Demonstrates module imports and usage

// Import all exports from math_utils
import {square, cube, factorial, PI, E, sumEvens} from math_utils

// Import specific functions from string_utils
import { join, repeat } from string_utils

// Using imported math functions
print("Square of 5:", square(5))
print("Cube of 3:", cube(3))
print("Factorial of 5:", factorial(5))

// Using imported constants
print("PI:", PI)
print("E:", E)

// Using array function
let numbers = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
print("Sum of even numbers:", sumEvens(numbers))

// Using imported string functions
let words = ["Hello", "SwiftLang", "World"]
print("Joined:", join(words, ", "))

let stars = repeat("*", 5)
print("Stars:", stars)

// Demonstrate that private functions are not accessible
// This would cause an error if uncommented:
// print(isEven(4))  // Error: isEven is not exported
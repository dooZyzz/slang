// Module System Example - Demonstrating import concepts
// Note: Multi-file modules are still in development

// In a real module system, these would be in math_utils.swift
func square(x: Number) -> Number {
    return x * x
}

func cube(x: Number) -> Number {
    return x * x * x
}

func factorial(n: Number) -> Number {
    if n <= 1 {
        return 1
    }
    return n * factorial(n - 1)
}

let PI = 3.14159265359
let E = 2.71828182846

func sumEvens(numbers: Array) -> Number {
    var sum = 0
    for num in numbers {
        if num % 2 == 0 {
            sum = sum + num
        }
    }
    return sum
}

// In a real module system, these would be in string_utils.swift
func strToUpperCase(str: String) -> String {
    return "[UPPER] " + str
}

func strToLowerCase(str: String) -> String {
    return "[lower] " + str
}

func strRepeat(str: String, times: Number) -> String {
    var result = ""
    var i = 0
    while i < times {
        result = result + str
        i = i + 1
    }
    return result
}

func strJoin(strings: Array, separator: String) -> String {
    var result = ""
    var i = 0
    for str in strings {
        if i > 0 {
            result = result + separator
        }
        result = result + str
        i = i + 1
    }
    return result
}

// Main program demonstrating module usage
print("=== Math Functions Demo ===")
print("Square of 5:", square(5))
print("Cube of 3:", cube(3))
print("Factorial of 5:", factorial(5))
print("PI:", PI)
print("E:", E)

let numbers = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
print("Sum of even numbers:", sumEvens(numbers))

print("\n=== String Functions Demo ===")
let words = ["Hello", "SwiftLang", "World"]
print("Joined:", strJoin(words, ", "))

let stars = strRepeat("*", 5)
print("Stars:", stars)

print("Uppercase:", strToUpperCase("hello"))
print("Lowercase:", strToLowerCase("HELLO"))

print("\n=== Module System Benefits ===")
print("When fully implemented, you'll be able to:")
print("- Import functions from other files")
print("- Use 'export' to make functions public")
print("- Organize code into reusable modules")
print("- Avoid naming conflicts with namespaces")
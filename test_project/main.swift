// Comprehensive test of SwiftLang features
import { greet, Calculator } from math_utils
import { formatList, capitalize } from string_utils
import { Person, createTeam } from data_types

print("=== SwiftLang Comprehensive Test Suite ===\n")

// Test 1: Basic variables and types
print("Test 1: Variables and Basic Types")
let name = "SwiftLang"
var version = 1.0
let isAwesome = true
let nothing = nil

print("Language: ${name} v${version}")
print("Is awesome? ${isAwesome}")
print("Nothing value: ${nothing}")

// Test 2: Arrays and operations
print("\nTest 2: Arrays")
let numbers = [1, 2, 3, 4, 5]
let doubled = numbers.map({ x in x * 2 })
let sum = numbers.reduce(0, { acc, x in acc + x })
let evens = numbers.filter({ x in x % 2 == 0 })

print("Original: ${numbers}")
print("Doubled: ${doubled}")
print("Sum: ${sum}")
print("Evens: ${evens}")

// Test 3: Control flow
print("\nTest 3: Control Flow")
for num in numbers {
    if num % 2 == 0 {
        print("${num} is even")
    } else {
        print("${num} is odd")
    }
}

// Test 4: Functions and closures
print("\nTest 4: Functions and Closures")
func makeMultiplier(factor) {
    return { x in x * factor }
}

let triple = makeMultiplier(3)
print("Triple 7: ${triple(7)}")

// Test 5: Imported modules
print("\nTest 5: Module Imports")
greet(name)

let calc = Calculator(10)
print("Calculator starting with 10:")
print("Add 5: ${calc.add(5)}")
print("Multiply by 2: ${calc.multiply(2)}")
print("Result: ${calc.getValue()}")

// Test 6: String utilities
print("\nTest 6: String Utilities")
let items = ["apple", "banana", "cherry"]
print("Formatted list: ${formatList(items)}")
print("Capitalized: ${capitalize("hello world")}")

// Test 7: Structs and data types
print("\nTest 7: Structs")
let person1 = Person("Alice", 30)
let person2 = Person("Bob", 25)
let team = createTeam("Dev Team", [person1, person2])

print("Team: ${team.name}")
print("Members: ${team.members.map({ p in p.name })}")

// Test 8: Optional chaining and nil coalescing
print("\nTest 8: Optionals")
let optional = person1
let safeName = optional?.name ?? "Unknown"
print("Safe name: ${safeName}")

// Test 9: Ternary and operators
print("\nTest 9: Operators")
let max = 10 > 5 ? 10 : 5
let bits = 5 & 3
let shifted = 8 >> 1

print("Max: ${max}")
print("5 & 3 = ${bits}")
print("8 >> 1 = ${shifted}")

// Test 10: String interpolation
print("\nTest 10: String Interpolation")
let multi = "This is a ${name} test " +
           "with version ${version} " +
           "and ${numbers.length} numbers"
print(multi)

print("\n=== All Tests Completed Successfully! ===")
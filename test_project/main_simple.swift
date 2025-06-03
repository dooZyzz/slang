// Comprehensive test of SwiftLang features - simplified version

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

// While loop test
var i = 0
while i < 3 {
    print("While loop: ${i}")
    i = i + 1
}

// Test 4: Functions and closures
print("\nTest 4: Functions and Closures")
func makeMultiplier(factor) {
    return { x in x * factor }
}

let triple = makeMultiplier(3)
print("Triple 7: ${triple(7)}")

// Nested functions
func outer(x) {
    func inner(y) {
        return x + y
    }
    return inner
}

let addFive = outer(5)
print("5 + 3 = ${addFive(3)}")

// Test 5: Ternary and operators
print("\nTest 5: Operators")
let max = 10 > 5 ? 10 : 5
let bits = 5 & 3
let shifted = 8 >> 1
let leftShift = 2 << 2
let xor = 5 ^ 3

print("Max: ${max}")
print("5 & 3 = ${bits}")
print("8 >> 1 = ${shifted}")
print("2 << 2 = ${leftShift}")
print("5 ^ 3 = ${xor}")

// Test 6: String interpolation
print("\nTest 6: String Interpolation")
let multi = "This is a ${name} test " +
           "with version ${version} " +
           "and ${numbers.length} numbers"
print(multi)

// Test 7: Array methods
print("\nTest 7: Array Methods")
let first = numbers.first()
let last = numbers.last()
let hasThree = numbers.includes(3)
let indexOf = numbers.indexOf(4)

print("First: ${first}")
print("Last: ${last}")
print("Has 3? ${hasThree}")
print("Index of 4: ${indexOf}")

// Test 8: Complex expressions
print("\nTest 8: Complex Expressions")
let complex = (5 + 3) * 2 - 1
let logical = true && false || true
let comparison = 5 > 3 && 10 <= 10

print("(5 + 3) * 2 - 1 = ${complex}")
print("true && false || true = ${logical}")
print("5 > 3 && 10 <= 10 = ${comparison}")

// Test 9: Function composition
print("\nTest 9: Function Composition")
func compose(f, g) {
    return { x in f(g(x)) }
}

let double = { x in x * 2 }
let addOne = { x in x + 1 }
let doubleThenAdd = compose(addOne, double)

print("Double then add one to 5: ${doubleThenAdd(5)}")

// Test 10: Recursive functions
print("\nTest 10: Recursion")
func factorial(n) {
    if n <= 1 {
        return 1
    }
    return n * factorial(n - 1)
}

print("Factorial of 5: ${factorial(5)}")

print("\n=== All Tests Completed Successfully! ===")
// SwiftLang Demo - Clean Working Examples

print("=== SwiftLang Demo ===\n")

// Basic Math Functions
print("MATH FUNCTIONS:")
func add(a, b) { return a + b }
func multiply(a, b) { return a * b }
func factorial(n) {
    if n <= 1 { return 1 }
    return n * factorial(n - 1)
}

print("5 + 3 = ${add(5, 3)}")
print("4 * 7 = ${multiply(4, 7)}")
print("5! = ${factorial(5)}")

// Closures
print("\nCLOSURES:")
let double = { x in x * 2 }
let square = { x in x * x }
let isEven = { x in x % 2 == 0 }

print("double(6) = ${double(6)}")
print("square(7) = ${square(7)}")
print("isEven(4) = ${isEven(4)}")

// Higher-Order Functions
print("\nHIGHER-ORDER:")
func apply(f, x) { return f(x) }
func compose(f, g) { return { x in f(g(x)) } }

let doubleThenSquare = compose(square, double)
print("apply(double, 5) = ${apply(double, 5)}")
print("double then square 3 = ${doubleThenSquare(3)}")

// Arrays
print("\nARRAYS:")
let numbers = [1, 2, 3, 4, 5]
let doubled = numbers.map(double)
let evens = numbers.filter(isEven)
let sum = numbers.reduce(0, add)

print("Numbers: ${numbers}")
print("Doubled: ${doubled}")
print("Evens: ${evens}")
print("Sum: ${sum}")

// Function Factories
print("\nFACTORIES:")
func makeAdder(n) { return { x in x + n } }
func makeMultiplier(n) { return { x in x * n } }

let add10 = makeAdder(10)
let times3 = makeMultiplier(3)

print("add10(5) = ${add10(5)}")
print("times3(7) = ${times3(7)}")

// Practical Examples
print("\nPRACTICAL:")

// Temperature conversion
func c2f(c) { return c * 9 / 5 + 32 }
func f2c(f) { return (f - 32) * 5 / 9 }

print("0°C = ${c2f(0)}°F")
print("100°F = ${f2c(100)}°C")

// Simple interest calculator
func interest(principal, rate, years) {
    return principal * rate * years / 100
}

print("Interest on 1000 at 5% for 3 years: ${interest(1000, 5, 3)}")

// Pattern: Pipeline
print("\nPIPELINE:")
func pipeline(value, operations) {
    return operations.reduce(value, { v, f in f(v) })
}

let ops = [
    { x in x + 5 },    // add 5
    double,            // double
    { x in x - 3 }     // subtract 3
]

print("Pipeline(10): 10 -> +5 -> *2 -> -3 = ${pipeline(10, ops)}")

// Control Flow
print("\nCONTROL FLOW:")
let i = 0
while i < 3 {
    print("Loop ${i}")
    i = i + 1
}

// Final Stats
print("\n=== Demo Complete ===")
let features = [
    "Functions", "Closures", "Arrays", "Recursion",
    "Higher-Order", "Map/Filter/Reduce", "Factories"
]
print("Demonstrated ${features.length} language features!")
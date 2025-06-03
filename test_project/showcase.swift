// SwiftLang Feature Showcase
// A collection of working examples demonstrating the language

print("=== SwiftLang Feature Showcase ===\n")

// --- CORE FEATURES ---
print("1. FUNCTIONS AND RECURSION")

// Factorial
func factorial(n) {
    if n <= 1 { return 1 }
    return n * factorial(n - 1)
}
print("   6! = ${factorial(6)}")

// Fibonacci
func fib(n) {
    if n < 2 { return n }
    return fib(n-1) + fib(n-2)
}
print("   fib(10) = ${fib(10)}")

// --- CLOSURES ---
print("\n2. CLOSURES AND CAPTURES")

// Basic closures
let double = { x in x * 2 }
let square = { x in x * x }
print("   double(7) = ${double(7)}")
print("   square(5) = ${square(5)}")

// Capturing variables
let factor = 10
let scale = { x in x * factor }
print("   scale(5) with factor ${factor} = ${scale(5)}")

// --- HIGHER-ORDER FUNCTIONS ---
print("\n3. HIGHER-ORDER FUNCTIONS")

// Functions taking functions
func apply(f, x) { return f(x) }
func twice(f, x) { return f(f(x)) }

print("   apply(double, 5) = ${apply(double, 5)}")
print("   twice(double, 3) = ${twice(double, 3)}")

// Functions returning functions
func makeAdder(n) {
    return { x in x + n }
}

let add5 = makeAdder(5)
let add10 = makeAdder(10)
print("   add5(7) = ${add5(7)}")
print("   add10(7) = ${add10(7)}")

// Function composition
func compose(f, g) {
    return { x in f(g(x)) }
}

let doubleThenSquare = compose(square, double)
print("   double then square 3 = ${doubleThenSquare(3)}")

// --- ARRAYS ---
print("\n4. ARRAYS AND TRANSFORMATIONS")

let nums = [1, 2, 3, 4, 5]
print("   Numbers: ${nums}")

// Map
let doubled = nums.map(double)
let squared = nums.map(square)
print("   Doubled: ${doubled}")
print("   Squared: ${squared}")

// Filter
let evens = nums.filter({ x in x % 2 == 0 })
print("   Evens: ${evens}")

// Reduce
let sum = nums.reduce(0, { a, b in a + b })
let product = nums.reduce(1, { a, b in a * b })
print("   Sum: ${sum}")
print("   Product: ${product}")

// --- CONTROL FLOW ---
print("\n5. CONTROL FLOW")

// Conditionals
let x = 10
if x > 5 {
    print("   ${x} is greater than 5")
} else {
    print("   ${x} is not greater than 5")
}

// Loops
print("   Counting to 5:")
let i = 1
while i <= 5 {
    print("   ${i}")
    i = i + 1
}

// --- STRING OPERATIONS ---
print("\n6. STRING INTERPOLATION")

let name = "SwiftLang"
let version = 1.0
let message = "Welcome to ${name} version ${version}!"
print("   ${message}")

// Multi-line concatenation
let long = "This is line one " +
           "and this is line two " +
           "and this is line three"
print("   ${long}")

// --- NESTED FUNCTIONS ---
print("\n7. NESTED FUNCTIONS AND SCOPES")

func outer(x) {
    func middle(y) {
        func inner(z) {
            return x + y + z
        }
        return inner
    }
    return middle
}

let f1 = outer(100)
let f2 = f1(20)
let result = f2(3)
print("   100 + 20 + 3 = ${result}")

// --- PRACTICAL EXAMPLES ---
print("\n8. PRACTICAL EXAMPLES")

// Temperature converter
func celsius_to_fahrenheit(c) {
    return c * 9 / 5 + 32
}

func fahrenheit_to_celsius(f) {
    return (f - 32) * 5 / 9
}

print("   0°C = ${celsius_to_fahrenheit(0)}°F")
print("   100°C = ${celsius_to_fahrenheit(100)}°F")
print("   32°F = ${fahrenheit_to_celsius(32)}°C")
print("   212°F = ${fahrenheit_to_celsius(212)}°C")

// Simple calculator
func calc(a, op, b) {
    if op == "+" { return a + b }
    if op == "-" { return a - b }
    if op == "*" { return a * b }
    if op == "/" { return a / b }
    return 0
}

print("   10 + 5 = ${calc(10, "+", 5)}")
print("   10 - 5 = ${calc(10, "-", 5)}")
print("   10 * 5 = ${calc(10, "*", 5)}")
print("   10 / 5 = ${calc(10, "/", 5)}")

// --- ADVANCED PATTERNS ---
print("\n9. ADVANCED PATTERNS")

// Partial application
func multiply(a, b) { return a * b }
func partial(f, a) {
    return { b in f(a, b) }
}

let times5 = partial(multiply, 5)
print("   times5(8) = ${times5(8)}")

// Pipeline
func pipeline(val, ops) {
    return ops.reduce(val, { acc, f in f(acc) })
}

let ops = [
    { x in x + 1 },
    { x in x * 2 },
    { x in x - 3 }
]
print("   Pipeline 5: ${pipeline(5, ops)}")

// Currying
func curry_add(a) {
    return { b in
        return { c in a + b + c }
    }
}

let addPartial = curry_add(10)
let addMore = addPartial(20)
print("   Curried 10 + 20 + 30 = ${addMore(30)}")

print("\n=== Showcase Complete ===")
print("SwiftLang successfully demonstrated:")
print("✓ Functions and recursion")
print("✓ Closures with captures")
print("✓ Higher-order functions")
print("✓ Array operations")
print("✓ Control flow")
print("✓ String interpolation")
print("✓ Nested scopes")
print("✓ Practical applications")
print("✓ Advanced patterns")
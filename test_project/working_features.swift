// SwiftLang Working Features Demo

print("=== SwiftLang Working Features ===\n")

// ✓ Variables and constants
let language = "SwiftLang"
var version = 1.0
let isReady = true

// ✓ Basic types
print("1. Basic Types:")
print("   String: ${language}")
print("   Number: ${version}")
print("   Boolean: ${isReady}")
print("   Nil: ${nil}")

// ✓ Functions
print("\n2. Functions:")
func greet(name) {
    return "Hello, ${name}!"
}
print("   ${greet("World")}")

// ✓ Recursion
func fibonacci(n) {
    if n <= 1 { return n }
    return fibonacci(n-1) + fibonacci(n-2)
}
print("   Fibonacci(7) = ${fibonacci(7)}")

// ✓ Closures
print("\n3. Closures:")
let double = { x in x * 2 }
let add = { a, b in a + b }
print("   Double 21: ${double(21)}")
print("   Add 15 + 27: ${add(15, 27)}")

// ✓ Closure captures
let factor = 10
let multiply = { x in x * factor }
print("   Multiply by ${factor}: ${multiply(5)}")

// ✓ Higher-order functions
print("\n4. Higher-Order Functions:")
func applyTwice(f, x) {
    return f(f(x))
}
print("   Apply double twice to 5: ${applyTwice(double, 5)}")

// ✓ Functions returning closures
func makeAdder(n) {
    return { x in x + n }
}
let add5 = makeAdder(5)
print("   Add5 to 10: ${add5(10)}")

// ✓ Nested closures
let makeMultiplier = { factor in
    { x in x * factor }
}
let times3 = makeMultiplier(3)
print("   Times 3: ${times3(8)}")

// ✓ Control flow
print("\n5. Control Flow:")
if version > 0.5 {
    print("   Version check: passed")
}

var count = 0
while count < 3 {
    print("   Count: ${count}")
    count = count + 1
}

// ✓ String interpolation
print("\n6. String Interpolation:")
let status = isReady
let message = "Using ${language} " +
              "version ${version} " +
              "which is ${status}"
print("   ${message}")

// ✓ Arithmetic operations
print("\n7. Arithmetic:")
print("   5 + 3 = ${5 + 3}")
print("   10 - 4 = ${10 - 4}")
print("   6 * 7 = ${6 * 7}")
print("   20 / 4 = ${20 / 4}")
print("   17 % 5 = ${17 % 5}")

// ✓ Comparison operations
print("\n8. Comparisons:")
print("   5 > 3: ${5 > 3}")
print("   2 < 7: ${2 < 7}")
print("   4 >= 4: ${4 >= 4}")
print("   6 <= 5: ${6 <= 5}")
print("   3 == 3: ${3 == 3}")
print("   4 != 4: ${4 != 4}")

// ✓ Logical operations
print("\n9. Logical Operations:")
print("   true && false: ${true && false}")
print("   true || false: ${true || false}")
print("   !true: ${!true}")

// ✓ Bitwise operations
print("\n10. Bitwise Operations:")
print("   5 & 3: ${5 & 3}")
print("   5 | 3: ${5 | 3}")
print("   5 ^ 3: ${5 ^ 3}")
print("   ~5: ${~5}")
print("   4 << 1: ${4 << 1}")
print("   8 >> 1: ${8 >> 1}")

print("\n=== All Working Features Demonstrated! ===")
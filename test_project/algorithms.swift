// Common algorithms implemented in SwiftLang

print("=== Algorithm Demonstrations ===\n")

// 1. Factorial
print("1. Factorial:")
func factorial(n) {
    if n <= 1 { return 1 }
    return n * factorial(n - 1)
}

print("   5! = ${factorial(5)}")
print("   7! = ${factorial(7)}")

// 2. Power
print("\n2. Power Function:")
func power(base, exp) {
    if exp == 0 { return 1 }
    if exp == 1 { return base }
    
    let half = power(base, exp / 2)
    if exp % 2 == 0 {
        return half * half
    } else {
        return base * half * half
    }
}

print("   2^10 = ${power(2, 10)}")
print("   3^5 = ${power(3, 5)}")

// 3. GCD (Greatest Common Divisor)
print("\n3. Greatest Common Divisor:")
func gcd(a, b) {
    if b == 0 { return a }
    return gcd(b, a % b)
}

print("   gcd(48, 18) = ${gcd(48, 18)}")
print("   gcd(100, 35) = ${gcd(100, 35)}")

// 4. Prime checker
print("\n4. Prime Number Check:")
func isPrime(n) {
    if n < 2 { return false }
    if n == 2 { return true }
    if n % 2 == 0 { return false }
    
    let i = 3
    while i * i <= n {
        if n % i == 0 { return false }
        i = i + 2
    }
    return true
}

let primes = [2, 3, 5, 7, 11, 13, 17, 19, 23, 29]
let testNumbers = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15]
print("   Prime checks:")
let i = 0
while i < 15 {
    let n = i + 1
    print("   ${n}: ${isPrime(n)}")
    i = i + 1
}

// 5. Fibonacci
print("\n5. Fibonacci Sequence:")
func fib(n) {
    if n < 2 { return n }
    return fib(n - 1) + fib(n - 2)
}

print("   First 10 Fibonacci numbers:")
let j = 0
while j < 10 {
    print("   F(${j}) = ${fib(j)}")
    j = j + 1
}

// 6. Array operations
print("\n6. Array Algorithms:")
let numbers = [3, 1, 4, 1, 5, 9, 2, 6, 5, 3]

// Sum
let sum = numbers.reduce(0, { a, b in a + b })
print("   Sum: ${sum}")

// Product
let product = numbers.reduce(1, { a, b in a * b })
print("   Product: ${product}")

// Max (simulated)
let max = numbers.reduce(numbers[0], { a, b in
    if a > b { a } else { b }
})
print("   Max: ${max}")

// Count evens
let evenCount = numbers.filter({ n in n % 2 == 0 }).length
print("   Even count: ${evenCount}")

// 7. Function composition
print("\n7. Function Composition:")
func compose(f, g) {
    return { x in f(g(x)) }
}

let double = { x in x * 2 }
let addOne = { x in x + 1 }
let square = { x in x * x }

let doubleThenAdd = compose(addOne, double)
let squareThenDouble = compose(double, square)
let addThenSquare = compose(square, addOne)

print("   double(3) then add 1: ${doubleThenAdd(3)}")
print("   square(4) then double: ${squareThenDouble(4)}")
print("   add 1 to 5 then square: ${addThenSquare(5)}")

// 8. Higher-order functions
print("\n8. Higher-Order Functions:")
func twice(f) {
    return { x in f(f(x)) }
}

let doubletwice = twice(double)
let squaretwice = twice(square)

print("   Double twice 3: ${doubletwice(3)}")
print("   Square twice 2: ${squaretwice(2)}")

// 9. Currying
print("\n9. Currying:")
func curry_add(a) {
    return { b in a + b }
}

func curry_multiply(a) {
    return { b in a * b }
}

let add5 = curry_add(5)
let times3 = curry_multiply(3)

print("   add5(10) = ${add5(10)}")
print("   times3(7) = ${times3(7)}")

// 10. Pipeline
print("\n10. Function Pipeline:")
func pipeline(value, ops) {
    return ops.reduce(value, { acc, f in f(acc) })
}

let operations = [double, addOne, square]
let result = pipeline(3, operations)
print("   3 -> double -> +1 -> square = ${result}")

print("\n=== Algorithm Demo Complete ===")
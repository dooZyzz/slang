// Fibonacci sequence generator with memoization

// Create a memoized fibonacci calculator
func createFibonacci() {
    // Cache for memoization (simulated with closure state)
    let cache = [0, 1]  // Base cases
    
    // Recursive fibonacci with memoization
    func fib(n) {
        if n < 2 {
            return n
        }
        
        // Check if we already calculated it
        if n < cache.length {
            return cache[n]
        }
        
        // Calculate and store
        let result = fib(n - 1) + fib(n - 2)
        // Note: We can't actually modify the cache array
        // This is a limitation - just showing the algorithm
        return result
    }
    
    return fib
}

// Different fibonacci implementations
print("=== Fibonacci Implementations ===\n")

// 1. Simple recursive
func fibRecursive(n) {
    if n < 2 { return n }
    return fibRecursive(n - 1) + fibRecursive(n - 2)
}

// 2. Iterative
func fibIterative(n) {
    if n < 2 { return n }
    
    let prev = 0
    let curr = 1
    let i = 2
    
    while i <= n {
        let next = prev + curr
        prev = curr
        curr = next
        i = i + 1
    }
    
    return curr
}

// 3. Tail recursive
func fibTailRec(n, prev, curr) {
    if n == 0 { return prev }
    if n == 1 { return curr }
    return fibTailRec(n - 1, curr, prev + curr)
}

// 4. Generator pattern
func fibGenerator() {
    let a = 0
    let b = 1
    
    return {
        let result = a
        let temp = a + b
        a = b
        b = temp
        return result
    }
}

// Test implementations
print("1. Recursive Implementation:")
print("   fib(10) = ${fibRecursive(10)}")
print("   fib(15) = ${fibRecursive(15)}")

print("\n2. Iterative Implementation:")
print("   fib(10) = ${fibIterative(10)}")
print("   fib(20) = ${fibIterative(20)}")
print("   fib(30) = ${fibIterative(30)}")

print("\n3. Tail Recursive Implementation:")
print("   fib(10) = ${fibTailRec(10, 0, 1)}")
print("   fib(20) = ${fibTailRec(20, 0, 1)}")

print("\n4. First 10 Fibonacci numbers:")
let numbers = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]
let fibs = numbers.map(fibIterative)
print("   ${fibs}")

// Performance comparison helper
func timeFunction(name, f, n) {
    print("\n${name} for fib(${n}):")
    let result = f(n)
    print("   Result: ${result}")
}

print("\n5. Performance Comparison:")
timeFunction("Recursive", fibRecursive, 20)
timeFunction("Iterative", fibIterative, 20)
timeFunction("Tail Recursive", { n in fibTailRec(n, 0, 1) }, 20)

// Fibonacci applications
print("\n6. Fibonacci Applications:")

// Golden ratio approximation
func goldenRatio(n) {
    let fn = fibIterative(n)
    let fn1 = fibIterative(n + 1)
    return fn1 / fn
}

print("   Golden ratio approximations:")
print("   F(10)/F(9) = ${goldenRatio(9)}")
print("   F(20)/F(19) = ${goldenRatio(19)}")
print("   F(30)/F(29) = ${goldenRatio(29)}")

// Sum of first n fibonacci numbers
func fibSum(n) {
    let sum = 0
    let i = 0
    while i <= n {
        sum = sum + fibIterative(i)
        i = i + 1
    }
    return sum
}

print("\n   Sum of first n Fibonacci numbers:")
print("   Sum(0..5) = ${fibSum(5)}")
print("   Sum(0..10) = ${fibSum(10)}")

print("\n=== Fibonacci Demo Complete ===")
// Design patterns and functional programming in SwiftLang

print("=== Functional Programming Patterns ===\n")

// 1. Map, Filter, Reduce
print("1. Array Transformations:")
let numbers = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]

// Map
let doubled = numbers.map({ x in x * 2 })
let squared = numbers.map({ x in x * x })
print("   Original: ${numbers}")
print("   Doubled: ${doubled}")
print("   Squared: ${squared}")

// Filter
let evens = numbers.filter({ x in x % 2 == 0 })
let odds = numbers.filter({ x in x % 2 != 0 })
print("   Evens: ${evens}")
print("   Odds: ${odds}")

// Reduce
let sum = numbers.reduce(0, { acc, x in acc + x })
let product = numbers.reduce(1, { acc, x in acc * x })
print("   Sum: ${sum}")
print("   Product: ${product}")

// 2. Function Factories
print("\n2. Function Factories:")

// Adder factory
func makeAdder(n) {
    return { x in x + n }
}

let add5 = makeAdder(5)
let add10 = makeAdder(10)
print("   add5(7) = ${add5(7)}")
print("   add10(7) = ${add10(7)}")

// Multiplier factory
func makeMultiplier(n) {
    return { x in x * n }
}

let double = makeMultiplier(2)
let triple = makeMultiplier(3)
print("   double(8) = ${double(8)}")
print("   triple(8) = ${triple(8)}")

// 3. Partial Application
print("\n3. Partial Application:")

func add(a, b) { return a + b }
func multiply(a, b) { return a * b }

// Create partially applied functions
func partial(f, a) {
    return { b in f(a, b) }
}

let add10partial = partial(add, 10)
let times5partial = partial(multiply, 5)

print("   add10(15) = ${add10partial(15)}")
print("   times5(8) = ${times5partial(8)}")

// 4. Function Composition
print("\n4. Function Composition:")

func compose(f, g) {
    return { x in f(g(x)) }
}

func pipe(g, f) {
    return { x in f(g(x)) }
}

let addOne = { x in x + 1 }
let square = { x in x * x }

let squareThenAdd = compose(addOne, square)
let addThenSquare = pipe(addOne, square)

print("   square(5) then add 1: ${squareThenAdd(5)}")
print("   add 1 to 5 then square: ${addThenSquare(5)}")

// 5. Callbacks and Handlers
print("\n5. Callbacks:")

func processData(data, callback) {
    // Simulate processing
    let result = data * 2
    callback(result)
}

processData(21, { result in
    print("   Processed result: ${result}")
})

// 6. Builder Pattern with Closures
print("\n6. Builder Pattern:")

func buildString(operations) {
    let result = "Hello"
    let i = 0
    while i < operations.length {
        result = operations[i](result)
        i = i + 1
    }
    return result
}

let stringOps = [
    { s in s + " World" },
    { s in s + "!" },
    { s in "[" + s + "]" }
]

print("   Built string: ${buildString(stringOps)}")

// 7. Memoization Pattern
print("\n7. Memoization (concept):")

func memoize(f) {
    // In real implementation, we'd use a cache
    return { n in
        print("   Computing f(${n})...")
        return f(n)
    }
}

func expensiveOperation(n) {
    // Simulate expensive computation
    return n * n * n
}

let memoizedOp = memoize(expensiveOperation)
print("   First call: ${memoizedOp(5)}")
print("   Second call: ${memoizedOp(5)}")

// 8. Strategy Pattern
print("\n8. Strategy Pattern:")

func calculate(a, b, strategy) {
    return strategy(a, b)
}

let addStrategy = { a, b in a + b }
let multiplyStrategy = { a, b in a * b }

// Power strategy needs to be a function since it has multiple statements
func powerStrategy(a, b) {
    let result = 1
    let i = 0
    while i < b {
        result = result * a
        i = i + 1
    }
    return result
}

print("   10 + 5 = ${calculate(10, 5, addStrategy)}")
print("   10 * 5 = ${calculate(10, 5, multiplyStrategy)}")
print("   2 ^ 8 = ${calculate(2, 8, powerStrategy)}")

// 9. Chain of Responsibility
print("\n9. Chain Pattern:")

func chain(value, operations) {
    return operations.reduce(value, { acc, op in op(acc) })
}

let mathChain = [
    { x in x + 5 },
    { x in x * 2 },
    { x in x - 3 }
]

print("   Chain 10: ${chain(10, mathChain)}")

// 10. Lazy Evaluation
print("\n10. Lazy Evaluation (concept):")

func lazy(computation) {
    let computed = false
    let result = nil
    
    return {
        print("   Computing...")
        computation()
    }
}

let lazyValue = lazy({ 
    print("   Expensive computation...")
    return 42
})

print("   Getting lazy value: ${lazyValue()}")

print("\n=== Patterns Demo Complete ===")
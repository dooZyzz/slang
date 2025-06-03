// Simple math module with just functions

export func add(a, b) {
    return a + b
}

export func multiply(a, b) {
    return a * b
}

export func factorial(n) {
    if n <= 1 {
        return 1
    }
    return n * factorial(n - 1)
}

export func power(base, exp) {
    if exp == 0 {
        return 1
    }
    let result = base
    let i = 1
    while i < exp {
        result = result * base
        i = i + 1
    }
    return result
}

// Module-level variable
export let PI = 3.14159

export func circleArea(radius) {
    return PI * radius * radius
}

// Higher-order function
export func twice(f, x) {
    return f(f(x))
}

// Function returning closure
export func makeAdder(n) {
    return { x in x + n }
}

// Testing module context capture
let modulePrefix = "[Math]"

export func logResult(value) {
    print("${modulePrefix} Result: ${value}")
}
// Math utilities module

export func greet(name) {
    print("Hello from math_utils, ${name}!")
}

// Calculator struct that maintains state
export struct Calculator {
    value
}

// Extension methods for Calculator
extension Calculator {
    func add(n) {
        self.value = self.value + n
        return self.value
    }
    
    func multiply(n) {
        self.value = self.value * n
        return self.value
    }
    
    func getValue() {
        return self.value
    }
}

// Some math helper functions
export func factorial(n) {
    if n <= 1 {
        return 1
    }
    return n * factorial(n - 1)
}

export func fibonacci(n) {
    if n <= 1 {
        return n
    }
    return fibonacci(n - 1) + fibonacci(n - 2)
}

// Higher-order function
export func applyOperation(x, y, op) {
    return op(x, y)
}

// Some predefined operations
export let add = { a, b in a + b }
export let subtract = { a, b in a - b }
export let multiply = { a, b in a * b }
export let divide = { a, b in a / b }
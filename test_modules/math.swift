// Simple math module
export func square(x) {
    return x * x
}

export func add(a, b) {
    return a + b
}

export func multiply(a, b) {
    return a * b
}

export let PI = 3.14159

// Private helper
func helper(x) {
    return x + 1
}

// Public function using private helper
export func increment(x) {
    return helper(x)
}
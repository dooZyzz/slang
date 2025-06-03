// Math operations module for calculator

// Private helper functions
func validateNumbers(a, b) {
    return true  // Simplified validation
}

func roundToDecimals(num, decimals) {
    // Simplified rounding
    return num
}

// Exported calculator operations
export func add(a, b) {
    if validateNumbers(a, b) {
        return a + b
    }
    return 0
}

export func subtract(a, b) {
    if validateNumbers(a, b) {
        return a - b
    }
    return 0
}

export func multiply(a, b) {
    if validateNumbers(a, b) {
        return a * b
    }
    return 0
}

export func divide(a, b) {
    if validateNumbers(a, b) {
        if b != 0 {
            return roundToDecimals(a / b, 2)
        }
        print("Error: Division by zero!")
        return 0
    }
    return 0
}

export func power(base, exponent) {
    var result = 1
    var i = 0
    while i < exponent {
        result = result * base
        i = i + 1
    }
    return result
}

export func sqrt(num) {
    // Simplified square root using Newton's method
    if num < 0 {
        print("Error: Cannot take square root of negative number")
        return 0
    }
    if num == 0 {
        return 0
    }
    
    var guess = num / 2
    var i = 0
    while i < 10 {  // 10 iterations for approximation
        guess = (guess + num / guess) / 2
        i = i + 1
    }
    return roundToDecimals(guess, 2)
}
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
}// Display module for calculator output

// Private formatting helpers
func padString(str, width) {
    // Simplified padding
    return str
}

func createBorder(width) {
    var border = ""
    var i = 0
    while i < width {
        border = border + "="
        i = i + 1
    }
    return border
}

// Exported display functions
export func showResult(operation, a, b, result) {
    let border = createBorder(40)
    print(border)
    print("Operation:", operation)
    print("Input 1:", a)
    print("Input 2:", b)
    print("Result:", result)
    print(border)
}

export func showSingleResult(operation, input, result) {
    let border = createBorder(40)
    print(border)
    print("Operation:", operation)
    print("Input:", input)
    print("Result:", result)
    print(border)
}

export func showMenu() {
    print("\n=== Calculator Menu ===")
    print("1. Add")
    print("2. Subtract")
    print("3. Multiply")
    print("4. Divide")
    print("5. Power")
    print("6. Square Root")
    print("0. Exit")
    print("=====================")
}

export func showWelcome() {
    let border = createBorder(50)
    print(border)
    print("    Welcome to SwiftLang Modular Calculator")
    print(border)
}// Main calculator application

// Show welcome message
showWelcome()

// Demonstrate calculator operations
print("\nDemonstrating Calculator Operations:")

// Addition
let sum = add(15, 25)
showResult("Addition", 15, 25, sum)

// Subtraction
let diff = subtract(100, 37)
showResult("Subtraction", 100, 37, diff)

// Multiplication
let product = multiply(12, 8)
showResult("Multiplication", 12, 8, product)

// Division
let quotient = divide(100, 4)
showResult("Division", 100, 4, quotient)

// Power
let pow_result = power(2, 8)
showResult("Power", 2, 8, pow_result)

// Square root
let sqrt_result = sqrt(144)
showSingleResult("Square Root", 144, sqrt_result)

// Test error handling
print("\nTesting Error Handling:")
let div_zero = divide(10, 0)
let sqrt_neg = sqrt(-16)

print("\nCalculator demonstration complete!")
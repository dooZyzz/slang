// Main calculator application

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
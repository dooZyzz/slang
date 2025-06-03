// A functional calculator in SwiftLang

// Calculator operations
func add(a, b) { return a + b }
func subtract(a, b) { return a - b }
func multiply(a, b) { return a * b }
func divide(a, b) { 
    if b == 0 {
        print("Error: Division by zero!")
        return 0
    }
    return a / b 
}
func power(base, exp) {
    if exp == 0 { return 1 }
    let result = base
    let i = 1
    while i < exp {
        result = result * base
        i = i + 1
    }
    return result
}

// Expression evaluator using closures
func createCalculator() {
    let memory = 0
    
    // Store operation for chaining
    let operate = { value, op, operand in
        if op == "+" { return add(value, operand) }
        if op == "-" { return subtract(value, operand) }
        if op == "*" { return multiply(value, operand) }
        if op == "/" { return divide(value, operand) }
        if op == "^" { return power(value, operand) }
        return value
    }
    
    // Create a calculator chain
    let chain = { initial in
        let value = initial
        
        let ops = {
            plus: { n in operate(value, "+", n) },
            minus: { n in operate(value, "-", n) },
            times: { n in operate(value, "*", n) },
            dividedBy: { n in operate(value, "/", n) },
            toPower: { n in operate(value, "^", n) },
            equals: { value }
        }
        
        return ops
    }
    
    return chain
}

// Demo the calculator
print("=== Functional Calculator Demo ===\n")

// Basic operations
print("Basic Operations:")
print("5 + 3 = ${add(5, 3)}")
print("10 - 4 = ${subtract(10, 4)}")
print("6 * 7 = ${multiply(6, 7)}")
print("20 / 4 = ${divide(20, 4)}")
print("2 ^ 8 = ${power(2, 8)}")

// Using calculator chains
print("\nChained Calculations:")
let calc = createCalculator()

// (5 + 3) * 2
let result1 = calc(5).plus(3).times(2).equals()
print("(5 + 3) * 2 = ${result1}")

// ((10 - 4) * 3) + 7
let result2 = calc(10).minus(4).times(3).plus(7).equals()
print("((10 - 4) * 3) + 7 = ${result2}")

// 2^3 * 5
let result3 = calc(2).toPower(3).times(5).equals()
print("2^3 * 5 = ${result3}")

// Higher-order calculator functions
print("\nHigher-Order Operations:")

// Apply operation to array of numbers
func mapCalculate(numbers, operation) {
    return numbers.map(operation)
}

let numbers = [1, 2, 3, 4, 5]
let doubled = mapCalculate(numbers, { x in x * 2 })
let squared = mapCalculate(numbers, { x in x * x })
let halved = mapCalculate(numbers, { x in x / 2 })

print("Numbers: ${numbers}")
print("Doubled: ${doubled}")
print("Squared: ${squared}")
print("Halved: ${halved}")

// Reduce with operations
let sum = numbers.reduce(0, add)
let product = numbers.reduce(1, multiply)

print("\nReductions:")
print("Sum: ${sum}")
print("Product: ${product}")

// Function composition for calculations
func compose(f, g) {
    return { x in f(g(x)) }
}

let addOne = { x in x + 1 }
let double = { x in x * 2 }
let square = { x in x * x }

let doubleThenAddOne = compose(addOne, double)
let squareThenDouble = compose(double, square)

print("\nComposed Functions:")
print("double then add one to 5: ${doubleThenAddOne(5)}")
print("square then double 4: ${squareThenDouble(4)}")

print("\n=== Calculator Demo Complete ===")
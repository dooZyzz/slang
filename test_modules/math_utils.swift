// math_utils.swift - A utility module for mathematical operations

// Export individual functions
export func square(x) {
    return x * x
}

export func cube(x: Number) -> Number {
    return x * x * x
}

export func factorial(n: Number) -> Number {
    if n <= 1 {
        return 1
    }
    return n * factorial(n - 1)
}

// Export constants
export let PI = 3.14159265359
export let E = 2.71828182846

// Private helper function (not exported)
func isEven(n: Number) -> Bool {
    return n % 2 == 0
}

// Export a function that uses the private helper
export func sumEvens(numbers: Array) -> Number {
    var sum = 0
    for num in numbers {
        if isEven(num) {
            sum = sum + num
        }
    }
    return sum
}
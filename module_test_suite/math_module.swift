// Math module with various mathematical functions

// Private helper functions
func isEven(n) {
    return n % 2 == 0
}

func isPositive(n) {
    return n > 0
}

// Exported mathematical functions
export func abs(x) {
    if isPositive(x) {
        return x
    }
    return -x
}

export func max(a, b) {
    if a > b {
        return a
    }
    return b
}

export func min(a, b) {
    if a < b {
        return a
    }
    return b
}

export func factorial(n) {
    if n <= 1 {
        return 1
    }
    return n * factorial(n - 1)
}

export func sumRange(start, end) {
    var sum = 0
    var i = start
    while i <= end {
        sum = sum + i
        i = i + 1
    }
    return sum
}

// Module initialization
print("[Math Module] Loaded successfully")
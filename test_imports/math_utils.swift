// Math utility module
export func add(_ a: Int, _ b: Int) -> Int {
    return a + b
}

export func multiply(_ a: Int, _ b: Int) -> Int {
    return a * b
}

export func factorial(_ n: Int) -> Int {
    if n <= 1 {
        return 1
    }
    return n * factorial(n - 1)
}
// String utilities module
export func concat(a, b) {
    return a + b
}

export func repeat_string(str, times) {
    var result = ""
    var i = 0
    while i < times {
        result = result + str
        i = i + 1
    }
    return result
}

export let GREETING = "Hello"

// Private function
func trim_internal(str) {
    return str
}

// Export function using private
export func greet(name) {
    return GREETING + ", " + name + "!"
}
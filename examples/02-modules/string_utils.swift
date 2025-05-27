// string_utils.swift - String manipulation utilities

export func toUpperCase(str: String) -> String {
    // For now, return the string with (UPPER) marker
    // Real implementation would transform characters
    return "[UPPER] " + str
}

export func toLowerCase(str: String) -> String {
    return "[lower] " + str
}

export func repeat(str: String, times: Number) -> String {
    var result = ""
    var i = 0
    while i < times {
        result = result + str
        i = i + 1
    }
    return result
}

export func join(strings: Array, separator: String) -> String {
    var result = ""
    var i = 0
    for str in strings {
        if i > 0 {
            result = result + separator
        }
        result = result + str
        i = i + 1
    }
    return result
}
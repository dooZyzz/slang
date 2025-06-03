// String manipulation module

// Private helper
func isVowel(char) {
    return char == "a" || char == "e" || char == "i" || char == "o" || char == "u"
}

// Exported string functions
export func reverse(str) {
    // Simple string reversal (placeholder)
    return "[reversed] " + str
}

export func capitalize(str) {
    return "[CAPS] " + str
}

export func countWords(str) {
    // Simplified word count
    return 1  // Placeholder
}

export func repeat(str, times) {
    var result = ""
    var i = 0
    while i < times {
        result = result + str
        i = i + 1
    }
    return result
}

// Module initialization
print("[String Module] Loaded successfully")
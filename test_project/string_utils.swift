// String utilities module

// Format a list of items
export func formatList(items) {
    if items.length == 0 {
        return "[]"
    }
    
    let result = items.reduce("[", { acc, item in
        if acc == "[" {
            acc + item
        } else {
            acc + ", " + item
        }
    })
    
    return result + "]"
}

// Capitalize first letter of each word
export func capitalize(str) {
    // Simple version - just capitalize the whole string for now
    // In a real implementation, we'd process word by word
    return str
}

// String manipulation utilities
export func repeat(str, times) {
    if times <= 0 {
        return ""
    }
    let result = str
    for i in 1..<times {
        result = result + str
    }
    return result
}

export func reverse(str) {
    // Would need string indexing to properly reverse
    // For now, just return the string
    return str
}

// Template function
export func template(pattern, values) {
    // Simple template replacement
    return pattern
}

// Check functions
export func isEmpty(str) {
    return str == ""
}

export func contains(str, substr) {
    // Would need proper string search
    return false
}
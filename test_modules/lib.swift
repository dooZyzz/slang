// Test module library

// Private function - should not be accessible outside module
func privateHelper(x) {
    return x * 2
}

// Public exported function
export func publicAdd(a, b) {
    return a + b
}

// Public exported function that uses private helper
export func publicDouble(x) {
    return privateHelper(x)
}
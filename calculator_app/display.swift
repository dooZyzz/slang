// Display module for calculator output

// Private formatting helpers
func padString(str, width) {
    // Simplified padding
    return str
}

func createBorder(width) {
    var border = ""
    var i = 0
    while i < width {
        border = border + "="
        i = i + 1
    }
    return border
}

// Exported display functions
export func showResult(operation, a, b, result) {
    let border = createBorder(40)
    print(border)
    print("Operation:", operation)
    print("Input 1:", a)
    print("Input 2:", b)
    print("Result:", result)
    print(border)
}

export func showSingleResult(operation, input, result) {
    let border = createBorder(40)
    print(border)
    print("Operation:", operation)
    print("Input:", input)
    print("Result:", result)
    print(border)
}

export func showMenu() {
    print("\n=== Calculator Menu ===")
    print("1. Add")
    print("2. Subtract")
    print("3. Multiply")
    print("4. Divide")
    print("5. Power")
    print("6. Square Root")
    print("0. Exit")
    print("=====================")
}

export func showWelcome() {
    let border = createBorder(50)
    print(border)
    print("    Welcome to SwiftLang Modular Calculator")
    print(border)
}
// Test cross-module function calls

// Module A functions that call Module B
export func processText(text) {
    print("Module A: Processing text:", text)
    // Would call string module functions
    let result = capitalize(text)
    return result
}

export func analyzeNumber(num) {
    print("Module A: Analyzing number:", num)
    // Would call math module functions
    let absolute = abs(num)
    return absolute
}

// Test within this module
print("Cross-module test:")
let text_result = processText("hello")
print("Result:", text_result)

let num_result = analyzeNumber(-42)
print("Result:", num_result)
// Comprehensive test of module function visibility

// Define module-private functions (should not be accessible outside)
func privateFunction1() {
    print("This is private function 1")
}

func privateFunction2(x) {
    print("Private function 2 with arg:", x)
    let result = x * 2
    return result
}

// Define exported functions
export func publicAPI1() {
    print("Public API 1 - accessible from outside")
    privateFunction1()  // Can call private functions internally
}

export func publicAPI2(value) {
    print("Public API 2 with value:", value)
    let result = privateFunction2(value)
    print("Result from private function:", result)
    return result
}

// Test within the same file/module
print("=== Testing within the same module ===")
print("Calling private functions directly:")
privateFunction1()
privateFunction2(5)

print("\nCalling public functions:")
publicAPI1()
publicAPI2(10)

print("\n=== Module visibility test complete ===")
print("When this is compiled as a module:")
print("- privateFunction1 and privateFunction2 should NOT be accessible from outside")
print("- publicAPI1 and publicAPI2 SHOULD be accessible from outside")
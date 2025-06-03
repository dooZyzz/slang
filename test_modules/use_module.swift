// Test using the bundled module

// This should work - calling exported function
let result = publicAdd(5, 3)
print("publicAdd(5, 3) =", result)

// This should work - calling exported function that uses private helper
let doubled = publicDouble(7)
print("publicDouble(7) =", doubled)

// This should NOT work - trying to call private function
// Uncomment to test:
// let private_result = privateHelper(10)
// print("privateHelper(10) =", private_result)
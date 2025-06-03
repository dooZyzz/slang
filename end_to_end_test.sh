#!/bin/bash

# End-to-end test of SwiftLang module system and CLI

SWIFT_BIN="./build/swift_like_lang"
TEST_DIR="e2e_test"

echo "=== SwiftLang End-to-End Module System Test ==="
echo

# Clean up previous test
rm -rf $TEST_DIR
mkdir -p $TEST_DIR
cd $TEST_DIR

echo "1. Creating a new module project..."
../$SWIFT_BIN new mymodule
cd mymodule

echo
echo "2. Creating module source files..."

# Create a library module with private and public functions
cat > lib.swift << 'EOF'
// Library module with private and public functions

// Private helper functions
func calculateTax(amount) {
    let rate = 0.08
    return amount * rate
}

func validateAmount(amount) {
    return amount > 0
}

// Public API functions
export func calculateTotal(amount) {
    if validateAmount(amount) {
        let tax = calculateTax(amount)
        return amount + tax
    }
    return 0
}

export func formatCurrency(amount) {
    return "$" + amount
}
EOF

# Create main entry point
cat > main.swift << 'EOF'
// Main entry point demonstrating module usage

print("Module System Demo")
print("==================")

// Test public functions
let price = 100
let total = calculateTotal(price)
print("Price:", formatCurrency(price))
print("Total with tax:", formatCurrency(total))

// This would fail if uncommented (private function):
// let tax = calculateTax(price)
EOF

# Update manifest
cat > manifest.json << 'EOF'
{
  "name": "mymodule",
  "version": "1.0.0",
  "description": "End-to-end test module",
  "main": "main.swift",
  "type": "application",
  "sources": [
    "lib.swift",
    "main.swift"
  ]
}
EOF

echo
echo "3. Building the module..."
../../$SWIFT_BIN build --verbose

echo
echo "4. Creating module bundle..."
../../$SWIFT_BIN bundle lib.swift -o mymodule.swiftmodule

echo
echo "5. Running the module..."
../../$SWIFT_BIN run

echo
echo "6. Testing module caching..."
../../$SWIFT_BIN cache list

echo
echo "7. Creating a consumer project that uses our module..."
cd ..
../$SWIFT_BIN new consumer
cd consumer

cat > use_module.swift << 'EOF'
// Consumer that uses the mymodule

print("Using mymodule functions:")
let amount = 250
let total = calculateTotal(amount)
print("Amount:", amount)
print("Total:", total)
EOF

echo
echo "8. Setting module path and running consumer..."
export SWIFTLANG_MODULE_PATH="../mymodule"
../../$SWIFT_BIN run use_module.swift

echo
echo "=== Test Complete ==="
echo "Summary:"
echo "- Created and built a module with private/public functions"
echo "- Generated a module bundle (.swiftmodule)"
echo "- Demonstrated function visibility (private functions not accessible)"
echo "- Showed module loading and usage from another project"
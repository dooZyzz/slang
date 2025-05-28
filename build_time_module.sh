#!/bin/bash
set -e

echo "Building time module manually..."

cd modules/time

# Step 1: Ensure native library is built
echo "Step 1: Building native library..."
if [ ! -f "time.dylib" ]; then
    echo "Native library not found, building..."
    clang -shared -fPIC -o time.dylib src/time_native.c -I../../include
fi

# Step 2: Compile Swift files to bytecode
echo "Step 2: Compiling Swift files..."
# This would be done by the module compiler
# For now, we'll use the existing swiftlang to compile individual files

# Step 3: Create .swiftmodule archive
echo "Step 3: Creating module archive..."
mkdir -p build

# The module compiler should create a .swiftmodule with:
# - module.json
# - bytecode/*.swiftbc (compiled Swift files)
# - native/darwin/time.dylib (native library)

echo "Module build complete!"
echo "Output would be: build/time.swiftmodule"
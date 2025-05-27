# SwiftLang Examples

## Working Examples

### ✅ 01-basics
Basic language features that work today:
- Variables and constants
- Functions (without type annotations)
- Arrays and array methods
- String interpolation
- Control flow

```bash
./cmake-build-debug/swiftlang examples/01-basics/main.swift
```

### ⚠️ 02-modules
Multi-file imports don't work. Use the simplified version:

```bash
./cmake-build-debug/swiftlang examples/02-modules/main_simple.swift
```

## Not Working Yet

### ❌ 03-extensions
Extension syntax not implemented. Parser error on line 46.

### ❌ 04-structs  
Struct syntax not implemented.

### ❌ 05-closures
The example uses type annotations which aren't implemented. However, basic closures work:

```swift
// This works:
let double = { x in x * 2 }

// This doesn't (type annotations):
let add = (a: Number, b: Number) -> Number => a + b
```

### ❌ 06-stdlib
Not tested yet.

## What Actually Works

Based on testing, these features work:

```swift
// Variables
var x = 10
let y = 20

// Functions (no type annotations)
func add(a, b) {
    return a + b
}

// Arrays with methods
let arr = [1, 2, 3]
let doubled = arr.map({ x in x * 2 })

// String interpolation
print("Result: ${x + y}")

// Objects
let obj = {
    name: "Test",
    greet: { print("Hello!") }
}

// Control flow
if x > 5 {
    print("Greater")
}

for item in arr {
    print(item)
}
```

Type annotations, extensions, and struct syntax are not yet implemented.
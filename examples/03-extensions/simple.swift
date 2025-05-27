// Simple extension test

// Number extensions
func Number.squared() {
    return this * this
}

// String extensions
func String.shout() {
    return this + "!"
}

// Test
print("Testing Number extensions:")
let n = 5
print("n =", n)
print("n.squared() =", n.squared())

print("\nTesting String extensions:")
let s = "Hello"
print("s =", s)
print("s.shout() =", s.shout())
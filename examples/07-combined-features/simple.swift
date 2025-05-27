// Simple Combined Features Example

// Structs
struct Point {
    var x: Number
    var y: Number
}

// Extensions
func Point.display() {
    print("(${this.x}, ${this.y})")
}

func Point.distanceFrom(other) {
    let dx = this.x - other.x
    let dy = this.y - other.y
    return dx * dx + dy * dy  // squared distance
}

func Number.double() {
    return this * 2
}

// Main program
print("=== Points Example ===")
let p1 = Point(x: 0, y: 0)
let p2 = Point(x: 3, y: 4)

print("Point 1:")
p1.display()
print("Point 2:")
p2.display()
print("Distance squared:", p1.distanceFrom(p2))

print("\n=== Number Extensions ===")
let n = 5
print("n =", n)
print("n.double() =", n.double())
print("n.double().double() =", n.double().double())

print("\n=== Array Operations ===")
let numbers = [1, 2, 3, 4, 5]
print("Numbers:", numbers)

// Map - double each number
let doubled = numbers.map({ x in x * 2 })
print("Doubled:", doubled)

// Filter - keep even numbers
let evens = numbers.filter({ x in x % 2 == 0 })
print("Evens:", evens)

// Reduce - sum all numbers
let sum = numbers.reduce(0, { acc, x in acc + x })
print("Sum:", sum)

print("\n=== Working with Arrays of Structs ===")
let points = [
    Point(x: 0, y: 0),
    Point(x: 1, y: 1),
    Point(x: 2, y: 2),
    Point(x: 3, y: 3)
]

print("All points:")
for p in points {
    print("  ", end: "")
    p.display()
}

// Find points where x equals y
print("\nPoints where x == y:")
for p in points {
    if p.x == p.y {
        print("  ", end: "")
        p.display()
    }
}

// Calculate total distance from origin
print("\nDistances from origin:")
for p in points {
    let origin = Point(x: 0, y: 0)
    let dist = origin.distanceFrom(p)
    print("  Point ", end: "")
    p.display()
    print(" -> distance squared =", dist)
}
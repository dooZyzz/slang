// Extension Methods in SwiftLang
// Demonstrates how to add methods to existing types using Kotlin-style syntax

// === Number Extensions ===
// Add methods to the built-in Number type

func Number.squared() {
    return this * this
}

func Number.cubed() {
    return this * this * this
}

func Number.isEven() {
    return this % 2 == 0
}

func Number.isOdd() {
    return !this.isEven()
}

func Number.abs() {
    if this < 0 {
        return -this
    }
    return this
}

func Number.times(action) {
    var i = 0
    while i < this {
        action(i)
        i = i + 1
    }
}

// === String Extensions ===
// Add methods to the built-in String type

func String.shout() {
    return this + "!"
}

func String.whisper() {
    return "(" + this + ")"
}

func String.repeat(times) {
    var result = ""
    var i = 0
    while i < times {
        result = result + this
        i = i + 1
    }
    return result
}

// === Array Extensions ===
// Add methods to the built-in Array type

func Array.isEmpty() {
    return this.length == 0
}

func Array.first() {
    if this.length > 0 {
        return this[0]
    }
    return nil
}

func Array.last() {
    if this.length > 0 {
        return this[this.length - 1]
    }
    return nil
}

// === Custom Struct Extensions ===
// Define structs and add extension methods

struct Point {
    var x: Number
    var y: Number
}

func Point.display() {
    print("Point(${this.x}, ${this.y})")
}

func Point.distanceSquared() {
    return this.x * this.x + this.y * this.y
}

func Point.translate(dx, dy) {
    return Point(x: this.x + dx, y: this.y + dy)
}

func Point.scale(factor) {
    return Point(x: this.x * factor, y: this.y * factor)
}

struct Rectangle {
    var width: Number
    var height: Number
}

func Rectangle.area() {
    return this.width * this.height
}

func Rectangle.perimeter() {
    return 2 * (this.width + this.height)
}

func Rectangle.isSquare() {
    return this.width == this.height
}

func Rectangle.display() {
    print("Rectangle ${this.width}x${this.height}")
}

struct Dog {
    var name: String
    var energy: Number
}

func Dog.bark() {
    if this.energy > 0 {
        print("${this.name} says: Woof!")
        this.energy = this.energy - 1
    } else {
        print("${this.name} is too tired to bark")
    }
}

func Dog.play() {
    if this.energy >= 3 {
        print("${this.name} is playing!")
        this.energy = this.energy - 3
    } else {
        print("${this.name} needs to rest first")
    }
}

func Dog.rest() {
    print("${this.name} is resting...")
    this.energy = this.energy + 5
    if this.energy > 10 {
        this.energy = 10
    }
}

func Dog.status() {
    print("${this.name} has ${this.energy} energy")
}

// === Examples ===

print("=== Number Extensions ===")
let n = 5
print("n =", n)
print("n.squared() =", n.squared())
print("n.cubed() =", n.cubed())
print("n.isEven() =", n.isEven())
print("n.isOdd() =", n.isOdd())

let negative = -10
print("\nnegative =", negative)
print("negative.abs() =", negative.abs())

print("\nCounting with n.times():")
n.times({ i in
    print("  Count:", i)
})

print("\n=== String Extensions ===")
let greeting = "Hello"
print("greeting =", greeting)
print("greeting.shout() =", greeting.shout())
print("greeting.whisper() =", greeting.whisper())
print("greeting.repeat(3) =", greeting.repeat(3))

print("\n=== Array Extensions ===")
let numbers = [1, 2, 3, 4, 5]
let empty = []

print("numbers =", numbers)
print("numbers.isEmpty() =", numbers.isEmpty())
print("numbers.first() =", numbers.first())
print("numbers.last() =", numbers.last())

print("\nempty =", empty)
print("empty.isEmpty() =", empty.isEmpty())
print("empty.first() =", empty.first())

print("\n=== Point Extensions ===")
let p1 = Point(x: 3, y: 4)
p1.display()
print("Distance squared from origin:", p1.distanceSquared())

let p2 = p1.translate(2, -1)
print("After translation by (2, -1):")
p2.display()

let p3 = p1.scale(2)
print("After scaling by 2:")
p3.display()

print("\n=== Rectangle Extensions ===")
let rect = Rectangle(width: 10, height: 5)
rect.display()
print("Area:", rect.area())
print("Perimeter:", rect.perimeter())
print("Is square?", rect.isSquare())

let square = Rectangle(width: 7, height: 7)
square.display()
print("Is square?", square.isSquare())

print("\n=== Dog Extensions ===")
let buddy = Dog(name: "Buddy", energy: 5)
buddy.status()
buddy.bark()
buddy.bark()
buddy.play()
buddy.status()
buddy.rest()
buddy.status()

print("\n=== Method Chaining ===")
// Chain multiple method calls
let p4 = Point(x: 1, y: 1)
print("Original point:")
p4.display()

let p5 = p4.translate(2, 3).scale(2).translate(-1, -1)
print("After translate, scale, translate:")
p5.display()

// Number method chaining
let result = 3.squared().squared()
print("\n3.squared().squared() =", result)
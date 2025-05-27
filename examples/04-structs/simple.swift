// Simple struct test

struct Point {
    var x: Number
    var y: Number
}

func Point.display() {
    print("Point(${this.x}, ${this.y})")
}

struct Person {
    var name: String
    var age: Number
}

func Person.greet() {
    print("Hello, I'm ${this.name}")
}

// Test
let p = Point(x: 10, y: 20)
p.display()

let person = Person(name: "Alice", age: 25)
person.greet()
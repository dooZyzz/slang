// Structs in SwiftLang
// Demonstrates struct definitions, fields, and extension methods

// Basic struct with two fields
struct Point {
    var x: Number
    var y: Number
}

// Extension methods for Point
func Point.display() {
    print("Point(${this.x}, ${this.y})")
}

func Point.distanceFromOrigin() {
    // Using squared distance since sqrt might not be available
    return this.x * this.x + this.y * this.y
}

func Point.translate(dx, dy) {
    return Point(x: this.x + dx, y: this.y + dy)
}

// Rectangle struct
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
    print("Rectangle(${this.width}x${this.height})")
}

// Person struct
struct Person {
    var name: String
    var age: Number
}

func Person.greet() {
    print("Hello, my name is ${this.name} and I'm ${this.age} years old.")
}

func Person.isAdult() {
    return this.age >= 18
}

func Person.haveBirthday() {
    this.age = this.age + 1
    print("Happy birthday ${this.name}! You are now ${this.age} years old.")
}

// Dog struct
struct Dog {
    var name: String
    var loudness: Number
}

func Dog.bark() {
    var message = "${this.name} says: "
    var i = 0
    while i < this.loudness {
        message = message + "Woof! "
        i = i + 1
    }
    print(message)
}

func Dog.play(otherDog) {
    print("${this.name} is playing with ${otherDog.name}!")
}

// Address struct
struct Address {
    var street: String
    var city: String
    var zipCode: String
}

func Address.format() {
    return "${this.street}, ${this.city} ${this.zipCode}"
}

// Employee struct with composition
struct Employee {
    var person: Person
    var address: Address
    var salary: Number
}

func Employee.getFullInfo() {
    print("Employee Information:")
    print("  Name: ${this.person.name}")
    print("  Age: ${this.person.age}")
    print("  Address: ${this.address.format()}")
    print("  Salary: $" + this.salary)
}

func Employee.giveRaise(percentage) {
    let raise = this.salary * (percentage / 100)
    this.salary = this.salary + raise
    print("${this.person.name} got a ${percentage}% raise! New salary: $" + this.salary)
}

// Bank Account struct
struct BankAccount {
    var owner: String
    var balance: Number
}

func BankAccount.deposit(amount) {
    if amount > 0 {
        this.balance = this.balance + amount
        print("Deposited $" + amount + ". New balance: $" + this.balance)
        return true
    } else {
        print("Invalid deposit amount")
        return false
    }
}

func BankAccount.withdraw(amount) {
    if amount > 0 && amount <= this.balance {
        this.balance = this.balance - amount
        print("Withdrew $" + amount + ". New balance: $" + this.balance)
        return true
    } else {
        print("Insufficient funds or invalid amount")
        return false
    }
}

func BankAccount.transfer(amount, to) {
    print("Transferring $" + amount + " from ${this.owner} to ${to.owner}")
    if this.withdraw(amount) {
        to.deposit(amount)
        print("Transfer complete!")
        return true
    } else {
        print("Transfer failed!")
        return false
    }
}

func BankAccount.display() {
    print("${this.owner}: $" + this.balance)
}

// === Examples ===

print("=== Basic Struct Usage ===")
let p1 = Point(x: 10, y: 20)
p1.display()
print("Distance from origin (squared):", p1.distanceFromOrigin())

let p2 = p1.translate(5, -10)
print("After translation:")
p2.display()

print("\n=== Rectangle ===")
let rect = Rectangle(width: 10, height: 5)
rect.display()
print("Area:", rect.area())
print("Perimeter:", rect.perimeter())
print("Is square?", rect.isSquare())

let square = Rectangle(width: 7, height: 7)
square.display()
print("Is square?", square.isSquare())

print("\n=== Person ===")
let alice = Person(name: "Alice", age: 25)
let bob = Person(name: "Bob", age: 17)

alice.greet()
bob.greet()

print("${alice.name} is adult?", alice.isAdult())
print("${bob.name} is adult?", bob.isAdult())

bob.haveBirthday()
print("${bob.name} is adult now?", bob.isAdult())

print("\n=== Dog ===")
let buddy = Dog(name: "Buddy", loudness: 3)
let max = Dog(name: "Max", loudness: 2)

buddy.bark()
max.bark()
buddy.play(max)

print("\n=== Struct Composition ===")
let addr = Address(
    street: "123 Main St",
    city: "SwiftCity",
    zipCode: "12345"
)

let emp = Employee(
    person: alice,
    address: addr,
    salary: 75000
)

emp.getFullInfo()
emp.giveRaise(10)

print("\n=== Array of Structs ===")
let points = [
    Point(x: 0, y: 0),
    Point(x: 10, y: 0),
    Point(x: 10, y: 10),
    Point(x: 0, y: 10)
]

print("Polygon points:")
for point in points {
    print("  (${point.x}, ${point.y})")
}

print("\n=== Bank Account Example ===")
let account1 = BankAccount(owner: "Alice", balance: 1000)
let account2 = BankAccount(owner: "Bob", balance: 500)

print("Initial balances:")
account1.display()
account2.display()

account1.deposit(200)
account1.withdraw(100)
account1.transfer(300, account2)

print("\nFinal balances:")
account1.display()
account2.display()

print("\n=== Method Chaining ===")
let p3 = Point(x: 1, y: 1)
print("Original point:")
p3.display()

let p4 = p3.translate(2, 3).translate(-1, -1)
print("After two translations:")
p4.display()

print("\n=== Structs with Closures and Array Methods ===")
// Create an array of dogs
let dogs = [
    Dog(name: "Rex", loudness: 2),
    Dog(name: "Buddy", loudness: 3),
    Dog(name: "Max", loudness: 4),
    Dog(name: "Tiny", loudness: 1)
]

// Map to get dog names
let dogNames = dogs.map({ d in d.name })
print("All dogs: ${dogNames}")

// Filter loud dogs
let loudDogs = dogs.filter({ d in d.loudness > 2 })
print("Number of loud dogs: ${loudDogs.length}")

// Map dogs to their loudness levels
let loudnessLevels = dogs.map({ d in d.loudness })
print("Loudness levels: ${loudnessLevels}")

// Create points and use with array methods
let testPoints = [
    Point(x: 3, y: 4),
    Point(x: 5, y: 12),
    Point(x: 8, y: 15),
    Point(x: 0, y: 0)
]

// Map to get distances
let distances = testPoints.map({ p in p.distanceFromOrigin() })
print("\nPoint distances (squared): ${distances}")

// Filter points far from origin
let farPoints = testPoints.filter({ p in p.distanceFromOrigin() > 100 })
print("Points far from origin: ${farPoints.length}")

// Use string interpolation in closures
let rectangles = [
    Rectangle(width: 3, height: 4),
    Rectangle(width: 5, height: 5),
    Rectangle(width: 2, height: 8)
]

let rectangleDescriptions = rectangles.map({ r in 
    "${r.width}x${r.height} (area: ${r.area()})"
})
print("\nRectangle descriptions: ${rectangleDescriptions}")

// Find squares
let squares = rectangles.filter({ r in r.isSquare() })
print("Number of squares: ${squares.length}")

print("\n=== Advanced Closure Examples ===")
// Method calls in closures
print("Dogs barking (called from map):")
dogs.map({ d in d.bark() })

// Complex transformations
let people = [
    Person(name: "Alice", age: 25),
    Person(name: "Bob", age: 17),
    Person(name: "Charlie", age: 30)
]

let adults = people.filter({ p in p.isAdult() })
let adultNames = adults.map({ p in p.name })
print("\nAdult names: ${adultNames}")

// Nested property access in closures
let employees = [
    Employee(
        person: Person(name: "John", age: 35),
        address: Address(street: "123 Main", city: "NYC", zipCode: "10001"),
        salary: 80000
    ),
    Employee(
        person: Person(name: "Jane", age: 28),
        address: Address(street: "456 Elm", city: "LA", zipCode: "90001"),
        salary: 90000
    )
]

let employeeNames = employees.map({ e in e.person.name })
print("\nEmployee names: ${employeeNames}")

let highEarners = employees.filter({ e in e.salary > 85000 })
print("High earners: ${highEarners.length}")

print("\n=== Demo Complete ===")
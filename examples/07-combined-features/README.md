# Combined Features Example

This example demonstrates how SwiftLang's features work together to create a realistic application.

## Features Demonstrated

1. **Structs** - Data models for User, Post, and Comment
2. **Extension Methods** - Adding behavior to structs and arrays
3. **Closures** - For filtering, mapping, and forEach operations
4. **Arrays** - Collections and functional operations
5. **Control Flow** - Conditionals and loops
6. **String Interpolation** - Building formatted output

## The Example

This example simulates a simple social media system with:
- Users with profiles
- Posts with likes
- Filtering and statistics
- Data transformations

## Key Patterns

### Data Modeling with Structs
```swift
struct User {
    var id: Number
    var name: String
    var email: String
    var age: Number
}
```

### Adding Behavior with Extensions
```swift
func User.isAdult() {
    return this.age >= 18
}
```

### Functional Array Operations
```swift
let adults = users.filter({ user in user.isAdult() })
let names = users.map({ user in user.name })
let totalAge = users.reduce(0, { sum, user in sum + user.age })
```

### Method Chaining
```swift
let result = numbers
    .filter({ n in n.isEven() })
    .map({ n in n.squared() })
    .reduce(0, { sum, n in sum + n })
```

### Custom Array Methods
```swift
func Array.findById(id) {
    for item in this {
        if item.id == id {
            return item
        }
    }
    return nil
}
```

## Running the Example

```bash
../../cmake-build-debug/swiftlang main.swift
```

## Expected Output

The example will:
1. Create and display users
2. Create posts and like them
3. Filter users by age criteria
4. Calculate statistics
5. Find specific items by ID
6. Demonstrate method chaining
7. Transform data structures
8. Perform complex queries

## Learning Points

1. **Composition** - Building complex functionality from simple parts
2. **Separation of Concerns** - Data (structs) vs behavior (extensions)
3. **Functional Programming** - Using map, filter, reduce
4. **Null Safety** - Checking for nil values
5. **Code Reuse** - Generic array methods work with any struct that has an `id` field

## Exercises

Try extending this example:
1. Add a Comment struct and display comments for posts
2. Create a method to find the most active user (most posts)
3. Add timestamps to posts and sort by date
4. Create a recommendation system based on likes
5. Add tags to posts and filter by tag
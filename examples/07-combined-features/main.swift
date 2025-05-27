// Combined Features Example
// Demonstrates how structs, extensions, closures, and arrays work together

// === Data Models ===

struct User {
    var id: Number
    var name: String
    var email: String
    var age: Number
}

struct Post {
    var id: Number
    var userId: Number
    var title: String
    var content: String
    var likes: Number
}

struct Comment {
    var id: Number
    var postId: Number
    var userId: Number
    var text: String
}

// === User Extensions ===

func User.display() {
    print("User #${this.id}: ${this.name} (${this.email})")
}

func User.isAdult() {
    return this.age >= 18
}

func User.canVote() {
    return this.isAdult() && this.age >= 21
}

// === Post Extensions ===

func Post.display() {
    print("Post #${this.id}: ${this.title}")
    print("  ${this.content}")
    print("  Likes: ${this.likes}")
}

func Post.like() {
    this.likes = this.likes + 1
}

func Post.isPopular() {
    return this.likes > 10
}

// === Array Extensions for filtering ===

func Array.filterUsers(predicate) {
    var result = []
    for user in this {
        if predicate(user) {
            result.push(user)
        }
    }
    return result
}

func Array.findById(id) {
    for item in this {
        if item.id == id {
            return item
        }
    }
    return nil
}

// === Statistics Functions ===

func calculateAverageAge(users) {
    if users.length == 0 {
        return 0
    }
    
    var sum = 0
    for user in users {
        sum = sum + user.age
    }
    return sum / users.length
}

func getMostLikedPost(posts) {
    if posts.length == 0 {
        return nil
    }
    
    var mostLiked = posts[0]
    for post in posts {
        if post.likes > mostLiked.likes {
            mostLiked = post
        }
    }
    return mostLiked
}

// === Demo Data ===

print("=== Creating Users ===")
let users = [
    User(id: 1, name: "Alice", email: "alice@example.com", age: 25),
    User(id: 2, name: "Bob", email: "bob@example.com", age: 17),
    User(id: 3, name: "Charlie", email: "charlie@example.com", age: 30),
    User(id: 4, name: "Diana", email: "diana@example.com", age: 22)
]

// Display all users
for user in users {
    user.display()
}

print("\n=== Creating Posts ===")
let posts = [
    Post(id: 1, userId: 1, title: "Hello World", content: "My first post!", likes: 5),
    Post(id: 2, userId: 1, title: "SwiftLang is awesome", content: "I love this language", likes: 15),
    Post(id: 3, userId: 3, title: "Tutorial: Structs", content: "How to use structs...", likes: 8),
    Post(id: 4, userId: 4, title: "Extension methods", content: "Kotlin-style extensions", likes: 12)
]

// Like some posts
print("\nLiking posts...")
posts[1].like()
posts[1].like()
posts[3].like()

// Display popular posts
print("\n=== Popular Posts ===")
for post in posts {
    if post.isPopular() {
        post.display()
    }
}

// === User Filtering ===
print("\n=== Adult Users ===")
let adults = users.filterUsers({ user in user.isAdult() })
for user in adults {
    print("${user.name} (age ${user.age})")
}

print("\n=== Users Who Can Vote ===")
let voters = users.filterUsers({ user in user.canVote() })
for user in voters {
    print("${user.name} can vote")
}

// === Statistics ===
print("\n=== Statistics ===")
print("Average age:", calculateAverageAge(users))
print("Average adult age:", calculateAverageAge(adults))

let mostLiked = getMostLikedPost(posts)
if mostLiked != nil {
    print("\nMost liked post: ${mostLiked.title} with ${mostLiked.likes} likes")
}

// === Finding specific items ===
print("\n=== Finding Items ===")
let user = users.findById(2)
if user != nil {
    print("Found user:")
    user.display()
}

let post = posts.findById(3)
if post != nil {
    print("\nFound post:")
    post.display()
}

// === Method Chaining Example ===
print("\n=== Transform Pipeline ===")

// Create a data processing pipeline
let numbers = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]

func Number.isEven() {
    return this % 2 == 0
}
func Number.squared() {
    return this * this
}
// Process numbers step by step to avoid issues
let evens = []
for n in numbers {
    if n.isEven() {
        evens.push(n)
    }
}

let squared = evens.map({ n in n.squared() })

let bigSquares = []
for n in squared {
    if n > 10 {
        bigSquares.push(n)
    }
}

// Manual sum since reduce might not work
var result = 0
for n in bigSquares {
    result = result + n
}

print("Even numbers, squared, filtered > 10, summed:", result)

// === Struct Transformation ===
print("\n=== User Transformation ===")

// Create modified users
let modifiedUsers = users.map({ user in
    User(
        id: user.id,
        name: user.name.toUpperCase(),
        email: user.email,
        age: user.age + 1
    )
})

print("Users after aging by 1 year:")
for user in modifiedUsers {
    print("  ${user.name} is now ${user.age}")
}

// === Complex Filtering ===
print("\n=== Complex Queries ===")

// Find posts by adult users with more than 10 likes
let popularAdultPosts = []
for post in posts {
    let author = users.findById(post.userId)
    if author != nil && author.isAdult() && post.likes > 10 {
        popularAdultPosts.push(post)
    }
}

print("Popular posts by adults:")
for post in popularAdultPosts {
    let author = users.findById(post.userId)
    if author != nil {
        print("  '${post.title}' by ${author.name} (${post.likes} likes)")
    }
}
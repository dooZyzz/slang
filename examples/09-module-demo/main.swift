// Example demonstrating module imports
// This example shows how to use multiple modules in SwiftLang

import hello
import time

print("=== Module Import Demo ===")
print("")

// Use the hello module
print("1. Using the hello module:")
hello.greet("SwiftLang User")
print("Module message: ${hello.MESSAGE}")
print("")

// Use the time module
print("2. Using the time module:")
let now = time.Time.now()
print("Current time: ${now.toString()}")
print("Time in milliseconds: ${now.toMillis()}")
print("")

// Create and use a duration
print("3. Working with durations:")
let duration = time.Duration.fromHours(2.5)
print("Duration: ${duration.toString()}")
print("")

// Demonstrate timer functionality
print("4. Timer demonstration:")
let timer = time.Timer.start()
print("Timer started!")
time.sleep(100)  // Mock sleep
print("Done!")
print("")

print("=== Demo Complete ===")
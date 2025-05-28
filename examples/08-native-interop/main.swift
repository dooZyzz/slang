// Example using the time module with native interop
// This demonstrates how to use a module that wraps native functionality

// Import the time module
// Note: In current implementation, module imports are being developed
// This shows the intended syntax
import @time
import {Time, Duration, Timer, sleep} from @time

print("=== Time Module Demo ===\n")

// Get current time
let now = Time.now()
print("Current time: ${now.toString()}")
print("Timestamp: ${now.timestamp}")

// Time arithmetic
print("\n=== Time Arithmetic ===")
let tomorrow = now.addDays(1)
print("Tomorrow: ${tomorrow.toString()}")

let nextWeek = now.addDays(7)
print("Next week: ${nextWeek.toString()}")

let inTwoHours = now.addHours(2)
print("In 2 hours: ${inTwoHours.toString()}")

// Time comparisons
print("\n=== Time Comparisons ===")
print("Is tomorrow after now? ${tomorrow.isAfter(now)}")
print("Is now before next week? ${now.isBefore(nextWeek)}")
print("Seconds until tomorrow: ${now.secondsUntil(tomorrow)}")

// Duration examples
print("\n=== Duration Examples ===")
let meetingDuration = Duration.fromMinutes(90)
print("Meeting duration: ${meetingDuration.toString()}")
print("In hours: ${meetingDuration.toHours()}")

let vacation = Duration.fromDays(14)
print("Vacation: ${vacation.toString()}")
print("In hours: ${vacation.toHours()}")

// Timer example
print("\n=== Timer Example ===")
let timer = Timer.start()
print("Timer started...")

// Simulate some work
let numbers = [1, 2, 3, 4, 5]
let squared = numbers.map({ n in n * n })
print("Squared numbers: ${squared}")

print("Elapsed time: ${timer.elapsedSeconds()} seconds")

// Sleep example (commented out to avoid delays)
// print("\n=== Sleep Example ===")
// print("Sleeping for 500ms...")
// sleep(500)
// print("Done sleeping!")

// Practical example: Rate limiting
print("\n=== Practical Example: Rate Limiter ===")
struct RateLimiter {
    var lastCallTime: Number
    var minInterval: Number  // milliseconds
}

func RateLimiter.create(minInterval: Number) {
    return RateLimiter(lastCallTime: 0, minInterval: minInterval)
}

func RateLimiter.canCall() {
    let now = 1234567890000  // Mock current time in millis
    let elapsed = now - this.lastCallTime
    return elapsed >= this.minInterval
}

func RateLimiter.recordCall() {
    this.lastCallTime = 1234567890000  // Mock current time
}

let limiter = RateLimiter.create(1000)  // 1 second rate limit
print("Can call? ${limiter.canCall()}")  // true (first call)
limiter.recordCall()
print("Can call immediately? ${limiter.canCall()}")  // false

// Another practical example: Stopwatch
print("\n=== Practical Example: Stopwatch ===")
struct Stopwatch {
    var laps: Array
    var timer: Timer
}

func Stopwatch.create() {
    return Stopwatch(
        laps: [],
        timer: Timer.start()
    )
}

func Stopwatch.lap() {
    let elapsed = this.timer.elapsed()
    this.laps.push(elapsed)
    this.timer.reset()
    return elapsed
}

func Stopwatch.getTotalTime() {
    var total = this.timer.elapsed()
    for lap in this.laps {
        total = total + lap
    }
    return total
}

let stopwatch = Stopwatch.create()
print("Stopwatch started")
// Simulate laps
stopwatch.laps.push(1230)  // Mock lap times
stopwatch.laps.push(980)
stopwatch.laps.push(1150)
print("Lap times: ${stopwatch.laps}")
print("Total time: ${stopwatch.getTotalTime()}ms")

print("\n=== Demo Complete ===")
print("The time module demonstrates:")
print("- Native C functions wrapped in SwiftLang")
print("- Clean, idiomatic API design")
print("- Struct-based abstractions")
print("- Practical utility functions")
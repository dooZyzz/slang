// Simple demonstration of native interop pattern
// This version works with current SwiftLang implementation

// Time struct to represent a moment in time
struct Time {
    var timestamp: Number
}

// Mock native functions (in real implementation, these would call C functions)
func native_time_now() {
    // In real implementation: calls C time() function
    return 1704067200  // 2024-01-01 00:00:00 UTC
}

func native_time_format(timestamp) {
    // In real implementation: calls C strftime() function
    if timestamp == 1704067200 {
        return "2024-01-01 00:00:00"
    }
    return "Unknown time"
}

func native_time_millis() {
    // In real implementation: calls C clock_gettime()
    return 1704067200000
}

// Time API using the native functions
func Time.now() {
    return Time(timestamp: native_time_now())
}

func Time.toString() {
    return native_time_format(this.timestamp)
}

func Time.addSeconds(seconds) {
    return Time(timestamp: this.timestamp + seconds)
}

func Time.addMinutes(minutes) {
    return Time(timestamp: this.timestamp + (minutes * 60))
}

func Time.addHours(hours) {
    return Time(timestamp: this.timestamp + (hours * 3600))
}

func Time.addDays(days) {
    return Time(timestamp: this.timestamp + (days * 86400))
}

func Time.isBefore(other) {
    return this.timestamp < other.timestamp
}

func Time.isAfter(other) {
    return this.timestamp > other.timestamp
}

// Duration for time intervals
struct Duration {
    var seconds: Number
}

func Duration.fromMinutes(minutes) {
    return Duration(seconds: minutes * 60)
}

func Duration.fromHours(hours) {
    return Duration(seconds: hours * 3600)
}

func Duration.fromDays(days) {
    return Duration(seconds: days * 86400)
}

func Duration.toString() {
    let totalSecs = this.seconds
    
    if totalSecs >= 86400 {
        let days = totalSecs / 86400
        return "${days} days"
    } else if totalSecs >= 3600 {
        let hours = totalSecs / 3600
        return "${hours} hours"
    } else if totalSecs >= 60 {
        let minutes = totalSecs / 60
        return "${minutes} minutes"
    } else {
        return "${totalSecs} seconds"
    }
}

// Timer for measuring elapsed time
struct Timer {
    var startTime: Number
}

func Timer.start() {
    return Timer(startTime: native_time_millis())
}

func Timer.elapsed() {
    return native_time_millis() - this.startTime
}

func Timer.elapsedSeconds() {
    return this.elapsed() / 1000
}

// Demo usage
print("=== Native Interop Pattern Demo ===\n")

// Working with Time
let now = Time.now()
print("Current time: ${now.toString()}")
print("Raw timestamp: ${now.timestamp}")

print("\n=== Time Arithmetic ===")
let tomorrow = now.addDays(1)
print("Tomorrow: ${tomorrow.toString()}")

let nextWeek = now.addDays(7)
print("Next week: ${native_time_format(nextWeek.timestamp)}")

let meeting = now.addHours(2).addMinutes(30)
print("Meeting time (2.5 hours): ${meeting.timestamp}")

print("\n=== Time Comparisons ===")
print("Is tomorrow after today? ${tomorrow.isAfter(now)}")
print("Is today before next week? ${now.isBefore(nextWeek)}")

print("\n=== Duration Examples ===")
let flightTime = Duration.fromHours(12.5)
print("Flight duration: ${flightTime.toString()}")

let marathon = Duration.fromMinutes(180)
print("Marathon time: ${marathon.toString()}")

let vacation = Duration.fromDays(14)
print("Vacation length: ${vacation.toString()}")

print("\n=== Practical Examples ===")

// 1. Event scheduling
struct Event {
    var name: String
    var startTime: Time
    var duration: Duration
}

func Event.endTime() {
    return Time(timestamp: this.startTime.timestamp + this.duration.seconds)
}

func Event.conflictsWith(other) {
    let thisEnd = this.endTime()
    let otherEnd = other.endTime()
    
    // Events conflict if they overlap
    return !(thisEnd.timestamp <= other.startTime.timestamp || 
             this.startTime.timestamp >= otherEnd.timestamp)
}

let meeting1 = Event(
    name: "Team Standup",
    startTime: now.addHours(1),
    duration: Duration.fromMinutes(30)
)

let meeting2 = Event(
    name: "Client Call", 
    startTime: now.addHours(1).addMinutes(15),
    duration: Duration.fromMinutes(45)
)

print("\nEvent: ${meeting1.name}")
print("Conflicts with ${meeting2.name}? ${meeting1.conflictsWith(meeting2)}")

// 2. Cache with expiration
struct CacheEntry {
    var value: String
    var expireTime: Time
}

func CacheEntry.isExpired(currentTime) {
    return currentTime.isAfter(this.expireTime)
}

let cacheEntry = CacheEntry(
    value: "cached data",
    expireTime: now.addMinutes(5)
)

print("\nCache entry expires in 5 minutes")
print("Is expired now? ${cacheEntry.isExpired(now)}")
print("Is expired tomorrow? ${cacheEntry.isExpired(tomorrow)}")

print("\n=== Pattern Benefits ===")
print("This pattern demonstrates:")
print("1. Native functions (C) provide low-level functionality")
print("2. SwiftLang structs provide type safety")
print("3. Extension methods add rich functionality")
print("4. Clean API hides implementation details")
print("5. Easy to test with mock implementations")

print("\n=== Real Implementation ===")
print("In a real native module:")
print("- native_time_now() would call C's time()")
print("- native_time_format() would call strftime()")
print("- native_time_millis() would call clock_gettime()")
print("- Functions would be loaded from a .dylib/.so file")
print("- Module system would handle the loading/binding")
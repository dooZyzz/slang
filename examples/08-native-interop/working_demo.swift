// Native Interop Pattern - Working Demo
// This demonstrates how native modules should be structured

print("=== Native Interop Pattern Demo ===")
print("")

// Mock native functions that would be implemented in C
let NATIVE_TIME_NOW = 1704067200  // 2024-01-01 00:00:00 UTC

// Time struct
struct Time {
    var timestamp: Number
}

// Time methods
func makeTime(timestamp) {
    return Time(timestamp: timestamp)
}

func timeNow() {
    return makeTime(NATIVE_TIME_NOW)
}

func timeAddDays(time, days) {
    return makeTime(time.timestamp + (days * 86400))
}

func timeAddHours(time, hours) {
    return makeTime(time.timestamp + (hours * 3600))
}

func timeFormat(time) {
    // Simplified formatting
    if time.timestamp == NATIVE_TIME_NOW {
        return "2024-01-01 00:00:00"
    } else if time.timestamp == NATIVE_TIME_NOW + 86400 {
        return "2024-01-02 00:00:00"
    } else if time.timestamp == NATIVE_TIME_NOW + 604800 {
        return "2024-01-08 00:00:00"
    } else {
        return "Time: " + time.timestamp
    }
}

// Duration struct
struct Duration {
    var seconds: Number
}

func makeDuration(seconds) {
    return Duration(seconds: seconds)
}

func durationFromHours(hours) {
    return makeDuration(hours * 3600)
}

func durationFromDays(days) {
    return makeDuration(days * 86400)
}

// Demo usage
print("1. Basic Time Operations")
let now = timeNow()
print("   Current time: " + timeFormat(now))
print("   Timestamp: " + now.timestamp)

print("")
print("2. Time Arithmetic")
let tomorrow = timeAddDays(now, 1)
print("   Tomorrow: " + timeFormat(tomorrow))

let nextWeek = timeAddDays(now, 7)
print("   Next week: " + timeFormat(nextWeek))

print("")
print("3. Duration Examples")
let flight = durationFromHours(12)
print("   Flight duration: " + flight.seconds + " seconds")

let vacation = durationFromDays(14)
print("   Vacation: " + vacation.seconds + " seconds")

print("")
print("4. Practical Example: Event Scheduling")

// Event struct
struct Event {
    var name: String
    var startTime: Time
    var durationSecs: Number
}

func makeEvent(name, startTime, durationSecs) {
    return Event(name: name, startTime: startTime, durationSecs: durationSecs)
}

func eventEndTime(event) {
    return makeTime(event.startTime.timestamp + event.durationSecs)
}

let meeting = makeEvent(
    "Team Meeting",
    timeAddHours(now, 2),
    1800  // 30 minutes
)

print("   Event: " + meeting.name)
print("   Start: " + timeFormat(meeting.startTime))
print("   End: " + timeFormat(eventEndTime(meeting)))

print("")
print("5. Native Interop Benefits:")
print("   - Native C functions provide performance")
print("   - SwiftLang provides clean API")
print("   - Structs add type safety")
print("   - Easy to test and maintain")

print("")
print("=== Demo Complete ===")
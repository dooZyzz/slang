# Native Interop Example

This example demonstrates how to create a SwiftLang module that wraps native C functionality, providing a clean and idiomatic API.

## Structure

```
08-native-interop/
├── time_module/
│   ├── src/
│   │   └── time_native.c    # Native C implementation
│   ├── time.swift            # SwiftLang wrapper API
│   └── module.json           # Module metadata
└── main.swift                # Example usage
```

## The Time Module

The time module demonstrates best practices for native interop:

### 1. Native C Layer (`time_native.c`)
- Implements low-level time functions using standard C libraries
- Exports functions that work with SwiftLang's tagged value system
- Handles platform-specific operations (timestamps, sleep, etc.)

### 2. SwiftLang Wrapper (`time.swift`)
- Provides high-level, idiomatic API using structs
- Adds convenience methods and type safety
- Handles unit conversions and formatting
- Exports a clean public interface

### 3. Module Configuration (`module.json`)
- Declares the module name and version
- Specifies the main SwiftLang file
- References the native library
- Lists exported symbols

## Key Concepts Demonstrated

### Native Function Binding
```c
// C function that returns a SwiftLang value
static TaggedValue native_time_now(int arg_count, TaggedValue* args) {
    time_t current_time = time(NULL);
    return NUMBER_VAL((double)current_time);
}
```

### SwiftLang Wrapper
```swift
// Clean API that hides native complexity
struct Time {
    var timestamp: Number
}

func Time.now() {
    return Time(timestamp: time_native.now())
}
```

### Type Safety
The wrapper provides type-safe abstractions:
- `Time` - represents a point in time
- `Duration` - represents a time interval
- `Timer` - for measuring elapsed time

### Extension Methods
Rich functionality through extension methods:
```swift
func Time.addDays(days: Number) {
    return Time(timestamp: this.timestamp + (days * 86400))
}

func Time.isBefore(other: Time) {
    return this.timestamp < other.timestamp
}
```

## Building the Native Module

To compile the native module:

```bash
# From the project root
gcc -shared -fPIC -o time_native.dylib \
    examples/08-native-interop/time_module/src/time_native.c \
    -I include
```

## Running the Example

```bash
# Build the time module
cd examples/08-native-interop/time_module
swiftlang build

# Run the example
cd ..
swiftlang run main.swift
```

## Current Implementation Status

**Note:** Full native module loading is still in development. This example shows:
- ✅ How native modules should be structured
- ✅ Best practices for API design
- ✅ Proper use of structs and extension methods
- 🚧 Actual native function calls (mocked in current version)
- 🚧 Dynamic library loading
- 🚧 Module import syntax

## Best Practices for Native Interop

1. **Keep Native Layer Simple**
   - Native functions should do one thing well
   - Return SwiftLang values (NUMBER_VAL, STRING_VAL, etc.)
   - Handle errors gracefully

2. **Rich SwiftLang Wrapper**
   - Use structs to model concepts
   - Add extension methods for functionality
   - Provide multiple convenience methods
   - Handle unit conversions in SwiftLang

3. **Documentation**
   - Document both native and SwiftLang APIs
   - Provide usage examples
   - Explain any platform-specific behavior

4. **Error Handling**
   - Validate inputs in SwiftLang layer
   - Return sensible defaults or nil on error
   - Consider using Result types for complex operations

## Extending the Example

To add new functionality:

1. Add native function in `time_native.c`
2. Export it in the module initialization
3. Add SwiftLang wrapper in `time.swift`
4. Update `module.json` if adding new exports
5. Test in `main.swift`

## Platform Considerations

The native code may need platform-specific implementations:
- macOS/Linux: `clock_gettime`, `nanosleep`
- Windows: `GetSystemTime`, `Sleep`
- Use conditional compilation for portability
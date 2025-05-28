// Demo consumer - shows how to use a built .swiftmodule

// When the time module is properly built as time.swiftmodule
// it should be importable like this:
import @time

// Use the module
let now = Time.now()
print("Current time: ${now.toString()}")
print("Timestamp: ${now.timestamp}")

// The workflow should be:
// 1. cd modules/time
// 2. swiftlang build     # Creates build/time.swiftmodule
// 3. cd ../../examples/08-native-interop
// 4. swiftlang run demo_consumer.swift

// The module loader will find time.swiftmodule in:
// - ~/.swiftlang/modules/time.swiftmodule (global cache)
// - ./modules/time/build/time.swiftmodule (project local)
// - ./build/modules/time.swiftmodule (build cache)
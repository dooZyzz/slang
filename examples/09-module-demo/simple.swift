// Simple module demo
import hello
import time

print("=== Simple Module Demo ===")

// Use hello module
hello.greet("Module System")
print("Message from hello: ${hello.MESSAGE}")

// Use time module functions
time.sleep(50)

print("=== Demo Complete ===")
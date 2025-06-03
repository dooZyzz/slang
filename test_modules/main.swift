// Main module that imports from other modules
import { square, add, PI } from math
import { greet, concat, GREETING } from string_utils

print("Testing math module:")
print("square(5) =", square(5))
print("add(3, 4) =", add(3, 4))
print("PI =", PI)

print("Testing string module:")
print("greet('World') =", greet("World"))
print("concat('Hello', ' there') =", concat("Hello", " there"))
print("GREETING =", GREETING)

print("Cross-module operations:")
var result = square(add(2, 3))
print("square(add(2, 3)) =", result)

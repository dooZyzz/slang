// Test import performance
import test_exports

// Test accessing exports
print("Testing export access...")
print("Function 5 returns:", test_exports.export_func_5())
print("Constant 7 is:", test_exports.CONST_7)

// Test import all
import * from test_exports

// Now we can use exports directly
print("Direct access after import *:")
print("Function 3 returns:", export_func_3())
print("Constant 2 is:", CONST_2)
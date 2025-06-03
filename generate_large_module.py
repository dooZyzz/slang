#!/usr/bin/env python3
"""Generate a large SwiftLang module for performance testing"""

def generate_large_module(num_functions=1000):
    code = """// Large module for performance testing
// This module contains {num} functions to test compilation and loading performance

""".format(num=num_functions)
    
    # Generate private helper functions
    for i in range(num_functions // 2):
        code += f"""func privateHelper{i}(x) {{
    let result = x * {i + 1}
    return result
}}

"""
    
    # Generate exported functions
    for i in range(num_functions // 2):
        code += f"""export func publicFunction{i}(a, b) {{
    let temp = privateHelper{i % 10}(a)
    return temp + b
}}

"""
    
    # Add module initialization
    code += """// Module initialization
print("[Large Module] Loaded with {} functions")
""".format(num_functions)
    
    return code

if __name__ == "__main__":
    # Generate modules of different sizes
    sizes = [100, 500, 1000]
    
    for size in sizes:
        filename = f"large_module_{size}.swift"
        code = generate_large_module(size)
        with open(filename, 'w') as f:
            f.write(code)
        print(f"Generated {filename} with {size} functions")
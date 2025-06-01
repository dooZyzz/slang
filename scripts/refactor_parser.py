#!/usr/bin/env python3
"""
Script to help refactor parser.c to use custom allocators
"""

import re
import sys

def refactor_parser_memory(content):
    """Refactor memory allocation patterns in parser code"""
    
    # Add includes at the top
    includes = '#include "utils/allocators.h"\n'
    content = content.replace('#include "parser/parser.h"', 
                              '#include "parser/parser.h"\n' + includes, 1)
    
    # Remove stdlib.h if it's only used for malloc/free
    content = re.sub(r'#include <stdlib\.h>\n', '', content)
    
    # Pattern replacements
    replacements = [
        # malloc(sizeof(Type)) -> MEM_NEW(alloc, Type)
        (r'malloc\s*\(\s*sizeof\s*\(\s*(\w+)\s*\)\s*\)', 
         r'MEM_NEW(allocators_get(ALLOC_SYSTEM_PARSER), \1)'),
        
        # malloc(n * sizeof(Type)) -> MEM_NEW_ARRAY(alloc, Type, n)
        (r'malloc\s*\(\s*(\w+)\s*\*\s*sizeof\s*\(\s*(\w+\*?)\s*\)\s*\)',
         r'MEM_NEW_ARRAY(allocators_get(ALLOC_SYSTEM_PARSER), \2, \1)'),
        
        # calloc(1, sizeof(Type)) -> MEM_NEW(alloc, Type)
        (r'calloc\s*\(\s*1\s*,\s*sizeof\s*\(\s*(\w+)\s*\)\s*\)',
         r'MEM_NEW(allocators_get(ALLOC_SYSTEM_PARSER), \1)'),
        
        # calloc(n, sizeof(Type)) -> MEM_NEW_ARRAY(alloc, Type, n)
        (r'calloc\s*\(\s*(\w+)\s*,\s*sizeof\s*\(\s*(\w+\*?)\s*\)\s*\)',
         r'MEM_NEW_ARRAY(allocators_get(ALLOC_SYSTEM_PARSER), \2, \1)'),
        
        # Simple malloc(size) -> MEM_ALLOC(alloc, size)
        (r'malloc\s*\(\s*([^)]+)\s*\)(?!\s*\*\s*sizeof)',
         r'MEM_ALLOC(allocators_get(ALLOC_SYSTEM_PARSER), \1)'),
        
        # strdup -> MEM_STRDUP
        (r'strdup\s*\(\s*([^)]+)\s*\)',
         r'MEM_STRDUP(allocators_get(ALLOC_SYSTEM_PARSER), \1)'),
    ]
    
    for pattern, replacement in replacements:
        content = re.sub(pattern, replacement, content)
    
    # Handle free() calls - need manual review for size tracking
    free_pattern = r'free\s*\(\s*([^)]+)\s*\)'
    free_matches = list(re.finditer(free_pattern, content))
    
    if free_matches:
        print(f"Found {len(free_matches)} free() calls that need manual size tracking:")
        for match in free_matches:
            line_num = content[:match.start()].count('\n') + 1
            print(f"  Line ~{line_num}: {match.group(0)}")
    
    # Handle realloc() calls - need special handling
    realloc_pattern = r'realloc\s*\(\s*([^,]+)\s*,\s*([^)]+)\s*\)'
    realloc_matches = list(re.finditer(realloc_pattern, content))
    
    if realloc_matches:
        print(f"\nFound {len(realloc_matches)} realloc() calls that need special handling:")
        for match in realloc_matches:
            line_num = content[:match.start()].count('\n') + 1
            print(f"  Line ~{line_num}: {match.group(0)}")
    
    return content

def add_allocator_helpers(content):
    """Add helper functions for common patterns"""
    
    helpers = '''
// Helper function to duplicate token lexeme using parser allocator
static char* parser_strdup_lexeme(const char* lexeme, size_t length)
{
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_PARSER);
    char* result = MEM_ALLOC(alloc, length + 1);
    if (result) {
        memcpy(result, lexeme, length);
        result[length] = '\\0';
    }
    return result;
}

// Helper to free string allocated by parser
static void parser_free_string(char* str)
{
    if (str) {
        Allocator* alloc = allocators_get(ALLOC_SYSTEM_PARSER);
        MEM_FREE(alloc, str, strlen(str) + 1);
    }
}

'''
    
    # Insert helpers after includes
    insert_pos = content.find('\n\n', content.find('#include'))
    if insert_pos > 0:
        content = content[:insert_pos] + '\n' + helpers + content[insert_pos:]
    
    return content

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python refactor_parser.py <parser.c>")
        sys.exit(1)
    
    filename = sys.argv[1]
    
    with open(filename, 'r') as f:
        content = f.read()
    
    # Perform refactoring
    content = refactor_parser_memory(content)
    content = add_allocator_helpers(content)
    
    # Write to new file
    output_filename = filename.replace('.c', '_refactored.c')
    with open(output_filename, 'w') as f:
        f.write(content)
    
    print(f"\nRefactored parser written to: {output_filename}")
    print("\nManual review needed for:")
    print("1. All free() calls - need to track sizes")
    print("2. All realloc() calls - need old and new sizes")
    print("3. Complex allocation patterns")
    print("4. Ensure all allocations use ALLOC_SYSTEM_PARSER")
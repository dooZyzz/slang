# Memory Allocator Refactoring Status

## Overview
This document tracks the progress of refactoring the SwiftLang codebase to use the custom memory allocator system instead of direct malloc/free calls.

## Completed Refactoring

### Core Allocator System ✅
- `include/utils/memory.h` - Memory allocator API with file/line tracking
- `src/utils/memory_platform.c` - Platform allocator (malloc/free wrapper)
- `src/utils/memory_arena.c` - Arena allocator for temporary allocations
- `src/utils/memory_freelist.c` - Freelist allocator for fixed-size blocks
- `src/utils/memory_trace.c` - Trace allocator for debugging and leak detection
- `include/utils/allocators.h` - Centralized subsystem allocator management
- `src/utils/allocators.c` - Allocator initialization and access

### Refactored Components ✅

#### Lexer (Complete)
- `src/lexer/lexer_refactored.c` - Parser allocator for temporary data
- `src/lexer/token_refactored.c` - Token memory management

#### AST (Complete)
- `src/ast/ast_refactored.c` - AST allocator with arena strategy
- All AST node creation functions use AST allocator
- No individual node destruction needed with arena

#### Compiler (Complete)
- `src/codegen/compiler_refactored.c` - Compiler allocator for internal structures
- VM allocator for runtime objects (strings, functions)
- Proper size tracking for all allocations

#### Semantic Analysis (Complete)
- `src/semantic/analyzer_refactored.c` - Compiler allocator
- `src/semantic/symbol_table_allocators.c` - Symbol allocator

#### Utilities (Partial)
- `src/utils/hash_map_refactored.c` - Configurable allocator support
- `src/parser/parser_allocator_helpers.c` - Parser helper functions

### Partially Refactored Components ⚠️

#### VM Core
- `src/runtime/core/vm_allocators.c` - Partial refactoring
- `src/runtime/core/object_allocators.c` - Partial refactoring  
- `src/runtime/core/string_pool_allocators.c` - Partial refactoring
- Original files still contain malloc/free calls that need refactoring

## Pending Refactoring

### High Priority 🔴

#### Parser (Complex - 2153 lines, 161 allocations)
- `src/parser/parser.c` - Needs manual refactoring
- 42 free() calls need size tracking
- 32 realloc() calls need old/new size handling
- Complex string and array management

#### VM Core Files
- `src/runtime/core/vm.c` - 70 allocations remaining
- `src/runtime/core/object.c` - 31 allocations remaining
- `src/runtime/core/string_pool.c` - 11 allocations remaining
- `src/runtime/core/object_hash.c` - Needs checking

### Medium Priority 🟡

#### Module System
- `src/runtime/modules/loader/module_loader.c`
- `src/runtime/modules/loader/module_compiler.c`
- `src/runtime/modules/loader/module_cache.c`
- `src/runtime/modules/formats/module_archive.c`
- `src/runtime/modules/formats/module_bundle.c`

#### Type System
- `src/semantic/type.c`
- `src/semantic/visitor.c`

### Low Priority 🟢

#### Standard Library
- `src/stdlib/stdlib.c`
- `src/stdlib/file.h`
- `src/stdlib/json.h`

#### Utilities
- `src/utils/error.c`
- `src/utils/logger.c`
- `src/utils/cli.c`
- `src/utils/compiler_wrapper.c`
- `src/utils/bytecode_format.c`

#### Tools
- `src/tools/bundle.c`

#### Tests
- All test files in `tests/`

## Refactoring Patterns

### Basic Replacements
```c
// Old
ptr = malloc(size);
ptr = calloc(1, size);
str = strdup(original);
free(ptr);

// New
ptr = MEM_ALLOC(allocator, size);
ptr = MEM_NEW(allocator, Type);
str = MEM_STRDUP(allocator, original);
MEM_FREE(allocator, ptr, size);  // Note: size required!
```

### Realloc Pattern
```c
// Old
ptr = realloc(ptr, new_size);

// New (must track old size)
Type* new_ptr = MEM_NEW_ARRAY(allocator, Type, new_count);
memcpy(new_ptr, old_ptr, old_count * sizeof(Type));
MEM_FREE(allocator, old_ptr, old_count * sizeof(Type));
ptr = new_ptr;
```

### Allocator Selection
- **VM Core**: `ALLOC_SYSTEM_VM` - Long-lived runtime structures
- **Objects**: `ALLOC_SYSTEM_OBJECTS` - Runtime objects (consider freelist)
- **Strings**: `ALLOC_SYSTEM_STRINGS` - String pool (arena)
- **Parser**: `ALLOC_SYSTEM_PARSER` - Temporary during parsing
- **AST**: `ALLOC_SYSTEM_AST` - AST nodes (arena)
- **Compiler**: `ALLOC_SYSTEM_COMPILER` - Compilation structures
- **Modules**: `ALLOC_SYSTEM_MODULES` - Module data

## Next Steps

1. **Parser Refactoring** (High complexity)
   - Create systematic approach for 161 allocations
   - Track all string and array sizes
   - Handle complex realloc patterns

2. **Complete VM Core**
   - Finish vm.c refactoring
   - Complete object.c and object_hash.c
   - Verify string pool integration

3. **Module System**
   - Consistent allocator usage
   - Proper lifecycle management

4. **Update Build System**
   - Modify CMakeLists.txt to use refactored files
   - Remove original files after testing

5. **Testing**
   - Run comprehensive test suite
   - Memory leak detection with trace allocator
   - Performance benchmarking

## Benefits Achieved

1. **Memory Tracking**: File/line information for all allocations
2. **Leak Detection**: Trace allocator identifies unfree memory
3. **Performance**: Arena allocators for temporary data
4. **Debugging**: Detailed allocation statistics
5. **Flexibility**: Per-subsystem allocator configuration
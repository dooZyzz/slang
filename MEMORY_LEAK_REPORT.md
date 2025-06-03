# Memory Leak Investigation Report

## Summary

Investigation reveals **significant memory management issues** in the slang codebase that can cause severe memory leaks leading to the 97% RAM usage you observed.

## Critical Findings

### 1. **No Garbage Collector Implemented**
- The VM creates objects but has no garbage collection mechanism
- Objects allocated with `object_create()` are never automatically freed
- This causes unbounded memory growth during program execution

### 2. **Widespread Direct malloc/free Usage**
Many core components bypass the custom allocator system and use direct memory allocation:

#### Core Runtime Components:
- `src/runtime/core/bootstrap.c` - Module allocation
- `src/runtime/packages/package_manager.c` - Extensive malloc/realloc/free usage
- `src/runtime/packages/package.c` - Direct memory management

#### Semantic Analysis:
- `src/semantic/type.c` - All type allocations use calloc/free directly
- `src/semantic/symbol_table.c` - Direct calloc usage
- `src/semantic/visitor.c` - Direct memory management

#### Utilities:
- `src/utils/cli.c` - Extensive realloc/free usage (20+ locations)
- `src/utils/compiler_wrapper.c` - Direct calloc and free
- `src/utils/bytecode_format.c` - Direct realloc/free
- `src/utils/logger.c` - Uses strdup (malloc internally)
- `src/utils/version.c` - Direct free calls

### 3. **strdup() Usage**
Multiple files use `strdup()` which allocates memory via malloc:
- Logger system
- CLI utilities
- Package manager
- Native modules

### 4. **Parser Infinite Loop Bug**
- Parser error recovery has an infinite loop when encountering certain syntax errors
- This causes unbounded error message generation
- While not a memory leak per se, it can consume massive amounts of memory

## Memory Leak Scenarios

1. **VM Object Leaks**: Every object created during program execution is leaked
2. **Type System Leaks**: All type information is allocated with malloc and may not be freed
3. **Symbol Table Leaks**: Symbol tables use direct allocation
4. **Package Manager Leaks**: Extensive direct memory usage in package loading
5. **CLI Memory Leaks**: Command-line parsing allocates memory that may not be freed

## Recommendations

### Immediate Actions:
1. **Implement a Garbage Collector** for the VM
2. **Replace all direct malloc/free calls** with allocator system calls
3. **Fix the parser infinite loop** bug
4. **Create allocator-aware string duplication** function

### Code Changes Needed:

1. **Create wrapper functions**:
```c
// In allocators.h
#define ALLOC_STRDUP(alloc, str) alloc_strdup(alloc, str)
char* alloc_strdup(Allocator* alloc, const char* str);
```

2. **Update type system** to use allocators:
```c
// Replace: calloc(1, sizeof(Type))
// With: MEM_NEW(allocators_get(ALLOC_SYSTEM_COMPILER), Type)
```

3. **Add GC to VM**:
- Implement mark-and-sweep or reference counting
- Track all allocated objects
- Periodic collection cycles

### Priority Order:
1. **HIGH**: Implement GC for VM objects
2. **HIGH**: Fix parser infinite loop
3. **MEDIUM**: Replace direct mallocs in semantic analysis
4. **MEDIUM**: Update package manager memory usage
5. **LOW**: Update utility functions

## Testing Recommendations

1. Use valgrind for comprehensive leak detection
2. Add memory usage monitoring to test suite
3. Create stress tests for object allocation
4. Monitor RSS/VmSize during long-running programs

## Conclusion

The memory leaks are caused by a combination of:
- Missing garbage collection
- Widespread bypass of the allocator system
- Direct memory allocation in core components

These issues compound during program execution, leading to the severe memory usage you observed. The custom allocator system is well-designed but is being bypassed in many critical areas.
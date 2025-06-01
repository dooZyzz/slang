# SwiftLang Memory Allocator System

## Overview

SwiftLang uses a custom memory allocator system that provides fine-grained control over memory allocation patterns, better debugging capabilities, and performance optimization through specialized allocators.

## Architecture

The allocator system consists of:

1. **Abstract Allocator Interface** (`Allocator`)
   - Provides a uniform interface for all allocator types
   - Supports allocation, reallocation, freeing, and statistics
   - Tracks file/line information for debugging
   - Supports optional tagging for categorizing allocations

2. **Allocator Implementations**
   - **Platform Allocator**: Wrapper around standard malloc/free
   - **Arena Allocator**: Linear allocator for temporary allocations
   - **Freelist Allocator**: Fixed-size block allocator for same-sized objects
   - **Trace Allocator**: Debugging allocator that tracks all allocations

3. **Subsystem Allocators**
   - Each major subsystem has its own allocator instance
   - Allows independent memory management and profiling

## Allocator Types

### Platform Allocator
- **Use Case**: General-purpose allocations, long-lived data
- **Characteristics**: Direct wrapper around malloc/free
- **Examples**: VM structures, module data, global state

### Arena Allocator
- **Use Case**: Temporary allocations with bulk deallocation
- **Characteristics**: Very fast allocation, no individual frees
- **Examples**: AST nodes, parser temporary data, per-frame allocations

### Freelist Allocator
- **Use Case**: Many allocations of the same size
- **Characteristics**: Reuses freed blocks, reduces fragmentation
- **Examples**: Object instances, closure cells, fixed-size nodes

### Trace Allocator
- **Use Case**: Debugging memory leaks and usage patterns
- **Characteristics**: Tracks every allocation with metadata
- **Examples**: Development builds, memory profiling

## Subsystem Allocation Strategy

| Subsystem | Allocator Type | Rationale |
|-----------|---------------|-----------|
| VM Core | Platform | Long-lived, various sizes |
| Objects | Platform/Freelist | Mix of sizes, some fixed |
| Strings | Arena | Immutable, bulk cleanup |
| Bytecode | Arena | Temporary during compilation |
| Compiler | Arena | Reset after each compilation |
| AST | Arena | Freed after compilation |
| Parser | Arena | Very temporary data |
| Symbols | Arena | Freed after analysis |
| Modules | Platform | Long-lived, persist |
| Stdlib | Platform | Global lifetime |
| Temp | Arena | General temporary use |

## Usage

### Basic Allocation

```c
// Using subsystem-specific macros
void* ptr = VM_ALLOC(size);                    // VM subsystem
Expr* expr = AST_NEW(Expr);                    // AST subsystem
char* str = STR_DUP("hello");                  // String subsystem

// Using generic allocator
Allocator* alloc = allocators_get(ALLOC_SYSTEM_VM);
void* ptr = MEM_ALLOC(alloc, size);
```

### Tagged Allocations

```c
// Tag allocations for debugging
void* data = MEM_ALLOC_TAGGED(alloc, size, "game-state");
Object* obj = OBJ_NEW(Object, "player-object");
```

### Arena Scope

```c
// Automatic cleanup with arena scope
WITH_ARENA(temp, 4096, {
    // All allocations here use temp arena
    void* data = MALLOC(1024);
    char* str = STRDUP("temporary");
    // Automatically freed when scope ends
});
```

### Memory Profiling

```bash
# Enable memory statistics
export SWIFTLANG_MEM_STATS=1
./swiftlang myprogram.swift

# Enable memory debugging (trace allocator)
export SWIFTLANG_MEM_DEBUG=1
./swiftlang myprogram.swift
```

## Migration Guide

### From malloc/free

```c
// Before
void* ptr = malloc(size);
free(ptr);

// After (simple)
void* ptr = MALLOC(size);
FREE_SIMPLE(ptr);

// After (with size tracking)
void* ptr = VM_ALLOC(size);
VM_FREE(ptr, size);
```

### From calloc

```c
// Before
void* ptr = calloc(1, size);

// After
void* ptr = VM_ALLOC_ZERO(size);
```

### From strdup

```c
// Before
char* copy = strdup(str);

// After
char* copy = VM_STRDUP(str);
```

### Type-safe Allocation

```c
// Before
MyStruct* s = calloc(1, sizeof(MyStruct));
int* arr = calloc(10, sizeof(int));

// After
MyStruct* s = VM_NEW(MyStruct);
int* arr = VM_NEW_ARRAY(int, 10);
```

## Building

```bash
# Build with custom allocators (default)
cmake -DUSE_CUSTOM_ALLOCATORS=ON ..

# Build with standard malloc/free
cmake -DUSE_CUSTOM_ALLOCATORS=OFF ..
```

## Performance Considerations

1. **Arena Allocators**: Extremely fast for allocation-heavy phases
2. **Freelist Allocators**: Reduce fragmentation for fixed-size objects
3. **Bulk Operations**: Reset arenas instead of individual frees
4. **Cache Locality**: Arena allocators improve cache performance

## Debugging Features

### Leak Detection
The trace allocator automatically detects memory leaks on shutdown:

```
=== MEMORY LEAKS DETECTED ===
350 bytes leaked in 2 allocations
  0x12345678: 200 bytes at player.swift:42 [game-state]
  0x87654321: 150 bytes at enemy.swift:15 [ai-data]
```

### Allocation Statistics
View detailed statistics per subsystem:

```
=== Memory Allocator Statistics ===

--- VM Core ---
Total Allocated:  524288 bytes
Current Usage:    102400 bytes
Peak Usage:       204800 bytes
Allocations:      1543

--- AST ---
Total Allocated:  65536 bytes
Current Usage:    0 bytes (reset after compilation)
Peak Usage:       32768 bytes
```

### Tagged Allocation Tracking
Group allocations by purpose:

```
=== Allocation by Tag ===
| Tag                  | Count | Current Size | Peak Size |
|---------------------|-------|--------------|-----------|
| game-state          |    15 |        4096  |     8192  |
| player-data         |     3 |        1024  |     1024  |
| ai-pathfinding      |     8 |        2048  |     4096  |
```

## Best Practices

1. **Choose the Right Allocator**: Use arena for temporary data, platform for long-lived
2. **Tag Important Allocations**: Makes debugging much easier
3. **Track Sizes**: Required for proper deallocation
4. **Reset Arenas**: More efficient than individual frees
5. **Profile First**: Use trace allocator to understand patterns
6. **Group Related Allocations**: Use the same allocator for related data

## Implementation Details

### Thread Safety
Currently, allocators are not thread-safe. Each thread should have its own allocator instances.

### Memory Overhead
- Platform: ~0 bytes per allocation
- Arena: ~0 bytes per allocation, some waste at end of blocks
- Freelist: ~0 bytes per allocation within blocks
- Trace: ~64 bytes per allocation for metadata

### Limitations
- Arena allocators don't support individual frees
- Freelist allocators only handle one block size
- Trace allocator has significant overhead
- No automatic garbage collection

## Future Enhancements

1. **Thread-Local Allocators**: Automatic per-thread allocation
2. **Pool Allocators**: Variable-size pools with size classes  
3. **Memory Limits**: Enforce memory budgets per subsystem
4. **Allocation Callbacks**: Hook into allocation events
5. **Memory Mapping**: Use mmap for large allocations
6. **GC Integration**: Optional garbage collection support
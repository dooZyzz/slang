# Module System Refactoring Plan

## Phase 1: Unify Module Caching

### Current State
- `ModuleLoader` has a simple array-based cache
- `module_cache.c` provides thread-safe caching with statistics
- Bundles have their own HashMap cache
- No coordination between caches

### Goal
Make `ModuleCache` the internal implementation of `ModuleLoader`'s caching mechanism.

### Implementation Steps

#### Step 1: Refactor ModuleLoader to use ModuleCache
```c
// In module.h - Update ModuleLoader struct
typedef struct ModuleLoader {
    ModuleLoaderType type;
    const char* name;
    struct ModuleLoader* parent;
    
    // Replace simple cache with ModuleCache
    struct ModuleCache* cache;  // Thread-safe cache implementation
    
    // Module search paths
    struct {
        char** paths;
        size_t count;
        size_t capacity;
    } search_paths;
    
    VM* vm;
    PackageSystem* package_system;
} ModuleLoader;
```

#### Step 2: Update module.c to use ModuleCache API
```c
// In module_loader_create()
ModuleLoader* module_loader_create(VM* vm, ModuleLoaderType type) {
    ModuleLoader* loader = calloc(1, sizeof(ModuleLoader));
    loader->type = type;
    loader->vm = vm;
    
    // Use ModuleCache instead of simple array
    loader->cache = module_cache_create(NULL);  // NULL for in-memory cache
    
    // ... rest of initialization
}

// Update module_get_cached()
Module* module_get_cached(ModuleLoader* loader, const char* path) {
    Module* module = module_cache_get(loader->cache, path);
    if (!module && loader->parent) {
        return module_get_cached(loader->parent, path);
    }
    return module;
}

// Update cache_module()
static void cache_module(ModuleLoader* loader, Module* module) {
    module_cache_put(loader->cache, module->path, module);
}
```

#### Step 3: Remove redundant caching in bundles
- Remove HashMap cache from Bundle struct
- Use ModuleLoader's cache when loading from bundles

## Phase 2: Unify Archive and Bundle Formats

### Current State
- `ModuleArchive`: Single module .swiftmodule files
- `ModuleBundle`: Multi-module .swb files  
- Both use ZIP with similar structure

### Goal
Single packaging format that scales from single module to full application.

### New Format: SwiftPackage (.swiftpkg)
```
.swiftpkg (ZIP format)
├── manifest.json          # Package metadata
├── modules/              # Module contents
│   ├── <module-name>/   
│   │   ├── metadata.json  # Module-specific metadata
│   │   ├── bytecode.swiftbc
│   │   └── native/       # Platform-specific libraries
│   │       ├── darwin-x86_64.dylib
│   │       └── linux-x86_64.so
│   └── ...
├── resources/            # Shared resources
└── dependencies.lock     # Resolved dependencies
```

### Implementation
1. Create new `package_format.h/c` with unified API
2. Implement readers for both old formats
3. Implement new format writer
4. Update module loader to use new format
5. Deprecate old format APIs

## Phase 3: Consolidate Metadata

### Current State
- Module metadata in multiple places
- Package metadata separate from module metadata
- Bundle metadata duplicates information

### Goal
Single metadata system managed by Package.

### Implementation
```c
// Unified metadata structure
typedef struct {
    // Basic info
    char* name;
    char* version;
    char* description;
    
    // Module type info
    enum {
        MODULE_TYPE_SOURCE,
        MODULE_TYPE_LIBRARY,
        MODULE_TYPE_APPLICATION,
        MODULE_TYPE_NATIVE
    } type;
    
    // Build info
    BuildConfig* build_config;
    
    // Dependencies
    Dependency* dependencies;
    size_t dependency_count;
    
    // Exports
    Export* exports;
    size_t export_count;
    
    // Platform support
    Platform* platforms;
    size_t platform_count;
} ModuleMetadata;
```

## Phase 4: Simplify Public API

### Goal
Clean, simple API that hides implementation complexity.

### New API Design
```c
// Module loading
Module* swiftlang_load_module(const char* name);
Module* swiftlang_load_module_from_path(const char* path);

// Package operations  
Package* swiftlang_package_open(const char* path);
Module* swiftlang_package_get_module(Package* pkg, const char* name);
void swiftlang_package_close(Package* pkg);

// Building/bundling
PackageBuilder* swiftlang_package_builder_new(const char* name);
void swiftlang_package_builder_add_module(PackageBuilder* b, const char* path);
void swiftlang_package_builder_add_resource(PackageBuilder* b, const char* src, const char* dst);
bool swiftlang_package_builder_build(PackageBuilder* b, const char* output);
void swiftlang_package_builder_free(PackageBuilder* b);

// Execution
int swiftlang_run_package(const char* package_path, int argc, char** argv);
```

## Migration Strategy

### Phase 1 (Immediate)
1. Make ModuleCache internal to ModuleLoader
2. Fix thread-safety issues
3. Add proper statistics tracking

### Phase 2 (Short term) 
1. Design new package format spec
2. Implement format readers/writers
3. Add compatibility layer for old formats

### Phase 3 (Medium term)
1. Consolidate metadata structures
2. Update all code to use unified metadata
3. Remove redundant metadata handling

### Phase 4 (Long term)
1. Deprecate old APIs with warnings
2. Provide migration guide
3. Remove deprecated code in v2.0

## Benefits

1. **Performance**: Single cache, no redundant lookups
2. **Simplicity**: One format, one cache, one metadata system  
3. **Reliability**: Thread-safe, proper memory management
4. **Maintainability**: Clear separation of concerns
5. **User Experience**: Simple, consistent API

## Risks and Mitigation

1. **Breaking Changes**: Use compatibility layers and deprecation warnings
2. **Performance Regression**: Benchmark before/after each phase
3. **Bug Introduction**: Comprehensive test suite for each phase
4. **User Confusion**: Clear documentation and migration guides
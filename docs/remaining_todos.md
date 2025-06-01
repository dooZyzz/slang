# Remaining TODO Items in SwiftLang

This document tracks all remaining TODO comments in the codebase after the reorganization and initial TODO resolution effort.

## Summary

Total remaining TODOs: 28 (10 completed)

## Completed TODOs

1. ✅ **Module Format Checksum Verification** - Fixed the checksum calculation and re-enabled verification
   - The issue was with file position handling during verification
   - Now properly saves and restores file position
   - Added error handling for memory allocation and file read failures

2. ✅ **Module Unloading with Reference Checking** - Implemented proper module unloading
   - Added reference counting to Module structure with thread-safe operations
   - Implemented module_ref(), module_unref(), and module_get_ref_count()
   - Fixed module_loader_unload_all() to properly iterate and unload modules
   - Modules are reference counted when loaded via OP_LOAD_MODULE

3. ✅ **Module Lifecycle Management** - Completed basic lifecycle management
   - Modules properly track loading states
   - Reference counting ensures safe unloading
   - Module resources are properly freed including native handles

4. ✅ **Proper LRU Cache Eviction** - Implemented full LRU eviction strategy
   - Added last_access_time tracking to Module structure
   - Cache get operations update access time
   - module_cache_trim() now properly sorts modules by access time
   - Only evicts modules with zero reference count
   - Properly frees evicted modules to prevent memory leaks

5. ✅ **Bundle Metadata JSON Parsing** - Implemented proper JSON parsing for bundles
   - Added json_get_string_from_bundle() helper function
   - Bundle metadata now parsed from bundle.json including name, version, type, entry_point
   - Added main_module field to BundleMetadata structure
   - Proper defaults for missing fields

6. ✅ **Module Version Extraction** - Fixed version TODO in bundle builder
   - Module versions now extracted from module metadata
   - Falls back to "1.0.0" if version not available
   - Proper metadata loading and cleanup

7. ✅ **Dependency Path Resolution** - Improved dependency resolution in bundles
   - Tries multiple standard locations for dependencies
   - Provides helpful warnings when dependencies can't be resolved
   - Supports common module directory structures

8. ✅ **Module Version Compatibility Checking** - Implemented version checking in dependency resolution
   - Added proper version compatibility checks in package_resolve_dependencies()
   - Uses existing module_check_version_compatibility() function
   - Handles cases where version info is missing with appropriate warnings
   - Supports flexible version requirements

9. ✅ **Bundle Execution** - Implemented execution of module's main function
   - Bundle can now execute entry point module
   - Looks for exported 'main' function
   - Handles both function and closure types
   - Returns appropriate exit codes
   - Differentiates between application and library bundles

10. ✅ **Chunk Lifecycle Management** - Fixed memory leak in module loading
    - Temporary chunks are now properly freed after execution
    - Clear separation between temporary and persistent chunks
    - No more memory leaks from module bytecode loading

## By Category

### 1. Module System TODOs

#### Module Bundle (`src/runtime/modules/formats/module_bundle.c`)
- **Version Management**: Get actual version instead of hardcoded "1.0.0"
- **Dependency Resolution**: Resolve dependency path properly
- **Bundle Metadata**: Parse bundle name from JSON instead of hardcoding
- **Module Execution**: Execute the module's main function
- **Security**: Implement signature verification

#### Module Cache (`src/runtime/modules/loader/module_cache.c`)
- **LRU Eviction**: Implement proper LRU eviction with hash map
  - Currently using simplified eviction

#### Module Loader (`src/runtime/modules/loader/module_loader.c`)
- **Memory Management**: Proper chunk lifecycle management

#### Module Unload (`src/runtime/modules/lifecycle/module_unload.c`)
- **Iteration**: Implement proper module iteration for unloading
- **Reference Counting**: Check for active references before unloading

#### Module Inspect (`src/runtime/modules/extensions/module_inspect.c`)
- **Metadata**: Get description from metadata
- **Package Info**: Implement when package metadata is available
- **Async Support**: Check for async support

### 2. Package System TODOs

#### Package Manager (`src/runtime/packages/package_manager.c`)
- Various package management features to implement

#### Package System (`src/runtime/packages/package.c`)
- Package-related functionality

### 3. Language Features TODOs

#### Parser (`src/parser/parser.c`)
- Various parsing improvements

#### Semantic Analyzer (`src/semantic/analyzer.c`)
- Type checking and semantic analysis improvements

#### Standard Library (`src/stdlib/stdlib.c`)
- Standard library enhancements

### 4. Tool and Utility TODOs

#### CLI (`src/utils/cli.c`)
- Command-line interface improvements

#### Syntax Test (`src/utils/syntax_test.c`)
- Test framework enhancements

### 5. Native Module TODOs

#### OpenGL Module (`modules/opengl/src/opengl_native.c`)
- OpenGL binding improvements

#### GLFW Module (`modules/glfw/src/glfw_native_simple.c`)
- GLFW binding improvements

#### Math Module (`modules/math/src/math_native.c`)
- Math module enhancements

## Prioritized Action Plan

### High Priority (Critical for stability)
1. Fix module format checksum calculation
2. Implement proper module unloading with reference checking
3. Complete module lifecycle management

### Medium Priority (Important features)
1. Implement proper LRU cache eviction
2. Add bundle metadata parsing from JSON
3. Implement module dependency resolution
4. Add signature verification for security

### Low Priority (Nice to have)
1. Add async support detection
2. Enhance module inspection capabilities
3. Improve native module bindings

## Next Steps

1. Start with fixing the checksum calculation in module_format.c
2. Implement proper module unloading to prevent memory leaks
3. Enhance the module cache with proper LRU eviction
4. Add comprehensive tests for all new functionality
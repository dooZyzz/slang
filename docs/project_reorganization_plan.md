# Project Reorganization Plan

## Current Issues
1. Runtime directory has 15 files with mixed concerns
2. Module-related files are scattered without clear hierarchy
3. Package management mixed with module loading
4. No clear separation between core runtime and extensions

## Proposed New Structure

```
src/
├── core/
│   ├── lexer/         # Lexical analysis
│   ├── parser/        # Syntax parsing
│   ├── ast/           # Abstract syntax tree
│   ├── semantic/      # Semantic analysis
│   └── codegen/       # Code generation
│
├── runtime/
│   ├── core/          # Core runtime functionality
│   │   ├── vm.c/h     # Virtual machine
│   │   ├── object.c/h # Object system
│   │   ├── string_pool.c/h
│   │   ├── bootstrap.c/h
│   │   └── coroutine.c/h
│   │
│   ├── modules/       # Module system
│   │   ├── loader/    # Module loading and caching
│   │   │   ├── module_loader.c/h (renamed from module.c)
│   │   │   ├── module_cache.c/h
│   │   │   └── module_compiler.c/h
│   │   │
│   │   ├── formats/   # Module file formats
│   │   │   ├── module_archive.c/h
│   │   │   ├── module_bundle.c/h
│   │   │   └── module_format.c/h
│   │   │
│   │   ├── extensions/ # Module extensions
│   │   │   ├── module_hooks.c/h
│   │   │   ├── module_inspect.c/h
│   │   │   └── module_natives.c/h
│   │   │
│   │   └── lifecycle/ # Module lifecycle
│   │       ├── module_unload.c/h
│   │       └── builtin_modules.c/h
│   │
│   └── packages/      # Package management
│       ├── package.c/h
│       └── package_manager.c/h
│
├── stdlib/            # Standard library
├── utils/             # Utilities
└── tools/             # Command-line tools

include/               # Headers follow same structure
```

## Benefits
1. Clear separation of concerns
2. Easier navigation and maintenance
3. Better module system organization
4. Logical grouping of related functionality

## Migration Steps
1. Create new directory structure
2. Move files to appropriate locations
3. Update include paths in source files
4. Update CMakeLists.txt
5. Test build and functionality
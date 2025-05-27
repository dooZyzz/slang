# SwiftLang Development Guide

## Building from Source

```bash
# Clone repository
git clone https://github.com/yourusername/swiftlang.git
cd swiftlang

# Build with CMake
mkdir cmake-build-debug
cd cmake-build-debug
cmake ..
make -j8

# Run tests
make test
```

## Project Structure

```
swiftlang/
├── include/        # Header files
│   ├── ast/        # AST nodes
│   ├── lexer/      # Tokenizer
│   ├── parser/     # Parser
│   ├── vm/         # Virtual machine
│   └── runtime/    # Runtime system
├── src/            # Implementation files
├── tests/          # Unit tests
├── examples/       # Example programs
└── modules/        # Native modules (math, glfw, etc.)
```

## Architecture

### Compilation Pipeline
1. **Lexer** - Tokenizes source code
2. **Parser** - Builds Abstract Syntax Tree (AST)
3. **Compiler** - Generates bytecode from AST
4. **VM** - Executes bytecode

### Key Components

**Virtual Machine** (`vm/vm.c`)
- Stack-based bytecode interpreter
- Mark-and-sweep garbage collector
- Object system with prototypes

**Parser** (`parser/parser.c`)
- Recursive descent parser
- Generates AST nodes
- Error recovery

**Module System** (`runtime/module.c`)
- Module loading and caching
- Export/import handling (partially implemented)

## Running Tests

```bash
# Run all tests
make test

# Run specific test suite
./test_runner

# Run individual test
./lexer_unit_standalone
```

## Adding Features

1. **Add tokens** in `lexer/token.h`
2. **Update lexer** in `lexer/lexer.c`
3. **Add AST nodes** in `ast/ast.h`
4. **Update parser** in `parser/parser.c`
5. **Add opcodes** in `vm/chunk.h`
6. **Update compiler** in `codegen/compiler.c`
7. **Implement in VM** in `vm/vm.c`
8. **Add tests** in `tests/unit/`

## Debugging

```bash
# Enable all debug output
./swiftlang --debug-all script.swift

# Use GDB/LLDB
lldb ./swiftlang
(lldb) run examples/test.swift
```

## Code Style

- C11 standard
- 4-space indentation
- Snake_case for functions/variables
- PascalCase for types
- Descriptive names

## Current Issues

1. **Module imports** - Multi-file imports fail at runtime
2. **Extension syntax** - Parser not implemented
3. **Struct syntax** - Parser not implemented
4. **Type system** - No type checking yet

## Contributing

1. Fork the repository
2. Create feature branch
3. Write tests for new features
4. Ensure all tests pass
5. Submit pull request

See the source code for implementation details.
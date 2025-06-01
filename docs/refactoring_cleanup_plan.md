# Refactoring Cleanup Plan

## Current State
- Memory allocator refactoring is partially complete
- Main executable builds and runs
- Some tests are failing due to incomplete implementations
- Many refactored files exist alongside original files

## Immediate Tasks

### 1. Complete Critical Refactorings
These files need to be fully refactored to get tests passing:

#### Compiler (compiler_refactored.c)
- Current: Only has stub implementation
- Need: Full compilation logic from original compiler.c
- Priority: HIGH - Required for running any SwiftLang code

#### AST (ast_refactored.c)  
- Current: Missing some functions
- Need: Complete all AST node creation and manipulation functions
- Priority: HIGH - Required for parser and compiler

### 2. Fix Test Failures
- Lexer tests failing on string handling
- Need to ensure lexer_refactored.c handles all token types correctly
- Check if string interpolation is implemented

### 3. Complete Remaining Core Files
#### High Priority:
- symbol_table.c → symbol_table_refactored.c
- type.c → type_refactored.c
- visitor.c → visitor_refactored.c
- bytecode_format.c → bytecode_format_refactored.c

#### Medium Priority:
- module_compiler.c → module_compiler_refactored.c
- module_unload.c → module_unload_refactored.c
- ast_printer.c → ast_printer_refactored.c

#### Low Priority:
- cli.c → cli_refactored.c
- logger.c → logger_refactored.c
- version.c → version_refactored.c
- test_framework.c → test_framework_refactored.c
- syntax_test.c → syntax_test_refactored.c

### 4. Package Management
- package.c → package_refactored.c
- package_manager.c → package_manager_refactored.c

## Cleanup Process

### Phase 1: Complete Refactoring (Current)
1. Fix critical missing functions
2. Get all tests passing
3. Ensure examples run correctly

### Phase 2: Validation
1. Run full test suite
2. Test all example programs
3. Run memory leak detection
4. Performance benchmarking

### Phase 3: File Renaming
Once everything works:
1. Create git commit before renaming
2. Remove old implementations:
   ```bash
   # Remove old files
   rm src/lexer/lexer.c
   rm src/lexer/token.c
   rm src/parser/parser.c
   # ... etc
   ```
3. Rename refactored files:
   ```bash
   # Rename refactored files
   mv src/lexer/lexer_refactored.c src/lexer/lexer.c
   mv src/lexer/token_refactored.c src/lexer/token.c
   mv src/parser/parser_refactored.c src/parser/parser.c
   # ... etc
   ```
4. Update CMakeLists.txt to remove "_refactored" suffixes
5. Update any remaining includes

### Phase 4: Final Cleanup
1. Remove temporary files and scripts
2. Update documentation
3. Run final validation
4. Create final commit

## Success Criteria
- [ ] All unit tests pass
- [ ] All example programs run correctly
- [ ] No memory leaks detected
- [ ] Performance is equal or better than original
- [ ] All old implementations removed
- [ ] All refactored files renamed
- [ ] Documentation updated

## Next Immediate Steps
1. Complete compiler_refactored.c implementation
2. Fix lexer string handling
3. Complete AST missing functions
4. Run tests and fix failures iteratively
# Contributing to SwiftLang

Thank you for your interest in contributing to SwiftLang! We welcome contributions from the community and are grateful for any help you can provide.

## Code of Conduct

By participating in this project, you agree to abide by our Code of Conduct:
- Be respectful and inclusive
- Welcome newcomers and help them get started
- Focus on constructive criticism
- Respect differing viewpoints and experiences

## How to Contribute

### Reporting Issues

Before creating a new issue:
1. Check if the issue already exists
2. Try to reproduce the issue with the latest version
3. Collect relevant information (error messages, stack traces, minimal reproduction)

When creating an issue, please include:
- Clear title and description
- Steps to reproduce
- Expected behavior
- Actual behavior
- System information (OS, compiler version)

### Suggesting Features

We love new ideas! When suggesting features:
1. Check if it's already been suggested
2. Explain the use case
3. Provide examples of how it would work
4. Consider backward compatibility

### Pull Requests

1. **Fork and Clone**
   ```bash
   git clone https://github.com/yourusername/swiftlang.git
   cd swiftlang
   ```

2. **Create a Branch**
   ```bash
   git checkout -b feature/your-feature-name
   ```

3. **Make Changes**
   - Follow the existing code style
   - Add tests for new functionality
   - Update documentation as needed
   - Keep commits focused and atomic

4. **Test Your Changes**
   ```bash
   mkdir build && cd build
   cmake ..
   make -j8
   make test
   ```

5. **Commit Guidelines**
   - Use clear, descriptive commit messages
   - Format: `<type>: <subject>`
   - Types: `feat`, `fix`, `docs`, `style`, `refactor`, `test`, `chore`
   - Example: `feat: add string interpolation support`

6. **Push and Create PR**
   ```bash
   git push origin feature/your-feature-name
   ```

### Development Setup

#### Prerequisites
- CMake 3.10+
- C11 compatible compiler (gcc, clang)
- Python 3.6+ (for test scripts)
- Git

#### Building
```bash
mkdir cmake-build-debug
cd cmake-build-debug
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j8
```

#### Running Tests
```bash
# Run all tests
make test

# Run specific test suite
./lexer_unit_standalone

# Run with memory leak detection
valgrind ./swift_like_lang run test.swift
```

### Code Style

- Use 4 spaces for indentation (no tabs)
- Opening braces on same line
- Function names in camelCase
- Types and constants in PascalCase
- Macros in UPPER_SNAKE_CASE
- Always use braces for control structures
- Add comments for complex logic

Example:
```c
void processToken(Token* token) {
    if (token->type == TOKEN_IDENTIFIER) {
        // Handle identifier tokens
        handleIdentifier(token);
    } else {
        reportError("Unexpected token");
    }
}
```

### Testing Guidelines

- Write tests for all new functionality
- Ensure all tests pass before submitting PR
- Use the test framework macros:
  ```c
  TEST_CASE("array map with closures") {
      const char* code = "[1, 2, 3].map({ x in x * 2 })";
      TestResult* result = run_test_code(code);
      ASSERT_SUCCESS(result);
      ASSERT_OUTPUT_CONTAINS(result, "[2, 4, 6]");
  }
  ```

### Documentation

- Update README.md for user-facing changes
- Add inline documentation for complex functions
- Update language guide for new features
- Include examples in documentation

## Project Structure

```
├── include/        # Public headers
├── src/           # Implementation files
│   ├── lexer/     # Tokenization
│   ├── parser/    # AST generation
│   ├── semantic/  # Type checking
│   ├── codegen/   # Bytecode generation
│   ├── runtime/   # VM and runtime
│   └── stdlib/    # Built-in functions
├── tests/         # Test suites
├── examples/      # Example programs
└── docs/          # Documentation
```

## Areas for Contribution

### Good First Issues
- Improve error messages
- Add more examples
- Enhance documentation
- Write additional tests

### Intermediate
- Implement missing operators
- Add new built-in functions
- Improve performance
- Fix edge cases

### Advanced
- Type system enhancements
- Optimization passes
- New language features
- Platform-specific improvements

## Getting Help

- Check the [documentation](docs/)
- Look at [existing examples](examples/)
- Ask questions in issues (tag with "question")
- Review similar language implementations

## Recognition

Contributors will be:
- Listed in CONTRIBUTORS.md
- Mentioned in release notes
- Given credit in commit messages

Thank you for contributing to SwiftLang! 🚀
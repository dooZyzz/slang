# SwiftLang CLI Reference

## Commands

### `swiftlang [file]`
Run a SwiftLang file directly.

```bash
./swiftlang hello.swift
```

### `swiftlang repl`
Start the interactive REPL (Read-Eval-Print Loop).

```bash
./swiftlang repl
> print(2 + 2)
4
> exit
```

### `swiftlang init`
Initialize a new project in the current directory.

```bash
mkdir myproject && cd myproject
../swiftlang init
```

Creates:
- `module.json` - Project configuration
- `main.swift` - Entry point

### `swiftlang new <name>`
Create a new project in a new directory.

```bash
./swiftlang new myapp
cd myapp
```

### `swiftlang build`
Build the current project (creates .swiftmodule files).

```bash
./swiftlang build
```

Options:
- `--output <dir>` - Output directory (default: build/)
- `--optimize` - Enable optimizations
- `--emit-bytecode` - Save bytecode files

### `swiftlang run`
Run the current project or a specific file.

```bash
# Run project in current directory
./swiftlang run

# Run specific file
./swiftlang run main.swift
```

### `swiftlang test`
Run test files in the tests/ directory.

```bash
./swiftlang test
```

### `swiftlang help [command]`
Show help for a specific command.

```bash
./swiftlang help build
```

### `swiftlang version`
Display version information.

## Options

### Debug Options
- `--debug-tokens` - Print tokens during lexing
- `--debug-ast` - Print AST after parsing
- `--debug-bytecode` - Print bytecode after compilation
- `--debug-trace` - Trace execution
- `--debug-all` - Enable all debug output

### Common Options
- `-h, --help` - Show help message
- `-v, --verbose` - Enable verbose output
- `-q, --quiet` - Suppress non-error output

## Project Structure

A SwiftLang project uses `module.json`:

```json
{
  "name": "myapp",
  "version": "0.1.0",
  "description": "My SwiftLang app",
  "main": "main.swift",
  "type": "application"
}
```

## Environment Variables

- `SWIFTLANG_MODULE_PATH` - Additional module search paths (colon-separated)

## Examples

```bash
# Run a single file
./swiftlang examples/hello.swift

# Create and run a new project
./swiftlang new calculator
cd calculator
../swiftlang run

# Debug execution
./swiftlang --debug-trace script.swift

# Build with optimizations
./swiftlang build --optimize
```
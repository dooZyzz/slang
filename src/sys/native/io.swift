// sys.native.io - Native I/O operations
mod sys.native.io

// This module provides native I/O operations
// The actual implementation is in the built-in io module

// Re-export built-in io functions
import { print, println, readLine, readFile, writeFile } from io

export { print, println, readLine, readFile, writeFile }
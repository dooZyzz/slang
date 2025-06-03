# unistd.h Windows Porting Guide

This document summarizes the unistd.h functions used in the Slang codebase and their Windows alternatives.

## Functions Used and Their Windows Alternatives

### 1. `unlink()` - Delete a file
**Used in:**
- `src/runtime/modules/lifecycle/module_unload.c` (line 83)
- `tests/unit/test_multi_module_unit.c` (line 66)
- `tests/unit/test_module_system_unit.c` (lines 59, 135)

**Windows alternative:**
```c
#ifdef _WIN32
    #include <windows.h>
    #define unlink(path) _unlink(path)
    // or use DeleteFile(path) from Windows API
#endif
```

### 2. `getpid()` - Get process ID
**Used in:**
- `src/runtime/modules/loader/module_loader.c` (line 977)

**Windows alternative:**
```c
#ifdef _WIN32
    #include <process.h>
    #define getpid() _getpid()
    // or use GetCurrentProcessId() from Windows API
#endif
```

### 3. `readlink()` - Read symbolic link
**Used in:**
- `src/utils/cli.c` (line 1353)

**Windows alternative:**
```c
#ifdef _WIN32
    // Windows doesn't have a direct equivalent
    // For reading executable path, use:
    GetModuleFileName(NULL, exe_path, sizeof(exe_path));
#endif
```

### 4. `popen()` / `pclose()` - Execute command and read output
**Used in:**
- `src/utils/compiler_wrapper.c` (lines 76, 81, 87)

**Windows alternative:**
```c
#ifdef _WIN32
    #define popen(cmd, mode) _popen(cmd, mode)
    #define pclose(stream) _pclose(stream)
#endif
```

### 5. `getcwd()` - Get current working directory
**Used in:**
- `src/runtime/packages/package_manager.c` (line 162)

**Windows alternative:**
```c
#ifdef _WIN32
    #include <direct.h>
    #define getcwd(buf, size) _getcwd(buf, size)
    // or use GetCurrentDirectory() from Windows API
#endif
```

## Platform-specific Header Organization

All `#include <unistd.h>` statements have been wrapped with:
```c
#ifndef _WIN32
#include <unistd.h>
#endif
```

## Recommended Approach

1. Create a platform compatibility header (e.g., `platform_compat.h`) that provides unified interfaces:

```c
#ifndef PLATFORM_COMPAT_H
#define PLATFORM_COMPAT_H

#ifdef _WIN32
    #include <windows.h>
    #include <process.h>
    #include <direct.h>
    #include <io.h>
    
    #define unlink(path) _unlink(path)
    #define getpid() _getpid()
    #define getcwd(buf, size) _getcwd(buf, size)
    #define popen(cmd, mode) _popen(cmd, mode)
    #define pclose(stream) _pclose(stream)
    
    // Custom readlink implementation for Windows
    static inline ssize_t readlink(const char* path, char* buf, size_t bufsiz) {
        // For executable path:
        DWORD len = GetModuleFileName(NULL, buf, bufsiz);
        return len > 0 ? len : -1;
    }
#else
    #include <unistd.h>
#endif

#endif // PLATFORM_COMPAT_H
```

2. Replace all `#include <unistd.h>` with `#include "utils/platform_compat.h"`

3. Test on Windows to ensure all functions work correctly.
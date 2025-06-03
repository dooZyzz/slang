#ifndef PLATFORM_COMPAT_H
#define PLATFORM_COMPAT_H

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>

#ifdef _WIN32
    #include <windows.h>
    #include <io.h>
    #include <direct.h>
    #include <process.h>
    
    // File access modes
    #define F_OK 0
    #define X_OK 1
    #define W_OK 2
    #define R_OK 4
    
    // POSIX function replacements
    #define access _access
    #define unlink _unlink
    #define getcwd _getcwd
    #define getpid _getpid
    #define popen _popen
    #define pclose _pclose
    #define mkdir(path, mode) _mkdir(path)
    #define chmod _chmod
    
    // Case-insensitive string comparison
    #define strcasecmp _stricmp
    #define strncasecmp _strnicmp
    
    // Directory separator
    #define PATH_SEPARATOR "\\"
    
    // Dynamic library extensions
    #define DYLIB_EXT ".dll"
    
    // Path constants
    #define PATH_MAX MAX_PATH
    
    // File type checks
    #define S_ISDIR(mode) (((mode) & _S_IFMT) == _S_IFDIR)
    #define S_ISREG(mode) (((mode) & _S_IFMT) == _S_IFREG)
    
    // realpath equivalent
    static inline char* realpath(const char* path, char* resolved_path) {
        return _fullpath(resolved_path, path, PATH_MAX);
    }
    
    // POSIX file descriptors
    #define STDOUT_FILENO 1
    #define STDERR_FILENO 2
    #define STDIN_FILENO 0
    
    // ssize_t definition
    typedef long long ssize_t;
    
    // Terminal size structure (dummy for Windows)
    struct winsize {
        unsigned short ws_row;
        unsigned short ws_col;
        unsigned short ws_xpixel;
        unsigned short ws_ypixel;
    };
    
    #define TIOCGWINSZ 0x5413
    
    // __VERSION__ for MSVC
    #ifndef __VERSION__
        #ifdef _MSC_VER
            #define __VERSION__ "MSVC"
        #else
            #define __VERSION__ "Unknown"
        #endif
    #endif
    
    // basename implementation for Windows
    static inline char* basename(char* path) {
        static char fname[_MAX_FNAME + _MAX_EXT];
        char ext[_MAX_EXT];
        _splitpath(path, NULL, NULL, fname, ext);
        strcat(fname, ext);
        return fname;
    }
    
    // Additional function for dlsym (temporary)
    #define dlsym GetProcAddress
    
#else
    #include <unistd.h>
    #include <strings.h>
    #include <sys/stat.h>
    #include <limits.h>
    
    // Directory separator
    #define PATH_SEPARATOR "/"
    
    // Dynamic library extensions
    #ifdef __APPLE__
        #define DYLIB_EXT ".dylib"
    #else
        #define DYLIB_EXT ".so"
    #endif
#endif

// Cross-platform directory creation
static inline int platform_mkdir(const char* path) {
#ifdef _WIN32
    return _mkdir(path);
#else
    return mkdir(path, 0755);
#endif
}

// Cross-platform temporary directory creation
static inline char* platform_mkdtemp(char* template_str) {
#ifdef _WIN32
    // Windows doesn't have mkdtemp, use GetTempPath + unique name
    if (_mktemp(template_str) == NULL) {
        return NULL;
    }
    if (_mkdir(template_str) != 0) {
        return NULL;
    }
    return template_str;
#else
    return mkdtemp(template_str);
#endif
}

#endif /* PLATFORM_COMPAT_H */
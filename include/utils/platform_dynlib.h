#ifndef PLATFORM_DYNLIB_H
#define PLATFORM_DYNLIB_H

#ifdef _WIN32
    #include <windows.h>
    
    typedef HMODULE platform_dynlib_t;
    
    static inline platform_dynlib_t platform_dynlib_open(const char* filename) {
        return LoadLibraryA(filename);
    }
    
    static inline void* platform_dynlib_symbol(platform_dynlib_t handle, const char* symbol) {
        return (void*)GetProcAddress(handle, symbol);
    }
    
    static inline void platform_dynlib_close(platform_dynlib_t handle) {
        FreeLibrary(handle);
    }
    
    static inline const char* platform_dynlib_error(void) {
        static char buffer[256];
        DWORD error = GetLastError();
        FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, NULL, error, 0, buffer, sizeof(buffer), NULL);
        return buffer;
    }
    
#else
    #include <dlfcn.h>
    
    typedef void* platform_dynlib_t;
    
    static inline platform_dynlib_t platform_dynlib_open(const char* filename) {
        return dlopen(filename, RTLD_LAZY);
    }
    
    static inline void* platform_dynlib_symbol(platform_dynlib_t handle, const char* symbol) {
        return dlsym(handle, symbol);
    }
    
    static inline void platform_dynlib_close(platform_dynlib_t handle) {
        dlclose(handle);
    }
    
    static inline const char* platform_dynlib_error(void) {
        return dlerror();
    }
#endif

#endif /* PLATFORM_DYNLIB_H */
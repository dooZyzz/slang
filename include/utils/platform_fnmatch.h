#ifndef PLATFORM_FNMATCH_H
#define PLATFORM_FNMATCH_H

#ifdef _WIN32
    // Simple fnmatch implementation for Windows
    #define FNM_NOMATCH 1
    #define FNM_PATHNAME (1 << 0)
    #define FNM_NOESCAPE (1 << 1)
    
    static inline int fnmatch(const char* pattern, const char* string, int flags) {
        // Very basic glob matching - just handle * and ?
        const char* s = string;
        const char* p = pattern;
        
        while (*p) {
            if (*p == '*') {
                // Skip multiple *
                while (*p == '*') p++;
                if (!*p) return 0; // Pattern ends with *, matches everything
                
                // Try to match the rest of the pattern
                while (*s) {
                    if (fnmatch(p, s, flags) == 0) return 0;
                    s++;
                }
                return FNM_NOMATCH;
            }
            if (*p == '?') {
                if (!*s) return FNM_NOMATCH;
                s++;
                p++;
            } else {
                if (*p != *s) return FNM_NOMATCH;
                s++;
                p++;
            }
        }
        
        return *s ? FNM_NOMATCH : 0;
    }
#else
    #include <fnmatch.h>
#endif

#endif /* PLATFORM_FNMATCH_H */
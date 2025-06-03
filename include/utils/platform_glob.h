#ifndef PLATFORM_GLOB_H
#define PLATFORM_GLOB_H

#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
    #include <windows.h>
    
    #define GLOB_ERR     (1 << 0)
    #define GLOB_MARK    (1 << 1)
    #define GLOB_NOSORT  (1 << 2)
    #define GLOB_NOCHECK (1 << 3)
    #define GLOB_NOESCAPE (1 << 4)
    #define GLOB_TILDE    (1 << 5)
    
    #define GLOB_NOSPACE 1
    #define GLOB_ABORTED 2
    #define GLOB_NOMATCH 3
    
    typedef struct {
        size_t gl_pathc;
        char** gl_pathv;
        size_t gl_offs;
    } glob_t;
    
    static inline int glob(const char* pattern, int flags, int (*errfunc)(const char*, int), glob_t* pglob) {
        WIN32_FIND_DATAA find_data;
        HANDLE hFind;
        char** pathv = NULL;
        size_t pathc = 0;
        size_t capacity = 16;
        
        pathv = (char**)malloc(capacity * sizeof(char*));
        if (!pathv) return GLOB_NOSPACE;
        
        hFind = FindFirstFileA(pattern, &find_data);
        if (hFind == INVALID_HANDLE_VALUE) {
            if (flags & GLOB_NOCHECK) {
                pathv[0] = strdup(pattern);
                pathc = 1;
            } else {
                free(pathv);
                return GLOB_NOMATCH;
            }
        } else {
            do {
                if (pathc >= capacity) {
                    capacity *= 2;
                    char** new_pathv = (char**)realloc(pathv, capacity * sizeof(char*));
                    if (!new_pathv) {
                        for (size_t i = 0; i < pathc; i++) free(pathv[i]);
                        free(pathv);
                        FindClose(hFind);
                        return GLOB_NOSPACE;
                    }
                    pathv = new_pathv;
                }
                
                pathv[pathc] = strdup(find_data.cFileName);
                if (!pathv[pathc]) {
                    for (size_t i = 0; i < pathc; i++) free(pathv[i]);
                    free(pathv);
                    FindClose(hFind);
                    return GLOB_NOSPACE;
                }
                pathc++;
            } while (FindNextFileA(hFind, &find_data));
            
            FindClose(hFind);
        }
        
        pglob->gl_pathc = pathc;
        pglob->gl_pathv = pathv;
        pglob->gl_offs = 0;
        
        return 0;
    }
    
    static inline void globfree(glob_t* pglob) {
        if (pglob->gl_pathv) {
            for (size_t i = 0; i < pglob->gl_pathc; i++) {
                if (pglob->gl_pathv[i]) free(pglob->gl_pathv[i]);
            }
            free(pglob->gl_pathv);
            pglob->gl_pathv = NULL;
            pglob->gl_pathc = 0;
        }
    }
    
#else
    #include <glob.h>
#endif

#endif /* PLATFORM_GLOB_H */
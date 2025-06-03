#ifndef PLATFORM_DIR_H
#define PLATFORM_DIR_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
    #include <windows.h>
    #include <io.h>
    
    typedef struct {
        HANDLE handle;
        WIN32_FIND_DATAA find_data;
        bool first_read;
        bool valid;
    } platform_dir_t;
    
    typedef struct {
        char name[MAX_PATH];
        bool is_directory;
    } platform_dirent_t;
    
#else
    #include <dirent.h>
    #include <sys/stat.h>
    #include <limits.h>
    
    typedef struct {
        DIR* dir;
        char* path;
    } platform_dir_t;
    
    typedef struct {
        char name[256];
        bool is_directory;
    } platform_dirent_t;
#endif

// Implementation
#ifdef _WIN32
static inline platform_dir_t* platform_opendir(const char* path) {
    platform_dir_t* dir = (platform_dir_t*)malloc(sizeof(platform_dir_t));
    if (!dir) return NULL;
    
    char search_path[MAX_PATH];
    snprintf(search_path, MAX_PATH, "%s\\*", path);
    
    dir->handle = FindFirstFileA(search_path, &dir->find_data);
    dir->first_read = true;
    dir->valid = (dir->handle != INVALID_HANDLE_VALUE);
    
    if (!dir->valid) {
        free(dir);
        return NULL;
    }
    
    return dir;
}

static inline bool platform_readdir(platform_dir_t* dir, platform_dirent_t* entry) {
    if (!dir || !dir->valid) return false;
    
    if (dir->first_read) {
        dir->first_read = false;
    } else {
        if (!FindNextFileA(dir->handle, &dir->find_data)) {
            return false;
        }
    }
    
    // Skip . and ..
    while (strcmp(dir->find_data.cFileName, ".") == 0 || 
           strcmp(dir->find_data.cFileName, "..") == 0) {
        if (!FindNextFileA(dir->handle, &dir->find_data)) {
            return false;
        }
    }
    
    strncpy(entry->name, dir->find_data.cFileName, MAX_PATH - 1);
    entry->name[MAX_PATH - 1] = '\0';
    entry->is_directory = (dir->find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    
    return true;
}

static inline void platform_closedir(platform_dir_t* dir) {
    if (dir) {
        if (dir->valid && dir->handle != INVALID_HANDLE_VALUE) {
            FindClose(dir->handle);
        }
        free(dir);
    }
}

#else

static inline platform_dir_t* platform_opendir(const char* path) {
    platform_dir_t* pdir = (platform_dir_t*)malloc(sizeof(platform_dir_t));
    if (!pdir) return NULL;
    
    pdir->dir = opendir(path);
    if (!pdir->dir) {
        free(pdir);
        return NULL;
    }
    
    pdir->path = strdup(path);
    return pdir;
}

static inline bool platform_readdir(platform_dir_t* pdir, platform_dirent_t* entry) {
    if (!pdir || !pdir->dir) return false;
    
    struct dirent* de;
    while ((de = readdir(pdir->dir)) != NULL) {
        // Skip . and ..
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) {
            continue;
        }
        
        strncpy(entry->name, de->d_name, sizeof(entry->name) - 1);
        entry->name[sizeof(entry->name) - 1] = '\0';
        
        // Check if it's a directory
        char full_path[PATH_MAX];
        snprintf(full_path, PATH_MAX, "%s/%s", pdir->path, de->d_name);
        
        struct stat st;
        if (stat(full_path, &st) == 0) {
            entry->is_directory = S_ISDIR(st.st_mode);
        } else {
            entry->is_directory = false;
        }
        
        return true;
    }
    
    return false;
}

static inline void platform_closedir(platform_dir_t* pdir) {
    if (pdir) {
        if (pdir->dir) closedir(pdir->dir);
        if (pdir->path) free(pdir->path);
        free(pdir);
    }
}

#endif

#endif /* PLATFORM_DIR_H */
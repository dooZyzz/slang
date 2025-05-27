#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <time.h>
#include "runtime/module.h"
#include "vm/vm.h"
#include "vm/object.h"
#include "vm/array.h"

// Native function: readFile(path: String) -> String?
static TaggedValue native_read_file(int arg_count, TaggedValue* args) {
    if (arg_count != 1 || !IS_STRING(args[0])) {
        return NIL_VAL;
    }
    
    const char* path = AS_STRING(args[0]);
    FILE* file = fopen(path, "rb");
    if (!file) {
        return NIL_VAL;
    }
    
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    char* buffer = malloc(size + 1);
    if (!buffer) {
        fclose(file);
        return NIL_VAL;
    }
    
    size_t read = fread(buffer, 1, size, file);
    buffer[read] = '\0';
    fclose(file);
    
    return STRING_VAL(buffer);
}

// Native function: writeFile(path: String, content: String) -> Bool
static TaggedValue native_write_file(int arg_count, TaggedValue* args) {
    if (arg_count != 2 || !IS_STRING(args[0]) || !IS_STRING(args[1])) {
        return BOOL_VAL(false);
    }
    
    const char* path = AS_STRING(args[0]);
    const char* content = AS_STRING(args[1]);
    
    FILE* file = fopen(path, "wb");
    if (!file) {
        return BOOL_VAL(false);
    }
    
    size_t len = strlen(content);
    size_t written = fwrite(content, 1, len, file);
    fclose(file);
    
    return BOOL_VAL(written == len);
}

// Native function: appendFile(path: String, content: String) -> Bool
static TaggedValue native_append_file(int arg_count, TaggedValue* args) {
    if (arg_count != 2 || !IS_STRING(args[0]) || !IS_STRING(args[1])) {
        return BOOL_VAL(false);
    }
    
    const char* path = AS_STRING(args[0]);
    const char* content = AS_STRING(args[1]);
    
    FILE* file = fopen(path, "ab");
    if (!file) {
        return BOOL_VAL(false);
    }
    
    size_t len = strlen(content);
    size_t written = fwrite(content, 1, len, file);
    fclose(file);
    
    return BOOL_VAL(written == len);
}

// Native function: exists(path: String) -> Bool
static TaggedValue native_exists(int arg_count, TaggedValue* args) {
    if (arg_count != 1 || !IS_STRING(args[0])) {
        return BOOL_VAL(false);
    }
    
    const char* path = AS_STRING(args[0]);
    struct stat st;
    return BOOL_VAL(stat(path, &st) == 0);
}

// Native function: isFile(path: String) -> Bool
static TaggedValue native_is_file(int arg_count, TaggedValue* args) {
    if (arg_count != 1 || !IS_STRING(args[0])) {
        return BOOL_VAL(false);
    }
    
    const char* path = AS_STRING(args[0]);
    struct stat st;
    if (stat(path, &st) != 0) {
        return BOOL_VAL(false);
    }
    
    return BOOL_VAL(S_ISREG(st.st_mode));
}

// Native function: isDirectory(path: String) -> Bool
static TaggedValue native_is_directory(int arg_count, TaggedValue* args) {
    if (arg_count != 1 || !IS_STRING(args[0])) {
        return BOOL_VAL(false);
    }
    
    const char* path = AS_STRING(args[0]);
    struct stat st;
    if (stat(path, &st) != 0) {
        return BOOL_VAL(false);
    }
    
    return BOOL_VAL(S_ISDIR(st.st_mode));
}

// Native function: mkdir(path: String) -> Bool
static TaggedValue native_mkdir(int arg_count, TaggedValue* args) {
    if (arg_count != 1 || !IS_STRING(args[0])) {
        return BOOL_VAL(false);
    }
    
    const char* path = AS_STRING(args[0]);
    return BOOL_VAL(mkdir(path, 0755) == 0);
}

// Native function: remove(path: String) -> Bool
static TaggedValue native_remove(int arg_count, TaggedValue* args) {
    if (arg_count != 1 || !IS_STRING(args[0])) {
        return BOOL_VAL(false);
    }
    
    const char* path = AS_STRING(args[0]);
    return BOOL_VAL(remove(path) == 0);
}

// Native function: listDir(path: String) -> Array?
static TaggedValue native_list_dir(int arg_count, TaggedValue* args) {
    if (arg_count != 1 || !IS_STRING(args[0])) {
        return NIL_VAL;
    }
    
    const char* path = AS_STRING(args[0]);
    DIR* dir = opendir(path);
    if (!dir) {
        return NIL_VAL;
    }
    
    Object* array = array_create_with_capacity(16);
    struct dirent* entry;
    
    while ((entry = readdir(dir)) != NULL) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        array_push(array, STRING_VAL(strdup(entry->d_name)));
    }
    
    closedir(dir);
    return OBJECT_VAL(array);
}

// Native function: getStats(path: String) -> Object?
static TaggedValue native_get_stats(int arg_count, TaggedValue* args) {
    if (arg_count != 1 || !IS_STRING(args[0])) {
        return NIL_VAL;
    }
    
    const char* path = AS_STRING(args[0]);
    struct stat st;
    if (stat(path, &st) != 0) {
        return NIL_VAL;
    }
    
    Object* stats = object_create();
    object_set_property(stats, "size", NUMBER_VAL(st.st_size));
    object_set_property(stats, "isFile", BOOL_VAL(S_ISREG(st.st_mode)));
    object_set_property(stats, "isDirectory", BOOL_VAL(S_ISDIR(st.st_mode)));
    object_set_property(stats, "mode", NUMBER_VAL(st.st_mode));
    object_set_property(stats, "mtime", NUMBER_VAL(st.st_mtime));
    object_set_property(stats, "atime", NUMBER_VAL(st.st_atime));
    object_set_property(stats, "ctime", NUMBER_VAL(st.st_ctime));
    
    return OBJECT_VAL(stats);
}

// Native function: getcwd() -> String
static TaggedValue native_getcwd(int arg_count, TaggedValue* args) {
    char buffer[4096];
    if (getcwd(buffer, sizeof(buffer)) == NULL) {
        return NIL_VAL;
    }
    return STRING_VAL(strdup(buffer));
}

// Native function: chdir(path: String) -> Bool
static TaggedValue native_chdir(int arg_count, TaggedValue* args) {
    if (arg_count != 1 || !IS_STRING(args[0])) {
        return BOOL_VAL(false);
    }
    
    const char* path = AS_STRING(args[0]);
    return BOOL_VAL(chdir(path) == 0);
}

// Native function: rename(oldPath: String, newPath: String) -> Bool
static TaggedValue native_rename(int arg_count, TaggedValue* args) {
    if (arg_count != 2 || !IS_STRING(args[0]) || !IS_STRING(args[1])) {
        return BOOL_VAL(false);
    }
    
    const char* old_path = AS_STRING(args[0]);
    const char* new_path = AS_STRING(args[1]);
    
    return BOOL_VAL(rename(old_path, new_path) == 0);
}

// Module initialization
bool swiftlang_module_init(Module* module) {
    // File operations
    module_register_native_function(module, "readFile", native_read_file);
    module_register_native_function(module, "writeFile", native_write_file);
    module_register_native_function(module, "appendFile", native_append_file);
    
    // Path operations
    module_register_native_function(module, "exists", native_exists);
    module_register_native_function(module, "isFile", native_is_file);
    module_register_native_function(module, "isDirectory", native_is_directory);
    module_register_native_function(module, "mkdir", native_mkdir);
    module_register_native_function(module, "remove", native_remove);
    module_register_native_function(module, "rename", native_rename);
    
    // Directory operations
    module_register_native_function(module, "listDir", native_list_dir);
    module_register_native_function(module, "getcwd", native_getcwd);
    module_register_native_function(module, "chdir", native_chdir);
    
    // File stats
    module_register_native_function(module, "getStats", native_get_stats);
    
    // Path separator
    module_export(module, "separator", STRING_VAL("/"));
    
    return true;
}
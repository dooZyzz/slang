#include "stdlib/file.h"
#include "vm/object.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

// File handle wrapper
typedef struct {
    FILE* file;
    char* mode;
    char* path;
} FileHandle;

// Helper to create a file handle object
static Object* create_file_handle(FILE* file, const char* mode, const char* path) {
    FileHandle* handle = malloc(sizeof(FileHandle));
    handle->file = file;
    handle->mode = strdup(mode);
    handle->path = strdup(path);
    
    Object* obj = object_create();
    // Store the handle as a native pointer property
    TaggedValue handle_val = {.type = VAL_NATIVE, .as.native = (NativeFn)handle};
    object_set_property(obj, "_handle", handle_val);
    object_set_property(obj, "path", STRING_VAL(strdup(path)));
    object_set_property(obj, "mode", STRING_VAL(strdup(mode)));
    
    return obj;
}

// Helper to get file handle from object
static FileHandle* get_file_handle(Object* obj) {
    TaggedValue* handle_val = object_get_property(obj, "_handle");
    if (handle_val && handle_val->type == VAL_NATIVE) {
        return (FileHandle*)handle_val->as.native;
    }
    return NULL;
}

// File.open(path, mode) - returns file handle object
TaggedValue file_open(int arg_count, TaggedValue* args) {
    if (arg_count < 2 || !IS_STRING(args[0]) || !IS_STRING(args[1])) {
        return NIL_VAL;
    }
    
    const char* path = AS_STRING(args[0]);
    const char* mode = AS_STRING(args[1]);
    
    FILE* file = fopen(path, mode);
    if (!file) {
        // Could return an error object instead
        return NIL_VAL;
    }
    
    Object* handle = create_file_handle(file, mode, path);
    
    // Add methods to the file handle
    object_set_property(handle, "close", NATIVE_VAL(file_close));
    object_set_property(handle, "read", NATIVE_VAL(file_read));
    object_set_property(handle, "write", NATIVE_VAL(file_write));
    object_set_property(handle, "readLine", NATIVE_VAL(file_readLine));
    object_set_property(handle, "writeLine", NATIVE_VAL(file_writeLine));
    
    return OBJECT_VAL(handle);
}

// file.close() - closes the file
TaggedValue file_close(int arg_count, TaggedValue* args) {
    if (arg_count < 1 || !IS_OBJECT(args[0])) {
        return NIL_VAL;
    }
    
    Object* obj = AS_OBJECT(args[0]);
    FileHandle* handle = get_file_handle(obj);
    
    if (!handle || !handle->file) {
        return BOOL_VAL(false);
    }
    
    int result = fclose(handle->file);
    handle->file = NULL;
    
    return BOOL_VAL(result == 0);
}

// file.read() - reads entire file content
TaggedValue file_read(int arg_count, TaggedValue* args) {
    if (arg_count < 1 || !IS_OBJECT(args[0])) {
        return NIL_VAL;
    }
    
    Object* obj = AS_OBJECT(args[0]);
    FileHandle* handle = get_file_handle(obj);
    
    if (!handle || !handle->file) {
        return NIL_VAL;
    }
    
    // Get file size
    long current = ftell(handle->file);
    fseek(handle->file, 0, SEEK_END);
    long size = ftell(handle->file);
    fseek(handle->file, current, SEEK_SET);
    
    if (size < 0) {
        return NIL_VAL;
    }
    
    // Read content
    char* buffer = malloc(size + 1);
    size_t read = fread(buffer, 1, size, handle->file);
    buffer[read] = '\0';
    
    return STRING_VAL(buffer);
}

// file.write(content) - writes content to file
TaggedValue file_write(int arg_count, TaggedValue* args) {
    if (arg_count < 2 || !IS_OBJECT(args[0]) || !IS_STRING(args[1])) {
        return NIL_VAL;
    }
    
    Object* obj = AS_OBJECT(args[0]);
    FileHandle* handle = get_file_handle(obj);
    
    if (!handle || !handle->file) {
        return BOOL_VAL(false);
    }
    
    const char* content = AS_STRING(args[1]);
    size_t len = strlen(content);
    size_t written = fwrite(content, 1, len, handle->file);
    
    return BOOL_VAL(written == len);
}

// file.readLine() - reads a line from file
TaggedValue file_readLine(int arg_count, TaggedValue* args) {
    if (arg_count < 1 || !IS_OBJECT(args[0])) {
        return NIL_VAL;
    }
    
    Object* obj = AS_OBJECT(args[0]);
    FileHandle* handle = get_file_handle(obj);
    
    if (!handle || !handle->file) {
        return NIL_VAL;
    }
    
    char buffer[1024];
    if (fgets(buffer, sizeof(buffer), handle->file)) {
        // Remove newline if present
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len-1] == '\n') {
            buffer[len-1] = '\0';
        }
        return STRING_VAL(strdup(buffer));
    }
    
    return NIL_VAL;
}

// file.writeLine(line) - writes a line to file
TaggedValue file_writeLine(int arg_count, TaggedValue* args) {
    if (arg_count < 2 || !IS_OBJECT(args[0]) || !IS_STRING(args[1])) {
        return NIL_VAL;
    }
    
    Object* obj = AS_OBJECT(args[0]);
    FileHandle* handle = get_file_handle(obj);
    
    if (!handle || !handle->file) {
        return BOOL_VAL(false);
    }
    
    const char* line = AS_STRING(args[1]);
    int result = fprintf(handle->file, "%s\n", line);
    
    return BOOL_VAL(result >= 0);
}

// File.exists(path) - checks if file exists
TaggedValue file_exists(int arg_count, TaggedValue* args) {
    if (arg_count < 1 || !IS_STRING(args[0])) {
        return BOOL_VAL(false);
    }
    
    const char* path = AS_STRING(args[0]);
    struct stat st;
    return BOOL_VAL(stat(path, &st) == 0 && S_ISREG(st.st_mode));
}

// File.delete(path) - deletes a file
TaggedValue file_delete(int arg_count, TaggedValue* args) {
    if (arg_count < 1 || !IS_STRING(args[0])) {
        return BOOL_VAL(false);
    }
    
    const char* path = AS_STRING(args[0]);
    return BOOL_VAL(unlink(path) == 0);
}

// File.copy(src, dest) - copies a file
TaggedValue file_copy(int arg_count, TaggedValue* args) {
    if (arg_count < 2 || !IS_STRING(args[0]) || !IS_STRING(args[1])) {
        return BOOL_VAL(false);
    }
    
    const char* src = AS_STRING(args[0]);
    const char* dest = AS_STRING(args[1]);
    
    FILE* in = fopen(src, "rb");
    if (!in) return BOOL_VAL(false);
    
    FILE* out = fopen(dest, "wb");
    if (!out) {
        fclose(in);
        return BOOL_VAL(false);
    }
    
    char buffer[4096];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), in)) > 0) {
        if (fwrite(buffer, 1, bytes, out) != bytes) {
            fclose(in);
            fclose(out);
            return BOOL_VAL(false);
        }
    }
    
    fclose(in);
    fclose(out);
    return BOOL_VAL(true);
}

// File.move(src, dest) - moves/renames a file
TaggedValue file_move(int arg_count, TaggedValue* args) {
    if (arg_count < 2 || !IS_STRING(args[0]) || !IS_STRING(args[1])) {
        return BOOL_VAL(false);
    }
    
    const char* src = AS_STRING(args[0]);
    const char* dest = AS_STRING(args[1]);
    
    return BOOL_VAL(rename(src, dest) == 0);
}

// Dir.create(path) - creates a directory
TaggedValue dir_create(int arg_count, TaggedValue* args) {
    if (arg_count < 1 || !IS_STRING(args[0])) {
        return BOOL_VAL(false);
    }
    
    const char* path = AS_STRING(args[0]);
    return BOOL_VAL(mkdir(path, 0755) == 0);
}

// Dir.delete(path) - deletes an empty directory
TaggedValue dir_delete(int arg_count, TaggedValue* args) {
    if (arg_count < 1 || !IS_STRING(args[0])) {
        return BOOL_VAL(false);
    }
    
    const char* path = AS_STRING(args[0]);
    return BOOL_VAL(rmdir(path) == 0);
}

// Dir.exists(path) - checks if directory exists
TaggedValue dir_exists(int arg_count, TaggedValue* args) {
    if (arg_count < 1 || !IS_STRING(args[0])) {
        return BOOL_VAL(false);
    }
    
    const char* path = AS_STRING(args[0]);
    struct stat st;
    return BOOL_VAL(stat(path, &st) == 0 && S_ISDIR(st.st_mode));
}

// Dir.list(path) - lists directory contents
TaggedValue dir_list(int arg_count, TaggedValue* args) {
    if (arg_count < 1 || !IS_STRING(args[0])) {
        return NIL_VAL;
    }
    
    const char* path = AS_STRING(args[0]);
    DIR* dir = opendir(path);
    if (!dir) {
        return NIL_VAL;
    }
    
    Object* result = array_create();
    struct dirent* entry;
    
    while ((entry = readdir(dir)) != NULL) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        array_push(result, STRING_VAL(strdup(entry->d_name)));
    }
    
    closedir(dir);
    return OBJECT_VAL(result);
}
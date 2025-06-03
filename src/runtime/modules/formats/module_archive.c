#include "runtime/modules/formats/module_archive.h"
#include "utils/allocators.h"
#include "runtime/modules/module_allocator_macros.h"
#include "utils/platform_compat.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#ifndef _WIN32
#include <unistd.h>
#endif

// Include miniz for ZIP support
#include "miniz.h"

#define ARCHIVE_MAGIC "SWIFTMOD"
#define ARCHIVE_VERSION 1

// Global archive format setting (default to ZIP)
static ArchiveFormat g_archive_format = ARCHIVE_FORMAT_ZIP;

typedef struct {
    char name[256];
    uint32_t size;
    uint32_t offset;
    uint8_t type; // 0=file, 1=directory
} ArchiveEntry;

struct ModuleArchive {
    char* path;
    FILE* file;
    bool write_mode;
    ArchiveFormat format;
    
    // For custom format
    struct {
        ArchiveEntry* entries;
        size_t count;
        size_t capacity;
    } entries;
    
    // Buffer for file data during writing (custom format)
    struct {
        uint8_t* data;
        size_t size;
        size_t capacity;
    } buffer;
    
    // For ZIP format
    mz_zip_archive zip_archive;
    bool zip_initialized;
};

ModuleArchive* module_archive_create(const char* path) {
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_MODULES);
    Allocator* str_alloc = allocators_get(ALLOC_SYSTEM_STRINGS);
    
    ModuleArchive* archive = MODULES_NEW_ZERO(ModuleArchive);
    archive->path = STRINGS_STRDUP(path);
    archive->write_mode = true;
    archive->format = g_archive_format;
    
    if (archive->format == ARCHIVE_FORMAT_ZIP) {
        // Initialize ZIP archive for writing
        memset(&archive->zip_archive, 0, sizeof(archive->zip_archive));
        if (!mz_zip_writer_init_file(&archive->zip_archive, path, 0)) {
            size_t path_len = strlen(archive->path) + 1;
            STRINGS_FREE(archive->path, path_len);
            MODULES_FREE(archive, sizeof(ModuleArchive));
            return NULL;
        }
        archive->zip_initialized = true;
    } else {
        // Custom format initialization
        archive->entries.capacity = 16;
        archive->entries.entries = MODULES_NEW_ARRAY_ZERO(ArchiveEntry, archive->entries.capacity);
        
        archive->buffer.capacity = 1024 * 1024; // 1MB initial buffer
        archive->buffer.data = MODULES_ALLOC(archive->buffer.capacity);
    }
    
    return archive;
}

void module_archive_destroy(ModuleArchive* archive) {
    if (!archive) return;
    
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_MODULES);
    Allocator* str_alloc = allocators_get(ALLOC_SYSTEM_STRINGS);
    
    if (archive->zip_initialized) {
        if (archive->write_mode) {
            mz_zip_writer_finalize_archive(&archive->zip_archive);
            mz_zip_writer_end(&archive->zip_archive);
        } else {
            mz_zip_reader_end(&archive->zip_archive);
        }
    }
    
    if (archive->file) {
        fclose(archive->file);
    }
    
    size_t path_len = strlen(archive->path) + 1;
    STRINGS_FREE(archive->path, path_len);
    
    if (archive->entries.entries) {
        MODULES_FREE(archive->entries.entries, archive->entries.capacity * sizeof(ArchiveEntry));
    }
    
    if (archive->buffer.data) {
        MODULES_FREE(archive->buffer.data, archive->buffer.capacity);
    }
    
    MODULES_FREE(archive, sizeof(ModuleArchive));
}

static void ensure_buffer_capacity(ModuleArchive* archive, size_t needed) {
    size_t required = archive->buffer.size + needed;
    if (required > archive->buffer.capacity) {
        Allocator* alloc = allocators_get(ALLOC_SYSTEM_MODULES);
        
        size_t old_capacity = archive->buffer.capacity;
        while (archive->buffer.capacity < required) {
            archive->buffer.capacity *= 2;
        }
        
        uint8_t* new_data = MODULES_ALLOC(archive->buffer.capacity);
        memcpy(new_data, archive->buffer.data, archive->buffer.size);
        MODULES_FREE(archive->buffer.data, old_capacity);
        archive->buffer.data = new_data;
    }
}

static void add_entry(ModuleArchive* archive, const char* name, uint8_t type, const uint8_t* data, size_t size) {
    if (archive->entries.count >= archive->entries.capacity) {
        Allocator* alloc = allocators_get(ALLOC_SYSTEM_MODULES);
        
        size_t old_capacity = archive->entries.capacity;
        archive->entries.capacity *= 2;
        
        ArchiveEntry* new_entries = MODULES_NEW_ARRAY(ArchiveEntry, archive->entries.capacity);
        memcpy(new_entries, archive->entries.entries, old_capacity * sizeof(ArchiveEntry));
        MODULES_FREE(archive->entries.entries, old_capacity * sizeof(ArchiveEntry));
        archive->entries.entries = new_entries;
    }
    
    ArchiveEntry* entry = &archive->entries.entries[archive->entries.count++];
    strncpy(entry->name, name, sizeof(entry->name) - 1);
    entry->size = size;
    entry->offset = archive->buffer.size;
    entry->type = type;
    
    if (data && size > 0) {
        ensure_buffer_capacity(archive, size);
        memcpy(archive->buffer.data + archive->buffer.size, data, size);
        archive->buffer.size += size;
    }
}

bool module_archive_add_file(ModuleArchive* archive, const char* file_path, const char* archive_path) {
    if (!archive || !archive->write_mode) return false;
    
    if (archive->format == ARCHIVE_FORMAT_ZIP) {
        // Use miniz to add file to ZIP
        return mz_zip_writer_add_file(&archive->zip_archive, archive_path, file_path, NULL, 0, MZ_DEFAULT_COMPRESSION);
    } else {
        Allocator* alloc = allocators_get(ALLOC_SYSTEM_MODULES);
        
        // Custom format
        FILE* f = fopen(file_path, "rb");
        if (!f) return false;
        
        fseek(f, 0, SEEK_END);
        size_t size = ftell(f);
        fseek(f, 0, SEEK_SET);
        
        uint8_t* data = MODULES_ALLOC(size);
        size_t read = fread(data, 1, size, f);
        fclose(f);
        
        if (read != size) {
            MODULES_FREE(data, size);
            return false;
        }
        
        add_entry(archive, archive_path, 0, data, size);
        MODULES_FREE(data, size);
        
        return true;
    }
}

bool module_archive_add_bytecode(ModuleArchive* archive, const char* module_name, const uint8_t* bytecode, size_t size) {
    if (!archive || !archive->write_mode) return false;
    
    char path[512];
    snprintf(path, sizeof(path), "bytecode/%s.swiftbc", module_name);
    
    if (archive->format == ARCHIVE_FORMAT_ZIP) {
        return mz_zip_writer_add_mem(&archive->zip_archive, path, bytecode, size, MZ_DEFAULT_COMPRESSION);
    } else {
        add_entry(archive, path, 0, bytecode, size);
        return true;
    }
}

bool module_archive_add_json(ModuleArchive* archive, const char* json_content) {
    if (!archive || !archive->write_mode) return false;
    
    if (archive->format == ARCHIVE_FORMAT_ZIP) {
        return mz_zip_writer_add_mem(&archive->zip_archive, "module.json", json_content, strlen(json_content), MZ_DEFAULT_COMPRESSION);
    } else {
        add_entry(archive, "module.json", 0, (const uint8_t*)json_content, strlen(json_content));
        return true;
    }
}

bool module_archive_add_native_lib(ModuleArchive* archive, const char* lib_path, const char* platform) {
    if (!archive || !archive->write_mode) return false;
    
    char archive_path[512];
    const char* filename = strrchr(lib_path, '/');
    if (!filename) filename = lib_path;
    else filename++;
    
    snprintf(archive_path, sizeof(archive_path), "native/%s/%s", platform, filename);
    
    return module_archive_add_file(archive, lib_path, archive_path);
}

bool module_archive_write(ModuleArchive* archive) {
    if (!archive || !archive->write_mode) return false;
    
    if (archive->format == ARCHIVE_FORMAT_ZIP) {
        // ZIP format is automatically written on finalize/destroy
        return true;
    } else {
        // Custom format
        FILE* f = fopen(archive->path, "wb");
        if (!f) return false;
        
        // Write header
        fwrite(ARCHIVE_MAGIC, 1, 8, f);
        uint32_t version = ARCHIVE_VERSION;
        fwrite(&version, sizeof(uint32_t), 1, f);
        
        // Write entry count
        uint32_t count = archive->entries.count;
        fwrite(&count, sizeof(uint32_t), 1, f);
        
        // Calculate data offset (after header and entry table)
        uint32_t data_offset = 8 + sizeof(uint32_t) + sizeof(uint32_t) + 
                              (archive->entries.count * sizeof(ArchiveEntry));
        
        // Update entry offsets
        for (size_t i = 0; i < archive->entries.count; i++) {
            archive->entries.entries[i].offset += data_offset;
        }
        
        // Write entry table
        fwrite(archive->entries.entries, sizeof(ArchiveEntry), archive->entries.count, f);
        
        // Write data
        fwrite(archive->buffer.data, 1, archive->buffer.size, f);
        
        fclose(f);
        archive->file = NULL;
        
        return true;
    }
}

ModuleArchive* module_archive_open(const char* path) {
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_MODULES);
    Allocator* str_alloc = allocators_get(ALLOC_SYSTEM_STRINGS);
    
    ModuleArchive* archive = MODULES_NEW_ZERO(ModuleArchive);
    archive->path = STRINGS_STRDUP(path);
    archive->write_mode = false;
    
    // Detect format
    if (module_archive_is_zip(path)) {
        archive->format = ARCHIVE_FORMAT_ZIP;
        
        // Initialize ZIP reader
        memset(&archive->zip_archive, 0, sizeof(archive->zip_archive));
        if (!mz_zip_reader_init_file(&archive->zip_archive, path, 0)) {
            size_t path_len = strlen(archive->path) + 1;
            STRINGS_FREE(archive->path, path_len);
            MODULES_FREE(archive, sizeof(ModuleArchive));
            return NULL;
        }
        archive->zip_initialized = true;
        
        // For ZIP, we don't need to load entries into memory
        return archive;
    } else {
        archive->format = ARCHIVE_FORMAT_CUSTOM;
        
        FILE* f = fopen(path, "rb");
        if (!f) {
            size_t path_len = strlen(archive->path) + 1;
            STRINGS_FREE(archive->path, path_len);
            MODULES_FREE(archive, sizeof(ModuleArchive));
            return NULL;
        }
        
        // Read and verify header
        char magic[8];
        if (fread(magic, 1, 8, f) != 8 || memcmp(magic, ARCHIVE_MAGIC, 8) != 0) {
            fclose(f);
            size_t path_len = strlen(archive->path) + 1;
            STRINGS_FREE(archive->path, path_len);
            MODULES_FREE(archive, sizeof(ModuleArchive));
            return NULL;
        }
        
        uint32_t version;
        if (fread(&version, sizeof(uint32_t), 1, f) != 1 || version != ARCHIVE_VERSION) {
            fclose(f);
            size_t path_len = strlen(archive->path) + 1;
            STRINGS_FREE(archive->path, path_len);
            MODULES_FREE(archive, sizeof(ModuleArchive));
            return NULL;
        }
        
        uint32_t count;
        if (fread(&count, sizeof(uint32_t), 1, f) != 1) {
            fclose(f);
            size_t path_len = strlen(archive->path) + 1;
            STRINGS_FREE(archive->path, path_len);
            MODULES_FREE(archive, sizeof(ModuleArchive));
            return NULL;
        }
        
        archive->file = f;
        
        // Read entry table
        archive->entries.count = count;
        archive->entries.capacity = count;
        archive->entries.entries = MODULES_NEW_ARRAY_ZERO(ArchiveEntry, count);
        
        if (fread(archive->entries.entries, sizeof(ArchiveEntry), count, f) != count) {
            module_archive_destroy(archive);
            return NULL;
        }
        
        return archive;
    }
}

static ArchiveEntry* find_entry(ModuleArchive* archive, const char* name) {
    for (size_t i = 0; i < archive->entries.count; i++) {
        if (strcmp(archive->entries.entries[i].name, name) == 0) {
            return &archive->entries.entries[i];
        }
    }
    return NULL;
}

bool module_archive_extract_json(ModuleArchive* archive, char** json_content, size_t* size) {
    if (!archive || archive->write_mode) return false;
    
    Allocator* str_alloc = allocators_get(ALLOC_SYSTEM_STRINGS);
    
    if (archive->format == ARCHIVE_FORMAT_ZIP) {
        // Extract from ZIP
        size_t uncomp_size;
        void* data = mz_zip_reader_extract_file_to_heap(&archive->zip_archive, "module.json", &uncomp_size, 0);
        if (!data) return false;
        
        *size = uncomp_size;
        *json_content = STRINGS_ALLOC(uncomp_size + 1);
        memcpy(*json_content, data, uncomp_size);
        (*json_content)[uncomp_size] = '\0';
        mz_free(data);
        return true;
    } else {
        // Custom format
        ArchiveEntry* entry = find_entry(archive, "module.json");
        if (!entry) return false;
        
        *size = entry->size;
        *json_content = STRINGS_ALLOC(entry->size + 1);
        
        fseek(archive->file, entry->offset, SEEK_SET);
        if (fread(*json_content, 1, entry->size, archive->file) != entry->size) {
            STRINGS_FREE(*json_content, entry->size + 1);
            return false;
        }
        
        (*json_content)[entry->size] = '\0';
        return true;
    }
}

bool module_archive_extract_bytecode(ModuleArchive* archive, const char* module_name, uint8_t** bytecode, size_t* size) {
    if (!archive || archive->write_mode) return false;
    
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_MODULES);
    
    char path[512];
    snprintf(path, sizeof(path), "bytecode/%s.swiftbc", module_name);
    
    if (archive->format == ARCHIVE_FORMAT_ZIP) {
        // Extract from ZIP
        size_t uncomp_size;
        void* data = mz_zip_reader_extract_file_to_heap(&archive->zip_archive, path, &uncomp_size, 0);
        if (!data) return false;
        
        *size = uncomp_size;
        *bytecode = MODULES_ALLOC(uncomp_size);
        memcpy(*bytecode, data, uncomp_size);
        mz_free(data);
        return true;
    } else {
        // Custom format
        ArchiveEntry* entry = find_entry(archive, path);
        if (!entry) return false;
        
        *size = entry->size;
        *bytecode = MODULES_ALLOC(entry->size);
        
        fseek(archive->file, entry->offset, SEEK_SET);
        if (fread(*bytecode, 1, entry->size, archive->file) != entry->size) {
            MODULES_FREE(*bytecode, entry->size);
            return false;
        }
        
        return true;
    }
}

bool module_archive_extract_native_lib(ModuleArchive* archive, const char* platform, const char* output_path) {
    if (!archive || archive->write_mode) return false;
    
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_MODULES);
    
    // Find native library for platform
    char prefix[256];
    snprintf(prefix, sizeof(prefix), "native/%s/", platform);
    
    if (archive->format == ARCHIVE_FORMAT_ZIP) {
        // Search in ZIP
        mz_uint num_files = mz_zip_reader_get_num_files(&archive->zip_archive);
        for (mz_uint i = 0; i < num_files; i++) {
            mz_zip_archive_file_stat file_stat;
            if (!mz_zip_reader_file_stat(&archive->zip_archive, i, &file_stat)) continue;
            
            if (strncmp(file_stat.m_filename, prefix, strlen(prefix)) == 0) {
                // Extract to output path
                if (mz_zip_reader_extract_to_file(&archive->zip_archive, i, output_path, 0)) {
                    chmod(output_path, 0755);
                    return true;
                }
            }
        }
        return false;
    } else {
        // Custom format
        for (size_t i = 0; i < archive->entries.count; i++) {
            ArchiveEntry* entry = &archive->entries.entries[i];
            if (strncmp(entry->name, prefix, strlen(prefix)) == 0) {
                // Extract to output path
                FILE* out = fopen(output_path, "wb");
                if (!out) return false;
                
                uint8_t* data = MODULES_ALLOC(entry->size);
                fseek(archive->file, entry->offset, SEEK_SET);
                if (fread(data, 1, entry->size, archive->file) != entry->size) {
                    MODULES_FREE(data, entry->size);
                    fclose(out);
                    return false;
                }
                
                fwrite(data, 1, entry->size, out);
                MODULES_FREE(data, entry->size);
                fclose(out);
                
                // Make executable
                chmod(output_path, 0755);
                
                return true;
            }
        }
        
        return false;
    }
}

size_t module_archive_get_entry_count(ModuleArchive* archive) {
    if (!archive) return 0;
    
    if (archive->format == ARCHIVE_FORMAT_ZIP) {
        return mz_zip_reader_get_num_files(&archive->zip_archive);
    } else {
        return archive->entries.count;
    }
}

const char* module_archive_get_entry_name(ModuleArchive* archive, size_t index) {
    if (!archive) return NULL;
    
    if (archive->format == ARCHIVE_FORMAT_ZIP) {
        static mz_zip_archive_file_stat file_stat;
        if (mz_zip_reader_file_stat(&archive->zip_archive, index, &file_stat)) {
            return file_stat.m_filename;
        }
        return NULL;
    } else {
        if (index >= archive->entries.count) return NULL;
        return archive->entries.entries[index].name;
    }
}

size_t module_archive_get_entry_size(ModuleArchive* archive, size_t index) {
    if (!archive) return 0;
    
    if (archive->format == ARCHIVE_FORMAT_ZIP) {
        mz_zip_archive_file_stat file_stat;
        if (mz_zip_reader_file_stat(&archive->zip_archive, index, &file_stat)) {
            return (size_t)file_stat.m_uncomp_size;
        }
        return 0;
    } else {
        if (index >= archive->entries.count) return 0;
        return archive->entries.entries[index].size;
    }
}

const char* module_archive_get_platform(void) {
    #ifdef __APPLE__
        #ifdef __aarch64__
        return "darwin-arm64";
        #else
        return "darwin-x86_64";
        #endif
    #elif defined(__linux__)
        #ifdef __aarch64__
        return "linux-arm64";
        #else
        return "linux-x86_64";
        #endif
    #elif defined(_WIN32)
        #ifdef _M_ARM64
        return "windows-arm64";
        #else
        return "windows-x86_64";
        #endif
    #else
    return "unknown";
    #endif
}

void module_archive_set_format(ArchiveFormat format) {
    g_archive_format = format;
}

ArchiveFormat module_archive_get_format(void) {
    return g_archive_format;
}

bool module_archive_is_zip(const char* path) {
    FILE* file = fopen(path, "rb");
    if (!file) return false;
    
    // Check for ZIP signature (PK\x03\x04)
    uint8_t sig[4];
    size_t read = fread(sig, 1, 4, file);
    fclose(file);
    
    return read == 4 && sig[0] == 'P' && sig[1] == 'K' && sig[2] == 0x03 && sig[3] == 0x04;
}
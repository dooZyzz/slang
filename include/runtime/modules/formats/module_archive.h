#ifndef LANG_MODULE_ARCHIVE_H
#define LANG_MODULE_ARCHIVE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "runtime/modules/formats/module_format.h"

// Module archive format (.swiftmodule)
// Structure:
// - module.json (required)
// - bytecode/ (directory containing compiled .swiftbc files)
// - native/ (optional directory containing platform-specific libraries)
// - resources/ (optional directory for other resources)

typedef struct ModuleArchive ModuleArchive;
typedef struct ModuleArchiveEntry ModuleArchiveEntry;

// Archive creation and manipulation
ModuleArchive* module_archive_create(const char* path);
void module_archive_destroy(ModuleArchive* archive);

// Add files to archive
bool module_archive_add_file(ModuleArchive* archive, const char* path, const char* archive_path);
bool module_archive_add_bytecode(ModuleArchive* archive, const char* module_name, const uint8_t* bytecode, size_t size);
bool module_archive_add_json(ModuleArchive* archive, const char* json_content);
bool module_archive_add_native_lib(ModuleArchive* archive, const char* lib_path, const char* platform);

// Write archive to disk
bool module_archive_write(ModuleArchive* archive);

// Archive reading
ModuleArchive* module_archive_open(const char* path);
bool module_archive_extract_json(ModuleArchive* archive, char** json_content, size_t* size);
bool module_archive_extract_bytecode(ModuleArchive* archive, const char* module_name, uint8_t** bytecode, size_t* size);
bool module_archive_extract_native_lib(ModuleArchive* archive, const char* platform, const char* output_path);

// Archive inspection
size_t module_archive_get_entry_count(ModuleArchive* archive);
const char* module_archive_get_entry_name(ModuleArchive* archive, size_t index);
size_t module_archive_get_entry_size(ModuleArchive* archive, size_t index);

// Platform detection
const char* module_archive_get_platform(void);

// Archive format selection
typedef enum {
    ARCHIVE_FORMAT_CUSTOM,  // Legacy custom format
    ARCHIVE_FORMAT_ZIP      // Standard ZIP format
} ArchiveFormat;

// Set/get archive format (default is ZIP)
void module_archive_set_format(ArchiveFormat format);
ArchiveFormat module_archive_get_format(void);

// Utility functions for ZIP format
bool module_archive_is_zip(const char* path);

#endif // LANG_MODULE_ARCHIVE_H
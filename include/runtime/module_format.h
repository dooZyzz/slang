#ifndef MODULE_FORMAT_H
#define MODULE_FORMAT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// SwiftLang Module Format (.swiftmodule)
// Binary format for compiled modules

#define SWIFTMODULE_MAGIC 0x53574D4F  // "SWMO"
#define SWIFTMODULE_VERSION 1

// Module format sections
typedef enum {
    SECTION_HEADER = 0x01,
    SECTION_METADATA = 0x02,
    SECTION_EXPORTS = 0x03,
    SECTION_IMPORTS = 0x04,
    SECTION_BYTECODE = 0x05,
    SECTION_DEBUG = 0x06,
    SECTION_NATIVES = 0x07,
    SECTION_CONSTANTS = 0x08,
    SECTION_END = 0xFF
} ModuleSectionType;

// Module header
typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t flags;
    uint32_t section_count;
    uint64_t timestamp;
    uint32_t checksum;
} ModuleHeader;

// Section header
typedef struct {
    uint8_t type;
    uint32_t size;
    uint32_t offset;
} SectionHeader;

// Export entry
typedef struct {
    char* name;
    uint8_t type;  // function, variable, constant, class
    uint32_t offset;  // Offset in bytecode section
    char* signature;  // Type signature
} ExportEntry;

// Import entry  
typedef struct {
    char* module_name;
    char* import_name;
    char* alias;
} ImportEntry;

// Native binding entry
typedef struct {
    char* export_name;
    char* native_name;
    char* signature;
} NativeBinding;

// Module writer
typedef struct ModuleWriter ModuleWriter;

ModuleWriter* module_writer_create(const char* path);
void module_writer_destroy(ModuleWriter* writer);

// Write sections
bool module_writer_add_metadata(ModuleWriter* writer, const char* name, const char* version);
bool module_writer_add_export(ModuleWriter* writer, const char* name, uint8_t type, 
                             uint32_t offset, const char* signature);
bool module_writer_add_import(ModuleWriter* writer, const char* module, 
                             const char* name, const char* alias);
bool module_writer_add_bytecode(ModuleWriter* writer, const uint8_t* code, size_t size);
bool module_writer_add_native_binding(ModuleWriter* writer, const char* export_name,
                                     const char* native_name, const char* signature);
bool module_writer_finalize(ModuleWriter* writer);

// Module reader
typedef struct ModuleReader ModuleReader;

ModuleReader* module_reader_create(const char* path);
void module_reader_destroy(ModuleReader* reader);

bool module_reader_verify(ModuleReader* reader);
const char* module_reader_get_name(ModuleReader* reader);
const char* module_reader_get_version(ModuleReader* reader);

size_t module_reader_get_export_count(ModuleReader* reader);
ExportEntry* module_reader_get_export(ModuleReader* reader, size_t index);
ExportEntry* module_reader_find_export(ModuleReader* reader, const char* name);

size_t module_reader_get_import_count(ModuleReader* reader);
ImportEntry* module_reader_get_import(ModuleReader* reader, size_t index);

const uint8_t* module_reader_get_bytecode(ModuleReader* reader, size_t* size);

size_t module_reader_get_native_binding_count(ModuleReader* reader);
NativeBinding* module_reader_get_native_binding(ModuleReader* reader, size_t index);

#endif
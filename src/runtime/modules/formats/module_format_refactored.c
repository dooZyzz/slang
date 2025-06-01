#include "runtime/modules/formats/module_format.h"
#include "utils/allocators.h"
#include "runtime/modules/module_allocator_macros.h"
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stddef.h>

// CRC32 for checksum
static uint32_t crc32_table[256];
static int crc32_table_initialized = 0;

static void init_crc32_table(void) {
    if (crc32_table_initialized) return;
    
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int k = 0; k < 8; k++) {
            c = (c & 1) ? (0xEDB88320 ^ (c >> 1)) : (c >> 1);
        }
        crc32_table[i] = c;
    }
    crc32_table_initialized = 1;
}

static uint32_t crc32(const uint8_t* data, size_t len) {
    init_crc32_table();
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc = crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return ~crc;
}

// Module writer implementation
typedef struct {
    ModuleSectionType type;
    void* data;
    size_t size;
} Section;

struct ModuleWriter {
    FILE* file;
    char* path;
    
    // Module info
    char* module_name;
    char* module_version;
    
    // Sections
    Section* sections;
    size_t section_count;
    size_t section_capacity;
    
    // Current offset
    size_t current_offset;
};

ModuleWriter* module_writer_create(const char* path) {
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_MODULES);
    Allocator* str_alloc = allocators_get(ALLOC_SYSTEM_STRINGS);
    
    ModuleWriter* writer = MODULES_NEW_ZERO(ModuleWriter);
    writer->path = STRINGS_STRDUP(path);
    writer->file = fopen(path, "wb");
    
    if (!writer->file) {
        STRINGS_FREE(writer->path, strlen(writer->path) + 1);
        MODULES_FREE(writer, sizeof(ModuleWriter));
        return NULL;
    }
    
    writer->section_capacity = 8;
    writer->sections = MODULES_NEW_ARRAY_ZERO(Section, writer->section_capacity);
    
    // Reserve space for header
    writer->current_offset = sizeof(ModuleHeader);
    fseek(writer->file, writer->current_offset, SEEK_SET);
    
    return writer;
}

void module_writer_destroy(ModuleWriter* writer) {
    if (!writer) return;
    
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_MODULES);
    Allocator* str_alloc = allocators_get(ALLOC_SYSTEM_STRINGS);
    
    // Free sections
    for (size_t i = 0; i < writer->section_count; i++) {
        if (writer->sections[i].data) {
            MODULES_FREE(writer->sections[i].data, writer->sections[i].size);
        }
    }
    MODULES_FREE(writer->sections, writer->section_capacity * sizeof(Section));
    
    if (writer->module_name) {
        STRINGS_FREE(writer->module_name, strlen(writer->module_name) + 1);
    }
    if (writer->module_version) {
        STRINGS_FREE(writer->module_version, strlen(writer->module_version) + 1);
    }
    if (writer->path) {
        STRINGS_FREE(writer->path, strlen(writer->path) + 1);
    }
    
    if (writer->file) {
        fclose(writer->file);
    }
    
    MODULES_FREE(writer, sizeof(ModuleWriter));
}

static void writer_add_section(ModuleWriter* writer, ModuleSectionType type, 
                              const void* data, size_t size) {
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_MODULES);
    
    if (writer->section_count >= writer->section_capacity) {
        size_t old_capacity = writer->section_capacity;
        writer->section_capacity *= 2;
        
        Section* new_sections = MODULES_NEW_ARRAY(Section, writer->section_capacity);
        memcpy(new_sections, writer->sections, old_capacity * sizeof(Section));
        MODULES_FREE(writer->sections, old_capacity * sizeof(Section));
        writer->sections = new_sections;
    }
    
    Section* section = &writer->sections[writer->section_count++];
    section->type = type;
    section->data = MODULES_ALLOC(size);
    memcpy(section->data, data, size);
    section->size = size;
    
    // Write actual data
    fwrite(data, 1, size, writer->file);
    writer->current_offset += size;
}

bool module_writer_add_metadata(ModuleWriter* writer, const char* name, const char* version) {
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_MODULES);
    Allocator* str_alloc = allocators_get(ALLOC_SYSTEM_STRINGS);
    
    writer->module_name = STRINGS_STRDUP(name);
    writer->module_version = STRINGS_STRDUP(version);
    
    // Create metadata section
    size_t name_len = strlen(name) + 1;
    size_t version_len = strlen(version) + 1;
    size_t metadata_size = 4 + name_len + 4 + version_len;
    
    uint8_t* metadata = MODULES_ALLOC(metadata_size);
    size_t offset = 0;
    
    // Write name length and name
    *(uint32_t*)(metadata + offset) = name_len;
    offset += 4;
    memcpy(metadata + offset, name, name_len);
    offset += name_len;
    
    // Write version length and version
    *(uint32_t*)(metadata + offset) = version_len;
    offset += 4;
    memcpy(metadata + offset, version, version_len);
    
    writer_add_section(writer, SECTION_METADATA, metadata, metadata_size);
    MODULES_FREE(metadata, metadata_size);
    
    return true;
}

bool module_writer_add_export(ModuleWriter* writer, const char* name, uint8_t type, 
                             uint32_t offset, const char* signature) {
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_MODULES);
    
    size_t name_len = strlen(name) + 1;
    size_t sig_len = signature ? strlen(signature) + 1 : 1;
    size_t export_size = 4 + name_len + 1 + 4 + 4 + sig_len;
    
    uint8_t* export_data = MODULES_ALLOC(export_size);
    size_t pos = 0;
    
    // Write name
    *(uint32_t*)(export_data + pos) = name_len;
    pos += 4;
    memcpy(export_data + pos, name, name_len);
    pos += name_len;
    
    // Write type
    export_data[pos++] = type;
    
    // Write offset
    *(uint32_t*)(export_data + pos) = offset;
    pos += 4;
    
    // Write signature
    *(uint32_t*)(export_data + pos) = sig_len;
    pos += 4;
    if (signature) {
        memcpy(export_data + pos, signature, sig_len);
    } else {
        export_data[pos] = '\0';
    }
    
    writer_add_section(writer, SECTION_EXPORTS, export_data, export_size);
    MODULES_FREE(export_data, export_size);
    
    return true;
}

bool module_writer_add_bytecode(ModuleWriter* writer, const uint8_t* code, size_t size) {
    writer_add_section(writer, SECTION_BYTECODE, code, size);
    return true;
}

bool module_writer_add_native_binding(ModuleWriter* writer, const char* export_name,
                                     const char* native_name, const char* signature) {
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_MODULES);
    
    size_t export_len = strlen(export_name) + 1;
    size_t native_len = strlen(native_name) + 1;
    size_t sig_len = signature ? strlen(signature) + 1 : 1;
    size_t binding_size = 4 + export_len + 4 + native_len + 4 + sig_len;
    
    uint8_t* binding_data = MODULES_ALLOC(binding_size);
    size_t pos = 0;
    
    // Write export name
    *(uint32_t*)(binding_data + pos) = export_len;
    pos += 4;
    memcpy(binding_data + pos, export_name, export_len);
    pos += export_len;
    
    // Write native name
    *(uint32_t*)(binding_data + pos) = native_len;
    pos += 4;
    memcpy(binding_data + pos, native_name, native_len);
    pos += native_len;
    
    // Write signature
    *(uint32_t*)(binding_data + pos) = sig_len;
    pos += 4;
    if (signature) {
        memcpy(binding_data + pos, signature, sig_len);
    } else {
        binding_data[pos] = '\0';
    }
    
    writer_add_section(writer, SECTION_NATIVES, binding_data, binding_size);
    MODULES_FREE(binding_data, binding_size);
    
    return true;
}

bool module_writer_finalize(ModuleWriter* writer) {
    if (!writer->file) return false;
    
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_MODULES);
    
    // Add end section
    uint8_t end_marker = 0;
    writer_add_section(writer, SECTION_END, &end_marker, 1);
    
    // Rewind to write header
    fseek(writer->file, 0, SEEK_SET);
    
    ModuleHeader header = {
        .magic = SWIFTMODULE_MAGIC,
        .version = SWIFTMODULE_VERSION,
        .flags = 0,
        .section_count = writer->section_count,
        .timestamp = time(NULL),
        .checksum = 0  // Will calculate after writing
    };
    
    fwrite(&header, sizeof(header), 1, writer->file);
    
    // Write section headers
    size_t section_offset = sizeof(ModuleHeader) + writer->section_count * sizeof(SectionHeader);
    for (size_t i = 0; i < writer->section_count; i++) {
        SectionHeader shdr = {
            .type = writer->sections[i].type,
            .size = writer->sections[i].size,
            .offset = section_offset
        };
        fwrite(&shdr, sizeof(shdr), 1, writer->file);
        section_offset += writer->sections[i].size;
    }
    
    // Write section data
    for (size_t i = 0; i < writer->section_count; i++) {
        fwrite(writer->sections[i].data, 1, writer->sections[i].size, writer->file);
    }
    
    // Calculate actual file size after all writes
    fseek(writer->file, 0, SEEK_END);
    size_t actual_file_size = ftell(writer->file);
    
    // Calculate checksum
    fseek(writer->file, 0, SEEK_SET);
    uint8_t* file_data = MODULES_ALLOC(actual_file_size);
    fread(file_data, 1, actual_file_size, writer->file);
    
    // Zero out checksum field for calculation
    ((ModuleHeader*)file_data)->checksum = 0;
    uint32_t checksum = crc32(file_data, actual_file_size);
    
    // Write checksum
    fseek(writer->file, offsetof(ModuleHeader, checksum), SEEK_SET);
    fwrite(&checksum, sizeof(checksum), 1, writer->file);
    
    // Ensure everything is written
    fflush(writer->file);
    
    MODULES_FREE(file_data, actual_file_size);
    fclose(writer->file);
    writer->file = NULL;
    
    return true;
}

// Module reader implementation
struct ModuleReader {
    FILE* file;
    char* path;
    
    ModuleHeader header;
    SectionHeader* sections;
    
    // Cached data
    char* module_name;
    char* module_version;
    
    ExportEntry* exports;
    size_t export_count;
    
    ImportEntry* imports;
    size_t import_count;
    
    NativeBinding* native_bindings;
    size_t native_binding_count;
    
    uint8_t* bytecode;
    size_t bytecode_size;
};

ModuleReader* module_reader_create(const char* path) {
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_MODULES);
    Allocator* str_alloc = allocators_get(ALLOC_SYSTEM_STRINGS);
    
    FILE* file = fopen(path, "rb");
    if (!file) return NULL;
    
    ModuleReader* reader = MODULES_NEW_ZERO(ModuleReader);
    reader->file = file;
    reader->path = STRINGS_STRDUP(path);
    
    // Read header
    if (fread(&reader->header, sizeof(ModuleHeader), 1, file) != 1) {
        module_reader_destroy(reader);
        return NULL;
    }
    
    // Verify magic
    if (reader->header.magic != SWIFTMODULE_MAGIC) {
        module_reader_destroy(reader);
        return NULL;
    }
    
    // Read section headers
    reader->sections = MODULES_NEW_ARRAY_ZERO(SectionHeader, reader->header.section_count);
    if (fread(reader->sections, sizeof(SectionHeader), reader->header.section_count, file) 
        != reader->header.section_count) {
        module_reader_destroy(reader);
        return NULL;
    }
    
    // Parse sections
    for (uint32_t i = 0; i < reader->header.section_count; i++) {
        SectionHeader* shdr = &reader->sections[i];
        
        switch (shdr->type) {
            case SECTION_METADATA: {
                fseek(file, shdr->offset, SEEK_SET);
                
                // Read name
                uint32_t name_len;
                fread(&name_len, sizeof(name_len), 1, file);
                reader->module_name = STRINGS_ALLOC(name_len);
                fread(reader->module_name, 1, name_len, file);
                
                // Read version
                uint32_t version_len;
                fread(&version_len, sizeof(version_len), 1, file);
                reader->module_version = STRINGS_ALLOC(version_len);
                fread(reader->module_version, 1, version_len, file);
                break;
            }
            
            case SECTION_EXPORTS: {
                fseek(file, shdr->offset, SEEK_SET);
                
                // Count exports (simplified - in production, parse properly)
                reader->export_count = 1;  // For now, assume one export per section
                size_t old_count = reader->export_count - 1;
                
                ExportEntry* new_exports = MODULES_NEW_ARRAY(ExportEntry, reader->export_count);
                if (reader->exports) {
                    memcpy(new_exports, reader->exports, old_count * sizeof(ExportEntry));
                    MODULES_FREE(reader->exports, old_count * sizeof(ExportEntry));
                }
                reader->exports = new_exports;
                
                ExportEntry* export = &reader->exports[reader->export_count - 1];
                
                // Read name
                uint32_t name_len;
                fread(&name_len, sizeof(name_len), 1, file);
                export->name = STRINGS_ALLOC(name_len);
                fread(export->name, 1, name_len, file);
                
                // Read type
                fread(&export->type, sizeof(export->type), 1, file);
                
                // Read offset
                fread(&export->offset, sizeof(export->offset), 1, file);
                
                // Read signature
                uint32_t sig_len;
                fread(&sig_len, sizeof(sig_len), 1, file);
                export->signature = STRINGS_ALLOC(sig_len);
                fread(export->signature, 1, sig_len, file);
                break;
            }
            
            case SECTION_BYTECODE: {
                reader->bytecode_size = shdr->size;
                reader->bytecode = MODULES_ALLOC(reader->bytecode_size);
                fseek(file, shdr->offset, SEEK_SET);
                fread(reader->bytecode, 1, reader->bytecode_size, file);
                break;
            }
            
            case SECTION_NATIVES: {
                fseek(file, shdr->offset, SEEK_SET);
                
                reader->native_binding_count++;
                size_t old_count = reader->native_binding_count - 1;
                
                NativeBinding* new_bindings = MODULES_NEW_ARRAY(NativeBinding, reader->native_binding_count);
                if (reader->native_bindings) {
                    memcpy(new_bindings, reader->native_bindings, old_count * sizeof(NativeBinding));
                    MODULES_FREE(reader->native_bindings, old_count * sizeof(NativeBinding));
                }
                reader->native_bindings = new_bindings;
                
                NativeBinding* binding = &reader->native_bindings[reader->native_binding_count - 1];
                
                // Read export name
                uint32_t export_len;
                fread(&export_len, sizeof(export_len), 1, file);
                binding->export_name = STRINGS_ALLOC(export_len);
                fread(binding->export_name, 1, export_len, file);
                
                // Read native name
                uint32_t native_len;
                fread(&native_len, sizeof(native_len), 1, file);
                binding->native_name = STRINGS_ALLOC(native_len);
                fread(binding->native_name, 1, native_len, file);
                
                // Read signature
                uint32_t sig_len;
                fread(&sig_len, sizeof(sig_len), 1, file);
                binding->signature = STRINGS_ALLOC(sig_len);
                fread(binding->signature, 1, sig_len, file);
                break;
            }
        }
    }
    
    return reader;
}

void module_reader_destroy(ModuleReader* reader) {
    if (!reader) return;
    
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_MODULES);
    Allocator* str_alloc = allocators_get(ALLOC_SYSTEM_STRINGS);
    
    if (reader->file) {
        fclose(reader->file);
    }
    
    if (reader->path) {
        STRINGS_FREE(reader->path, strlen(reader->path) + 1);
    }
    if (reader->sections) {
        MODULES_FREE(reader->sections, reader->header.section_count * sizeof(SectionHeader));
    }
    if (reader->module_name) {
        STRINGS_FREE(reader->module_name, strlen(reader->module_name) + 1);
    }
    if (reader->module_version) {
        STRINGS_FREE(reader->module_version, strlen(reader->module_version) + 1);
    }
    
    // Free exports
    for (size_t i = 0; i < reader->export_count; i++) {
        if (reader->exports[i].name) {
            STRINGS_FREE(reader->exports[i].name, strlen(reader->exports[i].name) + 1);
        }
        if (reader->exports[i].signature) {
            STRINGS_FREE(reader->exports[i].signature, strlen(reader->exports[i].signature) + 1);
        }
    }
    if (reader->exports) {
        MODULES_FREE(reader->exports, reader->export_count * sizeof(ExportEntry));
    }
    
    // Free imports
    for (size_t i = 0; i < reader->import_count; i++) {
        if (reader->imports[i].module_name) {
            STRINGS_FREE(reader->imports[i].module_name, strlen(reader->imports[i].module_name) + 1);
        }
        if (reader->imports[i].import_name) {
            STRINGS_FREE(reader->imports[i].import_name, strlen(reader->imports[i].import_name) + 1);
        }
        if (reader->imports[i].alias) {
            STRINGS_FREE(reader->imports[i].alias, strlen(reader->imports[i].alias) + 1);
        }
    }
    if (reader->imports) {
        MODULES_FREE(reader->imports, reader->import_count * sizeof(ImportEntry));
    }
    
    // Free native bindings
    for (size_t i = 0; i < reader->native_binding_count; i++) {
        if (reader->native_bindings[i].export_name) {
            STRINGS_FREE(reader->native_bindings[i].export_name, strlen(reader->native_bindings[i].export_name) + 1);
        }
        if (reader->native_bindings[i].native_name) {
            STRINGS_FREE(reader->native_bindings[i].native_name, strlen(reader->native_bindings[i].native_name) + 1);
        }
        if (reader->native_bindings[i].signature) {
            STRINGS_FREE(reader->native_bindings[i].signature, strlen(reader->native_bindings[i].signature) + 1);
        }
    }
    if (reader->native_bindings) {
        MODULES_FREE(reader->native_bindings, reader->native_binding_count * sizeof(NativeBinding));
    }
    
    if (reader->bytecode) {
        MODULES_FREE(reader->bytecode, reader->bytecode_size);
    }
    
    MODULES_FREE(reader, sizeof(ModuleReader));
}

bool module_reader_verify(ModuleReader* reader) {
    if (!reader || !reader->file) return false;
    
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_MODULES);
    
    // Save current position
    long current_pos = ftell(reader->file);
    
    // Get file size
    fseek(reader->file, 0, SEEK_END);
    size_t file_size = ftell(reader->file);
    fseek(reader->file, 0, SEEK_SET);
    
    // Read entire file
    uint8_t* data = MODULES_ALLOC(file_size);
    if (!data) {
        fseek(reader->file, current_pos, SEEK_SET);
        return false;
    }
    
    size_t bytes_read = fread(data, 1, file_size, reader->file);
    if (bytes_read != file_size) {
        MODULES_FREE(data, file_size);
        fseek(reader->file, current_pos, SEEK_SET);
        return false;
    }
    
    // Save checksum and zero it for calculation
    uint32_t stored_checksum = ((ModuleHeader*)data)->checksum;
    ((ModuleHeader*)data)->checksum = 0;
    
    // Calculate checksum
    uint32_t calculated_checksum = crc32(data, file_size);
    MODULES_FREE(data, file_size);
    
    // Restore file position
    fseek(reader->file, current_pos, SEEK_SET);
    
    // Verify checksum
    return stored_checksum == calculated_checksum;
}

const char* module_reader_get_name(ModuleReader* reader) {
    return reader ? reader->module_name : NULL;
}

const char* module_reader_get_version(ModuleReader* reader) {
    return reader ? reader->module_version : NULL;
}

size_t module_reader_get_export_count(ModuleReader* reader) {
    return reader ? reader->export_count : 0;
}

ExportEntry* module_reader_get_export(ModuleReader* reader, size_t index) {
    if (!reader || index >= reader->export_count) return NULL;
    return &reader->exports[index];
}

ExportEntry* module_reader_find_export(ModuleReader* reader, const char* name) {
    if (!reader) return NULL;
    
    for (size_t i = 0; i < reader->export_count; i++) {
        if (strcmp(reader->exports[i].name, name) == 0) {
            return &reader->exports[i];
        }
    }
    return NULL;
}

const uint8_t* module_reader_get_bytecode(ModuleReader* reader, size_t* size) {
    if (!reader) return NULL;
    if (size) *size = reader->bytecode_size;
    return reader->bytecode;
}

size_t module_reader_get_native_binding_count(ModuleReader* reader) {
    return reader ? reader->native_binding_count : 0;
}

NativeBinding* module_reader_get_native_binding(ModuleReader* reader, size_t index) {
    if (!reader || index >= reader->native_binding_count) return NULL;
    return &reader->native_bindings[index];
}
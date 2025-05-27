#include "runtime/module_format.h"
#include <stdlib.h>
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
    char* name;
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
    ModuleWriter* writer = calloc(1, sizeof(ModuleWriter));
    writer->path = strdup(path);
    writer->file = fopen(path, "wb");
    
    if (!writer->file) {
        free(writer->path);
        free(writer);
        return NULL;
    }
    
    writer->section_capacity = 8;
    writer->sections = calloc(writer->section_capacity, sizeof(Section));
    
    // Reserve space for header
    writer->current_offset = sizeof(ModuleHeader);
    fseek(writer->file, writer->current_offset, SEEK_SET);
    
    return writer;
}

void module_writer_destroy(ModuleWriter* writer) {
    if (!writer) return;
    
    // Free sections
    for (size_t i = 0; i < writer->section_count; i++) {
        free(writer->sections[i].name);
        free(writer->sections[i].data);
    }
    free(writer->sections);
    
    free(writer->module_name);
    free(writer->module_version);
    free(writer->path);
    
    if (writer->file) {
        fclose(writer->file);
    }
    
    free(writer);
}

static void writer_add_section(ModuleWriter* writer, ModuleSectionType type, 
                              const void* data, size_t size) {
    if (writer->section_count >= writer->section_capacity) {
        writer->section_capacity *= 2;
        writer->sections = realloc(writer->sections, 
                                  writer->section_capacity * sizeof(Section));
    }
    
    Section* section = &writer->sections[writer->section_count++];
    section->data = malloc(size);
    memcpy(section->data, data, size);
    section->size = size;
    
    // Write actual data
    fwrite(data, 1, size, writer->file);
    writer->current_offset += size;
}

bool module_writer_add_metadata(ModuleWriter* writer, const char* name, const char* version) {
    writer->module_name = strdup(name);
    writer->module_version = strdup(version);
    
    // Create metadata section
    size_t name_len = strlen(name) + 1;
    size_t version_len = strlen(version) + 1;
    size_t metadata_size = 4 + name_len + 4 + version_len;
    
    uint8_t* metadata = malloc(metadata_size);
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
    free(metadata);
    
    return true;
}

bool module_writer_add_export(ModuleWriter* writer, const char* name, uint8_t type, 
                             uint32_t offset, const char* signature) {
    size_t name_len = strlen(name) + 1;
    size_t sig_len = signature ? strlen(signature) + 1 : 1;
    size_t export_size = 4 + name_len + 1 + 4 + 4 + sig_len;
    
    uint8_t* export_data = malloc(export_size);
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
    free(export_data);
    
    return true;
}

bool module_writer_add_bytecode(ModuleWriter* writer, const uint8_t* code, size_t size) {
    writer_add_section(writer, SECTION_BYTECODE, code, size);
    return true;
}

bool module_writer_add_native_binding(ModuleWriter* writer, const char* export_name,
                                     const char* native_name, const char* signature) {
    size_t export_len = strlen(export_name) + 1;
    size_t native_len = strlen(native_name) + 1;
    size_t sig_len = signature ? strlen(signature) + 1 : 1;
    size_t binding_size = 4 + export_len + 4 + native_len + 4 + sig_len;
    
    uint8_t* binding_data = malloc(binding_size);
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
    free(binding_data);
    
    return true;
}

bool module_writer_finalize(ModuleWriter* writer) {
    if (!writer->file) return false;
    
    // Add end section
    uint8_t end_marker = 0;
    writer_add_section(writer, SECTION_END, &end_marker, 1);
    
    // Calculate total size
    size_t total_size = writer->current_offset;
    
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
            .type = (i == writer->section_count - 1) ? SECTION_END : (i + 1),
            .size = writer->sections[i].size,
            .offset = section_offset
        };
        fwrite(&shdr, sizeof(shdr), 1, writer->file);
        section_offset += writer->sections[i].size;
    }
    
    // Calculate checksum
    fseek(writer->file, 0, SEEK_SET);
    uint8_t* file_data = malloc(total_size);
    fread(file_data, 1, total_size, writer->file);
    
    // Zero out checksum field for calculation
    ((ModuleHeader*)file_data)->checksum = 0;
    uint32_t checksum = crc32(file_data, total_size);
    
    // Write checksum
    fseek(writer->file, offsetof(ModuleHeader, checksum), SEEK_SET);
    fwrite(&checksum, sizeof(checksum), 1, writer->file);
    
    free(file_data);
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
    FILE* file = fopen(path, "rb");
    if (!file) return NULL;
    
    ModuleReader* reader = calloc(1, sizeof(ModuleReader));
    reader->file = file;
    reader->path = strdup(path);
    
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
    reader->sections = calloc(reader->header.section_count, sizeof(SectionHeader));
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
                reader->module_name = malloc(name_len);
                fread(reader->module_name, 1, name_len, file);
                
                // Read version
                uint32_t version_len;
                fread(&version_len, sizeof(version_len), 1, file);
                reader->module_version = malloc(version_len);
                fread(reader->module_version, 1, version_len, file);
                break;
            }
            
            case SECTION_EXPORTS: {
                fseek(file, shdr->offset, SEEK_SET);
                
                // Count exports (simplified - in production, parse properly)
                reader->export_count = 1;  // For now, assume one export per section
                reader->exports = realloc(reader->exports, 
                                        (reader->export_count) * sizeof(ExportEntry));
                
                ExportEntry* export = &reader->exports[reader->export_count - 1];
                
                // Read name
                uint32_t name_len;
                fread(&name_len, sizeof(name_len), 1, file);
                export->name = malloc(name_len);
                fread(export->name, 1, name_len, file);
                
                // Read type
                fread(&export->type, sizeof(export->type), 1, file);
                
                // Read offset
                fread(&export->offset, sizeof(export->offset), 1, file);
                
                // Read signature
                uint32_t sig_len;
                fread(&sig_len, sizeof(sig_len), 1, file);
                export->signature = malloc(sig_len);
                fread(export->signature, 1, sig_len, file);
                break;
            }
            
            case SECTION_BYTECODE: {
                reader->bytecode_size = shdr->size;
                reader->bytecode = malloc(reader->bytecode_size);
                fseek(file, shdr->offset, SEEK_SET);
                fread(reader->bytecode, 1, reader->bytecode_size, file);
                break;
            }
            
            case SECTION_NATIVES: {
                fseek(file, shdr->offset, SEEK_SET);
                
                reader->native_binding_count++;
                reader->native_bindings = realloc(reader->native_bindings,
                                                reader->native_binding_count * sizeof(NativeBinding));
                
                NativeBinding* binding = &reader->native_bindings[reader->native_binding_count - 1];
                
                // Read export name
                uint32_t export_len;
                fread(&export_len, sizeof(export_len), 1, file);
                binding->export_name = malloc(export_len);
                fread(binding->export_name, 1, export_len, file);
                
                // Read native name
                uint32_t native_len;
                fread(&native_len, sizeof(native_len), 1, file);
                binding->native_name = malloc(native_len);
                fread(binding->native_name, 1, native_len, file);
                
                // Read signature
                uint32_t sig_len;
                fread(&sig_len, sizeof(sig_len), 1, file);
                binding->signature = malloc(sig_len);
                fread(binding->signature, 1, sig_len, file);
                break;
            }
        }
    }
    
    return reader;
}

void module_reader_destroy(ModuleReader* reader) {
    if (!reader) return;
    
    if (reader->file) {
        fclose(reader->file);
    }
    
    free(reader->path);
    free(reader->sections);
    free(reader->module_name);
    free(reader->module_version);
    
    // Free exports
    for (size_t i = 0; i < reader->export_count; i++) {
        free(reader->exports[i].name);
        free(reader->exports[i].signature);
    }
    free(reader->exports);
    
    // Free imports
    for (size_t i = 0; i < reader->import_count; i++) {
        free(reader->imports[i].module_name);
        free(reader->imports[i].import_name);
        free(reader->imports[i].alias);
    }
    free(reader->imports);
    
    // Free native bindings
    for (size_t i = 0; i < reader->native_binding_count; i++) {
        free(reader->native_bindings[i].export_name);
        free(reader->native_bindings[i].native_name);
        free(reader->native_bindings[i].signature);
    }
    free(reader->native_bindings);
    
    free(reader->bytecode);
    free(reader);
}

bool module_reader_verify(ModuleReader* reader) {
    if (!reader || !reader->file) return false;
    
    // Get file size
    fseek(reader->file, 0, SEEK_END);
    size_t file_size = ftell(reader->file);
    fseek(reader->file, 0, SEEK_SET);
    
    // Read entire file
    uint8_t* data = malloc(file_size);
    fread(data, 1, file_size, reader->file);
    
    // Save checksum and zero it for calculation
    uint32_t stored_checksum = ((ModuleHeader*)data)->checksum;
    ((ModuleHeader*)data)->checksum = 0;
    
    // Calculate checksum
    uint32_t calculated_checksum = crc32(data, file_size);
    free(data);
    
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
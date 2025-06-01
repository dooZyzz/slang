#ifndef BYTECODE_FORMAT_H
#define BYTECODE_FORMAT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "runtime/core/vm.h"

// SwiftLang Bytecode Format (.swiftbc)
// 
// Header (16 bytes):
//   Magic: "SWBC" (4 bytes)
//   Version: uint32_t (4 bytes)
//   Flags: uint32_t (4 bytes)
//   Header size: uint32_t (4 bytes)
//
// Sections:
//   Constants section
//   Code section
//   Debug section (optional)

#define BYTECODE_MAGIC "SWBC"
#define BYTECODE_VERSION 1

typedef struct {
    char magic[4];
    uint32_t version;
    uint32_t flags;
    uint32_t header_size;
} BytecodeHeader;

// Flags
#define BYTECODE_FLAG_DEBUG 0x01

// Serialize a chunk to bytecode
bool bytecode_serialize(Chunk* chunk, uint8_t** output, size_t* output_size);

// Deserialize bytecode to a chunk
bool bytecode_deserialize(const uint8_t* input, size_t input_size, Chunk* chunk);

// Helper functions for writing/reading
typedef struct {
    uint8_t* data;
    size_t size;
    size_t capacity;
    size_t position;
} BytecodeBuffer;

BytecodeBuffer* bytecode_buffer_create(size_t initial_capacity);
void bytecode_buffer_destroy(BytecodeBuffer* buffer);

void bytecode_write_u8(BytecodeBuffer* buffer, uint8_t value);
void bytecode_write_u32(BytecodeBuffer* buffer, uint32_t value);
void bytecode_write_u64(BytecodeBuffer* buffer, uint64_t value);
void bytecode_write_double(BytecodeBuffer* buffer, double value);
void bytecode_write_string(BytecodeBuffer* buffer, const char* str, size_t len);
void bytecode_write_bytes(BytecodeBuffer* buffer, const uint8_t* data, size_t len);

uint8_t bytecode_read_u8(BytecodeBuffer* buffer);
uint32_t bytecode_read_u32(BytecodeBuffer* buffer);
uint64_t bytecode_read_u64(BytecodeBuffer* buffer);
double bytecode_read_double(BytecodeBuffer* buffer);
char* bytecode_read_string(BytecodeBuffer* buffer, size_t* len);
bool bytecode_read_bytes(BytecodeBuffer* buffer, uint8_t* dest, size_t len);

#endif // BYTECODE_FORMAT_H
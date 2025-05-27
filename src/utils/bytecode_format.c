#include "utils/bytecode_format.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Buffer management
BytecodeBuffer* bytecode_buffer_create(size_t initial_capacity) {
    BytecodeBuffer* buffer = malloc(sizeof(BytecodeBuffer));
    if (!buffer) return NULL;
    
    buffer->data = malloc(initial_capacity);
    if (!buffer->data) {
        free(buffer);
        return NULL;
    }
    
    buffer->size = 0;
    buffer->capacity = initial_capacity;
    buffer->position = 0;
    return buffer;
}

void bytecode_buffer_destroy(BytecodeBuffer* buffer) {
    if (!buffer) return;
    free(buffer->data);
    free(buffer);
}

static void ensure_capacity(BytecodeBuffer* buffer, size_t needed) {
    size_t required = buffer->size + needed;
    if (required > buffer->capacity) {
        size_t new_capacity = buffer->capacity * 2;
        while (new_capacity < required) {
            new_capacity *= 2;
        }
        
        uint8_t* new_data = realloc(buffer->data, new_capacity);
        if (new_data) {
            buffer->data = new_data;
            buffer->capacity = new_capacity;
        }
    }
}

// Writing functions
void bytecode_write_u8(BytecodeBuffer* buffer, uint8_t value) {
    ensure_capacity(buffer, 1);
    buffer->data[buffer->size++] = value;
}

void bytecode_write_u32(BytecodeBuffer* buffer, uint32_t value) {
    ensure_capacity(buffer, 4);
    buffer->data[buffer->size++] = (value >> 24) & 0xFF;
    buffer->data[buffer->size++] = (value >> 16) & 0xFF;
    buffer->data[buffer->size++] = (value >> 8) & 0xFF;
    buffer->data[buffer->size++] = value & 0xFF;
}

void bytecode_write_u64(BytecodeBuffer* buffer, uint64_t value) {
    bytecode_write_u32(buffer, (uint32_t)(value >> 32));
    bytecode_write_u32(buffer, (uint32_t)(value & 0xFFFFFFFF));
}

void bytecode_write_double(BytecodeBuffer* buffer, double value) {
    union { double d; uint64_t u; } convert;
    convert.d = value;
    bytecode_write_u64(buffer, convert.u);
}

void bytecode_write_string(BytecodeBuffer* buffer, const char* str, size_t len) {
    bytecode_write_u32(buffer, (uint32_t)len);
    bytecode_write_bytes(buffer, (const uint8_t*)str, len);
}

void bytecode_write_bytes(BytecodeBuffer* buffer, const uint8_t* data, size_t len) {
    ensure_capacity(buffer, len);
    memcpy(buffer->data + buffer->size, data, len);
    buffer->size += len;
}

// Reading functions
uint8_t bytecode_read_u8(BytecodeBuffer* buffer) {
    if (buffer->position >= buffer->size) return 0;
    return buffer->data[buffer->position++];
}

uint32_t bytecode_read_u32(BytecodeBuffer* buffer) {
    if (buffer->position + 4 > buffer->size) return 0;
    
    uint32_t value = ((uint32_t)buffer->data[buffer->position] << 24) |
                     ((uint32_t)buffer->data[buffer->position + 1] << 16) |
                     ((uint32_t)buffer->data[buffer->position + 2] << 8) |
                     ((uint32_t)buffer->data[buffer->position + 3]);
    buffer->position += 4;
    return value;
}

uint64_t bytecode_read_u64(BytecodeBuffer* buffer) {
    uint64_t high = bytecode_read_u32(buffer);
    uint64_t low = bytecode_read_u32(buffer);
    return (high << 32) | low;
}

double bytecode_read_double(BytecodeBuffer* buffer) {
    union { double d; uint64_t u; } convert;
    convert.u = bytecode_read_u64(buffer);
    return convert.d;
}

char* bytecode_read_string(BytecodeBuffer* buffer, size_t* len) {
    uint32_t string_len = bytecode_read_u32(buffer);
    if (buffer->position + string_len > buffer->size) {
        *len = 0;
        return NULL;
    }
    
    char* str = malloc(string_len + 1);
    if (!str) {
        *len = 0;
        return NULL;
    }
    
    memcpy(str, buffer->data + buffer->position, string_len);
    str[string_len] = '\0';
    buffer->position += string_len;
    
    if (len) *len = string_len;
    return str;
}

bool bytecode_read_bytes(BytecodeBuffer* buffer, uint8_t* dest, size_t len) {
    if (buffer->position + len > buffer->size) return false;
    
    memcpy(dest, buffer->data + buffer->position, len);
    buffer->position += len;
    return true;
}

// Serialize a chunk to bytecode
bool bytecode_serialize(Chunk* chunk, uint8_t** output, size_t* output_size) {
    BytecodeBuffer* buffer = bytecode_buffer_create(1024);
    if (!buffer) return false;
    
    // Write header
    bytecode_write_bytes(buffer, (const uint8_t*)BYTECODE_MAGIC, 4);
    bytecode_write_u32(buffer, BYTECODE_VERSION);
    bytecode_write_u32(buffer, 0); // flags
    bytecode_write_u32(buffer, sizeof(BytecodeHeader));
    
    // Write constants section
    bytecode_write_u32(buffer, chunk->constants.count);
    
    for (int i = 0; i < chunk->constants.count; i++) {
        TaggedValue constant = chunk->constants.values[i];
        
        // Write type tag
        if (IS_NIL(constant)) {
            bytecode_write_u8(buffer, 0);
        } else if (IS_BOOL(constant)) {
            bytecode_write_u8(buffer, 1);
            bytecode_write_u8(buffer, AS_BOOL(constant) ? 1 : 0);
        } else if (IS_NUMBER(constant)) {
            bytecode_write_u8(buffer, 2);
            bytecode_write_double(buffer, AS_NUMBER(constant));
        } else if (IS_STRING(constant)) {
            bytecode_write_u8(buffer, 3);
            const char* str = AS_STRING(constant);
            bytecode_write_string(buffer, str, strlen(str));
        } else if (IS_FUNCTION(constant)) {
            // Serialize the entire function including its bytecode
            bytecode_write_u8(buffer, 5);
            Function* func = AS_FUNCTION(constant);
            
            // Write function metadata
            bytecode_write_string(buffer, func->name, strlen(func->name));
            bytecode_write_u32(buffer, func->arity);
            bytecode_write_u32(buffer, func->upvalue_count);
            
            // Recursively serialize the function's chunk
            uint8_t* func_bytecode;
            size_t func_bytecode_size;
            if (!bytecode_serialize(&func->chunk, &func_bytecode, &func_bytecode_size)) {
                return false;
            }
            
            // Write the serialized function bytecode
            bytecode_write_u32(buffer, func_bytecode_size);
            bytecode_write_bytes(buffer, func_bytecode, func_bytecode_size);
            free(func_bytecode);
        } else if (IS_OBJECT(constant)) {
            // Regular objects
            bytecode_write_u8(buffer, 4);
        } else {
            // Unknown type
            fprintf(stderr, "[WARNING] Unknown constant type %d, writing as NIL\n", constant.type);
            bytecode_write_u8(buffer, 0); // Write as NIL
        }
    }
    
    // Write code section
    bytecode_write_u32(buffer, chunk->count);
    bytecode_write_bytes(buffer, chunk->code, chunk->count);
    
    // Write line info if available
    if (chunk->lines) {
        bytecode_write_u32(buffer, chunk->count); // Same count as code
        for (int i = 0; i < chunk->count; i++) {
            bytecode_write_u32(buffer, chunk->lines[i]);
        }
    } else {
        bytecode_write_u32(buffer, 0); // No line info
    }
    
    // Return the buffer
    *output = buffer->data;
    *output_size = buffer->size;
    
    // Don't destroy buffer, we're returning its data
    free(buffer);
    return true;
}

// Deserialize bytecode to a chunk
bool bytecode_deserialize(const uint8_t* input, size_t input_size, Chunk* chunk) {
    fprintf(stderr, "[DEBUG] bytecode_deserialize called with size %zu\n", input_size);
    
    if (!input || input_size < 16) {
        fprintf(stderr, "[DEBUG] Invalid input or size too small\n");
        return false;
    }
    
    BytecodeBuffer buffer = {
        .data = (uint8_t*)input,
        .size = input_size,
        .capacity = input_size,
        .position = 0
    };
    
    // Read and verify header
    char magic[4];
    if (!bytecode_read_bytes(&buffer, (uint8_t*)magic, 4)) {
        fprintf(stderr, "[DEBUG] Failed to read magic\n");
        return false;
    }
    fprintf(stderr, "[DEBUG] Read magic: %.4s\n", magic);
    if (memcmp(magic, BYTECODE_MAGIC, 4) != 0) {
        fprintf(stderr, "[DEBUG] Magic mismatch\n");
        return false;
    }
    
    uint32_t version = bytecode_read_u32(&buffer);
    fprintf(stderr, "[DEBUG] Version: %u\n", version);
    if (version != BYTECODE_VERSION) {
        fprintf(stderr, "[DEBUG] Version mismatch (expected %u)\n", BYTECODE_VERSION);
        return false;
    }
    
    uint32_t flags = bytecode_read_u32(&buffer);
    uint32_t header_size = bytecode_read_u32(&buffer);
    fprintf(stderr, "[DEBUG] Flags: %u, Header size: %u\n", flags, header_size);
    
    // Skip to end of header if needed
    if (header_size > sizeof(BytecodeHeader)) {
        buffer.position = header_size;
    }
    
    // Read constants
    uint32_t constant_count = bytecode_read_u32(&buffer);
    fprintf(stderr, "[DEBUG] Constant count: %u\n", constant_count);
    
    for (uint32_t i = 0; i < constant_count; i++) {
        uint8_t type = bytecode_read_u8(&buffer);
        fprintf(stderr, "[DEBUG] Constant %u type: %u\n", i, type);
        
        switch (type) {
            case 0: // NIL
                chunk_add_constant(chunk, NIL_VAL);
                break;
            
            case 1: // BOOL
                {
                    uint8_t value = bytecode_read_u8(&buffer);
                    chunk_add_constant(chunk, BOOL_VAL(value != 0));
                }
                break;
            
            case 2: // NUMBER
                {
                    double value = bytecode_read_double(&buffer);
                    chunk_add_constant(chunk, NUMBER_VAL(value));
                }
                break;
            
            case 3: // STRING
                {
                    size_t len;
                    char* str = bytecode_read_string(&buffer, &len);
                    if (!str) return false;
                    chunk_add_constant(chunk, STRING_VAL(str));
                }
                break;
            
            case 4: // OBJECT
                // For now, add nil for objects
                chunk_add_constant(chunk, NIL_VAL);
                break;
            
            case 5: // FUNCTION
                {
                    // Read function metadata
                    size_t len;
                    char* name = bytecode_read_string(&buffer, &len);
                    if (!name) return false;
                    
                    uint32_t arity = bytecode_read_u32(&buffer);
                    uint32_t upvalue_count = bytecode_read_u32(&buffer);
                    
                    // Create the function
                    Function* func = function_create(name, arity);
                    func->upvalue_count = upvalue_count;
                    
                    // Read the function's bytecode
                    uint32_t func_bytecode_size = bytecode_read_u32(&buffer);
                    if (buffer.position + func_bytecode_size > buffer.size) {
                        free(name);
                        function_free(func);
                        return false;
                    }
                    
                    // Deserialize the function's chunk
                    if (!bytecode_deserialize(buffer.data + buffer.position, func_bytecode_size, &func->chunk)) {
                        free(name);
                        function_free(func);
                        return false;
                    }
                    buffer.position += func_bytecode_size;
                    
                    chunk_add_constant(chunk, FUNCTION_VAL(func));
                    free(name);
                }
                break;
            
            default:
                // Unknown type
                fprintf(stderr, "[DEBUG] Unknown constant type: %u\n", type);
                return false;
        }
    }
    
    // Read code
    uint32_t code_size = bytecode_read_u32(&buffer);
    
    for (uint32_t i = 0; i < code_size; i++) {
        uint8_t byte = bytecode_read_u8(&buffer);
        chunk_write(chunk, byte, 0); // Line info will be set later
    }
    
    // Read line info
    uint32_t line_count = bytecode_read_u32(&buffer);
    
    if (line_count > 0 && line_count == code_size) {
        // Update line info
        for (uint32_t i = 0; i < line_count; i++) {
            uint32_t line = bytecode_read_u32(&buffer);
            if (i < chunk->count && chunk->lines) {
                chunk->lines[i] = line;
            }
        }
    }
    
    return true;
}
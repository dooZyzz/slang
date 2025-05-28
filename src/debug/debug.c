#include "debug/debug.h"
#include <stdio.h>
#include <stdlib.h>

DebugFlags debug_flags = {false, false, false, false, false, false, false};

void debug_init(void) {
    debug_flags.print_tokens = false;
    debug_flags.print_ast = false;
    debug_flags.print_bytecode = false;
    debug_flags.trace_execution = false;
    debug_flags.module_loading = getenv("SWIFTLANG_DEBUG_MODULES") != NULL;
    debug_flags.module_cache = getenv("SWIFTLANG_DEBUG_CACHE") != NULL;
    debug_flags.module_hooks = getenv("SWIFTLANG_DEBUG_HOOKS") != NULL;
}

void debug_set_flags(bool tokens, bool ast, bool bytecode, bool trace) {
    debug_flags.print_tokens = tokens;
    debug_flags.print_ast = ast;
    debug_flags.print_bytecode = bytecode;
    debug_flags.trace_execution = trace;
}

static int simple_instruction(const char* name, int offset) {
    printf("%s\n", name);
    return offset + 1;
}

static int constant_instruction(const char* name, Chunk* chunk, int offset) {
    uint8_t constant = chunk->code[offset + 1];
    printf("%-16s %4d '", name, constant);
    if (constant < chunk->constants.count) {
        print_value(chunk->constants.values[constant]);
    } else {
        printf("<invalid constant>");
    }
    printf("'\n");
    return offset + 2;
}

static int byte_instruction(const char* name, Chunk* chunk, int offset) {
    uint8_t slot = chunk->code[offset + 1];
    printf("%-16s %4d\n", name, slot);
    return offset + 2;
}

static int jump_instruction(const char* name, int sign, Chunk* chunk, int offset) {
    uint16_t jump = (uint16_t)(chunk->code[offset + 1] << 8);
    jump |= chunk->code[offset + 2];
    printf("%-16s %4d -> %d\n", name, offset, offset + 3 + sign * jump);
    return offset + 3;
}

int disassemble_instruction(Chunk* chunk, int offset) {
    printf("%04d ", offset);
    if (offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1]) {
        printf("   | ");
    } else {
        printf("%4d ", chunk->lines[offset]);
    }
    
    uint8_t instruction = chunk->code[offset];
    switch (instruction) {
        case OP_CONSTANT:
            return constant_instruction("OP_CONSTANT", chunk, offset);
        case OP_NIL:
            return simple_instruction("OP_NIL", offset);
        case OP_TRUE:
            return simple_instruction("OP_TRUE", offset);
        case OP_FALSE:
            return simple_instruction("OP_FALSE", offset);
        case OP_POP:
            return simple_instruction("OP_POP", offset);
        case OP_DUP:
            return simple_instruction("OP_DUP", offset);
        case OP_GET_LOCAL:
            return byte_instruction("OP_GET_LOCAL", chunk, offset);
        case OP_SET_LOCAL:
            return byte_instruction("OP_SET_LOCAL", chunk, offset);
        case OP_GET_GLOBAL:
            return byte_instruction("OP_GET_GLOBAL", chunk, offset);
        case OP_SET_GLOBAL:
            return byte_instruction("OP_SET_GLOBAL", chunk, offset);
        case OP_DEFINE_GLOBAL:
            return byte_instruction("OP_DEFINE_GLOBAL", chunk, offset);
        case OP_EQUAL:
            return simple_instruction("OP_EQUAL", offset);
        case OP_NOT_EQUAL:
            return simple_instruction("OP_NOT_EQUAL", offset);
        case OP_GREATER:
            return simple_instruction("OP_GREATER", offset);
        case OP_GREATER_EQUAL:
            return simple_instruction("OP_GREATER_EQUAL", offset);
        case OP_LESS:
            return simple_instruction("OP_LESS", offset);
        case OP_LESS_EQUAL:
            return simple_instruction("OP_LESS_EQUAL", offset);
        case OP_ADD:
            return simple_instruction("OP_ADD", offset);
        case OP_SUBTRACT:
            return simple_instruction("OP_SUBTRACT", offset);
        case OP_MULTIPLY:
            return simple_instruction("OP_MULTIPLY", offset);
        case OP_DIVIDE:
            return simple_instruction("OP_DIVIDE", offset);
        case OP_MODULO:
            return simple_instruction("OP_MODULO", offset);
        case OP_NOT:
            return simple_instruction("OP_NOT", offset);
        case OP_NEGATE:
            return simple_instruction("OP_NEGATE", offset);
        case OP_AND:
            return simple_instruction("OP_AND", offset);
        case OP_OR:
            return simple_instruction("OP_OR", offset);
        case OP_BIT_AND:
            return simple_instruction("OP_BIT_AND", offset);
        case OP_BIT_OR:
            return simple_instruction("OP_BIT_OR", offset);
        case OP_BIT_XOR:
            return simple_instruction("OP_BIT_XOR", offset);
        case OP_BIT_NOT:
            return simple_instruction("OP_BIT_NOT", offset);
        case OP_SHIFT_LEFT:
            return simple_instruction("OP_SHIFT_LEFT", offset);
        case OP_SHIFT_RIGHT:
            return simple_instruction("OP_SHIFT_RIGHT", offset);
        case OP_PRINT:
            return simple_instruction("OP_PRINT", offset);
        case OP_JUMP:
            return jump_instruction("OP_JUMP", 1, chunk, offset);
        case OP_JUMP_IF_FALSE:
            return jump_instruction("OP_JUMP_IF_FALSE", 1, chunk, offset);
        case OP_JUMP_IF_TRUE:
            return jump_instruction("OP_JUMP_IF_TRUE", 1, chunk, offset);
        case OP_LOOP:
            return jump_instruction("OP_LOOP", -1, chunk, offset);
        case OP_CALL:
            return byte_instruction("OP_CALL", chunk, offset);
        case OP_RETURN:
            return simple_instruction("OP_RETURN", offset);
        case OP_LOAD_BUILTIN:
            return simple_instruction("OP_LOAD_BUILTIN", offset);
        case OP_TO_STRING:
            return simple_instruction("OP_TO_STRING", offset);
        case OP_ARRAY:
        case OP_BUILD_ARRAY:
            return byte_instruction("OP_ARRAY", chunk, offset);
        case OP_DEFINE_LOCAL:
            return byte_instruction("OP_DEFINE_LOCAL", chunk, offset);
        case OP_HALT:
            return simple_instruction("OP_HALT", offset);
        case OP_MODULE_EXPORT:
            return simple_instruction("OP_MODULE_EXPORT", offset);
        case OP_IMPORT_ALL_FROM:
            return simple_instruction("OP_IMPORT_ALL_FROM", offset);
        case OP_LOAD_MODULE:
            return simple_instruction("OP_LOAD_MODULE", offset);
        case OP_LOAD_NATIVE_MODULE:
            return simple_instruction("OP_LOAD_NATIVE_MODULE", offset);
        case OP_IMPORT_FROM:
            return simple_instruction("OP_IMPORT_FROM", offset);
        case OP_DEFINE_STRUCT: {
            printf("OP_DEFINE_STRUCT ");
            uint8_t name_const = chunk->code[offset + 1];
            uint8_t field_count = chunk->code[offset + 2];
            printf("name=%d fields=%d [", name_const, field_count);
            for (int i = 0; i < field_count; i++) {
                if (i > 0) printf(", ");
                printf("%d", chunk->code[offset + 3 + i]);
            }
            printf("]\n");
            return offset + 3 + field_count;
        }
        case OP_CREATE_STRUCT:
            return constant_instruction("OP_CREATE_STRUCT", chunk, offset);
        case OP_GET_FIELD:
        case OP_SET_FIELD:
            return constant_instruction(instruction == OP_GET_FIELD ? "OP_GET_FIELD" : "OP_SET_FIELD", chunk, offset);
        case OP_GET_PROPERTY:
        case OP_SET_PROPERTY:
            return constant_instruction(instruction == OP_GET_PROPERTY ? "OP_GET_PROPERTY" : "OP_SET_PROPERTY", chunk, offset);
        default:
            printf("Unknown opcode %d\n", instruction);
            return offset + 1;
    }
}

void disassemble_chunk(Chunk* chunk, const char* name) {
    printf("== %s ==\n", name);
    
    for (size_t offset = 0; offset < chunk->count;) {
        offset = disassemble_instruction(chunk, offset);
    }
}

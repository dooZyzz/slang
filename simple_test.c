#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "codegen/compiler.h"
#include "runtime/core/vm.h"
#include "debug/debug.h"

// Minimal test to check if bytecode is generated correctly
void test_function_compilation(void) {
    printf("Testing function compilation bytecode order\n");
    
    // Create a mock chunk
    Chunk chunk;
    chunk.count = 0;
    chunk.capacity = 256;
    chunk.code = malloc(256);
    chunk.lines = malloc(256 * sizeof(int));
    chunk.constants.count = 0;
    chunk.constants.capacity = 8;
    chunk.constants.values = malloc(8 * sizeof(TaggedValue));
    
    // Simulate compiling:
    // func f() {}
    // f()
    
    // 1. Add string constants
    chunk.constants.values[0] = (TaggedValue){.type = VAL_STRING, .as.string = "f"};
    chunk.constants.values[1] = (TaggedValue){.type = VAL_STRING, .as.string = "f"};
    chunk.constants.count = 2;
    
    // 2. Function definition
    printf("Function definition at offset %zu:\n", chunk.count);
    // OP_CONSTANT (function value would go here)
    chunk.code[chunk.count++] = OP_CONSTANT;
    chunk.code[chunk.count++] = 0; // function constant index
    // OP_DEFINE_GLOBAL
    chunk.code[chunk.count++] = OP_DEFINE_GLOBAL;
    chunk.code[chunk.count++] = 0; // name constant "f"
    
    printf("Function call at offset %zu:\n", chunk.count);
    // 3. Function call
    // OP_GET_GLOBAL
    chunk.code[chunk.count++] = OP_GET_GLOBAL;
    chunk.code[chunk.count++] = 1; // name constant "f"
    // OP_CALL
    chunk.code[chunk.count++] = OP_CALL;
    chunk.code[chunk.count++] = 0; // 0 arguments
    
    // 4. Return
    chunk.code[chunk.count++] = OP_NIL;
    chunk.code[chunk.count++] = OP_RETURN;
    
    // Print disassembly
    printf("\nDisassembly:\n");
    for (size_t i = 0; i < chunk.count; i++) {
        printf("%04zu ", i);
        
        uint8_t instruction = chunk.code[i];
        switch (instruction) {
            case OP_CONSTANT:
                printf("OP_CONSTANT     %d\n", chunk.code[++i]);
                break;
            case OP_DEFINE_GLOBAL:
                printf("OP_DEFINE_GLOBAL %d\n", chunk.code[++i]);
                break;
            case OP_GET_GLOBAL:
                printf("OP_GET_GLOBAL    %d\n", chunk.code[++i]);
                break;
            case OP_CALL:
                printf("OP_CALL         %d\n", chunk.code[++i]);
                break;
            case OP_NIL:
                printf("OP_NIL\n");
                break;
            case OP_RETURN:
                printf("OP_RETURN\n");
                break;
            default:
                printf("Unknown opcode %d\n", instruction);
                break;
        }
    }
    
    // Free memory
    free(chunk.code);
    free(chunk.lines);
    free(chunk.constants.values);
}

int main() {
    test_function_compilation();
    return 0;
}
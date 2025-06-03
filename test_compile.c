#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "codegen/compiler.h"
#include "runtime/core/vm.h"
#include "debug/debug.h"
#include "utils/allocators.h"

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <file>\n", argv[0]);
        return 1;
    }

    // Read file
    FILE* file = fopen(argv[1], "rb");
    if (!file) {
        fprintf(stderr, "Could not open file \"%s\".\n", argv[1]);
        return 1;
    }

    fseek(file, 0L, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);

    char* source = (char*)malloc(fileSize + 1);
    if (source == NULL) {
        fprintf(stderr, "Not enough memory to read \"%s\".\n", argv[1]);
        fclose(file);
        return 1;
    }

    size_t bytesRead = fread(source, sizeof(char), fileSize, file);
    if (bytesRead < fileSize) {
        fprintf(stderr, "Could not read file \"%s\".\n", argv[1]);
        free(source);
        fclose(file);
        return 1;
    }

    source[bytesRead] = '\0';
    fclose(file);

    // Initialize allocators
    allocators_init();

    // Initialize lexer
    Lexer lexer;
    lexer_init(&lexer, source);

    // Parse the source
    Parser parser;
    parser_init(&parser, &lexer);
    ProgramNode* program = parse(&parser);

    if (program == NULL) {
        fprintf(stderr, "Parse error\n");
        free(source);
        allocators_cleanup();
        return 1;
    }

    // Compile the program
    Chunk chunk;
    chunk_init(&chunk);
    
    if (!compile(program, &chunk)) {
        fprintf(stderr, "Compilation error\n");
        ast_free_program(program);
        chunk_free(&chunk);
        free(source);
        allocators_cleanup();
        return 1;
    }

    // Disassemble the bytecode
    printf("=== Compiled bytecode ===\n");
    disassemble_chunk(&chunk, "script");

    // Cleanup
    ast_free_program(program);
    chunk_free(&chunk);
    free(source);
    allocators_cleanup();

    return 0;
}
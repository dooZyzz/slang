#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "codegen/compiler.h"
#include "runtime/module_format.h"
#include "runtime/package.h"
#include "vm/vm.h"

static void print_usage(const char* program) {
    printf("Usage: %s <input.swift> -o <output.swiftmodule>\n", program);
    printf("Compile a Swift source file into a module\n");
}

int main(int argc, char* argv[]) {
    if (argc != 4 || strcmp(argv[2], "-o") != 0) {
        print_usage(argv[0]);
        return 1;
    }
    
    const char* input_file = argv[1];
    const char* output_file = argv[3];
    
    // Read source file
    FILE* file = fopen(input_file, "r");
    if (!file) {
        fprintf(stderr, "Error: Cannot open input file: %s\n", input_file);
        return 1;
    }
    
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    char* source = malloc(size + 1);
    fread(source, 1, size, file);
    source[size] = '\0';
    fclose(file);
    
    // Parse source
    Parser* parser = parser_create(source);
    ProgramNode* program = parser_parse_program(parser);
    
    if (parser->had_error) {
        fprintf(stderr, "Error: Failed to parse source file\n");
        parser_destroy(parser);
        free(source);
        return 1;
    }
    
    // Compile to bytecode
    Chunk chunk;
    chunk_init(&chunk);
    
    if (!compile(program, &chunk)) {
        fprintf(stderr, "Error: Failed to compile source file\n");
        parser_destroy(parser);
        chunk_free(&chunk);
        free(source);
        return 1;
    }
    
    // Extract module name from filename
    char module_name[256];
    const char* base = strrchr(input_file, '/');
    if (!base) base = input_file;
    else base++;
    
    strncpy(module_name, base, sizeof(module_name) - 1);
    char* dot = strrchr(module_name, '.');
    if (dot) *dot = '\0';
    
    // Create module writer
    ModuleWriter* writer = module_writer_create(output_file);
    if (!writer) {
        fprintf(stderr, "Error: Cannot create output file: %s\n", output_file);
        parser_destroy(parser);
        chunk_free(&chunk);
        free(source);
        return 1;
    }
    
    // Write module metadata
    module_writer_add_metadata(writer, module_name, "1.0.0");
    
    // Write bytecode
    module_writer_add_bytecode(writer, chunk.code, chunk.count);
    
    // TODO: Extract and write exports from AST
    
    // Finalize module
    if (!module_writer_finalize(writer)) {
        fprintf(stderr, "Error: Failed to finalize module\n");
        module_writer_destroy(writer);
        parser_destroy(parser);
        chunk_free(&chunk);
        free(source);
        return 1;
    }
    
    module_writer_destroy(writer);
    parser_destroy(parser);
    chunk_free(&chunk);
    free(source);
    
    printf("Successfully compiled %s to %s\n", input_file, output_file);
    return 0;
}
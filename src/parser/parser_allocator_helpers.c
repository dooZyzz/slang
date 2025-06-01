#include "parser/parser.h"
#include "utils/allocators.h"
#include <string.h>

// Helper functions for parser memory management using allocators

// Duplicate a string using parser allocator
char* parser_strdup(const char* str)
{
    if (!str) return NULL;
    return MEM_STRDUP(allocators_get(ALLOC_SYSTEM_PARSER), str);
}

// Duplicate a lexeme from token
char* parser_strdup_lexeme(Token* token)
{
    if (!token || !token->lexeme) return NULL;
    
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_PARSER);
    char* result = MEM_ALLOC(alloc, token->lexeme_length + 1);
    if (result) {
        memcpy(result, token->lexeme, token->lexeme_length);
        result[token->lexeme_length] = '\0';
    }
    return result;
}

// Free a string allocated by parser
void parser_free_string(char* str)
{
    if (!str) return;
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_PARSER);
    MEM_FREE(alloc, str, strlen(str) + 1);
}

// Allocate array with parser allocator
void* parser_alloc_array(size_t element_size, size_t count)
{
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_PARSER);
    return MEM_ALLOC(alloc, element_size * count);
}

// Reallocate array with parser allocator
void* parser_realloc_array(void* ptr, size_t old_size, size_t new_size)
{
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_PARSER);
    return MEM_REALLOC(alloc, ptr, old_size, new_size);
}

// Free array with parser allocator
void parser_free_array(void* ptr, size_t size)
{
    if (!ptr) return;
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_PARSER);
    MEM_FREE(alloc, ptr, size);
}

// Helper to resize dynamic arrays during parsing
#define PARSER_RESIZE_ARRAY(arr, type, old_cap, new_cap) \
    do { \
        type* new_arr = (type*)parser_alloc_array(sizeof(type), new_cap); \
        if (new_arr && arr) { \
            memcpy(new_arr, arr, sizeof(type) * old_cap); \
            parser_free_array(arr, sizeof(type) * old_cap); \
        } \
        arr = new_arr; \
    } while(0)

// Track allocation info for complex structures
typedef struct {
    void* ptr;
    size_t size;
    struct AllocInfo* next;
} AllocInfo;

typedef struct {
    AllocInfo* head;
} ParseContext;

// Initialize parse context
ParseContext* parser_context_create(void)
{
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_PARSER);
    return MEM_NEW(alloc, ParseContext);
}

// Add allocation to context for tracking
void parser_context_track(ParseContext* ctx, void* ptr, size_t size)
{
    if (!ctx || !ptr) return;
    
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_PARSER);
    AllocInfo* info = MEM_NEW(alloc, AllocInfo);
    if (info) {
        info->ptr = ptr;
        info->size = size;
        info->next = ctx->head;
        ctx->head = info;
    }
}

// Free all tracked allocations
void parser_context_destroy(ParseContext* ctx)
{
    if (!ctx) return;
    
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_PARSER);
    AllocInfo* current = ctx->head;
    while (current) {
        AllocInfo* next = current->next;
        // Note: We don't free the tracked pointers here 
        // as they're owned by the AST
        MEM_FREE(alloc, current, sizeof(AllocInfo));
        current = next;
    }
    MEM_FREE(alloc, ctx, sizeof(ParseContext));
}
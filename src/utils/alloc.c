#include "utils/alloc.h"
#include "utils/memory.h"

// Thread-local would be better, but keeping it simple for now
static Allocator* global_allocator = NULL;

Allocator* get_allocator(void) {
    if (!global_allocator) {
        global_allocator = mem_get_default_allocator();
    }
    return global_allocator;
}

void set_allocator(Allocator* allocator) {
    global_allocator = allocator;
}
#include <stdio.h>
#include <stdlib.h>
#include "utils/memory.h"
#include "utils/alloc.h"
#include "utils/hash_map_v2.h"

// Example struct
typedef struct Person {
    char* name;
    int age;
    struct Person* next;
} Person;

// Demonstrate platform allocator
void demo_platform_allocator(void) {
    printf("\n=== Platform Allocator Demo ===\n");
    
    Allocator* alloc = mem_create_platform_allocator();
    
    // Allocate some memory
    int* numbers = MEM_NEW_ARRAY(alloc, int, 10);
    for (int i = 0; i < 10; i++) {
        numbers[i] = i * i;
    }
    
    char* string = MEM_STRDUP(alloc, "Hello from platform allocator!");
    printf("String: %s\n", string);
    
    // Show stats
    char* stats = mem_format_stats(alloc);
    printf("%s\n", stats);
    free(stats);
    
    // Clean up
    MEM_FREE(alloc, numbers, sizeof(int) * 10);
    MEM_FREE(alloc, string, strlen(string) + 1);
    
    mem_destroy(alloc);
}

// Demonstrate arena allocator - great for temporary allocations
void demo_arena_allocator(void) {
    printf("\n=== Arena Allocator Demo ===\n");
    
    Allocator* arena = mem_create_arena_allocator(4096);
    
    // Create a linked list without worrying about individual frees
    Person* head = NULL;
    Person* tail = NULL;
    
    for (int i = 0; i < 5; i++) {
        Person* p = MEM_NEW(arena, Person);
        p->age = 20 + i;
        
        char name[32];
        snprintf(name, sizeof(name), "Person %d", i);
        p->name = MEM_STRDUP(arena, name);
        p->next = NULL;
        
        if (!head) {
            head = tail = p;
        } else {
            tail->next = p;
            tail = p;
        }
    }
    
    // Process the list
    printf("People in arena:\n");
    for (Person* p = head; p; p = p->next) {
        printf("  %s, age %d\n", p->name, p->age);
    }
    
    // Show stats before reset
    char* stats = mem_format_stats(arena);
    printf("\nBefore reset:\n%s\n", stats);
    free(stats);
    
    // Reset arena - all memory freed at once!
    mem_reset(arena);
    
    stats = mem_format_stats(arena);
    printf("\nAfter reset:\n%s\n", stats);
    free(stats);
    
    mem_destroy(arena);
}

// Demonstrate trace allocator - great for finding memory leaks
void demo_trace_allocator(void) {
    printf("\n=== Trace Allocator Demo ===\n");
    
    Allocator* platform = mem_create_platform_allocator();
    Allocator* trace = mem_create_trace_allocator(platform);
    
    // Set as default allocator
    set_allocator(trace);
    
    // Create hash map with traced allocations
    HashMap* map = hash_map_create();
    
    // Add some tagged allocations
    int* game_state = MEM_NEW_ARRAY_TAGGED(trace, int, 100, "game-state");
    float* physics_data = MEM_NEW_ARRAY_TAGGED(trace, float, 50, "physics");
    char* player_name = MEM_STRDUP_TAGGED(trace, "Player One", "player-data");
    
    // Add to hash map
    hash_map_put(map, "state", game_state);
    hash_map_put(map, "physics", physics_data);
    hash_map_put(map, "player", player_name);
    
    // Simulate a memory leak - forget to free physics_data
    MEM_FREE(trace, game_state, sizeof(int) * 100);
    MEM_FREE(trace, player_name, strlen(player_name) + 1);
    // MEM_FREE(trace, physics_data, sizeof(float) * 50);  // Oops, forgot this!
    
    // Destroy hash map
    hash_map_destroy(map);
    
    // Show detailed stats - will show leak by tag
    char* stats = mem_format_stats(trace);
    printf("%s\n", stats);
    free(stats);
    
    // Check for leaks
    printf("\nChecking for leaks...\n");
    mem_check_leaks(trace);
    
    // Clean up the leak
    MEM_FREE(trace, physics_data, sizeof(float) * 50);
    
    // Reset default allocator
    set_allocator(NULL);
    
    mem_destroy(trace);
    mem_destroy(platform);
}

// Demonstrate freelist allocator - great for fixed-size allocations
void demo_freelist_allocator(void) {
    printf("\n=== Freelist Allocator Demo ===\n");
    
    // Create freelist for 64-byte blocks
    Allocator* freelist = mem_create_freelist_allocator(64, 10);
    
    // Allocate some fixed-size nodes
    typedef struct Node {
        int data;
        struct Node* left;
        struct Node* right;
        char padding[64 - sizeof(int) - 2 * sizeof(struct Node*)];
    } Node;
    
    printf("Node size: %zu bytes\n", sizeof(Node));
    
    Node* nodes[5];
    for (int i = 0; i < 5; i++) {
        nodes[i] = MEM_NEW(freelist, Node);
        nodes[i]->data = i;
        nodes[i]->left = nodes[i]->right = NULL;
    }
    
    // Free some nodes
    MEM_FREE(freelist, nodes[1], sizeof(Node));
    MEM_FREE(freelist, nodes[3], sizeof(Node));
    
    // Allocate again - will reuse freed blocks
    Node* reused1 = MEM_NEW(freelist, Node);
    Node* reused2 = MEM_NEW(freelist, Node);
    
    printf("Reused nodes allocated at: %p, %p\n", (void*)reused1, (void*)reused2);
    printf("Original freed nodes were at: %p, %p\n", (void*)nodes[3], (void*)nodes[1]);
    
    // Show stats
    char* stats = mem_format_stats(freelist);
    printf("\n%s\n", stats);
    free(stats);
    
    mem_destroy(freelist);
}

// Demonstrate using different allocators together
void demo_mixed_allocators(void) {
    printf("\n=== Mixed Allocators Demo ===\n");
    
    // Create allocators
    Allocator* platform = mem_create_platform_allocator();
    Allocator* trace = mem_create_trace_allocator(platform);
    Allocator* arena = mem_create_arena_allocator(8192);
    
    // Use trace as default for debugging
    set_allocator(trace);
    
    // Long-lived data with platform allocator
    char* config = MEM_STRDUP_TAGGED(trace, "game.config", "config");
    
    // Temporary calculations with arena
    WITH_ARENA(temp_arena, 1024, {
        double* matrix = MEM_NEW_ARRAY(temp_arena, double, 16);
        for (int i = 0; i < 16; i++) {
            matrix[i] = i * 0.1;
        }
        
        // Process matrix...
        printf("Matrix sum: %.2f\n", matrix[0] + matrix[15]);
        // No need to free - arena will handle it
    });
    
    // Per-frame allocations with main arena
    for (int frame = 0; frame < 3; frame++) {
        printf("\nFrame %d:\n", frame);
        
        // Allocate frame data
        int* frame_data = MEM_NEW_ARRAY_TAGGED(arena, int, 100, "frame-data");
        for (int i = 0; i < 10; i++) {
            frame_data[i] = frame * 10 + i;
        }
        
        // Process frame...
        printf("  Frame data: %d, %d, %d...\n", 
               frame_data[0], frame_data[1], frame_data[2]);
        
        // Show arena usage
        AllocatorStats stats = mem_get_stats(arena);
        printf("  Arena usage: %zu bytes\n", stats.current_usage);
    }
    
    // Reset arena for next batch of frames
    printf("\nResetting arena...\n");
    mem_reset(arena);
    AllocatorStats stats = mem_get_stats(arena);
    printf("Arena usage after reset: %zu bytes\n", stats.current_usage);
    
    // Clean up
    MEM_FREE(trace, config, strlen(config) + 1);
    
    // Final memory report
    printf("\nFinal memory report:\n");
    char* trace_stats = mem_format_stats(trace);
    printf("%s\n", trace_stats);
    free(trace_stats);
    
    set_allocator(NULL);
    mem_destroy(arena);
    mem_destroy(trace);
    mem_destroy(platform);
}

int main(void) {
    printf("=== SwiftLang Memory Allocator Demo ===\n");
    printf("This demo shows different allocator types and their use cases.\n");
    
    // Initialize memory system
    mem_init();
    
    // Run demos
    demo_platform_allocator();
    demo_arena_allocator();
    demo_trace_allocator();
    demo_freelist_allocator();
    demo_mixed_allocators();
    
    // Shutdown memory system
    mem_shutdown();
    
    printf("\n=== Demo Complete ===\n");
    return 0;
}
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include "vm/object.h"
#include "vm/object_hash.h"
#include "vm/vm.h"

// Get current time in microseconds
static double get_time_us() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000.0 + tv.tv_usec;
}

// Benchmark property access
void benchmark_property_access(int num_properties, int num_lookups) {
    printf("\nBenchmarking property access with %d properties, %d lookups\n", 
           num_properties, num_lookups);
    
    // Create test data
    char** keys = malloc(num_properties * sizeof(char*));
    TaggedValue* values = malloc(num_properties * sizeof(TaggedValue));
    
    for (int i = 0; i < num_properties; i++) {
        keys[i] = malloc(32);
        snprintf(keys[i], 32, "property_%d", i);
        values[i] = NUMBER_VAL(i);
    }
    
    // Benchmark original linked list implementation
    Object* obj_list = object_create();
    double start = get_time_us();
    
    // Insert properties
    for (int i = 0; i < num_properties; i++) {
        object_set_property(obj_list, keys[i], values[i]);
    }
    
    double insert_time_list = get_time_us() - start;
    
    // Lookup properties
    start = get_time_us();
    for (int i = 0; i < num_lookups; i++) {
        int idx = rand() % num_properties;
        TaggedValue* val = object_get_property(obj_list, keys[idx]);
        (void)val; // Avoid unused warning
    }
    double lookup_time_list = get_time_us() - start;
    
    // Benchmark optimized hash table implementation
    Object* obj_hash = object_create_optimized();
    start = get_time_us();
    
    // Insert properties
    for (int i = 0; i < num_properties; i++) {
        object_set_property_optimized(obj_hash, keys[i], values[i]);
    }
    
    double insert_time_hash = get_time_us() - start;
    
    // Lookup properties
    start = get_time_us();
    for (int i = 0; i < num_lookups; i++) {
        int idx = rand() % num_properties;
        TaggedValue* val = object_get_property_optimized(obj_hash, keys[idx]);
        (void)val;
    }
    double lookup_time_hash = get_time_us() - start;
    
    // Benchmark batch insertion
    Object* obj_batch = create_module_export_object(num_properties);
    start = get_time_us();
    object_set_properties_batch(obj_batch, (const char**)keys, values, num_properties);
    double batch_time = get_time_us() - start;
    
    // Print results
    printf("Linked List Implementation:\n");
    printf("  Insert time: %.2f µs (%.2f µs per property)\n", 
           insert_time_list, insert_time_list / num_properties);
    printf("  Lookup time: %.2f µs (%.2f µs per lookup)\n", 
           lookup_time_list, lookup_time_list / num_lookups);
    
    printf("\nHash Table Implementation:\n");
    printf("  Insert time: %.2f µs (%.2f µs per property)\n", 
           insert_time_hash, insert_time_hash / num_properties);
    printf("  Lookup time: %.2f µs (%.2f µs per lookup)\n", 
           lookup_time_hash, lookup_time_hash / num_lookups);
    
    printf("\nBatch Insert (pre-sized hash):\n");
    printf("  Insert time: %.2f µs (%.2f µs per property)\n", 
           batch_time, batch_time / num_properties);
    
    printf("\nSpeedup:\n");
    printf("  Insert: %.2fx faster\n", insert_time_list / insert_time_hash);
    printf("  Lookup: %.2fx faster\n", lookup_time_list / lookup_time_hash);
    printf("  Batch: %.2fx faster than hash\n", insert_time_hash / batch_time);
    
    // Cleanup
    for (int i = 0; i < num_properties; i++) {
        free(keys[i]);
    }
    free(keys);
    free(values);
}

// Benchmark module loading scenario
void benchmark_module_loading() {
    printf("\n=== Module Loading Benchmark ===\n");
    
    // Small module (10 exports)
    benchmark_property_access(10, 1000);
    
    // Medium module (100 exports)
    benchmark_property_access(100, 10000);
    
    // Large module (1000 exports)
    benchmark_property_access(1000, 100000);
}

// Benchmark import * scenario
void benchmark_import_all(int num_properties) {
    printf("\nBenchmarking import * with %d properties\n", num_properties);
    
    // Create source module
    Object* source = create_module_export_object(num_properties);
    char** keys = malloc(num_properties * sizeof(char*));
    TaggedValue* values = malloc(num_properties * sizeof(TaggedValue));
    
    for (int i = 0; i < num_properties; i++) {
        keys[i] = malloc(32);
        snprintf(keys[i], 32, "export_%d", i);
        values[i] = NUMBER_VAL(i);
    }
    
    object_set_properties_batch(source, (const char**)keys, values, num_properties);
    
    // Benchmark copying all properties (import *)
    Object* target = object_create_optimized();
    
    double start = get_time_us();
    
    // Iterator-based approach
    struct {
        Object* target;
        int count;
    } context = { target, 0 };
    
    object_iterate_properties(source, 
        [](const char* key, TaggedValue* value, void* user_data) {
            struct { Object* target; int count; }* ctx = user_data;
            object_set_property_optimized(ctx->target, key, *value);
            ctx->count++;
        }, &context);
    
    double import_time = get_time_us() - start;
    
    printf("  Import * time: %.2f µs (%.2f µs per property)\n", 
           import_time, import_time / num_properties);
    printf("  Properties imported: %d\n", context.count);
    
    // Cleanup
    for (int i = 0; i < num_properties; i++) {
        free(keys[i]);
    }
    free(keys);
    free(values);
}

int main() {
    printf("SwiftLang Module Performance Benchmark\n");
    printf("=====================================\n");
    
    // Seed random number generator
    srand(time(NULL));
    
    // Run benchmarks
    benchmark_module_loading();
    
    printf("\n=== Import * Benchmark ===\n");
    benchmark_import_all(50);
    benchmark_import_all(200);
    
    return 0;
}
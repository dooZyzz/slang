


#include "utils/cli.h"
#include "utils/allocators.h"
#include "utils/logger.h"

int main(int argc, char* argv[]) {
    // Initialize allocator system with default configuration
    AllocatorConfig alloc_config = {
        .enable_trace = false,  // Set to true for memory debugging
        .enable_stats = true,
        .arena_size = 256 * 1024,  // 256KB arenas
        .object_pool_size = 256
    };
    
    // Check environment for memory debugging
    const char* mem_debug = getenv("SWIFTLANG_MEM_DEBUG");
    if (mem_debug && strcmp(mem_debug, "1") == 0) {
        alloc_config.enable_trace = true;
    }
    
    // Initialize allocators
    allocators_init(&alloc_config);
    
    // Run the CLI
    int result = cli_main(argc, argv);
    
    // Print memory statistics if enabled
    if (alloc_config.enable_stats) {
        const char* mem_stats = getenv("SWIFTLANG_MEM_STATS");
        if (mem_stats && strcmp(mem_stats, "1") == 0) {
            allocators_print_stats();
        }
    }
    
    // Check for memory leaks if tracing
    if (alloc_config.enable_trace) {
        allocators_check_leaks();
    }
    
    // Shutdown allocators
    allocators_shutdown();
    
    return result;
}
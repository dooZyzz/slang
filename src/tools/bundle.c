#include "runtime/module_bundle.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

/**
 * SwiftLang Bundle Tool
 * Command-line tool for creating and managing bundles.
 */

static void print_usage(const char* program) {
    printf("SwiftLang Bundle Tool\n");
    printf("Usage: %s <command> [options]\n\n", program);
    printf("Commands:\n");
    printf("  create    Create a new bundle\n");
    printf("  info      Show bundle information\n");
    printf("  list      List bundle contents\n");
    printf("  extract   Extract files from bundle\n");
    printf("  verify    Verify bundle integrity\n");
    printf("  run       Execute a bundle\n\n");
    printf("Create Options:\n");
    printf("  -o, --output <file>      Output bundle file (required)\n");
    printf("  -e, --entry <module>     Entry point module\n");
    printf("  -t, --type <type>        Bundle type (app|lib|plugin)\n");
    printf("  -n, --name <name>        Bundle name\n");
    printf("  -v, --version <version>  Bundle version\n");
    printf("  -d, --desc <desc>        Bundle description\n");
    printf("  -c, --compress           Compress bundle contents\n");
    printf("  -s, --strip              Strip debug information\n");
    printf("  -r, --recursive          Include dependencies recursively\n");
    printf("  -m, --module <path>      Add module (can be repeated)\n");
    printf("  -R, --resource <src:dst> Add resource file\n");
}

static int cmd_create(int argc, char** argv) {
    // Parse options
    static struct option long_options[] = {
        {"output", required_argument, 0, 'o'},
        {"entry", required_argument, 0, 'e'},
        {"type", required_argument, 0, 't'},
        {"name", required_argument, 0, 'n'},
        {"version", required_argument, 0, 'v'},
        {"desc", required_argument, 0, 'd'},
        {"compress", no_argument, 0, 'c'},
        {"strip", no_argument, 0, 's'},
        {"recursive", no_argument, 0, 'r'},
        {"module", required_argument, 0, 'm'},
        {"resource", required_argument, 0, 'R'},
        {0, 0, 0, 0}
    };
    
    BundleOptions options = {
        .type = BUNDLE_TYPE_APPLICATION,
        .compress = false,
        .strip_debug = false,
        .include_sources = false,
        .optimize = false
    };
    
    BundleMetadata metadata = {0};
    char** modules = NULL;
    size_t module_count = 0;
    char** resources = NULL;
    size_t resource_count = 0;
    bool recursive = false;
    
    int opt;
    while ((opt = getopt_long(argc, argv, "o:e:t:n:v:d:csrm:R:", long_options, NULL)) != -1) {
        switch (opt) {
            case 'o':
                options.output_path = optarg;
                break;
            case 'e':
                options.entry_point = optarg;
                metadata.entry_point = optarg;
                break;
            case 't':
                if (strcmp(optarg, "app") == 0) {
                    options.type = BUNDLE_TYPE_APPLICATION;
                } else if (strcmp(optarg, "lib") == 0) {
                    options.type = BUNDLE_TYPE_LIBRARY;
                } else if (strcmp(optarg, "plugin") == 0) {
                    options.type = BUNDLE_TYPE_PLUGIN;
                } else {
                    fprintf(stderr, "Invalid bundle type: %s\n", optarg);
                    return 1;
                }
                break;
            case 'n':
                metadata.name = optarg;
                break;
            case 'v':
                metadata.version = optarg;
                break;
            case 'd':
                metadata.description = optarg;
                break;
            case 'c':
                options.compress = true;
                break;
            case 's':
                options.strip_debug = true;
                break;
            case 'r':
                recursive = true;
                break;
            case 'm':
                modules = realloc(modules, (module_count + 1) * sizeof(char*));
                modules[module_count++] = optarg;
                break;
            case 'R':
                resources = realloc(resources, (resource_count + 1) * sizeof(char*));
                resources[resource_count++] = optarg;
                break;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    // Validate options
    if (!options.output_path) {
        fprintf(stderr, "Error: Output file required\n");
        return 1;
    }
    
    if (options.type == BUNDLE_TYPE_APPLICATION && !options.entry_point) {
        fprintf(stderr, "Error: Entry point required for application bundles\n");
        return 1;
    }
    
    // Create bundle
    printf("Creating bundle: %s\n", options.output_path);
    
    void* builder = bundle_builder_create(&options);
    if (!builder) {
        fprintf(stderr, "Failed to create bundle builder\n");
        return 1;
    }
    
    // Set metadata
    if (metadata.name || metadata.version || metadata.description) {
        bundle_builder_set_metadata(builder, &metadata);
    }
    
    // Add modules
    for (size_t i = 0; i < module_count; i++) {
        printf("Adding module: %s\n", modules[i]);
        if (!bundle_builder_add_module(builder, modules[i], NULL)) {
            fprintf(stderr, "Failed to add module: %s\n", modules[i]);
            bundle_builder_destroy(builder);
            return 1;
        }
        
        // Add dependencies if recursive
        if (recursive) {
            int deps = bundle_builder_add_dependencies(builder, modules[i], true);
            if (deps > 0) {
                printf("  Added %d dependencies\n", deps);
            }
        }
    }
    
    // Add entry point if not already added
    if (options.entry_point) {
        bundle_builder_add_module(builder, options.entry_point, NULL);
        if (recursive) {
            bundle_builder_add_dependencies(builder, options.entry_point, true);
        }
    }
    
    // Add resources
    for (size_t i = 0; i < resource_count; i++) {
        char* src = resources[i];
        char* dst = strchr(src, ':');
        if (dst) {
            *dst++ = '\0';
            printf("Adding resource: %s -> %s\n", src, dst);
            if (!bundle_builder_add_resource(builder, src, dst)) {
                fprintf(stderr, "Failed to add resource: %s\n", src);
            }
        }
    }
    
    // Build bundle
    if (!bundle_builder_build(builder)) {
        fprintf(stderr, "Failed to build bundle\n");
        bundle_builder_destroy(builder);
        return 1;
    }
    
    bundle_builder_destroy(builder);
    printf("Bundle created successfully: %s\n", options.output_path);
    
    free(modules);
    free(resources);
    return 0;
}

static int cmd_info(const char* bundle_path) {
    char* info = bundle_info_json(bundle_path);
    printf("%s", info);
    free(info);
    return 0;
}

static int cmd_list(const char* bundle_path) {
    void* bundle = bundle_open(bundle_path);
    if (!bundle) {
        fprintf(stderr, "Failed to open bundle: %s\n", bundle_path);
        return 1;
    }
    
    printf("Bundle: %s\n", bundle_path);
    
    const BundleMetadata* metadata = bundle_get_metadata(bundle);
    printf("Name: %s\n", metadata->name ? metadata->name : "unknown");
    printf("Version: %s\n", metadata->version ? metadata->version : "unknown");
    printf("Type: %s\n", 
           metadata->type == BUNDLE_TYPE_APPLICATION ? "application" :
           metadata->type == BUNDLE_TYPE_LIBRARY ? "library" : "plugin");
    
    if (metadata->entry_point) {
        printf("Entry Point: %s\n", metadata->entry_point);
    }
    
    printf("\nModules:\n");
    size_t count;
    char** modules = bundle_list_modules(bundle, &count);
    for (size_t i = 0; i < count; i++) {
        printf("  - %s\n", modules[i]);
        free(modules[i]);
    }
    free(modules);
    
    bundle_close(bundle);
    return 0;
}

static int cmd_verify(const char* bundle_path) {
    printf("Verifying bundle: %s\n", bundle_path);
    
    if (bundle_verify(bundle_path)) {
        printf("Bundle is valid\n");
        return 0;
    } else {
        printf("Bundle verification failed\n");
        return 1;
    }
}

static int cmd_run(const char* bundle_path, int argc, char** argv) {
    printf("Executing bundle: %s\n", bundle_path);
    return bundle_execute(bundle_path, argc, argv);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    const char* command = argv[1];
    
    if (strcmp(command, "create") == 0) {
        return cmd_create(argc - 1, argv + 1);
    } else if (strcmp(command, "info") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: %s info <bundle>\n", argv[0]);
            return 1;
        }
        return cmd_info(argv[2]);
    } else if (strcmp(command, "list") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: %s list <bundle>\n", argv[0]);
            return 1;
        }
        return cmd_list(argv[2]);
    } else if (strcmp(command, "verify") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: %s verify <bundle>\n", argv[0]);
            return 1;
        }
        return cmd_verify(argv[2]);
    } else if (strcmp(command, "run") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: %s run <bundle> [args...]\n", argv[0]);
            return 1;
        }
        return cmd_run(argv[2], argc - 3, argv + 3);
    } else {
        fprintf(stderr, "Unknown command: %s\n", command);
        print_usage(argv[0]);
        return 1;
    }
}
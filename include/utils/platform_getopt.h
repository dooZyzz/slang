#ifndef PLATFORM_GETOPT_H
#define PLATFORM_GETOPT_H

#ifdef _WIN32
// Simple getopt implementation for Windows
#include <stdio.h>
#include <string.h>

// Option argument types
#define no_argument 0
#define required_argument 1
#define optional_argument 2

// Long option structure
struct option {
    const char* name;
    int has_arg;
    int* flag;
    int val;
};

// Global variables
static char* optarg = NULL;
static int optind = 1;
static int opterr = 1;
static int optopt = '?';
static int optlong_ind = -1;

static int getopt(int argc, char* const argv[], const char* optstring) {
    static char* current = NULL;
    
    if (optind >= argc) {
        return -1;
    }
    
    if (current == NULL || *current == '\0') {
        current = argv[optind];
        if (current == NULL || current[0] != '-' || current[1] == '\0') {
            return -1;
        }
        
        if (current[1] == '-' && current[2] == '\0') {
            optind++;
            return -1;
        }
        
        current++;
        optind++;
    }
    
    char opt = *current++;
    const char* p = strchr(optstring, opt);
    
    if (p == NULL) {
        optopt = opt;
        if (opterr) {
            fprintf(stderr, "%s: invalid option -- '%c'\n", argv[0], opt);
        }
        return '?';
    }
    
    if (p[1] == ':') {
        if (*current != '\0') {
            optarg = current;
            current = NULL;
        } else if (optind < argc) {
            optarg = argv[optind++];
        } else {
            optopt = opt;
            if (opterr) {
                fprintf(stderr, "%s: option requires an argument -- '%c'\n", argv[0], opt);
            }
            return '?';
        }
    }
    
    return opt;
}

static int getopt_long(int argc, char* const argv[], const char* optstring,
                      const struct option* longopts, int* longindex) {
    static char* current = NULL;
    
    if (optind >= argc) {
        return -1;
    }
    
    if (current == NULL || *current == '\0') {
        current = argv[optind];
        if (current == NULL || current[0] != '-' || current[1] == '\0') {
            return -1;
        }
        
        // Handle "--" end of options
        if (current[1] == '-' && current[2] == '\0') {
            optind++;
            return -1;
        }
        
        // Handle long options "--option"
        if (current[1] == '-') {
            char* option_name = current + 2;
            char* equals_pos = strchr(option_name, '=');
            size_t name_len = equals_pos ? (equals_pos - option_name) : strlen(option_name);
            
            // Find matching long option
            for (int i = 0; longopts[i].name != NULL; i++) {
                if (strncmp(longopts[i].name, option_name, name_len) == 0 && 
                    strlen(longopts[i].name) == name_len) {
                    
                    if (longindex) *longindex = i;
                    optind++;
                    current = NULL;
                    
                    // Handle argument if required
                    if (longopts[i].has_arg == required_argument) {
                        if (equals_pos) {
                            optarg = equals_pos + 1;
                        } else if (optind < argc) {
                            optarg = argv[optind++];
                        } else {
                            if (opterr) {
                                fprintf(stderr, "%s: option '--%s' requires an argument\n", 
                                       argv[0], longopts[i].name);
                            }
                            return '?';
                        }
                    } else if (longopts[i].has_arg == optional_argument) {
                        if (equals_pos) {
                            optarg = equals_pos + 1;
                        } else {
                            optarg = NULL;
                        }
                    } else {
                        optarg = NULL;
                    }
                    
                    // Return value based on flag setting
                    if (longopts[i].flag) {
                        *longopts[i].flag = longopts[i].val;
                        return 0;
                    } else {
                        return longopts[i].val;
                    }
                }
            }
            
            // Long option not found
            if (opterr) {
                fprintf(stderr, "%s: unrecognized option '--%.*s'\n", 
                       argv[0], (int)name_len, option_name);
            }
            optind++;
            current = NULL;
            return '?';
        }
        
        // Handle short options "-o"
        current++;
        optind++;
    }
    
    // Process short option
    char opt = *current++;
    const char* p = strchr(optstring, opt);
    
    if (p == NULL) {
        optopt = opt;
        if (opterr) {
            fprintf(stderr, "%s: invalid option -- '%c'\n", argv[0], opt);
        }
        return '?';
    }
    
    if (p[1] == ':') {
        if (*current != '\0') {
            optarg = current;
            current = NULL;
        } else if (optind < argc) {
            optarg = argv[optind++];
        } else {
            optopt = opt;
            if (opterr) {
                fprintf(stderr, "%s: option requires an argument -- '%c'\n", argv[0], opt);
            }
            return '?';
        }
    }
    
    return opt;
}

#else
#include <getopt.h>
#endif

#endif /* PLATFORM_GETOPT_H */
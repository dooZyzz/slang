#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

/**
 * Semantic version parsing and comparison utilities.
 * Supports versions in the format: major.minor.patch[-prerelease][+build]
 * Examples: 1.0.0, 2.1.0-beta.1, 3.0.0+build.123
 */

typedef struct {
    int major;
    int minor;
    int patch;
    char* prerelease;
    char* build;
} Version;

/**
 * Parse a version string into a Version struct.
 * Returns true on success, false on failure.
 */
bool version_parse(const char* version_string, Version* version) {
    if (!version_string || !version) {
        return false;
    }
    
    // Initialize version
    memset(version, 0, sizeof(Version));
    
    // Create a working copy
    char* str = strdup(version_string);
    if (!str) {
        return false;
    }
    
    // Find build metadata (+)
    char* build_start = strchr(str, '+');
    if (build_start) {
        *build_start = '\0';
        version->build = strdup(build_start + 1);
    }
    
    // Find prerelease (-)
    char* prerelease_start = strchr(str, '-');
    if (prerelease_start) {
        *prerelease_start = '\0';
        version->prerelease = strdup(prerelease_start + 1);
    }
    
    // Parse major.minor.patch
    int parsed = sscanf(str, "%d.%d.%d", &version->major, &version->minor, &version->patch);
    if (parsed < 1) {
        free(str);
        return false;
    }
    
    // Default values for missing parts
    if (parsed < 2) version->minor = 0;
    if (parsed < 3) version->patch = 0;
    
    free(str);
    return true;
}

/**
 * Free memory allocated by version_parse.
 */
void version_free(Version* version) {
    if (version) {
        if (version->prerelease) {
            free(version->prerelease);
            version->prerelease = NULL;
        }
        if (version->build) {
            free(version->build);
            version->build = NULL;
        }
    }
}

/**
 * Compare two versions.
 * Returns: -1 if v1 < v2, 0 if v1 == v2, 1 if v1 > v2
 */
int version_compare(const Version* v1, const Version* v2) {
    // Compare major
    if (v1->major != v2->major) {
        return v1->major < v2->major ? -1 : 1;
    }
    
    // Compare minor
    if (v1->minor != v2->minor) {
        return v1->minor < v2->minor ? -1 : 1;
    }
    
    // Compare patch
    if (v1->patch != v2->patch) {
        return v1->patch < v2->patch ? -1 : 1;
    }
    
    // Handle prerelease versions
    // A version without prerelease is greater than one with
    if (!v1->prerelease && v2->prerelease) return 1;
    if (v1->prerelease && !v2->prerelease) return -1;
    
    // If both have prerelease, compare lexicographically
    if (v1->prerelease && v2->prerelease) {
        int cmp = strcmp(v1->prerelease, v2->prerelease);
        if (cmp != 0) return cmp < 0 ? -1 : 1;
    }
    
    // Build metadata doesn't affect version precedence
    return 0;
}

/**
 * Check if a version satisfies a version requirement.
 * Supports operators: =, >, >=, <, <=, ~> (compatible with)
 * Examples:
 *   "1.0.0" - exact match
 *   ">=1.0.0" - version 1.0.0 or higher
 *   "~>1.0.0" - compatible with 1.0.x (1.0.0 <= version < 2.0.0)
 *   "~>1.0" - compatible with 1.x (1.0.0 <= version < 2.0.0)
 */
bool version_satisfies(const char* version_string, const char* requirement) {
    if (!version_string || !requirement) {
        return false;
    }
    
    Version version;
    if (!version_parse(version_string, &version)) {
        return false;
    }
    
    // Skip whitespace
    while (*requirement && isspace(*requirement)) {
        requirement++;
    }
    
    bool result = false;
    
    // Check for operators
    if (strncmp(requirement, "~>", 2) == 0) {
        // Compatible with operator
        requirement += 2;
        while (*requirement && isspace(*requirement)) requirement++;
        
        Version req_version;
        if (version_parse(requirement, &req_version)) {
            // ~>1.0.0 means >= 1.0.0 and < 2.0.0
            // ~>1.0 means >= 1.0.0 and < 2.0.0
            // ~>1 means >= 1.0.0 and < 2.0.0
            if (version.major == req_version.major) {
                if (req_version.patch > 0 || strchr(requirement, '.') != strrchr(requirement, '.')) {
                    // Full version specified (1.2.3)
                    result = version.minor == req_version.minor && 
                            version_compare(&version, &req_version) >= 0;
                } else {
                    // Only major.minor specified (1.2)
                    result = version_compare(&version, &req_version) >= 0;
                }
            }
            version_free(&req_version);
        }
    } else if (strncmp(requirement, ">=", 2) == 0) {
        requirement += 2;
        while (*requirement && isspace(*requirement)) requirement++;
        
        Version req_version;
        if (version_parse(requirement, &req_version)) {
            result = version_compare(&version, &req_version) >= 0;
            version_free(&req_version);
        }
    } else if (strncmp(requirement, "<=", 2) == 0) {
        requirement += 2;
        while (*requirement && isspace(*requirement)) requirement++;
        
        Version req_version;
        if (version_parse(requirement, &req_version)) {
            result = version_compare(&version, &req_version) <= 0;
            version_free(&req_version);
        }
    } else if (strncmp(requirement, ">", 1) == 0) {
        requirement += 1;
        while (*requirement && isspace(*requirement)) requirement++;
        
        Version req_version;
        if (version_parse(requirement, &req_version)) {
            result = version_compare(&version, &req_version) > 0;
            version_free(&req_version);
        }
    } else if (strncmp(requirement, "<", 1) == 0) {
        requirement += 1;
        while (*requirement && isspace(*requirement)) requirement++;
        
        Version req_version;
        if (version_parse(requirement, &req_version)) {
            result = version_compare(&version, &req_version) < 0;
            version_free(&req_version);
        }
    } else if (strncmp(requirement, "=", 1) == 0) {
        requirement += 1;
        while (*requirement && isspace(*requirement)) requirement++;
        
        Version req_version;
        if (version_parse(requirement, &req_version)) {
            result = version_compare(&version, &req_version) == 0;
            version_free(&req_version);
        }
    } else {
        // No operator means exact match
        Version req_version;
        if (version_parse(requirement, &req_version)) {
            result = version_compare(&version, &req_version) == 0;
            version_free(&req_version);
        }
    }
    
    version_free(&version);
    return result;
}

/**
 * Format a version struct back to a string.
 * Caller must free the returned string.
 */
char* version_to_string(const Version* version) {
    if (!version) {
        return NULL;
    }
    
    // Calculate required size
    size_t size = snprintf(NULL, 0, "%d.%d.%d", version->major, version->minor, version->patch);
    if (version->prerelease) {
        size += strlen(version->prerelease) + 1; // +1 for '-'
    }
    if (version->build) {
        size += strlen(version->build) + 1; // +1 for '+'
    }
    
    char* result = malloc(size + 1);
    if (!result) {
        return NULL;
    }
    
    int written = snprintf(result, size + 1, "%d.%d.%d", version->major, version->minor, version->patch);
    if (version->prerelease) {
        written += snprintf(result + written, size + 1 - written, "-%s", version->prerelease);
    }
    if (version->build) {
        snprintf(result + written, size + 1 - written, "+%s", version->build);
    }
    
    return result;
}
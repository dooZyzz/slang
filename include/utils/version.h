#ifndef VERSION_H
#define VERSION_H

#include <stdbool.h>

/**
 * Semantic version structure.
 * Represents a version in the format: major.minor.patch[-prerelease][+build]
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
 * 
 * @param version_string The version string to parse (e.g., "1.2.3", "2.0.0-beta.1")
 * @param version Output parameter for the parsed version
 * @return true if parsing succeeded, false otherwise
 */
bool version_parse(const char* version_string, Version* version);

/**
 * Free memory allocated by version_parse.
 * 
 * @param version The version struct to free
 */
void version_free(Version* version);

/**
 * Compare two versions according to semantic versioning rules.
 * 
 * @param v1 First version to compare
 * @param v2 Second version to compare
 * @return -1 if v1 < v2, 0 if v1 == v2, 1 if v1 > v2
 */
int version_compare(const Version* v1, const Version* v2);

/**
 * Check if a version satisfies a version requirement.
 * Supports operators: =, >, >=, <, <=, ~> (compatible with)
 * 
 * Examples:
 *   "1.0.0" - exact match
 *   ">=1.0.0" - version 1.0.0 or higher
 *   "~>1.0.0" - compatible with 1.0.x (1.0.0 <= version < 2.0.0)
 *   "~>1.0" - compatible with 1.x (1.0.0 <= version < 2.0.0)
 * 
 * @param version_string The version to check
 * @param requirement The requirement string
 * @return true if the version satisfies the requirement
 */
bool version_satisfies(const char* version_string, const char* requirement);

/**
 * Format a version struct back to a string.
 * Caller must free the returned string.
 * 
 * @param version The version to format
 * @return A newly allocated string representation of the version
 */
char* version_to_string(const Version* version);

#endif // VERSION_H
# FindMiniZ.cmake - Find miniz library
# This module finds the miniz library (single file ZIP library)
#
# It sets the following variables:
#  MINIZ_FOUND - TRUE if miniz is found
#  MINIZ_INCLUDE_DIRS - The miniz include directories
#  MINIZ_LIBRARIES - The libraries needed to use miniz
#  MINIZ_DEFINITIONS - Compiler definitions for miniz

include(FetchContent)

# Try to find system-installed miniz first
find_path(MINIZ_INCLUDE_DIR
        NAMES miniz.h
        PATHS
        /usr/local/include
        /usr/include
        $ENV{PROGRAMFILES}/miniz/include
        $ENV{PROGRAMFILES\(X86\)}/miniz/include
)

find_library(MINIZ_LIBRARY
        NAMES miniz libminiz
        PATHS
        /usr/local/lib
        /usr/lib
        $ENV{PROGRAMFILES}/miniz/lib
        $ENV{PROGRAMFILES\(X86\)}/miniz/lib
)

if(MINIZ_INCLUDE_DIR AND MINIZ_LIBRARY)
    set(MINIZ_FOUND TRUE)
    set(MINIZ_INCLUDE_DIRS ${MINIZ_INCLUDE_DIR})
    set(MINIZ_LIBRARIES ${MINIZ_LIBRARY})
else()
    # If not found, download miniz as a single header/source file
    message(STATUS "miniz not found in system, downloading...")

    FetchContent_Declare(
            miniz
            GIT_REPOSITORY https://github.com/richgel999/miniz.git
            GIT_TAG 3.0.2
    )

    # Use the modern FetchContent approach
    FetchContent_GetProperties(miniz)
    if(NOT miniz_POPULATED)
        FetchContent_Populate(miniz)

        # Create miniz_export.h first
        file(WRITE "${miniz_SOURCE_DIR}/miniz_export.h"
"#ifndef MINIZ_EXPORT_H
#define MINIZ_EXPORT_H

/* Export macros for miniz library */
#ifdef _WIN32
  #ifdef MINIZ_BUILD_SHARED
    #ifdef MINIZ_EXPORTS
      #define MINIZ_EXPORT __declspec(dllexport)
    #else
      #define MINIZ_EXPORT __declspec(dllimport)
    #endif
  #else
    #define MINIZ_EXPORT
  #endif
#else
  #define MINIZ_EXPORT
#endif

#endif /* MINIZ_EXPORT_H */
")

        # Create a compatible CMakeLists.txt for miniz
        file(WRITE "${miniz_SOURCE_DIR}/CMakeLists.txt"
                "cmake_minimum_required(VERSION 3.10)
project(miniz C)

# Check which miniz source files exist
set(MINIZ_SOURCES)
if(EXISTS \${CMAKE_CURRENT_SOURCE_DIR}/miniz.c)
    list(APPEND MINIZ_SOURCES miniz.c)
endif()
if(EXISTS \${CMAKE_CURRENT_SOURCE_DIR}/miniz_tdef.c)
    list(APPEND MINIZ_SOURCES miniz_tdef.c)
endif()
if(EXISTS \${CMAKE_CURRENT_SOURCE_DIR}/miniz_tinfl.c)
    list(APPEND MINIZ_SOURCES miniz_tinfl.c)
endif()
if(EXISTS \${CMAKE_CURRENT_SOURCE_DIR}/miniz_zip.c)
    list(APPEND MINIZ_SOURCES miniz_zip.c)
endif()

# If we only found miniz.c, assume it's the amalgamated version
if(MINIZ_SOURCES STREQUAL \"miniz.c\")
    add_library(miniz STATIC miniz.c)
else()
    # For split source files, add all of them
    add_library(miniz STATIC \${MINIZ_SOURCES})
endif()

# Don't define MINIZ_NO_STDIO as we need file operations
# Define other useful macros
# Note: We don't define MINIZ_NO_ARCHIVE_APIS since we want archive support

target_include_directories(miniz PUBLIC \${CMAKE_CURRENT_SOURCE_DIR})

# Set properties for better compatibility
set_target_properties(miniz PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
)

# Windows-specific configurations
if(WIN32)
    # Ensure proper Windows API definitions
    target_compile_definitions(miniz PRIVATE
        _CRT_SECURE_NO_WARNINGS
        WIN32_LEAN_AND_MEAN
    )

    # Handle MSVC-specific settings
    if(MSVC)
        target_compile_options(miniz PRIVATE /W3)
        # Disable specific warnings that miniz might trigger
        target_compile_options(miniz PRIVATE
            /wd4996  # Disable deprecated function warnings
            /wd4244  # Disable conversion warnings
        )
    endif()
endif()
")

        # Add miniz as a subdirectory
        add_subdirectory(${miniz_SOURCE_DIR} ${miniz_BINARY_DIR})
    endif()

    set(MINIZ_FOUND TRUE)
    set(MINIZ_INCLUDE_DIRS ${miniz_SOURCE_DIR})
    set(MINIZ_LIBRARIES miniz)
    
    # Debug output
    message(STATUS "MINIZ_INCLUDE_DIRS set to: ${MINIZ_INCLUDE_DIRS}")
    message(STATUS "MINIZ_LIBRARIES set to: ${MINIZ_LIBRARIES}")
endif()

# Define compiler definitions for better portability
if(WIN32)
    set(MINIZ_DEFINITIONS
            "-DMINIZ_USE_UNALIGNED_LOADS_AND_STORES=0"
            "-D_CRT_SECURE_NO_WARNINGS"
    )
else()
    set(MINIZ_DEFINITIONS "-DMINIZ_USE_UNALIGNED_LOADS_AND_STORES=0")
endif()

if(MINIZ_FOUND)
    message(STATUS "Found miniz: ${MINIZ_INCLUDE_DIRS}")
else()
    message(FATAL_ERROR "Could not find or download miniz")
endif()

mark_as_advanced(MINIZ_INCLUDE_DIR MINIZ_LIBRARY)
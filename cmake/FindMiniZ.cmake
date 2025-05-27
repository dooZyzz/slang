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
    PATHS /usr/local/include /usr/include
)

find_library(MINIZ_LIBRARY
    NAMES miniz
    PATHS /usr/local/lib /usr/lib
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
    
    FetchContent_GetProperties(miniz)
    if(NOT miniz_POPULATED)
        FetchContent_Populate(miniz)
        
        # Create a simple CMakeLists.txt for miniz if it doesn't exist
        if(NOT EXISTS "${miniz_SOURCE_DIR}/CMakeLists.txt")
            file(WRITE "${miniz_SOURCE_DIR}/CMakeLists.txt" 
"cmake_minimum_required(VERSION 3.10)
project(miniz C)

add_library(miniz STATIC miniz.c)
target_include_directories(miniz PUBLIC \${CMAKE_CURRENT_SOURCE_DIR})
")
        endif()
        
        # Add miniz as a subdirectory
        add_subdirectory(${miniz_SOURCE_DIR} ${miniz_BINARY_DIR})
        
        set(MINIZ_FOUND TRUE)
        set(MINIZ_INCLUDE_DIRS ${miniz_SOURCE_DIR})
        set(MINIZ_LIBRARIES miniz)
    endif()
endif()

# Define MINIZ_USE_UNALIGNED_LOADS_AND_STORES=0 for better portability
set(MINIZ_DEFINITIONS "-DMINIZ_USE_UNALIGNED_LOADS_AND_STORES=0")

if(MINIZ_FOUND)
    message(STATUS "Found miniz: ${MINIZ_INCLUDE_DIRS}")
else()
    message(FATAL_ERROR "Could not find or download miniz")
endif()

mark_as_advanced(MINIZ_INCLUDE_DIR MINIZ_LIBRARY)
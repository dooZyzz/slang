# FindGLFW.cmake - Find GLFW library
# This module finds the GLFW library
#
# It sets the following variables:
#  GLFW_FOUND - TRUE if GLFW is found
#  GLFW_INCLUDE_DIRS - The GLFW include directories
#  GLFW_LIBRARIES - The libraries needed to use GLFW

# Try pkg-config first
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(PC_GLFW QUIET glfw3)
endif()

# Find include directory
find_path(GLFW_INCLUDE_DIR
    NAMES GLFW/glfw3.h
    HINTS
        ${PC_GLFW_INCLUDEDIR}
        ${PC_GLFW_INCLUDE_DIRS}
    PATHS
        /usr/local/include
        /usr/include
        /opt/local/include
        /opt/homebrew/include
)

# Find library
find_library(GLFW_LIBRARY
    NAMES glfw glfw3
    HINTS
        ${PC_GLFW_LIBDIR}
        ${PC_GLFW_LIBRARY_DIRS}
    PATHS
        /usr/local/lib
        /usr/lib
        /opt/local/lib
        /opt/homebrew/lib
)

# Handle the QUIETLY and REQUIRED arguments
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GLFW
    DEFAULT_MSG
    GLFW_LIBRARY GLFW_INCLUDE_DIR
)

if(GLFW_FOUND)
    set(GLFW_LIBRARIES ${GLFW_LIBRARY})
    set(GLFW_INCLUDE_DIRS ${GLFW_INCLUDE_DIR})
    
    # On macOS, we need to link against Cocoa, IOKit, and CoreVideo
    if(APPLE)
        find_library(COCOA_LIBRARY Cocoa)
        find_library(IOKIT_LIBRARY IOKit)
        find_library(COREVIDEO_LIBRARY CoreVideo)
        list(APPEND GLFW_LIBRARIES ${COCOA_LIBRARY} ${IOKIT_LIBRARY} ${COREVIDEO_LIBRARY})
    endif()
endif()

mark_as_advanced(GLFW_INCLUDE_DIR GLFW_LIBRARY)
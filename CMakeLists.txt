cmake_minimum_required(VERSION 3.5)
project(SwiftLikeLang C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Platform-specific compiler flags
if(MSVC)
    # MSVC flags
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W3 /wd4996 /wd4244 /wd4267")
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    # GCC/Clang flags
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic")
    if(APPLE)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-newline-eof")
    endif()
endif()

include_directories(include)

# Add module path for custom Find modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Find miniz for ZIP support
find_package(MiniZ REQUIRED)
include_directories(${MINIZ_INCLUDE_DIRS})
add_definitions(${MINIZ_DEFINITIONS})

# Find GLFW (optional, for GLFW module)
find_package(GLFW)
if(GLFW_FOUND)
    message(STATUS "GLFW found, building GLFW module")
else()
    message(STATUS "GLFW not found, skipping GLFW module")
endif()

# Source files - using refactored versions where available
set(LEXER_SOURCES
    src/lexer/token.c
    src/lexer/lexer.c
)

set(AST_SOURCES
    src/ast/ast.c
    src/ast/ast_printer.c  # Not refactored yet
)

set(PARSER_SOURCES
    src/parser/parser.c
)

set(VM_SOURCES
    src/runtime/core/vm_complete.c
    src/runtime/core/object_complete.c
    src/runtime/core/object_hash.c
    src/runtime/core/string_pool_complete.c
)

set(CODEGEN_SOURCES
    src/codegen/compiler.c
)

set(DEBUG_SOURCES
    src/debug/debug.c
)

set(UTILS_SOURCES
    src/utils/error.c
    src/utils/test_framework.c  # Not refactored yet
    src/utils/cli.c  # Not refactored yet
    src/utils/logger.c  # Not refactored yet
    src/utils/compiler_wrapper.c  # Not refactored yet
    src/utils/bytecode_format.c  # Not refactored yet
    src/utils/hash_map.c
    src/utils/version.c  # Not refactored yet
    src/utils/memory.c
    src/utils/memory_platform.c
    src/utils/memory_arena.c
    src/utils/memory_freelist.c
    src/utils/memory_trace.c
    src/utils/alloc.c
    src/utils/allocators.c
)

set(SEMANTIC_SOURCES
    src/semantic/visitor.c  # Not refactored yet
    src/semantic/type.c  # Not refactored yet
    src/semantic/symbol_table.c  # Not refactored yet
    src/semantic/analyzer.c
)

set(RUNTIME_SOURCES
    # Core runtime
    src/runtime/core/coroutine.c  # Not refactored yet
    src/runtime/core/bootstrap.c  # Not refactored yet
    src/runtime/core/gc.c  # Garbage collector
    
    # Module loader
    src/runtime/modules/loader/module_loader.c
    src/runtime/modules/loader/module_cache.c
    src/runtime/modules/loader/module_compiler.c  # Not refactored yet
    
    # Module formats
    src/runtime/modules/formats/module_format.c
    src/runtime/modules/formats/module_archive.c
    src/runtime/modules/module_bundle.c
    
    # Module extensions
    src/runtime/modules/extensions/module_hooks.c
    src/runtime/modules/extensions/module_inspect.c
    src/runtime/modules/extensions/module_natives.c
    
    # Module lifecycle
    src/runtime/modules/lifecycle/module_unload.c  # Not refactored yet
    src/runtime/modules/lifecycle/builtin_modules.c
    
    # Package management
    src/runtime/packages/package.c  # Not refactored yet
    src/runtime/packages/package_manager.c  # Not refactored yet
)

set(STDLIB_SOURCES
    src/stdlib/stdlib.c
)

# Create static library
add_library(lang_lib STATIC ${LEXER_SOURCES} ${AST_SOURCES} ${PARSER_SOURCES} ${VM_SOURCES} ${CODEGEN_SOURCES} ${DEBUG_SOURCES} ${UTILS_SOURCES} ${SEMANTIC_SOURCES} ${RUNTIME_SOURCES} ${STDLIB_SOURCES})

# Add miniz include directories to the target
target_include_directories(lang_lib PRIVATE ${MINIZ_INCLUDE_DIRS})

# Link with system libraries
if(WIN32)
    target_link_libraries(lang_lib ${MINIZ_LIBRARIES})
else()
    target_link_libraries(lang_lib ${CMAKE_DL_LIBS} ${MINIZ_LIBRARIES})
endif()

# Include test framework
include(cmake/TestFramework.cmake)

# Tests
enable_testing()

# All tests now use the new framework

# New framework-based test suites
# Build as both libraries and standalone executables
#add_dual_test_suite(lexer_unit tests/unit/test_lexer_unit.c)
add_test_suite(lexer_unit tests/unit/test_lexer_unit.c)
add_test_suite(vm_unit tests/unit/test_vm_unit.c)
add_test_suite(symbol_table_unit tests/unit/test_symbol_table_unit.c)
add_test_suite(error_reporter_unit tests/unit/test_error_reporter_unit.c)
add_test_suite(integration tests/integration/test_integration.c)
add_test_suite(array_methods_unit tests/unit/test_array_methods_unit.c)
add_test_suite(array_hof_unit tests/unit/test_array_hof_unit.c)
add_test_suite(string_interp_unit tests/unit/test_string_interp_unit.c)
add_test_suite(for_loop_unit tests/unit/test_for_loop_unit.c)
add_test_suite(modulo_unit tests/unit/test_modulo_unit.c)
add_test_suite(array_assign_unit tests/unit/test_array_assign_unit.c)
add_test_suite(ast_unit tests/unit/test_ast_unit.c)
add_test_suite(string_pool_unit tests/unit/test_string_pool_unit.c)
add_test_suite(object_unit tests/unit/test_object_unit.c)
add_test_suite(error_advanced_unit tests/unit/test_error_advanced_unit.c)
# add_test_suite(syntax_unit tests/unit/test_syntax_unit.c)  # Disabled - not implemented
add_test_suite(module_system_unit tests/unit/test_module_system_unit.c)
add_test_suite(multi_module_unit tests/unit/test_multi_module_unit.c)

# Create main executable
add_executable(swift_like_lang src/main.c)
target_include_directories(swift_like_lang PRIVATE ${MINIZ_INCLUDE_DIRS})

# Export symbols for native modules
if(NOT WIN32)
    target_link_libraries(swift_like_lang lang_lib miniz m)
    # Export all symbols for dlopen'd modules
    set_target_properties(swift_like_lang PROPERTIES
        LINK_FLAGS "-Wl,-export-dynamic"
    )
else()
    target_link_libraries(swift_like_lang lang_lib miniz)
endif()
# Create tools
# Bundle creation tool

# Set rpath for macOS
if(APPLE)
  set_target_properties(swift_like_lang PROPERTIES
    BUILD_RPATH "@loader_path"
    INSTALL_RPATH "@loader_path"
  )
endif()

# Add Doxygen documentation target (optional)
find_package(Doxygen)
if(DOXYGEN_FOUND)
    add_custom_target(doc
        ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Generating API documentation with Doxygen" VERBATIM
    )
endif()
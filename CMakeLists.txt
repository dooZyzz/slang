cmake_minimum_required(VERSION 3.10)
project(SwiftLikeLang C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic -Wno-newline-eof")
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

# Source files
set(LEXER_SOURCES
    src/lexer/token.c
    src/lexer/lexer.c
)

set(AST_SOURCES
    src/ast/ast.c
    # src/ast/ast_printer.c  # TODO: Fix AST printer to match current AST structure
)

set(PARSER_SOURCES
    src/parser/parser.c
)

set(VM_SOURCES
    src/vm/vm.c
    src/vm/array.c
    src/vm/object.c
    src/vm/string_pool.c
)

set(CODEGEN_SOURCES
    src/codegen/compiler.c
)

set(DEBUG_SOURCES
    src/debug/debug.c
)

set(UTILS_SOURCES
    src/utils/error.c
    src/utils/test_framework.c
    src/utils/syntax_test.c
    src/utils/cli.c
    src/utils/logger.c
    src/utils/compiler_wrapper.c
    src/utils/bytecode_format.c
    src/utils/hash_map.c
)

set(SEMANTIC_SOURCES
    src/semantic/visitor.c
    src/semantic/type.c
    src/semantic/symbol_table.c
    src/semantic/analyzer.c
)

set(RUNTIME_SOURCES
    src/runtime/coroutine.c
    src/runtime/module.c
    src/runtime/builtin_modules.c
    src/runtime/package.c
    src/runtime/module_format.c
    src/runtime/module_archive.c
    src/runtime/module_compiler.c
    src/runtime/package_manager.c
    src/runtime/bootstrap.c
)

# Remove this section - we're refactoring the existing system instead

set(STDLIB_SOURCES
    src/stdlib/stdlib.c
)

# Create static library
add_library(lang_lib STATIC ${LEXER_SOURCES} ${AST_SOURCES} ${PARSER_SOURCES} ${VM_SOURCES} ${CODEGEN_SOURCES} ${DEBUG_SOURCES} ${UTILS_SOURCES} ${SEMANTIC_SOURCES} ${RUNTIME_SOURCES} ${STDLIB_SOURCES})

# Link with system libraries
target_link_libraries(lang_lib ${CMAKE_DL_LIBS} ${MINIZ_LIBRARIES})

# Include test framework
include(cmake/TestFramework.cmake)

# Tests
enable_testing()

# All tests now use the new framework

# New framework-based test suites
# Build as both libraries and standalone executables
add_dual_test_suite(lexer_unit tests/unit/test_lexer_unit.c)
add_dual_test_suite(vm_unit tests/unit/test_vm_unit.c)
add_dual_test_suite(symbol_table_unit tests/unit/test_symbol_table_unit.c)
add_dual_test_suite(error_reporter_unit tests/unit/test_error_reporter_unit.c)
add_dual_test_suite(integration tests/integration/test_integration.c)
add_dual_test_suite(array_methods_unit tests/unit/test_array_methods_unit.c)
add_dual_test_suite(array_hof_unit tests/unit/test_array_hof_unit.c)
add_dual_test_suite(string_interp_unit tests/unit/test_string_interp_unit.c)
add_dual_test_suite(for_loop_unit tests/unit/test_for_loop_unit.c)
add_dual_test_suite(modulo_unit tests/unit/test_modulo_unit.c)
add_dual_test_suite(array_assign_unit tests/unit/test_array_assign_unit.c)
add_dual_test_suite(ast_unit tests/unit/test_ast_unit.c)
add_dual_test_suite(string_pool_unit tests/unit/test_string_pool_unit.c)
add_dual_test_suite(object_unit tests/unit/test_object_unit.c)
add_dual_test_suite(error_advanced_unit tests/unit/test_error_advanced_unit.c)
add_dual_test_suite(syntax_unit tests/unit/test_syntax_unit.c)
add_dual_test_suite(module_system_unit tests/unit/test_module_system_unit.c)
add_dual_test_suite(multi_module_unit tests/unit/test_multi_module_unit.c)

# Create the main test runner that uses all test libraries
create_test_runner(
    OUTPUT_NAME test_runner
    TEST_SUITES 
        lexer_unit
        vm_unit
        symbol_table_unit
        error_reporter_unit
        integration
        array_methods_unit
        array_hof_unit
        string_interp_unit
        for_loop_unit
        modulo_unit
        array_assign_unit
        ast_unit
        string_pool_unit
        object_unit
        error_advanced_unit
        syntax_unit
        module_system_unit
        multi_module_unit
)

# Add custom target to run all tests
add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} --verbose)

# Main executable (old system)
add_executable(swiftlang src/main.c)

target_link_libraries(swiftlang lang_lib)

# Test module hierarchy
# add_executable(test_module_hierarchy src/test_module_main.c)
# target_link_libraries(test_module_hierarchy lang_lib)

# Module builder tool - removed, functionality now in CLI

# Native modules (shared libraries)
add_library(net_module SHARED src/stdlib/net_native.c)
target_link_libraries(net_module lang_lib)
set_target_properties(net_module PROPERTIES PREFIX "" OUTPUT_NAME "net")

add_library(fs_module SHARED src/stdlib/fs_native.c)
target_link_libraries(fs_module lang_lib)
set_target_properties(fs_module PROPERTIES PREFIX "" OUTPUT_NAME "fs")

add_library(sys_module SHARED src/stdlib/sys_native.c)
target_link_libraries(sys_module lang_lib)
set_target_properties(sys_module PROPERTIES PREFIX "" OUTPUT_NAME "sys")

# Math module is now in modules/math/src/
add_library(math_module SHARED modules/math/src/math_native.c)
target_link_libraries(math_module m)  # Link with math library only, not lang_lib
target_include_directories(math_module PRIVATE include)
set_target_properties(math_module PROPERTIES PREFIX "" OUTPUT_NAME "math_native")

# Install native modules to a modules directory
install(TARGETS net_module fs_module sys_module
        LIBRARY DESTINATION lib/swiftlang/modules)

# GLFW module (if GLFW is found)
if(GLFW_FOUND)
    add_library(glfw_module SHARED modules/glfw/src/glfw_native_simple.c)
    target_link_libraries(glfw_module lang_lib ${GLFW_LIBRARIES})
    target_include_directories(glfw_module PRIVATE ${GLFW_INCLUDE_DIRS})
    set_target_properties(glfw_module PROPERTIES PREFIX "" OUTPUT_NAME "glfw_native")
    
    # Copy to modules directory for testing
    add_custom_command(TARGET glfw_module POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:glfw_module> ${CMAKE_BINARY_DIR}/modules/
    )
endif()

# OpenGL module
find_package(OpenGL)
if(OpenGL_FOUND)
    add_library(opengl_module SHARED modules/opengl/src/opengl_native.c)
    target_link_libraries(opengl_module lang_lib ${OPENGL_LIBRARIES})
    target_include_directories(opengl_module PRIVATE ${OPENGL_INCLUDE_DIR})
    set_target_properties(opengl_module PROPERTIES PREFIX "" OUTPUT_NAME "opengl_native")
    
    # macOS specific OpenGL settings
    if(APPLE)
        target_compile_definitions(opengl_module PRIVATE GL_SILENCE_DEPRECATION)
    endif()
    
    # Copy to modules directory for use in swiftmodule
    add_custom_command(TARGET opengl_module POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:opengl_module> ${CMAKE_BINARY_DIR}/modules/
    )
endif()

# Math module is already defined above, removed duplicate

# Syntax test runner - commented out as file doesn't exist
# add_executable(syntax_runner tests/syntax_runner.c)
# target_link_libraries(syntax_runner lang_lib)
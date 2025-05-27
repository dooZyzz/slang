# TestFramework.cmake - CMake functions for the test framework

# Function to create a test library that can be either static or shared
function(add_test_suite NAME SOURCE_FILE)
    set(options STANDALONE SHARED)
    set(oneValueArgs "")
    set(multiValueArgs DEPENDENCIES)
    cmake_parse_arguments(TEST "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    
    # Library name
    set(LIB_NAME "test_${NAME}_lib")
    
    # Create library (shared if requested)
    if(TEST_SHARED)
        add_library(${LIB_NAME} SHARED ${SOURCE_FILE})
        target_compile_definitions(${LIB_NAME} PRIVATE BUILDING_TEST_LIB)
    else()
        add_library(${LIB_NAME} STATIC ${SOURCE_FILE})
    endif()
    
    # Link dependencies
    target_link_libraries(${LIB_NAME} PRIVATE lang_lib ${TEST_DEPENDENCIES})
    
    # Create standalone executable if requested
    if(TEST_STANDALONE)
        set(EXE_NAME "test_${NAME}")
        add_executable(${EXE_NAME} ${SOURCE_FILE})
        target_link_libraries(${EXE_NAME} PRIVATE lang_lib ${TEST_DEPENDENCIES})
        target_compile_definitions(${EXE_NAME} PRIVATE BUILD_STANDALONE)
        
        # Add to CTest
        add_test(NAME ${NAME} COMMAND ${EXE_NAME})
    endif()
endfunction()

# Function to create the main test runner
function(create_test_runner)
    set(options "")
    set(oneValueArgs OUTPUT_NAME)
    set(multiValueArgs TEST_SUITES)
    cmake_parse_arguments(RUNNER "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    
    if(NOT RUNNER_OUTPUT_NAME)
        set(RUNNER_OUTPUT_NAME "test_runner")
    endif()
    
    # Create the main test runner
    add_executable(${RUNNER_OUTPUT_NAME} tests/test_main.c)
    
    # Link all test suite libraries
    foreach(suite ${RUNNER_TEST_SUITES})
        target_link_libraries(${RUNNER_OUTPUT_NAME} PRIVATE test_${suite}_lib)
    endforeach()
    
    target_link_libraries(${RUNNER_OUTPUT_NAME} PRIVATE lang_lib)
    
    # Add comprehensive test
    add_test(NAME AllTests COMMAND ${RUNNER_OUTPUT_NAME})
    
    # Add individual suite tests
    foreach(suite ${RUNNER_TEST_SUITES})
        add_test(NAME ${suite}_suite COMMAND ${RUNNER_OUTPUT_NAME} ${suite})
    endforeach()
endfunction()

# Function to build tests both as libraries and executables
function(add_dual_test_suite NAME SOURCE_FILE)
    # Build as library for the main runner
    add_test_suite(${NAME} ${SOURCE_FILE} ${ARGN})
    
    # Also build as standalone executable with different target name
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
    add_executable(${NAME}_standalone ${SOURCE_FILE} ${ARGN})
    target_link_libraries(${NAME}_standalone lang_lib)
    target_compile_definitions(${NAME}_standalone PRIVATE TEST_STANDALONE)
    
    # Add test that runs the standalone executable
    add_test(NAME ${NAME}_standalone COMMAND ${NAME}_standalone)
endfunction()
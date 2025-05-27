#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <signal.h>
#include "runtime/module.h"
#include "vm/vm.h"
#include "vm/object.h"
#include "vm/array.h"

// Native function: getEnv(name: String) -> String?
static TaggedValue native_get_env(int arg_count, TaggedValue* args) {
    if (arg_count != 1 || !IS_STRING(args[0])) {
        return NIL_VAL;
    }
    
    const char* name = AS_STRING(args[0]);
    const char* value = getenv(name);
    
    if (value == NULL) {
        return NIL_VAL;
    }
    
    return STRING_VAL(strdup(value));
}

// Native function: setEnv(name: String, value: String) -> Bool
static TaggedValue native_set_env(int arg_count, TaggedValue* args) {
    if (arg_count != 2 || !IS_STRING(args[0]) || !IS_STRING(args[1])) {
        return BOOL_VAL(false);
    }
    
    const char* name = AS_STRING(args[0]);
    const char* value = AS_STRING(args[1]);
    
    return BOOL_VAL(setenv(name, value, 1) == 0);
}

// Native function: exec(command: String) -> Int
static TaggedValue native_exec(int arg_count, TaggedValue* args) {
    if (arg_count != 1 || !IS_STRING(args[0])) {
        return NUMBER_VAL(-1);
    }
    
    const char* command = AS_STRING(args[0]);
    int result = system(command);
    
    // system() returns the exit status in a special format
    if (WIFEXITED(result)) {
        return NUMBER_VAL(WEXITSTATUS(result));
    }
    
    return NUMBER_VAL(-1);
}

// Native function: sleep(seconds: Double)
static TaggedValue native_sleep(int arg_count, TaggedValue* args) {
    if (arg_count != 1 || !IS_NUMBER(args[0])) {
        return NIL_VAL;
    }
    
    double seconds = AS_NUMBER(args[0]);
    if (seconds < 0) seconds = 0;
    
    struct timespec ts;
    ts.tv_sec = (time_t)seconds;
    ts.tv_nsec = (long)((seconds - ts.tv_sec) * 1e9);
    
    nanosleep(&ts, NULL);
    return NIL_VAL;
}

// Native function: time() -> Double
static TaggedValue native_time(int arg_count, TaggedValue* args) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    double time = ts.tv_sec + ts.tv_nsec / 1e9;
    return NUMBER_VAL(time);
}

// Native function: clock() -> Double
static TaggedValue native_clock(int arg_count, TaggedValue* args) {
    return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

// Native function: exit(code: Int)
static TaggedValue native_exit(int arg_count, TaggedValue* args) {
    int code = 0;
    if (arg_count > 0 && IS_NUMBER(args[0])) {
        code = (int)AS_NUMBER(args[0]);
    }
    exit(code);
    return NIL_VAL; // Never reached
}

// Native function: getpid() -> Int
static TaggedValue native_getpid(int arg_count, TaggedValue* args) {
    return NUMBER_VAL(getpid());
}

// Native function: getArgs() -> Array
static TaggedValue native_get_args(int arg_count, TaggedValue* args) {
    // This would need to be set during VM initialization
    // For now, return empty array
    Object* array = array_create_with_capacity(0);
    return OBJECT_VAL(array);
}

// Native function: platform() -> String
static TaggedValue native_platform(int arg_count, TaggedValue* args) {
    #ifdef __APPLE__
        return STRING_VAL("darwin");
    #elif __linux__
        return STRING_VAL("linux");
    #elif _WIN32
        return STRING_VAL("windows");
    #else
        return STRING_VAL("unknown");
    #endif
}

// Native function: arch() -> String
static TaggedValue native_arch(int arg_count, TaggedValue* args) {
    #if defined(__x86_64__) || defined(_M_X64)
        return STRING_VAL("x86_64");
    #elif defined(__aarch64__) || defined(_M_ARM64)
        return STRING_VAL("arm64");
    #elif defined(__i386__) || defined(_M_IX86)
        return STRING_VAL("x86");
    #elif defined(__arm__)
        return STRING_VAL("arm");
    #else
        return STRING_VAL("unknown");
    #endif
}

// Native function: hostname() -> String
static TaggedValue native_hostname(int arg_count, TaggedValue* args) {
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        return STRING_VAL("localhost");
    }
    hostname[255] = '\0';
    return STRING_VAL(strdup(hostname));
}

// Module initialization
bool swiftlang_module_init(Module* module) {
    // Environment
    module_register_native_function(module, "getEnv", native_get_env);
    module_register_native_function(module, "setEnv", native_set_env);
    
    // Process
    module_register_native_function(module, "exec", native_exec);
    module_register_native_function(module, "exit", native_exit);
    module_register_native_function(module, "getpid", native_getpid);
    module_register_native_function(module, "getArgs", native_get_args);
    
    // Time
    module_register_native_function(module, "sleep", native_sleep);
    module_register_native_function(module, "time", native_time);
    module_register_native_function(module, "clock", native_clock);
    
    // System info
    module_register_native_function(module, "platform", native_platform);
    module_register_native_function(module, "arch", native_arch);
    module_register_native_function(module, "hostname", native_hostname);
    
    return true;
}
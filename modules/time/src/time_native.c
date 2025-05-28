#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>  // For gettimeofday on macOS
#include "vm/vm.h"
#include "vm/object.h"
#include "runtime/module.h"

// Native function to get current timestamp
static TaggedValue native_time_now(int arg_count, TaggedValue* args) {
    (void)arg_count;
    (void)args;
    
    time_t current_time = time(NULL);
    return NUMBER_VAL((double)current_time);
}

// Native function to format timestamp
static TaggedValue native_time_format(int arg_count, TaggedValue* args) {
    if (arg_count < 1 || !IS_NUMBER(args[0])) {
        return STRING_VAL(strdup("Invalid timestamp"));
    }
    
    time_t timestamp = (time_t)AS_NUMBER(args[0]);
    char buffer[256];
    
    struct tm* timeinfo = localtime(&timestamp);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
    
    return STRING_VAL(strdup(buffer));
}

// Native function to get milliseconds
static TaggedValue native_time_millis(int arg_count, TaggedValue* args) {
    (void)arg_count;
    (void)args;
    
#ifdef __APPLE__
    // macOS doesn't have clock_gettime in older versions
    struct timeval tv;
    gettimeofday(&tv, NULL);
    double millis = tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    double millis = ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
#endif
    
    return NUMBER_VAL(millis);
}

// Native function to sleep for milliseconds
static TaggedValue native_time_sleep(int arg_count, TaggedValue* args) {
    if (arg_count < 1 || !IS_NUMBER(args[0])) {
        return BOOL_VAL(false);
    }
    
    int millis = (int)AS_NUMBER(args[0]);
    if (millis < 0) millis = 0;
    
    struct timespec ts;
    ts.tv_sec = millis / 1000;
    ts.tv_nsec = (millis % 1000) * 1000000;
    
    nanosleep(&ts, NULL);
    return BOOL_VAL(true);
}

// Module initialization function
void time_module_init(VM* vm) {
    // Create module object
    Object* module = object_create();
    
    // Add native functions
    object_set_property(module, "now", NATIVE_VAL(native_time_now));
    object_set_property(module, "format", NATIVE_VAL(native_time_format));
    object_set_property(module, "millis", NATIVE_VAL(native_time_millis));
    object_set_property(module, "sleep", NATIVE_VAL(native_time_sleep));
    
    // Register the module
    define_global(vm, "time_native", OBJECT_VAL(module));
}

// Module initialization function
bool swiftlang_time_module_init(Module* module) {
    // Export native functions
    module_export(module, "native_time_now", NATIVE_VAL(native_time_now));
    module_export(module, "native_time_format", NATIVE_VAL(native_time_format));
    module_export(module, "native_time_millis", NATIVE_VAL(native_time_millis));
    module_export(module, "native_time_sleep", NATIVE_VAL(native_time_sleep));
    
    return true;
}
#ifndef LANG_COROUTINE_H
#define LANG_COROUTINE_H

#include "runtime/core/vm.h"
#include <stddef.h>
#include <stdbool.h>

typedef enum {
    COROUTINE_SUSPENDED,
    COROUTINE_RUNNING,
    COROUTINE_COMPLETED,
    COROUTINE_FAILED
} CoroutineState;

typedef struct Coroutine Coroutine;
typedef struct Promise Promise;
typedef struct Executor Executor;

typedef TaggedValue (*CoroutineFunc)(Coroutine* coro, TaggedValue* args, size_t arg_count);

struct Coroutine {
    CoroutineState state;
    CoroutineFunc function;
    VM* vm;
    
    TaggedValue* stack;
    size_t stack_size;
    size_t stack_capacity;
    
    TaggedValue* locals;
    size_t local_count;
    
    uint8_t* code;
    size_t pc;
    size_t code_size;
    
    TaggedValue result;
    TaggedValue error;
    
    Promise* promise;
    Coroutine* awaiting;
    Coroutine** dependents;
    size_t dependent_count;
    size_t dependent_capacity;
    
    void* user_data;
};

struct Promise {
    CoroutineState state;
    TaggedValue result;
    TaggedValue error;
    
    Coroutine** waiting;
    size_t waiting_count;
    size_t waiting_capacity;
};

struct Executor {
    Coroutine** ready_queue;
    size_t ready_count;
    size_t ready_capacity;
    
    Coroutine** suspended;
    size_t suspended_count;
    size_t suspended_capacity;
    
    bool running;
    size_t tick_count;
};

Coroutine* coroutine_create(CoroutineFunc function, VM* vm);
void coroutine_destroy(Coroutine* coro);

bool coroutine_start(Coroutine* coro, TaggedValue* args, size_t arg_count);
bool coroutine_resume(Coroutine* coro);
TaggedValue coroutine_await(Coroutine* coro, Promise* promise);

void coroutine_yield(Coroutine* coro, TaggedValue value);
void coroutine_complete(Coroutine* coro, TaggedValue result);
void coroutine_fail(Coroutine* coro, TaggedValue error);

Promise* promise_create(void);
void promise_destroy(Promise* promise);
void promise_resolve(Promise* promise, TaggedValue result);
void promise_reject(Promise* promise, TaggedValue error);
bool promise_is_resolved(const Promise* promise);

Executor* executor_create(void);
void executor_destroy(Executor* executor);

void executor_schedule(Executor* executor, Coroutine* coro);
void executor_run(Executor* executor);
void executor_run_until_complete(Executor* executor, Coroutine* main_coro);

bool executor_has_work(const Executor* executor);
void executor_tick(Executor* executor);

TaggedValue async_function_call(VM* vm, const char* name, TaggedValue* args, size_t arg_count);
Promise* async_function_call_promise(VM* vm, const char* name, TaggedValue* args, size_t arg_count);

#define AWAIT(coro, promise) coroutine_await(coro, promise)
#define YIELD(coro, value) coroutine_yield(coro, value)

#endif
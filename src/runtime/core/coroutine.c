#include "runtime/core/coroutine.h"
#include <stdlib.h>
#include <string.h>

#define STACK_INITIAL_SIZE 256
#define DEPENDENT_INITIAL_SIZE 4
#define WAITING_INITIAL_SIZE 4
#define READY_QUEUE_INITIAL_SIZE 16

Coroutine* coroutine_create(CoroutineFunc function, VM* vm) {
    Coroutine* coro = calloc(1, sizeof(Coroutine));
    if (!coro) return NULL;
    
    coro->state = COROUTINE_SUSPENDED;
    coro->function = function;
    coro->vm = vm;
    
    coro->stack_capacity = STACK_INITIAL_SIZE;
    coro->stack = calloc(coro->stack_capacity, sizeof(TaggedValue));
    
    coro->dependent_capacity = DEPENDENT_INITIAL_SIZE;
    coro->dependents = calloc(coro->dependent_capacity, sizeof(Coroutine*));
    
    coro->promise = promise_create();
    
    return coro;
}

void coroutine_destroy(Coroutine* coro) {
    if (!coro) return;
    
    free(coro->stack);
    free(coro->locals);
    free(coro->code);
    free(coro->dependents);
    promise_destroy(coro->promise);
    free(coro);
}

static void ensure_stack_capacity(Coroutine* coro, size_t needed) {
    while (coro->stack_size + needed > coro->stack_capacity) {
        coro->stack_capacity *= 2;
        coro->stack = realloc(coro->stack, coro->stack_capacity * sizeof(TaggedValue));
    }
}


bool coroutine_start(Coroutine* coro, TaggedValue* args, size_t arg_count) {
    if (!coro || coro->state != COROUTINE_SUSPENDED) return false;
    
    if (args && arg_count > 0) {
        ensure_stack_capacity(coro, arg_count);
        memcpy(coro->stack, args, arg_count * sizeof(TaggedValue));
        coro->stack_size = arg_count;
    }
    
    coro->state = COROUTINE_RUNNING;
    
    if (coro->function) {
        coro->result = coro->function(coro, args, arg_count);
        if (coro->state == COROUTINE_RUNNING) {
            coroutine_complete(coro, coro->result);
        }
    }
    
    return true;
}

bool coroutine_resume(Coroutine* coro) {
    if (!coro || coro->state != COROUTINE_SUSPENDED) return false;
    
    coro->state = COROUTINE_RUNNING;
    
    if (coro->function) {
        coro->result = coro->function(coro, NULL, 0);
        if (coro->state == COROUTINE_RUNNING) {
            coroutine_complete(coro, coro->result);
        }
    }
    
    return true;
}

TaggedValue coroutine_await(Coroutine* coro, Promise* promise) {
    if (!coro || !promise) {
        return (TaggedValue){.type = VAL_NIL};
    }
    
    if (promise_is_resolved(promise)) {
        return promise->result;
    }
    
    coro->state = COROUTINE_SUSPENDED;
    coro->awaiting = NULL;
    
    if (promise->waiting_count >= promise->waiting_capacity) {
        promise->waiting_capacity = promise->waiting_capacity ? promise->waiting_capacity * 2 : WAITING_INITIAL_SIZE;
        promise->waiting = realloc(promise->waiting, promise->waiting_capacity * sizeof(Coroutine*));
    }
    
    promise->waiting[promise->waiting_count++] = coro;
    
    return (TaggedValue){.type = VAL_NIL};
}

void coroutine_yield(Coroutine* coro, TaggedValue value) {
    if (!coro) return;
    
    coro->state = COROUTINE_SUSPENDED;
    coro->result = value;
}

void coroutine_complete(Coroutine* coro, TaggedValue result) {
    if (!coro) return;
    
    coro->state = COROUTINE_COMPLETED;
    coro->result = result;
    
    if (coro->promise) {
        promise_resolve(coro->promise, result);
    }
    
    for (size_t i = 0; i < coro->dependent_count; i++) {
        Coroutine* dependent = coro->dependents[i];
        if (dependent && dependent->state == COROUTINE_SUSPENDED) {
            dependent->state = COROUTINE_SUSPENDED;
        }
    }
}

void coroutine_fail(Coroutine* coro, TaggedValue error) {
    if (!coro) return;
    
    coro->state = COROUTINE_FAILED;
    coro->error = error;
    
    if (coro->promise) {
        promise_reject(coro->promise, error);
    }
    
    for (size_t i = 0; i < coro->dependent_count; i++) {
        Coroutine* dependent = coro->dependents[i];
        if (dependent && dependent->state == COROUTINE_SUSPENDED) {
            coroutine_fail(dependent, error);
        }
    }
}

Promise* promise_create(void) {
    Promise* promise = calloc(1, sizeof(Promise));
    if (!promise) return NULL;
    
    promise->state = COROUTINE_SUSPENDED;
    promise->waiting_capacity = WAITING_INITIAL_SIZE;
    promise->waiting = calloc(promise->waiting_capacity, sizeof(Coroutine*));
    
    return promise;
}

void promise_destroy(Promise* promise) {
    if (!promise) return;
    free(promise->waiting);
    free(promise);
}

void promise_resolve(Promise* promise, TaggedValue result) {
    if (!promise || promise->state != COROUTINE_SUSPENDED) return;
    
    promise->state = COROUTINE_COMPLETED;
    promise->result = result;
    
    for (size_t i = 0; i < promise->waiting_count; i++) {
        Coroutine* coro = promise->waiting[i];
        if (coro && coro->state == COROUTINE_SUSPENDED) {
            coro->state = COROUTINE_SUSPENDED;
        }
    }
}

void promise_reject(Promise* promise, TaggedValue error) {
    if (!promise || promise->state != COROUTINE_SUSPENDED) return;
    
    promise->state = COROUTINE_FAILED;
    promise->error = error;
    
    for (size_t i = 0; i < promise->waiting_count; i++) {
        Coroutine* coro = promise->waiting[i];
        if (coro && coro->state == COROUTINE_SUSPENDED) {
            coroutine_fail(coro, error);
        }
    }
}

bool promise_is_resolved(const Promise* promise) {
    return promise && (promise->state == COROUTINE_COMPLETED || promise->state == COROUTINE_FAILED);
}

Executor* executor_create(void) {
    Executor* executor = calloc(1, sizeof(Executor));
    if (!executor) return NULL;
    
    executor->ready_capacity = READY_QUEUE_INITIAL_SIZE;
    executor->ready_queue = calloc(executor->ready_capacity, sizeof(Coroutine*));
    
    executor->suspended_capacity = READY_QUEUE_INITIAL_SIZE;
    executor->suspended = calloc(executor->suspended_capacity, sizeof(Coroutine*));
    
    return executor;
}

void executor_destroy(Executor* executor) {
    if (!executor) return;
    
    free(executor->ready_queue);
    free(executor->suspended);
    free(executor);
}

void executor_schedule(Executor* executor, Coroutine* coro) {
    if (!executor || !coro) return;
    
    if (executor->ready_count >= executor->ready_capacity) {
        executor->ready_capacity *= 2;
        executor->ready_queue = realloc(executor->ready_queue, 
                                       executor->ready_capacity * sizeof(Coroutine*));
    }
    
    executor->ready_queue[executor->ready_count++] = coro;
}

static void move_to_suspended(Executor* executor, Coroutine* coro) {
    if (executor->suspended_count >= executor->suspended_capacity) {
        executor->suspended_capacity *= 2;
        executor->suspended = realloc(executor->suspended,
                                     executor->suspended_capacity * sizeof(Coroutine*));
    }
    
    executor->suspended[executor->suspended_count++] = coro;
}

void executor_tick(Executor* executor) {
    if (!executor || executor->ready_count == 0) return;
    
    Coroutine* coro = executor->ready_queue[0];
    
    memmove(executor->ready_queue, executor->ready_queue + 1,
            (executor->ready_count - 1) * sizeof(Coroutine*));
    executor->ready_count--;
    
    if (coro->state == COROUTINE_SUSPENDED) {
        if (coroutine_resume(coro)) {
            switch (coro->state) {
                case COROUTINE_SUSPENDED:
                    move_to_suspended(executor, coro);
                    break;
                case COROUTINE_COMPLETED:
                case COROUTINE_FAILED:
                    break;
                case COROUTINE_RUNNING:
                    executor_schedule(executor, coro);
                    break;
            }
        }
    }
    
    executor->tick_count++;
}

void executor_run(Executor* executor) {
    if (!executor) return;
    
    executor->running = true;
    
    while (executor->running && executor_has_work(executor)) {
        executor_tick(executor);
    }
    
    executor->running = false;
}

void executor_run_until_complete(Executor* executor, Coroutine* main_coro) {
    if (!executor || !main_coro) return;
    
    executor_schedule(executor, main_coro);
    
    while (executor_has_work(executor) && 
           main_coro->state != COROUTINE_COMPLETED && 
           main_coro->state != COROUTINE_FAILED) {
        executor_tick(executor);
    }
}

bool executor_has_work(const Executor* executor) {
    return executor && executor->ready_count > 0;
}

TaggedValue async_function_call(VM* vm, const char* name, TaggedValue* args, size_t arg_count) {
    (void)name;  // Unused parameter
    Coroutine* coro = coroutine_create(NULL, vm);
    if (!coro) return (TaggedValue){.type = VAL_NIL};
    
    if (!coroutine_start(coro, args, arg_count)) {
        coroutine_destroy(coro);
        return (TaggedValue){.type = VAL_NIL};
    }
    
    TaggedValue result = coro->result;
    coroutine_destroy(coro);
    return result;
}

Promise* async_function_call_promise(VM* vm, const char* name, TaggedValue* args, size_t arg_count) {
    (void)name;  // Unused parameter
    Coroutine* coro = coroutine_create(NULL, vm);
    if (!coro) return NULL;
    
    Promise* promise = coro->promise;
    coro->promise = NULL;
    
    if (!coroutine_start(coro, args, arg_count)) {
        coroutine_destroy(coro);
        promise_destroy(promise);
        return NULL;
    }
    
    coroutine_destroy(coro);
    return promise;
}
#include "vm/vm_new.h"
#include "runtime/module_new.h"
#include <stdio.h>
#include <string.h>

// Helper macros
#define READ_BYTE() (*frame->ip++)
#define READ_SHORT() (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_CONSTANT() (frame->function->chunk.constants.values[READ_BYTE()])
#define READ_STRING() AS_STRING(READ_CONSTANT())

// Updated interpreter that uses module context
static InterpretResult run_new(VM_New* vm) {
    CallFrame* frame = &vm->frames[vm->frame_count - 1];
    
    for (;;) {
        #ifdef DEBUG_TRACE_EXECUTION
        // Debug trace would go here
        #endif
        
        uint8_t instruction = READ_BYTE();
        
        switch (instruction) {
            // Constants and literals (unchanged)
            case OP_CONSTANT: {
                TaggedValue constant = READ_CONSTANT();
                vm_push((VM*)vm, constant);
                break;
            }
            
            case OP_NIL:   vm_push((VM*)vm, NIL_VAL); break;
            case OP_TRUE:  vm_push((VM*)vm, BOOL_VAL(true)); break;
            case OP_FALSE: vm_push((VM*)vm, BOOL_VAL(false)); break;
            
            // Stack operations (unchanged)
            case OP_POP: vm_pop((VM*)vm); break;
            
            // Arithmetic (unchanged)
            case OP_ADD:
            case OP_SUBTRACT:
            case OP_MULTIPLY:
            case OP_DIVIDE:
                // Implementation unchanged
                break;
            
            // Local variables (unchanged)
            case OP_GET_LOCAL: {
                uint8_t slot = READ_BYTE();
                vm_push((VM*)vm, frame->slots[slot]);
                break;
            }
            
            case OP_SET_LOCAL: {
                uint8_t slot = READ_BYTE();
                frame->slots[slot] = peek((VM*)vm, 0);
                break;
            }
            
            // MODULE VARIABLE ACCESS - This is the key change
            case OP_GET_GLOBAL: {  // Should be renamed to OP_GET_MODULE_VAR
                char* name = READ_STRING();
                
                // Resolve in current module context
                TaggedValue* value = module_resolve(vm->current_module, name);
                
                if (!value) {
                    runtime_error((VM*)vm, "Undefined variable '%s'", name);
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                vm_push((VM*)vm, *value);
                break;
            }
            
            case OP_SET_GLOBAL: {  // Should be renamed to OP_SET_MODULE_VAR
                char* name = READ_STRING();
                TaggedValue value = peek((VM*)vm, 0);
                
                // First try to set existing variable
                TaggedValue* existing = module_get(vm->current_module, name);
                if (existing) {
                    // Check if it's const
                    // TODO: Need to check const flag
                    *existing = value;
                } else {
                    runtime_error((VM*)vm, "Undefined variable '%s'", name);
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            
            case OP_DEFINE_GLOBAL: {  // Should be renamed to OP_DEFINE_MODULE_VAR
                char* name = READ_STRING();
                TaggedValue value = peek((VM*)vm, 0);
                
                // Define in current module
                // TODO: Parse const/export flags from instruction
                uint8_t flags = VISIBILITY_PRIVATE;  // Default to private
                
                if (!module_define(vm->current_module, name, value, flags)) {
                    runtime_error((VM*)vm, "Variable '%s' already defined", name);
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                vm_pop((VM*)vm);
                break;
            }
            
            // IMPORT OPERATIONS - New opcodes
            case OP_IMPORT_MODULE: {  // import module_name
                char* module_name = READ_STRING();
                
                Module* imported = module_loader_resolve(vm->current_module->loader, module_name);
                if (!imported) {
                    runtime_error((VM*)vm, "Cannot find module '%s'", module_name);
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                // Ensure imported module is initialized
                if (!imported->initialized) {
                    Module* saved = vm->current_module;
                    vm->current_module = imported;
                    
                    // Execute the imported module
                    if (imported->bytecode) {
                        // TODO: Execute module bytecode
                    }
                    
                    vm->current_module = saved;
                    imported->initialized = true;
                }
                
                // Import all public members
                if (!module_import_all(vm->current_module, imported)) {
                    runtime_error((VM*)vm, "Failed to import module '%s'", module_name);
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            
            case OP_MODULE_EXPORT: {  // export name
                // This should be handled at compile time
                // At runtime, we just need to mark the variable as exported
                char* name = READ_STRING();
                
                // TODO: Update the variable's visibility flags
                // For now, this is a no-op
                break;
            }
            
            // FUNCTION CALLS - Updated to use module context
            case OP_CALL: {
                uint8_t arg_count = READ_BYTE();
                TaggedValue callee = peek((VM*)vm, arg_count);
                
                if (IS_FUNCTION(callee)) {
                    Function* function = AS_FUNCTION(callee);
                    
                    if (arg_count != function->arity) {
                        runtime_error((VM*)vm, "Expected %d arguments but got %d",
                                    function->arity, arg_count);
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    
                    if (vm->frame_count == FRAMES_MAX) {
                        runtime_error((VM*)vm, "Stack overflow");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    
                    // Set up new call frame
                    CallFrame* new_frame = &vm->frames[vm->frame_count++];
                    new_frame->function = function;
                    new_frame->ip = function->chunk.code;
                    new_frame->slots = vm->stack_top - arg_count - 1;
                    
                    // KEY CHANGE: Functions execute in their defining module's context
                    new_frame->saved_module = vm->current_module;
                    if (function->module) {
                        vm->current_module = function->module;
                    }
                    
                    // Update frame pointer
                    frame = new_frame;
                }
                else if (IS_NATIVE(callee)) {
                    // Native function call (unchanged)
                    NativeFn native = AS_NATIVE(callee);
                    TaggedValue* args = vm->stack_top - arg_count;
                    TaggedValue result = native(arg_count, args);
                    
                    vm->stack_top -= arg_count + 1;
                    vm_push((VM*)vm, result);
                }
                else {
                    runtime_error((VM*)vm, "Can only call functions");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            
            case OP_RETURN: {
                TaggedValue result = vm_pop((VM*)vm);
                
                // Restore previous module context
                if (frame->saved_module) {
                    vm->current_module = frame->saved_module;
                }
                
                // Close upvalues
                close_upvalues((VM*)vm, frame->slots);
                
                vm->frame_count--;
                if (vm->frame_count == 0) {
                    vm_push((VM*)vm, result);
                    return INTERPRET_OK;
                }
                
                vm->stack_top = frame->slots;
                vm_push((VM*)vm, result);
                
                frame = &vm->frames[vm->frame_count - 1];
                break;
            }
            
            // Other opcodes...
            // TODO: Implement remaining opcodes
            
            default:
                runtime_error((VM*)vm, "Unknown opcode %d", instruction);
                return INTERPRET_RUNTIME_ERROR;
        }
    }
}

// Execute module with new interpreter
InterpretResult vm_new_interpret(VM_New* vm, Module* module) {
    if (!module || !module->bytecode) {
        return INTERPRET_COMPILE_ERROR;
    }
    
    // Create initial call frame
    CallFrame* frame = &vm->frames[vm->frame_count++];
    frame->function = NULL;  // Main chunk has no function
    frame->ip = module->bytecode->code;
    frame->slots = vm->stack;
    frame->saved_module = NULL;
    
    // Set current module
    vm->current_module = module;
    
    return run_new(vm);
}
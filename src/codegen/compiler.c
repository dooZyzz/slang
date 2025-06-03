#include "codegen/compiler.h"
#include "semantic/visitor.h"
#include "ast/ast.h"
#include "runtime/core/vm.h"
#include "runtime/modules/loader/module_loader.h"
#include "utils/allocators.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static Compiler* current = NULL;

// Forward declarations
static void emit_byte(uint8_t byte);
static void* compile_import_stmt(ASTVisitor* visitor, Stmt* stmt);
static void* compile_export_stmt(ASTVisitor* visitor, Stmt* stmt);
static void init_compiler(Compiler* compiler, CompilerFunctionType type);
static void free_compiler(Compiler* compiler);
static void init_loop(Loop* loop);
static void free_loop(Loop* loop);
static void add_break_jump(Loop* loop, int jump);

static void emit_byte(uint8_t byte) {
    chunk_write(current->current_chunk, byte, 1);
}

static void emit_bytes(uint8_t byte1, uint8_t byte2) {
    emit_byte(byte1);
    emit_byte(byte2);
}

static void emit_short(uint16_t value) {
    emit_byte((value >> 8) & 0xff);
    emit_byte(value & 0xff);
}

static int emit_jump(uint8_t instruction) {
    emit_byte(instruction);
    emit_short(0xffff);
    return current->current_chunk->count - 2;
}

static void patch_jump(int offset) {
    int jump = current->current_chunk->count - offset - 2;
    
    if (jump > UINT16_MAX) {
        fprintf(stderr, "Too much code to jump over.\n");
        return;
    }
    
    current->current_chunk->code[offset] = (jump >> 8) & 0xff;
    current->current_chunk->code[offset + 1] = jump & 0xff;
}

static void emit_loop(int loop_start) {
    emit_byte(OP_LOOP);
    
    int offset = current->current_chunk->count - loop_start + 2;
    if (offset > UINT16_MAX) {
        fprintf(stderr, "Loop body too large.\n");
        return;
    }
    
    emit_short(offset);
}

static void emit_constant(TaggedValue value) {
    if (!current || !current->current_chunk) {
        fprintf(stderr, "ERROR: emit_constant called with NULL current or current_chunk\n");
        return;
    }
    int constant = chunk_add_constant(current->current_chunk, value);
    if (constant < 256) {
        emit_bytes(OP_CONSTANT, constant);
    } else {
        emit_byte(OP_CONSTANT_LONG);
        emit_byte(constant & 0xff);
        emit_byte((constant >> 8) & 0xff);
        emit_byte((constant >> 16) & 0xff);
    }
}

static uint8_t make_constant(TaggedValue value) {
    int constant = chunk_add_constant(current->current_chunk, value);
    if (constant > UINT8_MAX) {
        // TODO: Error handling
        return 0;
    }
    return (uint8_t)constant;
}

static void begin_scope(void) {
    current->scope_depth++;
}

static void end_scope(void) {
    current->scope_depth--;
    
    // Pop locals that are going out of scope
    while (current->locals.count > 0 &&
           current->locals.depths[current->locals.count - 1] > current->scope_depth) {
        emit_byte(OP_POP);

        // Free the local name using compiler allocator
        if (current->locals.names[current->locals.count - 1]) {
            Allocator* alloc = allocators_get(ALLOC_SYSTEM_COMPILER);
            size_t len = strlen(current->locals.names[current->locals.count - 1]) + 1;
            SLANG_MEM_FREE(alloc, current->locals.names[current->locals.count - 1], len);
        }
        current->locals.count--;
    }
}

static void mark_initialized(void) {
    if (current->scope_depth == 0) return; // Globals are implicitly initialized
    current->locals.depths[current->locals.count - 1] = current->scope_depth;
}

static void add_local(Compiler* compiler, const char* name) {
    if (compiler->locals.count >= UINT8_MAX) {
        fprintf(stderr, "Too many local variables in function.\n");
        return;
    }
    
    // Grow arrays if needed
    if (compiler->locals.count >= compiler->locals.capacity) {
        Allocator* alloc = allocators_get(ALLOC_SYSTEM_COMPILER);
        size_t old_capacity = compiler->locals.capacity;
        compiler->locals.capacity = old_capacity < 8 ? 8 : old_capacity * 2;
        
        // Reallocate names array
        char** new_names = MEM_NEW_ARRAY(alloc, char*, compiler->locals.capacity);
        if (compiler->locals.names && old_capacity > 0) {
            memcpy(new_names, compiler->locals.names, sizeof(char*) * old_capacity);
            SLANG_MEM_FREE(alloc, compiler->locals.names, sizeof(char*) * old_capacity);
        }
        compiler->locals.names = new_names;
        
        // Reallocate depths array
        int* new_depths = MEM_NEW_ARRAY(alloc, int, compiler->locals.capacity);
        if (compiler->locals.depths && old_capacity > 0) {
            memcpy(new_depths, compiler->locals.depths, sizeof(int) * old_capacity);
            SLANG_MEM_FREE(alloc, compiler->locals.depths, sizeof(int) * old_capacity);
        }
        compiler->locals.depths = new_depths;
    }
    
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_BYTECODE);
    compiler->locals.names[compiler->locals.count] = MEM_STRDUP(alloc, name);
    compiler->locals.depths[compiler->locals.count] = -1; // Mark as uninitialized
    compiler->locals.count++;
}

static int resolve_local(Compiler* compiler, const char* name) {
    for (int i = compiler->locals.count - 1; i >= 0; i--) {
        if (strcmp(name, compiler->locals.names[i]) == 0) {
            if (compiler->locals.depths[i] == -1) {
                // Error: Can't read local variable in its own initializer
                return -1;
            }
            return i;
        }
    }
    return -1;
}

static int add_upvalue(Compiler* compiler, uint8_t index, bool is_local) {
    // Upvalues allow closures to capture variables from enclosing scopes
    // Each upvalue tracks either:
    // - A local variable from the immediately enclosing function (is_local = true)
    // - An upvalue from the enclosing function (is_local = false)
    
    int upvalue_count = compiler->function->upvalue_count;
    
    // Check if we already have this upvalue (avoid duplicates)
    for (int i = 0; i < upvalue_count; i++) {
        CompilerUpvalue* upvalue = &compiler->upvalues.values[i];
        if (upvalue->index == index && upvalue->is_local == is_local) {
            return i;
        }
    }
    
    // Add new upvalue
    if (upvalue_count >= UINT8_MAX) {
        return -1; // Too many closure variables
    }
    
    // Grow array if needed
    if (compiler->upvalues.count >= compiler->upvalues.capacity) {
        Allocator* alloc = allocators_get(ALLOC_SYSTEM_COMPILER);
        size_t old_capacity = compiler->upvalues.capacity;
        compiler->upvalues.capacity = old_capacity < 8 ? 8 : old_capacity * 2;
        
        CompilerUpvalue* new_values = MEM_NEW_ARRAY(alloc, CompilerUpvalue, compiler->upvalues.capacity);
        if (compiler->upvalues.values && old_capacity > 0) {
            memcpy(new_values, compiler->upvalues.values, sizeof(CompilerUpvalue) * old_capacity);
            SLANG_MEM_FREE(alloc, compiler->upvalues.values, sizeof(CompilerUpvalue) * old_capacity);
        }
        compiler->upvalues.values = new_values;
    }
    
    compiler->upvalues.values[upvalue_count].is_local = is_local;
    compiler->upvalues.values[upvalue_count].index = index;
    compiler->upvalues.count++;
    
    return compiler->function->upvalue_count++;
}

static int resolve_upvalue(Compiler* compiler, const char* name) {
    if (compiler->enclosing == NULL) return -1;
    
    // Check if it's a local in the enclosing function
    int local = resolve_local(compiler->enclosing, name);
    if (local != -1) {
        return add_upvalue(compiler, (uint8_t)local, true);
    }
    
    // Check if it's an upvalue in the enclosing function
    int upvalue = resolve_upvalue(compiler->enclosing, name);
    if (upvalue != -1) {
        return add_upvalue(compiler, (uint8_t)upvalue, false);
    }
    
    return -1;
}

// String creation helpers using string allocator
static TaggedValue create_string_value(const char* str) {
    // Use the string-specific allocator and macro
    char* vm_str = STR_DUP(str);
    return (TaggedValue){.type = VAL_STRING, .as.string = vm_str};
}

// Loop tracking with allocator
static void init_loop(Loop* loop) {
    loop->enclosing = current->inner_most_loop;
    loop->start = current->current_chunk->count;
    loop->scope_depth = current->scope_depth;
    loop->break_count = 0;
    loop->break_capacity = 4;
    
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_COMPILER);
    loop->break_jumps = MEM_NEW_ARRAY(alloc, int, loop->break_capacity);
}

static void free_loop(Loop* loop) {
    if (loop->break_jumps) {
        Allocator* alloc = allocators_get(ALLOC_SYSTEM_COMPILER);
        SLANG_MEM_FREE(alloc, loop->break_jumps, sizeof(int) * loop->break_capacity);
        loop->break_jumps = NULL;
    }
}

static void add_break_jump(Loop* loop, int jump) {
    if (loop->break_count >= loop->break_capacity) {
        Allocator* alloc = allocators_get(ALLOC_SYSTEM_COMPILER);
        size_t old_capacity = loop->break_capacity;
        loop->break_capacity *= 2;
        
        int* new_jumps = MEM_NEW_ARRAY(alloc, int, loop->break_capacity);
        memcpy(new_jumps, loop->break_jumps, sizeof(int) * old_capacity);
        SLANG_MEM_FREE(alloc, loop->break_jumps, sizeof(int) * old_capacity);
        loop->break_jumps = new_jumps;
    }
    
    loop->break_jumps[loop->break_count++] = jump;
}

// Compilation functions for expressions
static void* compile_literal_expr(ASTVisitor* visitor, Expr* expr) {
    (void)visitor;  // Unused parameter
    LiteralExpr* lit = &expr->literal;
    
    switch (lit->type) {
        case LITERAL_NIL:
            emit_byte(OP_NIL);
            break;
        case LITERAL_BOOL:
            emit_byte(lit->value.boolean ? OP_TRUE : OP_FALSE);
            break;
        case LITERAL_INT: {
            TaggedValue value = {.type = VAL_NUMBER, .as.number = (double)lit->value.integer};
            emit_constant(value);
            break;
        }
        case LITERAL_FLOAT: {
            TaggedValue value = {.type = VAL_NUMBER, .as.number = lit->value.floating};
            emit_constant(value);
            break;
        }
        case LITERAL_STRING: {
            TaggedValue value = create_string_value(lit->value.string.value);
            emit_constant(value);
            break;
        }
    }
    
    return NULL;
}

static void* compile_binary_expr(ASTVisitor* visitor, Expr* expr) {
    BinaryExpr* bin = &expr->binary;
    
    // Compile left operand
    ast_accept_expr(bin->left, visitor);
    
    // Compile right operand
    ast_accept_expr(bin->right, visitor);
    
    // Emit operation
    switch (bin->operator.type) {
        case TOKEN_PLUS:         emit_byte(OP_ADD); break;
        case TOKEN_MINUS:        emit_byte(OP_SUBTRACT); break;
        case TOKEN_STAR:         emit_byte(OP_MULTIPLY); break;
        case TOKEN_SLASH:        emit_byte(OP_DIVIDE); break;
        case TOKEN_PERCENT:      emit_byte(OP_MODULO); break;
        case TOKEN_EQUAL_EQUAL:  emit_byte(OP_EQUAL); break;
        case TOKEN_NOT_EQUAL:    emit_byte(OP_NOT_EQUAL); break;
        case TOKEN_GREATER:      emit_byte(OP_GREATER); break;
        case TOKEN_GREATER_EQUAL:emit_byte(OP_GREATER_EQUAL); break;
        case TOKEN_LESS:         emit_byte(OP_LESS); break;
        case TOKEN_LESS_EQUAL:   emit_byte(OP_LESS_EQUAL); break;
        case TOKEN_AMPERSAND:    emit_byte(OP_BIT_AND); break;
        case TOKEN_PIPE:         emit_byte(OP_BIT_OR); break;
        case TOKEN_CARET:        emit_byte(OP_BIT_XOR); break;
        case TOKEN_SHIFT_LEFT:   emit_byte(OP_SHIFT_LEFT); break;
        case TOKEN_SHIFT_RIGHT:  emit_byte(OP_SHIFT_RIGHT); break;
        case TOKEN_AND_AND:      emit_byte(OP_AND); break;
        case TOKEN_OR_OR:        emit_byte(OP_OR); break;
        default:
            fprintf(stderr, "Unknown binary operator\n");
            break;
    }
    
    return NULL;
}

static void* compile_unary_expr(ASTVisitor* visitor, Expr* expr) {
    UnaryExpr* unary = &expr->unary;
    
    // Compile operand
    ast_accept_expr(unary->operand, visitor);
    
    // Emit operation
    switch (unary->operator.type) {
        case TOKEN_MINUS:  emit_byte(OP_NEGATE); break;
        case TOKEN_NOT:    emit_byte(OP_NOT); break;
        case TOKEN_TILDE:  emit_byte(OP_BIT_NOT); break;
        case TOKEN_PLUS:   /* No-op */ break;
        default:
            fprintf(stderr, "Unknown unary operator\n");
            break;
    }
    
    return NULL;
}

static void* compile_variable_expr(ASTVisitor* visitor, Expr* expr) {
    (void)visitor;  // Unused parameter
    VariableExpr* var = &expr->variable;
    
    // Special handling for 'this' in extension methods
    if (strcmp(var->name, "this") == 0 && current->function && 
        strstr(current->function->name, "_ext_") != NULL) {
        // In extension methods, 'this' is always the first local (slot 1)
        // Slot 0 contains the function itself
        emit_bytes(OP_GET_LOCAL, 1);
        return NULL;
    }
    
    // Check if it's a local variable
    int local = resolve_local(current, var->name);
    if (local != -1) {
        emit_bytes(OP_GET_LOCAL, (uint8_t)local);
        return NULL;
    }
    
    // Check if it's an upvalue
    int upvalue = resolve_upvalue(current, var->name);
    if (upvalue != -1) {
        emit_bytes(OP_GET_UPVALUE, (uint8_t)upvalue);
        return NULL;
    }
    
    // Must be global
    int name_constant = chunk_add_constant(current->current_chunk, 
        create_string_value(var->name));
    emit_bytes(OP_GET_GLOBAL, name_constant);
    
    return NULL;
}

static void* compile_assignment_expr(ASTVisitor* visitor, Expr* expr) {
    AssignmentExpr* assign = &expr->assignment;
    
    if (assign->target->type == EXPR_VARIABLE) {
        // Variable assignment
        VariableExpr* var = &assign->target->variable;
        
        // Compile value
        ast_accept_expr(assign->value, visitor);
        
        // Check if it's a local variable
        int local = resolve_local(current, var->name);
        if (local != -1) {
            emit_bytes(OP_SET_LOCAL, (uint8_t)local);
        } else {
            // Check if it's an upvalue
            int upvalue = resolve_upvalue(current, var->name);
            if (upvalue != -1) {
                emit_bytes(OP_SET_UPVALUE, (uint8_t)upvalue);
            } else {
                // Must be global
                int name_constant = chunk_add_constant(current->current_chunk,
                    create_string_value(var->name));
                emit_bytes(OP_SET_GLOBAL, name_constant);
            }
        }
    } else if (assign->target->type == EXPR_SUBSCRIPT) {
        // Array subscript assignment: array[index] = value
        SubscriptExpr* subscript = &assign->target->subscript;
        
        // Compile array
        ast_accept_expr(subscript->object, visitor);
        
        // Compile index
        ast_accept_expr(subscript->index, visitor);
        
        // Compile value
        ast_accept_expr(assign->value, visitor);
        
        // Emit set subscript instruction
        emit_byte(OP_SET_SUBSCRIPT);
    } else if (assign->target->type == EXPR_MEMBER) {
        // Property assignment: object.property = value
        MemberExpr* member = &assign->target->member;
        
        // Compile object
        ast_accept_expr(member->object, visitor);
        
        // Push property name
        TaggedValue prop = create_string_value(member->property);
        emit_constant(prop);
        
        // Compile value
        ast_accept_expr(assign->value, visitor);
        
        // Emit set property instruction
        emit_byte(OP_SET_PROPERTY);
    } else {
        fprintf(stderr, "Invalid assignment target\n");
    }
    
    return NULL;
}

static void* compile_array_literal_expr(ASTVisitor* visitor, Expr* expr) {
    ArrayLiteralExpr* array = &expr->array_literal;
    
    // Compile all elements
    for (size_t i = 0; i < array->element_count; i++) {
        ast_accept_expr(array->elements[i], visitor);
    }
    
    // Emit array creation instruction
    emit_bytes(OP_ARRAY, array->element_count);
    
    return NULL;
}

static void* compile_object_literal_expr(ASTVisitor* visitor, Expr* expr) {
    ObjectLiteralExpr* obj = &expr->object_literal;
    
    // Create empty object
    emit_byte(OP_CREATE_OBJECT);
    
    // Add each key-value pair
    for (size_t i = 0; i < obj->pair_count; i++) {
        // Duplicate object on stack
        emit_byte(OP_DUP);
        
        // Push key
        TaggedValue key_val = create_string_value(obj->keys[i]);
        emit_constant(key_val);
        
        // Compile value
        ast_accept_expr(obj->values[i], visitor);
        
        // Set property
        emit_byte(OP_SET_PROPERTY);
        emit_byte(OP_POP); // Pop the result
    }
    
    return NULL;
}

static void* compile_closure_expr(ASTVisitor* visitor, Expr* expr) {
    ClosureExpr* closure = &expr->closure;
    
    // Create a new compiler for the closure
    Compiler* enclosing = current;  // Save current before init_compiler changes it
    Compiler closure_compiler;
    init_compiler(&closure_compiler, FUNC_TYPE_FUNCTION);
    closure_compiler.enclosing = enclosing;  // Use the saved enclosing
    closure_compiler.function->name = MEM_STRDUP(allocators_get(ALLOC_SYSTEM_BYTECODE), "<closure>");
    closure_compiler.function->arity = closure->parameter_count;
    
    // current was already set by init_compiler
    
    // Add parameters as local variables starting at slot 1
    for (size_t i = 0; i < closure->parameter_count; i++) {
        add_local(&closure_compiler, closure->parameter_names[i]);
        // Parameters are always initialized - set depth directly
        closure_compiler.locals.depths[closure_compiler.locals.count - 1] = 0;
    }
    
    // Check if this is a single expression that should have implicit return
    bool needs_implicit_return = false;
    if (closure->body->type == STMT_BLOCK) {
        BlockStmt* block = &closure->body->block;
        if (block->statement_count == 1 && 
            block->statements[0]->type == STMT_EXPRESSION) {
            // Single expression statement - we'll add implicit return
            needs_implicit_return = true;
            // Compile the expression
            ast_accept_expr(block->statements[0]->expression.expression, visitor);
        } else {
            // Multiple statements or non-expression - compile normally
            ast_accept_stmt(closure->body, visitor);
        }
    } else {
        // Not a block statement, compile normally
        ast_accept_stmt(closure->body, visitor);
    }
    
    // Emit return
    if (needs_implicit_return) {
        // Expression value is on stack, return it
        emit_byte(OP_RETURN);
    } else if (closure_compiler.function->chunk.count == 0 || 
               closure_compiler.function->chunk.code[closure_compiler.function->chunk.count - 1] != OP_RETURN) {
        // No explicit return, return nil
        emit_byte(OP_NIL);
        emit_byte(OP_RETURN);
    }
    
    // Switch back to the enclosing compiler
    current = enclosing;
    
    // Add the function as a constant
    TaggedValue func_val = {.type = VAL_FUNCTION, .as.function = closure_compiler.function};
    int constant = chunk_add_constant(current->current_chunk, func_val);
    
    // Emit closure creation instruction with the function constant
    if (constant < 256) {
        emit_bytes(OP_CLOSURE, constant);
    } else {
        // Use OP_CLOSURE_LONG for constants >= 256
        emit_byte(OP_CLOSURE_LONG);
        emit_byte((constant >> 16) & 0xff);
        emit_byte((constant >> 8) & 0xff);
        emit_byte(constant & 0xff);
    }
    
    // Emit upvalue information for the VM to create the closure
    for (int i = 0; i < closure_compiler.function->upvalue_count; i++) {
        emit_byte(closure_compiler.upvalues.values[i].is_local ? 1 : 0);
        emit_byte(closure_compiler.upvalues.values[i].index);
    }
    
    // Note: We don't free the closure_compiler because the function is still needed
    // Only free the compiler's internal structures
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_COMPILER);
    if (closure_compiler.locals.names) {
        for (int i = 0; i < closure_compiler.locals.count; i++) {
            if (closure_compiler.locals.names[i]) {
                size_t len = strlen(closure_compiler.locals.names[i]) + 1;
                SLANG_MEM_FREE(alloc, closure_compiler.locals.names[i], len);
            }
        }
        SLANG_MEM_FREE(alloc, closure_compiler.locals.names, sizeof(char*) * closure_compiler.locals.capacity);
    }
    if (closure_compiler.locals.depths) {
        SLANG_MEM_FREE(alloc, closure_compiler.locals.depths, sizeof(int) * closure_compiler.locals.capacity);
    }
    if (closure_compiler.upvalues.values) {
        SLANG_MEM_FREE(alloc, closure_compiler.upvalues.values, sizeof(CompilerUpvalue) * closure_compiler.upvalues.capacity);
    }
    
    return NULL;
}

static void* compile_call_expr(ASTVisitor* visitor, Expr* expr) {
    CallExpr* call = &expr->call;
    
    // Check if this is a method call (callee is a member expression)
    if (call->callee->type == EXPR_MEMBER) {
        MemberExpr* member = &call->callee->member;
        
        // Compile the object (this will be the first argument)
        ast_accept_expr(member->object, visitor);
        
        // Duplicate the object on the stack
        emit_byte(OP_DUP);
        
        // Push property name
        TaggedValue prop = create_string_value(member->property);
        emit_constant(prop);
        
        // Get the method
        emit_byte(OP_GET_PROPERTY);
        
        // Now we have: [object, method] on stack
        // We need: [method, object, ...args]
        // So we swap them
        emit_byte(OP_SWAP);
        
        // Compile remaining arguments
        for (size_t i = 0; i < call->argument_count; i++) {
            ast_accept_expr(call->arguments[i], visitor);
        }
        
        // Call as a method (the VM will handle binding 'self')
        emit_bytes(OP_METHOD_CALL, call->argument_count);
    } else {
        // Regular function call
        ast_accept_expr(call->callee, visitor);
        
        // Compile arguments
        for (size_t i = 0; i < call->argument_count; i++) {
            ast_accept_expr(call->arguments[i], visitor);
        }
        
        // Emit call instruction
        emit_bytes(OP_CALL, call->argument_count);
    }
    
    return NULL;
}

static void* compile_subscript_expr(ASTVisitor* visitor, Expr* expr) {
    SubscriptExpr* subscript = &expr->subscript;
    
    // Compile object
    ast_accept_expr(subscript->object, visitor);
    
    // Compile index
    ast_accept_expr(subscript->index, visitor);
    
    // Emit subscript get instruction
    emit_byte(OP_GET_SUBSCRIPT);
    
    return NULL;
}

static void* compile_member_expr(ASTVisitor* visitor, Expr* expr) {
    MemberExpr* member = &expr->member;
    
    // Compile object
    ast_accept_expr(member->object, visitor);
    
    // Push property name as constant
    TaggedValue prop = create_string_value(member->property);
    int prop_const = chunk_add_constant(current->current_chunk, prop);
    
    // Emit the constant
    if (prop_const < 256) {
        emit_bytes(OP_CONSTANT, prop_const);
    } else {
        emit_byte(OP_CONSTANT_LONG);
        emit_byte(prop_const & 0xff);
        emit_byte((prop_const >> 8) & 0xff);
    }
    
    // Emit property get instruction
    emit_byte(OP_GET_PROPERTY);
    
    return NULL;
}

static void* compile_ternary_expr(ASTVisitor* visitor, Expr* expr) {
    TernaryExpr* ternary = &expr->ternary;
    
    // Compile condition
    ast_accept_expr(ternary->condition, visitor);
    
    // Jump if false
    int then_jump = emit_jump(OP_JUMP_IF_FALSE);
    emit_byte(OP_POP);  // Pop condition
    
    // Compile then branch
    ast_accept_expr(ternary->then_branch, visitor);
    
    int end_jump = emit_jump(OP_JUMP);
    
    patch_jump(then_jump);
    emit_byte(OP_POP);  // Pop condition
    
    // Compile else branch
    ast_accept_expr(ternary->else_branch, visitor);
    
    patch_jump(end_jump);
    
    return NULL;
}

static void* compile_nil_coalescing_expr(ASTVisitor* visitor, Expr* expr) {
    NilCoalescingExpr* coalesce = &expr->nil_coalescing;
    
    // Compile left operand
    ast_accept_expr(coalesce->left, visitor);
    
    // Duplicate for nil check
    emit_byte(OP_DUP);
    emit_byte(OP_NIL);
    emit_byte(OP_EQUAL);
    
    // Jump if not nil
    int skip_jump = emit_jump(OP_JUMP_IF_FALSE);
    
    // Pop the left value and evaluate right
    emit_byte(OP_POP);
    ast_accept_expr(coalesce->right, visitor);
    
    patch_jump(skip_jump);
    
    return NULL;
}

static void* compile_optional_chaining_expr(ASTVisitor* visitor, Expr* expr) {
    OptionalChainingExpr* opt = &expr->optional_chaining;
    
    // Compile operand
    ast_accept_expr(opt->operand, visitor);
    
    // Emit optional chaining instruction
    emit_byte(OP_OPTIONAL_CHAIN);
    
    return NULL;
}

static void* compile_force_unwrap_expr(ASTVisitor* visitor, Expr* expr) {
    ForceUnwrapExpr* unwrap = &expr->force_unwrap;
    
    // Compile operand
    ast_accept_expr(unwrap->operand, visitor);
    
    // Emit force unwrap instruction
    emit_byte(OP_FORCE_UNWRAP);
    
    return NULL;
}

static void* compile_type_cast_expr(ASTVisitor* visitor, Expr* expr) {
    TypeCastExpr* cast = &expr->type_cast;
    
    // Compile expression
    ast_accept_expr(cast->expression, visitor);
    
    // For now, type casts are no-ops at runtime
    // In the future, we might emit runtime type checks
    
    return NULL;
}

static void* compile_await_expr(ASTVisitor* visitor, Expr* expr) {
    AwaitExpr* await = &expr->await;
    
    // Compile expression
    ast_accept_expr(await->expression, visitor);
    
    // Emit await instruction
    emit_byte(OP_AWAIT);
    
    return NULL;
}

static void* compile_string_interp_expr(ASTVisitor* visitor, Expr* expr) {
    StringInterpExpr* interp = &expr->string_interp;
    
    // Start with the first string part
    TaggedValue first_part = create_string_value(interp->parts[0]);
    emit_constant(first_part);
    
    // For each expression
    for (size_t i = 0; i < interp->expr_count; i++) {
        // Compile the expression
        ast_accept_expr(interp->expressions[i], visitor);
        
        // Convert to string if needed (VM will handle this)
        emit_byte(OP_TO_STRING);
        
        // Concatenate with previous result
        emit_byte(OP_ADD); // String addition = concatenation
        
        // Add the next string part
        if (i + 1 < interp->part_count) {
            TaggedValue next_part = create_string_value(interp->parts[i + 1]);
            emit_constant(next_part);
            emit_byte(OP_ADD);
        }
    }
    
    return NULL;
}

// Initialize compiler with allocator
void init_compiler(Compiler* compiler, CompilerFunctionType type) {
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_COMPILER);
    
    compiler->enclosing = current;
    compiler->type = type;
    compiler->scope_depth = 0;
    
    // Initialize locals
    compiler->locals.count = 0;
    compiler->locals.capacity = 8;
    compiler->locals.names = MEM_NEW_ARRAY(alloc, char*, compiler->locals.capacity);
    compiler->locals.depths = MEM_NEW_ARRAY(alloc, int, compiler->locals.capacity);
    
    // Initialize upvalues
    compiler->upvalues.count = 0;
    compiler->upvalues.capacity = 8;
    compiler->upvalues.values = MEM_NEW_ARRAY(alloc, CompilerUpvalue, compiler->upvalues.capacity);
    
    // Create function using VM allocator (functions are runtime objects)
    Allocator* vm_alloc = allocators_get(ALLOC_SYSTEM_VM);
    compiler->function = MEM_NEW(vm_alloc, Function);
    if (!compiler->function) {
        fprintf(stderr, "ERROR: Failed to allocate Function in init_compiler\n");
        exit(1);  // Fatal error - cannot continue without function
    }
    compiler->function->arity = 0;
    compiler->function->upvalue_count = 0;
    compiler->function->name = NULL;
    compiler->function->module = NULL;  // Initialize module field
    chunk_init(&compiler->function->chunk);
    
    // Initialize module fields
    compiler->current_module = NULL;
    compiler->is_module_compilation = false;
    compiler->inner_most_loop = NULL;
    
    // Set current_chunk if we have a function
    if (compiler->function) {
        compiler->current_chunk = &compiler->function->chunk;
    }
    
    current = compiler;
    
    if (type != FUNC_TYPE_SCRIPT) {
        // Reserve slot 0 for 'this' in methods or the function itself
        compiler->locals.names[compiler->locals.count] = MEM_STRDUP(alloc, "");
        compiler->locals.depths[compiler->locals.count] = 0;
        compiler->locals.count++;
    }
}

// Clean up compiler resources
void free_compiler(Compiler* compiler) {
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_COMPILER);
    
    // Free local names
    for (int i = 0; i < compiler->locals.count; i++) {
        if (compiler->locals.names[i]) {
            size_t len = strlen(compiler->locals.names[i]) + 1;
            SLANG_MEM_FREE(alloc, compiler->locals.names[i], len);
        }
    }
    
    // Free arrays
    if (compiler->locals.names) {
        SLANG_MEM_FREE(alloc, compiler->locals.names, sizeof(char*) * compiler->locals.capacity);
    }
    if (compiler->locals.depths) {
        SLANG_MEM_FREE(alloc, compiler->locals.depths, sizeof(int) * compiler->locals.capacity);
    }
    if (compiler->upvalues.values) {
        SLANG_MEM_FREE(alloc, compiler->upvalues.values, sizeof(CompilerUpvalue) * compiler->upvalues.capacity);
    }
    
    current = compiler->enclosing;
}

// Compilation functions for statements
static void* compile_expression_stmt(ASTVisitor* visitor, Stmt* stmt) {
    Compiler* compiler = (Compiler*)visitor->context;
    ast_accept_expr(stmt->expression.expression, visitor);
    // Don't pop if this is the last expression statement (for REPL/testing)
    if (!compiler->is_last_expr_stmt) {
        emit_byte(OP_POP);  // Discard the result
    }
    return NULL;
}

static void* compile_var_decl_stmt(ASTVisitor* visitor, Stmt* stmt) {
    VarDeclStmt* var_decl = &stmt->var_decl;
    
    // Compile initializer or push nil
    if (var_decl->initializer) {
        ast_accept_expr(var_decl->initializer, visitor);
    } else {
        emit_byte(OP_NIL);
    }
    
    // Check if we're in a local scope
    if (current->scope_depth > 0) {
        // Define as local variable
        add_local(current, var_decl->name);
        mark_initialized();
    } else {
        // Define global variable
        int name_constant = chunk_add_constant(current->current_chunk,
            create_string_value(var_decl->name));
        emit_bytes(OP_DEFINE_GLOBAL, name_constant);
    }
    
    return NULL;
}

static void* compile_block_stmt(ASTVisitor* visitor, Stmt* stmt) {
    BlockStmt* block = &stmt->block;
    
    begin_scope();
    
    // Compile all statements in the block
    for (size_t i = 0; i < block->statement_count; i++) {
        ast_accept_stmt(block->statements[i], visitor);
    }
    
    end_scope();
    
    return NULL;
}

static void* compile_if_stmt(ASTVisitor* visitor, Stmt* stmt) {
    IfStmt* if_stmt = &stmt->if_stmt;
    
    // Compile condition
    ast_accept_expr(if_stmt->condition, visitor);
    
    // Jump if false
    int then_jump = emit_jump(OP_JUMP_IF_FALSE);
    emit_byte(OP_POP);  // Pop condition
    
    // Compile then branch
    ast_accept_stmt(if_stmt->then_branch, visitor);
    
    int else_jump = emit_jump(OP_JUMP);
    
    patch_jump(then_jump);
    emit_byte(OP_POP);  // Pop condition
    
    // Compile else branch if present
    if (if_stmt->else_branch) {
        ast_accept_stmt(if_stmt->else_branch, visitor);
    }
    
    patch_jump(else_jump);
    
    return NULL;
}

static void* compile_while_stmt(ASTVisitor* visitor, Stmt* stmt) {
    WhileStmt* while_stmt = &stmt->while_stmt;
    
    // Create a new loop structure
    Loop loop;
    init_loop(&loop);
    
    int loop_start = current->current_chunk->count;
    
    // Compile condition
    ast_accept_expr(while_stmt->condition, visitor);
    
    // Jump out if false
    int exit_jump = emit_jump(OP_JUMP_IF_FALSE);
    emit_byte(OP_POP);  // Pop condition
    
    // Compile body
    ast_accept_stmt(while_stmt->body, visitor);
    
    // Loop back
    emit_loop(loop_start);
    
    patch_jump(exit_jump);
    emit_byte(OP_POP);  // Pop condition
    
    // Patch all break jumps
    for (size_t i = 0; i < loop.break_count; i++) {
        patch_jump(loop.break_jumps[i]);
    }
    
    // Clean up
    free_loop(&loop);
    current->inner_most_loop = loop.enclosing;
    
    return NULL;
}

static void* compile_for_in_stmt(ASTVisitor* visitor, Stmt* stmt) {
    ForInStmt* for_in = &stmt->for_in;
    
    // Create a new loop structure
    Loop loop;
    init_loop(&loop);
    
    // Compile iterable
    ast_accept_expr(for_in->iterable, visitor);
    
    // Create iterator
    emit_byte(OP_GET_ITER);
    
    // Add dummy locals for the iterator state
    add_local(current, "");  // array
    add_local(current, "");  // index
    
    // Loop start (for continue)
    int loop_start = current->current_chunk->count;
    loop.start = loop_start;
    
    // Try to get next value
    emit_byte(OP_FOR_ITER);
    int exit_jump = emit_jump(OP_JUMP_IF_FALSE);
    emit_byte(OP_POP);  // Pop the boolean
    
    // Create new scope for loop variable
    begin_scope();
    
    // The element is now on top of stack, store it as a local
    add_local(current, for_in->variable_name);
    mark_initialized();
    
    // Execute body
    ast_accept_stmt(for_in->body, visitor);
    
    // End scope (pops loop variable)
    end_scope();
    
    // Loop back
    emit_loop(loop_start);
    
    patch_jump(exit_jump);
    emit_byte(OP_POP);  // Pop boolean
    emit_byte(OP_POP);  // Pop index
    emit_byte(OP_POP);  // Pop array
    
    // Remove the dummy locals for iterator state
    current->locals.count -= 2;
    
    // Patch all break jumps
    for (size_t i = 0; i < loop.break_count; i++) {
        patch_jump(loop.break_jumps[i]);
    }
    
    // Clean up
    free_loop(&loop);
    current->inner_most_loop = loop.enclosing;
    
    return NULL;
}

static void* compile_for_stmt(ASTVisitor* visitor, Stmt* stmt) {
    ForStmt* for_stmt = &stmt->for_stmt;
    
    // Create a new loop structure
    Loop loop;
    init_loop(&loop);
    
    // New scope for the loop (to contain the initializer variable)
    begin_scope();
    
    // Compile initializer
    if (for_stmt->initializer) {
        ast_accept_stmt(for_stmt->initializer, visitor);
    }
    
    // Loop start (for continue statements)
    int loop_start = current->current_chunk->count;
    loop.start = loop_start;
    
    // Compile condition
    int exit_jump = -1;
    if (for_stmt->condition) {
        ast_accept_expr(for_stmt->condition, visitor);
        exit_jump = emit_jump(OP_JUMP_IF_FALSE);
        emit_byte(OP_POP);  // Pop condition
    }
    
    // Execute body
    ast_accept_stmt(for_stmt->body, visitor);
    
    // Continue target (increment expression)
    loop.start = current->current_chunk->count;
    
    // Compile increment
    if (for_stmt->increment) {
        ast_accept_expr(for_stmt->increment, visitor);
        emit_byte(OP_POP);  // Pop increment result
    }
    
    // Loop back to condition
    emit_loop(loop_start);
    
    // Patch exit jump
    if (exit_jump != -1) {
        patch_jump(exit_jump);
        emit_byte(OP_POP);  // Pop condition
    }
    
    // End scope (pops any locals from initializer)
    end_scope();
    
    // Patch all break jumps
    for (size_t i = 0; i < loop.break_count; i++) {
        patch_jump(loop.break_jumps[i]);
    }
    
    // Clean up
    free_loop(&loop);
    current->inner_most_loop = loop.enclosing;
    
    return NULL;
}

static void* compile_return_stmt(ASTVisitor* visitor, Stmt* stmt) {
    ReturnStmt* ret = &stmt->return_stmt;
    
    if (ret->expression) {
        ast_accept_expr(ret->expression, visitor);
    } else {
        emit_byte(OP_NIL);
    }
    
    emit_byte(OP_RETURN);
    return NULL;
}

static void* compile_break_stmt(ASTVisitor* visitor, Stmt* stmt) {
    (void)visitor;  // Unused parameter
    (void)stmt;     // Unused parameter
    if (current->inner_most_loop == NULL) {
        fprintf(stderr, "Cannot break outside of a loop\n");
        return NULL;
    }
    
    // Pop any locals created in the loop
    int locals_to_pop = 0;
    for (int i = current->locals.count - 1; i >= 0; i--) {
        if (current->locals.depths[i] <= current->inner_most_loop->scope_depth) {
            break;
        }
        locals_to_pop++;
    }
    
    for (int i = 0; i < locals_to_pop; i++) {
        emit_byte(OP_POP);
    }
    
    // Emit jump and record it for patching later
    int jump_offset = emit_jump(OP_JUMP);
    add_break_jump(current->inner_most_loop, jump_offset);
    
    return NULL;
}

static void* compile_continue_stmt(ASTVisitor* visitor, Stmt* stmt) {
    (void)visitor;  // Unused parameter
    (void)stmt;     // Unused parameter
    if (current->inner_most_loop == NULL) {
        fprintf(stderr, "Cannot continue outside of a loop\n");
        return NULL;
    }
    
    // Pop any locals created in this iteration
    int locals_to_pop = 0;
    for (int i = current->locals.count - 1; i >= 0; i--) {
        if (current->locals.depths[i] <= current->inner_most_loop->scope_depth) {
            break;
        }
        locals_to_pop++;
    }
    
    for (int i = 0; i < locals_to_pop; i++) {
        emit_byte(OP_POP);
    }
    
    // Jump back to loop start
    emit_loop(current->inner_most_loop->start);
    
    return NULL;
}

static void* compile_defer_stmt(ASTVisitor* visitor, Stmt* stmt) {
    (void)visitor;  // Unused parameter
    (void)stmt;     // Unused parameter
    
    // For now, just emit a placeholder
    fprintf(stderr, "Defer statement not yet implemented in compiler\n");
    
    return NULL;
}


static void* compile_function_stmt(ASTVisitor* visitor, Stmt* stmt) {
    FunctionDecl* func = &stmt->function;
    
    // Save parent compiler state BEFORE init_compiler changes 'current'
    Compiler* parent_compiler = current;
    Module* parent_module = current->current_module;
    bool parent_is_module = current->is_module_compilation;
    Chunk* parent_chunk = current->current_chunk;
    
    // Create a new compiler for the function
    Compiler func_compiler;
    init_compiler(&func_compiler, FUNC_TYPE_FUNCTION);
    func_compiler.function->name = MEM_STRDUP(allocators_get(ALLOC_SYSTEM_VM), func->name);
    func_compiler.function->arity = func->parameter_count;
    
    // Preserve module context from parent compiler (use saved values!)
    func_compiler.current_module = parent_module;
    func_compiler.is_module_compilation = parent_is_module;
    
    // Save current state and switch to function compiler  
    current = &func_compiler;
    
    // Add parameters as local variables starting at slot 1
    for (size_t i = 0; i < func->parameter_count; i++) {
        add_local(&func_compiler, func->parameter_names[i]);
        // Parameters are always initialized - set depth directly
        func_compiler.locals.depths[func_compiler.locals.count - 1] = 0;
    }
    
    // Compile the function body
    if (func->body) {
        ast_accept_stmt(func->body, visitor);
    }
    
    // Emit return if not already present
    if (func_compiler.function->chunk.count == 0 || 
        func_compiler.function->chunk.code[func_compiler.function->chunk.count - 1] != OP_RETURN) {
        emit_byte(OP_NIL);
        emit_byte(OP_RETURN);
    }
    
    // Switch back to enclosing compiler
    current = parent_compiler;  // Use the saved parent compiler
    current->current_chunk = parent_chunk;  // Restore the correct chunk
    
    // Set module context if we're compiling a module
    if (current->is_module_compilation && current->current_module) {
        func_compiler.function->module = current->current_module;
    }
    
    // Add function as a constant
    if (!func_compiler.function) {
        fprintf(stderr, "ERROR: func_compiler.function is NULL!\n");
        return NULL;
    }
    // Add the function as a constant
    TaggedValue func_val = FUNCTION_VAL(func_compiler.function);
    int constant = chunk_add_constant(current->current_chunk, func_val);
    
    // Emit closure creation instruction with the function constant
    if (constant < 256) {
        emit_bytes(OP_CLOSURE, constant);
    } else {
        // Use OP_CLOSURE_LONG for constants >= 256
        emit_byte(OP_CLOSURE_LONG);
        emit_byte((constant >> 16) & 0xff);
        emit_byte((constant >> 8) & 0xff);
        emit_byte(constant & 0xff);
    }
    
    // Emit upvalue information for the closure
    for (int i = 0; i < func_compiler.function->upvalue_count; i++) {
        emit_byte(func_compiler.upvalues.values[i].is_local ? 1 : 0);
        emit_byte(func_compiler.upvalues.values[i].index);
    }
    
    // In modules, functions are module-scoped unless exported
    // Export statements will handle making them visible
    if (current->is_module_compilation) {
        // fprintf(stderr, "DEBUG: In module compilation mode\n");
        // Store in module scope but don't make global
        // The module execution will handle storing in module scope
        int name_constant = chunk_add_constant(current->current_chunk,
            create_string_value(func->name));
        // fprintf(stderr, "DEBUG: Added name constant: %d\n", name_constant);
        // Use SET_GLOBAL which will be intercepted by module execution
        emit_bytes(OP_SET_GLOBAL, name_constant);
        // fprintf(stderr, "DEBUG: Emitted SET_GLOBAL\n");
    } else {
        // In scripts, functions are global
        int name_constant = chunk_add_constant(current->current_chunk,
            create_string_value(func->name));
        emit_bytes(OP_DEFINE_GLOBAL, name_constant);
    }
    
    // Check if this is an extension method (name contains "_ext_")
    if (strstr(func->name, "_ext_") != NULL) {
        fprintf(stderr, "[DEBUG] Compiling extension method: %s\n", func->name);
        
        // Extract the type name and method name
        char* type_end = strstr(func->name, "_ext_");
        size_t type_len = type_end - func->name;
        Allocator* alloc = allocators_get(ALLOC_SYSTEM_COMPILER);
        char* type_name = MEM_NEW_ARRAY(alloc, char, type_len + 1);
        strncpy(type_name, func->name, type_len);
        type_name[type_len] = '\0';
        
        char* method_name = type_end + 5; // Skip "_ext_"
        
        fprintf(stderr, "[DEBUG] Extension type: %s, method: %s\n", type_name, method_name);
        
        // Check if this is a built-in type or a struct type
        if (strcmp(type_name, "Number") == 0) {
            // Number prototype
            emit_byte(OP_GET_OBJECT_PROTO);
            emit_byte(3); // Number type ID
        } else if (strcmp(type_name, "String") == 0) {
            // String prototype
            emit_byte(OP_GET_OBJECT_PROTO);
            emit_byte(2); // String type ID
        } else if (strcmp(type_name, "Array") == 0) {
            // Array prototype
            emit_byte(OP_GET_OBJECT_PROTO);
            emit_byte(1); // Array type ID
        } else if (strcmp(type_name, "Object") == 0) {
            // Object prototype
            emit_byte(OP_GET_OBJECT_PROTO);
            emit_byte(0); // Object type ID
        } else if (strcmp(type_name, "Function") == 0) {
            // Function prototype
            emit_byte(OP_GET_OBJECT_PROTO);
            emit_byte(4); // Function type ID
        } else {
            // Struct types use their own prototypes
            emit_constant(create_string_value(type_name));
            emit_byte(OP_GET_STRUCT_PROTO);
        }
        
        // Push the method name
        fprintf(stderr, "[DEBUG] About to emit method name constant: %s\n", method_name);
        emit_constant(create_string_value(method_name));
        
        // Push the function value again
        fprintf(stderr, "[DEBUG] About to emit function value constant\n");
        emit_constant(func_val);
        
        // Set property on prototype
        fprintf(stderr, "[DEBUG] About to emit OP_SET_PROPERTY\n");
        emit_byte(OP_SET_PROPERTY);
        emit_byte(OP_POP); // Pop the result
        
        fprintf(stderr, "[DEBUG] About to free type_name\n");
        SLANG_MEM_FREE(alloc, type_name, type_len + 1);
        fprintf(stderr, "[DEBUG] Extension method setup complete\n");
    }
    
    // Clean up function compiler internals (but not the function itself)
    // free_compiler doesn't free the function, only the compiler's internal structures
    free_compiler(&func_compiler);
    
    // IMPORTANT: The function itself is now owned by the bytecode and must not be freed
    
    // fprintf(stderr, "DEBUG: compile_function_stmt completed successfully\n");
    return NULL;
}

static void* compile_class_stmt(ASTVisitor* visitor, Stmt* stmt) {
    ClassDecl* class = &stmt->class_decl;
    
    // Compile constructor body
    Compiler ctor_compiler;
    init_compiler(&ctor_compiler, FUNC_TYPE_FUNCTION);
    ctor_compiler.function->name = MEM_STRDUP(allocators_get(ALLOC_SYSTEM_VM), class->name);
    ctor_compiler.function->arity = 0;
    
    Compiler* previous = current;
    current = &ctor_compiler;
    
    begin_scope();
    
    // Create new instance
    emit_byte(OP_CREATE_OBJECT);
    
    // Initialize properties
    for (size_t i = 0; i < class->member_count; i++) {
        Stmt* member = class->members[i];
        
        if (member->type == STMT_VAR_DECL) {
            // It's a property
            VarDeclStmt* prop = &member->var_decl;
            
            // Duplicate instance
            emit_byte(OP_DUP);
            
            // Push property name
            TaggedValue prop_name = create_string_value(prop->name);
            emit_constant(prop_name);
            
            // Push initial value
            if (prop->initializer) {
                ast_accept_expr(prop->initializer, visitor);
            } else {
                emit_byte(OP_NIL);
            }
            
            // Set property
            emit_byte(OP_SET_PROPERTY);
            emit_byte(OP_POP);
        }
    }
    
    // Add methods directly to the instance
    for (size_t i = 0; i < class->member_count; i++) {
        Stmt* member = class->members[i];
        
        if (member->type == STMT_FUNCTION) {
            // It's a method
            FunctionDecl* method = &member->function;
            
            // Duplicate instance
            emit_byte(OP_DUP);
            
            // Push method name
            TaggedValue method_name = create_string_value(method->name);
            emit_constant(method_name);
            
            // Compile method as a function
            Allocator* vm_alloc = allocators_get(ALLOC_SYSTEM_VM);
            Function* func = MEM_NEW(vm_alloc, Function);
            func->name = MEM_STRDUP(vm_alloc, method->name);
            func->arity = method->parameter_count;
            func->upvalue_count = 0;
            chunk_init(&func->chunk);
            
            // Compile method body
            Compiler method_compiler;
            init_compiler(&method_compiler, FUNC_TYPE_FUNCTION);
            method_compiler.function = func;
            method_compiler.current_chunk = &func->chunk;
            
            Compiler* saved = current;
            current = &method_compiler;
            
            begin_scope();
            
            // Add 'self' as first local
            add_local(&method_compiler, "self");
            mark_initialized();
            
            // Add method parameters
            for (size_t j = 0; j < method->parameter_count; j++) {
                add_local(&method_compiler, method->parameter_names[j]);
                mark_initialized();
            }
            
            // Compile method body
            ast_accept_stmt(method->body, visitor);
            
            // Emit return if needed
            if (method_compiler.function->chunk.count == 0 || 
                method_compiler.function->chunk.code[method_compiler.function->chunk.count - 1] != OP_RETURN) {
                emit_byte(OP_NIL);
                emit_byte(OP_RETURN);
            }
            
            end_scope();
            
            current = saved;
            
            // Add function as a constant
            TaggedValue func_val = FUNCTION_VAL(func);
            emit_constant(func_val);
            
            // Set method on instance
            emit_byte(OP_SET_PROPERTY);
            emit_byte(OP_POP);
            
            // Clean up method compiler
            free_compiler(&method_compiler);
        }
    }
    
    // Return the instance
    emit_byte(OP_RETURN);
    
    end_scope();
    
    current = previous;
    
    // Define the constructor function as a global
    TaggedValue ctor_val = FUNCTION_VAL(ctor_compiler.function);
    emit_constant(ctor_val);
    
    int name_constant = chunk_add_constant(current->current_chunk,
        create_string_value(class->name));
    emit_bytes(OP_DEFINE_GLOBAL, name_constant);
    
    // Clean up constructor compiler
    free_compiler(&ctor_compiler);
    
    return NULL;
}

static void* compile_struct_stmt(ASTVisitor* visitor, Stmt* stmt) {
    (void)visitor;  // Unused parameter
    StructDecl* struct_decl = &stmt->struct_decl;
    
    // Extract field names from member declarations
    size_t field_count = 0;
    for (size_t i = 0; i < struct_decl->member_count; i++) {
        if (struct_decl->members[i]->type == STMT_VAR_DECL) {
            field_count++;
        }
    }
    
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_COMPILER);
    char** field_names = MEM_NEW_ARRAY(alloc, char*, field_count);
    size_t field_index = 0;
    
    for (size_t i = 0; i < struct_decl->member_count; i++) {
        if (struct_decl->members[i]->type == STMT_VAR_DECL) {
            field_names[field_index++] = struct_decl->members[i]->var_decl.name;
        }
    }
    
    // Emit struct definition with inline field names
    emit_byte(OP_DEFINE_STRUCT);
    
    // Add struct name as a constant
    int name_const = chunk_add_constant(current->current_chunk, 
        create_string_value(struct_decl->name));
    emit_byte(name_const);
    
    // Emit field count
    emit_byte(field_count);
    
    // Emit field names as inline constants
    for (size_t i = 0; i < field_count; i++) {
        int field_const = chunk_add_constant(current->current_chunk,
            create_string_value(field_names[i]));
        emit_byte(field_const);
    }
    
    // Now create a constructor function that uses OP_CREATE_STRUCT
    Function* constructor = function_create(struct_decl->name, field_count);
    
    // Save current compiler state
    Compiler* enclosing = current;
    Compiler struct_compiler;
    init_compiler(&struct_compiler, FUNC_TYPE_FUNCTION);
    struct_compiler.function = constructor;
    struct_compiler.current_chunk = &constructor->chunk;
    current = &struct_compiler;
    
    // Add parameters as local variables
    for (size_t i = 0; i < field_count; i++) {
        add_local(&struct_compiler, field_names[i]);
        mark_initialized();
    }
    
    // Emit constructor body
    // Push field values in order
    for (size_t i = 0; i < field_count; i++) {
        emit_bytes(OP_GET_LOCAL, i + 1); // +1 to skip slot 0 (function itself)
    }
    
    // Create struct instance
    emit_byte(OP_CREATE_STRUCT);
    int struct_name_const = chunk_add_constant(current->current_chunk,
        create_string_value(struct_decl->name));
    emit_byte(struct_name_const);
    
    // Return the struct instance
    emit_byte(OP_RETURN);
    
    // Restore compiler state
    current = enclosing;
    
    // Define the constructor as a global function
    emit_constant(FUNCTION_VAL(struct_compiler.function));
    int name_constant = chunk_add_constant(current->current_chunk,
        create_string_value(struct_decl->name));
    emit_bytes(OP_DEFINE_GLOBAL, name_constant);
    
    // Clean up
    SLANG_MEM_FREE(alloc, field_names, sizeof(char*) * field_count);
    free_compiler(&struct_compiler);
    
    return NULL;
}

static void* compile_import_stmt(ASTVisitor* visitor, Stmt* stmt) {
    (void)visitor;  // Unused parameter
    ImportDecl* import = &stmt->import_decl;
    
    // Check if this is a builtin module
    const char* module_name = import->module_path;
    
    // Remove quotes from module path if present
    if (module_name[0] == '"' || module_name[0] == '\'') {
        size_t len = strlen(module_name);
        if (len >= 2 && module_name[len-1] == module_name[0]) {
            Allocator* alloc = allocators_get(ALLOC_SYSTEM_COMPILER);
            char* unquoted = MEM_NEW_ARRAY(alloc, char, len - 1);
            memcpy(unquoted, module_name + 1, len - 2);
            unquoted[len - 2] = '\0';
            module_name = unquoted;
        }
    }
    
    // For builtin modules, we need to emit code to load the functions
    bool is_builtin = strcmp(module_name, "string") == 0 ||
                      strcmp(module_name, "array") == 0 ||
                      strcmp(module_name, "io") == 0;
    
    if (is_builtin) {
        // Handle builtin modules
        switch (import->type) {
            case IMPORT_SPECIFIC:
                // Import specific exports
                for (size_t i = 0; i < import->specifier_count; i++) {
                    const char* export_name = import->specifiers[i].name;
                    const char* local_name = import->specifiers[i].alias ?
                        import->specifiers[i].alias : import->specifiers[i].name;
                    
                    // Emit bytecode to load builtin function
                    // Push module name
                    TaggedValue module_str = create_string_value(module_name);
                    emit_constant(module_str);
                    
                    // Push export name
                    TaggedValue export_str = create_string_value(export_name);
                    emit_constant(export_str);
                    
                    // Call builtin loader
                    emit_byte(OP_LOAD_BUILTIN);
                    
                    // Define as global
                    int name_constant = chunk_add_constant(current->current_chunk,
                        create_string_value(local_name));
                    emit_bytes(OP_DEFINE_GLOBAL, name_constant);
                }
                break;
                
            case IMPORT_ALL:
                // For now, we don't support importing all from builtin modules
                break;
                
            case IMPORT_DEFAULT:
                // Similar to specific imports but for "default" export
                if (import->default_name) {
                    TaggedValue module_str = create_string_value(module_name);
                    emit_constant(module_str);
                    
                    TaggedValue export_str = create_string_value("default");
                    emit_constant(export_str);
                    
                    emit_byte(OP_LOAD_BUILTIN);
                    
                    int name_constant = chunk_add_constant(current->current_chunk,
                        create_string_value(import->default_name));
                    emit_bytes(OP_DEFINE_GLOBAL, name_constant);
                }
                break;
                
            case IMPORT_NAMESPACE:
                // Not yet implemented for builtin modules
                break;
        }
    } else {
        // Handle file-based modules
        // First, add module path as a constant
        TaggedValue module_path_val = create_string_value(module_name);
        int path_constant = chunk_add_constant(current->current_chunk, module_path_val);
        
        // Emit OP_LOAD_MODULE with the constant index
        emit_bytes(OP_LOAD_MODULE, path_constant);
        
        // The module object is now on the stack
        switch (import->type) {
            case IMPORT_SPECIFIC:
                // Import specific exports
                for (size_t i = 0; i < import->specifier_count; i++) {
                    const char* export_name = import->specifiers[i].name;
                    const char* local_name = import->specifiers[i].alias ?
                        import->specifiers[i].alias : import->specifiers[i].name;
                    
                    // Duplicate module object
                    emit_byte(OP_DUP);
                    
                    // Add export name as constant and emit OP_IMPORT_FROM with index
                    TaggedValue export_str = create_string_value(export_name);
                    int export_constant = chunk_add_constant(current->current_chunk, export_str);
                    
                    // Get export from module
                    emit_bytes(OP_IMPORT_FROM, export_constant);
                    
                    // Define as global
                    int name_constant = chunk_add_constant(current->current_chunk,
                        create_string_value(local_name));
                    emit_bytes(OP_DEFINE_GLOBAL, name_constant);
                }
                // Pop the module object
                emit_byte(OP_POP);
                break;
                
            case IMPORT_ALL:
                // import module or import module as alias or import * from module
                if (import->alias) {
                    // import module as alias
                    int name_constant = chunk_add_constant(current->current_chunk,
                        create_string_value(import->alias));
                    emit_bytes(OP_DEFINE_GLOBAL, name_constant);
                } else if (import->namespace_alias) {
                    // Old style: import * as name from module
                    int name_constant = chunk_add_constant(current->current_chunk,
                        create_string_value(import->namespace_alias));
                    emit_bytes(OP_DEFINE_GLOBAL, name_constant);
                } else if (import->import_all_to_scope) {
                    // import * from module - import all exports into current scope
                    emit_byte(OP_IMPORT_ALL_FROM);
                } else {
                    // No alias - define module with its original name
                    const char* module_simple_name = module_name;
                    
                    // Handle @ prefix for local imports
                    if (module_name[0] == '@') {
                        module_simple_name = module_name + 1;  // Skip @
                    }
                    // Handle $ prefix for native imports
                    else if (module_name[0] == '$') {
                        module_simple_name = module_name + 1;  // Skip $
                    }
                    
                    // Extract last part after dots
                    const char* dot = strrchr(module_simple_name, '.');
                    if (dot) {
                        module_simple_name = dot + 1;
                    }
                    
                    // Extract last part after slashes
                    const char* slash = strrchr(module_simple_name, '/');
                    if (slash) {
                        module_simple_name = slash + 1;
                    }
                    
                    int name_constant = chunk_add_constant(current->current_chunk,
                        create_string_value(module_simple_name));
                    emit_bytes(OP_DEFINE_GLOBAL, name_constant);
                }
                break;
                
            case IMPORT_DEFAULT:
                // import name from 'module'
                if (import->default_name) {
                    // Push "default" as export name
                    TaggedValue export_str = create_string_value("default");
                    emit_constant(export_str);
                    
                    // Get default export
                    emit_byte(OP_IMPORT_FROM);
                    
                    // Define as global
                    int name_constant = chunk_add_constant(current->current_chunk,
                        create_string_value(import->default_name));
                    emit_bytes(OP_DEFINE_GLOBAL, name_constant);
                } else {
                    // Pop the module if no default name
                    emit_byte(OP_POP);
                }
                break;
                
            case IMPORT_NAMESPACE:
                // import * as namespace from 'module'
                if (import->namespace_alias) {
                    int name_constant = chunk_add_constant(current->current_chunk,
                        create_string_value(import->namespace_alias));
                    emit_bytes(OP_DEFINE_GLOBAL, name_constant);
                } else {
                    emit_byte(OP_POP);
                }
                break;
        }
    }
    
    return NULL;
}

static void* compile_export_stmt(ASTVisitor* visitor, Stmt* stmt) {
    (void)visitor;  // Unused parameter
    ExportDecl* export = &stmt->export_decl;
    
    switch (export->type) {
        case EXPORT_NAMED:
            // export { name1, name2 as alias2 }
            if (export->named_export.specifiers) {
                for (size_t i = 0; i < export->named_export.specifier_count; i++) {
                    const char* local_name = export->named_export.specifiers[i].name;
                    const char* export_name = export->named_export.specifiers[i].alias ? 
                        export->named_export.specifiers[i].alias : export->named_export.specifiers[i].name;
                    
                    // Push the export name
                    TaggedValue export_str = create_string_value(export_name);
                    emit_constant(export_str);
                    
                    // Get the local value
                    int local = resolve_local(current, local_name);
                    if (local != -1) {
                        emit_bytes(OP_GET_LOCAL, (uint8_t)local);
                    } else {
                        int name_constant = chunk_add_constant(current->current_chunk,
                            create_string_value(local_name));
                        emit_bytes(OP_GET_GLOBAL, name_constant);
                    }
                    
                    // Use the new MODULE_EXPORT opcode
                    emit_byte(OP_MODULE_EXPORT);
                }
            }
            break;
            
        case EXPORT_DEFAULT:
            // export default expression
            if (export->default_export.name) {
                // Push "default" as export name
                TaggedValue default_str = create_string_value("default");
                emit_constant(default_str);
                
                // Get the value to export
                int local = resolve_local(current, export->default_export.name);
                if (local != -1) {
                    emit_bytes(OP_GET_LOCAL, (uint8_t)local);
                } else {
                    int name_constant = chunk_add_constant(current->current_chunk,
                        create_string_value(export->default_export.name));
                    emit_bytes(OP_GET_GLOBAL, name_constant);
                }
                
                // Use the new MODULE_EXPORT opcode
                emit_byte(OP_MODULE_EXPORT);
            }
            break;
            
        case EXPORT_ALL:
            // export * from 'module' - re-export everything
            // This is more complex and would require loading another module
            break;
            
        case EXPORT_DECLARATION:
            // export function foo() {} or export var x = 10
            if (export->decl_export.declaration) {
                Stmt* decl = (Stmt*)export->decl_export.declaration;
                
                // First compile the declaration
                ast_accept_stmt(decl, visitor);
                
                // Extract the name from the declaration and export it
                const char* export_name = NULL;
                
                if (decl->type == STMT_VAR_DECL) {
                    export_name = decl->var_decl.name;
                } else if (decl->type == STMT_FUNCTION) {
                    export_name = decl->function.name;
                }
                
                if (export_name) {
                    // Get the value to export
                    int local = resolve_local(current, export_name);
                    if (local != -1) {
                        emit_bytes(OP_GET_LOCAL, (uint8_t)local);
                    } else {
                        int name_constant = chunk_add_constant(current->current_chunk,
                            create_string_value(export_name));
                        emit_bytes(OP_GET_GLOBAL, name_constant);
                    }
                    
                    // Add export name as constant for OP_MODULE_EXPORT
                    int export_name_constant = chunk_add_constant(current->current_chunk,
                        create_string_value(export_name));
                    emit_bytes(OP_MODULE_EXPORT, export_name_constant);
                }
            }
            break;
    }
    
    return NULL;
}

// Main compile function
bool compile(ProgramNode* program, Chunk* chunk) {
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_COMPILER);
    
    Compiler compiler;
    compiler.enclosing = NULL;
    compiler.type = FUNC_TYPE_SCRIPT;
    compiler.scope_depth = 0;
    
    // Initialize locals
    compiler.locals.count = 0;
    compiler.locals.capacity = 8;
    compiler.locals.names = MEM_NEW_ARRAY(alloc, char*, compiler.locals.capacity);
    compiler.locals.depths = MEM_NEW_ARRAY(alloc, int, compiler.locals.capacity);
    
    // Initialize upvalues
    compiler.upvalues.count = 0;
    compiler.upvalues.capacity = 8;
    compiler.upvalues.values = MEM_NEW_ARRAY(alloc, CompilerUpvalue, compiler.upvalues.capacity);
    
    // Create function - but DON'T initialize its chunk, use the provided one
    Allocator* vm_alloc = allocators_get(ALLOC_SYSTEM_VM);
    compiler.function = MEM_NEW(vm_alloc, Function);
    compiler.function->name = MEM_STRDUP(vm_alloc, "<script>");
    compiler.function->arity = 0;
    compiler.function->upvalue_count = 0;
    compiler.function->module = NULL;
    // Initialize the function's chunk properly
    chunk_init(&compiler.function->chunk);
    
    // Set the current chunk to the provided chunk - we'll compile into this
    compiler.current_chunk = chunk;
    
    // CRITICAL: Set current to point to our compiler BEFORE any compilation
    current = &compiler;
    
    // Initialize other fields
    compiler.is_last_expr_stmt = false;
    compiler.program = program;
    compiler.current_stmt_index = 0;
    compiler.current_module = NULL;
    compiler.is_module_compilation = false;
    compiler.inner_most_loop = NULL;
    
    // Create visitor for compilation
    ASTVisitor visitor = {
        .context = &compiler,
        // Expression visitors
        .visit_literal_expr = compile_literal_expr,
        .visit_binary_expr = compile_binary_expr,
        .visit_unary_expr = compile_unary_expr,
        .visit_variable_expr = compile_variable_expr,
        .visit_assignment_expr = compile_assignment_expr,
        .visit_call_expr = compile_call_expr,
        .visit_array_literal_expr = compile_array_literal_expr,
        .visit_object_literal_expr = compile_object_literal_expr,
        .visit_closure_expr = compile_closure_expr,
        .visit_subscript_expr = compile_subscript_expr,
        .visit_member_expr = compile_member_expr,
        .visit_ternary_expr = compile_ternary_expr,
        .visit_nil_coalescing_expr = compile_nil_coalescing_expr,
        .visit_optional_chaining_expr = compile_optional_chaining_expr,
        .visit_force_unwrap_expr = compile_force_unwrap_expr,
        .visit_type_cast_expr = compile_type_cast_expr,
        .visit_await_expr = compile_await_expr,
        .visit_string_interp_expr = compile_string_interp_expr,
        // Statement visitors
        .visit_expression_stmt = compile_expression_stmt,
        .visit_var_decl_stmt = compile_var_decl_stmt,
        .visit_block_stmt = compile_block_stmt,
        .visit_if_stmt = compile_if_stmt,
        .visit_while_stmt = compile_while_stmt,
        .visit_for_in_stmt = compile_for_in_stmt,
        .visit_for_stmt = compile_for_stmt,
        .visit_return_stmt = compile_return_stmt,
        .visit_break_stmt = compile_break_stmt,
        .visit_continue_stmt = compile_continue_stmt,
        .visit_defer_stmt = compile_defer_stmt,
        .visit_function_stmt = compile_function_stmt,
        .visit_class_stmt = compile_class_stmt,
        .visit_struct_stmt = compile_struct_stmt,
        .visit_import_stmt = compile_import_stmt,
        .visit_export_stmt = compile_export_stmt
    };
    
    // Visit each statement in the program
    for (size_t i = 0; i < program->statement_count; i++) {
        compiler.current_stmt_index = i;
        // fprintf(stderr, "DEBUG: Before compiling statement %zu: chunk=%p, chunk->count=%zu\n", 
        //         i, (void*)compiler.current_chunk, compiler.current_chunk->count);
        // fprintf(stderr, "DEBUG: Compiling statement %zu (type %d) at chunk offset %zu\n", 
        //         i, program->statements[i]->type, compiler.current_chunk->count);
        // Check if this is the last expression statement
        if (i == program->statement_count - 1 && 
            program->statements[i]->type == STMT_EXPRESSION) {
            compiler.is_last_expr_stmt = true;
        }
        ast_accept_stmt(program->statements[i], &visitor);
        // Ensure we're still using the right chunk after each statement
        compiler.current_chunk = chunk;
        current->current_chunk = chunk;
        // fprintf(stderr, "DEBUG: After compiling statement %zu: chunk=%p, chunk->count=%zu, actual chunk count=%zu\n", 
        //         i, (void*)compiler.current_chunk, compiler.current_chunk->count, chunk->count);
    }
    
    // Emit final return
    if (!compiler.is_last_expr_stmt) {
        emit_byte(OP_NIL);
    }
    emit_byte(OP_RETURN);
    
    // Clean up compiler
    free_compiler(&compiler);
    
    current = NULL;
    return true;
}

// Module compile function
bool compile_module(ProgramNode* program, Chunk* chunk, Module* module) {
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_COMPILER);
    
    Compiler compiler;
    compiler.enclosing = NULL;
    compiler.type = FUNC_TYPE_SCRIPT;
    compiler.scope_depth = 0;
    
    // Initialize locals
    compiler.locals.count = 0;
    compiler.locals.capacity = 8;
    compiler.locals.names = MEM_NEW_ARRAY(alloc, char*, compiler.locals.capacity);
    compiler.locals.depths = MEM_NEW_ARRAY(alloc, int, compiler.locals.capacity);
    
    // Initialize upvalues
    compiler.upvalues.count = 0;
    compiler.upvalues.capacity = 8;
    compiler.upvalues.values = MEM_NEW_ARRAY(alloc, CompilerUpvalue, compiler.upvalues.capacity);
    
    // Create function - but DON'T initialize its chunk, use the provided one
    Allocator* vm_alloc = allocators_get(ALLOC_SYSTEM_VM);
    compiler.function = MEM_NEW(vm_alloc, Function);
    compiler.function->name = MEM_STRDUP(vm_alloc, "<module>");
    compiler.function->arity = 0;
    compiler.function->upvalue_count = 0;
    compiler.function->module = module;
    // Don't copy the chunk - the main function doesn't need its own chunk
    // We'll use the provided chunk directly
    
    // Set the current chunk to the provided chunk
    compiler.current_chunk = chunk;
    
    // Initialize other fields
    compiler.is_last_expr_stmt = false;
    compiler.program = program;
    compiler.current_stmt_index = 0;
    compiler.current_module = module;
    compiler.is_module_compilation = true;
    compiler.inner_most_loop = NULL;
    
    current = &compiler;
    
    // Create visitor for compilation
    ASTVisitor visitor = {
        .context = &compiler,
        // Expression visitors
        .visit_literal_expr = compile_literal_expr,
        .visit_binary_expr = compile_binary_expr,
        .visit_unary_expr = compile_unary_expr,
        .visit_variable_expr = compile_variable_expr,
        .visit_assignment_expr = compile_assignment_expr,
        .visit_call_expr = compile_call_expr,
        .visit_array_literal_expr = compile_array_literal_expr,
        .visit_object_literal_expr = compile_object_literal_expr,
        .visit_closure_expr = compile_closure_expr,
        .visit_subscript_expr = compile_subscript_expr,
        .visit_member_expr = compile_member_expr,
        .visit_ternary_expr = compile_ternary_expr,
        .visit_nil_coalescing_expr = compile_nil_coalescing_expr,
        .visit_optional_chaining_expr = compile_optional_chaining_expr,
        .visit_force_unwrap_expr = compile_force_unwrap_expr,
        .visit_type_cast_expr = compile_type_cast_expr,
        .visit_await_expr = compile_await_expr,
        .visit_string_interp_expr = compile_string_interp_expr,
        // Statement visitors
        .visit_expression_stmt = compile_expression_stmt,
        .visit_var_decl_stmt = compile_var_decl_stmt,
        .visit_block_stmt = compile_block_stmt,
        .visit_if_stmt = compile_if_stmt,
        .visit_while_stmt = compile_while_stmt,
        .visit_for_in_stmt = compile_for_in_stmt,
        .visit_for_stmt = compile_for_stmt,
        .visit_return_stmt = compile_return_stmt,
        .visit_break_stmt = compile_break_stmt,
        .visit_continue_stmt = compile_continue_stmt,
        .visit_defer_stmt = compile_defer_stmt,
        .visit_function_stmt = compile_function_stmt,
        .visit_class_stmt = compile_class_stmt,
        .visit_struct_stmt = compile_struct_stmt,
        .visit_import_stmt = compile_import_stmt,
        .visit_export_stmt = compile_export_stmt
    };
    
    // Visit each statement in the program
    for (size_t i = 0; i < program->statement_count; i++) {
        compiler.current_stmt_index = i;
        // fprintf(stderr, "DEBUG: Before compiling statement %zu: chunk=%p, chunk->count=%zu\n", 
        //         i, (void*)compiler.current_chunk, compiler.current_chunk->count);
        // fprintf(stderr, "DEBUG: Compiling statement %zu (type %d) at chunk offset %zu\n", 
        //         i, program->statements[i]->type, compiler.current_chunk->count);
        // Check if this is the last expression statement
        if (i == program->statement_count - 1 && 
            program->statements[i]->type == STMT_EXPRESSION) {
            compiler.is_last_expr_stmt = true;
        }
        ast_accept_stmt(program->statements[i], &visitor);
        // Ensure we're still using the right chunk after each statement
        compiler.current_chunk = chunk;
        current->current_chunk = chunk;
        // fprintf(stderr, "DEBUG: After compiling statement %zu: chunk=%p, chunk->count=%zu, actual chunk count=%zu\n", 
        //         i, (void*)compiler.current_chunk, compiler.current_chunk->count, chunk->count);
    }
    
    // Emit final return
    if (!compiler.is_last_expr_stmt) {
        emit_byte(OP_NIL);
    }
    emit_byte(OP_RETURN);
    
    // Clean up compiler
    free_compiler(&compiler);
    
    current = NULL;
    return true;
}
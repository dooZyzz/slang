#include "ast/ast.h"
#include "utils/allocators.h"
#include <string.h>

// AST nodes use the AST allocator which has arena allocation strategy
// This means all AST nodes are freed together when compilation is done

Expr* expr_create_literal_nil(void) {
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_AST);
    Expr* expr = MEM_NEW(alloc, Expr);
    if (!expr) return NULL;
    
    expr->type = EXPR_LITERAL;
    expr->literal.type = LITERAL_NIL;
    return expr;
}

Expr* expr_create_literal_bool(bool value) {
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_AST);
    Expr* expr = MEM_NEW(alloc, Expr);
    if (!expr) return NULL;
    
    expr->type = EXPR_LITERAL;
    expr->literal.type = LITERAL_BOOL;
    expr->literal.value.boolean = value;
    return expr;
}

Expr* expr_create_literal_int(long long value) {
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_AST);
    Expr* expr = MEM_NEW(alloc, Expr);
    if (!expr) return NULL;
    
    expr->type = EXPR_LITERAL;
    expr->literal.type = LITERAL_INT;
    expr->literal.value.integer = value;
    return expr;
}

Expr* expr_create_literal_float(double value) {
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_AST);
    Expr* expr = MEM_NEW(alloc, Expr);
    if (!expr) return NULL;
    
    expr->type = EXPR_LITERAL;
    expr->literal.type = LITERAL_FLOAT;
    expr->literal.value.floating = value;
    return expr;
}

Expr* expr_create_literal_string(const char* value) {
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_AST);
    Expr* expr = MEM_NEW(alloc, Expr);
    if (!expr) return NULL;
    
    expr->type = EXPR_LITERAL;
    expr->literal.type = LITERAL_STRING;
    expr->literal.value.string.value = MEM_STRDUP(alloc, value);
    expr->literal.value.string.length = strlen(value);
    return expr;
}

Expr* expr_create_variable(const char* name) {
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_AST);
    Expr* expr = MEM_NEW(alloc, Expr);
    if (!expr) return NULL;
    
    expr->type = EXPR_VARIABLE;
    expr->variable.name = MEM_STRDUP(alloc, name);
    return expr;
}

Expr* expr_create_binary(Token operator, Expr* left, Expr* right) {
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_AST);
    Expr* expr = MEM_NEW(alloc, Expr);
    if (!expr) return NULL;
    
    expr->type = EXPR_BINARY;
    expr->binary.operator = operator;
    expr->binary.left = left;
    expr->binary.right = right;
    return expr;
}

Expr* expr_create_unary(Token operator, Expr* operand) {
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_AST);
    Expr* expr = MEM_NEW(alloc, Expr);
    if (!expr) return NULL;
    
    expr->type = EXPR_UNARY;
    expr->unary.operator = operator;
    expr->unary.operand = operand;
    return expr;
}

Expr* expr_create_assignment(Expr* target, Expr* value) {
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_AST);
    Expr* expr = MEM_NEW(alloc, Expr);
    if (!expr) return NULL;
    
    expr->type = EXPR_ASSIGNMENT;
    expr->assignment.target = target;
    expr->assignment.value = value;
    return expr;
}

Expr* expr_create_call(Expr* callee, Expr** arguments, size_t argument_count) {
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_AST);
    Expr* expr = MEM_NEW(alloc, Expr);
    if (!expr) return NULL;
    
    expr->type = EXPR_CALL;
    expr->call.callee = callee;
    expr->call.argument_count = argument_count;
    if (argument_count > 0) {
        expr->call.arguments = MEM_NEW_ARRAY(alloc, Expr*, argument_count);
        if (expr->call.arguments) {
            memcpy(expr->call.arguments, arguments, argument_count * sizeof(Expr*));
        }
    }
    return expr;
}

Expr* expr_create_subscript(Expr* object, Expr* index) {
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_AST);
    Expr* expr = MEM_NEW(alloc, Expr);
    if (!expr) return NULL;
    
    expr->type = EXPR_SUBSCRIPT;
    expr->subscript.object = object;
    expr->subscript.index = index;
    return expr;
}

Expr* expr_create_member(Expr* object, const char* property) {
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_AST);
    Expr* expr = MEM_NEW(alloc, Expr);
    if (!expr) return NULL;
    
    expr->type = EXPR_MEMBER;
    expr->member.object = object;
    expr->member.property = MEM_STRDUP(alloc, property);
    return expr;
}

Expr* expr_create_array_literal(Expr** elements, size_t element_count) {
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_AST);
    Expr* expr = MEM_NEW(alloc, Expr);
    if (!expr) return NULL;
    
    expr->type = EXPR_ARRAY_LITERAL;
    expr->array_literal.element_count = element_count;
    if (element_count > 0) {
        expr->array_literal.elements = MEM_NEW_ARRAY(alloc, Expr*, element_count);
        if (expr->array_literal.elements) {
            memcpy(expr->array_literal.elements, elements, element_count * sizeof(Expr*));
        }
    }
    return expr;
}

Expr* expr_create_object_literal(const char** keys, Expr** values, size_t pair_count) {
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_AST);
    Expr* expr = MEM_NEW(alloc, Expr);
    if (!expr) return NULL;
    
    expr->type = EXPR_OBJECT_LITERAL;
    expr->object_literal.pair_count = pair_count;
    
    if (pair_count > 0) {
        expr->object_literal.keys = MEM_NEW_ARRAY(alloc, const char*, pair_count);
        expr->object_literal.values = MEM_NEW_ARRAY(alloc, Expr*, pair_count);
        
        if (expr->object_literal.keys && expr->object_literal.values) {
            for (size_t i = 0; i < pair_count; i++) {
                expr->object_literal.keys[i] = MEM_STRDUP(alloc, keys[i]);
                expr->object_literal.values[i] = values[i];
            }
        }
    }
    
    return expr;
}

Expr* expr_create_closure(const char** parameter_names, TypeExpr** parameter_types, 
                         size_t parameter_count, TypeExpr* return_type, Stmt* body) {
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_AST);
    Expr* expr = MEM_NEW(alloc, Expr);
    if (!expr) return NULL;
    
    expr->type = EXPR_CLOSURE;
    expr->closure.parameter_count = parameter_count;
    expr->closure.return_type = return_type;
    expr->closure.body = body;
    
    if (parameter_count > 0) {
        expr->closure.parameter_names = MEM_NEW_ARRAY(alloc, const char*, parameter_count);
        expr->closure.parameter_types = MEM_NEW_ARRAY(alloc, TypeExpr*, parameter_count);
        
        if (expr->closure.parameter_names && expr->closure.parameter_types) {
            for (size_t i = 0; i < parameter_count; i++) {
                expr->closure.parameter_names[i] = MEM_STRDUP(alloc, parameter_names[i]);
                expr->closure.parameter_types[i] = parameter_types[i];
            }
        }
    }
    
    return expr;
}

Expr* expr_create_string_interp(char** parts, Expr** expressions, size_t part_count, size_t expr_count) {
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_AST);
    Expr* expr = MEM_NEW(alloc, Expr);
    if (!expr) return NULL;
    
    expr->type = EXPR_STRING_INTERP;
    expr->string_interp.part_count = part_count;
    expr->string_interp.expr_count = expr_count;
    
    if (part_count > 0) {
        expr->string_interp.parts = MEM_NEW_ARRAY(alloc, char*, part_count);
        if (expr->string_interp.parts) {
            for (size_t i = 0; i < part_count; i++) {
                expr->string_interp.parts[i] = MEM_STRDUP(alloc, parts[i]);
            }
        }
    }
    
    if (expr_count > 0) {
        expr->string_interp.expressions = MEM_NEW_ARRAY(alloc, Expr*, expr_count);
        if (expr->string_interp.expressions) {
            memcpy(expr->string_interp.expressions, expressions, expr_count * sizeof(Expr*));
        }
    }
    
    return expr;
}

// Statement creation functions

Stmt* stmt_create_expression(Expr* expression) {
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_AST);
    Stmt* stmt = MEM_NEW(alloc, Stmt);
    if (!stmt) return NULL;
    
    stmt->type = STMT_EXPRESSION;
    stmt->expression.expression = expression;
    return stmt;
}

Stmt* stmt_create_var_decl(bool is_mutable, const char* name, const char* type_annotation, Expr* initializer) {
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_AST);
    Stmt* stmt = MEM_NEW(alloc, Stmt);
    if (!stmt) return NULL;
    
    stmt->type = STMT_VAR_DECL;
    stmt->var_decl.is_mutable = is_mutable;
    stmt->var_decl.name = MEM_STRDUP(alloc, name);
    stmt->var_decl.type_annotation = type_annotation ? MEM_STRDUP(alloc, type_annotation) : NULL;
    stmt->var_decl.initializer = initializer;
    return stmt;
}

Stmt* stmt_create_block(Stmt** statements, size_t statement_count) {
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_AST);
    Stmt* stmt = MEM_NEW(alloc, Stmt);
    if (!stmt) return NULL;
    
    stmt->type = STMT_BLOCK;
    stmt->block.statement_count = statement_count;
    
    if (statement_count > 0) {
        stmt->block.statements = MEM_NEW_ARRAY(alloc, Stmt*, statement_count);
        if (stmt->block.statements) {
            memcpy(stmt->block.statements, statements, statement_count * sizeof(Stmt*));
        }
    }
    
    return stmt;
}

Stmt* stmt_create_if(Expr* condition, Stmt* then_branch, Stmt* else_branch) {
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_AST);
    Stmt* stmt = MEM_NEW(alloc, Stmt);
    if (!stmt) return NULL;
    
    stmt->type = STMT_IF;
    stmt->if_stmt.condition = condition;
    stmt->if_stmt.then_branch = then_branch;
    stmt->if_stmt.else_branch = else_branch;
    return stmt;
}

Stmt* stmt_create_while(Expr* condition, Stmt* body) {
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_AST);
    Stmt* stmt = MEM_NEW(alloc, Stmt);
    if (!stmt) return NULL;
    
    stmt->type = STMT_WHILE;
    stmt->while_stmt.condition = condition;
    stmt->while_stmt.body = body;
    return stmt;
}

Stmt* stmt_create_for_in(const char* variable_name, Expr* iterable, Stmt* body) {
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_AST);
    Stmt* stmt = MEM_NEW(alloc, Stmt);
    if (!stmt) return NULL;
    
    stmt->type = STMT_FOR_IN;
    stmt->for_in.variable_name = MEM_STRDUP(alloc, variable_name);
    stmt->for_in.iterable = iterable;
    stmt->for_in.body = body;
    return stmt;
}

Stmt* stmt_create_for(Stmt* initializer, Expr* condition, Expr* increment, Stmt* body) {
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_AST);
    Stmt* stmt = MEM_NEW(alloc, Stmt);
    if (!stmt) return NULL;
    
    stmt->type = STMT_FOR;
    stmt->for_stmt.initializer = initializer;
    stmt->for_stmt.condition = condition;
    stmt->for_stmt.increment = increment;
    stmt->for_stmt.body = body;
    return stmt;
}

Stmt* stmt_create_return(Expr* expression) {
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_AST);
    Stmt* stmt = MEM_NEW(alloc, Stmt);
    if (!stmt) return NULL;
    
    stmt->type = STMT_RETURN;
    stmt->return_stmt.expression = expression;
    return stmt;
}

Stmt* stmt_create_break(void) {
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_AST);
    Stmt* stmt = MEM_NEW(alloc, Stmt);
    if (!stmt) return NULL;
    
    stmt->type = STMT_BREAK;
    return stmt;
}

Stmt* stmt_create_continue(void) {
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_AST);
    Stmt* stmt = MEM_NEW(alloc, Stmt);
    if (!stmt) return NULL;
    
    stmt->type = STMT_CONTINUE;
    return stmt;
}

Stmt* stmt_create_function(const char* name, const char** parameter_names, const char** parameter_types,
                          size_t parameter_count, const char* return_type, Stmt* body) {
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_AST);
    Stmt* stmt = MEM_NEW(alloc, Stmt);
    if (!stmt) return NULL;
    
    stmt->type = STMT_FUNCTION;
    stmt->function.name = MEM_STRDUP(alloc, name);
    stmt->function.parameter_count = parameter_count;
    stmt->function.return_type = return_type ? MEM_STRDUP(alloc, return_type) : NULL;
    stmt->function.body = body;
    
    if (parameter_count > 0) {
        stmt->function.parameter_names = MEM_NEW_ARRAY(alloc, const char*, parameter_count);
        stmt->function.parameter_types = MEM_NEW_ARRAY(alloc, const char*, parameter_count);
        
        if (stmt->function.parameter_names && stmt->function.parameter_types) {
            for (size_t i = 0; i < parameter_count; i++) {
                stmt->function.parameter_names[i] = MEM_STRDUP(alloc, parameter_names[i]);
                stmt->function.parameter_types[i] = parameter_types[i] ? MEM_STRDUP(alloc, parameter_types[i]) : NULL;
            }
        }
    }
    
    return stmt;
}

Stmt* stmt_create_class(const char* name, const char* superclass, Stmt** members, size_t member_count) {
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_AST);
    Stmt* stmt = MEM_NEW(alloc, Stmt);
    if (!stmt) return NULL;
    
    stmt->type = STMT_CLASS;
    stmt->class_decl.name = MEM_STRDUP(alloc, name);
    stmt->class_decl.superclass = superclass ? MEM_STRDUP(alloc, superclass) : NULL;
    stmt->class_decl.member_count = member_count;
    
    if (member_count > 0) {
        stmt->class_decl.members = MEM_NEW_ARRAY(alloc, Stmt*, member_count);
        if (stmt->class_decl.members) {
            memcpy(stmt->class_decl.members, members, member_count * sizeof(Stmt*));
        }
    }
    
    return stmt;
}

Stmt* stmt_create_import(ImportType type, const char* module_path) {
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_AST);
    Stmt* stmt = MEM_NEW(alloc, Stmt);
    if (!stmt) return NULL;
    
    stmt->type = STMT_IMPORT;
    stmt->import_decl.type = type;
    stmt->import_decl.module_path = MEM_STRDUP(alloc, module_path);
    
    // Initialize other fields
    stmt->import_decl.alias = NULL;
    stmt->import_decl.namespace_alias = NULL;
    stmt->import_decl.specifiers = NULL;
    stmt->import_decl.specifier_count = 0;
    stmt->import_decl.is_local = false;
    stmt->import_decl.is_native = false;
    stmt->import_decl.import_all_to_scope = false;
    
    return stmt;
}

Stmt* stmt_create_export(ExportType type) {
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_AST);
    Stmt* stmt = MEM_NEW(alloc, Stmt);
    if (!stmt) return NULL;
    
    stmt->type = STMT_EXPORT;
    stmt->export_decl.type = type;
    
    // Initialize union fields
    memset(&stmt->export_decl.default_export, 0, sizeof(stmt->export_decl.default_export));
    
    return stmt;
}

// Program node creation

ProgramNode* program_create(Stmt** statements, size_t statement_count) {
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_AST);
    ProgramNode* program = MEM_NEW(alloc, ProgramNode);
    if (!program) return NULL;
    
    program->statement_count = statement_count;
    program->module_name = NULL;
    
    if (statement_count > 0) {
        program->statements = MEM_NEW_ARRAY(alloc, Stmt*, statement_count);
        if (program->statements) {
            memcpy(program->statements, statements, statement_count * sizeof(Stmt*));
        }
    }
    
    return program;
}

// AST destruction functions
// Note: With arena allocation, we don't need individual destroy functions
// The entire AST is freed when the arena is reset/destroyed

void expr_destroy(Expr* expr) {
    // No-op with arena allocation
    // AST nodes are freed together when the arena is cleared
}

void stmt_destroy(Stmt* stmt) {
    // No-op with arena allocation
}

void program_destroy(ProgramNode* program) {
    // No-op with arena allocation
}

// Type expression creation functions

TypeExpr* type_expr_identifier(const char* name) {
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_AST);
    TypeExpr* type_expr = MEM_NEW(alloc, TypeExpr);
    if (!type_expr) return NULL;
    
    type_expr->type = TYPE_IDENTIFIER;
    type_expr->identifier.name = MEM_STRDUP(alloc, name);
    return type_expr;
}

TypeExpr* type_expr_optional(TypeExpr* base_type) {
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_AST);
    TypeExpr* type_expr = MEM_NEW(alloc, TypeExpr);
    if (!type_expr) return NULL;
    
    type_expr->type = TYPE_OPTIONAL;
    type_expr->optional.wrapped = base_type;
    return type_expr;
}

TypeExpr* type_expr_array(TypeExpr* element_type) {
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_AST);
    TypeExpr* type_expr = MEM_NEW(alloc, TypeExpr);
    if (!type_expr) return NULL;
    
    type_expr->type = TYPE_ARRAY;
    type_expr->array.element = element_type;
    return type_expr;
}

TypeExpr* type_expr_dictionary(TypeExpr* key_type, TypeExpr* value_type) {
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_AST);
    TypeExpr* type_expr = MEM_NEW(alloc, TypeExpr);
    if (!type_expr) return NULL;
    
    type_expr->type = TYPE_DICTIONARY;
    type_expr->dictionary.key = key_type;
    type_expr->dictionary.value = value_type;
    return type_expr;
}

TypeExpr* type_expr_function(TypeExpr** param_types, size_t param_count, TypeExpr* return_type) {
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_AST);
    TypeExpr* type_expr = MEM_NEW(alloc, TypeExpr);
    if (!type_expr) return NULL;
    
    type_expr->type = TYPE_FUNCTION;
    type_expr->function.parameter_count = param_count;
    type_expr->function.return_type = return_type;
    
    if (param_count > 0) {
        type_expr->function.parameters = MEM_NEW_ARRAY(alloc, TypeExpr*, param_count);
        if (type_expr->function.parameters) {
            memcpy(type_expr->function.parameters, param_types, param_count * sizeof(TypeExpr*));
        }
    }
    
    return type_expr;
}

void type_expr_destroy(TypeExpr* type_expr) {
    // No-op with arena allocation
}

// Compatibility wrapper for ast_free_program
void ast_free_program(ProgramNode* program) {
    program_destroy(program);
}
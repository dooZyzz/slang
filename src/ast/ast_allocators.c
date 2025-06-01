#include "ast/ast.h"
#include "utils/allocators.h"
#include <stdlib.h>
#include <string.h>

// AST uses arena allocator - all nodes are freed at once when compilation is done

Expr* expr_create_literal_nil(void) {
    Expr* expr = AST_NEW(Expr);
    if (!expr) return NULL;
    expr->type = EXPR_LITERAL;
    expr->literal.type = LITERAL_NIL;
    return expr;
}

Expr* expr_create_literal_bool(bool value) {
    Expr* expr = AST_NEW(Expr);
    if (!expr) return NULL;
    expr->type = EXPR_LITERAL;
    expr->literal.type = LITERAL_BOOL;
    expr->literal.value.boolean = value;
    return expr;
}

Expr* expr_create_literal_int(long long value) {
    Expr* expr = AST_NEW(Expr);
    if (!expr) return NULL;
    expr->type = EXPR_LITERAL;
    expr->literal.type = LITERAL_INT;
    expr->literal.value.integer = value;
    return expr;
}

Expr* expr_create_literal_float(double value) {
    Expr* expr = AST_NEW(Expr);
    if (!expr) return NULL;
    expr->type = EXPR_LITERAL;
    expr->literal.type = LITERAL_FLOAT;
    expr->literal.value.floating = value;
    return expr;
}

Expr* expr_create_literal_string(const char* value) {
    Expr* expr = AST_NEW(Expr);
    if (!expr) return NULL;
    expr->type = EXPR_LITERAL;
    expr->literal.type = LITERAL_STRING;
    expr->literal.value.string.value = AST_DUP(value);
    expr->literal.value.string.length = strlen(value);
    return expr;
}

Expr* expr_create_variable(const char* name) {
    Expr* expr = AST_NEW(Expr);
    if (!expr) return NULL;
    expr->type = EXPR_VARIABLE;
    expr->variable.name = AST_DUP(name);
    return expr;
}

Expr* expr_create_binary(Token operator, Expr* left, Expr* right) {
    Expr* expr = AST_NEW(Expr);
    if (!expr) return NULL;
    expr->type = EXPR_BINARY;
    expr->binary.operator = operator;
    expr->binary.left = left;
    expr->binary.right = right;
    return expr;
}

Expr* expr_create_unary(Token operator, Expr* operand) {
    Expr* expr = AST_NEW(Expr);
    if (!expr) return NULL;
    expr->type = EXPR_UNARY;
    expr->unary.operator = operator;
    expr->unary.operand = operand;
    return expr;
}

Expr* expr_create_assignment(Expr* target, Expr* value) {
    Expr* expr = AST_NEW(Expr);
    if (!expr) return NULL;
    expr->type = EXPR_ASSIGNMENT;
    expr->assignment.target = target;
    expr->assignment.value = value;
    return expr;
}

Expr* expr_create_call(Expr* callee, Expr** arguments, size_t argument_count) {
    Expr* expr = AST_NEW(Expr);
    if (!expr) return NULL;
    expr->type = EXPR_CALL;
    expr->call.callee = callee;
    expr->call.argument_count = argument_count;
    if (argument_count > 0) {
        expr->call.arguments = AST_NEW_ARRAY(Expr*, argument_count);
        if (expr->call.arguments) {
            memcpy(expr->call.arguments, arguments, argument_count * sizeof(Expr*));
        }
    }
    return expr;
}

Expr* expr_create_subscript(Expr* object, Expr* index) {
    Expr* expr = AST_NEW(Expr);
    if (!expr) return NULL;
    expr->type = EXPR_SUBSCRIPT;
    expr->subscript.object = object;
    expr->subscript.index = index;
    return expr;
}

Expr* expr_create_member(Expr* object, const char* property) {
    Expr* expr = AST_NEW(Expr);
    if (!expr) return NULL;
    expr->type = EXPR_MEMBER;
    expr->member.object = object;
    expr->member.property = AST_DUP(property);
    return expr;
}

Expr* expr_create_array_literal(Expr** elements, size_t element_count) {
    Expr* expr = AST_NEW(Expr);
    if (!expr) return NULL;
    expr->type = EXPR_ARRAY_LITERAL;
    expr->array_literal.element_count = element_count;
    if (element_count > 0) {
        expr->array_literal.elements = AST_NEW_ARRAY(Expr*, element_count);
        if (expr->array_literal.elements) {
            memcpy(expr->array_literal.elements, elements, element_count * sizeof(Expr*));
        }
    }
    return expr;
}

Expr* expr_create_object_literal(const char** keys, Expr** values, size_t pair_count) {
    Expr* expr = AST_NEW(Expr);
    if (!expr) return NULL;
    expr->type = EXPR_OBJECT_LITERAL;
    expr->object_literal.pair_count = pair_count;
    if (pair_count > 0) {
        expr->object_literal.keys = AST_NEW_ARRAY(char*, pair_count);
        expr->object_literal.values = AST_NEW_ARRAY(Expr*, pair_count);
        if (expr->object_literal.keys && expr->object_literal.values) {
            for (size_t i = 0; i < pair_count; i++) {
                expr->object_literal.keys[i] = AST_DUP(keys[i]);
            }
            memcpy(expr->object_literal.values, values, pair_count * sizeof(Expr*));
        }
    }
    return expr;
}

Expr* expr_create_closure(const char** parameter_names, TypeExpr** parameter_types,
                         size_t parameter_count, TypeExpr* return_type, Stmt* body) {
    Expr* expr = AST_NEW(Expr);
    if (!expr) return NULL;
    expr->type = EXPR_CLOSURE;
    expr->closure.parameter_names = parameter_names;
    expr->closure.parameter_types = parameter_types;
    expr->closure.parameter_count = parameter_count;
    expr->closure.return_type = return_type;
    expr->closure.body = body;
    return expr;
}

Expr* expr_create_string_interp(char** parts, Expr** expressions, size_t part_count, size_t expr_count) {
    Expr* expr = AST_NEW(Expr);
    if (!expr) return NULL;
    expr->type = EXPR_STRING_INTERP;
    expr->string_interp.parts = parts;
    expr->string_interp.expressions = expressions;
    expr->string_interp.part_count = part_count;
    expr->string_interp.expr_count = expr_count;
    return expr;
}

Expr* expr_create_optional_chain(Expr* object, const char* property) {
    Expr* expr = AST_NEW(Expr);
    if (!expr) return NULL;
    expr->type = EXPR_OPTIONAL_CHAIN;
    expr->optional_chain.object = object;
    expr->optional_chain.property = AST_DUP(property);
    return expr;
}

Expr* expr_create_nil_coalesce(Expr* optional, Expr* default_value) {
    Expr* expr = AST_NEW(Expr);
    if (!expr) return NULL;
    expr->type = EXPR_NIL_COALESCE;
    expr->nil_coalesce.optional = optional;
    expr->nil_coalesce.default_value = default_value;
    return expr;
}

Expr* expr_create_struct_init(const char* struct_name, const char** field_names, 
                             Expr** field_values, size_t field_count) {
    Expr* expr = AST_NEW(Expr);
    if (!expr) return NULL;
    expr->type = EXPR_STRUCT_INIT;
    expr->struct_init.struct_name = AST_DUP(struct_name);
    expr->struct_init.field_count = field_count;
    
    if (field_count > 0) {
        expr->struct_init.field_names = AST_NEW_ARRAY(char*, field_count);
        expr->struct_init.field_values = AST_NEW_ARRAY(Expr*, field_count);
        
        if (expr->struct_init.field_names && expr->struct_init.field_values) {
            for (size_t i = 0; i < field_count; i++) {
                expr->struct_init.field_names[i] = AST_DUP(field_names[i]);
            }
            memcpy(expr->struct_init.field_values, field_values, field_count * sizeof(Expr*));
        }
    }
    
    return expr;
}

// Statement creation functions
Stmt* stmt_create_expression(Expr* expression) {
    Stmt* stmt = AST_NEW(Stmt);
    if (!stmt) return NULL;
    stmt->type = STMT_EXPRESSION;
    stmt->expression.expression = expression;
    return stmt;
}

Stmt* stmt_create_return(Expr* value) {
    Stmt* stmt = AST_NEW(Stmt);
    if (!stmt) return NULL;
    stmt->type = STMT_RETURN;
    stmt->return_stmt.value = value;
    return stmt;
}

Stmt* stmt_create_if(Expr* condition, Stmt* then_branch, Stmt* else_branch) {
    Stmt* stmt = AST_NEW(Stmt);
    if (!stmt) return NULL;
    stmt->type = STMT_IF;
    stmt->if_stmt.condition = condition;
    stmt->if_stmt.then_branch = then_branch;
    stmt->if_stmt.else_branch = else_branch;
    return stmt;
}

Stmt* stmt_create_while(Expr* condition, Stmt* body) {
    Stmt* stmt = AST_NEW(Stmt);
    if (!stmt) return NULL;
    stmt->type = STMT_WHILE;
    stmt->while_stmt.condition = condition;
    stmt->while_stmt.body = body;
    return stmt;
}

Stmt* stmt_create_for(Expr* initializer, Expr* condition, Expr* increment, Stmt* body) {
    Stmt* stmt = AST_NEW(Stmt);
    if (!stmt) return NULL;
    stmt->type = STMT_FOR;
    stmt->for_stmt.initializer = initializer;
    stmt->for_stmt.condition = condition;
    stmt->for_stmt.increment = increment;
    stmt->for_stmt.body = body;
    return stmt;
}

Stmt* stmt_create_break(void) {
    Stmt* stmt = AST_NEW(Stmt);
    if (!stmt) return NULL;
    stmt->type = STMT_BREAK;
    return stmt;
}

Stmt* stmt_create_continue(void) {
    Stmt* stmt = AST_NEW(Stmt);
    if (!stmt) return NULL;
    stmt->type = STMT_CONTINUE;
    return stmt;
}

Stmt* stmt_create_block(Stmt** statements, size_t statement_count) {
    Stmt* stmt = AST_NEW(Stmt);
    if (!stmt) return NULL;
    stmt->type = STMT_BLOCK;
    stmt->block.statement_count = statement_count;
    if (statement_count > 0) {
        stmt->block.statements = AST_NEW_ARRAY(Stmt*, statement_count);
        if (stmt->block.statements) {
            memcpy(stmt->block.statements, statements, statement_count * sizeof(Stmt*));
        }
    }
    return stmt;
}

Stmt* stmt_create_var(const char* name, TypeExpr* type, Expr* initializer) {
    Stmt* stmt = AST_NEW(Stmt);
    if (!stmt) return NULL;
    stmt->type = STMT_VAR;
    stmt->var.name = AST_DUP(name);
    stmt->var.type = type;
    stmt->var.initializer = initializer;
    return stmt;
}

Stmt* stmt_create_module(const char* name, Stmt** statements, size_t statement_count) {
    Stmt* stmt = AST_NEW(Stmt);
    if (!stmt) return NULL;
    stmt->type = STMT_MODULE;
    stmt->module.name = AST_DUP(name);
    stmt->module.statement_count = statement_count;
    if (statement_count > 0) {
        stmt->module.statements = AST_NEW_ARRAY(Stmt*, statement_count);
        if (stmt->module.statements) {
            memcpy(stmt->module.statements, statements, statement_count * sizeof(Stmt*));
        }
    }
    return stmt;
}

// Declaration creation functions
Decl* decl_create_var(const char* name, TypeExpr* type, Expr* initializer, bool is_exported) {
    Decl* decl = AST_NEW(Decl);
    if (!decl) return NULL;
    decl->type = DECL_VAR;
    decl->var.name = AST_DUP(name);
    decl->var.type = type;
    decl->var.initializer = initializer;
    decl->var.is_exported = is_exported;
    return decl;
}

Decl* decl_create_function(const char* name, const char** parameter_names, TypeExpr** parameter_types,
                          size_t parameter_count, TypeExpr* return_type, Stmt* body, bool is_exported) {
    Decl* decl = AST_NEW(Decl);
    if (!decl) return NULL;
    decl->type = DECL_FUNCTION;
    decl->function.name = AST_DUP(name);
    decl->function.parameter_count = parameter_count;
    
    if (parameter_count > 0) {
        decl->function.parameter_names = AST_NEW_ARRAY(char*, parameter_count);
        decl->function.parameter_types = parameter_types;
        
        if (decl->function.parameter_names) {
            for (size_t i = 0; i < parameter_count; i++) {
                decl->function.parameter_names[i] = AST_DUP(parameter_names[i]);
            }
        }
    } else {
        decl->function.parameter_names = NULL;
        decl->function.parameter_types = NULL;
    }
    
    decl->function.return_type = return_type;
    decl->function.body = body;
    decl->function.is_exported = is_exported;
    return decl;
}

Decl* decl_create_struct(const char* name, const char** field_names, TypeExpr** field_types, 
                        size_t field_count, bool is_exported) {
    Decl* decl = AST_NEW(Decl);
    if (!decl) return NULL;
    decl->type = DECL_STRUCT;
    decl->struct_decl.name = AST_DUP(name);
    decl->struct_decl.field_count = field_count;
    decl->struct_decl.is_exported = is_exported;
    
    if (field_count > 0) {
        decl->struct_decl.field_names = AST_NEW_ARRAY(char*, field_count);
        decl->struct_decl.field_types = field_types;
        
        if (decl->struct_decl.field_names) {
            for (size_t i = 0; i < field_count; i++) {
                decl->struct_decl.field_names[i] = AST_DUP(field_names[i]);
            }
        }
    } else {
        decl->struct_decl.field_names = NULL;
        decl->struct_decl.field_types = NULL;
    }
    
    return decl;
}

Decl* decl_create_extension(const char* type_name, Decl** methods, size_t method_count) {
    Decl* decl = AST_NEW(Decl);
    if (!decl) return NULL;
    decl->type = DECL_EXTENSION;
    decl->extension.type_name = AST_DUP(type_name);
    decl->extension.method_count = method_count;
    
    if (method_count > 0) {
        decl->extension.methods = AST_NEW_ARRAY(Decl*, method_count);
        if (decl->extension.methods) {
            memcpy(decl->extension.methods, methods, method_count * sizeof(Decl*));
        }
    } else {
        decl->extension.methods = NULL;
    }
    
    return decl;
}

Decl* decl_create_module(const char* name, const char* path) {
    Decl* decl = AST_NEW(Decl);
    if (!decl) return NULL;
    decl->type = DECL_MODULE;
    decl->module.name = AST_DUP(name);
    decl->module.path = AST_DUP(path);
    return decl;
}

// Type expression creation functions
TypeExpr* type_expr_create_name(const char* name) {
    TypeExpr* type = AST_NEW(TypeExpr);
    if (!type) return NULL;
    type->type = TYPE_EXPR_NAME;
    type->name.value = AST_DUP(name);
    return type;
}

TypeExpr* type_expr_create_array(TypeExpr* element_type) {
    TypeExpr* type = AST_NEW(TypeExpr);
    if (!type) return NULL;
    type->type = TYPE_EXPR_ARRAY;
    type->array.element_type = element_type;
    return type;
}

TypeExpr* type_expr_create_optional(TypeExpr* base_type) {
    TypeExpr* type = AST_NEW(TypeExpr);
    if (!type) return NULL;
    type->type = TYPE_EXPR_OPTIONAL;
    type->optional.base_type = base_type;
    return type;
}

TypeExpr* type_expr_create_function(TypeExpr** parameter_types, size_t parameter_count, 
                                   TypeExpr* return_type) {
    TypeExpr* type = AST_NEW(TypeExpr);
    if (!type) return NULL;
    type->type = TYPE_EXPR_FUNCTION;
    type->function.parameter_count = parameter_count;
    type->function.return_type = return_type;
    
    if (parameter_count > 0) {
        type->function.parameter_types = AST_NEW_ARRAY(TypeExpr*, parameter_count);
        if (type->function.parameter_types) {
            memcpy(type->function.parameter_types, parameter_types, parameter_count * sizeof(TypeExpr*));
        }
    } else {
        type->function.parameter_types = NULL;
    }
    
    return type;
}

TypeExpr* type_expr_create_object(void) {
    TypeExpr* type = AST_NEW(TypeExpr);
    if (!type) return NULL;
    type->type = TYPE_EXPR_OBJECT;
    return type;
}

// Note: All expr_free, stmt_free, decl_free, and type_expr_free functions
// are no longer needed with arena allocation - the arena is reset after compilation
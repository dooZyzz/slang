#include "ast/ast.h"
#include <stdlib.h>
#include <string.h>

Expr* expr_create_literal_nil(void) {
    Expr* expr = calloc(1, sizeof(Expr));
    expr->type = EXPR_LITERAL;
    expr->literal.type = LITERAL_NIL;
    return expr;
}

Expr* expr_create_literal_bool(bool value) {
    Expr* expr = calloc(1, sizeof(Expr));
    expr->type = EXPR_LITERAL;
    expr->literal.type = LITERAL_BOOL;
    expr->literal.value.boolean = value;
    return expr;
}

Expr* expr_create_literal_int(long long value) {
    Expr* expr = calloc(1, sizeof(Expr));
    expr->type = EXPR_LITERAL;
    expr->literal.type = LITERAL_INT;
    expr->literal.value.integer = value;
    return expr;
}

Expr* expr_create_literal_float(double value) {
    Expr* expr = calloc(1, sizeof(Expr));
    expr->type = EXPR_LITERAL;
    expr->literal.type = LITERAL_FLOAT;
    expr->literal.value.floating = value;
    return expr;
}

Expr* expr_create_literal_string(const char* value) {
    Expr* expr = calloc(1, sizeof(Expr));
    expr->type = EXPR_LITERAL;
    expr->literal.type = LITERAL_STRING;
    expr->literal.value.string.value = strdup(value);
    expr->literal.value.string.length = strlen(value);
    return expr;
}

Expr* expr_create_variable(const char* name) {
    Expr* expr = calloc(1, sizeof(Expr));
    expr->type = EXPR_VARIABLE;
    expr->variable.name = strdup(name);
    return expr;
}

Expr* expr_create_binary(Token operator, Expr* left, Expr* right) {
    Expr* expr = calloc(1, sizeof(Expr));
    expr->type = EXPR_BINARY;
    expr->binary.operator = operator;
    expr->binary.left = left;
    expr->binary.right = right;
    return expr;
}

Expr* expr_create_unary(Token operator, Expr* operand) {
    Expr* expr = calloc(1, sizeof(Expr));
    expr->type = EXPR_UNARY;
    expr->unary.operator = operator;
    expr->unary.operand = operand;
    return expr;
}

Expr* expr_create_assignment(Expr* target, Expr* value) {
    Expr* expr = calloc(1, sizeof(Expr));
    expr->type = EXPR_ASSIGNMENT;
    expr->assignment.target = target;
    expr->assignment.value = value;
    return expr;
}

Expr* expr_create_call(Expr* callee, Expr** arguments, size_t argument_count) {
    Expr* expr = calloc(1, sizeof(Expr));
    expr->type = EXPR_CALL;
    expr->call.callee = callee;
    expr->call.argument_count = argument_count;
    if (argument_count > 0) {
        expr->call.arguments = calloc(argument_count, sizeof(Expr*));
        memcpy(expr->call.arguments, arguments, argument_count * sizeof(Expr*));
    }
    return expr;
}

Expr* expr_create_subscript(Expr* object, Expr* index) {
    Expr* expr = calloc(1, sizeof(Expr));
    expr->type = EXPR_SUBSCRIPT;
    expr->subscript.object = object;
    expr->subscript.index = index;
    return expr;
}

Expr* expr_create_member(Expr* object, const char* property) {
    Expr* expr = calloc(1, sizeof(Expr));
    expr->type = EXPR_MEMBER;
    expr->member.object = object;
    expr->member.property = strdup(property);
    return expr;
}


Expr* expr_create_array_literal(Expr** elements, size_t element_count) {
    Expr* expr = calloc(1, sizeof(Expr));
    expr->type = EXPR_ARRAY_LITERAL;
    expr->array_literal.element_count = element_count;
    if (element_count > 0) {
        expr->array_literal.elements = calloc(element_count, sizeof(Expr*));
        memcpy(expr->array_literal.elements, elements, element_count * sizeof(Expr*));
    }
    return expr;
}

Expr* expr_create_object_literal(const char** keys, Expr** values, size_t pair_count) {
    Expr* expr = calloc(1, sizeof(Expr));
    expr->type = EXPR_OBJECT_LITERAL;
    expr->object_literal.pair_count = pair_count;
    if (pair_count > 0) {
        expr->object_literal.keys = calloc(pair_count, sizeof(char*));
        expr->object_literal.values = calloc(pair_count, sizeof(Expr*));
        for (size_t i = 0; i < pair_count; i++) {
            expr->object_literal.keys[i] = strdup(keys[i]);
        }
        memcpy(expr->object_literal.values, values, pair_count * sizeof(Expr*));
    }
    return expr;
}

Expr* expr_create_closure(const char** parameter_names, TypeExpr** parameter_types,
                         size_t parameter_count, TypeExpr* return_type, Stmt* body) {
    Expr* expr = calloc(1, sizeof(Expr));
    expr->type = EXPR_CLOSURE;
    expr->closure.parameter_names = parameter_names;
    expr->closure.parameter_types = parameter_types;
    expr->closure.parameter_count = parameter_count;
    expr->closure.return_type = return_type;
    expr->closure.body = body;
    return expr;
}

Expr* expr_create_string_interp(char** parts, Expr** expressions, size_t part_count, size_t expr_count) {
    Expr* expr = calloc(1, sizeof(Expr));
    expr->type = EXPR_STRING_INTERP;
    expr->string_interp.parts = parts;
    expr->string_interp.expressions = expressions;
    expr->string_interp.part_count = part_count;
    expr->string_interp.expr_count = expr_count;
    return expr;
}

Stmt* stmt_create_expression(Expr* expression) {
    Stmt* stmt = calloc(1, sizeof(Stmt));
    stmt->type = STMT_EXPRESSION;
    stmt->expression.expression = expression;
    return stmt;
}

Stmt* stmt_create_var_decl(bool is_mutable, const char* name, const char* type_annotation, Expr* initializer) {
    Stmt* stmt = calloc(1, sizeof(Stmt));
    stmt->type = STMT_VAR_DECL;
    stmt->var_decl.is_mutable = is_mutable;
    stmt->var_decl.name = strdup(name);
    if (type_annotation) {
        stmt->var_decl.type_annotation = strdup(type_annotation);
    }
    stmt->var_decl.initializer = initializer;
    return stmt;
}

Stmt* stmt_create_block(Stmt** statements, size_t statement_count) {
    Stmt* stmt = calloc(1, sizeof(Stmt));
    stmt->type = STMT_BLOCK;
    stmt->block.statement_count = statement_count;
    if (statement_count > 0) {
        stmt->block.statements = calloc(statement_count, sizeof(Stmt*));
        memcpy(stmt->block.statements, statements, statement_count * sizeof(Stmt*));
    }
    return stmt;
}

Stmt* stmt_create_if(Expr* condition, Stmt* then_branch, Stmt* else_branch) {
    Stmt* stmt = calloc(1, sizeof(Stmt));
    stmt->type = STMT_IF;
    stmt->if_stmt.condition = condition;
    stmt->if_stmt.then_branch = then_branch;
    stmt->if_stmt.else_branch = else_branch;
    return stmt;
}

Stmt* stmt_create_while(Expr* condition, Stmt* body) {
    Stmt* stmt = calloc(1, sizeof(Stmt));
    stmt->type = STMT_WHILE;
    stmt->while_stmt.condition = condition;
    stmt->while_stmt.body = body;
    return stmt;
}

Stmt* stmt_create_for_in(const char* variable_name, Expr* iterable, Stmt* body) {
    Stmt* stmt = calloc(1, sizeof(Stmt));
    stmt->type = STMT_FOR_IN;
    stmt->for_in.variable_name = strdup(variable_name);
    stmt->for_in.iterable = iterable;
    stmt->for_in.body = body;
    return stmt;
}

Stmt* stmt_create_for(Stmt* initializer, Expr* condition, Expr* increment, Stmt* body) {
    Stmt* stmt = calloc(1, sizeof(Stmt));
    stmt->type = STMT_FOR;
    stmt->for_stmt.initializer = initializer;
    stmt->for_stmt.condition = condition;
    stmt->for_stmt.increment = increment;
    stmt->for_stmt.body = body;
    return stmt;
}

Stmt* stmt_create_return(Expr* expression) {
    Stmt* stmt = calloc(1, sizeof(Stmt));
    stmt->type = STMT_RETURN;
    stmt->return_stmt.expression = expression;
    return stmt;
}

Stmt* stmt_create_break(void) {
    Stmt* stmt = calloc(1, sizeof(Stmt));
    stmt->type = STMT_BREAK;
    return stmt;
}

Stmt* stmt_create_continue(void) {
    Stmt* stmt = calloc(1, sizeof(Stmt));
    stmt->type = STMT_CONTINUE;
    return stmt;
}

Stmt* stmt_create_function(const char* name, const char** parameter_names, const char** parameter_types,
                          size_t parameter_count, const char* return_type, Stmt* body) {
    Stmt* stmt = calloc(1, sizeof(Stmt));
    stmt->type = STMT_FUNCTION;
    stmt->function.name = strdup(name);
    stmt->function.parameter_count = parameter_count;

    if (parameter_count > 0) {
        stmt->function.parameter_names = calloc(parameter_count, sizeof(char*));
        stmt->function.parameter_types = calloc(parameter_count, sizeof(char*));
        for (size_t i = 0; i < parameter_count; i++) {
            stmt->function.parameter_names[i] = strdup(parameter_names[i]);
            if (parameter_types[i]) {
                stmt->function.parameter_types[i] = strdup(parameter_types[i]);
            }
        }
    }

    if (return_type) {
        stmt->function.return_type = strdup(return_type);
    }
    stmt->function.body = body;
    stmt->function.is_async = false;
    stmt->function.is_throwing = false;
    stmt->function.is_mutating = false;
    return stmt;
}

Stmt* stmt_create_class(const char* name, const char* superclass, Stmt** members, size_t member_count) {
    Stmt* stmt = calloc(1, sizeof(Stmt));
    stmt->type = STMT_CLASS;
    stmt->class_decl.name = strdup(name);
    stmt->class_decl.superclass = superclass ? strdup(superclass) : NULL;
    stmt->class_decl.members = members;
    stmt->class_decl.member_count = member_count;
    return stmt;
}

Decl* decl_create_function(const char* name, const char** parameter_names, const char** parameter_types,
                          size_t parameter_count, const char* return_type, Stmt* body) {
    Decl* decl = calloc(1, sizeof(Decl));
    decl->type = DECL_FUNCTION;
    decl->function.name = strdup(name);
    decl->function.parameter_count = parameter_count;

    if (parameter_count > 0) {
        decl->function.parameter_names = calloc(parameter_count, sizeof(char*));
        decl->function.parameter_types = calloc(parameter_count, sizeof(char*));
        for (size_t i = 0; i < parameter_count; i++) {
            decl->function.parameter_names[i] = strdup(parameter_names[i]);
            decl->function.parameter_types[i] = strdup(parameter_types[i]);
        }
    }

    if (return_type) {
        decl->function.return_type = strdup(return_type);
    }
    decl->function.body = body;
    return decl;
}

Stmt* stmt_create_import(ImportType type, const char* module_path) {
    Stmt* stmt = calloc(1, sizeof(Stmt));
    stmt->type = STMT_IMPORT;
    stmt->import_decl.type = type;
    stmt->import_decl.module_path = strdup(module_path);
    stmt->import_decl.namespace_alias = NULL;
    stmt->import_decl.default_name = NULL;
    stmt->import_decl.specifiers = NULL;
    stmt->import_decl.specifier_count = 0;
    stmt->import_decl.is_native = false;
    stmt->import_decl.is_local = false;
    stmt->import_decl.alias = NULL;
    return stmt;
}

Stmt* stmt_create_export(ExportType type) {
    Stmt* stmt = calloc(1, sizeof(Stmt));
    stmt->type = STMT_EXPORT;
    stmt->export_decl.type = type;
    // Initialize the union fields based on type
    switch (type) {
        case EXPORT_DEFAULT:
            stmt->export_decl.default_export.name = NULL;
            break;
        case EXPORT_NAMED:
            stmt->export_decl.named_export.specifiers = NULL;
            stmt->export_decl.named_export.specifier_count = 0;
            stmt->export_decl.named_export.from_module = NULL;
            break;
        case EXPORT_ALL:
            stmt->export_decl.all_export.from_module = NULL;
            break;
        case EXPORT_DECLARATION:
            stmt->export_decl.decl_export.declaration = NULL;
            break;
    }
    return stmt;
}

ProgramNode* program_create(Stmt** statements, size_t statement_count) {
    ProgramNode* program = calloc(1, sizeof(ProgramNode));
    program->statement_count = statement_count;
    if (statement_count > 0) {
        program->statements = calloc(statement_count, sizeof(Stmt*));
        memcpy(program->statements, statements, statement_count * sizeof(Stmt*));
    }
    return program;
}

void expr_destroy(Expr* expr) {
    if (!expr) return;

    switch (expr->type) {
        case EXPR_LITERAL:
            if (expr->literal.type == LITERAL_STRING) {
                free(expr->literal.value.string.value);
            }
            break;

        case EXPR_VARIABLE:
            free((void*)expr->variable.name);
            break;

        case EXPR_BINARY:
            expr_destroy(expr->binary.left);
            expr_destroy(expr->binary.right);
            break;

        case EXPR_UNARY:
            expr_destroy(expr->unary.operand);
            break;

        case EXPR_ASSIGNMENT:
            expr_destroy(expr->assignment.target);
            expr_destroy(expr->assignment.value);
            break;

        case EXPR_CALL:
            expr_destroy(expr->call.callee);
            for (size_t i = 0; i < expr->call.argument_count; i++) {
                expr_destroy(expr->call.arguments[i]);
            }
            free(expr->call.arguments);
            break;

        case EXPR_ARRAY_LITERAL:
            for (size_t i = 0; i < expr->array_literal.element_count; i++) {
                expr_destroy(expr->array_literal.elements[i]);
            }
            free(expr->array_literal.elements);
            break;

        default:
            break;
    }

    free(expr);
}

void stmt_destroy(Stmt* stmt) {
    if (!stmt) return;

    switch (stmt->type) {
        case STMT_EXPRESSION:
            expr_destroy(stmt->expression.expression);
            break;

        case STMT_VAR_DECL:
            free((void*)stmt->var_decl.name);
            free((void*)stmt->var_decl.type_annotation);
            expr_destroy(stmt->var_decl.initializer);
            break;

        case STMT_BLOCK:
            for (size_t i = 0; i < stmt->block.statement_count; i++) {
                stmt_destroy(stmt->block.statements[i]);
            }
            free(stmt->block.statements);
            break;

        case STMT_IF:
            expr_destroy(stmt->if_stmt.condition);
            stmt_destroy(stmt->if_stmt.then_branch);
            stmt_destroy(stmt->if_stmt.else_branch);
            break;

        case STMT_WHILE:
            expr_destroy(stmt->while_stmt.condition);
            stmt_destroy(stmt->while_stmt.body);
            break;

        case STMT_FOR_IN:
            free((void*)stmt->for_in.variable_name);
            expr_destroy(stmt->for_in.iterable);
            stmt_destroy(stmt->for_in.body);
            break;

        case STMT_FOR:
            stmt_destroy(stmt->for_stmt.initializer);
            expr_destroy(stmt->for_stmt.condition);
            expr_destroy(stmt->for_stmt.increment);
            stmt_destroy(stmt->for_stmt.body);
            break;

        case STMT_RETURN:
            expr_destroy(stmt->return_stmt.expression);
            break;

        case STMT_DEFER:
            stmt_destroy(stmt->defer_stmt.statement);
            break;

        case STMT_FUNCTION:
            free((void*)stmt->function.name);
            for (size_t i = 0; i < stmt->function.parameter_count; i++) {
                free((void*)stmt->function.parameter_names[i]);
                free((void*)stmt->function.parameter_types[i]);
            }
            free((void*)stmt->function.parameter_names);
            free((void*)stmt->function.parameter_types);
            free((void*)stmt->function.return_type);
            stmt_destroy(stmt->function.body);
            break;

        case STMT_IMPORT:
            free((void*)stmt->import_decl.module_path);
            free((void*)stmt->import_decl.namespace_alias);
            free((void*)stmt->import_decl.default_name);
            for (size_t i = 0; i < stmt->import_decl.specifier_count; i++) {
                free((void*)stmt->import_decl.specifiers[i].name);
                free((void*)stmt->import_decl.specifiers[i].alias);
            }
            free(stmt->import_decl.specifiers);
            break;

        case STMT_EXPORT:
            switch (stmt->export_decl.type) {
                case EXPORT_DEFAULT:
                    free((void*)stmt->export_decl.default_export.name);
                    break;
                case EXPORT_NAMED:
                    for (size_t i = 0; i < stmt->export_decl.named_export.specifier_count; i++) {
                        free((void*)stmt->export_decl.named_export.specifiers[i].name);
                        free((void*)stmt->export_decl.named_export.specifiers[i].alias);
                    }
                    free(stmt->export_decl.named_export.specifiers);
                    free((void*)stmt->export_decl.named_export.from_module);
                    break;
                case EXPORT_ALL:
                    free((void*)stmt->export_decl.all_export.from_module);
                    break;
                case EXPORT_DECLARATION:
                    decl_destroy(stmt->export_decl.decl_export.declaration);
                    break;
            }
            break;

        default:
            break;
    }

    free(stmt);
}

void decl_destroy(Decl* decl) {
    if (!decl) return;

    switch (decl->type) {
        case DECL_FUNCTION:
            free((void*)decl->function.name);
            for (size_t i = 0; i < decl->function.parameter_count; i++) {
                free((void*)decl->function.parameter_names[i]);
                free((void*)decl->function.parameter_types[i]);
            }
            free(decl->function.parameter_names);
            free(decl->function.parameter_types);
            free((void*)decl->function.return_type);
            stmt_destroy(decl->function.body);
            break;

        default:
            break;
    }

    free(decl);
}

void program_destroy(ProgramNode* program) {
    if (!program) return;

    for (size_t i = 0; i < program->statement_count; i++) {
        stmt_destroy(program->statements[i]);
    }
    free(program->statements);
    free(program);
}

// Alias for compatibility with existing code that expects ast_free_program
void ast_free_program(ProgramNode* program) {
    program_destroy(program);
}



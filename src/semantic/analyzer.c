#include "semantic/analyzer.h"
#include "runtime/modules/lifecycle/builtin_modules.h"
#include <stdlib.h>
#include <string.h>

struct SemanticAnalyzer {
    SemanticContext context;
    ASTVisitor* visitor;
};

static void* visit_binary_expr(ASTVisitor* visitor, Expr* expr);
static void* visit_unary_expr(ASTVisitor* visitor, Expr* expr);
static void* visit_literal_expr(ASTVisitor* visitor, Expr* expr);
static void* visit_variable_expr(ASTVisitor* visitor, Expr* expr);
static void* visit_assignment_expr(ASTVisitor* visitor, Expr* expr);
static void* visit_call_expr(ASTVisitor* visitor, Expr* expr);
static void* visit_array_literal_expr(ASTVisitor* visitor, Expr* expr);
static void* visit_closure_expr(ASTVisitor* visitor, Expr* expr);

static void* visit_expression_stmt(ASTVisitor* visitor, Stmt* stmt);
static void* visit_var_decl_stmt(ASTVisitor* visitor, Stmt* stmt);
static void* visit_block_stmt(ASTVisitor* visitor, Stmt* stmt);
static void* visit_if_stmt(ASTVisitor* visitor, Stmt* stmt);
static void* visit_while_stmt(ASTVisitor* visitor, Stmt* stmt);
static void* visit_for_in_stmt(ASTVisitor* visitor, Stmt* stmt);
static void* visit_for_stmt(ASTVisitor* visitor, Stmt* stmt);
static void* visit_return_stmt(ASTVisitor* visitor, Stmt* stmt);
static void* visit_break_stmt(ASTVisitor* visitor, Stmt* stmt);
static void* visit_continue_stmt(ASTVisitor* visitor, Stmt* stmt);
static void* visit_function_stmt(ASTVisitor* visitor, Stmt* stmt);
static void* visit_import_stmt(ASTVisitor* visitor, Stmt* stmt);
static void* visit_export_stmt(ASTVisitor* visitor, Stmt* stmt);
static void* visit_subscript_expr(ASTVisitor* visitor, Expr* expr);
static void* visit_member_expr(ASTVisitor* visitor, Expr* expr);
static void* visit_string_interp_expr(ASTVisitor* visitor, Expr* expr);

static void* visit_function_decl(ASTVisitor* visitor, Decl* decl);

static void enter_scope(ASTVisitor* visitor);
static void exit_scope(ASTVisitor* visitor);

SemanticAnalyzer* semantic_analyzer_create(ErrorReporter* errors) {
    SemanticAnalyzer* analyzer = calloc(1, sizeof(SemanticAnalyzer));

    analyzer->context.symbols = symbol_table_create();
    analyzer->context.types = type_context_create();
    analyzer->context.errors = errors;

    type_context_register_builtin_types(analyzer->context.types);

    analyzer->visitor = visitor_create();
    analyzer->visitor->context = &analyzer->context;

    analyzer->visitor->visit_binary_expr = visit_binary_expr;
    analyzer->visitor->visit_unary_expr = visit_unary_expr;
    analyzer->visitor->visit_literal_expr = visit_literal_expr;
    analyzer->visitor->visit_variable_expr = visit_variable_expr;
    analyzer->visitor->visit_assignment_expr = visit_assignment_expr;
    analyzer->visitor->visit_call_expr = visit_call_expr;
    analyzer->visitor->visit_array_literal_expr = visit_array_literal_expr;
    analyzer->visitor->visit_closure_expr = visit_closure_expr;

    analyzer->visitor->visit_expression_stmt = visit_expression_stmt;
    analyzer->visitor->visit_var_decl_stmt = visit_var_decl_stmt;
    analyzer->visitor->visit_block_stmt = visit_block_stmt;
    analyzer->visitor->visit_if_stmt = visit_if_stmt;
    analyzer->visitor->visit_while_stmt = visit_while_stmt;
    analyzer->visitor->visit_for_in_stmt = visit_for_in_stmt;
    analyzer->visitor->visit_for_stmt = visit_for_stmt;
    analyzer->visitor->visit_return_stmt = visit_return_stmt;
    analyzer->visitor->visit_break_stmt = visit_break_stmt;
    analyzer->visitor->visit_continue_stmt = visit_continue_stmt;
    analyzer->visitor->visit_function_stmt = visit_function_stmt;
    analyzer->visitor->visit_import_stmt = visit_import_stmt;
    analyzer->visitor->visit_export_stmt = visit_export_stmt;
    analyzer->visitor->visit_subscript_expr = visit_subscript_expr;
    analyzer->visitor->visit_member_expr = visit_member_expr;
    analyzer->visitor->visit_string_interp_expr = visit_string_interp_expr;

    analyzer->visitor->visit_function_decl = visit_function_decl;

    analyzer->visitor->enter_scope = enter_scope;
    analyzer->visitor->exit_scope = exit_scope;

    return analyzer;
}

void semantic_analyzer_destroy(SemanticAnalyzer* analyzer) {
    if (!analyzer) return;

    visitor_destroy(analyzer->visitor);
    symbol_table_destroy(analyzer->context.symbols);
    type_context_destroy(analyzer->context.types);
    free(analyzer);
}

bool semantic_analyze(SemanticAnalyzer* analyzer, ProgramNode* program) {
    if (!analyzer || !program) return false;

    ast_visit_program(program, analyzer->visitor);

    semantic_analyzer_check_uninitialized_variables(analyzer);
    semantic_analyzer_check_unused_variables(analyzer);

    return !error_has_errors(analyzer->context.errors);
}

SymbolTable* semantic_analyzer_get_symbols(SemanticAnalyzer* analyzer) {
    return analyzer ? analyzer->context.symbols : NULL;
}

TypeContext* semantic_analyzer_get_types(SemanticAnalyzer* analyzer) {
    return analyzer ? analyzer->context.types : NULL;
}

static SemanticContext* get_context(ASTVisitor* visitor) {
    return (SemanticContext*)visitor->context;
}

static Type* infer_literal_type(LiteralExpr* lit) {
    switch (lit->type) {
        case LITERAL_NIL: return type_nil();
        case LITERAL_BOOL: return type_bool();
        case LITERAL_INT: return type_int();
        case LITERAL_FLOAT: return type_float();
        case LITERAL_STRING: return type_string();
    }
    return NULL;
}

static void* visit_literal_expr(ASTVisitor* visitor, Expr* expr) {
    (void)visitor; // Silence unused warning
    LiteralExpr* lit = &expr->literal;
    Type* type = infer_literal_type(lit);
    expr->computed_type = type;
    return type;
}

static void* visit_variable_expr(ASTVisitor* visitor, Expr* expr) {
    SemanticContext* ctx = get_context(visitor);
    VariableExpr* var = &expr->variable;

    Symbol* symbol = symbol_lookup(ctx->symbols, var->name);
    if (!symbol) {
        error_report_simple(ctx->errors, ERROR_LEVEL_ERROR, ERROR_PHASE_SEMANTIC,
                           var->name, 0, 0, "Undefined variable '%s'", var->name);
        expr->computed_type = type_unresolved(var->name);
        return expr->computed_type;
    }

    if ((symbol->kind == SYMBOL_VARIABLE || symbol->kind == SYMBOL_PARAMETER) &&
        !symbol->is_initialized) {
        error_report_simple(ctx->errors, ERROR_LEVEL_ERROR, ERROR_PHASE_SEMANTIC,
                           var->name, 0, 0, "Variable '%s' used before initialization", var->name);
    }

    symbol_mark_used(symbol);
    expr->computed_type = symbol->type;
    return symbol->type;
}

static void* visit_binary_expr(ASTVisitor* visitor, Expr* expr) {
    SemanticContext* ctx = get_context(visitor);
    BinaryExpr* bin = &expr->binary;

    Type* left_type = ast_accept_expr(bin->left, visitor);
    Type* right_type = ast_accept_expr(bin->right, visitor);

    if (!left_type || !right_type) {
        expr->computed_type = type_unresolved("unknown");
        return expr->computed_type;
    }

    switch (bin->operator.type) {
        case TOKEN_PLUS:
            // Handle string concatenation
            if (left_type->kind == TYPE_KIND_STRING && right_type->kind == TYPE_KIND_STRING) {
                expr->computed_type = type_string();
                return expr->computed_type;
            }
            // For numeric addition, be more permissive
            // Allow if both are numeric, or if either is ANY, or if both are unresolved
            if ((type_is_numeric(left_type) && type_is_numeric(right_type)) ||
                left_type->kind == TYPE_KIND_ANY || right_type->kind == TYPE_KIND_ANY ||
                left_type->kind == TYPE_KIND_UNRESOLVED || right_type->kind == TYPE_KIND_UNRESOLVED) {

                // Try to determine the result type
                if (left_type->kind == TYPE_KIND_ANY || right_type->kind == TYPE_KIND_ANY) {
                    expr->computed_type = type_any();
                } else if (left_type->kind == TYPE_KIND_UNRESOLVED || right_type->kind == TYPE_KIND_UNRESOLVED) {
                    expr->computed_type = type_any();
                } else {
                    expr->computed_type = type_common_type(left_type, right_type);
                }
                return expr->computed_type;
            }

            // Only error if we're certain the types are incompatible
            error_report_simple(ctx->errors, ERROR_LEVEL_ERROR, ERROR_PHASE_SEMANTIC,
                               "", 0, 0, "Arithmetic operations require numeric types");
            expr->computed_type = type_unresolved("error");
            return expr->computed_type;

        case TOKEN_MINUS:
        case TOKEN_STAR:
        case TOKEN_SLASH:
        case TOKEN_PERCENT:
            // For other arithmetic operations, use same permissive logic
            if ((type_is_numeric(left_type) && type_is_numeric(right_type)) ||
                left_type->kind == TYPE_KIND_ANY || right_type->kind == TYPE_KIND_ANY ||
                left_type->kind == TYPE_KIND_UNRESOLVED || right_type->kind == TYPE_KIND_UNRESOLVED) {

                if (left_type->kind == TYPE_KIND_ANY || right_type->kind == TYPE_KIND_ANY) {
                    expr->computed_type = type_any();
                } else if (left_type->kind == TYPE_KIND_UNRESOLVED || right_type->kind == TYPE_KIND_UNRESOLVED) {
                    expr->computed_type = type_any();
                } else {
                    expr->computed_type = type_common_type(left_type, right_type);
                }
                return expr->computed_type;
            }

            error_report_simple(ctx->errors, ERROR_LEVEL_ERROR, ERROR_PHASE_SEMANTIC,
                               "", 0, 0, "Arithmetic operations require numeric types");
            expr->computed_type = type_unresolved("error");
            return expr->computed_type;

        case TOKEN_EQUAL_EQUAL:
        case TOKEN_NOT_EQUAL:
        case TOKEN_GREATER:
        case TOKEN_GREATER_EQUAL:
        case TOKEN_LESS:
        case TOKEN_LESS_EQUAL:
            expr->computed_type = type_bool();
            return expr->computed_type;

        case TOKEN_AND_AND:
        case TOKEN_OR_OR:
            // For logical operations, be permissive with ANY type
            if ((left_type->kind != TYPE_KIND_BOOL || right_type->kind != TYPE_KIND_BOOL) &&
                left_type->kind != TYPE_KIND_ANY && right_type->kind != TYPE_KIND_ANY) {
                error_report_simple(ctx->errors, ERROR_LEVEL_ERROR, ERROR_PHASE_SEMANTIC,
                                   "", 0, 0, "Logical operations require Bool types");
            }
            expr->computed_type = type_bool();
            return expr->computed_type;

        default:
            expr->computed_type = type_unresolved("unknown");
            return expr->computed_type;
    }
}

static void* visit_unary_expr(ASTVisitor* visitor, Expr* expr) {
    SemanticContext* ctx = get_context(visitor);
    UnaryExpr* unary = &expr->unary;

    Type* operand_type = ast_accept_expr(unary->operand, visitor);
    if (!operand_type) {
        expr->computed_type = type_unresolved("unknown");
        return expr->computed_type;
    }

    switch (unary->operator.type) {
        case TOKEN_MINUS:
            if (!type_is_numeric(operand_type)) {
                error_report_simple(ctx->errors, ERROR_LEVEL_ERROR, ERROR_PHASE_SEMANTIC,
                                   "", 0, 0, "Unary minus requires numeric type");
            }
            expr->computed_type = operand_type;
            return expr->computed_type;

        case TOKEN_NOT:
            if (operand_type->kind != TYPE_KIND_BOOL) {
                error_report_simple(ctx->errors, ERROR_LEVEL_ERROR, ERROR_PHASE_SEMANTIC,
                                   "", 0, 0, "Logical NOT requires Bool type");
            }
            expr->computed_type = type_bool();
            return expr->computed_type;

        default:
            expr->computed_type = type_unresolved("unknown");
            return expr->computed_type;
    }
}

static void* visit_assignment_expr(ASTVisitor* visitor, Expr* expr) {
    SemanticContext* ctx = get_context(visitor);
    AssignmentExpr* assign = &expr->assignment;

    // First analyze the value being assigned
    Type* value_type = ast_accept_expr(assign->value, visitor);

    if (assign->target->type == EXPR_VARIABLE) {
        // Variable assignment
        VariableExpr* var = &assign->target->variable;
        Symbol* symbol = symbol_lookup(ctx->symbols, var->name);
        if (!symbol) {
            error_report_simple(ctx->errors, ERROR_LEVEL_ERROR, ERROR_PHASE_SEMANTIC,
                               var->name, 0, 0, "Undefined variable '%s'", var->name);
            expr->computed_type = type_unresolved(var->name);
            return expr->computed_type;
        }

        if (!symbol->is_mutable) {
            error_report_simple(ctx->errors, ERROR_LEVEL_ERROR, ERROR_PHASE_SEMANTIC,
                               var->name, 0, 0, "Cannot assign to immutable variable '%s'", var->name);
        }

        if (value_type && !type_is_assignable(value_type, symbol->type)) {
            error_report_simple(ctx->errors, ERROR_LEVEL_ERROR, ERROR_PHASE_SEMANTIC,
                               "", 0, 0, "Cannot assign value of type '%s' to variable of type '%s'",
                               type_to_string(value_type), type_to_string(symbol->type));
        }

        symbol_mark_initialized(symbol);
        expr->computed_type = symbol->type;
        return symbol->type;

    } else if (assign->target->type == EXPR_SUBSCRIPT) {
        // Array element assignment
        SubscriptExpr* subscript = &assign->target->subscript;

        // Analyze the array expression
        Type* array_type = ast_accept_expr(subscript->object, visitor);
        Type* index_type = ast_accept_expr(subscript->index, visitor);

        // Check that we're subscripting an array
        if (array_type && array_type->kind != TYPE_KIND_ARRAY && array_type->kind != TYPE_KIND_ANY) {
            error_report_simple(ctx->errors, ERROR_LEVEL_ERROR, ERROR_PHASE_SEMANTIC,
                               "", 0, 0, "Can only subscript arrays");
        }

        // Check that index is numeric
        if (index_type && !type_is_numeric(index_type) && index_type->kind != TYPE_KIND_ANY) {
            error_report_simple(ctx->errors, ERROR_LEVEL_ERROR, ERROR_PHASE_SEMANTIC,
                               "", 0, 0, "Array index must be numeric");
        }

        // The result type is the element type
        if (array_type && array_type->kind == TYPE_KIND_ARRAY) {
            Type* element_type = array_type->data.array->element_type;
            if (value_type && !type_is_assignable(value_type, element_type)) {
                error_report_simple(ctx->errors, ERROR_LEVEL_ERROR, ERROR_PHASE_SEMANTIC,
                                   "", 0, 0, "Cannot assign value of type '%s' to array element of type '%s'",
                                   type_to_string(value_type), type_to_string(element_type));
            }
            expr->computed_type = element_type;
            return element_type;
        } else {
            expr->computed_type = type_any();
            return type_any();
        }

    } else if (assign->target->type == EXPR_MEMBER) {
        // Member assignment
        MemberExpr* member = &assign->target->member;

        // Analyze the object expression
        ast_accept_expr(member->object, visitor);

        // For now, we'll accept any member assignment
        // In a full implementation, you'd check the object's type for the member
        expr->computed_type = value_type;
        return value_type;

    } else {
        error_report_simple(ctx->errors, ERROR_LEVEL_ERROR, ERROR_PHASE_SEMANTIC,
                           "", 0, 0, "Invalid assignment target");
        expr->computed_type = type_unresolved("error");
        return expr->computed_type;
    }
}

static void* visit_subscript_expr(ASTVisitor* visitor, Expr* expr) {
    SemanticContext* ctx = get_context(visitor);
    SubscriptExpr* subscript = &expr->subscript;

    // Analyze the array expression
    Type* array_type = ast_accept_expr(subscript->object, visitor);
    Type* index_type = ast_accept_expr(subscript->index, visitor);

    // Check that we're subscripting an array
    if (array_type && array_type->kind != TYPE_KIND_ARRAY && array_type->kind != TYPE_KIND_ANY) {
        error_report_simple(ctx->errors, ERROR_LEVEL_ERROR, ERROR_PHASE_SEMANTIC,
                           "", 0, 0, "Can only subscript arrays");
        expr->computed_type = type_unresolved("error");
        return expr->computed_type;
    }

    // Check that index is numeric
    if (index_type && !type_is_numeric(index_type) && index_type->kind != TYPE_KIND_ANY) {
        error_report_simple(ctx->errors, ERROR_LEVEL_ERROR, ERROR_PHASE_SEMANTIC,
                           "", 0, 0, "Array index must be numeric");
    }

    // The result type is the element type
    if (array_type && array_type->kind == TYPE_KIND_ARRAY) {
        expr->computed_type = array_type->data.array->element_type;
        return array_type->data.array->element_type;
    } else {
        // If it's ANY type or unresolved, return ANY
        expr->computed_type = type_any();
        return type_any();
    }
}

static void* visit_member_expr(ASTVisitor* visitor, Expr* expr) {
    SemanticContext* ctx = get_context(visitor);
    (void)ctx; // Silence unused warning
    MemberExpr* member = &expr->member;
    
    // Analyze the object expression
    Type* object_type = ast_accept_expr(member->object, visitor);
    
    // For now, we'll handle array properties specially
    if (object_type && object_type->kind == TYPE_KIND_ARRAY) {
        if (strcmp(member->property, "length") == 0) {
            // length property returns an integer
            expr->computed_type = type_int();
            return type_int();
        } else if (strcmp(member->property, "push") == 0 ||
                   strcmp(member->property, "pop") == 0 ||
                   strcmp(member->property, "map") == 0 ||
                   strcmp(member->property, "filter") == 0 ||
                   strcmp(member->property, "reduce") == 0) {
            // These are methods that return functions
            // For now, we'll return a generic function type
            expr->computed_type = type_function(NULL, 0, type_any());
            return expr->computed_type;
        }
    }
    
    // For any other cases, return ANY type
    expr->computed_type = type_any();
    return type_any();
}

static void* visit_string_interp_expr(ASTVisitor* visitor, Expr* expr) {
    StringInterpExpr* interp = &expr->string_interp;
    
    // Analyze all interpolated expressions
    for (size_t i = 0; i < interp->expr_count; i++) {
        ast_accept_expr(interp->expressions[i], visitor);
        // All expressions can be converted to string, so we don't need to check types
    }
    
    // String interpolation always produces a string
    expr->computed_type = type_string();
    return type_string();
}

static void* visit_call_expr(ASTVisitor* visitor, Expr* expr) {
    SemanticContext* ctx = get_context(visitor);
    CallExpr* call = &expr->call;

    Type* callee_type = ast_accept_expr(call->callee, visitor);
    
    // Allow ANY types and treat them as callable
    if (callee_type && callee_type->kind == TYPE_KIND_ANY) {
        // Accept all arguments without type checking
        for (size_t i = 0; i < call->argument_count; i++) {
            ast_accept_expr(call->arguments[i], visitor);
        }
        expr->computed_type = type_any();
        return type_any();
    }
    
    if (!callee_type || callee_type->kind != TYPE_KIND_FUNCTION) {
        error_report_simple(ctx->errors, ERROR_LEVEL_ERROR, ERROR_PHASE_SEMANTIC,
                           "", 0, 0, "Expression is not callable");
        expr->computed_type = type_unresolved("error");
        return expr->computed_type;
    }

    FunctionType* func_type = callee_type->data.function;
    
    // Skip argument count checking if the return type is ANY (built-in functions)
    bool skip_arg_check = (func_type->return_type && func_type->return_type->kind == TYPE_KIND_ANY);
    
    if (!skip_arg_check && call->argument_count != func_type->parameter_count) {
        error_report_simple(ctx->errors, ERROR_LEVEL_ERROR, ERROR_PHASE_SEMANTIC,
                           "", 0, 0, "Function expects %zu arguments, got %zu",
                           func_type->parameter_count, call->argument_count);
    }

    // Type check arguments
    if (skip_arg_check) {
        // Just analyze all arguments without type checking
        for (size_t i = 0; i < call->argument_count; i++) {
            ast_accept_expr(call->arguments[i], visitor);
        }
    } else {
        for (size_t i = 0; i < call->argument_count && i < func_type->parameter_count; i++) {
            Type* arg_type = ast_accept_expr(call->arguments[i], visitor);
            if (arg_type && !type_is_assignable(arg_type, func_type->parameter_types[i])) {
                error_report_simple(ctx->errors, ERROR_LEVEL_ERROR, ERROR_PHASE_SEMANTIC,
                                   "", 0, 0, "Argument %zu: cannot convert '%s' to '%s'",
                                   i + 1, type_to_string(arg_type),
                                   type_to_string(func_type->parameter_types[i]));
            }
        }
    }

    expr->computed_type = func_type->return_type;
    return func_type->return_type;
}

static void* visit_array_literal_expr(ASTVisitor* visitor, Expr* expr) {
    ArrayLiteralExpr* array = &expr->array_literal;

    if (array->element_count == 0) {
        expr->computed_type = type_array(type_any());
        return expr->computed_type;
    }

    Type* element_type = ast_accept_expr(array->elements[0], visitor);
    for (size_t i = 1; i < array->element_count; i++) {
        Type* elem_type = ast_accept_expr(array->elements[i], visitor);
        if (elem_type) {
            element_type = type_common_type(element_type, elem_type);
        }
    }

    expr->computed_type = type_array(element_type);
    return expr->computed_type;
}

static void* visit_closure_expr(ASTVisitor* visitor, Expr* expr) {
    SemanticContext* ctx = get_context(visitor);
    ClosureExpr* closure = &expr->closure;
    
    // Enter new scope for parameters
    enter_scope(visitor);
    
    // Process parameters
    Type** param_types = NULL;
    if (closure->parameter_count > 0) {
        param_types = malloc(closure->parameter_count * sizeof(Type*));
        for (size_t i = 0; i < closure->parameter_count; i++) {
            // For now, parameters have 'any' type if not specified
            Type* param_type = type_any();
            if (closure->parameter_types && closure->parameter_types[i]) {
                // TODO: Process type annotation when we have type expression support
            }
            param_types[i] = param_type;
            
            // Add parameter to symbol table
            Symbol* param = symbol_declare(ctx->symbols, closure->parameter_names[i],
                                         SYMBOL_VARIABLE, param_type, NULL);
            if (param) {
                param->is_mutable = false;
                symbol_mark_initialized(param);
            }
        }
    }
    
    // Analyze body
    Type* old_return_type = ctx->current_function_return_type;
    ctx->current_function_return_type = type_any();  // TODO: Infer return type
    
    bool old_in_function = ctx->in_function;
    ctx->in_function = true;
    
    ast_accept_stmt(closure->body, visitor);
    
    ctx->in_function = old_in_function;
    ctx->current_function_return_type = old_return_type;
    
    // Exit parameter scope
    exit_scope(visitor);
    
    // Create function type
    Type* return_type = type_void();  // TODO: Infer actual return type
    expr->computed_type = type_function(param_types, closure->parameter_count, return_type);
    
    if (param_types) free(param_types);
    
    return expr->computed_type;
}

static void* visit_expression_stmt(ASTVisitor* visitor, Stmt* stmt) {
    ast_accept_expr(stmt->expression.expression, visitor);
    return NULL;
}

static void* visit_var_decl_stmt(ASTVisitor* visitor, Stmt* stmt) {
    SemanticContext* ctx = get_context(visitor);
    VarDeclStmt* var_decl = &stmt->var_decl;

    Type* declared_type = NULL;
    if (var_decl->type_annotation) {
        declared_type = type_context_get(ctx->types, var_decl->type_annotation);
        if (!declared_type) {
            declared_type = type_unresolved(var_decl->type_annotation);
            error_report_simple(ctx->errors, ERROR_LEVEL_ERROR, ERROR_PHASE_SEMANTIC,
                               "", 0, 0, "Unknown type '%s'", var_decl->type_annotation);
        }
    }

    Type* init_type = NULL;
    if (var_decl->initializer) {
        init_type = ast_accept_expr(var_decl->initializer, visitor);
    }

    Type* final_type = declared_type;
    if (!final_type) {
        if (init_type) {
            final_type = init_type;
        } else {
            error_report_simple(ctx->errors, ERROR_LEVEL_ERROR, ERROR_PHASE_SEMANTIC,
                               var_decl->name, 0, 0, "Variable '%s' requires type annotation or initializer",
                               var_decl->name);
            final_type = type_unresolved(var_decl->name);
        }
    } else if (init_type && !type_is_assignable(init_type, final_type)) {
        error_report_simple(ctx->errors, ERROR_LEVEL_ERROR, ERROR_PHASE_SEMANTIC,
                           var_decl->name, 0, 0, "Cannot initialize variable of type '%s' with value of type '%s'",
                           type_to_string(final_type), type_to_string(init_type));
    }

    Symbol* symbol = symbol_declare(ctx->symbols, var_decl->name, SYMBOL_VARIABLE,
                                   final_type, NULL);
    if (!symbol) {
        error_report_simple(ctx->errors, ERROR_LEVEL_ERROR, ERROR_PHASE_SEMANTIC,
                           var_decl->name, 0, 0, "Variable '%s' already declared in this scope",
                           var_decl->name);
        return NULL;
    }

    symbol->is_mutable = var_decl->is_mutable;
    if (var_decl->initializer) {
        symbol_mark_initialized(symbol);
    }

    return NULL;
}

static void* visit_block_stmt(ASTVisitor* visitor, Stmt* stmt) {
    enter_scope(visitor);

    BlockStmt* block = &stmt->block;
    for (size_t i = 0; i < block->statement_count; i++) {
        ast_accept_stmt(block->statements[i], visitor);
    }

    exit_scope(visitor);
    return NULL;
}

static void* visit_if_stmt(ASTVisitor* visitor, Stmt* stmt) {
    SemanticContext* ctx = get_context(visitor);
    IfStmt* if_stmt = &stmt->if_stmt;

    Type* condition_type = ast_accept_expr(if_stmt->condition, visitor);
    if (condition_type && condition_type->kind != TYPE_KIND_BOOL) {
        error_report_simple(ctx->errors, ERROR_LEVEL_ERROR, ERROR_PHASE_SEMANTIC,
                           "", 0, 0, "If condition must be Bool, got '%s'",
                           type_to_string(condition_type));
    }

    ast_accept_stmt(if_stmt->then_branch, visitor);
    if (if_stmt->else_branch) {
        ast_accept_stmt(if_stmt->else_branch, visitor);
    }

    return NULL;
}

static void* visit_while_stmt(ASTVisitor* visitor, Stmt* stmt) {
    SemanticContext* ctx = get_context(visitor);
    WhileStmt* while_stmt = &stmt->while_stmt;

    Type* condition_type = ast_accept_expr(while_stmt->condition, visitor);
    if (condition_type && condition_type->kind != TYPE_KIND_BOOL) {
        error_report_simple(ctx->errors, ERROR_LEVEL_ERROR, ERROR_PHASE_SEMANTIC,
                           "", 0, 0, "While condition must be Bool, got '%s'",
                           type_to_string(condition_type));
    }

    bool old_in_loop = ctx->in_loop;
    ctx->in_loop = true;
    ast_accept_stmt(while_stmt->body, visitor);
    ctx->in_loop = old_in_loop;

    return NULL;
}

static void* visit_for_in_stmt(ASTVisitor* visitor, Stmt* stmt) {
    SemanticContext* ctx = get_context(visitor);
    ForInStmt* for_stmt = &stmt->for_in;

    Type* iterable_type = ast_accept_expr(for_stmt->iterable, visitor);

    enter_scope(visitor);

    Type* element_type = type_any();
    if (iterable_type && iterable_type->kind == TYPE_KIND_ARRAY) {
        element_type = iterable_type->data.array->element_type;
    }

    Symbol* loop_var = symbol_declare(ctx->symbols, for_stmt->variable_name,
                                     SYMBOL_VARIABLE, element_type, NULL);
    if (loop_var) {
        loop_var->is_mutable = false;
        symbol_mark_initialized(loop_var);
    }

    bool old_in_loop = ctx->in_loop;
    ctx->in_loop = true;
    ast_accept_stmt(for_stmt->body, visitor);
    ctx->in_loop = old_in_loop;

    exit_scope(visitor);
    return NULL;
}

static void* visit_for_stmt(ASTVisitor* visitor, Stmt* stmt) {
    SemanticContext* ctx = get_context(visitor);
    ForStmt* for_stmt = &stmt->for_stmt;
    
    enter_scope(visitor);
    
    // Process initializer
    if (for_stmt->initializer) {
        ast_accept_stmt(for_stmt->initializer, visitor);
    }
    
    // Process condition
    if (for_stmt->condition) {
        Type* cond_type = ast_accept_expr(for_stmt->condition, visitor);
        if (cond_type && cond_type->kind != TYPE_KIND_BOOL) {
            error_report_simple(ctx->errors, ERROR_LEVEL_ERROR, ERROR_PHASE_SEMANTIC,
                               "", 0, 0, "For loop condition must be boolean");
        }
    }
    
    // Process increment
    if (for_stmt->increment) {
        ast_accept_expr(for_stmt->increment, visitor);
    }
    
    // Process body
    bool old_in_loop = ctx->in_loop;
    ctx->in_loop = true;
    ast_accept_stmt(for_stmt->body, visitor);
    ctx->in_loop = old_in_loop;
    
    exit_scope(visitor);
    return NULL;
}

static void* visit_return_stmt(ASTVisitor* visitor, Stmt* stmt) {
    SemanticContext* ctx = get_context(visitor);
    ReturnStmt* ret = &stmt->return_stmt;

    if (!ctx->in_function) {
        error_report_simple(ctx->errors, ERROR_LEVEL_ERROR, ERROR_PHASE_SEMANTIC,
                           "", 0, 0, "Return statement outside of function");
        return NULL;
    }

    Type* value_type = type_void();
    if (ret->expression) {
        value_type = ast_accept_expr(ret->expression, visitor);
    }

    if (ctx->current_function_return_type &&
        !type_is_assignable(value_type, ctx->current_function_return_type)) {
        error_report_simple(ctx->errors, ERROR_LEVEL_ERROR, ERROR_PHASE_SEMANTIC,
                           "", 0, 0, "Cannot return value of type '%s' from function returning '%s'",
                           type_to_string(value_type),
                           type_to_string(ctx->current_function_return_type));
    }

    return NULL;
}

static void* visit_break_stmt(ASTVisitor* visitor, Stmt* stmt) {
    (void)stmt; // Silence unused warning
    SemanticContext* ctx = get_context(visitor);

    if (!ctx->in_loop) {
        error_report_simple(ctx->errors, ERROR_LEVEL_ERROR, ERROR_PHASE_SEMANTIC,
                           "", 0, 0, "Break statement outside of loop");
    }

    return NULL;
}

static void* visit_continue_stmt(ASTVisitor* visitor, Stmt* stmt) {
    (void)stmt; // Silence unused warning
    SemanticContext* ctx = get_context(visitor);

    if (!ctx->in_loop) {
        error_report_simple(ctx->errors, ERROR_LEVEL_ERROR, ERROR_PHASE_SEMANTIC,
                           "", 0, 0, "Continue statement outside of loop");
    }

    return NULL;
}


static void* visit_function_stmt(ASTVisitor* visitor, Stmt* stmt) {
    SemanticContext* ctx = get_context(visitor);
    FunctionDecl* func = &stmt->function;

    Type* return_type = type_void();
    if (func->return_type) {
        return_type = type_context_get(ctx->types, func->return_type);
        if (!return_type) {
            return_type = type_unresolved(func->return_type);
            error_report_simple(ctx->errors, ERROR_LEVEL_ERROR, ERROR_PHASE_SEMANTIC,
                               "", 0, 0, "Unknown return type '%s'", func->return_type);
        }
    }

    Type** param_types = calloc(func->parameter_count, sizeof(Type*));
    for (size_t i = 0; i < func->parameter_count; i++) {
        // Handle missing parameter types gracefully
        if (func->parameter_types && func->parameter_types[i]) {
            param_types[i] = type_context_get(ctx->types, func->parameter_types[i]);
            if (!param_types[i]) {
                param_types[i] = type_unresolved(func->parameter_types[i]);
                error_report_simple(ctx->errors, ERROR_LEVEL_ERROR, ERROR_PHASE_SEMANTIC,
                                   "", 0, 0, "Unknown parameter type '%s'", func->parameter_types[i]);
            }
        } else {
            // No type annotation - use Any type for flexibility
            param_types[i] = type_any();
        }
    }

    Type* func_type = type_function(param_types, func->parameter_count, return_type);

    Symbol* func_symbol = symbol_declare(ctx->symbols, func->name, SYMBOL_FUNCTION,
                                        func_type, NULL);
    if (!func_symbol) {
        error_report_simple(ctx->errors, ERROR_LEVEL_ERROR, ERROR_PHASE_SEMANTIC,
                           func->name, 0, 0, "Function '%s' already declared", func->name);
        free(param_types);
        return NULL;
    }

    func_symbol->data.function.arity = func->parameter_count;
    symbol_mark_initialized(func_symbol);

    enter_scope(visitor);

    for (size_t i = 0; i < func->parameter_count; i++) {
        Symbol* param = symbol_declare(ctx->symbols, func->parameter_names[i],
                                      SYMBOL_PARAMETER, param_types[i], NULL);
        if (param) {
            param->is_mutable = false;
            symbol_mark_initialized(param);
        }
    }

    bool old_in_function = ctx->in_function;
    Type* old_return_type = ctx->current_function_return_type;
    ctx->in_function = true;
    ctx->current_function_return_type = return_type;

    if (func->body) {
        ast_accept_stmt(func->body, visitor);
    }

    ctx->in_function = old_in_function;
    ctx->current_function_return_type = old_return_type;

    exit_scope(visitor);
    free(param_types);
    return NULL;
}

static void* visit_function_decl(ASTVisitor* visitor, Decl* decl) {
    SemanticContext* ctx = get_context(visitor);
    FunctionDecl* func = &decl->function;

    Type* return_type = type_void();
    if (func->return_type) {
        return_type = type_context_get(ctx->types, func->return_type);
        if (!return_type) {
            return_type = type_unresolved(func->return_type);
            error_report_simple(ctx->errors, ERROR_LEVEL_ERROR, ERROR_PHASE_SEMANTIC,
                               "", 0, 0, "Unknown return type '%s'", func->return_type);
        }
    }

    Type** param_types = calloc(func->parameter_count, sizeof(Type*));
    for (size_t i = 0; i < func->parameter_count; i++) {
        param_types[i] = type_context_get(ctx->types, func->parameter_types[i]);
        if (!param_types[i]) {
            param_types[i] = type_unresolved(func->parameter_types[i]);
            error_report_simple(ctx->errors, ERROR_LEVEL_ERROR, ERROR_PHASE_SEMANTIC,
                               "", 0, 0, "Unknown parameter type '%s'", func->parameter_types[i]);
        }
    }

    Type* func_type = type_function(param_types, func->parameter_count, return_type);

    Symbol* func_symbol = symbol_declare(ctx->symbols, func->name, SYMBOL_FUNCTION,
                                        func_type, NULL);
    if (!func_symbol) {
        error_report_simple(ctx->errors, ERROR_LEVEL_ERROR, ERROR_PHASE_SEMANTIC,
                           func->name, 0, 0, "Function '%s' already declared", func->name);
        free(param_types);
        return NULL;
    }

    func_symbol->data.function.arity = func->parameter_count;
    symbol_mark_initialized(func_symbol);

    enter_scope(visitor);

    for (size_t i = 0; i < func->parameter_count; i++) {
        Symbol* param = symbol_declare(ctx->symbols, func->parameter_names[i],
                                      SYMBOL_PARAMETER, param_types[i], NULL);
        if (param) {
            param->is_mutable = false;
            symbol_mark_initialized(param);
        }
    }

    bool old_in_function = ctx->in_function;
    Type* old_return_type = ctx->current_function_return_type;
    ctx->in_function = true;
    ctx->current_function_return_type = return_type;

    if (func->body) {
        ast_accept_stmt(func->body, visitor);
    }

    ctx->in_function = old_in_function;
    ctx->current_function_return_type = old_return_type;

    exit_scope(visitor);
    free(param_types);
    return NULL;
}

static void enter_scope(ASTVisitor* visitor) {
    SemanticContext* ctx = get_context(visitor);
    symbol_table_enter_scope(ctx->symbols);
}

static void exit_scope(ASTVisitor* visitor) {
    SemanticContext* ctx = get_context(visitor);
    symbol_table_exit_scope(ctx->symbols);
}

void semantic_analyzer_check_uninitialized_variables(SemanticAnalyzer* analyzer) {
    if (!analyzer) return;

    SymbolList* uninitialized = symbol_table_get_uninitialized(analyzer->context.symbols);
    for (size_t i = 0; i < uninitialized->count; i++) {
        Symbol* symbol = uninitialized->symbols[i];
        error_report_simple(analyzer->context.errors, ERROR_LEVEL_WARNING, ERROR_PHASE_SEMANTIC,
                           symbol->name, 0, 0, "Variable '%s' may be used uninitialized", symbol->name);
    }
    symbol_list_free(uninitialized);
}

void semantic_analyzer_check_unused_variables(SemanticAnalyzer* analyzer) {
    if (!analyzer) return;

    SymbolList* unused = symbol_table_get_unused(analyzer->context.symbols);
    for (size_t i = 0; i < unused->count; i++) {
        Symbol* symbol = unused->symbols[i];
        if (symbol->kind == SYMBOL_VARIABLE || symbol->kind == SYMBOL_PARAMETER) {
            error_report_simple(analyzer->context.errors, ERROR_LEVEL_WARNING, ERROR_PHASE_SEMANTIC,
                               symbol->name, 0, 0, "Variable '%s' is declared but never used", symbol->name);
        }
    }
    symbol_list_free(unused);
}

static void* visit_import_stmt(ASTVisitor* visitor, Stmt* stmt) {
    SemanticContext* ctx = get_context(visitor);
    ImportDecl* import = &stmt->import_decl;
    
    // Check if this is a builtin module
    bool is_builtin = false;
    const char* module_name = import->module_path;
    
    // Remove quotes from module path if present
    if (module_name[0] == '"' || module_name[0] == '\'') {
        size_t len = strlen(module_name);
        if (len >= 2 && module_name[len-1] == module_name[0]) {
            char* unquoted = malloc(len - 1);
            memcpy(unquoted, module_name + 1, len - 2);
            unquoted[len - 2] = '\0';
            module_name = unquoted;
        }
    }
    
    // Check if it's a builtin module (no path separators)
    if (!strchr(module_name, '/') && !strchr(module_name, '\\')) {
        is_builtin = true;
    }
    
    switch (import->type) {
        case IMPORT_ALL:
            // Import all exports from module
            if (is_builtin) {
                const char** names;
                TaggedValue* values;
                size_t count;
                
                if (builtin_module_get_all_exports(module_name, &names, &values, &count)) {
                    for (size_t i = 0; i < count; i++) {
                        // For now, mark all imports as functions with any type
                        symbol_declare(ctx->symbols, names[i], SYMBOL_FUNCTION,
                                     type_any(), NULL);
                    }
                } else {
                    error_report_simple(ctx->errors, ERROR_LEVEL_ERROR, ERROR_PHASE_SEMANTIC,
                                       module_name, 0, 0, "Module '%s' not found", module_name);
                }
            }
            break;
            
        case IMPORT_DEFAULT:
            // Import default export
            if (import->default_name) {
                if (is_builtin) {
                    TaggedValue value;
                    if (builtin_module_get_export(module_name, "default", &value)) {
                        symbol_declare(ctx->symbols, import->default_name, SYMBOL_FUNCTION,
                                     type_any(), NULL);
                    } else {
                        error_report_simple(ctx->errors, ERROR_LEVEL_ERROR, ERROR_PHASE_SEMANTIC,
                                           module_name, 0, 0, "Module '%s' has no default export", module_name);
                    }
                } else {
                    symbol_declare(ctx->symbols, import->default_name, SYMBOL_VARIABLE,
                                 type_any(), NULL);
                }
            }
            break;
            
        case IMPORT_SPECIFIC:
            // Import specific exports
            for (size_t i = 0; i < import->specifier_count; i++) {
                const char* export_name = import->specifiers[i].name;
                const char* local_name = import->specifiers[i].alias ?
                    import->specifiers[i].alias : import->specifiers[i].name;
                
                if (is_builtin) {
                    TaggedValue value;
                    if (builtin_module_get_export(module_name, export_name, &value)) {
                        symbol_declare(ctx->symbols, local_name, SYMBOL_FUNCTION,
                                     type_any(), NULL);
                    } else {
                        error_report_simple(ctx->errors, ERROR_LEVEL_ERROR, ERROR_PHASE_SEMANTIC,
                                           export_name, 0, 0, "Module '%s' has no export named '%s'", 
                                           module_name, export_name);
                    }
                } else {
                    symbol_declare(ctx->symbols, local_name, SYMBOL_VARIABLE,
                                 type_any(), NULL);
                }
            }
            break;
            
        case IMPORT_NAMESPACE:
            // Import all as namespace
            if (import->namespace_alias) {
                // For now, just create a namespace symbol
                // In a full implementation, this would be an object containing all exports
                symbol_declare(ctx->symbols, import->namespace_alias, SYMBOL_VARIABLE,
                             type_any(), NULL);
            }
            break;
    }
    
    return NULL;
}

static void* visit_export_stmt(ASTVisitor* visitor, Stmt* stmt) {
    (void)visitor;  // Unused parameter
    (void)stmt;     // Unused parameter
    // Export statements don't affect local semantic analysis
    // They are handled during module compilation
    return NULL;
}

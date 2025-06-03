#include "semantic/analyzer.h"
#include "runtime/modules/lifecycle/builtin_modules.h"
#include "utils/allocators.h"
#include "semantic/visitor.h"
#include "semantic/symbol_table.h"
#include "semantic/type.h"
#include "utils/error.h"
#include <string.h>
#include <stdio.h>

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
static void register_builtin_symbols(SymbolTable* table);

SemanticAnalyzer* semantic_analyzer_create(ErrorReporter* errors) {
    // Semantic analyzer uses compiler allocator (temporary during compilation)
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_COMPILER);
    SemanticAnalyzer* analyzer = MEM_NEW(alloc, SemanticAnalyzer);
    if (!analyzer) return NULL;

    analyzer->context.symbols = symbol_table_create();
    analyzer->context.types = type_context_create();
    analyzer->context.errors = errors;

    type_context_register_builtin_types(analyzer->context.types);

    analyzer->visitor = visitor_create();
    analyzer->visitor->context = &analyzer->context;

    // Register visitor methods
    analyzer->visitor->visit_binary_expr = visit_binary_expr;
    analyzer->visitor->visit_unary_expr = visit_unary_expr;
    analyzer->visitor->visit_literal_expr = visit_literal_expr;
    analyzer->visitor->visit_variable_expr = visit_variable_expr;
    analyzer->visitor->visit_assignment_expr = visit_assignment_expr;
    analyzer->visitor->visit_call_expr = visit_call_expr;
    analyzer->visitor->visit_array_literal_expr = visit_array_literal_expr;
    analyzer->visitor->visit_subscript_expr = visit_subscript_expr;
    analyzer->visitor->visit_member_expr = visit_member_expr;
    analyzer->visitor->visit_closure_expr = visit_closure_expr;
    analyzer->visitor->visit_string_interp_expr = visit_string_interp_expr;

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

    analyzer->visitor->visit_function_decl = visit_function_decl;

    analyzer->visitor->enter_scope = enter_scope;
    analyzer->visitor->exit_scope = exit_scope;

    // Create global scope
    symbol_table_enter_scope(analyzer->context.symbols);

    // Import builtin functions (stdlib)
    register_builtin_symbols(analyzer->context.symbols);

    return analyzer;
}

void semantic_analyzer_destroy(SemanticAnalyzer* analyzer) {
    if (!analyzer) return;

    Allocator* alloc = allocators_get(ALLOC_SYSTEM_COMPILER);
    
    // Exit global scope
    symbol_table_exit_scope(analyzer->context.symbols);

    symbol_table_destroy(analyzer->context.symbols);
    type_context_destroy(analyzer->context.types);
    visitor_destroy(analyzer->visitor);

    SLANG_MEM_FREE(alloc, analyzer, sizeof(SemanticAnalyzer));
}

void semantic_analyzer_analyze(SemanticAnalyzer* analyzer, ProgramNode* program) {
    ast_visit_program(program, analyzer->visitor);
}

bool semantic_analyze(SemanticAnalyzer* analyzer, ProgramNode* program) {
    semantic_analyzer_analyze(analyzer, program);
    return !error_has_errors(analyzer->context.errors);
}

SymbolTable* semantic_analyzer_get_symbols(SemanticAnalyzer* analyzer) {
    return analyzer ? analyzer->context.symbols : NULL;
}

TypeContext* semantic_analyzer_get_types(SemanticAnalyzer* analyzer) {
    return analyzer ? analyzer->context.types : NULL;
}

void semantic_analyzer_check_uninitialized_variables(SemanticAnalyzer* analyzer) {
    (void)analyzer; // Unused
    // TODO: Implement uninitialized variable checking
}

void semantic_analyzer_check_unused_variables(SemanticAnalyzer* analyzer) {
    (void)analyzer; // Unused
    // TODO: Implement unused variable checking
}

// Type checking helpers

static Type* check_binary_op_types(Type* left, Type* right, SlangTokenType op) {
    if (!left || !right) return NULL;

    // Arithmetic operators
    if (op == TOKEN_PLUS || op == TOKEN_MINUS || op == TOKEN_STAR || op == TOKEN_SLASH) {
        if (type_is_numeric(left) && type_is_numeric(right)) {
            // If either is float, result is float
            if (left->kind == TYPE_KIND_FLOAT || right->kind == TYPE_KIND_FLOAT) {
                return type_float();
            }
            return type_int();
        }

        // String concatenation
        if (op == TOKEN_PLUS && left->kind == TYPE_KIND_STRING && right->kind == TYPE_KIND_STRING) {
            return type_string();
        }
    }

    // Comparison operators
    if (op == TOKEN_LESS || op == TOKEN_GREATER || op == TOKEN_LESS_EQUAL || op == TOKEN_GREATER_EQUAL) {
        if (type_is_numeric(left) && type_is_numeric(right)) {
            return type_bool();
        }
    }

    // Equality operators
    if (op == TOKEN_EQUAL_EQUAL || op == TOKEN_NOT_EQUAL) {
        // Can compare any types for equality
        return type_bool();
    }

    // Logical operators
    if (op == TOKEN_AND_AND || op == TOKEN_OR_OR) {
        return type_bool();
    }

    return NULL;
}

static Type* check_unary_op_type(Type* operand, SlangTokenType op) {
    if (!operand) return NULL;

    switch (op) {
        case TOKEN_NOT:
            return type_bool();

        case TOKEN_MINUS:
        case TOKEN_PLUS:
            if (type_is_numeric(operand)) {
                return operand;
            }
            break;

        default:
            break;
    }

    return NULL;
}

// Visitor implementations

static void* visit_binary_expr(ASTVisitor* visitor, Expr* expr) {
    SemanticContext* ctx = visitor->context;

    // Visit operands
    ast_accept_expr(expr->binary.left, visitor);
    ast_accept_expr(expr->binary.right, visitor);

    Type* left_type = expr->binary.left->computed_type;
    Type* right_type = expr->binary.right->computed_type;

    Type* result_type = check_binary_op_types(left_type, right_type, expr->binary.operator.type);

    if (!result_type) {
        error_report_simple(ctx->errors, ERROR_LEVEL_ERROR, ERROR_PHASE_SEMANTIC,
            NULL, expr->binary.operator.line, expr->binary.operator.column,
            "Invalid operand types for binary operator");
    }

    expr->computed_type = result_type;
    return NULL;
}

static void* visit_unary_expr(ASTVisitor* visitor, Expr* expr) {
    SemanticContext* ctx = visitor->context;

    // Visit operand
    ast_accept_expr(expr->unary.operand, visitor);

    Type* operand_type = expr->unary.operand->computed_type;
    Type* result_type = check_unary_op_type(operand_type, expr->unary.operator.type);

    if (!result_type) {
        error_report_simple(ctx->errors, ERROR_LEVEL_ERROR, ERROR_PHASE_SEMANTIC,
            NULL, expr->unary.operator.line, expr->unary.operator.column,
            "Invalid operand type for unary operator");
    }

    expr->computed_type = result_type;
    return NULL;
}

static void* visit_literal_expr(ASTVisitor* visitor, Expr* expr) {
    SemanticContext* ctx = visitor->context;
    (void)ctx; // Unused

    switch (expr->literal.type) {
        case LITERAL_NIL:
            expr->computed_type = type_nil();
            break;
        case LITERAL_BOOL:
            expr->computed_type = type_bool();
            break;
        case LITERAL_INT:
            expr->computed_type = type_int();
            break;
        case LITERAL_FLOAT:
            expr->computed_type = type_float();
            break;
        case LITERAL_STRING:
            expr->computed_type = type_string();
            break;
    }

    return NULL;
}

static void* visit_variable_expr(ASTVisitor* visitor, Expr* expr) {
    SemanticContext* ctx = visitor->context;

    Symbol* symbol = symbol_lookup(ctx->symbols, expr->variable.name);
    if (!symbol) {
        // Allocate error message using compiler allocator
        Allocator* alloc = allocators_get(ALLOC_SYSTEM_COMPILER);
        size_t msg_len = strlen("Undefined variable: ") + strlen(expr->variable.name) + 1;
        char* msg = MEM_ALLOC(alloc, msg_len);
        if (msg) {
            snprintf(msg, msg_len, "Undefined variable: %s", expr->variable.name);
            error_report_simple(ctx->errors, ERROR_LEVEL_ERROR, ERROR_PHASE_SEMANTIC,
                NULL, 0, 0, "%s", msg);
            SLANG_MEM_FREE(alloc, msg, msg_len);
        }
        expr->computed_type = type_any();
    } else {
        expr->computed_type = symbol->type;
        symbol->is_used = true;
    }

    return NULL;
}

// Assignment expression visitor
static void* visit_assignment_expr(ASTVisitor* visitor, Expr* expr) {
    SemanticContext* ctx = visitor->context;
    
    // Visit the target and value expressions
    ast_accept_expr(expr->assignment.target, visitor);
    ast_accept_expr(expr->assignment.value, visitor);
    
    // For now, just use the value type
    expr->computed_type = expr->assignment.value->computed_type;
    
    // TODO: Check that target is assignable
    if (expr->assignment.target->type == EXPR_VARIABLE) {
        const char* var_name = expr->assignment.target->variable.name;
        Symbol* symbol = symbol_lookup(ctx->symbols, var_name);
        if (symbol) {
            symbol->is_used = true;
            if (!symbol->is_mutable) {
                error_report_simple(ctx->errors, ERROR_LEVEL_ERROR, ERROR_PHASE_SEMANTIC,
                    NULL, 0, 0, "Cannot assign to immutable variable: %s", var_name);
            }
        }
    }
    
    return NULL;
}

// Call expression visitor
static void* visit_call_expr(ASTVisitor* visitor, Expr* expr) {
    SemanticContext* ctx = visitor->context;
    (void)ctx; // Unused
    
    // Visit callee
    ast_accept_expr(expr->call.callee, visitor);
    
    // Visit arguments
    for (size_t i = 0; i < expr->call.argument_count; i++) {
        ast_accept_expr(expr->call.arguments[i], visitor);
    }
    
    // TODO: Type check function call
    expr->computed_type = type_any();
    return NULL;
}

// Array literal visitor
static void* visit_array_literal_expr(ASTVisitor* visitor, Expr* expr) {
    SemanticContext* ctx = visitor->context;
    (void)ctx; // Unused
    
    Type* element_type = NULL;
    
    // Visit all elements
    for (size_t i = 0; i < expr->array_literal.element_count; i++) {
        ast_accept_expr(expr->array_literal.elements[i], visitor);
        
        // TODO: Infer common element type
        if (i == 0) {
            element_type = expr->array_literal.elements[i]->computed_type;
        }
    }
    
    expr->computed_type = type_array(element_type ? element_type : type_any());
    return NULL;
}

// Subscript expression visitor
static void* visit_subscript_expr(ASTVisitor* visitor, Expr* expr) {
    SemanticContext* ctx = visitor->context;
    (void)ctx; // Unused
    
    // Visit object and index
    ast_accept_expr(expr->subscript.object, visitor);
    ast_accept_expr(expr->subscript.index, visitor);
    
    // TODO: Type check subscript
    expr->computed_type = type_any();
    return NULL;
}

// Member expression visitor
static void* visit_member_expr(ASTVisitor* visitor, Expr* expr) {
    SemanticContext* ctx = visitor->context;
    (void)ctx; // Unused
    
    // Visit object
    ast_accept_expr(expr->member.object, visitor);
    
    // TODO: Look up member type
    expr->computed_type = type_any();
    return NULL;
}

// Closure expression visitor
static void* visit_closure_expr(ASTVisitor* visitor, Expr* expr) {
    SemanticContext* ctx = visitor->context;
    (void)ctx; // Unused
    
    // TODO: Handle closure type
    expr->computed_type = type_any();
    return NULL;
}

// String interpolation visitor
static void* visit_string_interp_expr(ASTVisitor* visitor, Expr* expr) {
    SemanticContext* ctx = visitor->context;
    (void)ctx; // Unused
    
    // Visit all interpolated expressions
    for (size_t i = 0; i < expr->string_interp.expr_count; i++) {
        ast_accept_expr(expr->string_interp.expressions[i], visitor);
    }
    
    expr->computed_type = type_string();
    return NULL;
}

// Statement visitors

static void* visit_expression_stmt(ASTVisitor* visitor, Stmt* stmt) {
    ast_accept_expr(stmt->expression.expression, visitor);
    return NULL;
}

static void* visit_var_decl_stmt(ASTVisitor* visitor, Stmt* stmt) {
    SemanticContext* ctx = visitor->context;
    
    // Visit initializer if present
    Type* var_type = NULL;
    if (stmt->var_decl.initializer) {
        ast_accept_expr(stmt->var_decl.initializer, visitor);
        var_type = stmt->var_decl.initializer->computed_type;
    }
    
    // If no type and no initializer, use any type
    if (!var_type) {
        var_type = type_any();
    }
    
    // Create symbol
    Token dummy_token = {0}; // TODO: Get real token from stmt
    Symbol* symbol = symbol_declare(ctx->symbols, stmt->var_decl.name, SYMBOL_VARIABLE, var_type, &dummy_token);
    if (symbol) {
        symbol->is_mutable = stmt->var_decl.is_mutable;
        symbol_mark_initialized(symbol);
    }
    
    return NULL;
}

static void* visit_block_stmt(ASTVisitor* visitor, Stmt* stmt) {
    // Enter new scope
    enter_scope(visitor);
    
    // Visit all statements
    for (size_t i = 0; i < stmt->block.statement_count; i++) {
        ast_accept_stmt(stmt->block.statements[i], visitor);
    }
    
    // Exit scope
    exit_scope(visitor);
    
    return NULL;
}

static void* visit_if_stmt(ASTVisitor* visitor, Stmt* stmt) {
    // Visit condition
    ast_accept_expr(stmt->if_stmt.condition, visitor);
    
    // Visit then branch
    ast_accept_stmt(stmt->if_stmt.then_branch, visitor);
    
    // Visit else branch if present
    if (stmt->if_stmt.else_branch) {
        ast_accept_stmt(stmt->if_stmt.else_branch, visitor);
    }
    
    return NULL;
}

static void* visit_while_stmt(ASTVisitor* visitor, Stmt* stmt) {
    SemanticContext* ctx = visitor->context;
    bool prev_in_loop = ctx->in_loop;
    ctx->in_loop = true;
    
    // Visit condition
    ast_accept_expr(stmt->while_stmt.condition, visitor);
    
    // Visit body
    ast_accept_stmt(stmt->while_stmt.body, visitor);
    
    ctx->in_loop = prev_in_loop;
    return NULL;
}

static void* visit_for_in_stmt(ASTVisitor* visitor, Stmt* stmt) {
    SemanticContext* ctx = visitor->context;
    bool prev_in_loop = ctx->in_loop;
    ctx->in_loop = true;
    
    // Enter new scope for loop variable
    enter_scope(visitor);
    
    // Visit iterable
    ast_accept_expr(stmt->for_in.iterable, visitor);
    
    // Create loop variable
    Token dummy_token = {0}; // TODO: Get real token from stmt
    Symbol* symbol = symbol_declare(ctx->symbols, stmt->for_in.variable_name, SYMBOL_VARIABLE, type_any(), &dummy_token);
    if (symbol) {
        symbol->is_mutable = false; // Loop variables are const
        symbol_mark_initialized(symbol);
    }
    
    // Visit body
    ast_accept_stmt(stmt->for_in.body, visitor);
    
    exit_scope(visitor);
    ctx->in_loop = prev_in_loop;
    return NULL;
}

static void* visit_for_stmt(ASTVisitor* visitor, Stmt* stmt) {
    SemanticContext* ctx = visitor->context;
    bool prev_in_loop = ctx->in_loop;
    ctx->in_loop = true;
    
    // Enter new scope for loop variable
    enter_scope(visitor);
    
    // Visit initializer
    if (stmt->for_stmt.initializer) {
        ast_accept_stmt(stmt->for_stmt.initializer, visitor);
    }
    
    // Visit condition
    if (stmt->for_stmt.condition) {
        ast_accept_expr(stmt->for_stmt.condition, visitor);
    }
    
    // Visit body
    ast_accept_stmt(stmt->for_stmt.body, visitor);
    
    // Visit increment
    if (stmt->for_stmt.increment) {
        ast_accept_expr(stmt->for_stmt.increment, visitor);
    }
    
    exit_scope(visitor);
    ctx->in_loop = prev_in_loop;
    return NULL;
}

static void* visit_return_stmt(ASTVisitor* visitor, Stmt* stmt) {
    SemanticContext* ctx = visitor->context;
    
    if (!ctx->in_function) {
        error_report_simple(ctx->errors, ERROR_LEVEL_ERROR, ERROR_PHASE_SEMANTIC,
            NULL, 0, 0, "Return statement outside of function");
    }
    
    // Visit return value if present
    if (stmt->return_stmt.expression) {
        ast_accept_expr(stmt->return_stmt.expression, visitor);
    }
    
    return NULL;
}

static void* visit_break_stmt(ASTVisitor* visitor, Stmt* stmt) {
    (void)stmt; // Unused
    SemanticContext* ctx = visitor->context;
    
    if (!ctx->in_loop) {
        error_report_simple(ctx->errors, ERROR_LEVEL_ERROR, ERROR_PHASE_SEMANTIC,
            NULL, 0, 0, "Break statement outside of loop");
    }
    
    return NULL;
}

static void* visit_continue_stmt(ASTVisitor* visitor, Stmt* stmt) {
    (void)stmt; // Unused
    SemanticContext* ctx = visitor->context;
    
    if (!ctx->in_loop) {
        error_report_simple(ctx->errors, ERROR_LEVEL_ERROR, ERROR_PHASE_SEMANTIC,
            NULL, 0, 0, "Continue statement outside of loop");
    }
    
    return NULL;
}

static void* visit_function_stmt(ASTVisitor* visitor, Stmt* stmt) {
    SemanticContext* ctx = visitor->context;
    
    // TODO: Create function type
    Type* func_type = type_any();
    
    // Create symbol for function
    Token dummy_token = {0}; // TODO: Get real token from stmt
    Symbol* symbol = symbol_declare(ctx->symbols, stmt->function.name, SYMBOL_FUNCTION, func_type, &dummy_token);
    if (symbol) {
        symbol->data.function.arity = stmt->function.parameter_count;
        symbol_mark_initialized(symbol);
    }
    
    // Enter function scope
    bool prev_in_function = ctx->in_function;
    ctx->in_function = true;
    enter_scope(visitor);
    
    // Define parameters
    for (size_t i = 0; i < stmt->function.parameter_count; i++) {
        Token dummy_token = {0}; // TODO: Get real token from param
        Symbol* param = symbol_declare(ctx->symbols, stmt->function.parameter_names[i], SYMBOL_PARAMETER, type_any(), &dummy_token);
        if (param) {
            param->is_mutable = false; // Parameters are immutable by default
            symbol_mark_initialized(param);
        }
    }
    
    // Visit body
    ast_accept_stmt(stmt->function.body, visitor);
    
    exit_scope(visitor);
    ctx->in_function = prev_in_function;
    
    return NULL;
}

static void* visit_import_stmt(ASTVisitor* visitor, Stmt* stmt) {
    (void)visitor; // Unused
    (void)stmt; // Unused
    // TODO: Handle imports
    return NULL;
}

static void* visit_export_stmt(ASTVisitor* visitor, Stmt* stmt) {
    // Handle different export types
    switch (stmt->export_decl.type) {
        case EXPORT_DECLARATION:
            if (stmt->export_decl.decl_export.declaration) {
                ast_accept_decl(stmt->export_decl.decl_export.declaration, visitor);
            }
            break;
        default:
            // TODO: Handle other export types
            break;
    }
    return NULL;
}

static void* visit_function_decl(ASTVisitor* visitor, Decl* decl) {
    (void)visitor; // Unused
    (void)decl; // Unused
    // TODO: Handle function declarations
    return NULL;
}

static void enter_scope(ASTVisitor* visitor) {
    SemanticContext* ctx = visitor->context;
    symbol_table_enter_scope(ctx->symbols);
}

static void exit_scope(ASTVisitor* visitor) {
    SemanticContext* ctx = visitor->context;
    symbol_table_exit_scope(ctx->symbols);
}

// Helper function to register builtin symbols
static void register_builtin_symbols(SymbolTable* table) {
    // Register built-in types
    Token dummy_token = {0};
    symbol_declare(table, "Int", SYMBOL_TYPE, type_int(), &dummy_token);
    symbol_declare(table, "Float", SYMBOL_TYPE, type_float(), &dummy_token);
    symbol_declare(table, "Bool", SYMBOL_TYPE, type_bool(), &dummy_token);
    symbol_declare(table, "String", SYMBOL_TYPE, type_string(), &dummy_token);
    
    // TODO: Register built-in functions like print, etc.
}
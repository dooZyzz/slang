#include "semantic/visitor.h"
#include <stdlib.h>

void* ast_accept_expr(Expr* expr, ASTVisitor* visitor) {
    if (!expr || !visitor) return NULL;
    
    switch (expr->type) {
        case EXPR_BINARY:
            if (visitor->visit_binary_expr) {
                return visitor->visit_binary_expr(visitor, expr);
            }
            break;
        case EXPR_UNARY:
            if (visitor->visit_unary_expr) {
                return visitor->visit_unary_expr(visitor, expr);
            }
            break;
        case EXPR_LITERAL:
            if (visitor->visit_literal_expr) {
                return visitor->visit_literal_expr(visitor, expr);
            }
            break;
        case EXPR_VARIABLE:
            if (visitor->visit_variable_expr) {
                return visitor->visit_variable_expr(visitor, expr);
            }
            break;
        case EXPR_ASSIGNMENT:
            if (visitor->visit_assignment_expr) {
                return visitor->visit_assignment_expr(visitor, expr);
            }
            break;
        case EXPR_CALL:
            if (visitor->visit_call_expr) {
                return visitor->visit_call_expr(visitor, expr);
            }
            break;
        case EXPR_ARRAY_LITERAL:
            if (visitor->visit_array_literal_expr) {
                return visitor->visit_array_literal_expr(visitor, expr);
            }
            break;
        case EXPR_OBJECT_LITERAL:
            if (visitor->visit_object_literal_expr) {
                return visitor->visit_object_literal_expr(visitor, expr);
            }
            break;
        case EXPR_SUBSCRIPT:
            if (visitor->visit_subscript_expr) {
                return visitor->visit_subscript_expr(visitor, expr);
            }
            break;
        case EXPR_MEMBER:
            if (visitor->visit_member_expr) {
                return visitor->visit_member_expr(visitor, expr);
            }
            break;
        case EXPR_SELF:
            if (visitor->visit_self_expr) {
                return visitor->visit_self_expr(visitor, expr);
            }
            break;
        case EXPR_SUPER:
            if (visitor->visit_super_expr) {
                return visitor->visit_super_expr(visitor, expr);
            }
            break;
        case EXPR_CLOSURE:
            if (visitor->visit_closure_expr) {
                return visitor->visit_closure_expr(visitor, expr);
            }
            break;
        case EXPR_TERNARY:
            if (visitor->visit_ternary_expr) {
                return visitor->visit_ternary_expr(visitor, expr);
            }
            break;
        case EXPR_NIL_COALESCING:
            if (visitor->visit_nil_coalescing_expr) {
                return visitor->visit_nil_coalescing_expr(visitor, expr);
            }
            break;
        case EXPR_OPTIONAL_CHAINING:
            if (visitor->visit_optional_chaining_expr) {
                return visitor->visit_optional_chaining_expr(visitor, expr);
            }
            break;
        case EXPR_FORCE_UNWRAP:
            if (visitor->visit_force_unwrap_expr) {
                return visitor->visit_force_unwrap_expr(visitor, expr);
            }
            break;
        case EXPR_TYPE_CAST:
            if (visitor->visit_type_cast_expr) {
                return visitor->visit_type_cast_expr(visitor, expr);
            }
            break;
        case EXPR_AWAIT:
            if (visitor->visit_await_expr) {
                return visitor->visit_await_expr(visitor, expr);
            }
            break;
        case EXPR_STRING_INTERP:
            if (visitor->visit_string_interp_expr) {
                return visitor->visit_string_interp_expr(visitor, expr);
            }
            break;
    }
    
    return NULL;
}

void* ast_accept_stmt(Stmt* stmt, ASTVisitor* visitor) {
    if (!stmt || !visitor) return NULL;
    
    switch (stmt->type) {
        case STMT_EXPRESSION:
            if (visitor->visit_expression_stmt) {
                return visitor->visit_expression_stmt(visitor, stmt);
            }
            break;
        case STMT_VAR_DECL:
            if (visitor->visit_var_decl_stmt) {
                return visitor->visit_var_decl_stmt(visitor, stmt);
            }
            break;
        case STMT_BLOCK:
            if (visitor->visit_block_stmt) {
                return visitor->visit_block_stmt(visitor, stmt);
            }
            break;
        case STMT_IF:
            if (visitor->visit_if_stmt) {
                return visitor->visit_if_stmt(visitor, stmt);
            }
            break;
        case STMT_WHILE:
            if (visitor->visit_while_stmt) {
                return visitor->visit_while_stmt(visitor, stmt);
            }
            break;
        case STMT_FOR_IN:
            if (visitor->visit_for_in_stmt) {
                return visitor->visit_for_in_stmt(visitor, stmt);
            }
            break;
        case STMT_FOR:
            if (visitor->visit_for_stmt) {
                return visitor->visit_for_stmt(visitor, stmt);
            }
            break;
        case STMT_RETURN:
            if (visitor->visit_return_stmt) {
                return visitor->visit_return_stmt(visitor, stmt);
            }
            break;
        case STMT_BREAK:
            if (visitor->visit_break_stmt) {
                return visitor->visit_break_stmt(visitor, stmt);
            }
            break;
        case STMT_CONTINUE:
            if (visitor->visit_continue_stmt) {
                return visitor->visit_continue_stmt(visitor, stmt);
            }
            break;
        case STMT_DEFER:
            if (visitor->visit_defer_stmt) {
                return visitor->visit_defer_stmt(visitor, stmt);
            }
            break;
        case STMT_GUARD:
            if (visitor->visit_guard_stmt) {
                return visitor->visit_guard_stmt(visitor, stmt);
            }
            break;
        case STMT_SWITCH:
            if (visitor->visit_switch_stmt) {
                return visitor->visit_switch_stmt(visitor, stmt);
            }
            break;
        case STMT_THROW:
            if (visitor->visit_throw_stmt) {
                return visitor->visit_throw_stmt(visitor, stmt);
            }
            break;
        case STMT_DO_CATCH:
            if (visitor->visit_do_catch_stmt) {
                return visitor->visit_do_catch_stmt(visitor, stmt);
            }
            break;
        case STMT_FUNCTION:
            if (visitor->visit_function_stmt) {
                return visitor->visit_function_stmt(visitor, stmt);
            }
            break;
        case STMT_CLASS:
            if (visitor->visit_class_stmt) {
                return visitor->visit_class_stmt(visitor, stmt);
            }
            break;
        case STMT_STRUCT:
            if (visitor->visit_struct_stmt) {
                return visitor->visit_struct_stmt(visitor, stmt);
            }
            break;
        case STMT_IMPORT:
            if (visitor->visit_import_stmt) {
                return visitor->visit_import_stmt(visitor, stmt);
            }
            break;
        case STMT_EXPORT:
            if (visitor->visit_export_stmt) {
                return visitor->visit_export_stmt(visitor, stmt);
            }
            break;
    }
    
    return NULL;
}

void* ast_accept_decl(Decl* decl, ASTVisitor* visitor) {
    if (!decl || !visitor) return NULL;
    
    switch (decl->type) {
        case DECL_FUNCTION:
            if (visitor->visit_function_decl) {
                return visitor->visit_function_decl(visitor, decl);
            }
            break;
        case DECL_CLASS:
            if (visitor->visit_class_decl) {
                return visitor->visit_class_decl(visitor, decl);
            }
            break;
        case DECL_STRUCT:
            if (visitor->visit_struct_decl) {
                return visitor->visit_struct_decl(visitor, decl);
            }
            break;
        case DECL_ENUM:
            if (visitor->visit_enum_decl) {
                return visitor->visit_enum_decl(visitor, decl);
            }
            break;
        case DECL_PROTOCOL:
            if (visitor->visit_protocol_decl) {
                return visitor->visit_protocol_decl(visitor, decl);
            }
            break;
        case DECL_EXTENSION:
            if (visitor->visit_extension_decl) {
                return visitor->visit_extension_decl(visitor, decl);
            }
            break;
        case DECL_TYPEALIAS:
            if (visitor->visit_typealias_decl) {
                return visitor->visit_typealias_decl(visitor, decl);
            }
            break;
        case DECL_IMPORT:
            if (visitor->visit_import_decl) {
                return visitor->visit_import_decl(visitor, decl);
            }
            break;
        case DECL_EXPORT:
            if (visitor->visit_export_decl) {
                return visitor->visit_export_decl(visitor, decl);
            }
            break;
    }
    
    return NULL;
}

void* ast_accept_type(TypeExpr* type, ASTVisitor* visitor) {
    if (!type || !visitor) return NULL;
    
    switch (type->type) {
        case TYPE_IDENTIFIER:
            if (visitor->visit_identifier_type) {
                return visitor->visit_identifier_type(visitor, type);
            }
            break;
        case TYPE_OPTIONAL:
            if (visitor->visit_optional_type) {
                return visitor->visit_optional_type(visitor, type);
            }
            break;
        case TYPE_ARRAY:
            if (visitor->visit_array_type) {
                return visitor->visit_array_type(visitor, type);
            }
            break;
        case TYPE_DICTIONARY:
            if (visitor->visit_dictionary_type) {
                return visitor->visit_dictionary_type(visitor, type);
            }
            break;
        case TYPE_FUNCTION:
            if (visitor->visit_function_type) {
                return visitor->visit_function_type(visitor, type);
            }
            break;
        case TYPE_TUPLE:
            if (visitor->visit_tuple_type) {
                return visitor->visit_tuple_type(visitor, type);
            }
            break;
    }
    
    return NULL;
}

void ast_visit_program(ProgramNode* program, ASTVisitor* visitor) {
    if (!program || !visitor) return;
    
    if (visitor->pre_visit) {
        visitor->pre_visit(visitor);
    }
    
    for (size_t i = 0; i < program->statement_count; i++) {
        ast_accept_stmt(program->statements[i], visitor);
    }
    
    if (visitor->post_visit) {
        visitor->post_visit(visitor);
    }
}

ASTVisitor* visitor_create(void) {
    return calloc(1, sizeof(ASTVisitor));
}

void visitor_destroy(ASTVisitor* visitor) {
    free(visitor);
}
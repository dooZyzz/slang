#ifndef LANG_VISITOR_H
#define LANG_VISITOR_H

#include "ast/ast.h"
#include <stdbool.h>

typedef struct ASTVisitor ASTVisitor;

typedef void* (*VisitExprFn)(ASTVisitor* visitor, Expr* expr);
typedef void* (*VisitStmtFn)(ASTVisitor* visitor, Stmt* stmt);
typedef void* (*VisitDeclFn)(ASTVisitor* visitor, Decl* decl);
typedef void* (*VisitTypeFn)(ASTVisitor* visitor, TypeExpr* type);

struct ASTVisitor {
    void* context;
    
    VisitExprFn visit_binary_expr;
    VisitExprFn visit_unary_expr;
    VisitExprFn visit_literal_expr;
    VisitExprFn visit_variable_expr;
    VisitExprFn visit_assignment_expr;
    VisitExprFn visit_call_expr;
    VisitExprFn visit_array_literal_expr;
    VisitExprFn visit_object_literal_expr;
    VisitExprFn visit_subscript_expr;
    VisitExprFn visit_member_expr;
    VisitExprFn visit_self_expr;
    VisitExprFn visit_super_expr;
    VisitExprFn visit_closure_expr;
    VisitExprFn visit_ternary_expr;
    VisitExprFn visit_nil_coalescing_expr;
    VisitExprFn visit_optional_chaining_expr;
    VisitExprFn visit_force_unwrap_expr;
    VisitExprFn visit_type_cast_expr;
    VisitExprFn visit_await_expr;
    VisitExprFn visit_string_interp_expr;
    
    VisitStmtFn visit_expression_stmt;
    VisitStmtFn visit_var_decl_stmt;
    VisitStmtFn visit_block_stmt;
    VisitStmtFn visit_if_stmt;
    VisitStmtFn visit_while_stmt;
    VisitStmtFn visit_for_in_stmt;
    VisitStmtFn visit_for_stmt;
    VisitStmtFn visit_return_stmt;
    VisitStmtFn visit_break_stmt;
    VisitStmtFn visit_continue_stmt;
    VisitStmtFn visit_defer_stmt;
    VisitStmtFn visit_guard_stmt;
    VisitStmtFn visit_switch_stmt;
    VisitStmtFn visit_throw_stmt;
    VisitStmtFn visit_do_catch_stmt;
    VisitStmtFn visit_function_stmt;
    VisitStmtFn visit_class_stmt;
    VisitStmtFn visit_struct_stmt;
    VisitStmtFn visit_import_stmt;
    VisitStmtFn visit_export_stmt;
    
    VisitDeclFn visit_function_decl;
    VisitDeclFn visit_class_decl;
    VisitDeclFn visit_struct_decl;
    VisitDeclFn visit_enum_decl;
    VisitDeclFn visit_protocol_decl;
    VisitDeclFn visit_extension_decl;
    VisitDeclFn visit_typealias_decl;
    VisitDeclFn visit_import_decl;
    VisitDeclFn visit_export_decl;
    
    VisitTypeFn visit_identifier_type;
    VisitTypeFn visit_optional_type;
    VisitTypeFn visit_array_type;
    VisitTypeFn visit_dictionary_type;
    VisitTypeFn visit_function_type;
    VisitTypeFn visit_tuple_type;
    
    void (*enter_scope)(ASTVisitor* visitor);
    void (*exit_scope)(ASTVisitor* visitor);
    
    void (*pre_visit)(ASTVisitor* visitor);
    void (*post_visit)(ASTVisitor* visitor);
};

void* ast_accept_expr(Expr* expr, ASTVisitor* visitor);
void* ast_accept_stmt(Stmt* stmt, ASTVisitor* visitor);
void* ast_accept_decl(Decl* decl, ASTVisitor* visitor);
void* ast_accept_type(TypeExpr* type, ASTVisitor* visitor);

void ast_visit_program(ProgramNode* program, ASTVisitor* visitor);

ASTVisitor* visitor_create(void);
void visitor_destroy(ASTVisitor* visitor);

#endif

#include "ast/ast_printer.h"
#include <stdio.h>

static int indent_level = 0;

static void print_indent(void) {
    for (int i = 0; i < indent_level; i++) {
        printf("  ");
    }
}

static void print_token_type(SlangTokenType type) {
    switch (type) {
        case TOKEN_PLUS: printf("+"); break;
        case TOKEN_MINUS: printf("-"); break;
        case TOKEN_STAR: printf("*"); break;
        case TOKEN_SLASH: printf("/"); break;
        case TOKEN_PERCENT: printf("%%"); break;
        case TOKEN_EQUAL_EQUAL: printf("=="); break;
        case TOKEN_NOT_EQUAL: printf("!="); break;
        case TOKEN_LESS: printf("<"); break;
        case TOKEN_LESS_EQUAL: printf("<="); break;
        case TOKEN_GREATER: printf(">"); break;
        case TOKEN_GREATER_EQUAL: printf(">="); break;
        case TOKEN_AND_AND: printf("&&"); break;
        case TOKEN_OR_OR: printf("||"); break;
        default: printf("op(%d)", type); break;
    }
}

static void print_expr(Expr* expr);
static void print_stmt(Stmt* stmt);

static void print_expr_list(Expr** exprs, size_t count) {
    for (size_t i = 0; i < count; i++) {
        if (i > 0) printf(", ");
        print_expr(exprs[i]);
    }
}

static void print_stmt_list(Stmt** stmts, size_t count) {
    for (size_t i = 0; i < count; i++) {
        print_stmt(stmts[i]);
    }
}

static void print_expr(Expr* expr) {
    if (!expr) {
        printf("<null>");
        return;
    }
    
    switch (expr->type) {
        case EXPR_LITERAL:
            switch (expr->literal.type) {
                case LITERAL_FLOAT:
                    printf("%.2f", expr->literal.value.floating);
                    break;
                case LITERAL_INT:
                    printf("%lld", expr->literal.value.integer);
                    break;
                case LITERAL_STRING:
                    printf("\"%s\"", expr->literal.value.string.value);
                    break;
                case LITERAL_BOOL:
                    printf("%s", expr->literal.value.boolean ? "true" : "false");
                    break;
                case LITERAL_NIL:
                    printf("nil");
                    break;
                default:
                    printf("<literal>");
                    break;
            }
            break;
            
        case EXPR_VARIABLE:
            printf("%s", expr->variable.name);
            break;
            
        case EXPR_BINARY:
            printf("(");
            print_expr(expr->binary.left);
            printf(" ");
            print_token_type(expr->binary.operator.type);
            printf(" ");
            print_expr(expr->binary.right);
            printf(")");
            break;
            
        case EXPR_UNARY:
            printf("(");
            print_token_type(expr->unary.operator.type);
            print_expr(expr->unary.operand);
            printf(")");
            break;
            
        case EXPR_CALL:
            print_expr(expr->call.callee);
            printf("(");
            print_expr_list(expr->call.arguments, expr->call.argument_count);
            printf(")");
            break;
            
        case EXPR_CLOSURE:
            printf("{");
            for (size_t i = 0; i < expr->closure.parameter_count; i++) {
                if (i > 0) printf(", ");
                printf("%s", expr->closure.parameter_names[i]);
            }
            printf(" in\n");
            indent_level++;
            print_stmt_list(expr->closure.body->block.statements,
                          expr->closure.body->block.statement_count);
            indent_level--;
            print_indent();
            printf("}");
            break;
            
        case EXPR_ARRAY_LITERAL:
            printf("[");
            print_expr_list(expr->array_literal.elements, expr->array_literal.element_count);
            printf("]");
            break;
            
        case EXPR_OBJECT_LITERAL:
            printf("{");
            for (size_t i = 0; i < expr->object_literal.pair_count; i++) {
                if (i > 0) printf(", ");
                printf("%s: ", expr->object_literal.keys[i]);
                print_expr(expr->object_literal.values[i]);
            }
            printf("}");
            break;
            
        case EXPR_MEMBER:
            print_expr(expr->member.object);
            printf(".%s", expr->member.property);
            break;
            
        case EXPR_SUBSCRIPT:
            print_expr(expr->subscript.object);
            printf("[");
            print_expr(expr->subscript.index);
            printf("]");
            break;
            
        case EXPR_ASSIGNMENT:
            print_expr(expr->assignment.target);
            printf(" = ");
            print_expr(expr->assignment.value);
            break;
            
        default:
            printf("<expr type=%d>", expr->type);
            break;
    }
}


static void print_stmt(Stmt* stmt) {
    if (!stmt) {
        print_indent();
        printf("<null>\n");
        return;
    }
    
    print_indent();
    
    switch (stmt->type) {
        case STMT_EXPRESSION:
            print_expr(stmt->expression.expression);
            printf(";\n");
            break;
            
        case STMT_VAR_DECL:
            printf("%s %s", stmt->var_decl.is_mutable ? "var" : "let", stmt->var_decl.name);
            if (stmt->var_decl.initializer) {
                printf(" = ");
                print_expr(stmt->var_decl.initializer);
            }
            printf(";\n");
            break;
            
        case STMT_FUNCTION:
            printf("func %s(", stmt->function.name);
            for (size_t i = 0; i < stmt->function.parameter_count; i++) {
                if (i > 0) printf(", ");
                printf("%s", stmt->function.parameter_names[i]);
            }
            printf(") {\n");
            indent_level++;
            print_stmt_list(stmt->function.body->block.statements, 
                          stmt->function.body->block.statement_count);
            indent_level--;
            print_indent();
            printf("}\n");
            break;
            
        case STMT_BLOCK:
            printf("{\n");
            indent_level++;
            print_stmt_list(stmt->block.statements, stmt->block.statement_count);
            indent_level--;
            print_indent();
            printf("}\n");
            break;
            
        case STMT_IF:
            printf("if (");
            print_expr(stmt->if_stmt.condition);
            printf(") ");
            if (stmt->if_stmt.then_branch->type != STMT_BLOCK) {
                printf("\n");
                indent_level++;
            }
            print_stmt(stmt->if_stmt.then_branch);
            if (stmt->if_stmt.then_branch->type != STMT_BLOCK) {
                indent_level--;
            }
            if (stmt->if_stmt.else_branch) {
                print_indent();
                printf("else ");
                if (stmt->if_stmt.else_branch->type != STMT_BLOCK) {
                    printf("\n");
                    indent_level++;
                }
                print_stmt(stmt->if_stmt.else_branch);
                if (stmt->if_stmt.else_branch->type != STMT_BLOCK) {
                    indent_level--;
                }
            }
            break;
            
        case STMT_WHILE:
            printf("while (");
            print_expr(stmt->while_stmt.condition);
            printf(") ");
            if (stmt->while_stmt.body->type != STMT_BLOCK) {
                printf("\n");
                indent_level++;
            }
            print_stmt(stmt->while_stmt.body);
            if (stmt->while_stmt.body->type != STMT_BLOCK) {
                indent_level--;
            }
            break;
            
        case STMT_FOR_IN:
            printf("for %s in ", stmt->for_in.variable_name);
            print_expr(stmt->for_in.iterable);
            printf(" ");
            if (stmt->for_in.body->type != STMT_BLOCK) {
                printf("\n");
                indent_level++;
            }
            print_stmt(stmt->for_in.body);
            if (stmt->for_in.body->type != STMT_BLOCK) {
                indent_level--;
            }
            break;
            
        case STMT_RETURN:
            printf("return");
            if (stmt->return_stmt.expression) {
                printf(" ");
                print_expr(stmt->return_stmt.expression);
            }
            printf(";\n");
            break;
            
        case STMT_BREAK:
            printf("break;\n");
            break;
            
        case STMT_CONTINUE:
            printf("continue;\n");
            break;
            
        case STMT_IMPORT:
            printf("import %s", stmt->import_decl.module_path);
            if (stmt->import_decl.alias) {
                printf(" as %s", stmt->import_decl.alias);
            }
            printf(";\n");
            break;
            
        case STMT_EXPORT:
            printf("export ");
            switch (stmt->export_decl.type) {
                case EXPORT_DEFAULT:
                    printf("default ");
                    printf("%s", stmt->export_decl.default_export.name);
                    break;
                case EXPORT_NAMED:
                    printf("{ ");
                    for (size_t i = 0; i < stmt->export_decl.named_export.specifier_count; i++) {
                        if (i > 0) printf(", ");
                        printf("%s", stmt->export_decl.named_export.specifiers[i].name);
                        if (stmt->export_decl.named_export.specifiers[i].alias) {
                            printf(" as %s", stmt->export_decl.named_export.specifiers[i].alias);
                        }
                    }
                    printf(" }");
                    break;
                case EXPORT_DECLARATION:
                    // Export declaration is a Decl*, not Stmt*
                    printf("<export declaration>");
                    // TODO: Add print_decl function to handle Decl* types
                    return; // Already printed newline
                case EXPORT_ALL:
                    printf("*");
                    break;
            }
            printf(";\n");
            break;
            
        default:
            printf("<stmt type=%d>\n", stmt->type);
            break;
    }
}

void ast_print_program(ProgramNode* program) {
    if (!program) {
        printf("<null program>\n");
        return;
    }
    
    indent_level = 0;
    printf("Program (%zu statements):\n", program->statement_count);
    
    for (size_t i = 0; i < program->statement_count; i++) {
        print_stmt(program->statements[i]);
    }
}
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "utils/test_framework.h"
#include "utils/test_macros.h"
#include "ast/ast.h"
#include "lexer/token.h"

// Test creating and destroying literal expressions
DEFINE_TEST(create_literal_nil)
{
    Expr* expr = expr_create_literal_nil();
    TEST_ASSERT(suite, expr != NULL, "create_literal_nil");
    TEST_ASSERT(suite, expr->type == EXPR_LITERAL, "create_literal_nil");
    TEST_ASSERT(suite, expr->literal.type == LITERAL_NIL, "create_literal_nil");
    expr_destroy(expr);
}

DEFINE_TEST(create_literal_bool)
{
    Expr* expr_true = expr_create_literal_bool(true);
    TEST_ASSERT(suite, expr_true != NULL, "create_literal_bool");
    TEST_ASSERT(suite, expr_true->type == EXPR_LITERAL, "create_literal_bool");
    TEST_ASSERT(suite, expr_true->literal.type == LITERAL_BOOL, "create_literal_bool");
    TEST_ASSERT(suite, expr_true->literal.value.boolean == true, "create_literal_bool");
    expr_destroy(expr_true);

    Expr* expr_false = expr_create_literal_bool(false);
    TEST_ASSERT(suite, expr_false != NULL, "create_literal_bool");
    TEST_ASSERT(suite, expr_false->literal.value.boolean == false, "create_literal_bool");
    expr_destroy(expr_false);
}

DEFINE_TEST(create_literal_int)
{
    Expr* expr = expr_create_literal_int(42);
    TEST_ASSERT(suite, expr != NULL, "create_literal_int");
    TEST_ASSERT(suite, expr->type == EXPR_LITERAL, "create_literal_int");
    TEST_ASSERT(suite, expr->literal.type == LITERAL_INT, "create_literal_int");
    TEST_ASSERT(suite, expr->literal.value.integer == 42, "create_literal_int");
    expr_destroy(expr);

    // Test negative number
    Expr* expr_neg = expr_create_literal_int(-100);
    TEST_ASSERT(suite, expr_neg->literal.value.integer == -100, "create_literal_int");
    expr_destroy(expr_neg);
}

DEFINE_TEST(create_literal_float)
{
    Expr* expr = expr_create_literal_float(3.14159);
    TEST_ASSERT(suite, expr != NULL, "create_literal_float");
    TEST_ASSERT(suite, expr->type == EXPR_LITERAL, "create_literal_float");
    TEST_ASSERT(suite, expr->literal.type == LITERAL_FLOAT, "create_literal_float");
    TEST_ASSERT(suite, expr->literal.value.floating == 3.14159, "create_literal_float");
    expr_destroy(expr);
}

DEFINE_TEST(create_literal_string)
{
    const char* test_str = "Hello, World!";
    Expr* expr = expr_create_literal_string(test_str);
    TEST_ASSERT(suite, expr != NULL, "create_literal_string");
    TEST_ASSERT(suite, expr->type == EXPR_LITERAL, "create_literal_string");
    TEST_ASSERT(suite, expr->literal.type == LITERAL_STRING, "create_literal_string");
    TEST_ASSERT(suite, strcmp(expr->literal.value.string.value, test_str) == 0, "create_literal_string");
    TEST_ASSERT(suite, expr->literal.value.string.length == strlen(test_str), "create_literal_string");
    expr_destroy(expr);

    // Test empty string
    Expr* expr_empty = expr_create_literal_string("");
    TEST_ASSERT(suite, expr_empty->literal.value.string.length == 0, "create_literal_string");
    expr_destroy(expr_empty);
}

DEFINE_TEST(create_variable)
{
    const char* var_name = "myVariable";
    Expr* expr = expr_create_variable(var_name);
    TEST_ASSERT(suite, expr != NULL, "create_variable");
    TEST_ASSERT(suite, expr->type == EXPR_VARIABLE, "create_variable");
    TEST_ASSERT(suite, strcmp(expr->variable.name, var_name) == 0, "create_variable");
    expr_destroy(expr);
}

DEFINE_TEST(create_binary)
{
    Expr* left = expr_create_literal_int(10);
    Expr* right = expr_create_literal_int(20);
    
    Token op = {.type = TOKEN_PLUS, .lexeme = "+", .line = 1, .column = 1};
    Expr* expr = expr_create_binary(op, left, right);
    
    TEST_ASSERT(suite, expr != NULL, "create_binary");
    TEST_ASSERT(suite, expr->type == EXPR_BINARY, "create_binary");
    TEST_ASSERT(suite, expr->binary.operator.type == TOKEN_PLUS, "create_binary");
    TEST_ASSERT(suite, expr->binary.left == left, "create_binary");
    TEST_ASSERT(suite, expr->binary.right == right, "create_binary");
    
    expr_destroy(expr); // This should also destroy left and right
}

DEFINE_TEST(create_unary)
{
    Expr* operand = expr_create_literal_int(42);
    Token op = {.type = TOKEN_MINUS, .lexeme = "-", .line = 1, .column = 1};
    Expr* expr = expr_create_unary(op, operand);
    
    TEST_ASSERT(suite, expr != NULL, "create_unary");
    TEST_ASSERT(suite, expr->type == EXPR_UNARY, "create_unary");
    TEST_ASSERT(suite, expr->unary.operator.type == TOKEN_MINUS, "create_unary");
    TEST_ASSERT(suite, expr->unary.operand == operand, "create_unary");
    
    expr_destroy(expr); // This should also destroy operand
}

DEFINE_TEST(create_assignment)
{
    Expr* target = expr_create_variable("x");
    Expr* value = expr_create_literal_int(100);
    Expr* expr = expr_create_assignment(target, value);
    
    TEST_ASSERT(suite, expr != NULL, "create_assignment");
    TEST_ASSERT(suite, expr->type == EXPR_ASSIGNMENT, "create_assignment");
    TEST_ASSERT(suite, expr->assignment.target == target, "create_assignment");
    TEST_ASSERT(suite, expr->assignment.value == value, "create_assignment");
    
    expr_destroy(expr); // This should also destroy target and value
}

DEFINE_TEST(create_call)
{
    Expr* callee = expr_create_variable("print");
    Expr* args[] = {
        expr_create_literal_string("Hello"),
        expr_create_literal_int(42)
    };
    
    Expr* expr = expr_create_call(callee, args, 2);
    
    TEST_ASSERT(suite, expr != NULL, "create_call");
    TEST_ASSERT(suite, expr->type == EXPR_CALL, "create_call");
    TEST_ASSERT(suite, expr->call.callee == callee, "create_call");
    TEST_ASSERT(suite, expr->call.argument_count == 2, "create_call");
    TEST_ASSERT(suite, expr->call.arguments[0] == args[0], "create_call");
    TEST_ASSERT(suite, expr->call.arguments[1] == args[1], "create_call");
    
    expr_destroy(expr); // This should also destroy callee and arguments
}

DEFINE_TEST(create_array_literal)
{
    Expr* elements[] = {
        expr_create_literal_int(1),
        expr_create_literal_int(2),
        expr_create_literal_int(3)
    };
    
    Expr* expr = expr_create_array_literal(elements, 3);
    
    TEST_ASSERT(suite, expr != NULL, "create_array_literal");
    TEST_ASSERT(suite, expr->type == EXPR_ARRAY_LITERAL, "create_array_literal");
    TEST_ASSERT(suite, expr->array_literal.element_count == 3, "create_array_literal");
    TEST_ASSERT(suite, expr->array_literal.elements[0] == elements[0], "create_array_literal");
    TEST_ASSERT(suite, expr->array_literal.elements[1] == elements[1], "create_array_literal");
    TEST_ASSERT(suite, expr->array_literal.elements[2] == elements[2], "create_array_literal");
    
    expr_destroy(expr); // This should also destroy all elements
}

DEFINE_TEST(create_subscript)
{
    Expr* object = expr_create_variable("array");
    Expr* index = expr_create_literal_int(0);
    Expr* expr = expr_create_subscript(object, index);
    
    TEST_ASSERT(suite, expr != NULL, "create_subscript");
    TEST_ASSERT(suite, expr->type == EXPR_SUBSCRIPT, "create_subscript");
    TEST_ASSERT(suite, expr->subscript.object == object, "create_subscript");
    TEST_ASSERT(suite, expr->subscript.index == index, "create_subscript");
    
    expr_destroy(expr); // This should also destroy object and index
}

DEFINE_TEST(create_member)
{
    Expr* object = expr_create_variable("person");
    const char* property = "name";
    Expr* expr = expr_create_member(object, property);
    
    TEST_ASSERT(suite, expr != NULL, "create_member");
    TEST_ASSERT(suite, expr->type == EXPR_MEMBER, "create_member");
    TEST_ASSERT(suite, expr->member.object == object, "create_member");
    TEST_ASSERT(suite, strcmp(expr->member.property, property) == 0, "create_member");
    
    expr_destroy(expr); // This should also destroy object
}

// Test statement creation
DEFINE_TEST(create_expression_stmt)
{
    Expr* expr = expr_create_literal_int(42);
    Stmt* stmt = stmt_create_expression(expr);
    
    TEST_ASSERT(suite, stmt != NULL, "create_expression_stmt");
    TEST_ASSERT(suite, stmt->type == STMT_EXPRESSION, "create_expression_stmt");
    TEST_ASSERT(suite, stmt->expression.expression == expr, "create_expression_stmt");
    
    stmt_destroy(stmt); // This should also destroy expr
}

DEFINE_TEST(create_var_decl)
{
    Expr* init = expr_create_literal_int(10);
    Stmt* stmt = stmt_create_var_decl(true, "x", "Int", init);
    
    TEST_ASSERT(suite, stmt != NULL, "create_var_decl");
    TEST_ASSERT(suite, stmt->type == STMT_VAR_DECL, "create_var_decl");
    TEST_ASSERT(suite, stmt->var_decl.is_mutable == true, "create_var_decl");
    TEST_ASSERT(suite, strcmp(stmt->var_decl.name, "x") == 0, "create_var_decl");
    TEST_ASSERT(suite, strcmp(stmt->var_decl.type_annotation, "Int") == 0, "create_var_decl");
    TEST_ASSERT(suite, stmt->var_decl.initializer == init, "create_var_decl");
    
    stmt_destroy(stmt); // This should also destroy init
}

DEFINE_TEST(create_block)
{
    Stmt* stmts[] = {
        stmt_create_expression(expr_create_literal_int(1)),
        stmt_create_expression(expr_create_literal_int(2)),
        stmt_create_expression(expr_create_literal_int(3))
    };
    
    Stmt* block = stmt_create_block(stmts, 3);
    
    TEST_ASSERT(suite, block != NULL, "create_block");
    TEST_ASSERT(suite, block->type == STMT_BLOCK, "create_block");
    TEST_ASSERT(suite, block->block.statement_count == 3, "create_block");
    TEST_ASSERT(suite, block->block.statements[0] == stmts[0], "create_block");
    TEST_ASSERT(suite, block->block.statements[1] == stmts[1], "create_block");
    TEST_ASSERT(suite, block->block.statements[2] == stmts[2], "create_block");
    
    stmt_destroy(block); // This should also destroy all statements
}

DEFINE_TEST(create_if_stmt)
{
    Expr* condition = expr_create_literal_bool(true);
    Stmt* then_branch = stmt_create_expression(expr_create_literal_int(1));
    Stmt* else_branch = stmt_create_expression(expr_create_literal_int(2));
    
    Stmt* stmt = stmt_create_if(condition, then_branch, else_branch);
    
    TEST_ASSERT(suite, stmt != NULL, "create_if_stmt");
    TEST_ASSERT(suite, stmt->type == STMT_IF, "create_if_stmt");
    TEST_ASSERT(suite, stmt->if_stmt.condition == condition, "create_if_stmt");
    TEST_ASSERT(suite, stmt->if_stmt.then_branch == then_branch, "create_if_stmt");
    TEST_ASSERT(suite, stmt->if_stmt.else_branch == else_branch, "create_if_stmt");
    
    stmt_destroy(stmt); // This should destroy all parts
}

DEFINE_TEST(create_while_stmt)
{
    Expr* condition = expr_create_literal_bool(true);
    Stmt* body = stmt_create_expression(expr_create_literal_int(1));
    
    Stmt* stmt = stmt_create_while(condition, body);
    
    TEST_ASSERT(suite, stmt != NULL, "create_while_stmt");
    TEST_ASSERT(suite, stmt->type == STMT_WHILE, "create_while_stmt");
    TEST_ASSERT(suite, stmt->while_stmt.condition == condition, "create_while_stmt");
    TEST_ASSERT(suite, stmt->while_stmt.body == body, "create_while_stmt");
    
    stmt_destroy(stmt);
}

DEFINE_TEST(create_for_in_stmt)
{
    Expr* iterable = expr_create_variable("array");
    Stmt* body = stmt_create_expression(expr_create_literal_int(1));
    
    Stmt* stmt = stmt_create_for_in("item", iterable, body);
    
    TEST_ASSERT(suite, stmt != NULL, "create_for_in_stmt");
    TEST_ASSERT(suite, stmt->type == STMT_FOR_IN, "create_for_in_stmt");
    TEST_ASSERT(suite, strcmp(stmt->for_in.variable_name, "item") == 0, "create_for_in_stmt");
    TEST_ASSERT(suite, stmt->for_in.iterable == iterable, "create_for_in_stmt");
    TEST_ASSERT(suite, stmt->for_in.body == body, "create_for_in_stmt");
    
    stmt_destroy(stmt);
}

DEFINE_TEST(create_return_stmt)
{
    Expr* expr = expr_create_literal_int(42);
    Stmt* stmt = stmt_create_return(expr);
    
    TEST_ASSERT(suite, stmt != NULL, "create_return_stmt");
    TEST_ASSERT(suite, stmt->type == STMT_RETURN, "create_return_stmt");
    TEST_ASSERT(suite, stmt->return_stmt.expression == expr, "create_return_stmt");
    
    stmt_destroy(stmt);
}

DEFINE_TEST(create_break_continue)
{
    Stmt* break_stmt = stmt_create_break();
    TEST_ASSERT(suite, break_stmt != NULL, "create_break_continue");
    TEST_ASSERT(suite, break_stmt->type == STMT_BREAK, "create_break_continue");
    stmt_destroy(break_stmt);
    
    Stmt* continue_stmt = stmt_create_continue();
    TEST_ASSERT(suite, continue_stmt != NULL, "create_break_continue");
    TEST_ASSERT(suite, continue_stmt->type == STMT_CONTINUE, "create_break_continue");
    stmt_destroy(continue_stmt);
}

DEFINE_TEST(create_function_stmt)
{
    const char* param_names[] = {"x", "y"};
    const char* param_types[] = {"Int", "Int"};
    Stmt* body = stmt_create_return(expr_create_literal_int(0));
    
    Stmt* stmt = stmt_create_function("add", param_names, param_types, 2, "Int", body);
    
    TEST_ASSERT(suite, stmt != NULL, "create_function_stmt");
    TEST_ASSERT(suite, stmt->type == STMT_FUNCTION, "create_function_stmt");
    TEST_ASSERT(suite, strcmp(stmt->function.name, "add") == 0, "create_function_stmt");
    TEST_ASSERT(suite, stmt->function.parameter_count == 2, "create_function_stmt");
    TEST_ASSERT(suite, strcmp(stmt->function.parameter_names[0], "x") == 0, "create_function_stmt");
    TEST_ASSERT(suite, strcmp(stmt->function.parameter_names[1], "y") == 0, "create_function_stmt");
    TEST_ASSERT(suite, strcmp(stmt->function.parameter_types[0], "Int") == 0, "create_function_stmt");
    TEST_ASSERT(suite, strcmp(stmt->function.parameter_types[1], "Int") == 0, "create_function_stmt");
    TEST_ASSERT(suite, strcmp(stmt->function.return_type, "Int") == 0, "create_function_stmt");
    TEST_ASSERT(suite, stmt->function.body == body, "create_function_stmt");
    
    stmt_destroy(stmt);
}

DEFINE_TEST(create_program)
{
    Stmt* stmts[] = {
        stmt_create_var_decl(false, "x", NULL, expr_create_literal_int(10)),
        stmt_create_expression(expr_create_literal_int(20))
    };
    
    ProgramNode* program = program_create(stmts, 2);
    
    TEST_ASSERT(suite, program != NULL, "create_program");
    TEST_ASSERT(suite, program->statement_count == 2, "create_program");
    TEST_ASSERT(suite, program->statements[0] == stmts[0], "create_program");
    TEST_ASSERT(suite, program->statements[1] == stmts[1], "create_program");
    
    program_destroy(program);
}

// Define test suite
TEST_SUITE(ast_unit)
    TEST_CASE(create_literal_nil, "Create Literal Nil")
    TEST_CASE(create_literal_bool, "Create Literal Bool")
    TEST_CASE(create_literal_int, "Create Literal Int")
    TEST_CASE(create_literal_float, "Create Literal Float")
    TEST_CASE(create_literal_string, "Create Literal String")
    TEST_CASE(create_variable, "Create Variable")
    TEST_CASE(create_binary, "Create Binary Expression")
    TEST_CASE(create_unary, "Create Unary Expression")
    TEST_CASE(create_assignment, "Create Assignment")
    TEST_CASE(create_call, "Create Call Expression")
    TEST_CASE(create_array_literal, "Create Array Literal")
    TEST_CASE(create_subscript, "Create Subscript")
    TEST_CASE(create_member, "Create Member Access")
    TEST_CASE(create_expression_stmt, "Create Expression Statement")
    TEST_CASE(create_var_decl, "Create Variable Declaration")
    TEST_CASE(create_block, "Create Block Statement")
    TEST_CASE(create_if_stmt, "Create If Statement")
    TEST_CASE(create_while_stmt, "Create While Statement")
    TEST_CASE(create_for_in_stmt, "Create For-In Statement")
    TEST_CASE(create_return_stmt, "Create Return Statement")
    TEST_CASE(create_break_continue, "Create Break/Continue")
    TEST_CASE(create_function_stmt, "Create Function Statement")
    TEST_CASE(create_program, "Create Program")
END_TEST_SUITE(ast_unit)
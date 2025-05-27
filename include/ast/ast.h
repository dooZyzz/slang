#ifndef AST_H
#define AST_H

#include "lexer/token.h"
#include <stddef.h>
#include <stdbool.h>

typedef struct Type Type;
typedef struct Expr Expr;
typedef struct Stmt Stmt;
typedef struct Decl Decl;
typedef struct TypeExpr TypeExpr;

typedef enum {
    EXPR_BINARY,
    EXPR_UNARY,
    EXPR_LITERAL,
    EXPR_VARIABLE,
    EXPR_ASSIGNMENT,
    EXPR_CALL,
    EXPR_ARRAY_LITERAL,
    EXPR_OBJECT_LITERAL,
    EXPR_SUBSCRIPT,
    EXPR_MEMBER,
    EXPR_SELF,
    EXPR_SUPER,
    EXPR_CLOSURE,
    EXPR_TERNARY,
    EXPR_NIL_COALESCING,
    EXPR_OPTIONAL_CHAINING,
    EXPR_FORCE_UNWRAP,
    EXPR_TYPE_CAST,
    EXPR_AWAIT,
    EXPR_STRING_INTERP
} ExprType;

typedef enum {
    STMT_EXPRESSION,
    STMT_VAR_DECL,
    STMT_BLOCK,
    STMT_IF,
    STMT_WHILE,
    STMT_FOR_IN,
    STMT_FOR,
    STMT_RETURN,
    STMT_BREAK,
    STMT_CONTINUE,
    STMT_DEFER,
    STMT_GUARD,
    STMT_SWITCH,
    STMT_THROW,
    STMT_DO_CATCH,
    STMT_FUNCTION,
    STMT_CLASS,
    STMT_IMPORT,
    STMT_EXPORT,
    STMT_STRUCT,
    STMT_MODULE,
    STMT_VAR
} StmtType;

typedef enum {
    DECL_FUNCTION,
    DECL_CLASS,
    DECL_STRUCT,
    DECL_ENUM,
    DECL_PROTOCOL,
    DECL_EXTENSION,
    DECL_TYPEALIAS,
    DECL_IMPORT,
    DECL_EXPORT,
    DECL_MODULE  // Module declaration
} DeclType;

typedef enum {
    TYPE_IDENTIFIER,
    TYPE_OPTIONAL,
    TYPE_ARRAY,
    TYPE_DICTIONARY,
    TYPE_FUNCTION,
    TYPE_TUPLE
} TypeExprType;

typedef enum {
    LITERAL_NIL,
    LITERAL_BOOL,
    LITERAL_INT,
    LITERAL_FLOAT,
    LITERAL_STRING
} LiteralType;

typedef struct {
    LiteralType type;
    union {
        bool boolean;
        long long integer;
        double floating;
        struct {
            char* value;
            size_t length;
        } string;
    } value;
} LiteralExpr;

typedef struct {
    Token operator;
    Expr* left;
    Expr* right;
} BinaryExpr;

typedef struct {
    Token operator;
    Expr* operand;
} UnaryExpr;

typedef struct {
    const char* name;
} VariableExpr;

typedef struct {
    Expr* target;
    Expr* value;
} AssignmentExpr;

typedef struct {
    Expr* callee;
    Expr** arguments;
    size_t argument_count;
} CallExpr;

typedef struct {
    Expr** elements;
    size_t element_count;
} ArrayLiteralExpr;

typedef struct {
    Expr* object;
    Expr* index;
} SubscriptExpr;

typedef struct {
    Expr* object;
    const char* property;
} MemberExpr;

typedef struct {
    Expr* condition;
    Expr* then_branch;
    Expr* else_branch;
} TernaryExpr;

typedef struct {
    Expr* left;
    Expr* right;
} NilCoalescingExpr;

typedef struct {
    Expr* operand;
} OptionalChainingExpr;

typedef struct {
    Expr* operand;
} ForceUnwrapExpr;

typedef struct {
    Expr* expression;
    TypeExpr* target_type;
} TypeCastExpr;

typedef struct {
    Expr* expression;
} AwaitExpr;

typedef struct {
    const char** parameter_names;
    TypeExpr** parameter_types;
    size_t parameter_count;
    TypeExpr* return_type;
    Stmt* body;
} ClosureExpr;

typedef struct {
    const char** keys;
    Expr** values;
    size_t pair_count;
} ObjectLiteralExpr;

typedef struct {
    char** parts;          // String parts between interpolations
    Expr** expressions;    // Expressions to interpolate
    size_t part_count;     // Number of string parts (always expr_count + 1)
    size_t expr_count;     // Number of expressions
} StringInterpExpr;

struct Expr {
    ExprType type;
    Type* computed_type;
    union {
        LiteralExpr literal;
        BinaryExpr binary;
        UnaryExpr unary;
        VariableExpr variable;
        AssignmentExpr assignment;
        CallExpr call;
        ArrayLiteralExpr array_literal;
        ObjectLiteralExpr object_literal;
        SubscriptExpr subscript;
        MemberExpr member;
        TernaryExpr ternary;
        NilCoalescingExpr nil_coalescing;
        OptionalChainingExpr optional_chaining;
        ForceUnwrapExpr force_unwrap;
        TypeCastExpr type_cast;
        AwaitExpr await;
        ClosureExpr closure;
        StringInterpExpr string_interp;
    };
};

typedef struct {
    Expr* expression;
} ExpressionStmt;

typedef struct {
    bool is_mutable;
    const char* name;
    const char* type_annotation;
    Expr* initializer;
} VarDeclStmt;

typedef struct {
    Stmt** statements;
    size_t statement_count;
} BlockStmt;

typedef struct {
    Expr* condition;
    Stmt* then_branch;
    Stmt* else_branch;
} IfStmt;

typedef struct {
    Expr* condition;
    Stmt* body;
} WhileStmt;

typedef struct {
    const char* variable_name;
    Expr* iterable;
    Stmt* body;
} ForInStmt;

typedef struct {
    Stmt* initializer;  // Can be var declaration or expression
    Expr* condition;
    Expr* increment;
    Stmt* body;
} ForStmt;

typedef struct {
    Expr* expression;
} ReturnStmt;

typedef struct {
    int dummy;  // To avoid empty struct warning
} BreakStmt;

typedef struct {
    int dummy;  // To avoid empty struct warning
} ContinueStmt;

typedef struct {
    Stmt* statement;
} DeferStmt;

typedef struct {
    const char* name;
    const char** parameter_names;
    const char** parameter_types;
    size_t parameter_count;
    const char* return_type;
    Stmt* body;
    bool is_async;
    bool is_throwing;
    bool is_mutating;
} FunctionDecl;

// Import declaration types - moved here to be defined before Stmt
typedef enum {
    IMPORT_ALL,          // import "module"
    IMPORT_SPECIFIC,     // import { foo, bar } from "module"
    IMPORT_DEFAULT,      // import foo from "module"
    IMPORT_NAMESPACE    // import * as foo from "module"
} ImportType;

typedef struct {
    const char* name;
    const char* alias;  // NULL if no alias
} ImportSpecifier;

typedef struct {
    ImportType type;
    const char* module_path;      // Dotted path like "sys.native.io" or local path like "@/renderer"
    const char* namespace_alias;  // For IMPORT_NAMESPACE
    const char* default_name;     // For IMPORT_DEFAULT
    ImportSpecifier* specifiers;  // For IMPORT_SPECIFIC
    size_t specifier_count;
    bool is_native;              // True for native modules
    bool is_local;               // True for local imports (@ prefix)
    const char* alias;           // Import alias (e.g., import sys.io as io)
} ImportDecl;

typedef enum {
    EXPORT_DEFAULT,      // export default foo
    EXPORT_NAMED,        // export { foo, bar }
    EXPORT_ALL,          // export * from "module"
    EXPORT_DECLARATION   // export function foo() {}
} ExportType;

typedef struct {
    ExportType type;
    union {
        struct {
            const char* name;
        } default_export;
        struct {
            ImportSpecifier* specifiers;
            size_t specifier_count;
            const char* from_module;  // NULL if exporting local items
        } named_export;
        struct {
            const char* from_module;
        } all_export;
        struct {
            Decl* declaration;
        } decl_export;
    };
} ExportDecl;

typedef struct {
    const char* name;
    const char* superclass;
    Stmt** members;
    size_t member_count;
} ClassDecl;

typedef struct {
    const char* name;
    Stmt** members;
    size_t member_count;
} StructDecl;

typedef struct {
    const char* name;        // Module name (e.g., "com.example.utils")
    Decl** declarations;     // Module contents
    size_t decl_count;
    bool is_exported;        // Whether this module block is exported
} ModuleDecl;

struct Stmt {
    StmtType type;
    union {
        ExpressionStmt expression;
        VarDeclStmt var_decl;
        BlockStmt block;
        IfStmt if_stmt;
        WhileStmt while_stmt;
        ForInStmt for_in;
        ForStmt for_stmt;
        ReturnStmt return_stmt;
        BreakStmt break_stmt;
        ContinueStmt continue_stmt;
        DeferStmt defer_stmt;
        FunctionDecl function;
        ClassDecl class_decl;
        ImportDecl import_decl;
        ExportDecl export_decl;
        StructDecl struct_decl;
        ModuleDecl module_decl;
    };
};

struct Decl {
    DeclType type;
    union {
        FunctionDecl function;
        ClassDecl class_decl;
        StructDecl struct_decl;
        ImportDecl import_decl;
        ExportDecl export_decl;
        ModuleDecl module_decl;
    };
};


typedef struct {
    const char* name;
} IdentifierType;

typedef struct {
    TypeExpr* wrapped;
} OptionalType;

typedef struct {
    TypeExpr* element;
} ArrayTypeExpr;

typedef struct {
    TypeExpr* key;
    TypeExpr* value;
} DictionaryTypeExpr;

typedef struct {
    TypeExpr** parameters;
    size_t parameter_count;
    TypeExpr* return_type;
} FunctionTypeExpr;

typedef struct {
    TypeExpr** elements;
    size_t element_count;
} TupleTypeExpr;

struct TypeExpr {
    TypeExprType type;
    union {
        IdentifierType identifier;
        OptionalType optional;
        ArrayTypeExpr array;
        DictionaryTypeExpr dictionary;
        FunctionTypeExpr function;
        TupleTypeExpr tuple;
    };
};

typedef struct {
    const char* module_name;  // Optional module declaration at file level
    Stmt** statements;
    size_t statement_count;
} ProgramNode;

Expr* expr_create_literal_nil(void);
Expr* expr_create_literal_bool(bool value);
Expr* expr_create_literal_int(long long value);
Expr* expr_create_literal_float(double value);
Expr* expr_create_literal_string(const char* value);

Expr* expr_create_variable(const char* name);
Expr* expr_create_binary(Token operator, Expr* left, Expr* right);
Expr* expr_create_unary(Token operator, Expr* operand);
Expr* expr_create_assignment(Expr* target, Expr* value);
Expr* expr_create_call(Expr* callee, Expr** arguments, size_t argument_count);
Expr* expr_create_subscript(Expr* object, Expr* index);
Expr* expr_create_member(Expr* object, const char* property);
Expr* expr_create_array_literal(Expr** elements, size_t element_count);
Expr* expr_create_object_literal(const char** keys, Expr** values, size_t pair_count);
Expr* expr_create_closure(const char** parameter_names, TypeExpr** parameter_types, 
                         size_t parameter_count, TypeExpr* return_type, Stmt* body);
Expr* expr_create_string_interp(char** parts, Expr** expressions, size_t part_count, size_t expr_count);

Stmt* stmt_create_expression(Expr* expression);
Stmt* stmt_create_var_decl(bool is_mutable, const char* name, const char* type_annotation, Expr* initializer);
Stmt* stmt_create_block(Stmt** statements, size_t statement_count);
Stmt* stmt_create_if(Expr* condition, Stmt* then_branch, Stmt* else_branch);
Stmt* stmt_create_while(Expr* condition, Stmt* body);
Stmt* stmt_create_for_in(const char* variable_name, Expr* iterable, Stmt* body);
Stmt* stmt_create_for(Stmt* initializer, Expr* condition, Expr* increment, Stmt* body);
Stmt* stmt_create_return(Expr* expression);
Stmt* stmt_create_break(void);
Stmt* stmt_create_continue(void);
Stmt* stmt_create_function(const char* name, const char** parameter_names, const char** parameter_types,
                          size_t parameter_count, const char* return_type, Stmt* body);
Stmt* stmt_create_class(const char* name, const char* superclass, Stmt** members, size_t member_count);

Decl* decl_create_function(const char* name, const char** parameter_names, const char** parameter_types, 
                          size_t parameter_count, const char* return_type, Stmt* body);

ProgramNode* program_create(Stmt** statements, size_t statement_count);

// Import/Export creation functions
Stmt* stmt_create_import(ImportType type, const char* module_path);
Stmt* stmt_create_export(ExportType type);

void expr_destroy(Expr* expr);
void stmt_destroy(Stmt* stmt);
void decl_destroy(Decl* decl);
void program_destroy(ProgramNode* program);
void ast_free_program(ProgramNode* program);  // Alias for program_destroy

#endif

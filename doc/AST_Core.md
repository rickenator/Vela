# Core AST Design

This document describes the fundamental building blocks of the Vyn Abstract Syntax Tree (AST), including the base `Node` class, its primary derivatives (`Expression`, `Statement`, `Declaration`), the `NodeType` enumeration, and the Visitor pattern implementation. All definitions are sourced from `include/vyn/parser/ast.hpp`.

## 1. Base AST Node (`vyn::ast::Node`) and Derivatives

All AST nodes in Vyn inherit from a base `Node` class. `Expression`, `Statement`, and `Declaration` are key abstract base classes derived from `Node`.

```cpp
// Structure in include/vyn/parser/ast.hpp
namespace vyn::ast {

// Forward declarations for Visitor pattern and base types
class Node;
class Expression;
class Statement;
class Declaration;
class Visitor;
// ... (other forward declarations for all concrete node types are implicitly needed by Visitor)

// SourceLocation is defined in \"vyn/source_location.hpp\"

// Base AST Node
class Node {
public:
    SourceLocation loc;
    std::string inferredTypeName; // For type checking/codegen hints

    Node(SourceLocation loc);
    virtual ~Node() = default;
    virtual NodeType getType() const = 0;
    virtual std::string toString() const = 0;
    virtual void accept(Visitor& visitor) = 0;
};

// Base Expression Node
class Expression : public Node {
public:
    Expression(SourceLocation loc);
    // Note: Specific expression types derive from this.
};

// Base Statement Node
class Statement : public Node {
public:
    Statement(SourceLocation loc);
    // Note: Specific statement types derive from this.
};

// Base Declaration Node (Declarations are also Statements)
class Declaration : public Statement {
public:
    Declaration(SourceLocation loc);
    // Note: Specific declaration types derive from this.
};

// vyn::ast::TypeNode is the concrete class for type representations.
// PatternNode is a conceptual base for future pattern matching AST nodes.

} // namespace ast
// Note: The outer 'vyn' namespace is omitted here for brevity, assuming 'using namespace vyn;'
// or fully qualifying, e.g., vyn::ast::Node. The header uses 'namespace vyn { namespace ast { ... } }'.
```

Key members and features:

-   **`vyn::SourceLocation`**: Defined in `vyn/source_location.hpp`. Stores filename, line, and column.
-   **`vyn::ast::Node::loc`**: An instance of `SourceLocation`.
-   **`vyn::ast::Node::inferredTypeName`**: A string to store type information inferred during semantic analysis.
-   **`vyn::ast::Node::getType() const`**: Pure virtual, returns specific `NodeType`.
-   **`vyn::ast::Node::toString() const`**: Pure virtual, for string representation (debugging).
-   **`vyn::ast::Node::accept(Visitor& visitor)`**: Pure virtual, for the Visitor pattern.
-   **Base Derivatives**:
    -   `vyn::ast::Expression`: Base for expression nodes.
    -   `vyn::ast::Statement`: Base for statement nodes.
    -   `vyn::ast::Declaration`: Base for declaration nodes, inherits from `Statement`.
-   **`vyn::ast::TypeNode`**: The concrete class for type representations (see `AST_Types.md`).
-   **Pattern Nodes**: Planned for pattern matching (see `AST_Patterns.md` and `AST_Roadmap.md`).

## 2. Node Type Enumeration (`vyn::ast::NodeType`)

The `NodeType` enum, defined in `include/vyn/parser/ast.hpp`, identifies each concrete AST node type.

```cpp
// Enum in include/vyn/parser/ast.hpp
namespace vyn::ast {
enum class NodeType {
    // Literals
    IDENTIFIER,
    INTEGER_LITERAL,
    FLOAT_LITERAL,
    STRING_LITERAL,
    BOOLEAN_LITERAL,
    ARRAY_LITERAL,
    OBJECT_LITERAL,
    NIL_LITERAL,

    // Expressions
    UNARY_EXPRESSION,
    BINARY_EXPRESSION,
    CALL_EXPRESSION,
    CONSTRUCTION_EXPRESSION,
    ARRAY_INITIALIZATION_EXPRESSION,
    MEMBER_EXPRESSION,
    ASSIGNMENT_EXPRESSION,
    BORROW_EXPRESSION,
    POINTER_DEREF_EXPRESSION,
    ADDR_OF_EXPRESSION,
    FROM_INT_TO_LOC_EXPRESSION,
    ARRAY_ELEMENT_EXPRESSION,
    LOCATION_EXPRESSION,
    LIST_COMPREHENSION,
    IF_EXPRESSION,          // Added as per ast.hpp
    RANGE_EXPRESSION,       // Added as per ast.hpp

    // Statements
    BLOCK_STATEMENT,
    EXPRESSION_STATEMENT,
    IF_STATEMENT,
    FOR_STATEMENT,
    WHILE_STATEMENT,
    RETURN_STATEMENT,
    BREAK_STATEMENT,
    CONTINUE_STATEMENT,
    TRY_STATEMENT,
    THROW_STATEMENT,        // Added as per ast.hpp
    SCOPED_STATEMENT,       // Added as per ast.hpp
    // PATTERN_ASSIGNMENT_STATEMENT, // Not in ast.hpp, but in roadmap

    // Declarations
    VARIABLE_DECLARATION,
    FUNCTION_DECLARATION,   // Also FunctionParameter helper struct
    TYPE_ALIAS_DECLARATION,
    IMPORT_DECLARATION,     // Also ImportSpecifier helper struct
    STRUCT_DECLARATION,
    CLASS_DECLARATION,
    FIELD_DECLARATION,
    IMPL_DECLARATION,
    ENUM_DECLARATION,       // Also EnumVariant helper struct
    ENUM_VARIANT,           // Helper struct, also a node type
    GENERIC_PARAMETER,
    TEMPLATE_DECLARATION,
    TRAIT_DECLARATION,      // Added as per ast.hpp
    METHOD_SIGNATURE,       // Added as per ast.hpp

    // Types
    TYPE_NODE,              // General type representation
    BASIC_TYPE_NODE,        // e.g. i32, string (Added as per ast.hpp)
    POINTER_TYPE_NODE,      // e.g. *i32 (Added as per ast.hpp)
    REFERENCE_TYPE_NODE,    // e.g. &i32 (Added as per ast.hpp)
    ARRAY_TYPE_NODE,        // e.g. [i32; 5] (Added as per ast.hpp)
    FUNCTION_TYPE_NODE,     // e.g. fn(i32) -> i32 (Added as per ast.hpp)
    TUPLE_TYPE_NODE,        // e.g. (i32, bool) (Added as per ast.hpp)
    OPTIONAL_TYPE_NODE,     // e.g. ?i32 (Added as per ast.hpp)
    GENERIC_INSTANCE_TYPE_NODE, // e.g. Vec<i32> (Added as per ast.hpp)

    // Other
    MODULE,
    FUNCTION_PARAMETER,     // Helper struct, also a node type
    IMPORT_SPECIFIER        // Helper struct, also a node type
};
} // namespace vyn::ast
```

This enum is kept up-to-date with all implemented AST node classes in `include/vyn/parser/ast.hpp`.

## 3. Visitor Pattern

The AST employs the Visitor design pattern.

```cpp
// Visitor interface in include/vyn/parser/ast.hpp
namespace vyn::ast {

// Forward declarations of all concrete AST node types are needed here.
// For brevity, only a few are listed as examples. The actual header lists all.
class Identifier;
class IntegerLiteral;
// ... many more ...
class Module;
class TypeNode; // General TypeNode
class BasicTypeNode;
class PointerTypeNode;
// ... other specific TypeNode derivatives ...
class FunctionParameter;
class ImportSpecifier;
class EnumVariant;


class Visitor {
public:
    virtual ~Visitor() = default;

    // Literals
    virtual void visit(Identifier* node) = 0;
    virtual void visit(IntegerLiteral* node) = 0;
    virtual void visit(FloatLiteral* node) = 0;
    virtual void visit(StringLiteral* node) = 0;
    virtual void visit(BooleanLiteral* node) = 0;
    virtual void visit(ArrayLiteral* node) = 0;
    virtual void visit(ObjectLiteral* node) = 0;
    virtual void visit(NilLiteral* node) = 0;

    // Expressions
    virtual void visit(UnaryExpression* node) = 0;
    virtual void visit(BinaryExpression* node) = 0;
    virtual void visit(CallExpression* node) = 0;
    virtual void visit(ConstructionExpression* node) = 0;
    virtual void visit(ArrayInitializationExpression* node) = 0;
    virtual void visit(MemberExpression* node) = 0;
    virtual void visit(AssignmentExpression* node) = 0;
    virtual void visit(BorrowExpression* node) = 0;
    virtual void visit(PointerDerefExpression* node) = 0;
    virtual void visit(AddrOfExpression* node) = 0;
    virtual void visit(FromIntToLocExpression* node) = 0;
    virtual void visit(ArrayElementExpression* node) = 0;
    virtual void visit(LocationExpression* node) = 0;
    virtual void visit(ListComprehension* node) = 0;
    virtual void visit(IfExpression* node) = 0;        // Added as per ast.hpp
    virtual void visit(RangeExpression* node) = 0;     // Added as per ast.hpp

    // Statements
    virtual void visit(BlockStatement* node) = 0;
    virtual void visit(ExpressionStatement* node) = 0;
    virtual void visit(IfStatement* node) = 0;
    virtual void visit(ForStatement* node) = 0;
    virtual void visit(WhileStatement* node) = 0;
    virtual void visit(ReturnStatement* node) = 0;
    virtual void visit(BreakStatement* node) = 0;
    virtual void visit(ContinueStatement* node) = 0;
    virtual void visit(TryStatement* node) = 0;
    virtual void visit(ThrowStatement* node) = 0;      // Added as per ast.hpp
    virtual void visit(ScopedStatement* node) = 0;     // Added as per ast.hpp
    // virtual void visit(PatternAssignmentStatement* node) = 0; // Not in ast.hpp

    // Declarations
    virtual void visit(VariableDeclaration* node) = 0;
    virtual void visit(FunctionDeclaration* node) = 0;
    virtual void visit(TypeAliasDeclaration* node) = 0;
    virtual void visit(ImportDeclaration* node) = 0;
    virtual void visit(StructDeclaration* node) = 0;
    virtual void visit(ClassDeclaration* node) = 0;
    virtual void visit(FieldDeclaration* node) = 0;
    virtual void visit(ImplDeclaration* node) = 0;
    virtual void visit(EnumDeclaration* node) = 0;
    // EnumVariant is a Node, so it needs a visit method
    virtual void visit(EnumVariant* node) = 0;
    virtual void visit(GenericParameter* node) = 0;
    virtual void visit(TemplateDeclaration* node) = 0;
    virtual void visit(TraitDeclaration* node) = 0;    // Added as per ast.hpp
    virtual void visit(MethodSignature* node) = 0;   // Added as per ast.hpp

    // Types (Specific visit methods for each TypeNode derivative)
    virtual void visit(TypeNode* node) = 0; // General fallback or for the base
    virtual void visit(BasicTypeNode* node) = 0;
    virtual void visit(PointerTypeNode* node) = 0;
    virtual void visit(ReferenceTypeNode* node) = 0;
    virtual void visit(ArrayTypeNode* node) = 0;
    virtual void visit(FunctionTypeNode* node) = 0;
    virtual void visit(TupleTypeNode* node) = 0;
    virtual void visit(OptionalTypeNode* node) = 0;
    virtual void visit(GenericInstanceTypeNode* node) = 0;


    // Other (Helper structures that are also Nodes)
    virtual void visit(Module* node) = 0;
    virtual void visit(FunctionParameter* node) = 0;
    virtual void visit(ImportSpecifier* node) = 0;

};

} // namespace ast
} // namespace vyn
```

Each concrete AST node class implements the `accept` method:

```cpp
// Example for Identifier (in its implementation file or ast.cpp)
// Assuming using namespace vyn::ast;
void Identifier::accept(Visitor& visitor) {
    visitor.visit(this);
}
```

#ifndef VYN_PARSER_AST_HPP
#define VYN_PARSER_AST_HPP

#include <string>
#include <vector>
#include <memory>
#include <variant>
#include <optional>
#include <iostream> // Added for std::cout
#include "token.hpp"
#include "source_location.hpp"

namespace vyn {
namespace ast {

// Forward declarations - ALL AST NODE TYPES must be declared here
class Node;
class Module;
class Expression;
class Statement;
class Declaration;
class Visitor;

// Literals
class Identifier;
class IntegerLiteral;
class FloatLiteral;
class StringLiteral;
class BooleanLiteral;
class ArrayLiteral;
class ObjectLiteral;
class NilLiteral;

// Expressions
class UnaryExpression;
class BinaryExpression;
class CallExpression;
class MemberExpression;
class AssignmentExpression;
class BorrowExpression;
class PointerDerefExpression;
class AddrOfExpression;
class FromIntToLocExpression;
class ArrayElementExpression;
class LocationExpression;
class ListComprehension;
class IfExpression;
class ConstructionExpression;
class ArrayInitializationExpression;
class GenericInstantiationExpression;
class LogicalExpression; // Added
class ConditionalExpression; // Added
class SequenceExpression; // Added
class FunctionExpression; // Added
class ThisExpression; // Added
class SuperExpression; // Added
class AwaitExpression; // Added

// Statements
class BlockStatement;
class ExpressionStatement;
class IfStatement;
class ForStatement;
class WhileStatement;
class ReturnStatement;
class BreakStatement;
class ContinueStatement;
class TryStatement;
class UnsafeStatement;
class EmptyStatement;
class ExternStatement; // Added
class ThrowStatement; // Added
class MatchStatement; // Added
class YieldStatement; // Added
class YieldReturnStatement; // Added
class AssertStatement; // Added

// Declarations
class VariableDeclaration;
class FunctionDeclaration;
class TypeAliasDeclaration;
class ImportDeclaration;
class StructDeclaration;
class ClassDeclaration;
class FieldDeclaration;
class ImplDeclaration;
class EnumDeclaration;
class EnumVariant;
class GenericParameter;
class TemplateDeclaration;
class TraitDeclaration; // Added
class NamespaceDeclaration; // Added

// Types (These are typically part of TypeNode or used as type specifiers,
// but if they are distinct visitable AST nodes, they need to be here)
class TypeNode;
class TypeName; // Added
class PointerType; // Added
class ArrayType; // Added
class FunctionType; // Added
class OptionalType; // Added
class TupleTypeNode; // ADDED FORWARD DECLARATION


// --- Enums used by AST nodes ---
enum class BorrowKind {
    MUTABLE_BORROW,
    IMMUTABLE_VIEW
    // Add other kinds if necessary, e.g., UNIQUE_OWNERSHIP_TRANSFER
};

enum class OwnershipKind {
    MY,    // Unique ownership
    OUR,   // Shared ownership (e.g., reference counted)
    THEIR, // Borrowed/Viewed (non-owning), further specified by BorrowKind if applicable
    PTR    // Raw pointer (potentially non-owning, unsafe)
    // Add other kinds as needed
};


// --- Type aliases for smart pointers ---
using NodePtr = std::unique_ptr<Node>;
using ExprPtr = std::unique_ptr<Expression>;
using StmtPtr = std::unique_ptr<Statement>;
using DeclPtr = std::unique_ptr<Declaration>;
using TypeNodePtr = std::unique_ptr<TypeNode>;
using IdentifierPtr = std::unique_ptr<Identifier>;
using ArrayLiteralPtr = std::unique_ptr<ArrayLiteral>;
using BorrowExpressionPtr = std::unique_ptr<BorrowExpression>;

// --- Helper structs ---
// ... (FunctionParameter, ImportSpecifier) ...
// (These should be fine as long as they don't depend on AST node classes not yet declared)
struct FunctionParameter {
    std::unique_ptr<Identifier> name;
    TypeNodePtr typeNode; 
    FunctionParameter(std::unique_ptr<Identifier> n, TypeNodePtr tn = nullptr)
        : name(std::move(n)), typeNode(std::move(tn)) {}
};

struct ObjectProperty {
    SourceLocation loc;
    IdentifierPtr key; // Property key
    ExprPtr value;     // Property value

    ObjectProperty(SourceLocation loc, IdentifierPtr key, ExprPtr value)
        : loc(loc), key(std::move(key)), value(std::move(value)) {}
};

struct ImportSpecifier {
    std::unique_ptr<Identifier> importedName;
    std::unique_ptr<Identifier> localName;
    ImportSpecifier(std::unique_ptr<Identifier> imported, std::unique_ptr<Identifier> local = nullptr)
        : importedName(std::move(imported)), localName(std::move(local)) {}
};

// enum class NodeType must be defined before Visitor
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
    MEMBER_EXPRESSION,
    ASSIGNMENT_EXPRESSION,
    BORROW_EXPRESSION, 
    POINTER_DEREF_EXPRESSION, 
    ADDR_OF_EXPRESSION,       
    FROM_INT_TO_LOC_EXPRESSION, 
    ARRAY_ELEMENT_EXPRESSION, 
    LOCATION_EXPRESSION, 
    LIST_COMPREHENSION, 
    IF_EXPRESSION, 
    GENERIC_INSTANTIATION_EXPRESSION, 
    CONSTRUCTION_EXPRESSION,
    ARRAY_INITIALIZATION_EXPRESSION,
    LOGICAL_EXPRESSION, // Added
    CONDITIONAL_EXPRESSION, // Added
    SEQUENCE_EXPRESSION, // Added
    FUNCTION_EXPRESSION, // Added
    THIS_EXPRESSION, // Added
    SUPER_EXPRESSION, // Added
    AWAIT_EXPRESSION, // Added


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
    UNSAFE_STATEMENT, 
    EMPTY_STATEMENT, 
    EXTERN_STATEMENT, // Added
    THROW_STATEMENT, // Added
    MATCH_STATEMENT, // Added
    YIELD_STATEMENT, // Added
    YIELD_RETURN_STATEMENT, // Added
    ASSERT_STATEMENT, // Added

    // Declarations
    VARIABLE_DECLARATION,
    FUNCTION_DECLARATION,
    TYPE_ALIAS_DECLARATION,
    IMPORT_DECLARATION,
    STRUCT_DECLARATION,
    CLASS_DECLARATION,
    FIELD_DECLARATION,
    IMPL_DECLARATION,
    ENUM_DECLARATION,
    ENUM_VARIANT,
    GENERIC_PARAMETER, 
    TEMPLATE_DECLARATION,
    TRAIT_DECLARATION, // Added
    NAMESPACE_DECLARATION, // Added

    // Other
    TYPE_NODE, 
    MODULE,
    // Node types for TypeName, PointerType etc. if they are distinct visitable nodes
    TYPE_NAME, // Added
    POINTER_TYPE, // Added
    ARRAY_TYPE, // Added
    FUNCTION_TYPE, // Added
    OPTIONAL_TYPE, // Added
    TUPLE_TYPE // ADDED
};


// Visitor Interface
class Visitor {
public:
    virtual ~Visitor() = default;

    // Literals
    virtual void visit(Identifier* node) = 0;
    virtual void visit(IntegerLiteral* node) = 0;
    virtual void visit(FloatLiteral* node) = 0;
    virtual void visit(StringLiteral* node) = 0;
    virtual void visit(BooleanLiteral* node) = 0;
    virtual void visit(ObjectLiteral* node) = 0;
    virtual void visit(NilLiteral* node) = 0;

    // Expressions
    virtual void visit(UnaryExpression* node) = 0;
    virtual void visit(BinaryExpression* node) = 0;
    virtual void visit(CallExpression* node) = 0;
    virtual void visit(MemberExpression* node) = 0;
    virtual void visit(AssignmentExpression* node) = 0;
    virtual void visit(ArrayLiteral* node) = 0;
    virtual void visit(BorrowExpression* node) = 0;
    virtual void visit(PointerDerefExpression* node) = 0;
    virtual void visit(AddrOfExpression* node) = 0;
    virtual void visit(FromIntToLocExpression* node) = 0;
    virtual void visit(ArrayElementExpression* node) = 0;
    virtual void visit(LocationExpression* node) = 0;
    virtual void visit(ListComprehension* node) = 0;
    virtual void visit(IfExpression* node) = 0;
    virtual void visit(ConstructionExpression* node) = 0;
    virtual void visit(ArrayInitializationExpression* node) = 0;
    virtual void visit(GenericInstantiationExpression* node) = 0;
    virtual void visit(LogicalExpression* node) = 0;
    virtual void visit(ConditionalExpression* node) = 0;
    virtual void visit(SequenceExpression* node) = 0;
    virtual void visit(FunctionExpression* node) = 0;
    virtual void visit(ThisExpression* node) = 0;
    virtual void visit(SuperExpression* node) = 0;
    virtual void visit(AwaitExpression* node) = 0;

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
    virtual void visit(UnsafeStatement* node) = 0;
    virtual void visit(EmptyStatement* node) = 0;
    virtual void visit(ExternStatement* node) = 0;
    virtual void visit(ThrowStatement* node) = 0;
    virtual void visit(MatchStatement* node) = 0;
    virtual void visit(YieldStatement* node) = 0;
    virtual void visit(YieldReturnStatement* node) = 0;
    virtual void visit(AssertStatement* node) = 0;

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
    virtual void visit(EnumVariant* node) = 0;
    virtual void visit(GenericParameter* node) = 0;
    virtual void visit(TemplateDeclaration* node) = 0;
    virtual void visit(TraitDeclaration* node) = 0;
    virtual void visit(NamespaceDeclaration* node) = 0;
    
    // Other
    virtual void visit(TypeNode* node) = 0; 
    virtual void visit(Module* node) = 0;

    // Types (if they are distinct visitable nodes)
    virtual void visit(TypeName* node) = 0;
    virtual void visit(PointerType* node) = 0;
    virtual void visit(ArrayType* node) = 0;
    virtual void visit(FunctionType* node) = 0;
    virtual void visit(OptionalType* node) = 0;
    virtual void visit(TupleTypeNode* node) = 0; // ADDED visit method for TupleTypeNode
};

// Base AST Node
class Node {
public:
    SourceLocation loc; 
    std::string inferredTypeName; 
    std::shared_ptr<TypeNode> type;  // Add type member

    Node(SourceLocation loc) : loc(loc) {}
    virtual ~Node() = default;
    virtual NodeType getType() const = 0; 
    virtual std::string toString() const = 0; 
    virtual void accept(Visitor& visitor) = 0; 
};

// Base Expression Node
class Expression : public Node {
public:
    Expression(SourceLocation loc) : Node(loc) {}
};

// Base Statement Node
class Statement : public Node {
public:
    Statement(SourceLocation loc) : Node(loc) {}
};

// Base Declaration Node (Declarations are Statements)
class Declaration : public Statement {
public:
    Declaration(SourceLocation loc) : Statement(loc) {}
};
    

// --- START OF AST NODE CLASS DEFINITIONS ---
// (Ensure all classes forward-declared above are defined here or in included files)

// --- Literals ---
class Identifier : public Expression {
public:
    std::string name;

    Identifier(SourceLocation loc, std::string name);
    NodeType getType() const override;
    std::string toString() const override;
    void accept(Visitor& visitor) override;
};

class IntegerLiteral : public Expression {
public:
    int64_t value;

    IntegerLiteral(SourceLocation loc, int64_t value);
    virtual ~IntegerLiteral(); // Added destructor declaration
    NodeType getType() const override;
    std::string toString() const override;
    void accept(Visitor& visitor) override;
};

class FloatLiteral : public Expression {
public:
    double value;

    FloatLiteral(SourceLocation loc, double value);
    NodeType getType() const override;
    std::string toString() const override;
    void accept(Visitor& visitor) override;
};

class StringLiteral : public Expression {
public:
    std::string value;

    StringLiteral(SourceLocation loc, std::string value);
    NodeType getType() const override;
    std::string toString() const override;
    void accept(Visitor& visitor) override;
};

class BooleanLiteral : public Expression {
public:
    bool value;

    BooleanLiteral(SourceLocation loc, bool value);
    NodeType getType() const override;
    std::string toString() const override;
    void accept(Visitor& visitor) override;
};

// Represents an array literal expression: [elem1, elem2, ...]
class ArrayLiteral : public Expression { // Renamed from ArrayLiteralNode
public:
    std::vector<ExprPtr> elements;

    ArrayLiteral(SourceLocation loc, std::vector<ExprPtr> elements); // Renamed from ArrayLiteralNode
    NodeType getType() const override { return NodeType::ARRAY_LITERAL; } // Updated NodeType
    void accept(Visitor& visitor) override; 
    std::string toString() const override; 
};

// New: ObjectLiteral
class ObjectLiteral : public Expression {
public:
    TypeNodePtr typePath; // Optional type path for typed object literals
    std::vector<ObjectProperty> properties;

    ObjectLiteral(SourceLocation loc, TypeNodePtr typePath, std::vector<ObjectProperty> properties);
    ~ObjectLiteral() override; // Ensure virtual destructor
    NodeType getType() const override;
    std::string toString() const override;
    void accept(Visitor& visitor) override;
};


// Represents a borrow or view expression: borrow expr, view expr
class BorrowExpression : public Expression { // Renamed from BorrowExprNode
public:
    ExprPtr expression; 
    BorrowKind kind; // Uses the globally defined BorrowKind

    BorrowExpression(SourceLocation loc, ExprPtr expression, BorrowKind kind); // Renamed from BorrowExprNode
    NodeType getType() const override { return NodeType::BORROW_EXPRESSION; } // Updated NodeType
    void accept(Visitor& visitor) override; 
    std::string toString() const override; 
};

// Represents pointer dereference: at(ptr)
class PointerDerefExpression : public Expression {
public:
    ExprPtr pointer;
    PointerDerefExpression(SourceLocation loc, ExprPtr pointer);
    NodeType getType() const override;
    std::string toString() const override;
    void accept(Visitor& visitor) override;
};

// Represents address-of: addr(loc)
class AddrOfExpression : public Expression {
    ExprPtr location;
public:
    AddrOfExpression(SourceLocation loc, ExprPtr location);
    NodeType getType() const override;
    std::string toString() const override;
    void accept(Visitor& visitor) override;
    ExprPtr& getLocation() { return location; }
    const ExprPtr& getLocation() const { return location; }
};

// Represents conversion from integer to loc<T>: from<Type>(addr)
class FromIntToLocExpression : public Expression {
public:
    FromIntToLocExpression(const SourceLocation& loc, ExprPtr addr_expr, TypeNodePtr target_ty)
        : Expression(loc), address_expr(std::move(addr_expr)), target_type(std::move(target_ty)) {}

    const ExprPtr& getAddressExpression() const { return address_expr; }
    const TypeNodePtr& getTargetType() const { return target_type; }

    NodeType getType() const override;
    std::string toString() const override;
    void accept(Visitor& visitor) override;

private:
    ExprPtr address_expr;
    TypeNodePtr target_type;
};

// New: ArrayElementExpression - Represents element access: array[index]
class ArrayElementExpression : public Expression {
public:
    ExprPtr array;  // The array expression
    ExprPtr index;  // The index expression

    ArrayElementExpression(SourceLocation loc, ExprPtr array, ExprPtr index);
    ~ArrayElementExpression() override; // Was: default;
    NodeType getType() const override;
    std::string toString() const override;
    void accept(Visitor& visitor) override;
};

// New: LocationExpression - Represents loc(expression)
class LocationExpression : public Expression {
public:
    ExprPtr expression; // The expression whose location is being taken

    LocationExpression(SourceLocation loc, ExprPtr expression);
    ~LocationExpression() override; // Was: default;
    NodeType getType() const override;
    std::string toString() const override;
    void accept(Visitor& visitor) override;
};


// New NilLiteral Node
class NilLiteral : public Expression {
public:
    NilLiteral(SourceLocation loc);
    NodeType getType() const override;
    std::string toString() const override;
    void accept(Visitor& visitor) override;
};

// New: Represents list comprehension: [expr for var in iterable if condition]
class ListComprehension : public Expression {
public:
    ExprPtr elementExpr;        // The expression for each element
    IdentifierPtr loopVariable;   // The variable in the loop (e.g., 'x' in 'for x in ...')
    ExprPtr iterableExpr;       // The expression defining the iterable (e.g., 'my_list')
    ExprPtr conditionExpr;      // Optional condition (e.g., 'if x > 0')

    ListComprehension(SourceLocation loc, ExprPtr elementExpr, IdentifierPtr loopVariable, ExprPtr iterableExpr, ExprPtr conditionExpr = nullptr);
    ~ListComprehension() override; // Was: default;
    NodeType getType() const override;
    std::string toString() const override;
    void accept(Visitor& visitor) override;
};

// --- Expressions ---
class UnaryExpression : public Expression {
public:
    token::Token op; // The operator token (full type known from token.hpp)
    ExprPtr operand;    // The expression being operated on

    // Constructor takes const reference for op, which is then copied to the member
    UnaryExpression(SourceLocation loc, const token::Token& op, ExprPtr operand);
    virtual ~UnaryExpression();
    NodeType getType() const override;
    std::string toString() const override;
    void accept(Visitor& visitor) override;
};

class BinaryExpression : public Expression {
public:
    ExprPtr left;
    token::Token op; // The operator token
    ExprPtr right;

    BinaryExpression(SourceLocation loc, ExprPtr left, const token::Token& op, ExprPtr right);
    virtual ~BinaryExpression();
    NodeType getType() const override;
    std::string toString() const override;
    void accept(Visitor& visitor) override;
};

class CallExpression : public Expression {
public:
    ExprPtr callee;
    std::vector<ExprPtr> arguments;

    CallExpression(SourceLocation loc, ExprPtr callee, std::vector<ExprPtr> arguments);
    virtual ~CallExpression();
    NodeType getType() const override;
    std::string toString() const override;
    void accept(Visitor& visitor) override;
};

class ConstructionExpression : public Expression {
public:
    TypeNodePtr constructedType; // The type being constructed (e.g., MyStruct, my_module::MyType<T>)
    std::vector<ExprPtr> arguments;

    ConstructionExpression(SourceLocation loc, TypeNodePtr constructedType, std::vector<ExprPtr> arguments);
    // virtual ~ConstructionExpression() = default; // Default destructor is fine
    NodeType getType() const override;
    std::string toString() const override;
    void accept(Visitor& visitor) override;
};

class ArrayInitializationExpression : public Expression {
public:
    TypeNodePtr elementType;    // The type of the elements in the array
    ExprPtr sizeExpression;     // The expression defining the size of the array
    // For [Type; Size](), arguments are implicit (default initialization)

    ArrayInitializationExpression(SourceLocation loc, TypeNodePtr elementType, ExprPtr sizeExpression);
    // virtual ~ArrayInitializationExpression() = default; // Default destructor is fine
    NodeType getType() const override;
    std::string toString() const override;
    void accept(Visitor& visitor) override;
};

class GenericInstantiationExpression : public Expression {
public:
    ExprPtr baseExpression; // The expression being genericized (e.g., 'myFunc', 'MyType')
    std::vector<TypeNodePtr> genericArguments;
    SourceLocation lt_loc; // Location of '<'
    SourceLocation gt_loc; // Location of '>'

    GenericInstantiationExpression(SourceLocation loc, ExprPtr base, std::vector<TypeNodePtr> args, SourceLocation lt_loc, SourceLocation gt_loc);
    NodeType getType() const override;
    std::string toString() const override;
    void accept(Visitor& visitor) override;
};

class MemberExpression : public Expression {
public:
    ExprPtr object;   // The object whose member is being accessed
    ExprPtr property; // The property being accessed (Identifier or Expression if computed)
    bool computed;    // True if property is accessed with [], false for .

    MemberExpression(SourceLocation loc, ExprPtr object, ExprPtr property, bool computed);
    virtual ~MemberExpression();
    NodeType getType() const override;
    std::string toString() const override;
    void accept(Visitor& visitor) override;
};

class AssignmentExpression : public Expression {
public:
    ExprPtr left;  // LValue (Identifier or MemberExpression)
    token::Token op; // Assignment operator (e.g., =, +=)
    ExprPtr right; // RValue

    AssignmentExpression(SourceLocation loc, ExprPtr left, const token::Token& op, ExprPtr right);
    virtual ~AssignmentExpression();
    NodeType getType() const override;
    std::string toString() const override;
    void accept(Visitor& visitor) override;
};


// --- Statements ---
class BlockStatement : public Statement {
public:
    std::vector<StmtPtr> body;

    BlockStatement(SourceLocation loc, std::vector<StmtPtr> body);
    virtual ~BlockStatement();
    NodeType getType() const override;
    std::string toString() const override;
    void accept(Visitor& visitor) override;
};

// New EmptyStatement AST node
class EmptyStatement : public Statement {
public:
    EmptyStatement(SourceLocation loc);
    ~EmptyStatement() override = default;

    NodeType getType() const override;
    std::string toString() const override;
    void accept(Visitor& visitor) override;
};

// TryStatement AST node (define after BlockStatement)
class TryStatement : public Statement {
public:
    std::unique_ptr<BlockStatement> tryBlock;
    std::optional<std::string> catchIdent;
    std::unique_ptr<BlockStatement> catchBlock;
    std::unique_ptr<BlockStatement> finallyBlock;

    TryStatement(const SourceLocation& loc, std::unique_ptr<BlockStatement> tryBlock,
                 std::optional<std::string> catchIdent,
                 std::unique_ptr<BlockStatement> catchBlock,
                 std::unique_ptr<BlockStatement> finallyBlock);
    NodeType getType() const override;
    std::string toString() const override;
    void accept(Visitor& visitor) override;
};

class ExpressionStatement : public Statement {
public:
    ExprPtr expression;

    ExpressionStatement(SourceLocation loc, ExprPtr expression);
    virtual ~ExpressionStatement();
    NodeType getType() const override;
    std::string toString() const override;
    void accept(Visitor& visitor) override;
};

class IfStatement : public Statement {
public:
    ExprPtr test;
    StmtPtr consequent;
    StmtPtr alternate; // Optional, can be nullptr

    IfStatement(SourceLocation loc, ExprPtr test, StmtPtr consequent, StmtPtr alternate = nullptr);
    virtual ~IfStatement();
    NodeType getType() const override;
    std::string toString() const override;
    void accept(Visitor& visitor) override;
};

class ForStatement : public Statement {
public:
    NodePtr init;   // VariableDeclaration or ExpressionStatement or nullptr
    ExprPtr test;   // Expression or nullptr
    ExprPtr update; // Expression or nullptr
    StmtPtr body;

    ForStatement(SourceLocation loc, NodePtr init, ExprPtr test, ExprPtr update, StmtPtr body);
    virtual ~ForStatement();
    NodeType getType() const override;
    std::string toString() const override;
    void accept(Visitor& visitor) override;
};

class WhileStatement : public Statement {
public:
    ExprPtr test;
    StmtPtr body;

    WhileStatement(SourceLocation loc, ExprPtr test, StmtPtr body);
    virtual ~WhileStatement();
    NodeType getType() const override;
    std::string toString() const override;
    void accept(Visitor& visitor) override;
};

class ReturnStatement : public Statement {
public:
    ExprPtr argument; // Optional, can be nullptr

    ReturnStatement(SourceLocation loc, ExprPtr argument = nullptr);
    virtual ~ReturnStatement();
    NodeType getType() const override;
    std::string toString() const override;
    void accept(Visitor& visitor) override;
};

class BreakStatement : public Statement {
public:
    BreakStatement(SourceLocation loc);
    virtual ~BreakStatement();
    NodeType getType() const override;
    std::string toString() const override;
    void accept(Visitor& visitor) override;
};

class ContinueStatement : public Statement {
public:
    ContinueStatement(SourceLocation loc);
    virtual ~ContinueStatement();
    NodeType getType() const override;
    std::string toString() const override;
    void accept(Visitor& visitor) override;
};

// Forward declare TypeNode for use in declarations (already done above with TypeNodePtr)
// class TypeNode; // No longer TypeAnnotation
// using TypeNodePtr = std::unique_ptr<TypeNode>; // No longer TypeAnnotationPtr


// --- Other ---

// Generic Parameter Node
class GenericParameter : public Node { // Renamed from GenericParamNode
public:
    std::unique_ptr<Identifier> name;
    std::vector<TypeNodePtr> bounds; // e.g. T: Bound1 + Bound2 (replaces TypeAnnotationPtr)

    GenericParameter(SourceLocation loc, std::unique_ptr<Identifier> name, std::vector<TypeNodePtr> bounds = {}); // Renamed from GenericParamNode
    NodeType getType() const override;
    std::string toString() const override;
    void accept(Visitor& visitor) override;
}; // End of GenericParameter


// --- Full Class Definition for TemplateDeclaration ---
// Placed after Node, Statement, Declaration, NodeType, Visitor, Identifier, GenericParameter are defined.
class TemplateDeclaration : public Declaration { // Renamed from TemplateDeclarationNode
public:
    std::unique_ptr<Identifier> name;
    std::vector<std::unique_ptr<GenericParameter>> genericParams; 
    DeclPtr body;

    TemplateDeclaration(SourceLocation loc, std::unique_ptr<Identifier> name, std::vector<std::unique_ptr<GenericParameter>> genericParams, DeclPtr body); // Renamed from TemplateDeclarationNode
    NodeType getType() const override;
    std::string toString() const override;
    void accept(Visitor& visitor) override;
};
// Define Ptr alias after the class is fully defined.
using TemplateDeclarationPtr = std::unique_ptr<TemplateDeclaration>; // Renamed from TemplateDeclarationNodePtr


// Module (Root of the AST)
class Module : public Node {
public:
    std::vector<StmtPtr> body; // Sequence of statements (including declarations)

    Module(SourceLocation loc, std::vector<StmtPtr> body);
    NodeType getType() const override;
    std::string toString() const override;
    void accept(Visitor& visitor) override;
};

// --- New IfExpression Definition ---
class IfExpression : public Expression {
public:
    ExprPtr condition;
    ExprPtr thenBranch;
    ExprPtr elseBranch; // Vyn requires else for if-expressions

    IfExpression(SourceLocation loc, ExprPtr condition, ExprPtr thenBranch, ExprPtr elseBranch);
    ~IfExpression() override = default; // Or implement if needed

    NodeType getType() const override;
    std::string toString() const override;
    void accept(Visitor& visitor) override;
};

// New UnsafeStatement AST node
class UnsafeStatement : public Statement {
public:
    std::unique_ptr<BlockStatement> block; // The unsafe block

    UnsafeStatement(SourceLocation loc, std::unique_ptr<BlockStatement> blockStmt)
        : Statement(loc), block(std::move(blockStmt)) {}

    NodeType getType() const override { return NodeType::UNSAFE_STATEMENT; }
    void accept(Visitor& visitor) override { visitor.visit(this); }

    std::string toString() const override; // Declaration of toString
};

class TypeNode : public Node {
public:
    // Define TypeCategory as a nested enum
    enum class Category {
        IDENTIFIER,
        POINTER,
        ARRAY,
        FUNCTION,
        TUPLE,
        OPTIONAL,
        REFERENCE,
        SLICE,
        STRUCT,
        UNKNOWN
    };

    TypeNode(SourceLocation loc) : Node(loc) {}
    virtual ~TypeNode() = default;

    // For debugging: print the category of the type node
    void printCategory() const {
        // This function is just for debugging purposes and can be removed if not needed
        switch (getCategory()) {
            case Category::IDENTIFIER:     std::cout << "TypeNode Category: IDENTIFIER\n"; break;
            case Category::POINTER:        std::cout << "TypeNode Category: POINTER\n"; break;
            case Category::ARRAY:          std::cout << "TypeNode Category: ARRAY\n"; break;
            case Category::FUNCTION:       std::cout << "TypeNode Category: FUNCTION\n"; break;
            case Category::TUPLE:          std::cout << "TypeNode Category: TUPLE\n"; break;
            case Category::OPTIONAL:       std::cout << "TypeNode Category: OPTIONAL\n"; break;
            case Category::REFERENCE:      std::cout << "TypeNode Category: REFERENCE\n"; break;
            case Category::SLICE:          std::cout << "TypeNode Category: SLICE\n"; break;
            case Category::STRUCT:         std::cout << "TypeNode Category: STRUCT\n"; break;
            case Category::UNKNOWN:        std::cout << "TypeNode Category: UNKNOWN\n"; break;
        }
    }

    virtual Category getCategory() const = 0; // Pure virtual function for getting the category
    // NodeType getType() const override { return NodeType::TYPE_NODE; } // Each derived type should specify

    virtual bool isIntegerTy() const { return false; }
    virtual bool isLocationTy() const { return false; }
    virtual std::unique_ptr<TypeNode> clone() const = 0; // Add pure virtual clone method
};

// --- Full Class Definition for TypeName ---
class TypeName : public TypeNode {
public:
    std::unique_ptr<Identifier> identifier;
    std::vector<TypeNodePtr> genericArgs; // For generics like Vec<T>

    TypeName(SourceLocation loc, std::unique_ptr<Identifier> id, std::vector<TypeNodePtr> args = {});
    NodeType getType() const override;
    std::string toString() const override;
    void accept(Visitor& visitor) override;

    Category getCategory() const override { return Category::IDENTIFIER; } // TypeName is an identifier type
    bool isIntegerTy() const override; // Override
    std::unique_ptr<TypeNode> clone() const override; // Override clone
};
// --- End of TypeName Definition ---

// --- Full Class Definition for PointerType ---
class PointerType : public TypeNode {
public:
    TypeNodePtr pointeeType; // The type being pointed to

    PointerType(SourceLocation loc, TypeNodePtr pointee);
    NodeType getType() const override;
    std::string toString() const override;
    void accept(Visitor& visitor) override;

    Category getCategory() const override { return Category::POINTER; }
    bool isLocationTy() const override { return true; } // Override
    std::unique_ptr<TypeNode> clone() const override; // Override clone
};
// --- End of PointerType Definition ---

// --- Full Class Definition for ArrayType ---
class ArrayType : public TypeNode {
public:
    TypeNodePtr elementType; // The type of the array elements
    ExprPtr sizeExpression;  // The size of the array, if known

    ArrayType(SourceLocation loc, TypeNodePtr elementType, ExprPtr sizeExpression = nullptr);
    NodeType getType() const override;
    std::string toString() const override;
    void accept(Visitor& visitor) override;

    Category getCategory() const override { return Category::ARRAY; }
    std::unique_ptr<TypeNode> clone() const override; // Override clone
};
// --- End of ArrayType Definition ---

// --- Full Class Definition for FunctionType ---
class FunctionType : public TypeNode {
public:
    std::vector<TypeNodePtr> parameterTypes; // The types of the function parameters
    TypeNodePtr returnType;                 // The return type of the function

    FunctionType(SourceLocation loc, std::vector<TypeNodePtr> params, TypeNodePtr returnType);
    NodeType getType() const override;
    std::string toString() const override;
    void accept(Visitor& visitor) override;

    Category getCategory() const override { return Category::FUNCTION; }
    std::unique_ptr<TypeNode> clone() const override; // Override clone
};
// --- End of FunctionType Definition ---

// --- Full Class Definition for OptionalType ---
class OptionalType : public TypeNode {
public:
    TypeNodePtr containedType; // The type contained within the optional

    OptionalType(SourceLocation loc, TypeNodePtr containedType);
    NodeType getType() const override;
    std::string toString() const override;
    void accept(Visitor& visitor) override;

    Category getCategory() const override { return Category::OPTIONAL; }
    std::unique_ptr<TypeNode> clone() const override; // Override clone
};
// --- End of OptionalType Definition ---

// --- Full Class Definition for TupleTypeNode --- // ADDED
class TupleTypeNode : public TypeNode {
public:
    std::vector<TypeNodePtr> memberTypes;

    TupleTypeNode(SourceLocation loc, std::vector<TypeNodePtr> members);
    NodeType getType() const override;
    std::string toString() const override;
    void accept(Visitor& visitor) override;
    Category getCategory() const override { return Category::TUPLE; }
    std::unique_ptr<TypeNode> clone() const override; // Override clone
};
// --- End of TupleTypeNode Definition --- // ADDED


// --- Declarations (ensure full definitions are here) ---

// ImportDeclaration (Example, ensure others follow suit if not already complete)
class ImportDeclaration : public Declaration {
public:
    std::unique_ptr<StringLiteral> source; // e.g., "module_name" or "./file.vyn"
    std::vector<ImportSpecifier> specifiers; // For named imports: { A, B as C }
    std::unique_ptr<Identifier> defaultImport; // For default import: import X from ...
    std::unique_ptr<Identifier> namespaceImport; // For namespace import: import * as M from ...

    ImportDeclaration(SourceLocation loc,
                      std::unique_ptr<StringLiteral> source,
                      std::vector<ImportSpecifier> specifiers = {},
                      std::unique_ptr<Identifier> defaultImport = nullptr,
                      std::unique_ptr<Identifier> namespaceImport = nullptr);
    ~ImportDeclaration() override = default;
    NodeType getType() const override;
    std::string toString() const override;
    void accept(Visitor& visitor) override;
};

// VariableDeclaration
class VariableDeclaration : public Declaration {
public:
    std::unique_ptr<Identifier> id;
    bool isConst; // true for 'let', false for 'var'
    TypeNodePtr typeNode; // Optional type annotation
    ExprPtr init;         // Optional initializer

    VariableDeclaration(SourceLocation loc, std::unique_ptr<Identifier> id, bool isConst, TypeNodePtr typeNode = nullptr, ExprPtr init = nullptr);
    ~VariableDeclaration() override = default;
    NodeType getType() const override;
    std::string toString() const override;
    void accept(Visitor& visitor) override;
};

// FunctionDeclaration
class FunctionDeclaration : public Declaration {
public:
    std::unique_ptr<Identifier> id;
    std::vector<FunctionParameter> params;
    std::unique_ptr<BlockStatement> body;
    bool isAsync;
    TypeNodePtr returnTypeNode; // Optional return type annotation

    FunctionDeclaration(SourceLocation loc, std::unique_ptr<Identifier> id, std::vector<FunctionParameter> params, std::unique_ptr<BlockStatement> body, bool isAsync = false, TypeNodePtr returnTypeNode = nullptr);
    ~FunctionDeclaration() override = default;
    NodeType getType() const override;
    std::string toString() const override;
    void accept(Visitor& visitor) override;
};

// TypeAliasDeclaration
class TypeAliasDeclaration : public Declaration {
public:
    std::unique_ptr<Identifier> name;
    TypeNodePtr typeNode; // The type being aliased

    TypeAliasDeclaration(SourceLocation loc, std::unique_ptr<Identifier> name, TypeNodePtr typeNode);
    ~TypeAliasDeclaration() override = default;
    NodeType getType() const override;
    std::string toString() const override;
    void accept(Visitor& visitor) override;
};

// FieldDeclaration (typically part of Struct/Class)
class FieldDeclaration : public Declaration { // Or Node if not a standalone statement
public:
    std::unique_ptr<Identifier> name;
    TypeNodePtr typeNode;
    ExprPtr initializer; // Optional default value
    bool isMutable; // Or some other way to denote mutability/visibility

    FieldDeclaration(SourceLocation loc, std::unique_ptr<Identifier> name, TypeNodePtr typeNode, ExprPtr initializer = nullptr, bool isMutable = false);
    ~FieldDeclaration() override = default;
    NodeType getType() const override;
    std::string toString() const override;
    void accept(Visitor& visitor) override;
};

// StructDeclaration
class StructDeclaration : public Declaration {
public:
    std::unique_ptr<Identifier> name;
    std::vector<std::unique_ptr<GenericParameter>> genericParams;
    std::vector<std::unique_ptr<FieldDeclaration>> fields;

    StructDeclaration(SourceLocation loc, std::unique_ptr<Identifier> name, std::vector<std::unique_ptr<GenericParameter>> genericParams, std::vector<std::unique_ptr<FieldDeclaration>> fields);
    ~StructDeclaration() override = default;
    NodeType getType() const override;
    std::string toString() const override;
    void accept(Visitor& visitor) override;
};

// ClassDeclaration
class ClassDeclaration : public Declaration {
public:
    std::unique_ptr<Identifier> name;
    std::vector<std::unique_ptr<GenericParameter>> genericParams;
    // Members can be FieldDeclarations or FunctionDeclarations (methods)
    std::vector<DeclPtr> members; // Using DeclPtr to hold various member types

    ClassDeclaration(SourceLocation loc, std::unique_ptr<Identifier> name, std::vector<std::unique_ptr<GenericParameter>> genericParams, std::vector<DeclPtr> members);
    ~ClassDeclaration() override = default;
    NodeType getType() const override;
    std::string toString() const override;
    void accept(Visitor& visitor) override;
};

// ImplDeclaration
class ImplDeclaration : public Declaration {
public:
    std::unique_ptr<Identifier> name; // Optional name for the impl block (less common)
    std::vector<std::unique_ptr<GenericParameter>> genericParams;
    TypeNodePtr traitType; // Optional: if implementing a trait (e.g., impl MyTrait for MyType)
    TypeNodePtr selfType;  // The type for which methods are being implemented (e.g., MyType)
    std::vector<std::unique_ptr<FunctionDeclaration>> methods;

    ImplDeclaration(SourceLocation loc, TypeNodePtr selfType, std::vector<std::unique_ptr<FunctionDeclaration>> methods, std::unique_ptr<Identifier> name = nullptr, std::vector<std::unique_ptr<GenericParameter>> genericParams = {}, TypeNodePtr traitType = nullptr);
    ~ImplDeclaration() override = default;
    NodeType getType() const override;
    std::string toString() const override;
    void accept(Visitor& visitor) override;
};

// EnumVariant (typically part of EnumDeclaration)
class EnumVariant : public Node { // Not a Declaration itself, but a component
public:
    std::unique_ptr<Identifier> name;
    std::vector<TypeNodePtr> associatedTypes; // e.g., Option::Some(T) -> T is an associated type

    EnumVariant(SourceLocation loc, std::unique_ptr<Identifier> name, std::vector<TypeNodePtr> associatedTypes = {});
    ~EnumVariant() override = default;
    NodeType getType() const override;
    std::string toString() const override;
    void accept(Visitor& visitor) override;
};

// EnumDeclaration
class EnumDeclaration : public Declaration {
public:
    std::unique_ptr<Identifier> name;
    std::vector<std::unique_ptr<GenericParameter>> genericParams;
    std::vector<std::unique_ptr<EnumVariant>> variants;

    EnumDeclaration(SourceLocation loc, std::unique_ptr<Identifier> name, std::vector<std::unique_ptr<GenericParameter>> genericParams, std::vector<std::unique_ptr<EnumVariant>> variants);
    ~EnumDeclaration() override = default;
    NodeType getType() const override;
    std::string toString() const override;
    void accept(Visitor& visitor) override;
};

// Add StructType class
class StructType : public TypeNode {
public:
    struct Field {
        std::string name;
        std::shared_ptr<TypeNode> type;
    };
    std::vector<Field> fields;

    StructType(const SourceLocation& loc_) : TypeNode(loc_) {}
    
    void accept(Visitor& visitor) override {
        visitor.visit(this);
    }

    std::string toString() const override {
        std::string result = "struct { ";
        for (size_t i = 0; i < fields.size(); ++i) {
            if (i > 0) result += ", ";
            result += fields[i].name + ": " + fields[i].type->toString();
        }
        result += " }";
        return result;
    }

    Category getCategory() const override {
        return Category::STRUCT;
    }
};

} // namespace ast
} // namespace vyn

#endif // VYN_PARSER_AST_HPP
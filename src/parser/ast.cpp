// All includes at the top
#include <sstream>
#include <string>
#include <utility>
#include <optional>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include "vyn/parser/ast.hpp" // This now includes iostream, BorrowKind, OwnershipKind
#include "vyn/parser/token.hpp"

namespace vyn {
namespace ast {

// ObjectLiteral destructor
ObjectLiteral::~ObjectLiteral() = default;

// ObjectLiteral constructor implementation
ObjectLiteral::ObjectLiteral(SourceLocation loc, TypeNodePtr typePath, std::vector<ObjectProperty> properties)
    : Expression(loc), typePath(std::move(typePath)), properties(std::move(properties)) {}

NodeType ObjectLiteral::getType() const {
        return NodeType::OBJECT_LITERAL;
}

std::string ObjectLiteral::toString() const {
    std::stringstream ss;
    if (typePath) {
        ss << typePath->toString();
    }
    ss << "{\\";
    for (size_t i = 0; i < properties.size(); ++i) {
        ss << properties[i].key->toString() << ": " << properties[i].value->toString();
        if (i < properties.size() - 1) {
            ss << ", ";
        }
    }
    ss << "}";
    return ss.str();
}

void ObjectLiteral::accept(Visitor& visitor) {
    visitor.visit(this);
}

// --- IntegerLiteral ---
IntegerLiteral::IntegerLiteral(SourceLocation loc, int64_t val)
    : Expression(loc), value(val) {}

NodeType IntegerLiteral::getType() const {
    return NodeType::INTEGER_LITERAL;
}

std::string IntegerLiteral::toString() const {
    return std::to_string(value);
}

void IntegerLiteral::accept(Visitor& visitor) {
    visitor.visit(this);
}

IntegerLiteral::~IntegerLiteral() = default; // Added to ensure typeinfo is generated

// --- FloatLiteral ---
FloatLiteral::FloatLiteral(SourceLocation loc, double val)
    : Expression(loc), value(val) {}

NodeType FloatLiteral::getType() const {
    return NodeType::FLOAT_LITERAL;
}

std::string FloatLiteral::toString() const {
    return std::to_string(value);
}

void FloatLiteral::accept(Visitor& visitor) {
    visitor.visit(this);
}

// --- BooleanLiteral ---
BooleanLiteral::BooleanLiteral(SourceLocation loc, bool val)
    : Expression(loc), value(val) {}

NodeType BooleanLiteral::getType() const {
    return NodeType::BOOLEAN_LITERAL;
}

std::string BooleanLiteral::toString() const {
    return value ? "true" : "false";
}

void BooleanLiteral::accept(Visitor& visitor) {
    visitor.visit(this);
}

// --- NilLiteral ---
NilLiteral::NilLiteral(SourceLocation loc)
    : Expression(loc) {}

NodeType NilLiteral::getType() const {
    return NodeType::NIL_LITERAL;
}

std::string NilLiteral::toString() const {
    return "nil";
}

void NilLiteral::accept(Visitor& visitor) {
    visitor.visit(this);
}

// --- TryStatement Implementation ---
TryStatement::TryStatement(const SourceLocation& loc, std::unique_ptr<BlockStatement> tryBlock,
                 std::optional<std::string> catchIdent,
                 std::unique_ptr<BlockStatement> catchBlock,
                 std::unique_ptr<BlockStatement> finallyBlock)
    : Statement(loc),
      tryBlock(std::move(tryBlock)),
      catchIdent(std::move(catchIdent)),
      catchBlock(std::move(catchBlock)),
      finallyBlock(std::move(finallyBlock)) {}

NodeType TryStatement::getType() const { return NodeType::TRY_STATEMENT; }

std::string TryStatement::toString() const {
    std::stringstream ss;
    ss << "try " << (tryBlock ? tryBlock->toString() : "<null>");
    if (catchBlock) {
        ss << " catch";
        if (catchIdent) ss << "(" << *catchIdent << ")";
        ss << " " << catchBlock->toString();
    }
    if (finallyBlock) {
        ss << " finally " << finallyBlock->toString();
    }
    return ss.str();
}

void TryStatement::accept(Visitor& visitor) {
    visitor.visit(this);
}
// --- ImportDeclaration methods ---
ImportDeclaration::ImportDeclaration(
    SourceLocation loc,
    std::unique_ptr<StringLiteral> source_param, // Renamed to avoid conflict with member
    std::vector<ImportSpecifier> specifiers_param,
    std::unique_ptr<Identifier> defaultImport_param,
    std::unique_ptr<Identifier> namespaceImport_param)
    : Declaration(loc),
      source(std::move(source_param)),
      specifiers(std::move(specifiers_param)),
      defaultImport(std::move(defaultImport_param)),
      namespaceImport(std::move(namespaceImport_param)) {}

NodeType ImportDeclaration::getType() const { return NodeType::IMPORT_DECLARATION; }
std::string ImportDeclaration::toString() const { 
    std::stringstream ss;
    ss << "ImportDeclaration(source=";
    if (source) ss << source->toString(); else ss << "<null_source>";
    // Add specifiers, defaultImport, namespaceImport to string if needed
    ss << ")";
    return ss.str();
}
void ImportDeclaration::accept(Visitor& v) { v.visit(this); }

// --- STUBS for missing virtuals to resolve linker errors ---
// Expressions
NodeType CallExpression::getType() const { return NodeType::CALL_EXPRESSION; }
std::string CallExpression::toString() const { return "CallExpression"; }
void CallExpression::accept(Visitor& v) { v.visit(this); }

NodeType MemberExpression::getType() const { return NodeType::MEMBER_EXPRESSION; }
std::string MemberExpression::toString() const { return "MemberExpression"; }
void MemberExpression::accept(Visitor& v) { v.visit(this); }

NodeType UnaryExpression::getType() const { return NodeType::UNARY_EXPRESSION; }
std::string UnaryExpression::toString() const { return "UnaryExpression"; }
void UnaryExpression::accept(Visitor& v) { v.visit(this); }

NodeType BinaryExpression::getType() const { return NodeType::BINARY_EXPRESSION; }
std::string BinaryExpression::toString() const { return "BinaryExpression"; }
void BinaryExpression::accept(Visitor& v) { v.visit(this); }

NodeType AssignmentExpression::getType() const { return NodeType::ASSIGNMENT_EXPRESSION; }
std::string AssignmentExpression::toString() const { return "AssignmentExpression"; }
void AssignmentExpression::accept(Visitor& v) { v.visit(this); }

// Statements
NodeType BlockStatement::getType() const { return NodeType::BLOCK_STATEMENT; }
std::string BlockStatement::toString() const { return "BlockStatement"; }
void BlockStatement::accept(Visitor& v) { v.visit(this); }

NodeType ExpressionStatement::getType() const { return NodeType::EXPRESSION_STATEMENT; }
std::string ExpressionStatement::toString() const { return "ExpressionStatement"; }
void ExpressionStatement::accept(Visitor& v) { v.visit(this); }

NodeType IfStatement::getType() const { return NodeType::IF_STATEMENT; }
std::string IfStatement::toString() const { return "IfStatement"; }
void IfStatement::accept(Visitor& v) { v.visit(this); }

NodeType WhileStatement::getType() const { return NodeType::WHILE_STATEMENT; }
std::string WhileStatement::toString() const { return "WhileStatement"; }
void WhileStatement::accept(Visitor& v) { v.visit(this); }

NodeType ForStatement::getType() const { return NodeType::FOR_STATEMENT; }
std::string ForStatement::toString() const { return "ForStatement"; }
void ForStatement::accept(Visitor& v) { v.visit(this); }

NodeType ReturnStatement::getType() const { return NodeType::RETURN_STATEMENT; }
std::string ReturnStatement::toString() const { return "ReturnStatement"; }
void ReturnStatement::accept(Visitor& v) { v.visit(this); }

NodeType BreakStatement::getType() const { return NodeType::BREAK_STATEMENT; }
std::string BreakStatement::toString() const { return "BreakStatement"; }
void BreakStatement::accept(Visitor& v) { v.visit(this); }

NodeType ContinueStatement::getType() const { return NodeType::CONTINUE_STATEMENT; }
std::string ContinueStatement::toString() const { return "ContinueStatement"; }
void ContinueStatement::accept(Visitor& v) { v.visit(this); }

// Declarations
NodeType VariableDeclaration::getType() const { return NodeType::VARIABLE_DECLARATION; }
std::string VariableDeclaration::toString() const { return "VariableDeclaration"; }
void VariableDeclaration::accept(Visitor& v) { v.visit(this); }

NodeType FunctionDeclaration::getType() const { return NodeType::FUNCTION_DECLARATION; }
std::string FunctionDeclaration::toString() const { return "FunctionDeclaration"; }
void FunctionDeclaration::accept(Visitor& v) { v.visit(this); }

NodeType TypeAliasDeclaration::getType() const { return NodeType::TYPE_ALIAS_DECLARATION; }
std::string TypeAliasDeclaration::toString() const { return "TypeAliasDeclaration"; }
void TypeAliasDeclaration::accept(Visitor& v) { v.visit(this); }

// Out-of-line destructors to ensure vtables are generated
CallExpression::~CallExpression() = default;
MemberExpression::~MemberExpression() = default;
UnaryExpression::~UnaryExpression() = default;
BinaryExpression::~BinaryExpression() = default;
AssignmentExpression::~AssignmentExpression() = default;
BreakStatement::~BreakStatement() = default;
ContinueStatement::~ContinueStatement() = default;
ExpressionStatement::~ExpressionStatement() = default;
BlockStatement::~BlockStatement() = default;
IfStatement::~IfStatement() = default;
WhileStatement::~WhileStatement() = default;
ForStatement::~ForStatement() = default;
ReturnStatement::~ReturnStatement() = default;

// --- Expression Nodes ---
CallExpression::CallExpression(SourceLocation loc, ExprPtr callee, std::vector<ExprPtr> arguments)
    : Expression(loc), callee(std::move(callee)), arguments(std::move(arguments)) {}

MemberExpression::MemberExpression(SourceLocation loc, ExprPtr object, ExprPtr property, bool computed)
    : Expression(loc), object(std::move(object)), property(std::move(property)), computed(computed) {}

UnaryExpression::UnaryExpression(SourceLocation loc, const vyn::token::Token& op, ExprPtr operand)
    : Expression(loc), op(op), operand(std::move(operand)) {}

BinaryExpression::BinaryExpression(SourceLocation loc, ExprPtr left, const vyn::token::Token& op, ExprPtr right)
    : Expression(loc), left(std::move(left)), op(op), right(std::move(right)) {}

AssignmentExpression::AssignmentExpression(SourceLocation loc, ExprPtr left, const vyn::token::Token& op, ExprPtr right)
    : Expression(loc), left(std::move(left)), op(op), right(std::move(right)) {}

// --- Statement Nodes ---
BreakStatement::BreakStatement(SourceLocation loc)
    : Statement(loc) {}

ContinueStatement::ContinueStatement(SourceLocation loc)
    : Statement(loc) {}

ExpressionStatement::ExpressionStatement(SourceLocation loc, ExprPtr expression)
    : Statement(loc), expression(std::move(expression)) {}

BlockStatement::BlockStatement(SourceLocation loc, std::vector<StmtPtr> body_stmts) // Renamed body to body_stmts to avoid conflict
    : Statement(loc), body(std::move(body_stmts)) {}

IfStatement::IfStatement(SourceLocation loc, ExprPtr test, StmtPtr consequent, StmtPtr alternate)
    : Statement(loc), test(std::move(test)), consequent(std::move(consequent)), alternate(std::move(alternate)) {}

WhileStatement::WhileStatement(SourceLocation loc, ExprPtr test, StmtPtr body_stmt) // Renamed body to body_stmt
    : Statement(loc), test(std::move(test)), body(std::move(body_stmt)) {}

ForStatement::ForStatement(SourceLocation loc, NodePtr init_node, ExprPtr test_expr, ExprPtr update_expr, StmtPtr body_stmt) // Renamed parameters
    : Statement(loc), init(std::move(init_node)), test(std::move(test_expr)), update(std::move(update_expr)), body(std::move(body_stmt)) {}

ReturnStatement::ReturnStatement(SourceLocation loc, ExprPtr argument_expr) // Renamed argument
    : Statement(loc), argument(std::move(argument_expr)) {}

// --- Declaration Nodes ---
VariableDeclaration::VariableDeclaration(SourceLocation loc, std::unique_ptr<Identifier> id_ptr, bool isConst_val, TypeNodePtr typeNode_ptr, ExprPtr init_expr) // Renamed parameters
    : Declaration(loc), id(std::move(id_ptr)), isConst(isConst_val), typeNode(std::move(typeNode_ptr)), init(std::move(init_expr)) {}

FunctionDeclaration::FunctionDeclaration(SourceLocation loc, std::unique_ptr<Identifier> id_ptr, std::vector<FunctionParameter> params_vec, std::unique_ptr<BlockStatement> body_stmt, bool isAsync_val, TypeNodePtr returnTypeNode_ptr) // Renamed parameters
    : Declaration(loc), id(std::move(id_ptr)), params(std::move(params_vec)), body(std::move(body_stmt)), isAsync(isAsync_val), returnTypeNode(std::move(returnTypeNode_ptr)) {}

TypeAliasDeclaration::TypeAliasDeclaration(SourceLocation loc, std::unique_ptr<Identifier> name_ptr, TypeNodePtr typeNode_ptr) // Renamed parameters
    : Declaration(loc), name(std::move(name_ptr)), typeNode(std::move(typeNode_ptr)) {}

// Module (was Program)
Module::Module(SourceLocation loc, std::vector<StmtPtr> body_stmts) 
    : Node(loc), body(std::move(body_stmts)) {}

NodeType Module::getType() const { 
    return NodeType::MODULE; 
}

std::string Module::toString() const {
    std::stringstream ss;
    ss << "Module:\\\\n";
    for (const auto& stmt : body) {
        if (stmt) {
            ss << stmt->toString();
        }
    }
    return ss.str();
}

void Module::accept(Visitor& visitor) { 
    visitor.visit(this); 
}

// Identifier
Identifier::Identifier(SourceLocation loc, std::string n_val) // Renamed n to n_val
    : Expression(loc), name(std::move(n_val)) {} 

NodeType Identifier::getType() const { 
    return NodeType::IDENTIFIER; 
}

std::string Identifier::toString() const {
    std::stringstream ss;
    ss << "Identifier(" << name << ")";
    return ss.str();
}

void Identifier::accept(Visitor& visitor) { 
    visitor.visit(this); 
}

// --- StringLiteral ---
StringLiteral::StringLiteral(SourceLocation loc, std::string val) 
    : Expression(loc), value(std::move(val)) {} 

NodeType StringLiteral::getType() const { 
    return NodeType::STRING_LITERAL; 
}

std::string StringLiteral::toString() const {
    std::string s = this->value;
    std::string result = "\""; // Start with a double quote character
    for (char c : s) {
        switch (c) {
            case '\"': result += "\\\""; break; // Escape double quote
            case '\\': result += "\\\\"; break; // Escape backslash
            case '\n': result += "\\n"; break;   // Escape newline
            case '\r': result += "\\r"; break;   // Escape carriage return
            case '\t': result += "\\t"; break;   // Escape tab
            // Add other common escapes if necessary
            default:   result += c;
        }
    }
    result += "\""; // End with a double quote character
    return result;
}

void StringLiteral::accept(Visitor& visitor) { 
    visitor.visit(this); 
}

// --- ArrayLiteral ---
ArrayLiteral::ArrayLiteral(SourceLocation loc_param, std::vector<ExprPtr> elements_param) // Renamed from ArrayLiteralNode
    : Expression(loc_param), elements(std::move(elements_param)) {}

void ArrayLiteral::accept(Visitor& visitor) { // Renamed from ArrayLiteralNode
    visitor.visit(this);
}

std::string ArrayLiteral::toString() const { // Renamed from ArrayLiteralNode
    std::stringstream ss;
    ss << "ArrayLiteral(["; // Renamed from ArrayLiteralNode
    for (size_t i = 0; i < elements.size(); ++i) {
        if (elements[i]) ss << elements[i]->toString(); else ss << "<null_element>";
        if (i < elements.size() - 1) ss << ", ";
    }
    ss << "])";
    return ss.str();
}

// --- BorrowExpression ---
BorrowExpression::BorrowExpression(SourceLocation loc_param, ExprPtr expression_param, BorrowKind kind_param) // Renamed from BorrowExprNode
    : Expression(loc_param), expression(std::move(expression_param)), kind(kind_param) {}

void BorrowExpression::accept(Visitor& visitor) { // Renamed from BorrowExprNode
    visitor.visit(this);
}

std::string BorrowExpression::toString() const { // Renamed from BorrowExprNode
    std::string prefix_str; // Renamed prefix to prefix_str
    switch (kind) {
        case BorrowKind::MUTABLE_BORROW: // Uses globally defined BorrowKind
            prefix_str = "borrow ";
            break;
        case BorrowKind::IMMUTABLE_VIEW: // Uses globally defined BorrowKind
            prefix_str = "view ";
            break;
        // default case removed as BorrowKind should always be one of the defined values
    }
    if (expression) {
        return prefix_str + expression->toString();
    }
    return prefix_str + "nullptr";
}

// --- TypeNode and its derived classes ---
// TypeNode base class does not have direct instantiations for its previous factory methods.
// Those factory methods were creating TypeNode with a category and then populating specific fields.
// Now, derived classes (TypeName, PointerType, etc.) should be instantiated directly.
// The old TypeNode::toString() and TypeNode::accept() are now handled by derived classes.

// TypeName
TypeName::TypeName(SourceLocation loc, std::unique_ptr<Identifier> id, std::vector<TypeNodePtr> args)
    : TypeNode(loc), identifier(std::move(id)), genericArgs(std::move(args)) {}

NodeType TypeName::getType() const { return NodeType::TYPE_NAME; }

std::string TypeName::toString() const {
    std::string str_val;
    if (identifier) { 
        str_val += identifier->name;
    } else {
        str_val += "<unnamed_identifier_type>";
    }
    if (!genericArgs.empty()) {
        str_val += "<";
        for (size_t i = 0; i < genericArgs.size(); ++i) {
            if (genericArgs[i]) str_val += genericArgs[i]->toString(); else str_val += "<null_gen_arg>";
            if (i < genericArgs.size() - 1) {
                str_val += ", ";
            }
        }
        str_val += ">";
    }
    return str_val;
}

void TypeName::accept(Visitor& visitor) { visitor.visit(this); }

// PointerType
PointerType::PointerType(SourceLocation loc, TypeNodePtr pointee)
    : TypeNode(loc), pointeeType(std::move(pointee)) {}

NodeType PointerType::getType() const { return NodeType::POINTER_TYPE; }

std::string PointerType::toString() const {
    return "*" + (pointeeType ? pointeeType->toString() : "<null_pointee>");
}

void PointerType::accept(Visitor& visitor) { visitor.visit(this); }

// ArrayType
ArrayType::ArrayType(SourceLocation loc, TypeNodePtr elemType, ExprPtr sizeExpr)
    : TypeNode(loc), elementType(std::move(elemType)), sizeExpression(std::move(sizeExpr)) {}

NodeType ArrayType::getType() const { return NodeType::ARRAY_TYPE; }

std::string ArrayType::toString() const {
    std::string str_val = "[";
    if (elementType) str_val += elementType->toString(); else str_val += "<invalid_array_element_type>";
    if (sizeExpression) {
        str_val += "; ";
        str_val += sizeExpression->toString(); 
    }
    str_val += "]";
    return str_val;
}

void ArrayType::accept(Visitor& visitor) { visitor.visit(this); }

// FunctionType
FunctionType::FunctionType(SourceLocation loc, std::vector<TypeNodePtr> params, TypeNodePtr retType)
    : TypeNode(loc), parameterTypes(std::move(params)), returnType(std::move(retType)) {}

NodeType FunctionType::getType() const { return NodeType::FUNCTION_TYPE; }

std::string FunctionType::toString() const {
    std::string str_val = "fn(";
    for (size_t i = 0; i < parameterTypes.size(); ++i) {
        if (parameterTypes[i]) str_val += parameterTypes[i]->toString();
        else str_val += "<invalid_param_type>";
        if (i < parameterTypes.size() - 1) {
            str_val += ", ";
        }
    }
    str_val += ")";
    if (returnType) {
        str_val += " -> " + returnType->toString();
    } else {
        str_val += " -> <void_or_inferred_return_type>"; // Or just omit if void is implied
    }
    return str_val;
}

void FunctionType::accept(Visitor& visitor) { visitor.visit(this); }

// OptionalType
OptionalType::OptionalType(SourceLocation loc, TypeNodePtr contained)
    : TypeNode(loc), containedType(std::move(contained)) {}

NodeType OptionalType::getType() const { return NodeType::OPTIONAL_TYPE; }

std::string OptionalType::toString() const {
    return (containedType ? containedType->toString() : "<null_contained>") + "?";
}

void OptionalType::accept(Visitor& visitor) { visitor.visit(this); }

// --- Implementation for TupleTypeNode --- // ADDED
TupleTypeNode::TupleTypeNode(SourceLocation loc, std::vector<TypeNodePtr> members)
    : TypeNode(loc), memberTypes(std::move(members)) {}

NodeType TupleTypeNode::getType() const { return NodeType::TUPLE_TYPE; } // Corrected TUPLE_TYPE_NODE to TUPLE_TYPE

std::string TupleTypeNode::toString() const {
    std::string str = "(";
    for (size_t i = 0; i < memberTypes.size(); ++i) {
        str += memberTypes[i] ? memberTypes[i]->toString() : "null_type";
        if (i < memberTypes.size() - 1) str += ", ";
    }
    str += ")";
    return str;
}

void TupleTypeNode::accept(Visitor& visitor) { visitor.visit(this); }
// --- End of TupleTypeNode Implementation --- // ADDED

// --- GenericParameter ---
vyn::ast::GenericParameter::GenericParameter(vyn::SourceLocation loc, std::unique_ptr<vyn::ast::Identifier> name, std::vector<vyn::ast::TypeNodePtr> bounds)
    : Node(loc), name(std::move(name)), bounds(std::move(bounds)) {}

vyn::ast::NodeType vyn::ast::GenericParameter::getType() const { return vyn::ast::NodeType::GENERIC_PARAMETER; }

std::string vyn::ast::GenericParameter::toString() const {
    std::string s = name ? name->toString() : "<null_name>";
    if (!bounds.empty()) {
        s += ": ";
        for (size_t i = 0; i < bounds.size(); ++i) {
            if (bounds[i]) {
                s += bounds[i]->toString();
            } else {
                s += "<null_bound>";
            }
            if (i < bounds.size() - 1) {
                s += " + "; // Assuming '+' for multiple bounds, adjust if syntax is different
            }
        }
    }
    return s;
}

void vyn::ast::GenericParameter::accept(vyn::ast::Visitor& visitor) { visitor.visit(this); }

// --- FieldDeclaration ---
vyn::ast::FieldDeclaration::FieldDeclaration(vyn::SourceLocation loc, std::unique_ptr<vyn::ast::Identifier> name, vyn::ast::TypeNodePtr typeNode, vyn::ast::ExprPtr initializer, bool isMutable)
    : Declaration(loc), name(std::move(name)), typeNode(std::move(typeNode)), initializer(std::move(initializer)), isMutable(isMutable) {}

vyn::ast::NodeType vyn::ast::FieldDeclaration::getType() const { return vyn::ast::NodeType::FIELD_DECLARATION; }

std::string vyn::ast::FieldDeclaration::toString() const {
    std::string s = name ? name->toString() : "<null_field_name>";
    s += ": ";
    s += typeNode ? typeNode->toString() : "<null_type>";
    if (initializer) {
        s += " = " + initializer->toString();
    }
    if (isMutable) {
        s += " (mutable)";
    }
    return s;
}

void vyn::ast::FieldDeclaration::accept(vyn::ast::Visitor& visitor) { visitor.visit(this); }

// --- StructDeclaration ---
vyn::ast::StructDeclaration::StructDeclaration(vyn::SourceLocation loc, std::unique_ptr<vyn::ast::Identifier> name, std::vector<std::unique_ptr<vyn::ast::GenericParameter>> genericParams, std::vector<std::unique_ptr<vyn::ast::FieldDeclaration>> fields)
    : Declaration(loc), name(std::move(name)), genericParams(std::move(genericParams)), fields(std::move(fields)) {}

vyn::ast::NodeType vyn::ast::StructDeclaration::getType() const { return vyn::ast::NodeType::STRUCT_DECLARATION; }

std::string vyn::ast::StructDeclaration::toString() const {
    std::string s = "struct ";
    s += name ? name->toString() : "<null_struct_name>";
    // Add generic params and fields to string representation if needed
    return s;
}

void vyn::ast::StructDeclaration::accept(vyn::ast::Visitor& visitor) { visitor.visit(this); }

// --- ImplDeclaration ---
vyn::ast::ImplDeclaration::ImplDeclaration(
    vyn::SourceLocation loc,
    vyn::ast::TypeNodePtr selfType,
    std::vector<std::unique_ptr<vyn::ast::FunctionDeclaration>> methods,
    std::unique_ptr<vyn::ast::Identifier> name,
    std::vector<std::unique_ptr<vyn::ast::GenericParameter>> genericParams,
    vyn::ast::TypeNodePtr traitType)
    : Declaration(loc),
      name(std::move(name)),
      genericParams(std::move(genericParams)),
      traitType(std::move(traitType)),
      selfType(std::move(selfType)),
      methods(std::move(methods)) {}

vyn::ast::NodeType vyn::ast::ImplDeclaration::getType() const { return vyn::ast::NodeType::IMPL_DECLARATION; }

std::string vyn::ast::ImplDeclaration::toString() const {
    std::string s = "impl";
    if (traitType) {
        s += " " + traitType->toString();
    }
    s += " for " + (selfType ? selfType->toString() : "<null_self_type>");
    // Add methods, generics etc. to string if needed
    return s;
}

void vyn::ast::ImplDeclaration::accept(vyn::ast::Visitor& visitor) { visitor.visit(this); }

// --- EnumVariant ---
vyn::ast::EnumVariant::EnumVariant(vyn::SourceLocation loc, std::unique_ptr<vyn::ast::Identifier> name, std::vector<vyn::ast::TypeNodePtr> associatedTypes)
    : Node(loc), name(std::move(name)), associatedTypes(std::move(associatedTypes)) {}

vyn::ast::NodeType vyn::ast::EnumVariant::getType() const { return vyn::ast::NodeType::ENUM_VARIANT; }

std::string vyn::ast::EnumVariant::toString() const {
    std::string s = name ? name->toString() : "<null_variant_name>";
    if (!associatedTypes.empty()) {
        s += "(";
        for (size_t i = 0; i < associatedTypes.size(); ++i) {
            s += associatedTypes[i] ? associatedTypes[i]->toString() : "<null_type>";
            if (i < associatedTypes.size() - 1) s += ", ";
        }
        s += ")";
    }
    return s;
}

void vyn::ast::EnumVariant::accept(vyn::ast::Visitor& visitor) { visitor.visit(this); }

// --- EnumDeclaration ---
vyn::ast::EnumDeclaration::EnumDeclaration(vyn::SourceLocation loc, std::unique_ptr<vyn::ast::Identifier> name, std::vector<std::unique_ptr<vyn::ast::GenericParameter>> genericParams, std::vector<std::unique_ptr<vyn::ast::EnumVariant>> variants)
    : Declaration(loc), name(std::move(name)), genericParams(std::move(genericParams)), variants(std::move(variants)) {}

vyn::ast::NodeType vyn::ast::EnumDeclaration::getType() const { return vyn::ast::NodeType::ENUM_DECLARATION; }

std::string vyn::ast::EnumDeclaration::toString() const {
    std::string s = "enum ";
    s += name ? name->toString() : "<null_enum_name>";
    // Add generic params and variants to string representation if needed
    return s;
}

void vyn::ast::EnumDeclaration::accept(vyn::ast::Visitor& visitor) { visitor.visit(this); }

// --- TemplateDeclaration ---
// Note: ast.hpp has name, genericParams, body for TemplateDeclaration
vyn::ast::TemplateDeclaration::TemplateDeclaration(vyn::SourceLocation loc, std::unique_ptr<vyn::ast::Identifier> name, std::vector<std::unique_ptr<vyn::ast::GenericParameter>> genericParams, vyn::ast::DeclPtr body)
    : Declaration(loc), name(std::move(name)), genericParams(std::move(genericParams)), body(std::move(body)) {}

vyn::ast::NodeType vyn::ast::TemplateDeclaration::getType() const { return vyn::ast::NodeType::TEMPLATE_DECLARATION; }

std::string vyn::ast::TemplateDeclaration::toString() const {
    std::string s = "template<";
    for (size_t i = 0; i < genericParams.size(); ++i) {
        s += genericParams[i] ? genericParams[i]->toString() : "<null_param>";
        if (i < genericParams.size() - 1) s += ", ";
    }
    s += "> ";
    s += name ? name->toString() : "<null_template_name>";
    s += " " + (body ? body->toString() : "<null_body>");
    return s;
}

void vyn::ast::TemplateDeclaration::accept(vyn::ast::Visitor& visitor) { visitor.visit(this); }

// --- ClassDeclaration ---
vyn::ast::ClassDeclaration::ClassDeclaration(vyn::SourceLocation loc, std::unique_ptr<vyn::ast::Identifier> name, std::vector<std::unique_ptr<vyn::ast::GenericParameter>> genericParams, std::vector<vyn::ast::DeclPtr> members)
    : Declaration(loc), name(std::move(name)), genericParams(std::move(genericParams)), members(std::move(members)) {}

vyn::ast::NodeType vyn::ast::ClassDeclaration::getType() const { return vyn::ast::NodeType::CLASS_DECLARATION; }

std::string vyn::ast::ClassDeclaration::toString() const {
    std::string s = "class ";
    s += name ? name->toString() : "<null_class_name>";
    // Add generic params and members to string representation if needed
    return s;
}

void vyn::ast::ClassDeclaration::accept(vyn::ast::Visitor& visitor) { visitor.visit(this); }

// --- IfExpression ---
// ast.hpp: IfExpression(SourceLocation loc, ExprPtr condition, ExprPtr thenBranch, ExprPtr elseBranch);
vyn::ast::IfExpression::IfExpression(vyn::SourceLocation loc, vyn::ast::ExprPtr condition, vyn::ast::ExprPtr thenBranch, vyn::ast::ExprPtr elseBranch)
    : Expression(loc), condition(std::move(condition)), thenBranch(std::move(thenBranch)), elseBranch(std::move(elseBranch)) {}

vyn::ast::NodeType vyn::ast::IfExpression::getType() const { return vyn::ast::NodeType::IF_EXPRESSION; }

std::string vyn::ast::IfExpression::toString() const {
    std::string s = "if (" + (condition ? condition->toString() : "<null_cond>") + ") ";
    s += (thenBranch ? thenBranch->toString() : "<null_then>");
    s += " else " + (elseBranch ? elseBranch->toString() : "<null_else>");
    return s;
}

void vyn::ast::IfExpression::accept(vyn::ast::Visitor& visitor) { visitor.visit(this); }

// --- ConstructionExpression ---
// ast.hpp: ConstructionExpression(SourceLocation loc, TypeNodePtr constructedType, std::vector<ExprPtr> arguments);
vyn::ast::ConstructionExpression::ConstructionExpression(vyn::SourceLocation loc, vyn::ast::TypeNodePtr constructedType, std::vector<vyn::ast::ExprPtr> arguments)
    : Expression(loc), constructedType(std::move(constructedType)), arguments(std::move(arguments)) {}

vyn::ast::NodeType vyn::ast::ConstructionExpression::getType() const { return vyn::ast::NodeType::CONSTRUCTION_EXPRESSION; }

std::string vyn::ast::ConstructionExpression::toString() const {
    std::string s = constructedType ? constructedType->toString() : "<null_type>";
    s += "{";
    for (size_t i = 0; i < arguments.size(); ++i) {
        s += arguments[i] ? arguments[i]->toString() : "<null_arg>";
        if (i < arguments.size() - 1) s += ", ";
    }
    s += "}";
    return s;
}

void vyn::ast::ConstructionExpression::accept(vyn::ast::Visitor& visitor) { visitor.visit(this); }

// --- ArrayInitializationExpression ---
// ast.hpp: ArrayInitializationExpression(SourceLocation loc, TypeNodePtr elementType, ExprPtr sizeExpression);
vyn::ast::ArrayInitializationExpression::ArrayInitializationExpression(vyn::SourceLocation loc, vyn::ast::TypeNodePtr elementType, vyn::ast::ExprPtr sizeExpression)
    : Expression(loc), elementType(std::move(elementType)), sizeExpression(std::move(sizeExpression)) {}

vyn::ast::NodeType vyn::ast::ArrayInitializationExpression::getType() const { return vyn::ast::NodeType::ARRAY_INITIALIZATION_EXPRESSION; }

std::string vyn::ast::ArrayInitializationExpression::toString() const {
    return "[" + (elementType ? elementType->toString() : "<null_type>") + "; " + (sizeExpression ? sizeExpression->toString() : "<null_size>") + "]";
}

void vyn::ast::ArrayInitializationExpression::accept(vyn::ast::Visitor& visitor) { visitor.visit(this); }

// --- PointerDerefExpression ---
// ast.hpp: PointerDerefExpression(SourceLocation loc, ExprPtr pointer);
// Member in hpp is 'pointer', not 'expr'
vyn::ast::PointerDerefExpression::PointerDerefExpression(vyn::SourceLocation loc, vyn::ast::ExprPtr pointer)
    : Expression(loc), pointer(std::move(pointer)) {}

vyn::ast::NodeType vyn::ast::PointerDerefExpression::getType() const { return vyn::ast::NodeType::POINTER_DEREF_EXPRESSION; }

std::string vyn::ast::PointerDerefExpression::toString() const {
    return "*" + (pointer ? pointer->toString() : "<null_expr>");
}

void vyn::ast::PointerDerefExpression::accept(vyn::ast::Visitor& visitor) { visitor.visit(this); }

// --- AddrOfExpression ---
// ast.hpp: AddrOfExpression(SourceLocation loc, ExprPtr location);
// Member in hpp is 'location', not 'expr'. No 'isMutable' in hpp constructor.
// The header has:
// class AddrOfExpression : public Expression {
//     ExprPtr location;
// public:
//     AddrOfExpression(SourceLocation loc, ExprPtr location);
// This implies isMutable might be part of the Type system or semantic analysis, not direct AST.
// For now, matching the header strictly. If isMutable is needed, header must change.
vyn::ast::AddrOfExpression::AddrOfExpression(vyn::SourceLocation loc, vyn::ast::ExprPtr location)
    : Expression(loc), location(std::move(location)) {}

vyn::ast::NodeType vyn::ast::AddrOfExpression::getType() const { return vyn::ast::NodeType::ADDR_OF_EXPRESSION; }

std::string vyn::ast::AddrOfExpression::toString() const {
    // Assuming '&' for immutable borrow by default if not specified otherwise in AST structure
    return "&" + (location ? location->toString() : "<null_expr>");
}

void vyn::ast::AddrOfExpression::accept(vyn::ast::Visitor& visitor) { visitor.visit(this); }

// --- FromIntToLocExpression ---
// ast.hpp: FromIntToLocExpression(SourceLocation loc, ExprPtr address);
// Member in hpp is 'address', not 'intExpr'. No 'targetType' in hpp constructor.
vyn::ast::FromIntToLocExpression::FromIntToLocExpression(vyn::SourceLocation loc, vyn::ast::ExprPtr address)
    : Expression(loc), address(std::move(address)) {}

vyn::ast::NodeType vyn::ast::FromIntToLocExpression::getType() const { return vyn::ast::NodeType::FROM_INT_TO_LOC_EXPRESSION; }

std::string vyn::ast::FromIntToLocExpression::toString() const {
    // Target type is not part of this AST node per header, it's a semantic concept.
    return "from_int_to_loc(" + (address ? address->toString() : "<null_address>") + ")";
}

void vyn::ast::FromIntToLocExpression::accept(vyn::ast::Visitor& visitor) { visitor.visit(this); }

// --- GenericInstantiationExpression ---
// ast.hpp: GenericInstantiationExpression(SourceLocation loc, ExprPtr base, std::vector<TypeNodePtr> args, SourceLocation lt_loc, SourceLocation gt_loc);
// Members in hpp: baseExpression, genericArguments, lt_loc, gt_loc
vyn::ast::GenericInstantiationExpression::GenericInstantiationExpression(vyn::SourceLocation loc, vyn::ast::ExprPtr base, std::vector<vyn::ast::TypeNodePtr> args, vyn::SourceLocation lt_loc, vyn::SourceLocation gt_loc)
    : Expression(loc), baseExpression(std::move(base)), genericArguments(std::move(args)), lt_loc(lt_loc), gt_loc(gt_loc) {}

vyn::ast::NodeType vyn::ast::GenericInstantiationExpression::getType() const { return vyn::ast::NodeType::GENERIC_INSTANTIATION_EXPRESSION; }

std::string vyn::ast::GenericInstantiationExpression::toString() const {
    std::string s = baseExpression ? baseExpression->toString() : "<null_base>";
    s += "<";
    for (size_t i = 0; i < genericArguments.size(); ++i) {
        s += genericArguments[i] ? genericArguments[i]->toString() : "<null_arg>";
        if (i < genericArguments.size() - 1) s += ", ";
    }
    s += ">";
    return s;
}

void vyn::ast::GenericInstantiationExpression::accept(vyn::ast::Visitor& visitor) { visitor.visit(this); }

// --- UnsafeStatement ---
// ast.hpp: UnsafeStatement(SourceLocation loc, std::unique_ptr<BlockStatement> blockStmt)
// Member in hpp: block
// toString() is already declared in hpp.
std::string vyn::ast::UnsafeStatement::toString() const {
    return "unsafe " + (block ? block->toString() : "{}");
}
// Accept and getType are already defined in hpp for UnsafeStatement via macro/inline or direct definition.
// If they were meant to be out-of-line:
// vyn::ast::NodeType vyn::ast::UnsafeStatement::getType() const { return vyn::ast::NodeType::UNSAFE_STATEMENT; }
// void vyn::ast::UnsafeStatement::accept(vyn::ast::Visitor& visitor) { visitor.visit(this); }
// The header for UnsafeStatement has:
// NodeType getType() const override { return NodeType::UNSAFE_STATEMENT; }
// void accept(Visitor& visitor) override { visitor.visit(this); }
// So these are fine. Only toString() was needed out-of-line if not already provided.

// Ensure other existing definitions are compatible or remove/update them.
// The previous edit added many stubs and full definitions. This new edit replaces many of them.
// I'm assuming the ...existing code... markers will handle merging correctly.
// It's important that any destructors that were previously defined as defaulted in the .cpp
// (e.g., PointerDerefExpression::~PointerDerefExpression() = default;) and caused errors
// are now removed from the .cpp file if they are not declared in the .hpp file or are
// intended to be implicitly generated or defaulted in the header.
// The classes PointerDerefExpression, AddrOfExpression, FromIntToLocExpression,
// GenericInstantiationExpression, ListComprehension, LocationExpression do not have
// explicit destructors declared in ast.hpp, so their definitions should not be in ast.cpp.

} // namespace ast
} // namespace vyn

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
    ss << "{"; // Removed backslash before brace
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

// --- ArrayElementExpression ---
ArrayElementExpression::ArrayElementExpression(SourceLocation loc, ExprPtr arr, ExprPtr idx)
    : Expression(loc), array(std::move(arr)), index(std::move(idx)) {}

NodeType ArrayElementExpression::getType() const {
    return NodeType::ARRAY_ELEMENT_EXPRESSION;
}

std::string ArrayElementExpression::toString() const {
    return (array ? array->toString() : "nullptr") + "[" + (index ? index->toString() : "nullptr") + "]";
}

void ArrayElementExpression::accept(Visitor& visitor) {
    visitor.visit(this);
}
ArrayElementExpression::~ArrayElementExpression() = default;

// --- LocationExpression ---
LocationExpression::LocationExpression(SourceLocation loc, ExprPtr expression)
    : Expression(loc), expression(std::move(expression)) {}

NodeType LocationExpression::getType() const {
    return NodeType::LOCATION_EXPRESSION;
}

std::string LocationExpression::toString() const {
    return "loc(" + (expression ? expression->toString() : "nullptr") + ")";
}

void LocationExpression::accept(Visitor& visitor) {
    visitor.visit(this);
}
LocationExpression::~LocationExpression() = default;

// --- ListComprehension ---
ListComprehension::ListComprehension(SourceLocation loc, ExprPtr elementExpr, IdentifierPtr loopVariable, ExprPtr iterableExpr, ExprPtr conditionExpr)
    : Expression(loc),
      elementExpr(std::move(elementExpr)),
      loopVariable(std::move(loopVariable)),
      iterableExpr(std::move(iterableExpr)),
      conditionExpr(std::move(conditionExpr)) {}

NodeType ListComprehension::getType() const {
    return NodeType::LIST_COMPREHENSION;
}

std::string ListComprehension::toString() const {
    std::string str = "[" + (elementExpr ? elementExpr->toString() : "nullptr");
    str += " for " + (loopVariable ? loopVariable->toString() : "nullptr");
    str += " in " + (iterableExpr ? iterableExpr->toString() : "nullptr");
    if (conditionExpr) {
        str += " if " + conditionExpr->toString();
    }
    str += "]";
    return str;
}

void ListComprehension::accept(Visitor& visitor) {
    visitor.visit(this);
}
ListComprehension::~ListComprehension() = default;

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

IntegerLiteral::~IntegerLiteral() = default;

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

// --- Identifier ---
Identifier::Identifier(SourceLocation loc, std::string name_val)
    : Expression(loc), name(std::move(name_val)) {}

NodeType Identifier::getType() const {
    return NodeType::IDENTIFIER;
}

std::string Identifier::toString() const {
    return name;
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
    // Basic string representation, might need escaping for special characters
    return "\"" + value + "\"";
}

void StringLiteral::accept(Visitor& visitor) {
    visitor.visit(this);
}

// --- ArrayLiteral ---
ArrayLiteral::ArrayLiteral(SourceLocation loc, std::vector<ExprPtr> elems)
    : Expression(loc), elements(std::move(elems)) {}

std::string ArrayLiteral::toString() const {
    std::stringstream ss;
    ss << "[";
    for (size_t i = 0; i < elements.size(); ++i) {
        if (elements[i]) {
            ss << elements[i]->toString();
        }
        if (i < elements.size() - 1) {
            ss << ", ";
        }
    }
    ss << "]";
    return ss.str();
}

void ArrayLiteral::accept(Visitor& visitor) {
    visitor.visit(this);
}

// --- BorrowExpression ---
BorrowExpression::BorrowExpression(SourceLocation loc, ExprPtr expr, BorrowKind k)
    : Expression(loc), expression(std::move(expr)), kind(k) {}

std::string BorrowExpression::toString() const {
    std::string kindStr;
    switch (kind) {
        case BorrowKind::MUTABLE_BORROW:
            kindStr = "borrow_mut";
            break;
        case BorrowKind::IMMUTABLE_VIEW:
            kindStr = "view";
            break;
        // Add other cases if BorrowKind is expanded
        default:
            kindStr = "unknown_borrow_kind";
            break;
    }
    return kindStr + "(" + (expression ? expression->toString() : "nullptr") + ")";
}

void BorrowExpression::accept(Visitor& visitor) {
    visitor.visit(this);
}

// --- PointerDerefExpression ---
PointerDerefExpression::PointerDerefExpression(SourceLocation loc, ExprPtr ptr)
    : Expression(loc), pointer(std::move(ptr)) {}

NodeType PointerDerefExpression::getType() const {
    return NodeType::POINTER_DEREF_EXPRESSION;
}

std::string PointerDerefExpression::toString() const {
    return "at(" + (pointer ? pointer->toString() : "nullptr") + ")";
}

void PointerDerefExpression::accept(Visitor& visitor) {
    visitor.visit(this);
}

// --- AddrOfExpression ---
AddrOfExpression::AddrOfExpression(SourceLocation loc, ExprPtr loc_expr)
    : Expression(loc), location(std::move(loc_expr)) {}

NodeType AddrOfExpression::getType() const {
    return NodeType::ADDR_OF_EXPRESSION;
}

std::string AddrOfExpression::toString() const {
    return "addr(" + (location ? location->toString() : "nullptr") + ")";
}

void AddrOfExpression::accept(Visitor& visitor) {
    visitor.visit(this);
}

// --- FromIntToLocExpression ---
// Constructor is defined inline in ast.hpp
// REMOVE any explicit constructor definition that was here.
// For example, the erroneous:
// FromIntToLocExpression::FromIntToLocExpression(ExprPtr addr_expr, TypeNodePtr target_ty, SourceLocation loc)
//     : Expression(loc), address_expr(std::move(addr_expr)), target_type(std::move(target_ty)) {}
NodeType FromIntToLocExpression::getType() const {
    return NodeType::FROM_INT_TO_LOC_EXPRESSION;
}

std::string FromIntToLocExpression::toString() const {
    return "from<" + (target_type ? target_type->toString() : "UnknownType") + ">(" + 
           (address_expr ? address_expr->toString() : "nullptr") + ")";
}

void FromIntToLocExpression::accept(Visitor& visitor) {
    visitor.visit(this);
}

// --- UnaryExpression ---
UnaryExpression::UnaryExpression(SourceLocation loc, const token::Token& op_val, ExprPtr operand_val)
    : Expression(loc), op(op_val), operand(std::move(operand_val)) {}

UnaryExpression::~UnaryExpression() = default;

NodeType UnaryExpression::getType() const {
    return NodeType::UNARY_EXPRESSION;
}

std::string UnaryExpression::toString() const {
    return op.lexeme + (operand ? operand->toString() : "nullptr");
}

void UnaryExpression::accept(Visitor& visitor) {
    visitor.visit(this);
}

// --- BinaryExpression ---
BinaryExpression::BinaryExpression(SourceLocation loc, ExprPtr l, const token::Token& op_val, ExprPtr r)
    : Expression(loc), left(std::move(l)), op(op_val), right(std::move(r)) {}

BinaryExpression::~BinaryExpression() = default;

NodeType BinaryExpression::getType() const {
    return NodeType::BINARY_EXPRESSION;
}

std::string BinaryExpression::toString() const {
    return "(" + (left ? left->toString() : "nullptr") + " " + op.lexeme + " " + 
           (right ? right->toString() : "nullptr") + ")";
}

void BinaryExpression::accept(Visitor& visitor) {
    visitor.visit(this);
}

// --- CallExpression ---
CallExpression::CallExpression(SourceLocation loc, ExprPtr callee_val, std::vector<ExprPtr> arguments_val)
    : Expression(loc), callee(std::move(callee_val)), arguments(std::move(arguments_val)) {}

CallExpression::~CallExpression() = default;

NodeType CallExpression::getType() const {
    return NodeType::CALL_EXPRESSION;
}

std::string CallExpression::toString() const {
    std::stringstream ss;
    ss << (callee ? callee->toString() : "nullptr") << "(";
    for (size_t i = 0; i < arguments.size(); ++i) {
        if (arguments[i]) {
            ss << arguments[i]->toString();
        }
        if (i < arguments.size() - 1) {
            ss << ", ";
        }
    }
    ss << ")";
    return ss.str();
}

void CallExpression::accept(Visitor& visitor) {
    visitor.visit(this);
}

// --- ConstructionExpression ---
ConstructionExpression::ConstructionExpression(SourceLocation loc, TypeNodePtr constructed_type, std::vector<ExprPtr> args)
    : Expression(loc), constructedType(std::move(constructed_type)), arguments(std::move(args)) {}

NodeType ConstructionExpression::getType() const {
    return NodeType::CONSTRUCTION_EXPRESSION;
}

std::string ConstructionExpression::toString() const {
    std::stringstream ss;
    ss << (constructedType ? constructedType->toString() : "UnknownType") << "(";
    for (size_t i = 0; i < arguments.size(); ++i) {
        if (arguments[i]) {
            ss << arguments[i]->toString();
        }
        if (i < arguments.size() - 1) {
            ss << ", ";
        }
    }
    ss << ")";
    return ss.str();
}

void ConstructionExpression::accept(Visitor& visitor) {
    visitor.visit(this);
}

// --- ArrayInitializationExpression ---
ArrayInitializationExpression::ArrayInitializationExpression(SourceLocation loc, TypeNodePtr elem_type, ExprPtr size_expr)
    : Expression(loc), elementType(std::move(elem_type)), sizeExpression(std::move(size_expr)) {}

NodeType ArrayInitializationExpression::getType() const {
    return NodeType::ARRAY_INITIALIZATION_EXPRESSION;
}

std::string ArrayInitializationExpression::toString() const {
    return "[" + (elementType ? elementType->toString() : "UnknownType") + "; " + 
           (sizeExpression ? sizeExpression->toString() : "UnknownSize") + "]()";
}

void ArrayInitializationExpression::accept(Visitor& visitor) {
    visitor.visit(this);
}

// --- GenericInstantiationExpression ---
GenericInstantiationExpression::GenericInstantiationExpression(SourceLocation loc, ExprPtr base, std::vector<TypeNodePtr> args, SourceLocation lt, SourceLocation gt)
    : Expression(loc), baseExpression(std::move(base)), genericArguments(std::move(args)), lt_loc(lt), gt_loc(gt) {}

NodeType GenericInstantiationExpression::getType() const {
    return NodeType::GENERIC_INSTANTIATION_EXPRESSION;
}

std::string GenericInstantiationExpression::toString() const {
    std::stringstream ss;
    ss << (baseExpression ? baseExpression->toString() : "nullptr") << "<";
    for (size_t i = 0; i < genericArguments.size(); ++i) {
        if (genericArguments[i]) {
            ss << genericArguments[i]->toString();
        }
        if (i < genericArguments.size() - 1) {
            ss << ", ";
        }
    }
    ss << ">";
    return ss.str();
}

void GenericInstantiationExpression::accept(Visitor& visitor) {
    visitor.visit(this);
}

// --- MemberExpression ---
MemberExpression::MemberExpression(SourceLocation loc, ExprPtr obj, ExprPtr prop, bool comp)
    : Expression(loc), object(std::move(obj)), property(std::move(prop)), computed(comp) {}

MemberExpression::~MemberExpression() = default;

NodeType MemberExpression::getType() const {
    return NodeType::MEMBER_EXPRESSION;
}

std::string MemberExpression::toString() const {
    if (computed) {
        return (object ? object->toString() : "nullptr") + "[" + 
               (property ? property->toString() : "nullptr") + "]";
    } else {
        return (object ? object->toString() : "nullptr") + "." + 
               (property ? property->toString() : "nullptr");
    }
}

void MemberExpression::accept(Visitor& visitor) {
    visitor.visit(this);
}

// --- AssignmentExpression ---
AssignmentExpression::AssignmentExpression(SourceLocation loc, ExprPtr l, const token::Token& op_val, ExprPtr r)
    : Expression(loc), left(std::move(l)), op(op_val), right(std::move(r)) {}

AssignmentExpression::~AssignmentExpression() = default;

NodeType AssignmentExpression::getType() const {
    return NodeType::ASSIGNMENT_EXPRESSION;
}

std::string AssignmentExpression::toString() const {
    return "(" + (left ? left->toString() : "nullptr") + " " + op.lexeme + " " + 
           (right ? right->toString() : "nullptr") + ")";
}

void AssignmentExpression::accept(Visitor& visitor) {
    visitor.visit(this);
}

// --- BlockStatement ---
BlockStatement::BlockStatement(SourceLocation loc, std::vector<StmtPtr> body_val)
    : Statement(loc), body(std::move(body_val)) {}

BlockStatement::~BlockStatement() = default;

NodeType BlockStatement::getType() const {
    return NodeType::BLOCK_STATEMENT;
}

std::string BlockStatement::toString() const {
    std::stringstream ss;
    ss << "{\n";
    for (const auto& stmt : body) {
        if (stmt) {
            // Basic indentation for readability, can be improved
            std::string stmtStr = stmt->toString();
            std::string line;
            std::stringstream stmtStream(stmtStr);
            while (std::getline(stmtStream, line)) {
                ss << "  " << line << "\n";
            }
        }
    }
    ss << "}";
    return ss.str();
}

void BlockStatement::accept(Visitor& visitor) {
    visitor.visit(this);
}

// --- EmptyStatement ---
EmptyStatement::EmptyStatement(SourceLocation loc)
    : Statement(loc) {}

NodeType EmptyStatement::getType() const {
    return NodeType::EMPTY_STATEMENT;
}

std::string EmptyStatement::toString() const {
    return ";";
}

void EmptyStatement::accept(Visitor& visitor) {
    visitor.visit(this);
}

// --- TryStatement ---
TryStatement::TryStatement(const SourceLocation& loc_val, std::unique_ptr<BlockStatement> try_block,
                           std::optional<std::string> catch_ident, std::unique_ptr<BlockStatement> catch_block,
                           std::unique_ptr<BlockStatement> finally_block)
    : Statement(loc_val), tryBlock(std::move(try_block)), catchIdent(std::move(catch_ident)),
      catchBlock(std::move(catch_block)), finallyBlock(std::move(finally_block)) {}

NodeType TryStatement::getType() const {
    return NodeType::TRY_STATEMENT;
}

std::string TryStatement::toString() const {
    std::string str = "try " + (tryBlock ? tryBlock->toString() : "{}");
    if (catchBlock) {
        str += " catch";
        if (catchIdent) {
            str += " (" + *catchIdent + ")";
        }
        str += " " + catchBlock->toString();
    }
    if (finallyBlock) {
        str += " finally " + finallyBlock->toString();
    }
    return str;
}

void TryStatement::accept(Visitor& visitor) {
    visitor.visit(this);
}

// --- ExpressionStatement ---
ExpressionStatement::ExpressionStatement(SourceLocation loc, ExprPtr expr)
    : Statement(loc), expression(std::move(expr)) {}

ExpressionStatement::~ExpressionStatement() = default;

NodeType ExpressionStatement::getType() const {
    return NodeType::EXPRESSION_STATEMENT;
}

std::string ExpressionStatement::toString() const {
    return (expression ? expression->toString() : "nullptr") + ";";
}

void ExpressionStatement::accept(Visitor& visitor) {
    visitor.visit(this);
}

// --- IfStatement ---
IfStatement::IfStatement(SourceLocation loc, ExprPtr t, StmtPtr cons, StmtPtr alt)
    : Statement(loc), test(std::move(t)), consequent(std::move(cons)), alternate(std::move(alt)) {}

IfStatement::~IfStatement() = default;

NodeType IfStatement::getType() const {
    return NodeType::IF_STATEMENT;
}

std::string IfStatement::toString() const {
    std::string str = "if (" + (test ? test->toString() : "nullptr") + ") " + 
                      (consequent ? consequent->toString() : "{}");
    if (alternate) {
        str += " else " + alternate->toString();
    }
    return str;
}

void IfStatement::accept(Visitor& visitor) {
    visitor.visit(this);
}

// --- ForStatement ---
ForStatement::ForStatement(SourceLocation loc, NodePtr i, ExprPtr t, ExprPtr u, StmtPtr b)
    : Statement(loc), init(std::move(i)), test(std::move(t)), update(std::move(u)), body(std::move(b)) {}

ForStatement::~ForStatement() = default;

NodeType ForStatement::getType() const {
    return NodeType::FOR_STATEMENT;
}

std::string ForStatement::toString() const {
    std::string initStr = init ? init->toString() : ";";
    // If init is an ExpressionStatement, it already ends with a semicolon.
    // If it's a VariableDeclaration, it might not. We need to be careful here.
    // For simplicity, assume VariableDeclaration::toString() doesn't add a semicolon.
    if (init && init->getType() == NodeType::VARIABLE_DECLARATION) {
        // initStr does not end with ';'
    } else if (init && init->getType() == NodeType::EXPRESSION_STATEMENT) {
        // initStr already ends with ';', remove it for the for loop structure
        if (!initStr.empty() && initStr.back() == ';') {
            initStr.pop_back();
        }
    } else if (!init) {
        initStr = ""; // No initializer part
    }

    return "for (" + initStr + "; " + 
           (test ? test->toString() : "") + "; " + 
           (update ? update->toString() : "") + ") " + 
           (body ? body->toString() : "{}");
}

void ForStatement::accept(Visitor& visitor) {
    visitor.visit(this);
}

// --- WhileStatement ---
WhileStatement::WhileStatement(SourceLocation loc, ExprPtr t, StmtPtr b)
    : Statement(loc), test(std::move(t)), body(std::move(b)) {}

WhileStatement::~WhileStatement() = default;

NodeType WhileStatement::getType() const {
    return NodeType::WHILE_STATEMENT;
}

std::string WhileStatement::toString() const {
    return "while (" + (test ? test->toString() : "nullptr") + ") " + 
           (body ? body->toString() : "{}");
}

void WhileStatement::accept(Visitor& visitor) {
    visitor.visit(this);
}

// --- ReturnStatement ---
ReturnStatement::ReturnStatement(SourceLocation loc, ExprPtr arg)
    : Statement(loc), argument(std::move(arg)) {}

ReturnStatement::~ReturnStatement() = default;

NodeType ReturnStatement::getType() const {
    return NodeType::RETURN_STATEMENT;
}

std::string ReturnStatement::toString() const {
    return "return" + (argument ? " " + argument->toString() : "") + ";";
}

void ReturnStatement::accept(Visitor& visitor) {
    visitor.visit(this);
}

// --- BreakStatement ---
BreakStatement::BreakStatement(SourceLocation loc)
    : Statement(loc) {}

BreakStatement::~BreakStatement() = default;

NodeType BreakStatement::getType() const {
    return NodeType::BREAK_STATEMENT;
}

std::string BreakStatement::toString() const {
    return "break;";
}

void BreakStatement::accept(Visitor& visitor) {
    visitor.visit(this);
}

// --- ContinueStatement ---
ContinueStatement::ContinueStatement(SourceLocation loc)
    : Statement(loc) {}

ContinueStatement::~ContinueStatement() = default;

NodeType ContinueStatement::getType() const {
    return NodeType::CONTINUE_STATEMENT;
}

std::string ContinueStatement::toString() const {
    return "continue;";
}

void ContinueStatement::accept(Visitor& visitor) {
    visitor.visit(this);
}

// --- GenericParameter ---
GenericParameter::GenericParameter(SourceLocation loc, std::unique_ptr<Identifier> n, std::vector<TypeNodePtr> b)
    : Node(loc), name(std::move(n)), bounds(std::move(b)) {}

NodeType GenericParameter::getType() const {
    return NodeType::GENERIC_PARAMETER;
}

std::string GenericParameter::toString() const {
    std::string str = name ? name->toString() : "";
    if (!bounds.empty()) {
        str += ": ";
        for (size_t i = 0; i < bounds.size(); ++i) {
            if (bounds[i]) str += bounds[i]->toString();
            if (i < bounds.size() - 1) str += " + ";
        }
    }
    return str;
}

void GenericParameter::accept(Visitor& visitor) {
    visitor.visit(this);
}

// --- TemplateDeclaration ---
TemplateDeclaration::TemplateDeclaration(SourceLocation loc, std::unique_ptr<Identifier> n, std::vector<std::unique_ptr<GenericParameter>> gp, DeclPtr b)
    : Declaration(loc), name(std::move(n)), genericParams(std::move(gp)), body(std::move(b)) {}

NodeType TemplateDeclaration::getType() const {
    return NodeType::TEMPLATE_DECLARATION;
}

std::string TemplateDeclaration::toString() const {
    std::stringstream ss;
    ss << "template<";
    for (size_t i = 0; i < genericParams.size(); ++i) {
        if (genericParams[i]) ss << genericParams[i]->toString();
        if (i < genericParams.size() - 1) ss << ", ";
    }
    ss << "> " << (body ? body->toString() : "");
    return ss.str();
}

void TemplateDeclaration::accept(Visitor& visitor) {
    visitor.visit(this);
}

// --- Module ---
Module::Module(SourceLocation loc, std::vector<StmtPtr> b)
    : Node(loc), body(std::move(b)) {}

NodeType Module::getType() const {
    return NodeType::MODULE;
}

std::string Module::toString() const {
    std::stringstream ss;
    for (const auto& stmt : body) {
        if (stmt) ss << stmt->toString() << "\n";
    }
    return ss.str();
}

void Module::accept(Visitor& visitor) {
    visitor.visit(this);
}

// --- IfExpression ---
IfExpression::IfExpression(SourceLocation loc, ExprPtr cond, ExprPtr then_b, ExprPtr else_b)
    : Expression(loc), condition(std::move(cond)), thenBranch(std::move(then_b)), elseBranch(std::move(else_b)) {}

NodeType IfExpression::getType() const {
    return NodeType::IF_EXPRESSION;
}

std::string IfExpression::toString() const {
    return "if (" + (condition ? condition->toString() : "nullptr") + ") { " + 
           (thenBranch ? thenBranch->toString() : "nullptr") + " } else { " + 
           (elseBranch ? elseBranch->toString() : "nullptr") + " }";
}

void IfExpression::accept(Visitor& visitor) {
    visitor.visit(this);
}

// --- UnsafeStatement ---
// ast.hpp: UnsafeStatement(SourceLocation loc, std::unique_ptr<BlockStatement> blockStmt)
// Member in hpp: block
// toString() is already declared in hpp.
std::string UnsafeStatement::toString() const {
    return "unsafe " + (block ? block->toString() : "{}");
}

// --- TypeName ---
// ... existing code ...
TypeName::TypeName(SourceLocation loc, std::unique_ptr<Identifier> id, std::vector<TypeNodePtr> args)
    : TypeNode(loc), identifier(std::move(id)), genericArgs(std::move(args)) {}

NodeType TypeName::getType() const {
    return NodeType::TYPE_NAME;
}

std::string TypeName::toString() const {
    std::string str = identifier ? identifier->toString() : "UnknownIdentifier";
    if (!genericArgs.empty()) {
        str += "<";
        for (size_t i = 0; i < genericArgs.size(); ++i) {
            if (genericArgs[i]) str += genericArgs[i]->toString();
            if (i < genericArgs.size() - 1) str += ", ";
        }
        str += ">";
    }
    return str;
}

void TypeName::accept(Visitor& visitor) {
    visitor.visit(this);
}

bool TypeName::isIntegerTy() const {
    // Assuming integer types are named like "int", "int32", "int64", etc.
    // This might need to be more robust depending on the language's type system.
    if (identifier && (identifier->name == "int" || identifier->name == "int8" || identifier->name == "int16" || identifier->name == "int32" || identifier->name == "int64" ||
                       identifier->name == "uint" || identifier->name == "uint8" || identifier->name == "uint16" || identifier->name == "uint32" || identifier->name == "uint64")) {
        return true;
    }
    return false;
}

std::unique_ptr<TypeNode> TypeName::clone() const {
    std::vector<TypeNodePtr> clonedArgs;
    for (const auto& arg : genericArgs) {
        if (arg) {
            clonedArgs.push_back(arg->clone());
        } else {
            clonedArgs.push_back(nullptr);
        }
    }
    return std::make_unique<TypeName>(loc, identifier ? std::make_unique<Identifier>(identifier->loc, identifier->name) : nullptr, std::move(clonedArgs));
}

// --- PointerType ---
PointerType::PointerType(SourceLocation loc, TypeNodePtr pointee)
    : TypeNode(loc), pointeeType(std::move(pointee)) {}

NodeType PointerType::getType() const {
    return NodeType::POINTER_TYPE;
}

std::string PointerType::toString() const {
    return "ptr<" + (pointeeType ? pointeeType->toString() : "UnknownType") + ">";
}

void PointerType::accept(Visitor& visitor) {
    visitor.visit(this);
}

std::unique_ptr<TypeNode> PointerType::clone() const {
    return std::make_unique<PointerType>(loc, pointeeType ? pointeeType->clone() : nullptr);
}

// --- ArrayType ---
ArrayType::ArrayType(SourceLocation loc, TypeNodePtr et, ExprPtr se)
    : TypeNode(loc), elementType(std::move(et)), sizeExpression(std::move(se)) {}

NodeType ArrayType::getType() const {
    return NodeType::ARRAY_TYPE;
}

std::string ArrayType::toString() const {
    return "[" + (elementType ? elementType->toString() : "UnknownType") + 
           (sizeExpression ? "; " + sizeExpression->toString() : "") + "]";
}

void ArrayType::accept(Visitor& visitor) {
    visitor.visit(this);
}

std::unique_ptr<TypeNode> ArrayType::clone() const {
    // Cloning ExprPtr for sizeExpression is tricky as Expr is not a TypeNode.
    // For now, assume sizeExpression doesn't need deep cloning in this context or handle it appropriately.
    // This might require a more general clone mechanism for all Node types if deep copies of expressions are needed.
    // For TypeNode's clone, we primarily care about cloning the type structure.
    return std::make_unique<ArrayType>(loc, elementType ? elementType->clone() : nullptr, nullptr /* shallow copy sizeExpr for now */);
}

// --- FunctionType ---
FunctionType::FunctionType(SourceLocation loc, std::vector<TypeNodePtr> pt, TypeNodePtr rt)
    : TypeNode(loc), parameterTypes(std::move(pt)), returnType(std::move(rt)) {}

NodeType FunctionType::getType() const {
    return NodeType::FUNCTION_TYPE;
}

std::string FunctionType::toString() const {
    std::string str = "fn(";
    for (size_t i = 0; i < parameterTypes.size(); ++i) {
        if (parameterTypes[i]) str += parameterTypes[i]->toString();
        if (i < parameterTypes.size() - 1) str += ", ";
    }
    str += ") -> " + (returnType ? returnType->toString() : "void");
    return str;
}

void FunctionType::accept(Visitor& visitor) {
    visitor.visit(this);
}

std::unique_ptr<TypeNode> FunctionType::clone() const {
    std::vector<TypeNodePtr> clonedParams;
    for (const auto& param : parameterTypes) {
        if (param) {
            clonedParams.push_back(param->clone());
        } else {
            clonedParams.push_back(nullptr);
        }
    }
    return std::make_unique<FunctionType>(loc, std::move(clonedParams), returnType ? returnType->clone() : nullptr);
}

// --- OptionalType ---
OptionalType::OptionalType(SourceLocation loc, TypeNodePtr ct)
    : TypeNode(loc), containedType(std::move(ct)) {}

NodeType OptionalType::getType() const {
    return NodeType::OPTIONAL_TYPE;
}

std::string OptionalType::toString() const {
    return (containedType ? containedType->toString() : "UnknownType") + "?";
}

void OptionalType::accept(Visitor& visitor) {
    visitor.visit(this);
}

std::unique_ptr<TypeNode> OptionalType::clone() const {
    return std::make_unique<OptionalType>(loc, containedType ? containedType->clone() : nullptr);
}

// --- TupleTypeNode ---
TupleTypeNode::TupleTypeNode(SourceLocation loc, std::vector<TypeNodePtr> mt)
    : TypeNode(loc), memberTypes(std::move(mt)) {}

NodeType TupleTypeNode::getType() const {
    return NodeType::TUPLE_TYPE;
}

std::string TupleTypeNode::toString() const {
    std::string str = "(";
    for (size_t i = 0; i < memberTypes.size(); ++i) {
        if (memberTypes[i]) str += memberTypes[i]->toString();
        if (i < memberTypes.size() - 1) str += ", ";
    }
    str += ")";
    return str;
}

void TupleTypeNode::accept(Visitor& visitor) {
    visitor.visit(this);
}

std::unique_ptr<TypeNode> TupleTypeNode::clone() const {
    std::vector<TypeNodePtr> clonedMembers;
    for (const auto& member : memberTypes) {
        if (member) {
            clonedMembers.push_back(member->clone());
        } else {
            clonedMembers.push_back(nullptr);
        }
    }
    return std::make_unique<TupleTypeNode>(loc, std::move(clonedMembers));
}

// --- ImportDeclaration ---
ImportDeclaration::ImportDeclaration(SourceLocation loc_param,
                                     std::unique_ptr<StringLiteral> source_param,
                                     std::vector<ImportSpecifier> specifiers_param,
                                     std::unique_ptr<Identifier> defaultImport_param,
                                     std::unique_ptr<Identifier> namespaceImport_param)
    : Declaration(loc_param),
      source(std::move(source_param)),
      specifiers(std::move(specifiers_param)),
      defaultImport(std::move(defaultImport_param)),
      namespaceImport(std::move(namespaceImport_param)) {}

NodeType ImportDeclaration::getType() const {
    return NodeType::IMPORT_DECLARATION;
}

std::string ImportDeclaration::toString() const {
    std::string result = "import ";
    bool needsFrom = false;
    if (defaultImport) {
        result += defaultImport->toString();
        needsFrom = true;
    }

    if (!specifiers.empty()) {
        if (needsFrom) result += ", ";
        result += "{";
        for (size_t i = 0; i < specifiers.size(); ++i) {
            if (specifiers[i].importedName) { // Should always be true for a valid specifier
                result += specifiers[i].importedName->toString();
            }
            if (specifiers[i].localName) {
                result += " as " + specifiers[i].localName->toString();
            }
            if (i < specifiers.size() - 1) {
                result += ", ";
            }
        }
        result += "}";
        needsFrom = true;
    }

    if (namespaceImport) {
        if (needsFrom && (defaultImport || !specifiers.empty())) result += ", "; // Comma if other imports precede
        result += "* as " + namespaceImport->toString();
        needsFrom = true;
    }
    
    if (needsFrom) {
         result += " from ";
    }
    // If none of defaultImport, specifiers, or namespaceImport are present,
    // it implies a direct import of the source, e.g. import "module";
    // The parser currently creates specifiers like { null as alias } for 'import "path" as alias'
    // which is a bit unusual. The toString needs to handle specifiers[i].importedName being potentially null
    // if the parser logic for `import "path" as alias;` creates an ImportSpecifier with `importedName=nullptr` and `localName=alias`.
    // However, the current `ImportSpecifier` struct implies `importedName` is primary.
    // The `declaration_parser.cpp` for `import path as alias` does:
    // `specifiers.emplace_back(nullptr, std::move(alias));`
    // This means `importedName` can be null.
    // A more typical structure for `import "path" as M;` would use `namespaceImport`.
    // The current `ImportDeclaration` constructor call from `parse_import_declaration` is:
    // `std::make_unique<vyn::ast::ImportDeclaration>(loc, std::move(source), std::move(specifiers));`
    // If `alias` was present, `specifiers` contains one item: `{importedName=nullptr, localName=alias}`.
    // This is not ideal. `toString()` will try to print `specifiers[i].importedName->toString()`.
    // For now, let's assume `importedName` is always valid if a specifier exists.
    // The parser logic might need adjustment for `import "path" as M;` to use `namespaceImport`.

    result += source->toString();
    result += ";";
    return result;
}

void ImportDeclaration::accept(Visitor& visitor) {
    visitor.visit(this);
}

// --- VariableDeclaration ---
// ... existing code ...
VariableDeclaration::VariableDeclaration(SourceLocation loc, std::unique_ptr<Identifier> i, bool is_const, TypeNodePtr type_node, ExprPtr val_init)
    : Declaration(loc), id(std::move(i)), isConst(is_const), typeNode(std::move(type_node)), init(std::move(val_init)) {}

NodeType VariableDeclaration::getType() const {
    return NodeType::VARIABLE_DECLARATION;
}

std::string VariableDeclaration::toString() const {
    std::string str = isConst ? "let " : "var ";
    str += id ? id->toString() : "";
    if (typeNode) {
        str += ": " + typeNode->toString();
    }
    if (init) {
        str += " = " + init->toString();
    }
    str += ";";
    return str;
}

void VariableDeclaration::accept(Visitor& visitor) {
    visitor.visit(this);
}

// --- FunctionDeclaration ---
// ... existing code ...
FunctionDeclaration::FunctionDeclaration(SourceLocation loc, std::unique_ptr<Identifier> i, std::vector<FunctionParameter> ps, std::unique_ptr<BlockStatement> b, bool is_async, TypeNodePtr ret_type_node)
    : Declaration(loc), id(std::move(i)), params(std::move(ps)), body(std::move(b)), isAsync(is_async), returnTypeNode(std::move(ret_type_node)) {}

NodeType FunctionDeclaration::getType() const {
    return NodeType::FUNCTION_DECLARATION;
}

std::string FunctionDeclaration::toString() const {
    std::stringstream ss;
    if (isAsync) ss << "async ";
    ss << "fn " << (id ? id->toString() : "") << "(";
    for (size_t i = 0; i < params.size(); ++i) {
        ss << params[i].name->toString();
        if (params[i].typeNode) {
            ss << ": " << params[i].typeNode->toString();
        }
        if (i < params.size() - 1) ss << ", ";
    }
    ss << ")";
    if (returnTypeNode) {
        ss << " -> " << returnTypeNode->toString();
    }
    ss << " " << (body ? body->toString() : "{}");
    return ss.str();
}

void FunctionDeclaration::accept(Visitor& visitor) {
    visitor.visit(this);
}

// --- TypeAliasDeclaration ---
// ... existing code ...
TypeAliasDeclaration::TypeAliasDeclaration(SourceLocation loc, std::unique_ptr<Identifier> n, TypeNodePtr tn)
    : Declaration(loc), name(std::move(n)), typeNode(std::move(tn)) {}

NodeType TypeAliasDeclaration::getType() const {
    return NodeType::TYPE_ALIAS_DECLARATION;
}

std::string TypeAliasDeclaration::toString() const {
    return "type " + (name ? name->toString() : "") + " = " + (typeNode ? typeNode->toString() : "UnknownType") + ";";
}

void TypeAliasDeclaration::accept(Visitor& visitor) {
    visitor.visit(this);
}

// --- FieldDeclaration ---
// ... existing code ...
FieldDeclaration::FieldDeclaration(SourceLocation loc, std::unique_ptr<Identifier> n, TypeNodePtr tn, ExprPtr init_val, bool is_mut)
    : Declaration(loc), name(std::move(n)), typeNode(std::move(tn)), initializer(std::move(init_val)), isMutable(is_mut) {}

NodeType FieldDeclaration::getType() const {
    return NodeType::FIELD_DECLARATION;
}

std::string FieldDeclaration::toString() const {
    std::string str = (isMutable ? "mut " : "") + (name ? name->toString() : "");
    if (typeNode) {
        str += ": " + typeNode->toString();
    }
    if (initializer) {
        str += " = " + initializer->toString();
    }
    str += ";";
    return str;
}

void FieldDeclaration::accept(Visitor& visitor) {
    visitor.visit(this);
}

// --- StructDeclaration ---
// ... existing code ...
StructDeclaration::StructDeclaration(SourceLocation loc, std::unique_ptr<Identifier> n, std::vector<std::unique_ptr<GenericParameter>> gp, std::vector<std::unique_ptr<FieldDeclaration>> flds)
    : Declaration(loc), name(std::move(n)), genericParams(std::move(gp)), fields(std::move(flds)) {}

NodeType StructDeclaration::getType() const {
    return NodeType::STRUCT_DECLARATION;
}

std::string StructDeclaration::toString() const {
    std::stringstream ss;
    ss << "struct " << (name ? name->toString() : "");
    if (!genericParams.empty()) {
        ss << "<";
        for (size_t i = 0; i < genericParams.size(); ++i) {
            if (genericParams[i]) ss << genericParams[i]->toString();
            if (i < genericParams.size() - 1) ss << ", ";
        }
        ss << ">";
    }
    ss << " {\n";
    for (const auto& field : fields) {
        if (field) {
            // Basic indentation
            std::string fieldStr = field->toString();
            std::string line;
            std::stringstream fieldStream(fieldStr);
            while (std::getline(fieldStream, line)) {
                ss << "  " << line << "\n";
            }
        }
    }
    ss << "}";
    return ss.str();
}

void StructDeclaration::accept(Visitor& visitor) {
    visitor.visit(this);
}

// --- ClassDeclaration ---
// ... existing code ...
ClassDeclaration::ClassDeclaration(SourceLocation loc, std::unique_ptr<Identifier> n, std::vector<std::unique_ptr<GenericParameter>> gp, std::vector<DeclPtr> mems)
    : Declaration(loc), name(std::move(n)), genericParams(std::move(gp)), members(std::move(mems)) {}

NodeType ClassDeclaration::getType() const {
    return NodeType::CLASS_DECLARATION;
}

std::string ClassDeclaration::toString() const {
    std::stringstream ss;
    ss << "class " << (name ? name->toString() : "");
    if (!genericParams.empty()) {
        ss << "<";
        for (size_t i = 0; i < genericParams.size(); ++i) {
            if (genericParams[i]) ss << genericParams[i]->toString();
            if (i < genericParams.size() - 1) ss << ", ";
        }
        ss << ">";
    }
    ss << " {\n";
    for (const auto& member : members) {
        if (member) {
            // Basic indentation
            std::string memberStr = member->toString();
            std::string line;
            std::stringstream memberStream(memberStr);
            while (std::getline(memberStream, line)) {
                ss << "  " << line << "\n";
            }
        }
    }
    ss << "}";
    return ss.str();
}

void ClassDeclaration::accept(Visitor& visitor) {
    visitor.visit(this);
}

// --- ImplDeclaration ---
// ... existing code ...
ImplDeclaration::ImplDeclaration(SourceLocation loc, TypeNodePtr self_ty, std::vector<std::unique_ptr<FunctionDeclaration>> meths, std::unique_ptr<Identifier> n, std::vector<std::unique_ptr<GenericParameter>> gp, TypeNodePtr trait_ty)
    : Declaration(loc), selfType(std::move(self_ty)), methods(std::move(meths)), name(std::move(n)), genericParams(std::move(gp)), traitType(std::move(trait_ty)) {}

NodeType ImplDeclaration::getType() const {
    return NodeType::IMPL_DECLARATION;
}

std::string ImplDeclaration::toString() const {
    std::stringstream ss;
    ss << "impl";
    if (!genericParams.empty()) {
        ss << "<";
        for (size_t i = 0; i < genericParams.size(); ++i) {
            if (genericParams[i]) ss << genericParams[i]->toString();
            if (i < genericParams.size() - 1) ss << ", ";
        }
        ss << ">";
    }
    if (traitType) {
        ss << " " << traitType->toString();
    }
    ss << " for " << (selfType ? selfType->toString() : "UnknownType");
    if (name) {
        ss << " as " << name->toString();
    }
    ss << " {\n";
    for (const auto& method : methods) {
        if (method) {
            // Basic indentation
            std::string methodStr = method->toString();
            std::string line;
            std::stringstream methodStream(methodStr);
            while (std::getline(methodStream, line)) {
                ss << "  " << line << "\n";
            }
        }
    }
    ss << "}";
    return ss.str();
}

void ImplDeclaration::accept(Visitor& visitor) {
    visitor.visit(this);
}

// --- EnumVariant ---
// ... existing code ...
EnumVariant::EnumVariant(SourceLocation loc, std::unique_ptr<Identifier> n, std::vector<TypeNodePtr> assoc_types)
    : Node(loc), name(std::move(n)), associatedTypes(std::move(assoc_types)) {}

NodeType EnumVariant::getType() const {
    return NodeType::ENUM_VARIANT;
}

std::string EnumVariant::toString() const {
    std::string str = name ? name->toString() : "";
    if (!associatedTypes.empty()) {
        str += "(";
        for (size_t i = 0; i < associatedTypes.size(); ++i) {
            if (associatedTypes[i]) str += associatedTypes[i]->toString();
            if (i < associatedTypes.size() - 1) str += ", ";
        }
        str += ")";
    }
    return str;
}

void EnumVariant::accept(Visitor& visitor) {
    visitor.visit(this);
}

// --- EnumDeclaration ---
// ... existing code ...
EnumDeclaration::EnumDeclaration(SourceLocation loc, std::unique_ptr<Identifier> n, std::vector<std::unique_ptr<GenericParameter>> gp, std::vector<std::unique_ptr<EnumVariant>> vars)
    : Declaration(loc), name(std::move(n)), genericParams(std::move(gp)), variants(std::move(vars)) {}

NodeType EnumDeclaration::getType() const {
    return NodeType::ENUM_DECLARATION;
}

std::string EnumDeclaration::toString() const {
    std::stringstream ss;
    ss << "enum " << (name ? name->toString() : "");
    if (!genericParams.empty()) {
        ss << "<";
        for (size_t i = 0; i < genericParams.size(); ++i) {
            if (genericParams[i]) ss << genericParams[i]->toString();
            if (i < genericParams.size() - 1) ss << ", ";
        }
        ss << ">";
    }
    ss << " {\n";
    for (size_t i = 0; i < variants.size(); ++i) {
        if (variants[i]) {
            // Basic indentation
            std::string variantStr = variants[i]->toString();
            std::string line;
            std::stringstream variantStream(variantStr);
            while (std::getline(variantStream, line)) {
                ss << "  " << line;
            }
        }
        if (i < variants.size() - 1) ss << ",";
        ss << "\n";
    }
    ss << "}";
    return ss.str();
}

void EnumDeclaration::accept(Visitor& visitor) {
    visitor.visit(this);
}

// --- LogicalExpression ---
LogicalExpression::LogicalExpression(SourceLocation loc, ExprPtr left, const token::Token& op, ExprPtr right)
    : Expression(loc), left(std::move(left)), op(op), right(std::move(right)) {}

NodeType LogicalExpression::getType() const {
    return NodeType::LOGICAL_EXPRESSION;
}

std::string LogicalExpression::toString() const {
    return "(" + (left ? left->toString() : "nullptr") + " " + op.lexeme + " " + (right ? right->toString() : "nullptr") + ")";
}

void LogicalExpression::accept(Visitor& visitor) {
    visitor.visit(this);
}

// --- ConditionalExpression ---
ConditionalExpression::ConditionalExpression(SourceLocation loc, ExprPtr condition, ExprPtr thenExpr, ExprPtr elseExpr)
    : Expression(loc), condition(std::move(condition)), thenExpr(std::move(thenExpr)), elseExpr(std::move(elseExpr)) {}

NodeType ConditionalExpression::getType() const {
    return NodeType::CONDITIONAL_EXPRESSION;
}

std::string ConditionalExpression::toString() const {
    return "(" + (condition ? condition->toString() : "nullptr") + " ? " + (thenExpr ? thenExpr->toString() : "nullptr") + " : " + (elseExpr ? elseExpr->toString() : "nullptr") + ")";
}

void ConditionalExpression::accept(Visitor& visitor) {
    visitor.visit(this);
}

// --- SequenceExpression ---
SequenceExpression::SequenceExpression(SourceLocation loc, std::vector<ExprPtr> expressions)
    : Expression(loc), expressions(std::move(expressions)) {}

NodeType SequenceExpression::getType() const {
    return NodeType::SEQUENCE_EXPRESSION;
}

std::string SequenceExpression::toString() const {
    std::string str = "(";
    for (size_t i = 0; i < expressions.size(); ++i) {
        str += expressions[i] ? expressions[i]->toString() : "nullptr";
        if (i < expressions.size() - 1) str += ", ";
    }
    str += ")";
    return str;
}

void SequenceExpression::accept(Visitor& visitor) {
    visitor.visit(this);
}

// --- FunctionExpression ---
FunctionExpression::FunctionExpression(SourceLocation loc, std::vector<FunctionParameter> params, ExprPtr body, bool isAsync)
    : Expression(loc), params(std::move(params)), body(std::move(body)), isAsync(isAsync) {}

NodeType FunctionExpression::getType() const {
    return NodeType::FUNCTION_EXPRESSION;
}

std::string FunctionExpression::toString() const {
    std::string str = (isAsync ? std::string("async ") : std::string("")) + "fn(";
    for (size_t i = 0; i < params.size(); ++i) {
        str += params[i].name ? params[i].name->toString() : "_";
        if (params[i].typeNode) str += ": " + params[i].typeNode->toString();
        if (i < params.size() - 1) str += ", ";
    }
    str += ") => ";
    str += body ? body->toString() : "nullptr";
    return str;
}

void FunctionExpression::accept(Visitor& visitor) {
    visitor.visit(this);
}

// --- ThisExpression ---
ThisExpression::ThisExpression(SourceLocation loc)
    : Expression(loc) {}

NodeType ThisExpression::getType() const {
    return NodeType::THIS_EXPRESSION;
}

std::string ThisExpression::toString() const {
    return "this";
}

void ThisExpression::accept(Visitor& visitor) {
    visitor.visit(this);
}

// --- SuperExpression ---
SuperExpression::SuperExpression(SourceLocation loc)
    : Expression(loc) {}

NodeType SuperExpression::getType() const {
    return NodeType::SUPER_EXPRESSION;
}

std::string SuperExpression::toString() const {
    return "super";
}

void SuperExpression::accept(Visitor& visitor) {
    visitor.visit(this);
}

// --- AwaitExpression ---
AwaitExpression::AwaitExpression(SourceLocation loc, ExprPtr expr)
    : Expression(loc), expr(std::move(expr)) {}

NodeType AwaitExpression::getType() const {
    return NodeType::AWAIT_EXPRESSION;
}

std::string AwaitExpression::toString() const {
    return "await " + (expr ? expr->toString() : "nullptr");
}

void AwaitExpression::accept(Visitor& visitor) {
    visitor.visit(this);
}

// --- ThrowStatement ---
ThrowStatement::ThrowStatement(SourceLocation loc, ExprPtr expr)
    : Statement(loc), expr(std::move(expr)) {}

NodeType ThrowStatement::getType() const {
    return NodeType::THROW_STATEMENT;
}

std::string ThrowStatement::toString() const {
    return "throw " + (expr ? expr->toString() : "nullptr");
}

void ThrowStatement::accept(Visitor& visitor) {
    visitor.visit(this);
}

// --- MatchStatement ---
MatchStatement::MatchStatement(SourceLocation loc, ExprPtr expr, std::vector<std::pair<ExprPtr, ExprPtr>> cases)
    : Statement(loc), expr(std::move(expr)), cases(std::move(cases)) {}

NodeType MatchStatement::getType() const {
    return NodeType::MATCH_STATEMENT;
}

std::string MatchStatement::toString() const {
    std::string str = "match " + (expr ? expr->toString() : "nullptr") + " { ";
    for (const auto& c : cases) {
        str += (c.first ? c.first->toString() : "_") + " => " + (c.second ? c.second->toString() : "nullptr") + "; ";
    }
    str += "}";
    return str;
}

void MatchStatement::accept(Visitor& visitor) {
    visitor.visit(this);
}

// --- TraitDeclaration ---
TraitDeclaration::TraitDeclaration(SourceLocation loc, std::unique_ptr<Identifier> n, std::vector<std::unique_ptr<GenericParameter>> gp, std::vector<std::unique_ptr<FunctionDeclaration>> meths)
    : Declaration(loc), name(std::move(n)), genericParams(std::move(gp)), methods(std::move(meths)) {}

NodeType TraitDeclaration::getType() const {
    return NodeType::TRAIT_DECLARATION;
}

std::string TraitDeclaration::toString() const {
    std::stringstream ss;
    ss << "trait " << (name ? name->toString() : "") << " {\n";
    for (const auto& method : methods) {
        if (method) {
            std::string methodStr = method->toString();
            std::string line;
            std::stringstream methodStream(methodStr);
            while (std::getline(methodStream, line)) {
                ss << "  " << line << "\n";
            }
        }
    }
    ss << "}";
    return ss.str();
}

void TraitDeclaration::accept(Visitor& visitor) {
    visitor.visit(this);
}

// --- NamespaceDeclaration ---
NamespaceDeclaration::NamespaceDeclaration(SourceLocation loc, std::unique_ptr<Identifier> n, std::vector<DeclPtr> mems)
    : Declaration(loc), name(std::move(n)), members(std::move(mems)) {}

NodeType NamespaceDeclaration::getType() const {
    return NodeType::NAMESPACE_DECLARATION;
}

std::string NamespaceDeclaration::toString() const {
    std::stringstream ss;
    ss << "namespace " << (name ? name->toString() : "") << " {\n";
    for (const auto& member : members) {
        if (member) {
            std::string memberStr = member->toString();
            std::string line;
            std::stringstream memberStream(memberStr);
            while (std::getline(memberStream, line)) {
                ss << "  " << line << "\n";
            }
        }
    }
    ss << "}";
    return ss.str();
}

void NamespaceDeclaration::accept(Visitor& visitor) {
    visitor.visit(this);
}

// --- AssertStatement ---
AssertStatement::AssertStatement(SourceLocation loc, ExprPtr cond, ExprPtr msg)
    : Statement(loc), condition(std::move(cond)), message(std::move(msg)) {}

NodeType AssertStatement::getType() const {
    return NodeType::ASSERT_STATEMENT;
}

std::string AssertStatement::toString() const {
    std::string str = "assert(" + (condition ? condition->toString() : "") + ")";
    if (message) {
        str += ", " + message->toString();
    }
    str += ";";
    return str;
}

void AssertStatement::accept(Visitor& visitor) {
    visitor.visit(this);
}

} // namespace ast
} // namespace vyn

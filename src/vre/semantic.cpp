#include "vyn/semantic.hpp"
#include "vyn/parser/token.hpp"
#include "vyn/parser/ast.hpp"
#include "vyn/driver.hpp"
#include <stdexcept>
#include <memory>

namespace vyn {

// Scope class implementation
Scope::Scope(Scope* parent_scope) : parent(parent_scope) {}

SymbolInfo* Scope::find(const std::string& name) {
    auto it = symbols.find(name);
    if (it != symbols.end()) {
        return it->second;
    }
    if (parent) {
        return parent->find(name);
    }
    return nullptr;
}

void Scope::insert(const std::string& name, SymbolInfo* symbol) {
    symbols[name] = symbol;
}

Scope* Scope::getParent() {
    return parent;
}

// --- SemanticAnalyzer Implementation ---

void checkBorrow(ast::Node* node, const std::string& owner, bool isMutableBorrow, ast::TypeNode* type) {
    // Placeholder
}

void checkLocUnsafe(ast::Node* node) {
    // Placeholder
}

bool isRawLocationType(ast::Expression* expr) {
    // Placeholder
    return false;
}

// SemanticAnalyzer constructor
SemanticAnalyzer::SemanticAnalyzer(Driver& driver) : driver_(driver), currentScope(nullptr) {
    enterScope(); // Create global scope
}

// Basic visit methods for expressions
void SemanticAnalyzer::visit(ast::Identifier* node) {
    SymbolInfo* symbol = currentScope->lookup(node->name);
    if (!symbol) {
        addError("Undefined identifier: " + node->name, node);
        return;
    }
    expressionTypes[node] = symbol->type;
}

void SemanticAnalyzer::visit(ast::IntegerLiteral* node) {
    // Integer literals are of type 'int'
    expressionTypes[node] = new ast::TypeName(node->loc, std::make_unique<ast::Identifier>(node->loc, "int"));
}

void SemanticAnalyzer::visit(ast::FloatLiteral* node) {
    // Float literals are of type 'float'
    expressionTypes[node] = new ast::TypeName(node->loc, std::make_unique<ast::Identifier>(node->loc, "float"));
}

void SemanticAnalyzer::visit(ast::StringLiteral* node) {
    // String literals are of type 'string'
    expressionTypes[node] = new ast::TypeName(node->loc, std::make_unique<ast::Identifier>(node->loc, "string"));
}

void SemanticAnalyzer::visit(ast::BooleanLiteral* node) {
    // Boolean literals are of type 'bool'
    expressionTypes[node] = new ast::TypeName(node->loc, std::make_unique<ast::Identifier>(node->loc, "bool"));
}

void SemanticAnalyzer::visit(ast::NilLiteral* node) {
    // Nil literals are of type 'nil'
    expressionTypes[node] = new ast::TypeName(node->loc, std::make_unique<ast::Identifier>(node->loc, "nil"));
}

// Basic visit methods for statements
void SemanticAnalyzer::visit(ast::BlockStatement* node) {
    enterScope();
    for (auto& stmt : node->body) {
        stmt->accept(*this);
    }
    exitScope();
}

void SemanticAnalyzer::visit(ast::ExpressionStatement* node) {
    if (node->expression) {
        node->expression->accept(*this);
    }
}

void SemanticAnalyzer::visit(ast::EmptyStatement* node) {
    // Empty statements don't need any analysis
}

// Helper methods
void SemanticAnalyzer::addError(const std::string& message, const ast::Node* node) {
    errors.push_back(message);
}

void SemanticAnalyzer::enterScope() {
    currentScope = new SymbolTable(currentScope);
}

void SemanticAnalyzer::exitScope() {
    if (currentScope) {
        SymbolTable* parent = currentScope->getParent();
        delete currentScope;
        currentScope = parent;
    }
}

void SemanticAnalyzer::analyze(ast::Module* root) {
    if (root) {
        root->accept(*this);
    }
}

bool SemanticAnalyzer::isLValue(ast::Expression* expr) {
    return dynamic_cast<ast::Identifier*>(expr) != nullptr ||
           dynamic_cast<ast::MemberExpression*>(expr) != nullptr ||
           dynamic_cast<ast::ArrayElementExpression*>(expr) != nullptr;
}

bool SemanticAnalyzer::isRawLocationType(ast::Expression* expr) {
    auto it = expressionTypes.find(expr);
    if (it == expressionTypes.end()) return false;
    return dynamic_cast<ast::PointerType*>(it->second) != nullptr;
}

// Placeholder implementations for remaining visit methods
void SemanticAnalyzer::visit(ast::UnaryExpression* node) {}
void SemanticAnalyzer::visit(ast::BinaryExpression* node) {}
void SemanticAnalyzer::visit(ast::CallExpression* node) {}
void SemanticAnalyzer::visit(ast::ArrayElementExpression* node) {}
void SemanticAnalyzer::visit(ast::MemberExpression* node) {}
void SemanticAnalyzer::visit(ast::AssignmentExpression* node) {}
void SemanticAnalyzer::visit(ast::LogicalExpression* node) {}
void SemanticAnalyzer::visit(ast::ConditionalExpression* node) {}
void SemanticAnalyzer::visit(ast::SequenceExpression* node) {}
void SemanticAnalyzer::visit(ast::ObjectLiteral* node) {}
void SemanticAnalyzer::visit(ast::ArrayLiteral* node) {}
void SemanticAnalyzer::visit(ast::FunctionExpression* node) {}
void SemanticAnalyzer::visit(ast::ThisExpression* node) {}
void SemanticAnalyzer::visit(ast::SuperExpression* node) {}
void SemanticAnalyzer::visit(ast::AwaitExpression* node) {}
void SemanticAnalyzer::visit(ast::ListComprehension* node) {}
void SemanticAnalyzer::visit(ast::GenericInstantiationExpression* node) {}
void SemanticAnalyzer::visit(ast::PointerDerefExpression* node) {
    // Check if we're in an unsafe block
    if (!isInUnsafeBlock()) {
        addError("Pointer dereference (at) is only allowed inside an unsafe block.", node);
        return;
    }

    // Visit the pointer expression
    node->pointer->accept(*this);

    // Check if the expression is a pointer type
    auto it = expressionTypes.find(node->pointer.get());
    if (it == expressionTypes.end() || !isRawLocationType(node->pointer.get())) {
        addError("Cannot dereference non-pointer type", node);
        return;
    }

    // Set the type of the dereferenced expression
    auto* ptrType = dynamic_cast<ast::PointerType*>(it->second);
    if (!ptrType) {
        addError("Invalid pointer type", node);
        return;
    }
    expressionTypes[node] = ptrType->pointeeType.get();
}
void SemanticAnalyzer::visit(ast::AddrOfExpression* node) {
    // Visit the expression to get its type
    node->getLocation()->accept(*this);

    // Check if the expression is an lvalue
    if (!isLValue(node->getLocation().get())) {
        addError("Cannot take address of non-lvalue", node);
        return;
    }

    // Get the type of the expression
    auto it = expressionTypes.find(node->getLocation().get());
    if (it == expressionTypes.end()) {
        addError("Cannot take address of expression with unknown type", node);
        return;
    }

    // Create a new pointer type with the same type as the expression
    auto* pointeeType = it->second;
    expressionTypes[node] = new ast::PointerType(node->loc, std::unique_ptr<ast::TypeNode>(pointeeType));
}
void SemanticAnalyzer::visit(ast::FromIntToLocExpression* node) {
    // Example: Ensure the expression being converted is an integer type
    if (node->getAddressExpression()) {
        node->getAddressExpression()->accept(*this);
        // Check type of address_expr
        auto it = expressionTypes.find(node->getAddressExpression().get());
        if (it == expressionTypes.end() || (it->second && !it->second->isIntegerTy())) { // Added null check for it->second
            addError("Expression in from<T>() must be an integer type.", node);
        }
    } else {
        addError("Missing address expression in from<T>().", node);
    }

    if (node->getTargetType()) {
        node->getTargetType()->accept(*this);
        // Ensure target type is a location type (pointer)
        if (!node->getTargetType()->isLocationTy()) { // Assuming isLocationTy() or similar exists
            addError("Target type in from<T>() must be a location/pointer type.", node);
        }
    } else {
        addError("Missing target type in from<T>().", node);
    }
    // The type of FromIntToLocExpression is its target_type
    if (node->getTargetType()) { // Added null check for getTargetType()
        expressionTypes[node] = node->getTargetType().get();
    }
}
void SemanticAnalyzer::visit(ast::LocationExpression* node) {
    // Check if we're in an unsafe block
    if (!isInUnsafeBlock()) {
        addError("loc() (location-of) is only allowed inside an unsafe block.", node);
        return;
    }

    // Visit the expression to get its type
    // node->getExpression()->accept(*this); // Original line with getExpression()
    if (node->expression) { // Use direct member access
        node->expression->accept(*this);
    } else {
        addError("Missing expression in loc().", node);
        return; // Or handle error appropriately
    }

    // Create a pointer type for the expression
    // auto it = expressionTypes.find(node->getExpression().get()); // Original line
    auto it = expressionTypes.find(node->expression.get()); // Use direct member access
    if (it == expressionTypes.end()) {
        addError("Cannot get location of expression with unknown type", node);
        return;
    }

    // Set the type of the location expression
    auto* pointeeType = it->second;
    if (pointeeType) { // Added null check for pointeeType
        expressionTypes[node] = new ast::PointerType(node->loc, std::unique_ptr<ast::TypeNode>(pointeeType->clone())); // Assuming clone() method exists or handle ownership
    } else {
        addError("Pointee type is null in LocationExpression.", node);
    }
}
void SemanticAnalyzer::visit(ast::IfStatement* node) {
    // Visit the test condition
    node->test->accept(*this);

    // Visit the consequent block
    node->consequent->accept(*this);

    // Visit the alternate block if it exists
    if (node->alternate) {
        node->alternate->accept(*this);
    }
}
void SemanticAnalyzer::visit(ast::ForStatement* node) {
    // Enter a new scope for the loop
    enterScope();
    currentScope->isLoop = true;

    // Visit the initialization
    if (node->init) {
        node->init->accept(*this);
    }

    // Visit the test condition
    if (node->test) {
        node->test->accept(*this);
    }

    // Visit the update expression
    if (node->update) {
        node->update->accept(*this);
    }

    // Visit the body
    node->body->accept(*this);

    // Exit the loop scope
    exitScope();
}
void SemanticAnalyzer::visit(ast::WhileStatement* node) {
    // Enter a new scope for the loop
    enterScope();
    currentScope->isLoop = true;

    // Visit the test condition
    node->test->accept(*this);

    // Visit the body
    node->body->accept(*this);

    // Exit the loop scope
    exitScope();
}
void SemanticAnalyzer::visit(ast::ReturnStatement* node) {
    // Visit the return value if it exists
    if (node->argument) {
        node->argument->accept(*this);
    }
}
void SemanticAnalyzer::visit(ast::BreakStatement* node) {
    // Check if we're inside a loop
    if (!isInLoop()) {
        errors.push_back("Break statement must be inside a loop");
    }
}
void SemanticAnalyzer::visit(ast::ContinueStatement* node) {
    // Check if we're inside a loop
    if (!isInLoop()) {
        errors.push_back("Continue statement must be inside a loop");
    }
}
void SemanticAnalyzer::visit(ast::TryStatement* node) {
    // Visit the try block
    node->tryBlock->accept(*this);

    // Visit the catch block if it exists
    if (node->catchBlock) {
        node->catchBlock->accept(*this);
    }

    // Visit the finally block if it exists
    if (node->finallyBlock) {
        node->finallyBlock->accept(*this);
    }
}
void SemanticAnalyzer::visit(ast::UnsafeStatement* node) {
    // Enter a new scope for the unsafe block
    enterScope();
    currentScope->isUnsafeBlock = true;
    node->block->accept(*this);
    exitScope();
}
void SemanticAnalyzer::visit(ast::AssertStatement* node) {}
void SemanticAnalyzer::visit(ast::MatchStatement* node) {}
void SemanticAnalyzer::visit(ast::YieldStatement* node) {}
void SemanticAnalyzer::visit(ast::YieldReturnStatement* node) {}
void SemanticAnalyzer::visit(ast::VariableDeclaration* node) {}
void SemanticAnalyzer::visit(ast::FunctionDeclaration* node) {}
void SemanticAnalyzer::visit(ast::TypeAliasDeclaration* node) {}
void SemanticAnalyzer::visit(ast::ImportDeclaration* node) {}
void SemanticAnalyzer::visit(ast::StructDeclaration* node) {}
void SemanticAnalyzer::visit(ast::ClassDeclaration* node) {}
void SemanticAnalyzer::visit(ast::EnumDeclaration* node) {}
void SemanticAnalyzer::visit(ast::TraitDeclaration* node) {}
void SemanticAnalyzer::visit(ast::ImplDeclaration* node) {}
void SemanticAnalyzer::visit(ast::NamespaceDeclaration* node) {}
void SemanticAnalyzer::visit(ast::TypeName* node) {}
void SemanticAnalyzer::visit(ast::PointerType* node) {}
void SemanticAnalyzer::visit(ast::ArrayType* node) {}
void SemanticAnalyzer::visit(ast::FunctionType* node) {}
void SemanticAnalyzer::visit(ast::OptionalType* node) {}
void SemanticAnalyzer::visit(ast::GenericParameter* node) {}

bool SemanticAnalyzer::isInLoop() {
    // Check if we're inside a loop by looking at the current scope
    for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
        if ((*it)->isLoop) {
            return true;
        }
    }
    return false;
}

bool SemanticAnalyzer::isInUnsafeBlock() {
    // Check if we're in an unsafe block by traversing up the scope chain
    SymbolTable* scope = currentScope;
    while (scope) {
        if (scope->isUnsafeBlock) return true;
        scope = scope->getParent();
    }
    return false;
}

bool SemanticAnalyzer::isIntegerType(ast::TypeNode* type) {
    auto* typeName = dynamic_cast<ast::TypeName*>(type);
    return typeName && typeName->identifier->name == "Int";
}

} // namespace vyn

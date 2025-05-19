#include "vyn/vre/semantic.hpp" // Moved to vre subdirectory
#include "vyn/parser/token.hpp"
#include "vyn/parser/ast.hpp" // Added include for AST nodes
#include "vyn/driver.hpp" // Add this include
#include <stdexcept> // For std::runtime_error

namespace vyn {

// Scope class implementation
Scope::Scope(Scope* parent_scope) : parent(parent_scope) {}

Symbol* Scope::find(const std::string& name) {
    auto it = symbols.find(name);
    if (it != symbols.end()) {
        return it->second;
    }
    if (parent) {
        return parent->find(name);
    }
    return nullptr;
}

void Scope::insert(const std::string& name, Symbol* symbol) {
    symbols[name] = symbol;
}

Scope* Scope::getParent() {
    return parent;
}

// --- SemanticAnalyzer Implementation ---

SemanticAnalyzer::SemanticAnalyzer(Driver& driver) : driver_(driver), currentScope(new Scope()) {}

void SemanticAnalyzer::analyze(ast::Module* root) {
    if (root) {
        root->accept(*this);
    }
}

std::vector<std::string> SemanticAnalyzer::getErrors() {
    return errors;
}

void SemanticAnalyzer::enterScope() {
    currentScope = new Scope(currentScope);
}

void SemanticAnalyzer::exitScope() {
    Scope* oldScope = currentScope;
    currentScope = currentScope->getParent();
    delete oldScope;
}

// --- Visitor Method Implementations ---
void SemanticAnalyzer::visit(ast::UnsafeStatement* node) {
    bool outer_unsafe_state = in_unsafe_block_;
    in_unsafe_block_ = true;
    if (node->block) {
        node->block->accept(*this);
    }
    in_unsafe_block_ = outer_unsafe_state;
}

void SemanticAnalyzer::visit(ast::PointerDerefExpression* node) {
    if (!in_unsafe_block_) {
        errors.push_back("Error at " + node->loc.toString() + ": Pointer dereference 'at()' is only allowed inside an unsafe block.");
    }
    if (node->pointer) {
        node->pointer->accept(*this);
        // TODO: Type checking: pointer should be of a pointer type (e.g., loc<T>, ptr<T>)
        // For now, assume it's valid if in unsafe block
    }
}

void SemanticAnalyzer::visit(ast::AddrOfExpression* node) {
    if (!in_unsafe_block_) {
        errors.push_back("Error at " + node->loc.toString() + ": Taking address 'addr()' is only allowed inside an unsafe block.");
    }
    if (node->getLocation()) { // Assuming getLocation() exists and returns the expression
        node->getLocation()->accept(*this);
        // TODO: Type checking: result is loc<T> where T is type of expression
    }
}

void SemanticAnalyzer::visit(ast::FromIntToLocExpression* node) {
    if (!in_unsafe_block_) {
        errors.push_back("Error at " + node->loc.toString() + ": Casting integer to location 'from()' is only allowed inside an unsafe block.");
    }
    if (node->getAddress()) { // Assuming getAddress() exists and returns the expression
        node->getAddress()->accept(*this);
        // TODO: Type checking: address should be an integer type
        // TODO: Type checking: result is loc<T> where T is specified or inferred (how?)
    }
}

void SemanticAnalyzer::visit(ast::LocationExpression* node) {
    // loc(expr) itself is not inherently unsafe, but its usage (dereferencing) might be.
    // The safety of dereferencing is handled by PointerDerefExpression.
    if (node->getExpression()) { // Assuming getExpression() exists
        node->getExpression()->accept(*this);
        // TODO: Type checking: result is loc<T> where T is type of expression
    }
}

// Stub implementations for other visit methods
void SemanticAnalyzer::visit(ast::ExpressionStatement* node) {
    if (node->expression) {
        node->expression->accept(*this);
    }
}

void SemanticAnalyzer::visit(ast::BlockStatement* node) {
    enterScope();
    for (const auto& stmt : node->body) {
        if (stmt) {
            stmt->accept(*this);
        }
    }
    exitScope();
}

void SemanticAnalyzer::visit(ast::Identifier* node) { /* TODO: Implement (e.g., symbol lookup) */ }
void SemanticAnalyzer::visit(ast::IntegerLiteral* node) { /* TODO: Implement (e.g., type assignment) */ }
void SemanticAnalyzer::visit(ast::FloatLiteral* node) { /* TODO type assignment */ }
void SemanticAnalyzer::visit(ast::StringLiteral* node) { /* TODO type assignment */ }
void SemanticAnalyzer::visit(ast::BooleanLiteral* node) { /* TODO type assignment */ }
void SemanticAnalyzer::visit(ast::NilLiteral* node) { /* TODO type assignment */ }
void SemanticAnalyzer::visit(ast::ObjectLiteral* node) { for(auto& prop : node->properties) if(prop.value)prop.value->accept(*this); /*TODO type check*/ }
void SemanticAnalyzer::visit(ast::ArrayLiteral* node) { for(auto& elem : node->elements) if(elem)elem->accept(*this); /*TODO type check*/ }


void SemanticAnalyzer::visit(ast::UnaryExpression* node) {
    if (node->operand) node->operand->accept(*this);
    // TODO: Type checking for unary op
}

void SemanticAnalyzer::visit(ast::BinaryExpression* node) {
    if (node->left) node->left->accept(*this);
    if (node->right) node->right->accept(*this);
    // TODO: Type checking for binary op
}

void SemanticAnalyzer::visit(ast::CallExpression* node) {
    if (node->callee) node->callee->accept(*this);
    for (const auto& arg : node->arguments) {
        if (arg) arg->accept(*this);
    }
    // TODO: Type checking for function call
}

void SemanticAnalyzer::visit(ast::ArrayElementExpression* node) {
    if (node->object) node->object->accept(*this);
    if (node->index) node->index->accept(*this);
    // TODO: Type checking for array element access
}

void SemanticAnalyzer::visit(ast::MemberExpression* node) {
    if (node->object) node->object->accept(*this);
    // Property might be an identifier or an expression (computed)
    // If it's an expression, it needs to be visited.
    // If it's an identifier, it's part of the member access logic.
    // For now, assume property is handled during type checking of member access.
    // TODO: Type checking for member access
}

void SemanticAnalyzer::visit(ast::AssignmentExpression* node) {
    if (node->left) node->left->accept(*this); // Analyze lvalue (e.g. check if assignable)
    if (node->right) node->right->accept(*this); // Analyze rvalue
    // TODO: Type checking for assignment (left and right types compatible)
}

void SemanticAnalyzer::visit(ast::IfStatement* node) {
    if (node->test) node->test->accept(*this);
    if (node->consequent) node->consequent->accept(*this);
    if (node->alternate) node->alternate->accept(*this);
    // TODO: Type checking for condition (boolean)
}

void SemanticAnalyzer::visit(ast::ReturnStatement* node) {
    if (node->argument) {
        node->argument->accept(*this);
    }
    // TODO: Type checking for return value (matches function return type)
}

void SemanticAnalyzer::visit(ast::WhileStatement* node) {
    if (node->test) node->test->accept(*this);
    if (node->body) node->body->accept(*this);
    // TODO: Type checking for condition (boolean)
}

void SemanticAnalyzer::visit(ast::ForStatement* node) {
    enterScope();
    if (node->init) node->init->accept(*this);
    if (node->test) node->test->accept(*this);
    if (node->update) node->update->accept(*this);
    if (node->body) node->body->accept(*this);
    exitScope();
    // TODO: Type checking for condition (boolean)
}

void SemanticAnalyzer::visit(ast::VariableDeclaration* node) {
    if (node->init) {
        node->init->accept(*this);
        // TODO: Type checking: if typeNode is present, init must match.
        // TODO: If typeNode is absent, infer from init.
        // TODO: Add symbol to scope.
    } else {
        // TODO: If no init, typeNode must be present.
        // TODO: Add symbol to scope.
    }
    if (node->typeNode) {
        node->typeNode->accept(*this);
    }
}

void SemanticAnalyzer::visit(ast::FunctionDeclaration* node) {
    // TODO: Add function symbol to current scope.
    // TODO: Create new scope for function body.
    enterScope();
    for (auto& param : node->params) {
        if (param.typeNode) {
            param.typeNode->accept(*this);
        }
        // TODO: Add param symbol to function scope.
    }
    if (node->returnTypeNode) {
        node->returnTypeNode->accept(*this);
    }
    if (node->body) {
        node->body->accept(*this);
    }
    exitScope();
}

void SemanticAnalyzer::visit(ast::StructDeclaration* node) { /* TODO */ }
void SemanticAnalyzer::visit(ast::EnumDeclaration* node) { /* TODO */ }
void SemanticAnalyzer::visit(ast::TypeAliasDeclaration* node) { /* TODO */ }
void SemanticAnalyzer::visit(ast::ImplDeclaration* node) { /* TODO */ }
void SemanticAnalyzer::visit(ast::GenericParameter* node) { /* TODO */ }
void SemanticAnalyzer::visit(ast::BreakStatement* node) { /* TODO (check if inside loop/switch) */ }
void SemanticAnalyzer::visit(ast::ContinueStatement* node) { /* TODO (check if inside loop) */ }
void SemanticAnalyzer::visit(ast::EmptyStatement* node) { /* No-op */ }
void SemanticAnalyzer::visit(ast::ListComprehension* node) { /* TODO */ }
void SemanticAnalyzer::visit(ast::GenericInstantiationExpression* node) { /* TODO */ }
void SemanticAnalyzer::visit(ast::TryStatement* node) { /* TODO */ }
void SemanticAnalyzer::visit(ast::ImportDeclaration* node) { /* TODO */ }
void SemanticAnalyzer::visit(ast::ClassDeclaration* node) { /* TODO */ }
void SemanticAnalyzer::visit(ast::FieldDeclaration* node) { /* TODO */ }
void SemanticAnalyzer::visit(ast::EnumVariant* node) { /* TODO */ }
void SemanticAnalyzer::visit(ast::TemplateDeclaration* node) { /* TODO */ }
void SemanticAnalyzer::visit(ast::TypeNode* node) { /* TODO */ }
void SemanticAnalyzer::visit(ast::Module* node) {
    enterScope(); // Global scope for the module
    for (const auto& stmt : node->body) {
        if (stmt) {
            stmt->accept(*this);
        }
    }
    exitScope();
}
void SemanticAnalyzer::visit(ast::IfExpression* node) { /* TODO */ }
void SemanticAnalyzer::visit(ast::ConstructionExpression* node) { /* TODO */ }
void SemanticAnalyzer::visit(ast::ArrayInitializationExpression* node) { /* TODO */ }
void SemanticAnalyzer::visit(ast::BorrowExpression* node) { /* TODO */ }


// Helper method implementations (if any were kept or new ones added)
void SemanticAnalyzer::checkBorrow(ast::Node* node, const std::string& owner, bool isMutableBorrow, ast::TypeNode* type) {
    // Placeholder
}

void SemanticAnalyzer::checkLocUnsafe(ast::Node* node) {
    // Placeholder
}

bool SemanticAnalyzer::isRawLocationType(ast::Expression* expr) {
    // Placeholder
    return false;
}

// Ensure all other necessary methods from ast::Visitor are stubbed out if not fully implemented
// For example, if ast.hpp added new visitable nodes, their visit methods need to be added here.

} // namespace vyn

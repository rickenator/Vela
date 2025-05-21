#ifndef VYN_VRE_SEMANTIC_HPP
#define VYN_VRE_SEMANTIC_HPP

#include "../parser/ast.hpp" // Corrected relative include path
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <map> // Added missing include for std::map

namespace vyn {

class Driver; // Forward declaration

// Forward declarations are mostly handled by ast.hpp
// class TypeNode; // This is vyn::ast::TypeNode, already in ast.hpp

namespace ast { // Forward declare Node from ast namespace if not fully included by ast.hpp somehow, though it should be.
    class Node;
    class TypeNode;
} // namespace ast

enum class SymbolType {
    VARIABLE,
    FUNCTION,
    TYPE
};

struct Symbol {
    SymbolType type;
    std::string name;
    std::shared_ptr<ast::TypeNode> dataType; // Changed from ast::TypeNode* to shared_ptr
    bool isMutable;
    // Add other relevant symbol information, e.g., scope, definition location
};

class Scope {
public:
    Scope(Scope* parent = nullptr);
    Symbol* find(const std::string& name);
    void insert(const std::string& name, Symbol* symbol);
    Scope* getParent();
private:
    std::map<std::string, Symbol*> symbols;
    Scope* parent;
};

class SemanticAnalyzer : public ast::Visitor { // Inherit from ast::Visitor
public:
    explicit SemanticAnalyzer(Driver& driver); // Updated constructor
    void analyze(ast::Module* root);
    std::vector<std::string> getErrors();

    // Visitor methods (override from ast::Visitor)
    void visit(ast::Identifier* node) override;
    void visit(ast::IntegerLiteral* node) override;
    void visit(ast::FloatLiteral* node) override;
    void visit(ast::StringLiteral* node) override;
    void visit(ast::BooleanLiteral* node) override;
    void visit(ast::ObjectLiteral* node) override;
    void visit(ast::NilLiteral* node) override;
    void visit(ast::UnaryExpression* node) override;
    void visit(ast::BinaryExpression* node) override;
    void visit(ast::CallExpression* node) override;
    void visit(ast::MemberExpression* node) override;
    void visit(ast::AssignmentExpression* node) override;
    void visit(ast::ArrayLiteral* node) override;
    void visit(ast::BorrowExpression* node) override;
    void visit(ast::PointerDerefExpression* node) override;
    void visit(ast::AddrOfExpression* node) override;
    void visit(ast::FromIntToLocExpression* node) override;
    void visit(ast::ArrayElementExpression* node) override;
    void visit(ast::LocationExpression* node) override;
    void visit(ast::ListComprehension* node) override;
    void visit(ast::IfExpression* node) override;
    void visit(ast::ConstructionExpression* node) override;
    void visit(ast::ArrayInitializationExpression* node) override;
    void visit(ast::GenericInstantiationExpression* node) override;
    void visit(ast::BlockStatement* node) override;
    void visit(ast::ExpressionStatement* node) override;
    void visit(ast::IfStatement* node) override;
    void visit(ast::ForStatement* node) override;
    void visit(ast::WhileStatement* node) override;
    void visit(ast::ReturnStatement* node) override;
    void visit(ast::BreakStatement* node) override;
    void visit(ast::ContinueStatement* node) override;
    void visit(ast::TryStatement* node) override;
    void visit(ast::UnsafeStatement* node) override;
    void visit(ast::EmptyStatement* node) override; // Added for EmptyStatement
    void visit(ast::VariableDeclaration* node) override;
    void visit(ast::FunctionDeclaration* node) override;
    void visit(ast::TypeAliasDeclaration* node) override;
    void visit(ast::ImportDeclaration* node) override;
    void visit(ast::StructDeclaration* node) override;
    void visit(ast::ClassDeclaration* node) override;
    void visit(ast::FieldDeclaration* node) override;
    void visit(ast::ImplDeclaration* node) override;
    void visit(ast::EnumDeclaration* node) override;
    void visit(ast::EnumVariant* node) override;
    void visit(ast::GenericParameter* node) override;
    void visit(ast::TemplateDeclaration* node) override;
    void visit(ast::TypeNode* node) override;
    void visit(ast::Module* node) override;

private:
    Driver& driver_; // Add this line if not already present
    Scope* currentScope;
    std::vector<std::string> errors;
    bool in_unsafe_block_ = false; // Renamed from inUnsafeBlock for consistency

    // Helper methods
    void enterScope();
    void exitScope();
    // Removed private analyzeNode, analyzeAssignment, etc. as visit methods will be used.
    void checkBorrow(ast::Node* node, const std::string& owner, bool isMutableBorrow, ast::TypeNode* type);
    void checkLocUnsafe(ast::Node* node);
    bool isRawLocationType(ast::Expression* expr);
};

} // namespace vyn

#endif // VYN_VRE_SEMANTIC_HPP

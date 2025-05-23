#pragma once
/**
 * @file semantic.hpp
 * @brief This is the primary semantic analysis header for the Vyn compiler.
 *
 * IMPORTANT: This file was previously duplicated as include/vyn/vre/semantic.hpp
 * which was removed during cleanup. Do not create duplicate headers!
 *
 * The implementation for this header is in src/vre/semantic.cpp
 */

#include "vyn/parser/ast.hpp" // Ensure ast.hpp is included
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <unordered_set>

namespace vyn {

class Driver; // Forward declaration

namespace ast {
class Node;
class Module;
class ExpressionStatement;
class BlockStatement;
class IfStatement;
class ReturnStatement;
class WhileStatement;
class ForStatement;
class BreakStatement;
class ContinueStatement;
class ImportStatement; // Should be ImportDeclaration
class ExternStatement;
class TryCatchStatement; // Should be TryStatement
class ThrowStatement;
class MatchStatement;
class YieldStatement;
class YieldReturnStatement;
class AssertStatement;
class EmptyStatement;
class UnsafeStatement;

// Missing forward declarations for expressions
class Identifier;
class IntegerLiteral;
class UnaryExpression;
class BinaryExpression;
class CallExpression;
class ArrayElementExpression;
class MemberExpression;
class AssignmentExpression;
class LogicalExpression; // Add forward declaration
class ConditionalExpression; // Add forward declaration
class SequenceExpression; // Add forward declaration
class ObjectLiteral;
class ArrayLiteral;
class FunctionExpression; // Add forward declaration
class StringLiteral;
class FloatLiteral;
class BooleanLiteral;
class NilLiteral;
class ThisExpression; // Add forward declaration
class SuperExpression; // Add forward declaration
class AwaitExpression; // Add forward declaration
class ListComprehension;
class GenericInstantiationExpression;
class PointerDerefExpression;
class AddrOfExpression;
class FromIntToLocExpression;
class LocationExpression;
class ConstructionExpression; // Add forward declaration

// Missing forward declarations for declarations
class VariableDeclaration;
class FunctionDeclaration;
class StructDeclaration;
class EnumDeclaration;
class TypeAliasDeclaration;
class TraitDeclaration; // Add forward declaration
class ImplDeclaration;
class NamespaceDeclaration; // Add forward declaration

// Missing forward declarations for types
class TypeName; // Add forward declaration
class PointerType; // Add forward declaration
class ArrayType; // Add forward declaration
class FunctionType; // Add forward declaration
class OptionalType; // Add forward declaration
class GenericParameter;

class Visitor; // Forward declaration if it\'s in the ast namespace and defined elsewhere
}

struct BorrowInfo {
    std::string ownerName;
    bool isMutable;
    ast::Node* borrowNode; // The AST node that created the borrow
    // Potentially add ast::TypeNode* of the borrowed type if needed for more complex checks
};

struct SymbolInfo {
    enum class Kind { Variable, Function, Type };
    Kind kind;
    std::string name;
    bool isConst = false;
    ast::TypeNode* type = nullptr;
};

class Scope {
public:
    Scope(Scope* parent_scope = nullptr);
    SymbolInfo* find(const std::string& name);
    SymbolInfo* lookupDirect(const std::string& name); // Added declaration
    void insert(const std::string& name, SymbolInfo* symbol);
    Scope* getParent();

private:
    std::unordered_map<std::string, SymbolInfo*> symbols;
    Scope* parent;
};

class SymbolTable {
public:
    SymbolTable(SymbolTable* parent = nullptr) : parent(parent), isUnsafeBlock(false), isLoop(false) {}
    void add(const SymbolInfo& sym) { table[sym.name] = sym; }
    SymbolInfo* lookup(const std::string& name) {
        auto it = table.find(name);
        if (it != table.end()) return &it->second;
        if (parent) return parent->lookup(name);
        return nullptr;
    }
    SymbolInfo* lookupDirect(const std::string& name); // Added
    SymbolTable* getParent() { return parent; }
    bool isUnsafeBlock;
    bool isLoop;

private:
    std::unordered_map<std::string, SymbolInfo> table;
    SymbolTable* parent;
};

class SemanticAnalyzer : public ast::Visitor {
public:
    explicit SemanticAnalyzer(Driver& driver);
    void analyze(ast::Module* root);
    const std::vector<std::string>& getErrors() const { return errors; }

    // Helper methods
    bool isInLoop();
    bool isInUnsafeBlock();
    bool isIntegerType(ast::TypeNode* type);
    bool isReservedWord(const std::string& name);
    bool isLValue(ast::Expression* expr);
    bool areTypesCompatible(ast::TypeNode* typeA, ast::TypeNode* typeB); // Added

    // Statements
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
    void visit(ast::EmptyStatement* node) override;
    void visit(ast::AssertStatement* node) override;
    void visit(ast::MatchStatement* node) override;
    void visit(ast::YieldStatement* node) override;
    void visit(ast::YieldReturnStatement* node) override;
    void visit(ast::ExternStatement* node) override; // Added
    void visit(ast::ThrowStatement* node) override; // Added

    // Expressions
    void visit(ast::Identifier* node) override;
    void visit(ast::IntegerLiteral* node) override;
    void visit(ast::UnaryExpression* node) override;
    void visit(ast::BinaryExpression* node) override;
    void visit(ast::CallExpression* node) override;
    void visit(ast::ArrayElementExpression* node) override;
    void visit(ast::MemberExpression* node) override;
    void visit(ast::AssignmentExpression* node) override;
    void visit(ast::LogicalExpression* node) override;
    void visit(ast::ConditionalExpression* node) override;
    void visit(ast::SequenceExpression* node) override;
    void visit(ast::ObjectLiteral* node) override;
    void visit(ast::ArrayLiteral* node) override;
    void visit(ast::FunctionExpression* node) override;
    void visit(ast::StringLiteral* node) override;
    void visit(ast::FloatLiteral* node) override;
    void visit(ast::BooleanLiteral* node) override;
    void visit(ast::NilLiteral* node) override;
    void visit(ast::ThisExpression* node) override;
    void visit(ast::SuperExpression* node) override;
    void visit(ast::AwaitExpression* node) override;
    void visit(ast::ListComprehension* node) override;
    void visit(ast::GenericInstantiationExpression* node) override;
    void visit(ast::PointerDerefExpression* node) override;
    void visit(ast::AddrOfExpression* node) override;
    void visit(ast::FromIntToLocExpression* node) override;
    void visit(ast::LocationExpression* node) override;
    void visit(ast::ConstructionExpression* node) override;

    // Declarations
    void visit(ast::Module* node) override; // Added declaration
    void visit(ast::VariableDeclaration* node) override;
    void visit(ast::FunctionDeclaration* node) override;
    void visit(ast::ClassDeclaration* node) override; // Add this line
    void visit(ast::StructDeclaration* node) override;
    void visit(ast::EnumDeclaration* node) override;
    void visit(ast::TypeAliasDeclaration* node) override;
    void visit(ast::ImportDeclaration* node) override;
    void visit(ast::TraitDeclaration* node) override; // Uncommented
    void visit(ast::ImplDeclaration* node) override; // Ensure this is present
    void visit(ast::NamespaceDeclaration* node) override; // Uncommented
    void visit(ast::FieldDeclaration* node) override;
    void visit(ast::EnumVariant* node) override;
    void visit(ast::GenericParameter* node) override;
    void visit(ast::TemplateDeclaration* node) override;


    // Other
    void visit(ast::TypeNode* node) override;

    // Types
    void visit(ast::TypeName* node) override;
    void visit(ast::PointerType* node) override;
    void visit(ast::ArrayType* node) override;
    void visit(ast::FunctionType* node) override;
    void visit(ast::OptionalType* node) override;

private:
    Driver& driver_;
    SymbolTable* currentScope;
    std::vector<std::string> errors;
    std::unordered_map<ast::Node*, ast::TypeNode*> expressionTypes;
    std::vector<SymbolTable*> scopes;
    std::unordered_set<std::string> reservedWords; // Added for isReservedWord

    void enterScope();
    void exitScope();
    void addError(const std::string& message, const ast::Node* node);
    // bool isLValue(ast::Expression* expr); // Duplicate declaration removed
    bool isRawLocationType(ast::Expression* expr);
};

} // namespace vyn

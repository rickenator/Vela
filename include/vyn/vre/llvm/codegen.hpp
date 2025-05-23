#pragma once

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#include <map>
#include <memory>
#include <stack>
#include <string>
#include <vector>

#include "vyn/parser/ast.hpp"
#include "vyn/semantic.hpp" // For SourceLocation, UserTypeInfo
#include "vyn/driver.hpp"   // Added to resolve Driver type

// Forward declarations
namespace llvm {
    class Value;
    class Type;
    class Function;
    class BasicBlock;
    class StructType;
    class AllocaInst;
}

namespace vyn {
    class Driver; // Forward declaration might also work if full include causes issues
}


namespace vyn {

// Helper struct for storing information about user-defined types
struct UserTypeInfo {
    llvm::StructType* llvmType;
    std::map<std::string, unsigned> fieldIndices; // Map field name to index
    bool isStruct; // True if struct, false if class (or could be enum later)
    // Potentially: vtable, parent type info, etc.
};

// Helper struct to manage loop context
struct LoopContext {
    llvm::BasicBlock *loopHeader; // Block for the loop condition check
    llvm::BasicBlock *loopBody;   // Block for the loop body
    llvm::BasicBlock *loopUpdate; // Block for the loop increment/update
    llvm::BasicBlock *loopExit;   // Block after the loop
};


class LLVMCodegen : public ast::Visitor {
public:
    // explicit LLVMCodegen(); // Old constructor
    explicit LLVMCodegen(Driver& driver); // Constructor expects a Driver reference
    virtual ~LLVMCodegen(); // Add virtual destructor declaration

    void generate(vyn::ast::Module* astModule, const std::string& outputFilename); // Add declaration
    void dumpIR() const; // Add declaration
    std::unique_ptr<llvm::Module> releaseModule(); // Add declaration
    llvm::Module* getModule() const { return module.get(); } // Add method to get module pointer without releasing

private:
    Driver& driver_; // Add a Driver reference
    std::unique_ptr<llvm::LLVMContext> context;
    std::unique_ptr<llvm::Module> module;
    std::unique_ptr<llvm::IRBuilder<>> builder;

    // Basic LLVM types
    llvm::Type* voidType;
    llvm::Type* int1Type; // For booleans
    llvm::Type* int8Type;
    llvm::Type* int32Type;
    llvm::Type* int64Type;
    llvm::Type* floatType;
    llvm::Type* doubleType;
    llvm::Type* int8PtrType; // Generic pointer type (char*)
    llvm::StructType* rttiStructType; // For RTTI objects
    llvm::Type* stringType; // Placeholder for Vyn's string type representation

    // Current state
    llvm::Type* m_currentLLVMType = nullptr; // Initialize

    llvm::Value* m_currentLLVMValue = nullptr; // Unified value propagation

    // Scope and symbol management
    llvm::Function* currentFunction = nullptr; // Initialize
    llvm::StructType* currentClassType = nullptr; // Initialize
    LoopContext currentLoopContext;
    std::vector<LoopContext> loopStack;
    std::map<std::string, llvm::AllocaInst*> m_currentFunctionNamedValues;


    // Global and type information
    std::map<std::string, llvm::Value*> namedValues;
    std::map<std::string, UserTypeInfo> userTypeMap;
    std::map<std::string, llvm::Type*> typeParameterMap;
    std::map<vyn::ast::TypeNode*, llvm::Type*> m_typeCache;
    std::map<llvm::Value*, std::shared_ptr<vyn::ast::TypeNode>> valueTypeMap; // Maps LLVM values to AST types
    vyn::ast::TypeNode* m_currentImplTypeNode = nullptr; // Initialize
    vyn::ast::Module* m_currentVynModule = nullptr;
    bool m_isLHSOfAssignment = false;
    bool verbose = false;  // Controls detailed warning output
    bool m_isMemberAccessBase = false; // Controls Identifier behavior for member access

    // Helper methods
    llvm::Type* codegenType(vyn::ast::TypeNode* typeNode); // Converts vyn::TypeNode to llvm::Type
    llvm::Function* getCurrentFunction();
    llvm::BasicBlock* getCurrentBasicBlock();

    // Error and warning reporting
    void logError(const SourceLocation& loc, const std::string& message);
    void logWarning(const SourceLocation& loc, const std::string& message); // Added this line
    llvm::Value* createEntryBlockAlloca(llvm::Function* func, const std::string& varName, llvm::Type* type);
    llvm::AllocaInst* createEntryBlockAlloca(llvm::Type* type, const std::string& name);


    // Type system helpers
    std::string getTypeName(llvm::Type* type);
    llvm::Type* getPointeeTypeInfo(llvm::Value* ptr);

    llvm::Value* tryCast(llvm::Value* value, llvm::Type* targetType, const vyn::SourceLocation& loc);


    // RTTI (Run-Time Type Information)
    llvm::StructType* getOrCreateRTTIStructType();
    llvm::Value* generateRTTIObject(const std::string& typeName, int typeId); // typeId for distinguishing types


    // Loop handling
    void pushLoop(llvm::BasicBlock* header, llvm::BasicBlock* body, llvm::BasicBlock* update, llvm::BasicBlock* exit);
    void popLoop();

    // Struct field access
    int getStructFieldIndex(llvm::StructType* structType, const std::string& fieldName);

public:
    // Visitor methods overridden from vyn::Visitor, corrected to match ast.hpp
    // Literals
    void visit(vyn::ast::Identifier* node) override;
    void visit(vyn::ast::IntegerLiteral* node) override;
    void visit(vyn::ast::FloatLiteral* node) override;
    void visit(vyn::ast::StringLiteral* node) override;
    void visit(vyn::ast::BooleanLiteral* node) override;
    void visit(vyn::ast::ObjectLiteral* node) override;
    void visit(vyn::ast::NilLiteral* node) override;

    // Expressions
    void visit(vyn::ast::UnaryExpression* node) override;
    void visit(vyn::ast::BinaryExpression* node) override;
    void visit(vyn::ast::CallExpression* node) override; // Ensure this is declared
    void visit(vyn::ast::MemberExpression* node) override; // Ensure this is declared
    void visit(vyn::ast::AssignmentExpression* node) override; // Ensure this is declared
    void visit(vyn::ast::ArrayLiteral* node) override;
    void visit(vyn::ast::BorrowExpression* node) override;
    void visit(vyn::ast::PointerDerefExpression* node) override;
    void visit(vyn::ast::AddrOfExpression* node) override;
    void visit(vyn::ast::FromIntToLocExpression* node) override;
    void visit(vyn::ast::ArrayElementExpression* node) override;
    void visit(vyn::ast::LocationExpression* node) override; 
    void visit(vyn::ast::ListComprehension* node) override;
    void visit(vyn::ast::IfExpression* node) override; // Added this line
    void visit(vyn::ast::ConstructionExpression* node) override; // Existing "Added"
    void visit(vyn::ast::ArrayInitializationExpression* node) override; // Existing "Added
    void visit(vyn::ast::LogicalExpression* node) override;
    void visit(vyn::ast::ConditionalExpression* node) override;
    void visit(vyn::ast::SequenceExpression* node) override;
    void visit(vyn::ast::FunctionExpression* node) override;
    void visit(vyn::ast::ThisExpression* node) override;
    void visit(vyn::ast::SuperExpression* node) override;
    void visit(vyn::ast::AwaitExpression* node) override;

    // Add missing visit methods for expressions from ast.hpp if they are defined there
    // and are causing linker errors.
    // Based on linker errors, AssignmentExpression is already declared.
    // CallExpression and MemberExpression need to be checked if they are part of ast::Visitor
    // and implemented in LLVMCodegen.
    // It seems CallExpression and MemberExpression were expected by the vtable.

    // Statements
    void visit(vyn::ast::BlockStatement* node) override;
    void visit(vyn::ast::ExpressionStatement* node) override;
    void visit(vyn::ast::IfStatement* node) override;
    void visit(vyn::ast::WhileStatement* node) override;
    void visit(vyn::ast::ForStatement* node) override;
    void visit(vyn::ast::ReturnStatement* node) override;
    void visit(vyn::ast::BreakStatement* node) override;
    void visit(vyn::ast::ContinueStatement* node) override;
    void visit(vyn::ast::UnsafeStatement* node) override;
    void visit(vyn::ast::EmptyStatement* node) override;
    void visit(vyn::ast::MatchStatement* node) override; // Added this line
    void visit(vyn::ast::TryStatement* node) override; // Added this line

    // Declarations
    void visit(vyn::ast::VariableDeclaration* node) override;
    void visit(vyn::ast::FunctionDeclaration* node) override;
    void visit(vyn::ast::TypeAliasDeclaration* node) override;
    void visit(vyn::ast::ImportDeclaration* node) override;
    void visit(vyn::ast::StructDeclaration* node) override;
    void visit(vyn::ast::ClassDeclaration* node) override;
    void visit(vyn::ast::FieldDeclaration* node) override;
    void visit(vyn::ast::ImplDeclaration* node) override;
    void visit(vyn::ast::EnumDeclaration* node) override;
    void visit(vyn::ast::EnumVariant* node) override;
    void visit(vyn::ast::GenericParameter* node) override;
    void visit(vyn::ast::TemplateDeclaration* node) override;
    void visit(vyn::ast::TraitDeclaration* node) override;
    void visit(vyn::ast::NamespaceDeclaration* node) override;
    void visit(vyn::ast::Module* node) override;
    void visit(vyn::ast::GenericInstantiationExpression* node) override;
    void visit(vyn::ast::ThrowStatement* node) override;
    void visit(vyn::ast::TypeNode* node) override;
    void visit(vyn::ast::AssertStatement* node) override;
    void visit(vyn::ast::TypeName* node) override;
    void visit(vyn::ast::PointerType* node) override;
    void visit(vyn::ast::ArrayType* node) override;
    void visit(vyn::ast::FunctionType* node) override;
    void visit(vyn::ast::OptionalType* node) override;
    void visit(vyn::ast::TupleTypeNode* node) override;

};

} // namespace vyn

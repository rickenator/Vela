#include <vyn/vre/llvm/codegen.hpp>
#include <llvm/IR/Value.h>
#include <vyn/parser/ast.hpp>

namespace vyn {

    void LLVMCodegen::visit(ast::EmptyStatement* node) {
        // TODO: Implement codegen for EmptyStatement
    }
    void LLVMCodegen::visit(ast::ThrowStatement* node) {
        logError(node->loc, "ThrowStatement codegen not implemented.");
        m_currentLLVMValue = nullptr;
    }
    void LLVMCodegen::visit(ast::MatchStatement* node) {
        logError(node->loc, "MatchStatement codegen not implemented.");
        m_currentLLVMValue = nullptr;
    }
    void LLVMCodegen::visit(ast::AssertStatement* node) {
        logError(node->loc, "AssertStatement codegen not implemented.");
        m_currentLLVMValue = nullptr;
    }
    void LLVMCodegen::visit(ast::TraitDeclaration* node) {
        // TODO: Implement codegen for TraitDeclaration
    }
    void LLVMCodegen::visit(ast::NamespaceDeclaration* node) {
        // TODO: Implement codegen for NamespaceDeclaration
    }
    void LLVMCodegen::visit(ast::PointerType* node) {
        logError(node->loc, "PointerType codegen not implemented.");
        m_currentLLVMValue = nullptr;
    }
    void LLVMCodegen::visit(ast::ArrayType* node) {
        logError(node->loc, "ArrayType codegen not implemented.");
        m_currentLLVMValue = nullptr;
    }
    void LLVMCodegen::visit(ast::FunctionType* node) {
        logError(node->loc, "FunctionType codegen not implemented.");
        m_currentLLVMValue = nullptr;
    }
    void LLVMCodegen::visit(ast::OptionalType* node) {
        logError(node->loc, "OptionalType codegen not implemented.");
        m_currentLLVMValue = nullptr;
    }
    void LLVMCodegen::visit(ast::TupleTypeNode* node) {
        logError(node->loc, "TupleTypeNode codegen not implemented.");
        m_currentLLVMValue = nullptr;
    }
    void LLVMCodegen::visit(ast::LogicalExpression* node) {
        // TODO: Implement codegen for LogicalExpression
    }
    void LLVMCodegen::visit(ast::ConditionalExpression* node) {
        // TODO: Implement codegen for ConditionalExpression
    }
    void LLVMCodegen::visit(ast::SequenceExpression* node) {
        // TODO: Implement codegen for SequenceExpression
    }
    void LLVMCodegen::visit(ast::FunctionExpression* node) {
        // TODO: Implement codegen for FunctionExpression
    }
    void LLVMCodegen::visit(ast::ThisExpression* node) {
        // TODO: Implement codegen for ThisExpression
    }
    void LLVMCodegen::visit(ast::SuperExpression* node) {
        // TODO: Implement codegen for SuperExpression
    }
    void LLVMCodegen::visit(ast::AwaitExpression* node) {
        // TODO: Implement codegen for AwaitExpression
    }
    void LLVMCodegen::visit(ast::TypeName* node) {
        // TODO: Implement codegen for TypeName
    }

} // namespace vyn

#include "vyn/vre/llvm/codegen.hpp"
#include "vyn/parser/ast.hpp"
#include <llvm/IR/Value.h>
#include <llvm/IR/Instructions.h> // Required for llvm::AllocaInst

// Dummy implementations for missing visit methods to resolve linker errors.
// These should be properly implemented later.

namespace vyn {

// Removed duplicate BorrowExpression visitor - already implemented in cgen_expr.cpp

void LLVMCodegen::visit(vyn::ast::GenericInstantiationExpression* node) {
    // TODO: Implement
    m_currentLLVMValue = nullptr;
}

// Implementation for the missing createEntryBlockAlloca overload
llvm::AllocaInst* LLVMCodegen::createEntryBlockAlloca(llvm::Type* type, const std::string& name) {
    // TODO: This is a placeholder implementation. It needs to be reviewed and completed.
    // It should likely find the current function's entry block and insert the alloca there.
    if (!currentFunction) {
        // Handle error: No current function to add alloca to.
        // This might involve logging an error and returning nullptr, or throwing an exception.
        return nullptr;
    }
    llvm::IRBuilder<> TmpB(&currentFunction->getEntryBlock(), currentFunction->getEntryBlock().begin());
    return TmpB.CreateAlloca(type, nullptr, name);
}

} // namespace vyn

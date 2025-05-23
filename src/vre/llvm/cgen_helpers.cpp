#include "vyn/vre/llvm/codegen.hpp"
#include "vyn/parser/ast.hpp"
#include <llvm/IR/Instructions.h>
#include <llvm/IR/DerivedTypes.h>

using namespace vyn;

llvm::Type* LLVMCodegen::getPointeeTypeInfo(llvm::Value* ptr) {
    if (!ptr || !ptr->getType()->isPointerTy()) {
        return nullptr;
    }

    // Try to get type information in different ways
    if (auto* allocaInst = llvm::dyn_cast<llvm::AllocaInst>(ptr)) {
        return allocaInst->getAllocatedType();
    }
    
    // Try more approaches to determine type
    if (auto* gepInst = llvm::dyn_cast<llvm::GetElementPtrInst>(ptr)) {
        return gepInst->getSourceElementType();
    }
    
    // Check if the value has a name that might hint at its type
    std::string valueName = ptr->getName().str();
    if (!valueName.empty()) {
        // Look for type-specific naming patterns like "Point_obj"
        size_t objSuffix = valueName.find("_obj");
        if (objSuffix != std::string::npos) {
            std::string typeName = valueName.substr(0, objSuffix);
            // Check if we have this type registered
            auto it = userTypeMap.find(typeName);
            if (it != userTypeMap.end()) {
                return it->second.llvmType;
            }
        }
    }
    
    // For opaque pointers in LLVM IR, getting the pointee type is harder
    // We should fall back to our internal type systems or cached types
    
    return nullptr;
}

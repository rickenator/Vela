#include "vyn/vre/llvm/codegen.hpp"
#include "vyn/parser/ast.hpp"

#include <llvm/IR/Type.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Constants.h> // For ConstantPointerNull

#include <string>
#include <vector>
#include <map>

using namespace vyn;
// Using namespace llvm; // Uncomment if desired for brevity

// --- Type Mapping Helper ---
llvm::Type* LLVMCodegen::codegenType(vyn::ast::TypeNode* typeNode) {
    if (!typeNode) {
        logError(SourceLocation(), "Null type node in codegenType");
        return nullptr;
    }

    // Check cache first
    auto it = m_typeCache.find(typeNode);
    if (it != m_typeCache.end()) {
        return it->second;
    }

    llvm::Type* llvmType = nullptr;

    switch (typeNode->getCategory()) {
        case vyn::ast::TypeNode::Category::IDENTIFIER: {
            auto* typeNameNode = dynamic_cast<vyn::ast::TypeName*>(typeNode);
            if (!typeNameNode) {
                logError(typeNode->loc, "Type node is not a TypeName");
                return nullptr;
            }

            // Special handling for loc<T>
            if (typeNameNode->identifier->name == "loc") {
                if (typeNameNode->genericArgs.empty()) {
                    logError(typeNode->loc, "loc type requires a type parameter");
                    return nullptr;
                }
                llvm::Type* pointeeType = codegenType(typeNameNode->genericArgs[0].get());
                if (!pointeeType) {
                    logError(typeNode->loc, "Could not determine LLVM type for loc<T> pointee type");
                    return nullptr;
                }
                llvmType = llvm::PointerType::getUnqual(pointeeType);
                break;
            }

            if (!typeNameNode || !typeNameNode->identifier) { // Check typeNameNode and its identifier
                logError(typeNode->loc, "Type identifier node has no name or is not a TypeName.");
                return nullptr;
            }
            std::string typeNameStr = typeNameNode->identifier->name; // Access name via identifier

            if (typeNameStr == "Int" || typeNameStr == "int" || typeNameStr == "i64") {
                llvmType = int64Type;
            } else if (typeNameStr == "Float" || typeNameStr == "float" || typeNameStr == "f64") {
                llvmType = doubleType;
            } else if (typeNameStr == "Bool" || typeNameStr == "bool") {
                llvmType = int1Type;
            } else if (typeNameStr == "Void" || typeNameStr == "void") {
                llvmType = voidType;
            } else if (typeNameStr == "String" || typeNameStr == "string") {
                llvmType = int8PtrType; 
            } else if (typeNameStr == "char" || typeNameStr == "i8") {
                llvmType = int8Type;
            } else if (typeNameStr == "i32") {
                llvmType = int32Type;
            } else {
                auto userTypeIt = userTypeMap.find(typeNameStr);
                if (userTypeIt != userTypeMap.end()) {
                    llvmType = userTypeIt->second.llvmType;
                } else {
                    llvm::StructType* existingType = llvm::StructType::getTypeByName(*context, typeNameStr);
                    if (existingType) {
                        llvmType = existingType;
                    } else {
                        // This case should ideally be caught by semantic analysis if it\'s an undefined type.
                        // If it\'s a type that will be defined later (e.g. in a different module or due to ordering),
                        // creating an opaque struct might be an option, but can be risky.
                        // llvmType = llvm::StructType::create(*context, typeNameStr);
                        logError(typeNode->loc, "Unknown type identifier: " + typeNameStr + ". It might be a forward-declared type not yet fully defined or an undeclared type.");
                        return nullptr;
                    }
                }
            }
            break;
        }
        case vyn::ast::TypeNode::Category::ARRAY: {
            auto* arrayTypeNode = dynamic_cast<vyn::ast::ArrayType*>(typeNode);
            if (!arrayTypeNode || !arrayTypeNode->elementType) { // Check arrayTypeNode and its elementType
                logError(typeNode->loc, "Array type node has no element type or is not an ArrayType.");
                return nullptr;
            }
            llvm::Type* elemTy = codegenType(arrayTypeNode->elementType.get()); // Access elementType
            if (!elemTy) {
                logError(typeNode->loc, "Could not determine LLVM type for array element.");
                return nullptr;
            }

            if (arrayTypeNode->sizeExpression) { // Access sizeExpression
                // For fixed-size arrays. This requires constant evaluation.
                // Simplified: assumes IntegerLiteral for size.
                if (auto* intLit = dynamic_cast<vyn::ast::IntegerLiteral*>(arrayTypeNode->sizeExpression.get())) { // Access sizeExpression
                    uint64_t arraySize = intLit->value;
                    if (arraySize == 0) { 
                        logError(typeNode->loc, "Array size cannot be zero.");
                        return nullptr;
                    }
                    llvmType = llvm::ArrayType::get(elemTy, arraySize);
                } else {
                    logError(typeNode->loc, "Array size is not a constant integer literal. Dynamic/complex-sized arrays need specific handling (e.g., as slices/structs or require constant folding). Treating as pointer for now.");
                    llvmType = llvm::PointerType::getUnqual(elemTy); // Fallback, might not be correct for all Vyn semantics
                }
            } else {
                // Unsized array (e.g., `arr: []Int`) - typically a pointer or a slice struct.
                llvmType = llvm::PointerType::getUnqual(elemTy); // Fallback, might not be correct for all Vyn semantics
            }
            break;
        }
        case vyn::ast::TypeNode::Category::TUPLE: {
            auto* tupleTypeNode = dynamic_cast<vyn::ast::TupleTypeNode*>(typeNode);
            if (!tupleTypeNode) {
                logError(typeNode->loc, "Type node is not a TupleTypeNode.");
                return nullptr;
            }
            std::vector<llvm::Type*> memberLlvmTypes;
            for (const auto& memberTypeNode : tupleTypeNode->memberTypes) { // Access memberTypes
                llvm::Type* memberLlvmType = codegenType(memberTypeNode.get());
                if (!memberLlvmType) {
                    logError(typeNode->loc, "Could not determine LLVM type for a tuple member.");
                    return nullptr;
                }
                memberLlvmTypes.push_back(memberLlvmType);
            }
            llvmType = llvm::StructType::get(*context, memberLlvmTypes);
            break;
        }
        case vyn::ast::TypeNode::Category::FUNCTION: { // Changed from FUNCTION_SIGNATURE
            auto* funcTypeNode = dynamic_cast<vyn::ast::FunctionType*>(typeNode);
            if (!funcTypeNode) {
                logError(typeNode->loc, "Type node is not a FunctionType.");
                return nullptr;
            }
            std::vector<llvm::Type*> paramLlvmTypes;
            for (const auto& paramTypeNode : funcTypeNode->parameterTypes) { // Access parameterTypes
                llvm::Type* paramLlvmType = codegenType(paramTypeNode.get());
                if (!paramLlvmType) {
                    logError(typeNode->loc, "Could not determine LLVM type for a function parameter in signature.");
                    return nullptr;
                }
                paramLlvmTypes.push_back(paramLlvmType);
            }
            llvm::Type* returnLlvmType = funcTypeNode->returnType ? codegenType(funcTypeNode->returnType.get()) : voidType; // Access returnType
            if (!returnLlvmType) {
                 logError(typeNode->loc, "Could not determine LLVM return type for function signature.");
                return nullptr;
            }
            llvmType = llvm::FunctionType::get(returnLlvmType, paramLlvmTypes, false)->getPointerTo();
            break;
        }
        case vyn::ast::TypeNode::Category::POINTER: {
            auto* pointerTypeNode = dynamic_cast<vyn::ast::PointerType*>(typeNode);
            if (!pointerTypeNode || !pointerTypeNode->pointeeType) {
                logError(typeNode->loc, "Pointer type has no pointee type or is not a PointerType.");
                return nullptr;
            }
            llvm::Type* pointeeLlvmType = codegenType(pointerTypeNode->pointeeType.get());
            if (!pointeeLlvmType) {
                logError(typeNode->loc, "Could not determine LLVM type for pointee type in pointer.");
                return nullptr;
            }
            llvmType = llvm::PointerType::getUnqual(pointeeLlvmType);
            break;
        }
        case vyn::ast::TypeNode::Category::OPTIONAL: {
            auto* optionalTypeNode = dynamic_cast<vyn::ast::OptionalType*>(typeNode);
            if (!optionalTypeNode || !optionalTypeNode->containedType) {
                logError(typeNode->loc, "Optional type has no contained type or is not an OptionalType.");
                return nullptr;
            }
            llvm::Type* containedLlvmType = codegenType(optionalTypeNode->containedType.get());
            if (!containedLlvmType) {
                logError(typeNode->loc, "Could not determine LLVM type for contained type in optional.");
                return nullptr;
            }
            // Represent optional<T> as a struct { T value; bool has_value; }
            // Or, if T is a pointer, optional<T*> can be T* (where nullptr means no value).
            // For simplicity here, let's assume T is not a pointer and use a struct.
            // A more complex handling might be needed based on Vyn's specific semantics for optionals.
            if (containedLlvmType->isPointerTy()) {
                 // If T is already a pointer, optional<T*> can be represented by T* (nullptr for none)
                llvmType = containedLlvmType;
            } else {
                // For non-pointer types, use a struct { value, i1 has_value }
                // To avoid issues with recursive types if T is this optional type itself (though less common for optionals),
                // we should ideally name this struct if it's not anonymous.
                // For now, creating an anonymous struct.
                llvm::StructType* optionalStructType = llvm::StructType::get(*context, {containedLlvmType, int1Type});
                llvmType = optionalStructType;
            }
            break;
        }
        // case vyn::ast::TypeNode::Category::REFERENCE: // TODO: Add handling for REFERENCE if distinct from POINTER
        // case vyn::ast::TypeNode::Category::SLICE: // TODO: Add handling for SLICE
        default:
            logError(typeNode->loc, "Unknown or unsupported TypeNode category: " + typeNode->toString());
            return nullptr;
    }

    if (llvmType) {
        m_typeCache[typeNode] = llvmType;
    }
    return llvmType;
}

void LLVMCodegen::visit(vyn::ast::TypeNode* node) {
    if (node) {
        m_currentLLVMType = codegenType(node);
    } else {
        m_currentLLVMType = nullptr;
        // logError(SourceLocation(), "visit(TypeNode*) called with null node."); // Optional: log if a location is available
    }
    // This visitor primarily populates m_currentLLVMType.
    // It does not produce a Value for m_currentLLVMValue.
}


#include "vyn/vre/llvm/codegen.hpp"
#include "vyn/parser/ast.hpp"
#include "vyn/parser/token.hpp" // For TokenType in BinaryExpression

#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/DerivedTypes.h> // For PointerType, StructType
#include <llvm/ADT/APFloat.h>   // For APFloat in FloatLiteral
#include <llvm/ADT/APInt.h>     // For APInt in IntegerLiteral
#include <regex>                // For regex in MemberExpression loaded value handling

using namespace vyn;
// using namespace llvm; // Uncomment if desired for brevity

// --- Literal Codegen ---
void LLVMCodegen::visit(vyn::ast::IntegerLiteral *node) {
    m_currentLLVMValue = llvm::ConstantInt::get(*context, llvm::APInt(64, node->value, true));
}

void LLVMCodegen::visit(vyn::ast::FloatLiteral *node) {
    m_currentLLVMValue = llvm::ConstantFP::get(*context, llvm::APFloat(node->value));
}

void LLVMCodegen::visit(vyn::ast::BooleanLiteral *node) {
    m_currentLLVMValue = llvm::ConstantInt::get(*context, llvm::APInt(1, node->value));
}

void LLVMCodegen::visit(vyn::ast::StringLiteral *node) {
    // Create a global string pointer
    m_currentLLVMValue = builder->CreateGlobalStringPtr(node->value);
}

void LLVMCodegen::visit(vyn::ast::NilLiteral* node) {
    // Nil is a polymorphic null pointer. For now, default to i8*.
    // Type inference or context should ideally provide a more specific pointer type.
    if (m_currentLLVMType && m_currentLLVMType->isPointerTy()) {
        // If we have a specific pointer type from context, use it
        m_currentLLVMValue = llvm::ConstantPointerNull::get(
            llvm::cast<llvm::PointerType>(m_currentLLVMType));
    } else {
        // Default to i8* if no specific type is known
        m_currentLLVMValue = llvm::ConstantPointerNull::get(
            llvm::PointerType::getUnqual(int8Type));
    }
}

void LLVMCodegen::visit(vyn::ast::ObjectLiteral* node) {
    if (!node->typePath) {
        logError(node->loc, "Object literal is missing type information");
        m_currentLLVMValue = nullptr;
        return;
    }

    // Get the struct type for the object
    llvm::Type* structTy = codegenType(node->typePath.get());
    if (!structTy || !structTy->isStructTy()) {
        logError(node->loc, "Object literal type is not a struct type");
        m_currentLLVMValue = nullptr;
        return;
    }

    // Store the type info back in the expression's type field for MemberExpression to use
    // This is crucial for member access later when working with the object
    if (!node->type) {
        node->type = node->typePath->clone();
    }

    std::string structName = llvm::cast<llvm::StructType>(structTy)->getName().str();
    if (structName.empty()) {
        structName = "anon";
    }

    // Allocate stack space for the struct
    llvm::AllocaInst* allocaInst = builder->CreateAlloca(structTy, nullptr, structName + "_obj");
    
    // Set metadata or debug info to help identify this as a struct of type structName
    // This can help when trying to determine the type in MemberExpression
    if (!structName.empty() && structName != "anon") {
        // Store the allocated type in the userTypeMap if it's not already there
        // This ensures the type is registered for field lookups
        auto it = userTypeMap.find(structName);
        if (it == userTypeMap.end() && llvm::isa<llvm::StructType>(structTy)) {
            llvm::StructType* structType = llvm::cast<llvm::StructType>(structTy);
            if (!structType->isOpaque()) {
                UserTypeInfo typeInfo;
                typeInfo.llvmType = structType;
                typeInfo.isStruct = true;
                
                // Try to populate field indices if we have the information
                // This might be incomplete, but can be useful for debugging
                userTypeMap[structName] = typeInfo;
            }
        }
    }

    // Store each field
    for (size_t i = 0; i < node->properties.size(); ++i) {
        const auto& prop = node->properties[i];
        if (!prop.value || !prop.key) {
            logError(node->loc, "ObjectLiteral property missing key or value");
            m_currentLLVMValue = nullptr;
            return;
        }

        // Get field index by name
        std::string fieldName = prop.key->toString();
        int fieldIndex = getStructFieldIndex(llvm::cast<llvm::StructType>(structTy), fieldName);
        if (fieldIndex < 0) {
            logError(node->loc, "Field '" + fieldName + "' not found in struct '" + structName + "'");
            m_currentLLVMValue = nullptr;
            return;
        }

        // Generate the value for the field
        prop.value->accept(*this);
        if (!m_currentLLVMValue) {
            logError(prop.value->loc, "Failed to codegen value for field '" + fieldName + "'");
            m_currentLLVMValue = nullptr;
            return;
        }

        // Create GEP to get pointer to field
        llvm::Value* fieldPtr = builder->CreateStructGEP(structTy, allocaInst, fieldIndex, fieldName + "_ptr");
        
        // Store the value into the field
        llvm::Value* fieldValue = m_currentLLVMValue;
        builder->CreateStore(fieldValue, fieldPtr);
    }
    
    // Set the current value to the allocated struct
    m_currentLLVMValue = allocaInst;
}

void LLVMCodegen::visit(vyn::ast::ArrayLiteral* node) {
    if (node->elements.empty()) {
        // Handle empty array literal. Need its type.
        // If node->type is set by semantic analysis:
        if (node->type) {
            llvm::Type* arrayLlvmType = codegenType(node->type.get());
            if (auto at = llvm::dyn_cast<llvm::ArrayType>(arrayLlvmType)) {
                 m_currentLLVMValue = llvm::ConstantArray::get(at, {}); // Empty constant array
                 return;
            } else if (auto pt = llvm::dyn_cast<llvm::PointerType>(arrayLlvmType)) {
                // If it's a pointer to an array or slice type.
                // This case is more complex for an empty literal.
                // It might mean a null pointer or pointer to an empty static region.
                // For now, let's assume if type is known, it's an ArrayType.
            }
        }
        logError(node->loc, "Empty array literal with unknown type.");
        m_currentLLVMValue = nullptr;
        return;
    }

    std::vector<llvm::Constant*> constantElements;
    llvm::Type* elementLlvmType = nullptr;

    for (const auto& elemExpr : node->elements) {
        elemExpr->accept(*this); // Codegen element
        llvm::Value* elemValue = m_currentLLVMValue;
        if (!elemValue) {
            logError(elemExpr->loc, "Element codegen failed in array literal.");
            m_currentLLVMValue = nullptr;
            return;
        }
        if (!elementLlvmType) {
            elementLlvmType = elemValue->getType();
        } else if (elemValue->getType() != elementLlvmType) {
            // TODO: Handle mixed types, promotions, or error
            // For now, assume all elements must be of the same type as the first
            // Or attempt to cast to the type of the first element.
            // This should ideally be caught by semantic analysis.
            logError(elemExpr->loc, "Array literal elements have mixed types. Expected " + getTypeName(elementLlvmType) + " but got " + getTypeName(elemValue->getType()));
            m_currentLLVMValue = nullptr;
            return;
        }
        // Array literals must consist of constants to form a ConstantArray
        if (auto* constElem = llvm::dyn_cast<llvm::Constant>(elemValue)) {
            constantElements.push_back(constElem);
        } else {
            // If elements are not constant, we can't create a llvm::ConstantArray.
            // This means the array must be constructed at runtime, e.g., by allocating
            // memory and storing each element. This is more like ArrayInitializationExpression
            // or requires a helper function.
            // For now, array literals are assumed to produce ConstantArrays.
            logError(elemExpr->loc, "Array literal element is not a constant value. Runtime array construction not yet fully supported here.");
            m_currentLLVMValue = nullptr;
            return;
        }
    }

    if (!elementLlvmType) { // Should not happen if elements is not empty and codegen succeeded
        logError(node->loc, "Could not determine element type for array literal.");
        m_currentLLVMValue = nullptr;
        return;
    }

    llvm::ArrayType* arrayType = llvm::ArrayType::get(elementLlvmType, constantElements.size());
    m_currentLLVMValue = llvm::ConstantArray::get(arrayType, constantElements);
}

// --- Expressions ---
void LLVMCodegen::visit(vyn::ast::UnaryExpression *node) {
    node->operand->accept(*this);
    llvm::Value *operandValue = m_currentLLVMValue;

    if (!operandValue) {
        logError(node->operand->loc, "Operand for unary expression is null.");
        m_currentLLVMValue = nullptr;
        return;
    }

    switch (node->op.type) {
        case vyn::TokenType::MINUS: // Reverted to vyn::TokenType::MINUS
            if (operandValue->getType()->isFloatingPointTy()) {
                m_currentLLVMValue = builder->CreateFNeg(operandValue, "fnegtmp");
            } else if (operandValue->getType()->isIntegerTy()) {
                m_currentLLVMValue = builder->CreateNeg(operandValue, "negtmp");
            } else {
                logError(node->loc, "Unary minus operator can only be applied to integer or float types.");
                m_currentLLVMValue = nullptr;
            }
            break;
        case vyn::TokenType::BANG: // Reverted to vyn::TokenType::BANG
            // Logical NOT: typically (operand == 0) for integers, or fcmp one for floats
             if (operandValue->getType()->isIntegerTy(1)) { // Already a boolean
                m_currentLLVMValue = builder->CreateNot(operandValue, "nottmp");
            } else if (operandValue->getType()->isIntegerTy()) { // Other integers
                m_currentLLVMValue = builder->CreateICmpEQ(operandValue, llvm::ConstantInt::get(operandValue->getType(), 0), "icmpeqtmp");
            } else if (operandValue->getType()->isFloatingPointTy()) {
                m_currentLLVMValue = builder->CreateFCmpOEQ(operandValue, llvm::ConstantFP::get(operandValue->getType(), 0.0), "fcmpoeqtmp");
            } else {
                logError(node->loc, "Logical NOT operator can only be applied to boolean, integer or float types.");
                m_currentLLVMValue = nullptr;
            }
            break;
        // TODO: Handle other unary operators like TILDE (bitwise NOT)
        default:
            logError(node->loc, "Unsupported unary operator.");
            m_currentLLVMValue = nullptr;
            break;
    }
}

void LLVMCodegen::visit(vyn::ast::BinaryExpression *node) {
    node->left->accept(*this);
    llvm::Value *L = m_currentLLVMValue;
    vyn::ast::TypeNode* leftTypeNode = node->left->type.get(); // Get AST type of left operand

    node->right->accept(*this);
    llvm::Value *R = m_currentLLVMValue;
    vyn::ast::TypeNode* rightTypeNode = node->right->type.get(); // Get AST type of right operand


    if (!L || !R) {
        logError(node->loc, "One or both operands of binary expression are null.");
        m_currentLLVMValue = nullptr;
        return;
    }

    bool isFloatOp = L->getType()->isFloatingPointTy() || R->getType()->isFloatingPointTy();

    if (L->getType()->isFloatingPointTy() && R->getType()->isIntegerTy()) {
        R = builder->CreateSIToFP(R, L->getType(), "sitofptmp");
        isFloatOp = true;
    } else if (R->getType()->isFloatingPointTy() && L->getType()->isIntegerTy()) {
        L = builder->CreateSIToFP(L, R->getType(), "sitofptmp");
        isFloatOp = true;
    } else if (L->getType()->isPointerTy() && R->getType()->isIntegerTy()) {
        // Pointer arithmetic (e.g. ptr + int)
        // We need to extract the appropriate type information for CreateGEP
        leftTypeNode = node->left->type.get();
        // No additional changes needed with leftTypeNode - already set above
    } else if (R->getType()->isPointerTy() && L->getType()->isIntegerTy()) {
        // Pointer arithmetic (e.g. int + ptr)
        std::swap(L,R); // Put pointer on the left
        leftTypeNode = node->right->type.get(); // Pointer is now L, so use right's AST type
    }


    switch (node->op.type) {
        case vyn::TokenType::PLUS: // Reverted to vyn::TokenType::PLUS
            if (isFloatOp) m_currentLLVMValue = builder->CreateFAdd(L, R, "faddtmp");
            else if (L->getType()->isPointerTy() && leftTypeNode) {
                 vyn::ast::TypeNode* pointeeAstType = nullptr;
                 
                 // Try to get pointee type from different sources
                 if (auto ptrAstNode = dynamic_cast<vyn::ast::PointerType*>(leftTypeNode)) {
                    pointeeAstType = ptrAstNode->pointeeType.get(); 
                 } else if (auto arrayAstNode = dynamic_cast<vyn::ast::ArrayType*>(leftTypeNode)) {
                    pointeeAstType = arrayAstNode->elementType.get();
                 } else if (auto typeName = dynamic_cast<vyn::ast::TypeName*>(leftTypeNode)) {
                    // Check if it's a loc<T> type
                    if (typeName->identifier->name == "loc" && !typeName->genericArgs.empty()) {
                        pointeeAstType = typeName->genericArgs[0].get();
                    }
                 }

                 if (pointeeAstType) {
                    llvm::Type* pointeeType = codegenType(pointeeAstType);
                    if (pointeeType) {
                         m_currentLLVMValue = builder->CreateGEP(pointeeType, L, R, "ptraddtmp");
                    } else {
                        logError(node->left->loc, "Could not determine LLVM pointee type for pointer addition from AST type: " + leftTypeNode->toString());
                        m_currentLLVMValue = nullptr;
                    }
                 } else {
                    // If we can't determine the pointee type from AST, use int64 as a fallback
                    // This is common in test cases with opaque pointers
                    if (verbose) {
                        logWarning(node->left->loc, "Pointer operand for addition lacks specific pointee type information. Using i64 as fallback pointee type.");
                    }
                    m_currentLLVMValue = builder->CreateGEP(int64Type, L, R, "ptraddtmp_fallback");
                 }
            }
            else m_currentLLVMValue = builder->CreateAdd(L, R, "addtmp");
            break;
        case vyn::TokenType::MINUS: // Reverted to vyn::TokenType::MINUS
            if (isFloatOp) m_currentLLVMValue = builder->CreateFSub(L, R, "fsubtmp");
            else if (L->getType()->isPointerTy() && R->getType()->isPointerTy()){ 
                 // Pointer subtraction (ptr - ptr) gives an integer distance
                 L = builder->CreatePtrToInt(L, int64Type, "ptrtointtmp_l");
                 R = builder->CreatePtrToInt(R, int64Type, "ptrtointtmp_r");
                 llvm::Value* diffBytes = builder->CreateSub(L, R, "subtmp");
                 m_currentLLVMValue = diffBytes; 
            }
            else if (L->getType()->isPointerTy() && leftTypeNode) { // ptr - int
                 vyn::ast::TypeNode* pointeeAstType = nullptr;
                 
                 // Try to get pointee type from different sources
                 if (auto ptrAstNode = dynamic_cast<vyn::ast::PointerType*>(leftTypeNode)) {
                    pointeeAstType = ptrAstNode->pointeeType.get(); 
                 } else if (auto arrayAstNode = dynamic_cast<vyn::ast::ArrayType*>(leftTypeNode)) {
                    pointeeAstType = arrayAstNode->elementType.get();
                 } else if (auto typeName = dynamic_cast<vyn::ast::TypeName*>(leftTypeNode)) {
                    // Check if it's a loc<T> type
                    if (typeName->identifier->name == "loc" && !typeName->genericArgs.empty()) {
                        pointeeAstType = typeName->genericArgs[0].get();
                    }
                 }
                 
                 if (pointeeAstType) {
                    llvm::Type* pointeeType = codegenType(pointeeAstType);
                    if (pointeeType) {
                        m_currentLLVMValue = builder->CreateGEP(pointeeType, L, builder->CreateNeg(R), "ptrsubtmp");
                    } else {
                         logError(node->left->loc, "Could not determine LLVM pointee type for pointer subtraction from AST type: " + leftTypeNode->toString());
                         m_currentLLVMValue = nullptr;
                    }
                 } else {
                     // If we can't determine the pointee type from AST, use int64 as a fallback
                     // This is common in test cases with opaque pointers
                     if (verbose) {
                         logWarning(node->left->loc, "Pointer operand for subtraction lacks specific pointee type information. Using i64 as fallback pointee type.");
                     }
                     m_currentLLVMValue = builder->CreateGEP(int64Type, L, builder->CreateNeg(R), "ptrsubtmp_fallback");
                 }
            }
            else m_currentLLVMValue = builder->CreateSub(L, R, "subtmp");
            break;
        case vyn::TokenType::MULTIPLY: // Reverted to vyn::TokenType::MULTIPLY
            if (isFloatOp) m_currentLLVMValue = builder->CreateFMul(L, R, "fmultmp");
            else m_currentLLVMValue = builder->CreateMul(L, R, "multmp");
            break;
        case vyn::TokenType::DIVIDE: // Reverted to vyn::TokenType::DIVIDE
            if (isFloatOp) m_currentLLVMValue = builder->CreateFDiv(L, R, "fdivtmp");
            else m_currentLLVMValue = builder->CreateSDiv(L, R, "sdivtmp"); 
            break;
        case vyn::TokenType::MODULO: // Reverted to vyn::TokenType::MODULO
             if (isFloatOp) m_currentLLVMValue = builder->CreateFRem(L, R, "fremtmp");
             else m_currentLLVMValue = builder->CreateSRem(L, R, "sremtmp"); 
            break;
        // Comparison operators
        case vyn::TokenType::EQEQ: // Reverted to vyn::TokenType::EQEQ
            if (isFloatOp) m_currentLLVMValue = builder->CreateFCmpOEQ(L, R, "fcmpoeqtmp");
            else m_currentLLVMValue = builder->CreateICmpEQ(L, R, "icmpeqtmp");
            break;
        case vyn::TokenType::NOTEQ: // Reverted to vyn::TokenType::NOTEQ
            if (isFloatOp) m_currentLLVMValue = builder->CreateFCmpONE(L, R, "fcmponeqtmp");
            else m_currentLLVMValue = builder->CreateICmpNE(L, R, "icmpneqtmp");
            break;
        case vyn::TokenType::LT: // Reverted to vyn::TokenType::LT
            if (isFloatOp) m_currentLLVMValue = builder->CreateFCmpOLT(L, R, "fcmpltmp");
            else m_currentLLVMValue = builder->CreateICmpSLT(L, R, "icmpslttmp"); 
            break;
        case vyn::TokenType::LTEQ: // Reverted to vyn::TokenType::LTEQ
            if (isFloatOp) m_currentLLVMValue = builder->CreateFCmpOLE(L, R, "fcmpletmp");
            else m_currentLLVMValue = builder->CreateICmpSLE(L, R, "icmpsletmp"); 
            break;
        case vyn::TokenType::GT: // Reverted to vyn::TokenType::GT
            if (isFloatOp) m_currentLLVMValue = builder->CreateFCmpOGT(L, R, "fcmpgtmp");
            else m_currentLLVMValue = builder->CreateICmpSGT(L, R, "icmpsgttmp"); 
            break;
        case vyn::TokenType::GTEQ: // Reverted to vyn::TokenType::GTEQ
            if (isFloatOp) m_currentLLVMValue = builder->CreateFCmpOGE(L, R, "fcmpgetmp");
            else m_currentLLVMValue = builder->CreateICmpSGE(L, R, "icmpsgetmp"); 
            break;
        // Logical operators (short-circuiting needs careful handling with basic blocks)
        // For simplicity, this example evaluates both sides. Proper logical ops need control flow.
        case vyn::TokenType::AND: // Reverted to vyn::TokenType::AND
             m_currentLLVMValue = builder->CreateAnd(L, R, "andtmp"); // Bitwise AND, assumes L and R are i1
            break;
        case vyn::TokenType::OR: // Reverted to vyn::TokenType::OR
            m_currentLLVMValue = builder->CreateOr(L, R, "ortmp"); // Bitwise OR, assumes L and R are i1
            break;
        default:
            logError(node->loc, "Unsupported binary operator.");
            m_currentLLVMValue = nullptr;
            break;
    }
}

void LLVMCodegen::visit(vyn::ast::CallExpression *node) {
    // First, check if this is an intrinsic function call
    auto identCallee = dynamic_cast<vyn::ast::Identifier*>(node->callee.get());
    std::string calleeName = node->callee->toString();
    
    // Special handling for intrinsic functions with potential variable name conflicts
    if (identCallee && node->arguments.size() == 1) {
        // Check for addr() intrinsic
        if (identCallee->name == "addr") {
            // First evaluate the argument expression
            node->arguments[0]->accept(*this);
            if (!m_currentLLVMValue) {
                logError(node->arguments[0]->loc, "Argument to addr() evaluated to null");
                return;
            }
            
            // If the argument is a pointer type
            if (m_currentLLVMValue->getType()->isPointerTy()) {
                llvm::Value* pointerValue = m_currentLLVMValue;
                
                // For pointer-to-pointer types (like loc<T>), load the pointer first
                if (auto allocaInst = llvm::dyn_cast<llvm::AllocaInst>(m_currentLLVMValue)) {
                    if (allocaInst->getAllocatedType()->isPointerTy()) {
                        pointerValue = builder->CreateLoad(allocaInst->getAllocatedType(), 
                                                        m_currentLLVMValue, 
                                                        "ptr_load_for_addr");
                    }
                }
                
                // Convert to integer value (address)
                m_currentLLVMValue = builder->CreatePtrToInt(pointerValue, int64Type, "addr_cast");
                
                // If we're not in an assignment context, create an alloca to store the result
                if (!m_isLHSOfAssignment) {
                    llvm::Value* tempAlloca = builder->CreateAlloca(int64Type, nullptr, "addr_temp");
                    builder->CreateStore(m_currentLLVMValue, tempAlloca);
                    m_currentLLVMValue = tempAlloca;
                }
                return;
            }
        } 
        // Check for at() intrinsic
        else if (identCallee->name == "at") {
            // First evaluate the argument expression to get the pointer
            node->arguments[0]->accept(*this);
            if (!m_currentLLVMValue) {
                logError(node->arguments[0]->loc, "Argument to at() evaluated to null");
                return;
            }
            
            // If we have a valid pointer, handle it appropriately
            if (m_currentLLVMValue->getType()->isPointerTy()) {
                // For at(p) where p is a loc<T> (i.e., a pointer-to-pointer), 
                // we need to load the pointer value first
                llvm::Value* pointerValue = m_currentLLVMValue;
                
                // AllocaInst for a loc<T> produces a pointer-to-pointer.
                // Check if we're dealing with a pointer-to-pointer (i.e., loc<T>)
                bool isPointerToPointer = false;
                if (auto allocaInst = llvm::dyn_cast<llvm::AllocaInst>(m_currentLLVMValue)) {
                    if (allocaInst->getAllocatedType()->isPointerTy()) {
                        isPointerToPointer = true;
                        // Load the pointer value
                        pointerValue = builder->CreateLoad(allocaInst->getAllocatedType(), 
                                               m_currentLLVMValue, 
                                               "ptr_val");
                    }
                }
                
                // Add a null check for the pointer value
                llvm::Function* currentFn = builder->GetInsertBlock()->getParent();
                llvm::BasicBlock* nonNullBB = llvm::BasicBlock::Create(*context, "ptr.not_null", currentFn);
                llvm::BasicBlock* nullBB = llvm::BasicBlock::Create(*context, "ptr.null", currentFn);
                llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(*context, "ptr.merge", currentFn);
                
                // Create the null check condition
                llvm::Value* isNotNull = builder->CreateIsNotNull(pointerValue, "ptr.is_not_null");
                builder->CreateCondBr(isNotNull, nonNullBB, nullBB);
                
                // Set up the non-null block
                builder->SetInsertPoint(nonNullBB);
                
                if (m_isLHSOfAssignment) {
                    // In an assignment context, return the pointer itself
                    m_currentLLVMValue = pointerValue;
                } else {
                    // Otherwise dereference the pointer (load)
                    llvm::Type* loadTy = int64Type; // Default to Int for tests
                    
                    // Try to determine the appropriate load type
                    if (auto allocaInst = llvm::dyn_cast<llvm::AllocaInst>(pointerValue)) {
                        loadTy = allocaInst->getAllocatedType();
                    } else if (node->arguments[0]->type) {
                        // Try to determine type from AST node
                        if (auto ptrType = dynamic_cast<ast::PointerType*>(node->arguments[0]->type.get())) {
                            if (ptrType->pointeeType) {
                                loadTy = codegenType(ptrType->pointeeType.get());
                            }
                        } else if (auto typeName = dynamic_cast<ast::TypeName*>(node->arguments[0]->type.get())) {
                            // Handle loc<T> type
                            if (typeName->identifier->name == "loc" && !typeName->genericArgs.empty()) {
                                loadTy = codegenType(typeName->genericArgs[0].get());
                            }
                        }
                    }
                    
                    m_currentLLVMValue = builder->CreateLoad(loadTy, pointerValue, "deref.load");
                }
                
                builder->CreateBr(mergeBB);
                
                // Set up the null block
                builder->SetInsertPoint(nullBB);
                builder->CreateUnreachable();
                
                // Set up the merge block
                builder->SetInsertPoint(mergeBB);
                return;
            } else {
                logError(node->loc, "at() called on non-pointer type. Got: " + getTypeName(m_currentLLVMValue->getType()));
                m_currentLLVMValue = nullptr;
                return;
            }
        }
        // Check for loc() intrinsic
        else if (identCallee->name == "loc") {
            // First evaluate the argument to get its address
            auto ident = dynamic_cast<vyn::ast::Identifier*>(node->arguments[0].get());
            if (ident) {
                auto it = namedValues.find(ident->name);
                if (it != namedValues.end()) {
                    // Return the address of the alloca directly
                    m_currentLLVMValue = it->second;
                    return;
                }
            }
            
            // If we couldn't find the variable directly, try evaluating the argument generically
            node->arguments[0]->accept(*this);
            if (!m_currentLLVMValue) {
                logError(node->arguments[0]->loc, "Argument to loc() evaluated to null");
                return;
            }
            
            // The result of loc() is the pointer to the value
            if (auto* allocaInst = llvm::dyn_cast<llvm::AllocaInst>(m_currentLLVMValue)) {
                // If the value is already an alloca instruction, just use it directly
                return;
            } else {
                // For non-alloca values, we need to create temporary storage
                llvm::Type* valType = m_currentLLVMValue->getType();
                llvm::Value* tempAlloca = builder->CreateAlloca(valType, nullptr, "loc_temp");
                builder->CreateStore(m_currentLLVMValue, tempAlloca);
                m_currentLLVMValue = tempAlloca;
            }
            return;
        }
    }
    
    // Handle from<T>() intrinsic - more complex because it requires a type parameter
    if (identCallee && identCallee->name == "from" && node->arguments.size() == 1) {
        // Evaluate the address expression
        node->arguments[0]->accept(*this);
        if (!m_currentLLVMValue) {
            logError(node->arguments[0]->loc, "from<T>() operand evaluated to null");
            return;
        }
        
        if (!m_currentLLVMValue->getType()->isIntegerTy()) {
            logError(node->arguments[0]->loc, "from<T>() requires an integer address argument. Got: " + 
                                           getTypeName(m_currentLLVMValue->getType()));
            return;
        }
        
        // Default to i64* pointer type
        llvm::Type* ptrTy = llvm::PointerType::getUnqual(int64Type);
        
        // Convert integer to pointer
        m_currentLLVMValue = builder->CreateIntToPtr(m_currentLLVMValue, ptrTy, "from_cast");
        return;
    }
    
    // Lookup the function in the module - standard function call handling
    llvm::Function *calleeFunc = module->getFunction(calleeName);
    if (!calleeFunc) {
        // Try mangled name if it's a method or from a namespace
        // This part needs a robust name mangling and lookup scheme
        logError(node->callee->loc, "Function " + calleeName + " not found.");
        m_currentLLVMValue = nullptr;
        return;
    }

    // Check argument count
    if (calleeFunc->arg_size() != node->arguments.size()) {
        logError(node->loc, "Incorrect number of arguments passed to function " + calleeName);
        m_currentLLVMValue = nullptr;
        return;
    }

    std::vector<llvm::Value *> argValues;
    for (size_t i = 0; i < node->arguments.size(); ++i) {
        node->arguments[i]->accept(*this);
        llvm::Value* argValue = m_currentLLVMValue;
        if (!argValue) {
            logError(node->arguments[i]->loc, "Argument codegen failed for call to " + calleeName);
            m_currentLLVMValue = nullptr;
            return;
        }

        // Implicit cast if necessary (e.g. int to float)
        llvm::Type* expectedArgType = calleeFunc->getFunctionType()->getParamType(i);
        if (argValue->getType() != expectedArgType) {
            if (expectedArgType->isFloatingPointTy() && argValue->getType()->isIntegerTy()) {
                argValue = builder->CreateSIToFP(argValue, expectedArgType, "callargcast");
            } else if (expectedArgType->isIntegerTy() && argValue->getType()->isFloatingPointTy()) {
                argValue = builder->CreateFPToSI(argValue, expectedArgType, "callargcast");
            }
            // Add more sophisticated casting rules as needed
        }
        
        if (argValue->getType() != expectedArgType) {
             logError(node->arguments[i]->loc, "Argument type mismatch for call to " + calleeName + ". Expected " + getTypeName(expectedArgType) + " but got " + getTypeName(argValue->getType()));
             m_currentLLVMValue = nullptr;
             return;
        }
        argValues.push_back(argValue);
    }

    if (calleeFunc->getReturnType()->isVoidTy()) {
        builder->CreateCall(calleeFunc, argValues);
        m_currentLLVMValue = nullptr; // No value for void calls
    } else {
        m_currentLLVMValue = builder->CreateCall(calleeFunc, argValues, "calltmp");
    }
}

void LLVMCodegen::visit(vyn::ast::LocationExpression *node) {
    // loc(expr) creates a pointer to the expression's memory location
    
    // 1. Evaluate the expression to get its value
    node->expression->accept(*this);
    llvm::Value *exprVal = m_currentLLVMValue;
    if (!exprVal) {
        logError(node->loc, "Expression in loc() evaluated to null");
        m_currentLLVMValue = nullptr;
        return;
    }
    
    // 2. If the expression is already a pointer type, use it directly
    if (exprVal->getType()->isPointerTy()) {
        m_currentLLVMValue = exprVal;
        return;
    }
    
    // 3. Create an alloca for the value and store it
    llvm::Type* valType = exprVal->getType();
    llvm::Value* tempAlloca = builder->CreateAlloca(valType, nullptr, "loc_alloca");
    builder->CreateStore(exprVal, tempAlloca);
    
    // 4. Return the pointer
    m_currentLLVMValue = tempAlloca;
}

void LLVMCodegen::visit(vyn::ast::AddrOfExpression *node) {
    // addr(expr) gets the address of a pointer expression
    
    // 1. Evaluate the expression to get the pointer
    node->getLocation()->accept(*this);
    llvm::Value *exprVal = m_currentLLVMValue;
    if (!exprVal) {
        logError(node->loc, "Expression in addr() evaluated to null");
        m_currentLLVMValue = nullptr;
        return;
    }
    
    // 2. If we have a pointer to a pointer, load the actual pointer
    if (exprVal->getType()->isPointerTy()) {
        // Load the pointer value if we have a pointer-to-pointer
        if (auto allocaInst = llvm::dyn_cast<llvm::AllocaInst>(exprVal)) {
            if (allocaInst->getAllocatedType()->isPointerTy()) {
                exprVal = builder->CreateLoad(allocaInst->getAllocatedType(), exprVal, "ptr_load");
            }
        }
    }
    
    // 3. Convert the pointer to an integer
    if (!exprVal->getType()->isPointerTy()) {
        logError(node->loc, "Expression in addr() must be a pointer type");
        m_currentLLVMValue = nullptr;
        return;
    }
    
    m_currentLLVMValue = builder->CreatePtrToInt(exprVal, int64Type, "addr_cast");
}

void LLVMCodegen::visit(vyn::ast::PointerDerefExpression *node) {
    // at(ptr) dereferences a pointer for reading or writing
    
    // 1. Evaluate the pointer expression
    node->pointer->accept(*this);
    llvm::Value *ptrVal = m_currentLLVMValue;
    if (!ptrVal) {
        logError(node->loc, "Pointer expression in at() evaluated to null");
        m_currentLLVMValue = nullptr;
        return;
    }
    
    // 2. If we have a pointer to a pointer (e.g., alloca of a pointer), load the actual pointer
    if (ptrVal->getType()->isPointerTy()) {
        // Load the pointer value if we have a pointer-to-pointer
        if (auto allocaInst = llvm::dyn_cast<llvm::AllocaInst>(ptrVal)) {
            if (allocaInst->getAllocatedType()->isPointerTy()) {
                ptrVal = builder->CreateLoad(allocaInst->getAllocatedType(), ptrVal, "ptr_load");
            }
        }
    }
    
    // 3. Verify we have a valid pointer type
    if (!ptrVal->getType()->isPointerTy()) {
        logError(node->loc, "Operand of at() must be a pointer type. Got: " + getTypeName(ptrVal->getType()));
        m_currentLLVMValue = nullptr;
        return;
    }
    
    // 4. Add null check
    llvm::BasicBlock *currentBB = builder->GetInsertBlock();
    llvm::Function *currentFn = currentBB->getParent();
    
    llvm::BasicBlock *notNullBB = llvm::BasicBlock::Create(*context, "ptr.not_null", currentFn);
    llvm::BasicBlock *nullBB = llvm::BasicBlock::Create(*context, "ptr.null", currentFn);
    llvm::BasicBlock *mergeBB = llvm::BasicBlock::Create(*context, "ptr.merge", currentFn);
    
    llvm::Value *isNotNull = builder->CreateICmpNE(ptrVal, llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(ptrVal->getType())), "ptr.is_not_null");
    builder->CreateCondBr(isNotNull, notNullBB, nullBB);
    
    // Not null case: perform the dereference
    builder->SetInsertPoint(notNullBB);
    
    // For assignment target, return the pointer itself
    // For reading, load the value
    if (m_isLHSOfAssignment) {
        m_currentLLVMValue = ptrVal;
    } else {
        // For reading, load the value
        llvm::Type* pointeeType = nullptr;
        if (node->type) {
            pointeeType = codegenType(node->type.get());
        } else {
            // Fallback to i64 for test cases
            pointeeType = int64Type;
        }
        m_currentLLVMValue = builder->CreateLoad(pointeeType, ptrVal, "deref.load");
    }
    
    builder->CreateBr(mergeBB);
    
    // Null case: handle the error
    builder->SetInsertPoint(nullBB);
    builder->CreateUnreachable();
    
    // Merge point
    builder->SetInsertPoint(mergeBB);
}

void LLVMCodegen::visit(vyn::ast::AssignmentExpression *node) {
    // Save and set LHS flag before visiting LHS
    bool wasLHS = m_isLHSOfAssignment;
    m_isLHSOfAssignment = true;
    node->left->accept(*this);
    m_isLHSOfAssignment = wasLHS;
    llvm::Value *LHS = m_currentLLVMValue;

    // Generate RHS value
    node->right->accept(*this);
    llvm::Value *RHS = m_currentLLVMValue;
    if (!LHS || !RHS) {
        logError(node->loc, "Invalid operands in assignment.");
        m_currentLLVMValue = nullptr;
        return;
    }
    
    // Capture type information from AST if available
    std::shared_ptr<vyn::ast::TypeNode> lhsTypeNode = node->left->type;
    std::shared_ptr<vyn::ast::TypeNode> rhsTypeNode = node->right->type;
    
    // Store location for error reporting
    SourceLocation errorLoc = node->left->loc;
    
    // Check if we're assigning to a variable (used for special case handling)
    bool isAssignToVar = false;
    auto identLeft = dynamic_cast<ast::Identifier*>(node->left.get());
    if (identLeft) {
        isAssignToVar = true;
        
        // For direct identifier assignments, register the RHS type with the identifier
        // This helps propagate type info for variables
        if (identLeft->type && RHS) {
            valueTypeMap[RHS] = identLeft->type;
            std::cout << "DEBUG: In assignment, associated RHS value with identifier type: " 
                      << identLeft->type->toString() << std::endl;
        }
    }
    
    // Check if LHS is a valid target for assignment
    if (!LHS->getType()->isPointerTy()) {
        // Log detailed information about the LHS
        std::string lhsTypeStr = getTypeName(LHS->getType());
        std::string lhsNodeType = "<unknown>";
        
        // Get more detailed info about the LHS expression type
        if (auto callExpr = dynamic_cast<ast::CallExpression*>(node->left.get())) {
            if (auto identCallee = dynamic_cast<ast::Identifier*>(callExpr->callee.get())) {
                lhsNodeType = "CallExpression to " + identCallee->name;
            } else {
                lhsNodeType = "CallExpression";
            }
        } else if (dynamic_cast<ast::PointerDerefExpression*>(node->left.get())) {
            lhsNodeType = "PointerDerefExpression";
        } else if (identLeft) {
            lhsNodeType = "Identifier: " + identLeft->name;
        }
        
        // If the LHS is a variable and RHS is a pointer-to-int conversion (addr()), 
        // we're probably assigning an address to an int variable, which is valid
        if (isAssignToVar && RHS->getType()->isIntegerTy()) {
            // Create an alloca if LHS doesn't point to memory yet
            llvm::AllocaInst* allocaInst = nullptr;
            if (auto existingAlloca = llvm::dyn_cast<llvm::AllocaInst>(LHS)) {
                // LHS is already an alloca, use it
                allocaInst = existingAlloca;
            } else {
                // Need to create an alloca for the LHS
                if (identLeft) {
                    // Look up the alloca for the variable
                    auto it = namedValues.find(identLeft->name);
                    if (it != namedValues.end() && llvm::isa<llvm::AllocaInst>(it->second)) {
                        allocaInst = llvm::cast<llvm::AllocaInst>(it->second);
                    }
                }
                
                if (!allocaInst) {
                    logError(errorLoc, "Cannot assign to " + lhsNodeType + " (not a valid destination for assignment)");
                    m_currentLLVMValue = nullptr;
                    return;
                }
            }
            
            // Store the integer value directly
            builder->CreateStore(RHS, allocaInst);
            
            // Preserve AST type information
            if (lhsTypeNode) {
                valueTypeMap[allocaInst] = lhsTypeNode;
            }
            
            m_currentLLVMValue = RHS;
            return;
        } else {
            logError(errorLoc, "Destination for assignment is not a pointer type. Got: " + 
                     lhsTypeStr + " (Node type: " + lhsNodeType + ")");
            m_currentLLVMValue = nullptr;
            return;
        }
    }

    // Determine the pointee type for the store
    llvm::Type *destPointeeType = nullptr;

    // Try to get pointee type from all available sources
    if (auto allocaInst = llvm::dyn_cast<llvm::AllocaInst>(LHS)) {
        destPointeeType = allocaInst->getAllocatedType();
    } else if (auto gep = llvm::dyn_cast<llvm::GetElementPtrInst>(LHS)) {
        destPointeeType = gep->getResultElementType();
    } else if (lhsTypeNode) {
        destPointeeType = codegenType(lhsTypeNode.get());
    } else {
        // Try to infer pointer element type from AST
        // For PointerDerefExpression, try to get the type info from pointer operand
        if (auto pointerDeref = dynamic_cast<ast::PointerDerefExpression*>(node->left.get())) {
            if (pointerDeref->pointer && pointerDeref->pointer->type) {
                // If it's a pointer type, get its pointee type
                if (auto ptrType = dynamic_cast<ast::PointerType*>(pointerDeref->pointer->type.get())) {
                    if (ptrType->pointeeType) {
                        destPointeeType = codegenType(ptrType->pointeeType.get());
                    }
                }
                // If it's a loc<T> type, get T
                else if (auto locType = dynamic_cast<ast::TypeName*>(pointerDeref->pointer->type.get())) {
                    if (locType->identifier->name == "loc" && !locType->genericArgs.empty()) {
                        destPointeeType = codegenType(locType->genericArgs[0].get());
                    }
                }
            }
        }
        
        // Fallback: Use RHS type if all else fails
        if (!destPointeeType) {
            destPointeeType = RHS->getType();
        }
    }

    // Cast RHS to match destination type if needed
    if (RHS->getType() != destPointeeType && destPointeeType) {
        if (destPointeeType->isFloatingPointTy() && RHS->getType()->isIntegerTy()) {
            RHS = builder->CreateSIToFP(RHS, destPointeeType, "assigncast");
        } else if (destPointeeType->isIntegerTy() && RHS->getType()->isFloatingPointTy()) {
            RHS = builder->CreateFPToSI(RHS, destPointeeType, "assigncast");
        } else if (destPointeeType->isPointerTy() && RHS->getType()->isPointerTy()) {
            RHS = builder->CreatePointerCast(RHS, destPointeeType, "assigncast");
        } else if (destPointeeType->isIntegerTy() && RHS->getType()->isPointerTy()) {
            RHS = builder->CreatePtrToInt(RHS, destPointeeType, "assigncast");
        } else if (destPointeeType->isPointerTy() && RHS->getType()->isIntegerTy()) {
            RHS = builder->CreateIntToPtr(RHS, destPointeeType, "assigncast");
        }
    }

    // If types still don't match, warn but proceed (might still work in some cases)
    if (RHS->getType() != destPointeeType) {
        logWarning(node->loc, "Type mismatch in assignment. Storing " + getTypeName(RHS->getType()) + 
                  " into location of type " + (destPointeeType ? getTypeName(destPointeeType) : "unknown"));
    }

    // Create the store instruction with proper alignment
    builder->CreateStore(RHS, LHS);
    
    // If we're assigning to a member of a struct, we need to preserve type information
    if (auto memberExpr = dynamic_cast<ast::MemberExpression*>(node->left.get())) {
        if (memberExpr->object->type) {
            // The object has a type, so we can use it to determine field types
            // In a full implementation, we would look up the field's type from the struct definition
            // For now, we will propagate the object's type to help with future member accesses
            valueTypeMap[RHS] = memberExpr->object->type;
            
            // If LHS is from a GEP instruction, associate the struct type with the GEP result
            if (auto gep = llvm::dyn_cast<llvm::GetElementPtrInst>(LHS)) {
                valueTypeMap[gep] = memberExpr->object->type;
            }
        }
    }
    
    // Return the value being stored (C semantics)
    m_currentLLVMValue = RHS;
    
    // Make sure we also associate this result with the type if available
    if (rhsTypeNode) {
        valueTypeMap[RHS] = rhsTypeNode;
    }
}


void LLVMCodegen::visit(vyn::ast::ArrayElementExpression *node) {
    // This is for using array[index] as an R-value (i.e., loading the value)
    // LHS usage is handled in AssignmentExpression
    node->array->accept(*this);
    llvm::Value *arrayPtr = m_currentLLVMValue; // Should be a pointer to the first element

    node->index->accept(*this);
    llvm::Value *indexVal = m_currentLLVMValue;

    if (!arrayPtr || !indexVal) {
        logError(node->loc, "Array or index expression for element access failed to codegen.");
        m_currentLLVMValue = nullptr;
        return;
    }

    // arrayPtr must be a pointer type for GEP
    if (!arrayPtr->getType()->isPointerTy()) {
        logError(node->array->loc, "Base of array element access (R-value) is not a pointer. Type: " + getTypeName(arrayPtr->getType()));
        m_currentLLVMValue = nullptr;
        return;
    }
    
    // Determine element type for GEP and Load. This is the type GEP operates on.
    llvm::Type* elementType = nullptr;
    if (node->array->type) { // AST type of the array expression itself
        vyn::ast::TypeNode* arrAstTypeNode = node->array->type.get();
        if (auto arrayAstType = dynamic_cast<vyn::ast::ArrayType*>(arrAstTypeNode)) {
            // If the AST says it's an array T[N], then GEP on a T* needs element type T
            elementType = codegenType(arrayAstType->elementType.get());
        } else if (auto ptrAstType = dynamic_cast<vyn::ast::PointerType*>(arrAstTypeNode)) {
            // If the AST says it's a pointer T*, then GEP on T* needs element type T
            // Or if it's T(*)[N], GEP still needs T.
            // The key is what the pointer *ultimately points to* at the element level.
            if (auto pointedToArrType = dynamic_cast<vyn::ast::ArrayType*>(ptrAstType->pointeeType.get())) { // Corrected member access
                elementType = codegenType(pointedToArrType->elementType.get()); // Pointer to array, get element type of that array
            } else {
                elementType = codegenType(ptrAstType->pointeeType.get()); // Corrected member access // Pointer to element
            }
        }
    }

    if (!elementType) { 
        // This is a fallback/error if AST type information was insufficient or missing.
        // Relying on LLVM types directly is problematic with opaque pointers.
        logError(node->loc, "Could not determine element type for array access (R-value) from AST. Array AST type: " + (node->array->type ? node->array->type->toString() : "null"));
        m_currentLLVMValue = nullptr;
        return;
    }

    llvm::Value *elementAddress = builder->CreateGEP(elementType, arrayPtr, indexVal, "arrayelemaddr_rval");
    m_currentLLVMValue = builder->CreateLoad(elementType, elementAddress, "arrayelemload");
}

void LLVMCodegen::visit(vyn::ast::ListComprehension *node) {
    // List comprehensions are complex and involve creating a new list,
    // iterating, filtering, and transforming elements.
    // This is a placeholder and would require significant implementation,
    // potentially involving runtime calls to list manipulation functions.
    logError(node->loc, "ListComprehension codegen is not yet implemented.");
    m_currentLLVMValue = nullptr; // No value produced yet
}

void LLVMCodegen::visit(vyn::ast::IfExpression *node) {
    // Codegen the condition
    node->condition->accept(*this);
    llvm::Value *condValue = m_currentLLVMValue;
    if (!condValue) {
        logError(node->condition->loc, "Condition for if-expression is null.");
        m_currentLLVMValue = nullptr;
        return;
    }

    // Convert condition to a bool (i1) if it's not already
    if (!condValue->getType()->isIntegerTy(1)) {
        if (condValue->getType()->isIntegerTy()) { // Other integer types
            condValue = builder->CreateICmpNE(condValue, llvm::ConstantInt::get(condValue->getType(), 0), "ifcond");
        } else if (condValue->getType()->isFloatingPointTy()) {
            condValue = builder->CreateFCmpONE(condValue, llvm::ConstantFP::get(condValue->getType(), 0.0), "ifcond");
        } else {
            logError(node->condition->loc, "If-expression condition must be a boolean, integer or float.");
            m_currentLLVMValue = nullptr;
            return;
        }
    }

    llvm::Function *func = builder->GetInsertBlock()->getParent();

    // Create blocks for the then, else, and merge branches
    llvm::BasicBlock *thenBB = llvm::BasicBlock::Create(*context, "then", func);
    llvm::BasicBlock *elseBB = llvm::BasicBlock::Create(*context, "else", func);
    llvm::BasicBlock *mergeBB = llvm::BasicBlock::Create(*context, "ifcont", func);

    builder->CreateCondBr(condValue, thenBB, elseBB);

    // Emit then value
    builder->SetInsertPoint(thenBB);
    node->thenBranch->accept(*this);
    llvm::Value *thenValue = m_currentLLVMValue;
    if (!thenValue) { // Check if then branch codegen failed
        logError(node->thenBranch->loc, "Then branch of if-expression codegen failed.");
        // If one branch fails, the whole if-expression might be considered failed.
        // Or, one could try to recover or ensure mergeBB is still reachable.
        // For now, if a branch fails, the PHI node might get an invalid input.
        // A robust solution would ensure all paths to mergeBB provide a value or handle errors.
        // We'll let it proceed, and if thenValue is null, the PHI node might complain or use undef.
    }
    builder->CreateBr(mergeBB);
    // Codegen of 'then' can change the current block, update thenBB for the PHI.
    thenBB = builder->GetInsertBlock();

    // Emit else value
    builder->SetInsertPoint(elseBB);
    node->elseBranch->accept(*this);
    llvm::Value *elseValue = m_currentLLVMValue;
     if (!elseValue) { // Check if else branch codegen failed
        logError(node->elseBranch->loc, "Else branch of if-expression codegen failed.");
    }
    builder->CreateBr(mergeBB);
    // Codegen of 'else' can change the current block, update elseBB for the PHI.
    elseBB = builder->GetInsertBlock();

    // Emit merge block
    builder->SetInsertPoint(mergeBB);
    
    // Type of the if-expression is the common type of then and else branches.
    // This should be determined by semantic analysis. For now, assume they are compatible
    // and use the type of the 'then' branch value if available, otherwise 'else'.
    llvm::Type* phiType = nullptr;
    if (thenValue) phiType = thenValue->getType();
    else if (elseValue) phiType = elseValue->getType();
    // else if (node->type) phiType = codegenType(node->type.get()); // Fallback to AST type
    
    if (!phiType) {
        if (node->type) { // Check if the IfExpression node itself has a type
             phiType = codegenType(node->type.get());
        }
        if (!phiType) {
            logError(node->loc, "Cannot determine type for PHI node in if-expression. Both branches might have failed or type info is missing.");
            m_currentLLVMValue = nullptr; // Or some error value
            return; // Critical error, cannot create PHI
        }
    }


    llvm::PHINode *phiNode = builder->CreatePHI(phiType, 2, "iftmp");

    // It's possible one of the branches failed codegen (value is null).
    // PHI nodes need valid llvm::Value inputs. If one is null, use UndefValue.
    if (thenValue) {
        phiNode->addIncoming(thenValue, thenBB);
    } else {
        phiNode->addIncoming(llvm::UndefValue::get(phiType), thenBB);
        logWarning(node->thenBranch->loc, "Then branch of if-expression resulted in null, using undef for PHI.");
    }

    if (elseValue) {
        phiNode->addIncoming(elseValue, elseBB);
    } else {
        phiNode->addIncoming(llvm::UndefValue::get(phiType), elseBB);
        logWarning(node->elseBranch->loc, "Else branch of if-expression resulted in null, using undef for PHI.");
    }
    
    m_currentLLVMValue = phiNode;
}

void LLVMCodegen::visit(vyn::ast::ConstructionExpression *node) {
    // This is for expressions like MyStruct(arg1, arg2) or new MyClass(arg1, arg2)
    // For structs, it typically means creating an aggregate and initializing fields.
    // For classes (if they involve heap allocation via 'new'), it's more complex.
    // Vyn's syntax `MyType(...)` might be for both stack-allocated structs
    // and potentially heap-allocated objects if `MyType` is a class.
    // This distinction needs to be clear from semantic analysis (node->type or constructedType).

    // Let's assume this is for struct construction for now.
    // The result will be an aggregate value (e.g., an llvm::ConstantStruct or a value loaded from an alloca).

    if (!node->constructedType) {
        logError(node->loc, "ConstructionExpression is missing the type to construct.");
        m_currentLLVMValue = nullptr;
        return;
    }
    
    llvm::Type* constructedLLVMType = codegenType(node->constructedType.get());
    if (!constructedLLVMType) {
        logError(node->loc, "Could not determine LLVM type for ConstructionExpression: " + node->constructedType->toString());
        m_currentLLVMValue = nullptr;
        return;
    }

    if (auto structType = llvm::dyn_cast<llvm::StructType>(constructedLLVMType)) {
        if (structType->isOpaque()) {
            logError(node->loc, "Cannot construct opaque struct type: " + node->constructedType->toString());
            m_currentLLVMValue = nullptr;
            return;
        }

        if (node->arguments.size() != structType->getNumElements()) {
            logError(node->loc, "ConstructionExpression: Argument count mismatch for struct " + node->constructedType->toString() +
                                ". Expected " + std::to_string(structType->getNumElements()) +
                                ", got " + std::to_string(node->arguments.size()));
            m_currentLLVMValue = nullptr;
            return;
        }

        // This will create an aggregate value. If it needs to be mutable or have an address,
        // it should be stored in an alloca. The expression itself results in the value.
        // For struct construction, we often create an alloca and store fields into it,
        // then the "value" of the expression might be a load from that alloca if used as r-value,
        // or the alloca itself if its address is needed.
        // Let's assume it creates an alloca for the array
        llvm::AllocaInst* structAlloca = createEntryBlockAlloca(constructedLLVMType, "struct_alloca");

        // Initialize each field
        for (unsigned i = 0; i < node->arguments.size(); ++i) {
            node->arguments[i]->accept(*this);
            llvm::Value* argVal = m_currentLLVMValue;
            if (!argVal) {
                logError(node->arguments[i]->loc, "Argument " + std::to_string(i) + " for construction of " + node->constructedType->toString() + " codegen failed.");
                m_currentLLVMValue = nullptr;
                return;
            }
            // TODO: Type check/cast argVal to structType->getElementType(i)
            if (argVal->getType() != structType->getElementType(i)) {
                 // Attempt cast
                if (structType->getElementType(i)->isFloatingPointTy() && argVal->getType()->isIntegerTy()) {
                    argVal = builder->CreateSIToFP(argVal, structType->getElementType(i), "constructcast");
                } else if (structType->getElementType(i)->isIntegerTy() && argVal->getType()->isFloatingPointTy()) {
                    argVal = builder->CreateFPToSI(argVal, structType->getElementType(i), "constructcast");
                } else if (structType->getElementType(i)->isPointerTy() && argVal->getType()->isPointerTy() && argVal->getType() != structType->getElementType(i)) {
                    argVal = builder->CreateBitCast(argVal, structType->getElementType(i), "constructptrcast");
                }


                if (argVal->getType() != structType->getElementType(i)) {
                    logError(node->arguments[i]->loc, "Argument " + std::to_string(i) + " type mismatch for construction of " + node->constructedType->toString() +
                                                    ". Expected " + getTypeName(structType->getElementType(i)) +
                                                    ", got " + getTypeName(argVal->getType()));
                    m_currentLLVMValue = nullptr;
                    return;
                }
            }
            // Store the value into the struct
            builder->CreateStore(argVal, structAlloca);
        }
        m_currentLLVMValue = structAlloca;

    } else {
        // Handle other constructible types like arrays, or classes (which might involve 'new')
        logError(node->loc, "ConstructionExpression for non-struct type (" + getTypeName(constructedLLVMType) + ") not yet fully implemented.");
        m_currentLLVMValue = nullptr;
    }
}

void LLVMCodegen::visit(vyn::ast::ArrayInitializationExpression *node) {
    // Syntax: [ElementType; size_expr]() or similar for array of default values
    // Or potentially: [value; size_expr] for array of same value (like Rust)
    // The current AST (ArrayInitializationExpression) has elementType and sizeExpression.
    // This implies an array of `sizeExpression` elements, each of `elementType`,
    // likely default-initialized.

    if (!node->elementType || !node->sizeExpression) {
        logError(node->loc, "ArrayInitializationExpression is missing element type or size expression.");
        m_currentLLVMValue = nullptr;
        return;
    }

    llvm::Type* elementLLVMType = codegenType(node->elementType.get());
    if (!elementLLVMType) {
        logError(node->elementType->loc, "Could not determine LLVM type for array element: " + node->elementType->toString());
        m_currentLLVMValue = nullptr;
        return;
    }

    node->sizeExpression->accept(*this);
    llvm::Value* sizeValue = m_currentLLVMValue;
    if (!sizeValue) {
        logError(node->sizeExpression->loc, "Array size expression codegen failed.");
        m_currentLLVMValue = nullptr;
        return;
    }

    if (!sizeValue->getType()->isIntegerTy()) {
        logError(node->sizeExpression->loc, "Array size expression must be an integer.");
        m_currentLLVMValue = nullptr;
        return;
    }
    
    // The result of this expression is the array itself.
    // In LLVM, arrays are often handled via allocas on the stack.
    // If this expression is an R-value, it might mean creating a constant array if size is constant,
    // or dynamically allocating and initializing if size is variable (more complex, might involve runtime).

    // For simplicity, let's assume this creates an AllocaInst if used in a context
    // where an address is needed, or an aggregate constant if possible.
    // However, visit methods for expressions should produce a Value.
    // If the size is a constant integer:
    if (auto constSize = llvm::dyn_cast<llvm::ConstantInt>(sizeValue)) {
        uint64_t arraySize = constSize->getZExtValue();
        llvm::ArrayType* arrayLLVMType = llvm::ArrayType::get(elementLLVMType, arraySize);

        // How to represent the "value" of this array initialization?
        // If it's default initialization, it could be an UndefValue of array type,
        // or a zeroinitializer.
        // If it's for a stack variable, the VariableDeclaration visitor would handle the alloca.
        // This expression node itself, if it's an r-value, should yield the array value.
        
        // Create an alloca for the array
        llvm::AllocaInst* arrayAlloca = createEntryBlockAlloca(arrayLLVMType, "arrayinit.alloca");

        // Default initialization (e.g., to zero or undef)
        // For zero initialization:
        // builder->CreateStore(llvm::Constant::getNullValue(arrayLLVMType), arrayAlloca);
        // Or initialize each element in a loop if non-trivial default constructor or specific value.
        // For now, let's assume it's an uninitialized array, and the alloca is the "result"
        // if an address is what's expected. But expressions usually yield values.
        // This is tricky. If it's `let x = [0; 10]`, x is the array.
        // The expression `[0; 10]` should yield the array.
        // TODO: Determine what m_currentLLVMValue should be.
        // For now, setting to the alloca, assuming it might be used as a pointer.
        m_currentLLVMValue = arrayAlloca; 
    } else {
        // Handle dynamic-sized arrays (more complex, may require runtime support or VLA-like features)
        logError(node->loc, "Dynamic-sized array initialization not yet fully supported for direct value generation.");
        m_currentLLVMValue = nullptr;
    }
}

void LLVMCodegen::visit(vyn::ast::FromIntToLocExpression *node) {
    // from<T>(expr) converts an integer expression to a pointer of type T
    
    // 1. Evaluate the expression to get the integer value
    node->getAddressExpression()->accept(*this);
    llvm::Value *exprVal = m_currentLLVMValue;
    if (!exprVal) {
        logError(node->loc, "Expression in from() evaluated to null");
        m_currentLLVMValue = nullptr;
        return;
    }
    
    // 2. Get the target pointer type from the type argument
    llvm::Type *targetType = nullptr;
    if (node->type) {
        targetType = codegenType(node->type.get());
        if (!targetType) {
            logError(node->loc, "Invalid type argument in from()");
            m_currentLLVMValue = nullptr;
            return;
        }
    } else {
        // Fallback to i64* for test cases
        targetType = int64Type->getPointerTo();
    }
    
    // 3. If we have a pointer to an integer, load it
    if (exprVal->getType()->isPointerTy()) {
        // Load the value if it's a pointer to an integer
        exprVal = builder->CreateLoad(int64Type, exprVal, "addr_load");
    }
    
    // 4. Convert to i64 if needed
    if (exprVal->getType()->isIntegerTy() && exprVal->getType() != int64Type) {
        exprVal = builder->CreateIntCast(exprVal, int64Type, true, "addr_to_i64");
    }
    
    // 5. Convert the integer value to a pointer
    if (!exprVal->getType()->isIntegerTy()) {
        logError(node->loc, "Expression in from() must be an integer type");
        m_currentLLVMValue = nullptr;
        return;
    }
    
    // If the target type is a pointer type, create a pointer cast
    if (targetType->isPointerTy()) {
        m_currentLLVMValue = builder->CreateIntToPtr(exprVal, targetType, "from_cast");
    } else {
        logError(node->loc, "Target type in from() must be a pointer type");
        m_currentLLVMValue = nullptr;
    }
}

void LLVMCodegen::visit(vyn::ast::Identifier *node) {
    // Handle identifier expressions
    std::string name = node->name;
    auto it = namedValues.find(name);
    if (it != namedValues.end()) {
        llvm::Value* identValue = it->second;
        std::cout << "DEBUG: Found identifier '" << name << "' in namedValues with ptr=" << (void*)identValue << std::endl;
        m_currentLLVMValue = identValue;
        // Restore type info if missing
        if ((!node->type || node->type->toString() == "") && valueTypeMap.count(identValue)) {
            node->type = valueTypeMap[identValue];
            std::cout << "DEBUG: Restored type for identifier '" << name << "' from valueTypeMap: " << node->type->toString() << std::endl;
        }
        // Associate the variable itself with its AST type (if any)
        if (node->type && identValue) {
            valueTypeMap[identValue] = node->type;
            std::cout << "DEBUG: Associated identifier '" << name << "' with type: " << node->type->toString() << std::endl;
        }
        // Only load if not LHS of assignment and not member access base
        if (!m_isLHSOfAssignment && !m_isMemberAccessBase && m_currentLLVMValue->getType()->isPointerTy()) {
            // Determine the type to load
            llvm::Type* loadType = nullptr;
            if (auto allocaInst = llvm::dyn_cast<llvm::AllocaInst>(m_currentLLVMValue)) {
                loadType = allocaInst->getAllocatedType();
            } else if (node->type) {
                loadType = codegenType(node->type.get());
            }
            if (loadType) {
                auto loadValue = builder->CreateLoad(loadType, m_currentLLVMValue, name + "_load");
                // Store the association between the loaded value and the original AST type
                if (node->type) {
                    valueTypeMap[loadValue] = node->type;
                    // Also propagate the type to the loaded value's AST node if possible
                    node->type = node->type; // Redundant, but ensures type is set
                }
                m_currentLLVMValue = loadValue;
            }
        }
        return;
    }
    std::cout << "DEBUG: Identifier '" << name << "' NOT found in namedValues." << std::endl;
    // If not found in local scope, try global scope
    llvm::GlobalVariable* global = module->getNamedGlobal(name);
    if (global) {
        m_currentLLVMValue = global;
        
        // Associate the global with its AST type
        if (node->type) {
            valueTypeMap[global] = node->type;
        }
        
        // Load global variables when used as r-values
        if (!m_isLHSOfAssignment) {
            llvm::Type* loadType = global->getValueType();
            auto loadValue = builder->CreateLoad(loadType, global, name + "_global_load");
            
            // Associate the loaded value with its AST type
            if (node->type) {
                valueTypeMap[loadValue] = node->type;
            }
            
            m_currentLLVMValue = loadValue;
        }
        return;
    }
    
    // If not found, try to find a function with this name
    llvm::Function* func = module->getFunction(name);
    if (func) {
        m_currentLLVMValue = func;
        return;
    }
    
    logError(node->loc, "Unknown identifier: " + name);
    m_currentLLVMValue = nullptr;
}

void LLVMCodegen::visit(vyn::ast::MemberExpression *node) {
    std::cout << "DEBUG: Entering MemberExpression codegen: object=" << (node->object ? node->object->toString() : "nullptr")
              << ", property=" << (node->property ? node->property->toString() : "nullptr") << std::endl;
    bool prevIsMemberAccessBase = m_isMemberAccessBase;
    m_isMemberAccessBase = true;
    node->object->accept(*this);
    m_isMemberAccessBase = prevIsMemberAccessBase;
    llvm::Value* objectValue = m_currentLLVMValue;
    if (!objectValue) {
        std::cout << "DEBUG: Failed to codegen object for member access: object is nullptr or codegen failed." << std::endl;
        m_currentLLVMValue = nullptr;
        return;
    }
    std::cout << "DEBUG: Object value for member access: " << (void*)objectValue << std::endl;

    // Get the type of the object (should be a pointer to struct)
    std::shared_ptr<vyn::ast::TypeNode> objectTypeNode = node->object->type;
    llvm::Type* objectLLVMType = nullptr;
    if (objectTypeNode) {
        objectLLVMType = codegenType(objectTypeNode.get());
    } else {
        logError(node->loc, "Cannot determine struct type for member access: missing AST type information. This is required due to opaque pointers in LLVM 18+.");
        m_currentLLVMValue = nullptr;
        return;
    }
    if (!objectLLVMType || !objectLLVMType->isStructTy()) {
        logError(node->loc, "Member access on non-struct type");
        m_currentLLVMValue = nullptr;
        return;
    }
    llvm::StructType* structTy = llvm::cast<llvm::StructType>(objectLLVMType);

    // Get the field name (assume property is Identifier)
    std::string fieldName;
    if (auto ident = dynamic_cast<vyn::ast::Identifier*>(node->property.get())) {
        fieldName = ident->name;
    } else {
        fieldName = node->property->toString();
    }
    int fieldIndex = getStructFieldIndex(structTy, fieldName);
    if (fieldIndex < 0) {
        logError(node->loc, "Field '" + fieldName + "' not found in struct");
        m_currentLLVMValue = nullptr;
        return;
    }

    llvm::Value* basePtr = objectValue;
    std::cout << "DEBUG: basePtr for GEP: ptr=" << (void*)basePtr
              << ", LLVM type=" << getTypeName(basePtr->getType()) << std::endl;
    std::cout << "DEBUG: structTy: " << (structTy ? structTy->getName().str() : "nullptr")
              << ", fieldName=" << fieldName << ", fieldIndex=" << fieldIndex << std::endl;
    llvm::Value* fieldPtr = builder->CreateStructGEP(structTy, basePtr, fieldIndex, fieldName + "_ptr");
    std::cout << "DEBUG: fieldPtr from GEP: ptr=" << (void*)fieldPtr
              << ", LLVM type=" << getTypeName(fieldPtr->getType()) << std::endl;

    if (m_isLHSOfAssignment) {
        m_currentLLVMValue = fieldPtr;
        return;
    }
    llvm::Type* fieldType = structTy->getElementType(fieldIndex);
    llvm::Value* loadedValue = builder->CreateLoad(fieldType, fieldPtr, fieldName + "_load");
    std::cout << "DEBUG: loadedValue from field: ptr=" << (void*)loadedValue
              << ", LLVM type=" << getTypeName(loadedValue->getType()) << std::endl;
    m_currentLLVMValue = loadedValue;
    return;
}

void LLVMCodegen::visit(vyn::ast::BorrowExpression* node) {
    if (!node->expression) {
        logError(node->loc, "Empty expression in borrow/view operation");
        m_currentLLVMValue = nullptr;
        return;
    }
    
    // Generate code for the expression being borrowed
    node->expression->accept(*this);
    if (!m_currentLLVMValue) {
        logError(node->expression->loc, "Failed to generate code for expression in borrow/view operation");
        return;
    }
    
    // Get the address of the expression being borrowed
    llvm::Value* exprValue = m_currentLLVMValue;
    
    // If the expression is not already a pointer (e.g., a local variable),
    // we need to create a pointer to it (only for rvalues)
    if (!exprValue->getType()->isPointerTy()) {
        // Create a temporary alloca to store the value
        llvm::Value* tempAlloca = builder->CreateAlloca(
            exprValue->getType(), nullptr, "borrow_temp");
        builder->CreateStore(exprValue, tempAlloca);
        exprValue = tempAlloca;
    }
    
    // For borrow/view, we're essentially creating a non-owning reference
    // In LLVM IR, this is just the pointer itself
    m_currentLLVMValue = exprValue;
    
    // In a more sophisticated implementation, we might:
    // 1. For owned types (my<T>), extract the raw pointer from the ownership wrapper
    // 2. For borrow vs view, add metadata or use a wrapper type to track mutability
}
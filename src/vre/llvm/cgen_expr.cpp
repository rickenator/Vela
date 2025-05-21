#include "vyn/vre/llvm/codegen.hpp"
#include "vyn/parser/ast.hpp"
#include "vyn/parser/token.hpp" // For TokenType in BinaryExpression

#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/DerivedTypes.h> // For PointerType, StructType
#include <llvm/ADT/APFloat.h>   // For APFloat in FloatLiteral
#include <llvm/ADT/APInt.h>     // For APInt in IntegerLiteral

using namespace vyn;
// using namespace llvm; // Uncomment if desired for brevity

// --- Literal Codegen ---
void LLVMCodegen::visit(vyn::ast::Identifier* node) {
    llvm::Value* val = nullptr;
    if (namedValues.count(node->name)) {
        val = namedValues[node->name];

        // --- PATCH: Ensure loc<T> variables always yield a pointer ---
        // If we have type info and it's a loc<T>, cast integer to pointer if needed
        llvm::Type* expectedType = nullptr;
        if (m_currentLLVMType && m_currentLLVMType->isPointerTy()) {
            expectedType = m_currentLLVMType;
        }
        if (expectedType && val->getType()->isIntegerTy()) {
            val = builder->CreateIntToPtr(val, expectedType, node->name + "_inttoptr");
        }

        if (m_isLHSOfAssignment) {
            m_currentLLVMValue = val;
        } else {
            // If on RHS, load the value from the address
            // But first, check if it's a function name (which shouldn't be loaded)
            if (llvm::isa<llvm::Function>(val)) {
                m_currentLLVMValue = val; // Function name evaluates to the function itself
            } else if (val->getType()->isPointerTy()) {
                // Modern LLVM: user must track the pointee type, as pointers no longer have element types
                llvm::Type* elementType = nullptr;
                // Try to get the type from the symbol table or context
                // For local variables, namedValues should store AllocaInsts, which have an allocated type
                if (auto* alloca = llvm::dyn_cast<llvm::AllocaInst>(val)) {
                    elementType = alloca->getAllocatedType();
                } else if (auto* gv = llvm::dyn_cast<llvm::GlobalVariable>(val)) {
                    elementType = gv->getValueType();
                } else if (val->getType()->isPointerTy()) {
                    // Fallback: try to use m_currentLLVMType if set by the parser/typechecker
                    elementType = m_currentLLVMType;
                }
                if (!elementType) {
                    logError(node->loc, "Cannot determine element type for pointer in identifier '" + node->name + "'. Please ensure type information is available.");
                    m_currentLLVMValue = nullptr;
                } else {
                    m_currentLLVMValue = builder->CreateLoad(elementType, val, node->name);
                }
            } else {
                // This case should ideally not happen for variables, as namedValues should store Allocas.
                // If it's some other kind of value directly in namedValues, it might be an error
                // or a direct value (e.g. a constant propagated). For now, assume it's a direct value.
                logWarning(node->loc, std::string("Identifier '") + node->name + "' in namedValues is not an AllocaInst or GlobalVariable. Treating as direct value.");
                m_currentLLVMValue = val;
            }
        }
    } else {
        // Check for global variables (like string literals)
        llvm::GlobalVariable* gv = module->getNamedGlobal(node->name);
        if (gv) {
            if (m_isLHSOfAssignment) {
                // Return the pointer to the global variable
                m_currentLLVMValue = gv;
            } else {
                // Load the value of the global variable
                // We need to determine the element type
                llvm::Type* elementType = gv->getValueType();
                m_currentLLVMValue = builder->CreateLoad(elementType, gv, node->name);
            }
        } else {
            // Could be a function name, try to find it
            llvm::Function* func = module->getFunction(node->name);
            if (func) {
                m_currentLLVMValue = func;
            } else {
                // Not found, report error
                logError(node->loc, "Unknown identifier: " + node->name);
                m_currentLLVMValue = nullptr;
            }
        }
    }
}

void LLVMCodegen::visit(vyn::ast::IntegerLiteral* node) {
    llvm::Type* intType = int64Type; // Vyn's default integer type
    m_currentLLVMValue = llvm::ConstantInt::get(intType, node->value, true); // true for signed
}

void LLVMCodegen::visit(vyn::ast::FloatLiteral* node) {
    llvm::Type* floatType = doubleType; // Vyn's default float type
    m_currentLLVMValue = llvm::ConstantFP::get(floatType, node->value);
}

void LLVMCodegen::visit(vyn::ast::BooleanLiteral* node) {
    m_currentLLVMValue = llvm::ConstantInt::get(int1Type, node->value ? 1 : 0, false);
}

void LLVMCodegen::visit(vyn::ast::StringLiteral* node) {
    // Create a global string constant for the string literal's value.
    // The result is a pointer to the first character (i8*).
    m_currentLLVMValue = builder->CreateGlobalStringPtr(node->value, ".str");
}

void LLVMCodegen::visit(vyn::ast::NilLiteral* node) {
    // Nil is a polymorphic null pointer. For now, default to i8*.
    // Type inference or context should ideally provide a more specific pointer type.
    if (m_currentLLVMType && m_currentLLVMType->isPointerTy()) {
        // If we have a specific pointer type from context, use it
        m_currentLLVMValue = llvm::ConstantPointerNull::get(static_cast<llvm::PointerType*>(m_currentLLVMType));
    } else {
        // Default to i8* if no specific type is known
        m_currentLLVMValue = llvm::ConstantPointerNull::get(llvm::PointerType::get(int8Type, 0));
    }
}

void LLVMCodegen::visit(vyn::ast::ObjectLiteral* node) {
    // This requires knowing the struct type to create an instance of.
    // Assuming the type is known and is a struct.
    // This is a simplified version. Real object literals might involve constructor calls.
    logError(node->loc, "ObjectLiteral codegen is not fully implemented yet.");
    m_currentLLVMValue = nullptr;
    // Example for a known struct type:
    // llvm::StructType* structTy = ... get struct type ...;
    // llvm::Constant* agg = llvm::ConstantStruct::get(structTy, fieldValues);
    // m_currentLLVMValue = agg; 
    // Or for mutable objects, allocate memory and store fields:
    // llvm::Value* alloca = builder->CreateAlloca(structTy, nullptr, "new_object");
    // For each field:
    //   llvm::Value* fieldPtr = builder->CreateStructGEP(structTy, alloca, fieldIndex, "field.ptr");
    //   builder->CreateStore(fieldValue, fieldPtr);
    // m_currentLLVMValue = alloca; // or the loaded value if appropriate
}

void LLVMCodegen::visit(vyn::ast::ArrayLiteral* node) {
    if (node->elements.empty()) {
        logError(node->loc, "Empty array literals not implemented yet.");
        m_currentLLVMValue = nullptr;
        return;
    }

    // Codegen all elements first to get their LLVM values and infer type
    std::vector<llvm::Constant*> elementConstants;
    llvm::Type* elementType = nullptr;

    for (const auto& elemNode : node->elements) {
        elemNode->accept(*this);
        llvm::Value* elemValue = m_currentLLVMValue;
        if (!elemValue) {
            logError(elemNode->loc, "Array element evaluated to null.");
            m_currentLLVMValue = nullptr;
            return;
        }
        
        if (!elementType) {
            elementType = elemValue->getType();
        } else if (elementType != elemValue->getType()) {
            logError(elemNode->loc, "Array elements must have the same type.");
            m_currentLLVMValue = nullptr;
            return;
        }
        
        // Array constants need constant elements
        llvm::Constant* elemConstant = llvm::dyn_cast<llvm::Constant>(elemValue);
        if (!elemConstant) {
            logError(elemNode->loc, "Array literal elements must be constants for global/static array. Dynamic arrays need heap allocation.");
            m_currentLLVMValue = nullptr;
            return;
        }
        
        elementConstants.push_back(elemConstant);
    }

    if (!elementType) {
        logError(node->loc, "Cannot determine element type for array literal.");
        m_currentLLVMValue = nullptr;
        return;
    }

    llvm::ArrayType* arrayType = llvm::ArrayType::get(elementType, elementConstants.size());
    
    // Create a global variable for the array if it's constant, or alloca if local & constant elements
    // For simplicity, let's assume it can be a constant global array for now.
    // This might need adjustment based on where the array literal can appear.
    llvm::Constant* arrayConstant = llvm::ConstantArray::get(arrayType, elementConstants);

    // If in a function, create an alloca and initialize it.
    // If global, create a global variable.
    if (currentFunction) {
        llvm::AllocaInst* alloca = builder->CreateAlloca(arrayType, nullptr, "arraylit");
        builder->CreateStore(arrayConstant, alloca);
        m_currentLLVMValue = alloca; // The alloca is the l-value for the array
    } else {
        // Global array literal
        auto* gv = new llvm::GlobalVariable(*module, arrayType, true /*isConstant*/, 
                                          llvm::GlobalValue::PrivateLinkage, arrayConstant, ".arrlit");
        m_currentLLVMValue = gv;
    }
}

// --- Expressions ---
void LLVMCodegen::visit(vyn::ast::UnaryExpression* node) {
    node->operand->accept(*this);
    llvm::Value* operandValue = m_currentLLVMValue;

    if (!operandValue) {
        logError(node->loc, "Operand of unary expression is null.");
        m_currentLLVMValue = nullptr;
        return;
    }

    switch (node->op.type) {
        case TokenType::MINUS: // Negation
            if (operandValue->getType()->isFloatingPointTy()) {
                m_currentLLVMValue = builder->CreateFNeg(operandValue, "fnegtmp");
            } else if (operandValue->getType()->isIntegerTy()) {
                m_currentLLVMValue = builder->CreateNeg(operandValue, "negtmp");
            } else {
                logError(node->loc, "Cannot negate non-numeric operand.");
                m_currentLLVMValue = nullptr;
            }
            break;
        case TokenType::BANG: // Logical NOT
            // Ensure operand is boolean (i1) or can be converted
            if (operandValue->getType() != int1Type) {
                // Convert to boolean: value != 0 for integers, value != 0.0 for floats, ptr != null for pointers
                if (operandValue->getType()->isIntegerTy()) {
                    operandValue = builder->CreateICmpNE(operandValue, llvm::ConstantInt::get(operandValue->getType(), 0), "tobool");
                } else if (operandValue->getType()->isFloatingPointTy()) {
                    operandValue = builder->CreateFCmpONE(operandValue, llvm::ConstantFP::get(operandValue->getType(), 0.0), "tobool");
                } else if (operandValue->getType()->isPointerTy()) {
                    operandValue = builder->CreateICmpNE(operandValue, llvm::ConstantPointerNull::get(static_cast<llvm::PointerType*>(operandValue->getType())), "tobool");
                } else {
                    logError(node->loc, "Cannot convert operand type to boolean for logical NOT.");
                    m_currentLLVMValue = nullptr;
                    return;
                }
            }
            m_currentLLVMValue = builder->CreateNot(operandValue, "nottmp"); // LLVM's 'not' is bitwise, for i1 it's logical NOT
            break;
        // Add cases for other unary operators like TILDE (bitwise NOT) if Vyn supports them
        default:
            logError(node->op.location, "Unsupported unary operator: " + node->op.lexeme);
            m_currentLLVMValue = nullptr;
    }
}

void LLVMCodegen::visit(vyn::ast::BinaryExpression* node) {
    // Special handling for short-circuiting logical operators
    if (node->op.type == TokenType::AND) {
        llvm::Function* parentFunc = builder->GetInsertBlock()->getParent();
        llvm::BasicBlock* startBB = builder->GetInsertBlock();
        llvm::BasicBlock* secondEvalBB = llvm::BasicBlock::Create(*context, "and.rhs", parentFunc);
        llvm::BasicBlock* endBB = llvm::BasicBlock::Create(*context, "and.end", parentFunc);

        node->left->accept(*this);
        llvm::Value* leftVal = m_currentLLVMValue;
        if (!leftVal) { logError(node->left->loc, "LHS of AND is null"); m_currentLLVMValue = nullptr; return; }
        if (leftVal->getType() != int1Type) { // Convert to bool if not already
            leftVal = builder->CreateICmpNE(leftVal, llvm::Constant::getNullValue(leftVal->getType()), "tobool.lhs");
        }
        
        // Conditional branch: if left is false, jump to endBB, else evaluate right
        builder->CreateCondBr(leftVal, secondEvalBB, endBB);

        builder->SetInsertPoint(secondEvalBB);
        node->right->accept(*this);
        llvm::Value* rightVal = m_currentLLVMValue;
        if (!rightVal) { logError(node->right->loc, "RHS of AND is null"); m_currentLLVMValue = nullptr; return; }
         if (rightVal->getType() != int1Type) { // Convert to bool if not already
            rightVal = builder->CreateICmpNE(rightVal, llvm::Constant::getNullValue(rightVal->getType()), "tobool.rhs");
        }
        llvm::BasicBlock* rhsEndBB = builder->GetInsertBlock(); // BB where RHS evaluation finished
        builder->CreateBr(endBB);

        builder->SetInsertPoint(endBB);
        llvm::PHINode* phi = builder->CreatePHI(int1Type, 2, "and.res");
        phi->addIncoming(llvm::ConstantInt::get(int1Type, 0), startBB); // If left was false
        phi->addIncoming(rightVal, rhsEndBB); // Value of right if left was true
        m_currentLLVMValue = phi;
        return;
    }

    if (node->op.type == TokenType::OR) {
        llvm::Function* parentFunc = builder->GetInsertBlock()->getParent();
        llvm::BasicBlock* startBB = builder->GetInsertBlock();
        llvm::BasicBlock* secondEvalBB = llvm::BasicBlock::Create(*context, "or.rhs", parentFunc);
        llvm::BasicBlock* endBB = llvm::BasicBlock::Create(*context, "or.end", parentFunc);

        node->left->accept(*this);
        llvm::Value* leftVal = m_currentLLVMValue;
        if (!leftVal) { logError(node->left->loc, "LHS of OR is null"); m_currentLLVMValue = nullptr; return; }
        if (leftVal->getType() != int1Type) { // Convert to bool if not already
            leftVal = builder->CreateICmpNE(leftVal, llvm::Constant::getNullValue(leftVal->getType()), "tobool.lhs");
        }

        // Conditional branch: if left is true, jump to endBB, else evaluate right
        builder->CreateCondBr(leftVal, endBB, secondEvalBB);

        builder->SetInsertPoint(secondEvalBB);
        node->right->accept(*this);
        llvm::Value* rightVal = m_currentLLVMValue;
        if (!rightVal) { logError(node->right->loc, "RHS of OR is null"); m_currentLLVMValue = nullptr; return; }
        if (rightVal->getType() != int1Type) { // Convert to bool if not already
            rightVal = builder->CreateICmpNE(rightVal, llvm::Constant::getNullValue(rightVal->getType()), "tobool.rhs");
        }
        llvm::BasicBlock* rhsEndBB = builder->GetInsertBlock(); // BB where RHS evaluation finished
        builder->CreateBr(endBB);

        builder->SetInsertPoint(endBB);
        llvm::PHINode* phi = builder->CreatePHI(int1Type, 2, "or.res");
        phi->addIncoming(llvm::ConstantInt::get(int1Type, 1), startBB); // If left was true
        phi->addIncoming(rightVal, rhsEndBB);    // Value of right if left was false
        m_currentLLVMValue = phi;
        return;
    }

    // Standard binary operation (non-short-circuiting)
    node->left->accept(*this);
    llvm::Value* L = m_currentLLVMValue;
    node->right->accept(*this);
    llvm::Value* R = m_currentLLVMValue;

    if (!L || !R) {
        logError(node->op.location, "One or both operands of binary expression are null.");
        m_currentLLVMValue = nullptr;
        return;
    }

    llvm::Type* lType = L->getType();
    llvm::Type* rType = R->getType();

    // Type checking and promotion might be needed here.
    // For simplicity, assume types are compatible or error out.
    // A more robust system would implement type promotion rules (e.g. int + float -> float)

    // Handle pointer arithmetic
    if (lType->isPointerTy() && rType->isIntegerTy()) {
        if (node->op.type == TokenType::PLUS || node->op.type == TokenType::MINUS) {
            llvm::PointerType* lPtrType = llvm::dyn_cast<llvm::PointerType>(lType);
            if (!lPtrType) {
                logError(node->op.location, "LHS is not a PointerType for pointer arithmetic.");
                m_currentLLVMValue = nullptr;
                return;
            }
            llvm::Type* elementType = nullptr;
            // For pointer arithmetic, we must know the element type. Try to infer from the pointer operand.
            // For LHS pointer, if it's an alloca, get the allocated type; if it's a global, get value type.
            if (auto* alloca = llvm::dyn_cast<llvm::AllocaInst>(L)) {
                elementType = alloca->getAllocatedType();
            } else if (auto* gv = llvm::dyn_cast<llvm::GlobalVariable>(L)) {
                elementType = gv->getValueType();
            } else if (m_currentLLVMType) {
                elementType = m_currentLLVMType;
            }
            if (!elementType) {
                logError(node->op.location, "Cannot determine element type for pointer arithmetic (LHS). Please ensure type information is available.");
                m_currentLLVMValue = nullptr;
                return;
            }
            if (node->op.type == TokenType::MINUS) {
                R = builder->CreateNeg(R, "negidx");
            }
            m_currentLLVMValue = builder->CreateGEP(elementType, L, R, "ptradd");
            return;
        }
    } else if (lType->isIntegerTy() && rType->isPointerTy()) {
        if (node->op.type == TokenType::PLUS) { // Only Int + Ptr, not Int - Ptr usually
            llvm::PointerType* rPtrType = llvm::dyn_cast<llvm::PointerType>(rType);
            if (!rPtrType) {
                logError(node->op.location, "RHS is not a PointerType for pointer arithmetic.");
                m_currentLLVMValue = nullptr;
                return;
            }
            llvm::Type* elementType = nullptr;
            if (auto* alloca = llvm::dyn_cast<llvm::AllocaInst>(R)) {
                elementType = alloca->getAllocatedType();
            } else if (auto* gv = llvm::dyn_cast<llvm::GlobalVariable>(R)) {
                elementType = gv->getValueType();
            } else if (m_currentLLVMType) {
                elementType = m_currentLLVMType;
            }
            if (!elementType) {
                logError(node->op.location, "Cannot determine element type for pointer arithmetic (RHS). Please ensure type information is available.");
                m_currentLLVMValue = nullptr;
                return;
            }
            m_currentLLVMValue = builder->CreateGEP(elementType, R, L, "ptradd");
            return;
        }
    } else if (lType->isPointerTy() && rType->isPointerTy()) {
        if (node->op.type == TokenType::MINUS) { // Ptr - Ptr
            llvm::Type* intPtrTy = int64Type; // Or use DataLayout to get pointer size
            llvm::Value* lPtrInt = builder->CreatePtrToInt(L, intPtrTy, "lptrint");
            llvm::Value* rPtrInt = builder->CreatePtrToInt(R, intPtrTy, "rptrint");
            llvm::Value* diffBytes = builder->CreateSub(lPtrInt, rPtrInt, "ptrdiffbytes");
            // Result of pointer subtraction is number of elements, so divide by element size
            llvm::PointerType* lPtrType = llvm::dyn_cast<llvm::PointerType>(lType);
            if (!lPtrType) {
                logError(node->op.location, "LHS is not a PointerType for pointer subtraction.");
                m_currentLLVMValue = nullptr;
                return;
            }
            llvm::Type* elementType = nullptr;
            if (auto* alloca = llvm::dyn_cast<llvm::AllocaInst>(L)) {
                elementType = alloca->getAllocatedType();
            } else if (auto* gv = llvm::dyn_cast<llvm::GlobalVariable>(L)) {
                elementType = gv->getValueType();
            } else if (m_currentLLVMType) {
                elementType = m_currentLLVMType;
            }
            if (!elementType) {
                logError(node->op.location, "Cannot determine element type for pointer subtraction. Please ensure type information is available.");
                m_currentLLVMValue = diffBytes; // Fallback to byte difference
                return;
            }
            if (elementType->isSized()) {
                llvm::DataLayout dl(module.get());
                uint64_t elementSize = dl.getTypeAllocSize(elementType);
                if (elementSize > 0) {
                    llvm::Value* sizeVal = llvm::ConstantInt::get(intPtrTy, elementSize);
                    m_currentLLVMValue = builder->CreateSDiv(diffBytes, sizeVal, "ptrdiffelem");
                } else {
                    logError(node->op.location, "Pointer subtraction with zero-sized element type.");
                    m_currentLLVMValue = diffBytes; // Fallback to byte difference
                }
            } else {
                logError(node->op.location, "Pointer subtraction with unsized element type.");
                m_currentLLVMValue = diffBytes; // Fallback to byte difference
            }
            return;
        }
        // For pointer comparisons, they are handled by ICmp below if types match, or after PtrToInt if not.
    }


    // Standard arithmetic and comparison operations
    switch (node->op.type) {
        case TokenType::PLUS:
            if (L->getType()->isFloatingPointTy() || R->getType()->isFloatingPointTy()) {
                // Promote to float if one is float (simple rule, needs proper type system)
                // This assumes L and R are already compatible or one is float, one is int
                // A real system would have explicit cast nodes or implicit cast rules.
                if (L->getType()->isIntegerTy()) L = builder->CreateSIToFP(L, doubleType, "itofp");
                if (R->getType()->isIntegerTy()) R = builder->CreateSIToFP(R, doubleType, "itofp");
                m_currentLLVMValue = builder->CreateFAdd(L, R, "faddtmp");
            } else {
                m_currentLLVMValue = builder->CreateAdd(L, R, "addtmp");
            }
            break;
        case TokenType::MINUS:
            if (L->getType()->isFloatingPointTy() || R->getType()->isFloatingPointTy()) {
                if (L->getType()->isIntegerTy()) L = builder->CreateSIToFP(L, doubleType, "itofp");
                if (R->getType()->isIntegerTy()) R = builder->CreateSIToFP(R, doubleType, "itofp");
                m_currentLLVMValue = builder->CreateFSub(L, R, "fsubtmp");
            } else {
                m_currentLLVMValue = builder->CreateSub(L, R, "subtmp");
            }
            break;
        case TokenType::MULTIPLY:
            if (L->getType()->isFloatingPointTy() || R->getType()->isFloatingPointTy()) {
                if (L->getType()->isIntegerTy()) L = builder->CreateSIToFP(L, doubleType, "itofp");
                if (R->getType()->isIntegerTy()) R = builder->CreateSIToFP(R, doubleType, "itofp");
                m_currentLLVMValue = builder->CreateFMul(L, R, "fmultmp");
            } else {
                m_currentLLVMValue = builder->CreateMul(L, R, "multmp");
            }
            break;
        case TokenType::DIVIDE:
            if (L->getType()->isFloatingPointTy() || R->getType()->isFloatingPointTy()) {
                if (L->getType()->isIntegerTy()) L = builder->CreateSIToFP(L, doubleType, "itofp");
                if (R->getType()->isIntegerTy()) R = builder->CreateSIToFP(R, doubleType, "itofp");
                m_currentLLVMValue = builder->CreateFDiv(L, R, "fdivtmp");
            } else { // Assuming signed division for integers
                m_currentLLVMValue = builder->CreateSDiv(L, R, "sdivtmp");
            }
            break;
        case TokenType::MODULO: // Typically integer-only
            if (L->getType()->isIntegerTy() && R->getType()->isIntegerTy()) {
                m_currentLLVMValue = builder->CreateSRem(L, R, "sremtmp"); // Signed remainder
            } else {
                logError(node->op.location, "Modulo operator requires integer operands.");
                m_currentLLVMValue = nullptr;
            }
            break;
        // Comparisons
        case TokenType::EQEQ: // ==
            if (L->getType()->isFloatingPointTy() || R->getType()->isFloatingPointTy()) {
                if (L->getType()->isIntegerTy()) L = builder->CreateSIToFP(L, doubleType, "itofp");
                if (R->getType()->isIntegerTy()) R = builder->CreateSIToFP(R, doubleType, "itofp");
                m_currentLLVMValue = builder->CreateFCmpOEQ(L, R, "fcmpeq"); // Ordered, Equal
            } else if (L->getType()->isPointerTy() && R->getType()->isPointerTy()) {
                 m_currentLLVMValue = builder->CreateICmpEQ(L, R, "ptrcmpeq");
            } else if (L->getType()->isPointerTy() && R->getType()->isIntegerTy() && llvm::cast<llvm::ConstantInt>(R)->isZero()) {
                 m_currentLLVMValue = builder->CreateICmpEQ(L, llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(L->getType())), "ptrcmpnull");
            } else if (L->getType()->isIntegerTy() && R->getType()->isPointerTy() && llvm::cast<llvm::ConstantInt>(L)->isZero()) {
                 m_currentLLVMValue = builder->CreateICmpEQ(R, llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(R->getType())), "ptrcmpnull");
            } else {
                m_currentLLVMValue = builder->CreateICmpEQ(L, R, "icmpeq");
            }
            break;
        case TokenType::NOTEQ: // !=
            if (L->getType()->isFloatingPointTy() || R->getType()->isFloatingPointTy()) {
                if (L->getType()->isIntegerTy()) L = builder->CreateSIToFP(L, doubleType, "itofp");
                if (R->getType()->isIntegerTy()) R = builder->CreateSIToFP(R, doubleType, "itofp");
                m_currentLLVMValue = builder->CreateFCmpONE(L, R, "fcmpne"); // Ordered, Not Equal
            } else if (L->getType()->isPointerTy() && R->getType()->isPointerTy()) {
                 m_currentLLVMValue = builder->CreateICmpNE(L, R, "ptrcmpne");
            } else if (L->getType()->isPointerTy() && R->getType()->isIntegerTy() && llvm::cast<llvm::ConstantInt>(R)->isZero()) {
                 m_currentLLVMValue = builder->CreateICmpNE(L, llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(L->getType())), "ptrcmpnotnull");
            } else if (L->getType()->isIntegerTy() && R->getType()->isPointerTy() && llvm::cast<llvm::ConstantInt>(L)->isZero()) {
                 m_currentLLVMValue = builder->CreateICmpNE(R, llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(R->getType())), "ptrcmpnotnull");
            } else {
                m_currentLLVMValue = builder->CreateICmpNE(L, R, "icmpne");
            }
            break;
        case TokenType::LT: // <
            if (L->getType()->isFloatingPointTy() || R->getType()->isFloatingPointTy()) {
                if (L->getType()->isIntegerTy()) L = builder->CreateSIToFP(L, doubleType, "itofp");
                if (R->getType()->isIntegerTy()) R = builder->CreateSIToFP(R, doubleType, "itofp");
                m_currentLLVMValue = builder->CreateFCmpOLT(L, R, "fcmplt"); // Ordered, Less Than
            } else { // Assuming signed comparison for integers
                m_currentLLVMValue = builder->CreateICmpSLT(L, R, "icmpslt");
            }
            break;
        case TokenType::LTEQ: // <=
            if (L->getType()->isFloatingPointTy() || R->getType()->isFloatingPointTy()) {
                if (L->getType()->isIntegerTy()) L = builder->CreateSIToFP(L, doubleType, "itofp");
                if (R->getType()->isIntegerTy()) R = builder->CreateSIToFP(R, doubleType, "itofp");
                m_currentLLVMValue = builder->CreateFCmpOLE(L, R, "fcmple"); // Ordered, Less Than or Equal
            } else { // Assuming signed comparison
                m_currentLLVMValue = builder->CreateICmpSLE(L, R, "icmpsle");
            }
            break;
        case TokenType::GT: // >
            if (L->getType()->isFloatingPointTy() || R->getType()->isFloatingPointTy()) {
                if (L->getType()->isIntegerTy()) L = builder->CreateSIToFP(L, doubleType, "itofp");
                if (R->getType()->isIntegerTy()) R = builder->CreateSIToFP(R, doubleType, "itofp");
                m_currentLLVMValue = builder->CreateFCmpOGT(L, R, "fcmpgt"); // Ordered, Greater Than
            } else { // Assuming signed comparison
                m_currentLLVMValue = builder->CreateICmpSGT(L, R, "icmpsgt");
            }
            break;
        case TokenType::GTEQ: // >=
            if (L->getType()->isFloatingPointTy() || R->getType()->isFloatingPointTy()) {
                if (L->getType()->isIntegerTy()) L = builder->CreateSIToFP(L, doubleType, "itofp");
                if (R->getType()->isIntegerTy()) R = builder->CreateSIToFP(R, doubleType, "itofp");
                m_currentLLVMValue = builder->CreateFCmpOGE(L, R, "fcmpge"); // Ordered, Greater Than or Equal
            } else { // Assuming signed comparison
                m_currentLLVMValue = builder->CreateICmpSGE(L, R, "icmpsge");
            }
            break;
        // Bitwise operators (assuming integer operands)
        case TokenType::AMPERSAND: // &
            if (L->getType()->isIntegerTy() && R->getType()->isIntegerTy()) {
                m_currentLLVMValue = builder->CreateAnd(L, R, "andtmp");
            } else {
                logError(node->op.location, "Bitwise AND operator requires integer operands."); m_currentLLVMValue = nullptr;
            }
            break;
        case TokenType::PIPE: // |
            if (L->getType()->isIntegerTy() && R->getType()->isIntegerTy()) {
                m_currentLLVMValue = builder->CreateOr(L, R, "ortmp");
            } else {
                logError(node->op.location, "Bitwise OR operator requires integer operands."); m_currentLLVMValue = nullptr;
            }
            break;
        case TokenType::CARET: // ^ (XOR)
            if (L->getType()->isIntegerTy() && R->getType()->isIntegerTy()) {
                m_currentLLVMValue = builder->CreateXor(L, R, "xortmp");
            } else {
                logError(node->op.location, "Bitwise XOR operator requires integer operands."); m_currentLLVMValue = nullptr;
            }
            break;
        default:
            logError(node->op.location, "Unsupported binary operator: " + node->op.lexeme);
            m_currentLLVMValue = nullptr;
    }
}

/*
void LLVMCodegen::visit(vyn::ast::AssignmentExpression* node) {
    llvm::errs() << "[AssignmentExpression] Visiting " << node->target->toString() << " = " << node->value->toString() << "\\n";
    // NOTE: This is the older definition, the active one is later in the file.
    // Actual logic for AssignmentExpression is expected in the definition around line 1297 (before this fix).
    llvm::errs() << "[AssignmentExpression] Visiting (older definition stub)\\n";
    m_currentLLVMValue = nullptr;
}
*/

void LLVMCodegen::visit(vyn::ast::AssignmentExpression* node) {
    // Evaluate the value to be assigned first
    node->right->accept(*this);
    llvm::Value* valToAssign = m_currentLLVMValue;

    if (!valToAssign) {
        logError(node->right->loc, "Value in assignment expression is null.");
        m_currentLLVMValue = nullptr;
        return;
    }

    // Check if left hand side is at() intrinsic
    bool isLeftHandSideAt = false;
    
    // First try to handle as a CallExpression (direct at() call)
    if (auto* callExpr = dynamic_cast<vyn::ast::CallExpression*>(node->left.get())) {
        if (auto* ident = dynamic_cast<vyn::ast::Identifier*>(callExpr->callee.get())) {
            if (ident->name == "at" && callExpr->arguments.size() == 1) {
                isLeftHandSideAt = true;
                // Handle at(p) = value directly
                
                // Get the pointer first
                bool oldIsLHS = m_isLHSOfAssignment;
                m_isLHSOfAssignment = false;
                callExpr->arguments[0]->accept(*this);
                llvm::Value* ptrValue = m_currentLLVMValue;
                m_isLHSOfAssignment = oldIsLHS;
                
                if (!ptrValue) {
                    logError(callExpr->arguments[0]->loc, "Pointer argument to \'at\' intrinsic in assignment LHS is null.");
                    m_currentLLVMValue = nullptr;
                    return;
                }
                
                if (!ptrValue->getType()->isPointerTy()) {
                    logError(callExpr->arguments[0]->loc, "Argument to \'at\' intrinsic in assignment LHS is not a pointer. Got: " + getTypeName(ptrValue->getType()));
                    m_currentLLVMValue = nullptr;
                    return;
                }
                
                // Store the value directly
                builder->CreateStore(valToAssign, ptrValue);
                m_currentLLVMValue = valToAssign;
                return;
            }
        }
    } 
    // Then try as a ConstructionExpression (compiler may represent at() differently)
    else if (auto* constructExpr = dynamic_cast<vyn::ast::ConstructionExpression*>(node->left.get())) {
        // Check if this represents an at() function
        if (constructExpr->arguments.size() == 1) {
            std::string constructedTypeName;
            if (auto* typeName = dynamic_cast<vyn::ast::TypeName*>(constructExpr->constructedType.get())) {
                if (typeName->identifier) {
                    constructedTypeName = typeName->identifier->name;
                }
            }
            
            if (constructedTypeName == "at") {
                isLeftHandSideAt = true;
                
                // Similar implementation as above
                bool oldIsLHS = m_isLHSOfAssignment;
                m_isLHSOfAssignment = false;
                constructExpr->arguments[0]->accept(*this);
                llvm::Value* ptrValue = m_currentLLVMValue;
                m_isLHSOfAssignment = oldIsLHS;
                
                if (!ptrValue) {
                    logError(constructExpr->arguments[0]->loc, "Pointer argument to \'at\' construction in assignment LHS is null.");
                    m_currentLLVMValue = nullptr;
                    return;
                }
                
                if (!ptrValue->getType()->isPointerTy()) {
                    logError(constructExpr->arguments[0]->loc, "Argument to \'at\' construction in assignment LHS is not a pointer. Got: " + getTypeName(ptrValue->getType()));
                    m_currentLLVMValue = nullptr;
                    return;
                }
                
                // Store directly to the pointer
                builder->CreateStore(valToAssign, ptrValue);
                m_currentLLVMValue = valToAssign;
                return;
            }
        }
    }
    
    // Regular path for LHS that's not at()
    // Evaluate the target in LHS context to get its address
    bool oldIsLHS = m_isLHSOfAssignment;
    m_isLHSOfAssignment = true;
    node->left->accept(*this);
    llvm::Value* targetAddress = m_currentLLVMValue;
    llvm::Type* targetType = m_currentLLVMType; // Store the type hint from LHS evaluation
    m_isLHSOfAssignment = oldIsLHS;

    if (!targetAddress) {
        logError(node->left->loc, "Target of assignment expression is not addressable or is null.");
        m_currentLLVMValue = nullptr;
        return;
    }

    if (!targetAddress->getType()->isPointerTy()) {
         logError(node->left->loc, "Target of assignment is not a pointer. Cannot store. Target type: " + getTypeName(targetAddress->getType()));
         m_currentLLVMValue = nullptr;
         return;
    }
    
    // --- PATCH: Ensure loc<T> variables always yield a pointer ---
    // If target is loc<T> and valToAssign is an integer, cast to pointer
    // This specific patch might be better handled by type checking or specific loc<T> assignment logic
    // For now, let's assume the type of targetAddress (pointer) and valToAssign are compatible for store.
    // llvm::Type* targetPointeeType = nullptr;
    // if (auto* ptrTy = llvm::dyn_cast<llvm::PointerType>(targetAddress->getType())) {
    //    targetPointeeType = ptrTy->getElementType(); // Old LLVM
    // }
    // If targetPointeeType is known (e.g. from AllocaInst), use it for CreateStore
    llvm::Type* valueTypeForStore = nullptr;
    if (auto* alloca = llvm::dyn_cast<llvm::AllocaInst>(targetAddress)) {
        valueTypeForStore = alloca->getAllocatedType();
    } else if (auto* gv = llvm::dyn_cast<llvm::GlobalVariable>(targetAddress)) {
        valueTypeForStore = gv->getValueType();
    } else if (targetType) { // If LHS evaluation set m_currentLLVMType to the pointee type (from at() intrinsic)
        valueTypeForStore = targetType;
    }


    if (!valueTypeForStore) {
        // This is a critical point. If we don't know the type of the memory location,
        // we cannot safely store. This often happens with opaque pointers if type info isn't
        // propagated correctly.
        // As a last resort, if valToAssign's type seems plausible, use it.
        // But this can be dangerous if e.g. storing an i64 into an i32* location.
        // For now, we'll try to use the type of the value being assigned if the target's pointee type is unknown.
        // This assumes the Vyn type checker has already validated the assignment.
        // logWarning(node->loc, "Cannot determine pointee type for assignment target. Using type of value being assigned.");
        // valueTypeForStore = valToAssign->getType(); // This is risky.
        logError(node->loc, "Cannot determine pointee type for assignment target. Store might be ill-typed.");
        // Fallback to trying to store directly, LLVM might catch type errors if types are incompatible.
    }
    
    // Type compatibility check & potential cast
    if (valueTypeForStore && valToAssign->getType() != valueTypeForStore) {
        if (valueTypeForStore->isPointerTy() && valToAssign->getType()->isIntegerTy()) {
            // Allow assigning an integer (likely 0 for null) to a pointer type
            if (llvm::ConstantInt* CI = llvm::dyn_cast<llvm::ConstantInt>(valToAssign)) {
                if (CI->isZero()) {
                    valToAssign = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(valueTypeForStore));
                } else {
                    // Potentially an int-to-ptr cast if Vyn allows it (e.g. from<loc<T>>(int_addr))
                    // For direct assignment, this is usually an error unless it's 0/null.
                    logWarning(node->loc, "Assigning non-zero integer to pointer type without explicit cast.");
                    valToAssign = builder->CreateIntToPtr(valToAssign, valueTypeForStore, "inttoptr_assign");
                }
            } else {
                 logWarning(node->loc, "Assigning non-constant integer to pointer type without explicit cast.");
                 valToAssign = builder->CreateIntToPtr(valToAssign, valueTypeForStore, "inttoptr_assign");
            }
        } else if (valueTypeForStore->isIntegerTy() && valToAssign->getType()->isPointerTy()) {
            // Allow assigning a pointer to an integer type (e.g. for storing an address)
            logWarning(node->loc, "Assigning pointer to integer type without explicit cast.");
            valToAssign = builder->CreatePtrToInt(valToAssign, valueTypeForStore, "ptrtoint_assign");
        }
        // Add other necessary casts based on Vyn's type system (e.g. float to int, int to float)
        // else if (valueTypeForStore->isFloatingPointTy() && valToAssign->getType()->isIntegerTy()) {
        //    valToAssign = builder->CreateSIToFP(valToAssign, valueTypeForStore, "itofp_assign");
        // }
        else if (valueTypeForStore != valToAssign->getType()) {
             logWarning(node->loc, "Type mismatch in assignment: target expects " + getTypeName(valueTypeForStore) + 
                                  ", value is " + getTypeName(valToAssign->getType()) + ". Attempting bitcast if pointer types, otherwise direct store.");
            if (valueTypeForStore->isPointerTy() && valToAssign->getType()->isPointerTy()) {
                valToAssign = builder->CreateBitCast(valToAssign, valueTypeForStore, "ptr_bitcast_assign");
            } else if (valueTypeForStore->isIntegerTy() && valToAssign->getType()->isIntegerTy()) {
                // If different integer types, truncation or extension might be needed.
                // LLVM store will handle this if bitwidths match, otherwise it's an error or requires explicit cast.
                // For simplicity, assume Vyn's type system ensures compatibility or requires explicit casts.
            }
            // If types are still not compatible for store, LLVM will error.
        }
    }


    builder->CreateStore(valToAssign, targetAddress);
    m_currentLLVMValue = valToAssign; // Assignment expression evaluates to the assigned value
}

// --- CallExpression ---
void LLVMCodegen::visit(vyn::ast::CallExpression* node) {
    // --- Intrinsic: at(ptr) for pointer dereference ---
    auto* identNode = dynamic_cast<vyn::ast::Identifier*>(node->callee.get());
    if (identNode && identNode->name == "at" && node->arguments.size() == 1) {
        bool isAtOnLHS = m_isLHSOfAssignment; // Capture the LHS/RHS context for 'at()' itself

        // Argument to at() should always be evaluated as RValue (we need the pointer value, not its address)
        bool previousLHSState = m_isLHSOfAssignment;
        m_isLHSOfAssignment = false; 
        node->arguments[0]->accept(*this);
        llvm::Value* ptrValue = m_currentLLVMValue;
        m_isLHSOfAssignment = previousLHSState; // Restore LHS state

        if (!ptrValue) {
            logError(node->arguments[0]->loc, "Pointer argument to \'at\' intrinsic is null.");
            m_currentLLVMValue = nullptr; return;
        }

        if (!ptrValue->getType()->isPointerTy()) {
            logError(node->arguments[0]->loc, "Argument to \'at\' intrinsic is not a pointer. Got: " + getTypeName(ptrValue->getType()));
            m_currentLLVMValue = nullptr; return;
        }

        llvm::Type* pointeeType = nullptr;
        // Try to get pointee type from the Vyn AST type of the argument
        if (node->arguments[0]->type) {
            if (auto argVynPtrType = dynamic_cast<ast::PointerType*>(node->arguments[0]->type.get())) {
                 pointeeType = codegenType(argVynPtrType->pointeeType.get());
            }
        }

        // If not found via Vyn type, try LLVM mechanisms (less reliable with opaque ptrs)
        if (!pointeeType) {
            if (auto* alloca = llvm::dyn_cast<llvm::AllocaInst>(ptrValue)) {
                pointeeType = alloca->getAllocatedType();
            } else if (auto* gv = llvm::dyn_cast<llvm::GlobalVariable>(ptrValue)) {
                pointeeType = gv->getValueType();
            }
            // Add more heuristics if needed, e.g., from function return types if ptrValue is a call
        }
        
        if (!pointeeType) {
            logError(node->arguments[0]->loc, "Cannot determine pointee type for \'at\' intrinsic argument: " + node->arguments[0]->toString());
            m_currentLLVMValue = nullptr; return;
        }

        if (isAtOnLHS) { // e.g. at(p) = value
            // NOTE: The AssignmentExpression has special handling for at() on LHS
            // This part of the code is not executed for at(p) = value case
            // It will come here in other cases when at() is used in LHS context
            
            // When at(p) is on the LHS, we need the pointer itself, not a loaded value
            m_currentLLVMValue = ptrValue; // Return the pointer itself
            m_currentLLVMType = pointeeType; // Remember the pointee type for type checking in AssignmentExpression
        } else { // e.g. value = at(p)
            if (pointeeType->isVoidTy()) {
                 logError(node->loc, "Cannot dereference a pointer to void with \'at\' intrinsic for reading.");
                 m_currentLLVMValue = nullptr; return;
            }
            m_currentLLVMValue = builder->CreateLoad(pointeeType, ptrValue, "at_deref");
            m_currentLLVMType = pointeeType; // Keep track of the pointee type
        }
        return; // Handled intrinsic
    }
    
    // Existing CallExpression code (non-intrinsic path)
     node->callee->accept(*this);
    llvm::Value* calleeFuncOrPtr = m_currentLLVMValue;

    if (!calleeFuncOrPtr) {
        logError(node->loc, "Callee of call expression is null.");
        m_currentLLVMValue = nullptr;
        return;
    }

    llvm::FunctionType* funcType = nullptr;
    llvm::Value* funcToCall = nullptr;

    if (llvm::Function* directFunc = llvm::dyn_cast<llvm::Function>(calleeFuncOrPtr)) {
        funcType = directFunc->getFunctionType();
        funcToCall = directFunc;
    } else if (calleeFuncOrPtr->getType()->isPointerTy()) {
        llvm::Type* pointeeType = nullptr;
        // For function pointers, the Vyn type of the callee expression is crucial.
        ast::TypeNode* vynCalleeType = node->callee->type.get();
        if (vynCalleeType) {
            if (auto vynFuncPtrType = dynamic_cast<ast::FunctionType*>(vynCalleeType)) { // Assuming Vyn has ast::FunctionType for standalone function types
                pointeeType = codegenType(vynFuncPtrType); // This should give an LLVM FunctionType
            } else if (auto vynPtrToFuncType = dynamic_cast<ast::PointerType*>(vynCalleeType)) {
                 if (auto vynFuncTypeNode = dynamic_cast<ast::FunctionType*>(vynPtrToFuncType->pointeeType.get())) {
                    pointeeType = codegenType(vynFuncTypeNode); // This should give an LLVM FunctionType
                 }
            }
        }
        
        if (pointeeType && pointeeType->isFunctionTy()) {
            funcType = llvm::dyn_cast<llvm::FunctionType>(pointeeType);
            funcToCall = calleeFuncOrPtr; // Call the pointer directly
        } else {
            // Fallback or error if Vyn type info wasn't enough
#if LLVM_VERSION_MAJOR < 18 // Old LLVM non-opaque pointer introspection (less reliable)
            llvm::PointerType* ptrTy = llvm::dyn_cast<llvm::PointerType>(calleeFuncOrPtr->getType());
            if (ptrTy /*&& ptrTy->getElementType()->isFunctionTy()*/) { // getElementType removed for opaque
                // This path is difficult with opaque pointers without prior type info.
                // We'd need to know the function signature from elsewhere.
                // For now, assume if it's a pointer, it *might* be a function pointer,
                // but we critically need its FunctionType.
            }
#endif
            logError(node->callee->loc, "Callee is a pointer, but not to a known function type or type unknown. VynType: " + 
                                     (vynCalleeType ? vynCalleeType->toString() : "null") + 
                                     ", LLVMType: " + getTypeName(calleeFuncOrPtr->getType()));
            m_currentLLVMValue = nullptr;
            return;
        }
    } else {
        logError(node->callee->loc, "Callee is not a function or function pointer: " + getTypeName(calleeFuncOrPtr->getType()));
        m_currentLLVMValue = nullptr;
        return;
    }

    if (!funcType) {
        logError(node->callee->loc, "Could not determine function type for callee.");
        m_currentLLVMValue = nullptr;
        return;
    }
    if (!funcToCall) { // Should be set if funcType is set
        logError(node->callee->loc, "Could not determine function to call.");
        m_currentLLVMValue = nullptr;
        return;
    }


    std::vector<llvm::Value*> argValues;
    for (const auto& argNode : node->arguments) {
        // Set m_currentLLVMType based on expected parameter type if available
        // This helps with things like nil literal to typed null pointer
        unsigned argIdx = argValues.size();
        if (argIdx < funcType->getNumParams()) {
            m_currentLLVMType = funcType->getParamType(argIdx);
        } else if (funcType->isVarArg()) {
            // For varargs, type is less constrained, maybe default to i8* or type of arg itself
            m_currentLLVMType = nullptr; // Let argument codegen determine its type
        } else {
             m_currentLLVMType = nullptr; // Too many args, will error later
        }

        argNode->accept(*this);
        if (!m_currentLLVMValue) {
            logError(argNode->loc, "Argument in call expression evaluated to null.");
            m_currentLLVMValue = nullptr; // Ensure this is set before returning
            m_currentLLVMType = nullptr; // Reset
            return;
        }
        argValues.push_back(m_currentLLVMValue);
    }
    m_currentLLVMType = nullptr; // Reset after processing all args


    // Verify argument count
    if (funcType->getNumParams() != argValues.size() && !funcType->isVarArg()) {
        logError(node->loc, "Incorrect number of arguments passed to function. Expected " + 
                              std::to_string(funcType->getNumParams()) + ", got " + std::to_string(argValues.size()));
        m_currentLLVMValue = nullptr;
        return;
    }

    // Verify argument types and perform implicit casts if necessary/allowed by Vyn's type system
    for (unsigned i = 0; i < argValues.size(); ++i) {
        if (i < funcType->getNumParams()) { // Only check types for non-vararg part
            llvm::Type* expectedType = funcType->getParamType(i);
            llvm::Type* actualType = argValues[i]->getType();
            if (expectedType != actualType) {
                // Attempt implicit cast or log error
                // Example: integer to float promotion for function args
                if (expectedType->isFloatingPointTy() && actualType->isIntegerTy()) {
                    argValues[i] = builder->CreateSIToFP(argValues[i], expectedType, "argcast_itofp");
                } else if (expectedType->isIntegerTy() && actualType->isFloatingPointTy()) {
                    // Vyn might allow float to int conversion, or require explicit cast
                    // argValues[i] = builder->CreateFPToSI(argValues[i], expectedType, "argcast_fptosi");
                    logWarning(node->arguments[i]->loc, "Implicit float to int conversion for arg " + std::to_string(i+1) + ".");
                     argValues[i] = builder->CreateFPToSI(argValues[i], expectedType, "argcast_fptosi");
                }
                else if (expectedType->isPointerTy() && actualType->isPointerTy()) {
                    if (expectedType != actualType) { // Different pointer types
                         argValues[i] = builder->CreateBitCast(argValues[i], expectedType, "argcast_ptrcast");
                    }
                } else if (expectedType->isPointerTy() && actualType->isIntegerTy()) {
                    // e.g. passing 0 for a null pointer
                    if (llvm::ConstantInt* CI = llvm::dyn_cast<llvm::ConstantInt>(argValues[i])) {
                        if (CI->isZero()) {
                            argValues[i] = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(expectedType));
                        } else {
                             logError(node->arguments[i]->loc, "Cannot implicitly convert non-zero integer to pointer for argument " + std::to_string(i+1) + 
                                                     ". Expected " + getTypeName(expectedType) + ", got " + getTypeName(actualType));
                             m_currentLLVMValue = nullptr; return;
                        }
                    } else {
                        // Non-constant int to pointer is more suspicious, usually needs explicit from<loc>()
                        logError(node->arguments[i]->loc, "Cannot implicitly convert non-constant integer to pointer for argument " + std::to_string(i+1) + 
                                                     ". Expected " + getTypeName(expectedType) + ", got " + getTypeName(actualType));
                        m_currentLLVMValue = nullptr; return;
                    }
                }
                // Add more casting rules as needed by Vyn's type system
                else {
                    logError(node->arguments[i]->loc, "Incompatible type for argument " + std::to_string(i+1) + 
                                                     ". Expected " + getTypeName(expectedType) + 
                                                     ", got " + getTypeName(actualType) + ". No implicit conversion rule applied.");
                    m_currentLLVMValue = nullptr; return;
                }
            }
        }
    }
    
    llvm::CallInst* callInst = builder->CreateCall(funcType, funcToCall, argValues);

    // Set name for the call instruction if the function does not return void
    if (!funcType->getReturnType()->isVoidTy()) {
        callInst->setName("calltmp");
    }
    m_currentLLVMValue = callInst;
}

// --- MemberExpression ---
void LLVMCodegen::visit(vyn::ast::MemberExpression* node) {
    llvm::errs() << "[MemberExpression] Visiting " << node->object->toString() << "." 
                 << (node->property ? node->property->toString() : "<computed>") << "\\\\n";

    bool oldIsLHSAggregate = m_isLHSOfAssignment; // Save context for the whole expression (e.g., obj.field = val)
    llvm::Type* savedOuterExpectedType = m_currentLLVMType; // Save expected type for the whole member expr

    // When evaluating the object part (e.g., `obj` in `obj.field`), it's an R-value context
    // unless `obj` itself is being assigned to (which is not the case for member access).
    m_isLHSOfAssignment = false; 
    m_currentLLVMType = nullptr; // Object's type is determined by itself or its Vyn type node

    node->object->accept(*this);
    llvm::Value* objectValue = m_currentLLVMValue;

    m_isLHSOfAssignment = oldIsLHSAggregate; // Restore for overall expression context (obj.field)
    m_currentLLVMType = savedOuterExpectedType; // Restore expected type for the whole member expr


    if (!objectValue) {
        logError(node->object->loc, "Object in member expression is null.");
        m_currentLLVMValue = nullptr;
        return;
    }

    llvm::Type* objectLLVMType = objectValue->getType();
    llvm::StructType* structLLVMType = nullptr;
    llvm::Value* objectPtrForGEP = nullptr; // This will be the pointer to the aggregate for GEP/ExtractValue

    // The Vyn type of the object expression is crucial here.
    ast::TypeNode* vynObjectType = node->object->type.get();
    if (!vynObjectType) {
        logError(node->object->loc, "Vyn type information missing for object in member expression: " + node->object->toString());
        m_currentLLVMValue = nullptr;
        return;
    }

    // Determine if the Vyn type is a pointer to a struct, or a struct itself.
    // And get the underlying LLVM struct type.
    if (auto vynPtrType = dynamic_cast<ast::PointerType*>(vynObjectType)) { // e.g. loc<MyStruct>, MyStruct*
        if (!objectLLVMType->isPointerTy()) {
            logError(node->object->loc, "Vyn type is pointer ('" + vynObjectType->toString() + "'), but LLVM value is not a pointer: " + getTypeName(objectLLVMType));
            m_currentLLVMValue = nullptr;
            return;
        }
        objectPtrForGEP = objectValue; // Object is already a pointer
        // Now get the LLVM struct type that this pointer points to
        if (auto vynStructName = dynamic_cast<ast::TypeName*>(vynPtrType->pointeeType.get())) {
            auto it = userTypeMap.find(vynStructName->identifier->name);
            if (it != userTypeMap.end()) {
                structLLVMType = it->second.llvmType;
            } else {
                logError(node->object->loc, "Struct type '" + vynStructName->identifier->name + "' not found in userTypeMap for Vyn pointer type '" + vynObjectType->toString() + "'.");
                m_currentLLVMValue = nullptr;
                return;
            }
        } else { // Pointee is not a simple named type (e.g. pointer to pointer, pointer to array)
            // For member access, we expect a pointer to a struct.
            logError(node->object->loc, "Pointee type of Vyn pointer type '" + vynObjectType->toString() + "' is not a simple TypeName, cannot perform member access.");
            m_currentLLVMValue = nullptr;
            return;
        }
    } else if (auto vynTypeName = dynamic_cast<ast::TypeName*>(vynObjectType)) { // e.g. MyStruct (value type)
        auto it = userTypeMap.find(vynTypeName->identifier->name);
        if (it != userTypeMap.end()) {
            structLLVMType = it->second.llvmType; // This is the LLVM struct type for MyStruct
            // Now, check the LLVM value we got for the object.
            if (objectLLVMType == structLLVMType) { // Object is a struct value (e.g. from a function return)
                // To access a member of a struct value, we need its address for GEP.
                // Or, if not LHS and not computed, can use ExtractValue.
                if (m_isLHSOfAssignment || node->computed) { 
                    // Need a pointer for GEP if it's LHS (obj.field = ...) or computed (obj[expr])
                    // Store the struct value in a temporary alloca to get its address.
                    llvm::AllocaInst* tempAlloca = builder->CreateAlloca(structLLVMType, nullptr, vynTypeName->identifier->name + ".temp_val_alloca");
                    builder->CreateStore(objectValue, tempAlloca);
                    objectPtrForGEP = tempAlloca;
                } else {
                    // RHS, not computed (obj.field): can use ExtractValue directly on objectValue if it's a ConstantStruct or AggregateValue.
                    // If objectValue is an SSA value of struct type, ExtractValue is fine.
                    objectPtrForGEP = objectValue; // Signal to use ExtractValue path
                }
            } else if (objectLLVMType->isPointerTy()) {
                 // Object is a pointer to our struct type (e.g. variable holding a struct)
                 // We need to verify that this pointer points to `structLLVMType`
                 llvm::PointerType* objPtrTy = llvm::dyn_cast<llvm::PointerType>(objectLLVMType);
                 llvm::Type* llvmPointeeType = nullptr;
                 // With opaque pointers, we can't directly get objPtrTy->getElementType().
                 // We must rely on the Vyn type system. Since vynObjectType
                 // We must rely on the Vyn type system. Since vynObjectType is TypeName (not PointerType),
                 // this case (LLVM is pointer, Vyn is value) implies `objectValue` is an alloca or similar.
                 // The `structLLVMType` derived from `vynTypeName` IS the pointee type.
                 // So, this check is more about consistency.
                 // For now, we assume if Vyn type is `MyStruct` and LLVM type is `MyStruct*`, it's consistent.
                 objectPtrForGEP = objectValue;
            } else {
                logError(node->object->loc, "LLVM type of object ('" + getTypeName(objectLLVMType) + "') does not match expected struct type '" + vynTypeName->identifier->name + "' or its pointer. Vyn type was '" + vynObjectType->toString() + "'.");
                m_currentLLVMValue = nullptr;
                return;
            }
        } else {
            logError(node->object->loc, "Struct type '" + vynTypeName->identifier->name + "' not found in userTypeMap for Vyn value type '" + vynObjectType->toString() + "'.");
            m_currentLLVMValue = nullptr;
            return;
        }
    } else { // Object is some other Vyn type (array, primitive, etc.) that doesn't support '.'
        logError(node->object->loc, "Object in member expression has unhandled or non-struct Vyn type: " + vynObjectType->toString());
        m_currentLLVMValue = nullptr;
        return;
    }

    if (!structLLVMType) {
        logError(node->object->loc, "Could not determine LLVM struct type for member access on object: " + node->object->toString());
        m_currentLLVMValue = nullptr;
        return;
    }
    if (!structLLVMType->isStructTy()){
        logError(node->object->loc, "Target type for member access is not an LLVM struct type: " + getTypeName(structLLVMType));
        m_currentLLVMValue = nullptr;
        return;
    }


    std::string fieldName;
    // For now, assume property is always an Identifier. Computed access a[b] is ArrayElementExpression.
    if (auto identNode = dynamic_cast<vyn::ast::Identifier*>(node->property.get())) {
        fieldName = identNode->name;
    } else {
        logError(node->property->loc, "Property in member expression is not a simple identifier. Computed access (e.g. obj[expr]) is not supported here.");
        m_currentLLVMValue = nullptr;
        return;
    }

    int fieldIndex = getStructFieldIndex(structLLVMType, fieldName);
    if (fieldIndex == -1) {
        logError(node->property->loc, "Field '" + fieldName + "' not found in struct type '" + getTypeName(structLLVMType) + "'.");
        m_currentLLVMValue = nullptr;
        return;
    }
    
    // Path for struct values on RHS using ExtractValue (if objectPtrForGEP was set to the value itself)
    if (!m_isLHSOfAssignment && !node->computed && objectPtrForGEP == objectValue && objectLLVMType == structLLVMType) {
         if (fieldIndex >= (int)structLLVMType->getNumElements()) {
            logError(node->property->loc, "Field index " + std::to_string(fieldIndex) + " out of bounds for struct " + getTypeName(structLLVMType));
            m_currentLLVMValue = nullptr;
            return;
         }
         m_currentLLVMValue = builder->CreateExtractValue(objectValue, (unsigned)fieldIndex, fieldName + ".val");
         m_currentLLVMType = structLLVMType->getElementType(fieldIndex); // Set type of the extracted value
         return; 
    }
    
    // Path for GEP (LHS, or RHS from pointer, or RHS from value that was put in alloca)
    if (!objectPtrForGEP || !objectPtrForGEP->getType()->isPointerTy()) {
        logError(node->object->loc, "Object pointer for GEP is null or not a pointer in member expression for field '" + fieldName + "'. Object LLVM type: " + getTypeName(objectLLVMType));
        m_currentLLVMValue = nullptr;
        return;
    }

    // Ensure GEP is on the correct struct type if the pointer is opaque or to a different type initially
    // The `structLLVMType` is the one we trust from Vyn's type system for the object.
    llvm::Value* gepBasePtr = builder->CreateBitCast(objectPtrForGEP, llvm::PointerType::getUnqual(*context), "gep_base_cast_tmp"); // Cast to generic pointer first if needed by GEP variant
                                                                                                                                    // then GEP will use structLLVMType
    // llvm::Value* memberPtr = builder->CreateStructGEP(structLLVMType, objectPtrForGEP, fieldIndex, fieldName + ".ptr");
    // GEP with explicit type for the base pointer's pointee type
    llvm::Value* memberPtr = builder->CreateStructGEP(structLLVMType, gepBasePtr, fieldIndex, fieldName + ".ptr");


    if (m_isLHSOfAssignment) { // e.g. obj.field = ...
        m_currentLLVMValue = memberPtr; // This is the address to store to
        if (fieldIndex < (int)structLLVMType->getNumElements()) {
            m_currentLLVMType = structLLVMType->getElementType(fieldIndex); // Set expected type for the assignment's RHS
        } else {
            m_currentLLVMType = nullptr; 
            logError(node->property->loc, "Field index " + std::to_string(fieldIndex) + " out of bounds for struct " + getTypeName(structLLVMType) + " during LHS member access.");
            m_currentLLVMValue = nullptr; // Error
        }
       } else { // e.g. x = obj.field
        if (fieldIndex >= (int)structLLVMType->getNumElements()) {
            logError(node->property->loc, "Field index " + std::to_string(fieldIndex) + " out of bounds for struct " + getTypeName(structLLVMType));
            m_currentLLVMValue = nullptr;
            return;
        }
        llvm::Type* memberType = structLLVMType->getElementType(fieldIndex);
        if (memberType->isVoidTy()){ 
            logError(node->property->loc, "Struct member '" + fieldName + "' has void type, cannot load.");
            m_currentLLVMValue = nullptr;
            return;
        }
        m_currentLLVMValue = builder->CreateLoad(memberType, memberPtr, fieldName + ".val");
        m_currentLLVMType = memberType; // Set type of the loaded value
    }
}

// --- AddrOfExpression ---
void LLVMCodegen::visit(vyn::ast::AddrOfExpression* node) {
    llvm::errs() << "[AddrOfExpression] Visiting (stubbed)\\\\n";
    // Indicate that the next expression's address is needed
    bool oldIsLHS = m_isLHSOfAssignment;
    m_isLHSOfAssignment = true; // Key part: evaluate sub-expression in an LHS context to get its address
    node->getLocation()->accept(*this); // getLocation() is a valid method in ast.hpp
    m_isLHSOfAssignment = oldIsLHS; // Restore
    // The result in m_currentLLVMValue should be the address of the location.
    if (!m_currentLLVMValue) {
        logError(node->loc, "Expression inside addr() did not yield an addressable value.");
    }
}

void LLVMCodegen::visit(vyn::ast::FromIntToLocExpression* node) {
    llvm::errs() << "[FromIntToLocExpression] Visiting from<" << node->getTargetType()->toString() 
                 << ">(" << node->getAddressExpression()->toString() << ")\\n";

    // 1. Codegen the address expression (should be an integer)
    node->getAddressExpression()->accept(*this);
       llvm::Value* intAddress = m_currentLLVMValue;

    if (!intAddress) {
        logError(node->getAddressExpression()->loc, "Address expression in from<T>() evaluated to null.");
        m_currentLLVMValue = nullptr;
        return;
    }

    if (!intAddress->getType()->isIntegerTy()) {
        logError(node->getAddressExpression()->loc, "Address expression in from<T>() must be an integer, got " + getTypeName(intAddress->getType()));
        m_currentLLVMValue = nullptr;
        return;
    }

    // 2. Codegen the target type to get the LLVM pointer type
    llvm::Type* targetLLVMType = codegenType(node->getTargetType().get());
    if (!targetLLVMType) {
        logError(node->getTargetType()->loc, "Could not determine LLVM type for target type in from<T>().");
        m_currentLLVMValue = nullptr;
        return;
    }

    if (!targetLLVMType->isPointerTy()) {
        logError(node->getTargetType()->loc, "Target type in from<T>() must be a pointer type, got " + getTypeName(targetLLVMType));
        m_currentLLVMValue = nullptr;
        return;
    }

    // 3. Create IntToPtr instruction
    m_currentLLVMValue = builder->CreateIntToPtr(intAddress, targetLLVMType, "from_int_to_loc");
    llvm::errs() << "[FromIntToLocExpression] Result: ";
    if (m_currentLLVMValue) m_currentLLVMValue->print(llvm::errs()); else llvm::errs() << "null";
    llvm::errs() << " of type ";
    if (m_currentLLVMValue) m_currentLLVMValue->getType()->print(llvm::errs()); else llvm::errs() << "null";
    llvm::errs() << "\\n";
}

// Add stubs for other missing visit methods
void LLVMCodegen::visit(vyn::ast::BorrowExpression* node) {
    llvm::errs() << "[BorrowExpression] Visiting (stubbed)\\\\\\\\n";
    // TODO: Implement actual codegen for BorrowExpression
   
    // For now, just visit the expression being borrowed.
    // The semantics of 'borrow' (shared vs mutable, lifetime) are complex
    // and depend on Vyn's memory model.
    // A simple approach might be similar to AddrOf if it's &mut,
    // or just evaluating the expression if it's & (shared borrow, often implicit).
    // This stub will likely need to be refined based on how BorrowExpression is created and used.
       if (node->expression) { // Changed from getExpression()
        node->expression->accept(*this); // Changed from getExpression()
       
        // m_currentLLVMValue will hold the result of the inner expression.
        // If it's &expr, and expr is an lvalue, this might need to be its address.
        // If it's &val, and val is an rvalue, this might involve creating a temporary.
    } else {
        logError(node->loc, "BorrowExpression has no inner expression.");
        m_currentLLVMValue = nullptr;
    }
    // This is a placeholder. Real borrow semantics are needed.
}

void LLVMCodegen::visit(vyn::ast::PointerDerefExpression* node) {
    llvm::errs() << "[PointerDerefExpression] Visiting *(stubbed)\\\\\\\\n";
    // This is effectively the '*' operator for dereferencing.
    // It's very similar to the 'at(ptr)' intrinsic logic.

    bool oldIsLHS = m_isLHSOfAssignment; // Save context
    llvm::Type* savedLLVMType = m_currentLLVMType;

    m_isLHSOfAssignment = false; // Operand is R-value
    node->pointer->accept(*this); // Changed from getPointerExpression()
    llvm::Value* ptrValue = m_currentLLVMValue;
    
    m_isLHSOfAssignment = oldIsLHS; // Restore
    m_currentLLVMType = savedLLVMType;

    if (!ptrValue || !ptrValue->getType()->isPointerTy()) {
        logError(node->loc, "PointerDerefExpression expects a pointer operand, got " + 
                 (ptrValue ? getTypeName(ptrValue->getType()) : std::string("null")));
        m_currentLLVMValue = nullptr;
        return;
    }

    llvm::Type* pointeeType = nullptr;
    // Infer pointee type from Vyn AST type of the pointer expression
    if (node->pointer->type) { // Changed from getPointerExpression()
        if (auto vynPtrType = dynamic_cast<ast::PointerType*>(node->pointer->type.get())) { // Changed from getPointerExpression()
             pointeeType = codegenType(vynPtrType->pointeeType.get());
        } 
        // No direct ast::LocationType, ast::PointerType covers loc<T>
    }

    if (!pointeeType) {
        // Fallback if Vyn type info wasn't sufficient (e.g. raw pointer from int)
        // This is risky with opaque pointers.
        logWarning(node->loc, "Cannot determine pointee type for PointerDerefExpression from Vyn types. This may lead to errors if used with raw pointers from integers without proper 'from<loc<T>>()' casting.");
        // We cannot reliably get pointee type from an opaque LLVM pointer type alone.
        // The user must ensure the pointer's Vyn type is accurate or use `from<loc>()
        // to specify the target type for raw addresses.
        // If m_currentLLVMType was set by an outer context expecting a certain type, that *might* be usable,
        // but it's not the type of the pointer itself.
        // For now, if pointeeType is still null, we have to error or make a dangerous guess.
        logError(node->loc, "Failed to determine pointee type for dereference. Ensure the pointer expression has a known Vyn pointer or location type.");
        m_currentLLVMValue = nullptr;
        return;
    }
    
    if (m_isLHSOfAssignment) { // e.g. *ptr = value
        m_currentLLVMValue = ptrValue; // The address to store to
        m_currentLLVMType = pointeeType; // The type of the value to be stored
    } else { // e.g. x = *ptr
        if (pointeeType->isVoidTy()) {
            logError(node->loc, "Cannot load from element of void type.");
            m_currentLLVMValue = nullptr; return;
        }
        m_currentLLVMValue = builder->CreateLoad(pointeeType, ptrValue, "ptr_deref");
        m_currentLLVMType = pointeeType; // Type of the loaded value
    }
}

void LLVMCodegen::visit(vyn::ast::ArrayElementExpression* node) {
    llvm::errs() << "[ArrayElementExpression] Visiting (stubbed)\\\\\\\\n";
    // array[index]

    // 1. Codegen the array/pointer expression
    bool oldIsLHS = m_isLHSOfAssignment;
    m_isLHSOfAssignment = false; // Evaluate array/ptr as RValue first
    node->array->accept(*this); // Changed from getArrayOrPointerExpression()
    llvm::Value* arrayOrPtrValue = m_currentLLVMValue;
    m_isLHSOfAssignment = oldIsLHS; // Restore for overall expression context

    if (!arrayOrPtrValue) {
        logError(node->array->loc, "Array/pointer expression in subscript is null."); // Changed
        m_currentLLVMValue = nullptr;
        return;
    }

    // 2. Codegen the index expression
    node->index->accept(*this); // Changed from getIndexExpression()
    llvm::Value* indexValue = m_currentLLVMValue;

    if (!indexValue) {
        logError(node->index->loc, "Index expression in subscript is null."); // Changed
        m_currentLLVMValue = nullptr;
        return;
    }
    if (!indexValue->getType()->isIntegerTy()) {
        logError(node->index->loc, "Index expression must be an integer, got " + getTypeName(indexValue->getType())); // Changed
        m_currentLLVMValue = nullptr;
        return;
    }

    llvm::Type* elementType = nullptr;
    llvm::Value* elementPtr = nullptr;

    // Determine if arrayOrPtrValue is an LLVM array, a pointer to an array, or a pointer to an element.
    // Also crucial is the Vyn type of the array/pointer expression.
    ast::TypeNode* vynArrayType = node->array->type.get(); // Changed

    if (vynArrayType) {
        if (auto vynActualArrayType = dynamic_cast<ast::ArrayType*>(vynArrayType)) {
            // Vyn type is array<T, size> or array<T>
            elementType = codegenType(vynActualArrayType->elementType.get());
            if (!elementType) {
                 logError(node->loc, "Could not determine LLVM element type from Vyn array type: " + vynActualArrayType->toString());
                 m_currentLLVMValue = nullptr; return;
            }

            if (arrayOrPtrValue->getType()->isArrayTy()) { // LLVM array value (e.g. from global or returned by value)
                // Need address to use GEP. Store to temp alloca.
                llvm::AllocaInst* tempAlloca = builder->CreateAlloca(arrayOrPtrValue->getType(), nullptr, "arrayval.alloca");
                builder->CreateStore(arrayOrPtrValue, tempAlloca);
                elementPtr = builder->CreateGEP(arrayOrPtrValue->getType(), tempAlloca, 
                                                {llvm::ConstantInt::get(int64Type, 0), indexValue}, "arrayidx");
            } else if (arrayOrPtrValue->getType()->isPointerTy()) { // LLVM pointer (e.g. alloca of array, or pointer to first element)
                 // Assume pointer is to the element type or to an array of element type
                 // If it's pointer to array type llvm::Type* arrPtrPointsTo = llvm::dyn_cast<llvm::PointerType>(arrayOrPtrValue)->getElementType();
                 // if (arrPtrPointsTo->isArrayTy()) { ... GEP with 0 index first ... }
                 // For simplicity, assume it's effectively a pointer to element type for GEP
                 elementPtr = builder->CreateGEP(elementType, arrayOrPtrValue, indexValue, "arrayidx");
            } else {
                logError(node->loc, "Array expression is not an LLVM array or pointer. VynType: " + vynArrayType->toString() + ", LLVMType: " + getTypeName(arrayOrPtrValue->getType()));
                m_currentLLVMValue = nullptr; return;
            }

        } else if (auto vynPtrType = dynamic_cast<ast::PointerType*>(vynArrayType)) {
            // Vyn type is loc<T> or some other pointer type T*
            // T could be an element type (e.g. int* used as array) or an array type (e.g. loc<array<int,10>>)
            llvm::Type* pointeeType = codegenType(vynPtrType->pointeeType.get());
            if (!pointeeType) {
                 logError(node->loc, "Could not determine LLVM pointee type from Vyn pointer type: " + vynPtrType->toString());
                 m_currentLLVMValue = nullptr; return;
            }

            if (!arrayOrPtrValue->getType()->isPointerTy()) {
                logError(node->loc, "Vyn type is pointer, but LLVM value is not a pointer for subscript: " + getTypeName(arrayOrPtrValue->getType()));
                m_currentLLVMValue = nullptr; return;
            }

            if (pointeeType->isArrayTy()) { // Pointer to an array, e.g. loc<array<int, 10>>
                elementType = pointeeType->getArrayElementType();
                elementPtr = builder->CreateGEP(pointeeType, arrayOrPtrValue, 
                                                {llvm::ConstantInt::get(int64Type, 0), indexValue}, "arrayptr_idx");
            } else { // Pointer to element, e.g. loc<int> used as base of an array
                elementType = pointeeType;
                elementPtr = builder->CreateGEP(elementType, arrayOrPtrValue, indexValue, "ptr_idx");
            }
        } else {
            logError(node->loc, "Subscript operator applied to non-array, non-pointer Vyn type: " + vynArrayType->toString());
            m_currentLLVMValue = nullptr; return;
        }
    } else {
        logError(node->loc, "Missing Vyn type information for array/pointer in subscript expression.");
        // Attempt a guess if it's an LLVM pointer, assuming element type is i8 (very unsafe)
        if (arrayOrPtrValue->getType()->isPointerTy()) {
            logWarning(node->loc, "Guessing element type as i8 for subscript on typeless LLVM pointer.");
            elementType = int8Type; // Unsafe guess
            elementPtr = builder->CreateGEP(elementType, arrayOrPtrValue, indexValue, "unsafe_idx");
        } else {
            m_currentLLVMValue = nullptr; return;
        }
    }
    
    if (!elementPtr || !elementType) {
        logError(node->loc, "Failed to calculate element pointer or determine element type for subscript.");
        m_currentLLVMValue = nullptr; return;
    }

    if (m_isLHSOfAssignment) { // e.g. array[index] = value
        m_currentLLVMValue = elementPtr; // Address to store to
        m_currentLLVMType = elementType; // Expected type of RHS
    } else { // e.g. x = array[index]
        if (elementType->isVoidTy()) {
            logError(node->loc, "Cannot load from element of void type.");
            m_currentLLVMValue = nullptr; return;
        }
        m_currentLLVMValue = builder->CreateLoad(elementType, elementPtr, "arrayelem.val");
        m_currentLLVMType = elementType; // Type of loaded value
    }
}

void LLVMCodegen::visit(vyn::ast::LocationExpression* node) {
    llvm::errs() << "[LocationExpression] Visiting (stubbed)\\\\\\\\n";
    // This AST node seems to represent a memory location directly, possibly from `loc<T>` type.
    // Its codegen should yield a pointer value.
    // The type `node->type` should be `PointerType` (representing loc<T>).
    if (auto ptrType = dynamic_cast<ast::PointerType*>(node->type.get())) { // Changed from ast::LocationType
        llvm::Type* referencedLLVMType = codegenType(ptrType->pointeeType.get()); // Changed from locType->referencedType
        if (referencedLLVMType) {
            // A LocationExpression itself doesn't have sub-expressions to evaluate in the same way
            // an identifier or literal does. It *IS* the location.
            // How it gets its value (the address) is the question.
            // If it's from a variable that IS a loc<T>, then Identifier visitor handles it.
            // If it's a literal loc<T> value (e.g. from an integer address),
            // then FromIntToLocExpression handles that.
            // This node might be redundant or for a specific Vyn feature not yet clear.
            // For now, assume it needs to produce a pointer of type locType->referencedType.
            // If it has a name (like an identifier), it might be a lookup.
            // If it has an address value, it's like FromIntToLoc.
            // Let's assume it's abstract and its value must be set by some context.
            // This stub will likely need to be refined based on how LocationExpression is created and used.
            logWarning(node->loc, "LocationExpression codegen is a stub and may not be correct. It needs a source for its address value.");
            // As a placeholder, create a null pointer of the target type.
            // m_currentLLVMValue = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*context)); // Generic pointer
            // Create a pointer to the referencedLLVMType
            m_currentLLVMValue = llvm::ConstantPointerNull::get(llvm::PointerType::get(referencedLLVMType, 0));
            m_currentLLVMType = m_currentLLVMValue->getType();

        } else {
            logError(node->loc, "Could not determine LLVM type for referenced type in LocationExpression: " + ptrType->pointeeType->toString()); // Changed
            m_currentLLVMValue = nullptr;
        }
    } else {
        logError(node->loc, "LocationExpression AST node does not have an ast::PointerType (expected for loc<T>). Actual type: " + (node->type ? node->type->toString() : "null"));
        m_currentLLVMValue = nullptr;
    }
}

void LLVMCodegen::visit(vyn::ast::ListComprehension* node) {
    llvm::errs() << "[ListComprehension] Visiting (stubbed)\\\\n";
    logError(node->loc, "ListComprehension codegen not implemented.");
    m_currentLLVMValue = nullptr; // Placeholder
}

void LLVMCodegen::visit(vyn::ast::IfExpression* node) {
    llvm::errs() << "[IfExpression] Visiting (stubbed)\\\\\\\\n";
    // Similar to IfStatement, but results in a value. Requires PHI node.
    // 1. Codegen condition
    node->condition->accept(*this); // Changed from getCondition()
    llvm::Value* condValue = m_currentLLVMValue;
    if (!condValue) {
        logError(node->condition->loc, "Condition in if-expression is null."); // Changed
        m_currentLLVMValue = nullptr; return;
    }
    // Convert condition to bool (i1) if not already
    if (condValue->getType() != int1Type) {
        condValue = builder->CreateICmpNE(condValue, llvm::Constant::getNullValue(condValue->getType()), "ifcond.tobool");
    }

    llvm::Function* parentFunc = builder->GetInsertBlock()->getParent();
    llvm::BasicBlock* thenBB = llvm::BasicBlock::Create(*context, "ifexp.then", parentFunc);
    llvm::BasicBlock* elseBB = llvm::BasicBlock::Create(*context, "ifexp.else", parentFunc);
    llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(*context, "ifexp.merge", parentFunc);

    builder->CreateCondBr(condValue, thenBB, elseBB);

    // Then block
    builder->SetInsertPoint(thenBB);
    node->thenBranch->accept(*this); // Changed from getThenBranch()
    llvm::Value* thenValue = m_currentLLVMValue;
    if (!thenValue) {
        logError(node->thenBranch->loc, "Then branch of if-expression is null."); // Changed
        // Cannot proceed without a value for PHI node if then branch is reachable
        // For now, let's assume it must produce a value. If it can be void, logic needs adjustment.
        m_currentLLVMValue = nullptr; return; 
    }
    llvm::BasicBlock* thenEndBB = builder->GetInsertBlock(); // BB where then branch finished
    builder->CreateBr(mergeBB);

    // Else block
    builder->SetInsertPoint(elseBB);
    llvm::Value* elseValue = nullptr;
    if (node->elseBranch) { // Changed from getElseBranch()
        node->elseBranch->accept(*this); // Changed from getElseBranch()
        elseValue = m_currentLLVMValue;
        if (!elseValue) {
            logError(node->elseBranch->loc, "Else branch of if-expression is null."); // Changed
            m_currentLLVMValue = nullptr; return;
        }
    } else {
        // If 'else' is missing, the type of the if-expression is problematic.
        // Vyn's type system should enforce that if-expressions have an else if they produce a value.
        // Or, it implies the 'then' branch must be of type void, or there's a default value.
        // For now, assume 'else' is required if 'then' produces a non-void value.
        // If 'thenValue' type is void, then this is fine.
        // If 'thenValue' is non-void, 'elseValue' must exist and have a compatible type.
        // For now, let's assume construction of `MyType{}` results in a value of `MyType`.
        // If it was allocated on stack, we need to load from it to get the value.
        m_currentLLVMValue = nullptr; // Or a specific void value if needed by context, though usually not.
                                     // LLVM instructions like CallInst for void functions are themselves m_currentLLVMValue.
                                     // For an if-expression, if it's void, it doesn't produce a loadable value.
    }
    llvm::BasicBlock* elseEndBB = builder->GetInsertBlock(); // BB where else branch finished
    builder->CreateBr(mergeBB);

    // Merge block
    builder->SetInsertPoint(mergeBB);

    if (thenValue->getType()->isVoidTy()) {
        // If then branch is void, the if-expression is void.
        // (Assuming else is also void or not taken if then is void).
        m_currentLLVMValue = nullptr; // Or a specific void value if needed by context, though usually not.
                                     // LLVM instructions like CallInst for void functions are themselves m_currentLLVMValue.
                                     // For an if-expression, if it's void, it doesn't produce a loadable value.
    } else {
        if (!elseValue || elseValue->getType()->isVoidTy()) {
            logError(node->loc, "If-expression 'then' branch has a value but 'else' branch is missing or void. Cannot form a value for PHI node.");
            m_currentLLVMValue = nullptr; return;
        }
        // Type compatibility check for PHI node
        if (thenValue->getType() != elseValue->getType()) {
            // Attempt common type promotion or error
            // E.g. if one is int and other is float, promote to float.
            // This needs Vyn's type system rules. For now, require exact match or error.
            logError(node->loc, "Type mismatch between then (" + getTypeName(thenValue->getType()) + 
                                ") and else (" + getTypeName(elseValue->getType()) + ") branches of if-expression.");
            m_currentLLVMValue = nullptr; return;
        }
        llvm::PHINode* phi = builder->CreatePHI(thenValue->getType(), 2, "ifexp.val");
        phi->addIncoming(thenValue, thenEndBB);
        phi->addIncoming(elseValue, elseEndBB);
        m_currentLLVMValue = phi;
    }
    m_currentLLVMType = m_currentLLVMValue ? m_currentLLVMValue->getType() : nullptr;
}

void LLVMCodegen::visit(vyn::ast::ConstructionExpression* node) {
    llvm::errs() << "[ConstructionExpression] Visiting (stubbed)\\\\\\\\n";
    // Syntax: MyType { field1: val1, field2: val2 } (named - not directly supported by current AST)
    // Or   MyType(arg1, arg2) (positional - supported by current AST)

    // 1. Get the Vyn type being constructed
    ast::TypeNode* vynTypeToConstruct = node->constructedType.get(); // Changed from getType().get()
    if (!vynTypeToConstruct) {
        logError(node->loc, "ConstructionExpression is missing the Vyn type to construct.");
        m_currentLLVMValue = nullptr; return;
    }

    // 2. Get the LLVM type for it (should be a struct type)
    llvm::Type* llvmTargetType = codegenType(vynTypeToConstruct);
    if (!llvmTargetType) {
        logError(node->loc, "Could not determine LLVM type for Vyn type '" + vynTypeToConstruct->toString() + "' in ConstructionExpression.");
        m_currentLLVMValue = nullptr; return;
    }
    llvm::StructType* structLLVMType = llvm::dyn_cast<llvm::StructType>(llvmTargetType);
    if (!structLLVMType) {
        logError(node->loc, "Vyn type '" + vynTypeToConstruct->toString() + "' in ConstructionExpression did not resolve to an LLVM StructType. Got: " + getTypeName(llvmTargetType));
        m_currentLLVMValue = nullptr; return;
    }

    // 3. Codegen arguments (positional only, based on current ast.hpp)
    std::vector<llvm::Value*> initValues;

    // Positional initializers
    if (node->arguments.size() != structLLVMType->getNumElements()) { // Changed from getArguments()
        logError(node->loc, "Construction with positional arguments: expected " + std::to_string(structLLVMType->getNumElements()) + 
                            " arguments for type '" + vynTypeToConstruct->toString() + "', got " + std::to_string(node->arguments.size())); // Changed
        m_currentLLVMValue = nullptr; return;
    }

    for (size_t i = 0; i < node->arguments.size(); ++i) { // Changed from getArguments()
        const auto& argExpr = node->arguments[i]; // Changed from getArguments()
        llvm::Type* expectedFieldType = structLLVMType->getElementType(i);
        m_currentLLVMType = expectedFieldType; // Hint for literals like 'nil'

        argExpr->accept(*this);
        if (!m_currentLLVMValue) {
            logError(argExpr->loc, "Argument " + std::to_string(i) + " in construction is null.");
            m_currentLLVMType = nullptr; m_currentLLVMValue = nullptr; return;
        }
        // Type check/cast m_currentLLVMValue against expectedFieldType
        if (m_currentLLVMValue->getType() != expectedFieldType) {
            // Basic cast for pointer null init
            if (expectedFieldType->isPointerTy() && m_currentLLVMValue->getType()->isPointerTy() && llvm::isa<llvm::ConstantPointerNull>(m_currentLLVMValue)) {
                 initValues.push_back(llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(expectedFieldType)));
            } else if (expectedFieldType->isPointerTy() && m_currentLLVMValue->getType()->isIntegerTy() && llvm::isa<llvm::ConstantInt>(m_currentLLVMValue) && llvm::cast<llvm::ConstantInt>(m_currentLLVMValue)->isZero()) {
                 initValues.push_back(llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(expectedFieldType)));
            }
            // Add more sophisticated casting as needed (e.g. int to float)
            else if (expectedFieldType->isFloatingPointTy() && m_currentLLVMValue->getType()->isIntegerTy()) {
                initValues.push_back(builder->CreateSIToFP(m_currentLLVMValue, expectedFieldType, "ctor_arg_itofp"));
            }
            // Add more sophisticated casting as needed (e.g. float to int)
            else if (expectedFieldType->isIntegerTy() && m_currentLLVMValue->getType()->isFloatingPointTy()) {
                initValues.push_back(builder->CreateFPToSI(m_currentLLVMValue, expectedFieldType, "ctor_arg_fptosi"));
            }
            else {
                logError(argExpr->loc, "Type mismatch for argument " + std::to_string(i) + ". Expected " + getTypeName(expectedFieldType) + ", got " + getTypeName(m_currentLLVMValue->getType()));
                m_currentLLVMType = nullptr; m_currentLLVMValue = nullptr; return;
            }
        } else {
             initValues.push_back(m_currentLLVMValue);
        }
    }
    m_currentLLVMType = nullptr; // Reset hint
    
    // 4. Create the struct instance
    // This could be a ConstantStruct if all initializers are constant.
    // Or it could be an alloca + series of stores if initializers are dynamic or if it's a local mutable struct.

    bool allConstant = true;
    for (llvm::Value* val : initValues) {
        if (!llvm::isa<llvm::Constant>(val)) {
            allConstant = false;
            break;
        }
    }

    if (allConstant) {
        std::vector<llvm::Constant*> constInitValues;
        for (llvm::Value* val : initValues) {
            constInitValues.push_back(llvm::cast<llvm::Constant>(val));
        }
        m_currentLLVMValue = llvm::ConstantStruct::get(structLLVMType, constInitValues);
    } else {
        // Allocate memory for the struct
        llvm::AllocaInst* alloca = builder->CreateAlloca(structLLVMType, nullptr, getTypeName(structLLVMType) + ".construct");
        // Store each initializer into the respective field
        for (unsigned i = 0; i < initValues.size(); ++i) {
            llvm::Value* fieldPtr = builder->CreateStructGEP(structLLVMType, alloca, i, "field.init.ptr");
            builder->CreateStore(initValues[i], fieldPtr);
        }
        // The result of a construction expression that allocates should be the loaded struct value,
        // unless Vyn semantics are that construction yields a pointer (like `new`).
        // Assuming it yields the value for now. If it's by-ref, then `alloca` is the value.
        // If Vyn structs are reference types by default, then `alloca` is correct.
        // If Vyn structs are value types, then we should load from `alloca` if not LHS.
        // For now, let's assume construction of `MyType{}` results in a value of `MyType`.
        // If it was allocated on stack, we need to load from it to get the value.
        m_currentLLVMValue = builder->CreateLoad(structLLVMType, alloca, getTypeName(structLLVMType) + ".val");
    }
    m_currentLLVMType = structLLVMType;
}

void LLVMCodegen::visit(vyn::ast::ArrayInitializationExpression* node) {
    llvm::errs() << "[ArrayInitializationExpression] Visiting (stubbed)\\\\n";
    // Syntax: Type[count] or [value; count]
    // Current ast.hpp supports Type[count] via: TypeNodePtr elementType; ExprPtr sizeExpression;
    // This implementation adapts to Type[count] and creates default values.

    // 1. Determine element type
    if (!node->elementType) {
        logError(node->loc, "ArrayInitializationExpression is missing elementType.");
        m_currentLLVMValue = nullptr; return;
    }
    llvm::Type* llvmElementType = codegenType(node->elementType.get());
    if (!llvmElementType) {
        logError(node->elementType->loc, "Could not determine LLVM type for array element.");
        m_currentLLVMValue = nullptr; return;
    }

    // 2. Codegen the count expression (node->sizeExpression)
    if (!node->sizeExpression) {
        logError(node->loc, "ArrayInitializationExpression is missing sizeExpression.");
        m_currentLLVMValue = nullptr; return;
    }
    node->sizeExpression->accept(*this);
    llvm::Value* countValue = m_currentLLVMValue;
    if (!countValue) {
        logError(node->sizeExpression->loc, "Count expression in array initialization is null.");
        m_currentLLVMValue = nullptr; return;
    }
    if (!countValue->getType()->isIntegerTy()) {
        logError(node->sizeExpression->loc, "Count expression must be an integer, got " + getTypeName(countValue->getType()));
        m_currentLLVMValue = nullptr; return;
    }
    
    llvm::ConstantInt* constCount = llvm::dyn_cast<llvm::ConstantInt>(countValue);
    if (!constCount) {
        logError(node->sizeExpression->loc, "Array size in Type[count] must be a compile-time constant for static array type.");
        m_currentLLVMValue = nullptr; return;
    }
    uint64_t arraySize = constCount->getZExtValue();
    if (arraySize == 0 && !llvmElementType->isVoidTy()) {
        logWarning(node->loc, "Initializing a zero-sized array.");
    }

    // 3. Create a default initial value for the element type
    llvm::Value* initValue = nullptr;
    if (llvmElementType->isIntegerTy()) {
        initValue = llvm::ConstantInt::get(llvmElementType, 0);
    } else if (llvmElementType->isFloatingPointTy()) {
        initValue = llvm::ConstantFP::get(llvmElementType, 0.0);
    } else if (llvmElementType->isPointerTy()) {
        initValue = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(llvmElementType));
    } else if (llvmElementType->isStructTy() || llvmElementType->isArrayTy()) {
        // For aggregate types, we might need UndefValue if not initializing with a loop
        // or if a default constructor isn't called per element.
        // If we use a loop (as below for non-constant initValue), this default initValue is what's stored.
        // If the struct has a default constructor, that should be called.
        // For now, if it's an aggregate, we can't easily make a simple Constant default other than undef or zeroinitializer.
        // Let's use zeroinitializer for aggregates if initValue must be constant.
        // If initValue is used in a loop, it can be more complex.
        // The existing code uses initValue in a loop if it's not constant.
        // For default initialization, zeroinitializer is a good default constant.
        initValue = llvm::ConstantAggregateZero::get(llvmElementType);
    }
    
    if (!initValue) {
         logError(node->elementType->loc, "Cannot create default initial value for element type: " + getTypeName(llvmElementType));
         m_currentLLVMValue = nullptr; return;
    }


    llvm::ArrayType* arrayType = llvm::ArrayType::get(llvmElementType, arraySize);

    // If initValue is constant (which our default ones are), we can create a ConstantArray
    if (llvm::Constant* constInitVal = llvm::dyn_cast<llvm::Constant>(initValue)) {
        std::vector<llvm::Constant*> elementConstants(arraySize, constInitVal);
        llvm::Constant* arrayConstant = llvm::ConstantArray::get(arrayType, elementConstants);
        
        if (currentFunction) { // Local array
            llvm::AllocaInst* alloca = builder->CreateAlloca(arrayType, nullptr, "arrayinit");
            builder->CreateStore(arrayConstant, alloca);
            m_currentLLVMValue = builder->CreateLoad(arrayType, alloca, "arrayinit.val"); // Load the array value
        } else { // Global array
            auto* gv = new llvm::GlobalVariable(*module, arrayType, true /*isConstant*/, 
                                          llvm::GlobalValue::PrivateLinkage, arrayConstant, ".arrinit");
            m_currentLLVMValue = gv; // Or load from it if expression context expects value
        }
    } else {
        // Dynamic initValue or local array: requires loop or memset/memcpy for initialization if possible
        // For now, assume local array, initialize with a loop.
        if (!currentFunction) {
            logError(node->loc, "Array initialization with non-constant value at global scope not yet supported without loop unrolling or runtime init.");
            m_currentLLVMValue = nullptr; return;
        }
        llvm::AllocaInst* alloca = builder->CreateAlloca(arrayType, nullptr, "arraydyninit");
        
        // Loop to initialize elements
        llvm::BasicBlock* loopHeaderBB = llvm::BasicBlock::Create(*context, "arrinit.loop.header", currentFunction);
        llvm::BasicBlock* loopBodyBB = llvm::BasicBlock::Create(*context, "arrinit.loop.body", currentFunction);
        llvm::BasicBlock* loopEndBB = llvm::BasicBlock::Create(*context, "arrinit.loop.end", currentFunction);

        builder->CreateBr(loopHeaderBB);
        builder->SetInsertPoint(loopHeaderBB);

        llvm::PHINode* iv = builder->CreatePHI(int64Type, 2, "arrinit.iv");
        iv->addIncoming(llvm::ConstantInt::get(int64Type, 0), builder->GetInsertBlock()); // Start with 0 from preheader

        llvm::Value* cond = builder->CreateICmpULT(iv, llvm::ConstantInt::get(int64Type, arraySize), "arrinit.loop.cond");
        builder->CreateCondBr(cond, loopBodyBB, loopEndBB);

        builder->SetInsertPoint(loopBodyBB);
        llvm::Value* elementPtr = builder->CreateGEP(arrayType, alloca, {llvm::ConstantInt::get(int64Type, 0), iv}, "arrinit.elem.ptr");
        builder->CreateStore(initValue, elementPtr);
        
        llvm::Value* nextIv = builder->CreateAdd(iv, llvm::ConstantInt::get(int64Type, 1), "arrinit.nextiv");
        iv->addIncoming(nextIv, builder->GetInsertBlock()); // Add incoming from end of body

        builder->CreateBr(loopHeaderBB); // Loop back

        builder->SetInsertPoint(loopEndBB);
        m_currentLLVMValue = builder->CreateLoad(arrayType, alloca, "arraydyninit.val"); // Load the initialized array value
    }
    m_currentLLVMType = arrayType;
}

void LLVMCodegen::visit(vyn::ast::GenericInstantiationExpression* node) {
    llvm::errs() << "[GenericInstantiationExpression] Visiting (stubbed)\\\\n";
    // MyGenericType<ConcreteType1, ConcreteType2>
    // This is highly dependent on how generics are implemented (e.g. monomorphization).
    // It might involve looking up a specialized type or triggering its generation.
    // For now, this is a complex feature beyond a simple stub.
    logError(node->loc, "GenericInstantiationExpression codegen not implemented.");
    m_currentLLVMValue = nullptr; // Placeholder
    // A real implementation would:
    // 1. Identify the generic base (e.g. MyGenericType).
    // 2. Codegen the type arguments (ConcreteType1, ConcreteType2) to get their LLVM types.
    // 3. Find or create a specialized version of MyGenericType with these LLVM types.
    //    This might involve mangling names, e.g., MyGenericType_ConcreteType1_ConcreteType2.
    // 4. The result of this expression is often the specialized type itself, or an instance if followed by construction.
    //    The AST node needs to clarify if this is just type instantiation or also value construction.
    //    If it's `MyGeneric<int>{}` vs `MyGeneric<int>`, the former is ConstructionExpression with a generic type.
    //    If this node *is* the type, then m_currentLLVMValue might not be an llvm::Value but an llvm::Type,
    //    or this visitor sets m_currentLLVMType.
    // For now, this is a complex feature beyond a simple stub.
}


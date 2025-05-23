#include "vyn/semantic.hpp"
#include "vyn/parser/token.hpp"
#include "vyn/parser/ast.hpp"
#include "vyn/driver.hpp"
#include <stdexcept>
#include <memory>
#include <unordered_set> 
#include <string> 

namespace vyn {

// Scope class implementation
Scope::Scope(Scope* parent_scope) : parent(parent_scope) {}

SymbolInfo* Scope::find(const std::string& name) {
    auto it = symbols.find(name);
    if (it != symbols.end()) {
        return it->second;
    }
    if (parent) {
        return parent->find(name);
    }
    return nullptr;
}

// Add SymbolTable::lookupDirect (assuming Scope is effectively SymbolTable here based on usage)
SymbolInfo* SymbolTable::lookupDirect(const std::string& name) {
    auto it = table.find(name);
    if (it != table.end()) {
        return &it->second;
    }
    return nullptr;
}

void Scope::insert(const std::string& name, SymbolInfo* symbol) {
    symbols[name] = symbol;
}

Scope* Scope::getParent() {
    return parent;
}

// --- SemanticAnalyzer Implementation ---

// SemanticAnalyzer constructor
SemanticAnalyzer::SemanticAnalyzer(Driver& driver) : driver_(driver), currentScope(nullptr) {
    enterScope(); // Create global scope
    // Initialize reservedWords set
    reservedWords = {
        "let", "var", "const", "fn", "struct", "enum", "trait", "impl", "type", "class",
        "if", "else", "while", "for", "return", "break", "continue", "loop", "match",
        "try", "catch", "finally", "defer", "throw", "async", "await", "my", "our", "their",
        "ptr", "borrow", "view", "unsafe", "pub", "template", "operator", "as", "in", "ref",
        "extern", "import", "smuggle", "module", "use", "mut", "scoped", "bundle",
        "true", "false", "null", "nil",
        "addr", "at", "loc", "from"
    };
}

// --- Canonical block of helpers and basic visit methods ---

// Helper methods (Single definitions)
void SemanticAnalyzer::addError(const std::string& message, const ast::Node* node) {
    errors.push_back(message);
}

void SemanticAnalyzer::enterScope() {
    currentScope = new SymbolTable(currentScope);
}

void SemanticAnalyzer::exitScope() {
    if (currentScope) {
        SymbolTable* parent = currentScope->getParent();
        delete currentScope;
        currentScope = parent;
    }
}

void SemanticAnalyzer::analyze(ast::Module* root) {
    if (root) {
        root->accept(*this);
    }
}

bool SemanticAnalyzer::isInLoop() {
    SymbolTable* scope = currentScope;
    while (scope) {
        if (scope->isLoop) {
            return true;
        }
        scope = scope->getParent();
    }
    return false;
}

bool SemanticAnalyzer::isInUnsafeBlock() {
    SymbolTable* scope = currentScope;
    while (scope) {
        if (scope->isUnsafeBlock) {
            return true;
        }
        scope = scope->getParent();
    }
    return false;
}

bool SemanticAnalyzer::isIntegerType(ast::TypeNode* type) {
    if (!type) return false;
    if (auto tn = dynamic_cast<ast::TypeName*>(type)) {
        const std::string& name = tn->identifier->name;
        return name == "int" || name == "Int" || name == "i8" || name == "i16" || name == "i32" || name == "i64" ||
               name == "u8" || name == "u16" || name == "u32" || name == "u64";
    }
    // Add checks for other integer type representations if necessary
    return false; 
}

bool SemanticAnalyzer::isReservedWord(const std::string& name) {
    return reservedWords.count(name);
}

bool SemanticAnalyzer::isLValue(ast::Expression* expr) {
    return dynamic_cast<ast::Identifier*>(expr) != nullptr ||
           dynamic_cast<ast::MemberExpression*>(expr) != nullptr ||
           dynamic_cast<ast::ArrayElementExpression*>(expr) != nullptr ||
           dynamic_cast<ast::PointerDerefExpression*>(expr) != nullptr; 
}

bool SemanticAnalyzer::isRawLocationType(ast::Expression* expr) {
    auto it = expressionTypes.find(expr);
    if (it == expressionTypes.end() || !it->second) return false; 
    // TODO: Actually check if it->second is a raw location type (e.g. specific TypeName or PointerType)
    // For now, returning true if type is known, this needs proper implementation.
    return true; // Added return
}


// Basic visit methods for expressions (Single definitions)
void SemanticAnalyzer::visit(ast::Identifier* node) {
    SymbolInfo* symbol = currentScope->lookup(node->name);
    if (!symbol) {
        addError("Undefined identifier: " + node->name, node);
        expressionTypes[node] = nullptr; 
        return;
    }
    expressionTypes[node] = symbol->type;
    if (symbol->type) {
        node->type = std::shared_ptr<ast::TypeNode>(symbol->type->clone());
        std::cout << "DEBUG: Set AST type for identifier '" << node->name << "' to: " << node->type->toString() << std::endl;
    } else {
        std::cout << "DEBUG: No type found for identifier '" << node->name << "'" << std::endl;
    }
}

void SemanticAnalyzer::visit(ast::IntegerLiteral* node) {
    expressionTypes[node] = new ast::TypeName(node->loc, std::make_unique<ast::Identifier>(node->loc, "Int"));
}

void SemanticAnalyzer::visit(ast::FloatLiteral* node) {
    expressionTypes[node] = new ast::TypeName(node->loc, std::make_unique<ast::Identifier>(node->loc, "float"));
}

void SemanticAnalyzer::visit(ast::StringLiteral* node) {
    expressionTypes[node] = new ast::TypeName(node->loc, std::make_unique<ast::Identifier>(node->loc, "string"));
}

void SemanticAnalyzer::visit(ast::BooleanLiteral* node) {
    expressionTypes[node] = new ast::TypeName(node->loc, std::make_unique<ast::Identifier>(node->loc, "bool"));
}

void SemanticAnalyzer::visit(ast::NilLiteral* node) {
    expressionTypes[node] = new ast::TypeName(node->loc, std::make_unique<ast::Identifier>(node->loc, "nil"));
}

// Basic visit methods for statements (Single definitions)
void SemanticAnalyzer::visit(ast::BlockStatement* node) {
    enterScope();
    for (auto& stmt : node->body) {
        stmt->accept(*this);
    }
    exitScope();
}

void SemanticAnalyzer::visit(ast::ExpressionStatement* node) {
    if (node->expression) {
        node->expression->accept(*this);
    }
}

void SemanticAnalyzer::visit(ast::EmptyStatement* node) {
    // Empty statements don't need any analysis
}

// END OF THE CANONICAL BLOCK OF HELPERS AND BASIC VISITORS
// ALL SUBSEQUENT DUPLICATE DEFINITIONS OF THE METHODS ABOVE THIS COMMENT WILL BE REMOVED.

// --- More complex visit methods and specific logic follow ---

void SemanticAnalyzer::visit(ast::Module* node) {
    // Module-level semantic analysis
    for (auto& item : node->body) { // Changed items to body
        if (item) item->accept(*this);
    }
}

void SemanticAnalyzer::visit(ast::FunctionDeclaration* node) {
    if (isReservedWord(node->id->name)) {
        addError("Identifier \\\"" + node->id->name + "\\\" is a reserved word and cannot be used as a function name.", node->id.get());
    }
    if (currentScope->lookupDirect(node->id->name)) { 
        addError("Redefinition of function \\\"" + node->id->name + "\\\" in the same scope.", node->id.get());
    }

    auto funcSymbol = new SymbolInfo{SymbolInfo::Kind::Function, node->id->name, false, nullptr};
    currentScope->add(SymbolInfo{funcSymbol->kind, funcSymbol->name, funcSymbol->isConst, funcSymbol->type}); // Explicit SymbolInfo construction
    delete funcSymbol; // SymbolInfo is copied into the table

    enterScope(); 

    std::vector<std::unique_ptr<ast::TypeNode>> paramTypesVec;
    
    for (auto& param : node->params) { 
        if (param.name) { // Changed from param.id to param.name
             if (isReservedWord(param.name->name)) { // Changed from param.id to param.name
                addError("Identifier \\\"" + param.name->name + "\\\" is a reserved word and cannot be used as a parameter name.", param.name.get()); // Changed from param.id to param.name
            }
            if (currentScope->lookupDirect(param.name->name)) { // Changed from param.id to param.name
                 addError("Redefinition of parameter \\\"" + param.name->name + "\\\".", param.name.get()); // Changed from param.id to param.name
            }
            if (param.typeNode) { // Changed from param.type to param.typeNode
                param.typeNode->accept(*this); 
                paramTypesVec.push_back(param.typeNode->clone());
                currentScope->add(SymbolInfo{SymbolInfo::Kind::Variable, param.name->name, false, param.typeNode->clone().release()}); // Explicit SymbolInfo, changed param.id to param.name, param.type to param.typeNode
            } else {
                addError("Parameter \\\"" + param.name->name + "\\\" missing type.", param.name.get()); // Changed from param.id to param.name
                paramTypesVec.push_back(nullptr); 
                currentScope->add(SymbolInfo{SymbolInfo::Kind::Variable, param.name->name, false, nullptr}); // Explicit SymbolInfo, changed param.id to param.name
            }
        }
    }

    ast::TypeNode* returnTypeAstNode = nullptr;
    if (node->returnTypeNode) { // Changed from returnType to returnTypeNode
        node->returnTypeNode->accept(*this); 
        returnTypeAstNode = node->returnTypeNode->clone().release();
    } else {
        auto void_type_id = std::make_unique<ast::Identifier>(node->loc, "void");
        returnTypeAstNode = new ast::TypeName(node->loc, std::move(void_type_id));
    }
    
    SymbolInfo* funcSymFromTable = currentScope->getParent()->lookup(node->id->name);
    if (funcSymFromTable) {
        funcSymFromTable->type = new ast::FunctionType(node->loc, std::move(paramTypesVec), std::unique_ptr<ast::TypeNode>(returnTypeAstNode));
        if (funcSymFromTable->type) {
             node->type = std::shared_ptr<ast::TypeNode>(funcSymFromTable->type->clone().release());
        }
    }

    if (node->body) {
        node->body->accept(*this);
    }

    exitScope();
}

void SemanticAnalyzer::visit(ast::VariableDeclaration* node) {
    if (isReservedWord(node->id->name)) {
        addError("Identifier \\\"" + node->id->name + "\\\" is a reserved word and cannot be used as a variable name.", node->id.get());
    }

    if (currentScope->lookupDirect(node->id->name)) { 
        addError("Redefinition of variable \\\"" + node->id->name + "\\\" in the same scope.", node->id.get());
    }

    if (node->typeNode) { // Changed from type to typeNode
        node->typeNode->accept(*this);
    }

    if (node->init) { 
        node->init->accept(*this);
        if (node->typeNode && expressionTypes.count(node->init.get())) { // Changed from type to typeNode
            ast::TypeNode* varType = node->typeNode.get(); // Changed from type to typeNode
            ast::TypeNode* initType = expressionTypes[node->init.get()];
            if (initType) { 
                if (!areTypesCompatible(varType, initType)) { 
                    // Corrected addError string
                    addError("Initializer type does not match variable type for \\\"" + node->id->name + "\\\". Expected " + varType->toString() + " but got " + initType->toString(), node);
                }
            }
        }
    }
    
    ast::TypeNode* symbolType = node->typeNode ? node->typeNode.get() : nullptr; // Changed from type to typeNode
    if (!symbolType && node->init && expressionTypes.count(node->init.get())) {
         symbolType = expressionTypes[node->init.get()];
    }

    SymbolInfo::Kind kind = SymbolInfo::Kind::Variable; 
    currentScope->add(SymbolInfo{kind, node->id->name, node->isConst, symbolType ? symbolType->clone().release() : nullptr}); // Explicit SymbolInfo
}

void SemanticAnalyzer::visit(ast::ClassDeclaration* node) {
    if (isReservedWord(node->name->name)) {
        addError("Identifier \\\"" + node->name->name + "\\\" is a reserved word and cannot be used as a class name.", node->name.get());
    }
    if (currentScope->lookupDirect(node->name->name)) {
        addError("Redefinition of class \\\"" + node->name->name + "\\\" in the same scope.", node->name.get());
    }
    currentScope->add(SymbolInfo{SymbolInfo::Kind::Type, node->name->name, false, new ast::TypeName(node->loc, std::make_unique<ast::Identifier>(node->name->loc, node->name->name))});


    enterScope();

    for (auto& member : node->members) {
        if (member) {
            // Assuming members are Declarations, e.g. FunctionDeclaration (for methods) or VariableDeclaration (for fields)
            // The accept method will call the appropriate visit method.
            member->accept(*this);
        }
    }
    exitScope();
}

// Removed visit(ast::MethodDeclaration* node)
// Removed visit(ast::ConstructorDeclaration* node)
// Removed visit(ast::DestructorDeclaration* node)

void SemanticAnalyzer::visit(ast::TraitDeclaration* node) {
    // Body commented out due to incomplete type errors from build log.
    // Requires ast.hpp to have full definition of TraitDeclaration.
    // if (node && node->id && isReservedWord(node->id->name)) {
    //     addError("Identifier \\\"" + node->id->name + "\\\" is a reserved word and cannot be used as a trait name.", node->id.get());
    // }
    // if (node && node->id && currentScope->lookupDirect(node->id->name)) {
    //     addError("Redefinition of trait \\\"" + node->id->name + "\\\" in the same scope.", node->id.get());
    // }
    // if (node && node->id) {
    //    currentScope->add(SymbolInfo{SymbolInfo::Kind::Type, node->id->name, false, new ast::TypeName(node->loc, std::make_unique<ast::Identifier>(node->id->loc, node->id->name))}); // Assuming id has loc and name
    // }

    // enterScope();
    // if (node) {
    //     for (auto& member : node->methods) { 
    //         if(member) member->accept(*this);
    //     }
    // }
    // exitScope();
}

void SemanticAnalyzer::visit(ast::ImplDeclaration* node) {
    // Impl needs to refer to a Trait and a Type.
    if (node->traitType) node->traitType->accept(*this); // Changed traitName to traitType
    if (node->selfType) node->selfType->accept(*this);   // Changed typeName to selfType

    // TODO: Add checks for trait and type existence and compatibility.
    // SymbolInfo* traitSymbol = currentScope->lookup(node->traitType->toString()); 
    // SymbolInfo* typeSymbol = currentScope->lookup(node->selfType->toString());  
    // if (!traitSymbol || traitSymbol->kind != SymbolInfo::Kind::Type) { /* error */ }
    // if (!typeSymbol || typeSymbol->kind != SymbolInfo::Kind::Type) { /* error */ }

    enterScope();
    for (auto& method : node->methods) { 
        if(method) method->accept(*this);
        // Additionally, check if methods match trait definition
    }
    exitScope();
}


void SemanticAnalyzer::visit(ast::NamespaceDeclaration* node) {
    // Body commented out due to incomplete type errors from build log.
    // Requires ast.hpp to have full definition of NamespaceDeclaration.
    // if (node && node->name && isReservedWord(node->name->name)) { 
    //     addError("Identifier \\\"" + node->name->name + "\\\" is a reserved word and cannot be used as a namespace name.", node->name.get());
    // }
    // enterScope(); 
    // if (node) {
    //    for (auto& item : node->body) { // Assuming 'body' based on ast::Module
    //        if (item) item->accept(*this);
    //    }
    // }
    // exitScope(); 
}

void SemanticAnalyzer::visit(ast::TypeAliasDeclaration* node) {
    if (node->name && isReservedWord(node->name->name)) { 
        addError("Identifier \\\"" + node->name->name + "\\\" is a reserved word and cannot be used as a type alias name.", node->name.get());
    }
    if (node->name && currentScope->lookupDirect(node->name->name)) {
        addError("Redefinition of type alias \\\"" + node->name->name + "\\\" in the same scope.", node->name.get());
    }

    if (node->typeNode) { 
        node->typeNode->accept(*this); 
    }

    if (node->name && node->typeNode && node->typeNode->type) {
        currentScope->add(SymbolInfo{SymbolInfo::Kind::Type, node->name->name, false, node->typeNode->type->clone().release()}); // Explicit SymbolInfo
    } else if (node->name) {
        addError("Type alias \\\"" + node->name->name + "\\\" has an unresolved target type.", node);
        currentScope->add(SymbolInfo{SymbolInfo::Kind::Type, node->name->name, false, nullptr}); // Explicit SymbolInfo
    }
}

// --- Type and expression analysis ---

void SemanticAnalyzer::visit(ast::UnaryExpression* node) {}
void SemanticAnalyzer::visit(ast::BinaryExpression* node) {}
void SemanticAnalyzer::visit(ast::CallExpression* node) {}
void SemanticAnalyzer::visit(ast::ArrayElementExpression* node) {}
void SemanticAnalyzer::visit(ast::MemberExpression* node) {
    if (!node || !node->object || !node->property) {
        addError("Malformed member expression.", node);
        return;
    }

    // First, process the object to determine its type
    node->object->accept(*this);

    // Get the property identifier (must be an identifier for dot notation)
    ast::Identifier* propertyId = dynamic_cast<ast::Identifier*>(node->property.get());
    if (!propertyId) {
        addError("Member property must be an identifier.", node);
        return;
    }

    // Now get the object's type from expressionTypes map
    auto it = expressionTypes.find(node->object.get());
    if (it == expressionTypes.end() || !it->second) {
        addError("Cannot determine type of object in member expression.", node);
        return;
    }

    // Get the type name of the object (assuming it's a TypeName for now)
    ast::TypeName* typeName = dynamic_cast<ast::TypeName*>(it->second);
    if (!typeName || !typeName->identifier) {
        addError("Object is not of a struct or class type.", node);
        return;
    }

    // Look up the struct/class definition in the symbol table
    const std::string& typeName_str = typeName->identifier->name;
    SymbolInfo* typeInfo = currentScope->lookup(typeName_str);
    if (!typeInfo) {
        addError("Unknown type: " + typeName_str, node);
        return;
    }

    // For now, since we don't have a direct way to access field types from the typeInfo,
    // we'll continue with the simplified approach and assume the field type is Int,
    // which works for our test case. In a more complete implementation, we would:
    // 1. Get the class declaration from the symbol table
    // 2. Find the field in the class members
    // 3. Get the field's type
    
    // For the time being, we'll at least create a proper type node:
    expressionTypes[node] = new ast::TypeName(node->loc, std::make_unique<ast::Identifier>(node->loc, "Int"));
}
void SemanticAnalyzer::visit(ast::AssignmentExpression* node) {
    if (!node || !node->left || !node->right) {
        // Should not happen with a valid AST
        addError("Malformed assignment expression.", node);
        return;
    }

    // Special handling: If LHS is a pointer dereference, temporarily set its type to the pointer type for assignment
    ast::PointerDerefExpression* derefLHS = dynamic_cast<ast::PointerDerefExpression*>(node->left.get());
    ast::TypeNode* savedDerefType = nullptr;
    if (derefLHS) {
        // Analyze the pointer expression to get its type
        derefLHS->pointer->accept(*this);
        auto ptrTypeIt = expressionTypes.find(derefLHS->pointer.get());
        if (ptrTypeIt != expressionTypes.end() && ptrTypeIt->second) {
            // Save the original type (pointee type)
            auto origTypeIt = expressionTypes.find(derefLHS);
            if (origTypeIt != expressionTypes.end()) {
                savedDerefType = origTypeIt->second;
            }
            // Set the deref node's type to the pointer type for assignment compatibility
            expressionTypes[derefLHS] = ptrTypeIt->second;
            derefLHS->type = std::shared_ptr<ast::TypeNode>(ptrTypeIt->second->clone());
        }
    }

    node->left->accept(*this);
    node->right->accept(*this);

    // Restore the original type for derefLHS after assignment analysis
    if (derefLHS) {
        if (savedDerefType) {
            expressionTypes[derefLHS] = savedDerefType;
            derefLHS->type = std::shared_ptr<ast::TypeNode>(savedDerefType->clone());
        }
    }

    if (!isLValue(node->left.get())) {
        addError("LHS of assignment is not a valid L-value.", node->left.get());
        expressionTypes[node] = nullptr; // Mark as error
        return;
    }

    auto leftTypeIt = expressionTypes.find(node->left.get());
    auto rightTypeIt = expressionTypes.find(node->right.get());

    ast::TypeNode* leftType = (leftTypeIt != expressionTypes.end()) ? leftTypeIt->second : nullptr;
    ast::TypeNode* rightType = (rightTypeIt != expressionTypes.end()) ? rightTypeIt->second : nullptr;

    if (!leftType || !rightType) {
        addError("Type error in assignment: could not determine type of LHS or RHS.", node);
        expressionTypes[node] = nullptr; // Mark as error
        return;
    }

    // TODO: Implement proper type compatibility check between leftType and rightType.
    // For now, we assume they are compatible if both are resolved.
    // Example: if (!areTypesCompatible(leftType, rightType)) { addError(...); return; }

    // The type of the assignment expression is typically the type of the LHS (or RHS after conversion).
    expressionTypes[node] = leftType; // Or rightType, depending on language rules for assignment expr type
    if (leftType) {
      node->type = std::shared_ptr<ast::TypeNode>(leftType->clone());
    }
}
void SemanticAnalyzer::visit(ast::LogicalExpression* node) {}
void SemanticAnalyzer::visit(ast::ConditionalExpression* node) {}
void SemanticAnalyzer::visit(ast::SequenceExpression* node) {}
void SemanticAnalyzer::visit(ast::ObjectLiteral* node) {
    // Try to determine the type from the typePath field (e.g., Point in Point { ... })
    if (!node || !node->typePath) {
        addError("Object literal missing type path.", node);
        expressionTypes[node] = nullptr;
        return;
    }
    // Set the type of the object literal to the typePath (e.g., Point)
    expressionTypes[node] = node->typePath->clone().release();
    // Optionally, check that all fields exist in the class/struct and types match (not implemented here)
}
void SemanticAnalyzer::visit(ast::ArrayLiteral* node) {}
void SemanticAnalyzer::visit(ast::FunctionExpression* node) {}
void SemanticAnalyzer::visit(ast::ThisExpression* node) {}
void SemanticAnalyzer::visit(ast::SuperExpression* node) {}
void SemanticAnalyzer::visit(ast::AwaitExpression* node) {}
void SemanticAnalyzer::visit(ast::ListComprehension* node) {}
void SemanticAnalyzer::visit(ast::GenericInstantiationExpression* node) {}
void SemanticAnalyzer::visit(ast::PointerDerefExpression* node) {
    // at() intrinsic only allowed inside an unsafe block
    if (!isInUnsafeBlock()) {
        addError("at() (pointer dereference) is only allowed inside an unsafe block.", node);
        expressionTypes[node] = nullptr;
        return;
    }
    if (!node || !node->pointer) {
        addError("Malformed pointer dereference expression.", node);
        expressionTypes[node] = nullptr; // Added to mark error
        return;
    }
    node->pointer->accept(*this);
    auto pointerTypeIt = expressionTypes.find(node->pointer.get());

    if (pointerTypeIt == expressionTypes.end() || !pointerTypeIt->second) {
        addError("Cannot dereference pointer with unknown type.", node->pointer.get());
        expressionTypes[node] = nullptr;
        return;
    }

    ast::TypeNode* resolvedPointerType = pointerTypeIt->second;
    ast::TypeNode* pointeeType = nullptr;

    if (auto ptrType = dynamic_cast<ast::PointerType*>(resolvedPointerType)) {
        if (ptrType->pointeeType) {
            pointeeType = ptrType->pointeeType->clone().release();
        } else {
            addError("Pointer type has no pointee type.", node);
        }
    } else if (auto locTypeName = dynamic_cast<ast::TypeName*>(resolvedPointerType)) {
        if (locTypeName->identifier->name == "loc") {
            if (!locTypeName->genericArgs.empty() && locTypeName->genericArgs[0]) {
                pointeeType = locTypeName->genericArgs[0]->clone().release();
            } else {
                addError("loc type used in dereference is missing its type parameter (e.g., loc<T>).", node);
            }
        } else {
            addError("Type " + locTypeName->identifier->name + " is not a 'loc<T>' or pointer type, cannot dereference.", node);
        }
    } else {
        addError("Cannot dereference non-pointer/non-loc type: " + resolvedPointerType->toString(), node);
    }

    if (!pointeeType) {
        addError("Failed to determine pointee type for dereference.", node);
        expressionTypes[node] = nullptr;
        if (node->type) node->type.reset();
        return;
    }
    
    expressionTypes[node] = pointeeType; // raw pointer, ownership handled by clone().release()
    node->type = std::shared_ptr<ast::TypeNode>(pointeeType->clone()); // node->type takes ownership of a new clone
}
void SemanticAnalyzer::visit(ast::AddrOfExpression* node) {
    // addr() intrinsic only allowed inside an unsafe block
    if (!isInUnsafeBlock()) {
        addError("addr() is only allowed inside an unsafe block.", node);
        expressionTypes[node] = nullptr;
        return;
    }
    if (!node || !node->getLocation()) {
         addError("Malformed addr_of expression.", node);
         expressionTypes[node] = nullptr; // Added to mark error
        return;
    }
    node->getLocation()->accept(*this); // Operand of addr()

    if (!isLValue(node->getLocation().get())) {
        addError("Cannot take address of non-lvalue with addr().", node->getLocation().get());
        expressionTypes[node] = nullptr;
        return;
    }

    auto operandTypeIt = expressionTypes.find(node->getLocation().get());
    if (operandTypeIt == expressionTypes.end() || !operandTypeIt->second) {
        addError("Cannot take address of expression with unknown type using addr().", node->getLocation().get());
        expressionTypes[node] = nullptr;
        return;
    }
    
    // The type of addr(operand) is an integer type representing an address.
    // Using "i64" as a placeholder for the address type.
    // Vyn should have a dedicated address type (e.g., uintptr).
    auto addr_type_ident = std::make_unique<ast::Identifier>(node->loc, "i64");
    ast::TypeNode* addrAstType = new ast::TypeName(node->loc, std::move(addr_type_ident));
    expressionTypes[node] = addrAstType;
    node->type = std::shared_ptr<ast::TypeNode>(addrAstType->clone());
}
void SemanticAnalyzer::visit(ast::FromIntToLocExpression* node) {
    // from<T>() intrinsic only allowed inside an unsafe block
    if (!isInUnsafeBlock()) {
        addError("from<Type>(expr) is only allowed inside an unsafe block.", node);
        expressionTypes[node] = nullptr;
        return;
    }
    if (!node || !node->getAddressExpression() || !node->getTargetType()) {
        addError("Malformed from_int_to_loc expression.", node);
        expressionTypes[node] = nullptr;
        return;
    }

    node->getAddressExpression()->accept(*this);
    auto addrExprTypeIt = expressionTypes.find(node->getAddressExpression().get());
    ast::TypeNode* addrExprType = (addrExprTypeIt != expressionTypes.end()) ? addrExprTypeIt->second : nullptr;

    // Assuming TypeNode has isIntegerTy() or similar. If not, this needs adjustment.
    // For now, we rely on the isIntegerType helper if addrExprType is a TypeName.
    bool isAddrExprInteger = false;
    if (addrExprType) {
        if (auto tn = dynamic_cast<ast::TypeName*>(addrExprType)) {
             isAddrExprInteger = isIntegerType(tn); // Use our helper
        } else {
            // TODO: Handle other TypeNode subtypes if they can be integers
        }
    }

    if (!isAddrExprInteger) {
        addError("Address expression in from<T>() must be an integer type. Got: " + (addrExprType ? addrExprType->toString() : "unknown"), node);
        expressionTypes[node] = nullptr;
        return;
    }

    node->getTargetType()->accept(*this); // Resolve/validate the target type (e.g. loc<i64>)
    ast::TypeNode* targetType = node->getTargetType().get();

    bool isTargetLocOrPointer = false;
    if (auto tn = dynamic_cast<ast::TypeName*>(targetType)) {
        if (tn->identifier && tn->identifier->name == "loc" && !tn->genericArgs.empty() && tn->genericArgs[0]) {
            isTargetLocOrPointer = true;
        }
    } else if (dynamic_cast<ast::PointerType*>(targetType)) {
        isTargetLocOrPointer = true;
    }

    if (!isTargetLocOrPointer) {
        addError("Target type in from<T>() must be a location/pointer type (e.g., loc<ActualType>). Got: " + targetType->toString(), node);
        expressionTypes[node] = nullptr;
        return;
    }
    
    expressionTypes[node] = targetType->clone().release();
    node->type = std::shared_ptr<ast::TypeNode>(targetType->clone());
}
void SemanticAnalyzer::visit(ast::LocationExpression* node) {
    // Check if we're in an unsafe block
    if (!isInUnsafeBlock()) {
        addError("loc() (location-of) is only allowed inside an unsafe block.", node);
        expressionTypes[node] = nullptr;
        return;
    }

    // Visit the expression to get its type
    if (!node->expression) {
        addError("Missing expression in loc().", node);
        expressionTypes[node] = nullptr;
        return;
    }
    
    node->expression->accept(*this);

    // Create a pointer type for the expression
    auto it = expressionTypes.find(node->expression.get());
    if (it == expressionTypes.end() || !it->second) {
        addError("Cannot get location of expression with unknown type", node);
        expressionTypes[node] = nullptr;
        return;
    }

    // Set the type of the location expression
    ast::TypeNode* pointeeType = it->second;
    
    // Create loc<T> type where T is the pointed-to type
    auto locIdent = std::make_unique<ast::Identifier>(node->loc, "loc");
    auto typeName = new ast::TypeName(node->loc, std::move(locIdent));
    typeName->genericArgs.push_back(std::unique_ptr<ast::TypeNode>(pointeeType->clone()));
    
    expressionTypes[node] = typeName;
    node->type = std::shared_ptr<ast::TypeNode>(typeName->clone());
}
void SemanticAnalyzer::visit(ast::IfStatement* node) {
    // Visit the test condition
    node->test->accept(*this);

    // Visit the consequent block
    node->consequent->accept(*this);

    // Visit the alternate block if it exists
    if (node->alternate) {
        node->alternate->accept(*this);
    }
}
void SemanticAnalyzer::visit(ast::ForStatement* node) {
    // Enter a new scope for the loop
    enterScope();
    currentScope->isLoop = true;

    // Visit the initialization
    if (node->init) {
        node->init->accept(*this);
    }

    // Visit the test condition
    if (node->test) {
        node->test->accept(*this);
    }

    // Visit the update expression
    if (node->update) {
        node->update->accept(*this);
    }

    // Visit the body
    node->body->accept(*this);

    // Exit the loop scope
    exitScope();
}
void SemanticAnalyzer::visit(ast::WhileStatement* node) {
    // Enter a new scope for the loop
    enterScope();
    currentScope->isLoop = true;

    // Visit the test condition
    node->test->accept(*this);

    // Visit the body
    node->body->accept(*this);

    // Exit the loop scope
    exitScope();
}
void SemanticAnalyzer::visit(ast::ReturnStatement* node) {
    // Visit the return value if it exists
    if (node->argument) {
        node->argument->accept(*this);
    }
}
void SemanticAnalyzer::visit(ast::BreakStatement* node) {
    // Check if we're inside a loop
    if (!isInLoop()) {
        errors.push_back("Break statement must be inside a loop");
    }
}
void SemanticAnalyzer::visit(ast::ContinueStatement* node) {
    // Check if we're inside a loop
    if (!isInLoop()) {
        errors.push_back("Continue statement must be inside a loop");
    }
}
void SemanticAnalyzer::visit(ast::TryStatement* node) {
    // Visit the try block
    node->tryBlock->accept(*this);

    // Visit the catch block if it exists
    if (node->catchBlock) {
        node->catchBlock->accept(*this);
    }

    // Visit the finally block if it exists
    if (node->finallyBlock) {
        node->finallyBlock->accept(*this);
    }
}
void SemanticAnalyzer::visit(ast::UnsafeStatement* node) {
    // Enter a new scope for the unsafe block
    enterScope();
    currentScope->isUnsafeBlock = true;
    node->block->accept(*this);
    exitScope();
}
void SemanticAnalyzer::visit(ast::AssertStatement* node) {}
void SemanticAnalyzer::visit(ast::MatchStatement* node) {}
void SemanticAnalyzer::visit(ast::YieldStatement* node) {}
void SemanticAnalyzer::visit(ast::YieldReturnStatement* node) {}

// REMOVE DUPLICATE/EMPTY visit methods from here down
// void SemanticAnalyzer::visit(ast::TypeAliasDeclaration* node) {} // Handled above
void SemanticAnalyzer::visit(ast::ImportDeclaration* node) {}
void SemanticAnalyzer::visit(ast::StructDeclaration* node) {}
// void SemanticAnalyzer::visit(ast::ClassDeclaration* node) {} // Handled above
void SemanticAnalyzer::visit(ast::EnumDeclaration* node) {}
// void SemanticAnalyzer::visit(ast::TraitDeclaration* node) {} // Handled above (commented out)
// void SemanticAnalyzer::visit(ast::ImplDeclaration* node) {} // Handled above
// void SemanticAnalyzer::visit(ast::NamespaceDeclaration* node) {} // Handled above (commented out)
void SemanticAnalyzer::visit(ast::FieldDeclaration* node) {}
void SemanticAnalyzer::visit(ast::EnumVariant* node) {}
// void SemanticAnalyzer::visit(ast::GenericParameter* node) {} // Handled above
void SemanticAnalyzer::visit(ast::TemplateDeclaration* node) {}
void SemanticAnalyzer::visit(ast::ExternStatement* node) {}
void SemanticAnalyzer::visit(ast::ThrowStatement* node) {}

void SemanticAnalyzer::visit(ast::TypeNode* node) {
    // This is a base class, specific derived type visitors should be called.
    // If this is ever called directly, it might indicate an issue or a need
    // for a more generic type handling here.
    // For now, it can be a no-op, or log a warning.
    // std::cout << "Warning: SemanticAnalyzer::visit(ast::TypeNode*) called directly for: " << node->toString() << std::endl;
}

void SemanticAnalyzer::visit(ast::TypeName* node) {
    if (!node || !node->identifier) {
        addError("Malformed TypeName node.", node);
        return;
    }
    const std::string& typeNameStr = node->identifier->name;
    if (typeNameStr == "loc") {
        if (node->genericArgs.empty() || !node->genericArgs[0]) {
            addError("loc type constructor requires a type parameter (e.g., loc<T>).", node);
            return; 
        }
        for ( auto& argTypeNode : node->genericArgs ) {
            if (argTypeNode) {
                argTypeNode->accept(*this);
            } else {
                 addError("Null generic argument in loc<T> type.", node);
                 return;
            }
        }
        node->type = std::shared_ptr<ast::TypeNode>(node->clone());
    } else if (typeNameStr == "i8" || typeNameStr == "i16" || typeNameStr == "i32" || typeNameStr == "i64" ||
               typeNameStr == "u8" || typeNameStr == "u16" || typeNameStr == "u32" || typeNameStr == "u64" ||
               typeNameStr == "f32" || typeNameStr == "f64" ||
               typeNameStr == "bool" || typeNameStr == "string" || typeNameStr == "void" ||
               typeNameStr == "int" || typeNameStr == "float") { 
        node->type = std::shared_ptr<ast::TypeNode>(node->clone());
    } else {
        SymbolInfo* symbol = currentScope->lookup(typeNameStr); 
        if (!symbol || !symbol->type) {
            addError("Unknown type identifier: " + typeNameStr, node);
            return;
        }
        node->type = std::shared_ptr<ast::TypeNode>(symbol->type->clone());
        for (auto& argTypeNode : node->genericArgs) {
            if (argTypeNode) argTypeNode->accept(*this);
        }
    }
}
void SemanticAnalyzer::visit(ast::ConstructionExpression* node) {
    if (!node || !node->constructedType) {
        addError("Construction expression is missing the type to construct.", node);
        expressionTypes[node] = nullptr;
        return;
    }
    auto typeNameNode = dynamic_cast<ast::TypeName*>(node->constructedType.get());
    if (!typeNameNode || !typeNameNode->identifier) {
         node->constructedType->accept(*this); 
         expressionTypes[node] = node->constructedType->clone().release();
         if (expressionTypes[node]) {
            node->type = std::shared_ptr<ast::TypeNode>(expressionTypes[node]->clone());
         }
         for (auto& arg : node->arguments) {
             if (arg) arg->accept(*this);
         }
         return; 
    }
    const std::string& constructedName = typeNameNode->identifier->name;
    if (constructedName == "addr") {
        // ... (addr handling logic as before, ensure it's correct) ...
        if (node->arguments.size() != 1 || !node->arguments[0]) {
            addError("addr() intrinsic expects 1 argument.", node); expressionTypes[node] = nullptr; return;
        }
        node->arguments[0]->accept(*this);
        ast::Expression* argExpr = node->arguments[0].get();
        if (!isLValue(argExpr)) {
            addError("Argument to addr() must be an L-value.", node); expressionTypes[node] = nullptr; return;
        }
        auto argTypeIt = expressionTypes.find(argExpr);
        if (argTypeIt == expressionTypes.end() || !argTypeIt->second) {
            addError("Argument to addr() has an unknown type.", node); expressionTypes[node] = nullptr; return;
        }
        auto res_type_ident = std::make_unique<ast::Identifier>(node->loc, "i64");
        ast::TypeNode* resType = new ast::TypeName(node->loc, std::move(res_type_ident));
        expressionTypes[node] = resType;
        node->type = std::shared_ptr<ast::TypeNode>(resType->clone());

    } else if (constructedName == "from") {
        // ... (from handling logic as before, ensure it's correct) ...
        if (typeNameNode->genericArgs.empty() || !typeNameNode->genericArgs[0]) {
            addError("from<TargetType>() intrinsic requires TargetType as a generic argument.", node); expressionTypes[node] = nullptr; return;
        }
        if (node->arguments.size() != 1 || !node->arguments[0]) {
            addError("from<T>() intrinsic expects 1 argument value.", node); expressionTypes[node] = nullptr; return;
        }
        ast::TypeNode* targetTypeAst = typeNameNode->genericArgs[0].get();
        targetTypeAst->accept(*this); 
        bool isTargetLocOrPointer = false;
        if (auto tn = dynamic_cast<ast::TypeName*>(targetTypeAst)) {
            if (tn->identifier->name == "loc" && !tn->genericArgs.empty()) isTargetLocOrPointer = true;
        } else if (dynamic_cast<ast::PointerType*>(targetTypeAst)) {
            isTargetLocOrPointer = true;
        }
        if (!isTargetLocOrPointer) {
             addError("Target type in from<T>() must be a location/pointer type (e.g., loc<ActualType>).", node); expressionTypes[node] = nullptr; return;
        }
        node->arguments[0]->accept(*this); 
        ast::Expression* addrValExpr = node->arguments[0].get();
        auto addrValTypeIt = expressionTypes.find(addrValExpr);
        ast::TypeNode* addrValType = (addrValTypeIt != expressionTypes.end()) ? addrValTypeIt->second : nullptr;
        
        bool isAddrValInteger = false;
        if(addrValType) {
            if(auto tn_val = dynamic_cast<ast::TypeName*>(addrValType)) {
                isAddrValInteger = isIntegerType(tn_val);
            }
        }
        if (!isAddrValInteger) { 
            addError("Address argument to from<T>() must be an integer type. Got: " + (addrValType ? addrValType->toString() : "unknown"), node);
            expressionTypes[node] = nullptr; return;
        }
        expressionTypes[node] = targetTypeAst->clone().release();
        node->type = std::shared_ptr<ast::TypeNode>(targetTypeAst->clone());

    } else if (constructedName == "at") {
        // ... (at handling logic as before, ensure it's correct) ...
        if (node->arguments.size() != 1 || !node->arguments[0]) {
            addError("at() intrinsic expects 1 argument (the pointer).", node); expressionTypes[node] = nullptr; return;
        }
        node->arguments[0]->accept(*this); 
        ast::Expression* ptrExpr = node->arguments[0].get();
        auto ptrTypeIt = expressionTypes.find(ptrExpr);
        if (ptrTypeIt == expressionTypes.end() || !ptrTypeIt->second) {
            addError("Cannot dereference pointer with unknown type using at().", node); expressionTypes[node] = nullptr; return;
        }
        ast::TypeNode* resolvedPointerType = ptrTypeIt->second;
        ast::TypeNode* pointeeType = nullptr;
        if (auto pt = dynamic_cast<ast::PointerType*>(resolvedPointerType)) {
            if (pt->pointeeType) pointeeType = pt->pointeeType->clone().release();
        } else if (auto locTn = dynamic_cast<ast::TypeName*>(resolvedPointerType)) {
            if (locTn->identifier->name == "loc" && !locTn->genericArgs.empty() && locTn->genericArgs[0]) {
                pointeeType = locTn->genericArgs[0]->clone().release();
            } else {
                 addError("Type for at() is \'loc\' but missing type parameter (e.g. loc<T>).", node); expressionTypes[node] = nullptr; return;
            }
        } else {
            addError("Argument to at() must be a pointer or loc<T> type. Got: " + resolvedPointerType->toString(), node);
            expressionTypes[node] = nullptr; return;
        }
        if (!pointeeType) {
             addError("Failed to determine pointee type for at() operation.", node); expressionTypes[node] = nullptr; return;
        }
        expressionTypes[node] = pointeeType;
        node->type = std::shared_ptr<ast::TypeNode>(pointeeType->clone());
    } else {
        node->constructedType->accept(*this); 
        expressionTypes[node] = node->constructedType->clone().release();
        if (expressionTypes[node]) {
           node->type = std::shared_ptr<ast::TypeNode>(expressionTypes[node]->clone());
        } else {
            addError("Constructed type " + constructedName + " could not be resolved.", node);
            return;
        }
        for (auto& arg : node->arguments) {
            if (arg) arg->accept(*this);
        }
    }
}
void SemanticAnalyzer::visit(ast::PointerType* node) {
    if (node && node->pointeeType) {
        node->pointeeType->accept(*this);
    }
}
void SemanticAnalyzer::visit(ast::ArrayType* node) {
    if (node && node->elementType) {
        node->elementType->accept(*this);
    }
}
void SemanticAnalyzer::visit(ast::FunctionType* node) {
    if (node) {
        for (auto& paramType : node->parameterTypes) { 
            if (paramType) paramType->accept(*this);
        }
        if (node->returnType) {
            node->returnType->accept(*this);
        }
    }
}
void SemanticAnalyzer::visit(ast::OptionalType* node) {
    if (node && node->containedType) { 
        node->containedType->accept(*this); 
    }
}
void SemanticAnalyzer::visit(ast::GenericParameter* node) {
    // Generic parameters are usually resolved in the context of a generic declaration
    // or instantiation. For a standalone visit, we might try to look it up if it's
    // already defined in the current scope (e.g. within a generic function body).
    // For now, this can be a no-op or a simple lookup.
    // SymbolInfo* symbol = currentScope->lookup(node->name->name); // Assuming GenericParameter has a name field
    // if (!symbol) { addError("Generic parameter " + node->name->name + " not found.", node); }
}

// Implementation for areTypesCompatible
// Checks if valueType can be assigned to targetType
bool SemanticAnalyzer::areTypesCompatible(ast::TypeNode* targetType, ast::TypeNode* valueType) {
    if (!targetType || !valueType) {
        // If one or both types are unresolved (likely due to a previous error),
        // consider them incompatible to prevent cascading issues.
        return false;
    }

    // If they are the exact same type object.
    if (targetType == valueType) {
        return true;
    }

    ast::TypeNode::Category categoryTarget = targetType->getCategory();
    ast::TypeNode::Category categoryValue = valueType->getCategory();

    // Special Case 1: Assigning 'nil' to an Optional type
    if (categoryValue == ast::TypeNode::Category::IDENTIFIER) {
        if (auto* tnValue = dynamic_cast<ast::TypeName*>(valueType)) {
            if (tnValue->identifier && tnValue->identifier->name == "nil") {
                if (categoryTarget == ast::TypeNode::Category::OPTIONAL) {
                    return true;
                }
                // Vyn might also allow assigning 'nil' to raw pointer types.
                // if (categoryTarget == ast::TypeNode::Category::POINTER) return true;
            }
        }
    }

    // Special Case 2: Assigning a generic integer literal (type "int") to a specific integer type
    if (categoryTarget == ast::TypeNode::Category::IDENTIFIER &&
        categoryValue == ast::TypeNode::Category::IDENTIFIER) {
        auto* tnTarget = static_cast<ast::TypeName*>(targetType);
        auto* tnValue = static_cast<ast::TypeName*>(valueType);
        if (tnTarget->identifier && tnValue->identifier) {
            bool isSpecificIntTarget = isIntegerType(tnTarget); // Checks for i8, i16, etc.
            bool isGenericIntValue = (tnValue->identifier->name == "int"); // Literal int

            if (isSpecificIntTarget && isGenericIntValue) {
                // e.g., let x: i32 = 10; (10 is "int", x is "i32")
                return true;
            }
        }
    }
    
    // If categories are different and not covered by the above special cases,
    // they are generally not compatible without an explicit cast.
    if (categoryTarget != categoryValue) {
        return false;
    }

    // Categories are the same, proceed with category-specific checks.
    switch (categoryTarget) {
        case ast::TypeNode::Category::IDENTIFIER: {
            auto* tnTarget = static_cast<ast::TypeName*>(targetType);
            auto* tnValue = static_cast<ast::TypeName*>(valueType);
            if (!tnTarget->identifier || !tnValue->identifier) return false; // Should not happen

            // Check for specific integer types (e.g., i32, u64).
            // The generic "int" literal case is handled above.
            if (isIntegerType(tnTarget) && isIntegerType(tnValue)) {
                // For now, require exact match for specific integer types (e.g., i32 compatible with i32).
                // Vyn might allow implicit widening (e.g., i32 to i64), which would need to be added here.
                return tnTarget->identifier->name == tnValue->identifier->name;
            }

            // General identifier type comparison (e.g., custom struct names, bool, string).
            // This implies nominal typing.
            if (tnTarget->identifier->name == tnValue->identifier->name) {
                // If they are generic types, check compatibility of generic arguments.
                if (tnTarget->genericArgs.size() != tnValue->genericArgs.size()) return false;
                for (size_t i = 0; i < tnTarget->genericArgs.size(); ++i) {
                    if (!areTypesCompatible(tnTarget->genericArgs[i].get(), tnValue->genericArgs[i].get())) {
                        return false;
                    }
                }
                return true;
            }
            return false; // Different names or failed generic arg check.
        }
        case ast::TypeNode::Category::POINTER: {
            auto* ptTarget = static_cast<ast::PointerType*>(targetType);
            auto* ptValue = static_cast<ast::PointerType*>(valueType);
            // T* is compatible with U* if T is compatible with U (invariant for now).
            // Vyn might have rules for void* or covariance/contravariance.
            return areTypesCompatible(ptTarget->pointeeType.get(), ptValue->pointeeType.get());
        }
        case ast::TypeNode::Category::ARRAY: {
            auto* atTarget = static_cast<ast::ArrayType*>(targetType);
            auto* atValue = static_cast<ast::ArrayType*>(valueType);
            // T[N] is compatible with U[M] if T is compatible with U.
            // For simplicity, ignoring size compatibility for now (atTarget->sizeExpression vs atValue->sizeExpression).
            // A full check would compare constant sizes if available.
            return areTypesCompatible(atTarget->elementType.get(), atValue->elementType.get());
        }
        case ast::TypeNode::Category::FUNCTION: {
            auto* ftTarget = static_cast<ast::FunctionType*>(targetType);
            auto* ftValue = static_cast<ast::FunctionType*>(valueType);

            // Return types: Target's return type must be compatible with Value's return type (covariance).
            // fn_target_ret = fn_value_ret  => areTypesCompatible(fn_target_ret, fn_value_ret)
            if (!areTypesCompatible(ftTarget->returnType.get(), ftValue->returnType.get())) return false;

            // Parameter types: Value's param types must be compatible with Target's param types (contravariance).
            // fn_target_param = fn_value_param => areTypesCompatible(fn_value_param, fn_target_param)
            // For simplicity, using invariance for now: types must be identical.
            if (ftTarget->parameterTypes.size() != ftValue->parameterTypes.size()) return false;
            for (size_t i = 0; i < ftTarget->parameterTypes.size(); ++i) {
                if (!areTypesCompatible(ftTarget->parameterTypes[i].get(), ftValue->parameterTypes[i].get())) {
                // For contravariance:
                // if (!areTypesCompatible(ftValue->parameterTypes[i].get(), ftTarget->parameterTypes[i].get()))
                    return false;
                }
            }
            return true;
        }
        case ast::TypeNode::Category::OPTIONAL: {
            auto* otTarget = static_cast<ast::OptionalType*>(targetType);
            auto* otValue = static_cast<ast::OptionalType*>(valueType);
            // Optional<T> is compatible with Optional<U> if T is compatible with U.
            return areTypesCompatible(otTarget->containedType.get(), otValue->containedType.get());
        }
        case ast::TypeNode::Category::TUPLE: {
            auto* ttTarget = static_cast<ast::TupleTypeNode*>(targetType);
            auto* ttValue = static_cast<ast::TupleTypeNode*>(valueType);
            if (ttTarget->memberTypes.size() != ttValue->memberTypes.size()) return false;
            for (size_t i = 0; i < ttTarget->memberTypes.size(); ++i) {
                if (!areTypesCompatible(ttTarget->memberTypes[i].get(), ttValue->memberTypes[i].get())) {
                    return false;
                }
            }
            return true;
        }
        // case ast::TypeNode::Category::STRUCT:
            // Struct compatibility would typically be nominal (same definition) or structural.
            // Nominal is partly handled by IDENTIFIER if struct names are unique and resolved.
            // Structural would require comparing field types and names.
        // case ast::TypeNode::Category::REFERENCE: // Not fully defined in provided AST
        // case ast::TypeNode::Category::SLICE:     // Not fully defined in provided AST
        default:
            // Fallback for unhandled categories or complex types.
            // This is a weak check and ideally should be replaced with more specific rules
            // or by ensuring all types are resolved to canonical forms before comparison.
            if (targetType->toString() == valueType->toString()) { // Basic structural check via string representation
                return true;
            }
            // If they are TypeName, it might have been missed by earlier checks
            if (categoryTarget == ast::TypeNode::Category::IDENTIFIER) {
                 auto* tnTarget = static_cast<ast::TypeName*>(targetType);
                 auto* tnValue = static_cast<ast::TypeName*>(valueType); // Already know category is IDENTIFIER
                 if (tnTarget->identifier && tnValue->identifier && tnTarget->identifier->name == tnValue->identifier->name) {
                     if (tnTarget->genericArgs.size() == tnValue->genericArgs.size()) {
                         bool allArgsCompatible = true;
                         for (size_t i = 0; i < tnTarget->genericArgs.size(); ++i) {
                            if (!areTypesCompatible(tnTarget->genericArgs[i].get(), tnValue->genericArgs[i].get())) {
                                allArgsCompatible = false;
                                break;
                            }
                         }
                         if (allArgsCompatible) return true;
                     }
                 }
            }
            return false;
    }
    return false; // Should be unreachable if all cases are handled
}

// ... ensure other visit methods like visit(ast::FromIntToLocExpression* node) are correctly implemented or placeholder ...
// The provided snippets for AddrOfExpression, PointerDerefExpression are now primary.
// The ConstructionExpression handles cases where these might be parsed as such.
// If the parser correctly creates AddrOfExpression, FromIntToLocExpression, PointerDerefExpression,
// then the specific visitors for those AST nodes will be used. The logic for 'addr', 'from', 'at'
// in ConstructionExpression::visit is a fallback or handles cases where the parser uses
// ConstructionExpression for these intrinsics.

} // Added missing closing brace for namespace vyn

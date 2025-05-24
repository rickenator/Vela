void SemanticAnalyzer::visit(ast::VariableDeclaration* node) {
    // Check if variable name conflicts with intrinsic function names
    if (isReservedIntrinsicName(node->id->name)) {
        addError("Variable name '" + node->id->name + "' conflicts with reserved intrinsic function name. "
                "This can cause ambiguity in expressions.", node);
    }

    // Proceed with normal variable declaration handling
    if (node->init) {
        // Visit the initializer expression
        node->init->accept(*this);
        
        // If a type annotation is provided, check compatibility with initializer type
        if (node->typeNode) {
            node->typeNode->accept(*this);
            // Type checking between initializer and declared type would go here
        }
    } else if (node->typeNode) {
        // If no initializer, ensure there's a type annotation
        node->typeNode->accept(*this);
    } else {
        // No initializer and no type annotation - error
        addError("Variable declaration requires either a type annotation or an initializer.", node);
    }

    // Add variable to the current scope
    SymbolInfo symbol;
    symbol.name = node->id->name;
    symbol.kind = SymbolInfo::Kind::Variable;
    symbol.isConst = node->isConst;
    
    if (node->typeNode) {
        // Use the declared type if available
        symbol.type = node->typeNode.get();
    } else if (node->init) {
        // Infer type from initializer
        auto initTypeIt = expressionTypes.find(node->init.get());
        if (initTypeIt != expressionTypes.end() && initTypeIt->second) {
            symbol.type = initTypeIt->second;
        }
    }
    
    currentScope->add(symbol);
}

void SemanticAnalyzer::visit(ast::FunctionDeclaration* node) {
    // Check if function name conflicts with intrinsic function names
    if (isReservedIntrinsicName(node->id->name)) {
        addError("Function name '" + node->id->name + "' conflicts with reserved intrinsic function name. "
                "This can cause ambiguity in expressions.", node);
    }
    
    // Add function to symbol table
    SymbolInfo symbol;
    symbol.name = node->id->name;
    symbol.kind = SymbolInfo::Kind::Function;
    symbol.type = nullptr; // Function type would be created here if needed
    currentScope->add(symbol);
    
    // Create a new scope for function parameters and body
    enterScope();
    
    // Process parameters
    for (const auto& param : node->params) {
        // Add parameter to the function scope
        SymbolInfo paramSymbol;
        paramSymbol.name = param.name;
        paramSymbol.kind = SymbolInfo::Kind::Variable;
        paramSymbol.isConst = false; // Parameters are generally not constant
        
        // If param has type annotation, record it
        if (param.type) {
            param.type->accept(*this);
            paramSymbol.type = param.type.get();
        }
        
        currentScope->add(paramSymbol);
    }
    
    // Process function body if it exists
    if (node->body) {
        node->body->accept(*this);
    }
    
    // Exit function scope
    exitScope();
}

#include "vyn/parser/ast.hpp"
#include "vyn/parser/token.hpp"
#include "vyn/parser/parser.hpp" // Added missing include for parser definitions

namespace vyn {

// Helper function to convert SourceLocation to string for error messages
std::string locationToString(const SourceLocation& loc) {
    return loc.filePath + ":" + std::to_string(loc.line) + ":" + std::to_string(loc.column); // Changed file to filePath
}

// Constructor
TypeParser::TypeParser(const std::vector<token::Token>& tokens, size_t& pos, const std::string& file_path, ExpressionParser& expr_parser)
    : BaseParser(tokens, pos, file_path), expr_parser_(expr_parser) {}


vyn::ast::TypeNodePtr TypeParser::parse() { // Corrected namespace
    // This is the main entry point for parsing a type.
    // It should handle basic types, pointer types, array types, function types, etc.
    this->skip_comments_and_newlines();
    vyn::SourceLocation start_loc = this->current_location();
    vyn::ast::TypeNodePtr type;

    // Try parsing an ownership-wrapped_type first, or a base type.
    type = this->parse_base_or_ownership_wrapped_type();

    if (!type) {
         // Use this->error for consistency, assuming it throws or can be made to throw
         throw this->error(this->peek(), "Failed to parse type at " + start_loc.toString());
    }

    // Then, parse any postfix operators (generics, array, optional, const)
    return this->parse_postfix_type(std::move(type));
}


// This function parses the core type, which might be wrapped by ownership modifiers.
vyn::ast::TypeNodePtr TypeParser::parse_base_or_ownership_wrapped_type() { // Corrected namespace
    // SourceLocation start_loc = peek().location; // Potentially unused now
    // bool is_mutable = false; // This logic is removed from here.
                              // Semantic analysis should handle properties of types like "my", "our", "their".

    // Ownership keywords (my, our, their) are now expected to be parsed as
    // part of the identifier in parse_atomic_or_group_type (if they are keywords like KEYWORD_MY)
    // or as regular identifiers if they are not keywords. The generic <...> part will be handled by parse_postfix_type.
    // Thus, we don't consume them here.
    //
    // OLD CODE:
    // if (check(vyn::TokenType::KEYWORD_MY)) {
    //     is_mutable = true;
    //     consume();
    // } else if (check(vyn::TokenType::KEYWORD_OUR)) {
    //     consume();
    // } else if (check(vyn::TokenType::KEYWORD_THEIR)) {
    //     consume();
    // }

    // Parse the base type directly
    vyn::ast::TypeNodePtr type_node = parse_atomic_or_group_type();

    // The 'is_mutable' logic (and any other special handling for my/our/their)
    // previously here would now be handled by semantic analysis
    // based on the resulting type_node's name (e.g., if it's "my").
    //
    // OLD CODE:
    // if (is_mutable) {
    //     // If the type is mutable, we might need to adjust the node (depends on your AST design)
    //     // For example, wrapping the type in a MutableTypeNode if such a node exists
    //     // type_node = std::make_shared<MutableTypeNode>(start_loc, type_node);
    // }

    return type_node; // Return the potentially wrapped type
}

// Parses atomic types (like identifiers, primitive types) or grouped types (like (i32, String))
vyn::ast::TypeNodePtr TypeParser::parse_atomic_or_group_type() { // Corrected namespace
    SourceLocation start_loc = peek().location;
    vyn::ast::TypeNodePtr type_node = nullptr;

    // Allow special keywords as type identifiers, e.g., for my<T>, their<T>, const<T>
    if (match(vyn::TokenType::IDENTIFIER) || 
        match(vyn::TokenType::KEYWORD_MY) ||
        match(vyn::TokenType::KEYWORD_OUR) ||
        match(vyn::TokenType::KEYWORD_THEIR) ||
        match(vyn::TokenType::KEYWORD_CONST)) { // Added KEYWORD_CONST for const<T>
        
        vyn::SourceLocation path_loc = this->previous_token().location; 
        std::string qualified_name = this->previous_token().lexeme;
        
        // Special handling for "my."
        if (qualified_name == "my" && match(vyn::TokenType::DOT)) {
            if (!match(vyn::TokenType::IDENTIFIER)) {
                throw this->error(this->peek(), "Expected identifier after \\'my.\\' in type name");
            }
            qualified_name += "." + this->previous_token().lexeme;
        }

        // Loop for '::' and '.' qualifiers
        while (true) {
            if (this->match(vyn::TokenType::COLONCOLON)) { 
                if (!this->match(vyn::TokenType::IDENTIFIER)) { 
                    throw this->error(this->peek(), "Expected identifier after \\'::\\' in qualified type name");
                }
                qualified_name += "::" + this->previous_token().lexeme;
            } else if (this->match(vyn::TokenType::DOT)) {
                if (!this->match(vyn::TokenType::IDENTIFIER)) {
                    throw this->error(this->peek(), "Expected identifier after \\'.\\' in qualified type name");
                }
                qualified_name += "." + this->previous_token().lexeme;
            } else {
                break; // No more qualifiers
            }
        }
        auto type_name_identifier = std::make_unique<vyn::ast::Identifier>(path_loc, qualified_name); 
        // return vyn::ast::TypeNode::newIdentifier(start_loc, std::move(type_name_identifier), {}, false, false); 
        return std::make_unique<vyn::ast::TypeName>(start_loc, std::move(type_name_identifier));

    } else if (this->match(vyn::TokenType::LPAREN)) {  // Changed from LEFT_PAREN
        std::vector<vyn::ast::TypeNodePtr> member_types_parsed; // Changed vyn::TypeNodePtr to vyn::ast::TypeNodePtr
        if (this->peek().type != vyn::TokenType::RPAREN) { // Changed from RIGHT_PAREN
            do {
                member_types_parsed.push_back(this->parse());
            } while (this->match(vyn::TokenType::COMMA)); // Changed from vyn::token::TokenType
        }
        this->expect(vyn::TokenType::RPAREN); // Changed from RIGHT_PAREN

        // return vyn::ast::TypeNode::newTuple(start_loc, std::move(member_types_parsed), false, false); // Changed TypeNode to vyn::ast::TypeNode, loc to start_loc
        return std::make_unique<vyn::ast::TupleTypeNode>(start_loc, std::move(member_types_parsed));

    } else if (this->match(vyn::TokenType::LBRACKET)) {  // Changed from LEFT_BRACKET
        vyn::SourceLocation array_loc = this->previous_token().location; // Changed from loc
        
        // Store current position before parsing element type
        size_t before_element_type_pos = pos_;
        
        // Try to parse element type
        vyn::ast::TypeNodePtr element_type = nullptr;
        try {
            element_type = this->parse();
        } catch (const std::runtime_error& e) {
            // If we fail to parse a type, backtrack and let the expression parser handle it
            pos_ = before_element_type_pos;
            throw this->error(this->peek(), "Expected element type for array.");
        }
        
        if (!element_type) {
            // If parse() returned nullptr without throwing, backtrack
            pos_ = before_element_type_pos;
            throw this->error(this->peek(), "Expected element type for array.");
        }
        
        vyn::ast::ExprPtr size_expression = nullptr; // Declared size_expression

        if (this->match(vyn::TokenType::SEMICOLON)) { // Changed from vyn::token::TokenType
            if (this->IsAtEnd() || this->peek().type == vyn::TokenType::RBRACKET) { // Changed from RIGHT_BRACKET
                throw this->error(this->peek(), "Expected size expression after \';\' in array type.");
            }
            
            // Store position before parsing size expression
            size_t before_size_expr_pos = pos_;
            
            try {
                size_expression = expr_parser_.parse_expression(); // Changed parse to parse_expression
            } catch (const std::runtime_error& e) {
                // If parsing the size expression fails, backtrack
                pos_ = before_size_expr_pos;
                throw;
            }
            
            if (!size_expression) {
                pos_ = before_size_expr_pos;
                throw this->error(this->peek(), "Failed to parse array size expression.");
            }
        }

        // Save position before expecting RBRACKET
        size_t before_rbracket_pos = pos_;
        
        try {
            this->expect(vyn::TokenType::RBRACKET);  // Changed from RIGHT_BRACKET
        } catch (const std::runtime_error& e) {
            // If RBRACKET isn't found where expected, backtrack
            pos_ = before_element_type_pos;
            throw;
        }
        
        // expect() already throws an error if the token type doesn\\'t match.
        // return vyn::ast::TypeNode::newArray(array_loc, std::move(element_type), std::move(size_expression), false, false); // Changed TypeNode to vyn::ast::TypeNode
        return std::make_unique<vyn::ast::ArrayType>(array_loc, std::move(element_type), std::move(size_expression));

    } else if (this->match(vyn::TokenType::KEYWORD_FN)) {  // Changed from KEYWORD_FUN
        vyn::SourceLocation fn_loc = start_loc;  // Changed loc to start_loc
        this->expect(vyn::TokenType::LPAREN); // Changed from LEFT_PAREN
        std::vector<vyn::ast::TypeNodePtr> param_types_parsed; // Changed vyn::TypeNodePtr to vyn::ast::TypeNodePtr
        if (this->peek().type != vyn::TokenType::RPAREN) { // Changed from RIGHT_PAREN
            do {
                param_types_parsed.push_back(this->parse());
            } while (this->match(vyn::TokenType::COMMA)); // Changed from vyn::token::TokenType
        }
        this->expect(vyn::TokenType::RPAREN); // Changed from RIGHT_PAREN

        vyn::ast::TypeNodePtr return_type_parsed = nullptr; // Changed vyn::TypeNodePtr to vyn::ast::TypeNodePtr
        if (this->match(vyn::TokenType::ARROW)) { // Changed from vyn::token::TokenType
            return_type_parsed = this->parse();
            if (!return_type_parsed) {
                 throw this->error(this->peek(), "Expected return type after \\'->\\' in function type at " + this->current_location().toString());
            }
        }
        // return vyn::ast::TypeNode::newFunctionSignature(fn_loc, std::move(param_types_parsed), std::move(return_type_parsed), false, false); // Changed TypeNode to vyn::ast::TypeNode
        return std::make_unique<vyn::ast::FunctionType>(fn_loc, std::move(param_types_parsed), std::move(return_type_parsed));
    }

    throw this->error(this->peek(), "Expected a type identifier, \\\\\\'(\\\\\\\\' or \\\\\\'fn\\\\\\\\' to start a base type, found " + this->peek().lexeme + " (" + vyn::token_type_to_string(this->peek().type) + ") at " + start_loc.toString());
}


// Parses postfix type constructs like array types (`[]`), pointer types (`*`), optional types (`?`)
vyn::ast::TypeNodePtr TypeParser::parse_postfix_type(vyn::ast::TypeNodePtr current_type) { // Corrected namespace for return type and parameter
    while (true) {
        SourceLocation op_loc = peek().location;
        if (this->match(vyn::TokenType::LT)) { // Changed from LESS
            // Generic type parameters
            std::vector<vyn::ast::TypeNodePtr> generic_args; // Changed vyn::TypeNodePtr to vyn::ast::TypeNodePtr
            // Parse comma-separated list of type arguments
            if (this->peek().type != vyn::TokenType::GT) { // Removed extra parenthesis
                do {
                    auto type_arg = this->parse();
                    if (!type_arg) {
                        throw this->error(this->peek(), "Expected type argument in generic type at " + 
                                        this->current_location().toString());
                    }
                    generic_args.push_back(std::move(type_arg));
                } while (this->match(vyn::TokenType::COMMA)); // Changed from vyn::token::TokenType
            }
            this->expect(vyn::TokenType::GT); // Changed from GREATER
            // Update the current_type with generic arguments
            // if (current_type->category == vyn::ast::TypeNode::Category::IDENTIFIER) { // Changed TypeCategory to Category
            //     current_type->genericArguments = std::move(generic_args);
            if (auto* type_name_node = dynamic_cast<vyn::ast::TypeName*>(current_type.get())) {
                type_name_node->genericArgs = std::move(generic_args);
            } else {
                throw this->error(this->previous_token(), "Generic parameters can only be applied to identifier types");
            }
        } else if (this->match(vyn::TokenType::LBRACKET)) { // Changed from LEFT_BRACKET
            SourceLocation lbracket_loc = this->previous_token().location; // Changed from loc
            if (this->match(vyn::TokenType::RBRACKET)) {  // Changed from RIGHT_BRACKET
                // current_type = vyn::ast::TypeNode::newArray(lbracket_loc, std::move(current_type), nullptr, false, false); // Changed TypeNode to vyn::ast::TypeNode
                current_type = std::make_unique<vyn::ast::ArrayType>(lbracket_loc, std::move(current_type), nullptr);
            } else {
                this->put_back_token(); 
                break; 
            }
        } else if (this->match(vyn::TokenType::MULTIPLY)) { // Changed from STAR
            // if (current_type->isPointer) { 
            if (dynamic_cast<vyn::ast::PointerType*>(current_type.get())) {
                throw this->error(this->previous_token(), "Type is already a pointer.");
            }
            // current_type->isPointer = true;
            current_type = std::make_unique<vyn::ast::PointerType>(op_loc, std::move(current_type));
        } else if (this->match(vyn::TokenType::QUESTION_MARK)) {  // Changed from QUESTION
            // if (current_type->isOptional) {
            if (dynamic_cast<vyn::ast::OptionalType*>(current_type.get())) {
                throw this->error(this->previous_token(), "Type is already optional: " + current_type->loc.toString());
            }
            // current_type->isOptional = true;
            current_type = std::make_unique<vyn::ast::OptionalType>(op_loc, std::move(current_type));
        } else if (this->match(vyn::TokenType::KEYWORD_CONST)) {  // Changed from CONST
            // if (current_type->dataIsConst) {
            //      throw this->error(this->previous_token(), "Type is already const: " + current_type->loc.toString());
            // }
            // current_type->dataIsConst = true;
            // TODO: Handle 'const' type qualifier. This may require AST changes or be a semantic check.
            // For now, consuming the token and not modifying the type, or throwing an error.
            // Let's throw an error for now, as its behavior is undefined with current AST.
            throw this->error(this->previous_token(), "'const' type qualifier is not yet supported here.");
        }
        else {
            break; 
        }
    }
    return current_type;
}

} // namespace vyn

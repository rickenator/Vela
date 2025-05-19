#include "vyn/parser/parser.hpp" // For BaseParser and other parser components
#include "vyn/parser/ast.hpp"      // For AST node types like IntegerLiteral, etc.
#include "vyn/parser/token.hpp"    // For TokenType and Token
#include <stdexcept>               // For std::runtime_error
#include <algorithm> // Required for std::any_of, if used by match or other helpers
#include <functional> // Required for std::function
#include <vector> // Required for std::vector
// Add iostream if not already pulled in by parser.hpp for std::cerr, though DEBUG_PRINT should handle it.
// #include <iostream> 
// Add string if not already pulled in for std::to_string, though DEBUG_PRINT should handle it.
// #include <string>


namespace vyn {

    // Constructor
    ExpressionParser::ExpressionParser(const std::vector<token::Token>& tokens, size_t& pos, const std::string& file_path)
        : BaseParser(tokens, pos, file_path) {}

    // Public method to start parsing an expression
    vyn::ast::ExprPtr ExpressionParser::parse_expression() {
        DEBUG_PRINT("Entering parse_expression");
        DEBUG_TOKEN(peek());
        // This should call the highest precedence expression parser in your setup,
        // often assignment or logical OR. Assuming parse_assignment_expr() is the entry.
        auto expr = parse_assignment_expr(); 
        DEBUG_PRINT("Exiting parse_expression");
        if (expr) {
            DEBUG_PRINT("Successfully parsed expression.");
        } else {
            DEBUG_PRINT("Failed to parse expression or expression was null.");
        }
        DEBUG_TOKEN(peek()); // Token after parsing the whole expression
        return expr;
    }

    // Parses assignment expressions (e.g., x = 10, y += 5)
    vyn::ast::ExprPtr ExpressionParser::parse_assignment_expr() {
        vyn::ast::ExprPtr left = parse_logical_or_expr(); // Precedence: logical OR is higher than assignment

        if (match(TokenType::EQ) || match(TokenType::PLUSEQ) || match(TokenType::MINUSEQ) ||
            match(TokenType::MULTIPLYEQ) || match(TokenType::DIVEQ) || match(TokenType::MODEQ) ||
            match(TokenType::LSHIFTEQ) || match(TokenType::RSHIFTEQ) ||
            match(TokenType::BITWISEANDEQ) || match(TokenType::BITWISEOREQ) || match(TokenType::BITWISEXOREQ) ||
            match(TokenType::COLONEQ)) { // Added COLONEQ for completeness, though its semantics might differ
            
            token::Token op_token = previous_token();
            SourceLocation op_loc = op_token.location; // Location of the operator

            // Check if left is a valid LValue (identifier, member access, array element, pointer deref)
            bool is_valid_lvalue = false;
            if (dynamic_cast<ast::Identifier*>(left.get())) {
                is_valid_lvalue = true;
            } else if (auto member_expr = dynamic_cast<ast::MemberExpression*>(left.get())) {
                is_valid_lvalue = true;
            } else if (auto array_elem_expr = dynamic_cast<ast::ArrayElementExpression*>(left.get())) {
                is_valid_lvalue = true;
            } else if (dynamic_cast<ast::PointerDerefExpression*>(left.get())) {
                // Pointer dereference assignment (at(ptr))
                is_valid_lvalue = true;
            }
            // TODO: Consider adding ast::ArrayElementExpression here if not handled elsewhere - It is handled above.

            if (is_valid_lvalue) {
                vyn::ast::ExprPtr right = parse_expression(); // Parse the right-hand side
                return std::make_unique<ast::AssignmentExpression>(op_loc, std::move(left), op_token, std::move(right));
            } else {
                std::string expr_type_str = "unknown expression type";
                if (left) {
                    if (dynamic_cast<ast::Identifier*>(left.get())) expr_type_str = "identifier";
                    else if (dynamic_cast<ast::MemberExpression*>(left.get())) expr_type_str = "member expression";
                    else if (dynamic_cast<ast::ArrayElementExpression*>(left.get())) expr_type_str = "array element expression";
                    else if (dynamic_cast<ast::CallExpression*>(left.get())) expr_type_str = "call expression";
                    // Add other expression types if needed for better error messages
                }
                throw error(op_token, "Invalid left-hand side (" + expr_type_str + ") in assignment expression");
            }
        }
        return left; // Not an assignment, just return the parsed left expression
    }

    vyn::ast::ExprPtr ExpressionParser::parse_call_expression(vyn::ast::ExprPtr callee_expr) {
        std::vector<vyn::ast::ExprPtr> arguments;
        SourceLocation call_loc = previous_token().location; 

        if (!match(TokenType::RPAREN)) { 
            do {
                arguments.push_back(parse_expression());
            } while (match(TokenType::COMMA));
            expect(TokenType::RPAREN);
        }
        return std::make_unique<ast::CallExpression>(call_loc, std::move(callee_expr), std::move(arguments));
    }

    vyn::ast::ExprPtr ExpressionParser::parse_member_access(vyn::ast::ExprPtr object) {
        SourceLocation member_loc = peek().location;
        if (peek().type == TokenType::IDENTIFIER) {
            token::Token property_token = consume(); 
            auto property_identifier = std::make_unique<ast::Identifier>(property_token.location, property_token.lexeme);
            return std::make_unique<ast::MemberExpression>(member_loc, std::move(object), std::move(property_identifier), false /* not computed */);
        }
        // this->errors.push_back("Expected identifier for member access at " + location_to_string(member_loc));
        throw std::runtime_error("Expected identifier for member access at " + location_to_string(member_loc));
        return nullptr; 
    }

    vyn::ast::ExprPtr ExpressionParser::parse_primary() {
        DEBUG_PRINT("Entering parse_primary");
        DEBUG_TOKEN(peek());
        SourceLocation loc = peek().location;

        // Handle if statements as expressions (e.g. `let x = if cond { 1 } else { 0 }`)
        if (match(TokenType::KEYWORD_IF)) {
            expect(TokenType::LPAREN); // Expect \'(\' after \'if\'
            vyn::ast::ExprPtr condition = parse_expression(); // Condition
            expect(TokenType::RPAREN); // Expect \')\' after condition

            expect(TokenType::LBRACE); // Then block
            vyn::ast::ExprPtr then_branch = parse_expression(); // Expression inside then block
            expect(TokenType::RBRACE);

            vyn::ast::ExprPtr else_branch = nullptr;
            if (match(TokenType::KEYWORD_ELSE)) {
                expect(TokenType::LBRACE); // Else block
                else_branch = parse_expression(); // Expression inside else block
                expect(TokenType::RBRACE);
            } else {
                // In Vyn, if-expressions might require an else branch.
                // If not, the type system would need to handle it (e.g. Option type or similar)
                // For now, let's make else mandatory for if-expressions.
                throw error(peek(), "Expected 'else' branch for if-expression.");
            }
            return std::make_unique<ast::IfExpression>(loc, std::move(condition), std::move(then_branch), std::move(else_branch));
        }

        // Try to parse TypeName(arguments) - Constructor Call
        // This requires being able to parse a TypeNode first.
        // We need a TypeParser instance here.
        // For now, we\'ll assume TypeParser is part of `this` parser or can be created.
        // This is a lookahead and backtrack mechanism.
        size_t initial_pos = pos_;
        try {
            TypeParser type_parser(tokens_, pos_, current_file_path_, *this); // Pass *this for ExpressionParser reference
            ast::TypeNodePtr type_node = type_parser.parse(); // Call parse() instead of parse_type_annotation()
            
            if (type_node && match(TokenType::LPAREN)) { // Successfully parsed a type and found \'(\'
                // It's a ConstructionExpression
                std::vector<vyn::ast::ExprPtr> arguments;
                SourceLocation call_loc = previous_token().location; 
                if (!match(TokenType::RPAREN)) { 
                    do {
                        arguments.push_back(parse_expression());
                    } while (match(TokenType::COMMA));
                    expect(TokenType::RPAREN);
                }
                // The location for ConstructionExpression should ideally span from type to ')'
                // For now, using call_loc (location of LPAREN) is a simplification.
                // A better loc would be type_node->loc combined with the RPAREN location.
                return std::make_unique<ast::ConstructionExpression>(type_node->loc, std::move(type_node), std::move(arguments));
            } else {
                // Not a TypeName(...), backtrack
                pos_ = initial_pos;
            }
        } catch (const std::runtime_error& e) {
            // Parsing type failed, or not followed by \'(\', backtrack
            pos_ = initial_pos;
            // Log or handle error if needed, or just proceed to other parsing rules
        }
        // If it wasn\'t a TypeName(...), reset pos_ and try other primary expression forms.
        // Ensure pos_ is correctly managed by TypeParser or reset it manually.
        // The TypeParser must not consume tokens if it fails to parse a complete type for this to work well.


        // Try to parse [Type; Size]() - Array Initialization
        if (peek().type == TokenType::LBRACKET) { // Check before consuming for ArrayInit
            size_t before_array_init_pos = pos_;
            token::Token lbracket_peek = peek(); // Peek for location
            consume(); // Consume LBRACKET for ArrayInit attempt
            DEBUG_PRINT("Attempting to parse ArrayInitialization: [Type; Size]()");
            DEBUG_TOKEN(lbracket_peek); // Log the LBRACKET that was consumed

            try {
                TypeParser type_parser_for_array(tokens_, pos_, current_file_path_, *this); // Pass *this for ExpressionParser reference
                ast::TypeNodePtr element_type = nullptr;
                
                // Catch any exceptions from the type parser and backtrack
                try {
                    element_type = type_parser_for_array.parse(); // Call parse()
                } catch (const std::runtime_error& e) {
                    // Failed to parse type - this might be a list comprehension or array literal
                    pos_ = before_array_init_pos;
                    goto regular_array_literal; // Continue with normal array literal parsing
                }

                if (element_type && match(TokenType::SEMICOLON)) {
                    ast::ExprPtr size_expr = nullptr;
                    
                    // Try to parse size expression
                    try {
                        size_expr = parse_expression();
                    } catch (const std::runtime_error& e) {
                        // Failed to parse size expression
                        pos_ = before_array_init_pos;
                        goto regular_array_literal;
                    }
                    
                    if (!size_expr) {
                        pos_ = before_array_init_pos;
                        goto regular_array_literal;
                    }
                    
                    try {
                        expect(TokenType::RBRACKET);
                    } catch (const std::runtime_error& e) {
                        // Failed to find RBRACKET
                        pos_ = before_array_init_pos;
                        goto regular_array_literal;
                    }
                    
                    if (match(TokenType::LPAREN)) {
                        try {
                            expect(TokenType::RPAREN);
                            // It's an ArrayInitializationExpression
                            return std::make_unique<ast::ArrayInitializationExpression>(loc, std::move(element_type), std::move(size_expr));
                        } catch (const std::runtime_error& e) {
                            // Failed to find RPAREN
                            pos_ = before_array_init_pos;
                            goto regular_array_literal;
                        }
                    }
                    // If not LPAREN RPAREN, it's something else, backtrack
                    pos_ = before_array_init_pos; // Backtrack fully
                } else {
                    // Not [Type; Size], backtrack to before LBRACKET was consumed
                    pos_ = before_array_init_pos;
                }
            } catch (const std::runtime_error& e) {
                // Failed to parse type or other parts, backtrack
                pos_ = before_array_init_pos;
            }
            // If we backtracked, it might be a regular array literal, fall through to that logic.
            // Ensure pos_ is reset correctly if it was an array init attempt that failed.
            // The LBRACKET for array/list literal parsing is matched again below if this path fails and backtracks.
        }

regular_array_literal:
        // Typed Struct Literal: Identifier { ... }
        if (peek().type == TokenType::IDENTIFIER) {
            bool is_typed_struct = false;
            // Check if the next token after IDENTIFIER is LBRACE
            if (pos_ + 1 < tokens_.size() && tokens_[pos_ + 1].type == TokenType::LBRACE) {
                is_typed_struct = true;
            }

            if (is_typed_struct) {
                token::Token type_name_token = consume(); // Consume IDENTIFIER
                auto type_identifier_node = std::make_unique<ast::Identifier>(type_name_token.location, type_name_token.lexeme);
                // Create a TypeNode for the type path. For simplicity, assuming it's a simple identifier type without generics for now.
                // This might need to become a more complex type parsing if struct literals can have generic types like MyStruct<T>{...}
                auto type_path_node = std::make_unique<ast::TypeName>(type_name_token.location, std::move(type_identifier_node));

                expect(TokenType::LBRACE); // Consumes LBRACE
                SourceLocation struct_loc = type_name_token.location; // Location of the typed struct literal starts with the type name

                std::vector<ast::ObjectProperty> properties;
                if (!check(TokenType::RBRACE)) { // If not an empty struct {} or MyType{}
                    while (true) {
                        if (peek().type != TokenType::IDENTIFIER) {
                            throw error(peek(), "Expected identifier for struct field name.");
                        }
                        token::Token key_token = consume();
                        auto key_identifier = std::make_unique<ast::Identifier>(key_token.location, key_token.lexeme);
                        
                        vyn::ast::ExprPtr value = nullptr;
                        if (match(TokenType::COLON) || match(TokenType::EQ)) {
                            if (check(TokenType::COMMA) || check(TokenType::RBRACE)) {
                                throw error(peek(), "Expected expression for struct field value after ':' or '='.");
                            }
                            value = parse_expression();
                        } else {
                            // Shorthand: { fieldName }. AST.md: value is optional (null).
                            // If ast::ObjectProperty requires an actual expression for shorthand (e.g., an Identifier node),
                            // this would be: value = std::make_unique<ast::Identifier>(key_token.location, key_token.lexeme);
                            // Assuming nullptr is acceptable for the value in ast::ObjectProperty for shorthand.
                        }
                        properties.emplace_back(key_token.location, std::move(key_identifier), std::move(value));

                        if (match(TokenType::COMMA)) {
                            if (check(TokenType::RBRACE)) { // Trailing comma: { a:1, } or MyType{ a:1, }
                                break;
                            }
                            // Comma consumed, expect another property. If next is not IDENTIFIER, it's an error.
                            if (peek().type != TokenType::IDENTIFIER) {
                                throw error(peek(), "Expected identifier for struct field name after comma.");
                            }
                        } else {
                            break; // No comma, last property
                        }
                    }
                }
                expect(TokenType::RBRACE);
                return std::make_unique<ast::ObjectLiteral>(struct_loc, std::move(type_path_node), std::move(properties));
            } else {
                // Plain Identifier
                token::Token id_token = consume();
                return std::make_unique<ast::Identifier>(id_token.location, id_token.lexeme);
            }
        }

        if (is_literal(peek().type)) {
            DEBUG_PRINT("Parsing literal in parse_primary");
            auto lit_expr = parse_literal();
            DEBUG_PRINT("Exiting literal parsing in parse_primary");
            DEBUG_TOKEN(peek());
            return lit_expr;
        }

        if (match(TokenType::LPAREN)) {
            DEBUG_PRINT("Parsing grouped expression (LPAREN)");
            DEBUG_TOKEN(previous_token());
            vyn::ast::ExprPtr expr = parse_expression();
            expect(TokenType::RPAREN);
            DEBUG_PRINT("Exiting grouped expression (RPAREN)");
            DEBUG_TOKEN(previous_token());
            return expr;
        }
        
        // Array literals and list comprehensions: [element1, element2] or [expr for var in iterable ...]
        if (match(TokenType::LBRACKET)) { // This LBRACKET is for array/list literals
            DEBUG_PRINT("parse_primary: Matched LBRACKET for array/list literal.");
            DEBUG_TOKEN(previous_token()); // The LBRACKET token

            SourceLocation array_loc = previous_token().location;
            
            // Check for empty array
            if (check(TokenType::RBRACKET)) {
                DEBUG_PRINT("parse_primary: Parsing empty array literal []");
                consume(); // consume RBRACKET
                DEBUG_PRINT("parse_primary: Consumed RBRACKET for empty array.");
                DEBUG_TOKEN(previous_token());
                return std::make_unique<ast::ArrayLiteral>(array_loc, std::vector<vyn::ast::ExprPtr>{});
            }

            DEBUG_PRINT("parse_primary: Before parsing first_expr in array/list. Current token:");
            DEBUG_TOKEN(peek());
            // Use a specific precedence level if needed, e.g., parse_assignment_expr() to allow full expressions,
            // or parse_relational_expr() if elements are more constrained.
            // For `x*x for ...` and `0..10`, `parse_expression` (which calls `parse_assignment_expr`) is appropriate.
            vyn::ast::ExprPtr first_expr = parse_expression(); 
                                                                
            DEBUG_PRINT("parse_primary: After parsing first_expr in array/list. Current token:");
            DEBUG_TOKEN(peek());
            
            // After parsing the potential element expression, check for list comprehension
            if (match(TokenType::KEYWORD_FOR)) { 
                DEBUG_PRINT("parse_primary: Matched KEYWORD_FOR, parsing list comprehension.");
                DEBUG_TOKEN(previous_token()); // The FOR token

                if (!check(TokenType::IDENTIFIER)) {
                    throw error(peek(), "Expected identifier after 'for' in list comprehension.");
                }
                token::Token var_token = consume();
                DEBUG_PRINT("parse_primary: Consumed loop variable.");
                DEBUG_TOKEN(var_token);
                auto loop_var = std::make_unique<ast::Identifier>(var_token.location, var_token.lexeme);
                
                if (!match(TokenType::KEYWORD_IN)) { 
                    throw error(peek(), "Expected 'in' after loop variable in list comprehension.");
                }
                DEBUG_PRINT("parse_primary: Matched KEYWORD_IN.");
                DEBUG_TOKEN(previous_token()); // The IN token
                
                DEBUG_PRINT("parse_primary: Before parsing iterable_expr in list comprehension. Current token:");
                DEBUG_TOKEN(peek());
                vyn::ast::ExprPtr iterable_expr = parse_expression(); 
                DEBUG_PRINT("parse_primary: After parsing iterable_expr in list comprehension. Current token:");
                DEBUG_TOKEN(peek());
                
                vyn::ast::ExprPtr cond_expr = nullptr;
                if (match(TokenType::KEYWORD_IF)) { 
                    DEBUG_PRINT("parse_primary: Matched KEYWORD_IF for condition.");
                    DEBUG_TOKEN(previous_token()); // The IF token
                    DEBUG_PRINT("parse_primary: Before parsing cond_expr in list comprehension. Current token:");
                    DEBUG_TOKEN(peek());
                    cond_expr = parse_expression();
                    DEBUG_PRINT("parse_primary: After parsing cond_expr in list comprehension. Current token:");
                    DEBUG_TOKEN(peek());
                }

                DEBUG_PRINT("parse_primary: Before expect(RBRACKET) for list comprehension. Current token:");
                DEBUG_TOKEN(peek());
                expect(TokenType::RBRACKET);
                DEBUG_PRINT("parse_primary: Consumed RBRACKET for list comprehension.");
                DEBUG_TOKEN(previous_token());
                return std::make_unique<ast::ListComprehension>(array_loc, std::move(first_expr), std::move(loop_var), std::move(iterable_expr), std::move(cond_expr));
            } else {
                DEBUG_PRINT("parse_primary: Parsing regular array literal (after first element).");
                std::vector<vyn::ast::ExprPtr> elements;
                elements.push_back(std::move(first_expr)); // Add the already parsed first element

                while (match(TokenType::COMMA)) {
                    DEBUG_PRINT("parse_primary: Matched COMMA in array literal.");
                    DEBUG_TOKEN(previous_token()); // The COMMA token
                    // Check for trailing comma: [elem1, ]
                    if (check(TokenType::RBRACKET)) { 
                        DEBUG_PRINT("parse_primary: Trailing comma detected in array literal.");
                        break; 
                    }
                    DEBUG_PRINT("parse_primary: Before parsing next element in array literal. Current token:");
                    DEBUG_TOKEN(peek());
                    elements.push_back(parse_expression()); // Parse subsequent elements
                    DEBUG_PRINT("parse_primary: After parsing next element in array literal. Current token:");
                    DEBUG_TOKEN(peek());
                }
                DEBUG_PRINT("parse_primary: Before expect(RBRACKET) for array literal. Current token:");
                DEBUG_TOKEN(peek());
                expect(TokenType::RBRACKET);
                DEBUG_PRINT("parse_primary: Consumed RBRACKET for array literal.");
                DEBUG_TOKEN(previous_token());
                return std::make_unique<ast::ArrayLiteral>(array_loc, std::move(elements)); 
            }
        }

        // Anonymous Struct literals: { field1: value1, field2 }
        if (match(TokenType::LBRACE)) {
            SourceLocation struct_loc = previous_token().location;
            std::vector<ast::ObjectProperty> properties;

            if (!check(TokenType::RBRACE)) { // If not empty struct {}
                while (true) {
                    if (peek().type != TokenType::IDENTIFIER) {
                         throw error(peek(), "Expected identifier for struct field name.");
                    }
                    token::Token key_token = consume();
                    auto key_identifier = std::make_unique<ast::Identifier>(key_token.location, key_token.lexeme);
                    
                    vyn::ast::ExprPtr value = nullptr; 

                    if (match(TokenType::COLON) || match(TokenType::EQ)) {
                        // Check for missing value after ':' or '='
                        if (check(TokenType::COMMA) || check(TokenType::RBRACE)) {
                            throw error(peek(), "Expected expression for struct field value after ':' or '='.");
                        }
                        value = parse_expression();
                    } else {
                        // Shorthand: { fieldName }
                        // AST.md: value is optional (null).
                    }
                    properties.emplace_back(key_token.location, std::move(key_identifier), std::move(value));

                    if (match(TokenType::COMMA)) {
                        if (check(TokenType::RBRACE)) { // Trailing comma: { a:1, }
                            break; 
                        }
                        // Comma consumed, expect another property. If next is not IDENTIFIER, it's an error.
                        if (peek().type != TokenType::IDENTIFIER) {
                             throw error(peek(), "Expected identifier for struct field name after comma.");
                        }
                    } else {
                        break; // No comma, so this must be the last property
                    }
                }
            }
            expect(TokenType::RBRACE);
            return std::make_unique<ast::ObjectLiteral>(struct_loc, nullptr, std::move(properties));
        }

        throw error(peek(), "Expected primary expression.");
    }

    // Parses literal expressions (integers, floats, strings, booleans, null)
    vyn::ast::ExprPtr ExpressionParser::parse_literal() {
        DEBUG_PRINT("Entering parse_literal");
        DEBUG_TOKEN(peek());
        token::Token current_token = peek(); // Keep for location and lexeme, consume advances current_token internally
        switch (current_token.type) {
            case TokenType::INT_LITERAL: {
                consume(); 
                return std::make_unique<ast::IntegerLiteral>(current_token.location, std::stoll(current_token.lexeme));
            }
            case TokenType::FLOAT_LITERAL: {
                consume();
                return std::make_unique<ast::FloatLiteral>(current_token.location, std::stod(current_token.lexeme));
            }
            case TokenType::STRING_LITERAL: {
                consume();
                return std::make_unique<ast::StringLiteral>(current_token.location, current_token.lexeme);
            }
            case TokenType::KEYWORD_TRUE: {
                consume();
                return std::make_unique<ast::BooleanLiteral>(current_token.location, true);
            }
            case TokenType::KEYWORD_FALSE: {
                consume();
                return std::make_unique<ast::BooleanLiteral>(current_token.location, false);
            }
            case TokenType::KEYWORD_NULL:
            case TokenType::KEYWORD_NIL: {
                consume();
                return std::make_unique<ast::NilLiteral>(current_token.location);
            }
            default:
                // this->errors.push_back("Unexpected token in parse_literal: " + token_type_to_string(current_token.type) + " at " + location_to_string(current_token.location));
                throw std::runtime_error("Unexpected token in parse_literal: " + token_type_to_string(current_token.type) + " at " + location_to_string(current_token.location));
                return nullptr;
        }
        DEBUG_PRINT("Exiting parse_literal (should have returned or thrown)");
        return nullptr; // Should be unreachable if all cases return/throw
    }

    vyn::ast::ExprPtr ExpressionParser::parse_atom() {
        return parse_primary(); 
    }

    vyn::ast::ExprPtr ExpressionParser::parse_binary_expression(std::function<vyn::ast::ExprPtr()> parse_higher_precedence, const std::vector<TokenType>& operators) {
        DEBUG_PRINT("Entering parse_binary_expression");
        DEBUG_TOKEN(peek());
        vyn::ast::ExprPtr left = parse_higher_precedence();
        DEBUG_PRINT("parse_binary_expression: After parsing left operand. Current token:");
        DEBUG_TOKEN(peek());

        while (true) {
            bool operator_found = false;
            TokenType matched_op_type = TokenType::ILLEGAL; // Placeholder

            for (TokenType op_type : operators) {
                if (check(op_type)) { // Just check, don't consume
                    operator_found = true;
                    matched_op_type = op_type;
                    break;
                }
            }

            if (operator_found) {
                token::Token op_token = consume(); // Consume the operator (it must be matched_op_type)
                DEBUG_PRINT("parse_binary_expression: Matched operator.");
                DEBUG_TOKEN(op_token);
            
                DEBUG_PRINT("parse_binary_expression: Before parsing right operand. Current token:");
                DEBUG_TOKEN(peek());
                vyn::ast::ExprPtr right = parse_higher_precedence(); 
                DEBUG_PRINT("parse_binary_expression: After parsing right operand. Current token:");
                DEBUG_TOKEN(peek());
                left = std::make_unique<ast::BinaryExpression>(op_token.location, std::move(left), op_token, std::move(right));
            } else {
                DEBUG_PRINT("parse_binary_expression: No more matching operators found.");
                break; // No matching operator found
            }
        }
        DEBUG_PRINT("Exiting parse_binary_expression. Current token:");
        DEBUG_TOKEN(peek());
        return left;
    }

    vyn::ast::ExprPtr ExpressionParser::parse_logical_or_expr() {
        return parse_binary_expression([this]() { return parse_logical_and_expr(); }, {TokenType::OR});
    }

    vyn::ast::ExprPtr ExpressionParser::parse_logical_and_expr() {
        return parse_binary_expression([this]() { return parse_bitwise_or_expr(); }, {TokenType::AND});
    }

    vyn::ast::ExprPtr ExpressionParser::parse_bitwise_or_expr() {
        return parse_binary_expression([this]() { return parse_bitwise_xor_expr(); }, {TokenType::PIPE});
    }

    vyn::ast::ExprPtr ExpressionParser::parse_bitwise_xor_expr() {
        return parse_binary_expression([this]() { return parse_bitwise_and_expr(); }, {TokenType::CARET});
    }

    vyn::ast::ExprPtr ExpressionParser::parse_bitwise_and_expr() {
        return parse_binary_expression([this]() { return parse_equality_expr(); }, {TokenType::AMPERSAND});
    }

    vyn::ast::ExprPtr ExpressionParser::parse_equality_expr() {
        DEBUG_PRINT("Entering parse_equality_expr");
        DEBUG_TOKEN(peek());
        auto expr = parse_binary_expression([this]() {
            DEBUG_PRINT("parse_equality_expr: calling nested parse_relational_expr");
            DEBUG_TOKEN(peek());
            auto inner_expr = parse_relational_expr();
            DEBUG_PRINT("parse_equality_expr: returned from nested parse_relational_expr");
            DEBUG_TOKEN(peek());
            return inner_expr;
        }, {TokenType::EQEQ, TokenType::NOTEQ});
        DEBUG_PRINT("Exiting parse_equality_expr");
        DEBUG_TOKEN(peek());
        return expr;
    }

    vyn::ast::ExprPtr ExpressionParser::parse_relational_expr() {
        DEBUG_PRINT("Entering parse_relational_expr");
        DEBUG_TOKEN(peek());
        auto expr = parse_binary_expression([this]() { 
            DEBUG_PRINT("parse_relational_expr: calling nested parse_shift_expr");
            DEBUG_TOKEN(peek());
            auto inner_expr = parse_shift_expr(); 
            DEBUG_PRINT("parse_relational_expr: returned from nested parse_shift_expr");
            DEBUG_TOKEN(peek());
            return inner_expr;
        }, {TokenType::LT, TokenType::LTEQ, TokenType::GT, TokenType::GTEQ, TokenType::DOTDOT});
        DEBUG_PRINT("Exiting parse_relational_expr");
        DEBUG_TOKEN(peek());
        return expr;
    }

    vyn::ast::ExprPtr ExpressionParser::parse_shift_expr() {
        return parse_binary_expression([this]() { return parse_additive_expr(); }, {TokenType::LSHIFT, TokenType::RSHIFT});
    }

    vyn::ast::ExprPtr ExpressionParser::parse_additive_expr() {
        DEBUG_PRINT("Entering parse_additive_expr");
        DEBUG_TOKEN(peek());
        auto expr = parse_binary_expression([this]() {
            DEBUG_PRINT("parse_additive_expr: calling nested parse_multiplicative_expr");
            DEBUG_TOKEN(peek());
            auto inner_expr = parse_multiplicative_expr();
            DEBUG_PRINT("parse_additive_expr: returned from nested parse_multiplicative_expr");
            DEBUG_TOKEN(peek());
            return inner_expr;
        }, {TokenType::PLUS, TokenType::MINUS});
        DEBUG_PRINT("Exiting parse_additive_expr");
        DEBUG_TOKEN(peek());
        return expr;
    }

    vyn::ast::ExprPtr ExpressionParser::parse_multiplicative_expr() {
        DEBUG_PRINT("Entering parse_multiplicative_expr");
        DEBUG_TOKEN(peek());
        auto expr = parse_binary_expression([this]() {
            DEBUG_PRINT("parse_multiplicative_expr: calling nested parse_unary_expr");
            DEBUG_TOKEN(peek());
            auto inner_expr = parse_unary_expr();
            DEBUG_PRINT("parse_multiplicative_expr: returned from nested parse_unary_expr");
            DEBUG_TOKEN(peek());
            return inner_expr;
        }, {TokenType::MULTIPLY, TokenType::DIVIDE, TokenType::MODULO});
        DEBUG_PRINT("Exiting parse_multiplicative_expr");
        DEBUG_TOKEN(peek());
        return expr;
    }

    vyn::ast::ExprPtr ExpressionParser::parse_unary_expr() {
        if (match(TokenType::BANG) || match(TokenType::MINUS) || match(TokenType::TILDE) || match(TokenType::KEYWORD_AWAIT) || match(TokenType::KEYWORD_VIEW)) { // Added KEYWORD_VIEW
            token::Token op_token = previous_token();
            vyn::ast::ExprPtr operand = parse_unary_expr(); 
            // For await, we might want a specific AST node, e.g., AwaitExpression,
            // but UnaryExpression can work if the operator token type is distinct.
            return std::make_unique<ast::UnaryExpression>(op_token.location, op_token, std::move(operand));
        }
        // Attempt to parse TypeNode for potential constructor call or array initialization `[Type; Size]()`
        // This is a lookahead. We try to parse a type. If it fails, or if it's not followed by
        // '(' or by '; expr ) ( )', then we backtrack and parse as a normal primary expression.
        // This is complex because a TypeName can be a simple Identifier, which parse_primary also handles.

        // Create a TypeParser instance. Assuming it's part of the same parser structure
        // and can be instantiated or accessed here. For now, let's assume we can create it.
        // This requires TypeParser to be available and to have a parse_type_annotation() method.
        // If TypeParser is not designed to be used this way (e.g., it's part of a DeclarationParser),
        // this approach needs rethinking. For now, we'll assume it's usable.

        // Store current position to backtrack if type parsing isn't part of a constructor/array init
        size_t before_type_parse_pos = pos_;
        // It's tricky to integrate TypeParser here directly without knowing its interface
        // and how it handles errors or non-matches. A simple Identifier check might be
        // a first step, then extending to full TypeParser if an LPAREN follows.

        return parse_postfix_expr();
    }

    vyn::ast::ExprPtr ExpressionParser::parse_postfix_expr() {
        DEBUG_PRINT("Entering parse_postfix_expr");
        DEBUG_TOKEN(peek());
        vyn::ast::ExprPtr expr = parse_primary(); 
        DEBUG_PRINT("parse_postfix_expr: After parse_primary. Current token:");
        DEBUG_TOKEN(peek());

        while (true) {
            if (match(TokenType::LPAREN)) {
                DEBUG_PRINT("parse_postfix_expr: Matched LPAREN for call.");
                DEBUG_TOKEN(previous_token());
                // Parse call and convert special pointer-related calls to AST nodes
                auto calleeAstNode = std::move(expr); // This is the AST node for the thing being called
                auto parsedExprAfterCall = parse_call_expression(std::move(calleeAstNode)); // Returns unique_ptr<ast::Expression>

                ast::CallExpression* actualCallExpr = dynamic_cast<ast::CallExpression*>(parsedExprAfterCall.get());

                if (actualCallExpr) {
                    // It is a call expression, now check if it's one of our special intrinsics
                    ast::Identifier* calleeIdent = dynamic_cast<ast::Identifier*>(actualCallExpr->callee.get());
                    if (calleeIdent) {
                        const std::string& name = calleeIdent->name;
                        if ((name == "at" || name == "loc" || name == "addr" || name == "from")) {
                            if (actualCallExpr->arguments.size() == 1) {
                                // Special intrinsic with correct number of arguments.
                                // Move the argument out of the CallExpression's argument list.
                                auto arg = std::move(actualCallExpr->arguments[0]);
                                // Clear the arguments vector in the original CallExpression node
                                // as its content has been moved.
                                actualCallExpr->arguments.clear(); 

                                if (name == "at") {
                                    expr = std::make_unique<ast::PointerDerefExpression>(actualCallExpr->loc, std::move(arg));
                                } else if (name == "loc") {
                                    expr = std::make_unique<ast::LocationExpression>(actualCallExpr->loc, std::move(arg));
                                } else if (name == "addr") {
                                    expr = std::make_unique<ast::AddrOfExpression>(actualCallExpr->loc, std::move(arg));
                                } else if (name == "from") {
                                    expr = std::make_unique<ast::FromIntToLocExpression>(actualCallExpr->loc, std::move(arg));
                                }
                                // The unique_ptr parsedExprAfterCall (pointing to the original CallExpression)
                                // will go out of scope and the CallExpression AST node will be deleted if not moved,
                                // which is correct as we've created a new specialized AST node that replaces it.
                            } else {
                                // Special intrinsic name, but wrong number of arguments. Treat as a regular call.
                                expr = std::move(parsedExprAfterCall);
                            }
                        } else {
                            // Not a special intrinsic name. Treat as a regular call.
                            expr = std::move(parsedExprAfterCall);
                        }
                    } else {
                        // Callee is not an identifier. Treat as a regular call.
                        expr = std::move(parsedExprAfterCall);
                    }
                } else {
                    // parsedExprAfterCall was not a CallExpression (e.g., nullptr or some other Expression type).
                    // This case should ideally not be reached if parse_call_expression guarantees a CallExpression
                    // or throws on error. Assign it to expr to preserve the parsed node.
                    expr = std::move(parsedExprAfterCall);
                }
                DEBUG_PRINT("parse_postfix_expr: After processing call. Current token:");
                DEBUG_TOKEN(peek());
            } else if (match(TokenType::DOT)) {
                DEBUG_PRINT("parse_postfix_expr: Matched DOT for member access.");
                DEBUG_TOKEN(previous_token());
                expr = parse_member_access(std::move(expr));
                DEBUG_PRINT("parse_postfix_expr: After parse_member_access. Current token:");
                DEBUG_TOKEN(peek());
            } else if (match(TokenType::LBRACKET)) {
                DEBUG_PRINT("parse_postfix_expr: Matched LBRACKET for index access.");
                DEBUG_TOKEN(previous_token());
                SourceLocation bracket_loc = previous_token().location;
                DEBUG_PRINT("parse_postfix_expr: Before parsing index_expr. Current token:");
                DEBUG_TOKEN(peek());
                vyn::ast::ExprPtr index_expr = parse_expression();
                DEBUG_PRINT("parse_postfix_expr: After parsing index_expr. Current token:");
                DEBUG_TOKEN(peek());
                expect(TokenType::RBRACKET);
                DEBUG_PRINT("parse_postfix_expr: Consumed RBRACKET for index access.");
                DEBUG_TOKEN(previous_token());
                expr = std::make_unique<ast::ArrayElementExpression>(bracket_loc, std::move(expr), std::move(index_expr));
            } else if (peek().type == TokenType::LT) { // Potential generic instantiation
                DEBUG_PRINT("parse_postfix_expr: Found LT, potential generic instantiation. Current token:");
                DEBUG_TOKEN(peek());

                size_t snapshot_before_lt = pos_;
                token::Token lt_token = peek(); // For location, don't consume yet

                // Avoid conflict with << (LSHIFT) by checking next token if current is LT
                // This specific check might be better placed or refined,
                // e.g. by ensuring it's not part of a binary expression being parsed by a lower precedence parser.
                // For now, a simple check: if LT is followed by LT, it's likely LSHIFT.
                bool is_likely_shift = false;
                if (pos_ + 1 < tokens_.size() && tokens_[pos_ + 1].type == TokenType::LT) {
                    is_likely_shift = true;
                }
                // Also, relational operators are lower precedence. If this postfix parse fails,
                // the LT might be picked up by parse_relational_expr.

                if (!is_likely_shift) {
                    consume(); // Consume LT

                    std::vector<ast::TypeNodePtr> generic_arguments;
                    bool parsed_args_successfully = true;
                    
                    // Temporarily create a TypeParser for parsing arguments
                    // Note: ExpressionParser is passed to TypeParser constructor.
                    TypeParser arg_parser(tokens_, pos_, current_file_path_, *this);

                    if (peek().type != TokenType::GT) { // Not empty <>
                        while (true) {
                            size_t snapshot_before_arg = pos_;
                            try {
                                ast::TypeNodePtr arg = arg_parser.parse();
                                if (!arg) { // Should ideally not happen if TypeParser throws
                                    parsed_args_successfully = false;
                                    pos_ = snapshot_before_arg; // backtrack this arg attempt
                                    break;
                                }
                                generic_arguments.push_back(std::move(arg));
                            } catch (const std::runtime_error& e) {
                                pos_ = snapshot_before_arg; // Backtrack this arg attempt
                                parsed_args_successfully = false;
                                break;
                            }

                            if (match(TokenType::COMMA)) {
                                if (peek().type == TokenType::GT) { // Trailing comma like <T,> is invalid
                                    parsed_args_successfully = false;
                                    break;
                                }
                                // continue to next argument
                            } else {
                                break; // No comma, expect GT or failure
                            }
                        }
                    }

                    if (parsed_args_successfully && match(TokenType::GT)) {
                        // Successfully parsed expr<args>
                        expr = std::make_unique<ast::GenericInstantiationExpression>(
                            expr->loc, // loc of the base expression 'a'
                            std::move(expr),
                            std::move(generic_arguments),
                            lt_token.location,        // loc of '<'
                            previous_token().location // loc of '>'
                        );
                        // Successfully parsed generic instantiation, continue postfix loop
                        continue; 
                    } else {
                        // Failed to parse as expr<args> or expr<>
                        // Backtrack: reset pos_ to before the LT was consumed
                        pos_ = snapshot_before_lt;
                        // Break from postfix loop, allowing LT to be tried by other rules (e.g., relational)
                        break; 
                    }
                } else { // Was likely a shift or other construct, not generic LT for this postfix expression
                    break;
                }
            } else {
                DEBUG_PRINT("parse_postfix_expr: No more postfix operators.");
                break;
            }
        }
        DEBUG_PRINT("Exiting parse_postfix_expr. Current token:");
        DEBUG_TOKEN(peek());
        return expr;
    }

    // Helper to check if a token type is a literal
    bool ExpressionParser::is_literal(TokenType type) const { // Added const
        switch (type) {
            case TokenType::INT_LITERAL:
            case TokenType::FLOAT_LITERAL:
            case TokenType::STRING_LITERAL:
            // case TokenType::CHAR_LITERAL: // If you add char literals back
            case TokenType::KEYWORD_TRUE:
            case TokenType::KEYWORD_FALSE:
            case TokenType::KEYWORD_NULL:
            case TokenType::KEYWORD_NIL:
                return true;
            default:
                return false;
        }
    }

    bool ExpressionParser::is_expression_start(vyn::TokenType type) const {
        // This is a basic check. You might need to expand this based on your grammar.
        // For example, if expressions can start with keywords like 'new', 'await', etc.
        // or specific operators for unary expressions.
        return is_literal(type) ||
               type == TokenType::IDENTIFIER || // Covers plain identifiers and typed struct literals like `MyType { ... }`
               type == TokenType::LPAREN || // Grouped expression
               type == TokenType::MINUS ||  // Unary minus
               type == TokenType::BANG ||   // Unary not
               type == TokenType::TILDE ||  // Unary bitwise not
               type == TokenType::KEYWORD_AWAIT || // Added KEYWORD_AWAIT
               type == TokenType::LBRACKET || // Array literal
               type == TokenType::LBRACE ||   // Anonymous Struct literal { ... }
               type == TokenType::KEYWORD_FN || // Function expression/lambda
               type == TokenType::KEYWORD_IF; // If expression
        // Removed KEYWORD_NEW as it\'s not in token.hpp and not a primary expression start per AST.md
    }

} // namespace vyn

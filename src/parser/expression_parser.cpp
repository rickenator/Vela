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
         
        // Check for assignment operators
        std::optional<token::Token> op;
        if ((op = match(TokenType::EQ)) || (op = match(TokenType::PLUSEQ)) || (op = match(TokenType::MINUSEQ)) ||
            (op = match(TokenType::MULTIPLYEQ)) || (op = match(TokenType::DIVEQ)) || (op = match(TokenType::MODEQ)) ||
            (op = match(TokenType::LSHIFTEQ)) || (op = match(TokenType::RSHIFTEQ)) ||
            (op = match(TokenType::BITWISEANDEQ)) || (op = match(TokenType::BITWISEOREQ)) || (op = match(TokenType::BITWISEXOREQ)) ||
            (op = match(TokenType::COLONEQ))) {
            // Build assignment node using the operator token
            token::Token op_token = op.value();
            SourceLocation op_loc = op_token.location;
            // Parse RHS
            vyn::ast::ExprPtr right = parse_expression();
            return std::make_unique<ast::AssignmentExpression>(op_loc, std::move(left), op_token, std::move(right));
        }
        return left; // Not an assignment
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
        SourceLocation loc = peek().location; // General location, might be overridden

        // Handle if statements as expressions (e.g. `let x = if cond { 1 } else { 0 }`)
        if (match(TokenType::KEYWORD_IF)) {
            expect(TokenType::LPAREN); // Expect \\\'(\\\' after \\\'if\\\'
            vyn::ast::ExprPtr condition = parse_expression(); // Condition
            expect(TokenType::RPAREN); // Expect \\\')\\\' after condition

            expect(TokenType::LBRACE); // Then block
            vyn::ast::ExprPtr then_branch = parse_expression(); // Expression inside then block
            expect(TokenType::RBRACE);

            vyn::ast::ExprPtr else_branch = nullptr;
            if (match(TokenType::KEYWORD_ELSE)) {
                expect(TokenType::LBRACE); // Else block
                else_branch = parse_expression(); // Expression inside else block
                expect(TokenType::RBRACE);
            } else {
                throw error(peek(), "Expected \'else\' branch for if-expression.");
            }
            return std::make_unique<ast::IfExpression>(loc, std::move(condition), std::move(then_branch), std::move(else_branch));
        }

        // Try to parse TypeName(arguments) - Constructor Call
        // This requires being able to parse a TypeNode first.
        // We need a TypeParser instance here.
        // For now, we\\\'ll assume TypeParser is part of `this` parser or can be created.
        // This is a lookahead and backtrack mechanism.
        size_t initial_pos = pos_;
        try {
            TypeParser type_parser(tokens_, pos_, current_file_path_, *this); // Pass *this for ExpressionParser reference
            ast::TypeNodePtr type_node = type_parser.parse(); // Call parse() instead of parse_type_annotation()
            
            if (type_node && match(TokenType::LPAREN)) { // Successfully parsed a type and found '(' 
                std::vector<vyn::ast::ExprPtr> arguments;
                SourceLocation call_loc = previous_token().location; 
                if (!match(TokenType::RPAREN)) { 
                    do {
                        arguments.push_back(parse_expression());
                    } while (match(TokenType::COMMA));
                    expect(TokenType::RPAREN);
                }
                // Handle memory intrinsics parsed as type-like calls
                if (auto tname = dynamic_cast<ast::TypeName*>(type_node.get())) {
                    std::string name = tname->identifier ? tname->identifier->name : std::string();
                    if (name == "loc") {
                        if (arguments.size() != 1) throw error(peek(), "loc() expects 1 argument");
                        return std::make_unique<ast::LocationExpression>(call_loc, std::move(arguments[0]));
                    } else if (name == "addr") {
                        if (arguments.size() != 1) throw error(peek(), "addr() expects 1 argument");
                        return std::make_unique<ast::AddrOfExpression>(call_loc, std::move(arguments[0]));
                    } else if (name == "at") {
                        if (arguments.size() != 1) throw error(peek(), "at() expects 1 argument");
                        return std::make_unique<ast::PointerDerefExpression>(call_loc, std::move(arguments[0]));
                    } else if (name == "from") {
                        if (tname->genericArgs.size() != 1) throw error(peek(), "from<T>() expects a single generic type argument");
                        if (arguments.size() != 1) throw error(peek(), "from<T>() expects 1 argument");
                        auto targetType = std::move(tname->genericArgs[0]);
                        return std::make_unique<ast::FromIntToLocExpression>(call_loc, std::move(arguments[0]), std::move(targetType));
                    }
                }
                // Fallback to regular construction
                return std::make_unique<ast::ConstructionExpression>(type_node->loc, std::move(type_node), std::move(arguments));
            } else {
                // Not a TypeName(...), backtrack
                pos_ = initial_pos;
            }
        } catch (const std::runtime_error& e) {
            // Parsing type failed, or not followed by \\\'(\\\', backtrack
            pos_ = initial_pos;
            // Log or handle error if needed, or just proceed to other parsing rules
        }
        // If it wasn\\\'t a TypeName(...), reset pos_ and try other primary expression forms.
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
        // Handle 'from<Type>(expr)' syntax, Typed Struct Literals, and Plain Identifiers
        if (peek().type == TokenType::IDENTIFIER) {
            token::Token current_id_token = peek(); // Peek, don't consume yet

            if (current_id_token.lexeme == "from") {
                // Potential from<Type>(expr)
                // Lookahead: from < Type > ( expr )
                // Check for '<' after 'from'
                if (pos_ + 1 < tokens_.size() && tokens_[pos_ + 1].type == TokenType::LT) { // Changed LESS_THAN to LT
                    consume(); // Consume 'from'
                    SourceLocation from_loc = current_id_token.location;

                    expect(TokenType::LT); // Changed LESS_THAN to LT, Consume '<'

                    TypeParser type_parser(tokens_, pos_, current_file_path_, *this);
                    ast::TypeNodePtr target_type = type_parser.parse();
                    if (!target_type) {
                        throw error(peek(), "Expected type specification after 'from<'.");
                    }

                    expect(TokenType::GT); // Changed GREATER_THAN to GT, Consume '>'
                    expect(TokenType::LPAREN);      // Consume '('

                    vyn::ast::ExprPtr address_expr = parse_expression();

                    expect(TokenType::RPAREN);      // Consume ')'

                    return std::make_unique<ast::FromIntToLocExpression>(from_loc, std::move(address_expr), std::move(target_type));
                }
                // If "from" is not followed by "<", it will be treated like any other identifier below
                // (either as a typed struct name like "from { ... }" or a plain variable "from").
            }

            // Typed Struct Literal: Identifier { ... } or Plain Identifier
            bool is_typed_struct = false;
            // Check if the next token after IDENTIFIER is LBRACE
            if (pos_ + 1 < tokens_.size() && tokens_[pos_ + 1].type == TokenType::LBRACE) {
                is_typed_struct = true;
            }

            if (is_typed_struct) {
                token::Token type_name_token = consume(); // Consume IDENTIFIER
                auto type_identifier_node = std::make_unique<ast::Identifier>(type_name_token.location, type_name_token.lexeme);
                auto type_path_node = std::make_unique<ast::TypeName>(type_name_token.location, std::move(type_identifier_node));

                expect(TokenType::LBRACE); // Consumes LBRACE
                // Use type_name_token.location for the ObjectLiteral if it represents the start of the typed literal.
                // The 'loc' variable from the start of parse_primary() might be too broad if other constructs were peeked at.
                SourceLocation struct_loc = type_name_token.location; 

                std::vector<ast::ObjectProperty> properties;
                if (!check(TokenType::RBRACE)) { 
                    while (true) {
                        if (peek().type != TokenType::IDENTIFIER) {
                            throw error(peek(), "Expected identifier for struct field name.");
                        }
                        token::Token key_token = consume();
                        auto key_identifier = std::make_unique<ast::Identifier>(key_token.location, key_token.lexeme);
                        
                        vyn::ast::ExprPtr value = nullptr;
                        if (match(TokenType::COLON) || match(TokenType::EQ)) {
                            if (check(TokenType::COMMA) || check(TokenType::RBRACE)) {
                                throw error(peek(), "Expected expression for struct field value after \':\' or \'=\'.");
                            }
                            value = parse_expression();
                        } else {
                            // Shorthand: { fieldName }
                        }
                        properties.emplace_back(key_token.location, std::move(key_identifier), std::move(value));

                        if (match(TokenType::COMMA)) {
                            if (check(TokenType::RBRACE)) { 
                                break;
                            }
                            if (peek().type != TokenType::IDENTIFIER) {
                                throw error(peek(), "Expected identifier for struct field name after comma.");
                            }
                        } else {
                            break; 
                        }
                    }
                }
                expect(TokenType::RBRACE);
                return std::make_unique<ast::ObjectLiteral>(struct_loc, std::move(type_path_node), std::move(properties));
            } else {
                // Plain Identifier
                token::Token id_token = consume(); // Consume IDENTIFIER (this was current_id_token if not "from<...")
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

            // --- Lookahead to detect list comprehension by scanning tokens ---
            size_t snapshot_pos = pos_;
            bool will_comprehension = false;
            {
                size_t scan_pos = snapshot_pos;
                int bracket_nest = 1; // starting after consuming '['
                while (scan_pos < tokens_.size()) {
                    const auto& tk = tokens_[scan_pos];
                    if (tk.type == TokenType::LBRACKET) {
                        bracket_nest++;
                    } else if (tk.type == TokenType::RBRACKET) {
                        bracket_nest--;
                        if (bracket_nest == 0) break;
                    } else if (bracket_nest == 1 && tk.type == TokenType::KEYWORD_FOR) {
                        will_comprehension = true;
                        break;
                    }
                    scan_pos++;
                }
            }
            pos_ = snapshot_pos;

            // --- Actual parsing of first expression ---
            DEBUG_PRINT("parse_primary: Before parsing first_expr in array/list. Current token:");
            DEBUG_TOKEN(peek());
            vyn::ast::ExprPtr first_expr = parse_expression(); 
            DEBUG_PRINT("parse_primary: After parsing first_expr in array/list. Current token:");
            DEBUG_TOKEN(peek());

            // List comprehension if lookahead or actual FOR match
            if (will_comprehension || check(TokenType::KEYWORD_FOR)) {
                // Consume the 'for' keyword
                token::Token for_token = consume();
                DEBUG_PRINT("parse_primary: Matched KEYWORD_FOR, parsing list comprehension.");
                DEBUG_TOKEN(for_token); // The FOR token

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
        if (match(TokenType::BANG) || match(TokenType::MINUS) || match(TokenType::TILDE) || match(TokenType::KEYWORD_AWAIT)) {
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

    // Helper to parse postfix operations like calls, member access, subscripting
    vyn::ast::ExprPtr ExpressionParser::parse_postfix_expr() {
        DEBUG_PRINT("Entering parse_postfix_expr");
        DEBUG_TOKEN(peek());
        vyn::ast::ExprPtr expr = parse_primary();
        DEBUG_PRINT("After parse_primary in parse_postfix_expr");
        if(expr) { DEBUG_PRINT("Primary expr parsed successfully."); } else { DEBUG_PRINT("Primary expr is null."); }
        DEBUG_TOKEN(peek());

        while (true) {
            SourceLocation op_loc = peek().location; // Location of the operator (. [ ()
            if (match(TokenType::LPAREN)) {
                DEBUG_PRINT("parse_postfix_expr: Matched LPAREN for call.");
                // Check if \'expr\' is an identifier for an intrinsic function
                // This check needs to be robust. For example, ensure \'expr\' is indeed an Identifier.
                if (expr->getType() == ast::NodeType::IDENTIFIER) {
                    auto id = static_cast<ast::Identifier*>(expr.get());
                    std::string name = id->name;

                    // Handle intrinsic-like calls: loc(var), addr(loc_var), at(loc_var)
                    if (name == "loc" || name == "addr" || name == "at") {
                        std::vector<vyn::ast::ExprPtr> arguments;
                        if (!check(TokenType::RPAREN)) {
                            arguments.push_back(parse_expression());
                        }
                        expect(TokenType::RPAREN);
                        if (arguments.size() != 1) {
                            vyn::token::Token intrinsic_token(vyn::TokenType::IDENTIFIER, name, id->loc);
                            throw error(intrinsic_token,
                                std::string("Intrinsic '") + name + "' expects 1 argument, got " + std::to_string(arguments.size()));
                        }
                        // Create specialized AST node for intrinsic
                        auto& arg0 = arguments[0];
                        if (name == "loc") {
                            expr = std::make_unique<ast::LocationExpression>(op_loc, std::move(arg0));
                        } else if (name == "addr") {
                            expr = std::make_unique<ast::AddrOfExpression>(op_loc, std::move(arg0));
                        } else { // at
                            expr = std::make_unique<ast::PointerDerefExpression>(op_loc, std::move(arg0));
                        }
                        continue;
                    }
                }
                // If not an intrinsic or not an identifier, parse as a regular call expression
                expr = parse_call_expression(std::move(expr)); // parse_call_expression expects callee and handles args
            } else if (match(TokenType::DOT)) {
                DEBUG_PRINT("parse_postfix_expr: Matched DOT for member access.");
                DEBUG_TOKEN(previous_token());
                expr = parse_member_access(std::move(expr));
                DEBUG_PRINT("parse_postfix_expr: After parse_member_access. Current token:");
                DEBUG_TOKEN(peek());
            } else if (match(TokenType::LBRACKET)) { // This LBRACKET is for array element access
            token::Token bracket_token = previous_token(); // Location of \'[\'
            auto index_expr = parse_expression();
            expect(TokenType::RBRACKET);
            expr = std::make_unique<ast::ArrayElementExpression>(bracket_token.location, std::move(expr), std::move(index_expr));
        } else {
            break; 
        }
    }
    return expr; 
}

// Implementation for is_literal
bool ExpressionParser::is_literal(TokenType type) const {
    switch (type) {
        case TokenType::INT_LITERAL:
        case TokenType::FLOAT_LITERAL:
        case TokenType::STRING_LITERAL:
        case TokenType::KEYWORD_TRUE:
        case TokenType::KEYWORD_FALSE:
        case TokenType::KEYWORD_NULL:
        case TokenType::KEYWORD_NIL:
            return true;
        default:
            return false;
    }
}

// Implementation for is_expression_start
// This function helps in determining if a token can start an expression.
// It's used in contexts like parsing the body of a for loop or an if statement.
bool ExpressionParser::is_expression_start(vyn::TokenType type) const {
    // Primary expression starters
    if (type == TokenType::IDENTIFIER ||
        type == TokenType::INT_LITERAL ||
        type == TokenType::FLOAT_LITERAL ||
        type == TokenType::STRING_LITERAL ||
        type == TokenType::KEYWORD_TRUE ||
        type == TokenType::KEYWORD_FALSE ||
        type == TokenType::KEYWORD_NULL ||
        type == TokenType::KEYWORD_NIL ||
        type == TokenType::LPAREN ||    // Grouped expression or tuple
        type == TokenType::LBRACKET ||  // Array literal or list comprehension
        type == TokenType::LBRACE ||    // Object literal
        type == TokenType::KEYWORD_IF)  // If expression
    {
        return true;
    }

    // Unary operator starters
    if (type == TokenType::BANG ||
        type == TokenType::MINUS ||
        type == TokenType::TILDE ||
        type == TokenType::KEYWORD_AWAIT) // if await is an operator
    {
        return true;
    }
    
    // Keywords that can start expressions (like \'from<T>(e)\' or constructor calls if types are keywords)
    // This might overlap with IDENTIFIER if \'from\' is just an identifier.
    // Add specific keywords if they are distinct token types and can start expressions.
    // e.g. if \'new\' was a keyword for construction: case TokenType::KEYWORD_NEW: return true;

    return false;
}

} // namespace vyn

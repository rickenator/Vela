#include "vyn/parser/parser.hpp" // For BaseParser and other parser components
#include "vyn/parser/ast.hpp"      // For AST node types like IntegerLiteral, etc.
#include "vyn/parser/token.hpp"    // For TokenType and Token
#include <stdexcept>               // For std::runtime_error
#include <algorithm> // Required for std::any_of, if used by match or other helpers
#include <functional> // Required for std::function
#include <vector> // Required for std::vector

namespace vyn {

    // Global helper function for converting SourceLocation to string
    inline std::string location_to_string(const vyn::SourceLocation& loc) {
        return loc.filePath + ":" + std::to_string(loc.line) + ":" + std::to_string(loc.column);
    }


    // Constructor
    ExpressionParser::ExpressionParser(const std::vector<token::Token>& tokens, size_t& pos, const std::string& file_path)
        : BaseParser(tokens, pos, file_path) {}

    // Public method to start parsing an expression
    vyn::ast::ExprPtr ExpressionParser::parse_expression() {
        return parse_assignment_expr(); // Start with the lowest precedence
    }

    // Parses assignment expressions (e.g., x = 10, y += 5)
    vyn::ast::ExprPtr ExpressionParser::parse_assignment_expr() {
        vyn::ast::ExprPtr left = parse_logical_or_expr(); 
        SourceLocation op_loc = peek().location; 

        if (match(TokenType::EQ)) {
            token::Token op_token = previous_token(); 
            vyn::ast::ExprPtr right = parse_assignment_expr(); 
            if (dynamic_cast<ast::Identifier*>(left.get()) || dynamic_cast<ast::MemberExpression*>(left.get())) {
                return std::make_unique<ast::AssignmentExpression>(op_loc, std::move(left), op_token, std::move(right));
            } else {
                // Use the error reporting mechanism from BaseParser
                // this->errors.push_back("Invalid left-hand side in assignment expression at line " + std::to_string(op_loc.line) + ", column " + std::to_string(op_loc.column));
                // For now, let's throw, assuming error() is available and throws.
                // If BaseParser has an error vector, it should be used instead.
                // Assuming BaseParser::error() is available and suitable.
                // If not, this needs adjustment based on how errors are collected.
                // For now, throwing a runtime_error as a placeholder for proper error handling.
                throw std::runtime_error("Invalid left-hand side in assignment expression at " + location_to_string(op_loc));
                return nullptr; 
            }
        }
        return left; 
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
        SourceLocation loc = peek().location;

        // Try to parse TypeName(arguments) - Constructor Call
        // This requires being able to parse a TypeNode first.
        // We need a TypeParser instance here.
        // For now, we\'ll assume TypeParser is part of `this` parser or can be created.
        // This is a lookahead and backtrack mechanism.
        size_t initial_pos = pos_;
        try {
            TypeParser type_parser(tokens_, pos_, file_path_, *this); // Pass *this for ExpressionParser reference
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
        if (peek().type == TokenType::LBRACKET) {
            size_t before_array_init_pos = pos_;
            consume(); // Consume LBRACKET
            try {
                TypeParser type_parser_for_array(tokens_, pos_, file_path_, *this); // Pass *this for ExpressionParser reference
                ast::TypeNodePtr element_type = type_parser_for_array.parse(); // Call parse()

                if (element_type && match(TokenType::SEMICOLON)) {
                    ast::ExprPtr size_expr = parse_expression();
                    expect(TokenType::RBRACKET);
                    if (match(TokenType::LPAREN)) {
                        expect(TokenType::RPAREN);
                        // It's an ArrayInitializationExpression
                        return std::make_unique<ast::ArrayInitializationExpression>(loc, std::move(element_type), std::move(size_expr));
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
        }

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
                auto type_path_node = ast::TypeNode::newIdentifier(type_name_token.location, std::move(type_identifier_node));

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
            return parse_literal();
        }

        if (match(TokenType::LPAREN)) {
            vyn::ast::ExprPtr expr = parse_expression();
            expect(TokenType::RPAREN);
            return expr;
        }
        
        // Array literals: [element1, element2]
        if (match(TokenType::LBRACKET)) {
            SourceLocation array_loc = previous_token().location;
            std::vector<vyn::ast::ExprPtr> elements;
            if (!check(TokenType::RBRACKET)) { // If not immediately RBRACKET (empty array)
                while (true) {
                    // Check for missing expression before parsing an element
                    if (check(TokenType::COMMA) || check(TokenType::RBRACKET)) {
                        // If it's a COMMA, it means an empty element e.g. [1,,2] or [,] if it's the first element
                        // If it's an RBRACKET, it means an empty element before RBRACKET if a comma was just consumed e.g. [1,]
                        // This logic needs to be careful not to disallow valid trailing commas.
                        // The error should trigger if we expect an expression but find COMMA or RBRACKET.
                        // This happens if: (a) it's the first element and we see COMMA/RBRACKET, or (b) we just consumed a COMMA and see another COMMA/RBRACKET.
                        bool prev_was_comma = (pos_ > 0 && tokens_[pos_-1].type == TokenType::COMMA);
                        bool is_first_element_pos = (tokens_[pos_ -1].type == TokenType::LBRACKET); // Check if previous token was LBRACKET

                        if (check(TokenType::COMMA) && (elements.empty() && is_first_element_pos || prev_was_comma) ) {
                             throw error(peek(), "Expected expression for array element, found comma.");
                        }
                        // A RBRACKET here is fine if it's a trailing comma case, which is handled after parsing the element and matching COMMA.
                        // If it's an empty array `[]` or `[ ]`, the outer `if (!check(TokenType::RBRACKET))` handles it.
                    }

                    elements.push_back(parse_expression());

                    if (match(TokenType::COMMA)) {
                        if (check(TokenType::RBRACKET)) { // Trailing comma: [1, ]
                            break; 
                        }
                        // Comma consumed, expect another expression. If next is COMMA or RBRACKET, it's an error.
                        if (check(TokenType::COMMA) || check(TokenType::RBRACKET)) {
                            throw error(peek(), "Expected expression after comma in array literal.");
                        }
                    } else {
                        break; // No comma, so this must be the last element
                    }
                }
            }
            expect(TokenType::RBRACKET);
            return std::make_unique<ast::ArrayLiteralNode>(array_loc, std::move(elements)); 
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
        token::Token current_token = peek();
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
    }

    vyn::ast::ExprPtr ExpressionParser::parse_atom() {
        return parse_primary(); 
    }

    vyn::ast::ExprPtr ExpressionParser::parse_binary_expression(std::function<vyn::ast::ExprPtr()> parse_higher_precedence, const std::vector<TokenType>& operators) {
        vyn::ast::ExprPtr left = parse_higher_precedence();

        while (std::any_of(operators.begin(), operators.end(), [&](TokenType op_type){ return match(op_type); })) {
            token::Token op_token = previous_token();
            vyn::ast::ExprPtr right = parse_higher_precedence(); // Should be parse_higher_precedence for right-associativity of some ops, or current level for left-associativity
            left = std::make_unique<ast::BinaryExpression>(op_token.location, std::move(left), op_token, std::move(right));
        }
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
        return parse_binary_expression([this]() { return parse_relational_expr(); }, {TokenType::EQEQ, TokenType::NOTEQ});
    }

    vyn::ast::ExprPtr ExpressionParser::parse_relational_expr() {
        return parse_binary_expression([this]() { return parse_shift_expr(); }, {TokenType::LT, TokenType::LTEQ, TokenType::GT, TokenType::GTEQ});
    }

    vyn::ast::ExprPtr ExpressionParser::parse_shift_expr() {
        return parse_binary_expression([this]() { return parse_additive_expr(); }, {TokenType::LSHIFT, TokenType::RSHIFT});
    }

    vyn::ast::ExprPtr ExpressionParser::parse_additive_expr() {
        return parse_binary_expression([this]() { return parse_multiplicative_expr(); }, {TokenType::PLUS, TokenType::MINUS});
    }

    vyn::ast::ExprPtr ExpressionParser::parse_multiplicative_expr() {
        return parse_binary_expression([this]() { return parse_unary_expr(); }, {TokenType::MULTIPLY, TokenType::DIVIDE, TokenType::MODULO});
    }

    vyn::ast::ExprPtr ExpressionParser::parse_unary_expr() {
        if (match(TokenType::BANG) || match(TokenType::MINUS) || match(TokenType::TILDE)) {
            token::Token op_token = previous_token();
            vyn::ast::ExprPtr operand = parse_unary_expr(); 
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
        vyn::ast::ExprPtr expr = parse_primary(); 

        while (true) {
            if (match(TokenType::LPAREN)) {
                // Could be a normal function call OR a TypeName()
                // If 'expr' is an Identifier or a MemberExpression that resolves to a type name,
                // it could be a constructor call.
                // For now, we assume parse_primary correctly identifies TypeName as an Identifier
                // or MemberExpression. We need a way to distinguish TypeName(...) from funcName(...).
                // This might require semantic information or a syntactic marker.
                // If 'expr' is an ast::Identifier, we can try to parse it as a TypeNode first.
                // This is a simplification. A full solution would involve trying to parse a TypeNode
                // at the beginning of parse_primary or here, and if successful and followed by '(',
                // treat it as ConstructionExpression.

                if (auto ident_expr = dynamic_cast<ast::Identifier*>(expr.get())) {
                    // Check if this identifier could be a type name for a constructor call
                    // This is a heuristic. A more robust way would be to attempt to parse a TypeNode
                    // and if successful and followed by '(', then it's a ConstructionExpression.
                    // For now, if an Identifier is followed by '(', we'll assume it could be a constructor.
                    // We'll need to refine this. The problem is distinguishing `MyType()` from `myFunc()`.
                    // A simple heuristic: if the identifier is capitalized, it might be a type.
                    // This is not robust. Vyn might not have this convention.

                    // Let's assume for now that if parse_primary returned an Identifier,
                    // and it's followed by '(', it's a regular call. Construction calls
                    // like `MyType(...)` will need to be parsed more explicitly, perhaps by
                    // having parse_primary or a new function try to parse a TypeNode first.
                    // The current structure makes it hard to distinguish without more context
                    // or a change in how primary expressions involving types are parsed.

                    // For the task: `TypeName(arguments)`
                    // We need to parse `TypeName` as a `TypeNode`.
                    // If `expr` is an `Identifier` or `MemberExpression` that could be a type path.
                    // This is where `TypeParser` would be helpful.

                    // Let's assume `parse_primary` is modified to detect `TypeName LPAREN` directly
                    // or this function is structured to try parsing a TypeNode first.
                    // Given the current structure, we'll stick to `parse_call_expression` for now
                    // and address `TypeName()` parsing in `parse_primary` or a dedicated function.
                    expr = parse_call_expression(std::move(expr));
                } else if (auto member_expr = dynamic_cast<ast::MemberExpression*>(expr.get())) {
                    // Could be module.Type()
                     expr = parse_call_expression(std::move(expr));
                } else {
                    // If it's not an identifier or member access, it's a regular call on an expression value
                    expr = parse_call_expression(std::move(expr));
                }
            } else if (match(TokenType::DOT)) {
                expr = parse_member_access(std::move(expr));
            } else if (match(TokenType::LBRACKET)) {
                // Array subscript or potentially start of `[Type; Size]()` if `expr` was empty/placeholder
                // This part needs to be careful. If `expr` is a valid expression, it's a subscript.
                // The `[Type; Size]()` form starts with `LBRACKET` directly, handled in `parse_primary`.
                // this->errors.push_back("Array subscript operator not yet implemented at " + location_to_string(previous_token().location));
                // For now, assume it's an array element access, not array initialization.
                // The array initialization `[Type; Size]()` should be parsed in `parse_primary`
                // because it starts with `[` and doesn't have a preceding expression.
                SourceLocation op_loc = previous_token().location;
                vyn::ast::ExprPtr index_expr = parse_expression();
                expect(TokenType::RBRACKET);
                expr = std::make_unique<ast::ArrayElementExpression>(op_loc, std::move(expr), std::move(index_expr));
            } else {
                break;
            }
        }
        return expr;
    }

    vyn::ast::ExprPtr ExpressionParser::parse_primary_expr() {
        return parse_atom(); 
    }

    // Implementation for is_literal (can be moved to BaseParser if it's general)
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
               type == TokenType::LBRACKET || // Array literal
               type == TokenType::LBRACE ||   // Anonymous Struct literal { ... }
               type == TokenType::KEYWORD_FN || // Function expression/lambda
               type == TokenType::KEYWORD_IF; // If expression
        // Removed KEYWORD_AWAIT as it's not in primary expressions per AST.md and not in token.hpp
        // Removed KEYWORD_NEW as it's not in token.hpp and not a primary expression start per AST.md
    }

} // namespace vyn

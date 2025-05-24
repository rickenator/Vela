#include "vyn/parser/parser.hpp" // Should provide full defs for StatementParser and DeclarationParser
#include "vyn/parser/ast.hpp"
#include <stdexcept>
#include "vyn/parser/token.hpp" // Required for vyn::token::Token

namespace vyn {

StatementParser::StatementParser(const std::vector<token::Token>& tokens, size_t& pos, int indent_level, const std::string& file_path, TypeParser& type_parser, ExpressionParser& expr_parser, DeclarationParser* decl_parser)
    : BaseParser(tokens, pos, file_path), type_parser_(type_parser), expr_parser_(expr_parser), decl_parser_(decl_parser) {
    // indent_level is part of BaseParser or handled there if needed.
    // If BaseParser doesn't take indent_level, and it's needed here, add a member: this->indent_level_ = indent_level;
}

void StatementParser::set_declaration_parser(DeclarationParser* dp) {
    this->decl_parser_ = dp;
}

vyn::ast::StmtPtr StatementParser::parse() {
    // Skip any INDENT/DEDENT tokens before parsing a statement
    this->skip_indents_dedents();
    while (!this->IsAtEnd() && this->peek().type == vyn::TokenType::NEWLINE) {
        this->consume(); // Skip empty lines
    }

    if (this->IsAtEnd()) {
        return nullptr;
    }

    // Handle 'throw' statements as expression statements: parse throw expression and ignore its result
    if (this->peek().type == vyn::TokenType::IDENTIFIER && this->peek().lexeme == "throw") {
        this->consume(); // consume 'throw'
        // Parse the thrown expression (e.g., NetworkError(...))
        auto thrownExpr = this->expr_parser_.parse_expression();
        // Consume optional semicolon
        this->match(vyn::TokenType::SEMICOLON);
        return nullptr; // ignore throw statement in AST
    }
    vyn::token::Token current_token = this->peek();
    switch (current_token.type) {
        case vyn::TokenType::KEYWORD_LET:
        case vyn::TokenType::KEYWORD_MUT:
        case vyn::TokenType::KEYWORD_CONST:
        case vyn::TokenType::KEYWORD_VAR:
        case vyn::TokenType::KEYWORD_AUTO:
            return parse_var_decl();
        case vyn::TokenType::KEYWORD_ASYNC:
            if (decl_parser_) {
                return decl_parser_->parse_function();
            } else {
                throw std::runtime_error("Async function parsing not available in this context at " + location_to_string(current_token.location));
            }
        case vyn::TokenType::KEYWORD_CLASS:
            if (decl_parser_) {
                return decl_parser_->parse_class_declaration();
            } else {
                throw std::runtime_error("Class declaration parsing not available in this context at " + location_to_string(current_token.location));
            }
        case vyn::TokenType::KEYWORD_TEMPLATE:
            if (decl_parser_) {
                return decl_parser_->parse_template_declaration();
            } else {
                throw std::runtime_error("Template declaration parsing not available in this context at " + location_to_string(current_token.location));
            }
        case vyn::TokenType::KEYWORD_IF:
            return parse_if();
        case vyn::TokenType::KEYWORD_WHILE:
            return parse_while();
        case vyn::TokenType::KEYWORD_FOR:
            return parse_for();
        case vyn::TokenType::KEYWORD_RETURN:
            return parse_return();
        case vyn::TokenType::LBRACE:
            return parse_block();
        case vyn::TokenType::KEYWORD_TRY:
            return parse_try();
        case vyn::TokenType::KEYWORD_UNSAFE:
            return parse_unsafe();
        case vyn::TokenType::KEYWORD_DEFER:
            return parse_defer();
        case vyn::TokenType::KEYWORD_AWAIT:
            return parse_await();
        case vyn::TokenType::KEYWORD_BREAK:
            throw std::runtime_error("Break statement parsing not yet implemented at " + location_to_string(current_token.location));
        case vyn::TokenType::KEYWORD_CONTINUE:
            throw std::runtime_error("Continue statement parsing not yet implemented at " + location_to_string(current_token.location));
        default:
            // Check if this could be a relaxed syntax variable declaration (Type name)
            if (current_token.type == vyn::TokenType::IDENTIFIER) {
                // Save position in case we need to backtrack
                size_t saved_pos = this->pos_;
                
                try {
                    // Try to parse as a type
                    auto type_node = this->type_parser_.parse();
                    
                    if (type_node && !this->IsAtEnd() && this->peek().type == vyn::TokenType::IDENTIFIER) {
                        // This looks like a relaxed syntax declaration: Type name
                        // Rewind position and parse as variable declaration
                        this->pos_ = saved_pos;
                        return parse_var_decl();
                    }
                    
                    // Not a type or not followed by an identifier, so restore position
                    this->pos_ = saved_pos;
                } catch (...) {
                    // Not a valid type, restore position
                    this->pos_ = saved_pos;
                }
            }
            
            // Not a declaration, try parsing as an expression statement
            if (this->expr_parser_.is_expression_start(current_token.type)) {
                return parse_expression_statement();
            } else {
                throw std::runtime_error("Unexpected token at start of statement: '" + token_type_to_string(current_token.type) + "' at " + location_to_string(current_token.location));
            }
    }
}

std::unique_ptr<vyn::ast::ExpressionStatement> StatementParser::parse_expression_statement() {
    auto expr = this->expr_parser_.parse_expression();
    // Assuming expr_parser_.parse_expression() throws or returns a valid expression,
    // and does not return nullptr on failing to parse an expression that should be there.
    SourceLocation expr_loc = expr->loc; // Location of the statement is the location of the expression.

    // Check for optional semicolon or other valid terminators
    if (this->match(vyn::TokenType::SEMICOLON)) {
        // Semicolon consumed, all good.
    } else if (this->peek().type == vyn::TokenType::NEWLINE ||
               this->IsAtEnd() ||
               this->peek().type == vyn::TokenType::RBRACE ||
               this->peek().type == vyn::TokenType::DEDENT) {
        // Optional semicolon: if it's a newline, end of file, closing brace, or dedent, it's fine.
        // Do not consume newline, RBRACE, or DEDENT here, they might be significant for the outer structure.
    } else {
        throw std::runtime_error("Expected semicolon, newline, '}', or DEDENT after expression statement at " +
                                 location_to_string(this->peek().location) + ", got " +
                                 token_type_to_string(this->peek().type));
    }
    return std::make_unique<vyn::ast::ExpressionStatement>(expr_loc, std::move(expr));
}


std::unique_ptr<vyn::ast::BlockStatement> StatementParser::parse_block() {
    SourceLocation start_loc = this->expect(vyn::TokenType::LBRACE, "Expected '{' to start a block.").location;
    std::vector<vyn::ast::StmtPtr> statements;
    SourceLocation end_loc = start_loc;

    while (!this->IsAtEnd() && this->peek().type != vyn::TokenType::RBRACE) {
        // Skip newlines within the block if they are not separating statements meaningfully
        while (!this->IsAtEnd() && this->peek().type == vyn::TokenType::NEWLINE) {
            this->consume();
        }
        if (this->IsAtEnd() || this->peek().type == vyn::TokenType::RBRACE) {
            break; // End of block or file
        }
        statements.push_back(parse()); // Parse the statement within the block
        // Ensure that after a statement, we either have another statement, a newline, or the end of the block
        if (!this->IsAtEnd() && this->peek().type != vyn::TokenType::RBRACE) {
            if (this->peek().type == vyn::TokenType::SEMICOLON) {
                this->consume(); // Consume optional semicolon
            } else if (this->peek().type == vyn::TokenType::NEWLINE) {
                // Fine, next statement or end of block might be on new line
            } else if (this->expr_parser_.is_expression_start(this->peek().type) || this->is_statement_start(this->peek().type)) { // Changed here
                // Next statement starts immediately, this is fine.
            }
             else if (this->peek().type != vyn::TokenType::RBRACE && !this->is_statement_start(this->peek().type) && !this->expr_parser_.is_expression_start(this->peek().type)) { // Changed here
                 throw std::runtime_error("Expected newline, semicolon, or end of block after statement at " + location_to_string(this->peek().location) + ", got " + token_type_to_string(this->peek().type));
             }
        }
    }

    if (this->IsAtEnd() && this->peek().type != vyn::TokenType::RBRACE) { // Check before expect
        // If we are at the end of the file and haven't found an RBRACE,
        // it means the block was not properly closed.
        // The error message from expect() might be generic, so let's throw a specific one.
        // However, the current expect already produces a good message: "Expected '}' ... but found END_OF_FILE"
        // So, we can just let expect handle it, or customize.
        // For now, let expect produce its standard error.
        // If a more specific error is desired for EOF, it can be added here.
    }
    end_loc = this->expect(vyn::TokenType::RBRACE, "Expected \'}\' to end a block.").location;
    return std::make_unique<vyn::ast::BlockStatement>(start_loc, std::move(statements));
}


std::unique_ptr<vyn::ast::IfStatement> StatementParser::parse_if() {
    SourceLocation if_loc = this->expect(vyn::TokenType::KEYWORD_IF, "Expected 'if'.").location;
    this->expect(vyn::TokenType::LPAREN, "Expected '(' after 'if'.");
    auto condition = this->expr_parser_.parse_expression();
    this->expect(vyn::TokenType::RPAREN, "Expected ')' after if condition.");
    auto then_branch = parse_block(); // 'if' body must be a block
    vyn::ast::StmtPtr else_branch = nullptr;
    SourceLocation end_loc = then_branch->loc; // Use loc member

    if (this->match(vyn::TokenType::KEYWORD_ELSE)) {
        if (this->peek().type == vyn::TokenType::KEYWORD_IF) { // 'else if'
            else_branch = parse_if(); // Recursively parse the 'else if'
        } else { // 'else'
            else_branch = parse_block(); // 'else' body must be a block
        }
        if (else_branch) {
            end_loc = else_branch->loc; // Use loc member
        }
    }

    return std::make_unique<vyn::ast::IfStatement>(if_loc, std::move(condition), std::move(then_branch), std::move(else_branch));
}

std::unique_ptr<vyn::ast::WhileStatement> StatementParser::parse_while() {
    SourceLocation while_loc = this->expect(vyn::TokenType::KEYWORD_WHILE, "Expected 'while'.").location;
    this->expect(vyn::TokenType::LPAREN, "Expected '(' after 'while'.");
    auto condition = this->expr_parser_.parse_expression();
    this->expect(vyn::TokenType::RPAREN, "Expected ')' after while condition.");
    auto body = parse_block(); // 'while' body must be a block
    SourceLocation end_loc = body->loc; // Use loc member
    return std::make_unique<vyn::ast::WhileStatement>(while_loc, std::move(condition), std::move(body));
}

std::unique_ptr<vyn::ast::ForStatement> StatementParser::parse_for() {
    SourceLocation for_loc = this->expect(vyn::TokenType::KEYWORD_FOR, "Expected 'for'.").location;
    this->expect(vyn::TokenType::LPAREN, "Expected '(' after 'for'.");

    vyn::ast::StmtPtr initializer = nullptr;
    
    // Check for variable declarations with all possible starting tokens
    if (this->peek().type == vyn::TokenType::KEYWORD_LET || 
        this->peek().type == vyn::TokenType::KEYWORD_MUT ||
        this->peek().type == vyn::TokenType::KEYWORD_CONST || 
        this->peek().type == vyn::TokenType::KEYWORD_VAR ||
        this->peek().type == vyn::TokenType::KEYWORD_AUTO) {
        // Standard syntax variable declaration
        initializer = parse_var_decl(); // Parses the full variable declaration including semicolon
    } else if (this->peek().type != vyn::TokenType::SEMICOLON) {
        // Check for relaxed syntax (Type name) variable declarations
        if (this->peek().type == vyn::TokenType::IDENTIFIER) {
            // Save position in case we need to backtrack
            size_t saved_pos = this->pos_;
            
            try {
                // Try to parse as a type
                auto type_node = this->type_parser_.parse();
                
                if (type_node && !this->IsAtEnd() && this->peek().type == vyn::TokenType::IDENTIFIER) {
                    // This looks like a relaxed syntax declaration: Type name
                    // Rewind position and parse as variable declaration
                    this->pos_ = saved_pos;
                    initializer = parse_var_decl();
                } else {
                    // Not a type or not followed by an identifier, restore position
                    this->pos_ = saved_pos;
                    initializer = parse_expression_statement(); // Parse as expression statement
                }
            } catch (...) {
                // Not a valid type, restore position and parse as expression
                this->pos_ = saved_pos;
                initializer = parse_expression_statement(); // Parse as expression statement
            }
        } else {
            // Not starting with an identifier, parse as expression
            initializer = parse_expression_statement(); // Parses expression then expects semicolon
        }
    } else {
        this->expect(vyn::TokenType::SEMICOLON, "Expected semicolon after empty for-loop initializer."); // Consume semicolon for empty initializer
    }

    vyn::ast::ExprPtr condition = nullptr;
    if (this->peek().type != vyn::TokenType::SEMICOLON) {
        condition = this->expr_parser_.parse_expression();
    }
    this->expect(vyn::TokenType::SEMICOLON, "Expected semicolon after for-loop condition.");

    vyn::ast::ExprPtr increment = nullptr;
    if (this->peek().type != vyn::TokenType::RPAREN) {
        increment = this->expr_parser_.parse_expression();
    }
    this->expect(vyn::TokenType::RPAREN, "Expected ')' after for-loop clauses.");

    auto body = parse_block(); // 'for' body must be a block
    SourceLocation end_loc = body->loc; // Use loc member

    return std::make_unique<vyn::ast::ForStatement>(for_loc, std::move(initializer), std::move(condition), std::move(increment), std::move(body));
}

std::unique_ptr<vyn::ast::ReturnStatement> StatementParser::parse_return() {
    SourceLocation return_loc = this->expect(vyn::TokenType::KEYWORD_RETURN, "Expected 'return'.").location;
    vyn::ast::ExprPtr value = nullptr;
    SourceLocation end_loc = return_loc;

    if (this->peek().type != vyn::TokenType::SEMICOLON && this->peek().type != vyn::TokenType::NEWLINE && this->peek().type != vyn::TokenType::RBRACE && this->peek().type != vyn::TokenType::DEDENT) {
        value = this->expr_parser_.parse_expression();
        end_loc = value->loc; // Use loc member
    }

    if (this->peek().type == vyn::TokenType::SEMICOLON) {
        end_loc = this->peek().location;
        this->consume(); // Consume semicolon
    } else if (this->peek().type == vyn::TokenType::NEWLINE || this->IsAtEnd() || this->peek().type == vyn::TokenType::RBRACE || this->peek().type == vyn::TokenType::DEDENT) {
        // Optional semicolon at the end of a line or before closing brace
    } else {
        throw std::runtime_error("Expected semicolon or newline after return statement at " + location_to_string(this->peek().location));
    }

    return std::make_unique<vyn::ast::ReturnStatement>(return_loc, std::move(value));
}

std::unique_ptr<vyn::ast::VariableDeclaration> StatementParser::parse_var_decl() {
    // Declaration start location
    SourceLocation keyword_loc = this->current_location();
    
    // Check what kind of declaration this is
    bool is_const_decl = false;
    bool using_standard_syntax = true;
    bool auto_type_inference = false;
    
    // Check for 'auto' keyword first (type inference)
    if (this->match(vyn::TokenType::KEYWORD_AUTO)) {
        auto_type_inference = true;
        is_const_decl = false;
    }
    // Check for standard or relaxed syntax
    else if (this->match(vyn::TokenType::KEYWORD_VAR)) {
        is_const_decl = false;
        using_standard_syntax = true;
    } else if (this->match(vyn::TokenType::KEYWORD_CONST)) {
        is_const_decl = true;
        
        // Check if using standard syntax (const<Type>) or relaxed (const Type)
        if (this->peek().type == vyn::TokenType::LT) {
            using_standard_syntax = true;
        } else {
            using_standard_syntax = false;
        }
    } else {
        // This must be a relaxed syntax with just a type name (Type name)
        using_standard_syntax = false;
        is_const_decl = false;
    }
    
    // Parse the type
    ast::TypeNodePtr type_expr = nullptr;
    
    if (!auto_type_inference) {
        if (using_standard_syntax) {
            // Standard syntax: var<Type> or const<Type>
            this->expect(vyn::TokenType::LT, "Expected '<' after 'var'/'const'.");
            type_expr = this->type_parser_.parse();
            if (!type_expr) {
                throw std::runtime_error("Expected type inside '<>' in variable declaration at " + 
                                       location_to_string(this->peek().location));
            }
            this->expect(vyn::TokenType::GT, "Expected '>' after type in variable declaration.");
        } else {
            // Relaxed syntax: Type or const Type
            type_expr = this->type_parser_.parse();
            if (!type_expr) {
                throw std::runtime_error("Expected type in variable declaration at " + 
                                       location_to_string(this->peek().location));
            }
        }
    }
    
    // Next token is variable name
    vyn::token::Token name_token = this->expect(vyn::TokenType::IDENTIFIER, "Expected variable name.");
    auto identifier_node = std::make_unique<vyn::ast::Identifier>(name_token.location, name_token.lexeme);

    vyn::ast::ExprPtr initializer = nullptr;
    SourceLocation end_loc = name_token.location; // Default end_loc if no initializer

    if (this->match(vyn::TokenType::EQ)) {
        initializer = this->expr_parser_.parse_expression();
        if (initializer) {
            end_loc = initializer->loc;
        }
        
        // For auto, infer the type from the initializer
        if (auto_type_inference && initializer) {
            // Type will be inferred during semantic analysis
            // For now, we leave type_expr as nullptr
        }
    } else if (is_const_decl && !initializer) {
        // Constants usually require an initializer (could enforce later)
    } else if (auto_type_inference && !initializer) {
        throw std::runtime_error("'auto' variables must have an initializer at " + 
                               location_to_string(name_token.location));
    }

    if (this->peek().type == vyn::TokenType::SEMICOLON) {
        end_loc = this->peek().location;
        this->consume();
    } else if (this->peek().type == vyn::TokenType::NEWLINE || this->IsAtEnd() || 
               this->peek().type == vyn::TokenType::RBRACE || this->peek().type == vyn::TokenType::DEDENT ||
               this->peek().type == vyn::TokenType::END_OF_FILE ||
               is_statement_start(this->peek().type)) {
        // Optional semicolon - also accept statement starts and end of file
        // since semicolons are optional and the declaration might be the last thing in the file
    } else {
        throw std::runtime_error("Expected statement separator after variable declaration at " + 
                               location_to_string(this->peek().location));
    }

    // Create the VariableDeclaration AST node
    return std::make_unique<vyn::ast::VariableDeclaration>(
        keyword_loc,
        std::move(identifier_node),
        is_const_decl,
        std::move(type_expr),
        std::move(initializer)
    );
}

vyn::ast::ExprPtr StatementParser::parse_pattern() {
    // For now, a simple pattern is just an identifier.
    // This will be expanded for destructuring, etc.
    vyn::token::Token id_token = this->expect(vyn::TokenType::IDENTIFIER, "Expected identifier in pattern.");
    return std::make_unique<vyn::ast::Identifier>(id_token.location, id_token.lexeme); // Use lexeme
}


bool StatementParser::is_statement_start(vyn::TokenType type) const {
    switch (type) {
        case vyn::TokenType::KEYWORD_LET:
        case vyn::TokenType::KEYWORD_MUT:
        case vyn::TokenType::KEYWORD_CONST:
        case vyn::TokenType::KEYWORD_VAR: // Added var
        case vyn::TokenType::KEYWORD_AUTO: // Added auto
        case vyn::TokenType::KEYWORD_ASYNC: // Accept async
        case vyn::TokenType::KEYWORD_CLASS: // Added class
        case vyn::TokenType::KEYWORD_TEMPLATE: // Added template
        case vyn::TokenType::KEYWORD_IF:
        case vyn::TokenType::KEYWORD_WHILE:
        case vyn::TokenType::KEYWORD_FOR:
        case vyn::TokenType::KEYWORD_RETURN:
        case vyn::TokenType::LBRACE:
        case vyn::TokenType::KEYWORD_BREAK:
        case vyn::TokenType::KEYWORD_CONTINUE:
        case vyn::TokenType::KEYWORD_UNSAFE:
        case vyn::TokenType::IDENTIFIER: // Added identifier for relaxed syntax
            return true;
        default:
            return this->expr_parser_.is_expression_start(type); // Changed: An expression can also be a statement
    }
}

vyn::ast::StmtPtr StatementParser::parse_try() {
    SourceLocation try_loc = this->current_location();
    this->expect(vyn::TokenType::KEYWORD_TRY);

    // Parse the try block
    std::unique_ptr<ast::BlockStatement> try_block = nullptr;
    SourceLocation try_block_start_loc = peek().location;
    if (this->peek().type == vyn::TokenType::LBRACE) {
        try_block = this->parse_block();
    } else if (this->peek().type == vyn::TokenType::INDENT) {
        try_block_start_loc = consume().location; // Consume INDENT, capture its location
        std::vector<ast::StmtPtr> stmts;
        while (!this->IsAtEnd() && this->peek().type != vyn::TokenType::DEDENT && this->peek().type != vyn::TokenType::END_OF_FILE) {
            while (!this->IsAtEnd() && this->peek().type == vyn::TokenType::NEWLINE) this->consume();
            if (this->IsAtEnd() || this->peek().type == vyn::TokenType::DEDENT) break;
            // Using a sub-parser as per existing pattern in the file for indented blocks
            StatementParser stmt_parser(this->tokens_, this->pos_, 0 /*TODO: review indent_level for sub-parsers*/, this->current_file_path_, this->type_parser_, this->expr_parser_, this->decl_parser_);
            stmts.push_back(stmt_parser.parse());
            this->pos_ = stmt_parser.get_current_pos();
        }
        this->expect(vyn::TokenType::DEDENT);
        try_block = std::make_unique<ast::BlockStatement>(try_block_start_loc, std::move(stmts));
    } else {
        throw error(peek(), "Expected block (starting with '{' or indent) after 'try'.");
    }

    // Parse a single catch clause (if present)
    std::optional<std::string> catch_variable_name; // Stores the variable name 'e'
    // std::unique_ptr<ast::Identifier> catch_identifier_node; // Future: for richer AST
    // std::unique_ptr<ast::TypeNode> catch_type_node;         // Future: for richer AST
    std::unique_ptr<ast::BlockStatement> catch_block = nullptr;

    if (match(vyn::TokenType::KEYWORD_CATCH)) { // Consumes 'catch'
        if (match(vyn::TokenType::LPAREN)) { // Parses 'catch (e: Type)' or 'catch (e)'
            if (peek().type == vyn::TokenType::IDENTIFIER) {
                token::Token ident_token = consume();
                catch_variable_name = ident_token.lexeme;
                // catch_identifier_node = std::make_unique<ast::Identifier>(ident_token.location, ident_token.lexeme);

                if (match(vyn::TokenType::COLON)) {
                    auto parsed_type = type_parser_.parse(); // Parse the type
                    if (!parsed_type) {
                        throw error(peek(), "Expected type annotation after ':' in catch clause.");
                    }
                    // catch_type_node = std::move(parsed_type); // Store if AST supports it
                }
            } else {
                // If language allows 'catch ()' for anonymous catch-all, handle here.
                // For now, assume identifier is required if parentheses are present.
                throw error(peek(), "Expected identifier within parentheses in catch clause.");
            }
            expect(vyn::TokenType::RPAREN);
        } else if (peek().type == vyn::TokenType::IDENTIFIER) { // Parses 'catch e'
            token::Token ident_token = consume();
            catch_variable_name = ident_token.lexeme;
            // catch_identifier_node = std::make_unique<ast::Identifier>(ident_token.location, ident_token.lexeme);
        }
        // If neither LPAREN nor IDENTIFIER follows 'catch', it's 'catch { ... }' or 'catch <indent> ...'
        // In this case, catch_variable_name remains std::nullopt.

        // Parse the catch block
        SourceLocation catch_block_start_loc = peek().location;
        if (this->peek().type == vyn::TokenType::LBRACE) {
            catch_block = this->parse_block();
        } else if (this->peek().type == vyn::TokenType::INDENT) {
            catch_block_start_loc = consume().location; // Consume INDENT, capture its location
            std::vector<ast::StmtPtr> stmts;
            while (!this->IsAtEnd() && this->peek().type != vyn::TokenType::DEDENT && this->peek().type != vyn::TokenType::END_OF_FILE) {
                while (!this->IsAtEnd() && this->peek().type == vyn::TokenType::NEWLINE) this->consume();
                if (this->IsAtEnd() || this->peek().type == vyn::TokenType::DEDENT) break;
                StatementParser stmt_parser(this->tokens_, this->pos_, 0 /*TODO: review indent_level*/, this->current_file_path_, this->type_parser_, this->expr_parser_, this->decl_parser_);
                stmts.push_back(stmt_parser.parse());
                this->pos_ = stmt_parser.get_current_pos();
            }
            this->expect(vyn::TokenType::DEDENT);
            catch_block = std::make_unique<ast::BlockStatement>(catch_block_start_loc, std::move(stmts));
        } else {
            throw error(peek(), "Expected block (starting with '{' or indent) after 'catch' clause.");
        }
    }

    // Skip any additional catch clauses
    while (match(vyn::TokenType::KEYWORD_CATCH)) {
        // Skip catch parameters or variable name
        if (match(vyn::TokenType::LPAREN)) {
            int depth = 1;
            while (depth > 0 && !this->IsAtEnd()) {
                if (match(vyn::TokenType::LPAREN)) depth++;
                else if (match(vyn::TokenType::RPAREN)) depth--;
                else this->consume();
            }
        } else if (peek().type == vyn::TokenType::IDENTIFIER) {
            this->consume();
        }
        // Skip catch block
        if (peek().type == vyn::TokenType::LBRACE) {
            this->parse_block();
        } else if (peek().type == vyn::TokenType::INDENT) {
            this->consume(); // consume INDENT
            while (!this->IsAtEnd() && peek().type != vyn::TokenType::DEDENT && peek().type != vyn::TokenType::END_OF_FILE) {
                this->consume();
            }
            this->expect(vyn::TokenType::DEDENT);
        } else {
            throw error(peek(), "Expected block after 'catch' in try statement.");
        }
    }

    // Optionally parse a finally block
    std::unique_ptr<ast::BlockStatement> finally_block = nullptr;
    if (match(vyn::TokenType::KEYWORD_FINALLY)) { // Consumes 'finally'
        SourceLocation finally_block_start_loc = peek().location;
        if (this->peek().type == vyn::TokenType::LBRACE) {
            finally_block = this->parse_block();
        } else if (this->peek().type == vyn::TokenType::INDENT) {
            finally_block_start_loc = consume().location; // Consume INDENT, capture its location
            std::vector<ast::StmtPtr> stmts;
            while (!this->IsAtEnd() && this->peek().type != vyn::TokenType::DEDENT && this->peek().type != vyn::TokenType::END_OF_FILE) {
                while (!this->IsAtEnd() && this->peek().type == vyn::TokenType::NEWLINE) this->consume();
                if (this->IsAtEnd() || this->peek().type == vyn::TokenType::DEDENT) break;
                StatementParser stmt_parser(this->tokens_, this->pos_, 0/*TODO: review indent_level*/, this->current_file_path_, this->type_parser_, this->expr_parser_, this->decl_parser_);
                stmts.push_back(stmt_parser.parse());
                this->pos_ = stmt_parser.get_current_pos();
            }
            this->expect(vyn::TokenType::DEDENT);
            finally_block = std::make_unique<ast::BlockStatement>(finally_block_start_loc, std::move(stmts));
        } else {
            throw error(peek(), "Expected block (starting with '{' or indent) after 'finally'.");
        }
    }

    // Assuming ast::TryStatement constructor takes: try_loc, try_block, catch_variable_name (optional<string>), catch_block, finally_block
    return std::make_unique<ast::TryStatement>(try_loc, std::move(try_block), catch_variable_name, std::move(catch_block), std::move(finally_block));
}

// Minimal stub implementations for defer and await
vyn::ast::StmtPtr StatementParser::parse_defer() {
    this->consume(); // Consume 'defer'
    // TODO: Properly parse defer statement
    return nullptr; // Replace with a real DeferStatement node when implemented
}
vyn::ast::StmtPtr StatementParser::parse_await() {
    SourceLocation await_loc = consume().location; // Consume 'await' and get its location
    vyn::ast::ExprPtr expression = expr_parser_.parse_expression(); // Parse the expression being awaited

    if (!expression) {
        throw error(peek(), "Expected expression after 'await'.");
    }

    // Optional semicolon or newline handling (similar to ExpressionStatement)
    if (match(vyn::TokenType::SEMICOLON)) {
        // Semicolon consumed.
    } else if (peek().type == vyn::TokenType::NEWLINE ||
               IsAtEnd() ||
               peek().type == vyn::TokenType::RBRACE ||
               peek().type == vyn::TokenType::DEDENT) {
        // Optional semicolon: fine.
    } else {
        throw error(peek(), "Expected semicolon or newline after await statement.");
    }

    // Create an ExpressionStatement with a UnaryExpression for await
    // This assumes Await is handled as a UnaryExpression in the AST for now.
    // If a dedicated AwaitStatement or AwaitExpression AST node exists and is preferred,
    // adjust accordingly.
    token::Token await_op_token(TokenType::KEYWORD_AWAIT, "await", await_loc); // Create a token for await operator
    auto await_unary_expr = std::make_unique<ast::UnaryExpression>(await_loc, await_op_token, std::move(expression));
    return std::make_unique<ast::ExpressionStatement>(await_loc, std::move(await_unary_expr));
}

// Parses an unsafe block: 'unsafe { ... }'
std::unique_ptr<vyn::ast::UnsafeStatement> StatementParser::parse_unsafe() {
    SourceLocation loc = expect(vyn::TokenType::KEYWORD_UNSAFE, "Expected 'unsafe'").location;
    auto blockStmt = parse_block(); // parse_block consumes '{' and '}'
    return std::make_unique<vyn::ast::UnsafeStatement>(loc, std::move(blockStmt));
}
} // namespace vyn

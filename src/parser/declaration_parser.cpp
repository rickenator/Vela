#include "vyn/parser/parser.hpp"
#include "vyn/parser/ast.hpp"
#include <stdexcept> // For std::runtime_error
#include <vector>
#include <memory>
#include <string> // Required for std::to_string
#include <iostream> // Required for std::cerr


// Global helper function for converting SourceLocation to string
// inline std::string location_to_string(const vyn::SourceLocation& loc) {
// return loc.filePath + ":" + std::to_string(loc.line) + ":" + std::to_string(loc.column);
// }
// MOVED to parser.hpp


namespace vyn {

// Constructor updated to accept TypeParser, ExpressionParser, and StatementParser references
DeclarationParser::DeclarationParser(const std::vector<token::Token>& tokens, size_t& pos, const std::string& file_path, TypeParser& type_parser, ExpressionParser& expr_parser, StatementParser& stmt_parser)
    : BaseParser(tokens, pos, file_path), type_parser_(type_parser), expr_parser_(expr_parser), stmt_parser_(stmt_parser) {
    stmt_parser_.set_declaration_parser(this); // Set the back-reference
}

// Returns a declaration node.
// Expects the current token to be the start of a declaration.
// A declaration can be a function, struct, enum, impl, type alias, or variable declaration.
// It can also be a template declaration.
vyn::ast::DeclPtr DeclarationParser::parse() {
    while (this->peek().type == vyn::TokenType::COMMENT) {
        this->consume();
    }
    token::Token current_token = this->peek();
    token::Token next_token = this->peekNext();

    if (current_token.type == vyn::TokenType::KEYWORD_FN ||
        current_token.type == vyn::TokenType::KEYWORD_ASYNC || // Accept async fn
        (current_token.type == vyn::TokenType::IDENTIFIER && current_token.lexeme == "async" && next_token.type == vyn::TokenType::KEYWORD_FN)) {
        return this->parse_function();
    } else if (current_token.type == vyn::TokenType::KEYWORD_STRUCT) {
        auto struct_decl = this->parse_struct();
        return struct_decl;
    } else if (current_token.type == vyn::TokenType::KEYWORD_IMPL) {
        auto impl_decl = this->parse_impl();
        return impl_decl;
    } else if (current_token.type == vyn::TokenType::KEYWORD_CLASS) { // Changed this line
        auto class_decl = this->parse_class_declaration();
        return class_decl;
    } else if (current_token.type == vyn::TokenType::KEYWORD_ENUM) {
        auto enum_decl = this->parse_enum_declaration();
        return enum_decl;
    } else if (current_token.type == vyn::TokenType::IDENTIFIER && current_token.lexeme == "type") { // KEYWORD_TYPE
        return this->parse_type_alias_declaration();
    } else if (current_token.type == vyn::TokenType::KEYWORD_LET ||
               current_token.type == vyn::TokenType::KEYWORD_MUT || // Changed from KEYWORD_VAR
               current_token.type == vyn::TokenType::KEYWORD_CONST ||
               current_token.type == vyn::TokenType::KEYWORD_VAR || // Accept var
               current_token.type == vyn::TokenType::KEYWORD_AUTO) { // Accept auto
        return this->parse_global_var_declaration();
    } else {
        // Check if this could be a relaxed syntax variable declaration (Type name)
        // We need to try to parse it as a type and see if we succeed
        size_t saved_pos = this->pos_;  // Save position to restore if this isn't a declaration
        
        try {
            auto type_node = this->type_parser_.parse();
            if (type_node && this->peek().type == vyn::TokenType::IDENTIFIER) {
                // This appears to be a relaxed syntax variable declaration
                // Rewind position and parse as variable declaration
                this->pos_ = saved_pos;
                return this->parse_global_var_declaration();
            }
        } catch (...) {
            // Not a valid type, so not a relaxed syntax declaration
        }
        
        // Restore position if this wasn't a relaxed syntax declaration
        this->pos_ = saved_pos;
    }
    
    if (current_token.type == vyn::TokenType::KEYWORD_TEMPLATE) { // Changed this line
        return this->parse_template_declaration();
    } else if (current_token.type == vyn::TokenType::KEYWORD_IMPORT ||
               (current_token.type == vyn::TokenType::IDENTIFIER && current_token.lexeme == "import")) {
        return this->parse_import_declaration();
    } else if (current_token.type == vyn::TokenType::KEYWORD_SMUGGLE ||
               (current_token.type == vyn::TokenType::IDENTIFIER && current_token.lexeme == "smuggle")) {
        return this->parse_smuggle_declaration();
    }
    return nullptr;
}

// Note: ast.hpp does not have GenericParamNode, ParamNode, FuncDeclNode, StructDeclNode, ImplDeclNode, FieldDeclNode, ClassDeclNode, EnumVariantNode, EnumDeclNode, GlobalVarDeclNode
// These will need to be added to ast.hpp or the parser needs to be updated to use existing/general types.
// For now, assuming these types *will* exist as specialized Declaration nodes or similar.
// The return types and make_unique calls will reflect this assumption.
// If they are meant to be generic vyn::NodePtr, the parser.hpp and these definitions need to change.

// Assuming GenericParameter is a struct/class that can be held in a vector, not necessarily a Node.
// For now, let's assume it's a structure specific to FunctionDeclaration or similar, not a generic AST Node.
// If GenericParamNode is intended to be an AST node, it should derive from vyn::Node.
// For now, changing return type to std::vector<std::unique_ptr<vyn::ast::GenericParamNode>> as per parser.hpp
std::vector<std::unique_ptr<vyn::ast::GenericParameter>> DeclarationParser::parse_generic_params() {
    std::vector<std::unique_ptr<vyn::ast::GenericParameter>> generic_params;
    if (this->match(vyn::TokenType::LT)) { // <
        do {
            SourceLocation param_loc = this->current_location();
            if (this->peek().type != vyn::TokenType::IDENTIFIER) {
                throw std::runtime_error("Expected identifier for generic parameter name at " + location_to_string(param_loc));
            }
            auto param_name = std::make_unique<ast::Identifier>(param_loc, this->consume().lexeme);

            std::vector<ast::TypeNodePtr> bounds;
            if (this->match(vyn::TokenType::COLON)) {
                do {
                    auto bound_type = this->type_parser_.parse();
                    if (!bound_type) {
                        throw std::runtime_error("Expected trait bound type after \':\' for generic parameter at " + location_to_string(this->current_location()));
                    }
                    bounds.push_back(std::move(bound_type));
                } while (this->match(vyn::TokenType::PLUS));
            }
            generic_params.push_back(std::make_unique<vyn::ast::GenericParameter>(param_loc, std::move(param_name), std::move(bounds)));
        } while (this->match(vyn::TokenType::COMMA));
        this->expect(vyn::TokenType::GT); // >
    }
    return generic_params;
}


// parse_param returns vyn::NodePtr as per parser.hpp
// ast.hpp has FunctionParameter struct, but not as a Node.
// This implies parse_param should create a structure that's then used by parse_function,
// rather than returning a generic NodePtr directly into a list of Nodes for generic_params.
// For now, adhering to parser.hpp return type.
// This will likely require creating a wrapper Node for FunctionParameter or changing parser.hpp.
// Let's assume for now it creates an Identifier node for the parameter name.
std::unique_ptr<vyn::ast::Node> DeclarationParser::parse_param() {
    SourceLocation loc = this->current_location();
    // bool is_mutable = this->match(vyn::TokenType::KEYWORD_MUT).has_value(); // \'mut\' keyword for params?

    if (this->peek().type != vyn::TokenType::IDENTIFIER) {
        throw std::runtime_error("Expected parameter name (identifier) at " + location_to_string(loc));
    }
    auto name_ident = std::make_unique<ast::Identifier>(this->current_location(), this->consume().lexeme);

    this->expect(vyn::TokenType::COLON);

    auto type_annot = this->type_parser_.parse(); // Returns TypeNodePtr
    if (!type_annot) {
        throw std::runtime_error("Expected type annotation for parameter \'" + name_ident->name + "\' at " + location_to_string(this->current_location()));
    }
    
    if (this->match(vyn::TokenType::EQ)) {
        auto default_value = this->expr_parser_.parse_expression();
        if (!default_value) {
            throw std::runtime_error("Expected expression for default value of parameter \\\'" + name_ident->name + "\\\' at " + location_to_string(this->current_location()));
        }
    }
    // Returning Identifier as the Node. TypeAnnotation is associated but not part of this single Node.
    // This is a simplification. A proper ParameterNode would be better.
    // For now, to satisfy NodePtr, let's return the name. The type is parsed but not directly part of this return.
    // This means parse_function will need to call parse_param and then separately parse_type or this needs to return a more complex node.
    // Given parser.hpp, we return NodePtr. Let's assume FunctionParameter struct is built inside parse_function.
    // So, parse_param should probably return that struct.
    // But parser.hpp says parse_param returns vyn::NodePtr. This is a contradiction.
    // For now, let's assume parse_param is a helper that returns FunctionParameter struct, and parser.hpp is outdated for this specific case or refers to a different context of "param".
    // Let's change parse_param to return FunctionParameter for internal use by parse_function.
    // This means parser.hpp needs an update if parse_param is public API returning NodePtr.
    // For now, this function will not be used directly based on parser.hpp's return type.
    // The code below will be for a hypothetical internal helper.
    // ---
    // Reverting to parser.hpp signature for now and returning Identifier.
    // The type annotation will be parsed but not directly part of the returned Identifier node.
    // The caller (parse_function) will need to manage this.
    return name_ident; // This is problematic as type info is lost.
}

// Actual internal helper to build FunctionParameter struct
vyn::ast::FunctionParameter DeclarationParser::parse_function_parameter_struct() {
    SourceLocation loc = this->current_location();
    
    // Check if we're using the standard syntax (var<Type> or const<Type>) or
    // the relaxed syntax (Type or const Type)
    bool is_mutable = true; // Default to mutable (var) if not specified
    bool using_standard_syntax = true;
    
    // Check for relaxed syntax with 'const' keyword first
    if (this->match(vyn::TokenType::KEYWORD_CONST)) {
        is_mutable = false;
        
        // Check if the next token is '<' (standard syntax) or an identifier (relaxed syntax)
        if (this->peek().type == vyn::TokenType::LT) {
            // Standard syntax: const<Type>
            using_standard_syntax = true;
            this->expect(vyn::TokenType::LT);
        } else {
            // Relaxed syntax: const Type
            using_standard_syntax = false;
        }
    } else if (this->match(vyn::TokenType::KEYWORD_VAR)) {
        // Standard syntax: var<Type>
        is_mutable = true;
        this->expect(vyn::TokenType::LT);
    } else {
        // Relaxed syntax: Type (no var/const keyword)
        is_mutable = true;
        using_standard_syntax = false;
    }
    
    // Parse parameter type
    auto type_annot = this->type_parser_.parse(); // Returns TypeNodePtr
    if (!type_annot) {
        throw std::runtime_error("Expected type annotation for parameter at " + location_to_string(this->current_location()));
    }
    
    // If using standard syntax, expect the closing '>'
    if (using_standard_syntax) {
        this->expect(vyn::TokenType::GT);
    }
    
    // Parse parameter name
    if (this->peek().type != vyn::TokenType::IDENTIFIER) {
        throw std::runtime_error("Expected parameter name (identifier) after type at " + location_to_string(this->current_location()));
    }
    auto name_ident = std::make_unique<ast::Identifier>(this->current_location(), this->consume().lexeme);
    
    // Parse optional default value
    if (this->match(vyn::TokenType::EQ)) {
        auto default_value = this->expr_parser_.parse_expression();
        if (!default_value) {
            throw std::runtime_error("Expected expression for default value of parameter \\\'" + name_ident->name + "\\\' at " + location_to_string(this->current_location()));
        }
        // default_value is parsed but not stored in FunctionParameter
    }
    
    return vyn::ast::FunctionParameter(std::move(name_ident), std::move(type_annot), is_mutable);
}


// Returns vyn::FunctionDeclaration as per parser.hpp (assuming it's a DeclPtr compatible type)
std::unique_ptr<vyn::ast::FunctionDeclaration> DeclarationParser::parse_function() {
    // Skip INDENT/DEDENT tokens before parsing a function (especially for indentation-based bodies)
    this->skip_indents_dedents();
    SourceLocation loc = this->current_location();
    bool is_async = this->match(vyn::TokenType::KEYWORD_ASYNC).has_value();
    #ifdef VERBOSE
    std::cerr << "[DECL_PARSER] Function is " << (is_async ? "async" : "sync") << std::endl;
    #endif
    
    bool is_extern = this->match(vyn::TokenType::KEYWORD_EXTERN).has_value();
    #ifdef VERBOSE
    if (is_extern) {
        std::cerr << "[DECL_PARSER] Function is extern" << std::endl;
    }
    #endif
    
    this->expect(vyn::TokenType::KEYWORD_FN);
    
    // Parse the return type enclosed in angle brackets (new syntax)
    ast::TypeNodePtr return_type_node = nullptr;
    if (this->match(vyn::TokenType::LT)) { // <
        return_type_node = this->type_parser_.parse();
        if (!return_type_node) {
            throw std::runtime_error("Expected return type after \'<\' in function declaration at " + 
                                    location_to_string(this->current_location()));
        }
        this->expect(vyn::TokenType::GT); // >
    } else {
        // Default to void if no return type specified
        return_type_node = std::make_unique<ast::TypeName>(this->current_location(), 
                                                         std::make_unique<ast::Identifier>(this->current_location(), "Void"));
    }

    // Handle normal function names, \'operator+\' syntax, and keyword \'operator\' followed by operator token
    std::unique_ptr<ast::Identifier> name;
    
    if (this->peek().type == vyn::TokenType::IDENTIFIER) {
        std::string name_lexeme = this->peek().lexeme;
        SourceLocation name_loc = this->peek().location; 
        this->consume();
        
        // Check for operator keyword and operator symbol pattern
        if (name_lexeme == "operator" && this->IsOperator(this->peek())) {
            auto op_token = this->consume();
            name = std::make_unique<ast::Identifier>(name_loc, name_lexeme + op_token.lexeme);
        } else {
            name = std::make_unique<ast::Identifier>(name_loc, name_lexeme);
        }
    } else if (this->peek().type == vyn::TokenType::KEYWORD_OPERATOR) {
        SourceLocation op_loc = this->peek().location; 
        this->consume();
        
        if (!this->IsOperator(this->peek())) {
            throw std::runtime_error("Expected operator symbol after \'operator\' keyword at " + 
                                     location_to_string(this->current_location()));
        }
        auto op_token = this->consume();
        name = std::make_unique<ast::Identifier>(op_loc, "operator" + op_token.lexeme);
    } else {
        throw std::runtime_error("Expected function name at " + location_to_string(this->current_location()));
    }

    this->expect(vyn::TokenType::LPAREN);
    std::vector<vyn::ast::FunctionParameter> params_structs;
    if (this->peek().type != vyn::TokenType::RPAREN) {
        do {
            params_structs.push_back(this->parse_function_parameter_struct());
        } while (this->match(vyn::TokenType::COMMA));
    }
    this->expect(vyn::TokenType::RPAREN);

    // Variables to hold type information
    ast::TypeNodePtr throws_type = nullptr;

    // Expect arrow as mandatory separator between signature and body
    this->expect(vyn::TokenType::ARROW); // Arrow is now mandatory

    // Support for \\\'throws\\\' keyword after the return type
    if (this->peek().type == vyn::TokenType::IDENTIFIER && this->peek().lexeme == "throws") { // KEYWORD_THROWS if it exists, else IDENTIFIER
        this->consume();
        
        // Parse the error type that can be thrown
        if (this->peek().type == vyn::TokenType::IDENTIFIER) {
            auto error_type_name_loc = this->current_location();
            auto error_type_name_str = this->consume().lexeme;
            auto error_type_identifier = std::make_unique<ast::Identifier>(error_type_name_loc, error_type_name_str);
            throws_type = std::make_unique<ast::TypeName>(error_type_name_loc, std::move(error_type_identifier));
        } else {
            throw std::runtime_error("Expected error type after \\\'throws\\\' at " + location_to_string(this->current_location()));
        }
    }

    std::unique_ptr<ast::BlockStatement> body = nullptr;
    // Accept function declarations without a body (forward declarations)
    if (this->peek().type == vyn::TokenType::IDENTIFIER) {
        // Handle constructor return expressions like: Node { is_leaf: is_leaf_param }
        if (return_type_node && return_type_node->getCategory() == ast::TypeNode::Category::IDENTIFIER) {
            // Extract identifier name from return type
            // Cast to TypeName to access the identifier
            if (ast::TypeName* return_type_name_node = dynamic_cast<ast::TypeName*>(return_type_node.get())) {
                if (return_type_name_node->identifier) {
                    const auto& id_name = return_type_name_node->identifier->name;
                    if (this->peek().lexeme == id_name) {
                        SourceLocation stmt_loc = this->current_location();
                        vyn::StatementParser stmt_parser(this->tokens_, this->pos_, 0, this->current_file_path_, this->type_parser_, this->expr_parser_, this);
                        ast::StmtPtr single_stmt = stmt_parser.parse();
                        this->pos_ = stmt_parser.get_current_pos();
                        if (single_stmt) {
                            std::vector<ast::StmtPtr> statements;
                            statements.push_back(std::move(single_stmt));
                            body = std::make_unique<ast::BlockStatement>(stmt_loc, std::move(statements));
                        }
                    }
                }
            }
        }
    } else if (this->peek().type == vyn::TokenType::LBRACE) {
        vyn::StatementParser stmt_parser(this->tokens_, this->pos_, 0, this->current_file_path_, this->type_parser_, this->expr_parser_, this);
        body = stmt_parser.parse_block();
        this->pos_ = stmt_parser.get_current_pos();
    } else if (this->peek().type == vyn::TokenType::INDENT) {
        // Indentation-based function body
        this->consume(); // Consume exactly one INDENT
        std::vector<ast::StmtPtr> statements;
        // Use a local StatementParser with the current pos_ reference
        while (!this->IsAtEnd() && this->peek().type != vyn::TokenType::DEDENT && this->peek().type != vyn::TokenType::END_OF_FILE) {
            while (!this->IsAtEnd() && this->peek().type == vyn::TokenType::NEWLINE) this->consume();
            if (this->IsAtEnd() || this->peek().type == vyn::TokenType::DEDENT) break;
            vyn::StatementParser stmt_parser(this->tokens_, this->pos_, 0, this->current_file_path_, this->type_parser_, this->expr_parser_, this);
            statements.push_back(stmt_parser.parse());
            this->pos_ = stmt_parser.get_current_pos();
        }
        if (this->peek().type == vyn::TokenType::DEDENT) this->consume(); // Consume exactly one DEDENT
        body = std::make_unique<ast::BlockStatement>(this->current_location(), std::move(statements));
    } else {
        // Forward declaration or no body (e.g. for extern functions).
        // 'body' remains nullptr, which is correct for these cases.
    }

    return std::make_unique<vyn::ast::FunctionDeclaration>(loc, std::move(name), std::move(params_structs), std::move(body), is_async, std::move(return_type_node));
}

// StructDeclNode not in ast.hpp. Assuming a Declaration type for it.
// parser.hpp: std::unique_ptr<vyn::Declaration> parse_struct();
// Similar to struct, this needs a vyn::StructDeclaration in ast.hpp.
std::unique_ptr<vyn::ast::Declaration> DeclarationParser::parse_struct() {
    SourceLocation loc = this->current_location();
    this->expect(vyn::TokenType::KEYWORD_STRUCT);

    if (this->peek().type != vyn::TokenType::IDENTIFIER) {
        throw std::runtime_error("Expected struct name at " + location_to_string(this->current_location()));
    }
    auto name = std::make_unique<ast::Identifier>(this->current_location(), this->consume().lexeme);

    auto generic_params = this->parse_generic_params();

    this->expect(vyn::TokenType::LBRACE);
    
    std::vector<std::unique_ptr<ast::FieldDeclaration>> fields;

    while (this->peek().type != vyn::TokenType::RBRACE && this->peek().type != vyn::TokenType::END_OF_FILE) {
        SourceLocation field_loc = this->current_location();
        
        if (this->peek().type != vyn::TokenType::IDENTIFIER) {
            throw std::runtime_error("Expected field name in struct \'" + name->name + "\' at " + location_to_string(this->current_location()));
        }
        auto field_name = std::make_unique<ast::Identifier>(this->current_location(), this->consume().lexeme);
        
        this->expect(vyn::TokenType::COLON);
        
        auto field_type_node = this->type_parser_.parse();
        if (!field_type_node) {
            throw std::runtime_error("Expected type for field \'" + field_name->name + "\' in struct \'" + name->name + "\' at " + location_to_string(this->current_location()));
        }
        
        fields.push_back(std::make_unique<ast::FieldDeclaration>(field_loc, std::move(field_name), std::move(field_type_node), nullptr, false));

        this->skip_comments_and_newlines(); 
        if (this->match(vyn::TokenType::COMMA)) {
             this->skip_comments_and_newlines();
             if (this->peek().type == vyn::TokenType::RBRACE) break;
        } else if (this->peek().type != vyn::TokenType::RBRACE) {
            throw std::runtime_error("Expected comma or closing brace after struct field in \'" + name->name + "\' at " + location_to_string(this->current_location()));
        }
    }
    this->expect(vyn::TokenType::RBRACE);

    return std::make_unique<ast::StructDeclaration>(loc, std::move(name), std::move(generic_params), std::move(fields));
}


// ImplDeclNode not in ast.hpp. Assuming a Declaration type for it.
// parser.hpp: std::unique_ptr<vyn::Declaration> parse_impl();
// Similar to struct, this needs a vyn::ImplDeclaration in ast.hpp.
std::unique_ptr<vyn::ast::Declaration> DeclarationParser::parse_impl() {
    SourceLocation loc = this->current_location();
    this->expect(vyn::TokenType::KEYWORD_IMPL);

    auto generic_params = this->parse_generic_params();

    ast::TypeNodePtr trait_type_node = nullptr;
    ast::TypeNodePtr self_type_node = this->type_parser_.parse();
    if (!self_type_node) {
        throw std::runtime_error("Expected type name in impl block at " + location_to_string(this->current_location()));
    }

    if (this->match(vyn::TokenType::KEYWORD_FOR)) {
        trait_type_node = std::move(self_type_node);
        self_type_node = this->type_parser_.parse();
        if (!self_type_node) {
            throw std::runtime_error("Expected type name after \'for\' in impl block at " + location_to_string(this->current_location()));
        }
    }

    this->expect(vyn::TokenType::LBRACE);
    std::vector<std::unique_ptr<ast::FunctionDeclaration>> methods;
    while (!this->check(vyn::TokenType::RBRACE) && !this->IsAtEnd()) {
        methods.push_back(this->parse_function()); 
    }
    this->expect(vyn::TokenType::RBRACE);

    return std::make_unique<ast::ImplDeclaration>(loc, std::move(self_type_node), std::move(methods), nullptr, std::move(generic_params), std::move(trait_type_node));
}


std::unique_ptr<vyn::ast::Declaration> DeclarationParser::parse_enum_declaration() {
    SourceLocation loc = this->current_location();
    this->expect(vyn::TokenType::KEYWORD_ENUM);

    if (this->peek().type != vyn::TokenType::IDENTIFIER) {
        throw std::runtime_error("Expected enum name (identifier) at " + location_to_string(this->current_location()));
    }
    auto name = std::make_unique<ast::Identifier>(this->current_location(), this->consume().lexeme);

    auto generic_params = this->parse_generic_params();

    this->expect(vyn::TokenType::LBRACE);
    this->skip_comments_and_newlines();

    std::vector<std::unique_ptr<ast::EnumVariant>> variants;

    while (this->peek().type != vyn::TokenType::RBRACE && this->peek().type != vyn::TokenType::END_OF_FILE) {
        auto variant_node = this->parse_enum_variant();
        if (!variant_node) { 
            throw std::runtime_error("Failed to parse enum variant for enum \'" + name->name + "\' at " + location_to_string(this->current_location()));
        }
        variants.push_back(std::move(variant_node));

        this->skip_comments_and_newlines();
        if (this->match(vyn::TokenType::COMMA)) {
            this->skip_comments_and_newlines();
            if (this->peek().type == vyn::TokenType::RBRACE) {
                break;
            }
        } else if (this->peek().type != vyn::TokenType::RBRACE) {
             if (this->peek().type != vyn::TokenType::IDENTIFIER && this->peek().type != vyn::TokenType::RBRACE) {
                  throw std::runtime_error("Expected comma, closing brace, or next variant identifier after enum variant in enum \'" + name->name + "\' at " + location_to_string(this->current_location()));
             }
        }
    }

    this->expect(vyn::TokenType::RBRACE);

    return std::make_unique<ast::EnumDeclaration>(loc, std::move(name), std::move(generic_params), std::move(variants));
}


// Returns vyn::TypeAliasDeclaration as per parser.hpp (DeclPtr compatible)
std::unique_ptr<vyn::ast::TypeAliasDeclaration> DeclarationParser::parse_type_alias_declaration() {
    SourceLocation loc = this->current_location();
    this->expect(vyn::TokenType::KEYWORD_TYPE);

    if (this->peek().type != vyn::TokenType::IDENTIFIER) {
        throw std::runtime_error("Expected type alias name (identifier) at " + location_to_string(this->current_location()));
    }
    auto name = std::make_unique<ast::Identifier>(this->current_location(), this->consume().lexeme);

    auto generic_params = this->parse_generic_params();

    this->expect(vyn::TokenType::EQ);

    auto aliased_type_node = this->type_parser_.parse();
    if (!aliased_type_node) {
        throw std::runtime_error("Expected type definition after \'=\' for type alias \'" + name->name + "\' at " + location_to_string(this->current_location()));
    }

    this->expect(vyn::TokenType::SEMICOLON);
    return std::make_unique<vyn::ast::TypeAliasDeclaration>(loc, std::move(name), std::move(aliased_type_node));
}

// GlobalVarDeclNode not in ast.hpp. VariableDeclaration is the one.
// parser.hpp: std::unique_ptr<vyn::VariableDeclaration> parse_global_var_declaration();
std::unique_ptr<vyn::ast::VariableDeclaration> DeclarationParser::parse_global_var_declaration() {
    SourceLocation loc = this->current_location();
    
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
        // This might be the relaxed syntax with just a type name
        // Try to parse it as a type
        using_standard_syntax = false;
        is_const_decl = false;
    }
    
    // Parse the type
    ast::TypeNodePtr type_node = nullptr;
    
    if (!auto_type_inference) {
        if (using_standard_syntax) {
            // Standard syntax: var<Type> or const<Type>
            this->expect(vyn::TokenType::LT);
            type_node = this->type_parser_.parse();
            if (!type_node) {
                throw std::runtime_error("Expected type annotation inside '<>' in declaration at " + 
                                         location_to_string(this->current_location()));
            }
            this->expect(vyn::TokenType::GT);
        } else {
            // Relaxed syntax: Type or const Type
            type_node = this->type_parser_.parse();
            if (!type_node) {
                throw std::runtime_error("Expected type in declaration at " + 
                                         location_to_string(this->current_location()));
            }
        }
    }
    
    // Next token must be the variable name
    if (this->peek().type != vyn::TokenType::IDENTIFIER) {
        throw std::runtime_error("Expected identifier after type annotation in declaration at " + 
                                location_to_string(this->current_location()));
    }
    auto identifier = std::make_unique<ast::Identifier>(this->current_location(), this->consume().lexeme);

    ast::ExprPtr initializer = nullptr;
    if (this->match(vyn::TokenType::EQ)) {
        initializer = this->expr_parser_.parse_expression();
        if (!initializer) {
            throw std::runtime_error("Expected initializer expression after '=' in declaration at " + 
                                    location_to_string(this->current_location()));
        }
        
        // For auto, infer the type from the initializer
        if (auto_type_inference && initializer) {
            // Type will be inferred during semantic analysis
            // For now, we leave type_node as nullptr
        }
    } else if (is_const_decl && !initializer) {
        // Constants usually require an initializer (could enforce later)
    } else if (auto_type_inference && !initializer) {
        throw std::runtime_error("'auto' variables must have an initializer at " + 
                                location_to_string(this->current_location()));
    }

    this->expect(vyn::TokenType::SEMICOLON);
    return std::make_unique<vyn::ast::VariableDeclaration>(loc, std::move(identifier), is_const_decl, 
                                                       std::move(type_node), std::move(initializer));
}

// New: Parse Template Declaration
std::unique_ptr<vyn::ast::Declaration> DeclarationParser::parse_template_declaration() {
    SourceLocation loc = this->current_location();
    this->expect(vyn::TokenType::KEYWORD_TEMPLATE);

    // Parse template name
    if (this->peek().type != vyn::TokenType::IDENTIFIER) {
        throw std::runtime_error("Expected identifier after 'template' at " + location_to_string(this->current_location()));
    }
    auto name = std::make_unique<ast::Identifier>(this->current_location(), this->consume().lexeme);

    // Parse generic parameters (e.g., <T, U>)
    std::vector<std::unique_ptr<vyn::ast::GenericParameter>> generic_params;
    if (this->peek().type == vyn::TokenType::LT) {
        generic_params = this->parse_generic_params();
    }

    // Parse body (should be a class, struct, enum, or function declaration)
    this->expect(vyn::TokenType::LBRACE);
    this->skip_comments_and_newlines();
    std::unique_ptr<ast::Declaration> body_decl = nullptr;
    if (this->peek().type == vyn::TokenType::KEYWORD_CLASS) {
        body_decl = this->parse_class_declaration();
    } else if (this->peek().type == vyn::TokenType::KEYWORD_STRUCT) {
        body_decl = this->parse_struct();
    } else if (this->peek().type == vyn::TokenType::KEYWORD_ENUM) {
        body_decl = this->parse_enum_declaration();
    } else if (this->peek().type == vyn::TokenType::KEYWORD_FN || this->peek().type == vyn::TokenType::KEYWORD_ASYNC) {
        body_decl = this->parse_function();
    } else {
        throw std::runtime_error("Expected a class, struct, enum, or function declaration inside template body at " + location_to_string(this->current_location()));
    }
    this->expect(vyn::TokenType::RBRACE);
    return std::make_unique<ast::TemplateDeclaration>(loc, std::move(name), std::move(generic_params), std::move(body_decl));
}

// --- Import and Smuggle Declarations ---
std::unique_ptr<vyn::ast::ImportDeclaration> DeclarationParser::parse_import_declaration() {
    vyn::SourceLocation loc = this->current_location();
    this->expect(vyn::TokenType::KEYWORD_IMPORT);
    if (this->peek().type != vyn::TokenType::IDENTIFIER) {
        throw std::runtime_error("Expected identifier after \'import\' at " + location_to_string(this->current_location()));
    }
    std::string path = this->consume().lexeme;
    while (this->peek().type == vyn::TokenType::COLONCOLON || this->peek().type == vyn::TokenType::DOT) {
        this->consume();
        if (this->peek().type != vyn::TokenType::IDENTIFIER) {
            throw std::runtime_error("Expected identifier in import path at " + location_to_string(this->current_location()));
        }
        path += "::" + this->consume().lexeme;
    }
    std::unique_ptr<ast::Identifier> alias = nullptr;
    if (this->match(vyn::TokenType::KEYWORD_AS)) {
        if (this->peek().type != vyn::TokenType::IDENTIFIER) {
            throw std::runtime_error("Expected identifier after \'as\' in import at " + location_to_string(this->current_location()));
        }
        alias = std::make_unique<ast::Identifier>(this->current_location(), this->consume().lexeme);
    }
    this->match(vyn::TokenType::SEMICOLON);
    auto source = std::make_unique<ast::StringLiteral>(loc, path);
    std::vector<ast::ImportSpecifier> specifiers;
    if (alias) {
        specifiers.emplace_back(nullptr, std::move(alias));
    }
    return std::make_unique<vyn::ast::ImportDeclaration>(loc, std::move(source), std::move(specifiers));
}

std::unique_ptr<vyn::ast::ImportDeclaration> DeclarationParser::parse_smuggle_declaration() {
    vyn::SourceLocation loc = this->current_location();
    this->expect(vyn::TokenType::KEYWORD_SMUGGLE);
    if (this->peek().type != vyn::TokenType::IDENTIFIER) {
        throw std::runtime_error("Expected identifier after \'smuggle\' at " + location_to_string(this->current_location()));
    }
    std::string path = this->consume().lexeme;
    while (this->peek().type == vyn::TokenType::COLONCOLON || this->peek().type == vyn::TokenType::DOT) {
        this->consume();
        if (this->peek().type != vyn::TokenType::IDENTIFIER) {
            throw std::runtime_error("Expected identifier in smuggle path at " + location_to_string(this->current_location()));
        }
        path += "::" + this->consume().lexeme;
    }
    std::unique_ptr<ast::Identifier> alias = nullptr;
    if (this->match(vyn::TokenType::KEYWORD_AS)) {
        if (this->peek().type != vyn::TokenType::IDENTIFIER) {
            throw std::runtime_error("Expected identifier after \'as\' in smuggle at " + location_to_string(this->current_location()));
        }
        alias = std::make_unique<ast::Identifier>(this->current_location(), this->consume().lexeme);
    }
    this->match(vyn::TokenType::SEMICOLON);
    auto source = std::make_unique<ast::StringLiteral>(loc, path);
    std::vector<ast::ImportSpecifier> specifiers;
    if (alias) {
        specifiers.emplace_back(nullptr, std::move(alias));
    }
    return std::make_unique<vyn::ast::ImportDeclaration>(loc, std::move(source), std::move(specifiers));
}

std::unique_ptr<vyn::ast::Declaration> DeclarationParser::parse_class_declaration() {
    vyn::SourceLocation loc = this->current_location();
    this->expect(vyn::TokenType::KEYWORD_CLASS);

    if (this->peek().type != vyn::TokenType::IDENTIFIER) {
        throw std::runtime_error("Expected class name at " + location_to_string(this->current_location()));
    }
    auto class_name = std::make_unique<ast::Identifier>(this->current_location(), this->consume().lexeme);

    auto generic_params = this->parse_generic_params();

    this->expect(vyn::TokenType::LBRACE);
    
    std::vector<ast::DeclPtr> members;

    while (this->peek().type != vyn::TokenType::RBRACE && this->peek().type != vyn::TokenType::END_OF_FILE) {
        this->skip_comments_and_newlines();
        
        // Parse fields and methods
        if (this->peek().type == vyn::TokenType::KEYWORD_VAR || // Added KEYWORD_VAR
            this->peek().type == vyn::TokenType::KEYWORD_MUT ||
            this->peek().type == vyn::TokenType::KEYWORD_CONST ||
            this->peek().type == vyn::TokenType::KEYWORD_LET ||
            this->peek().type == vyn::TokenType::IDENTIFIER) {
            
            // Handle variable field declaration with var/const/let keywords
            bool is_mutable = false;
            if (this->peek().type == vyn::TokenType::KEYWORD_VAR || this->peek().type == vyn::TokenType::KEYWORD_MUT) { // Added KEYWORD_VAR
                is_mutable = true;
                this->consume(); // consume 'var' or 'mut'
            } else if (this->peek().type == vyn::TokenType::KEYWORD_CONST || this->peek().type == vyn::TokenType::KEYWORD_LET) { // Corrected TokenType, KEYWORD_CONST to CONST, KEYWORD_LET to LET
                is_mutable = false;
                this->consume(); // consume \'const\' or \'let\'
            }
            
            // This is a field declaration
            SourceLocation field_loc = this->current_location();
            
            if (this->peek().type != vyn::TokenType::IDENTIFIER) {
                throw std::runtime_error("Expected field name in class \'" + class_name->name + "\' at " + location_to_string(this->current_location()));
            }
            auto field_name = std::make_unique<ast::Identifier>(field_loc, this->consume().lexeme);
            
            this->expect(vyn::TokenType::COLON);
            
            auto field_type = this->type_parser_.parse();
            if (!field_type) {
                throw std::runtime_error("Expected type for field \'" + field_name->name + "\' in class \'" + class_name->name + "\' at " + location_to_string(this->current_location()));
            }
            
            ast::ExprPtr initializer = nullptr;
            if (this->match(vyn::TokenType::EQ)) {
                initializer = this->expr_parser_.parse_expression();
                if (!initializer) {
                    throw std::runtime_error("Expected initializer for field \\\'" + field_name->name + "\\\' in class \\\'" + class_name->name + "\\\' at " + location_to_string(this->current_location()));
                }
            }
            
            members.push_back(std::make_unique<ast::FieldDeclaration>(field_loc, std::move(field_name), std::move(field_type), std::move(initializer), is_mutable));
            
            // Optional comma or semicolon after field declaration - not required
            this->match(vyn::TokenType::COMMA);
            this->match(vyn::TokenType::SEMICOLON);
        }
        else if (this->peek().type == vyn::TokenType::KEYWORD_FN) {
             members.push_back(this->parse_function());
        }
        else if (this->peek().type == vyn::TokenType::KEYWORD_OPERATOR ) {
             members.push_back(this->parse_function());
        }
        else {
            // If it's not a recognized member, skip the token and report an error or break.
            // For now, let's assume it's an error to find an unrecognized token here.
            throw std::runtime_error("Unexpected token in class body: " + 
                                     vyn::token_type_to_string(this->peek().type) + 
                                     " (\'" + this->peek().lexeme + "\') at " + 
                                     location_to_string(this->current_location()));
        }
    }

    this->expect(vyn::TokenType::RBRACE);
    return std::make_unique<ast::ClassDeclaration>(loc, std::move(class_name), std::move(generic_params), std::move(members));
}


bool DeclarationParser::IsOperator(const vyn::token::Token& token) const {
    return token.type == vyn::TokenType::PLUS ||
           token.type == vyn::TokenType::MINUS ||
           token.type == vyn::TokenType::MULTIPLY ||
           token.type == vyn::TokenType::DIVIDE ||
           token.type == vyn::TokenType::MODULO ||
           token.type == vyn::TokenType::EQEQ ||
           token.type == vyn::TokenType::NOTEQ ||
           token.type == vyn::TokenType::LT ||
           token.type == vyn::TokenType::GT ||
           token.type == vyn::TokenType::LTEQ ||
           token.type == vyn::TokenType::GTEQ ||
           token.type == vyn::TokenType::AND ||
           token.type == vyn::TokenType::OR ||
           token.type == vyn::TokenType::AMPERSAND ||
           token.type == vyn::TokenType::PIPE ||
           token.type == vyn::TokenType::CARET ||
           token.type == vyn::TokenType::LSHIFT ||
           token.type == vyn::TokenType::RSHIFT ||
           token.type == vyn::TokenType::TILDE ||
           token.type == vyn::TokenType::LBRACKET; // For indexing operator []
}

std::unique_ptr<ast::EnumVariant> DeclarationParser::parse_enum_variant() {
    SourceLocation loc = this->current_location();
    if (this->peek().type != vyn::TokenType::IDENTIFIER) {
        throw std::runtime_error("Expected enum variant name (identifier) at " + location_to_string(loc));
    }
    auto name = std::make_unique<ast::Identifier>(this->current_location(), this->consume().lexeme);
    
    std::vector<ast::TypeNodePtr> associated_types;
    if (this->match(vyn::TokenType::LPAREN)) {
        if (this->peek().type != vyn::TokenType::RPAREN) {
            do {
                auto type_node = this->type_parser_.parse();
                if (!type_node) {
                    throw std::runtime_error("Expected type for enum variant parameter at " + location_to_string(this->current_location()));
                }
                associated_types.push_back(std::move(type_node));
            } while (this->match(vyn::TokenType::COMMA));
        }
        this->expect(vyn::TokenType::RPAREN);
    }
    return std::make_unique<ast::EnumVariant>(loc, std::move(name), std::move(associated_types));
}

} // namespace vyn

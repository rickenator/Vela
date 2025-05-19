#ifndef VYN_PARSER_PARSER_HPP
#define VYN_PARSER_PARSER_HPP

// Uncomment this line to enable verbose debugging
// #define VERBOSE

#include "ast.hpp" 
#include "token.hpp" 
#include <vector>
#include <memory>
#include <stdexcept> // For std::runtime_error
#include <optional>
#include <iostream> // For debug output
#include <functional> // Added for std::function
#include <set> // For parser verbose specifiers
#include <catch2/interfaces/catch_interfaces_capture.hpp> // For accessing current test name

namespace vyn { // Changed Vyn to vyn

    // Forward declaration for SourceLocation if not already included via ast.hpp indirectly
    // struct SourceLocation; // Assuming SourceLocation is in the vyn namespace

    // Helper function for converting SourceLocation to string
    // Moved from declaration_parser.cpp to be available more broadly
    inline std::string location_to_string(const vyn::SourceLocation& loc) {
        // Ensure <string> and <iostream> or similar for std::to_string are included
        // by files that include parser.hpp, or include them here.
        // For now, assuming ast.hpp or token.hpp (included above) bring in <string>.
        // If not, #include <string> might be needed here.
        return loc.filePath + ":" + std::to_string(loc.line) + ":" + std::to_string(loc.column);
    }

    // Runtime parser debug control globals (defined in main.cpp)
    extern std::set<std::string> g_verbose_parser_test_specifiers;
    extern bool g_make_all_parser_verbose;
    extern bool g_suppress_all_parser_debug_output;

    // Helper to determine if current test should show parser verbose output
    inline bool should_current_test_be_parser_verbose() {
        if (g_suppress_all_parser_debug_output) return false;
        if (g_make_all_parser_verbose) return true;
        if (g_verbose_parser_test_specifiers.empty()) return false;
        std::string current_test_name = Catch::getResultCapture().getCurrentTestName();
        if (!current_test_name.empty()) {
            if (g_verbose_parser_test_specifiers.count(current_test_name)) return true;
            for (const auto& spec : g_verbose_parser_test_specifiers) {
                if (current_test_name.find(spec) != std::string::npos) return true;
            }
        }
        return false;
    }

    // Debug output utility macros for parser
    #undef DEBUG_PRINT
    #undef DEBUG_TOKEN
    #define DEBUG_PRINT(msg) do { if (should_current_test_be_parser_verbose()) std::cerr << "[PDEBUG] " << __FUNCTION__ << ": " << msg << std::endl; } while(0)
    #define DEBUG_TOKEN(token) do { if (should_current_test_be_parser_verbose()) std::cerr << "[PTOKEN] " << vyn::token_type_to_string(token.type) \
                                                << " (" << token.lexeme << ") at " \
                                                << token.location.filePath << ":" \
                                                << token.location.line << ":" \
                                                << token.location.column << std::endl; } while(0)

    // Forward declarations for parser classes within vyn namespace
    class ExpressionParser;
    class TypeParser;
    class StatementParser;
    class DeclarationParser;
    class ModuleParser;

    class BaseParser {
        friend class Parser;
        friend class ExpressionParser;
    protected:
        const std::vector<vyn::token::Token>& tokens_; 
        size_t& pos_;
        std::vector<int> indent_levels_;
        std::string current_file_path_;

        // Constructor for direct use by parsers that own their token stream (like the main Parser class)
        BaseParser(const std::vector<vyn::token::Token>& tokens, size_t& pos, std::string file_path)
            : tokens_(tokens), pos_(pos), indent_levels_{0}, current_file_path_(std::move(file_path)) {}

        // Copy constructor (or similar) for use by parsers that delegate (like ExpressionParser from its parent)
        BaseParser(const BaseParser& other) = default; // Or implement custom copy logic if needed

        vyn::SourceLocation current_location() const; 
        void skip_comments_and_newlines();
        const vyn::token::Token& peek() const; 
        const vyn::token::Token& peekNext() const; 
        const vyn::token::Token& previous_token() const; 
        void put_back_token(); 
        vyn::token::Token consume(); 
        vyn::token::Token expect(vyn::TokenType type); 
        vyn::token::Token expect(vyn::TokenType type, const std::string& lexeme); // Checks type AND lexeme
        vyn::token::Token expect(vyn::TokenType type, const char* customErrorMessage); // Corrected: checks type, uses custom message on failure
        std::optional<vyn::token::Token> match(vyn::TokenType type); 
        std::optional<vyn::token::Token> match(vyn::TokenType type, const std::string& lexeme); 
        std::optional<vyn::token::Token> match(const std::vector<vyn::TokenType>& types); 
        bool check(vyn::TokenType type) const; 
        bool check(const std::vector<vyn::TokenType>& types) const; 
        bool IsAtEnd() const;

        void skip_indents_dedents();

        // Helper method to report errors
        [[noreturn]] std::runtime_error error(const vyn::token::Token& token, const std::string& message) const; 

        // Helper methods
        bool IsDataType(const vyn::token::Token &token) const; 
        bool IsLiteral(const vyn::token::Token &token) const; 
        bool IsOperator(const vyn::token::Token &token) const; 
        bool IsUnaryOperator(const vyn::token::Token &token) const; 
    public:
        size_t get_current_pos() const;
    };

    class ExpressionParser : public BaseParser {
    public:
        ExpressionParser(const std::vector<token::Token>& tokens, size_t& pos, const std::string& file_path);
        vyn::ast::ExprPtr parse_expression(); // Removed override
        bool is_expression_start(vyn::TokenType type) const; // Added declaration

    private:
        // Add declarations for all private helper methods used in expression_parser.cpp
        vyn::ast::ExprPtr parse_assignment_expr();
        vyn::ast::ExprPtr parse_logical_or_expr();
        vyn::ast::ExprPtr parse_logical_and_expr();
        vyn::ast::ExprPtr parse_bitwise_or_expr();
        vyn::ast::ExprPtr parse_bitwise_xor_expr();
        vyn::ast::ExprPtr parse_bitwise_and_expr();
        vyn::ast::ExprPtr parse_equality_expr();
        vyn::ast::ExprPtr parse_relational_expr();
        vyn::ast::ExprPtr parse_shift_expr();
        vyn::ast::ExprPtr parse_additive_expr();
        vyn::ast::ExprPtr parse_multiplicative_expr();
        vyn::ast::ExprPtr parse_unary_expr();
        vyn::ast::ExprPtr parse_postfix_expr();
        vyn::ast::ExprPtr parse_primary_expr(); // If this is different from parse_atom/parse_primary
        vyn::ast::ExprPtr parse_atom();
        vyn::ast::ExprPtr parse_literal();
        vyn::ast::ExprPtr parse_primary(); // General primary expression parsing
        vyn::ast::ExprPtr parse_call_expression(vyn::ast::ExprPtr callee_expr);
        vyn::ast::ExprPtr parse_member_access(vyn::ast::ExprPtr object);

        // Helper for binary expressions
        vyn::ast::ExprPtr parse_binary_expression(std::function<vyn::ast::ExprPtr()> parse_higher_precedence, const std::vector<TokenType>& operators);

        // Helper to check if a token type is a literal
        bool is_literal(TokenType type) const; // Added const
    };

    class TypeParser : public BaseParser {
    private:
        ExpressionParser& expr_parser_;

    public:
        TypeParser(const std::vector<vyn::token::Token>& tokens, size_t& pos, const std::string& file_path, ExpressionParser& expr_parser);
        vyn::ast::TypeNodePtr parse(); 

    private:
        vyn::ast::TypeNodePtr parse_base_or_ownership_wrapped_type(); 
        vyn::ast::TypeNodePtr parse_atomic_or_group_type();
        vyn::ast::TypeNodePtr parse_postfix_type(vyn::ast::TypeNodePtr base_type); 
    };

    class StatementParser : public BaseParser {
        int indent_level_;
        TypeParser& type_parser_;
        ExpressionParser& expr_parser_;
        DeclarationParser* decl_parser_; // Keep as is, will be set by constructor or setter
    public:
        StatementParser(const std::vector<token::Token>& tokens, size_t& pos, int indent_level, const std::string& file_path, TypeParser& type_parser, ExpressionParser& expr_parser, DeclarationParser* decl_parser = nullptr);
        void set_declaration_parser(DeclarationParser* dp); // Setter method
        vyn::ast::StmtPtr parse(); 
        std::unique_ptr<vyn::ast::ExpressionStatement> parse_expression_statement(); 
        std::unique_ptr<vyn::ast::BlockStatement> parse_block(); 
        vyn::ast::ExprPtr parse_pattern(); 
        vyn::ast::StmtPtr parse_try();
        vyn::ast::StmtPtr parse_defer();
        vyn::ast::StmtPtr parse_await();
        std::unique_ptr<vyn::ast::UnsafeStatement> parse_unsafe(); // Added declaration
    private:
        bool is_statement_start(vyn::TokenType type) const; // Added declaration
        std::unique_ptr<vyn::ast::IfStatement> parse_if(); 
        std::unique_ptr<vyn::ast::WhileStatement> parse_while(); 
        std::unique_ptr<vyn::ast::ForStatement> parse_for(); 
        std::unique_ptr<vyn::ast::ReturnStatement> parse_return(); 
        std::unique_ptr<vyn::ast::BreakStatement> parse_break(); 
        std::unique_ptr<vyn::ast::ContinueStatement> parse_continue(); 
        std::unique_ptr<vyn::ast::VariableDeclaration> parse_var_decl(); 
        std::unique_ptr<vyn::ast::Node> parse_struct_pattern(); 
        std::unique_ptr<vyn::ast::Node> parse_tuple_pattern(); 
    public:
        using BaseParser::IsAtEnd;
        using BaseParser::peek;
        using BaseParser::consume;
        using BaseParser::skip_indents_dedents;
    };

    class DeclarationParser : public BaseParser {
        TypeParser& type_parser_;
        ExpressionParser& expr_parser_;
        StatementParser& stmt_parser_;
    public:
        TypeParser& get_type_parser() { return type_parser_; }
        ExpressionParser& get_expr_parser() { return expr_parser_; }
    public:
        DeclarationParser(const std::vector<vyn::token::Token>& tokens, size_t& pos, const std::string& file_path, TypeParser& type_parser, ExpressionParser& expr_parser, StatementParser& stmt_parser);
        vyn::ast::DeclPtr parse();
        std::unique_ptr<vyn::ast::FunctionDeclaration> parse_function();
        std::unique_ptr<vyn::ast::Declaration> parse_struct();
        std::unique_ptr<vyn::ast::Declaration> parse_impl();
        std::unique_ptr<vyn::ast::Declaration> parse_class_declaration();
        std::unique_ptr<vyn::ast::Declaration> parse_field_declaration();
        std::unique_ptr<vyn::ast::Declaration> parse_enum_declaration();
        std::unique_ptr<vyn::ast::TypeAliasDeclaration> parse_type_alias_declaration();
        std::unique_ptr<vyn::ast::VariableDeclaration> parse_global_var_declaration();
        std::vector<std::unique_ptr<vyn::ast::GenericParameter>> parse_generic_params();
        std::unique_ptr<vyn::ast::Node> parse_param();
        std::unique_ptr<vyn::ast::Declaration> parse_template_declaration();
        std::unique_ptr<vyn::ast::ImportDeclaration> parse_import_declaration();
        std::unique_ptr<vyn::ast::ImportDeclaration> parse_smuggle_declaration();
        
    private:
        bool IsOperator(const vyn::token::Token& token) const;

    private:
        std::unique_ptr<vyn::ast::EnumVariant> parse_enum_variant(); 
        vyn::ast::FunctionParameter parse_function_parameter_struct(); 
    };

    class ModuleParser : public BaseParser {
        DeclarationParser& declaration_parser_;
    public:
        ModuleParser(const std::vector<vyn::token::Token>& tokens, size_t& pos, const std::string& file_path, DeclarationParser& declaration_parser); 
        std::unique_ptr<vyn::ast::Module> parse(); 
    };

    class Parser {
        private:
        // Order of declaration matters for initialization order if not explicitly managed
        // It's generally good practice to declare them in the order they'll be initialized.
        std::vector<vyn::token::Token> tokens_; // Store tokens if Parser owns them
        size_t current_pos_;                  // Store current_pos_ if Parser manages it directly
        std::string file_path_;               // Store file_path_ if Parser manages it directly

        BaseParser base_parser_; // This is the primary BaseParser instance

        ExpressionParser expression_parser_;
        TypeParser type_parser_;
        StatementParser statement_parser_;
        DeclarationParser declaration_parser_;
        ModuleParser module_parser_;

    public:
        Parser(const std::vector<vyn::token::Token>& tokens, std::string file_path);
        std::unique_ptr<vyn::ast::Module> parse_module(); 

        ExpressionParser& get_expression_parser() { return expression_parser_; }
        TypeParser& get_type_parser() { return type_parser_; } 
        StatementParser& get_statement_parser() { return statement_parser_; } 
        DeclarationParser& get_declaration_parser() { return declaration_parser_; } 
        ModuleParser& get_module_parser() { return module_parser_; } 
    };

} // namespace vyn

#endif // VYN_PARSER_PARSER_HPP
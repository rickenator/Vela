#include "vyn/parser/ast.hpp"
#include "vyn/parser/token.hpp"
#include "vyn/parser/parser.hpp" // Added missing include for parser definitions
#include <vector>
#include <memory>
#include <stdexcept> // Required for std::runtime_error

namespace vyn { // Changed Vyn to vyn

// Constructor for ModuleParser
ModuleParser::ModuleParser(const std::vector<vyn::token::Token>& tokens, size_t& pos, const std::string& file_path, DeclarationParser& declaration_parser) // Changed Vyn::Token to vyn::token::Token
    : BaseParser(tokens, pos, file_path), declaration_parser_(declaration_parser) {}

std::unique_ptr<vyn::ast::Module> ModuleParser::parse() {
    vyn::SourceLocation module_loc = this->current_location();
    std::vector<vyn::ast::StmtPtr> module_body; // Changed to ast::StmtPtr

    this->skip_comments_and_newlines();

    while (this->peek().type != vyn::TokenType::END_OF_FILE) { // Corrected namespace to vyn::TokenType
        vyn::token::Token current_token_before_parse = this->peek(); // Store token before parsing
        size_t pos_before_parse = this->pos_; // Store position before parsing

        // Try to parse a declaration first
        auto decl_node = this->declaration_parser_.parse();
        if (decl_node) {
            module_body.push_back(std::move(decl_node));
            // Debug: print token before consuming semicolons
            std::cerr << "[ModuleParser] Before consuming semicolons, next token: "
                      << vyn::token_type_to_string(this->peek().type) << " (" << this->peek().lexeme << ")\\\\n";
            // Consume all consecutive semicolons after top-level declaration (e.g., class, struct, etc.)
            while (this->peek().type == vyn::TokenType::SEMICOLON) {
                std::cerr << "[ModuleParser] Consuming semicolon at: " << this->peek().location.toString() << "\\\\n";
                this->consume();
            }
            std::cerr << "[ModuleParser] After consuming semicolons, next token: "
                      << vyn::token_type_to_string(this->peek().type) << " (" << this->peek().lexeme << ")\\\\n";
        } else {
            // If parsing a declaration failed and no tokens were consumed,
            // it means we encountered something unexpected.
            // To prevent an infinite loop, consume the token and report an error, or break.
            if (this->pos_ == pos_before_parse && this->peek().type == current_token_before_parse.type && this->peek().lexeme == current_token_before_parse.lexeme) {
                 std::cerr << "[ModuleParser] Error: No progress made. Unexpected token: "
                           << vyn::token_type_to_string(this->peek().type)
                           << " (\\\"" << this->peek().lexeme << "\\\") at "
                           << this->peek().location.toString() << ". Stopping parse.\\\\n";
                // Optionally, throw an exception here or add to an error list
                // For now, let's break to prevent infinite loop.
                // You might want to consume the token: this->consume();
                break; 
            }
        }
    }

    return std::make_unique<vyn::ast::Module>(module_loc, std::move(module_body));
}

} // namespace vyn

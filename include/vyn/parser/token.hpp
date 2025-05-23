#ifndef VYN_PARSER_TOKEN_HPP
#define VYN_PARSER_TOKEN_HPP

#include <string>
#include <utility> // For std::move
#include "source_location.hpp" // Corrected path

namespace vyn {

enum class TokenType {
    // Literals
    IDENTIFIER,
    INT_LITERAL,
    FLOAT_LITERAL,
    STRING_LITERAL,
    CHAR_LITERAL,

    // Keywords
    KEYWORD_LET, KEYWORD_VAR, KEYWORD_CONST, KEYWORD_IF, KEYWORD_ELSE,
    KEYWORD_WHILE, KEYWORD_FOR, KEYWORD_RETURN, KEYWORD_BREAK, KEYWORD_CONTINUE,
    KEYWORD_NULL, KEYWORD_TRUE, KEYWORD_FALSE, KEYWORD_FN, KEYWORD_STRUCT,
    KEYWORD_ENUM, KEYWORD_TRAIT, KEYWORD_IMPL, KEYWORD_TYPE, KEYWORD_MODULE,
    KEYWORD_USE, KEYWORD_PUB, KEYWORD_MUT, KEYWORD_TRY, KEYWORD_CATCH,
    KEYWORD_FINALLY, KEYWORD_DEFER, KEYWORD_MATCH, KEYWORD_SCOPED, KEYWORD_REF,
    KEYWORD_EXTERN, KEYWORD_AS, KEYWORD_IN, KEYWORD_CLASS, KEYWORD_TEMPLATE,
    KEYWORD_IMPORT, KEYWORD_SMUGGLE, KEYWORD_AWAIT, KEYWORD_ASYNC, KEYWORD_OPERATOR,
    KEYWORD_MY, KEYWORD_OUR, KEYWORD_THEIR, KEYWORD_PTR, // New ownership keywords
    KEYWORD_BORROW, KEYWORD_VIEW, // Changed from BORROW_MUT
    KEYWORD_NIL, // Added for nil literal
    KEYWORD_UNSAFE, // Added for unsafe blocks

    // Operators
    PLUS, MINUS, MULTIPLY, DIVIDE, MODULO, EQ, EQEQ, NOTEQ, LT, GT, LTEQ, GTEQ,
    AND, OR, BANG, AMPERSAND, PIPE, CARET, TILDE, LSHIFT, RSHIFT, DOTDOT,
    PLUSEQ, MINUSEQ, MULTIPLYEQ, DIVEQ, MODEQ, LSHIFTEQ, RSHIFTEQ,
    BITWISEANDEQ, BITWISEOREQ, BITWISEXOREQ, COLONEQ, // Compound assignment and colon-equals

    // Punctuation
    LPAREN, RPAREN, LBRACE, RBRACE, LBRACKET, RBRACKET, COMMA, DOT, COLON,
    SEMICOLON, ARROW, FAT_ARROW, COLONCOLON, AT, UNDERSCORE, QUESTION_MARK, // Added for optional type T?

    // Misc
    UNKNOWN,
    END_OF_FILE,
    COMMENT,
    NEWLINE,
    INDENT,
    DEDENT,
    ILLEGAL // Added for illegal/error tokens
};

std::string token_type_to_string(TokenType type);

namespace token {
    class Token {
    public:
        vyn::TokenType type;
        std::string lexeme;
        vyn::SourceLocation location;

        Token(vyn::TokenType type, const std::string& lexeme, const vyn::SourceLocation& loc);
        std::string toString() const;
    };
} // namespace token
} // namespace vyn

#endif // VYN_PARSER_TOKEN_HPP
#ifndef VYN_HPP
#define VYN_HPP

#include "vyn/parser/lexer.hpp" 
#include "vyn/parser/parser.hpp" // Corrected path
#include "vyn/parser/ast.hpp"
#include "vyn/parser/token.hpp"  // Corrected path
#include "vyn/semantic.hpp" // Added to make SemanticAnalyzer available
#include "vyn/vre/llvm/codegen.hpp" // Added for LLVMCodegen
#include "vyn/driver.hpp" // Added for vyn::Driver

/* // EBNF Grammar of the Vyn Language  // Uncommented
//
// Conventions:
//   IDENTIFIER:        Represents a valid identifier token.
//   INTEGER_LITERAL:   Represents an integer literal token.
//   FLOAT_LITERAL:     Represents a float literal token.
//   STRING_LITERAL:    Represents a string literal token.
//   BOOLEAN_LITERAL:   Represents 'true' or 'false'.
//   'keyword':         Denotes a literal keyword.
//   { ... }:           Represents zero or more occurrences (Kleene star).
//   [ ... ]:           Represents zero or one occurrence (optional).
//   ( ... | ... ):     Represents a choice (alternation).
//   ... ::= ... :      Defines a production rule.
//
// Grammar:
module                 ::= { module_item } EOF

module_item            ::= import_statement
                         | smuggle_statement
                         | class_declaration
                         | struct_declaration
                         | enum_declaration
                         | impl_declaration
                         | function_declaration
                         | variable_declaration
                         | constant_declaration
                         | type_alias_declaration
                         | trait_declaration // Added for template Comparable like constructs
                         | statement // Allow top-level statements like expressions for scripting or simple files

// Top-level declarations
import_statement       ::= 'import' path [ 'as' IDENTIFIER ] [';'] // Semicolon made optional
smuggle_statement      ::= 'smuggle' path [ 'as' IDENTIFIER ] [';'] // Semicolon made optional
path                   ::= IDENTIFIER { ('::' | '.') IDENTIFIER } // Allow . as path separator too

class_declaration      ::= [ 'pub' ] [ 'template' '<' type_parameter_list '>' ] 'class' IDENTIFIER [ 'extends' type ] [ 'implements' type_list ] '{' { class_member } '}'
class_member           ::= field_declaration | method_declaration | constructor_declaration
field_declaration      ::= [ 'pub' ] ( 'var' | 'const' ) IDENTIFIER ':' type [ '=' expression ] [';'] // Changed 'let' to 'var'
method_declaration     ::= [ 'pub' ] [ 'static' ] [ 'template' '<' type_parameter_list '>' ] [ 'async' ] 'fn' IDENTIFIER '(' [ parameter_list ] ')' [ '->' type ] [ 'throws' type_list ] ( block_statement | '=>' expression [';'] ) // Added throws, expression body
constructor_declaration::= [ 'pub' ] 'new' [ template_parameters ] '(' [ parameter_list ] ')' [ 'throws' type_list ] ( block_statement | '=>' expression [';'] ) // Added throws, expression body

struct_declaration     ::= [ 'pub' ] [ 'template' '<' type_parameter_list '>' ] 'struct' IDENTIFIER '{' { struct_field_declaration } '}'
struct_field_declaration ::= [ 'pub' ] IDENTIFIER ':' type [ '=' expression ] [';'] // Semicolon made optional

enum_declaration       ::= [ 'pub' ] [ 'template' '<' type_parameter_list '>' ] 'enum' IDENTIFIER '{' { enum_variant } '}'
enum_variant           ::= IDENTIFIER [ '(' type_list ')' ] [ '=' expression ] ','?

impl_declaration       ::= [ 'template' '<' type_parameter_list '>' ] 'impl' type [ 'for' type ] '{' { method_declaration } '}'

function_declaration   ::= [ 'pub' ] [ 'template' '<' type_parameter_list '>' ] [ 'async' ] 'fn' IDENTIFIER '(' [ parameter_list ] ')' [ '->' type ] [ 'throws' type_list ] ( block_statement | '=>' expression [';'] | statement ) // Added throws, expression body, or single statement body

trait_declaration      ::= [ 'pub' ] 'template' IDENTIFIER [ template_parameters ] '{' { method_signature } '}' // For template Comparable
method_signature       ::= [ 'async' ] 'fn' IDENTIFIER '(' [ parameter_list ] ')' [ '->' type ] [ 'throws' type_list ] ';'


variable_declaration   ::= [ 'pub' ] 'var' IDENTIFIER [ ':' type ] [ '=' expression ] [';'] // Changed 'let' to 'var', semicolon optional
constant_declaration   ::= [ 'pub' ] 'const' IDENTIFIER ':' type '=' expression [';'] // Semicolon optional

type_alias_declaration ::= [ 'pub' ] 'type' IDENTIFIER [ template_parameters ] '=' type [';'] // Semicolon optional


// Parameters and Type Lists
type_parameter_list    ::= type_parameter { ',' type_parameter }
type_parameter         ::= IDENTIFIER [ ':' type_bounds ] | expression // Allow expressions for const generics
type_bounds            ::= type { '+' type }
template_parameters    ::= '<' type_parameter_list '>'

parameter_list         ::= parameter { ',' parameter }
parameter              ::= IDENTIFIER ':' type [ '=' expression ] // Mutability via type e.g. &T, &const T

type_list              ::= type { ',' type }

// Statements
statement              ::= expression_statement
                         | block_statement
                         | if_statement
                         | for_statement
                         | while_statement
                         | loop_statement
                         | match_statement
                         | return_statement
                         | break_statement
                         | continue_statement
                         | defer_statement
                         | try_statement
                         | variable_declaration
                         | constant_declaration
                         | pattern_assignment_statement // For ref x = _
                         | scoped_statement // For scoped { ... }
                         | throw_statement // Added

expression_statement   ::= expression [';'] // Semicolon made optional
block_statement        ::= '{' { statement } '}'

if_statement           ::= 'if' expression ( block_statement | statement_without_block ) { 'else' 'if' expression ( block_statement | statement_without_block ) } [ 'else' ( block_statement | statement_without_block ) ]
statement_without_block ::= expression_statement | return_statement | break_statement | continue_statement | throw_statement // etc., any statement not ending in a block itself.

for_statement          ::= 'for' pattern 'in' expression block_statement
while_statement        ::= 'while' expression block_statement // Consider also: while_let_statement
loop_statement         ::= 'loop' block_statement
match_statement        ::= 'match' expression [ '{' ] { match_arm } [ '}' ] // Braces for arms made optional
match_arm              ::= pattern [ 'if' expression ] '=>' ( expression | block_statement | statement_without_block ) ','?
pattern                ::= IDENTIFIER [ '@' pattern ]
                         | literal
                         | '_'
                         | path '{' [ field_pattern { ',' field_pattern } [','] ] '}' // Struct pattern
                         | path '(' [ pattern_list ] ')' // Enum variant pattern
                         | '[' [ pattern_list ] ']' // Array/slice pattern
                         | '(' pattern_list ')' // Tuple pattern
                         | '&' [ 'const' ] pattern // For reference patterns
field_pattern          ::= IDENTIFIER ':' pattern | IDENTIFIER
pattern_list           ::= pattern { ',' pattern }
pattern_assignment_statement ::= pattern '=' expression [';'] // For ref x = _

return_statement       ::= 'return' [ expression ] [';'] // Semicolon made optional
break_statement        ::= 'break' [ IDENTIFIER ] [ expression ] [';'] // Semicolon optional
continue_statement     ::= 'continue' [ IDENTIFIER ] [';'] // Semicolon optional
defer_statement        ::= 'defer' ( expression_statement | block_statement ) // Semicolon handled by expr_stmt
try_statement          ::= 'try' block_statement { catch_clause } [ 'finally' block_statement ]
catch_clause           ::= 'catch' [ '(' IDENTIFIER ':' type ')' | IDENTIFIER ] block_statement // Adjusted for (e: Type)
scoped_statement       ::= 'scoped' block_statement // Added
throw_statement        ::= 'throw' expression [';'] // Added

// Expressions
expression             ::= assignment_expression
                           | BorrowExpr // Added BorrowExpr to the expression hierarchy

assignment_expression  ::= conditional_expression [ assignment_operator assignment_expression ]
assignment_operator    ::= '=' | '+=' | '-=' | '*=' | '/=' | '%=' | '&=' | '|=' | '^=' | '<<=' | '>>='

conditional_expression ::= logical_or_expression [ '?' expression ':' conditional_expression ] // Ternary operator
                         | if_expression // Added if as expression

logical_or_expression  ::= logical_and_expression { '||' logical_and_expression }
logical_and_expression ::= bitwise_or_expression { '&&' bitwise_or_expression }
bitwise_or_expression  ::= bitwise_xor_expression { '|' bitwise_xor_expression }
bitwise_xor_expression ::= bitwise_and_expression { '^' bitwise_and_expression }
bitwise_and_expression ::= equality_expression { '&' equality_expression }
equality_expression    ::= relational_expression { ( '==' | '!=' ) relational_expression }
relational_expression  ::= range_expression { ( '<' | '<=' | '>' | '>=' | 'is' | 'as' ) range_expression } // Changed to range_expression
range_expression       ::= shift_expression [ '..' shift_expression ] // Added range expression x..y
shift_expression       ::= additive_expression { ( '<<' | '>>' ) additive_expression }
additive_expression    ::= multiplicative_expression { ( '+' | '-' ) multiplicative_expression }
multiplicative_expression ::= unary_expression { ( '*' | '/' | '%' ) unary_expression }

unary_expression       ::= ( '!' | '-' | '+' | '*' | 'await' | 'throw' ) unary_expression // Removed '&' and '&const' as they are part of Type or handled by BorrowExpr
                         | primary_expression

primary_expression     ::= literal
                         | path_expression
                         | '(' expression ')'
                         | call_expression          // If callee is a type (path_expression), yields ConstructionExpression
                         | member_access_expression
                         | index_access_expression
                         | list_comprehension
                         | array_literal            // For [elem1, elem2] and [elem; count]
                         | array_construction       // For [Type; Size]() for default initialization
                         | tuple_literal
                         | struct_literal           // For TypeName{...} and anonymous {...}
                         | lambda_expression
                         | 'self' | 'super'
                         // if_expression is now part of conditional_expression

if_expression          ::= 'if' expression block_statement 'else' ( block_statement | if_expression ) // For if used as expression

literal                ::= INTEGER_LITERAL | FLOAT_LITERAL | STRING_LITERAL | BOOLEAN_LITERAL | 'null'

path_expression        ::= path [ type_arguments ]

call_expression        ::= primary_expression '(' [ argument_list ] ')' [ '?' ] // Consider array_type() here
argument_list          ::= expression { ',' expression }

member_access_expression ::= primary_expression ( '.' | '?.' | '::' ) IDENTIFIER // Added :: for static-like access on instances if needed, or path handles it
index_access_expression  ::= primary_expression '[' expression ']' [ '?' ]

list_comprehension     ::= '[' expression 'for' pattern 'in' expression [ 'if' expression ] ']'
array_literal          ::= '[' [ expression { ',' expression } [','] ] ']'  // e.g., [elem1, elem2, ...]
                         | '[' expression ';' expression ']'                // e.g., [value; count]
array_construction     ::= ArrayType '(' ')' // Default initialization, e.g., [Int; 10]()

tuple_literal          ::= '(' [ expression { ',' expression } [ ',' ] ] ')'

struct_literal         ::= [ path_expression ] '{' [ struct_literal_field { ',' struct_literal_field } [ ',' ] ] '}' // Allows TypeName{...} and anonymous {...}
struct_literal_field   ::= IDENTIFIER (':' | '=') expression | IDENTIFIER // Allow = for field init

lambda_expression      ::= [ 'async' ] ( '|' [ parameter_list ] '|' | IDENTIFIER ) [ '->' type ] ( '=>' expression | block_statement )

// Type System (NEW - based on mem_RFC.md)
Type                   ::= BaseType [ 'const' ] [ '?' ] // Added optional '?' for nullable types from old EBNF
BaseType               ::= IDENTIFIER // Represents a simple type name like Int, Foo
                         | OwnershipWrapper '<' Type '>'
                         | ArrayType
                         | TupleType
                         | FunctionType
                         | '(' Type ')' // Grouped type

OwnershipWrapper       ::= 'my' | 'our' | 'their' | 'ptr'

ArrayType              ::= '[' Type [ ';' Expression ] ']' // e.g., [Int], [Int; 5]
TupleType              ::= '(' [ Type { ',' Type } [ ',' ] ] ')' // e.g., (Int, String)
FunctionType           ::= [ 'async' ] 'fn' '(' [ Type { ',' Type } ] ')' [ '->' Type ] [ 'throws' TypeList ]

// Path and Arguments (mostly from old EBNF, ensure compatibility)
path                   ::= IDENTIFIER { ('::' | '.') IDENTIFIER }
type_arguments         ::= '<' type_argument_list '>'
type_argument_list     ::= type_argument { ',' type_argument }
type_argument          ::= Type | Expression // For const generics

// Borrow Expression (NEW - based on mem_RFC.md)
BorrowExpr             ::= 'borrow' '(' Expression ')'
                         | 'borrow_mut' '(' Expression ')'


// Note: The following old type productions are replaced or integrated above:
// core_type              ::= named_type ...
// named_type             ::= path [ type_arguments ] (now IDENTIFIER or path within BaseType, arguments via generic syntax if needed)
// array_type             ::= '[' type ( ';' expression )? ']' (now ArrayType)
// slice_type             ::= '[' type ']' (covered by ArrayType with no size expression)
// tuple_type             ::= '(' [ type_list [ ',' ] ] ')' (now TupleType)
// function_type          ::= [ 'async' ] 'fn' '(' [ type_list ] ')' [ '->' type ] [ 'throws' type_list ] (now FunctionType)
// reference_type         ::= '&' [ lifetime ] [ 'const' ] type (Replaced by 'their<Type>' and 'their<Type const>')
// pointer_type           ::= '*' ( 'const' | 'mut' ) type (Replaced by 'ptr<Type>' and 'ptr<Type const>')
// lifetime               ::= APOSTROPHE IDENTIFIER (Lifetimes are not in the current mem_RFC.md focus, can be added later if needed)

// End of EBNF Grammar
//
// General Notes:
// - Semicolon optionality: Many semicolons have been made optional ([';']) to align with examples.
//   The parser's actual strictness should be confirmed. If truly optional, it can lead to ambiguities.
// - Expression bodies for functions: `=> expression` added.
// - `let` vs `var`: `let` and `let mut` added to `variable_declaration`.
// - `if-expression`: Added to `conditional_expression`.
// - `statement_without_block`: Helper for `if` statements with non-block consequents.
// - Constructor calls: A `call_expression` of the form `TypeName ( arguments )`, where `TypeName` is a `path_expression` resolving to a type, is parsed as a `ConstructionExpression`.
//
// Note on 'box' keyword and 'while let':
// The 'box' keyword for heap allocation (e.g., 'box Type { ... }') and the
// 'while let' construct (e.g., 'while let Some(x) = option_val { ... }')
// are present in some example files but are NOT currently supported by the parser
// and are therefore not included in this EBNF. They represent planned or
// experimental features.
//
// Note on struct literal with type arguments:
// The parser expects struct literals like 'StructName { field: value }'.
// Explicit type arguments on the literal itself (e.g. 'StructName<T> { ... }')
// are not supported by the parser for struct literals; type arguments are resolved
// from context or the variable's type annotation.
*/

#endif // VYN_HPP
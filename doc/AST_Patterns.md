<!-- filepath: /home/rick/Projects/Vyn/doc/AST_Patterns.md -->
# Vyn AST: Pattern Nodes

This document details AST nodes used for pattern matching in Vyn, as defined in `include/vyn/parser/ast.hpp`. Pattern matching allows for destructuring and control flow based on the shape of data.

All specific pattern nodes inherit from a common base class, `PatternNode`, which itself inherits from `vyn::ast::Node`.

## Common Pointer Aliases

- `PatternPtr = std::unique_ptr<PatternNode>;`
- `IdentifierPtr = std::unique_ptr<Identifier>;`
- `ExprPtr = std::unique_ptr<Expression>;` // For LiteralPattern, RangePattern, etc.
- `TypeNodePtr = std::unique_ptr<TypeNode>;` // For StructPattern, EnumVariantPattern type paths

## 1. Base `PatternNode` (`vyn::ast::PatternNode`)

This is the abstract base class for all pattern nodes.

-   **C++ Class**: `vyn::ast::PatternNode`
-   **Inheritance**: `vyn::ast::Node` -> `vyn::ast::PatternNode`

```cpp
// Base class from include/vyn/parser/ast.hpp
namespace vyn::ast {

class PatternNode : public Node {
public:
    PatternNode(SourceLocation loc);
    // ... accept, getType, toString methods ...
    // virtual bool isIrrefutable() const = 0; // Example of a common method
};

} // namespace vyn::ast
```

## 2. Concrete Pattern Node Types

These classes inherit from `PatternNode` and represent concrete patterns.

### 2.1. `IdentifierPattern`

Matches a value and binds it to an identifier. Can also be used for struct field shorthand `Struct { field }` which binds `field`.

-   **C++ Class**: `vyn::ast::IdentifierPattern`
-   **`NodeType`**: `IDENTIFIER_PATTERN`
-   **Fields**:
    -   `name` (`IdentifierPtr`): The identifier to bind to.
    -   `isMutable` (`bool`): Whether the binding is mutable.
    -   `subPattern` (`std::optional<PatternPtr>`): Optional sub-pattern for destructuring (e.g., `identifier @ sub_pattern` or for enum variants like `Some(x)` where `x` is the identifier pattern).

```cpp
// From include/vyn/parser/ast.hpp
namespace vyn::ast {

class IdentifierPattern : public PatternNode {
public:
    IdentifierPtr name;
    bool isMutable;
    std::optional<PatternPtr> subPattern; // For 'name @ pattern' or struct shorthand field pattern

    IdentifierPattern(SourceLocation loc, IdentifierPtr name, bool isMutable, std::optional<PatternPtr> subPattern = std::nullopt);
    // ... accept, getType, toString methods ...
};

} // namespace vyn::ast
```

### 2.2. `LiteralPattern`

Matches a specific literal value.

-   **C++ Class**: `vyn::ast::LiteralPattern`
-   **`NodeType`**: `LITERAL_PATTERN`
-   **Fields**:
    -   `literal` (`ExprPtr`): The literal expression (e.g., `IntegerLiteral`, `StringLiteral`) to match against.

```cpp
// From include/vyn/parser/ast.hpp
namespace vyn::ast {

class LiteralPattern : public PatternNode {
public:
    ExprPtr literal; // Should be a Literal node (IntegerLiteral, StringLiteral etc.)

    LiteralPattern(SourceLocation loc, ExprPtr literal);
    // ... accept, getType, toString methods ...
};

} // namespace vyn::ast
```

### 2.3. `TuplePattern`

Destructures a tuple into its constituent parts.

-   **C++ Class**: `vyn::ast::TuplePattern`
-   **`NodeType`**: `TUPLE_PATTERN`
-   **Fields**:
    -   `elements` (`std::vector<PatternPtr>`): A vector of pattern nodes, one for each element of the tuple.

```cpp
// From include/vyn/parser/ast.hpp
namespace vyn::ast {

class TuplePattern : public PatternNode {
public:
    std::vector<PatternPtr> elements;

    TuplePattern(SourceLocation loc, std::vector<PatternPtr> elements);
    // ... accept, getType, toString methods ...
};

} // namespace vyn::ast
```

### 2.4. `StructPattern`

Destructures a struct by its field names.

-   **C++ Class**: `vyn::ast::StructPattern`
-   **`NodeType`**: `STRUCT_PATTERN`
-   **Fields**:
    -   `structType` (`TypeNodePtr`): The type of the struct being matched (e.g., `BasicTypeNode` for `MyStruct`).
    -   `fields` (`std::vector<std::pair<IdentifierPtr, PatternPtr>>`): A list of field names and the patterns to match their values against.
    -   `hasRest` (`bool`): Indicates if `..` is present, meaning not all fields need to be specified.

```cpp
// From include/vyn/parser/ast.hpp
namespace vyn::ast {

class StructPattern : public PatternNode {
public:
    TypeNodePtr structType; // e.g., BasicTypeNode for "MyStruct"
    std::vector<std::pair<IdentifierPtr, PatternPtr>> fields;
    bool hasRest; // True if '..' is used

    StructPattern(SourceLocation loc, TypeNodePtr structType,
                  std::vector<std::pair<IdentifierPtr, PatternPtr>> fields, bool hasRest);
    // ... accept, getType, toString methods ...
};

} // namespace vyn::ast
```

### 2.5. `EnumVariantPattern`

Matches and destructures a specific enum variant.

-   **C++ Class**: `vyn::ast::EnumVariantPattern`
-   **`NodeType`**: `ENUM_VARIANT_PATTERN`
-   **Fields**:
    -   `enumType` (`TypeNodePtr`): The type of the enum (e.g., `BasicTypeNode` for `Option`).
    -   `variantName` (`IdentifierPtr`): The name of the variant (e.g., `Some`).
    -   `arguments` (`std::vector<PatternPtr>`): Patterns for the arguments of the variant, if any.

```cpp
// From include/vyn/parser/ast.hpp
namespace vyn::ast {

class EnumVariantPattern : public PatternNode {
public:
    TypeNodePtr enumType; // e.g., BasicTypeNode for "Option"
    IdentifierPtr variantName; // e.g., Identifier for "Some"
    std::vector<PatternPtr> arguments;

    EnumVariantPattern(SourceLocation loc, TypeNodePtr enumType, IdentifierPtr variantName, std::vector<PatternPtr> arguments);
    // ... accept, getType, toString methods ...
};

} // namespace vyn::ast
```

### 2.6. `WildcardPattern`

Matches any value without binding it to a name (`_`).

-   **C++ Class**: `vyn::ast::WildcardPattern`
-   **`NodeType`**: `WILDCARD_PATTERN`

```cpp
// From include/vyn/parser/ast.hpp
namespace vyn::ast {

class WildcardPattern : public PatternNode {
public:
    WildcardPattern(SourceLocation loc);
    // ... accept, getType, toString methods ...
};

} // namespace vyn::ast
```

### 2.7. `RangePattern`

Matches a value if it falls within a specified range (e.g., `1..=5`, `'a'..'z'`).

-   **C++ Class**: `vyn::ast::RangePattern`
-   **`NodeType`**: `RANGE_PATTERN`
-   **Fields**:
    -   `start` (`ExprPtr`): The start of the range (must be a literal or const expression).
    -   `end` (`ExprPtr`): The end of the range (must be a literal or const expression).
    -   `isInclusive` (`bool`): True if the range end is inclusive (`..=`), false if exclusive (`..`).

```cpp
// From include/vyn/parser/ast.hpp
namespace vyn::ast {

class RangePattern : public PatternNode {
public:
    ExprPtr start; // Should be a compile-time constant expression (e.g. Literal)
    ExprPtr end;   // Should be a compile-time constant expression (e.g. Literal)
    bool isInclusive; // Differentiates '..' from '..='

    RangePattern(SourceLocation loc, ExprPtr start, ExprPtr end, bool isInclusive);
    // ... accept, getType, toString methods ...
};

} // namespace vyn::ast
```

### 2.8. `OrPattern`

Allows matching against multiple alternative patterns (e.g., `Some(0) | None` or `1 | 2 | 3`).

-   **C++ Class**: `vyn::ast::OrPattern`
-   **`NodeType`**: `OR_PATTERN`
-   **Fields**:
    -   `alternatives` (`std::vector<PatternPtr>`): A list of patterns, any of which can match.

```cpp
// From include/vyn/parser/ast.hpp
namespace vyn::ast {

class OrPattern : public PatternNode {
public:
    std::vector<PatternPtr> alternatives;

    OrPattern(SourceLocation loc, std::vector<PatternPtr> alternatives);
    // ... accept, getType, toString methods ...
};

} // namespace vyn::ast
```

## Visitor Integration

Each concrete pattern node implements `accept(Visitor& visitor)` which calls the corresponding `visit` method on the visitor.

```cpp
// In Visitor interface (include/vyn/parser/ast.hpp)
class Visitor {
public:
    // ... other visit methods ...

    virtual void visit(IdentifierPattern* node) = 0;
    virtual void visit(LiteralPattern* node) = 0;
    virtual void visit(TuplePattern* node) = 0;
    virtual void visit(StructPattern* node) = 0;
    virtual void visit(EnumVariantPattern* node) = 0;
    virtual void visit(WildcardPattern* node) = 0;
    virtual void visit(RangePattern* node) = 0;
    virtual void visit(OrPattern* node) = 0;
};
```

This structure, as defined in `include/vyn/parser/ast.hpp`, provides a comprehensive way to represent patterns in the AST and integrate them with the existing Visitor design pattern.

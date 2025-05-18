# AST Nodes: Literals

This document details the Abstract Syntax Tree (AST) nodes representing various literal values in the Vyn programming language. These nodes are fundamental for representing constant data within the source code.

All literal nodes inherit from `vyn::ast::Expression`.

## 1. `Identifier`

Represents an identifier in the source code. While not strictly a literal, it's a fundamental token often treated alongside literals in parsing and AST construction. It is used as an expression.

-   **C++ Class**: `vyn::ast::Identifier`
-   **`NodeType`**: `IDENTIFIER`
-   **Fields**:
    -   `name` (`std::string`): The name of the identifier.

```cpp
// From vyn/parser/ast.hpp
namespace vyn {
namespace ast {

class Identifier : public Expression {
public:
    std::string name;

    Identifier(SourceLocation loc, std::string name);
    NodeType getType() const override;
    std::string toString() const override;
    void accept(Visitor& visitor) override; // visitor.visit(this);
};

} // namespace ast
} // namespace vyn
```

## 2. `IntegerLiteral`

Represents an integer literal.

-   **C++ Class**: `vyn::ast::IntegerLiteral`
-   **`NodeType`**: `INTEGER_LITERAL`
-   **Fields**:\n    -   `value` (`int64_t`): The value of the literal.

```cpp
// From vyn/parser/ast.hpp
namespace vyn {
namespace ast {

class IntegerLiteral : public Expression {
public:
    int64_t value;

    IntegerLiteral(SourceLocation loc, int64_t value);
    // virtual ~IntegerLiteral(); // As per ast.hpp
    NodeType getType() const override;
    std::string toString() const override;
    void accept(Visitor& visitor) override; // visitor.visit(this);
};

} // namespace ast
} // namespace vyn
```

## 3. `FloatLiteral`

Represents a floating-point literal.

-   **C++ Class**: `vyn::ast::FloatLiteral`
-   **`NodeType`**: `FLOAT_LITERAL`
-   **Fields**:\n    -   `value` (`double`): The value of the literal.

```cpp
// From vyn/parser/ast.hpp
namespace vyn {
namespace ast {

class FloatLiteral : public Expression {
public:
    double value;

    FloatLiteral(SourceLocation loc, double value);
    NodeType getType() const override;
    std::string toString() const override;
    void accept(Visitor& visitor) override; // visitor.visit(this);
};

} // namespace ast
} // namespace vyn
```

## 4. `StringLiteral`

Represents a string literal.

-   **C++ Class**: `vyn::ast::StringLiteral`
-   **`NodeType`**: `STRING_LITERAL`
-   **Fields**:\n    -   `value` (`std::string`): The value of the literal.

```cpp
// From vyn/parser/ast.hpp
namespace vyn {
namespace ast {

class StringLiteral : public Expression {
public:
    std::string value;

    StringLiteral(SourceLocation loc, std::string value);
    NodeType getType() const override;
    std::string toString() const override;
    void accept(Visitor& visitor) override; // visitor.visit(this);
};

} // namespace ast
} // namespace vyn
```

## 5. `BooleanLiteral`

Represents a boolean literal (`true` or `false`).

-   **C++ Class**: `vyn::ast::BooleanLiteral`
-   **`NodeType`**: `BOOLEAN_LITERAL`
-   **Fields**:\n    -   `value` (`bool`): The value of the literal.

```cpp
// From vyn/parser/ast.hpp
namespace vyn {
namespace ast {

class BooleanLiteral : public Expression {
public:
    bool value;

    BooleanLiteral(SourceLocation loc, bool value);
    NodeType getType() const override;
    std::string toString() const override;
    void accept(Visitor& visitor) override; // visitor.visit(this);
};

} // namespace ast
} // namespace vyn
```

## 6. `NilLiteral`

Represents a `nil` literal, indicating the absence of a value.

-   **C++ Class**: `vyn::ast::NilLiteral`
-   **`NodeType`**: `NIL_LITERAL`

```cpp
// From vyn/parser/ast.hpp
namespace vyn {
namespace ast {

class NilLiteral : public Expression {
public:
    NilLiteral(SourceLocation loc);
    NodeType getType() const override;
    std::string toString() const override;
    void accept(Visitor& visitor) override; // visitor.visit(this);
};

} // namespace ast
} // namespace vyn
```

## 7. `ArrayLiteral`

Represents an array literal (e.g., `[1, 2, 3]`).

-   **C++ Class**: `vyn::ast::ArrayLiteral`
-   **`NodeType`**: `ARRAY_LITERAL`
-   **Fields**:\n    -   `elements` (`std::vector<ExprPtr>`): The elements of the array. (`ExprPtr` is `std::unique_ptr<Expression>`)

```cpp
// From vyn/parser/ast.hpp
namespace vyn {
namespace ast {

// using ExprPtr = std::unique_ptr<Expression>; // Defined in ast.hpp

class ArrayLiteral : public Expression {
public:
    std::vector<ExprPtr> elements;

    ArrayLiteral(SourceLocation loc, std::vector<ExprPtr> elements);
    NodeType getType() const override; // Returns NodeType::ARRAY_LITERAL
    std::string toString() const override;
    void accept(Visitor& visitor) override; // visitor.visit(this);
};

} // namespace ast
} // namespace vyn
```

## 8. `ObjectLiteral`

Represents an object or struct literal (e.g., `{ name: "Vyn", version: 0.1 }` or `MyStruct { field: value }`).

-   **C++ Class**: `vyn::ast::ObjectLiteral`
-   **`NodeType`**: `OBJECT_LITERAL`
-   **Fields**:
    -   `typePath` (`TypeNodePtr`, optional): The optional explicit type of the object being instantiated (e.g., `MyStruct` in `MyStruct { ... }`). (`TypeNodePtr` is `std::unique_ptr<TypeNode>`)
    -   `properties` (`std::vector<ObjectProperty>`): Key-value pairs representing the members of the object.

The `ObjectProperty` struct is defined as:
```cpp
// From vyn/parser/ast.hpp
namespace vyn {
namespace ast {

// using IdentifierPtr = std::unique_ptr<Identifier>; // Defined in ast.hpp
// using ExprPtr = std::unique_ptr<Expression>; // Defined in ast.hpp

struct ObjectProperty {
    SourceLocation loc;
    IdentifierPtr key; // Property key
    ExprPtr value;     // Property value

public:
    ObjectProperty(SourceLocation loc, IdentifierPtr key, ExprPtr value);
};

} // namespace ast
} // namespace vyn
```

And the `ObjectLiteral` class:
```cpp
// From vyn/parser/ast.hpp
namespace vyn {
namespace ast {

// using TypeNodePtr = std::unique_ptr<TypeNode>; // Defined in ast.hpp

class ObjectLiteral : public Expression {
public:
    TypeNodePtr typePath; // Optional: For typed struct literals like MyType { ... }
    std::vector<ObjectProperty> properties;

    ObjectLiteral(SourceLocation loc, TypeNodePtr typePath, std::vector<ObjectProperty> properties);
    // virtual ~ObjectLiteral(); // As per ast.hpp
    NodeType getType() const override;
    std::string toString() const override;
    void accept(Visitor& visitor) override; // visitor.visit(this);
};

} // namespace ast
} // namespace vyn
```

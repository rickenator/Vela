# AST Nodes: Expressions

This document describes the Abstract Syntax Tree (AST) nodes for various expressions in the Vyn programming language, as defined in `include/vyn/parser/ast.hpp`. Expressions are constructs that evaluate to a value.

All expression nodes inherit from `vyn::ast::Expression`.

## Common Pointer Aliases

Throughout the AST definitions, the following smart pointer aliases are used for brevity and clarity, representing `std::unique_ptr<T>`:

- `ExprPtr = std::unique_ptr<Expression>;`
- `IdentifierPtr = std::unique_ptr<Identifier>;`
- `TypeNodePtr = std::unique_ptr<TypeNode>;`

## 1. `UnaryExpression`

Represents a unary expression (e.g., `-x`, `!flag`).

-   **C++ Class**: `vyn::ast::UnaryExpression`
-   **`NodeType`**: `UNARY_EXPRESSION`
-   **Fields**:
    -   `op` (`std::string`): The unary operator (e.g., "-", "!").
    -   `operand` (`ExprPtr`): The expression the operator applies to.

```cpp
// From include/vyn/parser/ast.hpp
namespace vyn::ast {
class UnaryExpression : public Expression {
public:
    std::string op;
    ExprPtr operand;

    UnaryExpression(SourceLocation loc, std::string op, ExprPtr operand);
    // ... accept, getType, toString methods ...
};
} // namespace vyn::ast
```

## 2. `BinaryExpression`

Represents a binary expression (e.g., `a + b`, `x == y`).

-   **C++ Class**: `vyn::ast::BinaryExpression`
-   **`NodeType`**: `BINARY_EXPRESSION`
-   **Fields**:
    -   `op` (`std::string`): The binary operator (e.g., "+", "==").
    -   `left` (`ExprPtr`): The left-hand side operand.
    -   `right` (`ExprPtr`): The right-hand side operand.

```cpp
// From include/vyn/parser/ast.hpp
namespace vyn::ast {
class BinaryExpression : public Expression {
public:
    std::string op;
    ExprPtr left;
    ExprPtr right;

    BinaryExpression(SourceLocation loc, std::string op, ExprPtr left, ExprPtr right);
    // ... accept, getType, toString methods ...
};
} // namespace vyn::ast
```

## 3. `CallExpression`

Represents a function or method call.

-   **C++ Class**: `vyn::ast::CallExpression`
-   **`NodeType`**: `CALL_EXPRESSION`
-   **Fields**:
    -   `callee` (`ExprPtr`): The expression that evaluates to the function to be called (e.g., an identifier or a member expression).
    -   `arguments` (`std::vector<ExprPtr>`): The list of arguments passed to the function.

```cpp
// From include/vyn/parser/ast.hpp
namespace vyn::ast {
class CallExpression : public Expression {
public:
    ExprPtr callee;
    std::vector<ExprPtr> arguments;

    CallExpression(SourceLocation loc, ExprPtr callee, std::vector<ExprPtr> arguments);
    // ... accept, getType, toString methods ...
};
} // namespace vyn::ast
```

## 4. `ConstructionExpression`

Represents an expression that constructs an instance of a type using constructor-like syntax (e.g., `Point(10, 20)`).

-   **C++ Class**: `vyn::ast::ConstructionExpression`
-   **`NodeType`**: `CONSTRUCTION_EXPRESSION`
-   **Fields**:
    -   `type` (`TypeNodePtr`): The type being constructed.
    -   `arguments` (`std::vector<ExprPtr>`): The arguments passed to the constructor.

```cpp
// From include/vyn/parser/ast.hpp
namespace vyn::ast {
class ConstructionExpression : public Expression {
public:
    TypeNodePtr type;
    std::vector<ExprPtr> arguments;

    ConstructionExpression(SourceLocation loc, TypeNodePtr type, std::vector<ExprPtr> arguments);
    // ... accept, getType, toString methods ...
};
} // namespace vyn::ast
```

## 5. `ArrayInitializationExpression`

Represents the initialization of an array with a specific type and size, potentially with an initializer list (e.g., `[int; 3](1, 2, 3)` or `[int; 10]()`).

-   **C++ Class**: `vyn::ast::ArrayInitializationExpression`
-   **`NodeType`**: `ARRAY_INITIALIZATION_EXPRESSION`
-   **Fields**:
    -   `elementType` (`TypeNodePtr`): The type of the array elements.
    -   `size` (`ExprPtr`): The size of the array.
    -   `initializerList` (`std::optional<std::vector<ExprPtr>>`): Optional list of expressions to initialize array elements.

```cpp
// From include/vyn/parser/ast.hpp
namespace vyn::ast {
class ArrayInitializationExpression : public Expression {
public:
    TypeNodePtr elementType;
    ExprPtr size;
    std::optional<std::vector<ExprPtr>> initializerList;

    ArrayInitializationExpression(SourceLocation loc, TypeNodePtr elementType, ExprPtr size, std::optional<std::vector<ExprPtr>> initializerList);
    // ... accept, getType, toString methods ...
};
} // namespace vyn::ast
```

## 6. `MemberExpression`

Represents accessing a member of a struct, class, or object (e.g., `object.field`, `ptr->field`, `module::item`).

-   **C++ Class**: `vyn::ast::MemberExpression`
-   **`NodeType`**: `MEMBER_EXPRESSION`
-   **Fields**:
    -   `object` (`ExprPtr`): The expression whose member is being accessed.
    -   `member` (`IdentifierPtr`): The identifier of the member.
    -   `isArrow` (`bool`): True if accessed with `->` (pointer dereference), false for `.`.

```cpp
// From include/vyn/parser/ast.hpp
namespace vyn::ast {
class MemberExpression : public Expression {
public:
    ExprPtr object;
    IdentifierPtr member;
    bool isArrow; // True for ->, false for .

    MemberExpression(SourceLocation loc, ExprPtr object, IdentifierPtr member, bool isArrow);
    // ... accept, getType, toString methods ...
};
} // namespace vyn::ast
```

## 7. `AssignmentExpression`

Represents an assignment operation (e.g., `variable = value`).

-   **C++ Class**: `vyn::ast::AssignmentExpression`
-   **`NodeType`**: `ASSIGNMENT_EXPRESSION`
-   **Fields**:
    -   `left` (`ExprPtr`): The left-hand side of the assignment (l-value).
    -   `right` (`ExprPtr`): The right-hand side of the assignment (r-value).

```cpp
// From include/vyn/parser/ast.hpp
namespace vyn::ast {
class AssignmentExpression : public Expression {
public:
    ExprPtr left;
    ExprPtr right;

    AssignmentExpression(SourceLocation loc, ExprPtr left, ExprPtr right);
    // ... accept, getType, toString methods ...
};
} // namespace vyn::ast
```

## 8. `IfExpression`

Represents an if-expression, which evaluates to one of two values based on a condition (e.g., `if (cond) expr1 else expr2`).

-   **C++ Class**: `vyn::ast::IfExpression`
-   **`NodeType`**: `IF_EXPRESSION`
-   **Fields**:
    -   `condition` (`ExprPtr`): The condition to evaluate.
    -   `thenBranch` (`ExprPtr`): The expression to evaluate if the condition is true.
    -   `elseBranch` (`ExprPtr`): The expression to evaluate if the condition is false (must be present for it to be an expression).

```cpp
// From include/vyn/parser/ast.hpp
namespace vyn::ast {
class IfExpression : public Expression {
public:
    ExprPtr condition;
    ExprPtr thenBranch;
    ExprPtr elseBranch; // Mandatory for an if-expression

    IfExpression(SourceLocation loc, ExprPtr condition, ExprPtr thenBranch, ExprPtr elseBranch);
    // ... accept, getType, toString methods ...
};
} // namespace vyn::ast
```

## 9. `RangeExpression`

Represents a range expression (e.g., `1..10`, `a..=b`).

-   **C++ Class**: `vyn::ast::RangeExpression`
-   **`NodeType`**: `RANGE_EXPRESSION`
-   **Fields**:
    -   `start` (`ExprPtr`): The starting value of the range.
    -   `end` (`ExprPtr`): The ending value of the range.
    -   `isInclusive` (`bool`): Whether the range includes the end value (true for `..=`, false for `..`).

```cpp
// From include/vyn/parser/ast.hpp
namespace vyn::ast {
class RangeExpression : public Expression {
public:
    ExprPtr start;
    ExprPtr end;
    bool isInclusive;

    RangeExpression(SourceLocation loc, ExprPtr start, ExprPtr end, bool isInclusive);
    // ... accept, getType, toString methods ...
};
} // namespace vyn::ast
```

## 10. `BorrowExpression`

Represents a borrow expression (e.g., `&x`, `&mut y`).

-   **C++ Class**: `vyn::ast::BorrowExpression`
-   **`NodeType`**: `BORROW_EXPRESSION`
-   **Fields**:
    -   `operand` (`ExprPtr`): The expression being borrowed.
    -   `kind` (`BorrowKind`): The kind of borrow (e.g., `SHARED`, `MUTABLE`, `UNIQUE_MUTABLE`). `BorrowKind` is an enum defined in `ast.hpp`.

```cpp
// From include/vyn/parser/ast.hpp
namespace vyn::ast {
// enum class BorrowKind { SHARED, MUTABLE, UNIQUE_MUTABLE }; // Defined in ast.hpp
class BorrowExpression : public Expression {
public:
    ExprPtr operand;
    BorrowKind kind;

    BorrowExpression(SourceLocation loc, ExprPtr operand, BorrowKind kind);
    // ... accept, getType, toString methods ...
};
} // namespace vyn::ast
```

## 11. `PointerDerefExpression`

Represents dereferencing a pointer (e.g., `*ptr`).

-   **C++ Class**: `vyn::ast::PointerDerefExpression`
-   **`NodeType`**: `POINTER_DEREF_EXPRESSION`
-   **Fields**:
    -   `operand` (`ExprPtr`): The pointer expression being dereferenced.

```cpp
// From include/vyn/parser/ast.hpp
namespace vyn::ast {
class PointerDerefExpression : public Expression {
public:
    ExprPtr operand;

    PointerDerefExpression(SourceLocation loc, ExprPtr operand);
    // ... accept, getType, toString methods ...
};
} // namespace vyn::ast
```

## 12. `AddrOfExpression`

Represents the address-of operation (e.g., `&variable` in contexts where it means address, distinct from borrow for ownership system).

-   **C++ Class**: `vyn::ast::AddrOfExpression`
-   **`NodeType`**: `ADDR_OF_EXPRESSION`
-   **Fields**:
    -   `operand` (`ExprPtr`): The operand whose address is being taken.

```cpp
// From include/vyn/parser/ast.hpp
namespace vyn::ast {
class AddrOfExpression : public Expression {
public:
    ExprPtr operand;

    AddrOfExpression(SourceLocation loc, ExprPtr operand);
    // ... accept, getType, toString methods ...
};
} // namespace vyn::ast
```

## 13. `FromIntToLocExpression`

Represents a Vyn-specific expression for converting integers to a source location. Likely for internal or metaprogramming use.

-   **C++ Class**: `vyn::ast::FromIntToLocExpression`
-   **`NodeType`**: `FROM_INT_TO_LOC_EXPRESSION`
-   **Fields**:
    -   `line` (`ExprPtr`): Expression evaluating to the line number.
    -   `column` (`ExprPtr`): Expression evaluating to the column number.

```cpp
// From include/vyn/parser/ast.hpp
namespace vyn::ast {
class FromIntToLocExpression : public Expression {
public:
    ExprPtr line;
    ExprPtr column;

    FromIntToLocExpression(SourceLocation loc, ExprPtr line, ExprPtr column);
    // ... accept, getType, toString methods ...
};
} // namespace vyn::ast
```

## 14. `ArrayElementExpression`

Represents accessing an element of an array or slice (e.g., `array[index]`).

-   **C++ Class**: `vyn::ast::ArrayElementExpression`
-   **`NodeType`**: `ARRAY_ELEMENT_EXPRESSION`
-   **Fields**:
    -   `array` (`ExprPtr`): The array or slice expression.
    -   `index` (`ExprPtr`): The index expression.

```cpp
// From include/vyn/parser/ast.hpp
namespace vyn::ast {
class ArrayElementExpression : public Expression {
public:
    ExprPtr array;
    ExprPtr index;

    ArrayElementExpression(SourceLocation loc, ExprPtr array, ExprPtr index);
    // ... accept, getType, toString methods ...
};
} // namespace vyn::ast
```

## 15. `LocationExpression`

Represents an expression that evaluates to the current source code location. Likely for metaprogramming or debugging.

-   **C++ Class**: `vyn::ast::LocationExpression`
-   **`NodeType`**: `LOCATION_EXPRESSION`

```cpp
// From include/vyn/parser/ast.hpp
namespace vyn::ast {
class LocationExpression : public Expression {
public:
    LocationExpression(SourceLocation loc);
    // ... accept, getType, toString methods ...
};
} // namespace vyn::ast
```

## 16. `ListComprehension`

Represents a list comprehension (e.g., `[x * 2 for x in items if x > 0]`).

-   **C++ Class**: `vyn::ast::ListComprehension`
-   **`NodeType`**: `LIST_COMPREHENSION`
-   **Fields**:
    -   `output` (`ExprPtr`): The expression for each element in the new list (e.g., `x * 2`).
    -   `variable` (`IdentifierPtr`): The variable used in the comprehension (e.g., `x`).
    -   `iterable` (`ExprPtr`): The expression for the collection being iterated over (e.g., `items`).
    -   `condition` (`std::optional<ExprPtr>`): An optional condition to filter elements (e.g., `x > 0`).

```cpp
// From include/vyn/parser/ast.hpp
namespace vyn::ast {
class ListComprehension : public Expression {
public:
    ExprPtr output;
    IdentifierPtr variable;
    ExprPtr iterable;
    std::optional<ExprPtr> condition;

    ListComprehension(SourceLocation loc, ExprPtr output, IdentifierPtr variable, ExprPtr iterable, std::optional<ExprPtr> condition);
    // ... accept, getType, toString methods ...
};
} // namespace vyn::ast
```

*Note: `TernaryExpression` was previously listed but is not present in `include/vyn/parser/ast.hpp` and has been removed. Other expression types might be detailed in `AST_Literals.md` (like `Identifier`) or `AST_Types.md` (if type constructs can be expressions).* For planned expressions, refer to `AST_Roadmap.md`.

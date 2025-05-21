# Vyn AST: Type System Nodes

This document describes AST nodes related to Vyn's type system, as defined in `include/vyn/parser/ast.hpp`. These nodes are used to represent type information in variable declarations, function parameters, return types, and type-related expressions.

All specific type nodes inherit from the base `vyn::ast::TypeNode` class, which itself inherits from `vyn::ast::Node`.

## Common Pointer Aliases

- `TypeNodePtr = std::unique_ptr<TypeNode>;`
- `ExprPtr = std::unique_ptr<Expression>;`
- `IdentifierPtr = std::unique_ptr<Identifier>;`

## 1. Base `TypeNode` (`vyn::ast::TypeNode`)

This is the abstract base class for all type nodes. It provides common functionality and a way to handle types polymorphically.

-   **C++ Class**: `vyn::ast::TypeNode`
-   **`NodeType`**: `TYPE_NODE` (This is a general category; specific type nodes will have their own `NodeType` values like `BASIC_TYPE_NODE`, `POINTER_TYPE_NODE`, etc.)
-   **Inheritance**: `vyn::ast::Node` -> `vyn::ast::TypeNode`

```cpp
// Base class from include/vyn/parser/ast.hpp
namespace vyn::ast {

class TypeNode : public Node {
public:
    TypeCategory category; // e.g., BASIC, POINTER, ARRAY, FUNCTION, etc.

    TypeNode(SourceLocation loc, TypeCategory category);
    // Pure virtual or common methods like toString, accept, etc.
    // virtual std::string ToSignature() const = 0; // Example of a common method
    // ... accept, getType, toString methods ...
};

} // namespace vyn::ast
```

## 2. Specific Type Nodes

These classes inherit from `TypeNode` and represent concrete types in the language.

### 2.1. `BasicTypeNode`

Represents a basic, named type (e.g., `i32`, `string`, `MyStruct`).

-   **C++ Class**: `vyn::ast::BasicTypeNode`
-   **`NodeType`**: `BASIC_TYPE_NODE`
-   **Fields**:
    -   `name` (`IdentifierPtr`): The identifier representing the type name.

```cpp
// From include/vyn/parser/ast.hpp
namespace vyn::ast {

class BasicTypeNode : public TypeNode {
public:
    IdentifierPtr name;

    BasicTypeNode(SourceLocation loc, IdentifierPtr name);
    // ... accept, getType, toString, ToSignature methods ...
};

} // namespace vyn::ast
```

### 2.2. `PointerTypeNode`

Represents a pointer type (e.g., `*i32`, `*mut string`).

-   **C++ Class**: `vyn::ast::PointerTypeNode`
-   **`NodeType`**: `POINTER_TYPE_NODE`
-   **Fields**:
    -   `pointeeType` (`TypeNodePtr`): The type of the data pointed to.
    -   `isMutable` (`bool`): True if the pointer allows mutation of the pointed-to data (e.g., `*mut`).

```cpp
// From include/vyn/parser/ast.hpp
namespace vyn::ast {

class PointerTypeNode : public TypeNode {
public:
    TypeNodePtr pointeeType;
    bool isMutable;

    PointerTypeNode(SourceLocation loc, TypeNodePtr pointeeType, bool isMutable);
    // ... accept, getType, toString, ToSignature methods ...
};

} // namespace vyn::ast
```

### 2.3. `ReferenceTypeNode`

Represents a reference type (e.g., `&i32`, `&mut string`).

-   **C++ Class**: `vyn::ast::ReferenceTypeNode`
-   **`NodeType`**: `REFERENCE_TYPE_NODE`
-   **Fields**:
    -   `referencedType` (`TypeNodePtr`): The type of the data being referenced.
    -   `kind` (`BorrowKind`): The kind of borrow (e.g., `SHARED`, `MUTABLE`, `UNIQUE_MUTABLE`).

```cpp
// From include/vyn/parser/ast.hpp
namespace vyn::ast {
// enum class BorrowKind { SHARED, MUTABLE, UNIQUE_MUTABLE }; // Defined in ast.hpp

class ReferenceTypeNode : public TypeNode {
public:
    TypeNodePtr referencedType;
    BorrowKind kind;

    ReferenceTypeNode(SourceLocation loc, TypeNodePtr referencedType, BorrowKind kind);
    // ... accept, getType, toString, ToSignature methods ...
};

} // namespace vyn::ast
```

### 2.4. `ArrayTypeNode`

Represents an array type (e.g., `[i32; 5]`, `[string]`).

-   **C++ Class**: `vyn::ast::ArrayTypeNode`
-   **`NodeType`**: `ARRAY_TYPE_NODE`
-   **Fields**:
    -   `elementType` (`TypeNodePtr`): The type of the elements in the array.
    -   `size` (`std::optional<ExprPtr>`): An optional expression for the size of the array. If `std::nullopt`, it represents a slice.

```cpp
// From include/vyn/parser/ast.hpp
namespace vyn::ast {

class ArrayTypeNode : public TypeNode {
public:
    TypeNodePtr elementType;
    std::optional<ExprPtr> size; // nullopt for slices

    ArrayTypeNode(SourceLocation loc, TypeNodePtr elementType, std::optional<ExprPtr> size);
    // ... accept, getType, toString, ToSignature methods ...
};

} // namespace vyn::ast
```

### 2.5. `FunctionTypeNode`

Represents a function type (e.g., `fn(i32, bool) -> string`).

-   **C++ Class**: `vyn::ast::FunctionTypeNode`
-   **`NodeType`**: `FUNCTION_TYPE_NODE`
-   **Fields**:
    -   `parameterTypes` (`std::vector<TypeNodePtr>`): A vector of types for the function parameters.
    -   `returnType` (`TypeNodePtr`): The return type of the function.

```cpp
// From include/vyn/parser/ast.hpp
namespace vyn::ast {

class FunctionTypeNode : public TypeNode {
public:
    std::vector<TypeNodePtr> parameterTypes;
    TypeNodePtr returnType; // Could be a special 'void' type node or similar for no return

    FunctionTypeNode(SourceLocation loc, std::vector<TypeNodePtr> parameterTypes, TypeNodePtr returnType);
    // ... accept, getType, toString, ToSignature methods ...
};

} // namespace vyn::ast
```

### 2.6. `TupleTypeNode`

Represents a tuple type (e.g., `(i32, string, bool)`).

-   **C++ Class**: `vyn::ast::TupleTypeNode`
-   **`NodeType`**: `TUPLE_TYPE_NODE`
-   **Fields**:
    -   `elementTypes` (`std::vector<TypeNodePtr>`): A vector of types for the elements in the tuple.

```cpp
// From include/vyn/parser/ast.hpp
namespace vyn::ast {

class TupleTypeNode : public TypeNode {
public:
    std::vector<TypeNodePtr> elementTypes;

    TupleTypeNode(SourceLocation loc, std::vector<TypeNodePtr> elementTypes);
    // ... accept, getType, toString, ToSignature methods ...
};

} // namespace vyn::ast
```

### 2.7. `OptionalTypeNode`

Represents an optional type (e.g., `?i32`).

-   **C++ Class**: `vyn::ast::OptionalTypeNode`
-   **`NodeType`**: `OPTIONAL_TYPE_NODE`
-   **Fields**:
    -   `valueType` (`TypeNodePtr`): The underlying type that is optional.

```cpp
// From include/vyn/parser/ast.hpp
namespace vyn::ast {

class OptionalTypeNode : public TypeNode {
public:
    TypeNodePtr valueType;

    OptionalTypeNode(SourceLocation loc, TypeNodePtr valueType);
    // ... accept, getType, toString, ToSignature methods ...
};

} // namespace vyn::ast
```

### 2.8. `GenericInstanceTypeNode`

Represents an instantiation of a generic type (e.g., `Vec<i32>`, `Map<string, User>`, `loc<Int>`).

-   **C++ Class**: `vyn::ast::GenericInstanceTypeNode`
-   **`NodeType`**: `GENERIC_INSTANCE_TYPE_NODE`
-   **Fields**:
    -   `genericType` (`TypeNodePtr`): The base generic type (often a `BasicTypeNode` like `Vec` or `loc`).
    -   `typeArguments` (`std::vector<TypeNodePtr>`): The type arguments provided for the generic parameters.

```cpp
// From include/vyn/parser/ast.hpp
namespace vyn::ast {

class GenericInstanceTypeNode : public TypeNode {
public:
    TypeNodePtr genericType; // e.g., BasicTypeNode for "Vec" or "loc"
    std::vector<TypeNodePtr> typeArguments; // e.g., [BasicTypeNode for "i32"]

    GenericInstanceTypeNode(SourceLocation loc, TypeNodePtr genericType, std::vector<TypeNodePtr> typeArguments);
    // ... accept, getType, toString, ToSignature methods ...
};

} // namespace vyn::ast
```

#### Memory System Types

Vyn includes specialized types for low-level memory operations, which are typically represented using GenericInstanceTypeNode:

##### `loc<T>` Type

Represents a raw memory location (pointer) of type `T`.

- Syntax: `loc<T>`
- AST Representation: `GenericInstanceTypeNode` with:
  - `genericType`: `BasicTypeNode` with name "loc"
  - `typeArguments`: A vector containing a single entry of the pointee type `T`

This type is used in unsafe code blocks to work with raw memory. Operations on `loc<T>` include:
- Getting the address of a variable: `loc(x)`
- Dereferencing a pointer: `at(p)`
- Converting between pointer types: `from<loc<T>>(addr)`

These operations are typically represented using `ConstructionExpression` or `CallExpression` in the AST.

This structure allows Vyn's AST to accurately represent a wide variety of type constructs found in modern programming languages. The parser (`TypeParser`) is responsible for translating type syntax from the source code into these `TypeNode` structures.

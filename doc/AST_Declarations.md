# Vyn AST: Declarations

This document details the various declaration AST nodes used in Vyn, as defined in `include/vyn/parser/ast.hpp`. All declaration nodes inherit from `vyn::ast::Declaration` (which itself inherits from `vyn::ast::Statement`).

## Common Pointer Aliases

Throughout the AST definitions, the following smart pointer aliases are used for brevity and clarity, representing `std::unique_ptr<T>`:

- `IdentifierPtr = std::unique_ptr<Identifier>;`
- `TypeNodePtr = std::unique_ptr<TypeNode>;`
- `ExprPtr = std::unique_ptr<Expression>;`
- `StmtPtr = std::unique_ptr<Statement>;`
- `DeclPtr = std::unique_ptr<Declaration>;`
- `BlockStatementPtr = std::unique_ptr<BlockStatement>;`
- `FieldDeclarationPtr = std::unique_ptr<FieldDeclaration>;`
- `GenericParameterPtr = std::unique_ptr<GenericParameter>;`
- `FunctionDeclarationPtr = std::unique_ptr<FunctionDeclaration>;`


## 1. Variable Declaration (`vyn::ast::VariableDeclaration`)

Represents the declaration of a variable.

```cpp
// Structure in include/vyn/parser/ast.hpp
namespace vyn::ast {

class VariableDeclaration : public Declaration {
public:
    IdentifierPtr name;
    std::optional<TypeNodePtr> type; // Optional: type can be inferred
    std::optional<ExprPtr> initializer;
    bool isMutable;
    bool isConst; // For compile-time constants

    VariableDeclaration(SourceLocation loc, IdentifierPtr name,
                        std::optional<TypeNodePtr> type,
                        std::optional<ExprPtr> initializer,
                        bool isMutable, bool isConst);
    // ... accept, getType, toString methods ...
};

} // namespace vyn::ast
```

## 2. Function Parameter (`vyn::ast::FunctionParameter`)

Represents a single parameter in a function declaration. This is a helper structure and inherits directly from `Node`.

```cpp
// Structure in include/vyn/parser/ast.hpp
namespace vyn::ast {

class FunctionParameter : public Node { // Inherits from Node
public:
    IdentifierPtr name;
    TypeNodePtr type;
    bool isMutable; // Whether the parameter is mutable within the function body

    FunctionParameter(SourceLocation loc, IdentifierPtr name, TypeNodePtr type, bool isMutable);
    // ... accept, getType, toString methods ...
};

} // namespace vyn::ast
```

## 3. Function Declaration (`vyn::ast::FunctionDeclaration`)

Represents the declaration of a function.

```cpp
// Structure in include/vyn/parser/ast.hpp
namespace vyn::ast {

class FunctionDeclaration : public Declaration {
public:
    IdentifierPtr name;
    std::vector<FunctionParameter> parameters; // Note: stores FunctionParameter objects directly
    std::optional<TypeNodePtr> returnType;
    std::optional<BlockStatementPtr> body; // Optional for external/forward declarations
    std::vector<GenericParameterPtr> genericParameters;
    bool isExtern; // e.g., extern "C" void printf(...);
    bool isMember; // True if this is a method within a struct/class/impl

    FunctionDeclaration(SourceLocation loc, IdentifierPtr name,
                        std::vector<FunctionParameter> params,
                        std::optional<TypeNodePtr> returnType,
                        std::optional<BlockStatementPtr> body,
                        std::vector<GenericParameterPtr> genericParams,
                        bool isExtern, bool isMember);
    // ... accept, getType, toString methods ...
};

} // namespace vyn::ast
```

## 4. Type Alias Declaration (`vyn::ast::TypeAliasDeclaration`)

Represents a type alias (e.g., `type MyInt = i32;`).

```cpp
// Structure in include/vyn/parser/ast.hpp
namespace vyn::ast {

class TypeAliasDeclaration : public Declaration {
public:
    IdentifierPtr name;
    TypeNodePtr aliasedType;
    std::vector<GenericParameterPtr> genericParameters;

    TypeAliasDeclaration(SourceLocation loc, IdentifierPtr name,
                         TypeNodePtr aliasedType,
                         std::vector<GenericParameterPtr> genericParams);
    // ... accept, getType, toString methods ...
};

} // namespace vyn::ast
```

## 5. Import Specifier (`vyn::ast::ImportSpecifier`)

Represents a single item being imported in an import declaration (e.g., `foo` or `foo as bar` in `import module::{foo as bar};`). This is a helper structure and inherits directly from `Node`.

```cpp
// Structure in include/vyn/parser/ast.hpp
namespace vyn::ast {

class ImportSpecifier : public Node { // Inherits from Node
public:
    IdentifierPtr name;
    std::optional<IdentifierPtr> alias; // e.g., `bar` in `foo as bar`

    ImportSpecifier(SourceLocation loc, IdentifierPtr name, std::optional<IdentifierPtr> alias);
    // ... accept, getType, toString methods ...
};

} // namespace vyn::ast
```

## 6. Import Declaration (`vyn::ast::ImportDeclaration`)

Represents an import statement (e.g., `import module::submodule;` or `import module::{Item1, Item2};`).

```cpp
// Structure in include/vyn/parser/ast.hpp
namespace vyn::ast {

class ImportDeclaration : public Declaration {
public:
    std::vector<IdentifierPtr> path; // e.g., ["module", "submodule"]
    std::vector<ImportSpecifier> specifiers; // Specific items to import, empty if importing module directly or with '*'
    bool importAll; // True if `import module::*;`

    ImportDeclaration(SourceLocation loc, std::vector<IdentifierPtr> path,
                      std::vector<ImportSpecifier> specifiers, bool importAll);
    // ... accept, getType, toString methods ...
};

} // namespace vyn::ast
```

## 7. Struct Declaration (`vyn::ast::StructDeclaration`)

Represents the declaration of a struct.

```cpp
// Structure in include/vyn/parser/ast.hpp
namespace vyn::ast {

class StructDeclaration : public Declaration {
public:
    IdentifierPtr name;
    std::vector<FieldDeclarationPtr> fields;
    std::vector<GenericParameterPtr> genericParameters;

    StructDeclaration(SourceLocation loc, IdentifierPtr name,
                      std::vector<FieldDeclarationPtr> fields,
                      std::vector<GenericParameterPtr> genericParams);
    // ... accept, getType, toString methods ...
};

} // namespace vyn::ast
```

## 8. Class Declaration (`vyn::ast::ClassDeclaration`)

Represents the declaration of a class. *Note: The role of classes versus structs and traits/impls is under review. This node might be refined or superseded.*

```cpp
// Structure in include/vyn/parser/ast.hpp
namespace vyn::ast {

class ClassDeclaration : public Declaration {
public:
    IdentifierPtr name;
    std::vector<FieldDeclarationPtr> fields;
    std::vector<FunctionDeclarationPtr> methods; // Methods are FunctionDeclarations
    std::vector<GenericParameterPtr> genericParameters;
    // std::optional<TypeNodePtr> baseClass; // Potential future extension

    ClassDeclaration(SourceLocation loc, IdentifierPtr name,
                     std::vector<FieldDeclarationPtr> fields,
                     std::vector<FunctionDeclarationPtr> methods,
                     std::vector<GenericParameterPtr> genericParams);
    // ... accept, getType, toString methods ...
};

} // namespace vyn::ast
```

## 9. Field Declaration (`vyn::ast::FieldDeclaration`)

Represents a field within a struct or class. It's a declaration itself.

```cpp
// Structure in include/vyn/parser/ast.hpp
namespace vyn::ast {

class FieldDeclaration : public Declaration { // Field is a kind of Declaration
public:
    IdentifierPtr name;
    TypeNodePtr type;
    std::optional<ExprPtr> defaultValue; // Optional default value for the field
    bool isMutable; // If the field can be mutated after struct/class initialization

    FieldDeclaration(SourceLocation loc, IdentifierPtr name, TypeNodePtr type,
                     std::optional<ExprPtr> defaultValue, bool isMutable);
    // ... accept, getType, toString methods ...
};

} // namespace vyn::ast
```

## 10. Impl Declaration (`vyn::ast::ImplDeclaration`)

Represents an implementation block, used for implementing methods for a struct/class or for implementing a trait for a struct/class.

```cpp
// Structure in include/vyn/parser/ast.hpp
namespace vyn::ast {

class ImplDeclaration : public Declaration {
public:
    IdentifierPtr structName; // The struct/class being implemented for
    std::optional<IdentifierPtr> traitName; // Optional: if implementing a trait
    std::vector<FunctionDeclarationPtr> methods; // Methods are FunctionDeclarations
    std::vector<GenericParameterPtr> genericParameters; // Generics for the impl block itself

    ImplDeclaration(SourceLocation loc, IdentifierPtr structName,
                    std::optional<IdentifierPtr> traitName,
                    std::vector<FunctionDeclarationPtr> methods,
                    std::vector<GenericParameterPtr> genericParams);
    // ... accept, getType, toString methods ...
};

} // namespace vyn::ast
```

## 11. Enum Variant (`vyn::ast::EnumVariant`)

Represents a variant within an enum declaration. This is a helper structure and inherits directly from `Node`.

```cpp
// Structure in include/vyn/parser/ast.hpp
namespace vyn::ast {

class EnumVariant : public Node { // Inherits from Node
public:
    IdentifierPtr name;
    std::optional<TypeNodePtr> associatedType; // e.g., Option::Some(T) -> T is associatedType

    EnumVariant(SourceLocation loc, IdentifierPtr name, std::optional<TypeNodePtr> associatedType);
    // ... accept, getType, toString methods ...
};

} // namespace vyn::ast
```

## 12. Enum Declaration (`vyn::ast::EnumDeclaration`)

Represents the declaration of an enumeration.

```cpp
// Structure in include/vyn/parser/ast.hpp
namespace vyn::ast {

class EnumDeclaration : public Declaration {
public:
    IdentifierPtr name;
    std::vector<EnumVariant> variants; // Note: stores EnumVariant objects directly
    std::vector<GenericParameterPtr> genericParameters;

    EnumDeclaration(SourceLocation loc, IdentifierPtr name,
                    std::vector<EnumVariant> variants,
                    std::vector<GenericParameterPtr> genericParams);
    // ... accept, getType, toString methods ...
};

} // namespace vyn::ast
```

## 13. Generic Parameter (`vyn::ast::GenericParameter`)

Represents a generic parameter in a declaration (e.g., `T` in `fn foo<T>(p: T)` or `struct Bar<T: SomeTrait>`). This node was formerly named `GenericParamNode`. It inherits directly from `Node`.

```cpp
// Structure in include/vyn/parser/ast.hpp
namespace vyn::ast {

class GenericParameter : public Node { // Inherits from Node
public:
    IdentifierPtr name;
    std::optional<TypeNodePtr> constraint; // Optional trait or type constraint

    GenericParameter(SourceLocation loc, IdentifierPtr name, std::optional<TypeNodePtr> constraint);
    // ... accept, getType, toString methods ...
};

} // namespace vyn::ast
```

## 14. Template Declaration (`vyn::ast::TemplateDeclaration`)

Represents a template declaration, which is a parameterized declaration.
*Note: The exact semantics and usage of `TemplateDeclaration` are still being refined. It might be used for concepts like C++ templates or for more advanced metaprogramming features.*

```cpp
// Structure in include/vyn/parser/ast.hpp
namespace vyn::ast {

class TemplateDeclaration : public Declaration {
public:
    IdentifierPtr name; // Name of the template itself
    std::vector<GenericParameterPtr> parameters; // Template parameters
    DeclPtr declaration; // The actual declaration being templated (e.g., a FunctionDeclaration, StructDeclaration)

    TemplateDeclaration(SourceLocation loc, IdentifierPtr name,
                        std::vector<GenericParameterPtr> parameters,
                        DeclPtr declaration);
    // ... accept, getType, toString methods ...
};

} // namespace vyn::ast
```

## 15. Module (`vyn::ast::Module`)

Represents a module, which is typically a file containing a collection of declarations. This node inherits directly from `Node` as it's a top-level container rather than a language-level declaration statement.

```cpp
// Structure in include/vyn/parser/ast.hpp
namespace vyn::ast {

class Module : public Node { // Inherits from Node
public:
    std::optional<IdentifierPtr> name; // Optional module name (e.g., from 'module my_module;')
    std::vector<DeclPtr> declarations;

    Module(SourceLocation loc, std::optional<IdentifierPtr> name, std::vector<DeclPtr> declarations);
    // ... accept, getType, toString methods ...
};

} // namespace vyn::ast
```

This covers the primary declaration AST nodes. For planned declarations, refer to `AST_Roadmap.md`.

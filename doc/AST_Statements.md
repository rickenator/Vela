# AST Nodes: Statements

This document describes the Abstract Syntax Tree (AST) nodes for various statements in the Vyn programming language, as defined in `include/vyn/parser/ast.hpp`. Statements are units of execution. All statement nodes inherit from `vyn::ast::Statement`.

## Common Pointer Aliases

Throughout the AST definitions, the following smart pointer aliases are used for brevity and clarity, representing `std::unique_ptr<T>`:

- `ExprPtr = std::unique_ptr<Expression>;`
- `StmtPtr = std::unique_ptr<Statement>;`
- `IdentifierPtr = std::unique_ptr<Identifier>;`
- `BlockStatementPtr = std::unique_ptr<BlockStatement>;`
- `TypeNodePtr = std::unique_ptr<TypeNode>;` // For TryStatement catch type

## 1. `BlockStatement`

Represents a block of statements, typically enclosed in braces `{}`.

-   **C++ Class**: `vyn::ast::BlockStatement`
-   **`NodeType`**: `BLOCK_STATEMENT`
-   **Fields**:
    -   `statements` (`std::vector<StmtPtr>`): A vector of pointers to the statements contained within the block.

```cpp
// From include/vyn/parser/ast.hpp
namespace vyn::ast {
class BlockStatement : public Statement {
public:
    std::vector<StmtPtr> statements;

    BlockStatement(SourceLocation loc, std::vector<StmtPtr> statements);
    // ... accept, getType, toString methods ...
};
} // namespace vyn::ast
```

## 2. `ExpressionStatement`

Represents a statement that consists solely of an expression.

-   **C++ Class**: `vyn::ast::ExpressionStatement`
-   **`NodeType`**: `EXPRESSION_STATEMENT`
-   **Fields**:
    -   `expression` (`ExprPtr`): The expression that forms the statement.

```cpp
// From include/vyn/parser/ast.hpp
namespace vyn::ast {
class ExpressionStatement : public Statement {
public:
    ExprPtr expression;

    ExpressionStatement(SourceLocation loc, ExprPtr expression);
    // ... accept, getType, toString methods ...
};
} // namespace vyn::ast
```

## 3. `IfStatement`

Represents an if-conditional statement.

-   **C++ Class**: `vyn::ast::IfStatement`
-   **`NodeType`**: `IF_STATEMENT`
-   **Fields**:
    -   `condition` (`ExprPtr`): The expression evaluated to determine which branch to execute.
    -   `thenBranch` (`StmtPtr`): The statement (often a `BlockStatement`) executed if the condition is true.
    -   `elseBranch` (`std::optional<StmtPtr>`): The statement executed if the condition is false.

```cpp
// From include/vyn/parser/ast.hpp
namespace vyn::ast {
class IfStatement : public Statement {
public:
    ExprPtr condition;
    StmtPtr thenBranch;
    std::optional<StmtPtr> elseBranch;

    IfStatement(SourceLocation loc, ExprPtr condition, StmtPtr thenBranch, std::optional<StmtPtr> elseBranch);
    // ... accept, getType, toString methods ...
};
} // namespace vyn::ast
```

## 4. `ForStatement`

Represents a for loop.

-   **C++ Class**: `vyn::ast::ForStatement`
-   **`NodeType`**: `FOR_STATEMENT`
-   **Fields**:
    -   `iterator` (`IdentifierPtr`): The identifier for the loop variable.
    -   `range` (`ExprPtr`): The expression that evaluates to the range or collection.
    -   `body` (`StmtPtr`): The statement (usually a `BlockStatement`) executed for each iteration.

```cpp
// From include/vyn/parser/ast.hpp
namespace vyn::ast {
class ForStatement : public Statement {
public:
    IdentifierPtr iterator;
    ExprPtr range;
    StmtPtr body;

    ForStatement(SourceLocation loc, IdentifierPtr iterator, ExprPtr range, StmtPtr body);
    // ... accept, getType, toString methods ...
};
} // namespace vyn::ast
```

## 5. `WhileStatement`

Represents a while loop.

-   **C++ Class**: `vyn::ast::WhileStatement`
-   **`NodeType`**: `WHILE_STATEMENT`
-   **Fields**:
    -   `condition` (`ExprPtr`): The expression evaluated before each iteration.
    -   `body` (`StmtPtr`): The statement (usually a `BlockStatement`) executed in each iteration.

```cpp
// From include/vyn/parser/ast.hpp
namespace vyn::ast {
class WhileStatement : public Statement {
public:
    ExprPtr condition;
    StmtPtr body;

    WhileStatement(SourceLocation loc, ExprPtr condition, StmtPtr body);
    // ... accept, getType, toString methods ...
};
} // namespace vyn::ast
```

## 6. `ReturnStatement`

Represents a return statement.

-   **C++ Class**: `vyn::ast::ReturnStatement`
-   **`NodeType`**: `RETURN_STATEMENT`
-   **Fields**:
    -   `value` (`std::optional<ExprPtr>`): The expression whose value is returned.

```cpp
// From include/vyn/parser/ast.hpp
namespace vyn::ast {
class ReturnStatement : public Statement {
public:
    std::optional<ExprPtr> value;

    ReturnStatement(SourceLocation loc, std::optional<ExprPtr> value);
    // ... accept, getType, toString methods ...
};
} // namespace vyn::ast
```

## 7. `BreakStatement`

Represents a break statement.

-   **C++ Class**: `vyn::ast::BreakStatement`
-   **`NodeType`**: `BREAK_STATEMENT`

```cpp
// From include/vyn/parser/ast.hpp
namespace vyn::ast {
class BreakStatement : public Statement {
public:
    BreakStatement(SourceLocation loc);
    // ... accept, getType, toString methods ...
};
} // namespace vyn::ast
```

## 8. `ContinueStatement`

Represents a continue statement.

-   **C++ Class**: `vyn::ast::ContinueStatement`
-   **`NodeType`**: `CONTINUE_STATEMENT`

```cpp
// From include/vyn/parser/ast.hpp
namespace vyn::ast {
class ContinueStatement : public Statement {
public:
    ContinueStatement(SourceLocation loc);
    // ... accept, getType, toString methods ...
};
} // namespace vyn::ast
```

## 9. `ThrowStatement`

Represents a statement that throws an exception or error.

-   **C++ Class**: `vyn::ast::ThrowStatement`
-   **`NodeType`**: `THROW_STATEMENT`
-   **Fields**:
    -   `expression` (`ExprPtr`): The expression evaluating to the error object or value to be thrown.

```cpp
// From include/vyn/parser/ast.hpp
namespace vyn::ast {
class ThrowStatement : public Statement {
public:
    ExprPtr expression;

    ThrowStatement(SourceLocation loc, ExprPtr expression);
    // ... accept, getType, toString methods ...
};
} // namespace vyn::ast
```

## 10. `ScopedStatement`

Represents a statement that explicitly creates a new lexical scope.

-   **C++ Class**: `vyn::ast::ScopedStatement`
-   **`NodeType`**: `SCOPED_STATEMENT`
-   **Fields**:
    -   `body` (`StmtPtr`): The statement (typically a `BlockStatement`) executed within the new scope.

```cpp
// From include/vyn/parser/ast.hpp
namespace vyn::ast {
class ScopedStatement : public Statement {
public:
    StmtPtr body; // Likely a BlockStatement

    ScopedStatement(SourceLocation loc, StmtPtr body);
    // ... accept, getType, toString methods ...
};
} // namespace vyn::ast
```

## 11. `TryStatement`

Represents a try-catch(-finally) block for exception handling.

-   **C++ Class**: `vyn::ast::TryStatement`
-   **`NodeType`**: `TRY_STATEMENT`
-   **Fields**:
    -   `tryBlock` (`BlockStatementPtr`): The block of code to try.
    -   `catchVariable` (`std::optional<IdentifierPtr>`): The variable to bind the caught exception to.
    -   `catchType` (`std::optional<TypeNodePtr>`): The type of exception to catch.
    -   `catchBlock` (`std::optional<BlockStatementPtr>`): The block of code to execute if an exception is caught.
    -   `finallyBlock` (`std::optional<BlockStatementPtr>`): The block of code that is always executed.

```cpp
// From include/vyn/parser/ast.hpp
namespace vyn::ast {
class TryStatement : public Statement {
public:
    BlockStatementPtr tryBlock;
    std::optional<IdentifierPtr> catchVariable;
    std::optional<TypeNodePtr> catchType;
    std::optional<BlockStatementPtr> catchBlock;
    std::optional<BlockStatementPtr> finallyBlock;

    TryStatement(SourceLocation loc, BlockStatementPtr tryBlock,
                 std::optional<IdentifierPtr> catchVariable,
                 std::optional<TypeNodePtr> catchType,
                 std::optional<BlockStatementPtr> catchBlock,
                 std::optional<BlockStatementPtr> finallyBlock);
    // ... accept, getType, toString methods ...
};
} // namespace vyn::ast
```

## 10. `UnsafeBlockStatement`

Represents an unsafe code block where memory operations are allowed.

-   **C++ Class**: `vyn::ast::UnsafeBlockStatement`
-   **`NodeType`**: `UNSAFE_BLOCK_STATEMENT`
-   **Fields**:
    -   `body` (`BlockStatementPtr`): A pointer to the block statement containing the code in the unsafe block.

```cpp
// From include/vyn/parser/ast.hpp
namespace vyn::ast {
class UnsafeBlockStatement : public Statement {
public:
    BlockStatementPtr body;

    UnsafeBlockStatement(SourceLocation loc, BlockStatementPtr body);
    // ... accept, getType, toString methods ...
};
} // namespace vyn::ast
```

Unsafe blocks are used to contain low-level memory operations that could be unsafe if used incorrectly. Within an unsafe block, you can:

1. Create raw pointers with `loc<T>(expr)`
2. Dereference pointers with `at(ptr)`
3. Convert between pointer types with `from<loc<T>>(addr)`

Example:
```vyn
unsafe {
    var<loc<Int>> p = loc(x);
    at(p) = 99;  // Modify the value at the pointer location
    var<Int> q = at(p); // Read the value at the pointer location
}
```

These operations are unsafe because they bypass Vyn's memory safety guarantees, allowing for potential errors like null pointer dereferences, dangling pointers, and memory corruption.

*Note: `PatternAssignmentStatement` is mentioned in `AST_Roadmap.md` but not currently present in `vyn/parser/ast.hpp`'s statement definitions. It will be added here once implemented.*

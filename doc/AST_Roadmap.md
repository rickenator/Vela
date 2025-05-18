<!-- filepath: /home/rick/Projects/Vyn/doc/AST_Roadmap.md -->
<!-- Test modification May 18 -->
# Vyn AST: Roadmap and Planned Features

This document outlines planned features, future extensions, and areas for improvement for the Vyn Abstract Syntax Tree (AST). It incorporates items previously marked as "planned" in the original `AST.md` and addresses review suggestions related to future development (like Suggestion 10).

## 1. Planned AST Node Types

This section consolidates nodes that were previously mentioned as "planned" or are logical extensions based on language feature goals. For each, consideration will be given to their structure, visitor integration, and impact on parsing and semantic analysis.

### 1.1. Pattern Matching Nodes

*   **Status:** Conceptualized, details in `AST_Patterns.md`.
*   **Description:** A suite of nodes to represent various patterns used in `match` expressions, `let` bindings, and function parameters.
*   **Key Nodes (see `AST_Patterns.md` for details):**
    *   `PatternNode` (base)
    *   `IdentifierPattern`
    *   `LiteralPattern`
    *   `TuplePattern`
    *   `StructPattern`
    *   `EnumVariantPattern`
    *   `WildcardPattern`
    *   `RangePattern`
    *   `OrPattern`
*   **Action:** Implement these nodes in `ast.hpp`/`ast.cpp`, update `NodeType`, and integrate with the `Visitor` pattern.

### 1.2. Asynchronous Programming Nodes

*   **Status:** Planned.
*   **Description:** Nodes to support `async` functions and `await` expressions.
*   **Key Nodes:**
    *   `AsyncFunctionDeclaration` (or a flag on `FunctionDeclaration`): To mark a function as asynchronous.
    *   `AwaitExpression`: Represents an `await` operation on a future or promise.
        *   `PExpression operand`: The expression being awaited.
*   **Action:** Define these nodes, integrate with `Visitor`, and update parser to handle `async`/`await` keywords.

### 1.3. Module System Nodes

*   **Status:** Partially implemented through `ImportDeclaration`, but needs expansion.
*   **Description:** Nodes to fully support Vyn's module system, including module declarations and more complex import/export capabilities.
*   **Key Nodes:**
    *   `ModuleDeclaration`: (Potentially at the root of an AST file or as a special node) Declares the current file as a module with a specific path.
    *   `ExportDeclaration`: Wraps a declaration (function, struct, enum, const) to mark it as exported. Could also be a flag on existing declaration nodes.
    *   Enhanced `ImportDeclaration`: Support for selective imports (`import my_module::{foo, bar};`), aliasing (`import my_module::foo as FooBar;`).
*   **Action:** Refine module-related AST nodes and parsing logic.

### 1.4. Error Handling Nodes (Beyond Basic `TryStatement`)

*   **Status:** `TryStatement` exists. `ErrorNode` discussed in `AST_Design_Considerations.md`.
*   **Description:** More refined nodes for error handling and propagation.
*   **Key Nodes:**
    *   `ErrorNode` (or `ErrorExpression`, `ErrorStatement`): To represent constructs that failed to parse correctly but where recovery was possible. This helps later stages understand that part of the tree is invalid.
    *   `ThrowStatement`: Represents a `throw` statement for explicit error propagation.
        *   `PExpression errorExpression`: The expression evaluating to the error object to be thrown.
    *   Potentially a `ResultTypeNode` or similar if the language has explicit result types for error handling.
*   **Action:** Implement `ErrorNode` as discussed. Define and implement `ThrowStatement`. Evaluate need for other error-specific AST nodes as language features solidify.

### 1.5. Metaprogramming / Macros

*   **Status:** Future consideration.
*   **Description:** If Vyn incorporates procedural or declarative macros, the AST will need to represent macro definitions and invocations.
*   **Key Nodes:**
    *   `MacroDefinition`
    *   `MacroInvocationExpression`
    *   `MacroInvocationStatement`
*   **Action:** Design these if/when macros become a feature. This is a significant undertaking.

### 1.6. Advanced Type System Nodes

*   **Status:** `TypeNode` is somewhat generic. `AST_Types.md` and `AST_Design_Considerations.md` discuss improvements. The C++ implementation in `vyn/parser/ast.hpp` includes `BasicTypeNode`, `PointerTypeNode`, `SizedArrayTypeNode`, `UnsizedArrayTypeNode`, `TupleTypeNode`, `FunctionTypeNode`, and `GenericTypeNode`. This section outlines further specializations or distinct types.
*   **Description:** More specialized `TypeNode` subclasses for complex types not yet covered or needing distinct representation.
*   **Key Nodes (potential subclasses of `TypeNode` or distinct type nodes):**
    *   `SliceTypeNode`: Represents a slice type (e.g., `&[T]`), potentially distinct from `UnsizedArrayTypeNode` if semantics differ (e.g., fat pointer vs. simple pointer to unsized data).
        *   `TypeNodePtr elementType`
    *   `SelfTypeNode`: Represents the `Self` keyword used as a type, typically within trait or impl blocks, referring to the implementing type.
*   **Action:** Refine `TypeNode` hierarchy as discussed in `AST_Types.md` and `AST_Design_Considerations.md`, and implement these additional specialized type nodes as language features require.

### 1.7. Conditional Expressions (Ternary Operations)

*   **Status:** Future consideration.
*   **Description:** Nodes to support C-style ternary conditional expressions.
*   **Key Nodes:**
    *   `TernaryExpression`: Represents an expression like `condition ? then_expr : else_expr`.
        *   `PExpression condition`: The condition to evaluate.
        *   `PExpression thenExpression`: The expression to evaluate if the condition is true.
        *   `PExpression elseExpression`: The expression to evaluate if the condition is false.
*   **Action:** Define the `TernaryExpression` node, integrate it with the `Visitor` pattern, and update the parser to handle the syntax.

### 1.8. Pattern-Based Assignment Statements

*   **Status:** Future consideration.
*   **Description:** Statements that allow assigning to a pattern, enabling destructuring assignment outside of initial declarations (e.g., `(a, b) = some_function_returning_tuple();`).
*   **Key Nodes:**
    *   `PatternAssignmentStatement`: Represents an assignment where the left-hand side is a pattern.
        *   `PPattern pattern`: The pattern to assign to (referencing nodes from `AST_Patterns.md`).
        *   `PExpression value`: The expression whose value is being assigned and destructured.
*   **Action:** Define the `PatternAssignmentStatement` node, integrate with the `Visitor`, and update the parser. This will likely leverage existing pattern AST nodes.

### 1.9. Trait System Nodes

*   **Status:** Planned.
*   **Description:** Nodes to support trait definitions, method signatures within traits, and trait bounds for generics. These are fundamental for Vyn's polymorphism and interface system. `ImplDeclaration` for implementing traits/interfaces on types is already part of the AST.
*   **Key Nodes:**
    *   `TraitDeclaration`: Defines a new trait.
        *   `IdentifierPtr name`
        *   `std::vector<GenericParameterPtr> genericParameters`
        *   `std::vector<MethodSignaturePtr> methodSignatures` // Pointer to MethodSignature nodes
    *   `MethodSignature`: Declares a method signature within a trait (or potentially an `impl` block if freestanding method signatures are allowed there).
        *   `IdentifierPtr name`
        *   `std::vector<FunctionParameterPtr> parameters` // Reusing FunctionParameter from FunctionDeclaration
        *   `TypeNodePtr returnType`
        *   `std::vector<GenericParameterPtr> genericParameters` // For generic methods
        *   *(Consider flags: `isConst`, `isAsync`, `isStatic` etc.)*
    *   `TraitBoundNode`: Specifies that a generic type parameter must implement one or more traits (used in generic parameter definitions or where clauses).
        *   `TypeNodePtr constrainedType` // e.g., a `GenericTypeNode` representing 'T'
        *   `std::vector<TypeNodePtr> bounds` // e.g., `GenericTypeNode`s representing trait names like 'Printable', 'Iterable'
*   **Action:** Design and implement these nodes in `ast.hpp`/`ast.cpp`. Update `NodeType`, integrate with the `Visitor` pattern, and develop parsing rules for trait definitions and usage.

## 2. AST Infrastructure and Tooling Enhancements

### 2.1. Parent Pointers

*   **Status:** Discussed in `AST_Design_Considerations.md`.
*   **Description:** Consider adding non-owning parent pointers to `Node` to simplify upward tree traversal for some analyses.
*   **Action:** Evaluate trade-offs. If implemented, ensure no ownership cycles.

### 2.2. Enhanced Visitor Support

*   **Status:** Discussed in `AST_Design_Considerations.md`.
*   **Description:** Introduce a `BaseVisitor` with default implementations to reduce boilerplate for visitors that only care about a subset of node types.
*   **Action:** Implement `BaseVisitor` and update existing visitors if beneficial.

### 2.3. Source Location Tracking

*   **Status:** `SourceLocation` exists on `Node`.
*   **Description:** Ensure all nodes, including newly added ones, correctly and consistently store their source location (start and end). This is crucial for error reporting and tooling.
*   **Action:** Ongoing diligence during implementation of new nodes.

### 2.4. AST Serialization/Deserialization

*   **Status:** Not currently implemented.
*   **Description:** Ability to serialize the AST (e.g., to JSON, binary format) and deserialize it. Useful for debugging, caching, or inter-tool communication (e.g., language server).
*   **Action:** Plan and implement a serialization format if deemed necessary.

### 2.5. AST Pretty Printer

*   **Status:** `toString()` methods exist for debugging.
*   **Description:** A more robust AST pretty printer that can reconstruct Vyn-like source code from the AST. Useful for debugging and code generation/transformation tools.
*   **Action:** Enhance `toString()` or create a dedicated pretty-printing visitor.

## 3. Documentation and Consistency (Review Suggestion 10)

**Review Suggestion 10: Merge "planned" nodes into a roadmap section.**
> "The AST.md document lists several node types as "Planned" (e.g., `AsyncFunctionDeclaration`, `AwaitExpression`, `PatternNode` and its variants). It would be clearer to group these into a dedicated "Roadmap" or "Future Extensions" section, or even a separate document, rather than interspersing them with currently implemented nodes. This helps distinguish the current state from future aspirations."

*   **Status:** This document (`AST_Roadmap.md`) directly addresses this suggestion.
*   **Action:** Maintain this document as the central place for planned AST changes. Ensure that other AST documents (`AST_Core.md`, `AST_Expressions.md`, etc.) accurately reflect the *current* state of implemented AST nodes.

## 4. Long-Term Considerations

*   **AST Stability:** As the language evolves, aim for AST stability where possible, but be prepared to version or adapt the AST if significant language changes occur.
*   **Performance:** For large codebases, AST construction and traversal performance can be critical. Re-evaluate choices like `std::shared_ptr` vs `std::unique_ptr` and consider arena allocation (see `AST_Design_Considerations.md`).
*   **IDE Integration:** Design the AST to be amenable to consumption by language servers for features like code completion, hover information, and refactoring.

This roadmap provides a forward-looking view for the Vyn AST. Priorities and specific designs may evolve as the language implementation progresses.

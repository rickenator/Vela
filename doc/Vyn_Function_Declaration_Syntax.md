
# Vyn Function Declaration Syntax

This document defines the updated function declaration syntax in Vyn, aligning with the `<T>`-first style and requiring a mandatory arrow (`->`) to separate the signature from the body. Braces around the body are optional for single-expression functions.

---

## 1. Grammar (EBNF)

```ebnf
FunctionDecl ::= "fn" "<" Type ">" Identifier "(" ParamList ")" "->" Body

ParamList    ::= [ Param { "," Param } ]
// Standard parameter syntax
Param        ::= ("var" | "const") "<" Type ">" Identifier
// Relaxed parameter syntax (shorthand)
               | [ "const" ] Type Identifier

Body         ::= Block
               | Expression

Block        ::= "{" Statement* [ Expression ] "}"
Expression   ::= <any single Vyn expression>  // no braces required
```

- **Return type** is declared in `<Type>` immediately after `fn`.
- **Parameters** may use either:
  - Standard syntax: `var<T>` or `const<T>` before the parameter name
  - Shorthand syntax: `Type name` (implicitly mutable) or `const Type name` (immutable)
- **`->`** is **mandatory**: it clearly demarcates the end of the signature and the start of the body.
- **Braces `{}`** around the body are optional only when the body is a single expression.

---

## 2. Examples

### 2.1 Block Body (with braces)

```vyn
class Node {
  var<Bool> is_leaf

  // Constructor with a block body
  fn<Node> new(const<Bool> is_leaf_param) -> {
    Node { is_leaf: is_leaf_param }
  }
}
```

### 2.2 Single-Expression Body (braces optional)

```vyn
// A concise function doubling its input
fn<Int> double(var<Int> x) -> x * 2
```

```vyn
// Reading a value from a shared config
fn<Bool> is_debug(var<our<Config>> cfg) -> view(cfg).debug
```

### 2.3 Using Relaxed Parameter Syntax (shorthand)

```vyn
// Using shorthand syntax (equivalent to var<Int> x, var<Float> y)
fn<Float> add(Int x, Float y) -> x + y

// Mixed standard and shorthand syntax
fn<String> format(var<String> prefix, const Int value) -> prefix + value.to_string()

// Complex types with shorthand syntax
fn<Void> process(my<Task> task, const their<Data const> data) -> {
    task.run(data)
}

// With generics
fn<T> first_element<T>(my<[T]> array) -> array[0]
```

---

## 3. Rationale

1. **Uniformity**: mirrors `<T>` usage in `var<T>`, `const<T>`, intrinsics, and generics.  
2. **Clarity**: mandatory `->` prevents ambiguity when braces are omitted.  
3. **Conciseness**: single-expression bodies can omit braces, reducing noise.  
4. **Consistency**: keeps the return type in one place (`<Type>`) and the arrow purely as a separator.

---

## 4. Notes

- Multi-line or statement-rich bodies should always use braces:
  ```vyn
  fn<Void> perform(var<Int> x) -> {
    process(x)
    cleanup()
    return
  }
  ```
- The function’s return type is always taken from `<Type>`; no `-> Type` form is used.

# Vyn Intrinsics & Core Syntax

This document covers Vyn’s built-in intrinsics, variable declaration syntax (including type inference), and function declaration syntax, all aligned with the `<T>`‑first style.

---

## 1. Variable & Constant Declarations

Vyn uses two primary declaration forms:

```ebnf
Declaration ::= "var" "<" Type ">" Identifier [ "=" Expression ]
              | "var" "auto" Identifier "=" Expression
              | "const" "<" Type ">" Identifier [ "=" Expression ]
```

- **`var<T> name [= expr]`**  
  Mutable binding of type `T`.  
- **`var auto name = expr`**  
  Mutable binding with type `T` inferred from `expr`.  
- **`const<T> name [= expr]`**  
  Immutable binding of type `T`.  

> **Note:** `const auto` is not supported; use explicit `const<T>` for immutable bindings.

### Examples

```vyn
var<Int> x             // mutable Int, uninitialized
var<Int> y = 42        // mutable Int, initialized

var auto tree = BTree<Int, String, 3>::new()
// `tree` inferred as BTree<Int, String, 3>

const<String> s = "hello"  // immutable String
```

Ownership-aware declarations:

```vyn
var<my<Task>> task = my<Task>(Task { id: 1, payload: "foo" })
var<our<Config>> cfg  = our<Config>(Config { debug: true })
var<their<Foo>> b     = their<Foo>(owner)       // mutable borrow
var<their<Foo const>> v = their<Foo const>(owner)  // immutable borrow
```

Pointer declarations (inside `unsafe`):

```vyn
unsafe {
  var<loc<Int>> p = loc(x)
  at(p) = 99
}
```

---

## 2. Function Declaration Syntax

Functions follow a `<ReturnType>`‑first style, with a mandatory `->` separator. Braces are optional for single-expression bodies.

```ebnf
FunctionDecl ::= "fn" "<" Type ">" Identifier "(" ParamList ")" "->" Body

ParamList    ::= [ Param { "," Param } ]
Param        ::= ("var" | "const") "<" Type ">" Identifier

Body         ::= Block
               | Expression

Block        ::= "{" Statement* [ Expression ] "}"
Expression   ::= <any single Vyn expression>
```

- **Return type**: declared in `<Type>` after `fn`.  
- **Parameters**: `var<T>` or `const<T>` before each name.  
- **`->`**: mandatory separator between signature and body.  
- **Braces** `{}` optional only for single-expression bodies.

### Examples

```vyn
class Node {
  var<Bool> is_leaf

  // Constructor with block body
  fn<Node> new(const<Bool> is_leaf_param) -> {
    Node { is_leaf: is_leaf_param }
  }

  // Concise single-expression function
  fn<Int> double(var<Int> x) -> x * 2
}
```

---

## 3. Intrinsics Overview

Intrinsics are compiler-handled operations, split into **stable** (Sections 4–6) and **proposed/experimental** (Section 7).

### 3.1 Reserved Keywords

These names are reserved and cannot be used as identifiers:

- **Declarations**: `var`, `auto`, `const`  
- **Ownership & borrowing**: `my`, `our`, `their`, `borrow`, `view`  
- **Pointer & address**: `loc`, `at`, `addr`, `from`  
- **Type metadata**: `sizeof`, `alignof`, `offsetof`  
- **Visibility/macros**: `import`, `smuggle`, `share`

---

## 4. Ownership Intrinsics

### 4.1 Core Wrappers

```vyn
fn my<T>(value: T) -> my<T>
fn our<T>(value: T) -> our<T>
fn their<T>(owner: my<T> | our<T>) -> their<T>
fn their<T const>(owner: my<T> | our<T>) -> their<T const>
```

- **`my<T>(value)`**: wrap `value` in a unique-owned `my<T>`.  
- **`our<T>(value)`**: wrap `value` in a shared-owned `our<T>`.  
- **`their<T>(owner)`**: create a mutable borrow `their<T>` of `owner`.  
- **`their<T const>(owner)`**: create an immutable borrow `their<T const>` of `owner`.

### 4.2 Shorthand Borrowing

For convenience, the compiler provides **inferred** shorthand intrinsics:

```vyn
fn borrow(owner) -> their<T>
fn view(owner)   -> their<T const>
```

- **`borrow(owner)`** infers `T` from `owner` and returns `their<T>`.  
- **`view(owner)`** infers `T` from `owner` and returns `their<T const>`.

---

## 5. Memory Intrinsics (`unsafe` required)

```vyn
unsafe fn loc<T>(expr: T) -> loc<T>
unsafe fn at<T>(pointer: loc<T>) -> T
unsafe fn addr<T>(pointer: loc<T>) -> Int64
unsafe fn from<P>(addr: Int64) -> P
```

- **`loc<T>(expr)`**: address‑of a value → `loc<T>`.  
- **`at<T>(pointer)`**: dereference pointer → `T` (l-value/r-value).  
- **`addr<T>(pointer)`**: pointer → raw `Int64`.  
- **`from<P>(addr)`**: raw `Int64` → pointer `P`.

---

## 6. Type Metadata Intrinsics (safe)

```vyn
fn sizeof<T>() -> UInt
fn alignof<T>() -> UInt
fn offsetof<T>(field: identifier) -> UInt
```

- **`sizeof<T>()`**: size of `T` in bytes.  
- **`alignof<T>()`**: alignment of `T`.  
- **`offsetof<T>(field)`**: byte offset of `field` in `T`.

---

## 7. Proposed/Experimental Intrinsics

```vyn
fn offset<T>(ptr: loc<T>, count: Int) -> loc<T>
fn is_null<T>(ptr: loc<T>) -> Bool
fn aligned<T>(ptr: loc<T>) -> Bool
fn mem_copy(dst: loc<UInt8>, src: loc<UInt8>, n: UInt) -> Void
fn mem_set(ptr: loc<UInt8>, value: UInt8, n: UInt) -> Void
// Atomic & volatile operations
```

---

## 8. Usage Guidelines

1. **Declarations**: choose explicit (`var<T>`) or inferred (`var auto`).  
2. **Functions**: return in `<Type>`, arrow mandatory, braces optional for single expressions.  
3. **Ownership**: use `my<T>`, `our<T>`, `their<T>`, with optional `borrow()`/`view()` shorthand.  
4. **Intrinsics**: memory ops only in `unsafe`, metadata always safe.  
5. **Stability**: Sections 4–6 are stable; Section 7 is experimental.
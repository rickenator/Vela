# (Proposed) Vyn Declaration Syntax

Vyn uses a gen---

## Alternative Declaration Shorthand: `Type name`

Vyn treats bindings as mutable by default. For common local declarations, you can drop the angle-bracket form and write `Type name` instead of `var<Type> name` or `<Type> name`.

```ebnf
Declaration ::= [ "const" ] Type Identifier [ "=" Expression ]
              | "auto" Identifier "=" Expression
```

- **`Type name [= expr]`**  
  Mutable binding of `name` with type `Type`.  
- **`const Type name [= expr]`**  
  Immutable binding of `name` with type `Type`.  
- **`auto name = expr`**  
  Mutable binding with inferred type from `expr`.

> Note: this form is for locals and parameters only. Top-level or generic declarations still use `<T>` syntax.

---

## Examples

```vyn
// Mutable by default
Int   x = 0             // ⇔ var<Int> x = 0
Float ratio             // ⇔ var<Float> ratio

// Immutable
const String msg = "hi" // ⇔ const<String> msg = "hi"

// Type inference
auto count = 42         // inferred as Int

// Ownership wrappers (explicit form)
my<Task>   task    = my<Task>(Task { id: 1 })
our<Config> cfg    = our<Config>(Config { debug: true })
their<Foo> b       = borrow(owner)
```

---

## Rationale for Original Syntax

- **Consistency**: matches the `<T>` style used in intrinsics and generics.
- **Clarity**: the variable's type is front-and-center, reducing the visual noise of colons.
- **Uniformity**: single pattern for all bindings (`var<T>` and `const<T>`).

## Rationale for Shorthand Syntax

1. **Brevity**: minimal boilerplate for the common case.  
2. **Clarity**: `const` flags immutability explicitly.  
3. **Familiarity**: resembles C-style `int x;` but within Vyn's type-first mindset.  
4. **Flexibility**: use `<T> name` when emphasizing generics, or `Type name` when brevity matters.

The standard `var<Type>` and `const<Type>` syntax remains the canonical form for all advanced use cases.e declaration for variables and constants to align with intrinsic notation:

```ebnf
Declaration ::= ( "var" | "const" ) "<" Type ">" Identifier [ "=" Expression ]
```

- **`var<T> name [= expr]`**  
  Mutable binding of type `T`.
- **`const<T> name [= expr]`**  
  Immutable binding of type `T`.

Type annotations are **mandatory** in this form. Initialization (`= expr`) is optional.

---

## Examples

### Mutable variable

```vyn
var<Int> x          // `x` is a mutable `Int`, uninitialized (must be assigned before use)
var<Int> y = 42     // `y` is a mutable `Int`, initialized to 42

unsafe {
  var<loc<Int>> p = loc(x)     // pointer to `x`
  at(p) = 99
}
```

### Immutable constant

```vyn
const<String> s = "hello"   // `s` is an immutable `String`
const<Bool> flag           // `flag` is an immutable `Bool`, must be set in a `--init--` section
```

### Shared ownership

```vyn
var<our<Config>> cfg = our(Config { debug: true })
```

### Borrowing

```vyn
var<their<Foo>> b = borrow(owner)
var<their<Foo const>> v = view(owner)
```

---

## Rationale

- **Consistency**: matches the `<T>` style used in intrinsics and generics.
- **Clarity**: the variable’s type is front-and-center, reducing the visual noise of colons.
- **Uniformity**: single pattern for all bindings (`var<T>` and `const<T>`).

This syntax replaces the older `var name: Type [= expr]` form for all new Vyn code.
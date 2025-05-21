# Auto-Serialization & Runner Behavior in Vyn

**Status:** Approved for roadmap, planned for future implementation

## 1. Overview

Vyn's `fn main() -> T` can be zero-boilerplate by auto-serializing any "pure data" return value to JSON. The compiler/runtime will:

1. Call `main()`
2. Inspect its return type `T`:
   - If `T` implements `ToJson`, print its JSON form
   - If `T` is `Int`, use it as the process exit code
   - If `T` implements `ToString`, print its string form
   - Otherwise, emit a compile-time error

This enables scripts and API-style binaries to return structured data without manual `println` calls.

## 2. Built-in Auto-Derive

- **Primitives** (`Int`, `Float`, `Bool`, `String`) have built-in `to_json` implementations
- **Collections** (`[T]`, `Map<K,V>`, `Maybe<T>`, etc.) auto-serialize if their element types do
- **Structs & Classes**: The compiler auto-derives a `ToJson` implementation behind the scenes—no explicit derive attribute required

```vyn
struct User {
  var id: Int
  var name: String
}

fn main() -> User {
  return User { id: 1, name: "Rick" }
}
// Runner prints: {"id":1,"name":"Rick"}
```

## 3. Customization & Opt-Out

When you need to adjust the JSON shape:

```vyn
struct Secret {
  #[jsonIgnore]
  var token: String        // omitted from JSON output

  #[jsonName="user_name"]
  var name: String         // renamed key in JSON
}

// Hand-roll if you need full control:
impl ToJson for Secret {
  fn to_json(self) -> String {
    // custom serialization logic
  }
}
```

- **`#[jsonIgnore]`** — Skip a field in serialization
- **`#[jsonName="..."]`** — Use custom key name in JSON output
- **Manual `impl ToJson`** — Overrides the auto-derived implementation

## 4. Non-Serializable Types

Some types (functions, raw pointers, threads, etc.) cannot sensibly map to JSON. If `main() -> T` where `T` contains such a type, the compiler will:

- Reject the program with a clear error message
- Require you to convert or extract serializable data before returning

## 5. Runner Behavior

**Default mode:**
```bash
vyn run program.vyn
```
Calls `main()`, auto-prints JSON or uses return value as exit code.

**Explicit JSON flag (future):**
```bash
vyn run --json program.vyn
```
Forces JSON serialization even for `ToString` types.

**Error handling:**
- If `main()` throws, print stack trace + exit with non-zero code
- If serialization fails, print error + exit with non-zero code

## 6. Benefits

- Zero boilerplate for data-returning scripts and services
- Consistent, predictable runner semantics across all return types
- Extensible via attributes and manual `ToJson` implementations
- Compile-time safety: non-serializable types are caught early

## 7. Implementation Considerations

- The `ToJson` trait will be part of the standard library
- Auto-derivation will be performed during semantic analysis
- The runner will need to integrate with the serialization system
- Proper error messages will guide users toward correct usage

## 8. Future Extensions

- Support for binary serialization formats like MessagePack or CBOR
- Extended attribute system for controlling serialization behavior
- Integration with the planned standard library's network and file I/O modules

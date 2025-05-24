# Bundles & Sharing in Vyn

**Status:** Approved for roadmap, planned for future implementation  
**Last Updated:** May 21, 2025

## 1. Overview

Vyn's visibility model uses **bundles** to group modules (similar to Java packages) and **share** directives to export individual symbols:

- **`bundle(...)`** declares the module's package membership
- **`share(...)`** on a declaration exports it to specific bundles (or all)
- **`import`** succeeds only if your module's bundle is in the target's `share` list
- **`smuggle`** always bypasses visibility checks

This system provides fine-grained control over symbol visibility while keeping the mechanism intuitive and explicit in the source code.

## 2. File-Level Bundle Declaration

At the top of a file, use `bundle` to state which bundles (projects/subsystems) this module belongs to:

```vyn
// src/sort/Core.vyn
bundle(sort.Core, sort.Common)
```

- **`bundle(g1, g2, ...)`**: Associates this file with bundles g1, g2, etc.
- **No bundle declaration**: Module is unbundled; everything defaults to local visibility, and the module can only import symbols it `smuggles`.

## 3. Declaration-Level Sharing

Prefix any `fn`/`struct`/`class`/`var` with `share` to export it:

```vyn
// Exported everywhere
share(all) fn global_util() { ... }

// Exported only to sort.Core & sort.Database bundles
share(sort.Core, sort.Database) fn helper() { ... }

// Private to this file's bundles
fn internal() { ... }
```

- **`share(all)`**: Visible to every module in every bundle
- **`share(g1, g2, ...)`**: Visible only to those named bundles
- **No `share`**: Private to this file (even if it has `bundle(...)`)

## 4. Imports & Smuggle

```vyn
// In a file with bundle(sort.UI):
import sort.Core      // OK if sort.Core.vyn did share(sort.UI) or share(all)
import sort.Database  // ERROR if no overlapping bundle
smuggle sort.Database // OK - always allowed
```

- **`import <ModulePath>`**: Allowed only if your file's bundle list overlaps the target's share list
- **`smuggle <ModulePath>`**: Always allowed—bypasses bundle checks (for FFI, tests, bootstrapping)

## 5. Grammar Specification

```ebnf
FileHeader      ::= [ "bundle" "(" BundleList ")" ]
BundleList      ::= "all" | Group { "," Group }
Group           ::= identifier ("." identifier)*

DeclVisibility  ::= "share" "(" BundleList ")" | ε

Declaration     ::= DeclVisibility ( "fn" | "struct" | "class" | "var" ) …

ImportStmt      ::= "import" ModulePath
                  | "smuggle" ModulePath
ModulePath      ::= identifier { "::" identifier }
```

## 6. Examples

### 6.1 Core Module

```vyn
// src/sort/Core.vyn
bundle(sort, sort.Core, sort.Common)

share(sort.Core)
fn quicksort(nums: my<[Int]>) -> my<[Int]> { ... }

fn partition(nums: my<[Int]>, pivot: Int) -> (my<[Int]>, my<[Int]>) {
  ...  // inherits bundle(sort, sort.Core, sort.Common)
}
```

### 6.2 UI Module

```vyn
// src/sort/UI.vyn
bundle(sort, sort.UI, sort.Common)

import sort.Core      // OK: sort.Core shared with `sort`
import sort.Common    // OK

share(sort.UI)
fn<String> pretty_sort(var<my<[Int]>> nums) -> {
  var<my<[Int]>> sorted = quicksort(nums)
  return "[" + sorted.map(|x| x.to_string()).join(", ") + "]"
}
```

### 6.3 External Library

```vyn
// src/net/http.vyn
bundle(net.HTTP)

share(all)
fn request(url: String) -> Response { ... }
```

```vyn
// src/app/Main.vyn
bundle(app.Main)

import net.http      // OK: net.http shared with `all`
```

### 6.4 Math Library Example

```vyn
// File: src/math/Arithmetic.vyn
bundle(math)               // this module is in bundle "math"

// Exported only to modules that also declare bundle(math)
share(math) fn add(a: Int, b: Int) -> Int {
  return a + b
}

// Fully public to everyone
share(all) fn version() -> String {
  return "Arithmetic v1.0"
}

// No share → private helper
fn validate(a: Int, b: Int) { ... }
```

Consuming the library:

```vyn
// File: src/app/Main.vyn
bundle(app.Main)           // your app's bundle

import math.Arithmetic     // OK: Arithmetic.vyn shared with bundle(math) or all

fn main() -> Int {
  println("Lib version: {}", version())   // public
  let result = add(2,3)                   // shared with math-bundle
  return result
}
```

Notes:
- `share(math)` on `add` means only modules that declare `bundle(math)` can import it
- `share(all)` on `version` makes it universally available without needing `bundle(math)`
- Anything without `share` remains private to `Arithmetic.vyn`

## 7. Benefits

- **Modular packaging**: Bundle groups modules by project or subsystem
- **Fine-grained export**: Share controls which bundles see each symbol
- **One-way visibility**: Importers opt in to library bundles; libraries never import app bundles
- **No extra manifests**: Grouping and export live in source code
- **Fail-fast**: Illegal imports caught at compile time, preventing unintended dependencies

## 8. Implementation Considerations

- Bundle declarations are processed during parsing and semantic analysis
- Visibility checks are performed during import resolution
- Each module maintains a set of bundles it belongs to
- Each declaration keeps a set of bundles it's shared with
- Import checking compares the importing module's bundles against the target symbol's share list
- When no `share` is specified, the declaration inherits visibility from its file's bundles

## 9. Future Extensions

- **Bundle inheritance**: Expressing parent-child relationships between bundles
- **Bundle configurations**: Managing bundle hierarchies in projects with many modules
- **IDE integration**: Visualizing bundle dependencies and highlighting potential visibility issues

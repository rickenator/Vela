# Vyn Intrinsics

This document describes the intrinsic functions and operations in the Vyn programming language. Intrinsics are special functions that are recognized and handled directly by the compiler, often providing functionality that cannot be expressed in standard Vyn code.

## 1. Overview

Intrinsics in Vyn serve several purposes:

1. **Memory Operations**: Low-level memory manipulation (`loc`, `at`, `from`)
2. **Raw Performance**: Operations that need direct mapping to hardware instructions
3. **Special Semantics**: Operations with behavior that can't be represented in regular Vyn code

## 1.1 Reserved Identifiers

The following identifiers are reserved for intrinsic functions and should not be used as variable, function, or parameter names:

- `addr` - Used for the address-of intrinsic function
- `at` - Used for pointer dereferencing
- `loc` - Used for pointer creation
- `from` - Used for pointer type conversion

Using these names as variables or function names can lead to ambiguity and compilation errors, especially when they appear in contexts where the compiler might interpret them as intrinsic function calls.

## 2. Memory Intrinsics

These intrinsics provide low-level memory manipulation capabilities. They must be used within unsafe blocks.

### 2.1. `loc<T>(expr)`

Creates a pointer to a variable.

- **Syntax**: `loc(expr)` or `loc<T>(expr)`
- **Return Type**: `loc<T>` where `T` is the type of `expr`
- **AST Representation**:
  - Simple case: `CallExpression` with identifier "loc"
  - Generic case: `ConstructionExpression` with a `GenericInstanceTypeNode` for "loc"
- **LLVM Implementation**: Generates an LLVM instruction to compute the address of the argument

Example:
```vyn
var x: Int = 42;
unsafe {
    var p: loc<Int> = loc(x);  // Points to x
}
```

### 2.2. `at(pointer)`

Dereferences a pointer for reading or writing.

- **Syntax**: `at(pointer)`
- **Return Type**: The pointee type of `pointer`
- **AST Representation**: `CallExpression` with identifier "at"
- **LLVM Implementation**:
  - When used on RHS: Generates an LLVM load instruction
  - When used on LHS: Returns the pointer itself, allowing a store operation

Examples:
```vyn
unsafe {
    var p: loc<Int> = loc(x);
    var y: Int = at(p);  // Reading (load)
    at(p) = 99;         // Writing (store)
}
```

### 2.3. `from<loc<T>>(expr)`

Converts between pointer types or from integer to a typed pointer.

- **Syntax**: `from<loc<T>>(expr)`
- **Return Type**: `loc<T>`
- **AST Representation**: `ConstructionExpression` with a `GenericInstanceTypeNode` for "from"
- **LLVM Implementation**: Generates appropriate LLVM pointer cast or integer-to-pointer conversion

Example:
```vyn
unsafe {
    var addr: Int = 0x12345678;
    var p: loc<Int> = from<loc<Int>>(addr);
}
```

## 3. Future Intrinsics (Planned)

These intrinsics are planned for future implementation:

### 3.1. `sizeof<T>()`

Returns the size of a type in bytes.

- **Syntax**: `sizeof<T>()`
- **Return Type**: `UInt`

Example:
```vyn
var size: UInt = sizeof<Int>();  // Returns size of Int in bytes
```

### 3.2. `alignof<T>()`

Returns the alignment requirement of a type in bytes.

- **Syntax**: `alignof<T>()`
- **Return Type**: `UInt`

Example:
```vyn
var alignment: UInt = alignof<Double>();
```

### 3.3. `offsetof<T>(field)`

Returns the byte offset of a field within a struct.

- **Syntax**: `offsetof<MyStruct>("fieldName")`
- **Return Type**: `UInt`

Example:
```vyn
struct Point {
    var x: Int;
    var y: Int;
}

var offset: UInt = offsetof<Point>("y");  // Returns offset of y field
```

## 4. Implementation Notes

Intrinsics are implemented in the compiler as special cases in the AST visitor implementation. During code generation, intrinsic calls are recognized and expanded into the appropriate LLVM IR instructions rather than generating a regular function call.

The intrinsic implementation can be found in:
- AST handling: `src/vre/llvm/cgen_expr.cpp`
- Semantic analysis: `src/vre/semantic.cpp`

## 5. Usage Guidelines

- **Memory intrinsics** should only be used within unsafe blocks
- Intrinsics should be considered implementation details that might change
- When possible, prefer safe abstractions provided by the standard library over direct intrinsic use

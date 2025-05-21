# Vyn Memory Operations

This document describes Vyn's memory model and the operations available for low-level memory manipulation. Vyn follows a hybrid approach to memory management, with safe memory operations by default and explicit unsafe operations when needed.

## 1. Memory Safety Philosophy

Vyn's memory model is designed with these principles:

- **Safe by Default**: Normal Vyn code operates with memory safety guarantees
- **Explicit Unsafety**: Unsafe operations must be contained within `unsafe` blocks
- **Minimal Unsafe Surface**: The language minimizes the number of unsafe operations needed
- **Clear Intent**: Memory operations use clear syntax that indicates their purpose

## 2. Pointer Types

### 2.1. `loc<T>` Type

The `loc<T>` type represents a raw pointer to memory containing a value of type `T`.

- **Syntax**: `loc<T>`
- **AST Representation**: `GenericInstanceTypeNode` with a base type of "loc" and a type argument of `T`
- **Safety**: Always considered unsafe to dereference or modify

Example:
```vyn
var x: Int = 42;
var p: loc<Int>; // Declares a pointer to Int

unsafe {
    p = loc(x);  // Sets p to point to x
}
```

### 2.2. Memory Addresses

Raw memory addresses are represented using integers. The size depends on the target architecture:

- **32-bit systems**: `Int` (typically i32)
- **64-bit systems**: `Int` (typically i64)

## 3. Memory Operations

### 3.1. Creating Pointers with `loc(variable)`

The `loc()` operation creates a pointer to a variable.

- **Syntax**: `loc(expr)`
- **Return Type**: `loc<T>` where `T` is the type of `expr`
- **AST Representation**:
  - `ConstructionExpression` with type "loc" and the target expression as an argument
  - Or `CallExpression` with identifier "loc" and the target expression as an argument
- **Safety**: Must be used within an `unsafe` block

Example:
```vyn
var x: Int = 42;
unsafe {
    var p: loc<Int> = loc(x);
}
```

### 3.2. Dereferencing Pointers with `at(pointer)`

The `at()` operation accesses the value at a pointer's location.

- **Syntax**: `at(pointer)`
- **AST Representation**:
  - `CallExpression` with identifier "at" and the pointer as an argument
  - Or `ConstructionExpression` with type "at" and the pointer as an argument
- **Behavior**:
  - When used on the right side of an assignment: Loads the value from the pointer
  - When used on the left side of an assignment: Sets up a store to the pointer
- **Safety**: Must be used within an `unsafe` block

Examples:
```vyn
unsafe {
    var y: Int = at(p);  // Reading from a pointer (load)
    at(p) = 99;          // Writing to a pointer (store)
}
```

### 3.3. Converting Between Pointer Types with `from<loc<T>>(expr)`

The `from<loc<T>>()` operation converts between different pointer types or from an integer address to a typed pointer.

- **Syntax**: `from<loc<T>>(expr)`
- **Return Type**: `loc<T>`
- **AST Representation**: `ConstructionExpression` with a `GenericInstanceTypeNode` for "from" and the source expression as an argument
- **Safety**: Must be used within an `unsafe` block

Examples:
```vyn
unsafe {
    // Convert an integer to a pointer
    var addr: Int = 0x12345678;
    var p: loc<Int> = from<loc<Int>>(addr);
    
    // Convert between pointer types
    var p_void: loc<Void> = loc(x);
    var p_int: loc<Int> = from<loc<Int>>(p_void);
}
```

## 4. Unsafe Blocks

All memory operations must be contained within `unsafe` blocks, which are represented as `UnsafeBlockStatement` nodes in the AST.

- **Syntax**: `unsafe { ... }`
- **AST Representation**: `UnsafeBlockStatement` containing a `BlockStatement` with the unsafe operations
- **Purpose**: Explicitly marks code that may violate memory safety

Example:
```vyn
var x: Int = 42;
var p: loc<Int>;

unsafe {
    p = loc(x);
    at(p) = 99;
}

// The following would cause a compile-time error:
// p = loc(x);  // Error: 'loc' operation outside unsafe block
// at(p) = 99;  // Error: 'at' operation outside unsafe block
```

## 5. Error Cases and Safety Checks

The Vyn compiler and runtime perform various safety checks:

1. **Compile-time checks**:
   - Memory operations outside unsafe blocks are rejected
   - Type mismatches in pointer operations are caught
   - Null pointer constants are detected

2. **Runtime checks** (optional, based on build mode):
   - Null pointer dereferences can trigger runtime errors
   - Out-of-bounds pointer accesses may be detected

## 6. Implementation Details

Memory operations are implemented as intrinsics in the compiler:

1. `loc(x)` generates an LLVM instruction that computes the address of `x`
2. `at(p)` expands to a load or enables a store depending on context
3. `from<loc<T>>(expr)` generates appropriate pointer casts or integer-to-pointer conversions

The AST nodes and visitor implementations handle the different contexts for `at(p)`, particularly distinguishing between left-hand side (target of assignment) and right-hand side (value being read) usage.

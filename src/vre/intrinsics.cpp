#include <iostream> // For std::cout, std::endl
#include <string>
#include <vector>

namespace vyn {
namespace intrinsics {

/**
 * RUNTIME INTRINSICS
 * 
 * This file contains implementations of runtime intrinsics for the Vyn language.
 * These intrinsics are functions that are directly recognized and called by the runtime.
 *
 * NOTE: Memory operations such as loc(), at(), addr(), and from<loc<T>>() are NOT
 * implemented here. They are compiler intrinsics that are directly translated to LLVM
 * IR during code generation in src/vre/llvm/cgen_expr.cpp. These operations do not
 * have runtime function implementations.
 *
 * MEMORY OPERATION IMPLEMENTATION LOCATIONS:
 * - AST Nodes: include/vyn/parser/ast.hpp (LocationExpression, PointerDerefExpression, etc.)
 * - Semantic Analysis: src/vre/semantic.cpp (type checking & unsafe block verification)
 * - Code Generation: src/vre/llvm/cgen_expr.cpp (LLVM IR generation)
 */

// Console output intrinsic - this is an actual runtime function
void println(const std::string& output) {
    std::cout << output << std::endl;
}

// Future runtime intrinsics may be added here

} // namespace intrinsics
} // namespace vyn

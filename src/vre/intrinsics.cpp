#include <iostream> // For std::cout, std::endl
#include <string>
#include <vector>

// Potentially include VRE value types if intrinsics operate on them directly
// #include "vyn/vre/value.hpp"

namespace vyn {
namespace intrinsics {

// Example: Implementation for a 'println' intrinsic
// This is a simplified example. Actual implementation would depend on
// how Vyn values are represented and passed from the VRE.
void println(const std::string& output) {
    std::cout << output << std::endl;
}

// Add other intrinsic function implementations here.
// For example:
//
// void print_int(int64_t val) {
//     std::cout << val << std::endl;
// }
//
// vyn::vre::Value some_file_io_intrinsic(...) {
//     // ...
// }

} // namespace intrinsics
} // namespace vyn

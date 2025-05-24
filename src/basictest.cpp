#include "catch2/catch_test_macros.hpp"
#include "vyn/vyn.hpp"

// Function declaration from tests.cpp
int run_vyn_code(const std::string& source, const std::string& testName = "test_runtime.vyn", bool generateLLVMIR = false);

TEST_CASE("Basic execution test", "[basictest]") {
    std::string source = R"(
fn main() -> Int {
    var x: Int = 55;
    return x;
}
)";
    REQUIRE(run_vyn_code(source, "basictest", true) == 55);
}

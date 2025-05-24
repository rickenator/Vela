#define CATCH_CONFIG_MAIN
#include "vyn/vyn.hpp"
#include <catch2/catch_all.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <set> // Added for g_verbose_test_specifiers

// llvm includes for JIT
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/GenericValue.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>

#include "vyn/driver.hpp" // Added for vyn::Driver
#include <fstream> // Required for std::ofstream

// Forward declare run_vyn_code
int run_vyn_code(const std::string& source, const std::string& testName = "test_runtime.vyn", bool generateLLVMIR = false);

// Globals for verbose test control (defined in main.cpp)
extern std::set<std::string> g_verbose_test_specifiers;
extern bool g_make_all_tests_verbose;
extern bool g_suppress_all_debug_output; // For --no-debug-output

// Helper function to determine if the current test should be verbose
bool should_current_test_be_verbose() {
    if (g_suppress_all_debug_output) {
        return false;
    }
    if (g_make_all_tests_verbose) {
        return true;
    }
    if (g_verbose_test_specifiers.empty()) {
        return false; // Default to not verbose if no specific specifiers and not "all"
    }

    // Get current test name using IResultCapture
    // Requires <catch2/catch_interfaces_capture.hpp>
    std::string current_test_name = Catch::getResultCapture().getCurrentTestName();
    
    if (!current_test_name.empty()) {
        // Check for direct name match
        if (g_verbose_test_specifiers.count(current_test_name)) {
            return true;
        }
        // Check if any of the specifiers (which might be tags like "[my_tag]") 
        // are present as substrings in the current test name.
        // This is a simpler way to allow tag-like specifiers to work if they are part of the name.
        for (const auto& specifier : g_verbose_test_specifiers) {
            if (current_test_name.find(specifier) != std::string::npos) {
                return true;
            }
        }
    }

    // Direct tag checking from within the test case itself (outside of a listener)
    // is proving difficult with Catch2 API changes. For now, rely on name matching
    // or the user filtering tests by tag with Catch2 and using --debug-verbose all.

    return false;
}


// --- START: Dummy Visitor Implementations for Tests ---
// This is a temporary workaround to allow tests to compile.
// Ideally, SemanticAnalyzer and LLVMCodegen should implement all visit methods.

class DummySemanticAnalyzer : public vyn::SemanticAnalyzer {
public:
    DummySemanticAnalyzer(vyn::Driver& driver) : vyn::SemanticAnalyzer(driver) {}

    // Provide empty implementations for pure virtual methods not covered by SemanticAnalyzer
    void visit(vyn::ast::BorrowExpression* node) override {};
    void visit(vyn::ast::IfExpression* node) override {};
    void visit(vyn::ast::ConstructionExpression* node) override {};
    void visit(vyn::ast::ArrayInitializationExpression* node) override {};
    void visit(vyn::ast::ExternStatement* node) override {};
    void visit(vyn::ast::ThrowStatement* node) override {};
    void visit(vyn::ast::FieldDeclaration* node) override {};
    void visit(vyn::ast::EnumVariant* node) override {};
    void visit(vyn::ast::TemplateDeclaration* node) override {};
    void visit(vyn::ast::TypeNode* node) override {};
    void visit(vyn::ast::Module* node) override {
        // Call base class implementation to perform semantic analysis
        vyn::SemanticAnalyzer::visit(node);
    };
    void visit(vyn::ast::TupleTypeNode* node) override {};
    // Add any other missing pure virtuals from ast::Visitor that SemanticAnalyzer doesn't cover
    void visit(vyn::ast::LogicalExpression* node) override {};
    void visit(vyn::ast::ConditionalExpression* node) override {};
    void visit(vyn::ast::SequenceExpression* node) override {};
    void visit(vyn::ast::FunctionExpression* node) override {};
    void visit(vyn::ast::ThisExpression* node) override {};
    void visit(vyn::ast::SuperExpression* node) override {};
    void visit(vyn::ast::AwaitExpression* node) override {};
    void visit(vyn::ast::MatchStatement* node) override {};
    void visit(vyn::ast::YieldStatement* node) override {};
    void visit(vyn::ast::YieldReturnStatement* node) override {};
    void visit(vyn::ast::TraitDeclaration* node) override {};
    void visit(vyn::ast::NamespaceDeclaration* node) override {};
    void visit(vyn::ast::TypeName* node) override {};
    void visit(vyn::ast::PointerType* node) override {};
    void visit(vyn::ast::ArrayType* node) override {};
    void visit(vyn::ast::FunctionType* node) override {};
    void visit(vyn::ast::OptionalType* node) override {};
    void visit(vyn::ast::AssertStatement* node) override {};
};

class DummyLLVMCodegen : public vyn::LLVMCodegen {
public:
    DummyLLVMCodegen(vyn::Driver& driver) : vyn::LLVMCodegen(driver) {}

    // Provide empty implementations for pure virtual methods not covered by LLVMCodegen
    // From build log:
    void visit(vyn::ast::LogicalExpression* node) override {};
    void visit(vyn::ast::ConditionalExpression* node) override {};
    void visit(vyn::ast::SequenceExpression* node) override {};
    void visit(vyn::ast::FunctionExpression* node) override {};
    void visit(vyn::ast::ThisExpression* node) override {};
    void visit(vyn::ast::SuperExpression* node) override {};
    void visit(vyn::ast::AwaitExpression* node) override {};
    void visit(vyn::ast::EmptyStatement* node) override {};
    void visit(vyn::ast::ExternStatement* node) override {};
    void visit(vyn::ast::ThrowStatement* node) override {};
    void visit(vyn::ast::MatchStatement* node) override {};
    void visit(vyn::ast::YieldStatement* node) override {};
    void visit(vyn::ast::YieldReturnStatement* node) override {};
    void visit(vyn::ast::AssertStatement* node) override {};
    void visit(vyn::ast::TraitDeclaration* node) override {};
    void visit(vyn::ast::NamespaceDeclaration* node) override {};
    void visit(vyn::ast::TypeName* node) override {};
    void visit(vyn::ast::PointerType* node) override {};
    void visit(vyn::ast::ArrayType* node) override {};
    void visit(vyn::ast::FunctionType* node) override {};
    void visit(vyn::ast::OptionalType* node) override {};
    void visit(vyn::ast::TupleTypeNode* node) override {};
    // Add any other missing pure virtuals from ast::Visitor that LLVMCodegen doesn't cover
    // (cross-reference with SemanticAnalyzer's list and ast.hpp)
    void visit(vyn::ast::BorrowExpression* node) override {};
    void visit(vyn::ast::IfExpression* node) override {};
    void visit(vyn::ast::FieldDeclaration* node) override {};
    void visit(vyn::ast::EnumVariant* node) override {};
    void visit(vyn::ast::TemplateDeclaration* node) override {};
    void visit(vyn::ast::TypeNode* node) override {};
    void visit(vyn::ast::ListComprehension* node) override {};
    // Module visit is usually significant, ensure it's handled or explicitly dummied
    void visit(vyn::ast::Module* node) override {
        // Call base class implementation to generate LLVM IR
        vyn::LLVMCodegen::visit(node);
    };
    // void visit(vyn::ast::ArrayElementExpression* node) override {}; // Already in LLVMCodegen
    // void visit(vyn::ast::LocationExpression* node) override {}; // Already in LLVMCodegen

    // Add stubs for methods causing linker errors if not genuinely implemented in LLVMCodegen
    // These might be needed if LLVMCodegen is instantiated directly elsewhere, not just via DummyLLVMCodegen
    // However, the linker errors point to LLVMCodegen's vtable directly.
    // It's better to ensure LLVMCodegen implements them or they are pure virtual and DummyLLVMCodegen implements them.
    // For now, ensure DummyLLVMCodegen covers everything LLVMCodegen might be missing an impl for.

    // From linker errors, these are needed if LLVMCodegen doesn't provide them:
    // void visit(vyn::ast::CallExpression* node) override {}; // Already in LLVMCodegen decl, ensure Dummy has it if LLVMCodegen doesn't implement
    // void visit(vyn::ast::MemberExpression* node) override {}; // Already in LLVMCodegen decl, ensure Dummy has it if LLVMCodegen doesn't implement
    // void visit(vyn::ast::AssignmentExpression* node) override {}; // Implemented in LLVMCodegen, Dummy should inherit

    // Ensure all virtuals from ast::Visitor are covered if LLVMCodegen doesn't cover them.
    // This list should be cross-referenced with ast.hpp and LLVMCodegen's declarations.

    // Added based on previous edits to cgen_expr.cpp, ensure these are covered if not in LLVMCodegen
    // void visit(vyn::ast::IdentifierExpression* node) override {}; // Removed, assuming ast::Identifier is used.

};

// --- END: Dummy Visitor Implementations for Tests ---


TEST_CASE("Print parser version", "[parser]") {
    REQUIRE(true); // Placeholder to ensure test runs
}

TEST_CASE("Semantic: from(addr) only allowed in unsafe", "[semantic][pointer][unsafe][test38]") {
    std::string source_ok = R"(
fn main() -> Int{
    var<Int> addr = 0x1234;
    var<loc<Int>> p;
    unsafe {
        p = from<loc<Int>>(addr); // Updated syntax
    }
    return 0;
}
)";
    REQUIRE_NOTHROW(run_vyn_code(source_ok));

    std::string source_err = R"(
fn main() -> Int {
    var<Int> a = 0x1234;
    var<loc<Int>> p = from<loc<Int>>(a);
    return 0;
}
)";
    REQUIRE_THROWS(run_vyn_code(source_err));
}


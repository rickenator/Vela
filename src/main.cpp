#include "vyn/vyn.hpp"
#include "vyn/parser/lexer.hpp"   // For Lexer
#include "vyn/parser/parser.hpp"  // For vyn::Parser
#include <catch2/catch_session.hpp>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <set> // For test and parser verbosity specifiers
#include <algorithm> // For std::find

// Globals for test verbose control
std::set<std::string> g_verbose_test_specifiers;
bool g_make_all_tests_verbose = false;
bool g_suppress_all_debug_output = false;

// Globals for parser verbose control
namespace vyn {
    std::set<std::string> g_verbose_parser_test_specifiers;
    bool g_make_all_parser_verbose = false;
    bool g_suppress_all_parser_debug_output = false;
}

// Test comment by Copilot

int main(int argc, char* argv[]) {
    Catch::Session session; // Catch2 entry point

    std::vector<std::string> catch_args;
    catch_args.push_back(argv[0]); // Program name

    bool next_arg_is_test_specifier_for_verbose = false;
    bool test_mode_active = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--test") {
            test_mode_active = true;
            // Enter test mode for Catch2; do not forward our own flag
            continue;
        } else if (arg == "--debug-verbose") {
            if (i + 1 < argc) {
                std::string specifiers_str = argv[++i];
                if (specifiers_str == "all") {
                    g_make_all_tests_verbose = true;
                } else {
                    // Parse comma-separated specifiers
                    size_t start = 0;
                    size_t end = specifiers_str.find(',');
                    while (end != std::string::npos) {
                        g_verbose_test_specifiers.insert(specifiers_str.substr(start, end - start));
                        start = end + 1;
                        end = specifiers_str.find(',', start);
                    }
                    g_verbose_test_specifiers.insert(specifiers_str.substr(start));
                }
            } else {
                std::cerr << "Warning: --debug-verbose requires an argument (e.g., \"all\" or test_name,[tag])." << std::endl;
            }
        } else if (arg == "--no-debug-output") {
            g_suppress_all_debug_output = true;
        } else if (arg == "--debug-parser-verbose") {
            if (i + 1 < argc) {
                std::string spec_str = argv[++i];
                if (spec_str == "all") {
                    vyn::g_make_all_parser_verbose = true;
                } else {
                    size_t start = 0;
                    size_t end = spec_str.find(',');
                    while (end != std::string::npos) {
                        vyn::g_verbose_parser_test_specifiers.insert(spec_str.substr(start, end - start));
                        start = end + 1;
                        end = spec_str.find(',', start);
                    }
                    vyn::g_verbose_parser_test_specifiers.insert(spec_str.substr(start));
                }
            } else {
                std::cerr << "Warning: --debug-parser-verbose requires an argument." << std::endl;
            }
        } else if (arg == "--no-parser-debug-output") {
            vyn::g_suppress_all_parser_debug_output = true;
        }
        else {
            // If in test mode, or it\'s a general Catch2 arg, pass it along
             catch_args.push_back(arg);
        }
    }

    if (!test_mode_active && (g_make_all_tests_verbose || !g_verbose_test_specifiers.empty() || g_suppress_all_debug_output ||
                              vyn::g_make_all_parser_verbose || !vyn::g_verbose_parser_test_specifiers.empty() || vyn::g_suppress_all_parser_debug_output)) {
         std::cerr << "Warning: Debug verbosity flags (--debug-verbose, --no-debug-output, --debug-parser-verbose, --no-parser-debug-output) are intended for use with --test mode." << std::endl;
    }
    
    // Convert std::vector<std::string> to char* array for Catch2
    std::vector<char*> C_catch_args;
    for(const auto& s : catch_args) {
        C_catch_args.push_back(const_cast<char*>(s.c_str()));
    }

    int result = session.run(C_catch_args.size(), C_catch_args.data());

    // If tests were run, we might want to exit here.
    if (test_mode_active) {
        return result;
    }

    // If not in test mode, proceed with original file processing logic
    if (argc > 1) {
        std::string filename = argv[1];
        // ... (your existing file processing logic) ...
        std::cout << "Processing file: " << filename << std::endl;
         try {
            std::ifstream file(filename);
            if (!file.is_open()) {
                std::cerr << "Error: Could not open file " << filename << std::endl;
                return 1;
            }
            std::string source((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            file.close();

            Lexer lexer(source, filename);
            auto tokens = lexer.tokenize();
            
            // Optional: Print tokens if verbose mode is somehow enabled globally (not typical for non-test runs)
            // if (g_make_all_tests_verbose) { // Or some other global verbose flag
            //     for (const auto& token : tokens) {
            //         std::cout << vyn::token_type_to_string(token.type) << " (\'" << token.lexeme << "\')\\n";
            //     }
            // }

            vyn::Parser parser(tokens, filename);
            // if (g_make_all_tests_verbose) { // Or some other global verbose flag
            //    parser.set_verbose(true);
            // }
            std::unique_ptr<vyn::ast::Module> ast = parser.parse_module();
            
            // vyn::SemanticAnalyzer sema;
            // sema.analyze(ast.get());
            // auto errors = sema.getErrors();
            // if (!errors.empty()) {
            //     for (const auto& err : errors) {
            //         std::cerr << "Semantic Error: " << err << std::endl;
            //     }
            //     return 1; // Indicate semantic error
            // }

            // vyn::LLVMCodegen codegen;
            // codegen.generate(ast.get(), "output.ll"); // Example output name
            // std::cout << "LLVM IR generated to output.ll" << std::endl;


        } catch (const std::exception& e) {
            std::cerr << "Exception: " << e.what() << std::endl;
            return 1;
        }
    } else {
        std::cout << "Vyn Parser - Usage: " << argv[0] << " <filename> | --test [catch2_options]" << std::endl;
        std::cout << "                 " << argv[0] << " --test --debug-verbose <all|test_name,[tag],...>" << std::endl;
        std::cout << "                 " << argv[0] << " --test --no-debug-output" << std::endl;
        std::cout << "                 " << argv[0] << " --test --debug-parser-verbose <all|test_name,[tag],...>" << std::endl;
        std::cout << "                 " << argv[0] << " --test --no-parser-debug-output" << std::endl;
    }

    return result; // Or 0 if not running tests and successful
}
#pragma once

namespace vyn {

class Driver {
public:
    // Placeholder for now
    Driver() = default; 
    // Add any necessary methods or members that SemanticAnalyzer or LLVMCodegen might expect.
    // For example, if they need to report errors or access shared resources through the Driver:
    // void reportError(const std::string& message, const SourceLocation& loc);
    // SomeSharedResource& getSharedResource();
};

} // namespace vyn

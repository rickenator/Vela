#!/usr/bin/env python3
"""
Extract test cases from tests.cpp and convert them to individual .vyn test files.
"""

import re
import os
import sys
from pathlib import Path


def extract_test_case(test_case_text):
    """Extract test name, tags and code from a TEST_CASE block."""
    # Extract test name and tags
    match = re.search(r'TEST_CASE\("([^"]+)"(?:,\s*"([^"]+)")?\)', test_case_text)
    if not match:
        return None
    
    test_name = match.group(1)
    tags = match.group(2) if match.group(2) else ""
    
    # Extract test categories from tags
    categories = []
    if "[parser]" in tags:
        categories.append("parser")
    if "[lexer]" in tags:
        categories.append("lexer")
    if "[semantics]" in tags:
        categories.append("semantics")
    if "[runtime]" in tags:
        categories.append("runtime")
    
    # Simplify name for filename
    filename = re.sub(r'[^a-zA-Z0-9_]', '_', test_name.lower())
    filename = f"test_{filename}.vyn"
    
    # Extract the Vyn code from the test
    code_match = re.search(r'R"vyn\((.*?)\)vyn"', test_case_text, re.DOTALL)
    if not code_match:
        return None
    
    vyn_code = code_match.group(1).strip()
    
    # Determine the test type (parse-only, semantic, full)
    test_type = "parse-only"
    if "semantic" in tags.lower():
        test_type = "semantic-only"
    if "runtime" in tags.lower() or "exec" in tags.lower():
        test_type = "full"
    
    # Create the header
    header = [
        f"// @test: {test_name}",
        f"// @description: Extracted from tests.cpp - {test_name}",
        f"// @category: {', '.join(categories)}",
        f"// @expect: pass",
    ]
    
    if test_type == "parse-only":
        header.append("// @parse-only: true")
    elif test_type == "semantic-only":
        header.append("// @semantic-only: true")
    
    header.append("// @expect-output: n/a")
    header.append("// @expect-error: n/a")
    header.append("// @expect-return: n/a")
    header.append("")
    
    # Combine header and code
    result = "\n".join(header) + "\n" + vyn_code
    
    return {
        "name": test_name,
        "filename": filename,
        "code": result,
        "categories": categories,
        "test_type": test_type
    }


def extract_all_test_cases(cpp_file):
    """Extract all test cases from the C++ file."""
    with open(cpp_file, 'r') as f:
        content = f.read()
    
    # Find all TEST_CASE blocks with their content
    test_case_pattern = r'(TEST_CASE\s*\(\s*"[^"]+"\s*(?:,\s*"[^"]+")?\s*\)\s*\{.*?R"vyn\((.*?)\)vyn".*?)(?=TEST_CASE|$)'
    matches = re.finditer(test_case_pattern, content, re.DOTALL)
    
    # Process each test case
    test_cases = []
    for match in matches:
        full_test = match.group(1)
        # Extract test case info
        extracted = extract_test_case(full_test)
        if extracted:
            test_cases.append(extracted)
            print(f"Extracted test: {extracted['name']}")
    
    return test_cases


def save_test_cases(test_cases, output_dir):
    """Save the extracted test cases to individual files."""
    os.makedirs(output_dir, exist_ok=True)
    
    for test in test_cases:
        output_path = os.path.join(output_dir, test["filename"])
        with open(output_path, 'w') as f:
            f.write(test["code"])
        print(f"Created {output_path}")


def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <tests.cpp> <output_directory>")
        sys.exit(1)
    
    cpp_file = sys.argv[1]
    output_dir = sys.argv[2]
    
    if not os.path.exists(cpp_file):
        print(f"Error: File '{cpp_file}' not found.")
        sys.exit(1)
    
    test_cases = extract_all_test_cases(cpp_file)
    print(f"Found {len(test_cases)} test cases.")
    
    save_test_cases(test_cases, output_dir)
    print(f"Saved {len(test_cases)} test cases to {output_dir}")


if __name__ == "__main__":
    main()

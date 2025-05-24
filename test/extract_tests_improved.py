#!/usr/bin/env python3
"""
Extract test cases from tests.cpp and convert them to individual .vyn test files.

This script parses the Vyn compiler's tests.cpp file and extracts the test cases
into separate .vyn files with appropriate metadata headers. It handles both raw string
literals and regular string literals.
"""

import re
import os
import sys
from pathlib import Path


def extract_string_literal(text):
    """
    Extract string literals of various forms from the given text.
    Handles raw string literals like R"(...)" and regular string literals.
    """
    # Try to match raw string literals first
    raw_match = re.search(r'R"(?:\w*)?\((.*?)\)(?:\w*)?"', text, re.DOTALL)
    if raw_match:
        return raw_match.group(1)
    
    # Try to match regular string literals with escape sequences
    # This is more complex due to escape sequences
    string_match = re.search(r'"((?:\\.|[^"\\])*)"', text)
    if string_match:
        # Handle escape sequences
        return string_match.group(1).replace('\\n', '\n').replace('\\t', '\t')
    
    return None


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
    if len(categories) == 0:
        categories.append("uncategorized")
    
    # Simplify name for filename
    filename = re.sub(r'[^a-zA-Z0-9_]', '_', test_name.lower())
    filename = f"test_{filename}.vyn"
    
    # Find the source string in the test case
    source_match = re.search(r'std::string source\s*=\s*(.+?);\s*//?\s*(.*)', test_case_text, re.DOTALL)
    if not source_match:
        # Try without a comment
        source_match = re.search(r'std::string source\s*=\s*(.+?);\s*', test_case_text, re.DOTALL)
        if not source_match:
            return None
            
    source_text = source_match.group(1)
    comment = source_match.group(2) if len(source_match.groups()) > 1 else ""
    
    # Extract the original filename if present in the comment
    original_filename = ""
    filename_match = re.search(r'(\w+\.vyn)', comment)
    if filename_match:
        original_filename = filename_match.group(1)
        # Use the original filename from the comment if present
        filename = original_filename
    
    # Extract the Vyn code from the source string
    vyn_code = extract_string_literal(source_text)
    if vyn_code is None:
        return None
    
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
        f"// @expect: pass"
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
    # Match everything between TEST_CASE and the next TEST_CASE or end of file
    test_case_pattern = r'(TEST_CASE\s*\(\s*"[^"]+"\s*(?:,\s*"[^"]+")?\s*\)\s*\{.*?)(?=TEST_CASE|\Z)'
    matches = re.finditer(test_case_pattern, content, re.DOTALL)
    
    # Process each test case
    test_cases = []
    for match in matches:
        full_test = match.group(1)
        # Skip test cases that don't have a std::string source =
        if "std::string source =" not in full_test:
            continue
            
        # Extract test case info
        extracted = extract_test_case(full_test)
        if extracted:
            test_cases.append(extracted)
            print(f"Extracted test: {extracted['name']}")
    
    return test_cases


def save_test_cases(test_cases, output_dir):
    """Save the extracted test cases to individual files."""
    os.makedirs(output_dir, exist_ok=True)
    
    # Get existing files to avoid duplicates
    existing_files = set([f.name for f in Path(output_dir).glob("*.vyn")])
    
    for test in test_cases:
        # Check if file already exists
        if test["filename"] in existing_files:
            base_filename = test["filename"].replace(".vyn", "")
            count = 1
            while f"{base_filename}_{count}.vyn" in existing_files:
                count += 1
            test["filename"] = f"{base_filename}_{count}.vyn"
        
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

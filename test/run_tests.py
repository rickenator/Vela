#!/usr/bin/env python3
"""
Vyn Test Harness

This script discovers and runs test files in the Vyn language, 
using special directives in test file comments to determine how to run each test.
"""

import os
import re
import subprocess
import sys
import argparse
from pathlib import Path
from dataclasses import dataclass
from typing import Optional, List
import time
import json


def find_vyn_root():
    """Find the Vyn repository root directory.
    
    This function looks for the repository root by starting from this script's directory
    and walking up until it finds a directory with CMakeLists.txt and src/tests.cpp
    (which are clear indicators of the Vyn repository root).
    
    Returns:
        Path: The absolute path to the Vyn repository root
    """
    # Start from the directory where this script is located
    script_dir = Path(os.path.dirname(os.path.abspath(__file__)))
    current_dir = script_dir
    
    # Walk up directories until we find the Vyn repo root
    while True:
        # Check if this looks like the Vyn repo root
        if (current_dir / "CMakeLists.txt").exists() and (current_dir / "src" / "tests.cpp").exists():
            return current_dir
        
        # Go up one level
        parent_dir = current_dir.parent
        
        # If we've reached the filesystem root without finding it
        if parent_dir == current_dir:
            raise RuntimeError("Could not find Vyn repository root directory")
        
        current_dir = parent_dir

@dataclass
class TestCase:
    """Represents a single Vyn test case with its directives."""
    filename: str
    name: str = "Unnamed test"
    description: str = ""
    category: List[str] = None
    expect: str = "pass"  # pass or fail
    expect_error: Optional[str] = None
    expect_output: Optional[str] = None
    expect_return: Optional[str] = None
    parse_only: bool = False
    semantic_only: bool = False
    
    def __post_init__(self):
        if self.category is None:
            self.category = ["uncategorized"]

def parse_directives(file_path):
    """Parse test directives from comments at the top of the file."""
    test = TestCase(filename=str(file_path))
    
    try:
        with open(file_path, 'r') as f:
            content = f.read()
        
        # Extract directives using regex
        name_match = re.search(r'// @test:\s*(.*?)$', content, re.MULTILINE)
        if name_match:
            test.name = name_match.group(1).strip()
        
        desc_match = re.search(r'// @description:\s*(.*?)$', content, re.MULTILINE)
        if desc_match:
            test.description = desc_match.group(1).strip()
        
        cat_match = re.search(r'// @category:\s*(.*?)$', content, re.MULTILINE)
        if cat_match:
            test.category = [c.strip() for c in cat_match.group(1).split(',')]
        
        expect_match = re.search(r'// @expect:\s*(.*?)$', content, re.MULTILINE)
        if expect_match:
            test.expect = expect_match.group(1).strip().lower()
        
        error_match = re.search(r'// @expect-error:\s*(.*?)$', content, re.MULTILINE)
        if error_match:
            test.expect_error = error_match.group(1).strip()
        
        output_match = re.search(r'// @expect-output:\s*(.*?)$', content, re.MULTILINE)
        if output_match:
            test.expect_output = output_match.group(1).strip()
        
        return_match = re.search(r'// @expect-return:\s*(.*?)$', content, re.MULTILINE)
        if return_match:
            test.expect_return = return_match.group(1).strip()
        
        parse_match = re.search(r'// @parse-only:\s*(.*?)$', content, re.MULTILINE)
        if parse_match and parse_match.group(1).strip().lower() in ('true', 'yes', '1'):
            test.parse_only = True
        
        semantic_match = re.search(r'// @semantic-only:\s*(.*?)$', content, re.MULTILINE)
        if semantic_match and semantic_match.group(1).strip().lower() in ('true', 'yes', '1'):
            test.semantic_only = True
        
    except Exception as e:
        print(f"Error parsing test directives in {file_path}: {str(e)}")
        
    return test

def run_test(test, vyn_executable, verbose=False):
    """Run the test and verify results against expectations."""
    cmd = [vyn_executable]
    
    if test.parse_only:
        cmd.append("--parse-only")
    elif test.semantic_only:
        cmd.append("--semantic-only")
    
    cmd.append(test.filename)
    
    start_time = time.time()
    try:
        result = subprocess.run(cmd, capture_output=True, text=True)
        elapsed = time.time() - start_time
        
        # Check return code matches expectations
        expected_return_code = 0 if test.expect == "pass" else 1
        success = (result.returncode == expected_return_code)
        
        # Check output if specified
        if test.expect_output and test.expect_output != "n/a":
            if test.expect_output not in result.stdout:
                success = False
                
        # Check for expected error if failing test
        if test.expect == "fail" and test.expect_error and test.expect_error != "n/a":
            if test.expect_error not in result.stderr:
                success = False
        
        return {
            "success": success,
            "stdout": result.stdout,
            "stderr": result.stderr,
            "return_code": result.returncode,
            "execution_time": elapsed
        }
            
    except Exception as e:
        elapsed = time.time() - start_time
        return {
            "success": False,
            "stdout": "",
            "stderr": str(e),
            "return_code": -1,
            "execution_time": elapsed
        }

def format_result(result, test, verbose=False):
    """Format test result for display."""
    status = "PASS" if result["success"] else "FAIL"
    
    if verbose:
        output = f"\n{status}: {test.name} ({test.filename})\n"
        output += f"Description: {test.description}\n"
        output += f"Category: {', '.join(test.category)}\n"
        output += f"Execution time: {result['execution_time']:.4f}s\n"
        
        if not result["success"] or verbose > 1:
            output += "Output:\n"
            if result["stdout"]:
                output += result["stdout"] + "\n"
            output += "Error:\n"
            if result["stderr"]:
                output += result["stderr"] + "\n"
    else:
        output = f"{status}: {test.name}"
        
    return output

def main():
    # Find the Vyn repository root
    try:
        vyn_root = find_vyn_root()
    except RuntimeError as e:
        print(f"Error: {e}")
        sys.exit(1)

    parser = argparse.ArgumentParser(
        description='Vyn test harness',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Run all tests in test/units
  ./run_tests.py
  
  # Run parser category tests
  ./run_tests.py --category parser
  
  # Run tests with pattern matching
  ./run_tests.py --pattern "test1*.vyn" 
  
  # Run tests with increased verbosity
  ./run_tests.py -vv
        """
    )
    parser.add_argument('--vyn', default=None, help='Path to vyn executable (defaults to build/vyn)')
    parser.add_argument('--test-dir', default=None, help='Directory containing test files (defaults to test/units)')
    parser.add_argument('--pattern', default='*.vyn', help='File pattern for test files')
    parser.add_argument('--verbose', '-v', action='count', default=0, help='Increase verbosity')
    parser.add_argument('--category', help='Only run tests in this category')
    parser.add_argument('--json', help='Save results to JSON file')
    args = parser.parse_args()
    
    # Set default paths relative to the repository root
    if args.vyn is None:
        vyn_executable = str(vyn_root / "build" / "vyn")
    else:
        vyn_executable = args.vyn
        
    if args.test_dir is None:
        test_dir = vyn_root / "test" / "units"
    else:
        test_dir = Path(args.test_dir)
    
    # Verify executable exists
    if not os.path.isfile(vyn_executable):
        print(f"Error: Cannot find Vyn executable at {vyn_executable}")
        print(f"Build Vyn or specify executable with --vyn option")
        sys.exit(1)
    
    # Verify test directory exists
    if not test_dir.exists():
        print(f"Error: Test directory {test_dir} doesn't exist")
        sys.exit(1)
        
    if args.verbose:
        print(f"Vyn repository root: {vyn_root}")
        print(f"Using Vyn executable: {vyn_executable}")
        print(f"Using test directory: {test_dir}")
    
    results = []
    pass_count = 0
    fail_count = 0
    
    # Find all test files matching the pattern
    test_files = sorted(list(test_dir.glob(f"**/{args.pattern}")))
    
    if args.verbose:
        print(f"Found {len(test_files)} test files in {test_dir}")
    
    # Run each test and collect results
    for file in test_files:
        test = parse_directives(file)
        
        # Skip tests that don't match the category filter
        if args.category and args.category not in test.category:
            continue
        
        test_result = run_test(test, vyn_executable, args.verbose > 1)
        
        # Format and print result
        output = format_result(test_result, test, args.verbose)
        print(output)
        
        if test_result["success"]:
            pass_count += 1
        else:
            fail_count += 1
            
        results.append({
            "test": {
                "filename": test.filename,
                "name": test.name,
                "description": test.description,
                "category": test.category,
                "expect": test.expect,
                "parse_only": test.parse_only,
                "semantic_only": test.semantic_only
            },
            "result": test_result
        })
    
    # Print summary
    print(f"\nRan {len(results)} tests")
    print(f"Passed: {pass_count}")
    print(f"Failed: {fail_count}")
    
    # Save results to JSON file if requested
    if args.json:
        # If json path is not absolute, make it relative to the current working directory
        json_path = args.json
        if not os.path.isabs(json_path):
            json_path = os.path.join(os.getcwd(), json_path)
            
        with open(json_path, 'w') as f:
            json.dump(results, f, indent=2)
            if args.verbose:
                print(f"Results saved to {json_path}")
    
    # Return non-zero exit code if any tests failed
    if fail_count > 0:
        sys.exit(1)

if __name__ == "__main__":
    main()

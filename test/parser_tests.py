#!/usr/bin/env python3
"""
Vyn Parser Test Runner

This script specifically runs parser test files in the Vyn language,
focusing on the parser category tests.
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
        if parse_match:
            value = parse_match.group(1).strip().lower()
            test.parse_only = (value == "true" or value == "yes" or value == "1")
        
        semantic_match = re.search(r'// @semantic-only:\s*(.*?)$', content, re.MULTILINE)
        if semantic_match:
            value = semantic_match.group(1).strip().lower()
            test.semantic_only = (value == "true" or value == "yes" or value == "1")
        
        return test
    
    except Exception as e:
        print(f"Error parsing directives in {file_path}: {e}")
        return test

def run_test(test, vyn_executable, debug=False):
    """Run a single test using the Vyn compiler."""
    start_time = time.time()
    
    # Prepare the command
    cmd = [vyn_executable]
    
    if test.parse_only:
        cmd.append("--parse-only")
    elif test.semantic_only:
        cmd.append("--semantic-only")
    
    cmd.append(test.filename)
    
    if debug:
        print(f"Running command: {' '.join(cmd)}")
    
    # Run the command
    result = {
        "cmd": ' '.join(cmd),
        "stdout": "",
        "stderr": "",
        "returncode": 0,
        "success": False,
        "time": 0,
        "expected": test.expect
    }
    
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True)
        result["stdout"] = proc.stdout.strip()
        result["stderr"] = proc.stderr.strip()
        result["returncode"] = proc.returncode
        
        if test.expect == "pass":
            # Test should pass
            if proc.returncode == 0:
                result["success"] = True
            else:
                result["success"] = False
        else:
            # Test should fail
            if proc.returncode != 0:
                result["success"] = True
            else:
                result["success"] = False
    
    except Exception as e:
        result["stderr"] = str(e)
        result["returncode"] = -1
        result["success"] = False
    
    # Record the execution time
    result["time"] = round(time.time() - start_time, 3)
    
    return result

def format_result(result, test, verbose=0):
    """Format test result for display."""
    if result["success"]:
        status = "PASS"
    else:
        status = "FAIL"
    
    # For verbose mode, include more details
    if verbose > 0:
        output = f"{status}: {test.name} [{', '.join(test.category)}]\n"
        output += f"File: {test.filename}\n"
        output += f"Description: {test.description}\n"
        output += f"Expected: {test.expect}\n"
        output += f"Return code: {result['returncode']}\n"
        output += f"Time: {result['time']}s\n"
            
        if result["stdout"]:
            output += "-- Standard Output --\n"
            output += result["stdout"] + "\n"
            
        if result["stderr"]:
            output += "-- Standard Error --\n"
            output += result["stderr"] + "\n"
    else:
        output = f"{status}: {test.name}"
        
    return output

def main():
    """Main function to parse arguments and run tests."""
    # Find the Vyn repository root
    try:
        vyn_root = find_vyn_root()
    except RuntimeError as e:
        print(f"Error: {e}")
        sys.exit(1)
    
    parser = argparse.ArgumentParser(
        description='Vyn parser test runner',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Run all parser tests
  ./parser_tests.py
  
  # Run with higher verbosity
  ./parser_tests.py -vv
  
  # Run specific test pattern
  ./parser_tests.py --pattern "test1*.vyn"
  
  # Run tests and save results to JSON
  ./parser_tests.py --json results.json
        """
    )
    parser.add_argument('--vyn', default=None, help='Path to vyn executable (defaults to build/vyn)')
    parser.add_argument('--test-dir', default=None, help='Directory containing parser test files (defaults to test/units/parser)')
    parser.add_argument('--pattern', default='*.vyn', help='File pattern for test files')
    parser.add_argument('--verbose', '-v', action='count', default=0, help='Increase verbosity')
    parser.add_argument('--json', help='Save results to JSON file')
    parser.add_argument('--list', '-l', action='store_true', help='List available tests without running them')
    parser.add_argument('--filter', '-f', help='Only run tests matching this string in name or description')
    args = parser.parse_args()
    
    # Set default paths relative to the repository root
    if args.vyn is None:
        vyn_executable = str(vyn_root / "build" / "vyn")
    else:
        vyn_executable = args.vyn
        
    if args.test_dir is None:
        test_dir = vyn_root / "test" / "units" / "parser"
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
    
    # Collect test information
    tests = []
    for file in test_files:
        test = parse_directives(file)
        if args.filter and args.filter.lower() not in test.name.lower() and args.filter.lower() not in test.description.lower():
            continue
        tests.append(test)
    
    # If we're just listing tests, do that and exit
    if args.list:
        print(f"Found {len(tests)} tests matching criteria:")
        for test in tests:
            print(f"{test.name} - {test.description} [{', '.join(test.category)}]")
        sys.exit(0)
    
    # Run each test and collect results
    for test in tests:
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

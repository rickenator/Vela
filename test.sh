#!/bin/bash

# Get the directory of the script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
TEST_EXECUTABLE="$BUILD_DIR/vyn"
LOG_FILE="$BUILD_DIR/test_output.log"

# Ensure the build directory exists
if [ ! -d "$BUILD_DIR" ]; then
    echo "Build directory $BUILD_DIR not found. Please build the project first." >&2
    exit 1
fi

# Ensure the test executable exists
if [ ! -x "$TEST_EXECUTABLE" ]; then
    echo "Test executable $TEST_EXECUTABLE not found or not executable." >&2
    exit 1
fi

# Clear previous log file
> "$LOG_FILE"

echo "Starting test execution... Output will be in $LOG_FILE" | tee -a "$LOG_FILE"

# Get the list of test cases, excluding lines with errors or empty lines
TEST_CASES=$("$TEST_EXECUTABLE" --list-tests 2>/dev/null | grep -E '^  [A-Za-z0-9_]' | sed 's/^  //' | sed 's/\r$//')

if [ -z "$TEST_CASES" ]; then
    echo "No test cases found by $TEST_EXECUTABLE --list-tests" | tee -a "$LOG_FILE"
    exit 1
fi

IFS=$'\n' # Set Internal Field Separator to newline to handle test names with spaces

for test_case in $TEST_CASES; do
    # Skip lines that are actually tags (e.g., starting with '[')
    if [[ "$test_case" == \[* ]]; then
        continue
    fi
    echo "------------------------------------------------------------" | tee -a "$LOG_FILE"
    echo "RUNNING TEST: [$test_case]" | tee -a "$LOG_FILE"
    echo "------------------------------------------------------------" | tee -a "$LOG_FILE"
    
    # Run the individual test case. Redirect stdout and stderr to the log file.
    # The --success flag includes successful tests in the output.
    # The --verbosity high for more details from Catch2.
    # Pass the test_case directly, ensuring it's quoted to handle spaces.
    if "$TEST_EXECUTABLE" "$test_case" --success --verbosity high --durations yes >> "$LOG_FILE" 2>&1; then
        echo "PASSED: [$test_case]" | tee -a "$LOG_FILE"
    else
        echo "FAILED or HUNG: [$test_case]" | tee -a "$LOG_FILE"
        echo "Check $LOG_FILE for details." | tee -a "$LOG_FILE"
        # Optionally, you might want to exit here if a test fails or hangs
        # exit 1 
    fi
done

IFS=$' \t\n' # Reset IFS

echo "------------------------------------------------------------" | tee -a "$LOG_FILE"
echo "All tests completed. Check $LOG_FILE for full output." | tee -a "$LOG_FILE"

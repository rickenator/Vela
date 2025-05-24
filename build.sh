# filepath: build.sh
#!/bin/bash
# Build and run Vyn compiler with optional test support

# Define project root (use the directory containing this script)
VYN_ROOT=$(dirname "$(readlink -f "$0")")

# Process arguments
RUN_TESTS=0
VERBOSE=0
TEST_PATTERN="*.vyn"
TEST_CATEGORY=""

while [[ $# -gt 0 ]]; do
    case $1 in
        --run-tests)
            RUN_TESTS=1
            shift
            ;;
        --verbose|-v)
            VERBOSE=1
            shift
            ;;
        --test-pattern)
            TEST_PATTERN="$2"
            shift 2
            ;;
        --category)
            TEST_CATEGORY="$2"
            shift 2
            ;;
        *)
            # Assume it's a file to compile
            INPUT_FILE="$1"
            shift
            ;;
    esac
done

# Clean and rebuild
rm -rf $VYN_ROOT/build/*
mkdir -p $VYN_ROOT/build
cd $VYN_ROOT/build
cmake ..
make -j > build_output.log 2>&1

# Run tests if requested
if [ $RUN_TESTS -eq 1 ]; then
    echo "Running tests..."
    VERBOSITY=""
    if [ $VERBOSE -eq 1 ]; then
        VERBOSITY="-v"
    fi
    
    CATEGORY_FILTER=""
    if [ ! -z "$TEST_CATEGORY" ]; then
        CATEGORY_FILTER="--category $TEST_CATEGORY"
    fi
    
    make run-tests ARGS="--pattern '$TEST_PATTERN' $VERBOSITY $CATEGORY_FILTER"
elif [ ! -z "$INPUT_FILE" ]; then
    # Run the compiler on the specified file
    echo "Compiling $INPUT_FILE..."
    cd $VYN_ROOT
    ./build/vyn "$INPUT_FILE"
fi

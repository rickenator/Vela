# Vyn Test System

This directory contains the Vyn language test suite and test harness system.

## Test Structure

Tests in Vyn consist of `.vyn` files with special directives in comments at the top of the file.
These directives tell the test harness how to run and validate each test.

### Test Directives

Test directives are specified as comments at the top of each test file:

```vyn
// @test: Short Name of The Test
// @description: Longer description of what the test is checking
// @category: category1, category2
// @expect: pass
// @parse-only: true
// @expect-output: Expected output text
// @expect-error: Expected error message
// @expect-return: Expected return value
```

### Available Directives

| Directive | Description | Example |
|-----------|-------------|---------|
| `@test` | Short test name | `@test: Array Indexing` |
| `@description` | Detailed description | `@description: Tests array index bounds checking` |
| `@category` | Test category | `@category: parser, semantic, runtime, feature-X` |
| `@expect` | Expected result | `@expect: pass` or `@expect: fail` |
| `@expect-error` | Expected error pattern | `@expect-error: Type mismatch` |
| `@expect-output` | Expected stdout | `@expect-output: Hello, world!` |
| `@expect-return` | Expected return value | `@expect-return: 42` |
| `@parse-only` | Test parsing only | `@parse-only: true` |
| `@semantic-only` | Test parsing & semantics | `@semantic-only: true` |

## Running Tests

### Using the Build Script

The easiest way to run tests is with the build script:

```bash
./build.sh --run-tests          # Run all tests
./build.sh --run-tests --verbose  # Run all tests with verbose output
./build.sh --run-tests --category parser  # Run only parser tests
./build.sh --run-tests --test-pattern "test_*_syntax.vyn"  # Run specific tests
```

### Using CMake Directly

You can also use the CMake target directly:

```bash
cd build
make run-tests
```

### Using the Test Script Directly

You can run the test script directly for more control:

```bash
cd test
./run_tests.py --vyn ../build/vyn  # Point to your vyn executable
./run_tests.py --vyn ../build/vyn --verbose  # More detailed output
./run_tests.py --vyn ../build/vyn --category parser  # Filter by category
./run_tests.py --vyn ../build/vyn --pattern "test_relaxed_*.vyn"  # Test pattern
./run_tests.py --vyn ../build/vyn --json results.json  # Save results to JSON
```

## Writing New Tests

To create a new test:

1. Create a new `.vyn` file in the test directory
2. Add test directives at the top of the file as comments
3. Write your test code
4. Add it to the appropriate test category

### Example Test

```vyn
// @test: Integer Addition
// @description: Tests basic integer addition
// @category: runtime, arithmetic
// @expect: pass
// @expect-output: Result: 15
// @expect-return: 0

fn<Int> main() -> {
    var<Int> a = 5;
    var<Int> b = 10;
    var<Int> c = a + b;
    println("Result: " + c);
    return 0;
}
```

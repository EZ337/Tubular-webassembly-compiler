#!/bin/bash

# Initialize a counter for differing files
wat_count=0
wasm_count=0
test_count=20

error_pass_count=0
error_fail_count=0
error_test_count=11

P3_wat_count=0
P3_wasm_count=0
P3_test_count=30

P3_error_pass_count=0
P3_error_fail_count=0
P3_error_test_count=19

echo NEW PROJECT 4 TESTS

# Loop through all the regular test file pairs
for i in $(seq -w 01 $test_count); do
    # Set the file names
    code_file="test-${i}.tube"
    wat_file="test-${i}.wat"
    wasm_file="test-${i}.wasm"

    # Use Project4 to generate the WAT file for each test case.
    if [[ -f "../Project4" && -f "$code_file" ]]; then
        ../Project4 "$code_file" > "$wat_file"
    else
        echo "Executable ../Project4 or code file $code_file does not exist."
        continue
    fi

    if [ $? -ne 0 ]; then
        echo "Compilation of test $i to WAT format FAILED."
        rm -f "$wat_file"
        continue
    else
        ((wat_count++))
        echo "Compilation of test $i to WAT format SUCCESSFUL."
    fi

    # Double check that WAT file was generated; convert it to WASM
    if [[ -f "$wat_file" ]]; then
        wat2wasm "$wat_file"
    else
        echo "File '$wat_file' does not exist."
        continue
    fi

    if [ $? -ne 0 ]; then
        echo "                   ... to WASM format FAILED."
        rm -f "$wasm_file"
        continue
    else
        echo "                   ... to WASM format SUCCESSFUL."
    fi

    # Make sure WASM file was generated
    if [[ -f "$wasm_file" ]]; then
        ((wasm_count++))
    else
        echo "File '$wasm_file' does not exist."
    fi
done

echo ---
echo PROJECT 3 Testing

# Loop through all the regular test file pairs
for i in $(seq -w 01 $P3_test_count); do
    # Set the file names
    code_file="P3-test-${i}.tube"
    wat_file="P3-test-${i}.wat"
    wasm_file="P3-test-${i}.wasm"

    # Use Project4 to generate the WAT file for each test case.
    if [[ -f "../Project4" && -f "$code_file" ]]; then
        ../Project4 "$code_file" > "$wat_file"
    else
        echo "Executable ../Project4 or code file $code_file does not exist."
        continue
    fi

    if [ $? -ne 0 ]; then
        echo "Compilation of P3 test $i to WAT format FAILED."
        rm -f "$wat_file"
        continue
    else
        ((P3_wat_count++))
        echo "Compilation of P3 test $i to WAT format SUCCESSFUL."
    fi

    # Double check that WAT file was generated; convert it to WASM
    if [[ -f "$wat_file" ]]; then
        wat2wasm "$wat_file"
    else
        echo "File '$wat_file' does not exist."
        continue
    fi

    if [ $? -ne 0 ]; then
        echo "                   ... to WASM format FAILED."
        rm -f "$wasm_file"
        continue
    else
        echo "                   ... to WASM format SUCCESSFUL."
    fi

    # Make sure WASM file was generated
    if [[ -f "$wasm_file" ]]; then
        ((P3_wasm_count++))
    else
        echo "File '$wasm_file' does not exist."
    fi
done

echo ---
echo ERROR Testing

# Loop through all the ERROR test file pairs
for i in $(seq -w 01 $error_test_count); do
    # Set the file names
    code_file="test-error-${i}.tube"
    wat_file="output-error-${i}.wat"

    # Generate the output file for Project4
    if [[ -f "../Project4" && -f "$code_file" ]]; then
        ../Project4 "$code_file"
    else
        echo "Executable ../Project4 or code file $code_file does not exist."
        continue
    fi

    # Check the return code
    if [ $? -ne 0 ]; then
        echo "Error test $i ... Passed!"
        ((error_pass_count++))
    else
        echo "Error test $code_file failed (zero return code)."
        ((error_fail_count++))
    fi
done

echo ---
echo ERROR Testing from Project 3

# Loop through all the ERROR test file pairs
for i in $(seq -w 01 $P3_error_test_count); do
    # Set the file names
    code_file="P3-test-error-${i}.tube"
    wat_file="P3-output-error-${i}.wat"

    # Generate the output file for Project4
    if [[ -f "../Project4" && -f "$code_file" ]]; then
        ../Project4 "$code_file" > /dev/null 2>&1
    else
        echo "Executable ../Project4 or code file $code_file does not exist."
        continue
    fi

    # Check the return code
    if [ $? -ne 0 ]; then
        echo "Error test $i ... Passed!"
        ((P3_error_pass_count++))
    else
        echo "Error test $code_file failed (zero return code)."
        ((P3_error_fail_count++))
    fi
done

# Report the final count of differing files
echo ---
echo "Of $test_count regular test files..."
echo "...generated $wat_count WAT files"
echo "...converted $wasm_count WAT files to wasm files for testing."
echo "Of $P3_test_count Project 3 tests (that need to still work)..."
echo "...generated $P3_wat_count WAT files"
echo "...converted $P3_wasm_count WAT files to wasm files for testing."
echo "Passed $error_pass_count of $error_test_count error tests (Failed $error_fail_count)"
echo "Passed $P3_error_pass_count of $P3_error_test_count Project 3 error tests (Failed $P3_error_fail_count)"

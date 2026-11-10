#!/bin/bash

# Test runner for Surrogate Contact Model (Spec 04 T4.3-T4.5)
# 
# Usage: ./run_tests.sh [test_number]
#   ./run_tests.sh     # Run all tests
#   ./run_tests.sh 1   # Run test 01 only
#   ./run_tests.sh 2   # Run test 02 only

set -e

# ===== Configuration =====
LIGGGHTS_BIN="/home/su_estupinan@private.list.lu/SpaceLights/LIGGGHTS-PUBLIC/build/liggghts"
TEST_DIR="/home/su_estupinan@private.list.lu/SpaceLights/LIGGGHTS-PUBLIC/test_cases/surrogate_contact"
MODEL_DIR="/home/su_estupinan@private.list.lu/SpaceLights/LIGGGHTS-PUBLIC/SS_models"

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# ===== Functions =====
print_header() {
    echo -e "\n${BLUE}======================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}======================================${NC}\n"
}

print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

print_error() {
    echo -e "${RED}✗ $1${NC}"
}

print_warning() {
    echo -e "${YELLOW}⚠ $1${NC}"
}

check_prerequisites() {
    print_header "Checking Prerequisites"
    
    # Check binary
    if [ ! -f "$LIGGGHTS_BIN" ]; then
        print_error "LIGGGHTS binary not found: $LIGGGHTS_BIN"
        echo "Build with: cmake --build build -j"
        exit 1
    fi
    print_success "LIGGGHTS binary found"
    
    # Check model files
    if [ ! -f "$MODEL_DIR/ss-cube-walls/model.pt" ]; then
        print_error "Model file not found: $MODEL_DIR/ss-cube-walls/model.pt"
        exit 1
    fi
    print_success "Model file found"
    
    if [ ! -f "$MODEL_DIR/ss-cube-walls/scalers.dat" ]; then
        print_error "Scalers file not found: $MODEL_DIR/ss-cube-walls/scalers.dat"
        exit 1
    fi
    print_success "Scalers file found"
    
    # Check test files
    if [ ! -f "$TEST_DIR/test_01_cube_1m_falling.in" ]; then
        print_error "Test 01 input not found: $TEST_DIR/test_01_cube_1m_falling.in"
        exit 1
    fi
    print_success "Test 01 input found"
    
    if [ ! -f "$TEST_DIR/test_02_cube_1cm_falling.in" ]; then
        print_error "Test 02 input not found: $TEST_DIR/test_02_cube_1cm_falling.in"
        exit 1
    fi
    print_success "Test 02 input found"
}

run_test() {
    local test_num=$1
    local test_name=$2
    local input_file=$3
    
    print_header "Running Test $test_num: $test_name"
    
    # Create output directory
    local output_dir="$TEST_DIR/output/test_$test_num"
    mkdir -p "$output_dir"
    
    # Run test with output
    local output_log="$output_dir/output.log"
    local start_time=$(date +%s)
    
    echo "Input file: $input_file"
    echo "Output directory: $output_dir"
    echo "Starting at: $(date)"
    echo ""
    
    # Run LIGGGHTS
    cd "$output_dir"
    if $LIGGGHTS_BIN -in "$input_file" > "$output_log" 2>&1; then
        local end_time=$(date +%s)
        local duration=$((end_time - start_time))
        
        print_success "Test $test_num completed successfully"
        echo "Duration: ${duration}s"
        echo "Log file: $output_log"
        
        # Check for errors in output
        if grep -qi "error\|segmentation\|nan\|inf" "$output_log"; then
            print_warning "Potential errors detected in log file"
            echo "Last 20 lines of output:"
            tail -20 "$output_log"
        else
            echo "Last 10 lines of output:"
            tail -10 "$output_log"
        fi
        
        return 0
    else
        local end_time=$(date +%s)
        local duration=$((end_time - start_time))
        
        print_error "Test $test_num FAILED"
        echo "Duration: ${duration}s"
        echo "Error log saved to: $output_log"
        
        echo ""
        echo "Last 30 lines of output:"
        tail -30 "$output_log"
        
        return 1
    fi
}

# ===== Main =====
print_header "Surrogate Contact Model Test Suite"

# Check prerequisites
check_prerequisites

# Determine which tests to run
TEST_NUM=${1:-0}  # 0 = all, 1 = test 01, 2 = test 02

# Run tests
FAILED=0

if [ "$TEST_NUM" -eq 0 ] || [ "$TEST_NUM" -eq 1 ]; then
    if ! run_test 1 "Cube (L=1m) Falling on Plane" "$TEST_DIR/test_01_cube_1m_falling.in"; then
        FAILED=$((FAILED + 1))
    fi
fi

if [ "$TEST_NUM" -eq 0 ] || [ "$TEST_NUM" -eq 2 ]; then
    if ! run_test 2 "Cube (L=1cm) Falling on Plane" "$TEST_DIR/test_02_cube_1cm_falling.in"; then
        FAILED=$((FAILED + 1))
    fi
fi

# Summary
print_header "Test Summary"

if [ $FAILED -eq 0 ]; then
    print_success "All tests passed!"
    echo ""
    echo "Results location: $TEST_DIR/output/"
    echo "Check output files for detailed results."
    exit 0
else
    print_error "$FAILED test(s) failed"
    echo ""
    echo "Check log files in: $TEST_DIR/output/"
    exit 1
fi

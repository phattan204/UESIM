#!/bin/bash

# 5G UE Simulation gNB Integration Test Runner
# This script runs the gNB integration tests and captures network traffic

echo "5G UE Simulation gNB Integration Test Runner"
echo "============================================"

# Check if running on RHEL 8.5
if [ ! -f /etc/redhat-release ]; then
    echo "Warning: Not running on RHEL. Some features may not work as expected."
elif ! grep -q "Red Hat Enterprise Linux.*8\.5" /etc/redhat-release; then
    echo "Warning: Not running on RHEL 8.5. Some optimizations may be missing."
fi

# Check prerequisites
check_prerequisites() {
    echo "Checking prerequisites..."
    
    if ! command -v wireshark &> /dev/null; then
        echo "Error: Wireshark not found. Please install wireshark-cli"
        echo "  sudo yum install wireshark-cli"
        return 1
    fi
    
    if ! command -v tshark &> /dev/null; then
        echo "Error: tshark not found. Please install wireshark-cli"
        echo "  sudo yum install wireshark-cli"
        return 1
    fi
    
    if ! command -v tcpdump &> /dev/null; then
        echo "Error: tcpdump not found. Please install tcpdump"
        echo "  sudo yum install tcpdump"
        return 1
    fi
    
    echo "✓ All prerequisites satisfied"
    return 0
}

# Create capture directory
create_capture_dir() {
    local capture_dir="${1:-./captures}"
    mkdir -p "$capture_dir"
    echo "✓ Capture directory created: $capture_dir"
}

# Run OAI-O-RAN tests
run_oai_tests() {
    local capture_dir="${1:-./captures}"
    echo "Running OAI-O-RAN Integration Tests..."
    echo "======================================"
    
    create_capture_dir "$capture_dir"
    
    if [ ! -f "./test_gnb_oai" ]; then
        echo "Building OAI test executable..."
        make test-gnb-oai
        if [ $? -ne 0 ]; then
            echo "Error: Failed to build OAI test executable"
            return 1
        fi
    fi
    
    echo "Executing OAI-O-RAN tests with capture directory: $capture_dir"
    ./test_gnb_oai
    
    local result=$?
    if [ $result -eq 0 ]; then
        echo "✓ OAI-O-RAN tests completed successfully"
    else
        echo "✗ OAI-O-RAN tests failed with exit code: $result"
    fi
    
    return $result
}

# Run Commercial O-RAN tests
run_oran_tests() {
    local capture_dir="${1:-./captures}"
    echo "Running Commercial O-RAN Integration Tests..."
    echo "============================================"
    
    create_capture_dir "$capture_dir"
    
    if [ ! -f "./test_gnb_oran" ]; then
        echo "Building Commercial O-RAN test executable..."
        make test-gnb-oran
        if [ $? -ne 0 ]; then
            echo "Error: Failed to build Commercial O-RAN test executable"
            return 1
        fi
    fi
    
    echo "Executing Commercial O-RAN tests with capture directory: $capture_dir"
    ./test_gnb_oran
    
    local result=$?
    if [ $result -eq 0 ]; then
        echo "✓ Commercial O-RAN tests completed successfully"
    else
        echo "✗ Commercial O-RAN tests failed with exit code: $result"
    fi
    
    return $result
}

# Run all gNB integration tests
run_all_tests() {
    local capture_dir="${1:-./captures}"
    echo "Running All gNB Integration Tests..."
    echo "==================================="
    
    local oai_result=0
    local oran_result=0
    
    run_oai_tests "$capture_dir"
    oai_result=$?
    
    run_oran_tests "$capture_dir"
    oran_result=$?
    
    echo ""
    echo "Test Summary:"
    echo "============="
    echo "OAI-O-RAN Tests: $([ $oai_result -eq 0 ] && echo "PASSED" || echo "FAILED")"
    echo "Commercial O-RAN Tests: $([ $oran_result -eq 0 ] && echo "PASSED" || echo "FAILED")"
    
    if [ $oai_result -eq 0 ] && [ $oran_result -eq 0 ]; then
        echo "Overall Result: SUCCESS ✓"
        return 0
    else
        echo "Overall Result: FAILED ✗"
        return 1
    fi
}

# Show usage
show_usage() {
    echo "Usage: $0 [OPTIONS] {oai|oran|all}"
    echo ""
    echo "Options:"
    echo "  -c, --capture-dir DIR    Specify capture directory (default: ./captures)"
    echo "  -h, --help              Show this help message"
    echo ""
    echo "Targets:"
    echo "  oai    Run OAI-O-RAN integration tests"
    echo "  oran   Run Commercial O-RAN integration tests"
    echo "  all    Run all gNB integration tests"
    echo ""
    echo "Examples:"
    echo "  $0 oai                    # Run OAI tests with default capture dir"
    echo "  $0 -c /tmp/captures oran  # Run O-RAN tests with custom capture dir"
    echo "  $0 all                    # Run all tests"
}

# Main execution
main() {
    local target=""
    local capture_dir="./captures"
    
    # Parse command line arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            -c|--capture-dir)
                capture_dir="$2"
                shift 2
                ;;
            -h|--help)
                show_usage
                exit 0
                ;;
            oai|oran|all)
                target="$1"
                shift
                ;;
            *)
                echo "Unknown option: $1"
                show_usage
                exit 1
                ;;
        esac
    done
    
    # Check prerequisites
    check_prerequisites
    if [ $? -ne 0 ]; then
        exit 1
    fi
    
    # Validate target
    if [ -z "$target" ]; then
        echo "Error: No target specified"
        show_usage
        exit 1
    fi
    
    # Run tests based on target
    case $target in
        oai)
            run_oai_tests "$capture_dir"
            ;;
        oran)
            run_oran_tests "$capture_dir"
            ;;
        all)
            run_all_tests "$capture_dir"
            ;;
        *)
            echo "Error: Unknown target: $target"
            show_usage
            exit 1
            ;;
    esac
}

# Run main function
main "$@"
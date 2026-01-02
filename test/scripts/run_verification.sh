#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"

VERBOSE=""
HELP=false

for arg in "$@"; do
    case $arg in
        -v|--verbose)
            VERBOSE="-v"
            shift
            ;;
        -h|--help)
            HELP=true
            shift
            ;;
        *)
            ;;
    esac
done

if [ "$HELP" = true ]; then
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -v, --verbose    Enable verbose logging output"
    echo "  -h, --help       Show this help message"
    echo ""
    exit 0
fi

echo "=================================="
echo "  Fan Calculator Verifier"
echo "=================================="
echo ""

# Check if build directory exists
if [ ! -d "$BUILD_DIR" ]; then
    echo "Error: Build directory not found: $BUILD_DIR"
    echo "Please run: cd build && cmake .. && make"
    exit 1
fi

# Build the verification tool
echo "Building verification tool..."
cd "$BUILD_DIR"
make verify_fan_calculator

if [ $? -ne 0 ]; then
    echo "Error: Build failed"
    exit 1
fi

echo ""
echo "Build successful!"
echo ""

# Check if data/record directory exists
RECORD_DIR="$PROJECT_ROOT/data/record"
if [ ! -d "$RECORD_DIR" ]; then
    echo "Error: Record directory not found: $RECORD_DIR"
    exit 1
fi

# Count JSON files
RECORD_COUNT=$(find "$RECORD_DIR" -maxdepth 1 -name "*.json" | wc -l)
echo "Found $RECORD_COUNT record files in $RECORD_DIR"
echo ""

# Create log directory
LOG_DIR="$PROJECT_ROOT/logs"
mkdir -p "$LOG_DIR"

# Generate timestamp for log files
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
OUTPUT_LOG="$LOG_DIR/verification_${TIMESTAMP}.log"
GLOG_DIR="$LOG_DIR/glog_${TIMESTAMP}"

# Create glog directory if verbose mode is enabled
if [ -n "$VERBOSE" ]; then
    mkdir -p "$GLOG_DIR"
fi

# Run the verification tool
echo "Running verification..."
if [ -n "$VERBOSE" ]; then
    echo "(Verbose mode enabled)"
    echo "Output log: $OUTPUT_LOG"
    echo "Glog directory: $GLOG_DIR"
fi
echo ""

cd "$PROJECT_ROOT"

# Export glog environment variables
export GLOG_log_dir="$GLOG_DIR"
export GLOG_alsologtostderr=1

# Run with tee to show output and save to file
if [ -n "$VERBOSE" ]; then
    "$BUILD_DIR/test/scripts/verify_fan_calculator" $VERBOSE "$RECORD_DIR" 2>&1 | tee "$OUTPUT_LOG"
else
    "$BUILD_DIR/test/scripts/verify_fan_calculator" "$RECORD_DIR" 2>&1 | tee "$OUTPUT_LOG"
fi

echo ""
echo "Verification complete!"
echo "Output saved to: $OUTPUT_LOG"
if [ -n "$VERBOSE" ] && [ -d "$GLOG_DIR" ]; then
    GLOG_FILE_COUNT=$(find "$GLOG_DIR" -type f | wc -l)
    if [ $GLOG_FILE_COUNT -gt 0 ]; then
        echo "Glog files saved to: $GLOG_DIR"
        echo "  (Found $GLOG_FILE_COUNT log file(s))"
    fi
fi

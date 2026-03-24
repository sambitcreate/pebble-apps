#!/bin/bash
# Static analysis test suite for all Pebble apps
# Requires: cppcheck (brew install cppcheck)
# Optional: pebble SDK for full build tests

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PASS=0
FAIL=0
ERRORS=""

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[0;33m'
NC='\033[0m'

echo "======================================"
echo "  Pebble Apps Test Suite"
echo "======================================"
echo ""

# Check for cppcheck
if ! command -v cppcheck &> /dev/null; then
    echo -e "${RED}ERROR: cppcheck not found. Install with: brew install cppcheck${NC}"
    exit 1
fi

echo "Using cppcheck $(cppcheck --version 2>&1 | head -1)"
echo ""

# Test 1: cppcheck static analysis on all apps
echo "--- Static Analysis (cppcheck) ---"
for app_dir in "$SCRIPT_DIR"/*/; do
    app=$(basename "$app_dir")
    src="$app_dir/src/main.c"

    # Skip non-app directories
    [ ! -f "$src" ] && continue

    output=$(cppcheck --enable=warning,performance,portability \
        --suppress=missingIncludeSystem \
        --suppress=missingInclude \
        --suppress=unusedFunction \
        --suppress=checkersReport \
        --suppress=constParameterCallback \
        --suppress=constParameterPointer \
        --suppress=normalCheckLevelMaxBranches \
        --suppress=unmatchedSuppression \
        --error-exitcode=1 \
        "$src" 2>&1)

    if [ $? -eq 0 ]; then
        echo -e "  ${GREEN}PASS${NC}  $app"
        PASS=$((PASS + 1))
    else
        echo -e "  ${RED}FAIL${NC}  $app"
        ERRORS="$ERRORS\n--- $app ---\n$output"
        FAIL=$((FAIL + 1))
    fi
done

echo ""

# Test 2: Verify project structure
echo "--- Project Structure ---"
for app_dir in "$SCRIPT_DIR"/*/; do
    app=$(basename "$app_dir")
    [ ! -f "$app_dir/src/main.c" ] && continue

    missing=""
    [ ! -f "$app_dir/package.json" ] && missing="$missing package.json"
    [ ! -f "$app_dir/wscript" ] && missing="$missing wscript"
    [ ! -f "$app_dir/CLAUDE.md" ] && missing="$missing CLAUDE.md"
    [ ! -f "$app_dir/src/main.c" ] && missing="$missing src/main.c"

    if [ -z "$missing" ]; then
        echo -e "  ${GREEN}PASS${NC}  $app — all files present"
        PASS=$((PASS + 1))
    else
        echo -e "  ${RED}FAIL${NC}  $app — missing:$missing"
        FAIL=$((FAIL + 1))
    fi
done

echo ""

# Test 3: Verify package.json has correct target platforms
echo "--- Platform Targeting ---"
for app_dir in "$SCRIPT_DIR"/*/; do
    app=$(basename "$app_dir")
    pkg="$app_dir/package.json"
    [ ! -f "$pkg" ] && continue

    has_diorite=$(grep -c '"diorite"' "$pkg" || true)
    has_basalt=$(grep -c '"basalt"' "$pkg" || true)
    has_emery=$(grep -c '"emery"' "$pkg" || true)

    if [ "$has_diorite" -ge 1 ] && [ "$has_basalt" -ge 1 ] && [ "$has_emery" -ge 1 ]; then
        echo -e "  ${GREEN}PASS${NC}  $app — targets diorite, basalt, emery"
        PASS=$((PASS + 1))
    else
        echo -e "  ${RED}FAIL${NC}  $app — missing platform targets"
        FAIL=$((FAIL + 1))
    fi
done

echo ""

# Test 4: Check for common C issues
echo "--- Code Quality Checks ---"
for app_dir in "$SCRIPT_DIR"/*/; do
    app=$(basename "$app_dir")
    src="$app_dir/src/main.c"
    [ ! -f "$src" ] && continue

    issues=""

    # Check that every _create has a matching _destroy
    creates=$(grep -co '_create(' "$src" || true)
    destroys=$(grep -co '_destroy(' "$src" || true)
    if [ "$creates" -ne "$destroys" ]; then
        issues="$issues create/destroy mismatch($creates/$destroys)"
    fi

    # Check for main() entry point
    has_main=$(grep -c 'int main(void)' "$src" || true)
    if [ "$has_main" -lt 1 ]; then
        issues="$issues missing-main"
    fi

    # Check for app_event_loop
    has_loop=$(grep -c 'app_event_loop()' "$src" || true)
    if [ "$has_loop" -lt 1 ]; then
        issues="$issues missing-app_event_loop"
    fi

    if [ -z "$issues" ]; then
        echo -e "  ${GREEN}PASS${NC}  $app"
        PASS=$((PASS + 1))
    else
        echo -e "  ${YELLOW}WARN${NC}  $app —$issues"
        # Warnings don't count as failures
        PASS=$((PASS + 1))
    fi
done

echo ""

# Test 5: SDK build test (only if pebble CLI is available)
if command -v pebble &> /dev/null; then
    echo "--- SDK Build Test ---"
    for app_dir in "$SCRIPT_DIR"/*/; do
        app=$(basename "$app_dir")
        [ ! -f "$app_dir/src/main.c" ] && continue

        cd "$app_dir"
        if pebble build 2>&1 | grep -q "finished successfully"; then
            echo -e "  ${GREEN}PASS${NC}  $app — build succeeded"
            PASS=$((PASS + 1))
        else
            echo -e "  ${RED}FAIL${NC}  $app — build failed"
            FAIL=$((FAIL + 1))
        fi
        cd "$SCRIPT_DIR"
    done
    echo ""
else
    echo -e "${YELLOW}SKIP${NC}  SDK build tests (pebble CLI not installed)"
    echo "       Install with: uv tool install pebble-tool --python 3.13"
    echo ""
fi

# Summary
echo "======================================"
echo -e "  Results: ${GREEN}$PASS passed${NC}, ${RED}$FAIL failed${NC}"
echo "======================================"

if [ -n "$ERRORS" ]; then
    echo ""
    echo "--- Error Details ---"
    echo -e "$ERRORS"
fi

exit $FAIL

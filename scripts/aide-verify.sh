#!/bin/bash
#
# aide-verify.sh — three-stage verification loop for agentic use.
#
# Runs BUILD, FORMAT, and TEST in fixed order, failing fast on first failure.
# Each stage prints its result as the last line: [STAGE] PASS or [STAGE] FAIL.
# Agents can grep for '[BUILD]', '[FORMAT]', '[TEST]' to get the definitive result.
#
# FORMAT FAIL: clang-format is applied in-place before the diff check. If the
# diff is non-empty, the formatted files remain in the working tree — commit
# the formatting fix before re-running.
#

set -uo pipefail

# ---- BUILD ------------------------------------------------------------------

echo "==> Building..."
if make build; then
    echo "[BUILD] PASS"
else
    echo "[BUILD] FAIL"
    exit 1
fi

# ---- FORMAT -----------------------------------------------------------------

echo "==> Checking code format..."
make code-format
if git diff --exit-code -- src/main src/test; then
    echo "[FORMAT] PASS"
else
    echo "[FORMAT] FAIL"
    exit 1
fi

# ---- TEST -------------------------------------------------------------------

echo "==> Running tests..."
if make run-tests; then
    echo "[TEST] PASS"
else
    echo "[TEST] FAIL"
    exit 1
fi

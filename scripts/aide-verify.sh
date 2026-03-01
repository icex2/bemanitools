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

# ---- TOOLING CHECK ----------------------------------------------------------

missing=()
for cmd in make i686-w64-mingw32-gcc x86_64-w64-mingw32-gcc zip clang-format git wine unzip; do
    command -v "$cmd" >/dev/null 2>&1 || missing+=("$cmd")
done

if [ ${#missing[@]} -gt 0 ]; then
    echo "[PREREQ] FAIL"
    echo "Missing required commands: ${missing[*]}"
    echo ""
    echo "Packages to install (Debian/Ubuntu):"
    echo "  mingw-w64          — i686-w64-mingw32-gcc, x86_64-w64-mingw32-gcc"
    echo "  clang-format       — clang-format"
    echo "  wine wine32        — wine (also enable i386: dpkg --add-architecture i386)"
    echo "  zip unzip          — zip, unzip"
    exit 1
fi

# ---- BUILD ------------------------------------------------------------------

echo "==> Building..."
if make all; then
    echo "[BUILD] PASS"
else
    echo "[BUILD] FAIL"
    exit 1
fi

# ---- FORMAT -----------------------------------------------------------------

echo "==> Checking code format..."
if ! make code-format; then
    echo "[FORMAT] FAIL"
    exit 1
fi
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

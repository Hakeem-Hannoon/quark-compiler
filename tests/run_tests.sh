#!/usr/bin/env bash
#
# Quark end-to-end tests.
#
#   tests/cases/NAME.qr        compiles, runs, and its stdout must match NAME.out
#   tests/errors/NAME.qr       must fail to compile with the message in NAME.err
#
# Usage: QUARKC=build/quarkc tests/run_tests.sh

set -uo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
quarkc=${QUARKC:-$root/build/quarkc}

if [ ! -x "$quarkc" ]; then
    echo "no quarkc at $quarkc (build it first, or set QUARKC)" >&2
    exit 1
fi

workdir=$(mktemp -d)
trap 'rm -rf "$workdir"' EXIT

passed=0
failed=0

fail() {
    printf 'FAIL %s\n' "$1"
    shift
    printf '     %s\n' "$@"
    failed=$((failed + 1))
}

pass() {
    printf 'ok   %s\n' "$1"
    passed=$((passed + 1))
}

# --- programs that must compile, run, and print the expected output ----------

for source in "$root"/tests/cases/*.qr; do
    [ -e "$source" ] || continue

    name=$(basename "$source" .qr)
    expected_file="$root/tests/cases/$name.out"

    if [ ! -f "$expected_file" ]; then
        fail "$name" "missing expected output file: $expected_file"
        continue
    fi

    actual=$("$quarkc" "$source" -o "$workdir/$name" --run 2>&1)
    status=$?

    if [ $status -ne 0 ]; then
        fail "$name" "exited with $status" "$actual"
        continue
    fi

    expected=$(cat "$expected_file")

    if [ "$actual" != "$expected" ]; then
        fail "$name" "expected: $expected" "actual:   $actual"
        continue
    fi

    pass "$name"
done

# --- programs that must be rejected, with the right message ------------------

for source in "$root"/tests/errors/*.qr; do
    [ -e "$source" ] || continue

    name=$(basename "$source" .qr)
    expected_file="$root/tests/errors/$name.err"

    if [ ! -f "$expected_file" ]; then
        fail "error/$name" "missing expected error file: $expected_file"
        continue
    fi

    actual=$("$quarkc" "$source" -o "$workdir/$name" 2>&1)
    status=$?

    if [ $status -eq 0 ]; then
        fail "error/$name" "compiled successfully, expected an error"
        continue
    fi

    expected=$(cat "$expected_file")

    case "$actual" in
        *"$expected"*)
            pass "error/$name"
            ;;
        *)
            fail "error/$name" "expected message containing: $expected" "actual: $actual"
            ;;
    esac
done

# --- the AST visualisers -----------------------------------------------------
#
# The text and DOT renderings are compared against goldens. The SVG is checked
# structurally instead: its exact geometry is not worth pinning, but every AST
# node must appear as exactly one box, and the document must be closed.

for source in "$root"/tests/emit/*.qr; do
    [ -e "$source" ] || continue

    name=$(basename "$source" .qr)

    for format in tree dot; do
        expected_file="$root/tests/emit/$name.$format"
        [ -f "$expected_file" ] || continue

        flag="--emit-ast"
        [ "$format" = dot ] && flag="--emit-dot"

        actual=$("$quarkc" "$source" "$flag" 2>&1)

        if [ "$actual" != "$(cat "$expected_file")" ]; then
            fail "emit/$name.$format" "output does not match $expected_file"
            continue
        fi

        pass "emit/$name.$format"
    done

    svg=$("$quarkc" "$source" --emit-svg 2>&1)
    nodes=$(printf '%s\n' "$svg" | grep -c '<rect class="box')
    expected_nodes=$("$quarkc" "$source" --emit-ast | grep -c .)

    if [ "$nodes" -ne "$expected_nodes" ]; then
        fail "emit/$name.svg" "drew $nodes boxes for $expected_nodes AST nodes"
    elif ! printf '%s' "$svg" | head -1 | grep -q '^<svg '; then
        fail "emit/$name.svg" "does not start with an <svg element"
    elif ! printf '%s' "$svg" | tail -1 | grep -q '^</svg>$'; then
        fail "emit/$name.svg" "is not closed"
    else
        pass "emit/$name.svg"
    fi
done

printf '\n%d passed, %d failed\n' "$passed" "$failed"

[ "$failed" -eq 0 ]

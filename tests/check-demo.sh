#!/usr/bin/env bash
set -euo pipefail

repository_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
nox_compiler="${NOX:-nox}"
actual_output="$(mktemp)"
trap 'rm -f "${actual_output}"' EXIT

cd "${repository_root}"
"${nox_compiler}" run "$@" > "${actual_output}"
diff -u tests/demo-expected.txt "${actual_output}"
echo "penumbra demo: output matches tests/demo-expected.txt"

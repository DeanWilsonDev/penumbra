#!/usr/bin/env bash
set -euo pipefail

nox_root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
demo_binary="$("${nox_root}/build.sh" "$@" | tail -n 1)"
actual_output="$(mktemp)"
trap 'rm -f "${actual_output}"' EXIT
"${demo_binary}" > "${actual_output}"
diff -u "${nox_root}/demo/expected.txt" "${actual_output}"
echo "penumbra nox demo: output matches demo/expected.txt"

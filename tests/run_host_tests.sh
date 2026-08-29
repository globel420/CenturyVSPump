#!/usr/bin/env bash
set -euo pipefail

test_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
test_binary="$(mktemp "${TMPDIR:-/tmp}/vgreen-stock-schedule.XXXXXX")"
trap 'rm -f "$test_binary"' EXIT

"${CXX:-c++}" \
  -std=c++14 \
  -Wall \
  -Wextra \
  -Werror \
  -pedantic \
  "$test_dir/test_stock_schedule.cpp" \
  -o "$test_binary"

"$test_binary"

#!/usr/bin/env bash
set -euo pipefail

test_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
test_binary="$(mktemp "${TMPDIR:-/tmp}/vgreen-stock-schedule.XXXXXX")"
trap 'rm -f "$test_binary"' EXIT

for test_source in test_stock_schedule.cpp test_manual_button.cpp; do
"${CXX:-c++}" \
  -std=c++14 \
  -Wall \
  -Wextra \
  -Werror \
  -pedantic \
  "$test_dir/$test_source" \
  -o "$test_binary"

"$test_binary"
done

"${CXX:-c++}" -std=c++14 -Wall -Wextra -Werror -pedantic \
  -I "$test_dir/stubs" \
  "$test_dir/test_controller.cpp" \
  "$test_dir/../components/vgreen_evo_controller/VGreenEvoController.cpp" \
  -o "$test_binary"
"$test_binary"

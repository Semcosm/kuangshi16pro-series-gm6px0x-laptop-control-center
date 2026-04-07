#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
project_dir="$repo_root/kuangshi16pro-lcc"

shell_files=(
  "$project_dir/scripts/dev-run.sh"
  "$project_dir/scripts/lint.sh"
  "$project_dir/scripts/smoke-test.sh"
  "$project_dir/tests/hardware/lib.sh"
  "$project_dir/tests/hardware/run_real_smoke.sh"
  "$project_dir/tests/integration/test_dbus_smoke.sh"
)

for file in "${shell_files[@]}"; do
  bash -n "$file"
done

if command -v shellcheck >/dev/null 2>&1; then
  shellcheck "${shell_files[@]}"
fi

if git -C "$repo_root" grep -nI '[[:blank:]]$' -- \
    .github/workflows \
    kuangshi16pro-lcc >/tmp/lcc-lint-trailing-whitespace.txt; then
  cat /tmp/lcc-lint-trailing-whitespace.txt
  exit 1
fi

rm -f /tmp/lcc-lint-trailing-whitespace.txt

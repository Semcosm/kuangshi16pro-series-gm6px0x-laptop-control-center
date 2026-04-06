#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
project_dir="$repo_root/kuangshi16pro-lcc"

cd "$repo_root"
make -C "$project_dir"

exec dbus-run-session -- "$project_dir/build/lccd" --user

#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
project_dir="$repo_root/kuangshi16pro-lcc"

cd "$repo_root"
make -C "$project_dir"
make -C "$project_dir" test

dbus-run-session -- sh -lc "
  '$project_dir/build/lccd' --user >/tmp/lccd.log 2>&1 &
  daemon=\$!
  sleep 1
  '$project_dir/build/lccctl' capabilities --user-bus
  '$project_dir/build/lccctl' mode set turbo --user-bus
  '$project_dir/build/lccctl' power set --pl1 75 --pl2 130 --user-bus
  '$project_dir/build/lccctl' fan apply --preset demo --user-bus
  '$project_dir/build/lccctl' state --user-bus
  kill \$daemon
  wait \$daemon 2>/dev/null || true
"

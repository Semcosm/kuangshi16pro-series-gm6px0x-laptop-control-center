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
  busctl --user call io.github.semcosm.Lcc1 /io/github/semcosm/Lcc1 io.github.semcosm.Lcc1.Manager GetCapabilities
  busctl --user call io.github.semcosm.Lcc1 /io/github/semcosm/Lcc1 io.github.semcosm.Lcc1.Manager SetProfile s turbo
  busctl --user call io.github.semcosm.Lcc1 /io/github/semcosm/Lcc1 io.github.semcosm.Lcc1.Power SetPowerLimits yyyy 75 130 200 5
  busctl --user call io.github.semcosm.Lcc1 /io/github/semcosm/Lcc1 io.github.semcosm.Lcc1.Manager GetState
  kill \$daemon
  wait \$daemon 2>/dev/null || true
"

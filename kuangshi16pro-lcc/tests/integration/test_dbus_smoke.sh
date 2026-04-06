#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
project_dir="$repo_root/kuangshi16pro-lcc"

make -C "$project_dir" all

export PROJECT_DIR="$project_dir"
dbus-run-session -- bash <<'EOF'
set -euo pipefail

daemon_pid=0
introspection_file="/tmp/lcc-dbus-introspect.txt"

cleanup() {
  if [[ "$daemon_pid" -gt 0 ]]; then
    kill "$daemon_pid" 2>/dev/null || true
    wait "$daemon_pid" 2>/dev/null || true
  fi
  rm -f "$introspection_file"
}

trap cleanup EXIT

LCC_BACKEND=mock "$PROJECT_DIR/build/lccd" --user >/tmp/lccd-integration.log 2>&1 &
daemon_pid=$!
sleep 1

busctl --user introspect io.github.semcosm.Lcc1 /io/github/semcosm/Lcc1 \
  >"$introspection_file"
grep -q 'io.github.semcosm.Lcc1.Manager' "$introspection_file"
grep -q 'GetState' "$introspection_file"
grep -q 'SetMode' "$introspection_file"
grep -q 'ApplyFanTable' "$introspection_file"
grep -q 'SetPowerLimits' "$introspection_file"

state_before="$("$PROJECT_DIR/build/lccctl" state --user-bus)"
printf '%s\n' "$state_before" | grep -q '"backend":"mock"'

"$PROJECT_DIR/build/lccctl" mode set turbo --user-bus >/tmp/lcc-mode.out
"$PROJECT_DIR/build/lccctl" power set --pl1 75 --pl2 130 --user-bus >/tmp/lcc-power.out
"$PROJECT_DIR/build/lccctl" fan apply --preset M4T1 --user-bus >/tmp/lcc-fan.out

state_after="$("$PROJECT_DIR/build/lccctl" state --user-bus)"
printf '%s\n' "$state_after" | grep -q '"backend":"mock"'
printf '%s\n' "$state_after" | grep -q '"profile":"turbo"'
printf '%s\n' "$state_after" | grep -q '"fan_table":"M4T1"'
printf '%s\n' "$state_after" | grep -q '"pl1":75'
printf '%s\n' "$state_after" | grep -q '"pl2":130'
EOF

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
fake_sysfs_root="/tmp/lcc-smoke-sysfs-$$"

cleanup() {
  if [[ "$daemon_pid" -gt 0 ]]; then
    kill "$daemon_pid" 2>/dev/null || true
    wait "$daemon_pid" 2>/dev/null || true
  fi
  rm -f "$introspection_file"
  rm -rf "$fake_sysfs_root"
}

trap cleanup EXIT

make_dir() {
  mkdir -p "$1"
}

write_text() {
  printf '%s\n' "$2" >"$1"
}

init_fake_sysfs() {
  make_dir "$fake_sysfs_root/class/hwmon/hwmon0"
  make_dir "$fake_sysfs_root/class/thermal/thermal_zone0"
  make_dir "$fake_sysfs_root/class/powercap/intel-rapl:0"
  make_dir "$fake_sysfs_root/firmware/acpi"

  write_text "$fake_sysfs_root/class/hwmon/hwmon0/temp1_input" "61000"
  write_text "$fake_sysfs_root/class/hwmon/hwmon0/temp2_input" "56000"
  write_text "$fake_sysfs_root/class/hwmon/hwmon0/fan1_input" "2480"
  write_text "$fake_sysfs_root/class/hwmon/hwmon0/fan2_input" "2310"
  write_text "$fake_sysfs_root/class/thermal/thermal_zone0/temp" "59000"
  write_text \
    "$fake_sysfs_root/class/powercap/intel-rapl:0/constraint_0_power_limit_uw" \
    "45000000"
  write_text \
    "$fake_sysfs_root/class/powercap/intel-rapl:0/constraint_1_power_limit_uw" \
    "90000000"
  write_text "$fake_sysfs_root/firmware/acpi/platform_profile" "performance"
}

start_daemon() {
  local log_file="$1"
  shift

  stop_daemon
  env "$@" "$PROJECT_DIR/build/lccd" --user >"$log_file" 2>&1 &
  daemon_pid=$!
  sleep 1
}

stop_daemon() {
  if [[ "$daemon_pid" -gt 0 ]]; then
    kill "$daemon_pid" 2>/dev/null || true
    wait "$daemon_pid" 2>/dev/null || true
    daemon_pid=0
  fi
}

init_fake_sysfs

start_daemon /tmp/lccd-integration.log LCC_BACKEND=mock

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

start_daemon /tmp/lccd-standard.log \
  LCC_BACKEND=standard \
  LCC_STANDARD_ROOT="$fake_sysfs_root"

standard_state_before="$("$PROJECT_DIR/build/lccctl" state --user-bus)"
printf '%s\n' "$standard_state_before" | grep -q '"backend":"standard"'
"$PROJECT_DIR/build/lccctl" mode set office --user-bus >/tmp/lcc-standard-mode.out
standard_state_after="$("$PROJECT_DIR/build/lccctl" state --user-bus)"
printf '%s\n' "$standard_state_after" | grep -q '"backend":"standard"'
printf '%s\n' "$standard_state_after" | grep -q '"last_apply_backend":"standard"'
printf '%s\n' "$standard_state_after" | grep -q '"last_apply_stage":"write-platform-profile"'
printf '%s\n' "$standard_state_after" | grep -q '"profile":"office"'

start_daemon /tmp/lccd-amw0.log \
  LCC_BACKEND=amw0 \
  LCC_AMW0_DRY_RUN=1

"$PROJECT_DIR/build/lccctl" power set --pl1 70 --pl2 120 --user-bus >/tmp/lcc-amw0-power.out
amw0_state="$("$PROJECT_DIR/build/lccctl" state --user-bus)"
printf '%s\n' "$amw0_state" | grep -q '"backend":"amw0"'
printf '%s\n' "$amw0_state" | grep -q '"last_apply_backend":"amw0"'
printf '%s\n' "$amw0_state" | grep -q '"last_apply_stage":"write-pl2"'
printf '%s\n' "$amw0_state" | grep -q '"pl1":70'
printf '%s\n' "$amw0_state" | grep -q '"pl2":120'

start_daemon /tmp/lccd-mixed.log \
  LCC_STANDARD_ROOT="$fake_sysfs_root" \
  LCC_AMW0_DRY_RUN=1

"$PROJECT_DIR/build/lccctl" power set --pl1 75 --pl2 130 --user-bus >/tmp/lcc-mixed-power.out
mixed_state="$("$PROJECT_DIR/build/lccctl" state --user-bus)"
printf '%s\n' "$mixed_state" | grep -q '"backend":"standard"'
printf '%s\n' "$mixed_state" | grep -q '"backend_selected":"standard"'
printf '%s\n' "$mixed_state" | grep -q '"apply_power_limits":"amw0"'
printf '%s\n' "$mixed_state" | grep -q '"last_apply_backend":"amw0"'
printf '%s\n' "$mixed_state" | grep -q '"pl1":75'
printf '%s\n' "$mixed_state" | grep -q '"pl2":130'
EOF

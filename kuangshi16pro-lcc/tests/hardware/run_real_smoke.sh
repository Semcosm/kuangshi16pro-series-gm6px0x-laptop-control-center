#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=kuangshi16pro-lcc/tests/hardware/lib.sh
source "$script_dir/lib.sh"

project_dir="$(lcc_hw_project_dir)"
lccctl_bin="$project_dir/build/lccctl"
fixture_fan="$script_dir/fixtures/fan-balanced.json"
service_name="lccd.service"
matrix="${LCC_HW_MATRIX:-mixed}"
keep_artifacts="${LCC_KEEP_ARTIFACTS:-1}"
journal_lines="${LCC_JOURNAL_LINES:-200}"
skip_fan="${LCC_SKIP_FAN:-0}"
skip_power="${LCC_SKIP_POWER:-0}"
artifacts_root="$project_dir/artifacts/hardware-smoke"
run_dir="$artifacts_root/$(lcc_hw_now_stamp)"
override_dir="/run/systemd/system/${service_name}.d"
override_file="${override_dir}/50-hardware-smoke.conf"
override_tmp="$(mktemp /tmp/lcc-hardware-smoke.XXXXXX)"
initial_active=""
run_failed=0

normalize_matrix() {
  case "$1" in
    mixed|mixed-availability)
      printf 'mixed\n'
      ;;
    standard-only|amw0-forced|dry-run)
      printf '%s\n' "$1"
      ;;
    *)
      return 1
      ;;
  esac
}

validate_service_environment() {
  local environment_file="$1"

  case "$matrix" in
    standard-only)
      grep -q 'LCC_BACKEND=standard' "$environment_file"
      ;;
    amw0-forced)
      grep -q 'LCC_BACKEND=amw0' "$environment_file"
      ;;
    dry-run)
      grep -q 'LCC_AMW0_DRY_RUN=1' "$environment_file"
      ;;
    mixed)
      if grep -q 'LCC_BACKEND=' "$environment_file"; then
        return 1
      fi
      if grep -q 'LCC_AMW0_DRY_RUN=' "$environment_file"; then
        return 1
      fi
      ;;
  esac
}

validate_initial_state() {
  local state_file="$1"

  case "$matrix" in
    standard-only)
      grep -q '"backend":"standard"' "$state_file"
      grep -q '"apply_mode":false' "$state_file"
      grep -q '"apply_power_limits":false' "$state_file"
      grep -q '"apply_fan_table":false' "$state_file"
      ;;
    amw0-forced)
      grep -q '"backend":"amw0"' "$state_file"
      grep -q '"backend_selected":"amw0"' "$state_file"
      ;;
    mixed|dry-run)
      grep -q '"backend":"standard"' "$state_file"
      grep -q '"backend_selected":"standard"' "$state_file"
      ;;
  esac
}

step_allows_failure() {
  local step_name="$1"

  case "$matrix:$step_name" in
    standard-only:03-mode-office|standard-only:05-mode-turbo|standard-only:07-power-set|standard-only:09-fan-apply)
      return 0
      ;;
    dry-run:03-mode-office|dry-run:05-mode-turbo|dry-run:07-power-set|dry-run:09-fan-apply)
      return 0
      ;;
    *)
      return 1
      ;;
  esac
}

cleanup() {
  local status=$?

  rm -f "$override_tmp"

  if [[ -f "$override_file" ]]; then
    sudo rm -f "$override_file"
    sudo systemctl daemon-reload
    if [[ "$initial_active" == "active" ]]; then
      sudo systemctl restart "$service_name" >/dev/null
    else
      sudo systemctl stop "$service_name" >/dev/null 2>&1 || true
    fi
  fi

  if [[ "$status" -eq 0 && "$run_failed" -eq 0 ]] &&
      ! lcc_hw_is_truthy "$keep_artifacts"; then
    rm -rf "$run_dir"
  fi
}

run_step() {
  local step_name="$1"
  shift
  local step_dir="$run_dir/$step_name"
  local journal_cursor=""
  local command_rc=0
  local state_rc=0
  local -a command=("$@")

  mkdir -p "$step_dir"
  printf '%q ' "${command[@]}" >"$step_dir/command.txt"
  printf '\n' >>"$step_dir/command.txt"

  lcc_hw_log "step $step_name"
  journal_cursor="$(lcc_hw_journal_cursor "$service_name")"

  set +e
  "${command[@]}" >"$step_dir/stdout.txt" 2>"$step_dir/stderr.txt"
  command_rc=$?
  set -e
  printf '%s\n' "$command_rc" >"$step_dir/exit_code.txt"

  set +e
  "$lccctl_bin" state --system-bus >"$step_dir/state.json" \
    2>"$step_dir/state.stderr.txt"
  state_rc=$?
  set -e
  if [[ "$state_rc" -ne 0 ]]; then
    printf '{\"error\":\"state read failed\"}\n' >"$step_dir/state.json"
  fi

  lcc_hw_capture_journal "$service_name" "$journal_cursor" "$journal_lines" \
    "$step_dir/journal.log"
  lcc_hw_write_summary "$step_dir/state.json" "$step_dir/summary.txt" \
    "$command_rc" "$state_rc"
  lcc_hw_print_summary "$step_dir/summary.txt"

  if [[ "$state_rc" -ne 0 ]]; then
    run_failed=1
    return 1
  fi
  if ! lcc_hw_require_state_contract "$step_dir/state.json"; then
    run_failed=1
    return 1
  fi

  if [[ "$command_rc" -ne 0 ]] && ! step_allows_failure "$step_name"; then
    run_failed=1
    return 1
  fi

  return 0
}

trap cleanup EXIT

matrix="$(normalize_matrix "$matrix")"

if [[ "$EUID" -eq 0 ]]; then
  printf 'run this script as a regular user so mutating calls stay on the Polkit path\n' >&2
  exit 1
fi

lcc_hw_require_command make
lcc_hw_require_command sudo
lcc_hw_require_command systemctl
lcc_hw_require_command journalctl
lcc_hw_require_command grep
lcc_hw_require_command sed
lcc_hw_require_file "$fixture_fan"

mkdir -p "$artifacts_root"
mkdir -p "$run_dir"

make -C "$project_dir" all
lcc_hw_require_file "$lccctl_bin"

sudo -v

if ! sudo systemctl cat "$service_name" >/dev/null 2>&1; then
  printf 'systemd unit not installed: %s\n' "$service_name" >&2
  exit 1
fi

initial_active="$(systemctl is-active "$service_name" 2>/dev/null || true)"

lcc_hw_write_matrix_override "$matrix" "$override_tmp"
sudo install -d -m 0755 "$override_dir"
sudo install -m 0644 "$override_tmp" "$override_file"
sudo systemctl daemon-reload
sudo systemctl restart "$service_name"
sudo systemctl is-active --quiet "$service_name"

cat >"$run_dir/metadata.txt" <<EOF
matrix=$matrix
service=$service_name
user=$(id -un)
run_dir=$run_dir
fixture_fan=$fixture_fan
journal_lines=$journal_lines
skip_power=$skip_power
skip_fan=$skip_fan
EOF
cp "$override_tmp" "$run_dir/service-override.conf"
sudo systemctl show -p Environment "$service_name" \
  | tee "$run_dir/service-environment.txt" >/dev/null

lcc_hw_log "artifacts: $run_dir"
lcc_hw_log "matrix: $matrix"

if ! validate_service_environment "$run_dir/service-environment.txt"; then
  printf 'matrix service environment validation failed for %s\n' "$matrix" >&2
  run_failed=1
  exit 1
fi

run_step 01-state-initial "$lccctl_bin" state --system-bus
if ! validate_initial_state "$run_dir/01-state-initial/state.json"; then
  printf 'initial state validation failed for %s\n' "$matrix" >&2
  run_failed=1
  exit 1
fi
run_step 02-capabilities "$lccctl_bin" capabilities --system-bus
run_step 03-mode-office "$lccctl_bin" mode set office --system-bus
run_step 04-state-after-office "$lccctl_bin" state --system-bus
run_step 05-mode-turbo "$lccctl_bin" mode set turbo --system-bus
run_step 06-state-after-turbo "$lccctl_bin" state --system-bus

if ! lcc_hw_is_truthy "$skip_power"; then
  run_step 07-power-set "$lccctl_bin" power set --pl1 55 --pl2 95 --system-bus
  run_step 08-state-after-power "$lccctl_bin" state --system-bus
fi

if ! lcc_hw_is_truthy "$skip_fan"; then
  run_step 09-fan-apply "$lccctl_bin" fan apply --file "$fixture_fan" \
    --system-bus
  run_step 10-state-after-fan "$lccctl_bin" state --system-bus
fi

lcc_hw_log "completed"

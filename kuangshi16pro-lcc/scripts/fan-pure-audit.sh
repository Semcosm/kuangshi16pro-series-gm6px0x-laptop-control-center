#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
project_dir="$repo_root/kuangshi16pro-lcc"
mode_audit_script="$repo_root/scripts/amw0/amw0-mode-button-audit.sh"
fan_file="$project_dir/data/fan-tables/fan-balanced.json"
preset_name=""
use_user_bus="0"
settle_seconds="1"
mode="apply"
out_dir=""

lcc_fan_pure_audit_usage() {
  cat <<'EOF'
Usage:
  fan-pure-audit.sh apply [OUT_DIR] [OPTIONS]

Options:
  --file PATH      Use a fan table file. Default:
                   kuangshi16pro-lcc/data/fan-tables/fan-balanced.json
  --preset NAME    Use a built-in fan preset instead of --file.
  --settle SEC     Wait SEC seconds after fan apply before capturing "after".
                   Default: 1
  --user-bus       Use the user bus for fan apply.
  --system-bus     Use the system bus for fan apply. Default.
  -h, --help       Show this help.

Defaults:
  OUT_DIR defaults to /tmp/lcc-fan-pure-audit-<timestamp>

What it captures:
  - lccctl status before and after
  - OEM mode bytes plus fan-table tail bytes via amw0-mode-button-audit snapshot
  - turbostat header truth for package limits and temperature target
  - a safety summary that flags forbidden changes

Safety contract:
  Normal fan apply should only change the active fan table.
  It must not change:
  - profile
  - PL1 / PL2 / PL4 / TCC in lccctl state
  - OEM mode bytes
  - turbostat-reported package limits
  - turbostat-reported MSR_IA32_TEMPERATURE_TARGET
  Fan-table tail bytes are reported separately because they may explain vendor
  UI mode changes even when the core OEM mode bytes remain stable.
EOF
}

lcc_fan_pure_audit_stamp() {
  date '+%Y%m%d-%H%M%S'
}

lcc_fan_pure_audit_log() {
  printf '[fan-pure-audit] %s\n' "$*"
}

lcc_fan_pure_audit_find_lccctl() {
  if command -v lccctl >/dev/null 2>&1; then
    command -v lccctl
    return 0
  fi
  if [[ -x "$project_dir/build/lccctl" ]]; then
    printf '%s/build/lccctl\n' "$project_dir"
    return 0
  fi

  return 1
}

lcc_fan_pure_audit_json_get() {
  local file="$1"
  local key="$2"
  local match=""
  local value=""

  if [[ ! -f "$file" ]]; then
    printf 'none\n'
    return 0
  fi

  match="$(grep -o "\"$key\":\\(\"[^\"]*\"\\|null\\|true\\|false\\|[0-9][0-9]*\\)" \
    "$file" | head -n 1 || true)"
  if [[ -z "$match" ]]; then
    printf 'missing\n'
    return 0
  fi

  value="${match#*:}"
  value="${value#\"}"
  value="${value%\"}"
  if [[ -z "$value" || "$value" == "null" ]]; then
    value="none"
  fi

  printf '%s\n' "$value"
}

lcc_fan_pure_audit_json_get_object_key() {
  local file="$1"
  local object="$2"
  local key="$3"
  local match=""
  local value=""

  if [[ ! -f "$file" ]]; then
    printf 'none\n'
    return 0
  fi

  match="$(grep -o "\"$object\":{[^}]*\"$key\":\\(\"[^\"]*\"\\|null\\|true\\|false\\|[0-9][0-9]*\\)" \
    "$file" | head -n 1 || true)"
  if [[ -z "$match" ]]; then
    printf 'missing\n'
    return 0
  fi

  value="${match##*:}"
  value="${value#\"}"
  value="${value%\"}"
  if [[ -z "$value" || "$value" == "null" ]]; then
    value="none"
  fi

  printf '%s\n' "$value"
}

lcc_fan_pure_audit_write_state_summary() {
  local state_file="$1"
  local summary_file="$2"

  {
    printf 'profile=%s\n' \
      "$(lcc_fan_pure_audit_json_get_object_key "$state_file" "effective" "profile")"
    printf 'fan_table=%s\n' \
      "$(lcc_fan_pure_audit_json_get_object_key "$state_file" "effective" "fan_table")"
    printf 'pl1=%s\n' \
      "$(lcc_fan_pure_audit_json_get_object_key "$state_file" "power" "pl1")"
    printf 'pl2=%s\n' \
      "$(lcc_fan_pure_audit_json_get_object_key "$state_file" "power" "pl2")"
    printf 'pl4=%s\n' \
      "$(lcc_fan_pure_audit_json_get_object_key "$state_file" "power" "pl4")"
    printf 'tcc_offset=%s\n' \
      "$(lcc_fan_pure_audit_json_get_object_key "$state_file" "power" "tcc_offset")"
    printf 'last_apply_backend=%s\n' \
      "$(lcc_fan_pure_audit_json_get "$state_file" "last_apply_backend")"
    printf 'last_apply_stage=%s\n' \
      "$(lcc_fan_pure_audit_json_get "$state_file" "last_apply_stage")"
    printf 'last_apply_error=%s\n' \
      "$(lcc_fan_pure_audit_json_get "$state_file" "last_apply_error")"
    printf 'vendor_fan_level=%s\n' \
      "$(lcc_fan_pure_audit_json_get_object_key "$state_file" "thermal" "vendor_fan_level")"
  } >"$summary_file"
}

lcc_fan_pure_audit_capture_status() {
  local snapshot_dir="$1"

  "$lccctl_bin" status >"$snapshot_dir/state.stdout" 2>"$snapshot_dir/state.stderr"
  lcc_fan_pure_audit_write_state_summary "$snapshot_dir/state.stdout" \
    "$snapshot_dir/state-summary.txt"
}

lcc_fan_pure_audit_capture_mode_bytes() {
  local snapshot_dir="$1"
  local raw_file="$snapshot_dir/mode.stdout"
  local summary_file="$snapshot_dir/mode-summary.txt"
  local log_path=""

  sudo bash "$mode_audit_script" --changes-only >"$raw_file" 2>"$snapshot_dir/mode.stderr"
  grep 'MAFAN_CTL=' "$raw_file" | tail -n 1 >"$summary_file" || true
  if [[ -s "$summary_file" ]]; then
    return 0
  fi

  log_path="$(
    sed -n 's/^Done\. Review //p' "$raw_file" | tail -n 1 || true
  )"
  if [[ -n "$log_path" && -f "$log_path" ]]; then
    printf '%s\n' "$log_path" >"$snapshot_dir/mode.logpath"
    grep 'MAFAN_CTL=' "$log_path" | tail -n 1 >"$summary_file" || true
  fi
  if [[ ! -s "$summary_file" ]]; then
    printf 'missing\n' >"$summary_file"
  fi
}

lcc_fan_pure_audit_capture_turbostat() {
  local snapshot_dir="$1"
  local raw_file="$snapshot_dir/turbostat.stdout"
  local summary_file="$snapshot_dir/turbostat-summary.txt"
  local status=0

  set +e
  timeout 3s sudo turbostat --Summary --show PkgWatt,PkgTmp,Bzy_MHz --interval 1 \
    >"$raw_file" 2>"$snapshot_dir/turbostat.stderr"
  status=$?
  set -e
  printf '%s\n' "$status" >"$snapshot_dir/turbostat.exit"

  grep -E 'intel-rapl:0: package-0|PKG Limit #1:|PKG Limit #2:|MSR_IA32_TEMPERATURE_TARGET:' \
    "$raw_file" >"$summary_file" || printf 'missing\n' >"$summary_file"
}

lcc_fan_pure_audit_capture_snapshot() {
  local snapshot_dir="$1"

  mkdir -p "$snapshot_dir"
  lcc_fan_pure_audit_capture_status "$snapshot_dir"
  lcc_fan_pure_audit_capture_mode_bytes "$snapshot_dir"
  lcc_fan_pure_audit_capture_turbostat "$snapshot_dir"
}

lcc_fan_pure_audit_diff_optional() {
  local before_file="$1"
  local after_file="$2"
  local out_file="$3"

  if ! diff -u "$before_file" "$after_file" >"$out_file"; then
    return 0
  fi
  : >"$out_file"
}

lcc_fan_pure_audit_extract_line() {
  local file="$1"
  local pattern="$2"
  local line=""

  line="$(grep -F "$pattern" "$file" | head -n 1 || true)"
  if [[ -z "$line" ]]; then
    printf 'missing\n'
    return 0
  fi

  printf '%s\n' "$line"
}

lcc_fan_pure_audit_mode_token() {
  local file="$1"
  local key="$2"
  local line=""
  local match=""

  line="$(cat "$file" 2>/dev/null || true)"
  if [[ -z "$line" ]]; then
    printf 'missing\n'
    return 0
  fi

  match="$(printf '%s\n' "$line" | grep -o "${key}=[^ ]*" | head -n 1 || true)"
  if [[ -z "$match" ]]; then
    printf 'missing\n'
    return 0
  fi

  printf '%s\n' "${match#*=}"
}

lcc_fan_pure_audit_summary_value() {
  local file="$1"
  local key="$2"
  local match=""

  match="$(grep -E "^${key}=" "$file" | head -n 1 || true)"
  if [[ -z "$match" ]]; then
    printf 'missing\n'
    return 0
  fi

  printf '%s\n' "${match#*=}"
}

lcc_fan_pure_audit_write_safety_summary() {
  local before_dir="$1"
  local after_dir="$2"
  local summary_file="$3"
  local fan_table_before fan_table_after profile_before profile_after
  local pl1_before pl1_after pl2_before pl2_after pl4_before pl4_after
  local tcc_before tcc_after
  local mode_ctl_before mode_ctl_after idx_before idx_after p1_before p1_after
  local p2_before p2_after p3_before p3_after hlp_before hlp_after
  local lcse_before lcse_after ocpl_before ocpl_after
  local fsw_before fsw_after
  local tail1_before tail1_after tail2_before tail2_after
  local tailctl_before tailctl_after
  local ffan_before ffan_after
  local mode_changed tail_changed temp_target_changed rapl_changed pure_ok

  fan_table_before="$(lcc_fan_pure_audit_summary_value "$before_dir/state-summary.txt" "fan_table")"
  fan_table_after="$(lcc_fan_pure_audit_summary_value "$after_dir/state-summary.txt" "fan_table")"
  profile_before="$(lcc_fan_pure_audit_summary_value "$before_dir/state-summary.txt" "profile")"
  profile_after="$(lcc_fan_pure_audit_summary_value "$after_dir/state-summary.txt" "profile")"
  pl1_before="$(lcc_fan_pure_audit_summary_value "$before_dir/state-summary.txt" "pl1")"
  pl1_after="$(lcc_fan_pure_audit_summary_value "$after_dir/state-summary.txt" "pl1")"
  pl2_before="$(lcc_fan_pure_audit_summary_value "$before_dir/state-summary.txt" "pl2")"
  pl2_after="$(lcc_fan_pure_audit_summary_value "$after_dir/state-summary.txt" "pl2")"
  pl4_before="$(lcc_fan_pure_audit_summary_value "$before_dir/state-summary.txt" "pl4")"
  pl4_after="$(lcc_fan_pure_audit_summary_value "$after_dir/state-summary.txt" "pl4")"
  tcc_before="$(lcc_fan_pure_audit_summary_value "$before_dir/state-summary.txt" "tcc_offset")"
  tcc_after="$(lcc_fan_pure_audit_summary_value "$after_dir/state-summary.txt" "tcc_offset")"
  mode_ctl_before="$(lcc_fan_pure_audit_mode_token "$before_dir/mode-summary.txt" "MAFAN_CTL")"
  mode_ctl_after="$(lcc_fan_pure_audit_mode_token "$after_dir/mode-summary.txt" "MAFAN_CTL")"
  idx_before="$(lcc_fan_pure_audit_mode_token "$before_dir/mode-summary.txt" "IDX")"
  idx_after="$(lcc_fan_pure_audit_mode_token "$after_dir/mode-summary.txt" "IDX")"
  p1_before="$(lcc_fan_pure_audit_mode_token "$before_dir/mode-summary.txt" "P1")"
  p1_after="$(lcc_fan_pure_audit_mode_token "$after_dir/mode-summary.txt" "P1")"
  p2_before="$(lcc_fan_pure_audit_mode_token "$before_dir/mode-summary.txt" "P2")"
  p2_after="$(lcc_fan_pure_audit_mode_token "$after_dir/mode-summary.txt" "P2")"
  p3_before="$(lcc_fan_pure_audit_mode_token "$before_dir/mode-summary.txt" "P3")"
  p3_after="$(lcc_fan_pure_audit_mode_token "$after_dir/mode-summary.txt" "P3")"
  hlp_before="$(lcc_fan_pure_audit_mode_token "$before_dir/mode-summary.txt" "HLP")"
  hlp_after="$(lcc_fan_pure_audit_mode_token "$after_dir/mode-summary.txt" "HLP")"
  lcse_before="$(lcc_fan_pure_audit_mode_token "$before_dir/mode-summary.txt" "LCSE")"
  lcse_after="$(lcc_fan_pure_audit_mode_token "$after_dir/mode-summary.txt" "LCSE")"
  ocpl_before="$(lcc_fan_pure_audit_mode_token "$before_dir/mode-summary.txt" "OCPL")"
  ocpl_after="$(lcc_fan_pure_audit_mode_token "$after_dir/mode-summary.txt" "OCPL")"
  fsw_before="$(lcc_fan_pure_audit_mode_token "$before_dir/mode-summary.txt" "FSW")"
  fsw_after="$(lcc_fan_pure_audit_mode_token "$after_dir/mode-summary.txt" "FSW")"
  tail1_before="$(lcc_fan_pure_audit_mode_token "$before_dir/mode-summary.txt" "TAIL1")"
  tail1_after="$(lcc_fan_pure_audit_mode_token "$after_dir/mode-summary.txt" "TAIL1")"
  tail2_before="$(lcc_fan_pure_audit_mode_token "$before_dir/mode-summary.txt" "TAIL2")"
  tail2_after="$(lcc_fan_pure_audit_mode_token "$after_dir/mode-summary.txt" "TAIL2")"
  tailctl_before="$(lcc_fan_pure_audit_mode_token "$before_dir/mode-summary.txt" "TAILCTL")"
  tailctl_after="$(lcc_fan_pure_audit_mode_token "$after_dir/mode-summary.txt" "TAILCTL")"
  ffan_before="$(lcc_fan_pure_audit_mode_token "$before_dir/mode-summary.txt" "FFAN")"
  ffan_after="$(lcc_fan_pure_audit_mode_token "$after_dir/mode-summary.txt" "FFAN")"

  mode_changed="no"
  tail_changed="no"
  temp_target_changed="no"
  rapl_changed="no"
  pure_ok="yes"

  if [[ "$mode_ctl_before" != "$mode_ctl_after" || "$idx_before" != "$idx_after" || \
        "$p1_before" != "$p1_after" || "$p2_before" != "$p2_after" || \
        "$p3_before" != "$p3_after" || "$hlp_before" != "$hlp_after" || \
        "$lcse_before" != "$lcse_after" || "$ocpl_before" != "$ocpl_after" || \
        "$fsw_before" != "$fsw_after" ]]; then
    mode_changed="yes"
    pure_ok="no"
  fi
  if [[ "$tail1_before" != "$tail1_after" || "$tail2_before" != "$tail2_after" || \
        "$tailctl_before" != "$tailctl_after" ]]; then
    tail_changed="yes"
  fi
  if [[ "$(lcc_fan_pure_audit_extract_line "$before_dir/turbostat-summary.txt" "MSR_IA32_TEMPERATURE_TARGET:")" != \
        "$(lcc_fan_pure_audit_extract_line "$after_dir/turbostat-summary.txt" "MSR_IA32_TEMPERATURE_TARGET:")" ]]; then
    temp_target_changed="yes"
    pure_ok="no"
  fi
  if [[ "$(lcc_fan_pure_audit_extract_line "$before_dir/turbostat-summary.txt" "PKG Limit #1:")" != \
        "$(lcc_fan_pure_audit_extract_line "$after_dir/turbostat-summary.txt" "PKG Limit #1:")" ]]; then
    rapl_changed="yes"
    pure_ok="no"
  fi
  if [[ "$(lcc_fan_pure_audit_extract_line "$before_dir/turbostat-summary.txt" "PKG Limit #2:")" != \
        "$(lcc_fan_pure_audit_extract_line "$after_dir/turbostat-summary.txt" "PKG Limit #2:")" ]]; then
    rapl_changed="yes"
    pure_ok="no"
  fi
  if [[ "$profile_before" != "$profile_after" || "$pl1_before" != "$pl1_after" || \
        "$pl2_before" != "$pl2_after" || "$pl4_before" != "$pl4_after" || \
        "$tcc_before" != "$tcc_after" ]]; then
    pure_ok="no"
  fi

  {
    printf 'fan_table_before=%s\n' "$fan_table_before"
    printf 'fan_table_after=%s\n' "$fan_table_after"
    printf 'profile_before=%s\n' "$profile_before"
    printf 'profile_after=%s\n' "$profile_after"
    printf 'pl1_before=%s\n' "$pl1_before"
    printf 'pl1_after=%s\n' "$pl1_after"
    printf 'pl2_before=%s\n' "$pl2_before"
    printf 'pl2_after=%s\n' "$pl2_after"
    printf 'pl4_before=%s\n' "$pl4_before"
    printf 'pl4_after=%s\n' "$pl4_after"
    printf 'tcc_before=%s\n' "$tcc_before"
    printf 'tcc_after=%s\n' "$tcc_after"
    printf 'ffan_before=%s\n' "$ffan_before"
    printf 'ffan_after=%s\n' "$ffan_after"
    printf 'tail1_before=%s\n' "$tail1_before"
    printf 'tail1_after=%s\n' "$tail1_after"
    printf 'tail2_before=%s\n' "$tail2_before"
    printf 'tail2_after=%s\n' "$tail2_after"
    printf 'tailctl_before=%s\n' "$tailctl_before"
    printf 'tailctl_after=%s\n' "$tailctl_after"
    printf 'mode_bytes_changed=%s\n' "$mode_changed"
    printf 'fan_tail_changed=%s\n' "$tail_changed"
    printf 'temp_target_changed=%s\n' "$temp_target_changed"
    printf 'rapl_limits_changed=%s\n' "$rapl_changed"
    printf 'pure_fan_ok=%s\n' "$pure_ok"
  } >"$summary_file"
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    apply)
      mode="apply"
      shift
      ;;
    --file)
      fan_file="${2:?missing file path}"
      shift 2
      ;;
    --preset)
      preset_name="${2:?missing preset name}"
      shift 2
      ;;
    --settle)
      settle_seconds="${2:?missing settle seconds}"
      shift 2
      ;;
    --user-bus)
      use_user_bus="1"
      shift
      ;;
    --system-bus)
      use_user_bus="0"
      shift
      ;;
    -h|--help)
      lcc_fan_pure_audit_usage
      exit 0
      ;;
    *)
      if [[ -z "$out_dir" ]]; then
        out_dir="$1"
        shift
        continue
      fi
      lcc_fan_pure_audit_usage >&2
      exit 2
      ;;
  esac
done

if [[ "$mode" != "apply" ]]; then
  lcc_fan_pure_audit_usage >&2
  exit 2
fi

if [[ -n "$preset_name" && "$fan_file" != "$project_dir/data/fan-tables/fan-balanced.json" ]]; then
  printf 'use either --preset or --file, not both\n' >&2
  exit 2
fi

if [[ -z "$out_dir" ]]; then
  out_dir="/tmp/lcc-fan-pure-audit-$(lcc_fan_pure_audit_stamp)"
fi
mkdir -p "$out_dir"

lccctl_bin="$(lcc_fan_pure_audit_find_lccctl)" || {
  printf 'could not find lccctl in PATH or build tree\n' >&2
  exit 1
}

if [[ ! -f "$mode_audit_script" ]]; then
  printf 'missing mode audit helper: %s\n' "$mode_audit_script" >&2
  exit 1
fi

before_dir="$out_dir/before"
after_dir="$out_dir/after"

lcc_fan_pure_audit_capture_snapshot "$before_dir"

apply_stdout="$out_dir/apply.stdout"
apply_stderr="$out_dir/apply.stderr"
apply_exit="$out_dir/apply.exit"

apply_cmd=("$lccctl_bin" fan apply)
if [[ -n "$preset_name" ]]; then
  apply_cmd+=(--preset "$preset_name")
else
  apply_cmd+=(--file "$fan_file")
fi
if [[ "$use_user_bus" == "1" ]]; then
  apply_cmd+=(--user-bus)
fi

set +e
"${apply_cmd[@]}" >"$apply_stdout" 2>"$apply_stderr"
status=$?
set -e
printf '%s\n' "$status" >"$apply_exit"
if [[ "$status" -ne 0 ]]; then
  lcc_fan_pure_audit_log "fan apply failed; see $out_dir"
  exit "$status"
fi

sleep "$settle_seconds"
lcc_fan_pure_audit_capture_snapshot "$after_dir"

lcc_fan_pure_audit_diff_optional "$before_dir/state-summary.txt" \
  "$after_dir/state-summary.txt" "$out_dir/state-summary.diff"
lcc_fan_pure_audit_diff_optional "$before_dir/mode-summary.txt" \
  "$after_dir/mode-summary.txt" "$out_dir/mode-summary.diff"
lcc_fan_pure_audit_diff_optional "$before_dir/turbostat-summary.txt" \
  "$after_dir/turbostat-summary.txt" "$out_dir/turbostat-summary.diff"
lcc_fan_pure_audit_write_safety_summary "$before_dir" "$after_dir" \
  "$out_dir/safety-summary.txt"

lcc_fan_pure_audit_log "fan pure audit written to $out_dir"

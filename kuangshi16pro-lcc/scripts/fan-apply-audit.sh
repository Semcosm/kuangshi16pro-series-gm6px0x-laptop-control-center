#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
project_dir="$repo_root/kuangshi16pro-lcc"
watch_script="$repo_root/scripts/amw0/amw0-fan-rpm-audit.sh"
service_name="lccd.service"
watch_seconds="30"
interval="0.5"
extra_ec="0"
use_user_bus="0"
preset_name=""
fan_file="$project_dir/data/fan-tables/fan-balanced.json"
load_command=()

lcc_fan_apply_audit_usage() {
  cat <<'EOF'
Usage:
  fan-apply-audit.sh snapshot [OUT_DIR]
  fan-apply-audit.sh around [OUT_DIR] [OPTIONS] [-- LOAD_COMMAND...]
  fan-apply-audit.sh compare [OUT_DIR] [OPTIONS] [-- LOAD_COMMAND...]

Modes:
  snapshot
    Capture one lccctl state snapshot plus a one-shot FFAN/ECMG observation.

  around
    Apply a fan table, capture before/after state, then optionally run a watch
    window around a load command and record FFAN behavior.

  compare
    Capture one watch window for the currently active fan table, then apply the
    requested fan table and capture the same watch window again.

Options:
  --watch SEC      Watch window after apply. Default: 30
  --interval SEC   Sampling interval for FFAN watch. Default: 0.5
  --extra-ec       Include the read-only EC correlation block in the watch run.
  --preset NAME    Use a built-in fan preset instead of --file.
  --file PATH      Use a fan table file. Default:
                   kuangshi16pro-lcc/data/fan-tables/fan-balanced.json
  --user-bus       Use the user bus for the apply command.
  --system-bus     Use the system bus for the apply command. Default.

Defaults:
  OUT_DIR defaults to /tmp/lcc-fan-apply-audit-<timestamp>

Examples:
  kuangshi16pro-lcc/scripts/fan-apply-audit.sh snapshot
  kuangshi16pro-lcc/scripts/fan-apply-audit.sh around
  kuangshi16pro-lcc/scripts/fan-apply-audit.sh around --preset M4T1
  kuangshi16pro-lcc/scripts/fan-apply-audit.sh compare --preset M4T1 -- \
    stress-ng --cpu 16 --timeout 45s
  kuangshi16pro-lcc/scripts/fan-apply-audit.sh around --watch 45 -- \
    stress-ng --cpu 16 --timeout 45s

Notes:
  - Run this as a regular user from an active desktop session so the fan apply
    path stays on the normal Polkit flow.
  - FFAN and vendor_fan_level are treated as vendor fan activity telemetry,
    not as RPM.
  - compare mode measures current active fan behavior vs the requested table; it
    does not assume the baseline is system-default.
EOF
}

lcc_fan_apply_audit_stamp() {
  date '+%Y%m%d-%H%M%S'
}

lcc_fan_apply_audit_log() {
  printf '[fan-apply-audit] %s\n' "$*"
}

lcc_fan_apply_audit_run_privileged() {
  if [[ "${EUID:-$(id -u)}" -eq 0 ]]; then
    "$@"
    return 0
  fi

  sudo "$@"
}

lcc_fan_apply_audit_find_lccctl() {
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

lcc_fan_apply_audit_write_optional_command() {
  local target_file="$1"
  shift

  if [[ "$#" -eq 0 ]]; then
    return 0
  fi

  printf '%q' "$1" >"$target_file"
  shift
  while [[ "$#" -gt 0 ]]; do
    printf ' %q' "$1" >>"$target_file"
    shift
  done
  printf '\n' >>"$target_file"
}

lcc_fan_apply_audit_capture_command() {
  local snapshot_dir="$1"
  local name="$2"
  shift 2
  local stdout_file="$snapshot_dir/${name}.stdout"
  local stderr_file="$snapshot_dir/${name}.stderr"
  local exit_file="$snapshot_dir/${name}.exit"
  local status=0

  if [[ "$#" -eq 0 ]]; then
    printf '127\n' >"$exit_file"
    printf 'command not provided\n' >"$stderr_file"
    : >"$stdout_file"
    return 0
  fi

  set +e
  "$@" >"$stdout_file" 2>"$stderr_file"
  status=$?
  set -e
  printf '%s\n' "$status" >"$exit_file"
  return 0
}

lcc_fan_apply_audit_json_get() {
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

lcc_fan_apply_audit_json_get_object_key() {
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

lcc_fan_apply_audit_json_get_effective_meta_key() {
  local file="$1"
  local key="$2"
  local match=""
  local value=""

  if [[ ! -f "$file" ]]; then
    printf 'none\n'
    return 0
  fi

  case "$key" in
    source)
      match="$(grep -o "\"effective_meta\":{\"source\":\\(\"[^\"]*\"\\|null\\|true\\|false\\|[0-9][0-9]*\\)" \
        "$file" | head -n 1 || true)"
      ;;
    freshness)
      match="$(grep -o "\"effective_meta\":{\"source\":\\(\"[^\"]*\"\\|null\\|true\\|false\\|[0-9][0-9]*\\),\"freshness\":\\(\"[^\"]*\"\\|null\\|true\\|false\\|[0-9][0-9]*\\)" \
        "$file" | head -n 1 || true)"
      ;;
    *)
      printf 'missing\n'
      return 0
      ;;
  esac

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

lcc_fan_apply_audit_write_state_summary() {
  local state_file="$1"
  local summary_file="$2"

  {
    printf 'backend=%s\n' "$(lcc_fan_apply_audit_json_get "$state_file" "backend")"
    printf 'backend_selected=%s\n' \
      "$(lcc_fan_apply_audit_json_get "$state_file" "backend_selected")"
    printf 'effective_source=%s\n' \
      "$(lcc_fan_apply_audit_json_get_effective_meta_key "$state_file" "source")"
    printf 'effective_freshness=%s\n' \
      "$(lcc_fan_apply_audit_json_get_effective_meta_key "$state_file" "freshness")"
    printf 'requested_fan_table=%s\n' \
      "$(lcc_fan_apply_audit_json_get_object_key "$state_file" "requested" "fan_table")"
    printf 'effective_fan_table=%s\n' \
      "$(lcc_fan_apply_audit_json_get_object_key "$state_file" "effective" "fan_table")"
    printf 'last_apply_stage=%s\n' \
      "$(lcc_fan_apply_audit_json_get "$state_file" "last_apply_stage")"
    printf 'last_apply_backend=%s\n' \
      "$(lcc_fan_apply_audit_json_get "$state_file" "last_apply_backend")"
    printf 'last_apply_hardware_write=%s\n' \
      "$(lcc_fan_apply_audit_json_get "$state_file" "last_apply_hardware_write")"
    printf 'cpu_temp_c=%s\n' \
      "$(lcc_fan_apply_audit_json_get_object_key "$state_file" "thermal" "cpu_temp_c")"
    printf 'gpu_temp_c=%s\n' \
      "$(lcc_fan_apply_audit_json_get_object_key "$state_file" "thermal" "gpu_temp_c")"
    printf 'cpu_fan_rpm=%s\n' \
      "$(lcc_fan_apply_audit_json_get_object_key "$state_file" "thermal" "cpu_fan_rpm")"
    printf 'gpu_fan_rpm=%s\n' \
      "$(lcc_fan_apply_audit_json_get_object_key "$state_file" "thermal" "gpu_fan_rpm")"
    printf 'vendor_fan_level=%s\n' \
      "$(lcc_fan_apply_audit_json_get_object_key "$state_file" "thermal" "vendor_fan_level")"
  } >"$summary_file"
}

lcc_fan_apply_audit_expected_fan_table() {
  local apply_stdout="$1"
  local match=""

  if [[ ! -f "$apply_stdout" ]]; then
    printf 'none\n'
    return 0
  fi

  match="$(grep '^fan-table=' "$apply_stdout" | head -n 1 || true)"
  if [[ -z "$match" ]]; then
    printf 'none\n'
    return 0
  fi

  printf '%s\n' "${match#fan-table=}"
}

lcc_fan_apply_audit_require_apply_success() {
  local out_dir="$1"
  local state_file="$2"
  local expected_table=""
  local apply_exit=""
  local effective_fan_table=""
  local requested_fan_table=""
  local last_apply_error=""
  local last_apply_hardware_write=""
  local transaction_state=""

  expected_table="$(lcc_fan_apply_audit_expected_fan_table "$out_dir/apply.stdout")"
  apply_exit="$(cat "$out_dir/apply.exit")"
  effective_fan_table="$(lcc_fan_apply_audit_json_get_object_key "$state_file" "effective" "fan_table")"
  requested_fan_table="$(lcc_fan_apply_audit_json_get_object_key "$state_file" "requested" "fan_table")"
  last_apply_error="$(lcc_fan_apply_audit_json_get "$state_file" "last_apply_error")"
  last_apply_hardware_write="$(lcc_fan_apply_audit_json_get "$state_file" "last_apply_hardware_write")"
  transaction_state="$(lcc_fan_apply_audit_json_get_object_key "$state_file" "transaction" "state")"

  if [[ "$apply_exit" != "0" ]]; then
    printf 'apply command failed with exit=%s\n' "$apply_exit" >&2
    return 1
  fi
  if [[ "$last_apply_error" != "none" ]]; then
    printf 'apply reported last_apply_error=%s\n' "$last_apply_error" >&2
    return 1
  fi
  if [[ "$transaction_state" == "failed" ]]; then
    printf 'apply left transaction.state=failed\n' >&2
    return 1
  fi
  if [[ "$last_apply_hardware_write" != "true" ]]; then
    printf 'apply did not confirm last_apply_hardware_write=true\n' >&2
    return 1
  fi
  if [[ "$expected_table" != "none" && "$requested_fan_table" != "$expected_table" ]]; then
    printf 'requested fan table mismatch: expected=%s actual=%s\n' \
      "$expected_table" "$requested_fan_table" >&2
    return 1
  fi
  if [[ "$expected_table" != "none" && "$effective_fan_table" != "$expected_table" ]]; then
    printf 'effective fan table mismatch: expected=%s actual=%s\n' \
      "$expected_table" "$effective_fan_table" >&2
    return 1
  fi

  return 0
}

lcc_fan_apply_audit_journal_cursor() {
  lcc_fan_apply_audit_run_privileged journalctl -u "$service_name" -n 0 \
    --show-cursor --no-pager 2>/dev/null | sed -n 's/^-- cursor: //p' | tail -n 1
}

lcc_fan_apply_audit_capture_journal() {
  local cursor="$1"
  local output_file="$2"
  local lines="${3:-200}"

  if [[ -n "$cursor" ]]; then
    lcc_fan_apply_audit_run_privileged journalctl -u "$service_name" \
      --after-cursor "$cursor" -n "$lines" --no-pager -o short-iso \
      >"$output_file" 2>&1
    return 0
  fi

  lcc_fan_apply_audit_run_privileged journalctl -u "$service_name" -n "$lines" \
    --no-pager -o short-iso >"$output_file" 2>&1
}

lcc_fan_apply_audit_write_diff() {
  local before_file="$1"
  local after_file="$2"
  local diff_file="$3"

  if [[ ! -f "$before_file" || ! -f "$after_file" ]]; then
    return 0
  fi

  set +e
  diff -u "$before_file" "$after_file" >"$diff_file"
  case "$?" in
    0|1)
      ;;
    *)
      printf 'diff failed for %s and %s\n' "$before_file" "$after_file" \
        >"$diff_file"
      ;;
  esac
  set -e
}

lcc_fan_apply_audit_write_watch_summary() {
  local watch_stdout="$1"
  local summary_file="$2"

  awk '
    BEGIN {
      count = 0
      first = "none"
      last = "none"
      max = "none"
      wrapped = "missing"
    }
    {
      if (match($0, /FFAN=[0-9]+/)) {
        value = substr($0, RSTART + 5, RLENGTH - 5) + 0
        if (count == 0) {
          first = value
          max = value
        }
        last = value
        if (value > max) {
          max = value
        }
        count++
      }
    }
    /^wrapped_exit_status=/ {
      wrapped = substr($0, index($0, "=") + 1)
    }
    END {
      printf "ffan_sample_count=%d\n", count
      printf "ffan_first=%s\n", first
      printf "ffan_max=%s\n", max
      printf "ffan_last=%s\n", last
      printf "wrapped_exit_status=%s\n", wrapped
    }
  ' "$watch_stdout" >"$summary_file"
}

lcc_fan_apply_audit_run_watch_capture() {
  local out_dir="$1"
  local prefix="$2"
  local journal_cursor="$3"
  local watch_command=("$watch_script" --watch "$watch_seconds" --interval "$interval")

  if [[ "$extra_ec" == "1" ]]; then
    watch_command+=(--extra-ec)
  fi
  if [[ "${#load_command[@]}" -gt 0 ]]; then
    watch_command+=(-- "${load_command[@]}")
  fi

  set +e
  lcc_fan_apply_audit_run_privileged "${watch_command[@]}" >"$out_dir/${prefix}.stdout" \
    2>"$out_dir/${prefix}.stderr"
  printf '%s\n' "$?" >"$out_dir/${prefix}.exit"
  set -e
  lcc_fan_apply_audit_write_watch_summary "$out_dir/${prefix}.stdout" \
    "$out_dir/${prefix}-summary.txt"

  if command -v journalctl >/dev/null 2>&1; then
    lcc_fan_apply_audit_capture_journal "$journal_cursor" \
      "$out_dir/${prefix}-journal.log" 300
  fi
}

lcc_fan_apply_audit_summary_value() {
  local file="$1"
  local key="$2"
  local match=""

  if [[ ! -f "$file" ]]; then
    printf 'missing\n'
    return 0
  fi

  match="$(grep "^$key=" "$file" | head -n 1 || true)"
  if [[ -z "$match" ]]; then
    printf 'missing\n'
    return 0
  fi

  printf '%s\n' "${match#*=}"
}

lcc_fan_apply_audit_write_compare_summary() {
  local out_dir="$1"
  local baseline_file="$out_dir/baseline-watch-summary.txt"
  local custom_file="$out_dir/custom-watch-summary.txt"
  local baseline_max=""
  local custom_max=""
  local baseline_last=""
  local custom_last=""

  baseline_max="$(lcc_fan_apply_audit_summary_value "$baseline_file" "ffan_max")"
  custom_max="$(lcc_fan_apply_audit_summary_value "$custom_file" "ffan_max")"
  baseline_last="$(lcc_fan_apply_audit_summary_value "$baseline_file" "ffan_last")"
  custom_last="$(lcc_fan_apply_audit_summary_value "$custom_file" "ffan_last")"

  {
    printf 'baseline_ffan_max=%s\n' "$baseline_max"
    printf 'custom_ffan_max=%s\n' "$custom_max"
    printf 'baseline_ffan_last=%s\n' "$baseline_last"
    printf 'custom_ffan_last=%s\n' "$custom_last"
    if [[ "$baseline_max" =~ ^[0-9]+$ && "$custom_max" =~ ^[0-9]+$ ]]; then
      printf 'ffan_max_delta=%d\n' "$((custom_max - baseline_max))"
    else
      printf 'ffan_max_delta=missing\n'
    fi
    if [[ "$baseline_last" =~ ^[0-9]+$ && "$custom_last" =~ ^[0-9]+$ ]]; then
      printf 'ffan_last_delta=%d\n' "$((custom_last - baseline_last))"
    else
      printf 'ffan_last_delta=missing\n'
    fi
  } >"$out_dir/compare-summary.txt"
}

lcc_fan_apply_audit_snapshot() {
  local out_dir="$1"
  local label="$2"
  local snapshot_dir="$out_dir/$label"
  local lccctl_path=""

  mkdir -p "$snapshot_dir"
  date -Is >"$snapshot_dir/timestamp.txt"

  if lccctl_path="$(lcc_fan_apply_audit_find_lccctl)"; then
    printf '%s\n' "$lccctl_path" >"$snapshot_dir/lccctl-path.txt"
    lcc_fan_apply_audit_capture_command "$snapshot_dir" state "$lccctl_path" state
    lcc_fan_apply_audit_capture_command "$snapshot_dir" capabilities \
      "$lccctl_path" capabilities
    lcc_fan_apply_audit_write_state_summary "$snapshot_dir/state.stdout" \
      "$snapshot_dir/state-summary.txt"
  else
    printf 'lccctl not found in PATH or %s/build\n' "$project_dir" \
      >"$snapshot_dir/state.stderr"
    printf '127\n' >"$snapshot_dir/state.exit"
    : >"$snapshot_dir/state.stdout"
    printf 'lccctl not found in PATH or %s/build\n' "$project_dir" \
      >"$snapshot_dir/capabilities.stderr"
    printf '127\n' >"$snapshot_dir/capabilities.exit"
    : >"$snapshot_dir/capabilities.stdout"
  fi

  if command -v systemctl >/dev/null 2>&1; then
    set +e
    systemctl show -p ActiveState -p SubState -p UnitFileState "$service_name" \
      >"$snapshot_dir/service.txt" 2>"$snapshot_dir/service.stderr"
    set -e
  fi

  if [[ -x "$watch_script" ]]; then
    set +e
    lcc_fan_apply_audit_run_privileged "$watch_script" \
      >"$snapshot_dir/fan-monitor.txt" 2>"$snapshot_dir/fan-monitor.stderr"
    printf '%s\n' "$?" >"$snapshot_dir/fan-monitor.exit"
    set -e
  fi
}

mode="${1:-}"
case "$mode" in
  snapshot|around|compare)
    shift
    ;;
  -h|--help|'')
    lcc_fan_apply_audit_usage
    exit 0
    ;;
  *)
    printf 'unknown mode: %s\n\n' "$mode" >&2
    lcc_fan_apply_audit_usage >&2
    exit 1
    ;;
esac

out_dir="/tmp/lcc-fan-apply-audit-$(lcc_fan_apply_audit_stamp)"
if [[ "$#" -gt 0 && "${1:-}" != --* ]]; then
  out_dir="$1"
  shift
fi

while [[ "$#" -gt 0 ]]; do
  case "$1" in
    --watch)
      watch_seconds="${2:?missing watch seconds}"
      shift 2
      ;;
    --interval)
      interval="${2:?missing interval}"
      shift 2
      ;;
    --extra-ec)
      extra_ec="1"
      shift
      ;;
    --preset)
      preset_name="${2:?missing preset name}"
      shift 2
      ;;
    --file)
      fan_file="${2:?missing fan file path}"
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
    --)
      shift
      load_command=("$@")
      break
      ;;
    -h|--help)
      lcc_fan_apply_audit_usage
      exit 0
      ;;
    *)
      printf 'unknown option: %s\n\n' "$1" >&2
      lcc_fan_apply_audit_usage >&2
      exit 1
      ;;
  esac
done

if [[ -n "$preset_name" && "$fan_file" != "$project_dir/data/fan-tables/fan-balanced.json" ]]; then
  printf 'use either --preset or --file, not both\n' >&2
  exit 1
fi

if [[ "${#load_command[@]}" -gt 0 ]] && ! awk 'BEGIN { exit !('"${watch_seconds}"' > 0) }'; then
  printf '--watch must be greater than 0 when using -- LOAD_COMMAND...\n' >&2
  exit 1
fi

mkdir -p "$out_dir"

if [[ "${EUID:-$(id -u)}" -ne 0 ]]; then
  sudo -v
fi

if [[ "$mode" == "snapshot" ]]; then
  lcc_fan_apply_audit_snapshot "$out_dir" snapshot
  if command -v journalctl >/dev/null 2>&1; then
    lcc_fan_apply_audit_capture_journal "" "$out_dir/snapshot/journal.log" 120
  fi
  lcc_fan_apply_audit_log "snapshot written to $out_dir/snapshot"
  exit 0
fi

if [[ ! -x "$watch_script" ]]; then
  printf 'required watch script not found or not executable: %s\n' "$watch_script" >&2
  exit 1
fi

lccctl_path="$(lcc_fan_apply_audit_find_lccctl || true)"
if [[ -z "$lccctl_path" ]]; then
  printf 'lccctl not found in PATH or %s/build\n' "$project_dir" >&2
  exit 1
fi

apply_command=("$lccctl_path" fan apply)
if [[ -n "$preset_name" ]]; then
  apply_command+=(--preset "$preset_name")
else
  apply_command+=(--file "$fan_file")
fi
if [[ "$use_user_bus" == "1" ]]; then
  apply_command+=(--user-bus)
else
  apply_command+=(--system-bus)
fi

lcc_fan_apply_audit_write_optional_command "$out_dir/apply-command.txt" \
  "${apply_command[@]}"
lcc_fan_apply_audit_write_optional_command "$out_dir/load-command.txt" \
  "${load_command[@]}"

cat >"$out_dir/metadata.txt" <<EOF
mode=$mode
timestamp=$(date -Is)
service=$service_name
watch_seconds=$watch_seconds
interval=$interval
extra_ec=$extra_ec
bus=$( [[ "$use_user_bus" == "1" ]] && printf 'user' || printf 'system' )
fan_source=$( [[ -n "$preset_name" ]] && printf 'preset:%s' "$preset_name" || printf 'file:%s' "$fan_file" )
watch_script=$watch_script
EOF

if [[ "$mode" == "compare" ]]; then
  baseline_cursor=""
  apply_cursor=""
  custom_cursor=""

  lcc_fan_apply_audit_snapshot "$out_dir" baseline-before
  if command -v journalctl >/dev/null 2>&1; then
    baseline_cursor="$(lcc_fan_apply_audit_journal_cursor)"
  fi
  if awk 'BEGIN { exit !('"${watch_seconds}"' > 0) }'; then
    lcc_fan_apply_audit_run_watch_capture "$out_dir" "baseline-watch" \
      "$baseline_cursor"
  fi
  lcc_fan_apply_audit_snapshot "$out_dir" baseline-after
  lcc_fan_apply_audit_write_diff "$out_dir/baseline-before/state.stdout" \
    "$out_dir/baseline-after/state.stdout" "$out_dir/state-baseline.diff"

  if command -v journalctl >/dev/null 2>&1; then
    apply_cursor="$(lcc_fan_apply_audit_journal_cursor)"
  fi
  lcc_fan_apply_audit_capture_command "$out_dir" apply "${apply_command[@]}"
  if command -v journalctl >/dev/null 2>&1; then
    lcc_fan_apply_audit_capture_journal "$apply_cursor" "$out_dir/apply-journal.log" 200
    custom_cursor="$(lcc_fan_apply_audit_journal_cursor)"
  fi
  lcc_fan_apply_audit_snapshot "$out_dir" after-apply
  lcc_fan_apply_audit_write_diff "$out_dir/baseline-after/state.stdout" \
    "$out_dir/after-apply/state.stdout" "$out_dir/state-after-apply.diff"
  if ! lcc_fan_apply_audit_require_apply_success "$out_dir" \
    "$out_dir/after-apply/state.stdout"; then
    lcc_fan_apply_audit_log "fan apply compare audit failed during apply; artifacts written to $out_dir"
    exit 1
  fi

  if awk 'BEGIN { exit !('"${watch_seconds}"' > 0) }'; then
    lcc_fan_apply_audit_run_watch_capture "$out_dir" "custom-watch" \
      "$custom_cursor"
    lcc_fan_apply_audit_snapshot "$out_dir" after-watch
    lcc_fan_apply_audit_write_diff "$out_dir/after-apply/state.stdout" \
      "$out_dir/after-watch/state.stdout" "$out_dir/state-after-watch.diff"
  fi

  lcc_fan_apply_audit_write_compare_summary "$out_dir"
  lcc_fan_apply_audit_log "fan apply compare audit written to $out_dir"
  exit "$(cat "$out_dir/apply.exit")"
fi

lcc_fan_apply_audit_snapshot "$out_dir" before

apply_cursor=""
watch_cursor=""
if command -v journalctl >/dev/null 2>&1; then
  apply_cursor="$(lcc_fan_apply_audit_journal_cursor)"
fi

lcc_fan_apply_audit_capture_command "$out_dir" apply "${apply_command[@]}"

if command -v journalctl >/dev/null 2>&1; then
  lcc_fan_apply_audit_capture_journal "$apply_cursor" "$out_dir/apply-journal.log" 200
  watch_cursor="$(lcc_fan_apply_audit_journal_cursor)"
fi

lcc_fan_apply_audit_snapshot "$out_dir" after-apply
lcc_fan_apply_audit_write_diff "$out_dir/before/state.stdout" \
  "$out_dir/after-apply/state.stdout" "$out_dir/state-after-apply.diff"
if ! lcc_fan_apply_audit_require_apply_success "$out_dir" \
  "$out_dir/after-apply/state.stdout"; then
  lcc_fan_apply_audit_log "fan apply audit failed during apply; artifacts written to $out_dir"
  exit 1
fi

if awk 'BEGIN { exit !('"${watch_seconds}"' > 0) }'; then
  lcc_fan_apply_audit_run_watch_capture "$out_dir" "watch" "$watch_cursor"
  lcc_fan_apply_audit_snapshot "$out_dir" after-watch
  lcc_fan_apply_audit_write_diff "$out_dir/after-apply/state.stdout" \
    "$out_dir/after-watch/state.stdout" "$out_dir/state-after-watch.diff"
fi

lcc_fan_apply_audit_log "fan apply audit written to $out_dir"
exit "$(cat "$out_dir/apply.exit")"

#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
project_dir="$repo_root/kuangshi16pro-lcc"

lcc_powercap_audit_usage() {
  cat <<'EOF'
Usage:
  powercap-audit.sh snapshot [OUT_DIR]
  powercap-audit.sh around [OUT_DIR] -- COMMAND [ARGS...]

Modes:
  snapshot
    Capture one raw powercap snapshot plus lccctl state/capabilities.

  around
    Capture before/after snapshots around the provided command and write diffs.

Defaults:
  OUT_DIR defaults to /tmp/lcc-powercap-audit-<timestamp>

Examples:
  kuangshi16pro-lcc/scripts/powercap-audit.sh snapshot
  kuangshi16pro-lcc/scripts/powercap-audit.sh around -- lccctl power set --pl1 70 --pl2 120
EOF
}

lcc_powercap_audit_stamp() {
  date '+%Y%m%d-%H%M%S'
}

lcc_powercap_audit_log() {
  printf '[powercap-audit] %s\n' "$*"
}

lcc_powercap_audit_find_lccctl() {
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

lcc_powercap_audit_write_optional_command() {
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

lcc_powercap_audit_dump_file() {
  local path="$1"
  local value=""

  printf '%s=' "${path##*/}"
  if value="$(cat "$path" 2>&1)"; then
    printf '%s\n' "$value"
    return 0
  fi

  printf '[error] %s\n' "$value"
}

lcc_powercap_audit_zone_pairs() {
  local base="/sys/class/powercap"
  local entry=""
  local real_entry=""
  local child=""
  local child_real=""

  [[ -d "$base" ]] || return 0

  for entry in "$base"/*; do
    [[ -e "$entry" ]] || continue
    case "${entry##*/}" in
      intel-rapl*|intel-rapl-mmio*)
        real_entry="$(readlink -f "$entry")"
        [[ -d "$real_entry" ]] || continue
        printf '%s\t%s\n' "${entry##*/}" "$real_entry"
        for child in "$real_entry"/intel-rapl* "$real_entry"/intel-rapl-mmio*; do
          [[ -e "$child" ]] || continue
          [[ -d "$child" ]] || continue
          child_real="$(readlink -f "$child")"
          [[ -d "$child_real" ]] || continue
          printf '%s/%s\t%s\n' "${entry##*/}" "${child##*/}" "$child_real"
        done
        ;;
    esac
  done | awk -F '\t' '!seen[$2]++ { print $0 }' | sort
}

lcc_powercap_audit_dump_tree() {
  local label=""
  local zone_path=""
  local file_path=""
  local found=0

  while IFS=$'\t' read -r label zone_path; do
    [[ -n "$zone_path" ]] || continue
    found=1
    printf '\n## %s (%s)\n' "$label" "$zone_path"
    for file_path in \
      "$zone_path"/name \
      "$zone_path"/enabled \
      "$zone_path"/max_power_range_uw \
      "$zone_path"/energy_uj \
      "$zone_path"/constraint_*_name \
      "$zone_path"/constraint_*_power_limit_uw \
      "$zone_path"/constraint_*_max_power_uw \
      "$zone_path"/constraint_*_min_power_uw \
      "$zone_path"/constraint_*_time_window_us; do
      [[ -e "$file_path" ]] || continue
      [[ -f "$file_path" ]] || continue
      lcc_powercap_audit_dump_file "$file_path"
    done
  done < <(lcc_powercap_audit_zone_pairs)

  if [[ "$found" -eq 0 ]]; then
    printf 'no powercap zones found under /sys/class/powercap\n'
  fi
}

lcc_powercap_audit_capture_command() {
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

lcc_powercap_audit_snapshot() {
  local out_dir="$1"
  local label="$2"
  local snapshot_dir="$out_dir/$label"
  local lccctl_path=""

  mkdir -p "$snapshot_dir"
  date -Is >"$snapshot_dir/timestamp.txt"
  lcc_powercap_audit_dump_tree >"$snapshot_dir/powercap.txt"

  if lccctl_path="$(lcc_powercap_audit_find_lccctl)"; then
    printf '%s\n' "$lccctl_path" >"$snapshot_dir/lccctl-path.txt"
    lcc_powercap_audit_capture_command "$snapshot_dir" state "$lccctl_path" state
    lcc_powercap_audit_capture_command "$snapshot_dir" capabilities \
      "$lccctl_path" capabilities
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
    systemctl show -p ActiveState -p SubState -p UnitFileState lccd.service \
      >"$snapshot_dir/service.txt" 2>"$snapshot_dir/service.stderr"
    set -e
  fi
}

lcc_powercap_audit_write_diff() {
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

mode="${1:-}"
case "$mode" in
  snapshot|around)
    shift
    ;;
  -h|--help|'')
    lcc_powercap_audit_usage
    exit 0
    ;;
  *)
    printf 'unknown mode: %s\n\n' "$mode" >&2
    lcc_powercap_audit_usage >&2
    exit 1
    ;;
esac

out_dir="/tmp/lcc-powercap-audit-$(lcc_powercap_audit_stamp)"
if [[ "$#" -gt 0 && "${1:-}" != "--" ]]; then
  out_dir="$1"
  shift
fi

mkdir -p "$out_dir"

if [[ "$mode" == "snapshot" ]]; then
  lcc_powercap_audit_snapshot "$out_dir" snapshot
  lcc_powercap_audit_log "snapshot written to $out_dir/snapshot"
  exit 0
fi

if [[ "${1:-}" != "--" ]]; then
  printf 'around mode requires -- before the command\n' >&2
  exit 1
fi
shift

if [[ "$#" -eq 0 ]]; then
  printf 'around mode requires a command after --\n' >&2
  exit 1
fi

lcc_powercap_audit_write_optional_command "$out_dir/command.txt" "$@"
lcc_powercap_audit_snapshot "$out_dir" before
lcc_powercap_audit_capture_command "$out_dir" command "$@"
lcc_powercap_audit_snapshot "$out_dir" after
lcc_powercap_audit_write_diff "$out_dir/before/powercap.txt" \
  "$out_dir/after/powercap.txt" "$out_dir/powercap.diff"
lcc_powercap_audit_write_diff "$out_dir/before/state.stdout" \
  "$out_dir/after/state.stdout" "$out_dir/state.diff"
lcc_powercap_audit_log "before/after audit written to $out_dir"

exit "$(cat "$out_dir/command.exit")"

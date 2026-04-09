#!/usr/bin/env bash
set -euo pipefail

hwmon_root="${1:-/sys/class/hwmon}"

lcc_hwmon_audit_usage() {
  cat <<'EOF'
Usage:
  hwmon-audit.sh [HWMON_ROOT]

Examples:
  sudo kuangshi16pro-lcc/scripts/hwmon-audit.sh
  sudo kuangshi16pro-lcc/scripts/hwmon-audit.sh /sys/class/hwmon
EOF
}

lcc_hwmon_audit_dump_file() {
  local path="$1"

  printf '%s=' "$(basename "$path")"
  cat "$path"
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
  lcc_hwmon_audit_usage
  exit 0
fi

if [[ ! -d "$hwmon_root" ]]; then
  printf 'hwmon root not found: %s\n' "$hwmon_root" >&2
  exit 1
fi

shopt -s nullglob

for node in "$hwmon_root"/hwmon*; do
  local_files=("$node"/temp*_label "$node"/temp*_input "$node"/fan*_label
               "$node"/fan*_input)

  [[ -d "$node" ]] || continue

  printf '\n## %s\n' "$node"
  if [[ -f "$node/name" ]]; then
    lcc_hwmon_audit_dump_file "$node/name"
  fi

  for file in "${local_files[@]}"; do
    [[ -f "$file" ]] || continue
    lcc_hwmon_audit_dump_file "$file"
  done
done

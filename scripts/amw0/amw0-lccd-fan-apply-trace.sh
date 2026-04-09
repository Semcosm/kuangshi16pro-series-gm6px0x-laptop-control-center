#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
project_dir="$repo_root/kuangshi16pro-lcc"
service_name="lccd.service"
override_dir="/run/systemd/system/${service_name}.d"
override_file="${override_dir}/60-amw0-trace.conf"
mode_audit_script="$repo_root/scripts/amw0/amw0-mode-button-audit.sh"
fan_file="$project_dir/data/fan-tables/fan-balanced.json"
out_dir=""
override_tmp="$(mktemp /tmp/lcc-amw0-trace.XXXXXX)"
trace_file=""
journal_cursor=""
initial_active=""

usage() {
  cat <<'EOF'
Usage:
  amw0-lccd-fan-apply-trace.sh [OUT_DIR] [OPTIONS]

Options:
  --file PATH      Fan table file to apply. Default:
                   kuangshi16pro-lcc/data/fan-tables/fan-balanced.json
  -h, --help       Show this help.

What it does:
  - installs a temporary runtime override for lccd.service
  - sets LCC_AMW0_TRACE_FILE for the real daemon
  - restarts lccd.service
  - captures mode/status before and after a real `lccctl fan apply`
  - saves the raw AMW0 daemon trace and journal snippet

Default OUT_DIR:
  /tmp/lcc-amw0-lccd-trace-<timestamp>
EOF
}

log() {
  printf '[amw0-lccd-trace] %s\n' "$*"
}

stamp() {
  date '+%Y%m%d-%H%M%S'
}

find_lccctl() {
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

capture_mode_snapshot() {
  local snapshot_dir="$1"
  local raw_file="$snapshot_dir/mode.stdout"
  local summary_file="$snapshot_dir/mode-summary.txt"
  local log_path=""

  sudo bash "$mode_audit_script" --changes-only >"$raw_file" \
    2>"$snapshot_dir/mode.stderr"
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

capture_status_snapshot() {
  local snapshot_dir="$1"

  "$lccctl_bin" status >"$snapshot_dir/state.stdout" \
    2>"$snapshot_dir/state.stderr"
}

capture_snapshot() {
  local snapshot_dir="$1"

  mkdir -p "$snapshot_dir"
  capture_mode_snapshot "$snapshot_dir"
  capture_status_snapshot "$snapshot_dir"
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

  exit "$status"
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --file)
      fan_file="$2"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    -*)
      usage >&2
      exit 2
      ;;
    *)
      if [[ -n "$out_dir" ]]; then
        usage >&2
        exit 2
      fi
      out_dir="$1"
      shift
      ;;
  esac
done

if [[ -z "$out_dir" ]]; then
  out_dir="/tmp/lcc-amw0-lccd-trace-$(stamp)"
fi

lccctl_bin="$(find_lccctl)"
[[ -n "$lccctl_bin" ]] || {
  echo "lccctl not found in PATH or $project_dir/build" >&2
  exit 1
}
[[ -f "$fan_file" ]] || {
  echo "fan table not found: $fan_file" >&2
  exit 1
}
[[ -f "$mode_audit_script" ]] || {
  echo "missing mode audit helper: $mode_audit_script" >&2
  exit 1
}
if [[ "$EUID" -eq 0 ]]; then
  echo "run this script as a regular user so lccctl stays on the Polkit path" >&2
  exit 1
fi

mkdir -p "$out_dir"
trace_file="$out_dir/amw0.trace"

trap cleanup EXIT

sudo -v
if ! sudo systemctl cat "$service_name" >/dev/null 2>&1; then
  echo "systemd unit not installed: $service_name" >&2
  exit 1
fi

initial_active="$(systemctl is-active "$service_name" 2>/dev/null || true)"
journal_cursor="$(
  sudo journalctl -u "$service_name" -n 0 --show-cursor --no-pager 2>/dev/null |
    sed -n 's/^-- cursor: //p' | tail -n 1
)"

cat >"$override_tmp" <<EOF
[Service]
UnsetEnvironment=LCC_AMW0_TRACE_FILE
Environment=LCC_AMW0_TRACE_FILE=$trace_file
EOF

sudo install -d -m 0755 "$override_dir"
sudo install -m 0644 "$override_tmp" "$override_file"
sudo systemctl daemon-reload
sudo systemctl restart "$service_name"
sudo systemctl is-active --quiet "$service_name"

{
  printf 'timestamp=%s\n' "$(date --iso-8601=seconds)"
  printf 'service=%s\n' "$service_name"
  printf 'fan_file=%s\n' "$fan_file"
  printf 'trace_file=%s\n' "$trace_file"
  printf 'lccctl_bin=%s\n' "$lccctl_bin"
  printf 'initial_active=%s\n' "${initial_active:-unknown}"
} >"$out_dir/metadata.txt"
sudo systemctl show -p MainPID -p ExecMainStartTimestamp -p Environment \
  "$service_name" >"$out_dir/service-show.txt"

capture_snapshot "$out_dir/before"

set +e
"$lccctl_bin" fan apply --file "$fan_file" --system-bus \
  >"$out_dir/apply.stdout" 2>"$out_dir/apply.stderr"
apply_rc=$?
set -e
printf '%s\n' "$apply_rc" >"$out_dir/apply.exit"

capture_snapshot "$out_dir/after"

if [[ -n "$journal_cursor" ]]; then
  sudo journalctl -u "$service_name" --after-cursor "$journal_cursor" -n 200 \
    --no-pager -o short-iso >"$out_dir/journal.log"
else
  sudo journalctl -u "$service_name" -n 200 --no-pager -o short-iso \
    >"$out_dir/journal.log"
fi

if [[ -f "$trace_file" ]]; then
  sudo cat "$trace_file" >"$out_dir/amw0.trace.copy"
fi

diff -u "$out_dir/before/mode-summary.txt" "$out_dir/after/mode-summary.txt" \
  >"$out_dir/mode-summary.diff" || true
diff -u "$out_dir/before/state.stdout" "$out_dir/after/state.stdout" \
  >"$out_dir/state.diff" || true

log "trace audit written to $out_dir"

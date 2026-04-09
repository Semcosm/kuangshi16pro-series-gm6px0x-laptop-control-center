#!/usr/bin/env bash
set -euo pipefail

if [[ ${EUID:-$(id -u)} -ne 0 ]]; then
  echo "Run as root: sudo $0" >&2
  exit 1
fi

CALL_NODE="/proc/acpi/call"
FFAN_OFFSET="0x460"
INTERVAL="0.25"
HOLD_SECONDS="5"
RESTORE="1"
LEVEL=""

usage() {
  cat <<'EOF'
Usage:
  sudo ./scripts/amw0/amw0-ffan-poke.sh --level 10
  sudo ./scripts/amw0/amw0-ffan-poke.sh --level 10 --hold 8 --interval 0.2
  sudo ./scripts/amw0/amw0-ffan-poke.sh --level 0 --no-restore

Options:
  --level N        Target FFAN low nibble, 0..15.
  --hold SEC       Hold target for SEC seconds before restore. Default: 5
  --interval SEC   Sample interval while holding. Default: 0.25
  --no-restore     Do not restore the original raw byte after the hold window.

Notes:
  - This is an experimental direct ECMG write to FFAN at offset 0x460.
  - It preserves the original high nibble and only changes the low nibble.
  - This does not prove FFAN is a real actuator; it only tests whether the
    byte is writable and whether firmware keeps or overwrites the change.
EOF
}

acpi_eval() {
  local expr="$1"

  printf '%s\n' "${expr}" > "${CALL_NODE}"
  tr -d '\000' < "${CALL_NODE}" | tr -d '\n'
}

to_u32() {
  local raw="${1//$'\r'/}"

  raw="${raw//$'\n'/}"
  raw="${raw// /}"
  [[ "${raw}" =~ ^0x[0-9A-Fa-f]+$ || "${raw}" =~ ^[0-9]+$ ]] || return 1
  printf '%u' "$((raw))"
}

fmt_byte() {
  printf '0x%02X' "$(($1 & 0xff))"
}

ECRR_PATH=""
ECRW_PATH=""
probe_ecmg_paths() {
  local stem raw

  for stem in '\_SB.INOU' '\_SB_.INOU'; do
    raw="$(acpi_eval "${stem}.ECRR ${FFAN_OFFSET}" || true)"
    if to_u32 "${raw}" >/dev/null 2>&1; then
      ECRR_PATH="${stem}.ECRR"
      ECRW_PATH="${stem}.ECRW"
      return 0
    fi
  done

  echo "Could not probe ECRR/ECRW through acpi_call" >&2
  echo "Last reply: ${raw:-<empty>}" >&2
  return 1
}

read_ffan_raw() {
  local raw

  raw="$(acpi_eval "${ECRR_PATH} ${FFAN_OFFSET}")"
  to_u32 "${raw}"
}

write_ffan_raw() {
  local raw="$1"

  acpi_eval "${ECRW_PATH} ${FFAN_OFFSET} 0x$(printf '%X' "$((raw & 0xff))")" >/dev/null
}

sample_line() {
  local raw="$1"
  printf '%s raw=%s low_nibble=%u\n' \
    "$(date +%T.%3N)" "$(fmt_byte "${raw}")" "$((raw & 0x0f))"
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --level)
      LEVEL="${2:?missing level}"
      shift 2
      ;;
    --hold)
      HOLD_SECONDS="${2:?missing hold seconds}"
      shift 2
      ;;
    --interval)
      INTERVAL="${2:?missing interval}"
      shift 2
      ;;
    --no-restore)
      RESTORE="0"
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      usage >&2
      exit 2
      ;;
  esac
done

[[ -n "${LEVEL}" ]] || {
  usage >&2
  exit 2
}
[[ -w "${CALL_NODE}" ]] || {
  echo "acpi_call not writable: ${CALL_NODE}" >&2
  exit 1
}
[[ "${LEVEL}" =~ ^[0-9]+$ ]] || {
  echo "--level must be an integer between 0 and 15" >&2
  exit 2
}
(( LEVEL >= 0 && LEVEL <= 15 )) || {
  echo "--level must be between 0 and 15" >&2
  exit 2
}

probe_ecmg_paths

original_raw="$(read_ffan_raw)"
target_raw="$(((original_raw & 0xf0) | (LEVEL & 0x0f)))"

echo "== ffan-poke =="
echo "timestamp=$(date -Is)"
echo "ecrr_path=${ECRR_PATH}"
echo "ecrw_path=${ECRW_PATH}"
echo "offset=${FFAN_OFFSET}"
echo "hold_seconds=${HOLD_SECONDS}"
echo "interval=${INTERVAL}"
echo "restore=${RESTORE}"
echo "original_raw=$(fmt_byte "${original_raw}")"
echo "target_raw=$(fmt_byte "${target_raw}")"
echo

echo "[before] $(sample_line "${original_raw}")"
write_ffan_raw "${target_raw}"
after_write_raw="$(read_ffan_raw)"
echo "[after-write] $(sample_line "${after_write_raw}")"

if awk 'BEGIN { exit !('"${HOLD_SECONDS}"' > 0) }'; then
  steps="$(awk -v duration="${HOLD_SECONDS}" -v interval="${INTERVAL}" 'BEGIN { n = int((duration / interval) + 0.999); if (n < 1) n = 1; print n }')"
  for ((i = 1; i <= steps; i++)); do
    sleep "${INTERVAL}"
    current_raw="$(read_ffan_raw)"
    echo "[hold-${i}] $(sample_line "${current_raw}")"
  done
fi

if [[ "${RESTORE}" == "1" ]]; then
  echo
  write_ffan_raw "${original_raw}"
  restored_raw="$(read_ffan_raw)"
  echo "[after-restore] $(sample_line "${restored_raw}")"
fi

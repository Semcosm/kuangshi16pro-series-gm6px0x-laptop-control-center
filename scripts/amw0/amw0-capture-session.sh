#!/usr/bin/env bash
set -euo pipefail

# Safe AMW0 capture session.
# Default use:
#   sudo ./scripts/amw0/amw0-capture-session.sh
# Then press the performance / fan hotkeys while it runs.
#
# It records:
# - EC event / transport bytes: OSDF, LDAT, HDAT, FLAGS, CMDL, CMDH
# - Correlation bytes: HBTN, BRTS, W8BR, EC49, EC4C, EC64, EC65, EC6C, EC6D
# - If acpi_call is available, also WQAC0/WQAC1 and _WED(D2)
#
# This script is read-only. It does not send WMBC payloads.

if [[ ${EUID:-$(id -u)} -ne 0 ]]; then
  echo "Run as root: sudo $0" >&2
  exit 1
fi

require_cmd() {
  command -v "$1" >/dev/null 2>&1 || {
    echo "Missing required command: $1" >&2
    exit 1
  }
}

require_cmd ec_probe
require_cmd awk
require_cmd date
CALL_NODE="/proc/acpi/call"
LOG_DIR="${PWD}/amw0-logs"
TS="$(date +%Y%m%d-%H%M%S)"
LOG_FILE="${LOG_DIR}/capture-${TS}.log"
INTERVAL="0.2"
DURATION="60"
COUNT=0
CHANGES_ONLY=1
STOP=0

mkdir -p "${LOG_DIR}"

usage() {
  cat <<'EOF'
Usage:
  sudo ./scripts/amw0/amw0-capture-session.sh [--duration SEC] [--interval SEC] [--all]

Examples:
  sudo ./scripts/amw0/amw0-capture-session.sh
  sudo ./scripts/amw0/amw0-capture-session.sh --duration 30
  sudo ./scripts/amw0/amw0-capture-session.sh --duration 20 --interval 0.1
  sudo ./scripts/amw0/amw0-capture-session.sh --all

Notes:
  Default mode prints only changed samples.
  During capture, press performance / fan hotkeys several times.
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --duration)
      DURATION="${2:?missing duration}"
      shift 2
      ;;
    --interval)
      INTERVAL="${2:?missing interval}"
      shift 2
      ;;
    --all)
      CHANGES_ONLY=0
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

COUNT="$(python3 - <<PY
duration = float("${DURATION}")
interval = float("${INTERVAL}")
print(max(1, int(duration / interval)))
PY
)"

read_ec() {
  local reg="$1"
  ec_probe read "${reg}" | awk '{print $1}'
}

flags_desc() {
  local raw="$1"
  printf 'R=%d W=%d B=%d C=%d D=%d' \
    "$(( (raw >> 0) & 1 ))" \
    "$(( (raw >> 1) & 1 ))" \
    "$(( (raw >> 2) & 1 ))" \
    "$(( (raw >> 3) & 1 ))" \
    "$(( (raw >> 7) & 1 ))"
}

have_acpi_call() {
  [[ -w "${CALL_NODE}" ]]
}

acpi_eval() {
  local expr="$1"
  printf '%s\n' "${expr}" > "${CALL_NODE}"
  tr -d '\000' < "${CALL_NODE}" | tr -d '\n'
}

read_wqac() {
  local idx="$1"
  acpi_eval "\\_SB.AMW0.WQAC 0x$(printf '%X' "${idx}")"
}

read_wed_d2() {
  acpi_eval "\\_SB.AMW0._WED 0xD2"
}

sample_line() {
  local osdf ldat hdat flags cmdl cmdh hbtn brts w8br e49 e4c e64 e65 e6c e6d
  osdf="$(read_ec 0x7c)"
  ldat="$(read_ec 0x8a)"
  hdat="$(read_ec 0x8b)"
  flags="$(read_ec 0x8c)"
  cmdl="$(read_ec 0x8d)"
  cmdh="$(read_ec 0x8e)"
  hbtn="$(read_ec 0x78)"
  brts="$(read_ec 0x79)"
  w8br="$(read_ec 0x7a)"
  e49="$(read_ec 0x49)"
  e4c="$(read_ec 0x4c)"
  e64="$(read_ec 0x64)"
  e65="$(read_ec 0x65)"
  e6c="$(read_ec 0x6c)"
  e6d="$(read_ec 0x6d)"

  printf '%s OSDF=%s HBTN=%s BRTS=%s W8BR=%s LDAT=%s HDAT=%s FLAGS=%s(%s) CMDL=%s CMDH=%s EC49=%s EC4C=%s EC64=%s EC65=%s EC6C=%s EC6D=%s' \
    "$(date +%T.%3N)" \
    "${osdf}" "${hbtn}" "${brts}" "${w8br}" \
    "${ldat}" "${hdat}" "${flags}" "$(flags_desc "${flags}")" "${cmdl}" "${cmdh}" \
    "${e49}" "${e4c}" "${e64}" "${e65}" "${e6c}" "${e6d}"

  if have_acpi_call; then
    printf ' WQAC0=%s WQAC1=%s WED_D2=%s' \
      "$(read_wqac 0)" \
      "$(read_wqac 1)" \
      "$(read_wed_d2)"
  fi
}

{
  echo "AMW0 capture log: ${LOG_FILE}"
  echo "Timestamp: ${TS}"
  echo "Duration: ${DURATION}s"
  echo "Interval: ${INTERVAL}s"
  echo "Samples: ${COUNT}"
  echo "Changes-only: ${CHANGES_ONLY}"
  if have_acpi_call; then
    echo "acpi_call: enabled"
  else
    echo "acpi_call: unavailable, WQAC/_WED capture disabled"
  fi
  echo
  echo "Instructions:"
  echo "1. Keep this terminal running."
  echo "2. During capture, press performance / fan hotkeys several times."
  echo "3. If there is a vendor control-center key, press that too."
  echo
} | tee -a "${LOG_FILE}"

trap 'STOP=1' INT TERM

last=""
i=0
while (( i < COUNT )) && (( ! STOP )); do
  line="$(sample_line)"
  if (( ! CHANGES_ONLY )) || [[ "${line#* }" != "${last#* }" ]]; then
    printf '%s\n' "${line}" | tee -a "${LOG_FILE}"
    last="${line}"
  fi
  ((i += 1))
  (( STOP )) && break
  sleep "${INTERVAL}"
done

echo "Done. Review ${LOG_FILE}" | tee -a "${LOG_FILE}"

#!/usr/bin/env bash
set -euo pipefail

# Safe AMW0 event-path test.
# This exercises only WMBC(..., 0x03, value), which updates SAC1 and emits
# the D2 WMI event. It does not call OEMG / OEMF and does not send WKBC/SCMD.

if [[ ${EUID:-$(id -u)} -ne 0 ]]; then
  echo "Run as root: sudo $0" >&2
  exit 1
fi

CALL_NODE="/proc/acpi/call"
LOG_DIR="${PWD}/amw0-logs"
TS="$(date +%Y%m%d-%H%M%S)"
LOG_FILE="${LOG_DIR}/event-path-${TS}.log"

mkdir -p "${LOG_DIR}"

[[ -w "${CALL_NODE}" ]] || { echo "acpi_call not writable: ${CALL_NODE}" >&2; exit 1; }
command -v ec_probe >/dev/null 2>&1 || { echo "Missing ec_probe" >&2; exit 1; }

acpi_eval() {
  local expr="$1"
  printf '%s\n' "${expr}" > "${CALL_NODE}"
  tr -d '\000' < "${CALL_NODE}" | tr -d '\n'
}

read_ec() {
  local reg="$1"
  ec_probe read "${reg}" | awk '{print $1}'
}

snapshot() {
  local label="$1"
  {
    printf '[%s] %s\n' "${label}" "$(date +%T)"
    printf 'OSDF=%s HBTN=%s BRTS=%s W8BR=%s\n' \
      "$(read_ec 0x7c)" "$(read_ec 0x78)" "$(read_ec 0x79)" "$(read_ec 0x7a)"
    printf 'WQAC0=%s WQAC1=%s WED_D2=%s\n' \
      "$(acpi_eval '\\_SB.AMW0.WQAC 0x0')" \
      "$(acpi_eval '\\_SB.AMW0.WQAC 0x1')" \
      "$(acpi_eval '\\_SB.AMW0._WED 0xD2')"
    printf '\n'
  } | tee -a "${LOG_FILE}"
}

send_event() {
  local value="$1"
  acpi_eval "\\_SB.AMW0.WMBC 0x0 0x3 0x$(printf '%X' "$((value))")"
}

usage() {
  cat <<'EOF'
Usage:
  sudo ./amw0-event-path-test.sh [0x01 [0xF0 ...]]

Examples:
  sudo ./amw0-event-path-test.sh
  sudo ./amw0-event-path-test.sh 0x01 0xF0 0x00
EOF
}

if [[ $# -eq 0 ]]; then
  set -- 0x01 0xF0 0x00
fi

if [[ $# -gt 0 && ( "$1" == "-h" || "$1" == "--help" ) ]]; then
  usage
  exit 0
fi

{
  echo "AMW0 event-path log: ${LOG_FILE}"
  echo "Timestamp: ${TS}"
  echo
} | tee -a "${LOG_FILE}"

snapshot "baseline"

for arg in "$@"; do
  value="$((arg))"
  {
    printf '=== EVENT 0x%02X ===\n' "${value}"
    printf 'send_result=%s\n' "$(send_event "${value}")"
  } | tee -a "${LOG_FILE}"
  snapshot "after-0x$(printf '%02X' "${value}")"
done

echo "Done. Review ${LOG_FILE}" | tee -a "${LOG_FILE}"

#!/usr/bin/env bash
set -euo pipefail

# Probe Tongfang-style AMW0 WMI/ACPI methods through acpi_call.
# Default mode is read-only.

if [[ ${EUID:-$(id -u)} -ne 0 ]]; then
  echo "Run as root: sudo $0" >&2
  exit 1
fi

CALL_NODE="/proc/acpi/call"
LOG_DIR="${PWD}/amw0-logs"
TS="$(date +%Y%m%d-%H%M%S)"
LOG_FILE="${LOG_DIR}/probe-${TS}.log"

mkdir -p "${LOG_DIR}"

if [[ ! -w "${CALL_NODE}" ]]; then
  echo "acpi_call node is not writable: ${CALL_NODE}" >&2
  exit 1
fi

acpi_eval() {
  local expr="$1"
  printf '%s\n' "${expr}" > "${CALL_NODE}"
  tr -d '\000' < "${CALL_NODE}"
}

log_call() {
  local label="$1"
  local expr="$2"
  local result
  result="$(acpi_eval "${expr}")"
  printf '%s\n' "${label}: ${expr}" | tee -a "${LOG_FILE}"
  printf '%s\n' "${result}" | tee -a "${LOG_FILE}"
  printf '\n' | tee -a "${LOG_FILE}"
}

dump_read_only() {
  local i

  echo "=== WQAA (AA data block) ===" | tee -a "${LOG_FILE}"
  for i in 0 1 2 3; do
    log_call "WQAA[${i}]" "\\_SB.AMW0.WQAA 0x$(printf '%X' "${i}")"
  done

  echo "=== WQAB (AB data block) ===" | tee -a "${LOG_FILE}"
  for i in 0 1 2 3; do
    log_call "WQAB[${i}]" "\\_SB.AMW0.WQAB 0x$(printf '%X' "${i}")"
  done

  echo "=== WQAC (AC data block) ===" | tee -a "${LOG_FILE}"
  for i in 0 1 2 3 4 5 6 7 8 9; do
    log_call "WQAC[${i}]" "\\_SB.AMW0.WQAC 0x$(printf '%X' "${i}")"
  done

  echo "=== _WED events ===" | tee -a "${LOG_FILE}"
  for i in 0xD0 0xD1 0xD2; do
    log_call "_WED[${i}]" "\\_SB.AMW0._WED ${i}"
  done

  echo "=== WMBA/WMBB/WMBC read mode ===" | tee -a "${LOG_FILE}"
  for i in 0 1 2 3; do
    log_call "WMBA[${i},read]" "\\_SB.AMW0.WMBA 0x$(printf '%X' "${i}") 0x1 0x0"
    log_call "WMBB[${i},read]" "\\_SB.AMW0.WMBB 0x$(printf '%X' "${i}") 0x1 0x0"
  done
  for i in 0 1 2 3 4 5 6 7 8 9; do
    log_call "WMBC[${i},read]" "\\_SB.AMW0.WMBC 0x$(printf '%X' "${i}") 0x1 0x0"
  done
}

usage() {
  cat <<'EOF'
Usage:
  sudo ./amw0-probe.sh
    Read-only dump of \_SB.AMW0.WQAA/WQAB/WQAC.

  sudo ./amw0-probe.sh call '<acpi_call expression>'
    Execute a custom acpi_call expression once.

Examples:
  sudo ./amw0-probe.sh
  sudo ./amw0-probe.sh call '\_SB.AMW0.WQAC 0x0'
  sudo ./amw0-probe.sh call '\_SB.AMW0.WMBA 0x0 0x1 0x0'
EOF
}

main() {
  {
    echo "AMW0 probe log: ${LOG_FILE}"
    echo "Timestamp: ${TS}"
    echo
  } | tee -a "${LOG_FILE}"

  if [[ $# -eq 0 ]]; then
    dump_read_only
    echo "Done. Review ${LOG_FILE}" | tee -a "${LOG_FILE}"
    return 0
  fi

  if [[ $# -eq 2 && "$1" == "call" ]]; then
    log_call "custom" "$2"
    echo "Done. Review ${LOG_FILE}" | tee -a "${LOG_FILE}"
    return 0
  fi

  usage
  exit 2
}

main "$@"

#!/usr/bin/env bash
set -euo pipefail

# Historical EC probing helper kept for reference.
# It performs short writes to candidate registers and restores them, but AML
# analysis now shows 0x49 is ACUR rather than a fan register, so this script
# should not be treated as an AML-backed fan-control path.

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

READ_REGS=(0x40 0x42 0x48 0x49 0x4b 0x4c 0x64 0x65 0x6c 0x6d)
LOG_DIR="${PWD}/ec-probe-logs"
TS="$(date +%Y%m%d-%H%M%S)"
LOG_FILE="${LOG_DIR}/probe-${TS}.log"

mkdir -p "${LOG_DIR}"

read_reg() {
  local reg="$1"
  ec_probe read "${reg}" | awk '{print $1}'
}

sample_once() {
  local now
  now="$(date +%T)"
  printf "%s " "${now}" | tee -a "${LOG_FILE}"
  local reg value
  for reg in "${READ_REGS[@]}"; do
    value="$(read_reg "${reg}")"
    printf "%s=%s " "${reg}" "${value}" | tee -a "${LOG_FILE}"
  done
  printf "\n" | tee -a "${LOG_FILE}"
}

sample_loop() {
  local seconds="$1"
  local i
  for ((i = 0; i < seconds * 2; i++)); do
    sample_once
    sleep 0.5
  done
}

write_and_hold() {
  local reg="$1"
  local value="$2"
  local hold_seconds="$3"
  echo "WRITE ${reg}=${value} hold=${hold_seconds}s" | tee -a "${LOG_FILE}"
  ec_probe write "${reg}" "${value}"
  sample_loop "${hold_seconds}"
}

restore_reg() {
  local reg="$1"
  local value="$2"
  echo "RESTORE ${reg}=${value}" | tee -a "${LOG_FILE}"
  ec_probe write "${reg}" "${value}"
}

main() {
  local orig_49 orig_4c
  orig_49="$(read_reg 0x49)"
  orig_4c="$(read_reg 0x4c)"

  {
    echo "=== EC dual-fan probe ==="
    echo "Log: ${LOG_FILE}"
    echo "Warning: AML analysis does not support 0x49/0x4c as real fan-control registers."
    echo "Original 0x49=${orig_49} 0x4c=${orig_4c}"
    echo
    echo "[baseline]"
  } | tee -a "${LOG_FILE}"

  sample_loop 4

  echo | tee -a "${LOG_FILE}"
  echo "[test 0x49 -> 30]" | tee -a "${LOG_FILE}"
  write_and_hold 0x49 30 3
  restore_reg 0x49 "${orig_49}"
  sample_loop 3

  echo | tee -a "${LOG_FILE}"
  echo "[test 0x4c -> 45]" | tee -a "${LOG_FILE}"
  write_and_hold 0x4c 45 3
  restore_reg 0x4c "${orig_4c}"
  sample_loop 3

  echo | tee -a "${LOG_FILE}"
  echo "Done. Review ${LOG_FILE}" | tee -a "${LOG_FILE}"
}

main "$@"

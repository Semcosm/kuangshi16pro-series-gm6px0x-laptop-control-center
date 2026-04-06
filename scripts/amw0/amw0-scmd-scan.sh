#!/usr/bin/env bash
set -euo pipefail

# Minimal-risk scanner for AMW0 SCMD-style commands.
# Sends one-byte commands via WMBC(send, SAC1=0x0200) and records:
# - WQAC[0]/WQAC[1] transport dwords from AC00
# - EC transport/event bytes around OSDF and LDAT/HDAT/CMDL/CMDH
# - a small set of side-effect EC bytes that changed during earlier probes
#
# AML mapping notes:
# - WQAC[0]/WQAC[1] are GETC(0/1), i.e. SAC0/SAC1 from AC00.
# - For SCMD, byte 0 is the command and bytes 4..7 are route dword 0x0200.
# - AML clears SAC1/SAC2 after OEMG, so WQAC1 usually returns to zero.
# - EC 0x7c is OSDF, and 0x8a..0x8e are LDAT/HDAT/flags/CMDL/CMDH.

if [[ ${EUID:-$(id -u)} -ne 0 ]]; then
  echo "Run as root: sudo $0" >&2
  exit 1
fi

CALL_NODE="/proc/acpi/call"
LOG_DIR="${PWD}/amw0-logs"
TS="$(date +%Y%m%d-%H%M%S)"
LOG_FILE="${LOG_DIR}/scmd-scan-${TS}.log"

mkdir -p "${LOG_DIR}"

[[ -w "${CALL_NODE}" ]] || { echo "acpi_call not writable: ${CALL_NODE}" >&2; exit 1; }
command -v ec_probe >/dev/null 2>&1 || { echo "Missing ec_probe" >&2; exit 1; }

acpi_eval() {
  local expr="$1"
  printf '%s\n' "${expr}" > "${CALL_NODE}"
  tr -d '\000' < "${CALL_NODE}"
}

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

read_wqac() {
  local idx="$1"
  acpi_eval "\\_SB.AMW0.WQAC 0x$(printf '%X' "${idx}")"
}

send_scmd() {
  local cmd="$1"
  local expr
  expr="\\_SB.AMW0.WMBC 0x0 0x4 {0x$(printf '%02X' "${cmd}"),0x00,0x00,0x00,0x00,0x02,0x00,0x00,0x02,0x02,0x02,0x02,0x03,0x03,0x03,0x03,0x04,0x04,0x04,0x04,0x05,0x05,0x05,0x05,0x06,0x06,0x06,0x06,0x07,0x07,0x07,0x07,0x08,0x08,0x08,0x08,0x09,0x09,0x09,0x09}"
  acpi_eval "${expr}"
}

snapshot() {
  local label="$1"
  local osdf ldat hdat flags cmdl cmdh
  osdf="$(read_ec 0x7c)"
  ldat="$(read_ec 0x8a)"
  hdat="$(read_ec 0x8b)"
  flags="$(read_ec 0x8c)"
  cmdl="$(read_ec 0x8d)"
  cmdh="$(read_ec 0x8e)"
  {
    printf '[%s] %s\n' "${label}" "$(date +%T)"
    printf 'WQAC0=%s\n' "$(read_wqac 0)"
    printf 'WQAC1=%s\n' "$(read_wqac 1)"
    printf 'OSDF=%s LDAT=%s HDAT=%s FLAGS=%s (%s) CMDL=%s CMDH=%s\n' \
      "${osdf}" "${ldat}" "${hdat}" "${flags}" "$(flags_desc "${flags}")" "${cmdl}" "${cmdh}"
    printf 'EC49=%s EC4C=%s EC64=%s EC65=%s EC6C=%s EC6D=%s\n' \
      "$(read_ec 0x49)" "$(read_ec 0x4c)" "$(read_ec 0x64)" \
      "$(read_ec 0x65)" "$(read_ec 0x6c)" "$(read_ec 0x6d)"
    printf '\n'
  } | tee -a "${LOG_FILE}"
}

usage() {
  cat <<'EOF'
Usage:
  sudo ./scripts/amw0/amw0-scmd-scan.sh 0x90 [0x91 ...]
  sudo ./scripts/amw0/amw0-scmd-scan.sh --hold 10 0x87 0x94 0x97

Example:
  sudo ./scripts/amw0/amw0-scmd-scan.sh 0x80 0x81 0x82 0x90
EOF
}

HOLD_SECONDS=1
if [[ $# -ge 2 && "$1" == "--hold" ]]; then
  HOLD_SECONDS="$2"
  shift 2
fi

[[ $# -ge 1 ]] || { usage; exit 2; }

{
  echo "AMW0 SCMD scan log: ${LOG_FILE}"
  echo "Timestamp: ${TS}"
  echo
} | tee -a "${LOG_FILE}"

snapshot "baseline"

observe_window() {
  local label="$1"
  local steps i
  local max49=0 max4c=0 max64=0 max65=0 max6c=0 max6d=0
  steps=$(( HOLD_SECONDS < 1 ? 1 : HOLD_SECONDS ))

  for ((i = 0; i < steps; i++)); do
    local v49 v4c v64 v65 v6c v6d
    v49="$(read_ec 0x49)"
    v4c="$(read_ec 0x4c)"
    v64="$(read_ec 0x64)"
    v65="$(read_ec 0x65)"
    v6c="$(read_ec 0x6c)"
    v6d="$(read_ec 0x6d)"

    (( v49 > max49 )) && max49="$v49"
    (( v4c > max4c )) && max4c="$v4c"
    (( v64 > max64 )) && max64="$v64"
    (( v65 > max65 )) && max65="$v65"
    (( v6c > max6c )) && max6c="$v6c"
    (( v6d > max6d )) && max6d="$v6d"

    printf '[%s+t=%02d] %s EC49=%s EC4C=%s EC64=%s EC65=%s EC6C=%s EC6D=%s\n' \
      "${label}" "$i" "$(date +%T)" "$v49" "$v4c" "$v64" "$v65" "$v6c" "$v6d" \
      | tee -a "${LOG_FILE}"
    sleep 1
  done

  printf '[%s-summary] max49=%s max4c=%s max64=%s max65=%s max6c=%s max6d=%s\n\n' \
    "${label}" "$max49" "$max4c" "$max64" "$max65" "$max6c" "$max6d" \
    | tee -a "${LOG_FILE}"
}

for arg in "$@"; do
  cmd="$((arg))"
  {
    printf '=== CMD 0x%02X ===\n' "${cmd}"
    printf 'send_result=%s\n' "$(send_scmd "${cmd}")"
  } | tee -a "${LOG_FILE}"
  snapshot "postsend-0x$(printf '%02X' "${cmd}")"
  observe_window "cmd-0x$(printf '%02X' "${cmd}")"
  snapshot "after-0x$(printf '%02X' "${cmd}")"
done

echo "Done. Review ${LOG_FILE}" | tee -a "${LOG_FILE}"

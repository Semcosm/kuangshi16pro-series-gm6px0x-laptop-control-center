#!/usr/bin/env bash
set -euo pipefail

# Minimal-risk scanner for AMW0 WKBC-style 4-byte commands.
# Sends one payload at a time via WMBC(send, SAC1=0x0000 or 0x0001).
# AC00 layout from AML is:
# - bytes 0..3  = SA00..SA03 -> WKBC arguments
# - bytes 4..7  = SAC1 route dword
# and records:
# - WQAC[0]/WQAC[1] transport dwords from AC00
# - EC transport/event bytes around OSDF and LDAT/HDAT/CMDL/CMDH
# - a small set of side-effect EC bytes that changed during earlier probes
#
# AML mapping notes:
# - WQAC[0]/WQAC[1] are GETC(0/1), i.e. SAC0/SAC1 from AC00.
# - For WMBC(..., 0x04, buffer), WQAC0 mirrors SA00..SA03 as a dword.
# - AML clears SAC1/SAC2 after OEMG, so WQAC1 usually returns to zero.
# - EC 0x7c is OSDF, and 0x8a..0x8e are LDAT/HDAT/flags/CMDL/CMDH.

if [[ ${EUID:-$(id -u)} -ne 0 ]]; then
  echo "Run as root: sudo $0" >&2
  exit 1
fi

CALL_NODE="/proc/acpi/call"
LOG_DIR="${PWD}/amw0-logs"
TS="$(date +%Y%m%d-%H%M%S)"
LOG_FILE="${LOG_DIR}/wkbc-scan-${TS}.log"
HOLD_SECONDS=8

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

send_wkbc() {
  local sac1="$1"
  local sa00="$2"
  local sa01="$3"
  local sa02="$4"
  local sa03="$5"
  local expr
  expr="\\_SB.AMW0.WMBC 0x0 0x4 {0x$(printf '%02X' "$((sa00))"),0x$(printf '%02X' "$((sa01))"),0x$(printf '%02X' "$((sa02))"),0x$(printf '%02X' "$((sa03))"),0x$(printf '%02X' "$((sac1 & 0xff))"),0x$(printf '%02X' "$(((sac1 >> 8) & 0xff))"),0x00,0x00,0x02,0x02,0x02,0x02,0x03,0x03,0x03,0x03,0x04,0x04,0x04,0x04,0x05,0x05,0x05,0x05,0x06,0x06,0x06,0x06,0x07,0x07,0x07,0x07,0x08,0x08,0x08,0x08,0x09,0x09,0x09,0x09}"
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

usage() {
  cat <<'EOF'
Usage:
  sudo ./scripts/amw0/amw0-wkbc-scan.sh [--hold N] SAC1:SA00:SA01:SA02:SA03 [...]

Examples:
  sudo ./scripts/amw0/amw0-wkbc-scan.sh 0x0000:0x49:0x00:0x1E:0x00
  sudo ./scripts/amw0/amw0-wkbc-scan.sh --hold 10 0x0000:0x49:0x00:0x1E:0x00 0x0001:0x4c:0x00:0x28:0x00
EOF
}

if [[ $# -ge 2 && "$1" == "--hold" ]]; then
  HOLD_SECONDS="$2"
  shift 2
fi

[[ $# -ge 1 ]] || { usage; exit 2; }

{
  echo "AMW0 WKBC scan log: ${LOG_FILE}"
  echo "Timestamp: ${TS}"
  echo
} | tee -a "${LOG_FILE}"

snapshot "baseline"

for spec in "$@"; do
  IFS=: read -r sac1 sa00 sa01 sa02 sa03 <<<"${spec}"
  {
    printf '=== SPEC %s ===\n' "${spec}"
    printf 'send_result=%s\n' "$(send_wkbc "$((sac1))" "$((sa00))" "$((sa01))" "$((sa02))" "$((sa03))")"
  } | tee -a "${LOG_FILE}"
  snapshot "postsend-${spec}"
  observe_window "spec-${spec}"
  snapshot "after-${spec}"
done

echo "Done. Review ${LOG_FILE}" | tee -a "${LOG_FILE}"

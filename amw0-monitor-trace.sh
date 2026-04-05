#!/usr/bin/env bash
set -euo pipefail

# Use ec_probe monitor to capture full-EC changes while issuing one WMBC(send).
# This is slower to analyze but much better suited for very short EC transactions
# than repeated point reads from shell.

if [[ ${EUID:-$(id -u)} -ne 0 ]]; then
  echo "Run as root: sudo $0" >&2
  exit 1
fi

CALL_NODE="/proc/acpi/call"
LOG_DIR="${PWD}/amw0-logs"
TS="$(date +%Y%m%d-%H%M%S)"
BASE="${LOG_DIR}/monitor-trace-${TS}"
LOG_FILE="${BASE}.log"
MONITOR_OUT="${BASE}.monitor.txt"
INTERVAL="0.1"
PRE_DELAY="0.20"
POST_DELAY="0.50"

mkdir -p "${LOG_DIR}"

[[ -w "${CALL_NODE}" ]] || { echo "acpi_call not writable: ${CALL_NODE}" >&2; exit 1; }
command -v ec_probe >/dev/null 2>&1 || { echo "Missing ec_probe" >&2; exit 1; }

usage() {
  cat <<'EOF'
Usage:
  sudo ./amw0-monitor-trace.sh [--interval SEC] [--pre SEC] [--post SEC] wkbc SAC1:SA00:SA01:SA02:SA03
  sudo ./amw0-monitor-trace.sh [--interval SEC] [--pre SEC] [--post SEC] scmd CMD

Examples:
  sudo ./amw0-monitor-trace.sh wkbc 0x0000:0x49:0x00:0x1E:0x00
  sudo ./amw0-monitor-trace.sh wkbc 0x0001:0x4C:0x00:0x28:0x00
  sudo ./amw0-monitor-trace.sh scmd 0x87
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --interval)
      INTERVAL="${2:?missing interval}"
      shift 2
      ;;
    --pre)
      PRE_DELAY="${2:?missing pre delay}"
      shift 2
      ;;
    --post)
      POST_DELAY="${2:?missing post delay}"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      break
      ;;
  esac
done

[[ $# -ge 2 ]] || { usage >&2; exit 2; }

MODE="$1"
shift

acpi_eval() {
  local expr="$1"
  printf '%s\n' "${expr}" > "${CALL_NODE}"
  tr -d '\000' < "${CALL_NODE}" | tr -d '\n'
}

wm_payload_wkbc() {
  local sac1="$1" sa00="$2" sa01="$3" sa02="$4" sa03="$5"
  printf '{0x%02X,0x%02X,0x%02X,0x%02X,0x%02X,0x%02X,0x00,0x00,0x02,0x02,0x02,0x02,0x03,0x03,0x03,0x03,0x04,0x04,0x04,0x04,0x05,0x05,0x05,0x05,0x06,0x06,0x06,0x06,0x07,0x07,0x07,0x07,0x08,0x08,0x08,0x08,0x09,0x09,0x09,0x09}' \
    "$((sa00))" "$((sa01))" "$((sa02))" "$((sa03))" "$((sac1 & 0xff))" "$(((sac1 >> 8) & 0xff))"
}

wm_payload_scmd() {
  local cmd="$1"
  printf '{0x%02X,0x00,0x00,0x00,0x00,0x02,0x00,0x00,0x02,0x02,0x02,0x02,0x03,0x03,0x03,0x03,0x04,0x04,0x04,0x04,0x05,0x05,0x05,0x05,0x06,0x06,0x06,0x06,0x07,0x07,0x07,0x07,0x08,0x08,0x08,0x08,0x09,0x09,0x09,0x09}' \
    "$((cmd))"
}

case "${MODE}" in
  wkbc)
    IFS=: read -r sac1 sa00 sa01 sa02 sa03 <<<"${1:?missing wkbc spec}"
    PAYLOAD="$(wm_payload_wkbc "$((sac1))" "$((sa00))" "$((sa01))" "$((sa02))" "$((sa03))")"
    LABEL="wkbc:${1}"
    ;;
  scmd)
    cmd="${1:?missing scmd command}"
    PAYLOAD="$(wm_payload_scmd "$((cmd))")"
    LABEL="scmd:${cmd}"
    ;;
  *)
    usage >&2
    exit 2
    ;;
esac

{
  echo "AMW0 monitor trace log: ${LOG_FILE}"
  echo "Timestamp: ${TS}"
  echo "Label: ${LABEL}"
  echo "Payload: ${PAYLOAD}"
  echo "Monitor interval: ${INTERVAL}s"
  echo "Pre-delay: ${PRE_DELAY}s"
  echo "Post-delay: ${POST_DELAY}s"
  echo "Monitor output: ${MONITOR_OUT}"
  echo
  echo "Baseline WQAC0=$(acpi_eval '\_SB.AMW0.WQAC 0x0') WQAC1=$(acpi_eval '\_SB.AMW0.WQAC 0x1') WED_D2=$(acpi_eval '\_SB.AMW0._WED 0xD2')"
  echo
} | tee -a "${LOG_FILE}"

MONITOR_TIME="$(python3 - <<PY
import math
print(max(1, math.ceil(float("${PRE_DELAY}") + float("${POST_DELAY}") + 1.0)))
PY
)"

ec_probe monitor -i "${INTERVAL}" -t "${MONITOR_TIME}" > "${MONITOR_OUT}" 2>&1 &
MONITOR_PID=$!
cleanup() {
  kill "${MONITOR_PID}" >/dev/null 2>&1 || true
  wait "${MONITOR_PID}" 2>/dev/null || true
}
trap cleanup EXIT INT TERM

sleep "${PRE_DELAY}"

START_NS="$(date +%s%N)"
SEND_RESULT="$(acpi_eval "\\_SB.AMW0.WMBC 0x0 0x4 ${PAYLOAD}")"
END_NS="$(date +%s%N)"

{
  echo "SEND start_ns=${START_NS}"
  echo "SEND end_ns=${END_NS}"
  echo "SEND duration_ns=$((END_NS - START_NS))"
  echo "SEND result=${SEND_RESULT}"
  echo
} | tee -a "${LOG_FILE}"

sleep "${POST_DELAY}"

{
  echo "After WQAC0=$(acpi_eval '\_SB.AMW0.WQAC 0x0') WQAC1=$(acpi_eval '\_SB.AMW0.WQAC 0x1') WED_D2=$(acpi_eval '\_SB.AMW0._WED 0xD2')"
  echo
} | tee -a "${LOG_FILE}"

wait "${MONITOR_PID}" || true
trap - EXIT INT TERM

if grep -q '^ERROR:' "${MONITOR_OUT}"; then
  echo "Monitor failed; see ${MONITOR_OUT}" | tee -a "${LOG_FILE}"
  exit 1
fi

echo "Done. Review ${LOG_FILE} and ${MONITOR_OUT}" | tee -a "${LOG_FILE}"

#!/usr/bin/env bash
set -euo pipefail

# Trace transient EC bytes while issuing one AMW0 WMBC(send) request.
# This is meant to catch LDAT/HDAT/CMDL/CMDH/FLAGS during the short window
# where WKBC / SCMD is active, before AML clears those bytes again.

if [[ ${EUID:-$(id -u)} -ne 0 ]]; then
  echo "Run as root: sudo $0" >&2
  exit 1
fi

CALL_NODE="/proc/acpi/call"
LOG_DIR="${PWD}/amw0-logs"
TS="$(date +%Y%m%d-%H%M%S)"
LOG_FILE="${LOG_DIR}/wmbc-trace-${TS}.log"
INTERVAL="0.01"
PRE_DELAY="0.20"
POST_DELAY="0.50"
SAMPLE_EXTRA=0

mkdir -p "${LOG_DIR}"

[[ -w "${CALL_NODE}" ]] || { echo "acpi_call not writable: ${CALL_NODE}" >&2; exit 1; }
command -v ec_probe >/dev/null 2>&1 || { echo "Missing ec_probe" >&2; exit 1; }

usage() {
  cat <<'EOF'
Usage:
  sudo ./amw0-wmbc-trace.sh [--interval SEC] [--pre SEC] [--post SEC] [--extra] wkbc SAC1:SA00:SA01:SA02:SA03
  sudo ./amw0-wmbc-trace.sh [--interval SEC] [--pre SEC] [--post SEC] [--extra] scmd CMD

Examples:
  sudo ./amw0-wmbc-trace.sh wkbc 0x0000:0x49:0x00:0x1E:0x00
  sudo ./amw0-wmbc-trace.sh wkbc 0x0001:0x4C:0x00:0x28:0x00
  sudo ./amw0-wmbc-trace.sh scmd 0x00
  sudo ./amw0-wmbc-trace.sh --interval 0.005 --extra wkbc 0x0000:0x49:0x00:0x1E:0x00
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
    --extra)
      SAMPLE_EXTRA=1
      shift
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

sample_line() {
  local osdf ldat hdat flags cmdl cmdh
  osdf="$(read_ec 0x7c)"
  ldat="$(read_ec 0x8a)"
  hdat="$(read_ec 0x8b)"
  flags="$(read_ec 0x8c)"
  cmdl="$(read_ec 0x8d)"
  cmdh="$(read_ec 0x8e)"

  printf '%s OSDF=%s LDAT=%s HDAT=%s FLAGS=%s(%s) CMDL=%s CMDH=%s' \
    "$(date +%T.%3N)" \
    "${osdf}" "${ldat}" "${hdat}" "${flags}" "$(flags_desc "${flags}")" "${cmdl}" "${cmdh}"

  if (( SAMPLE_EXTRA )); then
    printf ' EC49=%s EC4C=%s EC64=%s EC65=%s EC6C=%s EC6D=%s' \
      "$(read_ec 0x49)" "$(read_ec 0x4c)" "$(read_ec 0x64)" \
      "$(read_ec 0x65)" "$(read_ec 0x6c)" "$(read_ec 0x6d)"
  fi
}

watch_loop() {
  while :; do
    printf 'TRACE %s\n' "$(sample_line)" >> "${LOG_FILE}"
    sleep "${INTERVAL}"
  done
}

snapshot() {
  local label="$1"
  {
    printf '[%s] %s\n' "${label}" "$(date +%T)"
    printf 'WQAC0=%s WQAC1=%s WED_D2=%s\n' \
      "$(acpi_eval '\_SB.AMW0.WQAC 0x0')" \
      "$(acpi_eval '\_SB.AMW0.WQAC 0x1')" \
      "$(acpi_eval '\_SB.AMW0._WED 0xD2')"
    printf 'EC %s\n' "$(sample_line)"
    printf '\n'
  } | tee -a "${LOG_FILE}"
}

{
  echo "AMW0 WMBC trace log: ${LOG_FILE}"
  echo "Timestamp: ${TS}"
  echo "Label: ${LABEL}"
  echo "Payload: ${PAYLOAD}"
  echo "Interval: ${INTERVAL}s"
  echo "Pre-delay: ${PRE_DELAY}s"
  echo "Post-delay: ${POST_DELAY}s"
  echo "Extra EC bytes: ${SAMPLE_EXTRA}"
  echo
} | tee -a "${LOG_FILE}"

snapshot "baseline"

watch_loop &
WATCH_PID=$!
cleanup() {
  kill "${WATCH_PID}" >/dev/null 2>&1 || true
  wait "${WATCH_PID}" 2>/dev/null || true
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

snapshot "after-send"

cleanup
trap - EXIT INT TERM

echo "Done. Review ${LOG_FILE}" | tee -a "${LOG_FILE}"

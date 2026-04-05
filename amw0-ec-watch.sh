#!/usr/bin/env bash
set -euo pipefail

# Watch AMW0-related EC state while pressing hotkeys or sending WMBC payloads.
# This focuses on AML-backed event/transport bytes:
# - 0x7c: OSDF
# - 0x8a..0x8e: LDAT/HDAT/flags/CMDL/CMDH
# It also keeps a few legacy side-effect bytes in view for correlation.

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

LOG_DIR="${PWD}/amw0-logs"
TS="$(date +%Y%m%d-%H%M%S)"
LOG_FILE="${LOG_DIR}/ec-watch-${TS}.log"
INTERVAL="0.2"
COUNT=0
CHANGES_ONLY=0

mkdir -p "${LOG_DIR}"

usage() {
  cat <<'EOF'
Usage:
  sudo ./amw0-ec-watch.sh [--interval SEC] [--count N] [--changes-only]

Examples:
  sudo ./amw0-ec-watch.sh
  sudo ./amw0-ec-watch.sh --changes-only
  sudo ./amw0-ec-watch.sh --interval 0.1 --count 300
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --interval)
      INTERVAL="${2:?missing interval}"
      shift 2
      ;;
    --count)
      COUNT="${2:?missing count}"
      shift 2
      ;;
    --changes-only)
      CHANGES_ONLY=1
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
}

{
  echo "AMW0 EC watch log: ${LOG_FILE}"
  echo "Timestamp: ${TS}"
  echo "Interval: ${INTERVAL}s"
  if (( COUNT > 0 )); then
    echo "Count: ${COUNT}"
  else
    echo "Count: unlimited"
  fi
  echo "Changes-only: ${CHANGES_ONLY}"
  echo
} | tee -a "${LOG_FILE}"

last=""
i=0
while :; do
  line="$(sample_line)"
  if (( ! CHANGES_ONLY )) || [[ "${line#* }" != "${last#* }" ]]; then
    printf '%s\n' "${line}" | tee -a "${LOG_FILE}"
    last="${line}"
  fi
  ((i += 1))
  if (( COUNT > 0 && i >= COUNT )); then
    break
  fi
  sleep "${INTERVAL}"
done

echo "Done. Review ${LOG_FILE}" | tee -a "${LOG_FILE}"

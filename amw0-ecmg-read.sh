#!/usr/bin/env bash
set -euo pipefail

# Read ECMG-backed fan/thermal fields via the AML helper \_SB.INOU.ECRR.
# This avoids needing a separate devmem-style utility and stays aligned
# with the firmware's own SystemMemory accessor.

if [[ ${EUID:-$(id -u)} -ne 0 ]]; then
  echo "Run as root: sudo $0" >&2
  exit 1
fi

CALL_NODE="/proc/acpi/call"
LOG_DIR="${PWD}/amw0-logs"
TS="$(date +%Y%m%d-%H%M%S)"
LOG_FILE="${LOG_DIR}/ecmg-read-${TS}.log"
INTERVAL="0.5"
WATCH_SECONDS="0"
LABEL="snapshot"

mkdir -p "${LOG_DIR}"

[[ -w "${CALL_NODE}" ]] || { echo "acpi_call not writable: ${CALL_NODE}" >&2; exit 1; }

usage() {
  cat <<'EOF'
Usage:
  sudo ./amw0-ecmg-read.sh
  sudo ./amw0-ecmg-read.sh --watch 10 --interval 0.2
  sudo ./amw0-ecmg-read.sh --label baseline

Options:
  --watch SEC      Re-sample for SEC seconds after the first sample.
  --interval SEC   Delay between samples when --watch is used. Default: 0.5
  --label TEXT     Label for the first sample. Default: snapshot
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --watch)
      WATCH_SECONDS="${2:?missing watch seconds}"
      shift 2
      ;;
    --interval)
      INTERVAL="${2:?missing interval}"
      shift 2
      ;;
    --label)
      LABEL="${2:?missing label}"
      shift 2
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

ECRR_PATH=""
probe_ecrr_path() {
  local candidate raw
  for candidate in '\_SB.INOU.ECRR' '\_SB_.INOU.ECRR'; do
    raw="$(acpi_eval "${candidate} 0x460" || true)"
    if to_u32 "${raw}" >/dev/null 2>&1; then
      ECRR_PATH="${candidate}"
      return 0
    fi
  done
  echo "Could not call ECRR through acpi_call" >&2
  echo "Last reply: ${raw:-<empty>}" >&2
  return 1
}

read_ecrr() {
  local offset="$1"
  local raw
  raw="$(acpi_eval "${ECRR_PATH} 0x$(printf '%X' "$((offset))")")"
  to_u32 "${raw}"
}

fmt_byte() {
  printf '0x%02X' "$(( $1 & 0xff ))"
}

sample_line() {
  local raw460 raw466 raw468
  local cput pcht sn1t sn2t sn3t sn4t sn5t
  local f1sh f1sl rawe8c rawe9d
  local ffan s0e3 s0e1 sdan f1dc f1cm f2dc f2cm

  raw460="$(read_ecrr 0x460)"
  raw466="$(read_ecrr 0x466)"
  raw468="$(read_ecrr 0x468)"
  cput="$(read_ecrr 0xE0D)"
  pcht="$(read_ecrr 0xE0E)"
  sn1t="$(read_ecrr 0xE10)"
  sn2t="$(read_ecrr 0xE12)"
  sn3t="$(read_ecrr 0xE14)"
  sn4t="$(read_ecrr 0xE16)"
  sn5t="$(read_ecrr 0xE18)"
  f1sh="$(read_ecrr 0xE1C)"
  f1sl="$(read_ecrr 0xE1D)"
  rawe8c="$(read_ecrr 0xE8C)"
  rawe9d="$(read_ecrr 0xE9D)"

  ffan=$(( raw460 & 0x0f ))
  s0e3=$(( (raw466 >> 4) & 1 ))
  s0e1=$(( (raw466 >> 6) & 1 ))
  sdan=$(( raw468 & 0x0f ))
  f1dc=$(( rawe8c & 0x7f ))
  f1cm=$(( (rawe8c >> 7) & 1 ))
  f2dc=$(( rawe9d & 0x7f ))
  f2cm=$(( (rawe9d >> 7) & 1 ))

  printf '%s FFAN=%u RAW460=%s S0E3=%u S0E1=%u SDAN=%u RAW466=%s RAW468=%s CPUT=%u PCHT=%u SN1T=%u SN2T=%u SN3T=%u SN4T=%u SN5T=%u F1SH=%u F1SL=%u F1DC=%u F1CM=%u F2DC=%u F2CM=%u RAWE8C=%s RAWE9D=%s' \
    "$(date +%T.%3N)" \
    "${ffan}" "$(fmt_byte "${raw460}")" \
    "${s0e3}" "${s0e1}" "${sdan}" "$(fmt_byte "${raw466}")" "$(fmt_byte "${raw468}")" \
    "${cput}" "${pcht}" "${sn1t}" "${sn2t}" "${sn3t}" "${sn4t}" "${sn5t}" \
    "${f1sh}" "${f1sl}" "${f1dc}" "${f1cm}" "${f2dc}" "${f2cm}" \
    "$(fmt_byte "${rawe8c}")" "$(fmt_byte "${rawe9d}")"
}

emit_sample() {
  local label="$1"
  printf '[%s] %s\n' "${label}" "$(sample_line)" | tee -a "${LOG_FILE}"
}

probe_ecrr_path

{
  echo "AMW0 ECMG read log: ${LOG_FILE}"
  echo "Timestamp: ${TS}"
  echo "ECRR path: ${ECRR_PATH}"
  echo "Watch seconds: ${WATCH_SECONDS}"
  echo "Interval: ${INTERVAL}s"
  echo
} | tee -a "${LOG_FILE}"

emit_sample "${LABEL}"

if awk 'BEGIN { exit !('"${WATCH_SECONDS}"' > 0) }'; then
  steps="$(awk -v duration="${WATCH_SECONDS}" -v interval="${INTERVAL}" 'BEGIN { n = int((duration / interval) + 0.999); if (n < 1) n = 1; print n }')"
  for ((i = 1; i <= steps; i++)); do
    sleep "${INTERVAL}"
    emit_sample "watch-${i}"
  done
fi

echo "Done. Review ${LOG_FILE}" | tee -a "${LOG_FILE}"

#!/usr/bin/env bash
set -euo pipefail

if [[ ${EUID:-$(id -u)} -ne 0 ]]; then
  echo "Run as root: sudo $0" >&2
  exit 1
fi

CALL_NODE="/proc/acpi/call"
HWMON_ROOT="/sys/class/hwmon"
INTERVAL="0.5"
WATCH_SECONDS="0"
LABEL="snapshot"
EXTRA_EC="0"
WRAPPED_PID=""
WRAPPED_STATUS=""
WRAPPED_CMD=()

usage() {
  cat <<'EOF'
Usage:
  sudo ./scripts/amw0/amw0-fan-rpm-audit.sh
  sudo ./scripts/amw0/amw0-fan-rpm-audit.sh --watch 20 --interval 0.5
  sudo ./scripts/amw0/amw0-fan-rpm-audit.sh --watch 30 --extra-ec
  sudo ./scripts/amw0/amw0-fan-rpm-audit.sh --watch 45 -- stress-ng --cpu 16 --timeout 45s

Options:
  --watch SEC      Re-sample ECMG candidates for SEC seconds.
  --interval SEC   Delay between samples when --watch is used. Default: 0.5
  --label TEXT     Label for the first ECMG sample. Default: snapshot
  --extra-ec       Also dump a small read-only EC correlation set.

Notes:
  - This script is read-only. It does not write AMW0, ECMG, or EC state.
  - The strongest current RPM candidates are:
      FFAN (0x460)
      F1SH/F1SL (0xE1C/0xE1D)
      F1DC/F2DC (0xE8C/0xE9D)
  - Use `-- COMMAND ...` to launch a load generator in sync with the watch
    window and avoid timing mistakes between terminals.
EOF
}

require_cmd() {
  command -v "$1" >/dev/null 2>&1 || {
    echo "Missing required command: $1" >&2
    exit 1
  }
}

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

fmt_byte() {
  printf '0x%02X' "$(($1 & 0xff))"
}

fmt_word() {
  printf '0x%04X' "$(($1 & 0xffff))"
}

cmd_to_string() {
  local out=""
  local arg

  for arg in "$@"; do
    if [[ -n "${out}" ]]; then
      out+=" "
    fi
    printf -v out '%s%q' "${out}" "${arg}"
  done
  printf '%s\n' "${out}"
}

cleanup() {
  local status=$?

  trap - EXIT INT TERM
  if [[ -n "${WRAPPED_PID}" ]] && kill -0 "${WRAPPED_PID}" 2>/dev/null; then
    kill "${WRAPPED_PID}" 2>/dev/null || true
    wait "${WRAPPED_PID}" 2>/dev/null || true
  fi
  exit "${status}"
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

dump_hwmon() {
  local node file
  local found_fan=0

  echo "== hwmon =="
  shopt -s nullglob
  for node in "${HWMON_ROOT}"/hwmon*; do
    local files=("${node}"/temp*_label "${node}"/temp*_input "${node}"/fan*_label
                 "${node}"/fan*_input)

    [[ -d "${node}" ]] || continue
    echo
    echo "## ${node}"
    if [[ -f "${node}/name" ]]; then
      printf 'name='
      cat "${node}/name"
    fi
    for file in "${files[@]}"; do
      [[ -f "${file}" ]] || continue
      printf '%s=' "$(basename "${file}")"
      cat "${file}"
      if [[ "$(basename "${file}")" == fan*_input ]]; then
        found_fan=1
      fi
    done
  done
  if [[ "${found_fan}" -eq 0 ]]; then
    echo
    echo "note=no hwmon fan*_input nodes were found"
  fi
}

dump_extra_ec() {
  local reg value
  local regs=(0x49 0x4c 0x64 0x65 0x6c 0x6d)

  command -v ec_probe >/dev/null 2>&1 || {
    echo "note=ec_probe not installed; skipping extra EC dump"
    return 0
  }

  echo
  echo "== ec-correlation =="
  echo "note=legacy correlation bytes only; not claimed as RPM registers"
  for reg in "${regs[@]}"; do
    value="$(ec_probe read "${reg}" | awk '{print $1}')"
    printf '%s=%s\n' "${reg}" "${value}"
  done
}

emit_ecmg_sample() {
  local sample_label="$1"
  local ffan raw460 raw466 raw468
  local cput pcht sn1t sn2t sn3t sn4t sn5t
  local f1sh f1sl f1_word_be f1_word_le
  local rawe8c rawe9d f1dc f1cm f2dc f2cm

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

  ffan=$((raw460 & 0x0f))
  f1_word_be=$(((f1sh << 8) | f1sl))
  f1_word_le=$(((f1sl << 8) | f1sh))
  f1dc=$((rawe8c & 0x7f))
  f1cm=$(((rawe8c >> 7) & 0x1))
  f2dc=$((rawe9d & 0x7f))
  f2cm=$(((rawe9d >> 7) & 0x1))

  printf '[%s] %s FFAN=%u RAW460=%s RAW466=%s RAW468=%s CPUT=%u PCHT=%u SN1T=%u SN2T=%u SN3T=%u SN4T=%u SN5T=%u F1SH=%u F1SL=%u F1_WORD_BE=%s F1_WORD_LE=%s F1DC=%u F1CM=%u F2DC=%u F2CM=%u RAWE8C=%s RAWE9D=%s\n' \
    "${sample_label}" "$(date +%T.%3N)" \
    "${ffan}" "$(fmt_byte "${raw460}")" "$(fmt_byte "${raw466}")" "$(fmt_byte "${raw468}")" \
    "${cput}" "${pcht}" "${sn1t}" "${sn2t}" "${sn3t}" "${sn4t}" "${sn5t}" \
    "${f1sh}" "${f1sl}" "$(fmt_word "${f1_word_be}")" "$(fmt_word "${f1_word_le}")" \
    "${f1dc}" "${f1cm}" "${f2dc}" "${f2cm}" \
    "$(fmt_byte "${rawe8c}")" "$(fmt_byte "${rawe9d}")"
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
    --extra-ec)
      EXTRA_EC="1"
      shift
      ;;
    --)
      shift
      WRAPPED_CMD=("$@")
      break
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

require_cmd awk
trap cleanup EXIT INT TERM

[[ -w "${CALL_NODE}" ]] || {
  echo "acpi_call not writable: ${CALL_NODE}" >&2
  exit 1
}

if [[ "${#WRAPPED_CMD[@]}" -gt 0 ]] && ! awk 'BEGIN { exit !('"${WATCH_SECONDS}"' > 0) }'; then
  echo "--watch must be greater than 0 when using -- COMMAND ..." >&2
  exit 2
fi

probe_ecrr_path

echo "== fan-rpm-audit =="
echo "timestamp=$(date -Is)"
echo "ecrr_path=${ECRR_PATH}"
echo "watch_seconds=${WATCH_SECONDS}"
echo "interval=${INTERVAL}"
echo "extra_ec=${EXTRA_EC}"
if [[ "${#WRAPPED_CMD[@]}" -gt 0 ]]; then
  echo "wrapped_command=$(cmd_to_string "${WRAPPED_CMD[@]}")"
fi
echo

dump_hwmon

echo
echo "== ecmg =="
echo "note=current strongest RPM candidates are FFAN, F1SH/F1SL, F1DC/F2DC"
emit_ecmg_sample "${LABEL}"

if [[ "${#WRAPPED_CMD[@]}" -gt 0 ]]; then
  "${WRAPPED_CMD[@]}" &
  WRAPPED_PID="$!"
  echo "wrapped_pid=${WRAPPED_PID}"
fi

if awk 'BEGIN { exit !('"${WATCH_SECONDS}"' > 0) }'; then
  steps="$(awk -v duration="${WATCH_SECONDS}" -v interval="${INTERVAL}" 'BEGIN { n = int((duration / interval) + 0.999); if (n < 1) n = 1; print n }')"
  for ((i = 1; i <= steps; i++)); do
    sleep "${INTERVAL}"
    emit_ecmg_sample "watch-${i}"
  done
fi

if [[ -n "${WRAPPED_PID}" ]]; then
  if wait "${WRAPPED_PID}"; then
    WRAPPED_STATUS="0"
  else
    WRAPPED_STATUS="$?"
  fi
  WRAPPED_PID=""
  echo
  echo "wrapped_exit_status=${WRAPPED_STATUS}"
fi

if [[ "${EXTRA_EC}" == "1" ]]; then
  dump_extra_ec
fi

#!/usr/bin/env bash
set -euo pipefail

# Audit AMW0/ECMG mode-related bytes while pressing the vendor mode button or
# while running a wrapped workload. This focuses on the current strongest
# candidates behind OEM mode switching:
# - 0x751: mode control bits (TBME / HIMODE / FANBOOST / UFME)
# - 0x7AB, 0x7B0..0x7B2: mode/profile index area
# - 0x7C7: helper bits (LCSE / OCPL)
# - 0x783..0x787: PL1 / PL2 / PL4 / TCC / fan switch speed
# - 0x460: FFAN vendor fan level
# - 0xF5D..0xF5F: fan-table tail bytes (status1 / status2 / control)

if [[ ${EUID:-$(id -u)} -ne 0 ]]; then
  echo "Run as root: sudo $0" >&2
  exit 1
fi

CALL_NODE="/proc/acpi/call"
LOG_DIR="${PWD}/amw0-logs"
TS="$(date +%Y%m%d-%H%M%S)"
LOG_FILE="${LOG_DIR}/mode-button-audit-${TS}.log"
WATCH_SECONDS="0"
INTERVAL="0.5"
LABEL="snapshot"
CHANGES_ONLY="0"

wrapped_cmd=()
wrapped_pid=""

mkdir -p "${LOG_DIR}"

[[ -w "${CALL_NODE}" ]] || {
  echo "acpi_call not writable: ${CALL_NODE}" >&2
  exit 1
}

usage() {
  cat <<'EOF'
Usage:
  sudo ./scripts/amw0/amw0-mode-button-audit.sh
  sudo ./scripts/amw0/amw0-mode-button-audit.sh --watch 20 --interval 0.2
  sudo ./scripts/amw0/amw0-mode-button-audit.sh --watch 20 --changes-only
  sudo ./scripts/amw0/amw0-mode-button-audit.sh --watch 45 -- stress-ng --cpu 16 --timeout 45s

Examples:
  1. Run with --watch and press the vendor mode button during the capture window.
  2. Wrap a workload and compare how 0x751 / 0x7C7 / 0x783..0x787 / 0xF5D..0xF5F react.

Options:
  --watch SEC        Re-sample for SEC seconds after the first sample.
  --interval SEC     Delay between samples. Default: 0.5
  --label TEXT       Label for the first sample. Default: snapshot
  --changes-only     Only print samples whose decoded payload changed.
  -h, --help         Show this help.
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
    --changes-only)
      CHANGES_ONLY="1"
      shift
      ;;
    --)
      shift
      wrapped_cmd=("$@")
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

ecrr_path=""
probe_ecrr_path() {
  local candidate raw

  for candidate in '\_SB.INOU.ECRR' '\_SB_.INOU.ECRR'; do
    raw="$(acpi_eval "${candidate} 0x751" || true)"
    if to_u32 "${raw}" >/dev/null 2>&1; then
      ecrr_path="${candidate}"
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

  raw="$(acpi_eval "${ecrr_path} 0x$(printf '%X' "$((offset))")")"
  to_u32 "${raw}"
}

fmt_byte() {
  printf '0x%02X' "$(( $1 & 0xff ))"
}

candidate_state_for_mode_ctl() {
  local mode_ctl="$1"

  if (((mode_ctl & 0x80) != 0 && (mode_ctl & 0x20) != 0)); then
    printf 'user-fan-hi-mode-like'
    return 0
  fi
  if (((mode_ctl & 0x80) != 0)); then
    printf 'custom-like'
    return 0
  fi
  if (((mode_ctl & 0x10) != 0 && (mode_ctl & 0x20) != 0)); then
    printf 'turbo-hi-mode-like'
    return 0
  fi
  if (((mode_ctl & 0x10) != 0)); then
    printf 'turbo-like'
    return 0
  fi
  if (((mode_ctl & 0x20) != 0)); then
    printf 'hi-mode-like'
    return 0
  fi

  printf 'non-turbo/non-custom'
}

sample_payload() {
  local mode_ctl mode_index profile1 profile2 profile3 helper
  local pl1 pl2 pl4 tcc fan_switch_speed ffan
  local tail1 tail2 tail_ctrl
  local tbme himode fanboost ufme lcse ocpl

  mode_ctl="$(read_ecrr 0x751)"
  mode_index="$(read_ecrr 0x7AB)"
  profile1="$(read_ecrr 0x7B0)"
  profile2="$(read_ecrr 0x7B1)"
  profile3="$(read_ecrr 0x7B2)"
  helper="$(read_ecrr 0x7C7)"
  pl1="$(read_ecrr 0x783)"
  pl2="$(read_ecrr 0x784)"
  pl4="$(read_ecrr 0x785)"
  tcc="$(read_ecrr 0x786)"
  fan_switch_speed="$(read_ecrr 0x787)"
  ffan="$(read_ecrr 0x460)"
  tail1="$(read_ecrr 0xF5D)"
  tail2="$(read_ecrr 0xF5E)"
  tail_ctrl="$(read_ecrr 0xF5F)"

  tbme=$(( (mode_ctl >> 4) & 1 ))
  himode=$(( (mode_ctl >> 5) & 1 ))
  fanboost=$(( (mode_ctl >> 6) & 1 ))
  ufme=$(( (mode_ctl >> 7) & 1 ))
  lcse=$(( (helper >> 1) & 1 ))
  ocpl=$(( (helper >> 2) & 7 ))

  printf 'MAFAN_CTL=%s TBME=%u HIMODE=%u FANBOOST=%u UFME=%u STATE=%s IDX=%s P1=%s P2=%s P3=%s HLP=%s LCSE=%u OCPL=%u PL1=%u PL2=%u PL4=%u TCC=%u FSW=%u FFAN=%u TAIL1=%s TAIL2=%s TAILCTL=%s' \
    "$(fmt_byte "${mode_ctl}")" \
    "${tbme}" "${himode}" "${fanboost}" "${ufme}" \
    "$(candidate_state_for_mode_ctl "${mode_ctl}")" \
    "$(fmt_byte "${mode_index}")" \
    "$(fmt_byte "${profile1}")" \
    "$(fmt_byte "${profile2}")" \
    "$(fmt_byte "${profile3}")" \
    "$(fmt_byte "${helper}")" \
    "${lcse}" "${ocpl}" \
    "${pl1}" "${pl2}" "${pl4}" "${tcc}" "${fan_switch_speed}" \
    "$((ffan & 0x0f))" \
    "$(fmt_byte "${tail1}")" \
    "$(fmt_byte "${tail2}")" \
    "$(fmt_byte "${tail_ctrl}")"
}

emit_sample() {
  local tag="$1"
  local payload
  local last_payload="${2:-}"

  payload="$(sample_payload)"
  if [[ "${CHANGES_ONLY}" == "1" && "${payload}" == "${last_payload}" ]]; then
    printf '%s' "${payload}"
    return 0
  fi

  printf '[%s] %s %s\n' "${tag}" "$(date +%T.%3N)" "${payload}" | tee -a "${LOG_FILE}"
  printf '%s' "${payload}"
}

launch_wrapped_command() {
  if (( ${#wrapped_cmd[@]} == 0 )); then
    return 0
  fi

  "${wrapped_cmd[@]}" &
  wrapped_pid="$!"
  printf 'wrapped_pid=%s\n' "${wrapped_pid}" | tee -a "${LOG_FILE}"
}

probe_ecrr_path

{
  echo "== mode-button-audit =="
  echo "timestamp=$(date --iso-8601=seconds)"
  echo "ecrr_path=${ecrr_path}"
  echo "watch_seconds=${WATCH_SECONDS}"
  echo "interval=${INTERVAL}"
  echo "changes_only=${CHANGES_ONLY}"
  if (( ${#wrapped_cmd[@]} > 0 )); then
    printf 'wrapped_command='
    printf '%q ' "${wrapped_cmd[@]}"
    printf '\n'
  fi
  echo
  echo "note=watch 0x751/0x7AB/0x7B0..0x7B2/0x7C7/0x783..0x787/0x460/0xF5D..0xF5F while pressing the vendor mode button"
} | tee -a "${LOG_FILE}"

last_payload=""
last_payload="$(emit_sample "${LABEL}" "${last_payload}")"
echo | tee -a "${LOG_FILE}" >/dev/null

launch_wrapped_command

steps="0"
if awk 'BEGIN { exit !('"${WATCH_SECONDS}"' > 0) }'; then
  steps="$(awk -v duration="${WATCH_SECONDS}" -v interval="${INTERVAL}" 'BEGIN { n = int((duration / interval) + 0.999); if (n < 1) n = 1; print n }')"
fi

i=0
while :; do
  if (( steps > 0 && i >= steps )); then
    break
  fi
  if (( steps == 0 && ${#wrapped_cmd[@]} == 0 )); then
    break
  fi
  if (( steps == 0 )) && [[ -n "${wrapped_pid}" ]] && ! kill -0 "${wrapped_pid}" 2>/dev/null; then
    break
  fi

  sleep "${INTERVAL}"
  ((i += 1))
  last_payload="$(emit_sample "watch-${i}" "${last_payload}")"
done

if [[ -n "${wrapped_pid}" ]]; then
  if wait "${wrapped_pid}"; then
    wrapped_status=0
  else
    wrapped_status=$?
  fi
  printf '\nwrapped_exit_status=%s\n' "${wrapped_status}" | tee -a "${LOG_FILE}"
fi

printf '\nDone. Review %s\n' "${LOG_FILE}" | tee -a "${LOG_FILE}"

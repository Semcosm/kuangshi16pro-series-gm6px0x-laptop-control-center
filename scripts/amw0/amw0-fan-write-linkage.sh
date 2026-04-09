#!/usr/bin/env bash
set -euo pipefail

if [[ ${EUID:-$(id -u)} -ne 0 ]]; then
  echo "Run as root: sudo $0" >&2
  exit 1
fi

CALL_NODE="/proc/acpi/call"
LOG_DIR="${PWD}/amw0-logs"
TS="$(date +%Y%m%d-%H%M%S)"
LOG_FILE="${LOG_DIR}/fan-write-linkage-${TS}.log"
INTERVAL="0.25"
WATCH_SECONDS="2"
PROBE=""
RESTORE="0"
EXPLICIT_VALUE=""
TRANSPORT="ecrw"
TABLE_FILE=""

offsets=()
labels=()
original_values=()
target_values=()

mkdir -p "${LOG_DIR}"

usage() {
  cat <<'EOF'
Usage:
  sudo ./scripts/amw0/amw0-fan-write-linkage.sh --probe cpu-up-0
  sudo ./scripts/amw0/amw0-fan-write-linkage.sh --probe cpu-point-0
  sudo ./scripts/amw0/amw0-fan-write-linkage.sh --probe tail-all
  sudo ./scripts/amw0/amw0-fan-write-linkage.sh --probe cpu-point-0 --transport wkbc1
  sudo ./scripts/amw0/amw0-fan-write-linkage.sh --probe cpu-up-0 --value 46 --restore
  sudo ./scripts/amw0/amw0-fan-write-linkage.sh --probe full-plan --transport wkbc1 --table-file kuangshi16pro-lcc/data/fan-tables/fan-fullspeed.json

Options:
  --probe NAME      Named minimal write experiment. Required.
  --value N         Override target byte for single-byte probes only.
                    Default: same-value write using the current byte.
  --watch SEC       Watch SEC seconds after the write. Default: 2
  --interval SEC    Sample interval while watching. Default: 0.25
  --transport NAME  Write path: ecrw, wkbc0, or wkbc1. Default: ecrw
  --table-file PATH Use plan values from a fan-table file instead of current EC
                    bytes. Best with full-no-tail or full-plan probes.
  --restore         Restore original bytes after the watch window.
  -h, --help        Show this help.

Probes:
  cpu-up-0          0xF00
  cpu-down-0        0xF10
  cpu-duty-0        0xF20
  cpu-point-0       0xF00, 0xF10, 0xF20
  cpu-all           full CPU fan-table region
  gpu-up-0          0xF30
  gpu-down-0        0xF40
  gpu-duty-0        0xF50
  gpu-point-0       0xF30, 0xF40, 0xF50
  gpu-all           full GPU fan-table region used by lccctl
  tail1             0xF5D
  tail2             0xF5E
  tailctl           0xF5F
  tail-all          0xF5D, 0xF5E, 0xF5F
  full-no-tail      CPU + GPU table region, no tail bytes
  full-plan         CPU + GPU region + tail bytes

Notes:
  - This is an experimental direct ECRW tool for reverse engineering.
  - Default behavior is safer: same-value writes only.
  - The goal is to isolate whether touching specific fan-table bytes causes
    0x751 mode-control changes such as 0x30 -> 0xA0.
  - `wkbc1` matches the current lccctl AMW0 backend route.
EOF
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

cmd_note() {
  printf '%s\n' "$*" | tee -a "${LOG_FILE}"
}

find_lccctl() {
  if command -v lccctl >/dev/null 2>&1; then
    command -v lccctl
    return 0
  fi
  if [[ -x "./kuangshi16pro-lcc/build/lccctl" ]]; then
    printf '%s\n' "./kuangshi16pro-lcc/build/lccctl"
    return 0
  fi

  return 1
}

ECRR_PATH=""
ECRW_PATH=""
probe_ecmg_paths() {
  local stem raw

  for stem in '\_SB.INOU' '\_SB_.INOU'; do
    raw="$(acpi_eval "${stem}.ECRR 0x751" || true)"
    if to_u32 "${raw}" >/dev/null 2>&1; then
      ECRR_PATH="${stem}.ECRR"
      ECRW_PATH="${stem}.ECRW"
      return 0
    fi
  done

  echo "Could not probe ECRR/ECRW through acpi_call" >&2
  echo "Last reply: ${raw:-<empty>}" >&2
  return 1
}

read_ecrr() {
  local offset="$1"
  local raw

  raw="$(acpi_eval "${ECRR_PATH} 0x$(printf '%X' "$((offset))")")"
  to_u32 "${raw}"
}

write_ecrw() {
  local offset="$1"
  local value="$2"

  acpi_eval "${ECRW_PATH} 0x$(printf '%X' "$((offset))") 0x$(printf '%X' "$((value & 0xff))")" >/dev/null
}

write_wkbc() {
  local route="$1"
  local offset="$2"
  local value="$3"
  local payload=""

  payload="$(
    printf '{0x%02X,0x%02X,0x%02X,0x00,0x%02X,0x%02X,0x00,0x00,0x02,0x02,0x02,0x02,0x03,0x03,0x03,0x03,0x04,0x04,0x04,0x04,0x05,0x05,0x05,0x05,0x06,0x06,0x06,0x06,0x07,0x07,0x07,0x07,0x08,0x08,0x08,0x08,0x09,0x09,0x09,0x09}' \
      "$((offset & 0xff))" "$(((offset >> 8) & 0xff))" "$((value & 0xff))" \
      "$((route & 0xff))" "$(((route >> 8) & 0xff))"
  )"

  acpi_eval "\\_SB.AMW0.WMBC 0x0 0x4 ${payload}" >/dev/null
}

write_byte() {
  local offset="$1"
  local value="$2"

  case "${TRANSPORT}" in
    ecrw)
      write_ecrw "${offset}" "${value}"
      ;;
    wkbc0)
      write_wkbc 0x0000 "${offset}" "${value}"
      ;;
    wkbc1)
      write_wkbc 0x0001 "${offset}" "${value}"
      ;;
    *)
      echo "Unsupported transport: ${TRANSPORT}" >&2
      exit 2
      ;;
  esac
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

  payload="$(sample_payload)"
  printf '[%s] %s %s\n' "${tag}" "$(date +%T.%3N)" "${payload}" | tee -a "${LOG_FILE}"
}

set_probe() {
  local index=0

  append_range() {
    local base="$1"
    local count="$2"
    local prefix="$3"

    for ((index = 0; index < count; index++)); do
      offsets+=($((base + index)))
      labels+=("${prefix}${index}")
    done
  }

  case "${PROBE}" in
    cpu-up-0)
      offsets=(0xF00)
      labels=("CPU_UP0")
      ;;
    cpu-down-0)
      offsets=(0xF10)
      labels=("CPU_DOWN0")
      ;;
    cpu-duty-0)
      offsets=(0xF20)
      labels=("CPU_DUTY0")
      ;;
    cpu-point-0)
      offsets=(0xF00 0xF10 0xF20)
      labels=("CPU_UP0" "CPU_DOWN0" "CPU_DUTY0")
      ;;
    cpu-all)
      append_range 0xF00 16 "CPU_UP"
      append_range 0xF10 16 "CPU_DOWN"
      append_range 0xF20 16 "CPU_DUTY"
      ;;
    gpu-up-0)
      offsets=(0xF30)
      labels=("GPU_UP0")
      ;;
    gpu-down-0)
      offsets=(0xF40)
      labels=("GPU_DOWN0")
      ;;
    gpu-duty-0)
      offsets=(0xF50)
      labels=("GPU_DUTY0")
      ;;
    gpu-point-0)
      offsets=(0xF30 0xF40 0xF50)
      labels=("GPU_UP0" "GPU_DOWN0" "GPU_DUTY0")
      ;;
    gpu-all)
      append_range 0xF30 16 "GPU_UP"
      append_range 0xF40 16 "GPU_DOWN"
      append_range 0xF50 13 "GPU_DUTY"
      ;;
    tail1)
      offsets=(0xF5D)
      labels=("TAIL1")
      ;;
    tail2)
      offsets=(0xF5E)
      labels=("TAIL2")
      ;;
    tailctl)
      offsets=(0xF5F)
      labels=("TAILCTL")
      ;;
    tail-all)
      offsets=(0xF5D 0xF5E 0xF5F)
      labels=("TAIL1" "TAIL2" "TAILCTL")
      ;;
    full-no-tail)
      append_range 0xF00 16 "CPU_UP"
      append_range 0xF10 16 "CPU_DOWN"
      append_range 0xF20 16 "CPU_DUTY"
      append_range 0xF30 16 "GPU_UP"
      append_range 0xF40 16 "GPU_DOWN"
      append_range 0xF50 13 "GPU_DUTY"
      ;;
    full-plan)
      append_range 0xF00 16 "CPU_UP"
      append_range 0xF10 16 "CPU_DOWN"
      append_range 0xF20 16 "CPU_DUTY"
      append_range 0xF30 16 "GPU_UP"
      append_range 0xF40 16 "GPU_DOWN"
      append_range 0xF50 13 "GPU_DUTY"
      offsets+=(0xF5D 0xF5E 0xF5F)
      labels+=("TAIL1" "TAIL2" "TAILCTL")
      ;;
    *)
      echo "Unsupported probe: ${PROBE}" >&2
      exit 2
      ;;
  esac
}

load_targets_from_plan() {
  local lccctl_bin=""
  local plan=""
  local line=""
  local addr_hex=""
  local value_hex=""
  local offset_hex=""
  local i=0

  lccctl_bin="$(find_lccctl)" || {
    echo "Could not find lccctl for --table-file plan extraction" >&2
    exit 1
  }

  plan="$("$lccctl_bin" fan apply --plan --file "$TABLE_FILE")"
  while IFS= read -r line; do
    [[ "$line" == *"ec-write"* ]] || continue
    addr_hex="$(printf '%s\n' "$line" | sed -n 's/.*addr=\(0x[0-9A-Fa-f]\{4\}\).*/\1/p')"
    value_hex="$(printf '%s\n' "$line" | sed -n 's/.*value=\(0x[0-9A-Fa-f]\{2\}\).*/\1/p')"
    [[ -n "$addr_hex" && -n "$value_hex" ]] || continue
    for i in "${!offsets[@]}"; do
      printf -v offset_hex '0x%04X' "$((offsets[i]))"
      if [[ "$addr_hex" == "$offset_hex" ]]; then
        target_values[i]="$((value_hex))"
        break
      fi
    done
  done <<<"$plan"
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --probe)
      PROBE="${2:?missing probe name}"
      shift 2
      ;;
    --value)
      EXPLICIT_VALUE="${2:?missing byte value}"
      shift 2
      ;;
    --watch)
      WATCH_SECONDS="${2:?missing watch seconds}"
      shift 2
      ;;
    --interval)
      INTERVAL="${2:?missing interval}"
      shift 2
      ;;
    --transport)
      TRANSPORT="${2:?missing transport name}"
      shift 2
      ;;
    --table-file)
      TABLE_FILE="${2:?missing table file path}"
      shift 2
      ;;
    --restore)
      RESTORE="1"
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

[[ -n "${PROBE}" ]] || {
  usage >&2
  exit 2
}
case "${TRANSPORT}" in
  ecrw|wkbc0|wkbc1)
    ;;
  *)
    echo "--transport must be ecrw, wkbc0, or wkbc1" >&2
    exit 2
    ;;
esac
[[ -w "${CALL_NODE}" ]] || {
  echo "acpi_call not writable: ${CALL_NODE}" >&2
  exit 1
}

set_probe
probe_ecmg_paths

if [[ -n "${EXPLICIT_VALUE}" ]]; then
  [[ "${#offsets[@]}" -eq 1 ]] || {
    echo "--value only works with single-byte probes" >&2
    exit 2
  }
  [[ "${EXPLICIT_VALUE}" =~ ^0x[0-9A-Fa-f]+$ || "${EXPLICIT_VALUE}" =~ ^[0-9]+$ ]] || {
    echo "--value must be a byte literal" >&2
    exit 2
  }
fi
if [[ -n "${TABLE_FILE}" && -n "${EXPLICIT_VALUE}" ]]; then
  echo "use either --value or --table-file, not both" >&2
  exit 2
fi
if [[ -n "${TABLE_FILE}" && ! -f "${TABLE_FILE}" ]]; then
  echo "table file not found: ${TABLE_FILE}" >&2
  exit 1
fi

for i in "${!offsets[@]}"; do
  original_values[i]="$(read_ecrr "${offsets[i]}")"
  if [[ -n "${EXPLICIT_VALUE}" ]]; then
    target_values[i]="$((EXPLICIT_VALUE))"
  else
    target_values[i]="${original_values[i]}"
  fi
done

if [[ -n "${TABLE_FILE}" ]]; then
  load_targets_from_plan
fi

{
  echo "== fan-write-linkage =="
  echo "timestamp=$(date -Is)"
  echo "ecrr_path=${ECRR_PATH}"
  echo "ecrw_path=${ECRW_PATH}"
  echo "probe=${PROBE}"
  echo "transport=${TRANSPORT}"
  if [[ -n "${TABLE_FILE}" ]]; then
    echo "table_file=${TABLE_FILE}"
  fi
  echo "watch_seconds=${WATCH_SECONDS}"
  echo "interval=${INTERVAL}"
  echo "restore=${RESTORE}"
  echo
  for i in "${!offsets[@]}"; do
    printf 'write[%u]=%s addr=0x%04X original=%s target=%s\n' \
      "$i" "${labels[i]}" "$((offsets[i]))" \
      "$(fmt_byte "${original_values[i]}")" \
      "$(fmt_byte "${target_values[i]}")"
  done
  echo
} | tee -a "${LOG_FILE}"

emit_sample "before"
echo | tee -a "${LOG_FILE}" >/dev/null

for i in "${!offsets[@]}"; do
  write_byte "${offsets[i]}" "${target_values[i]}"
done

emit_sample "after-write"

steps="0"
if awk 'BEGIN { exit !('"${WATCH_SECONDS}"' > 0) }'; then
  steps="$(awk -v duration="${WATCH_SECONDS}" -v interval="${INTERVAL}" 'BEGIN { n = int((duration / interval) + 0.999); if (n < 1) n = 1; print n }')"
fi

for ((i = 1; i <= steps; i++)); do
  sleep "${INTERVAL}"
  emit_sample "watch-${i}"
done

if [[ "${RESTORE}" == "1" ]]; then
  echo | tee -a "${LOG_FILE}" >/dev/null
  for i in "${!offsets[@]}"; do
    write_byte "${offsets[i]}" "${original_values[i]}"
  done
  emit_sample "after-restore"
fi

printf '\nDone. Review %s\n' "${LOG_FILE}" | tee -a "${LOG_FILE}"

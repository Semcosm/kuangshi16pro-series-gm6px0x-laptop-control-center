#!/usr/bin/env bash
set -euo pipefail

# Build and optionally send a WMBC buffer through acpi_call.
#
# AC00 layout from AML:
# - bytes 0..3  = SA00..SA03, passed to WKBC / RKBC / SCMD
# - bytes 4..7  = SAC1 route dword
#
# Route values:
# - 0x0000 / 0x0001 -> WKBC(SA00, SA01, SA02, SA03)
# - 0x0100          -> RKBC(SA00, SA01)
# - 0x0200          -> SCMD(SA00)

if [[ ${EUID:-$(id -u)} -ne 0 ]]; then
  echo "Run as root: sudo $0" >&2
  exit 1
fi

CALL_NODE="/proc/acpi/call"

usage() {
  cat <<'EOF'
Usage:
  sudo ./scripts/amw0/amw0-wmbc-pack.sh local  SLOT SA00 [SA01 [SA02 [SA03]]]
  sudo ./scripts/amw0/amw0-wmbc-pack.sh notify SLOT EVENT
  sudo ./scripts/amw0/amw0-wmbc-pack.sh send   SLOT SAC1 [SA00 [SA01 [SA02 [SA03]]]]

Modes:
  local   Use WMBC Arg1=0x2. Writes only AC00[idx=slot].
  notify  Use WMBC Arg1=0x3. Updates SAC1 and emits D2 event.
  send    Use WMBC Arg1=0x4. Executes OEMG and may touch hardware.

Examples:
  sudo ./scripts/amw0/amw0-wmbc-pack.sh local 0x0 0x49 0x00 0x1E 0x00
  sudo ./scripts/amw0/amw0-wmbc-pack.sh notify 0x0 0x0001
  sudo ./scripts/amw0/amw0-wmbc-pack.sh send   0x0 0x0001 0x49 0x00 0x1E 0x00
EOF
}

hexnorm() {
  local v="${1:-0}"
  printf '%u' "$((v))"
}

mk_buffer() {
  local sac1 sa00 sa01 sa02 sa03
  sac1="$(hexnorm "${1:-0}")"
  sa00="$(hexnorm "${2:-0}")"
  sa01="$(hexnorm "${3:-0}")"
  sa02="$(hexnorm "${4:-0}")"
  sa03="$(hexnorm "${5:-0}")"

  local s0 s1 s2 s3
  s0=$(( sac1        & 0xff ))
  s1=$(( (sac1 >> 8) & 0xff ))
  s2=$(( (sac1 >>16) & 0xff ))
  s3=$(( (sac1 >>24) & 0xff ))

  printf '{0x%02X,0x%02X,0x%02X,0x%02X,0x%02X,0x%02X,0x%02X,0x%02X,0x02,0x02,0x02,0x02,0x03,0x03,0x03,0x03,0x04,0x04,0x04,0x04,0x05,0x05,0x05,0x05,0x06,0x06,0x06,0x06,0x07,0x07,0x07,0x07,0x08,0x08,0x08,0x08,0x09,0x09,0x09,0x09}' \
    "$sa00" "$sa01" "$sa02" "$sa03" "$s0" "$s1" "$s2" "$s3"
}

acpi_eval() {
  local expr="$1"
  printf '%s\n' "${expr}" > "${CALL_NODE}"
  tr -d '\000' < "${CALL_NODE}"
}

[[ -w "${CALL_NODE}" ]] || { echo "acpi_call not writable: ${CALL_NODE}" >&2; exit 1; }
[[ $# -ge 3 ]] || { usage; exit 2; }

mode="$1"
shift
slot="$1"
shift
sac1="$1"
shift || true

sa00="${1:-0}"
sa01="${2:-0}"
sa02="${3:-0}"
sa03="${4:-0}"

case "${mode}" in
  local)
    arg1="0x2"
    sa00="$((sac1))"
    sa01="$(( ${1:-0} ))"
    sa02="$(( ${2:-0} ))"
    sa03="$(( ${3:-0} ))"
    sac1="0"
    ;;
  notify)
    arg1="0x3"
    sa00="0"
    sa01="0"
    sa02="0"
    sa03="0"
    ;;
  send)
    arg1="0x4"
    ;;
  *)
    usage
    exit 2
    ;;
esac

buf="$(mk_buffer "${sac1}" "${sa00}" "${sa01}" "${sa02}" "${sa03}")"

expr="\\_SB.AMW0.WMBC 0x$(printf '%X' "$((slot))") ${arg1} ${buf}"
echo "Expression:"
echo "${expr}"
echo
acpi_eval "${expr}"

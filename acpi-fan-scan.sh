#!/usr/bin/env bash
set -euo pipefail

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

require_cmd iasl
require_cmd grep
require_cmd sed

OUT_DIR="${PWD}/acpi-scan"
TS="$(date +%Y%m%d-%H%M%S)"
WORK_DIR="${OUT_DIR}/${TS}"
mkdir -p "${WORK_DIR}"

RAW_AML="${WORK_DIR}/dsdt.aml"
DSL_LOG="${WORK_DIR}/iasl.log"
REPORT="${WORK_DIR}/report.txt"
DSL_FILE="${WORK_DIR}/dsdt.dsl"

echo "Saving ACPI tables into ${WORK_DIR}"
cp /sys/firmware/acpi/tables/DSDT "${RAW_AML}"

pushd "${WORK_DIR}" >/dev/null
iasl -d dsdt.aml > "${DSL_LOG}" 2>&1 || true
popd >/dev/null

{
  echo "ACPI scan directory: ${WORK_DIR}"
  echo
  echo "Generated files:"
  ls -1 "${WORK_DIR}"
  echo
} > "${REPORT}"

append_matches() {
  local title="$1"
  local pattern="$2"
  {
    echo "=== ${title} ==="
    grep -niE "${pattern}" "${DSL_FILE}" || true
    echo
  } >> "${REPORT}"
}

append_matches "Fan keywords" 'fan|FAN|cpu fan|gpu fan'
append_matches "Control keywords" 'manual|auto|override|boost|pwm|duty|tach|rpm'
append_matches "EC field names" 'Field.*EC|OperationRegion.*EmbeddedControl|OperationRegion.*EC'
append_matches "Suspicious methods" 'Method .*([SG][A-Z0-9]{3}|F[A-Z0-9]{3}|WM[0-9A-Z]{2}|RM[0-9A-Z]{2})'
append_matches "Candidate registers near discovered values" '0x40|0x42|0x48|0x49|0x4B|0x4C|0x64|0x65|0x6C|0x6D'

echo "Report written to ${REPORT}"
echo "Open these if needed:"
echo "  ${REPORT}"
echo "  ${DSL_LOG}"

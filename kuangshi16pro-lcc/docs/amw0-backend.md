# AMW0 Backend Notes

This file records only the low-level facts that already matter for a Linux backend.

## Proven Transport Facts

- `AMW0` is a generic ACPI WMI transport device.
- `WMBC(..., 0x04, buffer)` is the useful send path.
- `AC00` layout is:
  - bytes `0..3` = `SA00..SA03`
  - bytes `4..7` = `SAC1`

## Route Selectors

- `SAC1 = 0x0000` or `0x0001`
  routes to `WKBC(SA00, SA01, SA02, SA03)`
- `SAC1 = 0x0100`
  routes to `RKBC(SA00, SA01)`
- `SAC1 = 0x0200`
  routes to `SCMD(SA00)`

## Proven Byte Mapping

Corrected traces already proved:

- `LDAT = SA00`
- `HDAT = SA01`
- `CMDL = SA02`
- `CMDH = SA03`

This is the byte order the Linux backend must preserve.

## Timing / Observation Facts

- corrected `WKBC` and `SCMD` sends take about `77-78 ms`
- `LDAT` / `HDAT` / `CMDL` / `CMDH` remain latched long enough to inspect after send
- `ECRR` is a cleaner Linux-side read path for `ECMG` than `/dev/mem`

## Remaining Risks

- `WKBC` / `RKBC` / `SCMD` are gated by `ECON`
- the current Linux setup may still be missing the firmware-side gate setup expected by AML
- fan-control state may live partly in `ECMG`, not just in plain EC byte space
- the Windows `GCUService` fan-table path is not yet proven to use `AMW0`
  `SetEcFanTable_*` may instead resolve to direct EC/ECMG writes through
  `ACPIDriverDll.dll` or `WMIWriteECRAM`

## Important Boundary

- `AMW0` currently gives us a proven Linux transport surface.
- `GCUService` currently gives us a proven Windows data model and EC address map.
- Those are not yet the same proof.
  The remaining reverse-engineering task is to connect the Windows-side
  `0x751` / `0x7AB` / `0xF00..0xF5F` writes to either:
  - direct EC/ECMG access, or
  - a reproducible `WKBC` / `SCMD` packet family on this machine.

## Relevant Scripts

- `amw0-wmbc-pack.sh`
- `amw0-wmbc-trace.sh`
- `amw0-ecmg-read.sh`
- `amw0-wkbc-scan.sh`
- `amw0-scmd-scan.sh`

These scripts are the practical reference for the first Linux backend implementation.

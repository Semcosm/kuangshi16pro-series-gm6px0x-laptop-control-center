# Kuangshi16Pro Series GM6PX0X Laptop Control Center

Linux reverse-engineering workspace for the Mechrevo Kuangshi16Pro Series `GM6PX0X`.

This repository focuses on three things:

- reverse engineering the ACPI/WMI transport exposed by `AMW0`
- identifying the real dual-fan control path used by the vendor control center
- building a Linux-side control-center-grade toolchain for this machine

## Current Scope

- `AMW0` transport facts and EC/ECMG observations are documented in:
  - `CURRENT-FINDINGS.md`
  - `AMW0-analysis.md`
- Windows package reverse engineering is documented in:
  - `windows-control-center-analysis.md`
  - `windows-control-center/`
- legacy NBFC-compatible experiments are kept only as historical reference in:
  - `legacy-nbfc-configs/`

## Top-Level Naming

The top-level tools are grouped by transport or source:

- `amw0-*`
  AMW0 WMI transport experiments, traces, scans, and payload helpers
- `ec-*`
  direct EC observation helpers
- `acpi-*`
  ACPI table collection helpers
- `windows-control-center/*`
  vendor Windows package artifacts and capture helpers

This naming is intentional and currently consistent enough for a Git repository.
The only old `nbfc` naming was moved into `legacy-nbfc-configs/`.

## Useful Entry Points

- `CURRENT-FINDINGS.md`
  short list of findings that are strong enough to rely on
- `AMW0-analysis.md`
  deeper AMW0, ECXP, and ECMG notes
- `amw0-ecmg-read.sh`
  read AML `ECMG` bytes through `\_SB.INOU.ECRR`
- `amw0-wmbc-trace.sh`
  trace a single `WMBC(..., 0x04, ...)` send
- `windows-control-center/windows-cc-capture.ps1`
  no-install Windows-side capture helper

## Notes

- `amw0-logs/` and `ec-probe-logs/` are runtime artifact directories.
- unpacked Windows package trees are intentionally not required for a clean repo checkout.
- the repository name uses Git-friendly ASCII; the full machine name remains in this README.

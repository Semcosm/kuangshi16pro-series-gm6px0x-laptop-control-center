# Source Map

This file maps the existing research into the Linux implementation work.

## Highest-Value Inputs

- `windows-control-center/GCUService_reverse_report.md`
  Best current source for the service-layer model, EC constants, and feature surface.
- `CURRENT-FINDINGS.md`
  Best current source for proven AMW0 / WKBC / SCMD transport facts.
- `AMW0-analysis.md`
  Deeper AML and EC/ECMG notes when `CURRENT-FINDINGS.md` is not enough.
- `windows-control-center-analysis.md`
  Package layout, Windows fan-table format, and message-path clues.

## Scripts To Reuse

- `amw0-wmbc-pack.sh`
  Reference for packing Linux-side `WMBC(..., 0x04, buffer)` payloads.
- `amw0-wmbc-trace.sh`
  Reference for tracing a single send and inspecting EC-side transient bytes.
- `amw0-ecmg-read.sh`
  Reference for reading `ECMG` through `\_SB.INOU.ECRR`.
- `amw0-wkbc-scan.sh`
  Reference for brute-force style command-path experiments.
- `amw0-scmd-scan.sh`
  Reference for `SCMD`-side experiments.

## Windows Reverse Inputs

- `windows-control-center/GCUService_reverse_report.md`
  Primary Windows-side reverse target for Linux parity.
- `windows-control-center/GCUService-interesting-strings.txt`
  Fast string index for names, topics, and command symbols.
- `windows-control-center/GCUService.exe`
  Assembly to keep decompiling with `ilspycmd`.

## Lower Priority Inputs

- `windows-control-center/GCUBridge_reverse_report.md`
  Useful only if the final hardware write path turns out to leave `GCUService`.
- `WQBA-analysis.md`
  Useful background; not a direct control path.
- `legacy-nbfc-configs/`
  Historical only; not a design source for the new control center.

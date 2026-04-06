# Linux Control Center Roadmap

This roadmap keeps the Linux implementation aligned with the `GCUService` reverse engineering work.

## Current Status Check

- PR1 completed
- PR2 completed
- PR3 completed, with the current minimal stable surface limited to `Manager`,
  `Fan`, and `Power`; there is no standalone `Thermal` XML in the stable API
- PR4 completed
- PR5 completed
- PR6 completed
- PR7 completed
- PR8 completed, with product commands fully clientized over D-Bus while
  developer commands still keep a direct path
- PR9 completed in repository code and workflow definitions; GitHub-side branch
  protection and required-check configuration still need final manual
  confirmation in the repository settings

## Main Goal

Reproduce the `GCUService.exe` service layer on Linux:

- operating mode
- profile selection
- dual-fan table programming
- `PL1` / `PL2` / `PL4`
- `TccOffset`
- status reads

UI comes later.

## Reverse Priorities

### First

- `SetOperatingModeProfileIndexThread`
- `SetModeSwitchChangeThread`
- `SetFanTableThread`
- `SetEcFanTable_Cpu`
- `SetEcFanTable_Gpu`

Goal:

- recover input shape
- recover mode-to-profile mapping
- recover the last write step to ACPI / WMI / EC

### Second

Clarify backend selection inside `GCUService`:

- ACPI driver only
- WMI only
- mixed read/write path

Linux should prefer the already-proven `AMW0` route before chasing Windows driver parity.

### Third

Map this machine to the vendor fan-table family and confirm:

- default office/gaming/turbo tables
- custom fan-curve persistence format
- dGPU direct connection behavior and restart requirements

## Linux Build Order

1. `backends/amw0`
   Raw packet packing and send/read helpers.
2. `core/profile` + `core/fan` + `core/power`
   `MainOption`, `ModeProfile`, fan table, and power-limit orchestration.
3. `state-reader`
   temps, fan RPM, mode, and support/status bits.
4. `ui`
   minimal panel after the backend works.

## Immediate Next Steps

1. keep decompiling `GCUService.exe`
2. export method-level call notes for the hot paths above
3. translate the Windows model into Linux-side config structs
4. implement a minimal Linux command set:
   - set mode
   - write fan table
   - set `PL1/PL2/PL4`
   - read status

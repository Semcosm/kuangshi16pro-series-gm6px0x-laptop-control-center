# Linux Control Center Roadmap

This roadmap keeps the Linux implementation aligned with the `GCUService`
reverse engineering work while tracking the current Linux product path.

## Current Baseline

- `lccd` is the privileged daemon and the system bus is the product control surface
- stable D-Bus v1 is intentionally limited to `Manager`, `Fan`, and `Power`
- product-facing `lccctl` commands are pure D-Bus clients; direct AMW0 access
  remains a developer path only
- systemd, D-Bus activation, system-bus config, and Polkit policy assets are
  already committed in-tree
- hardware validation already targets the installed system-bus daemon rather
  than ad hoc direct-write probes

## Main Goal

Reproduce the `GCUService.exe` service layer on Linux:

- operating mode
- profile selection
- dual-fan table programming
- `PL1` / `PL2` / `PL4`
- `TccOffset`
- status reads

UI comes later.

## Recently Completed

1. `PR13A`
   completed finishing/core: asserted contracts, pinned route attribution, and
   converged the deployment surface across docs, source, and checked-in system
   assets
2. `PR13B`
   completed build/install: productized install, uninstall, staging, and
   install-smoke for the already-existing deployment assets

## Current Follow-Up

- keep hardware validation centered on `sudo make -C kuangshi16pro-lcc install`
  plus `systemctl daemon-reload` and `enable --now`
- run and record the real-machine matrix against the installed system-bus
  daemon, not ad hoc build-tree paths
- keep extending product-facing service behavior and validation around the
  installed daemon path while reverse work continues
- treat `PL1` / `PL2` and `TccOffset` as the currently hardware-proven power
  controls on this machine; keep `PL4` in the product surface, but do not claim
  Intel-telemetry equivalence until it is independently validated
- prioritize the fan chain next: stronger RPM discovery, write-path validation,
  and clearer `effective_meta` semantics around fan-table state

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

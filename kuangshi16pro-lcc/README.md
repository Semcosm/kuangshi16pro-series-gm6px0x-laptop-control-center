# Kuangshi16Pro LCC

This directory contains the Linux-native implementation scaffold for the laptop
control center.

Layout:

- `src/daemon/`: future `lccd` system service and transaction coordinator
- `src/dbus/`: D-Bus server and introspection XML
- `src/core/`: backend-agnostic profile, fan, power, state, and capability logic
- `src/backends/`: standard Linux ABI backends first, AMW0 fallback second
- `src/cli/`: operator-facing CLI, intended to become a D-Bus client
- `include/lcc/`: public project headers
- `docs/`: architecture notes and reverse-guided implementation docs
- `data/`: imported OEM presets, fan tables, and model capability maps
- `tests/fixtures/`: current INI fixtures used by unit tests
- `systemd/` and `dbus/`: service and bus policy skeletons

Current focus:

- keep the current CLI and tests building while the tree moves toward a
  daemon + D-Bus architecture
- keep standard Linux ABI support as the preferred backend direction
- isolate `AMW0` / `INOU` as a vendor-specific fallback path

Start with:

- `docs/README.md`
- `docs/architecture.md`
- `docs/profiles.md`
- `docs/backend-amw0.md`

Build:

- `make -C kuangshi16pro-lcc`
- `make -C kuangshi16pro-lcc test`
- `./kuangshi16pro-lcc/build/lccctl --help`
- `./kuangshi16pro-lcc/build/lccd --help`

Current CLI scope:

- `status`: read `WQAC`, `_WED(D2)`, and probe `ECRR`
- `observe`: grouped reads for `mode`, `power`, `fan`, `thermal`, or `all`
  mode and thermal groups also print a decoded summary for the currently observed bytes
- `raw wmbc`: send a traced `WMBC(..., 0x04, buffer)` packet
- `mode set`, `power set`, `fan apply`, `profile apply`: build and print a staged EC plan

Config-driven examples:

- `./kuangshi16pro-lcc/build/lccctl fan apply --file kuangshi16pro-lcc/tests/fixtures/demo-fan.ini`
- `./kuangshi16pro-lcc/build/lccctl profile apply --file kuangshi16pro-lcc/tests/fixtures/demo-profile.ini`
- `./kuangshi16pro-lcc/build/lccctl observe mode`
- `./kuangshi16pro-lcc/build/lccctl observe all`

Current phase:

- `src/core/` already owns the staged profile/fan/power planning logic
- `src/backends/amw0/` now owns transport, packet formatting, EC address maps,
  and decode helpers
- `src/cli/` has been split into command-specific files
- `src/daemon/` and `src/dbus/` now build into a minimal `lccd` that exposes
  `GetCapabilities`, `GetState`, `SetProfile`, `ApplyFanTable`, and
  `SetPowerLimits` over D-Bus
- `data/`, `systemd/`, and `dbus/` now carry the first capability map and bus
  integration skeletons

Development helpers:

- `bash kuangshi16pro-lcc/scripts/dev-run.sh`
  start `lccd` on a temporary user bus
- `bash kuangshi16pro-lcc/scripts/smoke-test.sh`
  build, launch `lccd` on a temporary user bus, and call the D-Bus API

Current reverse-guided execution model:
- `SetFanTableThread` delegates into `MyFanTableCtrl::SetFanTable`
- `MyFanTableCtrl` owns both `FanTable_Manager1p5` and `MyEcCtrl`
- fan-table programming appears to be a staged flow: custom-mode control plus `SetEcFanTable_Cpu` / `SetEcFanTable_Gpu`
- mode switching should currently be treated as a candidate sequence around `0x7AB`, `0x7B0..0x7B2`, `0x751`, and `0x7C7`

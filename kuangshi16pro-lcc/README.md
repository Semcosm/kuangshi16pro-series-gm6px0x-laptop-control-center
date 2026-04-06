# Kuangshi16Pro LCC

This directory is reserved for the Linux implementation of the laptop control center.

Layout:

- `src/`: application and backend source code
- `src/common/`: shared error, logging, and utility code
- `src/backend-amw0/`: AMW0 `WMBC`/`WQAC`/`ECRR` transport layer
- `src/profile-service/`: Linux-side mode/profile/fan/power model
- `src/cli/`: minimal operator-facing CLI
- `docs/`: implementation-oriented notes and curated reverse-engineering inputs
- `fixtures/`: sample payloads, fan tables, and test data

Current focus:

- reproduce the `GCUService` mode/profile model on Linux
- implement an `AMW0`-backed hardware backend
- add fan, power-limit, and status-reading control paths first

Start with:

- `docs/README.md`
- `docs/implementation-model.md`
- `docs/amw0-backend.md`

Build:

- `make -C kuangshi16pro-lcc`
- `make -C kuangshi16pro-lcc test`
- `./kuangshi16pro-lcc/build/lccctl --help`

Current CLI scope:

- `status`: read `WQAC`, `_WED(D2)`, and probe `ECRR`
- `observe`: grouped reads for `mode`, `power`, `fan`, `thermal`, or `all`
  mode and thermal groups also print a decoded summary for the currently observed bytes
- `raw wmbc`: send a traced `WMBC(..., 0x04, buffer)` packet
- `mode set`, `power set`, `fan apply`, `profile apply`: build and print a staged EC plan

Config-driven examples:

- `./kuangshi16pro-lcc/build/lccctl fan apply --file kuangshi16pro-lcc/fixtures/demo-fan.ini`
- `./kuangshi16pro-lcc/build/lccctl profile apply --file kuangshi16pro-lcc/fixtures/demo-profile.ini`
- `./kuangshi16pro-lcc/build/lccctl observe mode`
- `./kuangshi16pro-lcc/build/lccctl observe all`

Current reverse-guided execution model:

- `SetFanTableThread` delegates into `MyFanTableCtrl::SetFanTable`
- `MyFanTableCtrl` owns both `FanTable_Manager1p5` and `MyEcCtrl`
- fan-table programming appears to be a staged flow: custom-mode control plus `SetEcFanTable_Cpu` / `SetEcFanTable_Gpu`
- mode switching should currently be treated as a candidate sequence around `0x7AB`, `0x7B0..0x7B2`, `0x751`, and `0x7C7`

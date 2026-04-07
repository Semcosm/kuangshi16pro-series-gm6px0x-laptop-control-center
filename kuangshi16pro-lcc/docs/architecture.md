# Architecture

`kuangshi16pro-lcc` is moving toward a Linux-native service model rather than a
Windows-style UI + broker + bridge-process stack.

Current direction:

- one privileged system daemon, `lccd`
- D-Bus on the system bus as the only public control surface
- Polkit-backed write authorization
- thin CLI/GUI clients that never touch EC/ACPI/sysfs directly
- standard Linux ABI first, vendor backend second

Priority order for backends:

1. `backends/standard`
   prefer `hwmon`, `thermal`, `platform_profile`, and `powercap`
2. `backends/amw0`
   fallback for capabilities that are not available through standard ABI
3. `backends/uefi`
   isolated path for persistent or reboot-required features

Code split in phase 1:

- `src/core/`
  backend-agnostic profile/fan/power/state logic
- `src/backends/`
  transport and hardware-access adapters
- `src/cli/`
  current operator-facing client
- `src/daemon/` and `src/dbus/`
  scaffold for the future service boundary

Backend execution contract:

- `include/lcc/backend.h` is the only stable interface between orchestration and
  hardware access
- daemon-side orchestration talks to a backend handle, not to AMW0 transport
  helpers directly
- backends expose the same contract for `probe`, `read_state`, `apply_profile`,
  `apply_mode`, `apply_power_limits`, and `apply_fan_table`
- apply/read calls return a shared `lcc_backend_result_t`, so change detection,
  hardware-write reporting, and reboot-required hints are modeled the same way
  across mock, standard, and vendor backends

The current CLI still talks to the AMW0 backend directly for observation and
transport experiments. That is a temporary transitional arrangement until the
daemon and D-Bus server are wired up.

Runtime diagnostic field semantics are defined separately in
`docs/observability.md` so `GetState` stays a stable operational contract while
backend routing keeps evolving internally.

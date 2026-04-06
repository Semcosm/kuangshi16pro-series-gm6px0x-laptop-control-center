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

The current CLI still talks to the AMW0 backend directly for observation and
transport experiments. That is a temporary transitional arrangement until the
daemon and D-Bus server are wired up.

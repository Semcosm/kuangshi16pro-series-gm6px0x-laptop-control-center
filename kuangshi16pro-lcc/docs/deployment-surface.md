# Deployment Surface

This note defines the current deployment surface for the installed Linux
control center. It is the contract that `docs/dbus-api.md`, the checked-in
system assets, and lint-level consistency checks all follow.

## Installed Assets

Current product assets:

- `lccd`
  privileged daemon that owns hardware access and the D-Bus service
- `lccctl`
  thin product-facing CLI that talks to `lccd` over D-Bus
- `systemd/lccd.service`
  systemd unit for the installed system-bus daemon
- `systemd/dbus-io.github.semcosm.Lcc1.service`
  D-Bus system-service activation file
- `dbus/io.github.semcosm.Lcc1.conf`
  system-bus policy for the stable v1 surface
- `dbus/io.github.semcosm.Lcc1.policy`
  Polkit action definitions for mutating methods

## Naming Contract

Stable runtime identifiers:

- bus name: `io.github.semcosm.Lcc1`
- object path: `/io/github/semcosm/Lcc1`
- systemd service: `lccd.service`
- D-Bus system service name: `io.github.semcosm.Lcc1.service`
- interfaces:
  - `io.github.semcosm.Lcc1.Manager`
  - `io.github.semcosm.Lcc1.Fan`
  - `io.github.semcosm.Lcc1.Power`

Stable v1 methods:

- `io.github.semcosm.Lcc1.Manager.GetCapabilities() -> s`
- `io.github.semcosm.Lcc1.Manager.GetState() -> s`
- `io.github.semcosm.Lcc1.Manager.SetMode(mode_name: s)`
- `io.github.semcosm.Lcc1.Manager.SetProfile(profile_name: s)`
- `io.github.semcosm.Lcc1.Fan.ApplyFanTable(table_name: s)`
- `io.github.semcosm.Lcc1.Power.SetPowerLimits(pl1: y, pl2: y, pl4: y, tcc_offset: y, has_pl1: b, has_pl2: b, has_pl4: b, has_tcc_offset: b)`

Method-to-authorization mapping:

- `io.github.semcosm.Lcc1.Manager.SetMode` -> `io.github.semcosm.Lcc1.set-mode`
- `io.github.semcosm.Lcc1.Manager.SetProfile` -> `io.github.semcosm.Lcc1.set-profile`
- `io.github.semcosm.Lcc1.Fan.ApplyFanTable` -> `io.github.semcosm.Lcc1.set-fan-table`
- `io.github.semcosm.Lcc1.Power.SetPowerLimits` -> `io.github.semcosm.Lcc1.set-power-limits`

## Authorization Contract

- reads are allowed by default on the system bus
- writes stay on the stable system-bus API and pass through the daemon-side
  Polkit authorization hook before backend execution
- the D-Bus system bus config exposes the mutating methods, but authorization is
  still decided inside `lccd`
- the user bus remains a development path where the daemon permits writes
  without Polkit

## Path Contract

Current checked-in path assumptions:

- `lccd.service` owns `BusName=io.github.semcosm.Lcc1`
- `lccd.service` starts the daemon with
  `ExecStart=/usr/lib/kuangshi16pro-lcc/lccd --system`
- D-Bus activation stays declarative with
  `Name=io.github.semcosm.Lcc1` and `SystemdService=lccd.service`
- the system-bus config is installed as
  `/etc/dbus-1/system.d/io.github.semcosm.Lcc1.conf`
- the Polkit policy is installed as
  `/usr/share/polkit-1/actions/io.github.semcosm.Lcc1.policy`

Makefile install productization:

- `make install` and `make uninstall` now own the deployed file layout
- staging installs use `DESTDIR`
- install layout is parameterized through `PREFIX`, `BINDIR`, `LIBEXECDIR`,
  `SYSTEMDUNITDIR`, `DBUSSYSTEMSERVICEDIR`, `DBUSSYSTEMCONFDIR`, and
  `POLKITACTIONSDIR`
- `make install-smoke` verifies staged install, rewritten `ExecStart`, and
  uninstall cleanup without touching the live system

# D-Bus API

Planned public API surface:

- bus name: `io.github.semcosm.Lcc1`
- object path: `/io/github/semcosm/Lcc1`
- interfaces:
  - `io.github.semcosm.Lcc1.Manager`
  - `io.github.semcosm.Lcc1.Thermal`
  - `io.github.semcosm.Lcc1.Fan`
  - `io.github.semcosm.Lcc1.Power`

Design rules:

- methods stay declarative, not transport-shaped
- public callers request target state, not EC bytes or `WMBC` packets
- read operations should be available without prompting when policy allows
- write operations go through policy checks in the daemon
- API responses should distinguish `requested` from `effective`

First candidate methods:

- `GetState()`
- `GetCapabilities()`
- `SetMode(mode_name)`
- `SetProfile(profile_name)`
- `SetPowerLimits(pl1, pl2, pl4, tcc_offset, has_pl1, has_pl2, has_pl4, has_tcc_offset)`
- `ApplyFanTable(table_name)`

Current implementation status:

- `io.github.semcosm.Lcc1.Manager`
  implements `GetCapabilities`, `GetState`, `SetMode`, and `SetProfile`
- `io.github.semcosm.Lcc1.Fan`
  implements `ApplyFanTable`
- `io.github.semcosm.Lcc1.Power`
  implements partial `SetPowerLimits`
- `io.github.semcosm.Lcc1.Thermal`
  implements `GetThermalState`

At this stage the daemon is stateful but still scaffold-level:

- write methods update daemon-owned requested/effective state
- they do not yet touch hardware backends
- `GetState` and `GetThermalState` return JSON strings to keep the first
  transport iteration simple
- `SetPowerLimits` accepts presence flags so CLI callers can update only the
  fields they actually set

Experimental features such as MUX or dGPU direct-connect should remain in a
separate interface or be hidden behind a capability gate.

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
- `SetProfile(profile_name)`
- `SetPowerLimits(pl1, pl2, pl4, tcc_offset)`
- `ApplyFanTable(table_name)`

Current implementation status:

- `io.github.semcosm.Lcc1.Manager`
  implements `GetCapabilities`, `GetState`, and `SetProfile`
- `io.github.semcosm.Lcc1.Fan`
  implements `ApplyFanTable`
- `io.github.semcosm.Lcc1.Power`
  implements `SetPowerLimits`
- `io.github.semcosm.Lcc1.Thermal`
  implements `GetThermalState`

At this stage the daemon is stateful but still scaffold-level:

- write methods update daemon-owned requested/effective state
- they do not yet touch hardware backends
- `GetState` and `GetThermalState` return JSON strings to keep the first
  transport iteration simple

Experimental features such as MUX or dGPU direct-connect should remain in a
separate interface or be hidden behind a capability gate.

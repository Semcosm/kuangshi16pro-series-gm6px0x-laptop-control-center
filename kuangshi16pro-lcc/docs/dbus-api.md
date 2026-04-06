# D-Bus API

Stable service surface for v1:

- bus name: `io.github.semcosm.Lcc1`
- object path: `/io/github/semcosm/Lcc1`
- interfaces:
  - `io.github.semcosm.Lcc1.Manager`
  - `io.github.semcosm.Lcc1.Fan`
  - `io.github.semcosm.Lcc1.Power`

Design rules:

- methods stay declarative, not transport-shaped
- product-facing clients talk only to D-Bus, not to backend internals
- reads and writes are split by policy
- every mutating method enters one daemon-side authorization hook before any
  state transition or backend apply
- every mutating method reaches the transaction executor before backend access
- responses distinguish `requested`, `effective`, and `pending`

Stable methods in v1:

- `Manager.GetCapabilities() -> s`
- `Manager.GetState() -> s`
- `Manager.SetMode(mode_name: s)`
- `Manager.SetProfile(profile_name: s)`
- `Fan.ApplyFanTable(table_name: s)`
- `Power.SetPowerLimits(pl1: y, pl2: y, pl4: y, tcc_offset: y, has_pl1: b, has_pl2: b, has_pl4: b, has_tcc_offset: b)`

Authorization model:

- reads are allowed by default on the system bus
- writes are gated twice:
  - bus policy only allows mutating calls from privileged senders on the system bus
  - the daemon runs a write authorization hook before dispatching the request
- the current hook allows:
  - any caller on the user bus, for development
  - privileged callers on the system bus
- the hook already carries an action id and is the reserved integration point for
  future Polkit checks

Current transport shape:

- `GetState` returns a JSON string with `requested`, `effective`, `pending`, and
  `transaction`
- `GetCapabilities` returns the model capability JSON
- `SetPowerLimits` uses presence flags so clients can perform partial updates

Non-stable or future-facing APIs such as thermal detail, MUX, or dGPU
direct-connect should stay out of the stable v1 surface until their semantics
are settled.

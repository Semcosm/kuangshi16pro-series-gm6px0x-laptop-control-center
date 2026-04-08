# State Contract

This note defines the public `GetState` JSON contract for `lccd`.

The goal is to keep the state surface explainable while backend routing keeps
evolving internally. The daemon may read from `standard`, overlay fields from
`amw0`, and preserve transaction diagnostics across refreshes, but the JSON
surface still needs stable meanings.

## Compatibility Rules

- key names are stable within D-Bus v1
- additive fields are allowed
- field removals or semantic rewrites are not
- keys that are part of the diagnostic contract must still exist even when
  their value is `null`
- any "not read" or "not currently explainable" attribution field must
  serialize as `null`, not as a guessed backend name or default string

## Target Objects

`requested`, `effective`, `pending`, and `last_apply_target` all use the same
target shape:

```json
{
  "profile": "turbo",
  "fan_table": "system-default",
  "power": {
    "pl1": 55,
    "pl2": 95,
    "pl4": null,
    "tcc_offset": null
  }
}
```

### Target Semantics

`requested`
: the daemon's current requested target after the last successful refresh

`effective`
: the best state the daemon can currently explain after the last successful
refresh; source and freshness attribution live in `effective_meta`

`pending`
: the composed target for an in-flight or failed transaction; `null` when there
is no pending transaction

`last_apply_target`
: the composed target that the most recent mutating command attempted to apply;
`null` when target composition never completed

## Power Object Semantics

The `power` member is intentionally tri-state.

`power = null`
: the daemon does not currently have any power-limit target to report

`power = { ... }`
: the daemon has at least one power-limit field to report

`power.<field> = null`
: that specific field is currently unknown and must not be interpreted as zero

Rules:

- once `power` is an object, all four keys must exist:
  `pl1`, `pl2`, `pl4`, `tcc_offset`
- unknown per-field values are serialized as `null`
- numeric `0` is reserved for a real known zero, not for "missing"
- mixed-source states are allowed:
  for example `pl1` and `pl2` may come from Linux `powercap` while `pl4` and
  `tcc_offset` remain `null`

## Effective Metadata

`effective_meta`
: explains where the current `effective` snapshot comes from and whether it is
  live readback, daemon-retained cache, or a mixture of both

Shape:

```json
{
  "source": "mixed",
  "freshness": "mixed",
  "components": {
    "profile": {"source": "amw0", "freshness": "live"},
    "fan_table": {"source": "cache", "freshness": "cache"},
    "power": {
      "source": "mixed",
      "freshness": "mixed",
      "fields": {
        "pl1": {"source": "standard", "freshness": "live"},
        "pl2": {"source": "standard", "freshness": "live"},
        "pl4": {"source": null, "freshness": null},
        "tcc_offset": {"source": "amw0", "freshness": "live"}
      }
    },
    "thermal": {"source": "standard", "freshness": "live"}
  }
}
```

Component meanings:

`source`
: which backend family currently explains that component, or `cache` when the
  daemon only has a retained value, or `mixed` when multiple explanations are
  combined

`freshness`
: `live`, `cache`, or `mixed`

Rules:

- overall `effective_meta.source` is derived from the component sources
- overall `effective_meta.freshness` is derived from the component freshness
- `effective_meta.components.power.source` and `.freshness` are derived from
  the per-field power attribution under `effective_meta.components.power.fields`
- `cache` means "daemon-retained and not confirmed by the current refresh"
- `mixed` means the daemon combined multiple contributors instead of observing a
  single clean readback path
- if a component cannot be attributed at all, both `source` and `freshness`
  must be `null`
- power-field attribution must use `null` when a specific field is not currently
  readable or explainable

## Route And Attribution Fields

`backend`
: the backend that produced the last successful state refresh

`backend_selected`
: the backend family selected during daemon startup and probe

`execution`
: the planned route table for `read_state`, `apply_profile`, `apply_mode`,
`apply_power_limits`, and `apply_fan_table`

`backend_fallback_reason`
: human-readable explanation for why the daemon is not operating as a pure
`standard` backend

## Apply Diagnostics

`last_apply_stage`
: the stage where the most recent mutating command stopped

`last_apply_backend`
: the backend that most recently executed the mutating command path

`last_apply_error`
: `null` on success, otherwise a symbolic `lcc_status_t` string

`last_apply_hardware_write`
: the backend-reported hardware-write result for the most recent mutating
command that reached backend apply

Rules:

- `last_apply_hardware_write = null` means the daemon has not yet reached a
  backend apply path for the last observed command window
- `true` means the backend reported a real hardware write
- `false` means the backend reported no hardware write, for example dry-run or
  a write path that stopped before touching hardware

## Legacy Compatibility Field

`hardware_write`
: legacy compatibility field kept for existing tooling

Interpretation:

- it is a coarse daemon-side summary, not precise last-command attribution
- callers that need exact attribution should prefer `last_apply_hardware_write`

## Transaction Object

The `transaction` object reports the currently pending or last failed
transaction snapshot. It is orthogonal to `last_apply_*`.

- `transaction` explains active or failed orchestration state
- `last_apply_*` explains the most recent mutating command outcome

Both views are needed because the daemon can be idle while still preserving the
diagnostics of the most recent mutating request.

# Observability Contract

This note defines the stable runtime diagnostics exposed by `lccd` through
`GetState`.

The goal is to explain three different questions without adding a second
diagnostic service:

- which backend last produced the effective state snapshot
- which backend family the daemon selected at startup
- which backend actually executed the most recent mutating command

## Stable State Fields

`backend`
: the backend that produced the last successful state refresh

`backend_selected`
: the daemon's selected backend family after startup and probe

`execution`
: the planned route table for each capability:
  `read_state`, `apply_profile`, `apply_mode`, `apply_power_limits`,
  `apply_fan_table`

`backend_fallback_reason`
: a short human-readable explanation of why the daemon is not running as a
  pure `standard` backend

`last_apply_stage`
: where the most recent mutating command stopped
  public stages are used for preflight or routing failures;
  backend detail stages are used once backend execution begins

`last_apply_backend`
: the backend that most recently executed the mutating command path

`last_apply_error`
: `null` on success, otherwise a symbolic `lcc_status_t` string

`last_apply_target`
: the composed target that the daemon attempted to apply

`transaction`
: the currently pending or last failed transaction snapshot

## Stage Model

The daemon uses public transaction stages in logs:

- `preflight-validate`
- `capability-gate`
- `backend-route`
- `apply`
- `state-refresh`
- `complete`

Backends may also report detail stages. These are preserved in
`last_apply_stage` once backend execution starts.

Current stable backend detail stages include:

- `standard`:
  `read-platform-profile`, `read-hwmon`, `read-thermal`, `read-powercap`,
  `write-platform-profile`
- `amw0`:
  `set-mode-index`, `set-mode-control`, `write-pl1`, `write-pl2`, `write-pl4`,
  `write-tcc-offset`, `custom-enable`, and the existing fan-plan stage labels

## Interpretation Rules

- `execution` describes the planned route, not the last executor
- `last_apply_backend` describes the last executor, not the global selection
- `backend` may differ from `last_apply_backend` when a converged backend reads
  through `standard` but writes through `amw0`
- if a command fails before target composition, `last_apply_target` is `null`
- if a command fails at capability gate, `last_apply_backend` is `null`

## Logging Contract

Transaction logs keep the public stage in `stage=` and attach executor or
backend detail in `backend=` and `detail=`.

The stable format is:

```text
transaction operation=<op> stage=<stage> backend=<executor|none> status=<status> target=<summary> detail=<detail|none>
```

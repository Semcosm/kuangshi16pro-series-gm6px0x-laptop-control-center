# Fan Chain Roadmap

This note turns the current fan implementation into a staged engineering plan.

The goal is not only "fan apply works on one machine", but:

- fan telemetry is explainable
- fan writes have observable verification
- `GetState` clearly separates live readback from retained cache
- hardware smoke can catch regressions without reverse-engineering the whole
  vendor path again

## Current Baseline

- `standard` fan telemetry currently reads fixed `hwmon` filenames:
  `fan1_input`, `fan2_input`, `temp1_input`, `temp2_input`
- `amw0` fan programming already has a staged write plan and good stage names
- successful `amw0` fan writes currently update daemon cache, but they do not
  yet prove the programmed table through a live readback path
- `converged` therefore often reports:
  `fan_table = cache/cache`
- on real hardware, `cpu_fan_rpm` and `gpu_fan_rpm` may still be `null` even
  when fans are obviously spinning
- current reverse work shows `FFAN` moving live with load, but only as a
  vendor fan level; it must not be mislabeled as RPM

## Engineering Principles

1. Never guess fan identity from slot order if labels or names can prove it.
2. Never upgrade cache to live without an explicit readback signal.
3. Keep product semantics and telemetry semantics separate.
4. Prefer stable Linux ABI reads first, then use vendor reads only to fill
   explainable gaps.
5. Every fan write path needs an acceptance signal, not only a transport
   success signal.

## Main Risks To Eliminate

### Risk 1: Wrong sensor attribution

Today `standard` assumes:

- `fan1_input` = CPU fan
- `fan2_input` = GPU fan

That is too brittle. On some machines the order differs, some sensors are
missing, and labels may live in `fan*_label`, `name`, or nearby temperature
channels instead.

### Risk 2: Write success without behavioral proof

Today `amw0` can successfully send a fan table and still only leave:

- `fan_table.source = cache`
- `fan_table.freshness = cache`

That is honest, but not strong enough for a mature product control path.

### Risk 3: Mixed thermal sources are under-explained

The daemon already has a solid `effective_meta` model for power. Fan and
thermal need the same level of rigor:

- which backend produced the fan-table identity
- which backend produced RPM and temperature signals
- whether the daemon is reporting live behavior or retained intent
- whether a signal is true RPM or only a vendor-defined fan level

## Recommended Implementation Order

### Phase 1: Make standard telemetry discovery model-driven

Upgrade `standard/hwmon.c` so it no longer hardcodes only:

- `temp1_input`
- `temp2_input`
- `fan1_input`
- `fan2_input`

Instead, discover and score sensors using:

- `name`
- `fan*_label`
- `temp*_label`
- presence of related fan and temp channels in the same `hwmon` node

Recommended behavior:

- identify package-level `hwmon` candidates first
- prefer nodes whose labels mention `cpu`, `gpu`, `pch`, `soc`, or similar
- if CPU/GPU mapping is still ambiguous, keep the value but leave role-specific
  attribution conservative rather than inventing certainty

Acceptance:

- the real machine stops returning `cpu_fan_rpm=null` and `gpu_fan_rpm=null`
  when Linux actually exports those sensors
- unit tests cover mislabeled, partially labeled, and swapped-order fixtures

Current status:

- completed the first discovery upgrade:
  `standard/hwmon.c` now scores `name`, `temp*_label`, and `fan*_label`
  instead of assuming only `temp1/temp2/fan1/fan2`
- unit coverage now includes:
  multiple `hwmon` directories
  `temp3` / `fan2` role assignment
  GPU `temp1` / `fan1` on an `amdgpu` node
- still pending:
  real-machine confirmation that the upgraded discovery resolves current
  `cpu_fan_rpm` / `gpu_fan_rpm` null gaps on the target laptop

### Phase 2: Introduce explicit fan telemetry attribution

Extend `effective_meta` interpretation for fan state so callers can tell:

- whether `fan_table` identity is only retained cache
- whether RPM is coming from `standard` live telemetry
- whether the daemon has mixed fan evidence
- whether `vendor_fan_level` is coming from `amw0` live telemetry

The existing model already has:

- `fan_table`
- `thermal`

Use that model more deliberately rather than inventing a parallel fan state
surface too early.

Recommended contract:

- `fan_table` continues to mean "which table the daemon believes is active"
- RPM remains under `thermal`
- documentation explicitly says fan-table identity and RPM are separate signals

Acceptance:

- `GetState` can explain "table came from cache, RPM came from standard live"
- logs and JSON tell the same story for fan operations

### Phase 3: Add fan write verification tiers

Fan writes need stronger post-apply behavior than "transport returned OK".

Recommended verification tiers:

1. transport success
   `amw0` plan executed without backend error
2. state refresh consistency
   daemon still reports the intended table in `requested` and `last_apply_target`
3. behavioral evidence
   RPM or thermal response moves in the expected direction under a controlled
   load window

The first two tiers can be product code and unit tests.
The third tier belongs in hardware validation, not in the daemon itself.

Recommended daemon behavior:

- keep `fan_table.source = cache` until a real readback signal exists
- do not fake `live` fan-table attribution from write success alone
- preserve detailed stage labels from the plan for postmortem use

Acceptance:

- fan apply success remains honest about cache vs live
- failure stages continue to pinpoint CPU table, GPU table, or finalize tail
  bytes

### Phase 4: Build a real hardware fan-validation recipe

Add a dedicated hardware smoke recipe for fan work:

- capture baseline `lccctl state`
- capture baseline RPM and temperatures
- apply a known table
- run a repeatable load window
- compare RPM and temperature deltas
- record `journal`, `state`, and a short operator summary

The key point is not to "prove every EC byte". It is to prove that:

- the daemon applied a named table
- the machine reacted in a way consistent with that table

Acceptance:

- hardware artifacts can distinguish:
  write path failed
  write path succeeded but behavior unchanged
  write path succeeded and behavior changed

### Phase 5: Only then consider live fan-table readback

If future reverse work reveals a reliable readback source for:

- active fan table ID
- custom fan table bytes in RAM
- fan-control mode bits

then `fan_table.source` can graduate from `cache` to `live` on supported
machines.

Until that source is proven, keep the current conservative semantics.

## Recommended Test Expansion

### Unit tests

- `standard/hwmon`
  add fixtures with:
  `fan*_label`
  swapped fan order
  only one fan present
  multiple `hwmon` directories
- `converged`
  assert mixed thermal evidence does not incorrectly rewrite fan-table
  attribution
- `amw0`
  keep stage-order tests and add failure tests for each major fan-plan stage

### Integration tests

- keep the current D-Bus smoke
- add one fan-oriented smoke that checks:
  `last_apply_stage`
  `last_apply_backend`
  `fan_table`
  `effective_meta`

### Hardware tests

- baseline idle
- CPU-only load
- combined CPU/GPU load when practical
- compare RPM movement, not only command success

## Acceptance Criteria For "Fan Chain Mature"

Call the fan path mature only when all of these are true:

- real hardware reports explainable RPM values on the supported machine
- `fan apply` keeps precise stage attribution on failure
- `GetState` cleanly distinguishes cache intent from live telemetry
- hardware smoke can show at least one reproducible behavioral effect after a
  table change
- documentation explains exactly what is proven and what is still inferred

## What Not To Do

- do not mark `fan_table` as `live` just because AMW0 writes succeeded
- do not hardcode CPU/GPU fan identity forever based on `fan1` / `fan2`
- do not treat one successful custom-table write as proof that all fan families
  share the same layout
- do not couple future fan validation to developer-only raw ACPI tooling

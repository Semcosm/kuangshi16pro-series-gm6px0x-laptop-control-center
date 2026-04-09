# Hardware Validation

PR11 keeps hardware validation on the product path:

- `lccd` on the system bus is the only service under test
- `lccctl` is the only client used for smoke validation
- `developer raw wmbc` is excluded from product verification

## Validation Prerequisites

Run real-machine validation only after the current daemon and D-Bus assets are
installed on the laptop under test.

Recommended preparation:

1. Build and preflight the current tree:

   ```bash
   make -C kuangshi16pro-lcc
   make -C kuangshi16pro-lcc test
   make -C kuangshi16pro-lcc install-smoke
   ```

2. Install or refresh the deployed daemon and system assets used by the smoke
   runner:

   ```bash
   sudo make -C kuangshi16pro-lcc install
   ```

3. Reload and start the daemon:

   ```bash
   sudo systemctl daemon-reload
   sudo systemctl enable --now lccd.service
   sudo systemctl status lccd.service --no-pager
   ```

4. Run the smoke runner from an active desktop session with a working Polkit
   agent. Mutating commands stay on the system bus and are expected to be
   authorized through Polkit, not by bypassing the daemon.

5. Confirm journal access before the run:

   ```bash
   sudo journalctl -u lccd.service -n 20 --no-pager
   ```

## Validation Matrix

`tests/hardware/run_real_smoke.sh` accepts `LCC_HW_MATRIX` and applies a
temporary runtime override to `lccd.service` for the duration of the run.

`standard-only`
: force `LCC_BACKEND=standard`

- expected focus: standard Linux ABI reads and mode writes
- expected caveat: if `platform_profile` is unavailable on the machine under
  test, mode writes may also stop at `capability-gate`
- expected caveat: `fan apply` and some `power set` calls may stop at
  `capability-gate`

`amw0-forced`
: force `LCC_BACKEND=amw0`

- expected focus: mode, power, and fan paths all execute on `amw0`
- expected diagnostics: `last_apply_backend=amw0` on mutating success or
  write-path failure

`mixed`
: converged selection with mixed backend availability

- expected focus: `backend_selected`, `execution`, and `last_apply_backend`
  stay mutually explainable
- expected diagnostics: state reads may stay on `standard` while mutating power
  or fan flows execute on `amw0`

`dry-run`
: leave backend selection converged and set `LCC_AMW0_DRY_RUN=1`

- expected focus: stable stage, route, and failure attribution even when the
  matrix is used for non-destructive validation
- expected caveat: command success is secondary to state and journal
  consistency

## Recorded Evidence

Each step writes a dedicated artifact directory:

```text
artifacts/hardware-smoke/YYYYMMDD-HHMMSS/<step>/
```

The runner records at least these files per step:

- `command.txt`
- `stdout.txt`
- `stderr.txt`
- `exit_code.txt`
- `state.json`
- `journal.log`
- `summary.txt`

Run-level artifacts also include:

- `metadata.txt`
- `contract.txt`
- `run-summary.txt`
- `service-override.conf`
- `service-environment.txt`

Review every step against the same checklist:

- executed command
- D-Bus client exit code and stdout/stderr
- `lccctl state` snapshot after the step
- `backend_selected`
- `effective_meta`
- `last_apply_stage`
- `last_apply_error`
- `last_apply_backend`
- `last_apply_hardware_write`
- `journalctl -u lccd.service` snippet captured for the same step window

The runner also prints a contract summary for each step, including:

- `backend`
- `backend_selected`
- `backend_fallback_reason`
- `effective_meta`
- `execution`
- `last_apply_stage`
- `last_apply_error`
- `last_apply_backend`
- `last_apply_hardware_write`
- `transaction`

When `state` capture succeeds, the hardware runner treats selected diagnostic
keys as a contract: the key must exist in JSON even when its value is `null`.
Missing keys are treated as a regression; `null` values remain valid state.
That includes attribution fields under `effective_meta`: if a component cannot
be read or explained, the contract expects explicit `null`, not a guessed
backend name. For power limits, this rule applies both to the parent
`effective_meta.components.power` summary and to each field under
`effective_meta.components.power.fields`.

Before any step assertions, the runner records `systemctl show -p Environment`
for `lccd.service` and fails fast if the selected matrix override is not
actually present in the service environment.

The runner also loads a pinned matrix contract from:

```text
tests/hardware/expectations/<matrix>.txt
```

Each step contract asserts the installed daemon path still reports the expected
`backend`, `backend_selected`, route table, and `last_apply_*` attribution for
that matrix. The goal is no longer only "command ran" but "route and failure
layer stayed explainable on real hardware."

## Result Interpretation

Successful validation means:

- the product command result matches the pinned matrix contract for that step
- `state.json` and `journal.log` tell the same story about route and outcome
- `backend_selected`, `execution`, and `last_apply_backend` remain consistent

Failed validation means the evidence shows one of these failure layers:

- `preflight-validate`
- `capability-gate`
- `backend-route`
- actual backend write path, such as `write-platform-profile` or `write-pl1`

Do not re-run with `developer raw wmbc` to explain a product failure. The first
artifact set from the daemon path should already be enough to identify whether
the failure is input validation, capability gating, route selection, or the
backend write itself.

## Powercap Audit

When `effective.power` looks suspicious, capture raw Linux `powercap` state
before changing backend code again.

Use:

```bash
kuangshi16pro-lcc/scripts/powercap-audit.sh snapshot
kuangshi16pro-lcc/scripts/powercap-audit.sh around -- lccctl power set --pl1 70 --pl2 120
```

The script writes a timestamped directory under `/tmp` by default. It captures:

- raw `powercap` zones and `constraint_*` files
- `lccctl state`
- `lccctl capabilities`
- `systemctl show lccd.service`
- in `around` mode, the command stdout/stderr/exit code plus `powercap.diff`
  and `state.diff`

Interpretation rules:

- if `powercap.txt` already shows the same `constraint_0/1_power_limit_uw`
  values as `lccctl state`, the daemon is mirroring Linux `powercap` correctly
- if `lccctl power set ...` changes daemon state but not `powercap.diff`, then
  the write path and Linux `powercap` are reporting different semantics and
  should not be conflated
- if `pl1/pl2` change while `pl4` or `tcc_offset` come from cache or `amw0`,
  treat that as an intentional split path rather than a state mismatch; confirm
  it through `last_apply_backend=mixed` and
  `effective_meta.components.power.fields`
- if only one field, such as `tcc_offset`, changes in `state.diff`, prefer the
  field-level attribution in `effective_meta.components.power.fields` over the
  parent `power.source`

Current evidence boundary:

- `PL1` and `PL2` are hardware-proven on this machine when `turbostat`,
  `intel-rapl:0`, and `MSR_PKG_POWER_LIMIT` all agree under load
- `TCC offset` is hardware-proven when `MSR_IA32_TEMPERATURE_TARGET` shows the
  expected offset
- `PL4` is not yet fully hardware-proven through standard Intel telemetry on
  this machine; treat it as a vendor-path value with clear state attribution,
  not as a fully validated equivalent of Intel `PKG Limit #4`

## Fan Apply Audit

When `fan apply` succeeds at the transport layer but fan behavior still feels
unclear, capture one dedicated artifact set instead of relying on ad-hoc
terminal history.

Use:

```bash
kuangshi16pro-lcc/scripts/fan-apply-audit.sh snapshot

kuangshi16pro-lcc/scripts/fan-apply-audit.sh around \
  --watch 45 \
  --file kuangshi16pro-lcc/data/fan-tables/fan-balanced.json \
  -- stress-ng --cpu 16 --timeout 45s

kuangshi16pro-lcc/scripts/fan-apply-audit.sh compare \
  --watch 45 \
  --file kuangshi16pro-lcc/data/fan-tables/fan-aggressive.json \
  -- stress-ng --cpu 16 --timeout 45s
```

The script writes a timestamped directory under `/tmp` by default. In
`around` mode it records:

- `before/`, `after-apply/`, and `after-watch/` state snapshots
- `apply.stdout`, `apply.stderr`, `apply.exit`
- `apply-journal.log` and `watch-journal.log`
- `watch.stdout` and `watch-summary.txt`
- `state-after-apply.diff` and `state-after-watch.diff`

In `compare` mode it additionally records:

- `baseline-watch-summary.txt`
- `custom-watch-summary.txt`
- `compare-summary.txt`
- `state-baseline.diff`

Interpretation rules:

- if `apply.exit` is zero and `state-after-apply.diff` shows
  `last_apply_backend=amw0` plus the expected `fan_table` target, the product
  fan write path succeeded
- if `watch-summary.txt` shows `ffan_max` rising under load and then falling
  after the load ends, that is behavioral evidence that the vendor fan path is
  reacting
- `vendor_fan_level` and `FFAN` are vendor fan activity levels, not RPM
- if `cpu_fan_rpm` and `gpu_fan_rpm` remain `null` on this machine, that is
  expected until a real RPM source is proven; do not substitute `FFAN`
- if `watch-summary.txt` stays flat while `apply` succeeds, treat that as
  "transport success without behavioral proof" and keep `fan_table` semantics
  conservative
- `compare` mode compares the current active fan behavior against the requested
  table; it does not imply the baseline is `system-default`
- when the active table may already be `fan-balanced`, prefer
  `data/fan-tables/fan-aggressive.json` for compare mode so the A/B
  window uses intentionally different fan curves

The hardware smoke summary also records:

- `thermal_cpu_temp_c`
- `thermal_gpu_temp_c`
- `thermal_cpu_fan_rpm`
- `thermal_gpu_fan_rpm`
- `thermal_vendor_fan_level`

## Runner Usage

Default run:

```bash
bash kuangshi16pro-lcc/tests/hardware/run_real_smoke.sh
```

Examples:

```bash
LCC_HW_MATRIX=standard-only \
  bash kuangshi16pro-lcc/tests/hardware/run_real_smoke.sh

LCC_HW_MATRIX=amw0-forced LCC_KEEP_ARTIFACTS=1 \
  bash kuangshi16pro-lcc/tests/hardware/run_real_smoke.sh

LCC_HW_MATRIX=dry-run LCC_SKIP_FAN=1 \
  bash kuangshi16pro-lcc/tests/hardware/run_real_smoke.sh
```

Supported environment variables:

- `LCC_HW_MATRIX=standard-only|amw0-forced|mixed|dry-run`
- `LCC_KEEP_ARTIFACTS=0|1`
- `LCC_JOURNAL_LINES=<count>`
- `LCC_SKIP_FAN=0|1`
- `LCC_SKIP_POWER=0|1`

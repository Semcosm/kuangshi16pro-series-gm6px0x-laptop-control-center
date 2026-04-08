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

2. Install or refresh the system assets used by the smoke runner:

   ```bash
   sudo make -C kuangshi16pro-lcc install
   ```

   Default install destinations:

   - `/usr/lib/kuangshi16pro-lcc/lccd`
   - `/etc/systemd/system/lccd.service`
   - `/usr/share/dbus-1/system-services/io.github.semcosm.Lcc1.service`
   - `/etc/dbus-1/system.d/io.github.semcosm.Lcc1.conf`
   - `/usr/share/polkit-1/actions/io.github.semcosm.Lcc1.policy`

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
- `service-override.conf`
- `service-environment.txt`

Review every step against the same checklist:

- executed command
- D-Bus client exit code and stdout/stderr
- `lccctl state` snapshot after the step
- `backend_selected`
- `last_apply_stage`
- `last_apply_error`
- `last_apply_backend`
- `journalctl -u lccd.service` snippet captured for the same step window

The runner also prints a condensed console summary for:

- `backend`
- `backend_selected`
- `backend_fallback_reason`
- `last_apply_stage`
- `last_apply_error`
- `last_apply_backend`

When `state` capture succeeds, the hardware runner treats selected diagnostic
keys as a contract: the key must exist in JSON even when its value is `null`.
Missing keys are treated as a regression; `null` values remain valid state.

Before any step assertions, the runner records `systemctl show -p Environment`
for `lccd.service` and fails fast if the selected matrix override is not
actually present in the service environment.

## Result Interpretation

Successful validation means:

- the product command returns success, or the matrix explicitly allows the
  failure mode
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

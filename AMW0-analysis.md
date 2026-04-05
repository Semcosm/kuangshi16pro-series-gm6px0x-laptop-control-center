# AMW0 reverse-engineering notes

These notes track the AML-backed facts that currently matter most for fan-control work on this machine.

## What AMW0 actually is

`Device (AMW0)` is a generic ACPI WMI bridge.

- `WQAA` / `WQAB` / `WQAC` are getters for static buffers `SAA*`, `SAB*`, `SAC*`.
- `WMBA` / `WMBB` / `WMBC` are the generic WMI entry points.
- `WMBC(..., 0x04, buffer)` copies the caller payload into `AC00` and runs `OEMG(AC00)`.
- `SETC(0, Arg1)` runs `OEMF(AC00)`.
- `OEMG` / `OEMF` then dispatch to `WKBC`, `RKBC`, or `SCMD`.
- Within this DSDT, those `WKBC` / `RKBC` / `SCMD` calls only appear under `OEMG` / `OEMF`.

The important byte layout detail is:

- bytes `0..3` of `AC00` are `SA00..SA03`
- bytes `4..7` of `AC00` are `SAC1`

So for `WMBC(..., 0x04, buffer)`:

- `WKBC` / `RKBC` / `SCMD` arguments come from bytes `0..3`
- route selection comes from the second dword (`SAC1`)

This was confirmed locally after fixing the payload builders:

- payload `49 00 1E 00 | 00 00 00 00` produced `WQAC0 = 0x1e0049`
- payload `4C 00 28 00 | 01 00 00 00` produced `WQAC0 = 0x28004c`

So `WQAC0` is reflecting the little-endian command dword from `SA00..SA03`, exactly as AML implies.

That means `AMW0` itself does not encode fan semantics. It is only the transport layer.

## Important ECXP bytes

These bytes live in the AML `EmbeddedControl` region and can be watched with `ec_probe`:

- `0x7c` = `OSDF`
- `0x8a` = `LDAT`
- `0x8b` = `HDAT`
- `0x8c` = transport flags byte containing `RFLG`, `WFLG`, `BFLG`, `CFLG`, `DRDY`
- `0x8d` = `CMDL`
- `0x8e` = `CMDH`

These are the most useful bytes for understanding what `WKBC`, `RKBC`, and `SCMD` are doing.

One confirmed correction from AML:

- EC `0x49` is `ACUR`, not a fan register.

So the old `0x49` / `0x4c` single-register fan hypothesis is not supported by AML.

Corrected `WKBC` traces directly established this ECXP transport mapping:

- `LDAT = SA00`
- `HDAT = SA01`
- `CMDL = SA02`
- `CMDH = SA03`

Observed local examples:

- `49 00 1E 00` -> `LDAT = 73`, `CMDL = 30`
- `4C 00 28 00` -> `LDAT = 76`, `CMDL = 40`
- `49 55 1E 66` -> `LDAT = 73`, `HDAT = 85`, `CMDL = 30`, `CMDH = 102`

The same `49 55 1E 66` test also produced `WQAC0 = 0x661e5549`, confirming that `WQAC0` is the little-endian dword built from `SA00..SA03`.

## Important ECMG fields

The fan-looking fields are not in the plain `EmbeddedControl` region. They are in the AML `SystemMemory` region `ECMG`, based at `ECMA = 0xFE410000`:

- `ECMG + 0x460` = `FFAN`
- `ECMG + 0xE1C` / `+0xE1D` = `F1SH` / `F1SL`
- `ECMG + 0xE8C` = `F1DC` + `F1CM`
- `ECMG + 0xE9D` = `F2DC` + `F2CM`

This is the strongest AML evidence so far for where real fan mode, speed, and duty data lives.

There is also an AML helper in `\_SB.INOU`:

- `ECRR (Arg0)` reads byte `0xFE410000 + Arg0`
- `ECRW (Arg0, Arg1)` writes byte `0xFE410000 + Arg0`

This was confirmed locally through `acpi_call`.

The first local `ECMG` reads currently show a very sparse state:

- `FFAN = 2`
- `F1SH`, `F1SL`, `F1DC`, `F2DC`, `CPUT`, `PCHT`, and `SN1T..SN5T` all read as `0`
- the same values remained unchanged for at least 5 seconds after a corrected `SCMD` send

So the AML-backed read path works, but most of the apparent fan/thermal fields are not yet populated in a useful way on the present Linux runtime.

## Event path

The EC query handlers around the same area show:

- `_Q53` sends `WMBC(0, 0x03, OSDF)`
- `_Q77` sends `WMBC(0, 0x03, 0xF0)`
- `_Q78` sends `WMBC(0, 0x03, 0xF0)`

So some hotkeys or EC notifications only emit WMI events. They do not directly execute `OEMG`.

This event path was confirmed locally with `amw0-event-path-test.sh`:

- `WMBC(0, 0x03, 0x01)` made `WQAC1 = 0x1` and `_WED(D2) = 0x1`
- `WMBC(0, 0x03, 0xF0)` made `WQAC1 = 0xF0` and `_WED(D2) = 0xF0`
- `WMBC(0, 0x03, 0x00)` returned both to zero
- `OSDF`, `HBTN`, `BRTS`, and `W8BR` stayed unchanged during that test

So `WMBC(..., 0x03, value)` is a working software-visible event channel, but it still does not touch the real hardware-control path.

Corrected `WKBC` tracing now shows that the real hardware path is being reached:

- `WMBC(..., 0x04, ...)` runtime increased from the old `4-6 ms` to about `77-78 ms`
- `FLAGS = 132` was observed during send, which matches `BFLG = 1` and `DRDY = 1` with the current AML bit interpretation
- `LDAT` / `CMDL` remained latched after send, so those bytes are stable enough to inspect post-call
- both tested route selectors, `SAC1 = 0x0000` and `SAC1 = 0x0001`, successfully reached `WKBC`

From AML, `SCMD` is a different transport shape:

- it writes only `CMDL = Arg0`
- it sets `CFLG = One`
- it does not populate `LDAT` / `HDAT` / `CMDH`

This is now partly confirmed locally:

- corrected `SCMD 0x00` tracing changed `CMDL` from `0x1E` to `0x00`
- `LDAT`, `HDAT`, and `CMDH` stayed at the previous latched `WKBC` values
- runtime was again about `78 ms`, consistent with reaching the real transport path
- the expected `CFLG` transition was not caught in the 10 ms polling snapshots

## Practical implications

- `WQBA` is solved and not useful for fan control.
- `WQAC0` / `WQAC1` are only `SAC0` / `SAC1` transport dwords from `AC00`.
- `WQAC0` reflects command bytes `SA00..SA03`, not the route selector.
- `WQAC1` is the route dword, but AML clears it after `WMBC(..., 0x04, ...)`.
- On this machine the current idle baseline has repeatedly shown `WQAC0 = 0x1`.
- The best current Linux-side observability is `OSDF` plus the `LDAT/HDAT/CMDL/CMDH` transport bytes.
- The best current fan candidates from AML are the `ECMG` fields `FFAN`, `F1DC/F2DC`, and `F1SH/F1SL`.

## Local tooling

Useful scripts in this directory now are:

- `amw0-ec-watch.sh` for watching EC event/transport bytes live
- `amw0-ecmg-read.sh` for reading AML `ECMG` fan/thermal bytes through `\_SB.INOU.ECRR`
- `amw0-scmd-scan.sh` for one-byte `SCMD` scans with transport logging
- `amw0-wkbc-scan.sh` for four-byte `WKBC` scans with transport logging
- `amw0-wmbc-pack.sh` for building and sending a single `WMBC` payload

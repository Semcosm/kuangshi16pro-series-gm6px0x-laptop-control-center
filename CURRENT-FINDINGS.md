# Current Findings (2026-04-05)

This file records only findings that are currently explicit enough to rely on.

## Confirmed

1. `WQBA.bin` is a standard compressed ACPI/WMI BMF blob, not a vendor fan-control schema.
   The decoded MOF in `WQBA.mof` is the generic Microsoft `AcpiTest_*` schema.

2. `AMW0` is a generic ACPI WMI transport device.
   `WQAA` / `WQAB` / `WQAC` are simple getters over `SAA*`, `SAB*`, `SAC*`.
   `WMBA` / `WMBB` / `WMBC` are the generic entry points.

3. `WMBC(..., 0x03, value)` is a working software event path.
   Local proof:
   `amw0-logs/event-path-20260405-140224.log`
   shows:
   - `0x01` -> `WQAC1 = 0x1`, `_WED(D2) = 0x1`
   - `0xF0` -> `WQAC1 = 0xF0`, `_WED(D2) = 0xF0`
   - `0x00` -> both return to `0x0`

4. `WMBC(..., 0x04, buffer)` uses `AC00` with this exact byte layout:
   - bytes `0..3` = `SA00..SA03`
   - bytes `4..7` = `SAC1`

5. Routing for `WMBC(..., 0x04, buffer)` is controlled by `SAC1`:
   - `0x0000` or `0x0001` -> `WKBC(SA00, SA01, SA02, SA03)`
   - `0x0100` -> `RKBC(SA00, SA01)`
   - `0x0200` -> `SCMD(SA00)`

6. `WQAC0` / `WQAC1` mean:
   - `WQAC0` = `SAC0` = first dword of `AC00` = command bytes `SA00..SA03`
   - `WQAC1` = `SAC1` = second dword of `AC00` = route selector
   AML clears `SAC1` and `SAC2` after `WMBC(..., 0x04, ...)`, so `WQAC1` usually falls back to zero after send.

7. The earlier payload builders were wrong before 2026-04-05 14:20 local work.
   They wrote the route dword into bytes `0..3` and the command bytes into `4..7`.
   All `WMBC(..., 0x04, ...)` tooling has now been corrected.

8. EC register `0x49` is `ACUR`, not a fan register.
   The old "write EC 0x49 / 0x4c directly" hypothesis is not supported by AML.

9. The strongest AML fan candidates are in `ECMG` system memory, not in the plain EC byte window:
   - `ECMG + 0x460` = `FFAN`
   - `ECMG + 0xE1C` / `+0xE1D` = `F1SH` / `F1SL`
   - `ECMG + 0xE8C` = `F1DC` + `F1CM`
   - `ECMG + 0xE9D` = `F2DC` + `F2CM`

10. AML also exposes a direct byte accessor for `ECMG` through `\_SB.INOU.ECRR` / `ECRW`.
    `ECRR (Arg0)` reads byte `0xFE410000 + Arg0`.
    `ECRW (Arg0, Arg1)` writes byte `0xFE410000 + Arg0`.
    This is a cleaner Linux-side observation path than relying on `/dev/mem`.

11. On this machine, `\_SB.INOU.ECRR` is callable through `acpi_call`.
    Local proof:
    - `amw0-logs/ecmg-read-20260405-211608.log`
    - `amw0-logs/ecmg-read-20260405-211627.log`

12. The current observable `ECMG` state is mostly zeroed or inactive under Linux, except for `FFAN = 2`.
    In the local `ECRR` reads:
    - `FFAN` stayed at `2`
    - `F1SH`, `F1SL`, `F1DC`, `F2DC`, `CPUT`, `PCHT`, and `SN1T..SN5T` all stayed `0`
    - the same zero-heavy state remained stable for at least 5 seconds after an `SCMD` send

13. `WKBC`, `RKBC`, and `SCMD` are all gated by `ECON`.
    In this DSDT, `ECON` is set only in `EC0._REG(0x03, One)`.
    That means Linux-side failure may simply be the EC transport gate not being opened the way firmware expects.

14. `WKBC` / `RKBC` / `SCMD` only appear under `OEMG` / `OEMF` in this DSDT.
    There is no second AML caller path hiding elsewhere.

15. Corrected `WKBC` tracing now proves the fixed payload layout is hitting the real EC transport path.
    Local proof:
    - `amw0-logs/wmbc-trace-20260405-141646.log`
    - `amw0-logs/wmbc-trace-20260405-141705.log`
    Both route selectors tested so far reached `WKBC`:
    - `SAC1 = 0x0000`
    - `SAC1 = 0x0001`

16. In those corrected `WKBC` traces:
    - send time increased to about `77-78 ms`, no longer the old `4-6 ms`
    - `WQAC0` became `0x1e0049` and then `0x28004c`
    - this matches the command dword built from `SA00..SA03`
    - `WQAC1` stayed `0x0`, consistent with AML clearing `SAC1`

17. The corrected `WKBC` traces now directly prove this EC-side argument mapping:
    - `LDAT = SA00`
    - `HDAT = SA01`
    - `CMDL = SA02`
    - `CMDH = SA03`

    Direct evidence:
    - payload `49 00 1E 00` led to `LDAT = 73 (0x49)` and `CMDL = 30 (0x1E)`
    - payload `4C 00 28 00` led to `LDAT = 76 (0x4C)` and `CMDL = 40 (0x28)`
    - payload `49 55 1E 66` led to `LDAT = 73 (0x49)`, `HDAT = 85 (0x55)`, `CMDL = 30 (0x1E)`, `CMDH = 102 (0x66)`

18. The `49 55 1E 66` test also confirmed the full little-endian command dword view in `WQAC0`.
    After send, `WQAC0 = 0x661e5549`, which exactly matches `SA00..SA03`.

19. Corrected `SCMD` tracing now shows the fixed payload layout is reaching the real `SCMD` transport behavior.
    Local proof:
    - `amw0-logs/wmbc-trace-20260405-211618.log`

20. In the corrected `SCMD` trace with command `0x00`:
    - send time was about `78 ms`, comparable to corrected `WKBC`
    - `WQAC0` became `0x0`, matching `SA00 = 0x00`
    - `CMDL` changed from the previous `0x1E` to `0x00`
    - `LDAT`, `HDAT`, and `CMDH` stayed latched at their previous `WKBC` values
    This matches AML: `SCMD` updates `CMDL` and uses a different flag path instead of repopulating all four transport bytes.

21. During corrected `WKBC` and `SCMD` execution, `FLAGS = 132` was observed.
    With the current AML bit mapping, that means:
    - `BFLG = 1`
    - `DRDY = 1`
    The expected `WFLG` / `CFLG` transitions were not captured yet, so their precise timing is still unclear.

22. `LDAT` / `HDAT` / `CMDL` / `CMDH` remained latched after the call in the observed traces.
    That means the EC-facing command bytes persist long enough to inspect after a successful `WKBC` send.

## Invalid Or Not Yet Trustworthy

1. Old `WKBC` / `SCMD` send logs produced before the payload-layout fix are not valid evidence for hardware semantics.
   They exercised the wrong bytes in `AC00`.

2. The first `monitor-trace` runs at `-i 0.005` are invalid.
   `ec_probe monitor` rejected that interval with `value too small`, so those runs did not actually capture EC changes.

3. The old direct-write EC probe on `0x49` / `0x4c` is now historical only.
   It showed values being overwritten by firmware, but AML says those bytes are not the real fan-control path.

## Observed Timing Facts

1. `WMBC(..., 0x04, ...)` calls completed in roughly `4-6 ms` in the previous traces.
   That is why shell-based repeated `ec_probe read` polling was too slow to catch transient EC transport bytes.

2. After the payload-layout fix, corrected `WKBC` traces completed in roughly `77-78 ms`.
   That is consistent with actually entering the AML `WKBC` path rather than returning immediately through the wrong layout.

## What Still Needs Proof

1. Verify whether a non-zero `SCMD` command produces the same "CMDL-only" transport pattern.

2. Verify whether `ECON` is actually `One` on this Linux setup when `AMW0` methods are called.

3. Observe the `ECMG` fan fields during real mode changes, hotkeys, or load changes now that an AML-backed read path is available.

## Pointers

- `WQBA-analysis.md`
- `AMW0-analysis.md`
- `WQBA.mof`
- `amw0-event-path-test.sh`
- `amw0-wkbc-scan.sh`
- `amw0-scmd-scan.sh`
- `amw0-wmbc-pack.sh`
- `amw0-wmbc-trace.sh`
- `amw0-monitor-trace.sh`
- `amw0-ecmg-read.sh`

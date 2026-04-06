# WQBA / BMOF notes

`WQBA.bin` is a standard compressed ACPI/WMI BMF blob.

The blob itself is treated as a local analysis artifact and is not required for
the Git-tracked repository layout. The decoded text remains in `WQBA.mof`.

- Header bytes are `46 4f 4d 42 01 00 00 00`, which matches the `FOMB` + version 1 format used by `bmfdec`.
- The next two little-endian dwords are the compressed size without the 16-byte header (`0x0A83`) and the decompressed size (`0x49F0`).
- Decompression with upstream `bmf2mof` succeeds with no patching. The decoded text is stored in `WQBA.mof`.

## What it contains

The MOF is not a vendor fan-control schema. It is the old Microsoft/ACPI WMI test schema:

- `AcpiTest_QSPackage`
- `AcpiTest_QSString`
- `AcpiTest_QULong`
- `AcpiTest_MPackage`
- `AcpiTest_MString`
- `AcpiTest_MULong`
- `AcpiTest_EventPackage`
- `AcpiTest_EventString`
- `AcpiTest_EventULong`

Those classes use the GUID range `{ABBC0F6A-8EA1-11D1-00A0-C90629100000}` through `{ABBC0F72-8EA1-11D1-00A0-C90629100000}` and describe generic package/string/ulong query, set, method, and event plumbing.

## How it connects to AMW0

In `acpi-scan/20260405-131022/dsdt.dsl`, `Device (AMW0)` exposes a `_WDG` buffer that maps those GUIDs to the AML methods:

- `AA` -> `{ABBC0F6A-8EA1-11D1-00A0-C90629100000}` flags `0x01`
- `AB` -> `{ABBC0F6B-8EA1-11D1-00A0-C90629100000}` flags `0x05`
- `AC` -> `{ABBC0F6C-8EA1-11D1-00A0-C90629100000}` flags `0x01`
- `BA` -> `{ABBC0F6D-8EA1-11D1-00A0-C90629100000}` flags `0x02`
- `BB` -> `{ABBC0F6E-8EA1-11D1-00A0-C90629100000}` flags `0x06`
- `BC` -> `{ABBC0F6F-8EA1-11D1-00A0-C90629100000}` flags `0x02`
- `D0` -> `{ABBC0F70-8EA1-11D1-00A0-C90629100000}` flags `0x08`
- `D1` -> `{ABBC0F71-8EA1-11D1-00A0-C90629100000}` flags `0x0C`
- `D2` -> `{ABBC0F72-8EA1-11D1-00A0-C90629100000}` flags `0x08`
- `BA` -> `{05901221-D566-11D1-B2F0-00A0C9062910}` flags `0x00`

The last GUID is the standard BMOF GUID. That is why Windows can fetch the embedded MOF blob through the same device even though `WQBA` appears only as a static AML `Name (...)` object.

## Practical conclusion

`WQBA` is useful as confirmation that the firmware exposes a normal ACPI WMI device, but it does not describe any real fan-control semantics.

The interesting logic is in AML itself:

- `WMBA` / `WMBB` / `WMBC` are generic WMI dispatchers.
- `WMBC(..., 0x04, buffer)` feeds `OEMG`.
- `SETC(0, Arg1)` feeds `OEMF`.
- `OEMG` / `OEMF` then call `WKBC`, `RKBC`, or `SCMD`, which are the real EC-facing paths.

One more useful consequence from the AML:

- `WQAC(0)` returns `SAC0`, the first dword of `AC00`.
- `WQAC(1)` returns `SAC1`, the second dword of `AC00`.
- For `WMBC(..., 0x04, buffer)`, AML copies the caller buffer into `AC00`, runs `OEMG(AC00)`, then clears `SAC1` and `SAC2`.
- `WKBC(SA00, SA01, SA02, SA03)` uses bytes `0..3` of `AC00`.
- `OEMG` routing uses `SAC1`, i.e. bytes `4..7` of `AC00`.

So `WQAC0` reflects the four command bytes, while `WQAC1` carries the route dword before AML clears it. `WQAC1` is still weak telemetry after send, but this byte ordering matters for any real `WMBC(..., 0x04, ...)` payload.

So `WQBA` can be considered solved and deprioritized. Further reverse engineering effort should stay focused on the `WKBC` and `SCMD` command space, not on the BMOF blob.

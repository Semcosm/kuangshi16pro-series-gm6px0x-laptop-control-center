# UEFI_Firmware Reverse Report

## Artifact

- installed paths:
  - `C:\Program Files\OEM\机械革命电竞控制台\DefaultTool\UEFI_Firmware.dll`
  - `C:\Program Files\OEM\机械革命电竞控制台\UniwillService\UEFI_Firmware.dll`
  - `C:\Program Files\OEM\机械革命电竞控制台\UniwillService\MyControlCenter\UEFI_Firmware.dll`
- file type:
  native `PE32+` x86-64 DLL

The two copies under `UniwillService` are byte-identical.

## Exports

All installed copies export only:

- `ReadUefi`
- `WriteUefi`

They do not export:

- `ReadSwitchUefi`
- `WriteSwitchUefi`

That mismatch matters because installed `GCUService.exe` still declares the switch
entry points via `DllImport`.

## Imports

The DLL imports:

- `GetFirmwareEnvironmentVariableW`
- `SetFirmwareEnvironmentVariableW`
- `OpenProcessToken`
- `LookupPrivilegeValueW`
- `AdjustTokenPrivileges`

So it is a direct firmware-variable bridge.

## Variable Identity

The `.rdata` section exposes the target variable directly:

- variable name: `UniWillVariable`
- vendor GUID: `{9f33f85c-13ca-4fd1-9c4a-96217722c593}`
- privilege string: `SeSystemEnvironmentPrivilege`

## Read/Write Shape

`ReadUefi(startAddr, endAddr)`:

- enables firmware-variable privilege first
- reads the whole firmware variable
- uses an internal `0x200`-byte working buffer
- returns a pointer into that buffer

`WriteUefi(startAddr, endAddr, buffer)`:

- enables privilege first
- reads the full `0x200`-byte variable into a local buffer
- patches the requested `[startAddr..endAddr]` range
- writes the whole `0x200`-byte blob back with `SetFirmwareEnvironmentVariableW`
- then re-reads it for verification/logging

So this is a whole-variable read/modify/write path over a fixed-size `0x200`-byte blob.

## Bridge-Side Field Mapping

`GCUBridge.exe` carries another very useful clue:
its internal `NvramVariableVClass.NvramVariable` implementation is not stubbed.

That bridge-side code confirms the main UEFI path is field-oriented over the same
`NVRAM_STRUCT`, not a separate opaque serializer.

Confirmed writable byte fields:

- `PowerMode`
- `MemoryOverClockSwitch`
- `ApExistFlag`
- `OverClockRecoveryFlag`
- `ACRecoveryStatus`
- `ApUseFlag`
- `OemDisplayMode`
- `FnKeyStatus`

Confirmed writable `ushort` fields:

- `ICpuCoreVoltageValue`
- `ICpuCoreVoltageOffsetValue`
- `ICpuCoreVoltageOffsetNegativeValue`
- `ICpuTauValue`

Confirmed writable `uint` fields:

- `ACpuFreqValue`
- `ACpuVoltageValue`

Confirmed writable byte-array fields:

- `RGBKeyboard08`
- `RGBKeyboard1A`
- `SmartLightbar08`
- `SmartLightbar1A`

There is also a dedicated helper:

- `SetFwVars_RGBLightbar(mode, r, g, b)`

The write flow is explicit:

1. `ReadUefi(0, NVRAM_STRUCT_SIZE)`
2. patch named fields in `NVRAM_STRUCT`
3. marshal the whole struct back into bytes
4. `WriteUefi(0, NVRAM_STRUCT_SIZE, array)`

This further supports the conclusion that the installed `UniWillVariable` path is
the main settings blob, not a sparse per-setting variable family.

## Current Limitation

The installed exports currently prove only the main UEFI blob path through
`UniWillVariable`.

They do not yet prove a separate installed switch-variable backend for
dGPU direct-connect, even though the managed service still declares
`ReadSwitchUefi` / `WriteSwitchUefi`.

Installed managed metadata also strengthens that caution:

- the visible `NVRAM_STRUCT` fields cover board ID, keyboard / lightbar state,
  battery limits, AC recovery, OC values, display mode, and Fn-key state
- that main struct does not expose an obvious dGPU / MUX / direct-connect field

So the current best interpretation is:

1. `UniWillVariable` is the main platform settings blob
2. direct-connect switching likely lives in a separate variable or separate backend
3. `ReadSwitchUefi` / `WriteSwitchUefi` remain unresolved until a real provider is found

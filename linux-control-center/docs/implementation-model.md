# Implementation Model

This note records the current Linux-facing model distilled from `GCUService`.

## Service Split

The first Linux implementation should be split into:

- `backend-amw0`
  Owns `WMBC` / `WKBC` / `SCMD` packing and hardware writes.
- `profile-service`
  Owns operating mode, profile selection, fan-table choice, and power-limit orchestration.
- `state-reader`
  Owns temperatures, fan RPM, current mode, and error-state reads.
- `ui`
  Thin client only; it should not own hardware policy.

## Windows Model To Mirror

`MainOption` is the global state object:

- `OperatingMode`
- `GamingProfileIndex`
- `OfficeProfileIndex`
- `TurboProfileIndex`
- `CustomProfileIndex`
- `FanBoostEnable`
- `FanSafetyProtectNotify`
- `DefaultNotify`

`ModeProfile` is the per-profile object:

- `CPU`
- `GPU`
- `FAN`
- `MEM`

Linux should mirror this shape instead of inventing a different top-level config layout.

## Key Subobjects

`CPU` includes:

- `PL1`, `PL2`, `PL4`
- `PL1_dc`, `PL2_dc`
- `PL1_S2`, `PL2_S2`
- `TccOffset`

`GPU` includes:

- `ConfigurableTGPTarget`
- `DynamicBoost`
- `DynamicBoostTotalProcessingPowerTarget`
- `WhisperMode*`

`FanSettings` includes:

- `TableName`
- `FanSwitchSpeedEnabled`
- `FanSwitchSpeed`
- `SkipSafetyAbnormalProtection`

`FanTable1p5` includes:

- `Activated`
- `Name`
- `FanControlRespective`
- `CPU[16]`
- `GPU[16]`

Each fan-table entry is:

- `ID`
- `UpT`
- `DownT`
- `Duty`

## Static Windows Config Sources

The unpacked Windows control center already ships profile templates that Linux can reuse.

- Static templates live under:
  `windows-control-center/extracted/ControlCenter_5.17.51.33_Mechrevo/UserFanTables/<platform>/`
- File names follow:
  `M1T1.json` .. `M2T3.json`
- Template payloads include:
  - `PL1`, `PL2`, `PL1_dc`, `PL2_dc`
  - `TCC`
  - optional `CTGP`, `DB`, `WM`
  - `CpuTemp_DefaultMaxLevel`, `GpuTemp_DefaultMaxLevel`
  - `CPU[16]`, `GPU[16]`

This means the first Linux implementation should load Windows-shaped JSON templates rather
than inventing a custom profile schema first.

Current interpretation:

- `T1..T3` likely represent profile levels inside a mode family.
- `M1/M2` likely represent two different mode families.
- These static names still do not map directly to `0x7AB` on `GM6PX0X`,
  because live reads kept `0x7AB = 0x00` across multiple mode switches.

Registry is also useful as a default-value source:

- `HKLM\\SOFTWARE\\OEM\\GamingCenter2\\MyFan3`
  stores `FanMode`, `OfficeMode`, `PowerSettingMode`, `TurboModeLevel`
- `...\\OfficeModeAdvL1..L4`
  store the 5-step office advanced PWM presets
- `HKLM\\SOFTWARE\\OEM\\GamingCenter2\\SMAPCTable`
  stores `PL1`, `PL2`, `PL4`, `TccOffset`, `DefaultTGP`, `DynamicBoost`

## Package Layer Boundary

The packaged Windows app is split more clearly than it first looked.
Mounted Windows installation paths show that the deploy boundary is:

- `C:\Program Files\WindowsApps\ControlCenter3_5.17.51.18_x64__...\`
  packaged UI resources + `GamingCenter3_Cross.dll` + `Win32/SystrayComponent.exe`
- `C:\Program Files\OEM\机械革命电竞控制台\UniwillService\`
  installed local service and MQTT bridge
- `C:\Program Files\OEM\机械革命电竞控制台\UniwillService\MyControlCenter\`
  native hardware bridge DLLs and `GCUService.exe`

Within that split:

- `GamingCenter3_Cross.dll`
  native XAML / ViewModel front-end layer
- `Win32/SystrayComponent.exe`
  tray + local MQTT helper
- `GCUBridge.exe`
  local MQTT service wrapper on port `13688`
- `GCUService.exe`
  service / business logic / hardware orchestration
- `ACPIDriverDll.dll`
  direct EC / IO / PCI / CMOS / `SMAPCTable` bridge
- `UEFI_Firmware.dll`
  firmware-variable bridge

Current static evidence says `GamingCenter3_Cross.dll` is not the final hardware backend:

- it is native, not managed
- it imports WinRT and socket APIs
- it exposes UI/view-model strings for modes, dGPU, fan settings, OC settings, battery, and display
- it contains local topic strings such as `Fan/Status`, `Customize/SupportInfo`,
  `Languages/Control`, `BatteryProtection/Control`, and `Display/Control`
- it does not visibly expose `WMBC`, `WQAC`, `ECRR`, `ACPIDriverDll`, or `WMI*ECRAM`

Installed-package evidence also resolves one earlier ambiguity:

- `AppxManifest.xml` depends on `Microsoft.NET.Native.Framework.2.2`
  and `Microsoft.NET.Native.Runtime.2.2`
- the imported `SharedLibrary.dll` comes from that Microsoft runtime dependency,
  not from an OEM bridge DLL

For Linux this is important:

- `GamingCenter3_Cross.dll` is useful for names, topic surfaces, and view-model fields
- the real backend is not inside the UWP package
- `GCUService.exe`, `ACPIDriverDll.dll`, and `UEFI_Firmware.dll`
  are the critical path for final write semantics
- the installed `MyControlCenter` directory itself also carries the real runtime
  JSON profiles and PowerShell helpers, not just binaries

What the installed native bridges already prove:

- `ACPIDriverDll.dll` exports:
  `ReadEC`, `WriteEC`, `ReadIO`, `WriteIO`, `ReadIndexIO`, `WriteIndexIO`,
  `ReadMEMB`, `WriteMEMB`, `ReadPCI`, `WritePCI`, `ReadCMOS`, `WriteCMOS`,
  and `SMAPCTable`
- `UEFI_Firmware.dll` exports:
  `ReadUefi`, `WriteUefi`
- installed `GCUService.exe` directly `DllImport`s:
  - `SMAPCTable` from `ACPIDriverDll.dll`
  - `ReadUefi`, `WriteUefi`, `ReadSwitchUefi`, `WriteSwitchUefi`
    from `UEFI_Firmware.dll`
- installed `GCUService.exe` still exposes strings for:
  `WMIReadECRAM`, `WMIWriteECRAM`, `WMIReadBiosRAM`, `WMIWriteBiosRAM`
- `AcpiCtrl` also exposes:
  - `IOCTL_GPD_ACPI_CUSTOMCTL = 0x9C40A504`
  - `SMRW_CMD_READ = 0xBB`
  - `SMRW_CMD_WRITE = 0xAA`

So the Windows service likely multiplexes at least three backend families:

1. direct low-level native access via `ACPIDriverDll.dll`
2. WMI-based EC / BIOS RAM access
3. UEFI / NVRAM variable access

One important caveat from the installed artifacts:

- all three installed `UEFI_Firmware.dll` copies export only
  `ReadUefi` and `WriteUefi`
- the managed service still declares `ReadSwitchUefi` and `WriteSwitchUefi`

So `SwitchUefi` is currently a service-layer clue, not a confirmed installed export.
Treat direct-connect / MUX support as unresolved until the actual switch backend is
located.

More importantly, `ACPIDriverDll.dll` is no longer just "a DLL with suggestive export names":

- it opens `\\.\ACPIDriver`
- it uses `CreateFileW -> DeviceIoControl -> CloseHandle`
- the exported operations map to a contiguous IOCTL family such as:
  - `ReadCMOS` = `0x9C40A480`
  - `WriteCMOS` = `0x9C40A484`
  - `ReadEC` = `0x9C40A488`
  - `WriteEC` = `0x9C40A48C`
  - `ReadMEMB` = `0x9C40A490`
  - `WriteMEMB` = `0x9C40A498`
  - `ReadPCI` = `0x9C40A4A0`
  - `WritePCI` = `0x9C40A4A4`
  - `ReadIO` = `0x9C40A4C0`
  - `WriteIO` = `0x9C40A4C4`
  - `ReadIndexIO` = `0x9C40A4C8`
  - `WriteIndexIO` = `0x9C40A4CC`
  - `TempRead1..3` = `0x9C40A4D0..0x9C40A4D8`
  - `TempWrite1..3` = `0x9C40A4DC..0x9C40A4E4`
  - `SMAPCTable` = `0x9C40A500`
  - `CustomCtl` = `0x9C40A504`

`UWACPIDriver.inf` also binds the kernel driver to `ACPI\\INOU0000`.
That lines up with the Linux-side `\_SB.INOU.*` namespace already observed on this machine.

`SMAPCTABLE_STRUCT` is also concrete rather than opaque.
It contains:

- `PL1`
- `PL2`
- `PL4`
- `TccOffset`
- `ConfigurableTGP`
- `DynamicBoost`
- `TargetTemperature`
- `DefaultTGP`
- `DynamicBoostCpuTdp`

`SmartApcTableCtrl` reads it through:

- `GetTableByDriver()`
- `GetTableByWMI()`
- `GetTableByDLL()`

and mutates it via:

- `WriteSMAPCTable(string varname, byte value)`

For Linux this strongly suggests a first-class `smart_apc_table` model
instead of scattering PL/TGP/TCC values across unrelated code paths.

The native transport layer is also more concrete now:

- `ACPIDriverDll.dll` is only a user-mode wrapper over `\\.\ACPIDriver`;
  the real transport lives in `UWACPIDriver.sys`.
- The byte EC path is explicit rather than guessed:
  - `ReadEC` / `ReadCMOS` place `addr` at request-buffer offset `0`
  - `WriteEC` / `WriteCMOS` place `addr` at `0` and `value` at `4`
  - input and output share the same large staging buffer
- `SMAPCTable` is a 128-byte structured transaction, not a sequence of byte writes.
- `UWACPIDriver.sys` forwards the OEM IOCTL family into ACPI method evaluation
  on the `INOU` device. Confirmed mappings are:
  - `ReadEC` -> `ECRR`
  - `WriteEC` -> `ECRW`
  - `SMAPCTable` -> `SMRW`
- `CustomCtl` is a generic ACPI trampoline rather than a single command:
  - bytes `0..3` are a 4-character ACPI method name
  - bytes `4..` are the method payload / arguments

For Linux this means the backend should keep:

- a structured `smart_apc_table` type
- a generic `acpi_eval_method(name, payload)` abstraction for future native paths
- `backend-amw0` as one transport option, not the only architectural model

The UEFI side is also clearer:

- installed `UEFI_Firmware.dll` exports only `ReadUefi` / `WriteUefi`
- those calls operate on a fixed `0x200`-byte firmware blob named
  `UniWillVariable`
- vendor GUID is `{9f33f85c-13ca-4fd1-9c4a-96217722c593}`
- writes are whole-blob read/modify/write operations

One caveat remains:

- installed artifacts still do not prove a separate `SwitchUefi` backend
- `ReadSwitchUefi` / `WriteSwitchUefi` are therefore still only service-layer clues
- dGPU direct-connect / MUX final-write semantics remain unresolved

## Message Bus Boundary

The Windows stack is also clearly split at the local message-bus layer:

- `GCUBridge.exe`
  runs the local MQTT broker on `localhost:13688`
- `SystrayComponent.exe`
  is a lightweight tray client
- `GCUService.exe`
  remains the main business-logic and hardware-orchestration process

The tray client does not talk to EC / ACPI directly.
Its mode clicks publish only high-level business commands on `Fan/Control`:

- `OPERATING_OFFICE_MODE`
- `OPERATING_GAMING_MODE`
- `OPERATING_TURBO_MODE`
- `OPERATING_CUSTOM_MODE`

It also initializes itself by requesting:

- `Customize/SupportControl` -> `GETSUPPORT`
- `Fan/Control` -> `GETSTATUS`

For Linux this matters because the future UI should mirror the same layering:

- UI emits high-level mode / profile commands
- a local service owns policy and hardware sequencing
- the hardware backend stays below that service boundary

## Installed Runtime Profiles

The installed directory contains real runtime JSON, not just package samples:

- `UserPofiles/MainOption.json`
- `UserPofiles/Mode1_Profile*.json`
- `UserPofiles/Mode2_Profile*.json`
- `UserPofiles/Mode3_Profile*.json`
- `UserPofiles/Mode4_Profile*.json`
- `UserFanTables/M1T1..M4T5.json`

`MainOption.json` mirrors the managed object model directly:

- `GamingProfileIndex`
- `OfficeProfileIndex`
- `TurboProfileIndex`
- `CustomProfileIndex`
- `OperatingMode`
- `FanBoostEnable`
- `WhisperMode*`

`GCU2_Define.OperatingMode` also fixes the global enum:

- office = `0`
- gaming = `1`
- turbo = `2`
- custom = `3`
- benchmark = `4`

The shipped profile families are highly useful Linux seed data:

- `Mode1_Profile1/2`: `PL1/PL2 = 75`, `TCC = 5`, `CTGP = 125`, `DB = 25`
- `Mode2_Profile1/2`: `PL1/PL2 = 45`, `TCC = 20`, `CTGP = 125`, `DB = 25`
- `Mode3_Profile1/2`: `PL1/PL2 = 130`, `TCC = 5`, `CTGP = 150`, `DB = 25`
- `Mode4_Profile1..5`: `M4T1..M4T5`, with `Mode4_Profile1` standing out as
  `PL1/PL2 = 130` and `FanSwitchSpeed = 500`

`UserFanTables/M4T1.json` is also the only family seed seen so far with:

- `"Activated": true`
- `"FanControlRespective": true`

That makes `Mode4` / `M4T*` the strongest current candidate for the
custom-user family. The likely family mapping is:

1. `Mode1` -> gaming
2. `Mode2` -> office or battery-saver-like
3. `Mode3` -> turbo
4. `Mode4` -> custom

This mapping is still an inference from shipped defaults and should be kept
explicitly labeled as such.

For implementation, Linux should ingest these JSON defaults as source data
instead of rebuilding OEM presets by hand.

## dGPU Device Switch vs Direct-Connect Switch

Installed `Command/enableDGpu.ps1` and `Command/disableDGpu.ps1` only do:

- `Enable-PnpDevice ... *NVIDIA* -Class Display`
- `Disable-PnpDevice ... *NVIDIA* -Class Display`

`GCService5.GPUDeviceItem` hardcodes the same PowerShell command strings.
At the same time, the service keeps separate state names:

- `sDGpu_Status`
- `sDGpuDirectConnectionSwitch_Status`
- `sDGpuDirectConnectionSwitch_Support`

For Linux this means:

1. do not model NVIDIA device enable/disable as the MUX/direct-connect path
2. keep a separate conceptual layer for "GPU device power saving" vs
   "display routing / direct-connect"
3. treat `SwitchUefi` as the leading direct-connect candidate, but not yet
   a proven backend

## Confirmed Fan Call Chain

Current IL-derived call edges already confirm this split:

- `MyFanManager_RamFan1p5.SetFanTableThread(...)`
  delegates to `MyFanTableCtrl.SetFanTable(string)`
- `MyFanTableCtrl`
  owns both `FanTable_Manager1p5` and `MyEcCtrl`
- `FanTable_Manager1p5`
  owns `SetEcFanTable`, `SetEcFanTable_Cpu`, and `SetEcFanTable_Gpu`

This means Linux should treat fan application as a staged operation:

1. service-layer orchestration
2. fan-control/custom-mode enable path
3. CPU table writes
4. GPU table writes

Do not collapse this into a single flat "write 96 bytes" abstraction.

## Proven EC Constants

From `Define.ECSpec` / `Define.RamFan1p5_ECSpec`:

- fan mode numbers:
  - gaming = `0`
  - office = `1`
  - turbo = `2`
  - custom = `3`
- key EC bytes:
  - fan-control bitfield = `0x751`
    AML names only `TBME` at bit 4 and `UFME` at bit 7 there.
    Do not model this as a simple `0/1/2/3` mode byte.
  - `PL1` = `0x783`
  - `PL2` = `0x784`
  - `PL4` = `0x785`
  - `TccOffset` = `0x786`
  - `FanSwitchSpeed` = `0x787`
  - mode/profile index bytes = `0x7AB`, `0x7B0`, `0x7B1`, `0x7B2`
  - helper control byte = `0x7C7`
- fan table layout:
  - CPU `UpT` base = `0xF00`
  - CPU `DownT` base = `0xF10`
  - CPU `Duty` base = `0xF20`
  - GPU `UpT` base = `0xF30`
  - GPU `DownT` base = `0xF40`
  - GPU `Duty` base = `0xF50`

Current working interpretation:

- `SetModeSwitchChangeThread` is probably at least a two-part operation:
  update mode/profile bytes around `0x7AB` and then update control bits at `0x751`.
- `turbo` and `custom` likely map to `TBME` / `UFME`.
- `office` vs `gaming` likely differs outside `0x751`, because AML does not expose two separate mode bits there.
- `SetEcFanTable_Cpu/Gpu` looks more like direct EC-RAM address programming over `0xF00..0xF5F`
  than a single opaque fan-mode opcode.
- fan / PL / TCC work is more likely to land on the
  `ACPIDriverDll.dll` or `WMI*ECRAM` side than on the UEFI side.
- persistent switch-like features such as dGPU direct-connect may still cross into
  the `UEFI_Firmware.dll` path, especially when a reboot is involved.

Observed Linux snapshot on this machine:

- `0x751 = 0x10`
  This matches `TBME` and strongly supports a turbo-like current state.
- `0x7AB = 0x00`
  This does not mirror the turbo bit directly, so `0x7AB` is not a simple mode byte.
- `0x7C7 = 0x0C`
  With the AML field layout, that decodes to `LCSE = 0` and `OCPL = 3`.
- `0x783..0x787 = 0x00`
  Current power-limit bytes are not populated in this Linux snapshot.
- `0xF00..0xF5F = 0x00`
  Current fan-table RAM is also not populated in this Linux snapshot.

Observed OEM preset raw values on this machine:

- `0x751 = 0x00`
- `0x751 = 0x10`
- `0x751 = 0x20`
- `0x751 = 0x30`
- `0x751 = 0xA0`

This tightens the current model:

- `0x10` still matches `TBME` and turbo-like state.
- `0xA0` matches the shipped constant `User_Fan_HiMode = 0xA0`.
- That strongly suggests `0x20` is the missing `HiMode` helper bit.
- `0x30` then becomes `HiMode + TBME`.
- Therefore Linux must not model `0x751` as only `{turbo bit, custom bit}`.
- For now, treat `0x20` as the OEM preset family/helper bit behind balanced or related presets.

## Feature Status

Ready for first Linux implementation:

- operating mode state model
- fan-table data model
- power-limit state model
- AMW0-backed send path

Partially understood:

- dGPU direct connection
- final `SetEcFanTable*` write packet format
- support/status discovery bits

Lower priority for the first usable Linux version:

- display feature and color calibration
- RGB keyboard and light bar
- battery protection UI
- liquid cooling

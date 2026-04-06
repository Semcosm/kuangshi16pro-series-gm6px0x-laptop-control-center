# GamingCenter3_Cross Reverse Report

## Artifact

- path: `windows-control-center/msix-x64/GamingCenter3_Cross.dll`
- file type: native `PE32+` DLL
- timestamp: `2025-12-03 16:57:48`
- `ilspycmd` cannot load it as a managed assembly because it has no CLR metadata

This already separates it from `GCUService.exe` and `SystrayComponent.exe`:
`GamingCenter3_Cross.dll` is a native packaged component, not a normal .NET service DLL.

## Import Profile

`objdump -p` shows these notable imports:

- `mrt100_app.dll`
- `api-ms-win-core-winrt-l1-1-0.dll`
- `api-ms-win-core-winrt-robuffer-l1-1-0.dll`
- `api-ms-win-core-winrt-string-l1-1-0.dll`
- `ws2_32.dll`
- `advapi32.dll`
- `kernel32.dll`
- `SharedLibrary.dll` with `CreateCommandLine`

This import set looks like a packaged WinRT / XAML application compiled to native code,
not a low-level EC or ACPI bridge.

## Resolved SharedLibrary Dependency

After checking the mounted Windows `C:` drive, `SharedLibrary.dll` is no longer a mystery:

- `ControlCenter3_5.17.51.18_x64__h329z55cwnj8g/AppxManifest.xml`
  depends on:
  - `Microsoft.NET.Native.Framework.2.2`
  - `Microsoft.NET.Native.Runtime.2.2`
- the actual DLL exists under:
  - `C:\Program Files\WindowsApps\microsoft.net.native.framework.2.2_*\SharedLibrary.dll`

So the `SharedLibrary.dll` import is .NET Native runtime glue,
not a vendor-specific hardware bridge.

## Strong UI Evidence

Strings show a large XAML / ViewModel surface:

- `Windows.UI.Xaml.*`
- `MainPage`
- `DisplayModeView`
- `Fan_SettingsView`
- `OverClock_SettingsView`
- `DgpuSwitchView`
- `LiquidCooling_SettingsView`
- `Mode_SelectingsView`

The data model exposed by strings matches the control-center UI layer:

- `GamingProfileIndex`, `OfficeProfileIndex`, `TurboProfileIndex`, `CustomProfileIndex`
- `CpuPL1`, `CpuPL2`, `CpuPL4`, `CpuTccOffset`
- `GpuConfigurableTGPTarget`, `GpuDynamicBoost`, `GpuWhisperMode*`
- `DGpuDirectConnectionSwitch`
- `FanSwitchSpeed`, `FanSwitchSpeedEnabled`

## Local Service / Topic Layer

The DLL also contains MQTT and topic strings:

- `MQTTDatsService`
- `Fan/Status`
- `Customize/SupportInfo`
- `Languages/Control`
- `Languages/Info`
- `BatteryProtection/Control`
- `Display/Control`

This fits the package split already seen elsewhere:

1. `GamingCenter3_Cross.dll`
   native UI / ViewModel layer
2. `SystrayComponent.exe`
   tray + local MQTT helper
3. `GCUService.exe`
   service / business logic / hardware orchestration

## What Is Not Present

Current static evidence does **not** show the final hardware bridge here.

Missing from visible imports / strings:

- `WMBC`, `WQAC`, `ECRR`, `ECMG`
- `ACPIDriverDll`
- `WMIReadECRAM`, `WMIWriteECRAM`
- `SetupAPI` / `CfgMgr32` / obvious EC-driver glue

So `GamingCenter3_Cross.dll` looks like a high-level client, not the final writer of EC bytes.

## Installed Service Boundary

Mounted Windows installation paths make the split much clearer:

- packaged app/UI:
  - `C:\Program Files\WindowsApps\ControlCenter3_5.17.51.18_x64__h329z55cwnj8g\`
- installed service/backend:
  - `C:\Program Files\OEM\机械革命电竞控制台\UniwillService\`
  - `C:\Program Files\OEM\机械革命电竞控制台\UniwillService\MyControlCenter\`

The OEM service path contains the real native bridge components that are absent from the package:

- `ACPIDriverDll.dll`
  exports `ReadEC`, `WriteEC`, `ReadIO`, `WriteIO`, `ReadPCI`, `WritePCI`,
  `ReadMEMB`, `WriteMEMB`, `ReadCMOS`, `WriteCMOS`, `ReadIndexIO`, `WriteIndexIO`,
  and `SMAPCTable`
- `UEFI_Firmware.dll`
  exports `ReadUefi` and `WriteUefi`
- `GCUService.exe`
  directly `DllImport`s `SMAPCTable` from `ACPIDriverDll.dll`
  and `ReadUefi` / `WriteUefi` from `UEFI_Firmware.dll`
- `GCUBridge.exe`
  is the local MQTT service wrapper on port `13688`, not the final EC writer

## Current Conclusion

`GamingCenter3_Cross.dll` is best understood as a native AOT UI shell that:

- owns pages, view-models, and feature switches
- speaks local packaged / MQTT-style control topics
- depends on the Microsoft .NET Native `SharedLibrary.dll` runtime

The real bottom path for `mode -> EC/ACPI/WMI write` still appears to live outside this DLL,
in the installed OEM service path:

- `GCUService.exe`
- `ACPIDriverDll.dll`
- `UEFI_Firmware.dll`

## Next Targets

- reverse `ACPIDriverDll.dll` exports and correlate them with `MyEcCtrl` / `WMIEC`
- separate which features go through EC / IO / PCI / `SMAPCTable`
- separate which features go through `UEFI_Firmware.dll` and persistent firmware variables
- keep treating `GamingCenter3_Cross.dll` as UI/protocol evidence, not as the final hardware backend

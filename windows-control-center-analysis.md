# Windows Control Center Analysis (2026-04-05)

This note records the static findings from the Mechrevo Control Center package:

- source archive:
  `windows-control-center/ControlCenter_5.17.51.33_Mechrevo.zip`
- extracted installer tree:
  `windows-control-center/extracted/ControlCenter_5.17.51.33_Mechrevo/`
- extracted UWP bundle:
  `windows-control-center/msix-x64/`
- extracted symbols:
  `windows-control-center/appxsym/GamingCenter3_Cross.pdb`

## Confirmed Package Layout

1. The package is split into three layers:
   - an outer installer:
     `ControlCenter_5.17.51.33_Mechrevo.exe`
   - a UWP bundle:
     `PreinstallKit/GamingCenter3_Cross.UWP_5.17.51.33_x64.msixbundle`
   - a symbol bundle:
     `PreinstallKit/GamingCenter3_Cross.UWP_5.17.51.33_x64.appxsym`

2. The extracted UWP package contains:
   - `GamingCenter3_Cross.exe` / `GamingCenter3_Cross.dll`
   - a full-trust helper:
     `Win32/SystrayComponent.exe`
   - support DLLs:
     `Win32/M2Mqtt.Net.dll`, `Win32/Newtonsoft.Json.dll`
   - branding/config:
     `appsettings.json`, `AppxManifest.xml`

3. `AppxManifest.xml` confirms the UWP app delegates background or privileged work to
   `Win32/SystrayComponent.exe` through:
   - `windows.appService` name `SystrayExtensionService`
   - `windows.fullTrustProcess`
   - `windows.startupTask`

4. `appsettings.json` and `setup.ini` confirm this package is a Mechrevo-branded
   customization with `CustomizeTarget = 17`.

## Confirmed Fan-Control Surface

1. The app clearly models two independent internal fans.
   Symbol and string evidence shows:
   - `CpuFanSpeed`
   - `GpuFanSpeed`
   - `CpuFanDuty`
   - `GpuFanDuty`
   - `FanViewModel`
   - `Fan_SettingsView`

2. The app has explicit fan-control verbs and mode names, including:
   - `GET_FAN_SPEED_CURVE_SETTING`
   - `SET_FAN_SPEED_CURVE_SETTING`
   - `RESTORE_FAN_SPEED_CURVE_SETTING`
   - `RESTORE_FAN_SPEED_CURVE_SETTING_ALL`
   - `SET_FAN_CONTROL_RESPECTIVE`
   - `FAN_BOOST_ON`
   - `FAN_BOOST_OFF`
   - `FAN_GAMING_MODE`
   - `FAN_TURBO_MODE`
   - `FAN_OFFICE_MODE`
   - `OPERATING_GAMING_MODE`
   - `OPERATING_OFFICE_MODE`
   - `OPERATING_TURBO_MODE`
   - `OPERATING_CUSTOM_MODE`

3. The UWP app exposes a dedicated fan message path over its internal transport:
   - `Fan/Control`
   - `Fan/Status`
   - `System/FanInfo`
   - `System/FanErrorInfo`

4. The app also exposes a support-discovery path:
   - `Customize/SupportControl`
   - `Customize/SupportInfo`
   This likely tells the UI whether fan curve editing, boost mode, liquid cooling,
   and model-specific features should be enabled.

## Confirmed Internal Architecture

1. The UWP app and helper use an MQTT-style local message bus.
   Evidence:
   - `Win32/M2Mqtt.Net.dll` is shipped with the package
   - both the UWP DLL and `SystrayComponent.exe` contain
     `UWP_Refactor.DataService.MQTTDatsService`
   - both contain `GetMQTT_Topics`
   - both reference `MQTTDatsService`

2. The helper is not just a tray icon.
   It is the likely bridge between the UI and the real hardware or vendor service.
   `SystrayComponent.exe` contains:
   - `EC_Control`
   - `EC_Status`
   - `FAN_FanSwitchSpeed`
   - `FAN_FanSwitchSpeedEnabled`
   - `FAN_SafetyProtect`
   - `FAN_SafetyProtectNotify`

3. The UWP side appears to be .NET Native compiled, while the helper is a normal
   managed .NET assembly.
   That makes static symbol discovery easier on the helper side, but the actual
   hardware write path may still be implemented in another preinstalled service
   or vendor component not included in this bundle.

## Confirmed User Fan Table Format

1. The installer ships a large model-specific table set under:
   `UserFanTables/<model>/M{1,2}T{1,2,3}.json`

2. Relevant model families present in this package include:
   - `PH6AQxx`
   - `PH6ARxx`
   - `PH6PG0x`
   - `PH6PG0x150W`
   - `PH6PG3x`
   - `PH6PG3x150W`
   - `PH6PG7x`
   - `PH6PG7x150W`
   - `PH6PGEx`
   - `PH6PRxx`
   - `PH6TRX1`

3. A fan-table JSON contains:
   - power-policy fields:
     `PL1`, `PL2`, `PL1_dc`, `PL2_dc`, `TCC`, `CTGP`, `DB`, `WM`
   - two separate 16-entry curves:
     `CPU[]`
     `GPU[]`

4. Each curve entry contains:
   - `ID`
   - `UpT`
   - `DownT`
   - `Duty`

5. Example from `UserFanTables/PH6PGEx/M1T1.json`:
   - `PL1 = 45`
   - `PL2 = 65`
   - `TCC = -5`
   - CPU curve and GPU curve each have 16 slots
   - valid slots are followed by `255/255/255` padding

6. Example from `UserFanTables/PH6PGEx/M2T1.json`:
   - `PL1 = 15`
   - `PL2 = 15`
   - noticeably lower duty targets
   - still separate CPU and GPU curves

7. This confirms the Windows control center is not using a single shared fan curve.
   It has a real two-fan abstraction with per-mode CPU and GPU duty schedules.

## Relation To This Linux Reverse Engineering Work

1. Our current Linux-side work already proved the low-level transport:
   - `WMBC(..., 0x04, buffer)` reaches `WKBC`, `RKBC`, `SCMD`
   - `WKBC` writes `LDAT`, `HDAT`, `CMDL`, `CMDH`

2. The Windows package now proves the missing upper layer:
   - vendor-defined dual-fan modes exist
   - vendor-defined dual-fan duty curves exist
   - the Control Center can read and write those settings

3. What is still missing is the final bridge between:
   - high-level objects like `SET_FAN_SPEED_CURVE_SETTING`
   - and the real hardware write path
   Whether that bridge ultimately uses ACPI WMI, EC commands, IOCTLs, or a separate
   vendor service is not yet proven from this package alone.

4. No direct `AMW0`, `WMBC`, or `DeviceIoControl` string was recovered from the
   extracted app and helper binaries during this pass.
   That does not rule them out:
   - the hardware access code may be in another preinstalled service or driver
   - the relevant code may be stripped or hidden behind generated .NET Native stubs
   - the outer installer may fetch or register additional components at install time

## Machine-Mapping Clue

1. The current Linux machine identifies itself as:
   - product name: `Kuangshi16Pro Series GM6PX0X`
   - board name: `GM6PX0X`

2. The shipped fan-table directory names do not directly contain `GM6PX0X`.
   They use another model-family naming scheme such as `PH6PGEx` and `PH6PG7x`.

3. Therefore one of the next Windows-side tasks is to determine which `PH6*`
   family the vendor software maps `GM6PX0X` onto.

## Highest-Value Next Steps

1. On Windows, capture local message traffic while changing:
   - fan mode
   - fan boost
   - custom fan curve
   - CPU fan and GPU fan sliders, if present

2. Identify the actual local broker or app-service transport details:
   - process that owns the broker
   - TCP port or named transport
   - topic payload format for `Fan/Control`

3. On Windows, watch file and registry access while the fan page opens:
   - which `UserFanTables/<model>/...` file is read
   - whether `GM6PX0X` is translated to `PH6PGEx`, `PH6PG7x`, or another family

4. Find the final privileged hop:
   - a vendor service
   - a kernel driver
   - an ACPI or WMI bridge
   - or a helper executable launched outside this package

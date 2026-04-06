# Call Paths

Current high-confidence Windows call-path model:

1. `GamingCenter3_Cross.dll`
   packaged UI / view-model layer
2. `SystrayComponent.exe`
   local tray client and lightweight topic publisher
3. `GCUBridge.exe`
   MQTT broker and process keeper
4. `GCUService.exe`
   mode/profile/fan/power orchestration
5. `ACPIDriverDll.dll` / `UEFI_Firmware.dll`
   native bridge layer
6. `UWACPIDriver.sys`
   kernel driver that dispatches to `INOU` ACPI methods

Linux should mirror the semantics of steps 4 through 6, not the process model
of steps 1 through 3.

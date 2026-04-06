# Reverse Findings

Implementation-relevant facts already strong enough to code against:

- `GCUService.exe` is the Windows-side business-logic center
- `SetFanTableThread` delegates into `MyFanTableCtrl::SetFanTable`
- fan programming is staged and writes CPU/GPU table blocks separately
- power limits map cleanly onto EC bytes around `0x783..0x786`
- mode switching likely involves `0x751`, `0x7AB`, `0x7B0..0x7B2`, and `0x7C7`
- `ACPIDriverDll.dll` and `UWACPIDriver.sys` confirm the Windows stack is
  really talking to `INOU` ACPI methods

This note is intentionally shorter than the main reverse reports. It exists to
highlight only the facts that should shape current code.

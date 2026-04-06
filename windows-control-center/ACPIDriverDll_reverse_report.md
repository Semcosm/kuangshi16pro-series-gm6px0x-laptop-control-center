# ACPIDriverDll Reverse Report

## Artifact

- installed path:
  `C:\Program Files\OEM\机械革命电竞控制台\UniwillService\MyControlCenter\ACPIDriverDll.dll`
- file type:
  native `PE32+` x86-64 DLL
- related kernel driver:
  `C:\Program Files\OEM\机械革命电竞控制台\UWACPIDriver\UWACPIDriver.sys`
- driver binding from `UWACPIDriver.inf`:
  `ACPI\INOU0000`

This lines up with the Linux-side `\_SB.INOU.*` namespace already observed on the target machine.

## High-Level Model

`ACPIDriverDll.dll` is a user-mode wrapper over a device path:

- `\\.\ACPIDriver`

Each exported operation follows the same shape:

1. open `\\.\ACPIDriver` with `CreateFileW`
2. populate a global request buffer
3. issue `DeviceIoControl`
4. close the handle

So this DLL is not doing the hardware access itself.
It is a thin protocol client for the `UWACPIDriver.sys` kernel driver.

## Managed-Side Constants

Installed `GCUService.exe` exposes the same protocol in `MyECIO.AcpiCtrl`:

- `IOCTL_GPD_ACPI_CMREAD   = 0x9C40A480`
- `IOCTL_GPD_ACPI_CMWRITE  = 0x9C40A484`
- `IOCTL_GPD_ACPI_ECREAD   = 0x9C40A488`
- `IOCTL_GPD_ACPI_ECWRITE  = 0x9C40A48C`
- `IOCTL_GPD_ACPI_MMREADB  = 0x9C40A490`
- `IOCTL_GPD_ACPI_MMREADD  = 0x9C40A494`
- `IOCTL_GPD_ACPI_MMWRITEB = 0x9C40A498`
- `IOCTL_GPD_ACPI_MMWRITED = 0x9C40A49C`
- `IOCTL_GPD_ACPI_PEREAD   = 0x9C40A4A0`
- `IOCTL_GPD_ACPI_PEWRITE  = 0x9C40A4A4`
- `IOCTL_GPD_ACPI_IOREAD   = 0x9C40A4C0`
- `IOCTL_GPD_ACPI_IOWRITE  = 0x9C40A4C4`
- `IOCTL_GPD_ACPI_INIOREAD = 0x9C40A4C8`
- `IOCTL_GPD_ACPI_INIOWRITE= 0x9C40A4CC`
- `IOCTL_GPD_ACPI_TMPREAD1 = 0x9C40A4D0`
- `IOCTL_GPD_ACPI_TMPREAD2 = 0x9C40A4D4`
- `IOCTL_GPD_ACPI_TMPREAD3 = 0x9C40A4D8`
- `IOCTL_GPD_ACPI_TMPWRITE1= 0x9C40A4DC`
- `IOCTL_GPD_ACPI_TMPWRITE2= 0x9C40A4E0`
- `IOCTL_GPD_ACPI_TMPWRITE3= 0x9C40A4E4`
- `IOCTL_GPD_ACPI_SMAPCTABLE = 0x9C40A500`
- `IOCTL_GPD_ACPI_CUSTOMCTL  = 0x9C40A504`

`AcpiCtrl` also defines:

- `SMRW_CMD_READ  = 0xBB`
- `SMRW_CMD_WRITE = 0xAA`

That strongly suggests `CustomRW(string methodName, int input, ref byte data)`
is a generic custom-control path on top of `IOCTL_GPD_ACPI_CUSTOMCTL`.

## Export Mapping

The export table maps directly onto that IOCTL family:

- `ReadCMOS` -> `0x9C40A480`
- `WriteCMOS` -> `0x9C40A484`
- `ReadEC` -> `0x9C40A488`
- `WriteEC` -> `0x9C40A48C`
- `ReadMEMB` -> `0x9C40A490`
- `WriteMEMB` -> `0x9C40A498`
- `ReadPCI` -> `0x9C40A4A0`
- `WritePCI` -> `0x9C40A4A4`
- `ReadIO` -> `0x9C40A4C0`
- `WriteIO` -> `0x9C40A4C4`
- `ReadIndexIO` -> `0x9C40A4C8`
- `WriteIndexIO` -> `0x9C40A4CC`
- `TempRead1/2/3` -> `0x9C40A4D0/0x9C40A4D4/0x9C40A4D8`
- `TempWrite1/2/3` -> `0x9C40A4DC/0x9C40A4E0/0x9C40A4E4`
- `SMAPCTable` -> `0x9C40A500`

Read-style exports store one input value in the request buffer and return the first byte.
Write-style exports store `addr + value` into the request buffer before issuing the IOCTL.

`SMAPCTable` is wider:

- helper code copies `0x80` bytes from the caller-provided pointer into the request buffer
- the driver call uses `0x9C40A500`
- the function returns a pointer back into the shared result buffer

So `SMAPCTable` is not a byte-at-a-time primitive.
It is a structured table transaction over a fixed 128-byte payload.

The simple byte-oriented exports also reveal the concrete request layout:

- `ReadCMOS` / `ReadEC` place `addr` at request-buffer offset `0`
- `WriteCMOS` / `WriteEC` place `addr` at `0` and `value` at `4`
- both input and output point at the same large shared buffer
- the wrapper always issues `DeviceIoControl(..., in=buf, out=buf, size=0x400000)`

So the EC byte path is no longer opaque.
At the protocol level it is effectively:

1. `addr`
2. optional `value`
3. fixed IOCTL
4. read the first returned byte back from offset `0`

## Installed Driver Dispatch

Looking one layer down into `UWACPIDriver.sys`, the kernel driver dispatches the same family
that the native DLL and managed constants describe:

- `0x9C40A480` / `0x9C40A484`
- `0x9C40A488` / `0x9C40A48C`
- `0x9C40A490` / `0x9C40A498`
- `0x9C40A4A0` / `0x9C40A4A4`
- `0x9C40A4C0` / `0x9C40A4C4`
- `0x9C40A4C8` / `0x9C40A4CC`
- `0x9C40A4D0..0x9C40A4E4`
- `0x9C40A500`
- `0x9C40A504`

This confirms the Windows stack is internally consistent:

- managed constants in `AcpiCtrl`
- user-mode wrappers in `ACPIDriverDll.dll`
- kernel dispatch in `UWACPIDriver.sys`

all describe the same real contract.

## ACPI Method Mapping Inside The Driver

The strongest new result is that the kernel driver forwards these OEM IOCTLs into
named ACPI method evaluations.

Confirmed mappings:

- `ReadCMOS` -> ACPI method `SMCR`
- `WriteCMOS` -> ACPI method `SMCW`
- `ReadEC` -> ACPI method `ECRR`
- `WriteEC` -> ACPI method `ECRW`
- `MMREADB` -> ACPI method `BRMM`
- `MMREADD` -> ACPI method `DRMM`
- `MMWRITEB` -> ACPI method `BWMM`
- `MMWRITED` -> ACPI method `DWMM`
- `PEREAD` -> ACPI method `DRCP`
- `PEWRITE` -> ACPI method `DWCP`
- `IOREAD` -> ACPI method `DROI`
- `IOWRITE` -> ACPI method `DWOI`
- `INIOREAD` -> ACPI method `POIR`
- `INIOWRITE` -> ACPI method `POIW`
- `TMPREAD1/2/3` -> `DR1T` / `DR2T` / `DR3T`
- `TMPWRITE1/2/3` -> `RW1T` / `RW2T` / `RW3T`
- `SMAPCTable` -> ACPI method `SMRW`

Those method names are present directly in the driver strings:

- `SMCR`
- `SMCW`
- `ECRR`
- `ECRW`
- `BRMM`
- `DRMM`
- `BWMM`
- `DWMM`
- `DRCP`
- `DWCP`
- `DROI`
- `DWOI`
- `POIR`
- `POIW`
- `DR1T`
- `DR2T`
- `DR3T`
- `RW1T`
- `RW2T`
- `RW3T`
- `SMRW`

The helper code also constructs what looks like standard ACPI method-evaluation request
structures and uses the control code `0x32c004`.

One useful nuance from the dispatch table:

- not every IOCTL in the family is exposed as a public `ACPIDriverDll.dll` export
- `MMREADD = 0x9C40A494` and `MMWRITED = 0x9C40A49C` still exist in the managed
  constants and the kernel-driver dispatch
- so the driver protocol surface is slightly broader than the exported DLL surface

Inference:

- `0x32c004` is very likely the standard Windows ACPI method-eval IOCTL
- `UWACPIDriver.sys` acts as a translator from OEM user-mode IOCTLs
  to underlying ACPI method calls on the `INOU` device

## CustomCtl Input Format

`IOCTL_GPD_ACPI_CUSTOMCTL = 0x9C40A504` is now much clearer.

Its dispatch path first splits the caller buffer into:

1. the first 4 bytes
2. the remaining payload

The first 4 bytes are reassembled into a 4-character ACPI method name.
So `CustomCtl` is best modeled as:

- `input[0..3]` = method name
- `input[4..]` = method payload / arguments
- output is returned through the caller-provided output buffer

This lines up cleanly with:

- `AcpiCtrl.CustomRW(string methodName, int input, ref byte data)`

and means Linux should keep a generic `acpi_eval_method(name, payload)` abstraction,
not just hard-code a few special cases.

## Smart APC Table

`SMAPCTABLE_STRUCT` in `GCUService.exe` is only nine bytes wide:

- `PL1`
- `PL2`
- `PL4`
- `TccOffset`
- `ConfigurableTGP`
- `DynamicBoost`
- `TargetTemperature`
- `DefaultTGP`
- `DynamicBoostCpuTdp`

`SmartApcTableCtrl` has multiple read paths:

- `GetTableByDriver()`
- `GetTableByWMI()`
- `GetTableByDLL()`

and writes by field name via:

- `WriteSMAPCTable(string varname, byte value)`

This is one of the clearest service-level models available for Linux replication.

## Service-Side Semantic Wrapper

Installed `GCUService.exe` wraps `AcpiCtrl` with `MyECIO.MyEcCtrl`.
Even though many method bodies are still obfuscated, the exposed method surface is already useful.

Examples include:

- `Read(string ReadName, ushort Addr, ref byte Data)`
- `Write(string ReadName, ushort Addr, byte Data)`
- `ReadPciDword(byte Bus, byte Dev, byte Func, uint Offset, ref uint pData)`
- `SetHWOC(string AcpiName, int SetValue, ref byte outputdata)`
- `GetProjectIdFromEC()`
- `GetSystemIdFromEC()`
- `GetAdapterWattFromEC()`
- `GetTouchPadLedStatusFromEC()`
- `GetTurboModeSupport()`
- `GetThermalProtectStatusFromEC()`
- `GetGPUCoreFreqFromEC()`
- `GetGPUMemFreqFromEC()`
- `SetCustomModetoEC(bool CMode)`

That tells us the Linux backend should not expose only raw EC reads and writes.
It should eventually grow a semantic service layer over the transport,
mirroring the responsibilities currently concentrated in `MyEcCtrl`.

## Linux Implication

For Linux, the important conclusion is not "reimplement Windows IOCTLs".

The useful abstraction is:

1. semantic EC / power / SMAPC operations from `GCUService`
2. backed by a low-level transport tied to the `INOU` ACPI device

On Windows that transport is `UWACPIDriver.sys + \\.\ACPIDriver`.
On Linux the closest current path is still the observed `\_SB.INOU.*` ACPI entry set.

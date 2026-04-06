# GCUService.exe 初步静态逆向报告

- 文件: `GCUService.exe`
- SHA256: `e2095ea378382e00f910c3bb9c031990b6002f262f932f838f9f7be80ea767c8`
- 类型: PE32+ x64 .NET/CLR 程序
- 构建时间: 2025-08-08 06:54:16 UTC
- PDB 路径: `E:\WorkSpace\CCU_49ver\MyControlCenter\MyControlCenter\obj\x64\Release\GCUService.pdb`

## 核心判断

1. 这不是简单的托盘辅助程序，而是 **服务端/业务核心**。
2. 命名基本未混淆，包含大量完整 namespace / type / method 名；但部分方法体带有明显的反编译干扰，仍然需要结合 IL、常量表和数据模型一起看。
3. 通信层同时出现 **MQTT** 与 **WCF** 线索，说明它既有本地服务/回调接口，也有内部消息总线。
4. 硬件抽象层明确出现 **ACPIDriverDll.dll**、`ReadACPI` / `WriteACPI`、`WMIReadECRAM` / `WMIWriteECRAM`、`IOCTL_GPD_ACPI_ECREAD` / `ECWRITE` 等字符串，说明它极大概率通过 OEM ACPI/EC 驱动打硬件。
5. 风扇/性能逻辑完整暴露：`SetFanTableThread`、`SetEcFanTable`、`SetOperatingModeProfileIndexThread`、`SetModeSwitchChangeThread`、`SetTurboMode`、`SetOverBoostMode`、`GPU_ConfigurableTGPTarget`、`GPU_DynamicBoostTotalProcessingPowerTarget`、`CPU_PL1/PL2/PL4` 等。
6. 除风扇外还负责电池保护、UEFI/NVRAM、RGB 键盘、液冷、飞行模式、Fn 热键、应用绑定等。

## 能直接看出的模块结构

### 服务/系统层
- `GCUService.WCFService`
- `GCUService.MySystem`
- `GCUService.MySetting`
- `GCUService.MyFan.Overclocking`
- `GCUService.MyFan.BTCooling`
- `GCUService.KeyMapping`
- `GCUService.GamingMonitor`

### 共享业务层（和前端共用）
- `MyControlCenter.MyFanManager_*`
- `MyControlCenter.MySystemManager_*`
- `MyControlCenter.MySettingManager_*`
- `MyControlCenter.PowerOptionAPI`
- `MyControlCenter.MqttClientCtrl`
- `MyControlCenter.OSDManager`
- `MyControlCenter.TrayCtrl`
- `MyControlCenter.TrayView`

这说明 `GCUService.exe` 很可能直接复用和前端同一套核心业务库，而不是纯粹独立实现。

## 通信架构线索

### MQTT
出现了大量 M2Mqtt 相关符号：
- `uPLibrary.Networking.M2Mqtt`
- `MqttClient`
- `Client_MqttMsgPublishReceived`
- `PublishTopic`
- `M_MQTTService_ClientMessage`
- `M_MQTTService_ClientConnection`

这说明服务内部存在较明显的消息总线设计，很多功能模块是靠 topic / publish / subscribe 串起来的。

### WCF
同时又有：
- `GCUService.WCFService`
- `IWCFService`
- `WCFServiceHost`
- `CallbackContract#GCUService.WCFService.IDataCallback:`

ILSpy 已经能直接确认 `IWCFService` 的公开接口只有两条：
- `Task StartSendingToClient(string data)`
- `void SendToServer(string Topic, string Command)`

这说明对 UI 暴露的本地控制面非常薄，实际业务语义很可能都塞在 `Topic + Command` 以及 MQTT payload 里。

所以它不是单一路径；比较像：
- 对外/对 UI：本地服务接口（WCF / broker）
- 对内模块：MQTT 风格消息总线

## 硬件控制线索

### ACPI / EC
最关键的字符串：
- `ACPIDriverDll.dll`
- `IOCTL_GPD_ACPI_ECREAD`
- `IOCTL_GPD_ACPI_ECWRITE`
- `ReadACPI`
- `WriteACPI`
- `CheckAcpiDriverDeviceExists`

### WMI
还同时有：
- `WMIReadECRAM`
- `WMIWriteECRAM`
- `GetTableByWMI`
- `StartWMIReceiveEvent`
- `WMIHandleEvent`

这意味着它大概率不是只走一种硬件后端，而是根据平台/机型在 **ACPI 驱动路径** 和 **WMI 路径** 间切换，或者两者兼用。

### 已确认的底层封装类

`MyECIO.AcpiCtrl` 已确认封装了：
- `DeviceIoControl`
- `CheckAcpiDriverDeviceExists`
- `Read(string ReadName, ushort Addr, ref byte Data)`
- `Write(string ReadName, ushort Addr, byte Data)`
- `ReadACPI(...)`
- `WriteACPI(...)`

`MyControlCenter.WMIEC` 已确认封装了：
- `StartWMIReceiveEvent`
- `WMIHandleEvent`
- `WMIReadECRAM(ulong Addr, ref object data)`
- `WMIWriteECRAM(ulong Addr, ulong Value)`
- `Smrw(...)`
- `GetSetULong2(...)`

`MyECIO.MyEcCtrl` 则是更高一层的 EC 能力抽象，向上暴露：
- `Read` / `Write`
- `GetProjectIdFromEC`
- `GetTurboModeSupport`
- `IsSuportRamFan1p5`
- `SetCustomModetoEC`
- `Set_APExistToEC`

## 风扇/性能模式线索

### 模式切换
- `SetOperatingModeProfileIndex`
- `SetOperatingModeProfileIndexThread`
- `SetModeSwitchChange`
- `SetModeSwitchChangeThread`
- `SetTurboMode`
- `GetTurboModeSupport`
- `SetOverBoostMode`

### 风扇表
- `SetFanTable`
- `SetFanTableThread`
- `SetEcFanTable`
- `SetEcFanTable_Cpu`
- `SetEcFanTable_Gpu`
- `LoadFanTableFromJson`
- `WriteFanTableToJson`
- `RestoreDefaultFanTable`
- `DefaultFanTable_Gaming`
- `DefaultFanTable_Office`
- `DefaultFanTable_Turbo`

### 可调功耗/性能参数
- `CPU_PL1`
- `CPU_PL2`
- `CPU_PL4`
- `CPU_TccOffset`
- `GPU_ConfigurableTGPTarget`
- `GPU_DynamicBoostSwitch`
- `GPU_DynamicBoostTotalProcessingPowerTarget`
- `SetNVPStateThread`

这已经非常像你要找的“真正工作原理”：UI 不是直接控硬件，而是通过服务把 **profile / operating mode / fan table / power limits** 下发到底层驱动或固件。

## ILSpy 深挖结果（2026-04-06）

### 已确认的配置对象模型

`MainOption` 是全局状态入口，至少包含：
- `OperatingMode`
- `GamingProfileIndex`
- `OfficeProfileIndex`
- `TurboProfileIndex`
- `CustomProfileIndex`
- `FanBoostEnable`
- `FanSafetyProtectNotify`
- `DefaultNotify`

`ModeProfile` 是单个模式配置对象，结构已经很清楚：
- `Activated`
- `Name`
- `CustomizeName`
- `OverClockingEnabled`
- `CPU`
- `GPU`
- `FAN`
- `MEM`

这直接说明 Linux 侧可以照着做一套相同分层，而不是自己重新发明配置格式。

### CPU / GPU / Fan 子对象

`CPU` 对象已确认包含：
- `PL1` / `PL2` / `PL4`
- `PL1_dc` / `PL2_dc`
- `PL1_S2` / `PL2_S2`
- `OffsetCoreVoltage`
- `TccOffset`
- `AmdSPL` / `AmdSPPT` / `AmdFPPT`
- `AmdCoreFreq` / `AmdCoreVoltage`

`GPU` 对象已确认包含：
- `CoreClockOffset`
- `MemoryClockOffset`
- `TargetTemperature`
- `ConfigurableTGPSwitch` / `ConfigurableTGPTarget`
- `DynamicBoostSwitch`
- `DynamicBoost`
- `DynamicBoostTotalProcessingPowerTarget`
- `WhisperMode*`

`FanSettings` 已确认包含：
- `TableName`
- `FanSwitchSpeedEnabled`
- `FanSwitchSpeed`
- `SkipSafetyAbnormalProtection`

### 风扇表数据结构

`FanTable1p5` 已确认包含：
- `Activated`
- `Name`
- `FanControlRespective`
- `CpuTemp_DefaultMaxLevel`
- `GpuTemp_DefaultMaxLevel`
- `CPU[]`
- `GPU[]`

每个表项是 `FanTable1p5Buffer`：
- `ID`
- `UpT`
- `DownT`
- `Duty`

这和包内 JSON 的双风扇 16 段结构完全对上了。

### 解包静态配置面

Windows 控制中心安装包里已经带了可直接利用的静态模板，不只是 UI 资源：

- 机型 fan/profile 模板在：
  `extracted/ControlCenter_5.17.51.33_Mechrevo/UserFanTables/<platform>/`
- 每个模板文件名形如：
  `M1T1.json`、`M1T2.json`、`M1T3.json`、`M2T1.json`、`M2T2.json`、`M2T3.json`
- 文件内容不只是 fan curve，还包含：
  - `PL1` / `PL2`
  - `PL1_dc` / `PL2_dc`
  - `TCC`
  - 可选的 `CTGP` / `DB` / `WM`
  - `CpuTemp_DefaultMaxLevel`
  - `GpuTemp_DefaultMaxLevel`
  - `CPU[16]` / `GPU[16]`

这说明 Linux 侧可以直接把这些 JSON 当成第一版 profile 数据源，而不是先发明自己的配置格式。

静态模板的差异也很有信息量：

- 以 `PH6PRxx` 为例：
  - `M1T1..M1T3` 是较高功耗组，`PL1=35/45/45`
  - `M2T1..M2T3` 是较低功耗组，`PL1=15/30/45`
  - 这组没有 `CTGP/DB/WM`
- 以 `PH6PG3x` 为例：
  - `M1T1..M1T3` 还带 `CTGP`、`DB`，例如 `CTGP=35/45/45`、`DB=7`
  - `M2T1..M2T3` 降到 `PL1=15` / `PL2=15`，并把 `CTGP/DB` 置成 `NA`

当前最稳的解释是：

1. `T1..T3` 很像同一模式族里的 profile 档位
2. `M1/M2` 很像两组不同模式族，而不是单纯的 profile 序号
3. 但在这台 `GM6PX0X` 上，它们还不能直接映射到 `0x7AB`
   因为实机观测里 `0x7AB` 在多次切换后仍保持 `0x00`

除了静态 JSON，registry 也有运行时默认值：

- `HKLM\SOFTWARE\OEM\GamingCenter2\MyFan3`
  保存 `FanMode`、`OfficeMode`、`PowerSettingMode`、`TurboModeLevel`
- `...\MyFan3\OfficeModeAdvL1..L4`
  保存 5 档 office advanced PWM
- `HKLM\SOFTWARE\OEM\GamingCenter2\SMAPCTable`
  保存 `PL1=130`、`PL2=130`、`PL4=200`、`TccOffset=5`
  以及 `DefaultTGP=125`、`DynamicBoost=25`

### 包内前端分层补充

继续看 `ControlCenter_5.17.51.33` 安装包后，现在可以把包内前端层再切清楚一层：

- `GCUService.exe`
  仍然是服务 / 模式 / 风扇 / 功耗核心
- `msix-x64/Win32/SystrayComponent.exe`
  是 .NET tray + 本地 MQTT helper
- `msix-x64/GamingCenter3_Cross.dll`
  是 native 的 XAML / ViewModel 前端层

`GamingCenter3_Cross.dll` 的关键证据：

- 它是 native `PE32+` DLL，没有 CLR metadata
- import 里有：
  - `mrt100_app.dll`
  - `api-ms-win-core-winrt-*`
  - `ws2_32.dll`
  - `SharedLibrary.dll`
- strings 里有大量：
  - `Windows.UI.Xaml.*`
  - `DisplayModeView`
  - `Fan_SettingsView`
  - `OverClock_SettingsView`
  - `DgpuSwitchView`
  - `LiquidCooling_SettingsView`
  - `Mode_SelectingsView`
- 同时它也带了本地 topic / data-service 痕迹：
  - `MQTTDatsService`
  - `Fan/Status`
  - `Customize/SupportInfo`
  - `Languages/Control`
  - `BatteryProtection/Control`
  - `Display/Control`

这说明 `GamingCenter3_Cross.dll` 更像：

1. NativeAOT 打包后的 UI / ViewModel 壳
2. 通过本地 topic / MQTT 与 service 层通信
3. 不像最终执行 `EC/ACPI/WMI` 写入的底层桥

同样关键的是，目前在这个包快照里**没有**看到明显的底层桥接 DLL：

- 没看到 `ACPIDriverDll.dll`
- 没看到 `UEFI_Firmware.dll`
- 没看到明显的 `WMI*ECRAM` companion DLL

因此，`GamingCenter3_Cross.dll` 本身不是 Linux 复刻的主战场；
它更像给我们补“上层对象模型、topic 名、功能面和文案映射”的证据。

详细记录见：
`windows-control-center/GamingCenter3_Cross_reverse_report.md`
以及：
`windows-control-center/ACPIDriverDll_reverse_report.md`

### 已安装 Windows 真部署层（2026-04-06）

这次直接看挂载出来的 `C:` 盘后，部署边界已经可以再压实一层。

真实硬件后端不在 `WindowsApps/ControlCenter3_*` 包里，而在：

- `C:\Program Files\OEM\机械革命电竞控制台\UniwillService\MyControlCenter\`

这个目录里至少有：

- `GCUService.exe`
- `ACPIDriverDll.dll`
- `UEFI_Firmware.dll`
- `NVControlSetting.dll`
- `IntelOverclockingSDK.dll`
- `RegistryUtils.dll`

其中另外两块也可以先定性：

- `NVControlSetting.dll`
  是一个很小的 native DLL，只导出 `InitNV` 和 `SetNVCtrlPanel`
  托管侧 `NVControlPanel` 枚举只有 `AUTOSELECT` / `HIGHPERFORMANE`
  更像 NVIDIA 控制面板 / GPU policy helper，不像 MUX / EC 最终写口
- `IntelOverclockingSDK.dll`
  是托管 `.NET` 程序集，内容明显带 Intel XTU profile/schema
  以及 `XTUService` / CLI 痕迹，说明部分 Intel CPU OC 能力可能走 XTU SDK /
  service，而不是完全靠 ACPI / EC

其中最关键的是两个 native bridge：

- `ACPIDriverDll.dll`
  导出：
  - `ReadEC` / `WriteEC`
  - `ReadIO` / `WriteIO`
  - `ReadIndexIO` / `WriteIndexIO`
  - `ReadMEMB` / `WriteMEMB`
  - `ReadPCI` / `WritePCI`
  - `ReadCMOS` / `WriteCMOS`
  - `SMAPCTable`
- `UEFI_Firmware.dll`
  导出：
  - `ReadUefi`
  - `WriteUefi`

`UEFI_Firmware.dll` 的 import 也直接坐实了它的职责：

- `GetFirmwareEnvironmentVariableW`
- `SetFirmwareEnvironmentVariableW`
- `LookupPrivilegeValueW`
- `AdjustTokenPrivileges`
- `OpenProcessToken`

也就是说，这个 DLL 不是普通配置文件 helper，而是明确的
UEFI / NVRAM 变量读写桥。

`ACPIDriverDll.dll` 这边也已经不只是“看到了导出名”。
它的 unicode strings 和反汇编都说明：

- 直接打开设备路径：
  `\\.\ACPIDriver`
- 用 `CreateFileW -> DeviceIoControl -> CloseHandle`
  这一固定模式执行所有导出操作
- 各导出基本就是“准备一个小 buffer，再发一个固定 IOCTL”

目前已经直接看到的 IOCTL 号包括：

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
- `TempRead1` -> `0x9C40A4D0`
- `TempRead2` -> `0x9C40A4D4`
- `TempRead3` -> `0x9C40A4D8`
- `TempWrite1` -> `0x9C40A4DC`
- `TempWrite2` -> `0x9C40A4E0`
- `TempWrite3` -> `0x9C40A4E4`
- `SMAPCTable` -> `0x9C40A500`
- `CustomCtl` -> `0x9C40A504`

安装版 `GCUService.exe` 的 `MyECIO.AcpiCtrl` 还直接带着完整协议常量：

- `IOCTL_GPD_ACPI_CMREAD`
- `IOCTL_GPD_ACPI_CMWRITE`
- `IOCTL_GPD_ACPI_ECREAD`
- `IOCTL_GPD_ACPI_ECWRITE`
- `IOCTL_GPD_ACPI_MMREADB`
- `IOCTL_GPD_ACPI_MMWRITEB`
- `IOCTL_GPD_ACPI_PEREAD`
- `IOCTL_GPD_ACPI_PEWRITE`
- `IOCTL_GPD_ACPI_IOREAD`
- `IOCTL_GPD_ACPI_IOWRITE`
- `IOCTL_GPD_ACPI_INIOREAD`
- `IOCTL_GPD_ACPI_INIOWRITE`
- `IOCTL_GPD_ACPI_TMPREAD1..3`
- `IOCTL_GPD_ACPI_TMPWRITE1..3`
- `IOCTL_GPD_ACPI_SMAPCTABLE`
- `IOCTL_GPD_ACPI_CUSTOMCTL`

以及两条自定义读写命令值：

- `SMRW_CMD_READ = 0xBB`
- `SMRW_CMD_WRITE = 0xAA`

这使得 `AcpiCtrl.CustomRW(string methodName, int input, ref byte data)`
很像一个复用 `IOCTL_GPD_ACPI_CUSTOMCTL` 的通用扩展口。

也就是说，`SMAPCTable` 并不是完全独立的“魔法口子”，
它看起来只是同一套 `ACPIDriver` 驱动协议里的一个 opcode。

更关键的是，驱动 INF 也和 Linux 侧现象对上了：

- `UWACPIDriver.inf` 安装 `UWACPIDriver.sys`
- 绑定硬件 ID：
  `ACPI\INOU0000`

这和 Linux 里已经实际读到的 `\_SB.INOU.ECRR`
非常吻合，说明 Windows 内核驱动和 Linux 侧当前观察到的
`INOU` ACPI 设备大概率就是同一族入口。

安装版 `GCUService.exe` 里也能直接看到托管层绑定：

- `[DllImport("ACPIDriverDll.dll")] SMAPCTable(IntPtr)`
- `[DllImport("UEFI_Firmware.dll")] ReadUefi(...)`
- `[DllImport("UEFI_Firmware.dll")] WriteUefi(...)`
- `[DllImport("UEFI_Firmware.dll")] ReadSwitchUefi(...)`
- `[DllImport("UEFI_Firmware.dll")] WriteSwitchUefi(...)`

同时 strings 里还能看到：

- `WMIReadECRAM`
- `WMIWriteECRAM`
- `WMIReadBiosRAM`
- `WMIWriteBiosRAM`
- `WriteECRAM`
- `WriteSMAPCTableRegistryAll`

这说明 `GCUService` 不是单一路径后端，而是至少同时掌握三组硬件写入面：

1. `ACPIDriverDll.dll` 暴露的 EC / IO / PCI / CMOS / `SMAPCTable`
2. `WMI*ECRAM` / `WMI*BiosRAM`
3. `UEFI_Firmware.dll` 暴露的 UEFI / NVRAM 变量

进一步看安装版 `WMIEC` / `WMIBIOS`，WMI 侧也已经有比较完整的 payload 形状：

- `WMIEC.Smrw(ulong Cmd, ulong Value, ref object RetData)`
- `WMIEC.GetSetULong2(ulong Cmd, ref object RetData)`
- `WMIEC.GetSetULong2(ulong Cmd, ulong Addr, ulong Offset, ulong Value, ref object RetData)`

并且它自己也定义了和 `AcpiCtrl` 一致的命令常量：

- `SMRW_CMD_READ = 0xBB`
- `SMRW_CMD_WRITE = 0xAA`
- `SMRW_CMD_OFFSET = 0`
- `SMRW_VALUE_OFFSET = 8`

以及 Smart APC table 专用的第二套布局：

- `GETSETULONG2_ADDR_OFFSET = 0`
- `GETSETULONG2_VALUE_OFFSET = 32`
- `GETSETULONG2_OFFSET_OFFSET = 40`
- `GETSETULONG2_CMD_OFFSET = 56`
- `GETSETULONG2_CMD_READ_SMART_APC_TABLE = 12`
- `GETSETULONG2_CMD_WRITE_SMART_APC_TABLE = 13`

这说明一个很关键的结构性事实：

- `driver` 路径和 `WMI` 路径虽然 transport 不同
- 但服务层在更高一层复用了同样的“命令 + 值 / 表结构”语义
- `SMRW` / `GetSetULong2` 很像这套统一内部协议的两种封包

UEFI / NVRAM 这边也新实了一层。
安装版里 `NVRAM_STRUCT` 已经能直接看到字段：

- `OemBoardSsid`
- `SupportByte`
- `ProjectID`
- `KeyboardType`
- `CustomerList`
- `PowerMode`
- `BatteryLimitation`
- `ChargeMaximumLimit`
- `ChargeMinimumLimit`
- `MemoryOverClockSwitch`
- `ICpuCoreVoltageOffsetValue/Maximum/Minimum`
- `ACpuFreqValue*`
- `ACpuVoltageValue*`
- `OverClockRecoveryFlag`
- `ApExistFlag`
- `ACRecoverySupport`
- `ACRecoveryStatus`
- `MemoryOverClockSupport`
- `ApUseFlag`
- `OemDisplayMode`
- `FnKeyStatus`
- `AudioCodecID`
- `PowerPhase`

但这里**没有**看到明确的 dGPU / MUX / direct-connect 字段。

同时安装版又单独导入了：

- `ReadSwitchUefi`
- `WriteSwitchUefi`

并且 dGPU 侧常量也更完整了：

- `DGgpuDirectConnectionMode.AMD_MSHybrid = 0`
- `DGgpuDirectConnectionMode.AMD_dGPU_only = 1`
- `DGgpuDirectConnectionMode.Intel_dGPU_only = 2`
- `DGgpuDirectConnectionMode.Intel_Dynamic = 4`

所以当前更稳的判断是：

1. 主 `NVRAM_STRUCT` 更偏向板级配置、超频、AC recovery、显示/键盘状态
2. dGPU 直连更可能走单独的 `SwitchUefi` 区，而不是主 `_fwvars` 结构体
3. `SetDGpuModeWithOverBoost` 很可能会同时碰 GPU mode 和某个独立的 switch UEFI 变量

这里又有一个很值得记下来的安装版细节：

- `C:\Program Files\OEM\机械革命电竞控制台\DefaultTool\UEFI_Firmware.dll`
- `C:\Program Files\OEM\机械革命电竞控制台\UniwillService\UEFI_Firmware.dll`
- `C:\Program Files\OEM\机械革命电竞控制台\UniwillService\MyControlCenter\UEFI_Firmware.dll`

三份 `UEFI_Firmware.dll` 实际导出表都只有：

- `ReadUefi`
- `WriteUefi`

而没有：

- `ReadSwitchUefi`
- `WriteSwitchUefi`

这意味着当前安装版 `GCUService.exe` 里出现的
`[DllImport("UEFI_Firmware.dll")] ReadSwitchUefi/WriteSwitchUefi`
和真实 native DLL 导出表之间存在不匹配。

当前最稳的解释只有两种：

1. 这两个 `DllImport` 是跨版本遗留或死声明，在这版安装物里不会实际执行
2. 还有未抓到的加载路径/模块在运行时兜底提供了 switch 入口

所以现在还不能把 `dGPU 直连 = 已确认的 UEFI 导出函数调用`
写死成结论；它仍然是强线索，不是最后闭环。

进一步看安装版 native 细节后，这条线还能再压实一层：

- `UEFI_Firmware.dll` 的真实变量名已经拿到：`UniWillVariable`
- vendor GUID 也直接在 `.rdata` 里：
  `{9f33f85c-13ca-4fd1-9c4a-96217722c593}`
- 主变量大小是固定的 `0x200` 字节
- `ReadUefi(start,end)` 本质是“整块读，再返回子区间指针”
- `WriteUefi(start,end,buffer)` 本质是“整块读 -> 覆盖子区间 -> 整块写回”

也就是说，主 UEFI/NVRAM 路径现在已经可以描述成：

- fixed variable: `UniWillVariable`
- fixed GUID
- fixed blob size: `0x200`
- whole-variable read/modify/write

详细记录见：
`windows-control-center/UEFI_Firmware_reverse_report.md`

### 安装版真实配置面

挂载后的 `C:\Program Files\OEM\机械革命电竞控制台\UniwillService\MyControlCenter`
不只是 DLL 目录，它本身就带着运行时配置：

- `UserPofiles/MainOption.json`
- `UserPofiles/Mode1_Profile*.json`
- `UserPofiles/Mode2_Profile*.json`
- `UserPofiles/Mode3_Profile*.json`
- `UserPofiles/Mode4_Profile*.json`
- `UserFanTables/M1T1..M4T5.json`

其中 `MainOption.json` 已经把服务层主状态坐实了：

- `GamingProfileIndex`
- `OfficeProfileIndex`
- `TurboProfileIndex`
- `CustomProfileIndex`
- `OperatingMode`
- `FanBoostEnable`
- `WhisperMode*`

当前机器安装态的 `MainOption.json` 里：

- `OperatingMode = 2`
- 四个 profile index 都是 `0`

`GCU2_Define.OperatingMode` 也给出了全局模式枚举：

- `Office = 0`
- `Gaming = 1`
- `Turbo = 2`
- `Custom = 3`
- `Benchmark = 4`

各 profile JSON 进一步暴露了 OEM 预设族：

- `Mode1_Profile1/2`:
  `PL1/PL2 = 75`, `TCC = 5`, `CTGP = 125`, `DB = 25`, `Table = M1T1/M1T2`
- `Mode2_Profile1/2`:
  `PL1/PL2 = 45`, `TCC = 20`, `CTGP = 125`, `DB = 25`, `Table = M2T1/M2T2`
- `Mode3_Profile1/2`:
  `PL1/PL2 = 130`, `TCC = 5`, `CTGP = 150`, `DB = 25`, `Table = M3T1/M3T2`
- `Mode4_Profile1..5`:
  `Table = M4T1..M4T5`
  其中 `Mode4_Profile1` 为 `PL1/PL2 = 130`, `FanSwitchSpeed = 500`
  其他 profile 更像可调用户档

同时 `UserFanTables/M4T1.json` 也和前面观测到的 custom 线索对上了：

- `"Activated": true`
- `"FanControlRespective": true`

而 `M1T1/M2T1/M3T1` 默认都是：

- `"Activated": false`
- `"FanControlRespective": false`

基于这些安装版默认值，一个相当稳但仍属于推断的映射是：

1. `Mode1` 更像 `Gaming`
2. `Mode2` 更像 `Office / Battery-saver-like`
3. `Mode3` 更像 `Turbo`
4. `Mode4` 更像 `Custom`

这条映射和 `MainOption` / `OperatingMode` / fan table 家族是一致的，
但还需要更多 live 切换数据彻底钉死。

### dGPU 设备开关和 dGPU 直连不是一回事

安装版目录里的 `Command` 脚本把这个差异直接暴露出来了：

- `enableDGpu.ps1`
  `Enable-PnpDevice -FriendlyName *NVIDIA* -Class Display`
- `disableDGpu.ps1`
  `Disable-PnpDevice -FriendlyName *NVIDIA* -Class Display`

而 `GCService5.GPUDeviceItem` 里也直接硬编码了同样的命令字符串：

- `EnableGPUCmdString`
- `DisableGPUCmdString`

同时 `MySettingParams` 里又并列存在两组状态名：

- `sDGpu_Status`
- `sDGpuDirectConnectionSwitch_Status`
- `sDGpuDirectConnectionSwitch_Support`

这说明：

1. `sDGpu_Status` 对应的是 Windows 设备层的 NVIDIA PnP enable/disable
2. `sDGpuDirectConnectionSwitch_*` 才是更接近 MUX / 直连切换的那条线
3. 所以 `enableDGpu.ps1/disableDGpu.ps1` 不能拿来当作 Linux 直连切换原理

### `CustomCtl` 真实语义

这轮把 `UWACPIDriver.sys` 也一起拆开后，
`CustomCtl` 已经从“名字像后门”提升到了“输入格式明确”：

- 驱动 case `0x9C40A504` 是单独分支
- 它先把用户缓冲区拆成两段：
  1. 前 4 字节
  2. 剩余 payload
- 前 4 字节被重组为 4 字符 ACPI method name
- 剩余 payload 作为方法参数走通用 ACPI eval 路径

因此当前最稳的解释是：

- `CustomCtl` = generic ACPI method trampoline
- 输入格式近似：
  `[methodName(4 bytes)][payload...]`

这和托管层的
`AcpiCtrl.CustomRW(string methodName, int input, ref byte data)`
是直接对上的。

### `UWACPIDriver.sys` 内部方法映射

内核驱动还把几个关键 opcode 的 ACPI 目标方法名直接暴露出来了：

- `ReadCMOS` -> `SMCR`
- `WriteCMOS` -> `SMCW`
- `ReadEC` -> `ECRR`
- `WriteEC` -> `ECRW`
- `MMREADB` -> `BRMM`
- `MMREADD` -> `DRMM`
- `MMWRITEB` -> `BWMM`
- `MMWRITED` -> `DWMM`
- `PEREAD` -> `DRCP`
- `PEWRITE` -> `DWCP`
- `IOREAD` -> `DROI`
- `IOWRITE` -> `DWOI`
- `INIOREAD` -> `POIR`
- `INIOWRITE` -> `POIW`
- `TMPREAD1..3` -> `DR1T` / `DR2T` / `DR3T`
- `TMPWRITE1..3` -> `RW1T` / `RW2T` / `RW3T`
- `SMAPCTable` -> `SMRW`

这些字符串直接存在于驱动本体里，而不是只存在于上层猜测里。
同时驱动内部使用控制码 `0x32c004` 去做 method evaluation；
从结构和常量看，它**很像**标准 Windows ACPI method-eval 路径。

这让整个链路第一次真正闭出一条主干：

1. `GCUService` 调 native wrapper / `CustomCtl`
2. `ACPIDriverDll.dll` 把参数塞进共享缓冲区后打 `\\.\ACPIDriver`
3. `UWACPIDriver.sys` 根据 `0x9C40A4xx/0x9C40A500/0x9C40A504` 分派
4. 最终转成对 `ECRR` / `ECRW` / `SMRW` 或任意 4 字符方法名的 ACPI 调用

`GCUBridge.exe` 也不是最终 EC writer。
安装版反编译显示它主要是本地 MQTT 服务封装：

- 启本地 broker，端口 `13688`
- `StartMQTTService()` / `StopMQTTService()`
- 同时也会 `DllImport("UEFI_Firmware.dll")`

因此当前最稳的 Windows 分层应改写成：

1. `WindowsApps/ControlCenter3_*`
   UWP / NativeAOT UI + tray 包
2. `C:\Program Files\OEM\机械革命电竞控制台\UniwillService\`
   本地服务与 MQTT bridge
3. `...\MyControlCenter\ACPIDriverDll.dll` / `UEFI_Firmware.dll`
   最接近硬件和固件的 native bridge

顺带也解决了 `GamingCenter3_Cross.dll` 里 `SharedLibrary.dll` 的来源问题：

- `AppxManifest.xml` 明确依赖
  `Microsoft.NET.Native.Framework.2.2`
  和 `Microsoft.NET.Native.Runtime.2.2`
- 实际 `SharedLibrary.dll` 也确实在
  `C:\Program Files\WindowsApps\microsoft.net.native.framework.2.2_*`

所以 `SharedLibrary.dll` 是 .NET Native runtime 依赖，不是 OEM 私有硬件桥。

### 新确认的 fan-table 调用链

这轮虽然方法体本身仍有 `Runtime exception` 干扰，但 IL 边已经又实了一层：

- `MyFanManager_RamFan1p5.SetFanTableThread(string)` 的闭包
  `<>c__DisplayClass168_0::<SetFanTableThread>b__0/b__1`
  都直接调用：
  `MyFanTableCtrl.SetFanTable(_tableName)`
- `MyFanTableCtrl` 本身同时持有：
  - `FanTable_Manager1p5 m_Manager`
  - `MyEcCtrl EcCtrl`
  - `BackgroundQueue backgroundQueue`
- `MyFanTableCtrl.DisableByService()` 和 `Uninstall()`
  都会走：
  `SetFanControlByRamFan1p5(false)`
- `FanTable_Manager1p5` 继续持有：
  - `MyEcCtrl EcCtrl`
  - `DefaultFanTable_Gaming/Office/Turbo`
  - `M1T1..M4T5`
  - `FileNames`
  并公开：
  `SetEcFanTable` / `SetEcFanTable_Cpu` / `SetEcFanTable_Gpu`
  `LoadFanTableFromJson` / `WriteFanTableToJson`
  `RefreshDefaultFanTable` / `TableTransform`

这说明 Linux 侧不能把 fan apply 理解成“UI 直接写 96 个 EC 字节”。
更接近真实结构的是：

1. `MyFanManager_RamFan1p5`
   负责服务层/模式层编排
2. `MyFanTableCtrl`
   负责 fan-table 任务入口与 custom-mode/fan-control 辅助控制
3. `FanTable_Manager1p5`
   负责 CPU/GPU 表项装配与真正的表写入
4. `MyEcCtrl`
   负责底层 EC/ACPI/WMI 能力

因此，Linux 第一版的工程拆分保持 `profile-service -> fan-table stage -> backend-amw0`
这个方向是正确的。

### 已确认的 EC 地址表

`Define.ECSpec` 和 `Define.RamFan1p5_ECSpec` 给出了非常值钱的常量：

- 模式号：
  `FAN_GAMING_MODE = 0`
  `FAN_OFFICE_MODE = 1`
  `FAN_TURBO_MODE = 2`
  `FAN_CUSTOM_MODE = 3`
- 风扇控制字节：
  `Turbo_Mode = 0x10`
  `FanBoost_Mode = 0x40`
  `User_Fan_Mode = 0x80`
  `User_Fan_Level1..5 = 0x81..0x85`
  `User_Fan_HiMode = 0xA0`
- 关键 EC 地址：
  - `ADDR_MAFAN_CONTROL_BYTE = 1873 (0x751)`
  - `ADDR_PL1_SETTING_VALUE = 1923 (0x783)`
  - `ADDR_PL2_SETTING_VALUE = 1924 (0x784)`
  - `ADDR_PL4_SETTING_VALUE = 1925 (0x785)`
  - `ADDR_TimAP_TccOffset_Setting = 1926 (0x786)`
  - `ADDR_TimAP_FanSwitchSpeedT100mSec = 1927 (0x787)`
  - `ADDR_MyFanCCI_Mode_Index = 1963 (0x7AB)`
  - `ADDR_MyFanCCI_Mode_Profile1 = 1968 (0x7B0)`
  - `ADDR_MyFanCCI_Mode_Profile2 = 1969 (0x7B1)`
  - `ADDR_MyFanCCI_Mode_Profile3 = 1970 (0x7B2)`

`RamFan1p5_ECSpec` 还直接给出了双风扇曲线在 EC RAM 里的布局：
- CPU `UpT` 起始：`3840 (0xF00)`
- CPU `DownT` 起始：`3856 (0xF10)`
- CPU `Duty` 起始：`3872 (0xF20)`
- GPU `UpT` 起始：`3888 (0xF30)`
- GPU `DownT` 起始：`3904 (0xF40)`
- GPU `Duty` 起始：`3920 (0xF50)`
- fan-table 状态/控制字节：
  `3933..3935 (0xF5D..0xF5F)`

这已经足够做 Linux 侧“表格式风扇控制”的后端原型。

### 新确认的模式/表写入后端形状

这轮把 AML `ECMG` 字段和 `Define.*` 常量再对了一遍，得到一个比之前更稳的结论：

- `0x751` 不是一个简单的 “0/1/2/3 模式号字节”。
  在 AML 里，这里至少明确有：
  - `TBME` at bit 4
  - `UFME` at bit 7
  这和 `Turbo_Mode = 0x10`、`User_Fan_Mode = 0x80`
  正好对上。
- 这意味着：
  - `turbo` 很可能是在 `0x751` 上置 `TBME`
  - `custom` 很可能是在 `0x751` 上置 `UFME`
  - `office` 和 `gaming` 的区分，大概率不只靠 `0x751`
- 已知的 `ADDR_MyFanCCI_Mode_Index = 0x7AB` 和
  `ADDR_MyFanCCI_Mode_Profile1..3 = 0x7B0..0x7B2`
  现在更像真正的模式/档位索引区，而不是陪衬常量。
- AML 还在 `0x7C7` 暴露了 `LCSE` / `OCPL` 这类辅助位，
  这和 `MyEcCtrl.SetCustomModetoEC` / `Set_APExistToEC`
  这类 helper 名字是能对上的。

因此，当前对 `SetModeSwitchChangeThread` 的最佳推断是：

1. 先更新模式/档位索引区 `0x7AB` / `0x7B0..0x7B2`
2. 再更新 `0x751` 的控制位
3. 视机型再碰辅助 helper 位，例如 `0x7C7`

对 `SetEcFanTable_*` 的最佳推断也更清楚了：

- 它更像是 **直接把 fan-table 字节写进 EC/ECMG 地址区**
  `0xF00..0xF5F`
- 然后再补 `0xF5D..0xF5F` 这些状态/控制尾字节
- 这更像 “地址 + 数据” 型后端，而不是一个单独的 fan opcode

换句话说，当前最像真相的“最终 packet format”其实分成两层：

- Windows 服务层看到的是：
  `addr/value` 式的 EC/ECMG 写入模型
- Linux AML/AMW0 层看到的是：
  `WKBC` / `SCMD` 这类 transport packet

两层都已经有实锤，但它们之间的最终映射还没完全闭环。

### Linux 实机观测（2026-04-06）

在当前这台 `GM6PX0X` Linux 实机上，通过 `\_SB.INOU.ECRR` 读到：

- `0x751 = 0x10`
- `0x7AB = 0x00`
- `0x7B0..0x7B2 = 0x00`
- `0x7C7 = 0x0C`
- `0x783..0x787 = 0x00`
- `0xF00..0xF5F = 0x00`
- `0x460 (FFAN) = 0x02`

这组实测值把上面的推断又压实了一层：

- `0x751 = 0x10` 与 `Turbo_Mode = 0x10` 完全对上，
  所以 `TBME` 这条解释现在已经非常可信。
- 但同时 `0x7AB = 0x00`，
  说明 `0x7AB` 并不是“跟着当前 turbo/custom 位直接变化的单字节镜像”。
- `0x7C7 = 0x0C` 按 AML 位布局可解成：
  `LCSE = 0`、`OCPL = 3`
  这更像一个辅助性能/helper 字段，而不是主模式号。
- `0x783..0x787` 和 `0xF00..0xF5F` 全零，
  说明在当前 Linux 运行态下，功耗参数和 fan-table RAM 还没有被像 Windows 控制中心那样初始化/下发出来。

因此，当前最稳的判断是：

1. `0x751` 负责 turbo/custom 这类主控制位
2. `0x7AB` / `0x7B0..0x7B2` 更像 profile index / slot 侧信息
3. `0x7C7` 更像辅助性能档/helper 位
4. 真正的 fan-table 生效区 `0xF00..0xF5F` 只有在某个更高层流程跑完后才会被填充

### Linux 切换差分（2026-04-06）

进一步切换 OEM 预设模式后，`0x751` 在这台机器上至少出现了这些原始值：

- `0x00`
- `0x10`
- `0x20`
- `0x30`
- `0xA0`

这组差分非常关键：

- `0x10` 继续支持 `TBME = turbo`
- `0xA0` 和常量 `User_Fan_HiMode = 0xA0` 直接对上
- 这反过来把 `0x20` 指向了一个更具体的解释：
  `HiMode` helper bit
- `0x30 = 0x20 + 0x10`
  更像 `HiMode + Turbo`
- `0xA0 = 0x20 + 0x80`
  更像 `HiMode + UserFan`

所以现在不能把 `0x751` 简化成：

- bit4 = turbo
- bit7 = custom

更准确的说法是：

- bit4 和 bit7 已经有较强证据
- bit5 很可能就是 `HiMode` 辅助位
- `office` / `gaming` / `overboost` / OEM 预设族里，至少有一部分状态仍压在 `0x751` 上

结合你这轮口头标注 “OEM 的省电 / 均衡 / 狂暴预设”，当前最稳的收敛是：

1. `0x30` 基本可以视为狂暴族，因为它等于 `HiMode + Turbo`
2. `0x20` 是非 turbo 的 `HiMode` 族，像均衡/基础性能预设
3. `0xA0` 是 `HiMode + UserFan` 族，说明 OEM 预设里至少有一个档位是走 user-fan 分支实现的

这里仍有一个边界：

- 这说明 OEM 文案和 EC bit 语义并不一定一一对应
- `省电/均衡/狂暴` 是产品层名字
- `HiMode/TBME/UFME` 是底层实现位
- 两层之间还需要继续靠更多切换差分去精确映射

### 已确认的调用边

虽然 `SetFanTableThread` / `SetModeSwitchChangeThread` / `SetOperatingModeProfileIndexThread` 的方法体还没完整抠出来，但 IL 里已经确认了一个关键边：

- `MyFanManager_RamFan1p5.SetFanTableThread(...)`
  最终会调用
  `FanTable.SetFanTable(_tableName)`

也就是说，`MyFanManager_*` 负责模式与策略编排，真正的 fan-table 写入逻辑继续下沉到 `FanTable_Manager1p5*`。

### `SystrayComponent.exe` 的真实作用

把安装版 `WindowsApps/.../Win32/SystrayComponent.exe` 也拆开后，tray 层的角色已经很明确：

1. 连接本地 MQTT broker
2. 订阅少量状态 topic
3. 把用户点击转换成 `Fan/Control` 的业务命令

它并不掌握底层硬件写口。

关键证据：

- `MQTTDatsService` 直接连接：
  - host = `localhost`
  - port = `13688`
- 订阅列表只有：
  - `Fan/Status`
  - `Tray/Status`
  - `Customize/SupportInfo`
  - `Languages/Info`
  - `Languages/Control`
  - `KeyboardManager/EnableStatus`
- 连接成功后，tray 主动发送的初始化请求只有：
  - `Customize/SupportControl` -> `{ Action = "GETSUPPORT" }`
  - `Fan/Control` -> `{ Action = "GETSTATUS" }`

托盘菜单动作的真实 payload 也已经拿到：

- office -> `Fan/Control` / `OPERATING_OFFICE_MODE`
- gaming -> `Fan/Control` / `OPERATING_GAMING_MODE`
- turbo -> `Fan/Control` / `OPERATING_TURBO_MODE`
- custom -> `Fan/Control` / `OPERATING_CUSTOM_MODE`

同时，tray 反序列化的 `MyRamFan1p5` 状态面再次坐实了服务层对象模型：

- `OperatingMode`
- `FanBoostEnable`
- `CPU_PL1/PL2/PL4`
- `CPU_TccOffset`
- `GPU_ConfigurableTGPTarget`
- `GPU_DynamicBoost`
- `GPU_WhisperMode*`
- `FAN_FanSwitchSpeedEnabled`
- `FAN_FanSwitchSpeed`

因此这一层现在可以定性为：

1. tray 只是 `Fan/Control` 的轻前端
2. 模式切换的业务语义仍然落在 `GCUService`
3. `OPERATING_*` 是总线层业务命令，不是 EC / ACPI 层命令

### `GCUBridge.exe` 的真实作用

`GCUBridge.exe` 也已经可以从“猜测的本地 broker”提升为“实现边界明确的 MQTT server”：

- 它是托管 `.NET` 程序集
- 监听端口固定为 `13688`
- 使用 `MQTTnet` 创建 broker
- 本地回环连接 `[::1]` / `127.0.0.1` 与外部连接走两套认证逻辑

本地客户端身份池是显式构造的：

- `UWPClient*`
- `MyKeyboard*`
- `MyTray*`
- `PluginClient*`
- `MyControlCenter`
- `MyTPDetector`
- `MyDynamicDesktop`
- `OcTool`
- `OcScanner`
- `OpenCL`

`TimerService` 进一步说明了部署边界：

- 启动本地 MQTT server
- 保活并拉起 `MyControlCenter\\GCUService`
- 在用户登录时向 `Languages/Control` 发布
  `{ Action = "EnableTray" }`

bridge 本身目前只确认会在 broker 内部直接处理极少数特殊 topic：

- `Service/SetPassword`
- `Service/FWBuffer`
- `Service/SetExitCommand`

其中 `Service/FWBuffer` 会直接调用
`NvramVariable.SetFwVarsBuf(...)`，
说明 bridge 自己也掌握一条 NVRAM buffer 更新旁路。

更重要的是，这条旁路的字段级写法已经能直接看到：

- byte:
  `PowerMode`、`MemoryOverClockSwitch`、`ApExistFlag`、`OverClockRecoveryFlag`、
  `ACRecoveryStatus`、`ApUseFlag`、`OemDisplayMode`、`FnKeyStatus`
- `ushort`:
  `ICpuCoreVoltageValue`、`ICpuCoreVoltageOffsetValue`、
  `ICpuCoreVoltageOffsetNegativeValue`、`ICpuTauValue`
- `uint`:
  `ACpuFreqValue`、`ACpuVoltageValue`
- byte arrays:
  `RGBKeyboard08`、`RGBKeyboard1A`、`SmartLightbar08`、`SmartLightbar1A`

bridge 里的 `NvramVariable` 还明确展示了写回流程：

1. `ReadUefi(0, NVRAM_STRUCT_SIZE)`
2. patch `NVRAM_STRUCT` 字段
3. marshal 整个 struct
4. `WriteUefi(0, NVRAM_STRUCT_SIZE, array)`

这又把一个边界压实了：

- `UniWillVariable` 这条主 UEFI 路径就是“整块 struct 读改写”
- 目前可见字段里仍然没有明确的 dGPU / MUX / direct-connect 开关
- 因此 `SwitchUefi` 依然没有被这条 bridge-side 实现闭环

所以现在可以把总线层分成三层：

1. `GCUBridge.exe` = 本地 MQTT broker + 进程保活
2. `SystrayComponent.exe` = 轻量 tray client
3. `GCUService.exe` = 主要业务编排和硬件后端协调者

### 当前限制

这轮 `ilspycmd` 已经确认了接口、字段、常量和部分调用边；但若要完整恢复业务流程，仍需继续处理那些反编译后直接抛 `Runtime exception` 的方法体。下一步应优先抓：
- `SetOperatingModeProfileIndexThread`
- `SetModeSwitchChangeThread`
- `SetFanTableThread`
- `FanTable_Manager1p5*.SetEcFanTable*`

同时，研究优先级也应相应调整：

- `GamingCenter3_Cross.dll`
  继续作为 UI / topic / 文案证据使用
- `ACPIDriverDll.dll`
  现在已经是第一优先级 native target
- `UEFI_Firmware.dll`
  是 dGPU 直连、持久化开关、固件变量类功能的重点目标

## 额外功能

- 电池保护：`BatteryProtection` / `BatteryProtection2` / `SetHealthProtectionHigh|Middle|Low`
- UEFI / NVRAM：`UEFI_Firmware.dll` / `ReadUefi` / `WriteUefi` / `NvramVariable`
- 飞行模式：`UWAirplane.dll` / `SetAirplaneMode`
- RGB 键盘 / 灯条：`MyRgbKeyboard.*`
- 液冷 / 外设：`LiquidCooling` / `LiquidHWOC`
- 应用绑定：`ApplicationBinding.AppProfileBinding`

## 对 Linux 复刻最重要的启发

1. **第一目标不是复刻 UI，而是复刻 GCUService 的“模式命令 -> 底层 ACPI/EC 调用”这层。**
2. 最值得继续追的类：
   - `MyControlCenter.MyFanManager_*`
   - `MyControlCenter.MySystemManager_*`
   - `GCUService.WCFService.*`
   - `MyControlCenter.MqttClientCtrl`
3. 最值得下手的方法名：
   - `SetOperatingModeProfileIndexThread`
   - `SetModeSwitchChangeThread`
   - `SetFanTableThread`
   - `SetEcFanTable*`
   - `ReadACPI` / `WriteACPI`
   - `WMIReadECRAM` / `WMIWriteECRAM`
4. 由于命名未明显混淆，下一步最有效的方法是直接上 **dnSpy / ILSpy** 反编译，而不是盲猜抓包。

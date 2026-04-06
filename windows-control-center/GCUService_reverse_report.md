# GCUService.exe 初步静态逆向报告

- 文件: `GCUService.exe`
- SHA256: `e2095ea378382e00f910c3bb9c031990b6002f262f932f838f9f7be80ea767c8`
- 类型: PE32+ x64 .NET/CLR 程序
- 构建时间: 2025-08-08 06:54:16 UTC
- PDB 路径: `E:\WorkSpace\CCU_49ver\MyControlCenter\MyControlCenter\obj\x64\Release\GCUService.pdb`

## 核心判断

1. 这不是简单的托盘辅助程序，而是 **服务端/业务核心**。
2. 命名基本未混淆，包含大量完整 namespace / type / method 名，后续用 dnSpy / ILSpy 继续逆向会很顺。
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

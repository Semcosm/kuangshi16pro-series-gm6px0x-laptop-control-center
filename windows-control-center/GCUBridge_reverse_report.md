# GCUBridge.exe 初步逆向报告

文件：`GCUBridge.exe`

- SHA256: `45b2c1d9d7d90cc9a31936655e872d4c108703a9f442e3b6e1d6037af59375b4`
- 类型：`PE32+ x86-64 .NET/CLR assembly`
- 调试符号路径（PDB）：`D:\development\ControlCenter3\Service\MQTTService\MQTTService\obj\x64\Release\GCUBridge.pdb`
- 形态判断：后台服务 / 本地 broker，而不是前端 UI

## 一、最核心结论

`GCUBridge.exe` 高概率就是这套控制中心的 **本地消息桥 / broker**。

从二进制中直接能看到：

- `MQTTnet`
- `MQTTnet.Server`
- `CreateMqttServer`
- `StartMqttserver`
- `MqttServer_ApplicationMessageReceived`
- `MqttServer_ClientConnected`
- `MqttServer_ClientSubscribedTopic`
- `PublishTopic`
- `ValidatingMqttClients`

这说明它不是普通“转发壳子”，而是自己内置了 **MQTT broker/server** 的角色。

## 二、它最像什么架构

结合前面的动态抓包，可推测：

- `GamingCenter3_Cross.exe` = 控制台前端桥接客户端
- `SystrayComponent.exe` = 托盘/常驻客户端
- `OSDTpDetect.exe` = 热键/OSD 客户端
- `GCUService.exe` = 业务核心服务
- `GCUBridge.exe` = **本地 MQTT broker / 消息总线 / 认证网关**

也就是：

```text
UI / Tray / OSD / Service
        |
        v
   GCUBridge.exe
   (MQTT broker)
        |
        v
   各功能模块 / 本地命令主题
```

## 三、它不是“开放网络服务”，而是偏向 localhost 内部总线

二进制里能直接看到这些线索：

- `127.0.0.1`
- `IfLocalhost`
- `OutofLocalHostNetwork`
- `GetActiveTcpListeners`
- `PortInUse`

这说明它显式关心：

1. 是否只允许 localhost 通信
2. 端口是否被占用
3. 是否存在本地以外的连接

所以它很像 **只开放给本机组件使用的本地控制总线**。

## 四、它有客户端认证，而且账号口令看起来是内置的

二进制里能直接看到：

- `ValidatingMqttClients`
- `MyControlCenterUser`
- `MyControlCenterPwd888881772688`
- `MyTPDetectorUser`
- `MyTPDetectorPwd888881772688`
- `MyDynamicDesktopUser`
- `OcScannerUser`
- `OcToolUser`
- `OpenCLUser`

这说明 `GCUBridge.exe` 不只是单纯监听端口，而是很可能：

- 对不同本地客户端分配固定用户名/密码
- 在 broker 层做 MQTT 客户端认证

也就是说，`GamingCenter3_Cross.exe`、`OSDTpDetect.exe`、`GCUService.exe` 等进程，极可能是以 **MQTT client** 身份接入 `GCUBridge.exe`。

## 五、已经看到的内部主题 / 命令线索

从字符串里能直接看到几条很像 MQTT topic 或内部控制消息：

- `Service/FWBuffer`
- `Service/SetExitCommand`
- `Service/SetPassword`

这些名字很像：

- 服务间控制 topic
- broker 配置/认证 topic
- 退出命令或缓冲指令

还没有直接看到大量“Fan/Mode/Profile”主题名，这反而支持一个判断：

- `GCUBridge.exe` 主要负责 **消息收发/认证/路由**
- 业务语义（风扇、性能模式、电池保护等）更多在 `GCUService.exe`

## 六、它带有安装/防火墙/隔离处理逻辑

二进制里还有这些线索：

- `SetupWindowsFirewall`
- `RemoveWindowsFirewall`
- `CheckNetIsolation.exe`
- `checknetisolation`
- `Service1Installer`
- `ServiceInstaller`
- `ProjectInstaller`
- `Service1Installer copy to System32`

这说明它可能负责：

- 安装为 Windows 服务
- 配置防火墙例外或本地访问策略
- 处理打包应用 / AppContainer / NetIsolation 兼容问题

这和 `ControlCenter3` 前端是 WindowsApps/MSIX 包的结构非常吻合：

前端包和传统后台服务之间，往往需要额外处理网络隔离与本地通信权限。

## 七、它还具备“按当前登录用户拉起进程”的能力

字符串里有：

- `CreateProcessAsUser`
- `CreateProcessAsUserWrapper`
- `WTSQueryUserToken`
- `GetCurrentActiveUser`
- `UserProcess`
- 一整套 `WTS*` 会话状态相关 API 名称

这说明它可能会：

- 在服务会话里运行
- 再把某些 UI/辅助组件拉起到当前交互用户会话里

这和你前面抓到的“托盘层/桥接层/前端层分离”也能对上。

## 八、它不是硬件控制最终落点

到目前为止，`GCUBridge.exe` 的字符串更像：

- broker
- service host
- MQTT server
- firewall / install / session helper

而不像最终的硬件控制层。

目前没有在它身上看到大量：

- `ReadACPI`
- `WriteACPI`
- `IOCTL_GPD_ACPI_*`
- `SetEcFanTable*`

这些更重的业务/硬件调用名字。

这些核心硬件语义，前面主要出现在 `GCUService.exe` 里。

## 九、对 Linux 版控制台的意义

这基本把架构拆成两层了：

### 协议层
由 `GCUBridge.exe` 实现

- 本地 localhost 通信
- MQTT broker
- 客户端认证
- 主题消息路由

### 业务层
由 `GCUService.exe` 实现

- 风扇模式
- 风扇表
- 电池保护
- 功耗/PL 限制
- ACPI/WMI/EC 调用

所以 Linux 版最现实的路线不是“复刻 GCUBridge”，而是：

1. 搞清 `GCUService` 的业务接口语义
2. 直接复刻业务层对 ACPI/EC 的访问
3. UI/CLI 自己做

除非你后面想做“兼容 Windows 协议”的完整替代品，才需要认真复刻 `GCUBridge` 这一层。

## 十、下一步最值得做的事

1. 继续反编译 `GCUBridge.exe`
   - 看 `MqttServerPort` 的真正取值来源
   - 看 `ValidatingMqttClients` 怎么校验用户名密码
   - 看 `PublishTopic` / `MqttServer_ApplicationMessageReceived` 里有哪些主题分发逻辑

2. 配合 `GCUService.exe`
   - 找它作为 MQTT client 时连接的用户名/密码/主题
   - 找风扇模式切换对应的 topic 名

3. 真正面向 Linux 的关键
   - 优先继续拆 `GCUService.exe` 的 `SetOperatingModeProfileIndex` / `SetFanTable` / `WriteACPI` / `WMIWriteECRAM`

## 当前判断一句话总结

`GCUBridge.exe` 不是风扇控制核心，而是 **控制中心内部的 localhost MQTT broker / 消息总线 / 认证桥**；
真正“懂风扇模式、性能模式、ACPI/EC 写入”的主体，仍然更像 `GCUService.exe`。

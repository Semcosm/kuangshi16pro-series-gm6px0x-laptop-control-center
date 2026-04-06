# GPU Mux

`dGPU only` / direct-connect support is still experimental.

What is known:

- Windows service metadata still mentions `ReadSwitchUefi` and
  `WriteSwitchUefi`
- installed `UEFI_Firmware.dll` exports only `ReadUefi` and `WriteUefi`
- the main `UniWillVariable` structure does not expose an obvious mux field

Implementation consequence:

- do not mix MUX switching into the main mode/profile path
- keep it behind a separate capability check
- treat it as reboot-required until a live Linux path is confirmed

For the Linux tree this means:

- MUX work belongs under `backends/uefi/` or another explicitly experimental
  feature area
- the daemon should expose requested/effective state separately for this class
  of feature

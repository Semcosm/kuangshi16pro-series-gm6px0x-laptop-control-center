# Repository Guidelines

## Project Structure & Module Organization
This repository is a research workspace, not a packaged application. Top-level Markdown files such as `README.md`, `CURRENT-FINDINGS.md`, `AMW0-analysis.md`, and `windows-control-center-analysis.md` hold the current reverse-engineering narrative. Root scripts are grouped by prefix: `amw0-*.sh` for AMW0/WMI transport work, `ec-*.sh` for EC observation, and `acpi-*.sh` for ACPI table capture. `windows-control-center/` stores vendor binaries, extracted artifacts, reverse reports, and PowerShell capture helpers. `legacy-nbfc-configs/` is historical reference only.

## Build, Test, and Development Commands
There is no Makefile or CI pipeline; development is script-driven.

- `bash -n amw0-wmbc-pack.sh amw0-wmbc-trace.sh`: syntax-check Bash helpers before commit.
- `sudo ./amw0-wmbc-pack.sh send 0x0 0x0001 0x49 0x00 0x1E 0x00`: issue one AMW0 `WMBC` send on Linux.
- `sudo ./amw0-wmbc-trace.sh wkbc 0x0000:0x49:0x00:0x1E:0x00`: capture EC-side timing and transient bytes.
- `powershell -ExecutionPolicy Bypass -File .\windows-control-center\windows-cc-capture.ps1`: collect Windows Control Center runtime evidence.

Run hardware-touching scripts only on the target laptop with `acpi_call` and required tools installed.

## Coding Style & Naming Conventions
Use Bash with `#!/usr/bin/env bash` and `set -euo pipefail`. Match the existing two-space indentation in shell and PowerShell files. Keep filenames descriptive and prefix-based: `amw0-*`, `ec-*`, `acpi-*`, `cc-*`. Prefer small, single-purpose scripts and write comments for transport layout, register meaning, or capture intent, not for obvious shell mechanics.

## Testing Guidelines
There is no formal test suite yet. Minimum validation is:

- syntax-check edited `.sh` files with `bash -n`
- re-run the affected capture or probe script on hardware
- record outcome changes in `CURRENT-FINDINGS.md` or the relevant analysis note

Treat generated logs and capture directories as artifacts, not tests.

## Commit & Pull Request Guidelines
Current history is short and inconsistent (`Initial import...`, several `Add files via upload`). For new work, use concise imperative subjects tied to the area changed, for example `amw0: fix WMBC payload packing` or `windows-cc: add deep capture notes`. PRs should explain hardware context, scripts run, files generated, and whether the change updates findings, tooling, or archived evidence. Include screenshots only when a Windows UI observation matters.

## Artifact & Safety Notes
Do not commit ignored runtime outputs such as `amw0-logs/`, `ec-probe-logs/`, `acpi-scan/`, extracted MSIX trees, or `windows-control-center/cc-capture-*`. Avoid destructive probes; prefer read/trace helpers first and document any write-path experiment clearly.

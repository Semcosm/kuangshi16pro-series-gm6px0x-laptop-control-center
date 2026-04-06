# Windows Control Center Notes

This directory is kept in Git for Windows-side research inputs, not as a vendor
binary archive.

Tracked here:

- reverse reports
- string triage outputs
- Windows package analysis notes

Intentionally untracked here:

- copied vendor executables such as `GCUService.exe` and `GCUBridge.exe`
- installer archives, extracted trees, and symbol bundles
- runtime capture outputs such as `cc-capture-*`, `cc-deep-capture-*`, and `cc-static-triage-*`

Windows capture and triage scripts live in `scripts/windows/` and write their
default outputs back into this ignored directory tree.

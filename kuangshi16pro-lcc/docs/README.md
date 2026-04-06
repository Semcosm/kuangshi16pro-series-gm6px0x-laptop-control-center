# Kuangshi16Pro LCC Docs

This directory contains the implementation-oriented notes for the Linux-native
control-center architecture.

Start here:

- `architecture.md`
  system daemon, D-Bus, Polkit, backend split, and first-phase boundaries
- `dbus-api.md`
  public bus name, object path, interface split, and request/effective-state model
- `profiles.md`
  mode/profile/fan/power objects mirrored from the Windows reverse work
- `backend-amw0.md`
  proven AMW0 / WMBC / WKBC / SCMD facts that matter for the vendor backend
- `gpu-mux.md`
  experimental MUX/direct-connect notes and why this stays off the main path
- `dev-notes/reverse-findings.md`
  condensed reverse-engineering facts that directly affect implementation
- `dev-notes/call-paths.md`
  call-edge summary from UI/tray/service/driver layers
- `source-map.md`
  which repository files are still worth reading, and what each one is for
- `roadmap.md`
  current build order and reverse-engineering priorities
- `ci.md`
  GitHub Actions checks and the protected-branch required-check list

These notes are curated for writing code. The full reverse-engineering record
remains in the repository root and `windows-control-center/`.

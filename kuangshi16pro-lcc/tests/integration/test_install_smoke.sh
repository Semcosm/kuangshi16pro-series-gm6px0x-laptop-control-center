#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
project_dir="$repo_root/kuangshi16pro-lcc"

stage_root="$(mktemp -d /tmp/lcc-install-smoke.XXXXXX)"
prefix="/opt/kuangshi16pro-lcc-smoke"
bindir="$prefix/bin"
libexecdir="$prefix/libexec/kuangshi16pro-lcc"
systemdunitdir="/etc/systemd/system"
dbussystemservicedir="$prefix/share/dbus-1/system-services"
dbussystemconfdir="/etc/dbus-1/system.d"
polkitactionsdir="$prefix/share/polkit-1/actions"

cleanup() {
  rm -rf "$stage_root"
}

require_file() {
  local path="$1"

  if [[ ! -f "$path" ]]; then
    printf 'missing file: %s\n' "$path" >&2
    exit 1
  fi
}

trap cleanup EXIT

make -C "$project_dir" all
make -C "$project_dir" install \
  DESTDIR="$stage_root" \
  PREFIX="$prefix" \
  BINDIR="$bindir" \
  LIBEXECDIR="$libexecdir" \
  SYSTEMDUNITDIR="$systemdunitdir" \
  DBUSSYSTEMSERVICEDIR="$dbussystemservicedir" \
  DBUSSYSTEMCONFDIR="$dbussystemconfdir" \
  POLKITACTIONSDIR="$polkitactionsdir"

require_file "$stage_root$bindir/lccctl"
require_file "$stage_root$libexecdir/lccd"
require_file "$stage_root$systemdunitdir/lccd.service"
require_file "$stage_root$dbussystemservicedir/io.github.semcosm.Lcc1.service"
require_file "$stage_root$dbussystemconfdir/io.github.semcosm.Lcc1.conf"
require_file "$stage_root$polkitactionsdir/io.github.semcosm.Lcc1.policy"

grep -Fq "ExecStart=$libexecdir/lccd --system" \
  "$stage_root$systemdunitdir/lccd.service"
grep -Fq "BusName=io.github.semcosm.Lcc1" \
  "$stage_root$systemdunitdir/lccd.service"
grep -Fq "SystemdService=lccd.service" \
  "$stage_root$dbussystemservicedir/io.github.semcosm.Lcc1.service"

if grep -Fq "$project_dir" "$stage_root$systemdunitdir/lccd.service"; then
  printf 'service file leaked build-tree path: %s\n' "$project_dir" >&2
  exit 1
fi

make -C "$project_dir" uninstall \
  DESTDIR="$stage_root" \
  PREFIX="$prefix" \
  BINDIR="$bindir" \
  LIBEXECDIR="$libexecdir" \
  SYSTEMDUNITDIR="$systemdunitdir" \
  DBUSSYSTEMSERVICEDIR="$dbussystemservicedir" \
  DBUSSYSTEMCONFDIR="$dbussystemconfdir" \
  POLKITACTIONSDIR="$polkitactionsdir"

if [[ -d "$stage_root" ]] &&
    find "$stage_root" -mindepth 1 -print -quit | grep -q .; then
  printf 'staging root not cleaned by uninstall: %s\n' "$stage_root" >&2
  exit 1
fi

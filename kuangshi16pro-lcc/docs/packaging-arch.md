# Arch Packaging

This note describes the current Arch Linux package flow for `kuangshi16pro-lcc`
from a live checkout.

## Build

Build the package from the repository root:

- `makepkg -f`

This currently produces:

- `kuangshi16pro-lcc-<pkgver>-x86_64.pkg.tar.zst`
- `kuangshi16pro-lcc-debug-<pkgver>-x86_64.pkg.tar.zst`

The package version is derived from the local Git checkout as
`r<commit-count>.g<short-sha>`.

## Legacy Cleanup

If you previously installed files by hand, remove the old unmanaged paths before
switching to the packaged layout. The common legacy paths are:

- `/usr/bin/lccctl`
- `/usr/lib/kuangshi16pro-lcc/lccd`
- `/usr/share/dbus-1/system-services/io.github.semcosm.Lcc1.service`
- `/usr/share/polkit-1/actions/io.github.semcosm.Lcc1.policy`
- `/etc/systemd/system/lccd.service`
- `/etc/systemd/system/multi-user.target.wants/lccd.service`
- `/etc/dbus-1/system.d/io.github.semcosm.Lcc1.conf`

Recommended scan:

- `sudo find /usr /etc /opt -maxdepth 5 \( -name 'lccctl' -o -name 'lccd' -o -name 'lccd.service' -o -name 'io.github.semcosm.Lcc1.service' -o -name 'io.github.semcosm.Lcc1.conf' -o -name 'io.github.semcosm.Lcc1.policy' -o -path '/usr/lib/kuangshi16pro-lcc' -o -path '/usr/share/licenses/kuangshi16pro-lcc' \) -print | sort`

Ownership check:

- `pacman -Qo /usr/bin/lccctl /usr/lib/kuangshi16pro-lcc/lccd /etc/systemd/system/lccd.service /usr/share/dbus-1/system-services/io.github.semcosm.Lcc1.service /etc/dbus-1/system.d/io.github.semcosm.Lcc1.conf /usr/share/polkit-1/actions/io.github.semcosm.Lcc1.policy`

If those files are not owned by a package, remove them before installing the
package so `/etc` overrides do not shadow the packaged layout.

## Install

Install the built package:

- `sudo pacman -U ./kuangshi16pro-lcc-<pkgver>-x86_64.pkg.tar.zst`

Installed package-owned files currently include:

- `/usr/bin/lccctl`
- `/usr/lib/kuangshi16pro-lcc/lccd`
- `/usr/lib/systemd/system/lccd.service`
- `/usr/share/dbus-1/system-services/io.github.semcosm.Lcc1.service`
- `/usr/share/dbus-1/system.d/io.github.semcosm.Lcc1.conf`
- `/usr/share/polkit-1/actions/io.github.semcosm.Lcc1.policy`
- `/usr/share/licenses/kuangshi16pro-lcc/LICENSE`
- `/usr/share/man/man1/lccctl.1`
- `/usr/share/man/man8/lccd.8`

## Verify

Check package ownership and installed payload:

- `pacman -Ql kuangshi16pro-lcc`
- `systemctl cat lccd.service`
- `man 1 lccctl`
- `man 8 lccd`

Start the daemon manually for verification:

- `sudo systemctl start lccd.service`
- `systemctl status lccd.service --no-pager`
- `lccctl capabilities`
- `lccctl state`

Enable it at boot only if you want the daemon to stay resident:

- `sudo systemctl enable --now lccd.service`

## Notes

- `standard` remains the preferred read-state backend where available.
- `amw0` remains the fallback execution backend for mutating operations on this
  machine family.
- `namcap` validation is recommended after package changes, but it depends on
  the `namcap` package being installed on the build host.

# Maintainer: chen

_repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"

pkgname=kuangshi16pro-lcc
pkgver=r36.g1e010b6
pkgrel=1
pkgdesc="Linux control center daemon and CLI for the Mechrevo Kuangshi16Pro GM6PX0X series"
arch=('x86_64')
url="https://github.com/Semcosm/kuangshi16pro-series-gm6px0x-laptop-control-center"
license=('AGPL-3.0-or-later')
depends=('dbus' 'glib2' 'polkit' 'systemd-libs')
makedepends=('gcc' 'make' 'pkgconf' 'systemd')
install="${pkgname}.install"
options=('!debug' '!lto')

pkgver() {
  if git -C "${_repo_root}" rev-parse --git-dir >/dev/null 2>&1; then
    printf 'r%s.g%s' \
      "$(git -C "${_repo_root}" rev-list --count HEAD)" \
      "$(git -C "${_repo_root}" rev-parse --short HEAD)"
  else
    printf 'r0.gunknown'
  fi
}

build() {
  make -C "${_repo_root}/kuangshi16pro-lcc" clean all
}

check() {
  make -C "${_repo_root}/kuangshi16pro-lcc" clean test
  bash "${_repo_root}/kuangshi16pro-lcc/tests/integration/test_install_smoke.sh"
  bash "${_repo_root}/kuangshi16pro-lcc/scripts/check_deployment_surface.sh"
}

package() {
  make -C "${_repo_root}/kuangshi16pro-lcc" install \
    DESTDIR="${pkgdir}" \
    PREFIX=/usr \
    BINDIR=/usr/bin \
    LIBEXECDIR=/usr/lib/kuangshi16pro-lcc \
    SYSTEMDUNITDIR=/usr/lib/systemd/system \
    DBUSSYSTEMSERVICEDIR=/usr/share/dbus-1/system-services \
    DBUSSYSTEMCONFDIR=/usr/share/dbus-1/system.d \
    POLKITACTIONSDIR=/usr/share/polkit-1/actions \
    LICENSEDIR=/usr/share/licenses/kuangshi16pro-lcc
}

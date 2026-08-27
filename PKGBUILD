# Maintainer: xiaomaogou66 <your-email@example.com>
# AUR-style PKGBUILD for the patched libfprint with Realtek 3274:9011 support.
# Install with: makepkg -si
# (use a proxy env if gitlab/github is slow, e.g. https_proxy=http://127.0.0.1:7897)

pkgname=libfprint-9011
pkgver=1.94.100
pkgrel=1
pkgdesc="libfprint with Realtek 3274:9011 (Mechrevo Wujie 14) driver patch"
url="https://github.com/xiaomaogou66/Mechrevo-Wujie14-Unlock"
arch=(x86_64)
license=(LGPL-2.1-or-later)
depends=(
  libgcc
  glib2
  glibc
  libgudev
  libgusb
  openssl
  pixman
)
makedepends=(
  git
  glib2-devel
  meson
  ninja
  systemd
)
provides=(libfprint libfprint-2.so)
conflicts=(libfprint)
groups=(fprint)
source=("libfprint-9011::git+https://gitlab.freedesktop.org/libfprint/libfprint.git#tag=v$pkgver"
        "realtek-9011.patch")
b2sums=('SKIP' 'SKIP')

prepare() {
  cd "$pkgname"
  patch -p1 < "$srcdir/realtek-9011.patch"
}

build() {
  local meson_options=(
    -D introspection=false
    -D gtk-examples=false
    -D doc=false
    -D installed-tests=false
  )
  arch-meson "$pkgname" build "${meson_options[@]}" || meson setup build --prefix=/usr "${meson_options[@]}"
  meson compile -C build
}

package() {
  meson install -C build --destdir "$pkgdir"
}

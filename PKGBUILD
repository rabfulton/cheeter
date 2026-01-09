# Maintainer: Rab <rab@example.com>
pkgname=cheeter-git
pkgver=r0.1
pkgrel=1
pkgdesc="Context-aware cheatsheet overlay daemon"
arch=('x86_64')
url="https://github.com/rab/cheeter"
license=('MIT')
depends=('glib2' 'gtk3' 'poppler-glib' 'at-spi2-core' 'libx11')
makedepends=('git' 'pkgconf' 'gcc' 'make')
source=("${pkgname}::git+file://${PWD}")
sha256sums=('SKIP')

pkgver() {
	cd "$pkgname"
	printf "r%s.%s" "$(git rev-list --count HEAD)" "$(git rev-parse --short HEAD)"
}

build() {
	cd "$pkgname"
	make
}

package() {
	cd "$pkgname"
	make install DESTDIR="$pkgdir" PREFIX=/usr
}

#!/bin/bash
# Ref: http://pkgs.fedoraproject.org/cgit/rpms/rpm.git/tree/rpm.spec

set -ex

# WITH_XZ='0'
# WITH_INT_BDB='1'
# WITH_PLUGINS='0'
# WITH_SANITIZER='1'
# WITH_LIBARCHIVE='0'
# WITH_LIBIMAEVM='0'
# WITH_NDB='1'

_USR='/usr'
_SYSCONFDIR='/etc'
_VAR='/var'
_LIBDIR="$(rpm --eval '%{_libdir}')"
_TARGET_PLATFORM="$(rpm --eval '%{_build}')"
PREFIX="${_USR}"
# PREFIX="$(pwd)/dist"

print_usage() {
    echo "Usage: $0 {install_dependency|build|check}" 1>&2
}

install_dependency() {
    echo 'Installing dependency packages ...'
    PACKAGES="
        libdb-devel
        coreutils
        popt
        curl
        fakechroot
        redhat-rpm-config
        gawk
        elfutils-devel
        elfutils-libelf-devel
        readline-devel
        zlib-devel
        openssl-devel
        popt-devel
        file-devel
        gettext-devel
        ncurses-devel
        bzip2-devel
        lua-devel
        libcap-devel
        libacl-devel
        xz-devel
        libarchive-devel
        binutils-devel
        automake
        libtool
        libselinux-devel
        dbus-devel
        libasan
        libubsan
        ima-evm-utils
        python-devel
        python3-devel
        which
        autoconf
        rpm-build
    "
    for PACKAGE in ${PACKAGES}; do
        dnf -y install "${PACKAGE}"
    done
}

build() {
    echo 'Building ...'

    CPPFLAGS="${CPPFLAGS} -DLUA_COMPAT_APIINTCASTS"
    CFLAGS="${CFLAGS} -DLUA_COMPAT_APIINTCASTS"
    LDFLAGS="${LDFLAGS} -L/${_LIBDIR}/libdb"
    export CPPFLAGS CFLAGS LDFLAGS

    ./autogen.sh --noconfigure
    # ./configure --prefix="${PREFIX}" --enable-python
    ./configure \
        --prefix="${PREFIX}" \
        --sysconfdir="${_SYSCONFDIR}" \
        --localstatedir="${_VAR}" \
        --sharedstatedir="${_VAR}/lib" \
        --libdir="${_LIBDIR}" \
        --build="${_TARGET_PLATFORM}" \
        --host="${_TARGET_PLATFORM}" \
        --with-vendor=redhat \
        --with-external-db \
        --with-lua \
        --with-selinux \
        --with-cap \
        --with-acl \
        --with-ndb \
        --enable-python \
        --with-crypto=openssl
    make -j8
}

check() {
    echo 'Checking ...'
    make check || (
        # Failure case
        print_failed_tests_log
        exit 1
    )
}

print_failed_tests_log() {
    echo 'Printing rpmtests.log ...'
    find tests -name rpmtests.log | while read -r LOG_FILE; do
        echo "## ========================== ##"
        echo "${LOG_FILE}"
        echo "## ========================== ##"
        cat "${LOG_FILE}"
        echo
    done
}

all() {
    install_dependency && build && check
}

ROOT_DIR="$(dirname "${0}")/.."
cd "${ROOT_DIR}"

case "$1" in
    install_dependency)
        install_dependency
        ;;
    build)
        build
        ;;
    check)
        check
        ;;
    all)
        all
        ;;
    *)
        print_usage
        ;;
esac

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
DEFAULT_PREFIX='/usr'
DEFAULT_SYSCONFDIR='/etc'
DEFAULT_LOCALSTATEDIR='/var'
DEFAULT_SHAREDSTATEDIR='/var/lib'
DEFAULT_LIBDIR="$(rpm --eval '%{_libdir}')"
DEFAULT_TARGET_PLATFORM="$(rpm --eval '%{_build}')"
ROOT_DIR="$(dirname "${0}")/.."

print_usage() {
    echo "Usage: $0 {install_dependency|build|check}" 1>&2
}

install_dependency() {
    echo 'Installing dependency packages ...'
    local packages=(
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
    )
    for package in ${packages[*]}; do
        dnf -y install "${package}"
    done
}

build() {
    echo 'Building ...'

    # To customize from outside.
    PREFIX="${PREFIX:-${DEFAULT_PREFIX}}"
    SYSCONFDIR="${SYSCONFDIR:-${DEFAULT_SYSCONFDIR}}"
    LOCALSTATEDIR="${LOCALSTATEDIR:-${DEFAULT_LOCALSTATEDIR}}"
    SHAREDSTATEDIR="${SHAREDSTATEDIR:-${DEFAULT_SHAREDSTATEDIR}}"
    LIBDIR="${LIBDIR:-${DEFAULT_LIBDIR}}"
    TARGET_PLATFORM="${TARGET_PLATFORM:-${DEFAULT_TARGET_PLATFORM}}"
    CONFIGURE_ARGS="
        --prefix=${PREFIX}
        --sysconfdir=${SYSCONFDIR}
        --localstatedir=${LOCALSTATEDIR}
        --sharedstatedir=${SHAREDSTATEDIR}
        --libdir=${LIBDIR}
        --build=${TARGET_PLATFORM}
        --host=${TARGET_PLATFORM}
        --with-vendor=redhat
        --with-external-db
        --with-lua
        --with-selinux
        --with-cap
        --with-acl
        --with-ndb
        --enable-python
        --with-crypto=openssl
    "

    CPPFLAGS="${CPPFLAGS} -DLUA_COMPAT_APIINTCASTS"
    CFLAGS="${CFLAGS} -DLUA_COMPAT_APIINTCASTS"
    LDFLAGS="${LDFLAGS} -L/${LIBDIR}/libdb"
    export CPPFLAGS CFLAGS LDFLAGS

    ./autogen.sh --noconfigure
    ./configure ${CONFIGURE_ARGS}
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
    find tests -name rpmtests.log | while read -r log_file; do
        echo "## ========================== ##"
        echo "${log_file}"
        echo "## ========================== ##"
        cat "${log_file}"
        echo
    done
}

check-all() {
    # TODO Create array object of hash.
    # declare -A test_cases
    #
    # test_cases=(
    #     (
    #         [name]='test_for_system_install'
    #     )
    #     (
    #         [name]='test_for_user_install'
    #         [env]=(
    #             [CONFIGURE_ARGS]='--prefix ${ROOT_DIR}/dist'
    #         )
    #     )
    # )

    install_dependency
    build
    check
}

cd "${ROOT_DIR}"

case "${1}" in
    install_dependency)
        install_dependency
        ;;
    build)
        build
        ;;
    check)
        check
        ;;
    check-all)
        check-all
        ;;
    *)
        print_usage
        ;;
esac

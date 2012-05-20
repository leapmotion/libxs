#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
AC_PREREQ([2.64])
#
# The Crossroads version number is extracted from include/xs.h using
# the version.sh script. Hence, it should be updated there.
# The version in git should reflect the *next* version planned.
#
AC_INIT([libxs], [m4_esyscmd([./version.sh])],
    [crossroads-dev@groups.crossroads.io], [libxs], [http://www.crossroads.io/])

AC_CONFIG_AUX_DIR([config])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([src/platform.hpp])
AM_INIT_AUTOMAKE([tar-ustar dist-zip foreign])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

# ABI version for libxs (Libtool -version-info)
#
# Don't change this unless you know exactly what you're doing and have read and
# understand:
# http://www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html
#
# libxs -version-info current:revision:age
LIBXS_ABI_VERSION="2:0:0"
AC_SUBST(LIBXS_ABI_VERSION)

# ABI version for XS libzmq compatibility library
#
# This must be set to the ABI version of libzmq we are emulating.
# This is currently libzmq 2.1 which is ABI 1.0.0.
#
# libzmq -version-info current:revision:age
LIBZMQ_ABI_VERSION="1:0:0"
AC_SUBST([LIBZMQ_ABI_VERSION])

# Take a copy of original flags
XS_ORIG_CFLAGS="${CFLAGS:-none}"
XS_ORIG_CPPFLAGS="${CPPFLAGS:-none}"
XS_ORIG_CXXFLAGS="${CXXFLAGS:-none}"

# Checks for programs.
AC_PROG_CC
AC_PROG_CC_C99
AC_PROG_CXX
AM_PROG_CC_C_O
AC_PROG_SED
AC_PROG_AWK

# Enable system extensions
AC_USE_SYSTEM_EXTENSIONS

# Libtool configuration for different targets. See acinclude.m4
AC_ARG_VAR([XMLTO], [Path to xmlto command])
AC_PATH_PROG([XMLTO], [xmlto])
AC_ARG_VAR([ASCIIDOC], [Path to asciidoc command])
AC_PATH_PROG([ASCIIDOC], [asciidoc])
LIBXS_CONFIG_LIBTOOL
LT_INIT([win32-dll])

# Check whether to build a with debug symbols
LIBXS_CHECK_ENABLE_DEBUG

# Check wheter to enable code coverage
LIBXS_WITH_GCOV

# Checks for libraries.
AX_PTHREAD([
    LIBS="$PTHREAD_LIBS $LIBS"
    CFLAGS="$CFLAGS $PTHREAD_CFLAGS"
    CXXFLAGS="$CXXFLAGS $PTHREAD_CFLAGS"
    CC="$PTHREAD_CC"
])

AC_CHECK_LIB([rt], [clock_gettime])

# Set pedantic
libxs_pedantic="yes"

# By default compiling with -Werror except OSX.
libxs_werror="yes"

# By default use DSO visibility
libxs_dso_visibility="yes"

# Whether we are on mingw or not.
libxs_on_mingw32="no"

# For host type checks
AC_CANONICAL_HOST

# OS-specific tests
AS_CASE(["${host_os}"],
    [*linux*], [
        AC_DEFINE([XS_HAVE_LINUX], [1], [Have Linux OS])

        AS_CASE(["${host_os}"],
            [*android*], [
                AC_DEFINE([XS_HAVE_ANDROID], [1], [Have Android OS])
            ]
        )
     ],
    [*solaris*], [
        AC_DEFINE([XS_HAVE_SOLARIS], [1], [Have Solaris OS])
        AC_CHECK_LIB([socket], [socket])
        AC_CHECK_LIB([nsl], [gethostbyname])
    ],
    [*freebsd*], [
        AC_DEFINE([XS_HAVE_FREEBSD], [1], [Have FreeBSD OS])
    ],
    [*darwin*], [
        libxs_pedantic="no"
        libxs_werror="no"
        AC_DEFINE([XS_HAVE_OSX], [1], [Have DarwinOSX OS])
        AC_LANG_PUSH([C++])
        LIBXS_CHECK_LANG_FLAG_PREPEND([-Wno-uninitialized])
        AC_LANG_POP([C++])
     ],
    [*netbsd*], [
        AC_DEFINE([XS_HAVE_NETBSD], [1], [Have NetBSD OS])
    ],
    [openbsd*], [
        AC_DEFINE([XS_HAVE_OPENBSD], [1], [Have OpenBSD OS])
    ],
    [*nto-qnx*], [
        libxs_pedantic="no"
        AC_DEFINE([XS_HAVE_QNXNTO], [1], [Have QNX Neutrino OS])
        AC_CHECK_LIB([socket], [socket])
    ],
    [*aix*], [
        AC_DEFINE([XS_HAVE_AIX], [1], [Have AIX OS])
    ],
    [*hpux*], [
        AC_DEFINE([XS_HAVE_HPUX], [1], [Have HPUX OS])
        LIBXS_CHECK_LANG_FLAG_PREPEND([-Ae])
        AC_CHECK_FUNCS([gethrtime])
    ],
    [*mingw32*], [
        AC_DEFINE([XS_HAVE_WINDOWS], [1], [Have Windows OS])
        AC_DEFINE([XS_HAVE_MINGW32], [1], [Have MinGW32])
        AC_CHECK_HEADERS([windows.h])
        AC_CHECK_LIB([ws2_32], [main], [],
            [AC_MSG_ERROR([cannot link with ws2_32.dll.])])
        AC_CHECK_LIB([rpcrt4], [main], [],
            [AC_MSG_ERROR([cannot link with rpcrt4.dll.])])
        AC_CHECK_LIB([iphlpapi], [main], [],
            [AC_MSG_ERROR([cannot link with iphlpapi.dll.])])
        # mingw32 defines __int64_t as long long
        AC_LANG_PUSH([C++])
        LIBXS_CHECK_LANG_FLAG_PREPEND([-Wno-long-long])
        AC_LANG_POP([C++])
        libxs_on_mingw32="yes"
        libxs_dso_visibility="no"

        AS_IF([test "x$enable_static" = "xyes"], [
            AC_MSG_ERROR([Building static libraries is not supported under MinGW32])
        ])
    ],
    [*cygwin*], [
        AC_DEFINE([XS_HAVE_CYGWIN], [1], [Have Cygwin])
        AS_IF([test "x$enable_static" = "xyes"], [
            AC_MSG_ERROR([Building static libraries is not supported under Cygwin])
        ])
    ],
    [AC_MSG_WARN([unsupported system: ${host_os}.])]
)

#
# Check if the compiler supports -fvisibility=hidden flag. MinGW32 uses __declspec
#
AS_IF([test "x$libxs_dso_visibility" = "xyes"], [
    AC_LANG_PUSH([C++])
    LIBXS_CHECK_LANG_VISIBILITY([LIBXS_EXTRA_CXXFLAGS="$libxs_cv_[]_AC_LANG_ABBREV[]_visibility_flag ${LIBXS_EXTRA_CXXFLAGS}"])
    AC_LANG_POP([C++])
])

# CPU-specific optimizations
AS_CASE(["${host_cpu}"],
    [*sparc*], [
        AC_LANG_PUSH([C++])
        LIBXS_CHECK_LANG_FLAG_PREPEND([-mcpu=v9])
        AC_LANG_POP([C++])
    ]
)

# Check whether to build docs / install man pages
LIBXS_CHECK_DOC_BUILD

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([ \
    link.h \
    dirent.h \
    dlfcn.h \
    errno.h \
    fcntl.h \
    arpa/inet.h \
    netinet/tcp.h \
    netinet/in.h \
    stddef.h \
    stdlib.h \
    string.h \
    sys/socket.h \
    sys/stat.h \
    sys/time.h \
    sys/types.h \
    time.h \
    unistd.h \
    limits.h
])

# Check if we have ifaddrs.h header file.
AC_CHECK_HEADERS([ifaddrs.h],
    [AC_DEFINE([XS_HAVE_IFADDRS], [1], [Have ifaddrs.h header.])])

###############################################################################
# Check available atomic implementations
#
# We have following variants:
#   o  gcc __sync_*() [XS_ATOMIC_GCC_SYNC]
#   o  optional atomic.h with atomic_cas_32() on solaris and netbsd [XS_ATOMIC_SOLARIS]
#   o  windows specific [XS_HAVE_WINDOWS]
#   o  handwritten inline asm for x86 and arm [no specific define]
#   o  fallback to mutexes (or force by hands) [no specific define]

#
# Check for Solaris and NetBSD style atomic.h
#
AC_LANG_PUSH([C++])
AC_MSG_CHECKING([for Solaris/NetBSD-style atomic.h])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
    [[#include <atomic.h>]],
    [[uint32_t value;
      atomic_cas_32 (&value, 0, 0);
      return 0;]])],
    [solaris_style_atomic=yes],
    [solaris_style_atomic=no])
AC_MSG_RESULT([$solaris_style_atomic])
AS_IF([test "x$solaris_style_atomic" = "xyes"], [
    AC_DEFINE([XS_ATOMIC_SOLARIS], [1], [Solaris/NetBSD-style atomic.h with atomic_cas_32()])
])
AC_LANG_POP([C++])

#
# Check for GCC-style __sync_*() builtins
#
AC_LANG_PUSH([C++])
AC_MSG_CHECKING([for GCC-style __sync_*() atomic builtins])
AC_LINK_IFELSE([AC_LANG_PROGRAM(
    [volatile void* p;],
    [[  int r;
        r = __sync_bool_compare_and_swap(&p, (void*) 0x12345, (void*) 0);
        return r;]])],
    [gcc_style_sync_atomic=yes],
    [gcc_style_sync_atomic=no])
AC_MSG_RESULT([$gcc_style_sync_atomic])
AS_IF([test "x$gcc_style_sync_atomic" = "xyes"], [
    AC_DEFINE([XS_ATOMIC_GCC_SYNC], [1], [GCC-style __sync_*() atomic builtins])
])
AC_LANG_POP([C++])


###############################################################################
# Check polling system                                                        #
###############################################################################

# Allow users to override the polling system
AC_ARG_WITH([poller],
    [AS_HELP_STRING([--with-poller],
        [choose polling system manually. valid values are kqueue, epoll, devpoll, poll or select [default=autodetect]])],
    [], [with_poller=autodetect])

# Check the various polling systems
LIBXS_CHECK_KQUEUE
LIBXS_CHECK_EPOLL
LIBXS_CHECK_DEVPOLL
LIBXS_CHECK_POLL
LIBXS_CHECK_SELECT

# Select the best available poller
AS_CASE([${with_poller}],
    [kqueue], [
        AS_IF([test x$acx_cv_have_kqueue != xyes], [
            AC_MSG_ERROR([kqueue() poller selected but not available])
        ])
        AC_DEFINE([XS_FORCE_KQUEUE], [1], [Forces use of kqueue()])
        libxs_cv_poller=kqueue
    ],
    [epoll], [
        AS_IF([test x$acx_cv_have_epoll != xyes], [
            AC_MSG_ERROR([epoll() poller selected but not available])
        ])
        AC_DEFINE([XS_FORCE_EPOLL], [1], [Forces use of epoll()])
        libxs_cv_poller=epoll
    ],
    [devpoll], [
        AS_IF([test x$acx_cv_have_devpoll != xyes], [
            AC_MSG_ERROR([/dev/poll poller selected but not available])
        ])
        AC_DEFINE([XS_FORCE_DEVPOLL], [1], [Forces use of /dev/poll])
        libxs_cv_poller=devpoll
    ],
    [poll], [
        AS_IF([test x$acx_cv_have_poll != xyes], [
            AC_MSG_ERROR([poll() poller selected but not available])
        ])
        AC_DEFINE([XS_FORCE_POLL], [1], [Forces use of poll()])
        libxs_cv_poller=poll
    ],
    [select], [
        AS_IF([test x$acx_cv_have_select != xyes], [
            AC_MSG_ERROR([select() poller selected but not available])
        ])
        AC_DEFINE([XS_FORCE_SELECT], [1], [Forces use of select()])
        libxs_cv_poller=select
    ],
    [autodetect], [
        AS_IF([test x$acx_cv_have_kqueue = xyes], [
            libxs_cv_poller=kqueue
        ], [test x$acx_cv_have_epoll = xyes], [
            libxs_cv_poller=epoll
        ], [test x$acx_cv_have_devpoll = xyes], [
            libxs_cv_poller=devpoll
        ], [test x$acx_cv_have_poll = xyes], [
            libxs_cv_poller=poll
        ], [test x$acx_cv_have_select = xyes], [
            libxs_cv_poller=select
        ], [
            AC_MSG_ERROR([no suitable polling system found])
        ])
    ],
    [*], [
        AC_MSG_ERROR([Invalid poller selected: ${with_poller}])
    ]
)

# Force not to use eventfd
AC_ARG_ENABLE([eventfd],
    [AS_HELP_STRING([--disable-eventfd], [disable eventfd [default=no]])],
    [xs_disable_eventfd=yes], [xs_disable_eventfd=no])

AS_IF([test "x$xs_disable_eventfd" != "xyes"], [
    # Check if we have eventfd.h header file.
    AC_CHECK_HEADERS([sys/eventfd.h],
        [AC_DEFINE([XS_HAVE_EVENTFD], [1], [Have eventfd extension.])])
])

# Use c++ in subsequent tests
AC_LANG_PUSH([C++])

AC_HEADER_STDBOOL
AC_C_CONST
AC_C_INLINE

# Checks for typedefs, structures, and compiler characteristics.
AS_IF([test "x$libxs_cv_[]_AC_LANG_ABBREV[]_intel_compiler" = "xyes"], [
    dnl 279: controlling expression is constant
    dnl Fixes build with ICC 12.x
    LIBXS_CHECK_WITH_FLAG([-wd279], [AC_TYPE_SIZE_T])
    LIBXS_CHECK_WITH_FLAG([-wd279], [AC_TYPE_SSIZE_T])
], [
    AC_TYPE_SIZE_T
    AC_TYPE_SSIZE_T
])

AC_TYPE_UINT32_T
AC_C_VOLATILE

#  libzmq compatibility mode
libxs_libzmq="no"
AC_ARG_ENABLE([libzmq], [AS_HELP_STRING([--enable-libzmq],
    [install libzmq compatibility library and headers [default=no]])],
    [enable_libzmq=$enableval], [enable_libzmq=no])

AS_IF([test "x$enable_libzmq" != "xno"], [
    AC_MSG_NOTICE([enabling libzmq compatibility library and headers])
    libxs_libzmq="yes"
])

# Plug-in support
AC_ARG_ENABLE([plugins], [AS_HELP_STRING([--enable-plugins],
    [enable plugins [default=auto]])],
    [enable_plugins=$enableval], [enable_plugins=auto])

AS_IF([test "x$enable_plugins" != "xno"], [
    # Check if libdl is required, then check for dlopen and lstat, they are
    # both needed to enable plug-in support
    AC_SEARCH_LIBS([dlopen], [dl])
    AC_CHECK_FUNCS([lstat dlopen], [], [
        AS_IF([test "x$enable_plugins" = "xyes"], [
            AC_MSG_ERROR([Plugins support not available])
        ], [
            AC_MSG_WARN([Plugins support disabled])
            enable_plugins=no
        ])
    ])

    # If shared library support is disabled disable plugins support
    AS_IF([test "x$enable_shared" = "xno"], [
        AC_MSG_WARN([Shared library support disabled, disabling plug-ins])
        enable_plugins=no
    ], [
        enable_plugins=yes
        AC_DEFINE([XS_HAVE_PLUGINS], [1], [Whether plugins support is enabled])
    ])
])

#  PGM extension
libxs_pgm_ext="no"

pgm_basename="libpgm-5.1.118~dfsg"

AC_ARG_WITH([pgm], [AS_HELP_STRING([--with-pgm],
    [build libxs with PGM extension [default=no]])],
    [with_pgm_ext=$withval], [with_pgm_ext=no])

# build using system pgm
AC_ARG_WITH([system-pgm], [AS_HELP_STRING([--with-system-pgm],
    [build libxs with PGM extension. Requires pkg-config [default=no]])],
    [with_system_pgm_ext=yes], [with_system_pgm_ext=no])

AS_IF([test "x$with_pgm_ext" != "xno" \
    && test "x$with_system_pgm_ext" != "xno"],
[
    AC_MSG_ERROR([--with-pgm and --with-system-pgm cannot be specified together])
])

AS_IF([test "x$with_pgm_ext" != "xno"], [
    # This allows placing the tar.gz to foreign/openpgm
    # and using ./configure --with-pgm=libpgm-x.y.z
    AS_IF([test "x$with_pgm_ext" != "xyes"], [
        pgm_basename="$with_pgm_ext"
    ])

    # Unpack libpgm
    AC_MSG_NOTICE([Unpacking ${pgm_basename}.tar.gz])
    libxs_pwd=`pwd`
    cd foreign/openpgm

    AS_IF([! (gzip -dc "${pgm_basename}.tar.gz" || echo "failed") | ${am__untar}], [
        AC_MSG_ERROR([cannot unpack the foreign/openpgm/${pgm_basename}.tar.gz file])
    ])

    cd "${libxs_pwd}"

    AS_IF([test -d foreign/openpgm/build-staging], [
        rm -rf foreign/openpgm/build-staging
    ])

    mv foreign/openpgm/${pgm_basename} foreign/openpgm/build-staging
    pgm_srcdir=foreign/openpgm/build-staging/openpgm/pgm

    AS_IF([! test -d foreign/openpgm/build-staging/openpgm/pgm/config], [
        mkdir foreign/openpgm/build-staging/openpgm/pgm/config
    ])

    # DSO symbol visibility for openpgm
    AC_LANG_PUSH([C])
    LIBXS_CHECK_LANG_VISIBILITY([ac_configure_args="CFLAGS='$libxs_cv_[]_AC_LANG_ABBREV[]_visibility_flag' ${ac_configure_args}"])
    AC_LANG_POP([C])

    pgm_subdir=build-staging/openpgm/pgm
    AC_SUBST([pgm_subdir])

    AC_SUBST([pgm_srcdir])
    AC_CONFIG_SUBDIRS([foreign/openpgm/build-staging/openpgm/pgm/])

    # Success!
    AC_DEFINE([XS_HAVE_OPENPGM], [1], [Have OpenPGM extension])
    libxs_pgm_ext="yes"
])

# Build with system openpgm
AS_IF([test "x$with_system_pgm_ext" != "xno"], [
    m4_ifdef([PKG_CHECK_MODULES], [
        PKG_CHECK_MODULES([OpenPGM], [openpgm-5.1 >= 5.1])
        AC_DEFINE([XS_HAVE_OPENPGM], [1], [Have OpenPGM extension])
        LIBXS_EXTRA_CXXFLAGS="$OpenPGM_CFLAGS $LIBXS_EXTRA_CXXFLAGS"
        LIBS="$OpenPGM_LIBS $LIBS"
    ],
    [AC_MSG_ERROR([--with-system-pgm requires a working pkg-config installation])])
])

AC_SUBST([pgm_basename])

# Set -Wall, -Werror and -pedantic
AC_LANG_PUSH([C++])

# Check how to enable -Wall
LIBXS_LANG_WALL([CPPFLAGS="$libxs_cv_[]_AC_LANG_ABBREV[]_wall_flag $CPPFLAGS"])

AS_IF([test "x$libxs_werror" = "xyes" \
    && test "x$libxs_cv_[]_AC_LANG_ABBREV[]_sun_studio_compiler" != "xyes"],
[
    LIBXS_LANG_WERROR([CPPFLAGS="$libxs_cv_[]_AC_LANG_ABBREV[]_werror_flag $CPPFLAGS"])
])

AS_IF([test "x$libxs_pedantic" = "xyes"], [
    LIBXS_LANG_STRICT([CPPFLAGS="$libxs_cv_[]_AC_LANG_ABBREV[]_strict_flag $CPPFLAGS"])
])
AC_LANG_POP([C++])

AM_CONDITIONAL([BUILD_LIBZMQ], [test "x$libxs_libzmq" = "xyes"])
AM_CONDITIONAL([BUILD_PGM], [test "x$libxs_pgm_ext" = "xyes"])
AM_CONDITIONAL([ON_MINGW], [test "x$libxs_on_mingw32" = "xyes"])

# Checks for library functions.
AC_CHECK_FUNCS([ \
    perror \
    gettimeofday \
    clock_gettime \
    memset \
    socket \
    getifaddrs \
    freeifaddrs
])

LIBXS_CHECK_SOCK_CLOEXEC([
    AC_DEFINE([XS_HAVE_SOCK_CLOEXEC], [1],
        [Whether SOCK_CLOEXEC is defined and functioning.])
])

# Subst LIBXS_EXTRA_CFLAGS & CXXFLAGS & LDFLAGS
AC_SUBST([LIBXS_EXTRA_CFLAGS])
AC_SUBST([LIBXS_EXTRA_CXXFLAGS])
AC_SUBST([LIBXS_EXTRA_LDFLAGS])

AC_CONFIG_FILES([
    Makefile
    src/Makefile
    doc/Makefile
    perf/Makefile
    src/libxs.pc
    builds/msvc/Makefile
    tests/Makefile
    foreign/openpgm/Makefile
])

AS_IF([test "x$libxs_libzmq" == "xyes"], [
    AC_CONFIG_FILES([src/libzmq.pc])
])

AC_OUTPUT

# Print configuration
cat <<EOF | tee -a config.log

## -------------- ##
## Configuration. ##
## -------------- ##

Configuration for $PACKAGE_STRING
    Host OS: $host_os
    Debugging information: ${enable_debug-no}
    GCC code coverage reporting: ${XS_GCOV-no}
    Polling system: $libxs_cv_poller
    Disable eventfd: $xs_disable_eventfd
    Build libzmq compatibility library and headers: $libxs_libzmq
    Plugins support: $enable_plugins
    PGM extension: $with_pgm_ext
    Use system-provided PGM library: $with_system_pgm_ext
    Build documentation: $libxs_build_doc

Environment variables:
    CPPFLAGS: $CPPFLAGS
    CFLAGS: $CFLAGS
    CXXFLAGS: $CXXFLAGS
    LDFLAGS: $LDFLAGS

Installation:
    Prefix: $prefix
    Install man pages: $libxs_install_man

EOF

AS_IF([test "x$libxs_libzmq" == "xyes"], [
    AC_MSG_WARN([The --enable-libzmq option was specified:])
    AC_MSG_WARN([libzmq compatibility library and headers are enabled.])
    AC_MSG_WARN([This will overwrite any existing installation of libzmq.])
])

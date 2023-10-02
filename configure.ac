#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([fanplayer], [1.0.0], [chenk@apical.com.cn])

AC_CONFIG_SRCDIR([fanplayer.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile fanplayer.pc])

AM_INIT_AUTOMAKE([subdir-objects])

# Checks for programs.
AC_PROG_CC
LT_INIT

# Checks for pkg-config
PKG_PROG_PKG_CONFIG

# Checks for libraries.
AC_CHECK_LIB([m], [sqrt])
AC_CHECK_LIB([pthread], [pthread_create])

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h stdint.h stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_CHECK_FUNCS([clock_gettime ftruncate getcwd inet_ntoa localtime_r memset mkdir pow select socket sqrt strcasecmp strstr strtol])

PKG_CHECK_MODULES(LIBAVUTILS, [libavutil >= 55.78.100])
LIBAVUTILS_VERSION=`$PKG_CONFIG libavutil --modversion`
AC_MSG_NOTICE("using libavutil version $LIBAVUTILS_VERSION")
CFLAGS="$CFLAGS `$PKG_CONFIG libavutil --cflags`"
LDFLAGS="$LDFLAGS `$PKG_CONFIG libavutil --libs`"

PKG_CHECK_MODULES(LIBAVSWRESAMPLE, [libswresample >= 2.9.100])
LIBAVSWRESAMPLE_VERSION=`$PKG_CONFIG libswresample --modversion`
AC_MSG_NOTICE("using libswresample version $LIBAVSWRESAMPLE_VERSION")
CFLAGS="$CFLAGS `$PKG_CONFIG libswresample --cflags`"
LDFLAGS="$LDFLAGS `$PKG_CONFIG libswresample --libs`"

PKG_CHECK_MODULES(LIBAVSWSCALE, [libswscale >= 4.8.100])
LIBAVSWSCALE_VERSION=`$PKG_CONFIG libswscale --modversion`
AC_MSG_NOTICE("using libswscale version $LIBAVSWSCALE_VERSION")
CFLAGS="$CFLAGS `$PKG_CONFIG libswscale --cflags`"
LDFLAGS="$LDFLAGS `$PKG_CONFIG libswscale --libs`"

PKG_CHECK_MODULES(LIBAVCODEC, [libavcodec >= 57.107.100])
LIBAVCODEC_VERSION=`$PKG_CONFIG libavcodec --modversion`
AC_MSG_NOTICE("using libavcodec version $LIBAVCODEC_VERSION")
CFLAGS="$CFLAGS `$PKG_CONFIG libavcodec --cflags`"
LDFLAGS="$LDFLAGS `$PKG_CONFIG libavcodec --libs`"

PKG_CHECK_MODULES(LIBAVFORMAT, [libavformat >= 57.83.100])
LIBAVFORMAT_VERSION=`$PKG_CONFIG libavformat --modversion`
AC_MSG_NOTICE("using libavformat version $LIBAVFORMAT_VERSION")
CFLAGS="$CFLAGS `$PKG_CONFIG libavformat --cflags`"
LDFLAGS="$LDFLAGS `$PKG_CONFIG libavformat --libs`"

# Checks for host enviroment
AC_CANONICAL_HOST
case $host_os in
  *cygwin* | *mingw*)
    win32="yes"
  ;;
  *linux*)
    linux="yes"
  ;;
esac
AM_CONDITIONAL([WIN32], [test "x$win32" = "xyes"])
AM_CONDITIONAL([LINUX], [test "x$linux" = "xyes"])

# Checks for Optional Features
AC_ARG_ENABLE(debug, [AS_HELP_STRING([--enable-debug], [enable debug mode [default=no]])], [], [debug=no])
AM_CONDITIONAL([ENABLE_DEBUG], [test x"$enable_debug" = x"yes"])

# Checks for Optional Packages
# Checks for libavdev
AC_ARG_WITH([libavdev], [AS_HELP_STRING([--with-libavdev], [--with-libavdev, use libavdev for audio & video rendering])], [], [with_libavdev="no"])
AS_IF([test "x$with_libavdev" != "xno"],
  [ PKG_CHECK_MODULES(LIBAVDEV, [libavdev >= 1.0.0])
    LIBAVDEV_VERSION=`$PKG_CONFIG libavdev --modversion`
    AC_MSG_NOTICE("using libavdev version $LIBAVDEV_VERSION")
    CFLAGS="$CFLAGS `$PKG_CONFIG libavdev --cflags`"
    LDFLAGS="$LDFLAGS `$PKG_CONFIG libavdev --libs`"
  ])
AM_CONDITIONAL([WITH_LIBAVDEV], [test x"$with_libavdev" == x"yes"])

AM_CONDITIONAL([DISABLE_SHARED], [test x"$enable_shared" == x"no"])

AC_OUTPUT

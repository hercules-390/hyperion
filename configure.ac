AC_INIT(hercules.h)
AC_CONFIG_AUX_DIR(autoconf)

AM_INIT_AUTOMAKE(hercules,2.14a)

dnl We can switch to using a config.h once every source file includes it
dnl AM_CONFIG_HEADER(autoconf/config.h)

AC_CANONICAL_HOST

CFLAGS="-Wall $CFLAGS"

# Programs
AC_PROG_CC

# OS-specific tests

case "$host_os" in
	linux*)
		build_hercifc=true
		;;
	*)
		build_hercifc=false
		;;
esac

AM_CONDITIONAL(BUILD_HERCIFC, [test "$build_hercifc" = "true"])

# Libraries
AC_CHECK_LIB(pthread, pthread_create)
AC_CHECK_LIB(m, sqrt)
dnl This will only work for zlib >= 1.0.2
dnl AC_CHECK_LIB(z, zlibVersion)
AC_CHECK_LIB(z, main)
AC_CHECK_LIB(bz2, BZ2_bzBuffToBuffDecompress,
[have_libbz2=yes],[have_libbz2=no])

# Headers
AC_CHECK_HEADERS(byteswap.h linux/if_tun.h inttypes.h)
AC_CHECK_HEADER(fenv.h,,[AC_DEFINE([NO_IEEE_SUPPORT])])

# Compiler characteristics

# It would be nice if we could check for __attribute__((regparm))
# automatically.  For now, disable it when GCC is not in use, and when
# on non-x86 architectures (below)
if test x$GCC != xyes; then
  AC_DEFINE(NO_ATTR_REGPARM)
fi

case "$host_cpu" in
	  i586|i686) ;;
  	  *)
	    # Don't try to use the regparm attribute on non-i386
            # (avoids many compiler warnings)
	    AC_DEFINE(NO_ATTR_REGPARM)

            # byteswap assembler routines are i386-only
            AC_DEFINE(NO_ASM_BYTESWAP)
	    ;;
esac

AC_ARG_ENABLE(cckd-bzip2,
[  --enable-cckd-bzip2         enable bzip2 compression for emulated DASD],
[case "${enableval}" in
  yes) cckd_bzip2=yes ;;
  no) cckd_bzip2=no ;;
  *) AC_MSG_ERROR(bad value ${enableval} for --enable-cckd-bzip2) ;;
esac],[cckd_bzip2=$have_libbz2])

AC_ARG_ENABLE(het-bzip2,
[  --enable-het-bzip2          enable bzip2 compression for emulated tapes],
[case "${enableval}" in
  yes) het_bzip2=yes ;;
  no) het_bzip2=no ;;
  *) AC_MSG_ERROR(bad value ${enableval} for --enable-het-bzip2) ;;
esac],[het_bzip2=$have_libbz2])

if test $cckd_bzip2 = yes -o $het_bzip2 = yes; then
  if test $have_libbz2 != yes; then
    AC_MSG_ERROR(bzip2 compression requested but libbz2 not found)
  else
    LIBS="-lbz2 $LIBS"
  fi
fi

if test $cckd_bzip2 = yes; then
  AC_DEFINE(CCKD_BZIP2)
fi

if test $het_bzip2 = yes; then
  AC_DEFINE(HET_BZIP2)
fi

# Library functions

AC_CHECK_FUNCS(sqrtl)

# User-definable settings

AC_ARG_ENABLE(profiling,
[  --enable-profiling          enable profiling],
[case "${enableval}" in
  yes) CFLAGS="-pg -DPROFILE_CPU $CFLAGS" ;;
  no) ;;
  *) AC_MSG_ERROR([bad value ${enableval} for --enable-profiling]) ;;
esac])

AC_ARG_ENABLE(debug,
[  --enable-debug              enable debugging],
[case "${enableval}" in
  yes)
    debug=true
    CFLAGS="-g $CFLAGS"
    ;;
  no) debug=false ;;
  *) AC_MSG_ERROR([bad value ${enableval} for --enable-debug]) ;;
esac],
[debug=false])

AC_ARG_ENABLE(setuid-hercifc,
[  --enable-setuid-hercifc     install hercifc setuid root],
[case "${enableval}" in
  yes) setuid_hercifc=yes ;;
  no)  setuid_hercifc=no ;;
  *) AC_MSG_ERROR(bad value ${enableval} for --enable-setuid-hercifc) ;;
esac],[setuid_hercifc=no])
AM_CONDITIONAL(SETUID_HERCIFC, test x$setuid_hercifc = xyes)

AC_MSG_CHECKING([for optimization flags to use])

AC_ARG_ENABLE(optimization,
[  --enable-optimization[=FLAGS] enable optimization],
[case "${enableval}" in
  yes) optimize=true ;;
  no) optimize=false ;;
  *) optimize=false; OFLAGS="${enableval}" ;;
esac],
[optimize=true])

if test x$optimize = xtrue -a x$debug != xtrue; then
  case "$host_cpu" in
	  i586|i686)
	    if test "$GCC" = "yes"; then
		OFLAGS="-O3 -march=$host_cpu -fomit-frame-pointer $OFLAGS"
	    else
                OFLAGS="-O2 $OFLAGS"
	    fi
	    ;;
	  *)
	    OFLAGS="-O2 $OFLAGS"
	    ;;
  esac
fi

if test "x$OFLAGS" = x; then
  AC_MSG_RESULT([(none)])
else
  AC_MSG_RESULT([$OFLAGS])
  CFLAGS="$OFLAGS $CFLAGS"
fi

AH_TEMPLATE([NO_ATTR_REGPARM],
	[Define if your gcc does not support __attribute__((regparm(n)))])
AH_TEMPLATE([NO_ASM_BYTESWAP],
	[Define to disable assembler routines for byte swapping])
AH_TEMPLATE([NO_IEEE_SUPPORT],
	[Define to disable IEEE floating point support])
AH_TEMPLATE([CCKD_BZIP2],
	[Define to enable bzip2 compression in emulated DASDs])
AH_TEMPLATE([HET_BZIP2],
	[Define to enable bzip2 compression in emulated tapes])

AC_OUTPUT(Makefile util/Makefile html/Makefile)

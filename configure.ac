AC_INIT(hercules.h)
AC_CONFIG_AUX_DIR(autoconf)

AM_INIT_AUTOMAKE(hercules,2.16a.1)
AM_CONFIG_HEADER(config.h)
AM_MAINTAINER_MODE

AC_CANONICAL_HOST

# Initialization
CFLAGS="-Wall $CFLAGS"

# Programs
AC_PROG_CC

# OS-specific settings that we can't figure out any other way (yet)

case "$host_os" in
	linux*)
		build_hercifc=true
		;;
        *cygwin*)
		build_hercifc=false
		AC_DEFINE([WIN32])
		AC_DEFINE([NO_SIGABEND_HANDLER])
		;;
	*)
		build_hercifc=false
		;;
esac

# Arch-specific settings that we can't figure out any other way (yet)

case "$host_cpu-$GCC" in
        hppa*-yes)
                CFLAGS="$CFLAGS -ffunction-sections"
        ;;
esac

AM_CONDITIONAL(BUILD_HERCIFC, [test "x$build_hercifc" = "xtrue"])

# Libraries
AC_CHECK_LIB(m, sqrt)
AC_CHECK_LIB(socket, connect)
AC_CHECK_LIB(nsl, gethostbyname)
AC_CHECK_LIB(resolv, inet_aton)
dnl This will only work for zlib >= 1.0.2
dnl AC_CHECK_LIB(z, zlibVersion)
AC_CHECK_LIB(z, main)
AC_CHECK_LIB(bz2, BZ2_bzBuffToBuffDecompress,
[have_libbz2=yes],[have_libbz2=no])

AC_ARG_ENABLE(fthreads,
              AC_HELP_STRING([--enable-fthreads],
                             [use included fthreads instead of libpthread]),
              [case "${host_os},${enableval}" in
                 *cygwin*,yes)
                   build_fthreads=true
                   AC_DEFINE([OPTION_FTHREADS])
                   ;;
                 *,yes)
                   AC_MSG_ERROR([fthreads are only for win32])
                   ;;
                 *)
                   ;;
               esac],
              [AC_CHECK_LIB(pthread, pthread_create,
               [],
               [case "$host_os" in
                  *cygwin*)
                    # Alternate pthreads location for older cygwin
                    AC_MSG_NOTICE([Looking for pthreadw32 in /usr/DLL])
                    LDFLAGS_SAVE="$LDFLAGS"
                    LDFLAGS="$LDFLAGS -L/usr/DLL"
                    AC_CHECK_LIB(pthreadw32, pthread_create,
                                 [],
                                 [LDFLAGS="$LDFLAGS_SAVE"])
                    ;;
                esac]) ])

AM_CONDITIONAL(BUILD_FTHREADS, [test x$build_fthreads = xtrue])

dnl AC_MSG_NOTICE([Checking to see if -pthread is needed to compile])

AC_MSG_CHECKING([whether ${CC-cc} accepts -pthread])

echo 'void f(){}' >conftest.c
if test -z "`${CC-cc} -pthread -c conftest.c 2>&1`"; then
        LIBS="-pthread $LIBS"
        AC_MSG_RESULT([yes])
else
        AC_MSG_RESULT([no])
fi
rm -f conftest*

# Headers
AC_CHECK_HEADERS(byteswap.h)
AC_CHECK_HEADER(fenv.h,,[AC_DEFINE([NO_IEEE_SUPPORT])])

if test x$build_fthreads != xtrue; then
  AC_CHECK_HEADER(pthread.h,
                  [],
                  [case "$host_os" in
                     *cygwin*)
                       # Alternate pthreads location for older cygwin
                       AC_MSG_NOTICE([Looking for pthread.h in /usr/Pthreads])
                       CFLAGS_SAVE="$CFLAGS"
                       CFLAGS="$CFLAGS -I/usr/Pthreads"
                       AC_CHECK_HEADER(pthread.h,
                                       [],
                                       [CFLAGS="$CFLAGS_SAVE"
                                        AC_DEFINE([NOTHREAD])
                                        AC_MSG_WARN([Unable to find pthread.h, disabling threads]) ])
                       ;;
                     *)
                       AC_MSG_WARN([Unable to find pthread.h, disabling threads])
                       AC_DEFINE([NOTHREAD])
                       ;;
                   esac])
fi

dnl AC_MSG_NOTICE([Checking how to get fixed-size data types])
AC_CHECK_HEADER(inttypes.h,
                [AC_DEFINE([HAVE_INTTYPES_H])],
                [AC_CHECK_TYPE(u_int8_t,
                               [AC_DEFINE([HAVE_U_INT])],
                               [AC_MSG_ERROR([Unable to find fixed-size data types])] )] )

# Compiler characteristics

AC_C_BIGENDIAN

# GCC only seems to support regparm on i386 right now, but that could
# change in the future.
AC_CACHE_CHECK([whether gcc supports __attribute__((regparm(n)))],
               [ac_cv_attr_regparm],
               [if test x$GCC != xyes; then
                  # __attribute__ is a GNU C extension
                  ac_cv_attr_regparm=no
                else
                  SAVE_CFLAGS="$CFLAGS"
                  CFLAGS="-Wall -Werror"
                  AC_COMPILE_IFELSE([void conftest () __attribute__ ((regparm(1)));],
                                    [ac_cv_attr_regparm=yes],
                                    [ac_cv_attr_regparm=no])
                  CFLAGS="$SAVE_CFLAGS"
                fi])

if test x$ac_cv_attr_regparm != xyes; then
  AC_DEFINE(NO_ATTR_REGPARM)
fi

case "$host_cpu" in
	  i486|i586|i686) ;;
  	  *)
            # byteswap assembler routines are i486+-only
            AC_DEFINE(NO_ASM_BYTESWAP)
	    ;;
esac

AC_ARG_ENABLE(cckd-bzip2,
              AC_HELP_STRING([--enable-cckd-bzip2],
                             [enable bzip2 compression for emulated DASD]),
              [case "${enableval}" in
                 yes) cckd_bzip2=yes ;;
                 no) cckd_bzip2=no ;;
                 *) AC_MSG_ERROR(bad value ${enableval} for --enable-cckd-bzip2) ;;
               esac],
              [cckd_bzip2=$have_libbz2])

AC_ARG_ENABLE(het-bzip2,
              AC_HELP_STRING([--enable-het-bzip2],
                             [enable bzip2 compression for emulated tapes]),
              [case "${enableval}" in
                 yes) het_bzip2=yes ;;
                 no) het_bzip2=no ;;
                 *) AC_MSG_ERROR(bad value ${enableval} for --enable-het-bzip2) ;;
               esac],
              [het_bzip2=$have_libbz2])

if test $cckd_bzip2 = yes || test $het_bzip2 = yes; then
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

AC_CHECK_FUNCS(sqrtl ldexpl fabsl fmodl)
AC_CHECK_FUNCS([setresuid setreuid],
               [enable_setuid=true;break])
if test x$enable_setuid != xtrue; then
  AC_DEFINE([NO_SETUID])
fi

# User-definable settings

AC_ARG_ENABLE(profiling,
              AC_HELP_STRING([--enable-profiling], [enable execution profiling]),
              [case "${enableval}" in
                yes)
                  enable_profiling=true
                  CFLAGS="-pg -DPROFILE_CPU $CFLAGS"
                ;;
                no) ;;
                *) AC_MSG_ERROR([bad value ${enableval} for --enable-profiling]) ;;
               esac])

AC_ARG_ENABLE(debug,
              AC_HELP_STRING([--enable-debug], [enable debugging]),
              [case "${enableval}" in
                yes)
                  debug=true
                  CFLAGS="-g $CFLAGS"
		  AC_DEFINE([DEBUG])
                  ;;
                no) debug=false ;;
                *) AC_MSG_ERROR([bad value ${enableval} for --enable-debug]) ;;
               esac],
              [debug=false])

AC_ARG_ENABLE(setuid-hercifc,
              AC_HELP_STRING([--enable-setuid-hercifc],
                             [install hercifc setuid root]),
              [case "${enableval}" in
                yes) setuid_hercifc=yes ;;
                no)  setuid_hercifc=no ;;
                *) AC_MSG_ERROR(bad value ${enableval} for --enable-setuid-hercifc) ;;
               esac],
              [setuid_hercifc=no])
AM_CONDITIONAL(SETUID_HERCIFC, test x$setuid_hercifc = xyes)

AC_MSG_CHECKING([for optimization flags to use])

AC_ARG_ENABLE(optimization,
              AC_HELP_STRING([--enable-optimization=FLAGS],
                             [enable/disable automatic optimization, or specify flags]),
              [case "${enableval}" in
                yes) auto_optimize=true ;;
                no) auto_optimize=false ;;
                *) auto_optimize=false; OFLAGS="${enableval}" ;;
               esac],
              [test x$debug != xtrue && auto_optimize=true])

if test x$auto_optimize = xtrue; then
  case "$host_cpu" in
	  i586|i686)
	    if test "$GCC" = "yes"; then
		OFLAGS="-O3 -march=$host_cpu $OFLAGS"
                if test x$enable_profiling != xtrue && test x$debug != xtrue; then
                  OFLAGS="-fomit-frame-pointer $OFLAGS"
                fi
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

AC_ARG_ENABLE(external-gui,
              AC_HELP_STRING([--disable-external-gui],
                             [disable interface to external Windows GUI]),
              [case "${enableval}" in
                 yes) build_external_gui=true ;;
                 no) build_external_gui=false ;;
                 *) AC_MSG_ERROR(bad value ${enableval} for --enable-external-gui) ;;
               esac],
              [case "$host_os" in
                 *cygwin*) build_external_gui=true ;;
                 *) build_external_gui=false ;;
               esac])

if test x$build_external_gui = xtrue; then
  AC_DEFINE([EXTERNALGUI])
fi

AC_ARG_ENABLE(custom,
              AC_HELP_STRING([--enable-custom=STRING],
                             [provide a custom description for this build]),
              [AC_DEFINE_UNQUOTED([CUSTOM_BUILD_STRING], "${enableval}") ])

dnl This is ugly. The object is to get the value of PKGDATADIR into
dnl config.h, but this is the only way I've found to do so. The incantation
dnl "${prefix}/share" is needed because ${datadir} doesn't expand to the
dnl desired data directory. If there's a better way to get there, PLEASE
dnl replace this. --JRM
if test "x$prefix" = "xNONE"; then
  PKGDATADIR="/usr/local/share/${PACKAGE}"
else
  PKGDATADIR="${prefix}/share/${PACKAGE}"
fi
AC_DEFINE_UNQUOTED([PKGDATADIR], "${PKGDATADIR}")

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
AH_TEMPLATE([EXTERNALGUI],
            [Define to build interface to external Windows GUI])
AH_TEMPLATE([HAVE_U_INT],
            [Define if your system uses u_int8_t, etc. instead of uint8_t])
AH_TEMPLATE([NO_SETUID],
            [Define to disable setuid operation])
AH_TEMPLATE([NO_SIGABEND_HANDLER],
            [Define to disable sigabend_handler (please describe this better)])
AH_TEMPLATE([WIN32],
            [Define when building under Win32 (cygwin)])
AH_TEMPLATE([NOTHREAD],
            [Define to disable use of threads])
AH_TEMPLATE([OPTION_FTHREADS],
            [Define to use included threads implementation (fthreads)])
AH_TEMPLATE([CUSTOM_BUILD_STRING],
            [Define to provide additional information about this build])
AH_TEMPLATE([DEBUG],
            [Define to enable extra debugging code])
AH_TEMPLATE([PKGDATADIR],
            [Directory where the HTTP server will find documents])

AC_OUTPUT(Makefile util/Makefile html/Makefile)

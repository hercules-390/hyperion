AC_INIT(hercules.h)
AC_CONFIG_AUX_DIR(autoconf)

AM_INIT_AUTOMAKE(hercules,2.17)
AM_CONFIG_HEADER(config.h)
AM_MAINTAINER_MODE

AC_CANONICAL_HOST

# Initialization
CFLAGS="-W -Wall $CFLAGS"

#loadable modules
LIBS="-ldl"

# Programs
AC_PROG_CC

# OS-specific settings that we can't figure out any other way (yet)

case "$host_os" in
        darwin*)    
               if test $host_vendor = apple ; then
                   CPPFLAGS=-traditional-cpp
                   AC_DEFINE([_BSD_SOCKLEN_T_],[int])     
                   AC_DEFINE([_INTL_REDIRECT_MACROS])     
               fi
               ;;
        linux*)
                build_hercifc=true
                ;;
        *cygwin*)
                build_hercifc=false
                use_dlltool=true
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

AM_CONDITIONAL(USE_DLLTOOL, [test "x$use_dlltool" = "xtrue"])

# System Services
AC_SYS_LARGEFILE

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

AM_GNU_GETTEXT()

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

AC_ARG_ENABLE(fishhang,
              AC_HELP_STRING([--enable-fishhang], [debug correct lock handling (fthreads only)]),
              [case "${enableval}" in
                yes)
                  fishhang=true
                  CFLAGS="-DFISH_HANG $CFLAGS"
                  AC_DEFINE([FISH_HANG])
                  ;;
                no) fishhang=false ;;
                *) AC_MSG_ERROR([bad value ${enableval} for --enable-fishhang]) ;;
               esac],
              [fishhang=false])

AM_CONDITIONAL(BUILD_FISHHANG, [test x$fishhang = xtrue])

if test x$fishhang = xtrue; then
  if test x$build_fthreads != xtrue; then
    AC_MSG_ERROR(fishhang requested but fthreads not specified)
  fi
fi

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
case "$host_os" in
        *cygwin*)
                ;;
        *)
                AC_CHECK_HEADER(fenv.h,,[AC_DEFINE([NO_IEEE_SUPPORT])])
                ;;
esac

if test x$build_fthreads != xtrue; then
  AC_CHECK_HEADER(pthread.h,
                  [],
                  [case "$host_os" in
                     *cygwin*)
                       # Alternate pthreads location for older cygwin
                       AC_MSG_NOTICE([Looking for pthread.h in /usr/Pthreads])
                       CFLAGS="$CFLAGS -I/usr/Pthreads"
                       AC_CHECK_HEADER(pthread.h,
                                       [],
                                       [AC_MSG_ERROR([Unable to find pthread.h]) ])
                       ;;
                     *)
                       AC_MSG_ERROR([Unable to find pthread.h])
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

if test x$ac_cv_attr_regparm = xyes; then
   case "$host_os" in
        *cygwin*)
                AC_CACHE_CHECK([for cygwin regparm bug],
                               [ac_cv_attr_regparm_cygwin_bug],
                               [AC_TRY_RUN([
/* from Fish */

typedef struct _REGS
{
        int a, b, c, d;
        char e[4000];
}
REGS;

int func1 (int a, int b, int c, REGS *regs) __attribute__ ((regparm(3)));
int func2 (int a, int b, int c, REGS *regs) __attribute__ ((regparm(3)));

int main ( int argc, char *argv[] )
{
        REGS* regs = 0;
        exit(func1 (1, 2, 3, regs));
}

int func1 (int a, int b, int c, REGS *regs) __attribute__ ((regparm(3)))
{
        REGS iregs;
        return func2 (a, b, c, &iregs);
}

int func2 (int a, int b, int c, REGS *regs) __attribute__ ((regparm(3)))
{
        return ((a == 1 && b == 2 && c == 3) ? 0 : 1);
}
],
        ac_cv_attr_regparm_cygwin_bug=no,
        ac_cv_attr_regparm_cygwin_bug=yes,
        true)])
        ;;
   esac

   if test x$ac_cv_attr_regparm_cygwin_bug = xyes; then
      ac_cv_attr_regparm=no
   fi
fi

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

# Check for presence of memrchr in system
AC_CHECK_FUNCS(memrchr,[have_memrchr=yes],[have_memrchr=no])
if test $have_memrchr = no; then
    AC_DEFINE(BUILTIN_MEMRCHR)
fi
AM_CONDITIONAL(USE_BUILTIN_MEMRCHR, [test x$have_memrchr = xno])

AC_CHECK_FUNCS(strerror_r,,[AC_DEFINE(BUILTIN_STRERROR_R)])

AC_CHECK_FUNCS(sqrtl ldexpl fabsl fmodl frexpl)
AC_CHECK_FUNCS(strsignal)
AC_CHECK_FUNC(InitializeCriticalSectionAndSpinCount)

# If we have setresuid AND getresuid, we can do setuid stuff
AC_CHECK_FUNCS([setresuid getresuid],
               [enable_setuid=true],[enable_setuid=false;break])

if test x$enable_setuid != xtrue; then
   # Likewise if we have setreuid AND getreuid
   AC_CHECK_FUNCS([setreuid getreuid],
                  [enable_setuid=true],[enable_setuid=false;break])
fi

# Otherwise, not
if test x$enable_setuid != xtrue; then
  AC_DEFINE([NO_SETUID])
fi

# User-definable settings

AC_ARG_ENABLE(debug,
              AC_HELP_STRING([--enable-debug], [enable debugging (TRACE/VERIFY/ASSERT macros)]),
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

AC_ARG_ENABLE(configsymbols,
              AC_HELP_STRING([--disable-configsymbols],
                             [enable/disable symbolic substitutions in configuration file]),
              [case "${enableval}" in
                yes) opt_configsymbols=true ;;
                no) opt_configsymbols=false ;;
                *) opt_configsymbols=true;;
               esac],[opt_configsymbols=true])
if test x$opt_configsymbols = xtrue; then
    AC_DEFINE([OPTION_CONFIG_SYMBOLS])
fi


if test x$auto_optimize = xtrue; then
  case "$host_cpu" in
          powerpc)
            OFLAGS="-O3 $OFLAGS"
            ;;
          i586|i686)
            if test "$GCC" = "yes"; then
                OFLAGS="-O3 -march=$host_cpu $OFLAGS"
                if test x$debug != xtrue; then
                  OFLAGS="-fomit-frame-pointer $OFLAGS"
                fi
            else
                OFLAGS="$OFLAGS"
            fi
            ;;
          *)
            OFLAGS="$OFLAGS"
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
if test "x$prefix" = "xNONE"; then
  LOCALEDIR="/usr/local/share/locale"
else
  PKGDATADIR="${prefix}/share/locale}"
fi
AC_DEFINE_UNQUOTED([LOCALEDIR], "${LOCALEDIR}")

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
            [Define to enable extra debugging code (TRACE/VERIFY/ASSERT macros)])
AH_TEMPLATE([FISH_HANG],
            [Define to debug correct fthreads LOCK handling (requires fthreads)])
AH_TEMPLATE([PKGDATADIR],
            [Directory where the HTTP server will find documents])
AH_TEMPLATE([LOCALEDIR],
            [Directory where to find NLS data])
AH_TEMPLATE([_BSD_SOCKLEN_T_],
            [Define missing macro on apple darwin (osx) platform])
AH_TEMPLATE([_INTL_REDIRECT_MACROS],
            [Define to 1 if non-Intel architecture (gettext)])
AH_TEMPLATE([BUILTIN_MEMRCHR],
            [Define to use built-in memrchr implementation])
AH_TEMPLATE([BUILTIN_STRERROR_R],
            [Define to use built-in strerror_r implementation])
AH_TEMPLATE([OPTION_CONFIG_SYMBOLS],
            [Define to enable symbolic substitutions in configuration file])
AH_TEMPLATE([HDL_USE_LIBTOOL],
            [Define to enable use libtool instead of dlopen/dlsym/dlclose])

AC_OUTPUT([Makefile util/Makefile html/Makefile po/Makefile.in po/Makefile man/Makefile  intl/Makefile ])


AC_INIT(hercules.h)
AC_CONFIG_AUX_DIR(autoconf)

AM_INIT_AUTOMAKE(hercules,3.02-cvs)
AM_CONFIG_HEADER(config.h)
AM_MAINTAINER_MODE

##------------------------------------------------------------
## Begin JAP unwanted "-g -O2" optimization flags fix...
##
## AM_GNU_GETTEXT()
##
## AC_CANONICAL_HOST
##
## # Initialization
## CFLAGS="-W -Wall $CFLAGS"
##
## # Programs
## AC_PROG_CC
## AC_PROG_LIBTOOL
##

# Initialization
CFLAGS="-W -Wall $CFLAGS"
# ISW20030826-2 : add -I./intl as specified by gettext/info - Maintainers - Adjusting files - src/Makefile.in - Paragraph before last
# added line below
CPPFLAGS="-I ./intl"

# Programs  (Note use "HC_PROG_CC" and not "AC_PROG_CC". We now
#            have our own "hercules.m4" macro defined in the
#            autoconf directory that fixes an undesired compiler
#            optimization flags issue)

AC_PROG_CC

##
## End JAP unwanted "-g -O2" optimization flags fix...
##------------------------------------------------------------

AC_DISABLE_STATIC

modexecdir=${libdir}/${PACKAGE}
AC_SUBST(modexecdir)

# ISW20030827-1 : Enable use of embedded libltdl 1 Line added
AC_LIBTOOL_DLOPEN
AC_LIBTOOL_WIN32_DLL
AC_PROG_LIBTOOL
# ISW20030827-1 : Enable use of embedded libltdl 1 Line added
AC_LIB_LTDL
AC_SUBST([LIBTOOL_DEPS])

AM_GNU_GETTEXT(use-libtool)



##
## ISW - Add flag to warn about duplicates (if necessary)
##
HC_LD_DISALLOWDUPS()

HC_LD_DUPSHAREGETOPT()
HC_HAVE_OPTERR()

# OS-specific settings that we can't figure out any other way (yet)

build_hercifc=false
use_dlltool=false
is_cygwin=false

case "$host_os" in
        darwin*)
                if test $host_vendor = apple ; then
##                 AC_DEFINE([_BSD_SOCKLEN_T_],[int])
                   AC_DEFINE([_INTL_REDIRECT_MACROS])
##              Do whatever is necessary to get the following symbol defined
##               so the included libltdl will be built and used
##                 AC_PROVIDE_AC_LIBTOOL_DLOPEN
                build_hercifc=true
                fi
                ;;
        linux*)
                build_hercifc=true
                ;;
        *cygwin*)
                is_cygwin=true
                build_hercifc=false
                use_dlltool=true
                CFLAGS="$CFLAGS -Wno-format"
                AC_DEFINE([WIN32])
##              AC_DEFINE([NO_SIGABEND_HANDLER])
                ;;
        *)
                build_hercifc=false
                ;;
esac

AM_CONDITIONAL(BUILD_HERCIFC, [test "x$build_hercifc" = "xtrue"])

# Arch-specific settings that we can't figure out any other way (yet)

case "$host_cpu-$GCC" in
        hppa*-yes)
                CFLAGS="$CFLAGS -ffunction-sections"
        ;;
        x86_64-yes|s390x-yes)
                CFLAGS="$CFLAGS -m64"
        ;;
esac


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

# OPTION_DYNAMIC_LOAD requires availability of either dlopen
# or libtool. If neither available OPTION_DYNAMIC_LOAD is not
# possible and USE_DLLTOOL shouldn't be set either. (Each test
# done twice; first time to cause default autoconf behavior,
# the second time to set our flag)

# Leave this one so ltdl doesn't get to think dlopen is not available
AC_CHECK_LIB(dl,dlopen)
# ISW : Following lines commented out - UNCONDITIONALLY use libtool
# AC_CHECK_LIB(dl,dlopen, have_dlopen=true, have_dlopen=false )
# ISW20030827-1 : 1 lines added, 2 lines commented out
have_lt_dlopen=true
# ISW : Following lines - UNCONDITIONALLY use libtool
have_dlopen=false
# AC_CHECK_LIB(ltdl,lt_dlopen)
# AC_CHECK_LIB(ltdl,lt_dlopen, have_lt_dlopen=true, have_lt_dlopen=false )

# AC_CHECK_LIB(gcrypt,gcry_cipher_open, have_gcrypt=true, have_gcrypt=false )

# ISW20041011 :
# Was :
# AM_PATH_LIBGCRYPT("0.4.4",[have_gcrypt=true],[have_gcrypt=false])
AM_PATH_LIBGCRYPT("1.1.90",[have_gcrypt=true],[have_gcrypt=false])

AM_CONDITIONAL(HAVE_GCRYPT, [test "x$have_gcrypt" = "xtrue"])

if test "x$have_gcrypt" = "xtrue" ; then
    OCFLAGS="$CFLAGS"
    CFLAGS="$CFLAGS $GCRYPT_CFLAGS"
    AC_MSG_CHECKING([whether gcry_cipher_open takes 3 or 4 args])
    AC_COMPILE_IFELSE([
#include <gcrypt.h>
int main()
{
  GCRY_CIPHER_HD hd;
  hd = gcry_cipher_open(0,0,0);
  return 0;
}
],[gcrypt_form=3],
    AC_COMPILE_IFELSE(
[
#include <gcrypt.h>
int main()
{
  gcry_cipher_hd_t hd;
  gcry_cipher_open(&hd,0,0,0);
  return 0;
}
],[gcrypt_form=4],[gcrypt_form=0]))
    case $gcrypt_form in
        3)
        AC_MSG_RESULT([3])
        ;;
        4)
        AC_MSG_RESULT([4])
        ;;
        *)
        AC_MSG_RESULT([unknown])
        ;;
    esac
    CFLAGS="$OCFLAGS"
    AC_DEFINE_UNQUOTED(GCRYPT_OPENFORMAT,${gcrypt_form})

fi

opt_dynamic_load=true

AC_ARG_ENABLE(dynamic-load,
              AC_HELP_STRING([--disable-dynamic-load],
                             [disable/enable dynamic loader option]),
              [case "${enableval}" in
                yes) opt_dynamic_load=true ;;
                no) opt_dynamic_load=false ;;
                *) opt_dynamic_load=true ;;
               esac],[opt_dynamic_load=true])

if test $opt_dynamic_load = true; then
  if test $have_dlopen = true || test $have_lt_dlopen = true; then
    AC_DEFINE([OPTION_DYNAMIC_LOAD])
    if test $have_dlopen != true; then
      AC_DEFINE([HDL_USE_LIBTOOL])
    fi
  else
    opt_dynamic_load=false
    use_dlltool=false
  fi
else
  opt_dynamic_load=false
  use_dlltool=false
fi

AM_CONDITIONAL(OPTION_DYNAMIC_LOAD, [test "x$opt_dynamic_load" = "xtrue"])
AM_CONDITIONAL(USE_DLLTOOL,         [test "x$use_dlltool"      = "xtrue"])
AM_CONDITIONAL(BUILD_SHARED,        [test "x$enable_shared"    = "xyes"])

if test x$enable_shared = xyes; then
    AC_DEFINE(HDL_BUILD_SHARED)
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
  if test $is_cygwin = true && test $opt_dynamic_load != true; then
    AC_MSG_ERROR([dynamic loader option required for external gui support on win32])
  else
    if test $is_cygwin = true && test $use_dlltool != true; then
      AC_MSG_ERROR([internal error: external gui specified for win32 but use_dlltool not set])
    else
      AC_DEFINE([EXTERNALGUI])
    fi
  fi
fi

max_num_cpus=8
default_num_cpus=2
num_cpus_error=no

AC_ARG_ENABLE(multi-cpu,
              AC_HELP_STRING([--enable-multi-cpu=NUMBER|no],
                             [enable/disable multi-cpu support (valid range is 1-8, default is 2)]),
              [case "${enableval}" in
                no)              num_cpu_engines=1 ;;
                yes)             num_cpu_engines=${default_num_cpus} ;;
                0|9)             num_cpus_error=yes ;;
                1|2|3|4|5|6|7|8) num_cpu_engines=${enableval} ;;
                *)               num_cpus_error=yes ;;
               esac],
              [num_cpu_engines=${default_num_cpus}])

if test $num_cpus_error = no; then
  AC_DEFINE_UNQUOTED(MAX_CPU_ENGINES,${num_cpu_engines})
else
  AC_MSG_ERROR(bad value "${enableval}" for --enable-multi-cpu)
fi

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

if test x$fishhang = xtrue; then
  if test x$build_fthreads != xtrue; then
    AC_MSG_ERROR(fishhang requested but fthreads not specified)
  fi
fi

AM_CONDITIONAL(BUILD_FISHHANG, [test x$fishhang = xtrue])

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

AC_CHECK_HEADERS(linux/if_tun.h)

# Compiler characteristics

AC_C_BIGENDIAN


#
# TEST FOR STRUCTURE ALIGNEMENT
#
cat > conftest.h << __EOF
#include <stdio.h>
struct sizeoftest
{
    unsigned char a;
};
__EOF

AC_CHECK_SIZEOF(int)
AC_CHECK_SIZEOF(long)
AC_CHECK_SIZEOF(size_t)
AC_CHECK_SIZEOF(int *)
# FIXME: Disabled - because some builtin ffs seem to be causing problem
#        however, gcc 3.4 will barf on certain march= 
# AC_CHECK_FUNCS(ffs)

AC_CHECK_SIZEOF(struct sizeoftest,,[#include "conftest.h"])

AC_MSG_CHECKING([whether ${CC-cc} can/needs a byte structure alignement flag])
hc_cv_sizeok=1
hc_cv_needflag=no
if test "x$ac_cv_sizeof_struct_sizeoftest" != "x1"; then
    case "$host_cpu-$GCC" in
        arm*-yes|xscale*-yes|sh*-yes|pxa*-yes)
               CFLAGS="$CFLAGS -mstructure-size-boundary=8"
                hc_cv_needflag=yes
               ;;
        *)
                hc_cv_sizeok=0
                ;;
    esac
fi
if test "x$hc_cv_sizeok" = "x0"; then
    AC_MSG_ERROR([Size of structures are aligned/rounded and we don't know how to tell the compiler otherwise])
fi
AC_MSG_RESULT([$hc_cv_needflag])


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

if test "$GCC" = "yes" && test x$ac_cv_attr_regparm = xyes; then
   case "$host_os" in
        *cygwin*)
                AC_CACHE_CHECK([for cygwin __attribute__ ((regparm(3))) bug],
                               [ac_cv_cygwin_gcc_attr_regparm_bug],
                               [AC_TRY_RUN([
/* from Fish */

typedef struct _REGS
{
        int a, b, c, d;
        char e[5000];
}
REGS;

int func1 (int a, int b, int c, REGS *regs) __attribute__ ((regparm(3)));
int func2 (int a, int b, int c, REGS *regs) __attribute__ ((regparm(3)));

int main ( int argc, char *argv[] )
{
        REGS* regs = 0;
        exit(func1 (1, 2, 3, regs));
}

int func1 (int a, int b, int c, REGS *regs)
{
        REGS iregs;
        return func2 (a, b, c, &iregs);
}

int func2 (int a, int b, int c, REGS *regs)
{
        return ((a == 1 && b == 2 && c == 3) ? 0 : 1);
}
],
        ac_cv_cygwin_gcc_attr_regparm_bug=no,
        ac_cv_cygwin_gcc_attr_regparm_bug=yes,
        ac_cv_cygwin_gcc_attr_regparm_bug=yes)])
        ;;
   esac

   if test x$ac_cv_cygwin_gcc_attr_regparm_bug = xyes; then
      ac_cv_attr_regparm=no
   fi
fi

if test x$ac_cv_attr_regparm != xyes; then
  AC_DEFINE(NO_ATTR_REGPARM)
fi


if test "$GCC" = "yes"; then
case "$host_os" in
      *cygwin*)
          AC_CACHE_CHECK([for cygwin gcc __builtin_alloca bug],
                         [ac_cv_cygwin_gcc_alloca_bug],
                         [SAVE_CFLAGS="$CFLAGS"
                          CFLAGS="-O2 -fomit-frame-pointer"
                          AC_TRY_RUN([
/*
 * Test for presence of gcc 3.2 compiler bug.
 *
 *  Required compiler options:
 *
 *      -O2 -fomit-frame-pointer
 *
 *  Note: using any other -O option other than
 *  -O2 DOES NOT cause the bug to occur!, nor
 *  does using -fno-omit-frame-pointer! In fact,
 *  using "-fno-omit-frame-pointer" is the sug-
 *  gested workaround for this particular bug.
 */

int foo ()
{
    char a[5000+16];
    memset(a,0xCD,5000);
    a[5000]=0;
    return strlen(a);
}

int main ( int argc, char* argv[] )
{
    return (foo() != 5000);
}
],
          ac_cv_cygwin_gcc_alloca_bug=no,
          ac_cv_cygwin_gcc_alloca_bug=yes,
          ac_cv_cygwin_gcc_alloca_bug=yes)
          CFLAGS="$SAVE_CFLAGS"])
          ;;
      *)
          AC_CACHE_VAL(ac_cv_cygwin_gcc_alloca_bug, ac_cv_cygwin_gcc_alloca_bug=no)
          ;;
esac
fi

if test "$GCC" = "yes"; then
case "$host_os" in
      darwin*)
          AC_CACHE_CHECK([for OS X gcc preprocessor macro argument count bug],
                         [ac_cv_darwin_gcc_cpp_bug],
                         [AC_COMPILE_IFELSE([
#include <stdio.h>
#define MACRO(_x,_args...)  printf(_x, ## _args)
int  main( int argc, char **argv, char **arge )
{
        MACRO( "bare printf\n" );
        return 0;
}
                         ],
                         [ac_cv_darwin_gcc_cpp_bug=no],
                         [ac_cv_darwin_gcc_cpp_bug=yes])])
          ;;
      *)
          ac_cv_darwin_gcc_cpp_bug=no
          ;;
esac

if test x$ac_cv_darwin_gcc_cpp_bug != xno; then
  CPPFLAGS="${CPPFLAGS} -traditional-cpp"
fi
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
AC_CHECK_FUNCS(strsignal getopt_long)
AC_CHECK_FUNCS(InitializeCriticalSectionAndSpinCount)
AC_CHECK_FUNCS(iconv)
AC_CHECK_FUNCS(strlcpy strlcat)

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
  case "$host_cpu-$GCC" in
          arm-yes)
            OFLAGS="-O3 -frename-registers"
            ;;
          xscale-yes|arm*-yes)
            OFLAGS="-O3 -frename-registers -mcpu=$host_cpu -mtune=$host_cpu"
            ;;
          x86_64-yes)
                OFLAGS="-O3 -march=k8 -fomit-frame-pointer"
           ;;
          i386-yes|i486-yes|i586-yes|i686-yes|i786-yes)
            if test $host_cpu = "i786" ; then
                ac_xarch=pentium4
            else
                ac_xarch=$host_cpu
            fi
            if test "$GCC" = "yes"; then
                if ${CC-cc} -E - << __EOF | fgrep "yes" > /dev/null 2>&1
#if __GNUC__ == 2 && __GNUC_MINOR__ == 96
yes
#endif
__EOF
                then
                    if test $host_cpu = i686; then
                        ac_xarch=i586
                    fi
                fi
                OFLAGS="-O3 -march=$ac_xarch $OFLAGS"
                if test x$debug != xtrue && test x$ac_cv_cygwin_gcc_alloca_bug != xyes; then
                  OFLAGS="-fomit-frame-pointer $OFLAGS"
                elif test x$ac_cv_cygwin_gcc_alloca_bug = xyes; then
                  OFLAGS="-fno-omit-frame-pointer $OFLAGS"
                fi
            else
                OFLAGS="$OFLAGS"
            fi
            ;;
          *)
            OFLAGS="-O3 $OFLAGS"
            ;;
  esac
elif test x$ac_cv_cygwin_gcc_alloca_bug = xyes; then
  OFLAGS="-fno-omit-frame-pointer $OFLAGS"
fi

if test "x$OFLAGS" = x; then
  AC_MSG_RESULT([(none)])
else
  AC_MSG_RESULT([$OFLAGS])
  CFLAGS="$OFLAGS $CFLAGS"
fi


AC_ARG_ENABLE(custom,
              AC_HELP_STRING([--enable-custom=STRING],
                             [provide a custom description for this build]),
              [AC_DEFINE_UNQUOTED([CUSTOM_BUILD_STRING], "${enableval}") ])

dnl This is ugly. The object is to get the value of PKGDATADIR into
dnl config.h, but this is the only way I've found to do so. All of this
dnl rigamarole is needed because ${datadir} doesn't expand to the
dnl desired data directory. If there's a better way to get there, PLEASE
dnl replace this. --JRM

dnl If DESTPREFIX is set, we're doing an RPM build, and we want to use that
dnl value. If it's not set, and prefix is set and not equal to NONE, then
dnl we'll use that. If prefix is NONE, then default to /usr/local.
dnl Note: This prefix has /share/locale, /share/$PACKAGE, or /lib/$PACKAGE
dnl appended in the next step, so it must NOT have those components included
dnl in $DESTPREFIX itself.

if test "x${DESTPREFIX}" = "x"; then
  if test "x$prefix" = "xNONE"; then
    DESTPREFIX="/usr/local"
  else
    DESTPREFIX="${prefix}"
  fi
fi

PKGDATADIR="${DESTPREFIX}/share/${PACKAGE}"
AC_DEFINE_UNQUOTED([PKGDATADIR], "${PKGDATADIR}")
LOCALEDIR="${DESTPREFIX}/share/locale"
AC_DEFINE_UNQUOTED([LOCALEDIR], "${LOCALEDIR}")
MODULESDIR="${DESTPREFIX}/lib/${PACKAGE}"
AC_DEFINE_UNQUOTED([MODULESDIR], "${MODULESDIR}")
AC_MSG_NOTICE([Package destination directory prefixes:])
AC_MSG_NOTICE([Data:      ${PKGDATADIR}])
AC_MSG_NOTICE([Locale:    ${LOCALEDIR}])
AC_MSG_NOTICE([Libraries: ${MODULESDIR}])

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
AH_TEMPLATE([OPTION_DYNAMIC_LOAD],
            [Define to enable Hercules Dynamic Loader feature])
AH_TEMPLATE([HDL_USE_LIBTOOL],
            [Define to cause dynamic loader to use libtool instead of dlopen])
AH_TEMPLATE([MAX_CPU_ENGINES],
            [Defines the maximum number of emulated CPU engines])
AH_TEMPLATE([HDL_BUILD_SHARED],
            [Define to to indicate shared libraries are being used])
AH_TEMPLATE([NEED_GETOPT_WRAPPER],
            [Define to indicate a wrapper for getopt is needed])
AH_TEMPLATE([NEED_GETOPT_OPTRESET],
            [Define to indicate optreset exists])
AH_TEMPLATE([MODULESDIR],
            [Directory where HERCULES modules are installed])
AH_TEMPLATE([GCRYPT_OPENFORMAT],
            [Semantic to use for gcry_cipher_open function])

AC_OUTPUT([Makefile util/Makefile html/Makefile crypto/Makefile po/Makefile.in po/Makefile man/Makefile intl/Makefile m4/Makefile ])

AC_INIT(hercules.h)
AM_INIT_AUTOMAKE(hercules,2.14a)

AC_CANONICAL_HOST

# Programs
AC_PROG_CC

CFLAGS="$CFLAGS -Wall"

case "$host_cpu" in
	i[56]86)
	    OFLAGS="-O3 $OFLAGS"
	    if test "$GCC" = "yes"; then
		OFLAGS="-march=$host_cpu $OFLAGS"
	    fi
	    ;;
	*)
	    # Don't try to use the regparm attribute on non-i386
            # (avoids many compiler warnings)
	    AC_DEFINE(NO_ATTR_REGPARM)
	    ;;
esac
CFLAGS="$OFLAGS $CFLAGS"

# Libraries
AC_CHECK_LIB(pthread, pthread_create)
AC_CHECK_LIB(m, sqrt)
dnl This will only work for zlib >= 1.0.2
dnl AC_CHECK_LIB(z, zlibVersion)
AC_CHECK_LIB(z, main)
AC_CHECK_LIB(bz2, BZ2_bzBuffToBuffDecompress,
[have_libbz2=yes],[have_libbz2=no])

# Headers
AC_CHECK_HEADERS(byteswap.h linux/if_tun.h)

# Types
# AC_CHECK_SIZEOF(double)
# AC_CHECK_SIZEOF(long double)

# if test $SIZEOF_DOUBLE -eq $SIZEOF_LONG_DOUBLE; then
#   AC_DEFINE(HAVE_LONG_DOUBLE)
# fi

# Compiler characteristics

# It would be nice if we could check for __attribute__((regparm))
# automatically.  For now, use it with gcc.
if test x$GCC != xyes; then
  AC_DEFINE(NO_ATTR_REGPARM)
fi

AC_ARG_ENABLE(cckd-bzip2,
[  --enable-cckd-bzip2     enable Compressed CKD bzip2 compression],
[case "${enableval}" in
  yes) cckd_bzip2=yes ;;
  no) cckd_bzip2=no ;;
  *) AC_MSG_ERROR(bad value ${enableval} for --enable-cckd-bzip2) ;;
esac],[cckd_bzip2=$have_libbz2])

AC_ARG_ENABLE(het-bzip2,
[  --enable-het-bzip2      enable HET bzip2 compression],
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
[  --enable-profiling      enable profiling],
[case "${enableval}" in
  yes) CFLAGS="$CFLAGS -pg" ;;
  no) ;;
  *) CFLAGS="$CFLAGS ${enableval}" ;;
esac])

AC_ARG_ENABLE(setuid-hercifc,
[  --enable-setuid-hercifc install hercifc setuid root],
[case "${enableval}" in
  yes) setuid_hercifc=yes ;;
  no)  setuid_hercifc=no ;;
  *) AC_MSG_ERROR(bad value ${enableval} for --enable-setuid-hercifc) ;;
esac],[setuid_hercifc=no])
AM_CONDITIONAL(SETUID_HERCIFC, test x$setuid_hercifc = xyes)

# Maybe something for PROFILE_CPU here?

AC_OUTPUT(Makefile util/Makefile html/Makefile)

# HC_PROG_CC
# --------------------------------------------------------------------
# AC_PROG_CC (actually _AC_PROC_CC_G) takes it upon itself to
# put "-g -O2" in CFLAGS. While this may be good for most packages
# using autoconf, we have our own "optimize" function that this
# interferes with.
#
# Notes: AC_BEFORE will emit a warning of AC_PROG_CC was expanded
#        prior to this macro, just in case something gets put in
#        configure.ac before us.
#        AC_REQUIRE will expand AC_PROG_CC for us.
#
AC_DEFUN([HC_PROG_CC],
[   AC_BEFORE([HC_PROG_CC],[AC_PROG_CC])
    AC_REQUIRE([AC_PROG_CC])
    # Restore the saved CFLAGS from autoconf invocation
    CFLAGS=$ac_env_CFLAGS_value
])

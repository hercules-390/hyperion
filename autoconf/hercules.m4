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
AC_DEFUN([HC_LD_DISALLOWDUPS],
[
        AC_REQUIRE([AC_PROG_LIBTOOL])
        AC_REQUIRE([AC_PROG_LD_GNU])
        if test "x$lt_cv_prog_gnu_ld" = "xyes"; then
                LDFLAGS="$LDFLAGS -Wl,--warn-common"
        fi
])

AC_DEFUN([HC_LD_DUPSHAREGETOPT],
[
    ac_use_dup_sharegetopt=auto
    AC_ARG_ENABLE(getoptwrapper,
        AC_HELP_STRING([--enable-getoptwrapper],
                       [force use of the getopt wrapper kludge]),
              [case "${enableval}" in
                yes) 
                    ac_use_dup_sharegetopt=yes
                    ;;
                no) 
                    ac_use_dup_sharegetopt=no
                    ;;
                auto)
                    ac_use_dup_sharegetopt=auto
                    ;;
                *) 
                    ac_use_dup_sharegetopt=auto
                    ;;
               esac],
                     [
                    ac_use_dup_sharegetopt=auto
                     ])
    _HC_LD_DUPSHAREGETOPT($ac_use_dup_sharegetopt)
]
)
AC_DEFUN([_HC_LD_DUPSHAREGETOPT],
[
    rm -f libconftest*
    rm -f .libs/libconftest*
    AC_REQUIRE([AC_PROG_LIBTOOL])
    AC_MSG_CHECKING([whether use of a getopt wrapper is necessary])
    if test "x$1" = "xauto"; then
        if test $(./libtool --features | fgrep "enable shared libraries" | wc -l) -eq 1;then
            cat > conftest1.c << DUPGETOPT1
            extern char *optarg;
            extern int optind;
            extern int test2();
            int test1()
            {
                int i;
                char *c;
                i=optind;
                c=optarg;
                getopt(0,0,0);
                test2();
                return 0;
            }
DUPGETOPT1
            cat > conftest2.c << DUPGETOPT2
            extern char *optarg;
            extern int optind;
            int test2()
            {
                int i;
                char *c;
                i=optind;
                c=optarg;
                getopt(0,0,0);
                return 0;
            }
DUPGETOPT2
            ./libtool --mode=compile ${CC-cc} conftest1.c -c -o conftest1.lo > /dev/null 2>&1
            ./libtool --mode=link ${CC-cc} -shared -rpath /lib -no-undefined conftest1.lo -o libconftest1.la > /dev/null 2>&1
            ./libtool --mode=compile ${CC-cc} conftest2.c -c -o conftest2.lo > /dev/null 2>&1
            ./libtool --mode=link ${CC-cc} -shared -rpath /lib -no-undefined conftest2.lo libconftest1.la -o libconftest2.la > /dev/null 2>&1
            if test $? = 0; then
                ac_cv_dup_getopt=no
                ac_cv_dup_getoptmsg=no
            else
                ac_cv_dup_getopt=yes
                ac_cv_dup_getoptmsg=yes
            fi
        else
                ac_cv_dup_getopt=no
                ac_cv_dup_getoptmsg=no
        fi
        rm -f *conftest*
        rm -f .libs/*conftest*
    else
        ac_cv_dup_getopt="$1"
        ac_cv_dup_getoptmsg="$1 (forced)"
    fi
    AC_MSG_RESULT($ac_cv_dup_getoptmsg)
    if test "x$ac_cv_dup_getopt" = "xyes";then
        AC_DEFINE([NEED_GETOPT_WRAPPER])
    fi
])

AC_DEFUN([HC_HAVE_OPTERR],
[
        AC_CACHE_CHECK([whether to use optreset],
                       [ac_cv_need_optreset],
                       [AC_TRY_LINK([],
[extern int optreset;
optreset=1;
getopt(0,0,0);
],
        ac_cv_need_optreset=yes,
        ac_cv_need_optreset=no)
                      ]
        )
        if test "x$ac_cv_need_optreset" = "xyes"; then
            AC_DEFINE([NEED_GETOPT_OPTRESET])
        fi
]
)

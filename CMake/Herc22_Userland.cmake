# Herc22_Userland.cmake - perform Userland tests to build Hercules

#[[   Copyright 2017 by Stephen Orso.

      Distributed under the Boost Software License, Version 1.0.
      See accompanying file BOOST_LICENSE_1_0.txt or copy at
      http://www.boost.org/LICENSE_1_0.txt)
]]

#[[

This file checks for all required and optional target system components
used when building Hercules.  If any required component is missing,
the build cannot proceed.  If an optional component is missang and is
specifically requested in a build option, say Regina Rexx, the build
cannot proceed.

The tests are not as simple as yes-no, though.  If a preferred header
or function is not available, a Hercules-internal substitute might do.
The logic to trigger substitution through preprocessor macros is here;
Hercules code must adapt to the missing function/header by testing
preprocessor variables.

If an optional component is missing from the target system and is
not specifically requested by a build option, the capability enabled
by the component is not included in Hercules.

]]


message( "Checking for required public headers" )

herc_Check_Include_Files( ctype.h           FAIL )
herc_Check_Include_files( dlfcn.h           FAIL )
herc_Check_Include_Files( errno.h           FAIL )
herc_Check_Include_Files( fcntl.h           FAIL )
herc_Check_Include_Files( limits.h          FAIL )
herc_Check_Include_Files( setjmp.h          FAIL )
herc_Check_Include_Files( stdarg.h          FAIL )
herc_Check_Include_Files( stdio.h           FAIL )
herc_Check_Include_Files( stdlib.h          FAIL )
herc_Check_Include_Files( stddef.h          FAIL )
herc_Check_Include_Files( string.h          FAIL )
herc_Check_Include_Files( sched.h           FAIL )
herc_Check_Include_Files( time.h            FAIL )
herc_Check_Include_Files( sys/ioctl.h       FAIL )
herc_Check_Include_Files( sys/stat.h        FAIL )
herc_Check_Include_Files( sys/time.h        FAIL )
herc_Check_Include_Files( sys/types.h       FAIL )
herc_Check_Include_Files( sys/mman.h        FAIL )
herc_Check_Include_Files( sys/resource.h    FAIL )
herc_Check_Include_Files( sys/utsname.h     FAIL )
herc_Check_Include_Files( sys/wait.h        FAIL )
herc_Check_Include_Files( pwd.h             FAIL )
herc_Check_Include_Files( unistd.h          FAIL )
herc_Check_Include_Files( math.h            FAIL )

# definitions in assert.h are used only when compiled under Microsoft,
# in only one module, hatomic.h, and it is assumed to exist under
# Microsoft.  So there is no need (for the moment) to test for it here.


# Function strsignal helpful but not required.  If it does not exist, we
# need signal.h so that Hercules can compile its own version in strsignal.c.
# But because signal.h is required by dbgtrace.h and the build fails without
# it, strsignal is really optional for the build.

herc_Check_Function_Exists( strsignal OK )


# Needed by a "BREAK_INTO_DEBUGGER" macro, defined in dbgtrace.h and
# referenced in hthreads.c and ptrace.c.  This is worth a further look.
# And if the macro is changed, the test above for strsignal needs
# to be augmented such that if strsignal does not exist, signal.h
# remains mandatory.

herc_Check_Include_Files( signal.h FAIL )


# See if we have the declarations and structure members needed to
# create a sigabend handler.

herc_Check_Struct_Member( sigaction sa_sigaction signal.h OK )
herc_Check_Symbol_Exists( SIGUSR1                signal.h OK )
herc_Check_Symbol_Exists( SIGUSR2                signal.h OK )
herc_Check_Symbol_Exists( SIGPIPE                signal.h OK )
herc_Check_Symbol_Exists( SIGBUS                 signal.h OK )

if( NOT ( HAVE_SIGACTION_SA_SIGACT
        AND HAVE_SIGUSR1
        AND HAVE_SIGUSR2
        AND HAVE_SIGPIPE
        AND HAVE_SIGBUS )
    )
    set( NO_SIGABEND_HANDLER 1 )
endif( )


# sys/sysctl requires a type definition available in a number of headers.
# We pick stddef, and we know it exists.  It is used only on Apple and
# FreeBSD, and is not required on other systems.  For the moment, we shall
# mark it optional.

herc_Check_Include_Files( "stddef.h;sys/sysctl.h" OK )


# sys/sockio.h must be explicitly included on Solaris; it is included by
# ioctl.h on BSD systems.  Do the test here so the config.h macro can be
# defined.

herc_Check_Include_Files( sys/sockio.h    OK )



# Check for public header variables defined by libtool use in the
# autotools build.

herc_Check_Include_Files( dirent.h FAIL )


# Check networking public headers, which are *required* to build Hercules.

#
# On OS X 10.3, sys/types.h apparently must be included before sys/socket.h
#
# On other Darwin and BSD-based systems except FreeBSD 11 (and 12?),
# sys/socket.h must be included before net/if.h, net/route.h, and
# netinet/in.h.  It is not clear if sys/types.h and/or sys/socket.h are
# needed for net/tcp.h or netinet/tcp.h.
#
# So for the sake of simplicity and robustness, all network headers are
# tested by including sys/types.h and, if present on the system,
# sys/socket.h before the header under test.
#
# It is also not clear if Hercules needs net/route.h
#

set( prereq_Headers sys/types.h )
herc_Check_Include_Files( "${prereq_Headers};sys/socket.h" FAIL )
if ( ${HAVE_SYS_SOCKET_H} )
    set( prereq_Headers ${prereq_Headers} sys/socket.h )
endif( )

herc_Check_Include_Files( "${prereq_Headers};netinet/in.h" FAIL )
herc_Check_Include_Files( "${prereq_Headers};net/if.h" FAIL )
herc_Check_Include_Files( "${prereq_Headers};netdb.h" FAIL )

# It is not clear whether Hercules needs route.h.

herc_Check_Include_Files( "${prereq_Headers};net/route.h" OK )

# tcp.h might be found in either net or netinet.  If it's not in netinet,
# we will test net and let that issue the failure message.

herc_Check_Include_Files( "${prereq_Headers};netinet/tcp.h" OK )
if ( HAVE_NETINET_TCP_H )
else( )
    herc_Check_Include_Files( "${prereq_Headers};net/tcp.h" FAIL )
endif( )

herc_Check_Include_Files( "${prereq_Headers};arpa/inet.h" FAIL )


# Header linux/if_tun.h is needed (based on tests below) only for hercifc.
# So Hercules will build without it, but hercifc will not on Linux systems.
# On BSD systems, again based on configure.ac, linux/if_tun.h is not
# required (hint: not many linux directories on a BSD system), and
# net/if.h (tested above and required) will be enough.

if( BUILD_HERCIFC AND ( CMAKE_SYSTEM_NAME MATCHES "Linux" ) )
    herc_Check_Include_Files( "linux/if_tun.h" OK )
    if( NOT HAVE_LINUX_IF_TUN_H )
        herc_Save_Error( "Header \"linux/if_tun.h\" is needed to build hercifc on Linux" )
    endif( )
endif( )


# Now that we have a complete set of headers for networking, check for
# networking structures and members.  Note that the variable prereq_Headers
# used above for public header tests is also used here.  If the header
# is not required and does not exist, then any identifiers in the header
# do not exist...and should not be required either.

if( ${HAVE_NETINET_IN_H} )
    herc_Check_Struct_Member( in_addr      s_addr   "${prereq_Headers};netinet/in.h" FAIL )
    herc_Check_Struct_Member( sockaddr_in  sin_len  "${prereq_Headers};netinet/in.h" OK )
endif( )


# Hercules htypes.h will typedef in_addr_t to unsigned int if it is not
# defined, so if missing, Hercules can build.  It should be defined # in
# netinet/in.h, a required header.

set( CMAKE_EXTRA_INCLUDE_FILES "${prereq_Headers};netinet/in.h" )
check_type_size( in_addr_t IN_ADDR_T )
unset( CMAKE_EXTRA_INCLUDE_FILES )


# socklen_t should be defined in sys/socket.h, one of the headers in
# ${prereq_Headers}.  If sys/socket.h is missing, and therefore socklen_t
# is missing, Hercules will define socklen_t as unsigned int.

set( CMAKE_EXTRA_INCLUDE_FILES "${prereq_Headers}" )
check_type_size( socklen_t SOCKLEN_T )
unset( CMAKE_EXTRA_INCLUDE_FILES )


# In configure.ac, much work is done testing for IPV6 headers.  Sadly,
# this work is wasted.  Only hifr.h includes IPV6 headers, and here's
# a summary:
#   linux/ipv6.h        - included only if it exists and if IPV6 and
#                         HAVE_IN6_IFREQ_IFR6_ADDR are defined.
#   netinet6/in6_var.h  - included only if it exists and if IPV6 and
#                         HAVE_IN6_IFREQ_IFR6_ADDR are defined.
# Unfortunately for all of the above, HAVE_IN6_IFREQ_IFR6_ADDR is
# *NEVER* defined.  On Linux systems, HAVE_STRUCT_IN6_IFREQ_IFR6_ADDR
# is defined (note the addition of "STRUCT" in the macro).  On BSD
# systems (including Apple??), HAVE_STRUCT_IN6_IFREQ_IFR_IFRU_IFRU_FLAGS
# is defined.

# And nowhere in Hercules is there an attempt to deal with the naming
# differences (IFR6 versus IFRU) between Linux and BSD IPV6
# implementations.

# If HAVE_IN6_IFREQ_IFR6_ADDR is not defined, then hifr.h defines its
# own IPV6 networking structures (not many are needed) and uses the
# Linux convention to do so.   Because Hercules works without the IPV6
# headers, we will not code any of things in configure.ac into CMake.
# "Fixing" this would likely mean breaking stuff that now works.  This
# should be fixed, but not here and not now; I am thinking CMake build
# for Hercules V3 or later.  See Hercules issue #223.  See configure.ac
# line 724 and following (as of commit 1428eee).

# (https://github.com/hercules-390/hyperion/blob/1428eee3844f1445a6ed80b10da4d81bbc7f4840/configure.ac)


# Test for TCP/IP keepalive support.  Either SO_KEEPALIVE or TCP_KEEPALIVE
# indicate basic support.  If any of TCP_KEEPIDLE, TCP_KEEPINTVL, or
# TCP_KEEPCNT are present, then partial keepalive is supported by the
# target, and if all three present, the the target supports full keepalive.

# The following tests still make use of ${prereq_Headers} in addition to
# those specifically required for each macro.  The TCP_KEEP* macros are
# defined in tcp.h, a required header, whereever that happens to be.

if( HAVE_NETINET_TCP_H )
    set( keepalive_Headers ${prereq_Headers} netinet/tcp.h )
elseif( HAVE_NET_TCP_H )
    set( keepalive_Headers ${prereq_Headers} net/tcp.h )
endif( )

herc_Check_Symbol_Exists( SO_KEEPALIVE "${prereq_Headers}" OK )
if( NOT HAVE_DECL_SO_KEEPALIVE )
    herc_Check_Symbol_Exists( TCP_KEEPALIVE "${keepalive_Headers}" OK )
endif( )

if( HAVE_DECL_SO_KEEPALIVE OR HAVE_DECL_TCP_KEEPALIVE )
    set( HAVE_BASIC_KEEPALIVE 1 )
    herc_Check_Symbol_Exists( TCP_KEEPIDLE  "${keepalive_Headers}" OK )
    herc_Check_Symbol_Exists( TCP_KEEPINTVL "${keepalive_Headers}" OK )
    herc_Check_Symbol_Exists( TCP_KEEPCNT   "${keepalive_Headers}" OK )
    if( HAVE_DECL_TCP_KEEPIDLE              # If any of the three, partial support
            OR HAVE_DECL_TCP_KEEPINTVL
            OR HAVE_DECL_TCP_KEEPCNT )
        set( HAVE_PARTIAL_KEEPALIVE 1 )     # All of the three, full support
        if( HAVE_DECL_TCP_KEEPIDLE
                AND HAVE_DECL_TCP_KEEPINTVL
                AND HAVE_DECL_TCP_KEEPCNT )
            set( HAVE_FULL_KEEPALIVE 1 )
        endif( )
    endif( )
endif( )


# Sundry networking macros.

herc_Check_Symbol_Exists( IFNAMSIZ        "${prereq_Headers};net/if.h"     OK )


# The following IOCTL values SIOC* are used for CTC support only.

# Linux systems define IOCTL values as constants in linux/sockios.h
# (note the plural), which is included by sys/ioctl.h.

# BSD systems define IOCTL values as constants in sys/sockio.h (note the 
# singular), which is included by sys/ioctl.h.

# Solaris systems also define IOCTL values as constants in sys/sockio.h 
# (singular, just like BSD), and it must be included explicitly.

# For BSD systems, the Hercules autotools tests never detected the SIOC* 
# macros and so the corresponding functionality was never built in the
# Hercules.  And as a result, functionality enabled by these macros has
# not received much use on BSD.

# IOCTL values are used in hostopts.h to get and/or set interface options
# in the CTC modules ctc_ptp.c, hercifc.c, and tuntap.c.

# So we shall check for the existence of linux/sockios.h as a tell-tale
# for a Linux-based SIOC* implementation and sys/sockio.h as a tell-tale
# for a BSD or Solaris-based SIOC* implementation.  If neither exist, 
# then none of the values will be defined.  If sys/sockio.h exists, we 
# will include it in the build.  This allows builds on Solaris without
# having to test for the Solaris target.  

# For compatibilty with the autotools build, if this is a BSD-style
# SIOC* macro definition set, the tests are skipped and config.h will
# reflect an absence of this capability.  Someone with CTC experience
# (me perhaps, once I buy a modified Delorean?) can test the BSD stuff.

set( SIOC_headers "" )
herc_Check_Include_Files( "${prereq_Headers};linux/sockios.h" OK )
if( HAVE_LINUX_SOCKIOS_H )
    set( SIOC_headers "${prereq_Headers};sys/ioctl.h" )
else( )
    herc_Check_Include_Files( "${prereq_Headers};sys/sockio.h" OK )
    if( HAVE_SYS_SOCKIO_H )
        set( SIOC_headers "${prereq_Headers};sys/ioctl.h;sys/sockio.h" )
    endif( )
endif( )

if( (NOT ("${SIOC_headers}" STREQUAL ""))
        AND ("${HAVE_LINUX_SOCKIOS_H}")
        )
    herc_Check_Symbol_Exists( SIOCSIFNETMASK  "${SIOC_headers}"  OK )
    herc_Check_Symbol_Exists( SIOCSIFBRDADDR  "${SIOC_headers}"  OK )
    herc_Check_Symbol_Exists( SIOCSIFHWADDR   "${SIOC_headers}"  OK )
    herc_Check_Symbol_Exists( SIOCGIFHWADDR   "${SIOC_headers}"  OK )
    herc_Check_Symbol_Exists( SIOCADDRT       "${SIOC_headers}"  OK )
    herc_Check_Symbol_Exists( SIOCDELRT       "${SIOC_headers}"  OK )
    herc_Check_Symbol_Exists( SIOCDIFADDR     "${SIOC_headers}"  OK )
endif( )
unset( SIOC_headers )


# On Windows, fthreads may be used in lieu of pthreads.  So pthread.h is
# potentially optional on Windows but not otherwise.  If pthreads is to be
# used on Windows and is allowed, we will issue any message about a missing
# pthread.h there.  Otherwise, its absence fails the build.

set( CMAKE_THREADS_PREFER_PTHREAD 1)
set( THREADS_PREFER_PTHREAD_FLAG 1)
find_package( Threads REQUIRED )
set( herc_Threads_Target "Threads::Threads" )

if( WIN32 )
    herc_Check_Include_Files( pthread.h OK )
else( )
    if( NOT CMAKE_USE_PTHREADS_INIT )
        herc_Save_Error( "Unable to use pthreads on this target.  Hercules requires pthreads on non-Windows systems." )
    endif( )
    herc_Check_Include_Files( pthread.h FAIL )
endif( )
set( CMAKE_EXTRA_INCLUDE_FILES "sys/types.h" )
check_type_size( pthread_t SIZEOF_PTHREAD_T )
unset( CMAKE_EXTRA_INCLUDE_FILES )


# At least one of the following integer type headers must exist.
# Hercules file hstdint.h tests first for stdint.h, then inttypes.h,
# then unistd.h.  Because inttypes.h normally includes stdint.h, one
# would think that the test order should be inttypes.h, stdint.h, and
# then unistd.h.

# Especially because unistd.h is included unconditionally for all
# non-Windows builds in hstdinc.h.

herc_Check_Include_Files( inttypes.h OK )
if( ${HAVE_INTTYPES_H} )
else( )
    herc_Check_Include_Files( stdint.h OK )
    if( ${HAVE_STDINT_H} )
    else( )
        if( ${HAVE_UNISTD_H} )  # Required, tested earlier.
        else( )
            herc_Save_Error( "Neither stdint.h, inttypes.h, nor unistd.h found.  At least one is required" )
        endif( )
    endif( )
endif( )


# check for functions and libraries required to build Hercules.  CMake
# caching complicates this in two ways:
# - If a function is not included in the c runtime library, the cached
#   result must be removed before testing another library.  Otherwise
#   CMake uses the cached result instead of testing the second library.
# - If a function is found in another library, we must cache the name of
#   the other library, lest on a subsequent build the cached "function
#   present result be seen without knowing that another library is needed
#   to generate that result.

message( "Checking for required functions and libraries" )

# The following functions should be in the basic c Runtime library.

herc_Check_Function_Exists( getopt_long FAIL )
herc_Check_Function_Exists( strerror_r FAIL )
# The following function is not used in Hercules.
# herc_Check_Function_Exists( sys_siglist FAIL )
herc_Check_Function_Exists( sleep FAIL )
herc_Check_Function_Exists( usleep FAIL )
herc_Check_Function_Exists( nanosleep FAIL )
herc_Check_Function_Exists( strtok_r FAIL )
herc_Check_Function_Exists( gettimeofday FAIL )
herc_Check_Function_Exists( getpgrp FAIL )
herc_Check_Function_Exists( getlogin FAIL )
herc_Check_Function_Exists( getlogin_r FAIL )
herc_Check_Function_Exists( realpath FAIL )
herc_Check_Function_Exists( fsync FAIL )
herc_Check_Function_Exists( ftruncate FAIL )
herc_Check_Function_Exists( fork FAIL )
herc_Check_Function_Exists( sysconf FAIL )


# The following functions are used in Hercules without guarding.  When
# built by configure.ac, libtool autotools code tested for these functions.
# We test for them here just for completeness.  Because there is no
# guarding, there is no need for config.h macros.

herc_Check_Function_Exists( memcpy    FAIL )
herc_Check_Function_Exists( strchr    FAIL )
herc_Check_Function_Exists( strcmp    FAIL )
herc_Check_Function_Exists( strrchr   FAIL )


# dlerror is part of libdl on some systems, notably Linux, and part of
# the standard C runtime on others, notably BSD.  CMake handles the
# different locations of libdl, and we will use that to test for dlerror.

set( CMAKE_REQUIRED_LIBRARIES "${CMAKE_DL_LIBS}" )
herc_Check_Function_Exists( dlerror   FAIL )
set( CMAKE_REQUIRED_LIBRARIES "" )


# Pipe is required for non-Windows builds, and not required on Windows.
# On Windows, socketpair() is used in its place.  Hercules does not
# check HAVE_PIPE; it is assumed to exist.  So the build fails without it.

if( NOT WIN32 )
    herc_Check_Function_Exists( pipe FAIL )
endif( )

# If SIZEOF_OFF_T is 8, then large file support is possible.  Test for
# fseeko so Hercules will use it if available.  Hercules will build
# correctly without it.

herc_Check_Function_Exists( fseeko OK )


# On many systems, inet_aton is found in the basic c runtime.  On others
# (e.g., SunOS, Solaris) it is included in libnsl.  For Windows, it does
# not exist...but on Windows the lack is OK because w23util.c includes a
# substitute inet_aton.  However, w32util.c does not provide a copy of
# inet_ntoa, which is used far more often in Hercules and for which there
# is no guarding in the code.  So the following is more about seeing
# whether libnsl is needed.

if( NOT WIN32 )
    set( CMAKE_REQUIRED_LIBRARIES ${link_libnsl} )
    herc_Check_Function_Exists( inet_aton OK )
    if( NOT HAVE_INET_ATON )
        unset( HAVE_INET_ATON CACHE )
        set( CMAKE_REQUIRED_LIBRARIES "${name_libnsl}" )
        herc_Check_Function_Exists( inet_aton FAIL )
        if( HAVE_INET_ATON )
            set( link_libnsl "${name_libnsl}" CACHE INTERNAL "include nameserver lookup functions" )
        endif( )
    endif( NOT HAVE_INET_ATON )
    herc_Check_Function_Exists( inet_ntoa FAIL )
    set( CMAKE_REQUIRED_LIBRARIES "" )
endif( NOT WIN32 )


# On some (?) systems, gethostbyname is found in the basic c runtime.
# On others (e.g., SunOS, Solaris) it is included in libnsl.  For Windows,
# it does not exist...but on Windows the lack is OK because w23util.c
# includes a substitute gethostbyname.

# Noteworthy also is the fact that gethostbyname is deprecated and replaced
# by getnameinfo.

if( NOT WIN32 )
    set( CMAKE_REQUIRED_LIBRARIES ${link_libnsl} )
    herc_Check_Function_Exists( gethostbyname OK )
    if( NOT HAVE_GETHOSTBYNAME )
        unset( HAVE_GETHOSTBYNAME CACHE )
        set( CMAKE_REQUIRED_LIBRARIES "${name_libnsl}" )
        herc_Check_Function_Exists( gethostbyname FAIL )
        if( HAVE_GETHOSTBYNAME )
            set( link_libnsl "${name_libnsl}" CACHE INTERNAL "" FORCE )
        endif( )
    endif( NOT HAVE_GETHOSTBYNAME )
    set( CMAKE_REQUIRED_LIBRARIES "" )
endif( NOT WIN32 )


# The following tests for memory allocation functions needs to be reviewed
# along with the hmalloc.h file in Hercules.  The current configure.ac
# code sets the variables but never establishes any of them as "must-have."
# So we shall do the same for the moment.  Given that valloc has been
# deprecated for eons of computer years and pvalloc is a GNU extension,
# this is worth persuing.  It would be interesting to see what would happen
# if none of the functions were defined.

herc_Check_Function_Exists( mlock OK )
herc_Check_Function_Exists( mlockall OK )
herc_Check_Function_Exists( pvalloc OK )
herc_Check_Function_Exists( valloc OK )
herc_Check_Function_Exists( posix_memalign OK )


# The following function is only needed on Windows

if( WIN32 )
    herc_Check_Function_Exists( InitializeCriticalSectionAndSpinCount FAIL )
endif ( WIN32 )


# The function sched_yield is required; its absence will fail the build.
# If not part of the basic c runtime, it can be found in librt, the
# POSIX Real-time Extensions library (Solaris, SunOS).  And if librt
# contains sched_yield, it may also contain fdatasync.  If the fdatasync
# function is missing, Hercules will use fsync instead.

set( CMAKE_REQUIRED_LIBRARIES ${link_librt} )
herc_Check_Function_Exists( sched_yield OK )
herc_Check_Function_Exists( fdatasync OK )
if( NOT HAVE_SCHED_YIELD )
    unset( HAVE_SCHED_YIELD CACHE )
    set( CMAKE_REQUIRED_LIBRARIES "${name_librt}" )
    herc_Check_Function_Exists( sched_yield FAIL)
    if( HAVE_SCHED_YIELD )
        set( link_librt ${name_librt} CACHE INTERNAL "include POSIX real time extensions" )
        if( NOT HAVE_FDATASYNC )
            unset( HAVE_FDATASYNC CACHE )
            herc_Check_Function_Exists( fdatasync OK )
        endif( NOT HAVE_FDATASYNC )
    endif( HAVE_SCHED_YIELD )
endif( NOT HAVE_SCHED_YIELD )
set( CMAKE_REQUIRED_LIBRARIES "" )


# For OS X 10.6 autoconf defines HAVE_FDATASYNC even though there is
# no function prototype declared for fdatasync() and unistd.h contains
# define _POSIX_SYNCHRONIZED_IO (-1) which indicates that fdatasync is
# not supported. So to decide whether fdatasync really can be used, we
# create a new symbol HAVE_FDATASYNC_SUPPORTED which is defined only if
# HAVE_FDATASYNC is defined and _POSIX_SYNCHRONIZED_IO is not negative.

# A better test for the above is to just use check_prototype_definition.

# And this could be an error in autoconf, not a problem with OS X 10.6
# header files.  Lacking an OS X 10.6 system, there is no way to tell.

if( HAVE_FDATASYNC )
    check_prototype_definition(fdatasync
            "int fdatasync( int fd )"
            "NULL"
            unistd.h
            WORKING_FDATASYNC )
    if( NOT WORKING_FDATASYNC )
        unset( HAVE_FDATASYNC CACHE )
    endif( )
endif( )


# The functions pow() and floor() are either in the basic c runtime library
# or in the libm library.  If a cached result used the math library, be sure
# it is included on the retest.

set( CMAKE_REQUIRED_LIBRARIES ${link_libm} )
herc_Check_Function_Exists( pow OK )
if( NOT HAVE_POW )
    unset( HAVE_POW CACHE )
    set( CMAKE_REQUIRED_LIBRARIES "${name_libm}" )
    herc_Check_Function_Exists( pow FAIL)
    if( HAVE_POW )
        set( link_libm "${name_libm}" CACHE INTERNAL "include math library" )
        herc_Check_Function_Exists( floor FAIL)
        herc_Check_Function_Exists( ldexp FAIL )
    endif( )
endif( NOT HAVE_POW )
set( CMAKE_REQUIRED_LIBRARIES "" )


# If none of the following five functions are defined, then the build needs
# NO_SETUID defined.  If the first two functions (setresuid, getresuid) are
# are avaiable, that's enough.  If those two are missing, then the next
# three (setreuid, geteuid, getuid) are needed.  And if they are missing,
# Setuid suppport is disabled in Hercules, but the build continues.
# (* so this should really be in CMakeHercCheckOptional.cmake )

herc_Check_Function_Exists( setresuid OK )
herc_Check_Function_Exists( getresuid OK )
if( NOT (HAVE_SETRESUID AND HAVE_GETRESUID) )
    herc_Check_Function_Exists( setreuid OK )
    herc_Check_Function_Exists( geteuid OK )
    herc_Check_Function_Exists( getuid OK )
    if( NOT (HAVE_SETREUID AND HAVE_GETEUID AND HAVE_GETUID) )
        set( NO_SETUID 1 )
    endif( )
endif( )


# In Linux and BSD, connect() is defined in libc and no additional link
# libraries are needed.  In Solaris, the socket library is needed.

set( CMAKE_REQUIRED_LIBRARIES ${link_libsocket} )
herc_Check_Function_Exists( connect OK )
if( NOT HAVE_CONNECT )
    unset( HAVE_CONNECT CACHE )
    set( CMAKE_REQUIRED_LIBRARIES "${name_libsocket}" )
    herc_Check_Function_Exists( connect FAIL )
    if( HAVE_CONNECT )
        set( link_libsocket "${name_libsocket}" CACHE INTERNAL "include socket library" )
    endif( )
endif( NOT HAVE_CONNECT )
set( CMAKE_REQUIRED_LIBRARIES "" )


# In Linux and BSD, socketpair() is defined in libc and no additional
# link libraries are needed.  In Solaris, the socket library is needed.

set( CMAKE_REQUIRED_LIBRARIES ${link_libsocket} )
herc_Check_Function_Exists( socketpair OK )
if( NOT HAVE_SOCKETPAIR )
    unset( HAVE_SOCKETPAIR CACHE )
    set( CMAKE_REQUIRED_LIBRARIES "${name_libsocket}" )
    herc_Check_Function_Exists( socketpair FAIL )
    if( HAVE_CONNECT )
        set( link_libsocket "${name_libsocket}" CACHE INTERNAL "include socket library" )
    endif( )
endif( NOT HAVE_SOCKETPAIR )
set( CMAKE_REQUIRED_LIBRARIES "" )


# See if optreset is required to use getopt.  Optreset is normally
# needed for BSD systems and not for Linux systems.  If required, optreset
# is declared extern int in unistd.h.

herc_Check_Symbol_Exists( optreset unistd.h OK )
if( HAVE_DECL_OPTRESET )
    set( NEED_GETOPT_OPTRESET 1)
endif( )



# check for required/desirable macros.

# The following is only required for Windows builds, w32util.h,
# and if it does not exist, it is typedef'd to long.

#### AC_CHECK_TYPES( useconds_t, [hc_cv_have_useconds_t=yes], [hc_cv_have_useconds_t=no] )  (in types.h)

# The following is only required for Windows builds, hscutl2.h,
# and if it does not exist, it is typedef'd to unsigned long.

#### AC_CHECK_TYPES( id_t,       [hc_cv_have_id_t=yes],       [hc_cv_have_id_t=no]       )  (in sys/types.h)


# The following test for which header includes a timespec structure is
# used only for Windows builds that use fthreads (i.e., most Windows
# builds.  It is not required for open source builds.

#### AC_CHECK_MEMBERS( [struct timespec.tv_nsec],
####     [
####         hc_cv_timespec_in_sys_types_h=yes
####         hc_cv_timespec_in_time_h=no
####     ],
####     [
####         AC_CHECK_MEMBERS( [struct timespec.tv_nsec],
####             [
####                 hc_cv_timespec_in_sys_types_h=no
####                 hc_cv_timespec_in_time_h=yes
####             ],
####             [
####                 hc_cv_timespec_in_sys_types_h=no
####                 hc_cv_timespec_in_time_h=no
####             ],
####             [#include <time.h>]
####         )
####     ],
####     [#include <sys/types.h>]
#### )


# Any of the following four types, if not defined, are typedef'd in htypes.h
# to unsigned versions of the underlying type.  They can be found on BSD
# systems in sys/types.h and on Windows systems in winsock.h.  So we need
# to see if they exist so that htypes.h does not try to define them if
# they do exist.  They appear to be synonyms for uint8_t etc in C99, so
# that might be a better long-term solution to this.

set( CMAKE_EXTRA_INCLUDE_FILES "sys/types.h" )
check_type_size( u_char   U_CHAR )
check_type_size( u_short  U_SHORT )
check_type_size( u_int    U_INT )
check_type_size( u_long   U_LONG )
unset( CMAKE_EXTRA_INCLUDE_FILES )


# The following test is used as a telltale for systems that use u_intn_t
# instead of uintn_t.  The Hercules header htypes.h will define integer
# based on the above four tests and the next test.  Note that in
# configure.ac, if the following type exists, then u_int is presumed to
# exist as well.  I suspect this is an error in configure.ac.

check_type_size( u_int8_t U_INT8_T )


message( "Checking for optional headers and libraries." )


herc_Check_Include_Files( assert.h OK )

# The following functions exist in some distributions and not in others.
# If the target system does not have them, Hercules includes its own
# copy of the functions.

herc_Check_Function_Exists( memrchr OK )   # found in Linux
herc_Check_Function_Exists( strlcpy OK )   # found in BSD
herc_Check_Function_Exists( strlcat OK )   # found in BSD


# If we have the header byteswap.h, then we will use it. If not,
# hbyteswp.h will be included instead.  And if we are able to determine
# that the target is 32-bit or 64-bit Intel, we will use asm to perform
# two-byte and four-byte swaps (on MSVC this is done using intrinsics;
# see hbyteswp.h for details.

# Unfortunately, CMake does not provide and easy-to-use primitive for
# determining the target platform.  So for the moment, if byteswap.h
# is not provided by the host system, we shall disable assembler byte
# swapping in hbyteswp.h.

herc_Check_Include_Files( byteswap.h        OK )
if( NOT HAVE_BYTESWAP_H )
    string( TOUPPER "${CMAKE_SYSTEM_PROCESSOR}" uland_proc )
    if( NOT ( "${uland_proc}" IN_LIST herc_Intel ) )
        set( NO_ASM_BYTESWAP 1 )
    endif( )
    unset( uland_proc )
endif( )


# Header malloc.h has been deprecated, and its content moved to stdlib.h.
# However, some Linux systems in their infinite wisdom (sorry for being
# snarky) retain the header and use it to define extensions to the scope
# of memory management functions.  Other distros, notably FreeBSD,
# include an #error directive telling you to include stdlib.h instead.
# I guess that moves from deprecated to being spit upon.   So...The BSD
# approach causes the test to fail, which is fine.  If a Linux system
# has malloc.h, then we will include it in Hercules, but I will bet that
# Hercules does not use the Linux extensions.

herc_Check_Include_Files( malloc.h          OK )


# termios.h, scandir, and alphasort, if avaiable, enable UNIX shell tab
# key handling.  If any are missing, no tab key handling.  Old versions
# of Solaris (pre 2.9, SunOS 5.9) lack these routines.  Header presence
# generates code in hconsole.c to put the console in character-by-
# character mode, and function presence generates code in fillfnam.c to
# scan directories when the tab key is pressed.  Termios.h also enables
# one-character command shortcuts on the CUI panel (after Esc).

# And while we are at it, we test the scandir prototype to see if the
# directory entry arrays passed to the filter and comparison functions
# are passed as const.

# Currently, hostopts.h assumes that only Windows includes const in the
# scandir function prototype.  However, FreeBSD 11 and Debian 8.6 also
# include const.  (I suspect all modern BSD and Linux systems include
# const in the prototype.)

# Code changes are needed in Hercules to replace the currentt target
# OS-based test, so that shall happen in V3.

herc_Check_Include_Files(   termios.h       OK )
herc_Check_Function_Exists( scandir         OK )
herc_Check_Function_Exists( alphasort       OK )
check_prototype_definition( scandir
        "int scandir(const char *dirp,
                struct dirent ***namelist,
                int (*filter)(const struct dirent *),
                int (*compar)(const struct dirent **, const struct dirent **));"
        "NULL"
        "dirent.h"
        CONST_IN_SCANDIR_PROTOTYPE )



# Headers and declarations for real tape devices.  On some systems (BSD?)
# sys/param.h must be included before sys/mount.h.  Header sys/mtio.h and
# MTEWARN are used only for SCSI tape utility routines (scsiutil.c)

herc_Check_Include_Files( sys/param.h             OK )
if( HAVE_SYS_PARAM_H )
    set( CMAKE_EXTRA_INCLUDE_FILES sys/param.h )
endif( )
herc_Check_Include_Files( sys/mount.h             OK )
unset( CMAKE_EXTRA_INCLUDE_FILES )

herc_Check_Include_Files( sys/mtio.h              OK )
herc_Check_Symbol_Exists( MTEWARN    sys/mtio.h   OK)


# Headers for shared device server routines (shared.c)

herc_Check_Include_Files(   sys/un.h        OK )


# See if zlib is installed.  Both the library and the public header need
# to be checked so the HAVE_ variables are set as needed for the build.
# If the library and header are missing, the build can continue.

herc_Check_Include_Files( zlib.h OK )
if( HAVE_ZLIB_H )
    set( CMAKE_REQUIRED_LIBRARIES ${link_libz} )
    check_library_exists( z uncompress "" HAVE_LIBZ )
    if( HAVE_LIBZ )
        set( link_libz ${name_libz} CACHE INTERNAL "include zlib commpression" )
    endif( )
    set( CMAKE_REQUIRED_LIBRARIES "" )
endif( )


# See if bzip2 is installed.  Both the library and the public header need
# to be checked so the HAVE_ variables are set as needed for comparison
# with any specified bzip2 user options.  Hercules does not require bzip2
# unless a user option requires it.

herc_Check_Include_Files( bzlib.h OK )
if( HAVE_BZLIB_H )
    set( CMAKE_REQUIRED_LIBRARIES ${link_libbz2} )
    check_library_exists( bz2 BZ2_bzBuffToBuffDecompress "" HAVE_BZ2 )
    if( HAVE_BZ2 )
        set( link_libbz2 ${name_libbz2} CACHE INTERNAL "include bzip2 compression" )
    endif( )
endif( )
set( CMAKE_REQUIRED_LIBRARIES "" )


# See if Open Object Rexx is installed.  Open Object Rexx will be
# included in the Hercules build if avaiable and not excluded by user
# option.  Hercules does not require Open Object Rexx to build correctly.
# The HAVE_ varibles created by these tests are not used by the build and
# need not be included in config.h

# The configure.ac script tested for both public headers, so we shall too.

herc_Check_Include_Files( oorexxapi.h OK )
herc_Check_Include_Files( rexx.h OK )


# See if Regina Rexx is installed.  Apparently the header rexxsaa.h
# has the potential to be installed in a "normal" directory or in a
# regina subdirectory; one presumes the latter location simplifies
# concurrent installation of Open Object Rexx and Regina Rexx.  Regina
# Rexx will be included in the Hercules build if avaiable and not
# excluded by user option.  Hercules does not require Regina Rexx
# to build correctly.

herc_Check_Include_Files( regina/rexxsaa.h OK )
if( NOT HAVE_REGINA_REXXSAA_H )
    herc_Check_Include_Files( rexxsaa.h OK )
endif( )

# Check for regular expression support as indicated by regex.h.
# Hercules does not require regular expression support to build.
# If regular expression support is present, the Automatic Operator
# functions will be included.

herc_Check_Include_Files( regex.h OK )


# See if sys/capability.h and the corresponding library cap exist.
# If they do, then Hercules can be built with support for fine-grained
# capabilities.  Header prctl.h enables some additional control
# over capabilities and is a libc-based wrapper for the system call of
# the same name.  Both Capabilities and prctl are Linux-only
# implementations of POSIX 1003.1e.

# Hercules code only checks for the headers; the functions and libraries
# are assumed to exist.  And if the library does not exist, we assume
# that there is no point to including the headers either, so we unset
# the HAVE_SYS_CAPABILITY_H variable too.  This makes sense for FreeBSD
# where the header exists but does something different from the Linux
# version.

herc_Check_Include_Files( sys/capability.h OK )
if( HAVE_SYS_CAPABILITY_H )
    check_library_exists( cap cap_set_proc "" HAVE_CAP )
    if( HAVE_CAP )
        set( link_libcap ${name_libcap} CACHE INTERNAL "include POSIX 1003.1e capabilities" )
        herc_Check_Include_Files( sys/prctl.h OK )
    else( )
        unset( HAVE_SYS_CAPABILITY_H )
    endif( )
endif( )


# Miscelaneous probes of the target userland

herc_Check_Symbol_Exists( LOGIN_NAME_MAX         "unistd.h"    OK )
herc_Check_Symbol_Exists( _SC_NPROCESSORS_CONF   "unistd.h"    OK )
herc_Check_Symbol_Exists( _SC_NPROCESSORS_ONLN   "unistd.h"    OK )


return ( )

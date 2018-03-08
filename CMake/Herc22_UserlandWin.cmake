# Herc22_Userland.cmake - perform Userland tests to build Hercules

#[[   Copyright 2017 by Stephen Orso.

      Distributed under the Boost Software License, Version 1.0.
      See accompanying file BOOST_LICENSE_1_0.txt or copy at
      http://www.boost.org/LICENSE_1_0.txt)
]]

#[[

This file checks the Windows SDK for this build to see that it is an SDK
for at least Windows XP Professional 64-bit, Windows version and mod
5.2.  Microsoft identifies Windows Server 2003 as 5.2 as well, and that
is fine too.  If the SDK is 5.2 or better, then Hercules will build;
SDK version is a sufficient test.

One other probe is needed; certain combinations of SDK and Visual Studio
multiply define the ceil function, once in the VS intrinsics and once in
the math.h library.  The work-around is to #include math.h first if this
is the case.

]]

message( "Checking Microsoft C compiler and Windows SDK versions" )

# CMake exposes the MSVC compiler version in MSVC_VERSION.  Easy test.

if( MSVC_VER STRLESS "1500" )
    herc_Save_Error("Unsupported Microsof Visual C compiler, less than 2008" )
endif( )

# The SDK version is tested by looking at macros defined in ntverp.h.
# An SDK that is at least 5.02 (Win XP Pro 64-Bit or Win2003 Server R2)
# can be used to build Hercules.  For the moment, we will not target
# versions of Windows older than the build system.  Although it is
# not difficult to target 5.02 (Win XP Pro 64 etc) by defining macros
# to specify an older target.

# VER_PRODUCTMAJORVERSION VER_PRODUCTMINORVERSION

# Target Windows XP Pro 64-bit by setting  _WIN32_WINNT and WINVER to
# _WIN32_WINNT_WS03 (0x0502) and setting NTDDI_VERSION to
# NTDDI_WS03 (0x05020000)

string( CONCAT herc_TestSource    # Test SDK level for at least 5.02
            "#include <windows.h>\n"
            "#include <ntverp.h>\n"
            "#if VER_PRODUCTMAJORVERSION < 5\n"
            "#define BAD_VERSION 1\n"
            "#endif\n"
            "#if VER_PRODUCTMAJORVERSION == 5 && VER_PRODUCTMINORVERSION < 2\n"
            "#define BAD_VERSION 1\n"
            "#endif\n"
            "#ifdef BAD_VERSION\n"
            "#error SDK in use is not at least WinXP Professional 64-bit or Windows Server 2003\n"
            "#endif\n"
            "int main() {return 0;}\n"
            )
herc_Check_Compile_Capability( "${herc_TestSource}" HAVE_5_02_SDK TRUE )
if( NOT HAVE_5_02_SDK )
    herc_Save_Error("Windows SDK does not support at least Windows 5.02 (Windows XP Pro 64-bit or Windows server 2003)" )
else( )
    message( STATUS "Supported Windows SDK found" )
endif( )


# Test to see if math.h must be included before intrin.h

string( CONCAT herc_TestSource    # Test SDK level for at least 5.02
            "#include <windows.h>\n"
            "#include <ntverp.h>\n"
            "#include <intrin.h>\n"
            "#include <math.h>\n"
            "int main() {return 0;}\n"
            )
herc_Check_Compile_Capability( "${herc_TestSource}" HAVE_MATH_H_FIRST FALSE )




# The following header files require windows.h to be included before the
# header under test, or the compilation of the header under test will
# fail.  We do not bother testing for them because all are in the SDK
# appropriate to any version of Windows that is supportd.

# herc_Check_Include_Files( "windows.h;wincon.h"          FAIL )
# herc_Check_Include_Files( "windows.h;tlhelp32.h"        FAIL )
# herc_Check_Include_Files( "windows.h;dbghelp.h"         FAIL )
# herc_Check_Include_Files( "windows.h;mstcpip.h"         FAIL )      # (need struct tcp_keepalive)
# herc_Check_Include_Files( "winsock2.h;netioapi.h"       FAIL )      # For if_nametoindex


# definitions in assert.h are used only when compiled under Microsoft,
# in only one module, hatomic.h, and it is assumed to exist under
# Microsoft.  So there is no need (for the moment) to test for it here.

# herc_Check_Include_Files( assert.h          FAIL )


# for Windows.h, consider lean_and_mean or more granular
# API exclusion to reduce build time.   See
# https://msdn.microsoft.com/en-us/library/windows/desktop/aa383745(v=vs.85).aspx

# All Hercules-supported Windows versions support IPV6.  But some of them
# require specific enablement of IPV6, and many of those systems, which are
# older versions of Windows, need specific programming for IPV6.



# Function strsignal helpful but not required.  If it does not exist, we
# need signal.h so that Hercules can compile its own version in strsignal.c.
# But because signal.h is required by dbgtrace.h and the build fails without
# it, strsignal is really optional for the build.

# herc_Check_Function_Exists( strsignal OK )


# Needed by a "BREAK_INTO_DEBUGGER" macro, defined in dbgtrace.h and
# referenced in hthreads.c and ptrace.c.  This is worth a further look.
# And if the macro is changed, the test above for strsignal needs
# to be augmented such that if strsignal does not exist, signal.h
# remains mandatory.

# herc_Check_Include_Files( signal.h FAIL )


# Windows (any supported version) supports Basic and Partial keepalive.

set( HAVE_BASIC_KEEPALIVE 1   )
set( HAVE_PARTIAL_KEEPALIVE 1 )
message( "Hercules on Windows has partial keeepalive support" )

message( "Checking C99 header stdbool.h" )

# Test for stdbool.h.  Its presence or absence needs to be recorded
# for correct expansion of the SoftFloat-3a headers included in ieee.c.
# Hercules will build without stdbool.h, but Hercules will not work
# when it is built without knowing about stdbool.h and SoftFloat-3a
# is built with it.

herc_Check_Include_Files( stdbool.h OK )


message( "Checking for optional headers and libraries." )


# See if Open Object Rexx is installed correctly.  Herc12_OptEdit.cmake
# set OOREXX_DIR to the correct installation directory for the bitness
# of the Hercules being built.  See if the two ooRexx headers are in the
# api directory.  If OOREXX_DIR is not set, then either there is
# no Open Object Rexx package installed, or the builder did not wish to
# include Open Object Rexx support.

if( OOREXX_DIR )
    set( CMAKE_REQUIRED_INCLUDES  "${OOREXX_DIR}/api" )
    herc_Check_Include_Files( oorexxapi.h OK )
    herc_Check_Include_Files( rexx.h OK )
    unset( CMAKE_REQUIRED_INCLUDES )
    if( NOT ( HAVE_REXX_H AND HAVE_OOREXXAPI_H) )
        herc_Save_Error( "Unable to locate Open Object Rexx development headers in \"${OOREXX_DIR}\\api\" " )
    endif( )
endif( )


# See if Regina Rexx is installed correctly.  Herc12_OptEdit.cmake
# set RREXX_DIR to the correct installation directory for the bitness
# of the Hercules being built.  See if the rexxsaa.h header is in the
# include directory.  If RREXX_DIR is not set, then either there is
# no Regina Rexx package installed, or the builder did not wish to
# include Regina Rexx support.


if( RREXX_DIR )
    set( CMAKE_REQUIRED_INCLUDES  "${RREXX_DIR}/include" )
    herc_Check_Include_Files( rexxsaa.h OK )
    unset( CMAKE_REQUIRED_INCLUDES )
    if( NOT HAVE_REXXSAA_H )
        herc_Save_Error( "Unable to locate Regina Rexx development header \"${RREXX_DIR}\\include\\rexxsaa.h\" " )
    endif( )
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


return ( )

# herc_setWindowsTarget.cmake - Set Windows API target version

#[[   Copyright 2017 by Stephen Orso.

      Distributed under the Boost Software License, Version 1.0.
      See accompanying file BOOST_LICENSE_1_0.txt or copy at
      http://www.boost.org/LICENSE_1_0.txt)
]]

macro( herc_setWindowsTarget WinTarget Windows_version )

#[[ ###########   macro herc_setWindowsTarget   ###########

Function/Operation
- Translates a symbolic Windows target version into a descriptive
  target version.
- If a specific version of Windows is the target, the variables
  WINVER, _WIN32_WINNT, and NTDDI_VERSION are set to the hex values
  that correspond to the target.
- If an invalid target version is specified, the returned descriptive
  target version is set to ${Windows_version}-NOTFOUND.

Input Parameters
- WinTarget: symbolic name of the minimum Windows version that compiled
  code should target.  In addition to the Windows version names listed
  in the variable Windows_versions, the following symbolic values are
  allowed:
  - DIST: Target the oldest version of Windows listed in Windows_versions.
    "DIST" means code is being compiled for distribution.
  - HOST: Target the version of Windows being used to compile.  In this
    case, WINVER, _WIN32_WINNT, and NTDDI_VERSION are not set, and the
    Windows userland will build for the host.
  - <blank>: a synonym for HOST.

Output
- If the symbolic target "DIST" is passed, then Windows_version is set
  the first value in the Windows_version list, and WINVER, _WIN32_WINNT,
  and NTDDI_VERSION are set to the hex values corresponding to that
  version.
- If the symbolic target "HOST" is passed, the host Windows header
  sdkddtver.h is analyzed to extract the WINVER hex string, that hex
  string is translated into the corresponding Windows version text, and
  WINVER, _WIN32_WINNT, and NTDDI_VERSION are not set.
  - If the hex string indicates a Windows version newer than the highest
    supported version of Windows, the highest supported version of
    Windows is reported with a "+" suffix.
  - If the hex string indicates a Windows version lower than the lowest
    supported version of Windows, Windows_version is set to
    ${Windows_version}-NOTFOUND.
- If any other version of Windows is requested, WINVER, _WIN32_WINNT,
  and NTDDI_VERSION are set to the hex values for that Windows version.
- If an invalid symbolic target version is passed, Windows_version is
  set to ${Windows_version}-NOTFOUND.
- If the build will target a valid Windows version, a message is issued
  naming the target Windows version.

Notes
- Windows target version names and codes are defined in Windows_versions
  and Windows_vercodes, respectively.  The version names names are our
  own invention; the hex strings are defined by Microsoft and documented
  here:

    https://msdn.microsoft.com/en-us/library/windows/desktop/aa383745(v=vs.85).aspx

  They are also defined in Windows header SDKDDKVer.h (the source for
  the Windows 10 values).
- The lowest supported version of Windows and its corresponding hex code
  must be the first element in the list, in orderby hex code.
- The highest supported version of Windows and its corresponding hex code
  must be the last element in the list, in orderby hex code.

Note that the versions listed reflect the Software Development Kit (SDK)
version, not the underlying host/target system.  The Windows SDK 6.0A
included with Visual C++ 2008 SP1 Express Edition, for example, reports
Windows Vista as the default target.

]]


set( Windows_versions     WINXP364   WINVISTA  WIN7    WIN8    WIN10  )
set( Windows_vercodes     0x0502     0x0600    0x0601  0x0602  0x0A00 )

list( LENGTH Windows_vercodes __winvercode_max )
math( EXPR __winvercode_max "${__winvercode_max} - 1" ) # decrement max into rel 0 index
list( GET Windows_vercodes "${__winvercode_max}" Windows_vercode_max )
list( GET Windows_versions "${__winvercode_max}" Windows_version_max )

string( TOUPPER "${WinTarget}"  __WinTarget )

if( "${__WinTarget}" STREQUAL "DIST" )
    # DIST means use the lowest supported Windows version as the target
    # for compilations.
    list( GET Windows_versions 0 ${Windows_version} )
    set ( __winvercode_index 0 )

elseif( ("${__WinTarget}" STREQUAL "HOST") OR ("${__WinTarget}" STREQUAL "" ) )
    # HOST or blank means target for the Windows version of the host
    # System.  See if the host is at least at the minimum version and
    # determine the appropriate descriptive string.
    find_path( __sdkddkver_h_path sdkddkver.h )
    file( STRINGS ${__sdkddkver_h_path}/sdkddkver.h __define_winver REGEX "#define  *WINVER  *0" )
    # Only one line in sdkddkver.h will match the above REGEX.  Extract
    # the Windows version code.
    string( REGEX REPLACE   "^#define +WINVER +"   ""  Windows_vercode  "${__define_winver}" )
    list( FIND Windows_vercodes "${Windows_vercode}" __winvercode_index )

    if( "${Windows_vercode_max}" STRLESS "${Windows_vercode}" )
    # NOTE: above test fails on EBCDIC systems because numbers colate
    # higher than letters on such systems.  ***************************
    # Target system Windows version higher than current highest known
    # supported Windows Version.  It will probably work.  Label it
    # WIN10+ and leave the target variables unset to accept the host
    # version as the target version.
        set( Windows_version "${Windows_version_max}+" )

    else( )
    # Host Windows version at or below highest supported, or is an
    # invalid Windows version string.
        list( FIND Windows_vercodes "${Windows_vercode}" __winvercode_index )
        if( __winvercode_index LESS 0 )   # Host is too old, not supported.
            set( Windows_version "${Windows_version}-${Windows_vercode}-NOTFOUND" )
        else( )   # Host is supported.  Get descriptor
            list( GET Windows_versions "${__winvercode_index}" Windows_version )
        endif( )
        unset( __winvercode_index )
    endif( )

else( )
# A Windows version was specified.  It must be in the list.
    list( FIND Windows_versions "${__WinTarget}" __winvercode_index )
    if( __winvercode_index LESS 0 )   # Host is not in list, invalid,
        set( Windows_version "${Windows_version}-${WinTarget}-NOTFOUND" )
        unset( __winvercode_index )
    else( )   # Host is supported.  Get descriptor
        list( GET Windows_versions "${__winvercode_index}" Windows_version )
    endif( )

endif( )

set( __WinTarget "${${Windows_version}}" )
if( __WinTarget )
    message( STATUS "Windows system target is ${${Windows_version}}" )
endif( )

# Set the Windows version codes if required because a caller specified
# DIST or a specific version of Windows.

if( NOT "${__winvercode_index}" STREQUAL "" )
    list( GET Windows_vercodes "${__winvercode_index}" Windows_vercode )
    set( WINVER "${Windows_vercode}" )
    set( _WIN32_WINNT "${WINVER}" )
    set( NTDDI_VERSION "${WINVER}0000" )
endif( )

unset( __sdkddkver_h_path )
unset( __define_winver )
unset( __winvercode_index )
unset( __WinTarget )

endmacro( )

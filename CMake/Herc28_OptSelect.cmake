# CMakeHercOptSelect.cmake - Set Hercules build user options for config.h

#[[   Copyright 2017 by Stephen Orso.

      Distributed under the Boost Software License, Version 1.0.
      See accompanying file BOOST_LICENSE_1_0.txt or copy at
      http://www.boost.org/LICENSE_1_0.txt)
]]


#[[

Process build options in the the context of:

 1. Options explicitly requested by the builder on the CMake command line
    or in the CMake GUI/ccmake.
 2. Build defaults in the absence of an option selection or exclusion by
    the builder.
 3. Capabilities of the target architecture, Operating System, and userland
    including packages installed on the target system, such as REXX.

If a conflict exists between an explicitly requested build option and the
capabilities of the target system, an error message is issued and the
build is terminated.

In the absence of an explicit exclusion, such as -DIPV6=NO, if the target
system supports inclusion of a given option, it is included.  The exceptions
are:
 * The getoptwrapper kludge is only included when requested.  If not
   requested, it is excluded.
 * The build includes fthreads only for Windows builds.  It is not included
   by default for non-Windows builds, and if explicitly requested for a
   non-Windows build, an error message is generated and the build is
   terminated.
 * All exceptions are documented in CMakeHercOptDef.cmake

Notes
- See the following thread for a discussion of the issues around use of
  file( DOWNLOAD ... ), which is used  here to determine the version
  of external packages included in the Hercules-390 github project.
     https://cmake.org/pipermail/cmake-developers/2013-July/019416.html
]]

message( "Setting build options" )

message( STATUS "External package root is ${EXTPKG_ROOT}" )



# ----------------------------------------------------------------------
#
# Option ADD-CFLAGS
#
# ----------------------------------------------------------------------

# We cannot process the ADD-CFLAGS here.  If specified, it must be
# appended to CMAKE_C_FLAGS after all C Compiler Options CMake scripts
# have run.  ADD-CFLAGS will be appended in CMakeLists.txt


# ----------------------------------------------------------------------
#
# Option AUTOMATIC-OPERATOR
#
# ----------------------------------------------------------------------

# Automatic Operator requires regular expression support, indicated by
# the presence of the regex.h header.

If( NOT "${HAVE_REGEX_H}"  )
    if( "${AUTOMATIC-OPERATOR}" )
        herc_Save_Error( "AUTOMATIC-OPERATOR=YES specified but required header \"regex.h\" is missing." )
    endif( )
elseif( ("${AUTOMATIC-OPERATOR}" STREQUAL "YES") OR (${buildWith_AUTOMATIC-OPERATOR} AND (NOT "${AUTOMATIC-OPERATOR}" STREQUAL "NO")) )
    set( OPTION_HAO 1 )
endif( )


# ----------------------------------------------------------------------
#
# Options BZIP2 and BZIP2_DIR
#
# ----------------------------------------------------------------------

# If BZIP2=SYSTEM, do nothing.  Herc12_OptEdit.cmake has already
# confirmed existence of a system BZip2 library.

# If BZIP2=NO, do nothing.  BZip2 support will be excluded.

# If BZIP2=YES or blank and BZIP2_DIR pointed to a valid BZip2
# development directory, then Herc12_OptEdit.cmake found the import
# target script and set HAVE_BZIP2_TARGET to its directory.  It will be
# imported here.

# If BZIP2=HERCULES, and BZIP2_DIR was not set, do nothing.  The script
# Herc12_OptEdit.cmake does not check for a system installed package
# in this case, and therefore BZIP2_FOUND will not be set.  The script
# Herc41_ExtPackageBuild.cmake will build the BZIP2 package.

# If BZIP2=YES, HERCULES, or blank, BZIP2_DIR was not set, and the
# target system has a version of BZip2 installed, compare the versions
# of the target system with the Hercules-390 BZip2 repository.  If the
# target system BZip2 is older, unsed BZIP2_FOUND so that it will not be
# used.   Herc41_ExtPackageBuild.cmake will build the BZIP2 package from
# the Hercules-390 BZip2 repository.

if( "${BZIP2}" STREQUAL "NO" )
    message( STATUS "BZIP2=NO: BZip2 support excluded from Hercules" )
    unset( HAVE_BZIP2_TARGET )
    unset( BZIP2_FOUND )

elseif( "${BZIP2}" STREQUAL "SYSTEM" )
    message( STATUS "BZIP2=SYSTEM: Using BZip2 ${BZIP2_VERSION_STRING} from system libraries." )
    unset( HAVE_BZIP2_TARGET )

elseif( HAVE_BZIP2_TARGET )
    include( ${HAVE_BZIP2_TARGET}/bzip2.cmake )
    message( STATUS "Using BZip2 found in specified directory ${BZIP2_INSTALL_DIR}" )
    unset( BZIP2_FOUND )

elseif( BZIP2_FOUND AND ( NOT "${BZIP2}" STREQUAL "HERCULES" ) )
    # Target system has a BZip2 library installed and builder does not
    # require use of the BZip2 in the Hercules-390 repository.  Use the
    # repository version only if it is newer than the library on the
    # target system.

    # Make a temporary file name
    set(_counter 0)
    while(EXISTS "${PROJECT_BINARY_DIR}/bzip_h_${_counter}.tmp")
        math(EXPR _counter "${_counter} + 1")
    endwhile( )
    set( _bzipver_temp "${PROJECT_BINARY_DIR}/bzip_h_${_counter}.tmp" )

    # get bzlib.h from hercules repo.  Note: URL below is the result
    # of the redirect done when displaying the file on github and
    # clicking the "raw" button.
    file( DOWNLOAD http://raw.githubusercontent.com/Hercules-390/h390BZip/master/pkg_src/bzlib.h
            "${_bzipver_temp}"
            TIMEOUT 10
            LOG _dl_log
            STATUS _bzip2_dl_status )

    if( NOT "${_bzip2_dl_status}" STREQUAL "0;\"No error\"" )
        message( WARNING "Unable to locate bzlib.h in Hercules-390 project: ${_bzip2_dl_status}" )
        message( WARNING "From file( DOWNLOAD ):\n${_dl_log}" )

    else( )
        # Get the BZip2 version from bzlib.h in the repository.  Tset it
        # agaist the version found on the target system by
        # find_package( BZip2 MODULE ).
        file( STRINGS "${_bzipver_temp}" herc_BZip2_Version
                REGEX "bzip2/libbzip2 version [0-9]+\\.[^ ]+ of [0-9]+ " )
        string( REGEX
                REPLACE ".* bzip2/libbzip2 version ([0-9]+\\.[^ ]+) of [0-9]+ .*" "\\1"
                herc_BZip2_Version "${herc_BZip2_Version}" )
        file( REMOVE "${_bzipver_temp}" )
        if( herc_BZip2_Version VERSION_GREATER BZIP2_VERSION_STRING )
            message( STATUS "Using BZip2 ${herc_BZip2_Version} from Hercules-390 project." )
            unset( BZIP2_FOUND )
        else( )
            message( STATUS "Using BZip2 ${BZIP2_VERSION_STRING} from system libraries." )
        endif( )
    endif( )

    unset( _counter )
    unset( _bzipver_temp )
    unset( _bzip2_dl_status )

endif( )


# ----------------------------------------------------------------------
#
# Option CAPABILITIES
#
# ----------------------------------------------------------------------

# This option allows finer control of process privileges than that
# afforded by setuid() all by itself.  And setuid() is a pre-requisite
# for capabilities.  The config.h variable is used in hmacros.h.

# Notes:
# 1. libcap-dev(el) is not normally included in a typical distro install,
#    so it must be explicitly installed.
# 2. FreeBSD does not appear to have a libcap package.
# 3. Due to a typo in configure.ac, capabilities were included in an
#    autotools build only when --enable-capabilities was explicitly
#    coded.  The default is no...despite coding that appears to set up
#    a default of yes.
# 4. Most open source systems require libcap-dev.  One (Leap) requires
#    libcap-devel.

If( NOT ("${HAVE_CAP}" AND "${HAVE_SYS_CAPABILITY_H}" AND (NOT "${NO_SETUID}")) )
    if( "${CAPABILITIES}" )
        if( NOT "${HAVE_CAP}" )
            herc_Save_Error( "CAPABILITIES=YES specified but required library \"cap\" is missing." )
        endif( )
        if( NOT "${HAVE_SYS_CAPABILITY_H}" )
            herc_Save_Error( "CAPABILITIES=YES specified but required header \"sys/capability.h\" is missing." )
        endif( )
        if( "${NO_SETUID}" )
            herc_Save_Error( "CAPABILITIES=YES specified but target system does not have setuid support." )
        endif( )
    endif( )
elseif( ("${CAPABILITIES}" STREQUAL "YES") OR (${buildWith_CAPABILITIES} AND (NOT "${CAPABILITIES}" STREQUAL "NO")) )
    set( OPTION_CAPABILITIES 1 )
    set( link_libcap  "${name_libcap}" CACHE INTERNAL "POSIX 1003.1e capabilities interface library" )
endif( )



# ----------------------------------------------------------------------
#
# Option CCKD-BZIP2
#
# ----------------------------------------------------------------------

# The bz2 library and public header are required for support of bzip2
# compression of CCKD files.  Note that if herc12_OptEdit.cmake did not
# identify a suitable bzip2 library, Herc41_ExtPackageBuild.cmake will
# build one.  So there is never a case where CCKD_BZIP2=YES cannot
# be honored.

if( ("${CCKD-BZIP2}" STREQUAL "YES") OR (${buildWith_CCKD-BZIP2} AND (NOT "${CCKD-BZIP2}" STREQUAL "NO")) )
    set( CCKD_BZIP2 1 )
endif( )


# ----------------------------------------------------------------------
#
# Option CUSTOM
#
# ----------------------------------------------------------------------

# If specified, set the variable CUSTOM_BUILD_STRING so the generated
# config.h includes it as a quoted string.

if( CUSTOM )
    set( CUSTOM_BUILD_STRING "${CUSTOM}" )
endif( )


# ----------------------------------------------------------------------
#
# Option DEBUG
#
# ----------------------------------------------------------------------

# Option DEBUG is a bit tricky because the CMake build option and the
# config.h preprocessor variable to be set have the same name.  We
# cannot use DEBUG as the variable containing the value to which the
# preprocessor variable should be set because it may be YES/NO/"" and
# config.h requires 1 or #undef.  So we use DEBUG_BUILD to transfer
# the value of 1 or #undef into config and use preprocessor code to
# get config.h DEBUG set/unset correctly.  Later we can change Hercules
# to use DEBUG_BUILD.  (Or, maybe better yet, change all preprocessor
# options that are driven by user CMake options to begin with "OPTION_".

if( ("${DEBUG}" STREQUAL "YES") OR (${buildWith_DEBUG} AND (NOT "${DEBUG}" STREQUAL "NO")) )
    set( DEBUG_BUILD 1 )
endif( )


# ----------------------------------------------------------------------
#
# Option EXTERNAL-GUI
#
# ----------------------------------------------------------------------

# Option EXTERNAL-GUI requires OPTION_DYNAMIC_LOAD.  But OPTION_DYNAMIC_LOAD
# is a basic requirement of Hercules, so there is nothing to test.

if( ("${EXTERNAL-GUI}" STREQUAL "YES")
            OR (${buildWith_EXTERNAL-GUI} AND (NOT "${EXTERNAL-GUI}" STREQUAL "NO")) )
    set( EXTERNALGUI 1 )
endif( )


# ----------------------------------------------------------------------
#
# Option EXTPKG_DIR
#
# ----------------------------------------------------------------------

# Edits on EXTPKG_DIR have validated the path, created the path, and set
# EXTPKG_ROOT to either the builder-specified or default external
# package path.  There is nothing to be done here.


# ----------------------------------------------------------------------
#
# Option FTHREADS
#
# ----------------------------------------------------------------------

# FTHREADS is supported only on Windows builds.

if( ("${FTHREADS}" STREQUAL "YES")
            OR (${buildWith_FTHREADS} AND (NOT "${FTHREADS}" STREQUAL "NO")) )
    if( WIN32 )
        set( OPTION_FTHREADS 1)
    else( )
        herc_Save_error( "Option FTHREADS is supported only for target system Windows." )
    endif( )
endif( )


# ----------------------------------------------------------------------
#
# Option GETOPTWRAPPER
#
# ----------------------------------------------------------------------

# autoconfig had a fairly complex test for the need for the getoptwrapper
# kludge.  And this has been transformed into a non-libtool version.  But
# absent a system that requires it, there can be no validation.  So we
# shall leave the test out and document the ommission with encouragement
# to report so the test case can be tested.  I suspect an early ELF system
# will be needed for testing.

if( ("${GETOPTWRAPPER}" STREQUAL "YES")
            OR (buildWith_GETOPTWRAPPER AND (NOT "${GETOPTWRAPPER}" STREQUAL "NO")) )
    set( NEED_GETOPT_WRAPPER 1 )
endif( )


# ----------------------------------------------------------------------
#
# Option HET-BZIP2
#
# ----------------------------------------------------------------------

# The bz2 library and public header are required for support of bzip2
# compression of het files.  Note that if herc12_OptEdit.cmake did not
# identify a suitable bzip2 library, Herc41_ExtPackageBuild.cmake will
# build one.  So there is never a case where HET_BZIP2=YES cannot
# be honored.

if( ("${HET-BZIP2}" STREQUAL "YES") OR (${buildWith_HET-BZIP2} AND (NOT "${HET-BZIP2}" STREQUAL "NO")) )
    set( HET_BZIP2 1 )
endif( )


# ----------------------------------------------------------------------
#
# Option HQA_DIR
#
# ----------------------------------------------------------------------

# If HQA_INSTALL_DIR is non-blank, then CMakeHercOptEdit.cmake has
# validated that the directory, whether specified as an environment
# variable or CMake command line option, exists.  Here we make sure that
# the directory, if defaulted, exists, and that, whether defaulted or
# specified, it has the required structure.

# If it does, then because CMakeCheckUserland.cmake has already run,
# the test for the header hqa.h is done here.

# Note that there should be no default for HQA_DIR, although we will
# presume there might be in this code.  Note also that the hqa.h
# header is optional even if the HQA_DIR option is specified.  Hercules
# will build quite nicely without it.

if( ("${HQA_INSTALL_DIR}" STREQUAL "" )
        AND ( NOT ("${buildWith_HQA_DIR}" STREQUAL "") ) )
    set( HQA_INSTALL_DIR "${buildWith_HQA_DIR}" )
    get_filename_component( HQA_INSTALL_DIR "${HQA_INSTALL_DIR}"
                ABSOLUTE
                BASE_DIR "${CMAKE_BINARY_DIR}"
                CACHE )
    if( NOT (IS_DIRECTORY "${HQA_INSTALL_DIR}") )
        herc_Save_Error( "Unable to locate default HQA installation directory \"${HQA_INSTALL_DIR}\\\" " )
    endif( )
endif( )

if( NOT ("${HQA_INSTALL_DIR}" STREQUAL "") )
#   See if hqa.h exists.  It is optional.
    set( CMAKE_REQUIRED_INCLUDES "${HQA_INSTALL_DIR}" )
    herc_Check_Include_Files( "hqa.h" OK )
    if( HAVE_HQA_H )
        message( STATUS "Including hqa.h build scenario from ${HQA_INSTALL_DIR}" )
    else( )
        message( STATUS "Build scenario header hqa.h not found in ${HQA_INSTALL_DIR}" )
    endif( )
    set( CMAKE_REQUIRED_INCLUDES "" )
endif( )


# ----------------------------------------------------------------------
#
# Option INTERLOCKED-ACCESS-FACILITY-2
#
# ----------------------------------------------------------------------

# This option requires some kind of atomic operation.  However, hatomic.h
# adapts to what the target host provides, even if that is nothing.  So
# we only need to set the variable for inclusion in config.h. The only
# oddball here is that config.h must *disable* it if requested, not enable
# it if needed.

if( ("${INTERLOCKED-ACCESS-FACILITY-2}" STREQUAL "YES")
        OR (${buildWith_INTERLOCKED-ACCESS-FACILITY-2}
                AND (NOT "${INTERLOCKED-ACCESS-FACILITY-2}" STREQUAL "NO")) )
else( )
    set( DISABLE_IAF2 1 )
endif( )


# ----------------------------------------------------------------------
#
# Option IPV6.
#
# ----------------------------------------------------------------------

# If required headers exist and not explicitly excluded by the builder,
# set the variable ENABLES_IPV6 to 1.  If required headers are missing
# and IPV6 was explicitly requested, save an error message and thereby
# terminate the build.

# Note that there are *NO* required headers for IPV6 at the moment.
# Due to a typo in hifr.h, a target system-provided header is never
# used to build IPV6 support.  The header hifr.h provides it's own
# The commented lines below are the original test from configure.ac
# to determine if IPV6 support is possible.  Right now, because hifr.h
# includes enough on its own.

If( FALSE )
#if test "$hc_cv_have_in6_ifreq_ifr6_addr" != "yes" &&
#      test "$hc_cv_have_in6_ifreq_ifru_addr" != "yes")
    if( "${IPV6}" )
        herc_Save_Error( "IPV6=YES specified but required headers for IPV6 support are missing." )
    endif( )
elseif( ("${IPV6}" STREQUAL "YES") OR (${buildWith_IPV6} AND (NOT "${IPV6}" STREQUAL "NO")) )
    set( ENABLE_IPV6 1 )
endif( )


# ----------------------------------------------------------------------
#
# Option LARGEFILE
#
# ----------------------------------------------------------------------

# All Windows versions supported by Hercules have large file support.
# Windows implements large file support by implementing a separate API
# for file operations, similar to UNIX-like transitional large file
# support.

# Large file access on UNIX-like systems requires a 64-bit file offset
# type (off_t) and file control functions that use that type.

# Some targets provide this as the only choice, including 64-bit open
# source targets, FreeBSD of any bitness since 2.0, and all Apple/Darwin
# (?) versions.  On these targets, off_t starts out as a 64-bit type and
# large file support is automatic and cannot be disabled.

# Some 32-bit open source targets, notably GNU/Linux targets, and Solaris
# use a 32-bit file offset by default.  Defining _FILE_OFFSET_BITS=64
# prior to including any system headers causes those headers to define a
# 64-bit off_t and corresponding file control functions.  To code being
# compiled, such targets behave exactly the same as targets for which a
# 64-bit off_t is the only choice.  The macro _LFS_LARGEFILE tells code
# being compiled that large file support is possible but does not tell
# if that code is enabled.

# Some 32-bit targets (which ones?) only support transitional large file
# support, which offers an alternative file offset type off64_t and file
# control function alternates that can exist side-by-side with the non-
# large type and functions.  Availability of transitional support is
# revealed by the _LFS64_LARGEFILE macro.  No specific enablement is
# needed for transitional support.  If Hercules is to be built without
# large file support on a system that includes transitional support,
# the build tools must instruct Hercules to not use transitional support.

# The _LFS_LARGEFILE and _LFS64_LARGEFILE macros are defined in posix_opt.h.

# Open source LFS support options are defined in section "1.6 Conformance"
# of http://www.unix.org/version2/whatsnew/lfs20mar.html.

# Hercules code supports both native (64-bit off_t) and transitional
# approaches to large file support).  The existence of _LFS_LARGEFILE
# and/or _LFS64_LARGEFILE is tested in Herc20_TargetEnv.cmake.

# Selected AIX systems need _LARGE_FILES set to 1 to enable LFS.
# The _LARGE_FILES macro in AIX apparently serves the same purpose as
# _FILE_OFFSET_BITS=64 in 32-bit open source systems.

# Here is detail quoted from SG24-5674-01, "Developing and Porting C
# and C++ Applications on AIX," June 2003, p. 129 , which applies to the
# compilers available on AIX 5L version 5.2.  The prior version of the
# Redbook does not mention Large File System support at all.
#    "Beginning with AIX Version 4.2.1, there are two methods to work
#     around the addressability of large files in the 32-bit user
#     process model:
#       If the -D_LARGE_FILE option is specified when compiling the
#       program, the off_t type is redefined as long long (64-bit signed
#       integer). Also, most system calls and library routines are
#       redefined to support large files."
# 64-bit versions of AIX support LFS with the same preparations as
# open source 64-bit systems, i.e., none.  See p. 139.

# And off_t is defined consistent with the target's
# large file support capabilities, 4 if none enabled, or 8 if large file
# support is native or enabled.  If fseeko is not defined, then Hercules
# will use fseek, which has a file offset parameter of long.


if( ( ${SIZEOF_OFF_T} EQUAL 8 ) OR WIN32 )
# If the target's off_t is 8 bytes (64 bits) by default or we are building
# for Windows, then large file support is present by default and cannot
# be turned off or disabled.
    set( buildWith_LARGEFILE          "YES" CACHE INTERNAL "${help_Sumry_LARGEFILE}" )
    if( "${LARGEFILE}" STREQUAL "NO" )
        Herc_Save_Error( "Large file support cannot be disabled on this target; LARGEFILE=${LARGEFILE} rejected." )
    endif( )
    set( herc_LargeFile_Status "Using native large file support." )

else( )
# Change default for LFS support based on target capabilities.  And issue
# diagnostic and fail the build if LARGEFILE requested on a system that
# that does not have native or transitional large file support.
    if( NOT (HAVE_DECL__LFS_LARGEFILE OR HAVE_DECL__LFS64_LARGEFILE) )
        set( buildWith_LARGEFILE          "NO"  CACHE INTERNAL "${help_Sumry_LARGEFILE}" )
        set( DISABLE_LARGEFILE 1 )
        set( herc_LargeFile_Status "Not available on this target." )
        if( "${LARGEFILE}" STREQUAL "YES" )
            Herc_Save_Error( "Large File System support is not available on this target; LARGEFILE=${LARGEFILE} rejected." )
        endif( )
    endif( )
endif( )

if( NOT ( (${SIZEOF_OFF_T} EQUAL 8) OR WIN32 OR DISABLE_LARGEFILE ) )
# LFS-capable target and LFS support requested or defaulted.  See what
# macro is needed to enable that support.
    if( ("${LARGEFILE}" STREQUAL "YES")
            OR ( ("${LARGEFILE}" STREQUAL "") AND ("${buildWith_LARGEFILE}" STREQUAL "YES") ) )
        # Try _FILE_OFFSET_BITS = 64 (GNU/Linux) first and see if
        # SIZEOF_OFF_T changes from 4 to 8.
        set( CMAKE_REQUIRED_DEFINITIONS "-D_FILE_OFFSET_BITS=64" )
        set( CMAKE_EXTRA_INCLUDE_FILES "sys/types.h" )
        CHECK_TYPE_SIZE( "off_t" SIZEOF_OFF_T_X LANGUAGE C )
        if( ${SIZEOF_OFF_T_X} EQUAL 8 )
            set( herc_LargeFile_Status "Using specifically-enabled large file support." )
            set( _FILE_OFFSET_BITS 64 )
            set( SIZEOF_OFF_T 8 CACHE INTERNAL "CHECK_TYPE_SIZE: sizeof(off_t)" FORCE)
        else( )
            # Try _LARGE_FILE = 1 (AIX) and see if SIZEOF_OFF_T changes
            # from 4 to 8.
            unset( SIZEOF_OFF_T_X )
            set( CMAKE_REQUIRED_DEFINITIONS "-D_LARGE_FILE=1" )
            CHECK_TYPE_SIZE( "off_t" SIZEOF_OFF_T LANGUAGE C )
            if( ${SIZEOF_OFF_T_X} EQUAL 8 )
                set( herc_LargeFile_Status "Using specifically-enabled large file support." )
                set( SIZEOF_OFF_T 8 CACHE INTERNAL "CHECK_TYPE_SIZE: sizeof(off_t)" FORCE)
                set( _LARGE_FILE 1 )
            else( )
                if( "${LARGEFILE}" STREQUAL "YES" )
                    string( CONCAT err_msg "LARGEFILE=${LARGEFILE} specified and target system advertises large file support,\n"
                            "but CMake is unable to determine how to enablet that support."
                            )
                    herc_Save_Error( "${err_msg}" )
                    unset( err_msg )
                endif( )
            endif( )
        endif( )
        set( CMAKE_REQUIRED_DEFINITIONS "" )
        set( CMAKE_EXTRA_INCLUDE_FILES "" )
    else( )
        set( herc_LargeFile_Status "Available but suppressed by -DLARGEFILE=NO" )
    endif( )
endif( )


# Ensure Apple Darwin (Mac OS 10.5 Leopard) uses a 64-bit inode if LFS
# is enabled.  This changes the definition of the stat structure in
# sys/stat.h.  Mac OS greater than 10.5 does not require this define.
# See https://developer.apple.com/legacy/library/documentation/Darwin/Reference/ManPages/man2/stat64.2.html
# for details and confirmation that this macro is required only for 10.5.

if( (${SIZEOF_OFF_T} equal 8)
        AND ( APPLE )
        AND (CMAKE_SYSTEM_NAME MATCHES "Darwin" ) )
    set( _DARWIN_USE_64_BIT_INODE 1 )
endif( )


# ----------------------------------------------------------------------
#
# Option MULTI-CPU
#
# ----------------------------------------------------------------------

# If YES, support for 8 CPUs is generated.  If NO, a single-CPU Hercules
# is build.  If a number (already edited to be between 1 and 128) that
# number of CPUs is built if the target architecture supports it.  The
# uint128_t type are required for more than 64 processors.  If MULTI_CPU
# is not defined for the build, hconsts.h will check the __uint128_t
# type and set the cpu count to be build to 128 if present and 64 otherwise.

if( NOT ("${MULTI-CPU}" STREQUAL "") )
    set( temp_Multi_CPU "${MULTI-CPU}" )
else( )
    set( temp_Multi_CPU "${BuildWith_MULTI-CPU}" )
endif( )

if( ("${temp_Multi_CPU}" STREQUAL "YES") )
    set( MAX_CPU_ENGINES 8 )
elseif( ("${temp_Multi_CPU}" STREQUAL "NO") )
    set( MAX_CPU_ENGINES 1 )
elseif( NOT ("${temp_Multi_CPU}" STREQUAL "") )
    if( NOT HAVE___uint128_t AND ("${temp_Multi_CPU}" GREATER 64) )
        if( NOT ("${MULTI-CPU}" STREQUAL "") )
            string( CONCAT err_msg
                    "MULTI-CPU=${temp_Multi_CPU} exceeds maximum of 64 on systems without type __uint128_t\n"
                    "             Note: 32-bit systems do not generally include __uint128_t"
                    )
            herc_Save_Error( "${err_msg}" )
            unset( err_msg )
        else( )
            herc_Save_Error( "Build default for MULTI-CPU=${temp_Multi_CPU} exceeds maximum of 64; see cmake/CMakeHercOptDef.cmake" )
        endif( )
    else( )
        set( MAX_CPU_ENGINES "${temp_Multi_CPU}" )
    endif( )
endif( )
unset( temp_Multi_CPU )


# ----------------------------------------------------------------------
#
# Option OBJECT_REXX
#
# ----------------------------------------------------------------------

# Object Rexx requires two headers, rexx.h and oorexxapi.h.

If( NOT ("${HAVE_REXX_H}" AND "${HAVE_OOREXXAPI_H}") )
    if( "${OBJECT-REXX}" )
        if( NOT "${HAVE_REXX_H}" )
            herc_Save_Error( "OBJECT-REXX=YES specified but required header \"rexx.h\" is missing." )
        endif( )
        if( NOT "${HAVE_OOREXXAPI_H}" )
            herc_Save_Error( "OBJECT_REXX=YES specified but required header \"oorexxapi.h\" is missing." )
        endif( )
    endif( )
elseif( ("${OBJECT-REXX}" STREQUAL "YES") OR (buildWith_OBJECT-REXX AND (NOT ("${OBJECT-REXX}" STREQUAL "NO"))) )
    set( ENABLE_OBJECT_REXX 1 )
endif( )


# ----------------------------------------------------------------------
#
# Options PCRE and PCRE_DIR
#
# ----------------------------------------------------------------------

# If PCRE=NO, do nothing.  PCRE support will be excluded.

# If PCRE=YES or blank and PCRE_DIR pointed to a valid PCRE development
# directory, then Herc12_OptEdit.cmake found the import target script
# and set HAVE_PCRE_TARGET to its directory.  It will be imported here.

# If PCRE=YES or blank and PCRE_DIR was not set,
# Herc41_ExtPackageBuild.cmake will build the PCRE package from
# the Hercules-390 PCRE repository.

# We ensure that PCRE_FOUND is unset even though Windows systems do not
# have a system-installed version of PCRE because that lets the code for
# PCRE processing in this build to be consistent with that for BZip2 and
# Zlib.

# The PCRE build creates two targets: PCRE and PCREPOSIX.  Both are
# included in the pcre.cmake target import script.

if( WIN32 )
    if( "${PCRE}" STREQUAL "NO" )
        message( STATUS "PCRE=NO: PCRE support excluded from Hercules" )
        unset( HAVE_PCRE_TARGET )
        unset( PCRE_FOUND )

    elseif( HAVE_PCRE_TARGET )
        include( ${HAVE_PCRE_TARGET}/pcre.cmake )
        message( STATUS "Using PCRE found in specified directory ${PCRE_INSTALL_DIR}" )
        unset( PCRE_FOUND )

    endif( )
endif( )


# ----------------------------------------------------------------------
#
# Option REGINA_REXX
#
# ----------------------------------------------------------------------

# Regina Rexx needs rexxsaa.h, which may or may not be in the regina subdirectory.

If( NOT ("${HAVE_REXXSAA_H}" OR "${HAVE_REGINA_REXXSAA_H}") )
    if( "${REGINA-REXX}" )
        herc_Save_Error( "REGINA-REXX=YES specified but required header \"rexxsaa.h\" is missing." )
    endif( )
elseif( ("${REGINA-REXX}" STREQUAL "YES") OR (buildWith_REGINA-REXX AND (NOT ("${REGINA-REXX}" STREQUAL "NO"))) )
    set( ENABLE_REGINA_REXX 1 )
endif( )


# ----------------------------------------------------------------------
#
# Option S3FH_DIR
#
# ----------------------------------------------------------------------

# If S3FH_DIR is blank, then Herc41_ExtPackageBuild.cmake will build
# SoftFloat-3a and import its targets.  No action is required.

# If S3FH_DIR is non-blank, then Herc12_OptEdit.cmake has validated
# that an import target script exits.  We will import it here.

if( HAVE_S3FH_TARGET )
    include( ${HAVE_S3FH_TARGET}/s3fh.cmake )
endif( )


# ----------------------------------------------------------------------
#
# Option SETUID-HERCIFC
#
# ----------------------------------------------------------------------

# The decision to build hercifc is based on platform and made in CMakeHercTarget.
# If we are not building hercifc, then SETUID-HERCIFC is invalid.  If
# SETUID-HERCIFC is other than YES or NO, then it is understood to be
# a group name, and hercifc is set to that.

if( NOT BUILD_HERCIFC )
    if( NOT ("${SETUID-HERCIFC}" STREQUAL ""
            OR "${SETUID-HERCIFC}" STREQUAL "NO") )
        herc_Save_Error( "SETUID-HERCIFC=${SETUID-HERCIFC} specified but hercifc will not be built on this platform." )
    endif( )
else( )
    if( NOT "${SETUID-HERCIFC}" STREQUAL "NO" )
        set( SETUID_HERCIFC 1 )
        if( NOT "${SETUID-HERCIFC}" STREQUAL "YES" )
            set( HERCIFC_GROUPSET 1 )
            set( HERCIFC_GROUPSET "${SETUID-HERCIFC}" )
        endif( )
    endif( )
endif( NOT BUILD_HERCIFC )


# ----------------------------------------------------------------------
#
# Option SYNCIO
#
# ----------------------------------------------------------------------

# The default for this is currently yes, but will change in the near
# future.  There are no header or library requirements for this option.

if( ("${SYNCIO}" STREQUAL "YES") OR (${buildWith_SYNCIO} AND (NOT "${SYNCIO}" STREQUAL "NO")) )
    set( OPTION_SYNCIO 1 )
else( )
    set( OPTION_NOSYNCIO 1 )
endif( )


# ----------------------------------------------------------------------
#
# Option WINTARGET
#
# ----------------------------------------------------------------------

# Preprocessor variables for WINTARGET were set in herc12_OptEdit.cmake.
# Nothing needs to be done here.


# ----------------------------------------------------------------------
#
# Options ZLIB and ZLIB_DIR
#
# ----------------------------------------------------------------------

# If ZLIB=SYSTEM, do nothing.  Herc12_OptEdit.cmake has already
# confirmed existence of a system Zlib library.

# If ZLIB=NO, do nothing.  Zlib support will be excluded.

# If ZLIB=YES or blank and ZLIB_DIR pointed to a valid Zlib
# development directory, then Herc12_OptEdit.cmake found the import
# target script and set HAVE_ZLIB_TARGET to its directory.  It will be
# imported here.

# If ZLIB=HERCULES, and ZLIB_DIR was not set, do nothing.  The script
# Herc12_OptEdit.cmake does not check for a system installed package
# in this case, and therefore ZLIB_FOUND will not be set.  The script
# Herc41_ExtPackageBuild.cmake will build the ZLIB package.

# If ZLIB=YES, HERCULES, or blank, ZLIB_DIR was not set, and the
# target system has a version of Zlib installed, compare the versions
# of the target system with the Hercules-390 Zlib repository.  If the
# target system Zlib is older, unsed ZLIB_FOUND so that it will not be
# used.   Herc41_ExtPackageBuild.cmake will build the ZLIB package from
# the Hercules-390 Zlib repository.

if( "${ZLIB}" STREQUAL "NO" )
    message( STATUS "ZLIB=NO: Zlib support excluded from Hercules" )
    unset( HAVE_ZLIB_TARGET )
    unset( ZLIB_FOUND )

elseif( "${ZLIB}" STREQUAL "SYSTEM" )
    message( STATUS "ZLIB=SYSTEM: Using Zlib ${ZLIB_VERSION_STRING} from system libraries." )
    unset( HAVE_ZLIB_TARGET )

elseif( HAVE_ZLIB_TARGET )
    include( ${HAVE_ZLIB_TARGET}/zlib.cmake )
    message( STATUS "Using Zlib found in specified directory ${ZLIB_INSTALL_DIR}" )
    unset( ZLIB_FOUND )

elseif( ZLIB_FOUND AND ( NOT "${ZLIB}" STREQUAL "HERCULES" ) )
    # Target system has a Zlib library installed and builder does not
    # require use of the Zlib in the Hercules-390 repository.  Use the
    # repository version only if it is newer than the library on the
    # target system.

    # Make a temporary file name
    set(_counter 0)
    while(EXISTS "${PROJECT_BINARY_DIR}/zlib_h_${_counter}.tmp")
        math(EXPR _counter "${_counter} + 1")
    endwhile( )
    set( _zlibver_temp "${PROJECT_BINARY_DIR}/zlib_h_${_counter}.tmp" )

    # get bzlib.h from hercules repo.  Note: URL below is the result
    # of the redirect done when displaying the file on github and
    # clicking the "raw" button.
    file( DOWNLOAD http://raw.githubusercontent.com/Hercules-390/h390Zlib/master/pkg_src/zlib.h
            "${_zlibver_temp}"
            TIMEOUT 10
            LOG _dl_log
            STATUS _zlib_dl_status )

    if( NOT "${_zlib_dl_status}" STREQUAL "0;\"No error\"" )
        message( WARNING "Unable to locate zlib.h in Hercules-390 project: ${_zlib_dl_status}" )
        message( WARNING "From file( DOWNLOAD ):\n${_dl_log}" )

    else( )
        # Get the Zlib version from zlib.h in the repository.  Tset it
        # agaist the version found on the target system by
        # find_package( Zlib MODULE ).  The Zlib version is #define'd.
        file( STRINGS "${_zlibver_temp}" herc_Zlib_Version
                    REGEX "^#define[ \t]+ZLIB_VERSION[ \t]+\"[^\"]*\"$")
        string( REGEX
                REPLACE "^#define +ZLIB_VERSION +\"([0-9]+\\.[^\"]+).*" "\\1"
                herc_Zlib_Version "${herc_Zlib_Version}" )
        file( REMOVE "${_zlibver_temp}" )
        if( herc_Zlib_Version VERSION_GREATER ZLIB_VERSION_STRING )
            message( STATUS "Using Zlib ${herc_Zlib_Version} from Hercules-390 project." )
            unset( ZLIB_FOUND )
        else( )
            message( STATUS "Using Zlib ${ZLIB_VERSION_STRING} from system libraries." )
        endif( )
    endif( )

    unset( _counter )
    unset( _zlibver_temp )
    unset( _zlib_dl_status )

endif( )



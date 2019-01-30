# Herc12_OptEdit.cmake - Edit Hercules build user options

#[[   Copyright 2017 by Stephen Orso.

      Distributed under the Boost Software License, Version 1.0.
      See accompanying file BOOST_LICENSE_1_0.txt or copy at
      http://www.boost.org/LICENSE_1_0.txt)
]]


#[[
This script edits user options specified on the command line for the
Hercules build.  Edits are relatively simple; yes/no options must
recognizably be yes/no (YES/yes/Y/N etc.), paths must exist, and,
for multi-cpu, if not yes/no, then a value between 1 and 128 inclusive.

If some variant of yes/no is specified, it is canonicallized to
YES/NO to simplify testing when the options are processed.

Only syntax errors are handled here.  Herc28_OptSelect.cmake reports
errors such as requesting something not supported by the target system,
SoftFloat-3a not being in the directory specified, or a cpu count that
exceeds the maximum supported by the target architecture or userland
(64-bit and uint128_t are needed).

For _DIR options, editing includes issuing the find_package() command
to collect paths to the public headers and libraries.

]]


if( DEFINED HELP )
    foreach( help_Text IN LISTS herc_OptionList )
        string( LENGTH "${help_Text}" opt_len)
        if ( ${opt_len} LESS 14 )
            string( SUBSTRING "${help_Text}:                   " 0 15 pad )
        else( )
            set( pad "${help_Text}:" )
        endif( )
        message( "${pad} ${help_Sumry_${help_Text}}" )
    endforeach( )
    unset( HELP CACHE )
    unset( HELP )
    return( )
endif( )


herc_Check_User_Option_YesNo( AUTOMATIC-OPERATOR FAIL )
herc_Check_User_Option_YesNo( CAPABILITIES       FAIL )
herc_Check_User_Option_YesNo( CCKD-BZIP2         FAIL )
herc_Check_User_Option_YesNo( DEBUG              FAIL )
herc_Check_User_Option_YesNo( EXTERNAL-GUI       FAIL )
herc_Check_User_Option_YesNo( FTHREADS           FAIL )
herc_Check_User_Option_YesNo( GETOPTWRAPPER      FAIL )
herc_Check_User_Option_YesNo( HET-BZIP2          FAIL )
herc_Check_User_Option_YesNo( INTERLOCKED-ACCESS-FACILITY-2 FAIL )
herc_Check_User_Option_YesNo( IPV6               FAIL )
herc_Check_User_Option_YesNo( LARGEFILE          FAIL )
herc_Check_User_Option_YesNo( OBJECT-REXX        FAIL )
herc_Check_User_Option_YesNo( SYNCIO             FAIL )

herc_Check_User_Option_YesNoSysHerc( BZIP2       FAIL )
herc_Check_User_Option_YesNoSysHerc( ZLIB        FAIL )


# For option ADD-CFLAGS, we will run a trial compile with the provided
# options.  If the compile fails, then the options are invalid.

if( NOT ("${ADD-CFLAGS}" STREQUAL "") )
    string( CONCAT herc_TestSource    # Test added flags for validity.
                "/* Test for valid added c flags */\n"
                "int main()\n"
                "   {return 0;}\n"
                )

    file( WRITE ${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/HercTempProg.c "${herc_TestSource}" )

    try_compile( tc_return_var ${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}
                SOURCES ${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/HercTempProg.c
                COMPILE_DEFINITIONS ${ADD-CFLAGS} ${all_warnings_are_errors}
                OUTPUT_VARIABLE tc_output_var
            )

    if( NOT tc_return_var )
        set( opt_err_msg "ADD-CFLAGS \"${ADD-CFLAGS}\" rejected by c compiler.\n\n${tc_output_var}" )
        herc_Save_Error( "${opt_err_msg}" )
        unset( opt_err_msg )
    endif( )
endif( )


# BZIP2 cannot be SYSTEM if the build target is Windows.  If BZIP2
# is SYSTEM, then BZip2 must be installed on the target system and
# can be found by find_package( BZip2 MODULE ).

# Herc28 will import any target if BZIP2_DIR has been set correctly.
# Herc41_ExtPackageBuild.cmake will decide whether we build BZip2 or
# build an import target from information returned by find_package( )
# based on the setting of BZIP2 and comparng the versions of the
# target system's BZip2 versus the Zlib repository in the Hercules-390
# project.

if( NOT "${BZIP2}" STREQUAL "NO" )
    #  BZIP2 is blank, YES, HERCULES, or SYSTEM.  If this is a
    #  non-Windows target, see if
    #  BZip2 headers and library are installed in the system directories.

    if( NOT ( WIN32 OR "${BZIP2}" STREQUAL "HERCULES" ) )
        # Use of the BZip2 library installed on the system is a
        # possibility.  See what version if any is installed.
        find_package(BZip2 MODULE )
    endif( )

    if( "${BZIP2}" STREQUAL "SYSTEM" )
        if( WIN32 )   # SYSTEM not valid for Windows targets
            herc_Save_Error( "BZIP2=SYSTEM can only be specified for non-Windows targets." )
        elseif( NOT BZIP2_FOUND )   # No BZip2 lib/headers installed
            herc_Save_Error( "BZIP2=SYSTEM but BZip2 headers and/or library are not installed." )
        endif( )
        # Herc41_ExtPackageBuild.cmake will create the import target
        # using the information returned by find_package(BZip2 MODULE ).
    endif( )
    # Herc28_OptSelect.cmake will decide whether to use the system-
    # installed BZip2 based on its existence and version, and whether
    # BZIP2_DIR points us at a specific build or install directory.
endif( )


# CCKD-BZIP2 cannot be YES if BZIP2=NO.

if( "${CCKD-BZIP2}" STREQUAL "YES" )
    if( "${BZIP2}" STREQUAL "NO" )
        herc_Save_Error( "CCKD-BZIP2=YES cannot be specified with BZIP2=NO" )
    endif( )
endif( )


# There are no edits for option CUSTOM.  It is there or not.


# EXTPKG_DIR must either 1) exist and be writable or 2) creatable.
# Because the directory may not be needed--as would be the case if
# installation directories were specified for all external
# packages, we might be creating a directory we will not use.  If
# the builder specifies this option, then we presume he or she will not
# be surprised by directory creation even if it is not used.  And the
# default location for EXTPKG_DIR is in the Hercules build directory,
# so creating an additional diretory there should not be a (big?)
# problem.

if( "${EXTPKG_DIR}" STREQUAL "")
    set( EXTPKG_ROOT "${buildWith_EXTPKG_DIR}" )
else( )
    set( EXTPKG_ROOT "${EXTPKG_DIR}" )
endif( )

get_filename_component( EXTPKG_ROOT "${EXTPKG_ROOT}"
            ABSOLUTE
            BASE_DIR "${CMAKE_BINARY_DIR}"
            CACHE )

# Create the EXTPKG directory if it does not exist
if( NOT (EXISTS ${EXTPKG_ROOT} ) )
    file( MAKE_DIRECTORY ${EXTPKG_ROOT} )
endif( )

# directory was created, or something (file or directory) exists.
# make sure it is a directory.
if( NOT (IS_DIRECTORY "${EXTPKG_ROOT}") )
    if( "${EXTPKG_DIR}" STREQUAL "")
        herc_Save_Error( "Unable to locate or create default EXTPKG_DIR directory \"${buildwith_EXTPKG_DIR}\" (= \"${EXTPKG_ROOT}\")" )
    else( )
        herc_Save_Error( "Unable to locate or create specified EXTPKG_DIR directory \"${EXTPKG_DIR}\" (= \"${EXTPKG_ROOT}\") " )
    endif( )

else( )     # It is a directory.  Make sure we can create a subdirectory in it.
#   get a timestamp of the form yyyydddhhmmss
    string( TIMESTAMP temp_fname "%Y-%j-%H-%M-%S" )
    set( temp_fname "CMakeDirTest${temp_fname}" )
    execute_process(
            COMMAND ${CMAKE_COMMAND} -E make_directory ${EXTPKG_ROOT}/${temp_fname}
            RESULT_VARIABLE md_result_var
            OUTPUT_VARIABLE md_output_var
            ERROR_VARIABLE md_output_var
        )
    if( "${md_result_var}" STREQUAL "0" )
        execute_process( COMMAND ${CMAKE_COMMAND} -E remove_directory ${EXTPKG_ROOT}/${temp_fname} )
    else( )
        string( CONCAT md_output_var
                "Unable to create directory, rc=${md_result_var}, in EXTPKG_DIR directory \"${EXTPKG_ROOT}\"\n"
                "           Message: ${md_output_var}"
            )
        herc_Save_Error( "${md_output_var}" )
    endif( )
    unset( md_result_var )
    unset( md_output_var )
endif( )


# Git protocol to be used by default when cloning external packages and
# the HTML documentation project.  The git: protocol is the fastest but
# includes no validation of the source of a clone.  It uses a high port
# number and is often blocked on company networks.  The https: protocol
# represents a good compromise.

string( TOLOWER "${GIT_CLONE}" _GIT_CLONE )
if ( NOT (
           ("${_GIT_CLONE}" STREQUAL "" )
        OR ("${_GIT_CLONE}" STREQUAL "https:" )
        OR ("${_GIT_CLONE}" STREQUAL "git:" ) ) )
    herc_Save_Error( "Invalid -DGIT_CLONE= value \"${GIT_CLONE}\".  Must be \"https:\" or \"git:\"" )
endif( )
unset( _GIT_CLONE )


# HET-BZIP2 cannot be YES if BZIP2=NO.

if( "${HET-BZIP2}" STREQUAL "YES" )
    if( "${BZIP2}" STREQUAL "NO" )
        herc_Save_Error( "HET-BZIP2=YES cannot be specified with BZIP2=NO" )
    endif( )
endif( )


# MULTI-CPU may be YES, NO, or a value up to the maximum supported
# by the hardware and userland (must be 64-bit and have uint128_t
# for 128 CPUs, otherwise the max is 64).  We do not know about
# hardware and userland yet, so we shall syntax edit only for
# the moment.

herc_Check_User_Option_YesNo( MULTI-CPU OK )

if( NOT ( ("${MULTI-CPU}" STREQUAL "")
            OR ("${MULTI-CPU}" STREQUAL "YES")
            OR ("${MULTI-CPU}" STREQUAL "NO" ) ) )
    if( NOT ( ${MULTI-CPU} MATCHES "^[0-9]+$" ) )
        herc_Save_Error( "Invalid value \"${MULTI-CPU}\" for MULTI-CPU, must be YES, NO, or a number between 1 and 128" )
    elseif( (${MULTI-CPU} GREATER 128) OR (${MULTI-CPU} LESS 1) )
        herc_Save_Error( "Out of range value \"${MULTI-CPU}\" for MULTI-CPU, must be YES, NO, or a number between 1 and 128" )
    endif( )
endif( )


# OPTIMIZATION may be YES, NO, or anything the c compiler will accept.
# So anything goes.  We call the Yes/No function to canonicalize
# Y/y/YES/Yes/yes into YES etc for later testing.  Note that the
# Yes/No function *does not* change the value if it not a synonym for
# Yes, No, or Target.  If an option string is specified, we run a
# test compile to make sure the specified options are valid for the
# compiler.  If YES is specified, we make sure the compiler is one
# that has an optimization script.

herc_Check_User_Option_YesNo( OPTIMIZATION OK )

# If YES is specified or defaulted, we have to be able to recognize
# the c compiler as one for which an automated optimization script
# exists.  Apple uses clang or gcc depending on version, so while
# we use a separate script for Apple, we do not need a separate test.

if( NOT (${CMAKE_C_COMPILER_ID} IN_LIST herc_Compilers )
        AND ( ("${OPTIMIZATION}" STREQUAL "YES")
            OR ( ("${OPTIMIZATION}" STREQUAL "")
                AND ("${buildWith_OPTIMIZATION}" STREQUAL "YES") ) )
        )
    herc_Save_Error( "Unrecognized c compiler \"${CMAKE_C_COMPILER_ID}\" - automatic optimization not possible." )
endif( )

unset( opt_flags)
if( (NOT ( "${OPTIMIZATION}" IN_LIST herc_YES_NO)) )
    set( opt_flags "${OPTIMIZATION}" )
elseif( ("${OPTIMIZATION}" STREQUAL "")
            AND (NOT ("${buildWith_OPTIMIZATION}" IN_LIST herc_YES_NO) )
    )
    set( opt_flags "${buildWith_OPTIMIZATION}" )
endif( )

if( NOT ("${opt_flags}" STREQUAL "") )
    string( CONCAT herc_TestSource    # sample program to validate options
                "/* Test for valid builder-provided optimization c flags */\n"
                "int main()\n"
                "   {return 0;}\n"
                )
    file( WRITE ${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/HercTempProg.c "${herc_TestSource}" )

    try_compile( tc_return_var ${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}
                SOURCES ${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/HercTempProg.c
                COMPILE_DEFINITIONS ${opt_flags} ${all_warnings_are_errors}
                OUTPUT_VARIABLE tc_output_var
            )

    if( NOT tc_return_var )      # Non-zero return code means compiler rejected flags
        set( opt_err_msg "OPTIMIZATION flags \"${opt_flags}\" rejected by c compiler\n\n${tc_output_var}" )
        herc_Save_Error( "${opt_err_msg}" )
        unset( opt_err_msg )
    endif( )

    file( REMOVE ${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/HercTempProg.c )

endif( )
unset( opt_flags)


# PCRE is valid only for Windows systems, and cannot be SYSTEM because
# Windows does not have a system-installed PCRE library.

# Herc28 will import any target if PCRE_DIR has been set correctly.
# Herc41_ExtPackageBuild.cmake will decide whether we build PCRE or
# use the import target based on the setting of PCRE_DIR.

if( NOT ("${PCRE}" STREQUAL "" OR "${PCRE}" STREQUAL "NO" OR WIN32 ) )
    message( STATUS "PCRE=${PCRE} ignored for non-Windows targets." )

elseif( NOT "${PCRE}" STREQUAL "NO" )
    #  PCRE is blank, YES, HERCULES, or SYSTEM and this is a Windows
    #  target system.

    if( "${PCRE}" STREQUAL "SYSTEM" )
        herc_Save_Error( "PCRE=SYSTEM cannot be specified for Windows targets." )
    endif( )
    # Herc28_OptSelect.cmake will import the PCRE target if PCRE_DIR
    # points us at a specific build or install directory.
endif( )


# OBJECT-REXX - Include support for Open Object Rexx

herc_Check_User_Option_YesNo( OBJECT-REXX        FAIL )

if( WIN32 )
    # If building on Windows, locate the correct Open Object Rexx directory
    # for the bitness of the Hercules being built and set OOREXX_DIR
    # to that directory.  Herc22_UserlandWin.cmake will check for
    # the needed headers, Herc28_OptSelect will issue any error messages,
    # and the OOREXX_DIR will be included in the build configuration.

    if( ("${OBJECT-REXX}" STREQUAL "YES")
            OR (buildWith_OBJECT-REXX
                    AND (NOT ("${OBJECT-REXX}" STREQUAL "NO")))  )
        # Open Object Rexx takes pains to ensure that only one bitness
        # of ooRexx is installed at a time.  And because the prepackaged
        # install executable is 32-bit, the registry keys written by
        # installation always end up in HKLM\SOFTWARE\WOW6432Node.  So
        # we can only determine if an ooRexx is installed, and if
        # installed, its bitness.

        # If the bitness does not match, then we must ignore the ooRexx
        # installed package.  It will not work.

        get_filename_component( __OOREXX_PATH
            [HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\ooRexx;InstallLocation]
            ABSOLUTE
            BASE_DIR ""
            )

        # Apparently, find_path requires native path on Windows.
        file(TO_NATIVE_PATH "${__OOREXX_PATH}" __OOREXX_PATH)
        find_path( OOREXX_DIR
                    NAMES rexx.dll
                    PATHS ${__OOREXX_PATH}
                    NO_DEFAULT_PATH
                    )

        message( "Open Object Rexx package directory is \"${OOREXX_DIR}\"" )

        if( OOREXX_DIR )
            # We need to read a registry key UninstallBitness.  Treat
            # the key as a file name and use get_filename_component()
            # to get the key value.
            get_filename_component( __OOREXX_BITNESS
                [HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\ooRexx;UninstallBitness]
            ABSOLUTE
            BASE_DIR ""
            )
            if( ( ${CMAKE_SIZEOF_VOID_P} EQUAL 4 AND "${__OOREXX_BITNESS}" STREQUAL "x86_64" ) )
                unset( OOREXX_DIR )
                if( "${OBJECT-REXX}" STREQUAL "YES")
                    herc_Save_Error("Hercules 32-bit can only use 32-bit ooRexx, found ooRexx for ${__OOREXX_BITNESS}" )
                endif( )
            elseif( ( ${CMAKE_SIZEOF_VOID_P} EQUAL 8 AND "${__OOREXX_BITNESS}" STREQUAL "x86_32" ) )
                unset( OOREXX_DIR )
                if( "${OBJECT-REXX}" STREQUAL "YES")
                    herc_Save_Error("Hercules 64-bit can only use 64-bit ooRexx, found ooRexx for ${__OOREXX_BITNESS}" )
                endif( )
            endif( )
        elseif( "${OBJECT-REXX}" STREQUAL "YES")
            herc_Save_Error( "Unable to find Open Object Rexx package in system location \"${__OOREXX_PATH}\"" )

        endif( )        # OOREXX_DIR FOUND
    endif( )        # OBJECT_REXX=YES specified or defaulted.
endif( )        # Windows build


# REGINA-REXX - Include support for Regina Rexx

herc_Check_User_Option_YesNo( REGINA-REXX        FAIL )

if( WIN32 )
    # If building on Windows, locate the correct Regina Rexx directory
    # for the bitness of the Hercules being built and set RREXX_DIR
    # to that directory.  Herc22_UserlandWin.cmake will check for
    # the needed headers, Herc28_OptSelect will issue any error messages,
    # and the RREXX_DIR will be included in the build configuration.

    if( ("${REGINA-REXX}" STREQUAL "YES")
            OR (buildWith_REGINA-REXX
                    AND (NOT ("${REGINA-REXX}" STREQUAL "NO")))  )
        # The environment variable REGINA_HOME is set by the Regina Rexx binary
        # installation to the directory in which Regina Rexx was installed.
        # But on 64-bit systems, there are complications:
        # 1. Both the 32-bit and the 64-bit versions may be installed.  In this
        #    case, the installer is expected to change REGINA_HOME (and the
        #    system PATH) to point to the bitness to be used.
        # 2. The bitness of Regina Rexx must match the bitness of Hercules.
        # So we cannot trust REGINA_HOME in the hands of the casual Windows
        # builder.

        # This boils down to if we are building a 32-bit version of Hercules
        # on a 64-bit system, we must look in a different place for a Regina
        # Rexx of the correct bitness.  If we have a Program Files (x86)
        # directory, then we are building on a 64-bit host.

        # And when running a 32-bit Hercules with Regina Rexx from the build
        # directory, we will need to see if Windows automatically loads the
        # 32-bit Rexx DLL from Program Files (x86).

        if( "${CMAKE_SIZEOF_VOID_P}" EQUAL 4 AND (NOT "$ENV{ProgramFiles\(x86\)}" STREQUAL "" ))
            get_filename_component( __RREXX_PATH
                [HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\ReginaRexx;InstallLocation]
                ABSOLUTE
                BASE_DIR ""
                )
        else( )
            get_filename_component( __RREXX_PATH
                [HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\ReginaRexx;InstallLocation]
                ABSOLUTE
                BASE_DIR ""
                )
        endif( )

        # Apparently, find_path requires native path on Windows.
        file(TO_NATIVE_PATH "${__RREXX_PATH}" __RREXX_PATH)
        find_path( RREXX_DIR
                    NAMES regina.dll
                    PATHS ${__RREXX_PATH}
                    NO_DEFAULT_PATH
                    )

        message( "Regina Rexx package directory is \"${RREXX_DIR}\"" )
        if( ("${REGINA-REXX}" STREQUAL "YES") AND NOT RREXX_DIR )
            herc_Save_Error( "Unable to find Regina Rexx package in system location \"${__RREXX_PATH}\"" )
        endif( )

        endif( )      # REGINA-REXX = "YES" specified or defaulted
endif( )      # Windows build


# SETUID-HERCIFC may be YES, NO, or a group name.  We must take on faith
# that the target system will have the group name specified.  From a
# syntax perspective, anything goes here.  We call the Yes/No function
# to canonicalize Y/y/YES/Yes/yes into YES/NO etc for later testing.
# Note that the Yes/No function *does not* change the value if it not a
# synonym for Yes, No, or Target.

herc_Check_User_Option_YesNo( SETUID-HERCIFC OK )
if( NOT ( ("${SETUID-HERCIFC}" STREQUAL "YES")
        OR ("${SETUID-HERCIFC}" STREQUAL "NO")
        OR ("${SETUID-HERCIFC}" STREQUAL "") ) )
    STRING( CONCAT err_msg
            "This build script does not yet support changing the group ownership of hercifc.\n"
            "              \"YES\" and \"NO\" are the only supported values for SETUID-HERCIFC"
            )
    herc_Save_Error( "${err_msg}" )
    unset( err_msg )
endif( )


# WINTARGET -  Windows version to be targeted for this build.

# When building for Windows, ensure that the correct Windows API version
# is used.  The default is the API version of the host system.  While we
# are here, ensure the debug library gets a different name and a exports
# file is created for later creation of the import library.

if( NOT "${WINTARGET}" STREQUAL "" )
    if( WIN32 )
        include( CMake/herc_setWindowsTarget.cmake )
        herc_setWindowsTarget( "${WINTARGET}" Windows_version )
        if( "${Windows_version}" MATCHES "-NOTFOUND" )
            herc_Save_Error( "WINTARGET= value \"${WINTARGET}\" is not valid ${Windows_version} ${Windows_versions}" )
        endif( )
    else( )
        herc_Save_Error( "WINTARGET= can only be specified when Windows is the target system" )
    endif( )   # if( WIN32 )
endif( )


# Zlib cannot be SYSTEM if the build target is not Windows.  If Zlib
# is SYSTEM, then Zlib must be installed on the target system and
# can be found by find_package( Zlib MODULE ).

# Herc28 will import any target if BZIP2_DIR has been set correctly.
# Herc41_ExtPackageBuild.cmake will decide whether we build Zlib or
# build an import target from information returned by find_package( )
# based on the setting of BZIP2 and comparng the versions of the
# target system's Zlib versus the Zlib repository in the Hercules-390
# project.

if( NOT "${ZLIB}" STREQUAL "NO" )
    #  ZLIB is blank, YES, HERCULES, or SYSTEM.  If this is a
    #  non-Windows target, see if
    #  Zlib headers and library are installed in the system directories.

    if( NOT ( WIN32 OR "${ZLIB}" STREQUAL "HERCULES" ) )
        # Use of the Zlib library installed on the system is a
        # possibility.  See what version if any is installed.
        find_package(ZLIB MODULE )
    endif( )

    if( "${ZLIB}" STREQUAL "SYSTEM" )
        if( WIN32 )   # SYSTEM not valid for Windows targets
            herc_Save_Error( "ZLIB=SYSTEM can only be specified for non-Windows targets." )
        elseif( NOT ZLIB_FOUND )   # No Zlib lib/headers installed
            herc_Save_Error( "ZLIB=SYSTEM but Zlib headers and/or library are not installed." )
        endif( )
        # Herc41_ExtPackageBuild.cmake will create the import target
        # using the information returned by find_package(ZLIB MODULE ).
    endif( )
    # Herc28_OptSelect.cmake will decide whether to use the system-
    # installed Zlib based on its existence and version, and whether
    # ZLIB_DIR points us at a specific build or install directory.
endif( )




#-----------------------------------------------------------------------------
#
# Edit directory options.  All directory options are edited in this
# section because the logic for each is very similar.
#
#-----------------------------------------------------------------------------

# Path name editing for HQA_DIR is relatively simple: if a path is
# specified, the path must exist.  A null path means build scenario
# selection via a macro contained in hqa.h will not be supported.  Note
# that it is not a requirement to have hqa.h present in the specified
# HQA_DIR.

# HQA_DIR may be specified on the CMake command line (-DHQA_DIR="xxx")
# or via a system environment variable.

if( NOT ("${HQA_DIR}" STREQUAL "") )
    set( HQA_INSTALL_DIR "${HQA_DIR}" )
elseif( NOT ("$ENV{HQA_DIR}" STREQUAL "") )
    set( HQA_INSTALL_DIR "$ENV{HQA_DIR}" )
endif( )

if( NOT ("${HQA_INSTALL_DIR}" STREQUAL "") )
    get_filename_component( HQA_INSTALL_DIR "${HQA_INSTALL_DIR}"
                ABSOLUTE
                BASE_DIR "${CMAKE_BINARY_DIR}"
                CACHE )
    if( NOT (IS_DIRECTORY "${HQA_INSTALL_DIR}" ) )
        herc_Save_Error( "Unable to locate HQA_DIR directory \"${HQA_INSTALL_DIR}\" " )
    endif( )
endif( )


# Convert BZIP2_DIR to an absolute directory name and make sure it
# contains a target import script in the bzip2-targets subdirectory.
# The target import script deals with the specifics of the package
# directory structure.  Herc28_OptSelect.cmake will import the bzip2
# target.  If the builder did not specify BZIP2_DIR, then
# Herc41_ExtPackageBuld.cmake will decide what should be done (use a
# BZip2 package on the target or build one from the Hercules project).

# If the builder specified both BZIP2=SYSTEM and BZIP2_DIR=<something>,
# then BZIP2_DIR will be edited for validity.  If valid, BZIP2=SYSTEM
# takes precedence.  A warning message will be issued and BZIP2_DIR
# will be ignored.

if( NOT ("${BZIP2_DIR}" STREQUAL "") )
    if( "${BZIP2}" STREQUAL "NO" )
        herc_Save_Error( "A BZIP2_DIR cannot be specified with BZIP2=NO" )
    endif( )
    get_filename_component( BZIP2_INSTALL_DIR "${BZIP2_DIR}"
                ABSOLUTE
                BASE_DIR "${CMAKE_BINARY_DIR}"
                CACHE )
    if( NOT (IS_DIRECTORY "${BZIP2_INSTALL_DIR}") )
        herc_Save_Error( "Unable to locate specified BZIP2_DIR directory \"${BZIP2_INSTALL_DIR}\" (= \"${BZIP2_INSTALL_DIR}\") " )
    else( )
        find_path( HAVE_BZIP2_TARGET   NAMES bzip2.cmake PATHS "${BZIP2_INSTALL_DIR}/bzip2-targets" NO_DEFAULT_PATH )
        if( HAVE_BZIP2_TARGET )
            if( "${BZIP2}" STREQUAL "SYSTEM" )
                message( WARNING "BZIP2=SYSTEM and BZIP2_DIR specified.  BZIP2_DIR ignored.  System BZip2 used." )
            endif( )
        else( )
            herc_Save_Error( "BZIP2_DIR=${BZIP2_DIR} does not have target import script bzip2-targets/bzip2.cmake" )
        endif( )
    endif( )
endif( )


# Convert PCRE_DIR to an absolute directory name and make sure it
# contains a target import script in the pcre-targets subdirectory.
# The target import script deals with the specifics of the package
# directory structure.  Herc28_OptSelect.cmake will import the pcre
# target.  If the builder did not specify PCRE_DIR, then
# Herc41_ExtPackageBuld.cmake will decide what should be done (use a
# BZip2 package on the target or build one from the Hercules project).

# If the builder specified both PCRE=SYSTEM and PCRE_DIR=<something>,
# then PCRE_DIR will be edited for validity.  If valid, PCRE=SYSTEM
# takes precedence.  A warning message will be issued and PCRE_DIR
# will be ignored.

if( NOT ("${PCRE_DIR}" STREQUAL "") )
    if( "${PCRE}" STREQUAL "NO" )
        herc_Save_Error( "A PCRE_DIR cannot be specified with PCRE=NO" )
    endif( )
    get_filename_component( PCRE_INSTALL_DIR "${PCRE_DIR}"
                ABSOLUTE
                BASE_DIR "${CMAKE_BINARY_DIR}"
                CACHE )
    if( NOT (IS_DIRECTORY "${PCRE_INSTALL_DIR}") )
        herc_Save_Error( "Unable to locate specified PCRE_DIR directory \"${PCRE_INSTALL_DIR}\" (= \"${PCRE_INSTALL_DIR}\") " )
    else( )
        find_path( HAVE_PCRE_TARGET   NAMES pcre.cmake PATHS "${PCRE_INSTALL_DIR}/pcre-targets" NO_DEFAULT_PATH )
        if( NOT HAVE_PCRE_TARGET )
            herc_Save_Error( "PCRE_DIR=${PCRE_DIR} does not have target import script pcre-targets/pcre.cmake" )
        endif( )
    endif( )
endif( )


# Convert S3FH_DIR to an absolute directory name and make sure it
# contains a target import script in the s3fh-targets subdirectory.
# The target import script deals with the specifics of the package
# directory structure.  Herc28_OptSelect.cmake will import the SoftFloat
# target.  If the builder did not specify S3FH_DIR, SoftFloat-3a will
# be built by Herc41_ExtPackageBuild.cmake.

if( NOT ("${S3FH_DIR}" STREQUAL "") )
    get_filename_component( S3FH_INSTALL_DIR "${S3FH_DIR}"
                ABSOLUTE
                BASE_DIR "${CMAKE_BINARY_DIR}"
                CACHE )
    if( NOT (IS_DIRECTORY "${S3FH_INSTALL_DIR}") )
        herc_Save_Error( "Unable to locate specified S3FH_DIR directory \"${S3FH_INSTALL_DIR}\" (= \"${S3FH_INSTALL_DIR}\") " )
    else( )
        find_path( HAVE_S3FH_TARGET   NAMES s3fh.cmake PATHS "${S3FH_INSTALL_DIR}/s3fh-targets" NO_DEFAULT_PATH )
        if( NOT HAVE_S3FH_TARGET )
            herc_Save_Error( "S3FH_DIR=${S3FH_DIR} does not have target import script s3fh-targets/s3fh.cmake" )
        endif( )
    endif( )
endif( )


# Convert ZLIB_DIR to an absolute directory name and make sure it
# contains a target import script in the zlib-targets subdirectory.
# The target import script deals with the specifics of the package
# directory structure.  Herc28_OptSelect.cmake will import the Zlib
# target.  If the builder did not specify ZLIB_DIR, then
# Herc41_ExtPackageBuld.cmake will decide what should be done (use a
# Zlib package on the target or build one from the Hercules project).

# If the builder specified both ZLIB=SYSTEM and ZLIB_DIR=<something>,
# then ZLIB_DIR will be edited for validity.  If valid, ZLIB=SYSTEM
# takes precedence.  A warning message will be issued and ZLIB_DIR
# will be ignored.

if( NOT ("${ZLIB_DIR}" STREQUAL "") )
    if( "${ZLIB}" STREQUAL "NO" )
        herc_Save_Error( "A ZLIB_DIR cannot be specified with ZLIB=NO" )
    endif( )
    get_filename_component( ZLIB_INSTALL_DIR "${ZLIB_DIR}"
                ABSOLUTE
                BASE_DIR "${CMAKE_BINARY_DIR}"
                CACHE )
    if( NOT (IS_DIRECTORY "${ZLIB_INSTALL_DIR}") )
        herc_Save_Error( "Unable to locate specified ZLIB_DIR directory \"${ZLIB_INSTALL_DIR}\" (= \"${ZLIB_INSTALL_DIR}\") " )
    else( )
        find_path( HAVE_ZLIB_TARGET   NAMES zlib.cmake PATHS "${ZLIB_INSTALL_DIR}/zlib-targets" NO_DEFAULT_PATH )
        if( HAVE_ZLIB_TARGET )
            if( "${ZLIB}" STREQUAL "SYSTEM" )
                message( WARNING "ZLIB=SYSTEM and ZLIB_DIR specified.  ZLIB_DIR ignored.  System Zlib used." )
            endif( )
        else( )
            herc_Save_Error( "ZLIB_DIR=${ZLIB_DIR} does not have target import script zlib-targets/zlib.cmake" )
        endif( )
    endif( )
endif( )




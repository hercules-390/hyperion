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
    return( )
endif( )


herc_Check_User_Option_YesNo( AUTOMATIC-OPERATOR FAIL )
herc_Check_User_Option_YesNo( DEBUG              FAIL )
herc_Check_User_Option_YesNo( CAPABILITIES       FAIL )
herc_Check_User_Option_YesNo( CCKD-BZIP2         FAIL )
herc_Check_User_Option_YesNo( EXTERNAL-GUI       FAIL )
herc_Check_User_Option_YesNo( FTHREADS           FAIL )
herc_Check_User_Option_YesNo( GETOPTWRAPPER      FAIL )
herc_Check_User_Option_YesNo( HET-BZIP2          FAIL )
herc_Check_User_Option_YesNo( INTERLOCKED-ACCESS-FACILITY-2 FAIL )
herc_Check_User_Option_YesNo( IPV6               FAIL )
herc_Check_User_Option_YesNo( LARGEFILE          FAIL )
herc_Check_User_Option_YesNo( OBJECT-REXX        FAIL )
herc_Check_User_Option_YesNo( REGINA-REXX        FAIL )
herc_Check_User_Option_YesNo( SYNCIO             FAIL )


# There are no edits for option ADD-CFLAGS.  It is there or not.

# There are no edits for option CUSTOM.  It is there or not.


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
        herc_Save_Error( "Invalid value \"${MULTI-CPU}\" for MULTI-CPU, must be YES, NO, or a number between 1 and 128" "")
    elseif( (${MULTI-CPU} GREATER 128) OR (${MULTI-CPU} LESS 1) )
        herc_Save_Error( "Out of range value \"${MULTI-CPU}\" for MULTI-CPU, must be YES, NO, or a number between 1 and 128" "")
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
    herc_Save_Error( "Unrecognized c compiler \"${CMAKE_C_COMPILER_ID}\" - automatic optimization not possible." "" )
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
                "int main()"
                "{
                    return 0;"
                "}"
                )
    file( WRITE ${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/HercTempProg.c "${herc_TestSource}" )

    try_compile( tc_return_var ${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}
                SOURCES ${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/HercTempProg.c
                COMPILE_DEFINITIONS "${opt_flags} -Wall -Werror"
                OUTPUT_VARIABLE tc_output_var
            )

    if( NOT tc_return_var )      # Non-zero return code means compiler rejected flags
        set( opt_err_msg "C compiler rejected optimization flags \"${opt_flags}\" \n\n${tc_output_var}" )
        herc_Save_Error( "${opt_err_msg}" "" )
    endif( )

    file( REMOVE ${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/HercTempProg.c )

endif( )
unset( opt_flags)


# SETUID-HERCIFC may be YES, NO, or a group name.  We must take on faith
# that the target system will have the group name specified.  From a
# syntax perspective, anything goes here.  We call the Yes/No function
# to canonicalize Y/y/YES/Yes/yes into YES/NO etc for later testing.
# Note that the Yes/No function *does not* change the value if it not a
# synonym for Yes, No, or Target.

herc_Check_User_Option_YesNo( SETUID-HERCIFC OK )
message( "${SETUID-HERCIFC}" )
if( NOT ( ("${SETUID-HERCIFC}" STREQUAL "YES")
        OR ("${SETUID-HERCIFC}" STREQUAL "NO")
        OR ("${SETUID-HERCIFC}" STREQUAL "") ) )
    STRING( CONCAT err_msg
            "This build script does not yet support changing the group ownership of hercifc.\n"
            "              \"YES\" and \"NO\" are the only supported values for SETUID-HERCIFC"
            )
    herc_Save_Error( "${err_msg}" "" )
    unset( err_msg )
endif( )


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
#        herc_Save_Message( "Unable to locate HQA_DIR directory \"${HQA_INSTALL_DIR}\" " "")
    endif( )
endif( )


# Convert S3FH_DIR to an absolute path, make sure it exists.  We do not
# care about symlinks because we are not building into it.  Proper
# directory structure of the default or specified directory is done in
# Herc28_OptSelect.cmake, and Herc22_Userland.cmake will
# ensure the headers and library are present.

if( NOT ("${S3FH_DIR}" STREQUAL "") )
    get_filename_component( S3FH_INSTALL_DIR "${S3FH_DIR}"
                ABSOLUTE
                BASE_DIR "${CMAKE_BINARY_DIR}"
                CACHE )
    message( "S3FH_INSTALL_DIR set to \"${S3FH_INSTALL_DIR}\"" )
    if( NOT (IS_DIRECTORY "${S3FH_INSTALL_DIR}") )
        herc_Save_Message( "Unable to locate specified S3FH_DIR directory \"${S3FH_INSTALL_DIR}\" (= \"${S3FH_INSTALL_DIR}\") " "")
    endif( )
endif( )



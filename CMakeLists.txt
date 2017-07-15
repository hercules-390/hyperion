# CMakeLists.txt - top-level driver for CMake Hercules Build

#[[   Copyright 2017 by Stephen Orso.

      Distributed under the Boost Software License, Version 1.0.
      See accompanying file BOOST_LICENSE_1_0.txt or copy at
      http://www.boost.org/LICENSE_1_0.txt)
]]

# CMakeLists.txt used to build Hercules and its separate component library
# dyncrypto.  Also used to build the separately-licensed decNumber library.

#[[-------------------------------------------------------------------------

            Shortcomings/shortcuts/work remaining:

- This has been tested on Debian 8.6 (Jessie) using the Unix Makefile
  generator, with GNU Make doing the build under the covers.  The command
  `cmake --build .` invokes make.
- This has been tested on FreeBSD 11.0 using the Unix Makefile generator
  and using the Ninja generator.



           To Do List
- Create an 'uninstall' target
- Create a 'clean' target.
- Write a prolog(ue) to match that written for CMakeHercGitVer.cmake
- Write a brief task-oriented update tutorial, to deal with at least
  adding a new compilation unit to an existing executable or shared
  library, a new executable, a new shared library.  For extra credit,
  document the addition of a test for a new required function in an existing
  library and one in a new library.


--------------------------------------------------------------------------- ]]

cmake_minimum_required( VERSION 3.4 )  # 3.4 is required for the Windows export library support.
cmake_policy(SET CMP0048 NEW)          # Set version variables from project directive


# Prevent an in-source build.  REALPATH resolves any symlinks.  There is
# no way to programatically remove CMakeCache.txt nor CMakeFiles/*
# from within a CMakeLists.txt, so we must just instruct the builder.
# And if the builder fails to clean up the source directory, the stale
# CMakeCache.txt file in the source directory will confound the build
# process until it is remove.  So we will check for it and complain if
# one is found.

get_filename_component( SOURCE_DIR "${CMAKE_SOURCE_DIR}" REALPATH)
get_filename_component( BINARY_DIR "${CMAKE_BINARY_DIR}" REALPATH)

if( (EXISTS ${SOURCE_DIR}/CMakeCache.txt )
        OR (EXISTS ${SOURCE_DIR}/CMakeFiles) )
    if( "{SOURCE_DIR}" STREQUAL ${CMAKE_SOURCE_DIR} )
        set( message_source_dir "${CMAKE_SOURCE_DIR}" )
    elseif( )
        set( message_source_dir "${CMAKE_SOURCE_DIR} = ${SOURCE_DIR}" )
    endif( )
    string( CONCAT error_message
"-------------------------------------------------------------\n"
"Existing file CMakeCache.txt and directory CMakeFiles/ *MUST*\n"
"be removed from the source directory:\n\n"
"    ${CMAKE_SOURCE_DIR}\n\n"
"Hercules cannot be built until CMakeCache.txt and CMakeFiles/.\n"
"are removed from ${CMAKE_SOURCE_DIR}.\n"
"-------------------------------------------------------------\n"
    )
    message( ${error_message} )
    message( FATAL_ERROR "Hercules will not be built" )
endif( )

if( "${SOURCE_DIR}" STREQUAL "${BINARY_DIR}" )
    string( CONCAT error_message
"-------------------------------------------------------------\n"
"In-source builds are not allowed for Hercules\n\n"
"   ${CMAKE_SOURCE_DIR} = ${SOURCE_DIR}\n"
"   ${CMAKE_BINARY_DIR} = ${BINARY_DIR}\n\n"
"Before continuing, remove the file CMakeCache.txt and the\n"
"directory CMakeFiles from the source directory.\n"
"-------------------------------------------------------------"
    )
    message( ${error_message} )
    message( FATAL_ERROR "Hercules will not be built" )
endif( )
execute_process( COMMAND ${CMAKE_COMMAND} -E remove ${PROJECT_SOURCE_DIRECTORY}/CMakeCache.txt )
set( CMAKE_DISABLE_IN_SOURCE_BUILD ON )
set( CMAKE_DISABLE_SOURCE_CHANGES  ON )


project( Hercules VERSION 4.0.0.0 )
string( TIMESTAMP BUILD_TIMESTAMP "Date: %Y-%m-%d Time: %H:%M:%S UTC" UTC )


# Define variables for the installation targets, one for libraries, one for
# executables, one for the html content.  The relative paths are used in
# install() directives, and the full paths are passed to the compiler in
# -D options, to be changed to config.h when this CMake process creates a
# config.h

set( exec_rel_dir      "bin" )
set( library_rel_dir   "bin" )              # shared libraries in the bin directory for now
set( http_rel_dir      "share/hercules" )   # target for Hercules' console web server
set( exec_dir          "${CMAKE_INSTALL_PREFIX}/${exec_rel_dir}" )
set( library_dir       "${CMAKE_INSTALL_PREFIX}/${library_rel_dir}" )
set( http_dir          "${CMAKE_INSTALL_PREFIX}/${http_rel_dir}" )


# Define list of supported compilers and Intel processers for IN_LIST
# tests.  See https://cmake.org/cmake/help/v3.4/variable/CMAKE_LANG_COMPILER_ID.html
# for a complete list of possible values for CMAKE_C_COMPILER_ID.

set( herc_CompilerClang Clang )
set( herc_CompilerGNU   GNU   )
set( herc_Compilers ${herc_CompilerClang} ${herc_CompilerGNU} )

set( herc_Intel_32  "I386;I486;I586;I686;I786" )
set( herc_Intel_64  "X86_64;AMD64" )
set( herc_Intel     "${herc_Intel_32};${herc_Intel_64}" )
set( herc_YES_NO  "YES;NO" )

# Ensure /usr/local/include is part of the compiler preprocessor -I
# includes.  We may wish to do the same for /usr/local/lib.  And
# because the local directory is used to build Hercules, it should
# also be used for all CMake tests for public headers.

include_directories( BEFORE SYSTEM "/usr/local/include" )
set( CMAKE_REQUIRED_INCLUDES "/usr/local/include" )


# Reference strings for library names.  libm is required and the build
# will be failed if it is missing.  libz and libbz2 are option and included
# in the build if installed on the target system.  The remaining libraries
# are included in the build if required for the target system (librt,
# libnsl) or by build options (libcap).

set( name_libz         "z"   )    # zlib commpression library
set( name_libbz2       "bz2" )    # bzip2 compression library
set( name_libm         "m"   )    # IEEE math library, needed for STSI instruction
set( name_librt        "rt"  )    # POSIX real time extensions library, needed for Solaris
set( name_libnsl       "nsl" )    # Nameserver lookup library, needed for Solaris
set( name_libcap       "cap" )    # POSIX 1003.1e capabilities interface library
set( name_libsocket    "socket" ) # Socket library for Solaris, HP-UX, maybe others


# Everything (almost) in Hercules must be compiled with position-independent
# code, so we shall just make it global.  On systems that support it of course.

set( CMAKE_POSITION_INDEPENDENT_CODE TRUE )


# Set the shared library suffix appropriate to the target_link_libraries
# Darwin / OS/X will come later.  This should be in CMakeTargetOS.cmake

if( WIN32 )
    set( shared_lib_suffix ".dll" )
else( )
    set( shared_lib_suffix ".so" )
endif( )


#  ALL CMake functions needed for this build should be included here.
#  CMake functions create the enviroment used by this build.

include( TestBigEndian )            # Enable endian test
include( CheckTypeSize )            # Enable tests for types and their sizes
include( CheckIncludeFiles )        # Enable tests for include headers
include( CheckLibraryExists )       # Enable tests for specific libraries
include( CheckStructHasMember )     # Enable tests for membership in a structure
include( CheckSymbolExists )        # Enable existence tests for symbols & function prototypes
include( CheckFunctionExists )      # Enable tests for function existence
include( CheckPrototypeDefinition ) # Enable tests of function prototypes
include( CheckSymbolExists )        # Enable tests for specific symbols

include( GenerateExportHeader )     # Required for implicitly loaded Windows DLLs

include( ExternalProject )          # enable build of SoftFloat-3a, other
                                    # external projects

include( CTest )                    # Include test directives
enable_testing( )                   # Enable them

include( CPack )                    # Enable creation of installable binary and source packages
include( InstallRequiredSystemLibraries )  # CPack: required to build CPack installers

include( CMake/Herc00_Includes.cmake ) # Get Hercules-specific CMake functions and macros


set( herc_Error 0 )                 # No errors detected yet


# Define all user options and edit them.  If user option HELP is defined,
# CMakeHercOptEdit.cmake will display help and return without doing any
# other processing.  And if help is displayed, then nothing else is done.

include( CMake/Herc10_OptDef.cmake )          # define user options and build defaults

include( CMake/Herc12_OptEdit.cmake )         # edit all provided user options
if( HELP )
    return( )
endif( )


# probe the target hardware and operating system, the Userland, and the
# characteristics of the c compiler.

include( CMake/Herc20_TargetEnv.cmake )       # collect info about the target system
include( CMake/Herc22_Userland.cmake )        # check for userland requirements.
include( CMake/Herc25_Compilergcclike.cmake ) # check compiler capabilities


# Interpret builder-specified options in the context of the target
# environment and set build options accordingly, particularly where the
# builder allows one or more options to default.

include( CMake/Herc28_OptSelect.cmake )       # Make selected options effective


# Four different routines are used to set options for the c compiler:
# one each for clang, gcc, Apple gcc, and unknown.  Expect three
# additional routines: Windows, Intel C, and AIX.

# While early versions of Apple mac OS used gcc for some or all of the
# compilation, the Apple gcc compiler is modified from the base
# equivalent version.  So setting Apple gcc options is separate from but
# very similar to that for gcc.

# Apple may also have modified clang for use with mac OS, but this is
# not certain.  For the moment, the same script is used to set clang
# options on Apple and non-Apple platforms.

# Versions newer than gcc 4.2.1 were never used by Apple because, in my
# opinion, those newer versions are licensed GPL3.  So this script
# assumes that if a gcc version newer than 4.2.1 is in use, it is a
# user-built gcc that was not modified by Apple, and gcc compiler options
# can be handled in the same way it is for GNU/Linux or BSD systems.
# Apple gcc 4.2.1 or older is presumed to be the Apple-specific compiler
# included in Xcode or CodeWarrior.

# When the c compiler is not recognized, there is little we can do, but
# we can let the builder specify an optimization string.  If a builder
# expects automatic optimization with an unrecognized compiler, we will
# abort the build.

if( "${CMAKE_C_COMPILER_ID}" STREQUAL "${herc_CompilerClang}" )
    include( CMake/Herc31_COptsClang.cmake )

elseif( "${CMAKE_C_COMPILER_ID}" STREQUAL "${herc_CompilerGNU}" )
    if( (APPLE)
            AND ( CMAKE_C_COMPILER_VERSION VERSION_LESS "4.2.2" ) )
        include( CMake/Herc31_COptsAppleGNU.cmake )
    else( )
        include( CMake/Herc31_COptsGNU.cmake )
    endif( )

else( )
    include( CMake/Herc31_COptsunknown.cmake )

endif( )

if( NOT ("${ADD-CFLAGS}" STREQUAL "") )
    set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${ADD-CFLAGS}" )
endif( )


# Anything that can abort the build has already been detected.  If there
# are any error messages, send them to stderr and abort the build.

if( herc_Error )
    message( "Generation and build terminated due to ${herc_Error} errors:" )
    foreach( index RANGE 1 ${herc_Error} )
        message( "Error ${index}/${herc_Error}: ${herc_EMessage${index}}" )
    endforeach( index )
    message( FATAL_ERROR "Generation and build terminated..." )
    return( )
endif( herc_Error )


# Define targets for the external projects used by Hercules.  At the
# moment there is only one, SoftFloat-3a For Hercules.

include( CMake/Herc41_ExtPackageBuild.cmake )


# Assemble the link library string to be used for compilation units.  This
# is done in one place rather than just adding libraries as the need is
# identified so that link library ordering can be addressed explicitly
# in one place rather than as a side effect of library test order.

set( link_alllibs
            ${link_libm}
            ${link_libz}
            ${link_libbz2}
            ${link_librt}
            ${link_libnsl}
            ${link_libcap}
            ${link_libsocket}
            ${CMAKE_DL_LIBS}
      )


# create config.h

configure_file( CMake/CMakeconfig.h.in config.h @ONLY )


# Set the RPATH to be used when installing targets
# Use (don't skip) the full RPATH for the build tree, and do not use the
#    install directory RPATH when building.  This enables execution from
#    the build directory prior to install.  CMake will update the RPATH
#    when installing.  A side-effect of this approach is that older CMake
#    versions will re-link everything on an install.
# Snide Note: Autotools build always re-links on an install, so the CMake
#    approach is no worse and sometimes better.

set( CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/bin" )
set( CMAKE_SKIP_BUILD_RPATH  FALSE )
set( CMAKE_BUILD_WITH_INSTALL_RPATH FALSE )


# Include directories for compiles.  The binary directory includes
# config.h, created (eventually, but not now) herein.  The decNumber and
# s3fh directories precede the source directory so that we get the
# authoritative public headers, not just something that lets an IDE do
# incremental compilation.

include_directories( ${PROJECT_BINARY_DIR}
                     ${PROJECT_SOURCE_DIR}/decNumber
                     ${S3FH_INSTALL_DIR}/include
                     ${PROJECT_SOURCE_DIR}
        )

if( "${HAVE_HQA_H}" )
    include_directories( BEFORE ${HQA_INSTALL_DIR} )
endif( )


# Link directories.  When the CMake build is completed, all shared libraries
# will be built into the same build directory.  No more shared/dynamic
# libraries in subdirectories.  There are not that many libraries.  When
# fully implemented, some of the hunting for shared libraries that happens
# in hdl_open() in hdl.c can be removed.

link_directories( ${PROJECT_BINARY_DIR}
                  ${PROJECT_BINARY_DIR}/decNumber
                  ${S3FH_INSTALL_DIR}/lib
        )


# If the source directory is a git repository, extract the git status into
# commitinfo.h so the status can be displayed on startup.  If not a git
# repository, then create a dummy commitinfo.h to enable the build.  The
# script CMakeHercGitVer.cmake issues a few user messages, so we will
# do the top border here.

message( "------------------------------------------------------------------------" )
include( CMake/Herc01_GitVer.cmake )     # Set values for git repository variables
                                         # And display a message about it.


# Share some information about where things are going and how they get there.

message( "\n------------------------------------------------------------------------" )
message( "Install prefix          ${CMAKE_INSTALL_PREFIX}" )
message( "Source directory        ${PROJECT_SOURCE_DIR}" )
message( "Build directory         ${PROJECT_BINARY_DIR}" )
message( "SoftFloat-3a directory  ${S3FH_INSTALL_DIR}" )
message( "Executable directory    ${exec_dir}" )
message( "Library directory       ${library_dir}" )
message( "Data Directory (http)   ${http_dir}" )
message( "Generator               ${CMAKE_GENERATOR}" )
message( "Build command line:     ${CMAKE_MAKE_PROGRAM}" )
message( "C compiler version:     ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}" )
message( "C compiler options:     ${CMAKE_C_FLAGS}" )
message( "Hercules link libs:     ${link_alllibs}" )
message( "Target processor:       ${CMAKE_SYSTEM_PROCESSOR}" )
message( "------------------------------------------------------------------------\n\n" )


# Create all targets and tests needed to build Hercules.

include( CMake/Herc60_CreateTargets.cmake )

# That's it.

return( )





#### ##--------------------------------------------------------------#
#### ##  AIX-specific stuff - not presently supported in the build.  #
#### ##  A willing tester would be a great help.                     #
#### ##--------------------------------------------------------------#
#### #
#### #if test "$hc_cv_is_aix" = "yes"; then
#### #
#### #  if test "$target_os" = "$host_os"; then
#### #
#### #    # Not cross-compiling (likely case); use same architecture
#### #    # as that of the actual hardware (CPU) as reported by the
#### #    # prtconf -c  ==> "CPU Type: 32-bit" or "CPU Type: 64-bit"
#### #
#### #    if prtconf -c | grep '64' 1>/dev/null 2>&1
#### #    then
#### #      hc_cv_build_aix64=yes
#### #    else
#### #      hc_cv_build_aix64=no
#### #    fi
#### #  else
#### #    # Cross-compiling (unlikely case); rely on value of OBJECT_MODE
#### #    # variable to tell us for which architecture they want to build...
#### #
#### #    if echo $OBJECT_MODE | grep '64' 1>/dev/null 2>&1
#### #    then
#### #      hc_cv_build_aix64=yes
#### #    else
#### #      hc_cv_build_aix64=no
#### #    fi
#### #  fi
#### #  if test "$hc_cv_build_aix64" = "yes"; then
#### #    if test "$GCC" = "yes"; then
#### #      CC="$CC -maix64"
#### #      CXX="$CXX -maix64"
#### #    else
#### #      # VisualAge presumed...
#### #      CC="$CC -q64"
#### #      CXX="$CXX -q64"
#### #    fi
#### #    test "$AR" = "" && AR="ar"
#### #    test "$NM" = "" && NM="nm"
#### #    AR="$AR -X64"
#### #    NM="$NM -X64 -B"
#### #  fi
#### #  LDFLAGS="$LDFLAGS -L/usr/local/lib"
#### #  CPPFLAGS="$CPPFLAGS -I/usr/local/include -D_LARGE_FILES -D_LFS_LARGEFILE -D_LINUX_SOURCE_COMPAT"
#### #fi

#### ##----------------------------------------------------------------#
#### ##  The following is also AIX-specific accorting to README.AIX,   #
#### ##  but there is no preprocessor code in Hercules to test it at   #
#### ##  the moment.                                                   #
#### ##----------------------------------------------------------------#
#### #
#### #if test "$hc_cv_have_sys_mtio_h" = "yes"; then
#### #
#### #  AC_CHECK_MEMBERS( [struct mtget.mt_gstat],
#### #                    [hc_cv_have_mtget_mt_gstat=yes  ],
#### #                    [hc_cv_have_mtget_mt_gstat=no   ],
#### #                    [#include <sys/mtio.h>] )
#### #fi












#### #
#### ################################################################################
#### ##   DONE! -- Define our OUTPUT values and then exit...
#### ################################################################################
#### #
#### #
#### ##---------------------------------------------------------------#
#### ##  (place only AC_DEFINE here; place AC_DEFINE_UNQUOTED above)  #
#### ##---------------------------------------------------------------#
#### #
#### #test "$hc_cv_is_windows"                  = "yes"  &&  AC_DEFINE(WIN32)
#### #
#### #
#### #
#### ##--------------------------------------------------#
#### ##  CFLAGS  (compiler flags)                        #
#### ##--------------------------------------------------#
#### #
#### #if test "$hc_cv_is_windows"             = "yes"; then
#### #
#### #    if test "$hc_cv_have_pthread_h"         = "yes" &&
#### #       test "x$hc_cv_alt_pthread_location" != "x";  then
#### #
#### #        CFLAGS="$CFLAGS -I${hc_cv_alt_pthread_location}"
#### #    fi
#### #
#### #    CFLAGS="$CFLAGS -Wno-format"
#### #fi
#### #
#### #

# Note: Following test creates macro USE_DLLTOOL, which not referenced
# in Hercules.  It appears that USE_DLLTOOL was a macro needed when
# doing a MinGW / Cygwin build of Hercules.

#### #AM_CONDITIONAL( USE_DLLTOOL,         [ test "$hc_cv_is_windows"       = "yes" ] )








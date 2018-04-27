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
- Write a prolog(ue) to match that written for CMakeHercGitVer.cmake
- Write a brief task-oriented update tutorial, to deal with at least
  adding a new compilation unit to an existing executable or shared
  library, a new executable, a new shared library.  For extra credit,
  document the addition of a test for a new required function in an existing
  library and one in a new library.


--------------------------------------------------------------------------- ]]


# ----------------------------------------------------------------------
# Preamble: basic CMake setup, prevent an in-source build, set
# configuration type(s), set the minimum CMake version required, and
# define the install directory structure appropriate for the target
# system.
# ----------------------------------------------------------------------

set( herc_Error 0 CACHE INTERNAL "Error count" )                 # No errors detected yet

cmake_minimum_required( VERSION 3.4 )  # 3.4 is required for the Windows export library support.
#  cmake_policy(SET CMP0048 NEW)          # Set version variables from project directive
cmake_policy( VERSION 3.4 )            # Use new policies in effect as of 3.4

# Enable shallow git clones if using CMake 3.6 or better.  The GIT_SHALLOW
# option and its value must be passed in two variables; CMake will not
# accept them in a single string.  These two variables are used in
# Herc03_ExtPackageBuild.cmake.

if( NOT ( ${CMAKE_VERSION} VERSION_LESS "3.6.0" ) )
    set( GIT_SHALLOW_OPTION        "GIT_SHALLOW" )
    set( GIT_SHALLOW_OPTION_VALUE  "1" )
endif( )

# Git to be at least 1.8.5.1; 1.8.5 fails.

find_package( Git )          # Set GIT_FOUND, GIT_EXECUTABLE, and GIT_VERSION_STRING
if( "${GIT_VERSION_STRING}" VERSION_LESS "1.8.5.1" )
    herc_Save_Error( "Git client must be at least version 1.8.5.1, found ${GIT_VERSION_STRING}" )
endif( )


# Prevent an in-source build.  REALPATH resolves any symlinks.  There is
# no way to programatically remove CMakeCache.txt nor CMakeFiles/*
# from within a CMakeLists.txt, so we must just instruct the builder.
# And if the builder fails to clean up the source directory, the stale
# CMakeCache.txt file in the source directory will confound the build
# process until it is removed.  So check for it, and if one is found,
# complain and fail the configure.

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
"Hercules cannot be configured until CMakeCache.txt and\n"
"CMakeFiles/. are removed from ${CMAKE_SOURCE_DIR}.\n"
"-------------------------------------------------------------\n"
    )
    message( ${error_message} )
    message( FATAL_ERROR "Hercules will not be configured" )
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
    message( FATAL_ERROR "Hercules will not be configured" )
endif( )
execute_process( COMMAND ${CMAKE_COMMAND} -E remove ${PROJECT_SOURCE_DIRECTORY}/CMakeCache.txt )
set( CMAKE_DISABLE_IN_SOURCE_BUILD ON )
set( CMAKE_DISABLE_SOURCE_CHANGES  ON )


project( Hercules VERSION 4.0.0.0 LANGUAGES C )
string( TIMESTAMP BUILD_TIMESTAMP "Date: %Y-%m-%d Time: %H:%M:%S UTC" UTC )

# Everything (almost) in Hercules must be compiled with position-
# independent code, so we shall just make it global.

set( CMAKE_POSITION_INDEPENDENT_CODE TRUE )

# Determine the configuration type if using a single-configuration
# generator, and remove extraneous configuration types if using a
# multiple-configuration generator.

if( "${CMAKE_CONFIGURATION_TYPES}" STREQUAL "" )
    # Generator in use creates build scripts for exactly one configuration.
    # Determine whether this is a release or debug build.
    if( DEBUG )
        set( CMAKE_BUILD_TYPE "Debug" )
    else( )
        set( CMAKE_BUILD_TYPE "Release" )
    endif( )
    message( STATUS "Single configuration generator; configuring for ${CMAKE_BUILD_TYPE}" )

else( )
    # For generators that support multiple configurations with build-time
    # configuration selection (Visual Studio or mac OS Xcode), CMake
    # defaults to four configurations: Release, Debug, MinSizeRel, and
    # RelWithDebInfo.  For now, only the first two are needed.  Remove the
    # other two.
    list( REMOVE_ITEM CMAKE_CONFIGURATION_TYPES
                MinSizeRel
                RelWithDebInfo
                )
    message ( STATUS "Available configuration Types: ${CMAKE_CONFIGURATION_TYPES}"  )
endif( )


# It is deucedly difficult to determine which bitness of Windows is
# being configured by CMake.  If using the NMake, JOM, or Ninja
# generators, the Windows environment variable PLATFORM is either x86
# or x64.  If using any Visual Studio generator, the bitness may be
# either a suffix to the generator name ("Win64") or specified by the
# CMake --build option -A [Arch], which may be either x86 or x64.

set( WINDOWS_64BIT FALSE )      # Assume 32-bit build for now

# Case 1: Single configuration generator in a 64-bit Visual Studio
#         command prompt.
if( ( ("${CMAKE_GENERATOR}" MATCHES "^NMake Makefiles" )
            OR ("${CMAKE_GENERATOR}" MATCHES "^NMake Makefiles JOM" )
            OR ("${CMAKE_GENERATOR}" MATCHES "^Ninja" ) )
        AND ( $ENV{PLATFORM} MATCHES "[Xx]64" ) )
    set( WINDOWS_64BIT TRUE )

# Case 2: Visual Studio generator with 64-bit architecture specified
#         using -A x64.
elseif( ( "${CMAKE_GENERATOR}" MATCHES "^Visual Studio" )
            AND ( "${CMAKE_GENERATOR_PLATFORM}" MATCHES "[Xx]64" ) )
    set( WINDOWS_64BIT TRUE )

# Case 3: Visual Studio generator with 64-bit architecture specified
#         using Win64 suffix on generator name.
elseif( ( "${CMAKE_GENERATOR}" MATCHES "^Visual Studio" )
            AND ( "${CMAKE_GENERATOR}" MATCHES "Win64$" ) )
    set( WINDOWS_64BIT TRUE )
endif( )

# There is also some CMAKE_INSTALL_PREFIX fixup to be done when using
# the NMake Makefiles generator on Windows.  This generator always
# assumes an install prefix of "C\Program Files (x86)\<projname>", but
# the actual bitness of the build is set by which Visual Studio command
# prompt is used.  The Windows variable PLATFORM identifies the bitness
# of the Visual Studio command prompt; we will remove " (x86)" from
# the install prefix for 64-bit builds.

# And if the builder specifies CMAKE_INSTALL_PREFIX, we will leave it
# alone.

if( CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT
            AND WINDOWS_64BIT )
    string( REPLACE " (x86)" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}" )
    set(CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}" CACHE PATH "CMake install prefix for x64 Windows" FORCE)
endif( )


# Define variables for the installation targets, one for libraries, one for
# executables, one for the html content.  The relative paths are used in
# install() directives, and the full paths are passed to the compiler in
# config.h for documentation purposes.

# Note that for Windows targets, the package name is included in
# CMAKE_INSTALL_PREFIX; the documentation of this variable is incorrect
# through CMake release 3.6.3.  See the gitlab report at
# https://gitlab.kitware.com/cmake/cmake/issues/16211.


# For non-Windows builds, we will append the project name to the share
# directory, which is used for http sources.

# This is the expected installation directory structure, assuming a
# default CMAKE_INSTALLATION_PREFIX:

# Windows 64-bit:
#   c:\Program Files\Hercules         Executables and DLLs
#   c:\Program Files\Hercules\share   http server root

# Windows 32-bit:
#   c:\Program Files (x86)\Hercules         Executables and DLLs
#   c:\Program Files (x86)\Hercules\share   http server root

# UNIX-like systems:
#   /usr/local/bin/                   executables and dynamic libraries
#   /usr/local/lib/                   shared libraries and loadable modules
#   /usr/local/share/Hercules         http server root

if( WIN32 )
    set( exec_rel_dir "." )         # install dir for executables
    set( libs_rel_dir "." )         # install dir for shared libraries (DLL)
    set( mods_rel_dir "." )         # install dir for loadable modules (DLL)
    set( mods_rel_dir_ins "." )     # module search rel dir (inst. dir)
    set( mods_rel_dir_bld "." )     # module search rel dir (build dir)
    set( http_rel_dir "html" )      # install dir for web server files
    set( http_rel_dir_ins "html" )  # web server default root rel to exe.
else(  )
    set( exec_rel_dir "bin" )  # executables destination
    set( libs_rel_dir "lib" )  # install dir for shared libraries (.so or .dylib)
    set( mods_rel_dir "lib" )  # install dir for loadable modules (.so)
    set( mods_rel_dir_ins "../lib" )    # module search rel dir (inst. dir)
    set( mods_rel_dir_bld "." )         # module search rel dir (build dir)
    set( http_rel_dir "share/${PROJECT_NAME}" )   # install dir for web server files
    set( http_rel_dir_ins "../${http_rel_dir}" )  # web server default root rel to exe.
endif( )

if( "${CMAKE_CONFIGURATION_TYPES}" STREQUAL "" )
# Single configuration generator creates the html directory as a
# subdirectory of the build directory, which is also the directory that
# contains the executables.
    set( http_rel_dir_bld "html" )
else( )
# Multi-configuration generator creates the executable directory *AND*
# the html directory as subdirectories of the build build directory.
    set( http_rel_dir_bld "../html" )
endif( )

set( http_dir          "${CMAKE_INSTALL_PREFIX}/${http_rel_dir}" )


# ----------------------------------------------------------------------
# Define sets of useful constant strings.
# ----------------------------------------------------------------------

# Define list of supported compilers and Intel processers for IN_LIST
# tests.  See https://cmake.org/cmake/help/v3.4/variable/CMAKE_LANG_COMPILER_ID.html
# for a complete list of possible values for CMAKE_C_COMPILER_ID.

set( herc_CompilerAppleClang AppleClang )
set( herc_CompilerClang      Clang      )
set( herc_CompilerGNU        GNU        )
set( herc_CompilerMSVC       MSVC       )
set( herc_Compilers ${herc_CompilerAppleClang}
                    ${herc_CompilerClang}
                    ${herc_CompilerGNU}
                    ${herc_CompilerMSVC}
    )


# Lists of supported Intel processor types, used for optimization and flag
# setting when compiling for an Intel target.

set( herc_Intel_32         "I386;I486;I586;I686;I786;X86" )
set( herc_Intel_64         "X86_64;AMD64" )
set( herc_Intel            "${herc_Intel_32};${herc_Intel_64}" )


# Windows target version names and codes, used to ensure compilations
# target the correct version of the Windows internal API.  The names are
# our own invention; the hex strings are defined by Microsoft and
# documented at https://msdn.microsoft.com/en-us/library/windows/desktop/aa383745(v=vs.85).aspx
# and in header SDKDDKVer.h.  The specified or default version code is
# included in the generated config.h; the default version is the version
# of the target machine and SDK used to build Hercules.

set( herc_Windows_vers     "WINXP364; WINVISTA; WIN7;   WIN8;   WIN10"  )
set( herc_Windows_vercodes "0x0502;   0x0600;   0x0601; 0x0602; 0x0A00" )


# Sundry other helpful list definitions

set( herc_YES_NO           "YES;NO" )


# Reference strings for library names.  libm is required and the build
# will be failed if it is missing.  The remaining libraries are included
# in the build if required for the target system (librt, libnsl) or by
# build options (libcap).

set( name_libm         "m"   )    # IEEE math library, needed for STSI instruction
set( name_librt        "rt"  )    # POSIX real time extensions library, needed for Solaris
set( name_libnsl       "nsl" )    # Nameserver lookup library, needed for Solaris
set( name_libcap       "cap" )    # POSIX 1003.1e capabilities interface library
set( name_libsocket    "socket" ) # Socket library for Solaris, HP-UX, maybe others


# ----------------------------------------------------------------------
# Set up CMake to correctly probe the target system.
# ----------------------------------------------------------------------


# Ensure /usr/local/include is part of the compiler preprocessor -I
# includes.  We may wish to do the same for /usr/local/lib.  And
# because the local directory is used to build Hercules, it should
# also be used for all CMake tests for public headers.

if( NOT WIN32 )
    include_directories( BEFORE SYSTEM "/usr/local/include" )
    set( CMAKE_REQUIRED_INCLUDES "/usr/local/include" )
endif( )


# Set c compiler flag string to a) enable all warnings and b) treat all
# warnings as errors.  This must be done early because herc12_OptEdit.cmake
# does compilations to edit options.  These flags are only used during
# probes of the target system; they are not the start of the C compiler
# option string that will be used to build Hercules.

if( "${CMAKE_C_COMPILER_ID}" STREQUAL "${herc_CompilerMSVC}" )
    set( all_warnings_are_errors "/W4 /WX" )
else( )
    set( all_warnings_are_errors "-Wall -Werror" )
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


# ----------------------------------------------------------------------
# Define all user options and edit them.  If user option HELP is defined,
# CMakeHercOptEdit.cmake will display help and return without doing any
# other processing.  And if help is displayed, then nothing else is done.
# ----------------------------------------------------------------------


include( CMake/Herc10_OptDef.cmake )          # define user options and build defaults

include( CMake/Herc12_OptEdit.cmake )         # edit all provided user options
if( HELP )
    return( )
endif( )


# ----------------------------------------------------------------------
# probe the target system hardware, operating system, userland, and c
# compiler.
# ----------------------------------------------------------------------

include( CMake/Herc20_TargetEnv.cmake )       # collect info about the target system

# There are two userland tests specific to macOS.  Apart from that,
# userland testing for macOS is the same as that for UNIX-like systems.
# So while macOS might merit a separate userland script on principle, it
# makes little sense to split the script for two tests when there is so
# much overlap otherwise.  Maybe later if additional tests are needed.

if( WIN32 )
    include( CMake/Herc22_UserlandWin.cmake )     # check for Windowns userland requirements.
else( )
    include( CMake/Herc22_Userland.cmake )        # check for userland requirements.
endif( )

# Probe the characteristics of the c compiler.  For the moment we assume
# that if the MSVC compiler is not in use, the compiler is gcc-like

if( "${CMAKE_C_COMPILER_ID}" STREQUAL "${herc_CompilerMSVC}" )
    include( CMake/Herc25_CompilerMSVC.cmake ) # check compiler capabilities
else( )
    include( CMake/Herc25_Compilergcclike.cmake ) # check compiler capabilities
endif( )


# ----------------------------------------------------------------------
# Interpret builder-specified options in the context of the target
# system.  Set compiler and build options based accordingly.
# ----------------------------------------------------------------------

include( CMake/Herc28_OptSelect.cmake )       # Make selected options effective


# Five different routines are used to set options for the c compiler,
# one each for clang, gcc, Apple gcc, Windows, and unknown.  Expect
# additional routines for Windows-clang, Apple-clang, Intel C, and AIX.
# If we can find test articles.

# When the c compiler is not recognized, there is little we can do, but
# we can let the builder specify an optimization string.  If a builder
# expects automatic optimization with an unrecognized compiler, we will
# abort the build.

if( ("${CMAKE_C_COMPILER_ID}" STREQUAL "${herc_CompilerClang}" )
        OR ( "${CMAKE_C_COMPILER_ID}" MATCHES "^${herc_CompilerAppleClang}" )
        )
    include( CMake/Herc31_COptsClang.cmake )

elseif( "${CMAKE_C_COMPILER_ID}" STREQUAL "${herc_CompilerGNU}" )
    if( (APPLE)
            AND ( CMAKE_C_COMPILER_VERSION VERSION_LESS "4.2.2" ) )
        include( CMake/Herc31_COptsAppleGNU.cmake )
    else( )
        include( CMake/Herc31_COptsGNU.cmake )
    endif( )

elseif( "${CMAKE_C_COMPILER_ID}" STREQUAL "${herc_CompilerMSVC}" )
    include( CMake/Herc31_COptsMSVC.cmake )

else( )
    include( CMake/Herc31_COptsUnknown.cmake )

endif( )

# This should really be done in the respective Herc31.  Note that
# any specified ADD-CFLAGS have already been edited for valitidy
# in Herc12_OptEdit.cmake.

if( NOT ("${ADD-CFLAGS}" STREQUAL "") )
    set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${ADD-CFLAGS}" )
endif( )


# ----------------------------------------------------------------------
# Anything that can abort the build has already been detected.  If there
# are any error messages, send them to stderr and abort the build.
# ----------------------------------------------------------------------

if( herc_Error )
    message( "Generator:              ${CMAKE_GENERATOR} ${CMAKE_GENERATOR_PLATFORM}" )

    message( "Generation and build terminated due to ${herc_Error} errors:" )
    foreach( index RANGE 1 ${herc_Error} )
        message( "Error ${index}/${herc_Error}: ${herc_EMessage${index}}" )
    endforeach( index )
    message( FATAL_ERROR "Generation and build terminated..." )
    return( )
endif( herc_Error )


# ----------------------------------------------------------------------
# Define targets for the external projects used by Hercules.  For now,
# external packages include BZip2, SoftFloat-3a For Hercules, and Zlib.
# When the target system is Windows, PCRE is added to that list.
# ----------------------------------------------------------------------

include( CMake/Herc41_ExtPackageBuild.cmake )



# ----------------------------------------------------------------------
# Assemble the include directory set, the link library set, and the
# install RPATH (UNIX-like only).  Create a configured config.h.
# ----------------------------------------------------------------------


# Include directories for compiles.  The binary directory will include
# config.h.  The decNumber directory precedes the source directory so
# that we get the authoritative public headers, not just something that
# lets an IDE do reasonable incremental compilation.

include_directories( ${PROJECT_BINARY_DIR}
                     ${PROJECT_SOURCE_DIR}
        )

if( "${HAVE_HQA_H}" )
    include_directories( BEFORE ${HQA_INSTALL_DIR} )
endif( )


# Assemble the link library string to be used for compilation units.  This
# is done in one place rather than just adding libraries as the need is
# identified so that link library ordering can be addressed explicitly
# in one place rather than as a side effect of library test order.

set( link_alllibs
            ${link_libm}
            ${link_librt}
            ${link_libnsl}
            ${link_libcap}
            ${link_libsocket}
            ${CMAKE_DL_LIBS}
      )

if( WIN32 )
    set( link_alllibs ${link_alllibs} ws2_32 )
endif( )


# ----------------------------------------------------------------------
# Set the RPATH to be used by UNIX-like and macOS systems when running
# Hercules from the build directory and when installing Hercules.
# Windows does not use RPATH.
# ----------------------------------------------------------------------

# Use (don't skip) the full RPATH for the build tree, and do not use the
# install directory RPATH when building.  This enables execution from
# the build directory prior to install.  CMake correctly handles setup
# of RPATH for execution from the build tree, regardless of whether
# external packages are built by the Hercules CMake build or previously
# built outside the Hercules build tree.

# CMake will update the RPATH in place in the executable or shared
# library when installing.

# Note that CMake only includes path(s) for shared libraries in the
# build RPATH.  It does not include paths for dynamically loaded
# mobules.  If loadable module paths need to be in the build RPATH,
# they need to be coded.  Hercules doees not require this because
# loadable modules are built in the same directory as the executable,
# but if that changes, add code here to deal with it.

# use, i.e. don't skip the full RPATH for the build tree
set( CMAKE_SKIP_BUILD_RPATH  FALSE )
# when building, don't use the install RPATH
# only later when installing
set( CMAKE_BUILD_WITH_INSTALL_RPATH FALSE )

# define the RPATH relative to the executable path
# will make possible to move around/rename the "PREFIX" path
if( APPLE )
    # add the automatically determined parts of the RPATH
    set( CMAKE_INSTALL_RPATH "@executable_path/../${libs_rel_dir}" )
    if( NOT ( "${libs_rel_dir}" STREQUAL "${mods_rel_dir}" ) )
        set( CMAKE_INSTALL_RPATH
                "${CMAKE_INSTALL_RPATH}"
                "@executable_path/../${mods_rel_dir}"
            )
    endif( )

else( APPLE )
    # If Hercules is to be installed in other than a standard location for
    # the target system, include the non-standard lib directory in the
    # installation RPATH.  The lib directory is relative to $ORIGIN, the
    # linking loader's synonym for the directory from which the executable
    # was fetched.

    # Following inspired by the CMake Wiki RPATH page at
    # https://cmake.org/Wiki/CMake_RPATH_handling.
    # See that page for copyright, attribution, and license.
    set( CMAKE_INSTALL_RPATH "\$ORIGIN/../${libs_rel_dir}" )
    if( NOT ( "${libs_rel_dir}" STREQUAL "${mods_rel_dir}" ) )
        set( CMAKE_INSTALL_RPATH
                "${CMAKE_INSTALL_RPATH}"
                "\$ORIGIN/../${mods_rel_dir}"
            )
    endif( )
    # End of code inspired by Wiki

endif( APPLE )

# create config.h.

configure_file( CMake/CMakeconfig.h.in config.h @ONLY )

if( WIN32 )
    set( CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} /DHAVE_CONFIG_H" )
endif( )


# ----------------------------------------------------------------------
# Create the initial commitinfo.h.  This file will be recreated on each
# build when the git repository has been updated.
# ----------------------------------------------------------------------

# If the source directory is a git repository, extract the git status into
# commitinfo.h so the status can be displayed on startup.  If not a git
# repository, then create a dummy commitinfo.h to enable the build.  The
# script CMakeHercGitVer.cmake issues a few user messages, so we will
# do the top border here.

message( "------------------------------------------------------------------------" )
execute_process( COMMAND ${CMAKE_COMMAND}
                    -DBDIR=${PROJECT_BINARY_DIR}
                    -DSDIR=${PROJECT_SOURCE_DIR}
                    -DGIT_EXECUTABLE=${GIT_EXECUTABLE}
                    -DGIT_FOUND=${GIT_FOUND}
                    -P ${PROJECT_SOURCE_DIR}/CMake/Herc01_GitVer.cmake
                OUTPUT_VARIABLE result_var
                ERROR_VARIABLE result_var
            )
message( "${result_var}")


# Share some information about where things are going and how they get there.

message( "\n------------------------------------------------------------------------" )
message( "Install prefix          ${CMAKE_INSTALL_PREFIX}" )
message( "Executable directory    ${exec_rel_dir}" )
message( "Library directory       ${libs_rel_dir}" )
message( "Modules directory       ${mods_rel_dir}" )
message( "Data Directory (http)   ${http_dir}" )
message( "Source directory        ${PROJECT_SOURCE_DIR}" )
message( "Build directory         ${PROJECT_BINARY_DIR}" )
if( "${CMAKE_GENERATOR_TOOLSET}" STREQUAL "" )
    message( "Generator:              ${CMAKE_GENERATOR} ${CMAKE_GENERATOR_PLATFORM}" )
else( )
    message( "Generator & toolset:     ${CMAKE_GENERATOR}, ${CMAKE_GENERATOR_TOOLSET}" )
endif( )
message( "Build command line:     ${CMAKE_MAKE_PROGRAM}" )
message( "C compiler version:     ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}" )
message( "C options (Release):    ${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_RELEASE}" )
message( "C options (Debug):      ${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_DEBUG}" )
if( WIN32 )
    message( "Resource compiler opt:  ${CMAKE_RC_FLAGS}" )
    message( "Linker opts (exec):     ${CMAKE_EXE_LINKER_FLAGS} ${CMAKE_EXE_LINKER_FLAGS_RELEASE}" )
    message( "Linker opts (shlb):     ${CMAKE_SHARED_LINKER_FLAGS} ${CMAKE_SHARED_LINKER_FLAGS_RELEASE}" )
    message( "Linker opts (modl):     ${CMAKE_MODULE_LINKER_FLAGS} ${CMAKE_MODULE_LINKER_FLAGS_RELEASE}" )
else( )
    message( "Install RPATH:          ${CMAKE_INSTALL_RPATH}" )
endif( )
message( "Large file support:     ${herc_LargeFile_Status}" )
message( "Hercules link libs:     ${link_alllibs}" )
message( "Target processor:       ${CMAKE_SYSTEM_PROCESSOR}" )
message( "------------------------------------------------------------------------\n\n" )



# ----------------------------------------------------------------------
#
# Create all targets and tests needed to build Hercules.
#
# ----------------------------------------------------------------------


include( CMake/Herc60_CreateTargets.cmake )

# If the target system is Windows, create targets to copy the external
# package DLLs to the directory that contains the Hercules executable.

if( WIN32 )
    include( CMake/Herc65_ExtPackageWinCopy.cmake )
endif( )

# Copy the tests directory to the build tree so that execution of test
# scripts do not require the source tree.

file( COPY ${PROJECT_SOURCE_DIR}/tests/ DESTINATION ${PROJECT_BINARY_DIR}/tests )

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



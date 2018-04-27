# Herc31_COptsMSVC.cmake  -  Set C compiler options - used for the
#                            Microsoft Visual Studio C compiler.

#[[   Copyright 2017 by Stephen Orso.

      Distributed under the Boost Software License, Version 1.0.
      See accompanying file BOOST_LICENSE_1_0.txt or copy at
      http://www.boost.org/LICENSE_1_0.txt)
]]

#[[

CMake default c flags for an MSVC build are:

  Base:    /DWIN32 /D_WINDOWS /W3

    /W3   Display production quality warnings.

  Release: /MD /O2 /Ob2 /DNDEBUG

    /MD   Use multithreaded dynamic runtime library
    /O2   Optimize for speed
    /Ob2  Inline whatever the compiler likes.  Included in /O2

  Debug:   /MDd /Zi /Ob0 /Od /RTC1

    /MDd  Use multithreaded dynamic runtime library debug version
    /Od   No optimization
    /Ob0  Disable inlining
    /Zi   Generate debugging information
    /RTC1 Enable runtime stack frame checking and uninitialized variable
        checking.

Options that are required to build Hercules are *always* added
to the base C command line options.  These include defining
warnings as errors, linker warnings as errors, multiprocessor
exploitation during the build.

Configuration-specific options such as debug and optimization are
added to the configuration-speciic CMake C flags variables.  Flags
needed to create .pdb files are specified on both of the configuration-
specific flags variables so that it is easy to remove .pdb creation
from the Release configuration when appropriate.

A builder-provided optimization string is appended to _BOTH_ the
Release and Debug strings so that the builder can override any
optimization or debug string provided by CMake or this script.

OPTIMIZE=YES means this script will uset the default CMake flags
for a Release configuration and will in addition set the /favor
compiler option based on the detected host processor.  OPTIMIZE=YES
is ignored for a Debug build.

OPTIMIZE=NO means the CMake default optimization options (/O2 /Ob2)
are removed from the Release configuration.  No change is made to
the Debug configuration.

See https://msdn.microsoft.com/en-us/library/k1ack8f1(v=vs.140).aspx for
specifics of the optimizations associated with the variants of the /O
option.  That page is for Visual Studio 2015, but that page has the best
links to options available for Visual Studio 2017 and for older
versions.

For processor-based stuff (OPTIMIZE=YES specified or defaulted, we look
at the environment variables PROCESSOR_IDENTIFIER (Windows) and Platform
(Visual Studio command prompt) to determine the host processor and the
appropriate /flavor: tag.  When compiling for other than the host,
OPTIMIZE should be used to specify the /flavor: option.  For distribution
builds, OPTIMIZE="/flavor:blend" should be used as a best choice for an
unknown target CPU.


]]


# ----------------------------------------------------------------------
#
# Set basic C compiler flags other than optimization flags.
#
# ----------------------------------------------------------------------

# The nmake Windows build, if ASSEMBLY_LISTINGS was defined, would
# put those listings in the msvc.AMD64.cod subdirectory of the build
# tree.   We will see where they appear in a CMake build and act
# accordingly.

if( DEFINED ASSEMBLY_LISTINGS )
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /FAcs /Fa${PROJECT_BINARY_DIR}/listings" )
endif( )

# Useful MSVC c compiler flags

string( CONCAT CMAKE_C_FLAGS "${CMAKE_C_FLAGS} "
        "/nologo "   # suppress startup banner line and copyright notice
        "/D HAVE_CONFIG_H "    #  Indicate config.h is present
        "/we4101 "  # Treat as error: 'identifier' : unreferenced local variable
        "/we4102 "  # Treat as error: 'label' : unreferenced label
        "/we4702 "  # Treat as error: unreachable code
        "/WX"       # Treat all warnings as errors
        )

# MSVC 2015 and better can use multiple processors to build.  Multiple
# processors interfers with /Gm, and the /Gm option is reputed to not
# work well anyway, so /MP wins.  Besides, the documentation leads me to
# believe that /Gm is useful for C++ only, what with its references to
# changed classes.

if( NOT _MSC_VER STRGREATER "1499" )       # Visual Studio 2008 or better
    string( CONCAT CMAKE_C_FLAGS "${CMAKE_C_FLAGS} "
        "/MP"               # Use multiple processors if available
        )
endif( )

# Disable some nuisance warnings that get displayed by MSVC 2015 or
# newer.  This applies to both debug and release builds.

if( NOT _MSC_VER STRLESS "1900" )     # Visual Studio 2015 or better
    string( CONCAT CMAKE_C_FLAGS "${CMAKE_C_FLAGS} "
            "/wd4091 "  # 'keyword' : ignored on left of 'type' when no
                        # ...variable is declared
            "/wd4172 "  # returning address of local variable or temporary
            "/wd4312 "  # 'operation': conversion from 'type1' to 'type2'
                        # ...of greater size
            "/wd4477 "  # ‘<function>’ : format string ‘<format-string>’
                        # ...requires an argument of type ‘<type>’, but
                        # ...variadic argument <position> has type ‘<type>’
            "/wd4776 "  # ‘%<conversion-specifier>’ is not allowed in
                        # ...the format string of function ‘<function>’
            "/wd4996"   # The compiler encountered a deprecated declaration.
            )
endif( )


# ----------------------------------------------------------------------
#
# Set configuration-dependent C compiler flags other than optimization
# flags.
#
# ----------------------------------------------------------------------

# add /GL (global optimization) and /Zi (produce a separate .pdb file
# with detailed information for the debugger.

#set( CMAKE_COMPILE_PDB_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" )
set( CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /GL /Zi" )

# The CMake debug configuration flags are sufficient for a Windows build.
# We will add preprocessor definitions DEBUG and _DEBUG.

string( CONCAT CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} "
        "/D DEBUG "
        "/D _DEBUG"
        )

set( CMAKE_COMPILE_DEFINITIONS_DEBUG "${CMAKE_COMPILE_DEFINITIONS_DEBUG} DEBUG _DEBUG" )


# ----------------------------------------------------------------------
#
# Set the Resource Compiler flag.  These are actually macro definitions,
# but there does not seem to be a CDEFS CMake variable.  So we do it in
# CMAKE_RC_FLAGS.
#
# ----------------------------------------------------------------------

# While MS is quite clear that the ANSI predefined macros are _not_
# defined for the resource compiler, apparently at least one important
# MS-specific one is not either:  _WIN64.  So we shall use SIZEOF_SIZE_P
# to set it.  See: https://msdn.microsoft.com/en-us/library/windows/desktop/aa381032(v=vs.85).aspx
# for Microsoft's limited discussion of this.

if( SIZEOF_SIZE_P GREATER 4 )
    set( CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} -D _WIN64" )
endif( )

if( DEFINED PRERELEASE )
    set( CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} -D PRERELEASE=1" )
endif( )


# ----------------------------------------------------------------------
#
# Set configuration-independent linker flags
#
# ----------------------------------------------------------------------

# Do not suppress Manifests with /MANIFEST:NO.  Manifests are required to
# to connect to the correct C runtime DLL on pre-VS2015 builds.

# Generate map files.  The nmake-based Windows build scripts created map
# files in a subdirectory of the source tree.  A CMake build creates the
# map files in the build directory for executables and shared libraries.

set( CMAKE_EXE_LINKER_FLAGS    "${CMAKE_EXE_LINKER_FLAGS} /MAP" )
set( CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /MAP" )
set( CMAKE_MODULE_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /MAP" )

# Default base address: The nmake-based Windows build scripts based
# Hercules at the 4mb line (0x400000).  There is a nice discussion of
# the reason for this choice at:
#
#   https://blogs.msdn.microsoft.com/oldnewthing/20141003-00/?p=43923/
#
# Starting with Visual Studio 2015, Microsoft changed the defaults for
# /BASE for 64-bit systems to 0x140000000 for executables and
# 0x180000000 for DLLs.
#
# Starting with Visual Studio 2015, Microsoft recommends /DYNAMICBASE
# to enable Address Space Layout Randomization (ASLR).  The linker
# option that supports ASLR, /DYNAMICBASE, is supported in Visual Studio
# 2008 and newer, and ASLR itself is supported on Windows Vista and
# higher.

# /DEBUG is used for both Debug and Release so we get PDB files for
# either configuration.

set( CMAKE_EXE_LINKER_FLAGS    "${CMAKE_EXE_LINKER_FLAGS} /DEBUG /DYNAMICBASE" )
set( CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /DEBUG /DYNAMICBASE" )
set( CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} /DEBUG /DYNAMICBASE" )


# ----------------------------------------------------------------------
# Above this point, everything that has been added to compiler and
# linker flags is needed for any compilation/link and has nothing to do
# with optimization.
# ----------------------------------------------------------------------


# ----------------------------------------------------------------------
#
# Set configuration-dependent C compiler optimization flags
#
# ----------------------------------------------------------------------

# The OPTIMIZE= option really does not address the question of multi-
# configuration generators, nor does it address the availability of
# CMake Debug and Release configurations.  Between the reasonable
# optimization defaults selected by CMake for both Release and Debug
# when Windows is targeted, optimization flags are set as follows:

# OPTIMIZE=YES:


# Builder-specified automatic optimization not YES nor NO.  So the
# builder has provided an optimization string that we shall use.
# The builder-supplied string must be appended to the configuration-
# specific C flags for the release configuration.  The flags for the
# debug configuration are not changed.
#

if( NOT ("${OPTIMIZATION}" STREQUAL "") )
    if( NOT ( "${OPTIMIZATION}" IN_LIST herc_YES_NO) )
        set( CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} ${OPTIMIZATION}" )
        set( CMAKE_C_FLAGS_DEBUG   "${CMAKE_C_FLAGS_DEBUG} ${OPTIMIZATION}" )
        return( )
    endif( )
endif( )


# Builder specified OPTIMIZATION=NO.  Remove the default optimization
# flags from CMAKE_C_FLAGS_RELEASE and replace them with those from
# CMAKE_C_FLAGS_DEBUG.

if( "${OPTIMIZATION}" STREQUAL "NO" )
    string( REGEX REPLACE " \/O[^ ]*" "" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}" )
    string( REGEX MATCHALL " \/O[^ ]*" __noopt_flags  "${CMAKE_C_FLAGS_DEBUG}" )
    set( CMAKE_C_FLAGS_RELEASE  "${CMAKE_C_FLAGS_RELEASE} ${__noopt_flags}" )
    message( "OPTIMIZATION=NO: ${__noopt_flags} leads to ${CMAKE_C_FLAGS_RELEASE}" )
    unset( __noopt_flags )
    return( )       # all done.
endif( )


# At this point, OPTIMIZE was specified as YES or left blank.  Auto-
# optimize.  Default optimization flags provided by CMake are good.
# Just add /favor, which controls optimization for specific processors.

# Prior to Visual Studio 2012, the /favor option was available only in
# x64 compilers and could only favor Intel or AMD processors.

# There is no good way to determine if host=target because Windows
# reports all 64-bit processors, Intel and AMD (and ATOM?), as AMD64.
# If we know that we are building for the host, then we can use the
# Windows environment variable PROCESSOR_IDENTIFIER to distinquish
# between Intel, Intel ATOM/Centrino, and AMD processors.

# The default /favor:blend supports x64 optimizations only until Visual
# Studio 2012, when /favor:blend is documented as supporting x86 and
# x64 optimizations.  VS2012 also added /favor:ATOM.

# For the moment, we will support /favor:ATOM on ATOM processors when we
# can (VS2012 or better), /favor:AMD64 when we can detect an AMD 64-bit
# processor, and /favor:Intel64 on an Intel 64-bit processor.

string( REGEX MATCH "Intel|ATOM|AMD" __processor_arch "$ENV{PROCESSOR_IDENTIFIER}" )

if( MSVC_VERSION STRLESS "1800" )   # Less than VS 2012.  x64 Intel or AMD only
    if( WINDOWS_64BIT )         # and targetting a 64-bit system
        if( "${__processor_arch}" STREQUAL "Intel" )
            set( CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /favor:INTEL64" )
        elseif( "${__processor_arch}" STREQUAL "Intel" )
            set( CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /favor:AMD64" )
        endif( )
    endif( )
else( )                 # VS2012 or better.
    if( WINDOWS_64BIT )
        if( "${__processor_arch}" STREQUAL "Intel" )
            set( CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /favor:INTEL64" )
        elseif( "${__processor_arch}" STREQUAL "Intel" )
            set( CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /favor:AMD64" )
        endif( )
    elseif( "${__processor_arch}" STREQUAL "ATOM" )
        set( CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /favor:ATOM" )
    else( )
        set( CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /favor:blend" )
    endif( )
endif( )


# ----------------------------------------------------------------------
#
# Set configuration-dependent linker flags.
#
# ----------------------------------------------------------------------

# Allow link-time code generation for Release builds (required and
# implied for obj's compiled with /GL).

set( CMAKE_EXE_LINKER_FLAGS_RELEASE    "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG" )
set( CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /LTCG" )
set( CMAKE_MODULE_LINKER_FLAGS_RELEASE "${CMAKE_LINKER_LINKER_FLAGS_RELEASE} /INCREMENTAL:NO /LTCG" )



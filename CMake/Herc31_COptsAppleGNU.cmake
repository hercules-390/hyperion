# Herc31_COptsApplegcc.cmake - Set C compiler options - used for Apple
#                                when Apple gcc is used to build Hercules.

#[[   Copyright 2017 by Stephen Orso.

      Distributed under the Boost Software License, Version 1.0.
      See accompanying file BOOST_LICENSE_1_0.txt or copy at
      http://www.boost.org/LICENSE_1_0.txt)
]]

#[[
Apple is a bit weird for two reasons:
1) Early Apple Mac OS versions used gcc, and later versions
   used clang.  For a time, both were in use, gcc for
   preprocessing, llvm for code generation.  See the web site
   https://trac.macports.org/wiki/XcodeVersionInfo for a summary
   of the compilers included in the Apple development tool Xcode,
   and the relationship between Xcode versions and Mac OS versions.
2) Apple had no problem modifying gcc or (maybe) clang/llvm to suit
   their needs.  So Apple gcc is not the same as gcc in the open
   source world, and Apple clang may not be the same as an open source
   clang.

A further complication is that a user of a modern Apple Mac OS, say,
10.9 Mavericks or newer, can build and install a modern GNU gcc
development suite and compile with that.

This script deals only with gcc versions 4.2.1 or older on Apple
systems, which are presumed to be Apple-modified versions of gcc
included with Xcode or its predecessor CodeWarrior.  Higher gcc
versions are presumed to be user-built and are handled by the "normal"
Hercules gcc options script.


So MacOS situation is:
- pre-10.0                        Non-Darwin systems, not supported

- 10.0 (Cheetah, Mar 2001):       gcc 2.95.2, ABI 2.95.2
                                  10.0 is really not recommended

- 10.1 (Puma, Oct 2001):          gcc 2.95.2, ABI 2.95.2

- 10.2 (Jaguar, Aug 2002):        gcc 3.1, ABI 3.1

  Note: The gcc ABIs for 2.95 and 3.1 are not compatible with each
        other nor with gcc 3.3 and newer versions.

- 10.3 (Panther, Oct 2003):       gcc 3.3; older gcc versions
                                  available for projects targetting
                                  the ABI of older Mac OS versions

- 10.4 (Tiger, Apr 2005):         gcc 3.3, 4.0.0, 4.0.1
                                  First Mac OS to support Intel
                                  Last Mac OS to support PowerPC G3

- 10.5 (Leopard, Oct 2007):       gcc 4.2.0
                                  gcc 4.2.1, added in Xcode 3.1
                                  llvm/gcc 4.2.1, added in Xcode 3.1
                                  last version to support all Intel
                                  processors and PowerPC G4 and G5

- 10.6 (Snow Leopard, Aug 2009):  gcc 4.2.0, removed in Xcode 4.0+
                                  gcc 4.2.1, removed in Xcode 4.2
                                  llvm/gcc 4.2.1, added in Xcode 3.1
                                  clang Apple
                                  Intel support only
                                  last Mac OS to support 32-bit processors

- 10.7 (Lion, Jul 2011):          llvm/gcc 4.2.1
                                  clang Apple

- 10.8 (Mountain Lion, Jul 2012): llvm/gcc 4.2.1, removed in Xcode 5.0
                                  clang Apple

- All MacOS versions from 10.9 (Mavericks, Oct 2013) forward use the
  Apple variant of clang.  It is not clear if there is any real variation
  from the generally available clang.

Some options are needed for a correct build, for example
-fno-strict-aliasing when it is required.  Most other options appeared
after 4.2.1, which means a user gcc build, and those are not dealt
here.

Options that are required to build Hercules are *always* added
to the C command line options, as are the debugger options
("-W -Wall -g3 -ggdb3").  If OPTIMIZE="<whatever>" is coded on the
CMake command line, the builder's options are appended to the *always*
created options, which gives the builder the opportunity to add any
preferred optimization options, such as -O3 (which is never added
by this CMake script), and to negate any of the *always* created
options, because when options conflict, the last one wins.

It is not clear what happens in gcc when one specifies -O3 *after*
turning off something enabled by -O3, for example -fno-strict-aliasing.
But that is the builder's problem.  Ours here is to make sure the
optimize flag selection process is clear.

OPTIMIZE=YES means this script will interpret the target system's
and compiler's capabilities and nature and craft a reasonable and
workable set of optimization flags, starting with -O2.

OPTIMIZE=NO means only the options required to build Hercules and
the debugger options -W, -Wall, -g3, and -ggdb3 are included, as
is -O0.  No other options are included.

Apple gcc 3.3 optimization options enabled by each level of -O are
available at this mirror (for how long?):

   http://mirror.informatimago.com/next/developer.apple.com/documentation/DeveloperTools/gcc-3.3/gcc/Optimize-Options.html

and for 4.0.1, look here:

   http://mirror.informatimago.com/next/developer.apple.com/documentation/DeveloperTools/gcc-4.0.1/gcc/Optimize-Options.html

I haven't found mirrors for 2.95, 3.1, or 4.2.x

So rather than mess around with whether -fomit-frame-pointer should
or should not be included, we shall let the compiler include it
based on the optimization profile (-O0 through -O2) that is to be
used for the build.  Anyone feeling strongly about can always
override the compiler flags by passing -DOPTIMIZATION="whatever"
to CMake.

For processor-based stuff, we will use CMAKE_SYSTEM_PROCESSOR for
the processor name; this is what config.guess does in autotools.
CMAKE_SYSTEM_PROCESSOR is set to the host's uname -p output unless
cross-compiling, in which case the builder must set things up
on their own.
]]


# Apparently, selected versions of the old Apple gcc default preprocessor
# had an issue where preprocessor macro argument counting did not work.
# The circumvention was to use the Apple preprocessor, available in
# versions 2.95 and 3.1.  In Apple 3.3 and up, there appears to be no
# cure for the issue.  So for Apple gcc 3.3 and up, if argument counting
# is broken, the build must be failed.

# For details, review this quote from the Apple gcc 3.3 release notes:
#     ... the -traditional-cpp option has changed.
#    In Apple GCC 3.1 and earlier Apple GCC compilers,
#    -traditional-cpp was used to toggle between the
#    standard GNU GCC preprocessor and Apple's own
#    preprocessor, "cpp-precomp". The GNU GCC compiler
#    interpreted -traditional-cpp differently on all
#    other platforms. Since cpp-precomp has been removed
#    for Apple's GCC 3.3 compiler, the standard GNU
#    meaning of -traditional-cpp has been restored. By
#    default, the GCC 3.3 preprocessor conforms to the
#    ISO C standard. Using the -tradtional-cpp option
#    means the C preprocessor should instead try to
#    emulate the old "K&R C".

# The test in configure.ac for a K&R preprocessor when -traditional-cpp
# is included seems to be an empirical test for Apple gcc 3.3 or
# better.  We will just test the gcc version.

# Test to see if preprocessor macro argument counting is broken.

string( CONCAT herc_TestSource    # Test for broken argument counting
            "#include <stdio.h>\n"
            "#define MACRO(_x,_args...) printf(_x, ## _args)\n"
            "int  main( int argc, char **argv, char **arge )\n"
            "{\n"
            "MACRO( "bare printf\n" );\n"
            "return 0;\n"
            "}\n"
         )
herc_Check_Compile_Capability( "${herc_TestSource}" APPLE_GCC_ARGCT_BUSTED TRUE )

if( APPLE_GCC_ARGCT_BUSTED AND ( CMAKE_C_COMPILER_VERSION VERSION_LESS "3.3.0" )
    set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -traditional-cpp -Wno-endif-labels" )
else( )
    herc_Save_Error( "Apple gcc preprocessor arg count broken and cpp-precomp not available" "")
endif( )

unset( APPLE_GCC_ARGCT_BUSTED )


# Basic C flags. Enable all warnings, and enable gdb debugging options
# including macro definitions.  Set the calculated set of optimization
# flags to the null string.

set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DHAVE_CONFIG_H -W -Wall -g3 -ggdb3" )


# Flags needed to deal with issues in the toolchain.  I am not sure if
# the toolchain included with older mac OS versions has very strict
# aliasing, but if we found it, we will try to turn it off.  The Apple
# gcc 3.3 documentation includes -fstrict-aliasing, which implies the
# existence of -fno-strict-aliasing.

if( HAVE_VERY_STRICT_ALIASING )
    set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-strict-aliasing" )
endif( HAVE_VERY_STRICT_ALIASING )


# ----------------------------------------------------------------------
# Above this point, everything that has been added to c flags is needed
# for compilation and has nothing to do with optimization.
# ----------------------------------------------------------------------

# Builder-specified automatic optimization not YES nor NO.  So the
# builder has provided an optimization string that we shall use.  And
# exit, because there is nothing else we shall do.

if( NOT ("${OPTIMIZATION}" STREQUAL "") )
    if( NOT (${OPTIMIZATION} IN_LIST herc_YES_NO) )
        set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OPTIMIZATION}" )
        return( )
    endif( )
endif( )


# If automatic optimization is specified or defaulted to NO, or if this
# is a debug build, then add -O0 to the c flags and we are done.

if( DEBUG_BUILD
        OR ("${OPTIMIZATION}" STREQUAL "NO")
        OR ( ("${buildWith_OPTIMIZATION}" STREQUAL "NO")
                AND ("${OPTIMIZATION}" STREQUAL "") )
        )
    set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-omit-frame-pointer -O0" )
    return( )
endif( )


# Not a debug build, no builder-specified automatic optimization, and
# the default automatic optimization not YES or NO.  So the default
# automatic optimization is a string that we shall use.  Note: this is
# a really unlikely case, but if a Hercules maintainer decides that a
# default optimization string should be provided, we will respect it.

# And such a default may well be a poor idea because the default string
# would have to be compiler-agnostic and target-agnostic.

if( NOT ("${buildWith_OPTIMIZATION}" STREQUAL "YES") )
    set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${buildWith_OPTIMIZATION}" )
    return( )
endif( )


# ----------------------------------------------------------------------
# Automatic optimization requested or defaulted
# ----------------------------------------------------------------------

# Automatic optimization specified as or defaulted to YES.  Now the fun
# begins.  The first part is easy: -O2.  We do not do -O3; there is no
# consensus that it represents a better choice.  And if -O3 is the
# builder's wish, OPTIMIZATION="-O3" is the way to do it.

set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2" )


# Now see if we can improve on things.  Improvements are typically
# target-processor dependent, and that is the case now, but who knows
# the future brings....

# Determine if this is this a cross-platform build.

if( "${CMAKE_HOST_SYSTEM_PROCESSOR}" STREQUAL "${CMAKE_SYSTEM_PROCESSOR}" )
    set( host_is_target 1 )
else( )
    set( host_is_target 0 )
endif( )


# Convert the processor to upper case for later comparisons.

string( TOUPPER "${CMAKE_SYSTEM_PROCESSOR}" target_processor )


# Intel 64-bit processor-specific compiler flags.

# Some notes:
# 1) if the builder is cross-compiling, it is the builder's
#    responsibility to set a system processor that is valid for the
#    GNU compiler in use.
# 2) The Apple GNU gcc compilers do not accept -march=native.  Although
#    Apple only used Intel 64-bit processors, prior to 4.2.0 -march=K8
#    (an AMD processor type) is required.  4.2.0 and 4.2.1 expect
#    -march=core2.

if( "${target_processor}" IN_LIST herc_Intel_64 )

    if( NOT host_is_target )
        set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=${CMAKE_SYSTEM_PROCESSOR}" )
    else( )
        if( CMAKE_C_COMPILER_VERSION VERSION_LESS "4.2.0" )
            set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=K8" )
        else( )
            set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=core2" )
        endif( )
    endif( )

    unset( target_processor )
    unset( host_is_target )
    return( )
endif( )


# Intel 32-bit processor-specific compiler flags.

# Some notes:
# 1) if the builder is cross-compiling, it is the builder's
#    responsibility to set a system processor that is valid for the
#    GNU compiler in use.
# 2) Apparently, some systems report i786, but GNU does not accept
#    that.  Use Pentium4 instead.
# 3) The GNU compiler built with selected very old Red Had distros
#    apparently report version 2.9.6, even though that was never a
#    valid GNU release number.  And compilers reporting 2.9.6 do not
#    understand i686.
# 4) Apple gcc compilers do not accept -march=native.

if( "${target_processor}" IN_LIST herc_Intel_32 )

    if( "${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "i786" )
        set( I386_target "Pentium4" )
    else( )
        set( I386_target "${CMAKE_SYSTEM_PROCESSOR}" )
    endif( )

    set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=${I386_target}" )

    unset( I386_target )
    unset( target_processor )
    unset( host_is_target )
    return( )
endif( )


# if we reach this point, we did not recognize the processor.  So we
# cannot do any target processor-based optimizations.  Continue without
# them and hope for the best.

unset( target_processor )
unset( host_is_target )

return( )


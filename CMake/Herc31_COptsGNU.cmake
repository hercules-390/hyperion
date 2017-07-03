# Herc31_COptsGNU.cmake  -  Set C compiler options - used for GNU gcc on
#                           other than Apple platforms.

#[[   Copyright 2017 by Stephen Orso.

      Distributed under the Boost Software License, Version 1.0.
      See accompanying file BOOST_LICENSE_1_0.txt or copy at
      http://www.boost.org/LICENSE_1_0.txt)
]]

#[[
Set C compiler options - used for any gcc except for gcc versions
4.2.1 or less on Apple platforms.

Some options are needed for a correct build, for example
-mstructure-size-boundary=8 for certain ARM flavors, and
-fno-strict-aliasing when it is required.  Other options are
truly optimization related, such as -frename-registers on ARM.

Options that are required to build Hercules are *always* added
to the C command line options, as are the debugger options
("-W -Wall -g3 -ggdb3").  If OPTIMIZE="<whatever>" is coded on the
CMake command line, the builder's options are appended to the *always*
created options, which gives the builder the opportunity to add any
preferred optimization options, such as -O3 (which is never added
by this CMake script), and to negate any of the *always* created
options, because when options conflict, the last one wins.

It is not clear what happens in GCC when one specifies -O3 *after*
turning off something enabled by -O3, for example -fno-strict-aliasing.
But that is the builder's problem.  Ours here is to make sure the
optimize flag selection process is clear.

OPTIMIZE=YES means this script will interpret the target system's
and compiler's capabilities and nature and craft a reasonable and
workable set of optimization flags, starting with -O2.

OPTIMIZE=NO means only the options required to build Hercules and
the debugger options -W, -Wall, -g3, and -ggdb3 are included, as
is -O0.  No other options are included.

See https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html for
specifics of the optimizations turned on at each level of -O.  In
particular, note that any optimization level other than -Os or -O0
include -fomit-frame-pointer if doing so preserves the ability to
use the debugger.

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

Gcc 4.3.0 included support for the option -minline-stringops-dynamically
(see https://gcc.gnu.org/onlinedocs/gcc-4.3.0/gcc/i386-and-x86_002d64-Options.html#i386-and-x86_002d64-Options)
The gcc option summary was not updated for -minline-stringops-dynamically
until 4.4.0.)

The most recent Hercules configure.ac checks for a GNU compiler issue
that breaks implicit __alloca calls.  This issue affects only MinGW
and Cygwin versions of the GNU compilers, and then only versions 3.0.x
and 3.1.x.  A patch was written for 3.3 and backported to 3.2.  Because
this is a MinGW/Cygwin issue only, and Hercules does not use either
tool to build Hercules, testing around this issue has not been included
in CMake scripts for Hercules.  See:
- https://gcc.gnu.org/bugzilla/show_bug.cgi?id=8750 (desc. and 3.3 patch)
- https://gcc.gnu.org/ml/gcc-patches/2003-02/msg01482.html (3.2 patch)
]]


# Basic C flags. Enable all warnings, and enable gdb debugging options
# including macro definitions.  Set the calculated set of optimization
# flags to the null string.

set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DHAVE_CONFIG_H -W -Wall -g3 -ggdb3" )


# Flags needed to deal with issues in the toolchain.

if( HAVE_VERY_STRICT_ALIASING )
    set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-strict-aliasing" )
endif( HAVE_VERY_STRICT_ALIASING )


# If structures are padded and the target is ARM-like, then we must
# tell the compiler to pad to an 8-bit (one byte) boundary.

if( ("${target_processor}" MATCHES "ARM")
        OR ("${target_processor}" MATCHES "XSCALE") )
    if( NOT STRUCT_NOT_PADDED )
        set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mstructure-size-boundary=8" )
    endif( )
endif( )


# ----------------------------------------------------------------------
# Above this point, everything that has been added to c flags is needed
# for compilation and has nothing to do with optimization.
# ----------------------------------------------------------------------

# Builder-specified automatic optimization not YES nor NO.  So the
# builder has provided an optimization string that we shall use.

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
# would have to be compiler-agnostic.

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


# Make the target processor uppercase to simplify testing

string( TOUPPER "${CMAKE_SYSTEM_PROCESSOR}" target_processor )


# Intel 64-bit processor-specific compiler flags.

# Some notes:
# 1) if the builder is cross-compiling, it is the builder's
#    responsibility to set a system processor that is valid for the
#    GNU compiler in use.
# 2) GNU compilers 4.2.0 and better accept -march=native.  Prior to
#    4.2.0, Hercules used -march=K8 for all 64-bit processors, even on
#    Apple builds (which use a script separate from this one).

if( "${target_processor}" IN_LIST herc_Intel_64 )

    if( NOT host_is_target )
        set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=${CMAKE_SYSTEM_PROCESSOR}" )
    else( )
        if( CMAKE_C_COMPILER_VERSION VERSION_LESS "4.2.0" )
            set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=K8" )
        else( )
            set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=native" )
        endif( )
    endif( )

    if( CMAKE_C_COMPILER_VERSION VERSION_GREATER "4.2.9" )
        set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -minline-stringops-dynamically" )
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
# 4) GNU compilers 4.2.0 and better accept -march=native.
# 5) GNU compilers 4.3.0 and better accept -minline-stringops-dynamically.
#    This does not appear in GNU options summary documentation until
#    4.4.0, but it is there; see above.

if( "${target_processor}" IN_LIST herc_Intel_32 )

    if( "${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "i786" )
        set( I386_target "Pentium4" )
    elseif( ("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "i686")
            AND (CMAKE_C_COMPILER_VERSION VERSION_EQUAL "2.9.6")
            )
        set( I386_target "I586" )
    elseif( host_is_target
            AND (CMAKE_C_COMPILER_VERSION VERSION_GREATER "4.1.9")
            )
        set( I386_target "native" )
    else( )
        set( I386_target "${CMAKE_SYSTEM_PROCESSOR}" )
    endif( )

    set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=${I386_target}" )
    unset( I386_target )

    if( CMAKE_C_COMPILER_VERSION VERSION_GREATER "4.2.9" )
        set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -minline-stringops-dynamically" )
    endif( )
    unset( target_processor )
    unset( host_is_target )
    return( )
endif( )


# The older ARM processor is just reported as "ARM" in CMAKE_SYSTEM_PROCESSOR.
# There is not much that can be added for the original ARM.

if( "${target_processor}" STREQUAL "ARM" )
    string( CONCAT optimization_flags "${optimization_flags} "
            "-frename-registers"
            )
    unset( target_processor )
    unset( host_is_target )
    return( )
endif( )


# Newer ARM prcoessors, those with a number after "ARM", apparently
# benefit from -mcpu and -mtune, while older ones do not. This is
# apparently true for Xscale cpus too, but it is not clear how those
# systems are identified.

if( "${target_processor}" STREQUAL "ARM" )
    string( CONCAT optimization_flags "${optimization_flags} "
            "-mcpu=${CMAKE_SYSTEM_PROCESSOR} "
            "-mtune=${CMAKE_SYSTEM_PROCESSOR} "
            "-frename-registers"
            )
    unset( target_processor )
    unset( host_is_target )
    return( )
endif( )


# XSCALE processors are based on ARM and also apparently benefit from
# -mcpu and -mtune.

if( "${target_processor}" MATCHES "XSCALE" )
    string( CONCAT optimization_flags "${optimization_flags} "
            "-mcpu=${CMAKE_SYYTEM_PROCESSOR} "
            "-mtune=${CMAKE_SYSTEM_PROCESSOR} "
            "-frename-registers"
            )
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


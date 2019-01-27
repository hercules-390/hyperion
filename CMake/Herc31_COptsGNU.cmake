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
-mstructure-size-boundary=8 for certain ARM flavors.  Other options are
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
turning off something enabled by -O3.  But that is the builder's
problem.  Ours here is to make sure the optimize flag selection process
is clear.

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

set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DHAVE_CONFIG_H -g3 -ggdb3" )


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
# Below this point, everything affects the optimization flags.  We will
# set CMAKE_C_FLAGS_RELEASE only based on -DOPTIMIZE string or the flags
# determined by this script.
# ----------------------------------------------------------------------

# Builder-specified automatic optimization not YES nor NO.  So the
# builder has provided an optimization string that we shall use.

if( NOT ("${OPTIMIZATION}" STREQUAL "") )
    if( NOT (${OPTIMIZATION} IN_LIST herc_YES_NO) )
        set( CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} ${OPTIMIZATION}" )
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
    set( CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -fno-omit-frame-pointer -O0" )
    return( )
endif( )


# Not a debug build, builder-specified automatic optimization is not
# NO, and if builder-specified automatic optimization is not blank,
# the builder-specified automatic optimization is not NO.

# Determine automatic optizimation options.

# ----------------------------------------------------------------------
# Automatic optimization requested or defaulted
# ----------------------------------------------------------------------

# Automatic optimization specified as or defaulted to YES.  Now the fun
# begins.  The first part is easy: -O2.  We do not do -O3; there is no
# consensus that it represents a better choice.  And if -O3 is the
# builder's wish, OPTIMIZATION="-O3" is the way to do it.  Note: the
# spaces at the beginning and end of the input expression are essential.

string(REGEX REPLACE " -O3 " " "
       CMAKE_C_FLAGS_RELEASE " ${CMAKE_C_FLAGS_RELEASE} "
       )
set( CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O2" )


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

# For Intel processors, two checks apply to both 32-bit and 64-bit
# processors: -minline-stringops-dynamically and -march=native.
# We will test both here.  If -minline-stringops-dynamically is
# accepted, we will add it to the option string.  -march=native
# availability will be addressed based on processor bitness.

if( "${target_processor}" IN_LIST herc_Intel )
    CHECK_C_COMPILER_FLAG("-march=native" HAVE__MARCH_NATIVE)
    CHECK_C_COMPILER_FLAG("-minline-stringops-dynamically" HAVE__MINLINE-STRINGOPS-DYNAMICALLY)

    if( HAVE__MINLINE-STRINGOPS-DYNAMICALLY )
        set( CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -minline-stringops-dynamically" )
    endif( )

endif( )


# Intel 64-bit processor-specific compiler flags.

# Some notes:
# 1) if the builder is cross-compiling, it is the builder's
#    responsibility to set a system processor that is valid for the
#    GNU compiler in use.
# 2) GNU compilers 4.2.0 and better accept -march=native.  Prior to
#    4.2.0, Hercules used -march=K8 for all 64-bit processors, even on
#    Apple builds (which use a script separate from this one).
# 3) One target/compiler combination, NetBSD 7.0.1 and gcc 4.8.4,
#    reject -march=native.  Use -march=x86-64 instead.

if( "${target_processor}" IN_LIST herc_Intel_64 )
    if( NOT host_is_target )
        set( CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -march=x86-64" )

    elseif( CMAKE_C_COMPILER_VERSION VERSION_LESS "4.2.0" )
        set( CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -march=K8" )
    elseif( HAVE__MARCH_NATIVE )
        set( CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -march=native" )
    else( )
        set( CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -march=x86-64" )
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
# 6) One target/compiler combination, NetBSD 7.0.1 and gcc 4.8.4,
#    reject -march=native.  Use ${CMAKE_SYSTEM_PROCESSOR} instead.

if( "${target_processor}" IN_LIST herc_Intel_32 )

    if( "${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "i786" )
        set( CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -march=pentium4" )
    elseif( ("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "i686")
            AND (CMAKE_C_COMPILER_VERSION VERSION_EQUAL "2.9.6")
            )
        set( CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -march=i586" )
    elseif( host_is_target
            AND ( HAVE__MARCH_NATIVE )
            )
        set( CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -march=native" )
    else( )
        set( CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -march=${CMAKE_SYSTEM_PROCESSOR}" )
    endif( )

    unset( target_processor )
    unset( host_is_target )
    return( )
endif( )


# Optimization flags for ARM

set( optimization_flags "")

if( "${target_processor}" STREQUAL "ARM" )
    # The older ARM processor is just reported as "ARM" in CMAKE_SYSTEM_PROCESSOR.
    # There is not much that can be added for the original ARM.
    string( CONCAT optimization_flags "${optimization_flags} "
            "-frename-registers"
            )

elseif( "${target_processor}" MATCHES "ARM" )
    # Newer ARM prcoessors, those with a number after "ARM", apparently
    # benefit from -mcpu and -mtune, while older ones do not. This is
    # apparently true for Xscale cpus too, but it is not clear how those
    # systems are identified.
    string( CONCAT optimization_flags "${optimization_flags} "
            "-mcpu=${CMAKE_SYSTEM_PROCESSOR} "
            "-mtune=${CMAKE_SYSTEM_PROCESSOR} "
            "-frename-registers"
            )

elseif( "${target_processor}" MATCHES "XSCALE" )
    # XSCALE processors are based on ARM and also apparently benefit from
    # -mcpu and -mtune.
    string( CONCAT optimization_flags "${optimization_flags} "
            "-mcpu=${CMAKE_SYYTEM_PROCESSOR} "
            "-mtune=${CMAKE_SYSTEM_PROCESSOR} "
            "-frename-registers"
            )

endif( )


unset( target_processor )
unset( host_is_target )

return( )


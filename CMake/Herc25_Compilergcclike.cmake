# Herc25_Optgcclike.cmake - Analyze compiler capabilities for gcc-like compilers

#[[   Copyright 2017 by Stephen Orso.

      Distributed under the Boost Software License, Version 1.0.
      See accompanying file BOOST_LICENSE_1_0.txt or copy at
      http://www.boost.org/LICENSE_1_0.txt)
]]

#[[

Function/Operation
- Probes the functionality of gcc-like compilers (gcc, clang, Intel) on
  the target system to set preprocessor macros needed by Hercules.
- Missing but required capabilities result in an error message being addded
  to the herc_SaveError message array with the appropriate increment of
  the herc_Error variable.  This has the effect of terminating the build.
- Predefined types are also tested in this module.

Input Parameters
- None.  The caller has determined that a gcc-like compiler is in use for
  this build.

Output
- The following CMake variables are set by this file.  Those marked with an
  asterisk (*) are intended to be included in config.h, and the balance are
  for downstream use by the Hercules CMake build.

Notes
- While clang seems to be free of much of the special-case nonsense associated
  with gcc, there are still variables to be set to expose compiler capabilities
  to Hercules.
- clang 3.8 and 4.0 both report an equivalent gcc version of 4.2.1, but this
  does not mean that clang behaves like gcc 4.2.1.   So we test what needs to
  be tested without thinking about the reported gcc version.

]]

message( "Testing gcc-like c compiler capabilities and characteristics" )


# Test for the pre-defined types required by Hercules.  There are not
# that many.  Pre-defined types are distinct from types defined by
# standard headers.

check_type_size( __uint128_t  HAVE___UINT128_T )


# Test for atomic intrinsics.  There is some question in my mind whether
# this is a userland test or a compiler test.  But we will do it here
# for the nonce, and in part because different compilers implement atomic
# intrisics in different ways.  Start by testing for stdatomic.h.  If this
# missing there are no atomics.

# In order of preference, Hercules will use atomic intrinsics as follows:
# - C11 atomics
# - __atomic built-in functions
# - __sync build-in funcctions.
# This ordering reflects the relative performance of the atomic intrinsics,
# from best to worst.

# Only the `and`, `or`, and `xor` are needed, and only the byte/char type
# is used.  The atomic ops are used only for the NI(Y), OI(Y), and XOI(Y)
# instructions.

# Other atomic instrinsics would be needed for the "LOAD AND" family of
# instructions.

# It is interesting that the code for NI(Y), OI(Y), and XOI(Y) always use
# atomic intrinsics whether or not the Interlocked-Access Facility 2 (IAF2)
# is enabled or not, nor whether IAF2 is even appropriate for the architecture
# mode.

herc_Check_Include_Files( stdatomic.h OK )
if( HAVE_STDATOMIC_H )
    herc_Check_Symbol_Exists( __STDC_NO_ATOMICS__ stdatomic.h OK )
endif( )


# If we have C11 atomic intrinsics, see if they are lock free.  Preprocessor
# variables are supposed to define this, but at least one compiler (gcc 4.9.2)
# sets up the strings as function calls.  So...we will run a program that
# retrieves the lock free status for each atomic type and creates a .cmake
# script to set the required cmake variables.

if( NOT HAVE_DECL___STDC_NO_ATOMICS__ )
    if( HAVE_STDATOMIC_H )
        if( NOT CMAKE_REQUIRED_QUIET )
            message( STATUS "Checking for C11 intrinsics" )
        endif( )
        herc_Check_C11_Atomics( "${PROJECT_BINARY_DIR}/CMakeHercC11LockFree.cmake" )
        if( C11_ATOMICS_AVAILABLE )
            include( ${PROJECT_BINARY_DIR}/CMakeHercC11LockFree.cmake )
        endif( )
    endif( )
endif( )


# No C11 intrinsics.  Test for the "__atomic_*" functions.
# These are the second choice.

if( NOT C11_ATOMICS_AVAILABLE )
    if( NOT CMAKE_REQUIRED_QUIET )
        message( STATUS "Checking for atomic intrinsics" )
    endif( )
    try_compile(HAVE_ATOMIC_INTRINSICS ${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}
                SOURCES ${CMAKE_SOURCE_DIR}/CMake/CMakeHercTestAtomic.c
            )
    if( HAVE_ATOMIC_INTRINSICS )
        set( HAVE_ATOMIC_INTRINSICS 1 )    # Change TRUE to 1 for autotools compatibility.
    endif( )
endif( )


# Neither C11 nor __atomic_* intrinsics.  Test for the "sync" functions.
# These are the third choice.

if( NOT (C11_ATOMICS_AVAILABLE OR HAVE_ATOMIC_INTRINSICS) )
    if( NOT CMAKE_REQUIRED_QUIET )
        message( STATUS "Checking for sync builtins" )
    endif( )
    try_compile(HAVE_SYNC_BUILTINS ${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}
                SOURCES ${CMAKE_SOURCE_DIR}/CMake/CMakeHercTestSync.c
            )
    if( HAVE_SYNC_BUILTINS )
        set( HAVE_SYNC_BUILTINS 1 )    # Change TRUE to 1 for autotools compatibility.
    endif( )
endif( )


# Test for support of C99 flexible arrays.  These are supported by clang
# (and other compilers), but we will run the test before we set the variable.

if( NOT CMAKE_REQUIRED_QUIET )
    message( STATUS "Checking for C99 flexible arrays" )
endif( )
try_compile(C99_FLEXIBLE_ARRAYS
        ${PROJECT_BINARY_DIR}
        SOURCES ${PROJECT_SOURCE_DIR}/CMake/CMakeHercTestC99FlexArrays.c
        )
if( C99_FLEXIBLE_ARRAYS )
    set( C99_FLEXIBLE_ARRAYS 1 )    # for autotools compatibility
else( )
    unset( C99_FLEXIBLE_ARRAYS )
endif( )


# Test if the compiler pads structures to other than a byte boundary.
# Hercules requires a byte boundary.  GCC on ARM allows specification
# of byte alignment in compiler -m options if the toolchain pads
# structures to other than a byte boundary.

# On other architectures, any padding found fails the build.

herc_Check_Struct_padding( GCC_STYLE_PACK STRUCT_NOT_PADDED )
if( NOT (STRUCT_NOT_PADDED OR (CMAKE_SYSTEM_PROCESSOR MATCHES "ARM")) )
    herc_Save_Error( "The c compiler pads structures to greater than a one-byte boundary" )
endif( )


# Test for compiler support of packed structures.  Clang uses gcc-style
# packing directives, so this is a verification only.  We need not get
# crazy here, as we might have to with other compilers on non-x86 targets.

# Packed structure support is required to build Hercules.

herc_Check_Packed_Struct( GCC_STYLE_PACK STRUCT_PACKED_OK )
if( NOT STRUCT_PACKED_OK  )
    herc_Save_Error( "The c compiler does not support packed structures" )
endif( )


# Test for compiler support of sundry other things that are helpful in the
# Hercules build.  Absence of any of these does not impact the build apart
# from reflecting presence or absence in config.h.   Because CMake always
# links as part of try_compile, we need to ensure fully formed c main
# programs.

string( CONCAT herc_TestSource    # Test for __attribute__ ((format(printf,x,y)))
            "__attribute__ ((format(printf,1,2))) void logmsg(char *str, ...)\n"
            "   {return;}\n"
            "int main()\n"
            "   {return 0;}\n"
            )
herc_Check_Compile_Capability( "${herc_TestSource}" HAVE_ATTR_PRINTF TRUE )


# The autotools build for __attribute__ ((regparm(1))) was first tested if it
# existed and then tested if it was broken.  Just as easy to only test if
# is broken.  Whether its broken because __attribute__ ((regparm(1))) does
# not exist or because it incorrectly handles regparm(3) does not matter.

# The regparm(3) test in configure.ac dealt with a gcc error in versions
# 3.2 and 3.3 that affected Cygwin (and MinGW?) builds, and this CMake
# script does not support Cygwin nor MinGW.  So it is not clear that
# testing for a busted regparm(3) is even required.  But we

herc_Check_Regparm_Works( HAVE_ATTR_REGPARM )

string( CONCAT herc_TestSource    # Test for gcc diagostic pragma
            "/* Test an ancient diagnostic */\n"
            "#pragma GCC diagnostic ignored \"-Wformat\"\n"
            "int main()\n"
            "   {return 0;}\n"
            )
herc_Check_Compile_Capability( "${herc_TestSource}" HAVE_GCC_DIAG_PRAGMA TRUE )

string( CONCAT herc_TestSource    # Test for gcc diagnostic push/pop pragma
            "/* Test for push/pop */\n"
            "#pragma GCC diagnostic push \n"
            "int main()\n"
            "   {return 0;}\n"
            )
herc_Check_Compile_Capability( "${herc_TestSource}" HAVE_GCC_DIAG_PUSHPOP TRUE )

string( CONCAT herc_TestSource    # Test for 'function defined but not used' warnings],
            "static int unused() {return 1;}"
            "int main(int argc, char **argv)"
            "{  return 0;   }"
            )
herc_Check_Compile_Capability( "${herc_TestSource}" HAVE_GCC_UNUSED_FUNC_WARNING FALSE )

string( CONCAT herc_TestSource    # Test for variable set but not used' warnings]
            "int main(int argc, char **argv)"
            "{int rc;  rc = 2;"
            "    return 0;"
            "}"
            )
herc_Check_Compile_Capability( "${herc_TestSource}" HAVE_GCC_SET_UNUSED_WARNING FALSE )


herc_Check_Strict_Aliasing( HAVE_VERY_STRICT_ALIASING )

return( )




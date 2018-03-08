# Herc20_Target.cmake - Analyze target architecture and host for Hercules

#[[   Copyright 2017 by Stephen Orso.

      Distributed under the Boost Software License, Version 1.0.
      See accompanying file BOOST_LICENSE_1_0.txt or copy at
      http://www.boost.org/LICENSE_1_0.txt)
]]


#[[
This script sets values for the build that are determined by the
hardware architecture or target operating system.  This includes the
various sizeof_* macros.

Target OS decisions are made here because there are so few of them.

SIZEOF_INT   used to set width of effective address 32/64 bits in feature.h
SIZEOF_LONG  Used to determine how much to print when printing longs, also
                  as proxy for 32/64 machines in vstore.h.  Although that
                  choice is neutered by force of OPTION_STRICT_ALIGNMENT,
                  although the neutering is neutered by the typo in the
                  spelling of OPTION_STRICT_ALLIGNMENT [sic].
SIZEOF_OFF_T      Required for large file support.
SIZEOF_SIZE_T     used as a proxy to determine 32/64 bit system.  Also required
                  for large file support.
WORDS_BIGENDIAN   undefined means little-endian, but we should address the matter
                  explicitly.
WINDOWS_BUILD     Building on Windows.  Windows is different enough from
                  UNIX-like systems that it gets its own variable.
]]

message( "Checking target system properties" )


# The big one comes first: is the target hardware bigendian or not

test_big_endian( WORDS_BIGENDIAN )

# Test for a Windows build.  Defining WINDOWS_BUILD does a few things in
# config.h for compatibility with Hercules, which currently assumes all
# Windows builds have no config.h

if( WIN32 )
    set( WINDOWS_BUILD 1 )
    if( WINDOWS_64BIT )
        set( HOST_ARCH "x64" )
    else( )
        set( HOST_ARCH "x86" )
    endif( )
else( )
    set( HOST_ARCH "${CMAKE_SYSTEM_PROCESSOR}" )
endif( )


# Size of int.  This is used to determine for some architectures the
# word defined by VADR_L.  Why this is important is not clear on a
# first read of the code, but stuff does not work if it is wrong.

check_type_size("int" SIZEOF_INT LANGUAGE C )
if( NOT SIZEOF_INT )
    herc_Save_Error( "Unable to determine size of \"int\"." )
endif( )


# Size of size_t.  Used as a proxy for determining whether the target hardware
# is 64-bits when determining the maximum number of CPUs that can be generated.
# Used in tape support for maximum tape size.  Also used when determining the
# maximum disk size that can be reported.

check_type_size("size_t" SIZEOF_SIZE_T LANGUAGE C )
if( NOT SIZEOF_SIZE_T )
    herc_Save_Error( "Unable to determine size of \"size_t\"." )
endif( )


# Size of long.  This is used to determine print format sizes and
# for a part of large filesystem support.

check_type_size("long" SIZEOF_LONG LANGUAGE C )
if( NOT SIZEOF_LONG )
    herc_Save_Error( "Unable to determine size of \"long\"." )
endif( )


# Size of int *.  This macro is used frequently in the 3705 device emulation
# code and to determine how large an integer pointer is when a pointer needs
# to be printf'd.

check_type_size("int *" SIZEOF_INT_P LANGUAGE C )
if( NOT SIZEOF_INT_P )
    herc_Save_Error( "Unable to determine size of \"int *\"." )
endif( )


# The size of off_t determines in part whether the build will support
# large files on non-AIX systems.  AIX always requires LFS support

check_type_size("off_t" SIZEOF_OFF_T LANGUAGE C )
if( NOT SIZEOF_OFF_T )
    herc_Save_Error( "Unable to determine size of \"off_t\"." )
endif( )


# If the host does not natively support large files and is an open
# source system, see if it advertises the POSIX macros that indicate the
# availability of large file support.  CMake variables HAVE_DECL_...
# will be set for later testing in Herc28_OptSelect.cmake.  Find the
# discussion of open system support for LFS in Herc28_OptSelect.cmake
# under the heading of Option LARGEFILE.

# Hercules-supported Windows versions always support large files.

if( NOT ( (SIZEOF_OFF_T EQUAL 8) OR WIN32 ) )
    herc_Check_Symbol_Exists( _LFS_LARGEFILE    "unistd.h" OK )
    herc_Check_Symbol_Exists( _LFS64_LARGEFILE  "unistd.h" OK )
endif( )


# Build hercifc only if the target is BSD, Linux, or Apple-Darwin

if( APPLE )
    if(CMAKE_SYSTEM_NAME MATCHES "Darwin" )
        set( BUILD_HERCIFC 1 )
    endif( )
elseif( UNIX )
#   We do not build hercifc for AIX, only Linux & BSD
    if(CMAKE_SYSTEM_NAME MATCHES "Linux" )
        set( BUILD_HERCIFC 1 )
    elseif(CMAKE_SYSTEM_NAME MATCHES "BSD" )
        set( BUILD_HERCIFC 1 )
    endif( )
endif( )


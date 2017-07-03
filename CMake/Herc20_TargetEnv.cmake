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
]]

message( "Checking target system hardware properties" )


# The big one comes first: is the target hardware bigendian or not

TEST_BIG_ENDIAN( WORDS_BIGENDIAN )


# Size of int.  This is used to determine for some architectures the
# word defined by VADR_L.  Why this is important is not clear on a
# first read of the code, but stuff does not work if it is wrong.

CHECK_TYPE_SIZE("int" SIZEOF_INT LANGUAGE C )
if( NOT SIZEOF_INT )
    herc_Save_Error( "Unable to determine size of \"int\"." "" )
endif( )


# Size of size_t.  Used as a proxy for determining whether the target hardware
# is 64-bits when determining the maximum number of CPUs that can be generated.
# Used in tape support for maximum tape size.  Also used when determining the
# maximum disk size that can be reported.

CHECK_TYPE_SIZE("size_t" SIZEOF_SIZE_T LANGUAGE C )
if( NOT SIZEOF_SIZE_T )
    herc_Save_Error( "Unable to determine size of \"size_t\"." "" )
endif( )


# Size of long.  This is used to determine print format sizes and
# for a part of large filesystem support.

CHECK_TYPE_SIZE("long" SIZEOF_LONG LANGUAGE C )
if( NOT SIZEOF_LONG )
    herc_Save_Error( "Unable to determine size of \"long\"." "" )
endif( )


# Size of int *.  This macro is used frequently in the 3705 device emulation
# code and to determine how large an integer pointer is when a pointer needs
# to be prindf'd.

CHECK_TYPE_SIZE("int *" SIZEOF_INT_P LANGUAGE C )
if( NOT SIZEOF_INT_P )
    herc_Save_Error( "Unable to determine size of \"int *\"." "" )
endif( )


# The size of off_t determines in part whether the build will support
# large files on non-AIX systems.  AIX always requires LFS support

CHECK_TYPE_SIZE("off_t" SIZEOF_OFF_T LANGUAGE C )
if( NOT SIZEOF_OFF_T )
    herc_Save_Error( "Unable to determine size of \"off_t\"." "" )
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
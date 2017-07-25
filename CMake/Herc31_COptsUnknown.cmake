# Herc31_COptsunknown.cmake  -  Set C compiler options when building
#                               an unrecognized compiler.

#[[   Copyright 2017 by Stephen Orso.

      Distributed under the Boost Software License, Version 1.0.
      See accompanying file BOOST_LICENSE_1_0.txt or copy at
      http://www.boost.org/LICENSE_1_0.txt)
]]


# Unrecognized C compiler.  We do not know how to control it through
# command line options.  Which means we cannot automatically create
# optimization flags or flags to deal with detected oddities.

# The default for automatic optimization is NO for an unrecognized
# compiler.  If OPTIMIZATION=YES is specified, the build is terminated
# with an error message.

# If a builder specifies NO, the build continues without comment.

# If a builder specifies a string, that string is used.


set( CMAKE_C_FLAGS  "-DHAVE_CONFIG_H" )

if( "${OPTIMIZATION}" STREQUAL "YES" )
    herc_Save_Error( "Automatic optimization not possible with unrecognized c compiler: \"${CMAKE_C_COMPILER_ID}\"\n" )
    return( )
endif( )


# The builder has provided flags to be used by the compiler.

if( NOT ("${OPTIMIZATION}" STREQUAL "NO") )
    set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OPTIMIZATION}" )
endif( )


return( )



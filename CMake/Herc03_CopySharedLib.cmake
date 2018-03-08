# Herc02_ExtPackageBuild.cmake - Build an external package needed by
#                                Hercules

#[[   Copyright 2017 by Stephen Orso.

      Distributed under the Boost Software License, Version 1.0.
      See accompanying file BOOST_LICENSE_1_0.txt or copy at
      http://www.boost.org/LICENSE_1_0.txt)
]]

if( IN2 )
    message( "Copying ${NAME} shared libraries to ${OUT}" )
else( )
    message( "Copying ${NAME} shared library to ${OUT}" )
endif( )


if( IN1 )
    if( EXISTS "${IN1}" )
        file( COPY "${IN1}" DESTINATION "${OUT}" )
    endif( )
endif( )

if( IN2 )
    if( EXISTS "${IN2}" )
        file( COPY "${IN2}" DESTINATION "${OUT}" )
    endif( )
endif( )

if( IN3 )
    if( EXISTS "${IN3}" )
        file( COPY "${IN3}" DESTINATION "${OUT}" )
    endif( )
endif( )
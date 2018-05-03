# Herc03_CopySharedLib.cmake - Copy one, two, or three shared libraries
#                              from a source directory to a target
#                              directory.

#[[   Copyright 2017 by Stephen Orso.

      Distributed under the Boost Software License, Version 1.0.
      See accompanying file BOOST_LICENSE_1_0.txt or copy at
      http://www.boost.org/LICENSE_1_0.txt)
]]

#[[

This CMake script is the target of Add_Custom_Command calls in
Herc65_ExtPackageWinCopy.cmake.  The added commands ensure that shared
libraries created by executable packages are copied to the Hercules
binary directory when Hercules is built on Windows.

This script and the commands are not needed on UNIX-like and macOS
builds because the shared libraries are located using the RPATH
setting in the calling executable or shared library.
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
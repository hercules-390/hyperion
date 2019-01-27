# Herc41_ExtPackageBuild.cmake - Build external packages needed by
#                                Hercules

#[[   Copyright 2017 by Stephen Orso.

      Distributed under the Boost Software License, Version 1.0.
      See accompanying file BOOST_LICENSE_1_0.txt or copy at
      http://www.boost.org/LICENSE_1_0.txt)
]]

#[[

Build any required external packages that are required by Hercules or
have been requested by the builder or defaulted.

Package         Target
SoftFloat-3a    SoftFloat       Required by Hercules
BZip2           bz2             Optional, but included by default
PCRE            pcre            Windows only, optional, included by default
Zlib            zlib            Optional, but included by default


If the builder provided an installation directory for a given package,
then there is no need for it to be built here.  The import target for
a builder-specified package was imported in Herc28_OptSelect.cmake.


]]

include( CMake/Herc02_ExtPackageBuild.cmake )


# ----------------------------------------------------------------------
#
# SoftFloat-3a package, CMake target SoftFloat.
#
# ----------------------------------------------------------------------

if( NOT HAVE_S3FH_TARGET )
    herc_ExtPackageBuild( SoftFloat-3a S3FH SoftFloat ${git_protocol}//github.com/hercules-390/SoftFloat-3a master )
    set( herc_building_SoftFloat-3a TRUE )
    add_dependencies( SoftFloat S3FH )

endif( )


# ----------------------------------------------------------------------
#
# BZip2 package, CMake target bz2
#
# ----------------------------------------------------------------------

if( NOT "${BZIP2}" STREQUAL "NO" )

    if( NOT ( BZIP2_FOUND OR HAVE_BZIP2_TARGET ) )
        herc_ExtPackageBuild( BZip2 BZIP2 bz2 ${git_protocol}//github.com/hercules-390/h390BZip master )
        set( herc_building_BZip2 TRUE )
        set( HAVE_BZIP2_TARGET "${EXTPKG_ROOT}/BZip2/build/bzip2_target" )

    elseif( HAVE_BZIP2_TARGET )

    elseif( BZIP2_FOUND )
        message( STATUS "Creating target bz2 for BZip2 ${BZIP2_VERSION_STRING} installed on target system" )
        herc_Create_System_Import_Target( bz2 BZIP2 )

    endif()

# if we are not using a target system-provided BZip2 library, install it.

    if( HAVE_BZIP2_TARGET )
        herc_Install_Imported_Target( bz2 BZip2 )
    endif( )

    set( HAVE_BZLIB_H 1 )

endif( NOT "${BZIP2}" STREQUAL "NO" )


# ----------------------------------------------------------------------
#
# PCRE package, CMake target pcre
#
# ----------------------------------------------------------------------


if( WIN32 AND ( NOT "${PCRE}" STREQUAL "NO" ) )

    if( NOT HAVE_PCRE_TARGET )
        herc_ExtPackageBuild( PCRE PCRE pcre ${git_protocol}//github.com/hercules-390/h390PCRE master )
        set( herc_building_PCRE TRUE )
        set( HAVE_PCRE_TARGET "${EXTPKG_ROOT}/PCRE/build/pcre_target" )
    endif( )

# if we are not using a target system-provided ZLib library, install it.

    if( HAVE_PCRE_TARGET )
        herc_Install_Imported_Target( pcre PCRE )
        herc_Install_Imported_Target( pcreposix PCREPOSIX )
    endif( )

    set( HAVE_PCRE 1 )

endif( WIN32 AND ( NOT "${PCRE}" STREQUAL "NO" )  )


# ----------------------------------------------------------------------
#
# Zlib package, CMake target zlib
#
# ----------------------------------------------------------------------

if( NOT "${ZLIB}" STREQUAL "NO" )

    if( NOT ( ZLIB_FOUND OR HAVE_ZLIB_TARGET ) )
        herc_ExtPackageBuild( Zlib ZLIB zlib ${git_protocol}//github.com/hercules-390/h390Zlib master )
        set( herc_building_Zlib TRUE )
        set( HAVE_ZLIB_TARGET "${EXTPKG_ROOT}/Zlib/build/zlib_target" )

    elseif( HAVE_ZLIB_TARGET )

    elseif( ZLIB_FOUND )
        message( STATUS "Creating target zlib for Zlib ${ZLIB_VERSION_STRING} installed on target system" )
        herc_Create_System_Import_Target( zlib ZLIB )

    endif( )

# if we are not using a target system-provided ZLib library, install it.

    if( HAVE_ZLIB_TARGET )
        herc_Install_Imported_Target( zlib Zlib )
    endif( )

    set( HAVE_ZLIB_H 1 )
    set( HAVE_LIBZ   1 )

endif( NOT "${ZLIB}" STREQUAL "NO" )




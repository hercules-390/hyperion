# crypto/CMakeLists.txt -  Create the build for the crypto directory.

#[[   Copyright 2017 by Stephen Orso.

      Distributed under the Boost Software License, Version 1.0.
      See accompanying file BOOST_LICENSE_1_0.txt or copy at
      http://www.boost.org/LICENSE_1_0.txt)
]]


# Build the dyncrypt shared library.  This build script depends on many
# things being done by the script in the parent directory, including
# CMake minimum version, policy settings, in-source build prevention,
# and variables defining the build environment.

set(target_name dyncrypt )

set( ${target_name}_sources
        aes.h
        des.h
        sha1.h
        sha256.h
        aes.c
        des.c
        dyncrypt.c
        sha1.c
        sha256.c
      )

# Add the target for dyncrypt.so.  Because this shared library is
# loaded dynamically by Hercules, not at runtime by the the target
# system's linking loader, we do not wish the "lib" prefix on the
# library name.

add_library(${target_name} SHARED ${${target_name}_sources} )
add_dependencies( ${target_name} hercs herc )
set_target_properties( ${target_name} PROPERTIES PREFIX "" )


# If an external package is being built automatically for Hercules,
# then we need to add a dependency on that package so that it gets
# built before crypto and is made available to crypto.  If the builder
# provided a directory containing a previously-built package, then the
# package has been built already and a dependency is not needed.

# Crypto does not need any external packages.  But crypto includes
# hstdinc.h, which does include the headers.  So we have a dependency.
# It also creates a requirement for a target include directory.

# Because the external packages are used in relatively few places in
# Hercules, there seems to be little value in including the headers
# for them in hstdinc.h.

# Take note: targets in mixed case are the externalproject_add() targets
# that must be built before hercs, hence the dependencies on hercs to
# get the external procjects built before hercs.

if( herc_building_BZip2 )
    add_dependencies( ${target_name} BZip2 )
    target_include_directories( ${target_name} PRIVATE $<TARGET_PROPERTY:bz2,INTERFACE_INCLUDE_DIRECTORIES> )
endif( )
if( herc_building_PCRE )
    add_dependencies( ${target_name} PCRE )
    target_include_directories( ${target_name} PRIVATE $<TARGET_PROPERTY:pcre,INTERFACE_INCLUDE_DIRECTORIES> )
endif( )
if( herc_building_Zlib )
    add_dependencies( ${target_name} Zlib )
    target_include_directories( ${target_name} PRIVATE $<TARGET_PROPERTY:zlib,INTERFACE_INCLUDE_DIRECTORIES> )
endif( )


# See the notes in Herc60_CreateTargets.cmake for details on the
# need to make implicitly linked shared libraries available to
# the linker.
if( WIN32 OR APPLE )
    target_link_libraries( ${target_name} herc hercu hercs )
endif( )


# Install the shared library, which is included in LIBRARY on non-DLL
# platforms and in RUNTIME on DLL platforms.  (The Windows import
# library is included in ARCHIVE.)

install(    TARGETS ${target_name}
            LIBRARY DESTINATION ${library_rel_dir}
            RUNTIME DESTINATION ${library_rel_dir}
            )


# Include directories for compiles.  This library includes many
# Hercules headers and is very tightly integrated with Hercules.
# The parent binary directory includes config.h, created by the parent
# directory CMakeLists.txt script. The crypto directory precedes the
# source directory to ensure authoritative crypto library headers,
# although crypto and Hercules are so tightly integrated that there
# should be no naming conflicts.

include_directories(
        ${PROJECT_BINARY_DIR}
        ${PROJECT_SOURCE_DIR}/crypto
        ${PROJECT_SOURCE_DIR}
      )


# Link directories.  Dyncrypt needs to link with shared libraries
# created in the Hercules build, so we need to include the parent.

link_directories(
        ${PROJECT_BINARY_DIR}/crypto
        ${PROJECT_BINARY_DIR}
      )


# if building on Windows, add a post-build command to copy the generated
# DLL to the project build directory.

if( WIN32 )
    add_custom_command(
            TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:${target_name}>
                $<TARGET_FILE_DIR:hercules>
            )
endif()



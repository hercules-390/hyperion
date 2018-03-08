# decNumber/CMakeLists.txt - CMake Hercules Build for the decNumber library

#[[   Copyright 2017 by Stephen Orso.

      Distributed under the Boost Software License, Version 1.0.
      See accompanying file BOOST_LICENSE_1_0.txt or copy at
      http://www.boost.org/LICENSE_1_0.txt)
]]
# Build the decNumber shared library.  This build script depends on many
# things being done by the script in the parent directory, including
# CMake minimum version, policy settings, in-source build prevention,
# and variables defining the build environment.


set(target_name "decNumber" )

set( ${target_name}_public_headers
      )

set( ${target_name}_sources
        decContext.h
        decNumber.h
        decNumberLocal.h
        decDPD.h
        decimal128.h
        decimal32.h
        decimal64.h
        decNumberLocal.h
        decPacked.h
        decContext.c
        decimal32.c
        decimal64.c
        decimal128.c
        decNumber.c
        decPacked.c
      )

# include the export definitions file if building under Windows

if( WIN32 )
    set( ${target_name}_sources ${${target_name}_sources} decnumber.def )
endif( )


add_library( ${target_name} SHARED
        ${${target_name}_sources};${${target_name}_public_headers}
        )


# If building on Windows, disable C4244 "conversion from 'x' to 'y',
# possible loss of data"  This needs to be done here because much of
# decNumber does not include config.h, where it could be done as
# a pragma, and because we do not wish to suppress 4244 for all of
# Hercules.

if( WIN32 )
    target_compile_options( ${target_name} PRIVATE /wd4244 )
endif( )


# Install the shared library, which is included in LIBRARY on non-DLL
# platforms and in RUNTIME on DLL platforms.  The Windows import
# library is included in ARCHIVE, which is not required because
# decNumber is consumed by other targets in this script.  If
# decNumber gets built as an external package, then ARCHIVE would be
# needed in the install command.

install(    TARGETS ${target_name}
            LIBRARY DESTINATION ${library_rel_dir}
            RUNTIME DESTINATION ${library_rel_dir}
            )


# Include directories for compiles.  The decNumber library is not
# sensitive to the target system, so it needs no exposure to the tests
# performed by the parent CMakeLists.txt or the config.h generated
# therefrom.

include_directories( ${PROJECT_BINARY_DIR}
                     ${PROJECT_SOURCE_DIR}/decNumber
        )


# Link directories.  There is no need for the decNumber link to reference
# objects outside the decNumber directory.

link_directories( ${PROJECT_BINARY_DIR}/decNumber )


# if building on Windows, add a post-build command to copy the generated
# DLL to the project build directory.  This does not seem to be needed.

if( WIN32 )
    add_custom_command(
            TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:${target_name}>
                $<TARGET_FILE_DIR:hercs>
            )
endif()


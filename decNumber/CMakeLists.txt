# Copyright 2017 by Stephen Orso.  Will be licensed once complete.

# Build the decNumber shared library.  This build script depends on many
# things being done by the script in the parent directory, including
# CMake minimum version, policy settings, in-source build prevention,
# and variables defining the build environment.


set(target_name "decNumber" )

set( ${target_name}_sources
        decContext.c ;
        decimal32.c ;
        decimal64.c ;
        decimal128.c ;
        decNumber.c ;
        decPacked.c
      )

add_library(${target_name} SHARED ${${target_name}_sources} )
install( TARGETS ${target_name} DESTINATION ${library_rel_dir})


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


# if building on Windows, export headers and add a post-build command to
# copy the generated DLL to the project build directory

if( WIN32 )
    add_custom_command(
            TARGET decNumber POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/decNumber.dll
                ${PROJECT_BINARY_DIR}/$<CONFIG>
          )
    generate_export_header( ${target_name}
            BASE_NAME ${target_name}
            EXPORT_MACRO_NAME decNumber_EXPORT
            EXPORT_FILE_NAME decNumber_Export.h
            STATIC_DEFINE decNumber_BUILT_AS_STATIC
          )
endif()
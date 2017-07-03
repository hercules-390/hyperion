# Copyright 2017 by Stephen Orso.  Will be licensed once complete.

# Build the dyncrypt shared library.  This build script depends on many
# things being done by the script in the parent directory, including
# CMake minimum version, policy settings, in-source build prevention,
# and variables defining the build environment.


set(target_name dyncrypt )

set( ${target_name}_sources
        aes.c ;
        des.c ;
        dyncrypt.c ;
        sha1.c ;
        sha256.c
      )

# Add the target for dyncrypt.so.  Because this shared library is
# loaded dynamically by Hercules, not at runtime by the the target
# system's linking loader, we do not wish the "lib" prefix on the
# library name.

add_library(${target_name} SHARED ${${target_name}_sources} )
set_target_properties( ${target_name} PROPERTIES PREFIX "" )
install( TARGETS ${target_name} DESTINATION ${library_rel_dir})


# Include directories for compiles.  This library includes many
# Hercules headers and is very tightly integrated with Hercules.
# The parent binary directory includes config.h, created by the
# parent directory CMakeLists.txt script (eventually, but not now).
# The crypto directory precedes the source directory to ensure
# authoritative crypto library headers, although crypto and Hercules
# are so tightly integrated that there should be no naming conflicts.

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


# if building on Windows, export headers and add a post-build command to
# copy the generated DLL to the project build directory

if( WIN32 )
    generate_export_header( ${target_name}
            BASE_NAME ${target_name}
            EXPORT_MACRO_NAME ${target_name}_EXPORT
            EXPORT_FILE_NAME ${target_name}_Export.h
            STATIC_DEFINE ${target_name}_BUILT_AS_STATIC
        )
    add_custom_command(
            TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/${target_name}.dll
                    ${PROJECT_BINARY_DIR}/$<CONFIG>
        )
endif()
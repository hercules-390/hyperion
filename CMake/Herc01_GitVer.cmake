# Herc01_GitVer.cmake - Collect git repository status for the build

#[[   Copyright 2017 by Stephen Orso.

      Distributed under the Boost Software License, Version 1.0.
      See accompanying file BOOST_LICENSE_1_0.txt or copy at
      http://www.boost.org/LICENSE_1_0.txt)
]]


#[[
  Collect current status of ${GIT_EXECUTABLE} repostory for creation of
  version/mod/patch/commit count/modified

  With thanks to Enrico for prototype code for the following.
]]

#[[
   Arguments -
     - None

   Other Input -
     - Git status as collected by various git command executions
     - commitinfo.h.in, template for the commitinfo.h file
     - CMakeCache.txt, with current values for the following:
         HERC_GIT_BRANCH         - name of the git branch being built
         HERC_GIT_COMMIT_HASH    - git hash of last commit
         HERC_GIT_COMMIT_COUNT   - count of commits made to repository
         HERC_GIT_CHANGED_FILES  - count of files changed since last commit
         HERC_GIT_NEW_FILES      - count of files added since last commit

   Output -
     - Variables in the caller's state:
         GIT_BRANCH         - name of the git branch being built
         GIT_COMMIT_HASH    - git hash of last commit
         GIT_COMMIT_COUNT   - count of commits made to repository
         GIT_CHANGED_FILES  - count of files changed since last commit
         GIT_NEW_FILES      - count of files added since last commit
     - CMakeCache.txt, updated with the cached versions of the above variables.
     - commitinfo.h, but only if the file does not exist or has been changed.

   Return Code -
     - Not set.  Yet....

   Notes -
     - If commitinfo.h does not exist, it is created and variable values cached.
     - If the build is not from a clone of a git repository, a null commitinfo.h
       is created.
     - If a git command line tool cannot be found, a commitinfo.h file is created
       without git status information and an informational message is displayed
       to stdout.  Likewise if the source directory is not a clone of a git
       respository.
]]


# List of directory names to exclude from counts of new and changed files.
# Each directory is rooted at the source directory and must end with a
# forward slash.  CMake deals with Windows versus open source directory
# separators.  Separate each directory name with "|".  This string is used
# as part of a regular expression.

set( EXCLUDE_LIBS "tests/|html/|man/|scripts/|util/" )

# Macro to cache current git status values and create commitinfo.h
macro( create_commitinfo )
# Update CMakeCache.txt with updated git status values
    set( HERC_GIT_BRANCH         ${GIT_BRANCH}         CACHE INTERNAL "Git branch being built"              FORCE )
    set( HERC_GIT_COMMIT_HASH    ${GIT_COMMIT_HASH}    CACHE INTERNAL "Hash of most recent commit"          FORCE )
    set( HERC_GIT_COMMIT_MESSAGE ${GIT_COMMIT_MESSAGE} CACHE INTERNAL "Message from most recent commit"     FORCE )
    set( HERC_GIT_COMMIT_COUNT   ${GIT_COMMIT_COUNT}   CACHE INTERNAL "Count of commits in repo"            FORCE )
    set( HERC_GIT_CHANGED_FILES  ${GIT_CHANGED_FILES}  CACHE INTERNAL "Count of changed files since commit" FORCE )
    set( HERC_GIT_NEW_FILES      ${GIT_NEW_FILES}      CACHE INTERNAL "Count of new files since commit"     FORCE )
# configure commitinfo.h to pass current git status to source
configure_file(
      "${PROJECT_SOURCE_DIR}/CMake/commitinfo.h.in"
      "${PROJECT_BINARY_DIR}/commitinfo.h"
      )
endmacro( create_commitinfo )


# Do we have a shot at filling in the variables?  Check for a .git directory,
# and then check for an installed git command.
if( EXISTS ${CMAKE_SOURCE_DIR}/.git )
    find_package( Git )          # Set GIT_FOUND, GIT_EXECUTABLE, and GIT_VERSION
    if ( NOT GIT_FOUND )
        message(WARNING "Git directory '.git' found but git not installed.  No git status available")
    endif()
else()
    message(WARNING "Git directory '.git' not found.  No git status available")
    set( GIT_FOUND "NO")
    set( GIT_VERSION "None")
    set( GIT_EXECUTABLE "message( ERROR \"Git execution attempted when Git not found\")" )
endif()


# If we are unable to run git status commands, dummy up values and create commitinfo.h
if( NOT GIT_FOUND )
    message( "Not a git clone.  No git status" )
    set( GIT_COMMIT_STAMP "without git status information")
    set( GIT_BRANCH "No-git")
    set( GIT_COMMIT_HASH "unknown")
    set( GIT_COMMIT_MESSAGE "Source directory not a git clone.  ")
    set( GIT_COMMIT_COUNT "0")
    set( GIT_CHANGED_FILES "0")
    set( GIT_NEW_FILES "0")
    create_commitinfo( )
    return()                             # All done...
endif()


# Get the date and time of the most recent commit
execute_process(
    COMMAND ${GIT_EXECUTABLE} log -1 --format="based on the last commit on %ci by %cn"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_STAMP
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
# Remove leading and trailing quotes
separate_arguments( GIT_COMMIT_STAMP UNIX_COMMAND ${GIT_COMMIT_STAMP} )

# Get the current working branch
execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_BRANCH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Get the latest abbreviated commit hash of the working branch
execute_process(
    COMMAND ${GIT_EXECUTABLE} log -1 --format=%h
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
# Get the commit message included with the latest commit to the working branch
execute_process(
    COMMAND ${GIT_EXECUTABLE} log -n 1 --pretty=format:"%s"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_MESSAGE
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Get the count of commits to the current working branch
execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-list ${GIT_BRANCH} --count
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_COUNT
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Get the list of new/changed/deleted files
execute_process(
    COMMAND ${GIT_EXECUTABLE} status --porcelain
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    RESULT_VARIABLE RC
    OUTPUT_VARIABLE NEW_CHANGED_FILES
    OUTPUT_STRIP_TRAILING_WHITESPACE
)


if( NOT ${RC} EQUAL 0 )
    set( GIT_NEW_FILES "invalid" )
    set( GIT_CHANGED_FILES "invalid" )
    message( "Command \"${GIT_EXECUTABLE} status\" in directory ${path} failed with error:\n${error}" )
else()
    if( "${NEW_CHANGED_FILES}" STREQUAL "" )
        set( GIT_NEW_FILES "0" )
        set( GIT_CHANGED_FILES "0" )
    else()
        string(REPLACE "\n" " ;" NEW_CHANGED_FILES ${NEW_CHANGED_FILES})
        foreach(ITEM ${NEW_CHANGED_FILES})
            string(REGEX REPLACE ".*MathFunctions/|${EXCLUDE_LIBS}.*" "" ITEM ${ITEM})
            set(PURE_NC_FILES ${PURE_NC_FILES} ${ITEM})
        endforeach()
	    string(REGEX MATCHALL "\\?\\? |A  " GIT_NEW_FILES ${PURE_NC_FILES} )
        string(REGEX MATCHALL " M " GIT_CHANGED_FILES ${PURE_NC_FILES})
        list(LENGTH GIT_NEW_FILES GIT_NEW_FILES)
	    list(LENGTH GIT_CHANGED_FILES GIT_CHANGED_FILES)
    endif()
endif()


# if the current git signature differs from that which has been cached, recreate commitinfo.h
# and cache the new values.  If commitinfo.h does not exist, create it anyway.

if(       (${HERC_GIT_BRANCH}         MATCHES  ${GIT_BRANCH})
      AND (${HERC_GIT_COMMIT_HASH}    MATCHES  ${GIT_COMMIT_HASH})
      AND (${HERC_GIT_COMMIT_MESSAGE} MATCHES  ${GIT_COMMIT_MESSAGE})
      AND (${HERC_GIT_COMMIT_COUNT}   MATCHES  ${GIT_COMMIT_COUNT})
      AND (${HERC_GIT_CHANGED_FILES}  MATCHES  ${GIT_CHANGED_FILES})
      AND (${HERC_GIT_NEW_FILES}      MATCHES  ${GIT_NEW_FILES})
      )
    set( GIT_DELTA "unchanged:" )
    if( NOT EXISTS ${PROJECT_BINARY_DIR}/commitinfo.h )
        create_commitinfo( )
    endif( )
else( )
    create_commitinfo( )
    set( GIT_DELTA "changed:" )
endif( )

message("Git status ${GIT_DELTA} branch ${GIT_BRANCH}, hash ${GIT_COMMIT_HASH}, commits ${GIT_COMMIT_COUNT}, changed files ${GIT_CHANGED_FILES}, new files ${GIT_NEW_FILES}")


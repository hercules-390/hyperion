# Herc00_Includes.cmake - Hercules build function library.

#[[   Copyright 2017 by Stephen Orso.

      Distributed under the Boost Software License, Version 1.0.
      See accompanying file BOOST_LICENSE_1_0.txt or copy at
      http://www.boost.org/LICENSE_1_0.txt)
]]

#[[
This library includes four functions and one macro that are used frequently
throughout the build.

Functions:
- herc_Define_Executable    - Define a target for an executable program
- herc_Deine_Shared_Lib     - Define a target for a shared library
- herc_Check_Include_Files  - Test for a header and set the HAVE_ variable
- herc_Check_Function       - Test for a function and set the HAVE_ variable
- herc_Check_Struct_Member  - Test a structure for a member and set HAVE_STRUCT
- herc_Check_Symbol_Exists  - Test whether a string exists as a macro or identifier
- herc_Check_C11_Atomics    - Collect lock free status of C11 atomic intrinics
- herc_Check_Packed_Struct  - Determine if packed structs are supported
- herc_Check_Regparm_Works  - Test if __attribute__ ((regparm(3)) works
- herc_Check_Compile_Capability - Test sundry compiler capabilities
- herc_Check_Strict_Aliasing - Test for problems created by strict aliasing
- herc_Check_User_Option_YesNo - Validate a user option as YES/NO/TARGET

Macro:
- herc_Save_Error           - Save error message for presentation at build end.

  --------------------------------------------------------------------  ]]



#[[  ###########   function herc_Save_Error   ###########

This macro accepts a single string as input and updates the associative
array herc_EMessage with that string.  The count of strings stored in
the associative array, herc_Error, is incremented.

The goal of this macro and the array is to accumulate error messages
for presentation as a group at the end of the build.

]]

macro( herc_Save_Error error_message scope)
        math( EXPR index "${herc_Error} + 1" )
        set( herc_EMessage${index} "${error_message}" ${scope} )
        set( herc_Error "${index}" ${scope} )
endmacro( herc_Save_Error )



#[[ ###########   function herc_Define_Executable   ###########

Function/Operation
- Creates a target for an executable and specifies the sources that must
  be compiled to build the executable.
- Specifies for that target the libraries that must be included during
  link.

Input Parameters
- Name of the target executable to be added to the build.
- Semi-colon delimited list of source files that must be compiled and
  build into the executable.
- Semi-colon delimited list of the libraries (other targets) that must be
  used when linking this executable.

Output
- A target defining the executable and its build and link actions.

Notes
- Target link libraries are transitive...if hercu requires hercs and hercu
  is needed to link the target, there is no need to list hercs as a target
  link library.  CMake will figure it out.
]]

function( herc_Define_Executable executable sources libs )
    add_executable( ${executable} ${sources} )
    target_link_libraries( ${executable} ${libs} ${link_alllibs} )
    install(TARGETS ${executable} DESTINATION ${exec_rel_dir} )
endfunction ( herc_Define_Executable )



#[[ ###########   function herc_Define_Shared_Lib   ###########

Function/Operation
- Creates a target for a shared library to be loaded execution.
- Specifies for that target the libraries that must be included during
  link.
- For open source systems, supports libraries loaded by the linking
  loader at program start and libraries explicitly loaded through dlopen().
- For Windows, creates create an export library including all functions
  in the shared library and adds a post-build command to copy the resulting
  DLL to the project binary directory.  This lets the DLL live with its
  calling EXE(s).

Input Parameters
- Name of the target library to be added to the build.
- Semi-colon delimited list of source files that must be compiled and
  build into the library.
- Semi-colon delimited list of the libraries (other targets) that must be
  used when linking this library.
- A flag to indicate whether a shared library or a dynamically loaded
  library is to be built.  If the parameter dynamiclib is set to "dynamic"
  the prefix is set to null, which is the Hercules expectation for
  dynamically-loaded libraries.  This has no impact on Windows, which does
  not have "lib" as an implied prefix.  But on open source, which stores
  shared library xxx as libxxx.so, this matters.

Output
- A target defining the shared library and its build, link, and post-build
  actions.

Notes
- Target link libraries are transitive...if hercu requires hercs and hercu
  is needed to link the target, there is no need to list hercs as a target
  link library.  CMake will figure it out.
]]

function( herc_Define_Shared_Lib libname sources libs dynamiclib)
    add_library( ${libname} SHARED "${sources}" )
    target_link_libraries( ${libname} ${libs} ${link_alllibs})

    if( "${dynamiclib}" STREQUAL "dynamic" )
        SET_TARGET_PROPERTIES( ${libname} PROPERTIES PREFIX "" )
    endif( )

    install( TARGETS ${libname} DESTINATION ${library_rel_dir} )

    if( WIN32 )
        generate_export_header( ${libname}
              BASE_NAME ${libname}
              EXPORT_MACRO_NAME ${libname}_EXPORT
              EXPORT_FILE_NAME ${libname}_Export.h
              STATIC_DEFINE ${libname}_BUILT_AS_STATIC
              )
        add_custom_command(
              TARGET ${libname} POST_BUILD
              COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/${libname}.dll
                    ${PROJECT_BINARY_DIR}/$<CONFIG>
              )
    endif()

endfunction( herc_Define_Shared_Lib )



#[[ ###########   function herc_Check_Include_Files   ###########

Function/Operation
- Check for a header.
- If found, set the HAVE_xxx_H variable.
- If the header is not found, an error message is stored for later
  if the caller identified a missing header as failing the build.
- If the header provided is really a list of ;-separated headers,
  the HAVE_xxx_H variable is derived from the last header name in
  the list.  The preceding headers are prerequisites needed for a
  valid test.

Input Parameters
- Name of the header to be tested for presence.
- A flag indicating whether a missing header is terminal for the build.

Output
- If the header exists, variable HAVE_<header> is defined to 1.
- If the flag is "FAIL" and the header is not found, a message is added
  to the herc_EMessage array and the herc_Error message count is incremented.

Notes
- Absence of certain headers is likely terminal, while absence of other
  headers merely means that certain functions, such as compression, cannot
  be included in the build.  The flag parameter lets the caller inform this
  routine of the distinction.
- Note that the "have" variable name is constructed by this function,
  and testing the "have" variable requires one level of indirection,
  in the form ${${header_varname}}

]]

function( herc_Check_Include_Files header fail_flag )
#             Strip out pre-requisite headers
    string( REGEX REPLACE ".*;" "" header_name "${header}" )
#             Convert invalid characters into underscores, prepend HAVE_
    string( REGEX REPLACE "[./]" "_" header_varname "HAVE_${header_name}" )
#             Force to upper case.
    string( TOUPPER "${header_varname}" header_varname)
    CHECK_INCLUDE_FILES( "${header}" ${header_varname} )
    if( (NOT "${${header_varname}}") AND ("${fail_flag}" STREQUAL "FAIL") )
        herc_Save_Error( "Required header '${header_name}' not found." PARENT_SCOPE )
    endif( )

endfunction ( herc_Check_Include_Files )



#[[ ###########   function herc_Check_Struct_Member   ###########

Function/Operation
- Check a structure in a header to see if it includes a specified
  member.
- If found, set the HAVE_STRUCT_xxx variable.
- If not found, an error message is stored for later if the caller
  identified a missing variable as failing the build.

Input Parameters
- Name of the structure to be tested.  There is no need to include the
  word "struct" in the first parameter.
- Name of the member the structure should contain.
- Name of the header containing the structure to be tested.  CMake
  syntax applies, and semicolon-delimited list of headers can be
  checked if one does not care which header defines the structure.
- A flag indicating whether a missing header is terminal for the build.

Output
- If structure contains the member, variable HAVE_STRUCT_<structname>_<member>
  is defined to 1.
- If the flag is "FAIL" and the member is not found, a message is added
  to the herc_EMessage array and the herc_Error message count is incremented.

Notes
- Absence of certain structure members is likely terminal, while absence of
  others merely means that certain functions, such as compression, cannot
  be included in the build.  The flag parameter lets the caller inform this
  routine of the distinction.
- Note that the "have" variable name is constructed by this function,
  and testing the "have" variable requires one level of indirection,
  in the form ${${struct_mem_varname}}

]]

function( herc_Check_Struct_Member struct mem headers fail_flag )
#             Convert invalid characters into underscores, prepend HAVE_
    string( REGEX REPLACE "[./]" "_" varname "HAVE_STRUCT_${struct}_${mem}" )
#             Force to upper case.
    string( TOUPPER "${varname}" varname)
    CHECK_STRUCT_HAS_MEMBER( "struct ${struct}" "${mem}" "${headers}" ${varname} )
    if( (NOT "${${varname}}") AND ("${fail_flag}" STREQUAL "FAIL" ))
        herc_Save_Error( "Required member '${mem}' not found in structure ${struct}." PARENT_SCOPE )
    endif( )

endfunction ( herc_Check_Struct_Member )



#[[ ###########   function herc_Check_Symbol_Exists   ###########

Function/Operation
- Check for a symbol or macro definition in a public header.
- If found, set the HAVE_STRUCT_xxx variable.
- If not found, an error message is stored for later if the caller
  identified a missing variable as failing the build.

Input Parameters
- Name of the header to be tested for presence.
- List of public headers to be included in the trial compile that
  tests for the existence of the variable.
- A flag indicating whether a missing header is terminal for the build.

Output
- If the symbol exists, variable HAVE_VAR_<varname> is defined to 1.
- If the flag is "FAIL" and the header is not found, a message is added
  to the herc_EMessage array and the herc_Error message count is incremented.

Notes
- Absence of certain identifiers is likely terminal, while absence of
  others merely means that certain functions, such as compression, cannot
  be included in the build.  The flag parameter lets the caller inform this
  routine of the distinction.
- Note that the "have" variable name is constructed by this function,
  and testing the "have" variable requires one level of indirection,
  in the form ${${var_varname}}

]]

function( herc_Check_Symbol_Exists var headers fail_flag )
#             Convert invalid characters into underscores, prepend HAVE_
    string( REGEX REPLACE "[./]" "_" varname "HAVE_DECL_${var}" )
#             Force to upper case.
    string( TOUPPER "${varname}" varname )
    CHECK_SYMBOL_EXISTS( "${var}" "${headers}" ${varname} )
    if( (NOT "${${varname}}") AND ("${fail_flag}" STREQUAL "FAIL" ))
#               Strip out pre-requisite headers for error message
        string( REGEX REPLACE ".*;" "" header_name "${headers}" )
        herc_Save_Error( "Required identifier '${var}' not found in ${header_name}." PARENT_SCOPE )
    endif( )

endfunction ( herc_Check_Symbol_Exists )



#[[ ###########   function herc_Check_Function_Exists   ###########

Function/Operation
- Check for a function.
- If found, set the HAVE_xxx variable.
- If not found and requested by the caller a message is added to the
   herc_EMessage array.

Input Parameters
- Name of the header to be tested for presence.
- A flag indicating whether a missing header is terminal for the build.

Output
- If function exists in any of the libraries specified in CMAKE_REQUIRED_LIBRARIES,
  variable HAVE_<func> is defined to 1.
- If the flag is "FAIL" and the header is not found, a message is added
  to the herc_EMessage array and the herc_Error message count is incremented.

Notes
- Absence of certain functions likely terminal, while absence of other
  functions merely means that certain functions, such as compression, cannot
  be included in the build.
- Note that the "have" variable name is constructed by this function,
  and testing the "have" variable requires one level of indirection,
  in the form ${${header_varname}}

]]

function( herc_Check_Function_Exists func fail_flag )
    string( TOUPPER "HAVE_${func}" func_varname)
    CHECK_FUNCTION_EXISTS( ${func} ${func_varname} )
    if( (NOT "${${func_varname}}") AND ("${fail_flag}" STREQUAL "FAIL" ))
        herc_Save_Error( "Required function '${func}' not found." PARENT_SCOPE )
    endif( )

endfunction ( herc_Check_Function_Exists )



#[[ ###########   function herc_Check_C11_Atomics   ###########

Function/Operation
- Create a CMake include file that contains the lock free status of
  C11 atomic intrinsics.  This file will be included by the caller
  to set these varables for the build.
- A C program is used to create this file because at least one c compiler,
  gcc 4.9.2, does not define the lock free statuses as preprocessor
  macros, but rather as run-time function calls.
- In addition, the variable C11_ATOMICS_AVAILABLE is set to 1 in the parent
  scope to indicate successful completion of the program and creation of the
  include file.

Input Parameters
- None.

Output
- C11_ATOMICS_AVAILABLE set to 1 in the parent scope upon successful
  completion.
- A CMake include file containing set directives to set the values of the
  preprocessor macros that indicate whether C11 atomics are lock free.
- If the test program fails to compile or run, a dummy include file is
  created, the compile or execution listing is displayed, and an error
  messege is saved; saving the message will terminate the build.

Notes
- The c program CMakeHercTestC11Atomic.c is executed by this function.

]]

function( herc_Check_C11_Atomics output_cmake_file )

file( WRITE "${output_cmake_file}" "#  C11 test program failed    " )

try_run(RUN_RESULT_VAR COMPILE_RESULT_VAR
        ${PROJECT_BINARY_DIR}
        SOURCES ${PROJECT_SOURCE_DIR}/CMake/CMakeHercTestC11Atomic.c
        COMPILE_OUTPUT_VARIABLE C11_CompileOutput
        RUN_OUTPUT_VARIABLE C11_RunOutput
        )

if( COMPILE_RESULT_VAR )
    if( ${RUN_RESULT_VAR} STREQUAL "NOT-FOUND" )
        set( C11_RunOutput "Execution of C11 atomics test program failed.  Execution output follows\n" ${C11_RunOutput} )
        herc_Save_Error( "${C11_RunOutput}" PARENT_SCOPE )
    else( )
        set( C11_ATOMICS_AVAILABLE 1 PARENT_SCOPE )
        file( WRITE  "${output_cmake_file}"  ${C11_RunOutput} )
    endif( )
else( )
    set( C11_CompileOutput "Compile of C11 atomics test program failed.  Output follows:\n" ${C11_CompileOutput} )
    herc_Save_Error( "${C11_CompileOutput}" PARENT_SCOPE )
endif( COMPILE_RESULT_VAR )

endfunction ( herc_Check_C11_Atomics )



#[[ ###########   function herc_Check_Struct_Padding   ###########

Function/Operation
- Determine whether the c compiler pads structures to other than a byte
  boundary by running CMakeHercTestStructPadding.c, a c program that
  checks the size of a 1 byte structure and returns zero only if the
  size is 1.
- Hercules requires packed structures to be padded to no more than a
  one-byte boundary.  GCC on ARM can be tickled into doing one-byte
  padding if needed.  Otherwise the caller will have to fail the build.

Input Parameters
- The type of compiler directive needed for packed structures, either
  GCC_STYLE_PACK or MSVC_STYLE_PACK.  This function does not edit the
  parameter for validity, but the c program does.
- The name of a variable to be set in the caller's scope to 1 (true) if
  one-byte padding is in effect and to 0 (false) if not.

Output
- If one-byte padding is detected, the variable named by the caller is
  set to 1 (true).
- If the test program fails to compile or run, an error message is saved
  along with the compile or execution output.  Saving the message will
  terminate the build.

Notes
- The c program CMakeHercTestStructPadding.c is executed by this function.

]]

function( herc_Check_Struct_Padding packing_type result_var )

if( NOT CMAKE_REQUIRED_QUIET )
    message( STATUS "Checking packed structure padding" )
endif( )

try_run(RUN_RESULT_VAR COMPILE_RESULT_VAR
        ${PROJECT_BINARY_DIR}
        SOURCES ${PROJECT_SOURCE_DIR}/CMake/CMakeHercTestStructPadding.c
        COMPILE_DEFINITIONS -D${packing_type}
        COMPILE_OUTPUT_VARIABLE PS_CompileOutput
        RUN_OUTPUT_VARIABLE PS_RunOutput
        )

if( COMPILE_RESULT_VAR )
    if( ${RUN_RESULT_VAR} STREQUAL "NOT-FOUND" )
        set( PS_RunOutput "Execution of packed structure test program failed.  Execution output follows\n" ${PS_RunOutput} )
        herc_Save_Error( "${PS_RunOutput}" PARENT_SCOPE )
    elseif( ${RUN_RESULT_VAR} EQUAL 0 )
        set( ${result_var} 1 PARENT_SCOPE )
    endif( )
else( )
    set( PS_CompileOutput "Compile of packed structure test program failed.  Output follows:\n" ${PS_CompileOutput} )
    herc_Save_Error( "${PS_CompileOutput}" PARENT_SCOPE )
endif( COMPILE_RESULT_VAR )

endfunction ( herc_Check_Struct_Padding )



#[[ ###########   function herc_Check_Packed_Struct   ###########

Function/Operation
- Determine whether the c compiler can create a packed structure by running
  CMakeHercTestPackedStruct.c, a c program that compares the size of a
  structure containing a single int variable with one containing a char
  followed by an int.
- Packed structures are a requirement of Hercules.  If a packed structure
  cannot be created, an error message is issued and the build is failed.

Input Parameters
- The type of compiler directive needed for packed structures, either
  GCC_STYLE_PACK or MSVC_STYLE_PACK.  This function does not edit the
  parameter for validity, but the c program does.
- The name of a variable to be set in the caller's scope to 1 (true) if
  packed structures are supported and to 0 (false) if not.

Output
- If packed structures are supported, the variable named by the caller
  is set to 1 (true).
- If the test program fails to compile or run, an error message is saved
  along with the compile or execution output.  Saving the message will
  terminate the build.

Notes
- The c program CMakeHercTestPackedStruct.c is executed by this function.
- As presently written, this function does not have the capability to
  let the caller make multiple probes of a compiler for an appropriate
  directive to create a packed structure.  It is one and done.

]]

function( herc_Check_Packed_Struct packing_type result_var )

if( NOT CMAKE_REQUIRED_QUIET )
    message( STATUS "Checking for packed structure support" )
endif( )

try_run(RUN_RESULT_VAR COMPILE_RESULT_VAR
        ${PROJECT_BINARY_DIR}
        SOURCES ${PROJECT_SOURCE_DIR}/CMake/CMakeHercTestPackedStruct.c
        COMPILE_DEFINITIONS -D${packing_type}
        COMPILE_OUTPUT_VARIABLE PS_CompileOutput
        RUN_OUTPUT_VARIABLE PS_RunOutput
        )

if( COMPILE_RESULT_VAR )
    if( ${RUN_RESULT_VAR} STREQUAL "NOT-FOUND" )
        set( PS_RunOutput "Execution of packed structure test program failed.  Execution output follows\n" ${PS_RunOutput} )
        herc_Save_Error( "${PS_RunOutput}" PARENT_SCOPE )
    elseif( ${RUN_RESULT_VAR} EQUAL 0 )
        set( ${result_var} 1 PARENT_SCOPE )
    endif( )
else( )
    set( PS_CompileOutput "Compile of packed structure test program failed.  Output follows:\n" ${PS_CompileOutput} )
    herc_Save_Error( "${PS_CompileOutput}" PARENT_SCOPE )
endif( COMPILE_RESULT_VAR )

endfunction ( herc_Check_Packed_Struct )



#[[ ###########   function herc_Check_Regparm_Works   ###########

Function/Operation
- Determine whether the c compiler supports __attribute__ ((regparm(x)))
  and whether that support is broken when using regparm(3).
- This function runs a c program CMakeHercTestRegparm3.c.  If the
  compile fails, then regparm is not supported.  If the compile works
  but the program ends with a non-zero return code, then regparm(x) works
  for values of 0, 1, and 2.  (A value of zero seems silly.)
- __attribute__ ((regparm(x))) is not required to build Hercules.

Input Parameters
- The name of the result variable to be set to 1 if the compiler supports
  __attribute__ ((regparm(x))) for values up to 3.

Output
- The caller provided result variable is set to 1 if regparm works.
- No error or other indication is provided if it does not.

Notes
- The c program CMakeHercTestRegparm3.c is executed by this function.
- Only the Compression Call instructions use regparm(3).  It would be
  reasonable to enhance the build such that if regparm works but not
  for regparm(3), that cmpsc.h be changed to use regparm(2).

]]

function( herc_Check_Regparm_Works result_var )

try_run(RUN_RESULT_VAR COMPILE_RESULT_VAR
        ${PROJECT_BINARY_DIR}
        SOURCES ${PROJECT_SOURCE_DIR}/CMake/CMakeHercTestRegparm3.c
        COMPILE_DEFINITIONS "-O3 -fomit-frame-pointer"
        COMPILE_OUTPUT_VARIABLE R3_CompileOutput
        RUN_OUTPUT_VARIABLE R3_RunOutput
        )

if( COMPILE_RESULT_VAR )
    if( ${RUN_RESULT_VAR} STREQUAL "NOT-FOUND" )
        set( PS_RunOutput "Execution of packed structure test program failed.  Execution output follows\n" ${R3_RunOutput} )
        herc_Save_Error( "${R3_RunOutput}" PARENT_SCOPE )
    elseif( NOT ${RUN_RESULT_VAR} )
        set( ${result_var} 1 PARENT_SCOPE )
    else( )
        set( ${result_var} 0 PARENT_SCOPE )
    endif( )
else( )
    set( ${result_var} 0 PARENT_SCOPE )
endif( COMPILE_RESULT_VAR )

endfunction ( herc_Check_Regparm_Works )



#[[ ###########   function herc_Check_Compile_Capability   ###########

Function/Operation
- Determine whether the c compiler can compile a (short) c program provided
  by the caller without generating any errors or warnings.
- The program is provided by the caller in a variable.
- A variable provided by the caller is set to 1 if compilation generates
  neither errors nor warnings and undefined otherwise.

Input Parameters
- A variable containing the c program to be compiled.
- The name of the result variable to be set/unset based on the result
  of the compilation.
- A flag indicating whether success (has the capability) is indicated
  by a clean compilation (TRUE) or by an error or warning message (FALSE).

Output
- If the c program compiles cleanly, the variable provided by the caller
  is set to 1.
- None if errors or warnings were generated by the compilation.

Notes
- If the code submitted for compilation or the results need to be examined,
  these can be found in the CMakeError.log and/or the CMakeOutput.log files
  in the CMakeFiles directory.

]]

function( herc_Check_Compile_Capability program_text return_var want_clean )

file( WRITE ${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/HercTempProg.c "${program_text}" )

try_compile( ${return_var} ${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}
            SOURCES ${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/HercTempProg.c
            COMPILE_DEFINITIONS "-Wall -Werror"
            OUTPUT_VARIABLE tc_output_var
        )

if( ${want_clean} AND ${return_var} )      # success means clean compile
    set( ${return_var} 1 PARENT_SCOPE )    # Change TRUE to 1 for autotools compatibility.
elseif( NOT (${want_clean} OR ${return_var}) )  # sucess means a return code
    set( ${return_var} 1 PARENT_SCOPE )    # Change TRUE to 1 for autotools compatibility.
else( )
    set( ${return_var} 0 PARENT_SCOPE )    # failed.
endif( )

file( REMOVE ${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/HercTempProg.c )

endfunction ( herc_Check_Compile_Capability )



#[[ ###########   function herc_Check_Strict_Aliasing   ###########

Function/Operation
- Determine whether the c compiler uses strict aliasing.  This test
  is taken from the configure.ac formerly used for Hercules.

Input Parameters
- None

Output
- The result_var parameter is set if the compiler does strict aliasing.
- If the test program fails to compile or run, an error message is saved
  along with the compile or execution output.  Saving the message will
  terminate the build.

Notes
- The c program CMakeHercTestStrictAliasing.c is executed by this function.

]]

function( herc_Check_Strict_Aliasing result_var )

try_run(RUN_RESULT_VAR COMPILE_RESULT_VAR
        ${PROJECT_BINARY_DIR}
        SOURCES ${PROJECT_SOURCE_DIR}/CMake/CMakeHercTestStrictAliasing.c
        COMPILE_DEFINITIONS -O3 -fstrict-aliasing
        COMPILE_OUTPUT_VARIABLE SA_CompileOutput
        RUN_OUTPUT_VARIABLE SA_RunOutput
        )

if( COMPILE_RESULT_VAR )
    if( ${RUN_RESULT_VAR} STREQUAL "NOT-FOUND" )
        set( PS_RunOutput "Execution of strict aliasing test program failed.  Execution output follows\n" ${SA_RunOutput} )
        herc_Save_Error( "${SA_RunOutput}" PARENT_SCOPE )
    elseif( NOT ${RUN_RESULT_VAR} )
        set( ${return_var} 1 PARENT_SCOPE )      # compiler strict aliasing should not be a problem
    else( )
        set( ${return_var} 0 PARENT_SCOPE )      # compiler strict aliasing is a problem
    endif( )
else( )
    set( PS_CompileOutput "Compile of strict aliasing test program failed.  Output follows:\n" ${SA_CompileOutput} )
    herc_Save_Error( "${SA_CompileOutput}" PARENT_SCOPE )
endif( COMPILE_RESULT_VAR )

endfunction ( herc_Check_Strict_Aliasing )


#[[ ###########   function herc_Check_User_Option_YesNo   ###########

Function/Operation
- Validate a Yes/No user option,  Valid values are YES, NO, TARGET, or
  the null string.  The test is case insensitive and values may be
  abbreviated to their first character.
- If a valid value is provided, it is converted to canonical form (YES, NO,
  or null).  An input of TARGET is converted to the null string.
- If the user specified other than YES, NO, or TARGET for a Yes/No option,
  and set the fail_flag to true, store an error message for later issuance.
  Storing the error message will fail the build.
- If an option is null, it was not specified, and this function returns
  to the caller without performing any actions.

Input Parameters
- The name of the Yes/No variable to be validated.
- A flag indicating whether a value of other than YES or NO is allowed.
  Most options are pure YES|NO options, while some permit a value instead
  of YES|NO.

Output
- If valid, and a synonym for Yes or No, the variable is canonicallized to
  YES or NO.
- If invalid and fail_flag is TRUE, an error message is saved in the
  herc_EMessage array, and as a result, the build is failed.

Notes
- It is up to the balance of the build to examine the user option variable
  if a buildWith_ option is not supported by the target.  If the user did
  not specify the option, the subject feature should be excluded from the
  build without comment.  If it was explicitly requested, then the build
  is expected to issue a message.

]]

function( herc_Check_User_Option_YesNo option_name fail_flag )

if( DEFINED ${option_name} )
    if ( NOT ("${${option_name}}" STREQUAL "") )
        STRING( TOUPPER "${${option_name}}" option_value )
        if( ("${option_value}" STREQUAL "YES") OR ("${option_value}" STREQUAL "Y") )
            set( ${option_name} YES CACHE STRING "${help_Sumry_${option_name}}" FORCE)
        elseif(("${option_value}" STREQUAL "NO") OR ("${option_value}" STREQUAL "N") )
            set( ${option_name} NO CACHE INTERNAL "${help_Sumry_${option_name}}" FORCE )
        elseif(("${option_value}" STREQUAL "TARGET") OR ("${option_value}" STREQUAL "T") )
            set( ${option_name} "" CACHE INTERNAL "${help_Sumry_${option_name}}" FORCE )
        elseif( "${fail_flag}" STREQUAL "FAIL" )
            herc_Save_Error("Invalid value \"${${option_name}}\" for ${option_name}, not YES , NO, or TARGET" PARENT_SCOPE )
        endif( )
    endif( )
endif( )

endfunction( herc_Check_User_Option_YesNo )


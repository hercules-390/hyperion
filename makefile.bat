@echo off

rem -------------------------------------------------------------------
rem
rem                         MAKEFILE.BAT
rem
rem  If this batch file works okay, then it was created by Fish.
rem  If it doesn't, then I don't know who created it! ;-)
rem
rem  Designed to be called either from the command line or by a
rem  Visual Studio makefile project with the "Build command line"
rem  set to:
rem
rem               makefile.bat  %1  %2  %3  %4
rem
rem  Where:
rem
rem   %1   DEBUG or RETAIL (used in 'setenv' call)        ((REQUIRED))
rem
rem   %2   name of makefile to use (makefile-dllmod.msvc) ((REQUIRED))
rem
rem   %3   MAX_CPU_ENGINES value ( 2 to 32 )              ((REQUIRED))
rem
rem   %4   extra nmake parameter (usually '-a' to force   ((OPTIONAL))
rem                               rebuild of all targets
rem                               or 'clean' to clean up
rem                               all build directories)
rem
rem -------------------------------------------------------------------
rem  Additional nmake arguments (for reference):
rem
rem   -nologo   suppress copyright banner
rem   -s        silent (suppress commands echoing)
rem   -k        keep going if error(s)
rem   -g        display !INCLUDEd files  (apparently VS 8.0 only)
rem
rem -------------------------------------------------------------------
rem
rem                      *** CHANGE HISTORY ***
rem
rem  07/31/05  Fish  Temporarily remove -s (slient) option to see the
rem                  actual options being passed to compiler / linker
rem  07/31/05  Fish  Done; putting -s (slient) option back. (I only
rem                  documented this silliness only so i could start
rem                  with a non-empty change history section. <grin>
rem  09/01/05  Fish  '-g' option apparently only for VS 8.0
rem  11/17/05  Fish  Support for building within DevStudio 6.0
rem                  whenever VS 8.0 is also installed.
rem  11/29/05  Fish  Added comments re: 'ASSEMBLY_LISTINGS' option
rem
rem -------------------------------------------------------------------

if "%1" == "" (
    echo "Format: makefile.bat DEBUG|RETAIL <makefile-name> <#of_cpu_engines> [-a|clean]"
    goto :EOF
)

rem  NOTE: 'MSSdk' not normally defined for most users,
rem  but it IS defined if Visual Studio is installed...

if not "%VS80COMNTOOLS%" == "" (

    rem note "vSvars.bat", not "vCvars.bat"!

    call "%VS80COMNTOOLS%vsvars32.bat"

) else if not "%MSSdk%" == "" (

    rem 'MSSdk' ends with "\." (backslash dot)
    rem echo *** MSSdk ***

    call "%MSSdk%\setenv" /XP32 /%1

) else if not "%VCToolkitInstallDir%" == "" (

    rem 'VCToolkitInstallDir' does NOT end with backslash!
    rem echo *** VCToolkitInstallDir ***

    call "%VCToolkitInstallDir%vcvars32.bat"
    call "%VCToolkitInstallDir%setenv" /XP32 /%1

) else (

    echo ABORT: Neither 'VCToolkit' nor 'Platform SDK' is installed!
    goto :EOF
)

set CFG=%1
set MAX_CPU_ENGINES=%3

rem
rem Uncomment the below to ALWAYS generate assembly (.cod) listings, -OR-
rem define it yourself with 'set' command before invoking this batch file
rem
rem set ASSEMBLY_LISTINGS=1


if "%VS80COMNTOOLS%" == "" (

    nmake -nologo -s %4 -f %2
    
) else (

    nmake -nologo -s %4 -f %2 -g
)

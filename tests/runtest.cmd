@if defined TRACEON (@echo on) else (@echo off)

  REM  If this batch file works, then it was written by Fish.
  REM  If it doesn't then I don't know who the heck wrote it.

  goto :init


::-----------------------------------------------------------------------------
::                              RUNTEST.CMD
::-----------------------------------------------------------------------------
:help

  echo.
  echo     NAME
  echo.
  echo         %~n0:    Run Hercules automated test(s).
  echo.
  echo     SYNOPSIS
  echo.
  echo         %~n0     [tstname]  [64 ^| 32]  [build]
  echo.
  echo     ARGUMENTS
  echo.
  echo         tstname     The specific test to be run. If specified, only
  echo                     the base filename should be given; the filename
  echo                     extension of .tst is presumed. If not specified
  echo                     then all *.tst tests will be run.
  echo.
  echo         64,32       Which CPU architecture of Hercules binaries
  echo                     should be used: the 64-bit version or the 32-
  echo                     bit version. Optional. The default is 64-bit.
  echo.
  echo         build       Either 'DEBUG' or 'RETAIL' indicating which build
  echo                     of Hercules should be used: the UNoptimized Debug
  echo                     build or the optimized Release build.  Optional.
  echo                     The default is to use the optimized Release build.
  echo.
  echo     NOTES
  echo.
  echo         %~nx0 requires both %tool1% and %tool2% to be installed.
  echo         %~n0 expects to be run from within the 'tests' subdirectory
  echo         where %~nx0 itself and all of the *.tst files exist.
  echo.
  echo     EXIT STATUS
  echo.
  echo         n           Number of tests which have failed: 0 if all
  echo                     tests passed (success). Non-zero otherwise.
  echo.
  echo     AUTHOR
  echo.
  echo         "Fish"      (David B. Trout)
  echo.
  echo     VERSION
  echo.
  echo         1.2         (October 12, 2015)
  echo.

  set /a "rc=1"
  %exit%


::-----------------------------------------------------------------------------
::                               INIT
::-----------------------------------------------------------------------------
:init

  setlocal
  pushd .

  set "tool1=rexx.exe"
  set "tool2=sed.exe"

  set "return=goto :EOF"
  set "exit=goto :exit"
  set "TRACE=if defined DEBUG echo"
  set "rc=0"

  set "wfn=allTests"

  goto :begin


::-----------------------------------------------------------------------------
::                                EXIT
::-----------------------------------------------------------------------------
:exit

  popd
  endlocal & exit /b %rc%


::-----------------------------------------------------------------------------
::                                BEGIN
::-----------------------------------------------------------------------------
:begin

  if /i "%~1" == "?"       goto :help
  if /i "%~1" == "/?"      goto :help
  if /i "%~1" == "-?"      goto :help
  if /i "%~1" == "-h"      goto :help
  if /i "%~1" == "--help"  goto :help

  call :load_tools
  if not "%rc%" == "0" %exit%


  if "%*" == "" (
    echo Begin: "%~n0" ...
  ) else (
    echo Begin: "%~n0 %*" ...
  )
  goto :parse_args


::-----------------------------------------------------------------------------
::                             load_tools
::-----------------------------------------------------------------------------
:load_tools

  call :findtool "%tool1%"
  if not defined # (
    echo ERROR: "%tool1%" not found.       1>&2
    echo INFO:  Use "%~n0 /?" to get help. 1>&2
    set /a "rc=1"
  )

  call :findtool "%tool2%"
  if not defined # (
    echo ERROR: "%tool2%" not found.       1>&2
    echo INFO:  Use "%~n0 /?" to get help. 1>&2
    set /a "rc=1"
  )

  %return%


::-----------------------------------------------------------------------------
::                             findtool
::-----------------------------------------------------------------------------
:findtool

  set "@=%path%"
  set "path=.;%path%"
  set "#=%~$PATH:1"
  set "path=%@%"
  %return%


::-----------------------------------------------------------------------------
::                             isdir
::-----------------------------------------------------------------------------
:isdir

  if not exist "%~1" (
    set "isdir="
    %return%
  )
  set "isdir=%~a1"
  if defined isdir (
    if /i not "%isdir:~0,1%" == "d" set "isdir="
  )
  %return%


::-----------------------------------------------------------------------------
::                  Parse arg 1:  which test ro run
::-----------------------------------------------------------------------------
:parse_args

  set "tstname=%~1"
  if not exist "%tstname%*.tst" (
    echo ERROR: Test^(s^) "%tstname%*.tst" not found. 1>&2
    echo INFO:  Use "%~n0 /?" to get help.            1>&2
    %exit%
  )


::-----------------------------------------------------------------------------
::    Parse arg 3:  retail/debug build option (only first char is checked)
::-----------------------------------------------------------------------------

  set "dbg=%~3"
  if not "%dbg%" == "" (
    if /i not "%dbg:~0,1%" == "d" (
      if /i not "%dbg:~0,1%" == "r" (
        echo ERROR: Argument 3 must be either Retail or Debug. 1>&2
        echo INFO:  Use "%~n0 /?" to get help.                 1>&2
        %exit%
      )
    )
    if /i "%dbg:~0,1%" == "d" set "dbg=debug."
    if /i "%dbg:~0,1%" == "r" set "dbg="
  )


::-----------------------------------------------------------------------------
::    Parse arg 2:  which herc binaries directory (32-bit or 64-bit)
::-----------------------------------------------------------------------------

  set "bitness=%~2"
  if "%bitness%" == "" set "bitness=64"
  if "%bitness%" == "64" (
    set "hbindir=msvc.%dbg%AMD64.bin"
  ) else (
    if "%bitness%" == "32" (
      set "hbindir=msvc.%dbg%dllmod.bin"
    ) else (
      echo ERROR: Argument 2 must be either 64 or 32. 1>&2
      echo INFO   Use "%~n0 /?" to get help.          1>&2
      %exit%
    )
  )
  call :isdir "..\%hbindir%"
  if not defined isdir (
    if defined dbg set "xxx=debug "
    echo ERROR: %bitness%-bit %xxx%directory "..\%hbindir%" does not exist. 1>&2
    echo INFO:  Use "%~n0 /?" to get help. 1>&2
    %exit%
  )


::-----------------------------------------------------------------------------
::            Delete any leftover work files from previous run
::-----------------------------------------------------------------------------

  if exist %wfn%.tst  del /f %wfn%.tst
  if exist %wfn%.rc   del /f %wfn%.rc
  if exist %wfn%.out  del /f %wfn%.out
  if exist %wfn%.sout del /f %wfn%.sout


::-----------------------------------------------------------------------------
::   Build test script consisting of all *.tst files concatenated together
::-----------------------------------------------------------------------------

  for %%a in (%tstname%*.tst) do type "%%a" >> %wfn%.tst
  echo pause 0.1 >> %wfn%.tst
  echo exit >> %wfn%.tst


::-----------------------------------------------------------------------------
::        Build startup .rc file which invokes the test script
::-----------------------------------------------------------------------------

  echo msglevel -debug  >> %wfn%.rc
  echo t+               >> %wfn%.rc
  echo script %wfn%.tst >> %wfn%.rc


::-----------------------------------------------------------------------------
::    Start Hercules using specified .rc file which calls our test script
::-----------------------------------------------------------------------------

  ..\%hbindir%\hercules.exe -f tests.conf -r %wfn%.rc > %wfn%.out 2>&1


::-----------------------------------------------------------------------------
::          Select only the logfile lines we are interested in
::-----------------------------------------------------------------------------

  sed.exe -n -e "/^HHC01603I \*Testcase/,/^HHC01603I \*Done/p" > %wfn%.sout %wfn%.out


::-----------------------------------------------------------------------------
:: Call rexx helper script to parse the selected log lines and generate report
::-----------------------------------------------------------------------------

  rexx.exe redtest.rexx %wfn%.sout
  set /a "rc=%errorlevel%"

  if %rc% EQU 0 (
    echo End: ** Success! **
  ) else (
    echo End: ** FAILURE! **
  )

::-----------------------------------------------------------------------------
::                              ALL DONE!
::-----------------------------------------------------------------------------

  %exit%

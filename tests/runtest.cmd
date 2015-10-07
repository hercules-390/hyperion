@if defined TRACEON (@echo on) else (@echo off)

  REM Print what arguments were passed to us
  echo %*

::-----------------------------------------------------------------------------
::                              runtest.cmd
::-----------------------------------------------------------------------------
::
::  Arg #1 (required) is the required name of the base Hercules directory.
::
::  Arg #2 (optional) is either 32 or 64 indicating which binaries directory
::                    to use (i.e. 32-bit or 64-bit) The default is 64-bit.
::
::  Arg #3 (optional) is 'debug' to indicate the non-optimized debug version
::                    of Hercules should be used rather than the optimized
::                    version. The default is to use the optimized version.
::
::-----------------------------------------------------------------------------

  setlocal
  pushd .

  set "tool1=sed.exe"
  set "tool2=rexx.exe"

  set "wfn=allTests"
  set "return=goto :EOF"
  set "exit=goto :exit"
  set "rc=0"

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

  call :load_tools
  if not "%rc%" == "0" %exit%

  goto :parse_args


::-----------------------------------------------------------------------------
::                             load_tools
::-----------------------------------------------------------------------------
:load_tools

  call :findtool "%tool1%"
  if not defined # (
    echo ERROR: "%tool1%" not found. 1>&2
    set /a "rc=1"
  )

  call :findtool "%tool2%"
  if not defined # (
    echo ERROR: "%tool2%" not found. 1>&2
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
::                  Parse arg 1:  base hercules directory
::-----------------------------------------------------------------------------
:parse_args

  set "hercdir=%~a1"
  if defined hercdir (
    if /i not "%hercdir:~0,1%" == "d" set "hercdir="
  )
  if not defined hercdir (
    echo ERROR: Argument 1 must be the name of the base Hercules directory.
    %exit%
  )
  set "hercdir=%~1"


::-----------------------------------------------------------------------------
::    Parse arg 3:  debug option (only first character is checked)
::-----------------------------------------------------------------------------

  set "debug=%~3"
  if not "%debug%" == "" (
    if /i not "%debug:~0,1%" == "d" (
      echo ERROR: Argument 3 must be either debug or unspecified.
      %exit%
    )
    set "debug=debug."
  )

::-----------------------------------------------------------------------------
::    Parse arg 2:  which herc binaries directory (32-bit or 64-bit)
::-----------------------------------------------------------------------------

  set "hbindir=%~2"
  if "%hbindir%" == "" set "hbindir=64"
  if "%hbindir%" == "64" (
    set "hbindir=msvc.%debug%AMD64.bin"
  ) else (
    if "%hbindir%" == "32" (
      set "hbindir=msvc.%debug%dllmod.bin"
    ) else (
      echo ERROR: Argument 2 must be either 32 or 64.
      %exit%
    )
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

  for %%a in (%hercdir%\tests\*.tst) do type "%%a" >> %wfn%.tst
  echo pause 1 >> %wfn%.tst
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

  %hercdir%\%hbindir%\hercules.exe -f %hercdir%\tests\tests.conf -r %wfn%.rc > %wfn%.out


::-----------------------------------------------------------------------------
::          Select only the logfile lines we are interested in
::-----------------------------------------------------------------------------

  sed.exe -n -e "/^HHC01603I \*Testcase/,/^HHC01603I \*Done/p" > %wfn%.sout %wfn%.out


::-----------------------------------------------------------------------------
:: Call rexx helper script to parse the selected log lines and generate report
::-----------------------------------------------------------------------------

  rexx.exe %hercdir%\tests\redtest.rexx %wfn%.sout


::-----------------------------------------------------------------------------
::                              ALL DONE!
::-----------------------------------------------------------------------------

  %exit%

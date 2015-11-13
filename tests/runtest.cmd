@if defined TRACEON (@echo on) else (@echo off)

  REM  If this batch file works, then it was written by Fish.
  REM  If it doesn't then I don't know who the heck wrote it.

  goto :init


::-----------------------------------------------------------------------------
::                            RUNTEST.CMD
::-----------------------------------------------------------------------------
:help

  echo.
  echo     NAME
  echo.
  echo         %~n0:    Run Hercules automated test(s).
  echo.
  echo     SYNOPSIS
  echo.
  echo         %~n0     [tstname]  [64 ^| 32 ^| ..]  [build]
  echo                     [/r [rpts [fails]]]  [/t factor]
  echo.
  echo     ARGUMENTS
  echo.
  echo         tstname     The specific test to be run. If specified, only
  echo                     the base filename should be given; the filename
  echo                     extension of .tst is presumed. If not specified
  echo                     then all *.tst tests will be run.
  echo.
  echo         64,32,..    Which host architecture of Hercules binaries
  echo                     should be used: the 64-bit version, the 32-bit
  echo                     version, or , if '..' is specified, whichever
  echo                     version resides in the parent directory. The
  echo                     default is 64.
  echo.
  echo         build       Either 'DEBUG' or 'RETAIL' indicating which build
  echo                     of Hercules should be used: the UNoptimized Debug
  echo                     build or the optimized Release build.  Optional.
  echo                     The default is to use the optimized Release build
  echo                     for 64- or 32-bit binaries. This option is ignored
  echo                     if '..' is specified for the previous option.
  echo.
  echo         /r          Optional repeat switch. When specified, %~n0
  echo                     will re-run the specified test^(s^) rpts times or
  echo                     until fails failures occur. The default if neither
  echo                     is specified is %defrpts% repeats or %deffail% failures.
  echo.
  echo         /t factor   Is an optional test timeout factor between 1.0
  echo                     and %mttof%. The test timeout factor is used to
  echo                     increase each test scripts' timeout value so as
  echo                     to compensate for the speed of your own system
  echo                     compared to the speed of the system the tests
  echo                     were designed for.
  echo.
  echo                     Use a value greater than 1.0 on slower systems
  echo                     to give each test a slightly longer period of
  echo                     time to complete.
  echo.
  echo                     Timeout values (specified as an optional value on
  echo                     the special 'runtest' test script command) are a
  echo                     safety feature designed to prevent runaway tests
  echo                     from running forever. Normally tests always end
  echo                     automatically the very moment they are done.
  echo.
  echo     NOTES
  echo.
  echo         %~nx0 requires %tool1% to be installed and expects to
  echo         be run from within the 'tests' subdirectory where %~nx0
  echo         itself and all of its *.tst test files are.
  echo.
  echo         The 'tests' subdirectory is expected to be a subdirectory
  echo         of either the main Hercules source code directory (for those
  echo         who build Hercules for themselves) or a subdirectory of the
  echo         installed binaries directory (for those who use pre-built
  echo         binaries). For those using pre-built binaries ".." should
  echo         be specified as the second argument (see further above).
  echo.
  echo         %~nx0 uses "%wfn%" as the base name for all of its
  echo         work files. Therefore all "%wfn%.*" files in the current
  echo         directory (usually 'tests'; see further above) are subject
  echo         to being deleted as a result of invoking %~n0.
  echo.
  echo         The results of the tests run are directed into a Hercules
  echo         logfile called %wfn%.out. If any test fails you should
  echo         begin your investigation there.
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
  echo         2.0         (November 8, 2015)
  echo.

  set "hlp=1"
  set /a "rc=1"
  %exit%


::-----------------------------------------------------------------------------
::                              INIT
::-----------------------------------------------------------------------------
:init

  setlocal
  pushd .

  set "TRACE=if defined DEBUG echo"
  set "return=goto :EOF"
  set "exit=goto :exit"
  set "maxrc=0"
  set "rc=0"
  set "hlp="

  set "tool1=rexx.exe"

  set "wfn=allTests"
  set "cfg=tests.conf"
  set "ttof="
  call :calc_mttof

  set "tstname="
  set "bitness="
  set "build="
  set "repeat="
  set "maxruns="
  set "maxfail="

  set "deftstname=*"
  set "defbitness=64"
  set "defbuild=retail"
  set "defrepeat="
  set "defrpts=100"
  set "deffail=5"

  set "cmdargs=%*"
  goto :parse_args


::-----------------------------------------------------------------------------
::                           calc_mttof
::-----------------------------------------------------------------------------
:calc_mttof

  @REM 'mttof' is the maximum test timeout factor (runtest script statement)
  @REM 'msto' is the maximum scripting timeout (i.e. script pause statement)

  setlocal
  set "mttof="
  set "msto=300.0"
  echo WScript.Echo Eval(WScript.Arguments(0)) > %wfn%.vbs
  for /f %%i in ('cscript //nologo %wfn%.vbs "(((4.0 * 1024.0 * 1024.0 * 1024.0) - 1.0) / 1000000.0 / %msto%)"') do set mttof=%%i
  for /f %%i in ('cscript //nologo %wfn%.vbs "Int(10.0 * %mttof%) / 10.0"') do set mttof=%%i
  del %wfn%.vbs
  endlocal && set "mttof=%mttof%"
  %return%


::-----------------------------------------------------------------------------
::                           load_tools
::-----------------------------------------------------------------------------
:load_tools

  call :findtool "%tool1%"
  if not defined # (
    echo ERROR: required tool "%tool1%" not found. 1>&2
    set /a "rc=1"
  )

  %return%


::-----------------------------------------------------------------------------
::                           findtool
::-----------------------------------------------------------------------------
:findtool

  set "@=%path%"
  set "path=.;%path%"
  set "#=%~$PATH:1"
  set "path=%@%"
  %return%


::-----------------------------------------------------------------------------
::                            isfile
::-----------------------------------------------------------------------------
:isfile

  if not exist "%~1" (
    set "isfile="
    %return%
  )
  set "isfile=%~a1"
  if defined isfile (
    if /i "%isfile:~0,1%" == "d" set "isfile="
  )
  %return%


::-----------------------------------------------------------------------------
::                            isdir
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
::                            isnum
::-----------------------------------------------------------------------------
:isnum

  set "@=%~1"
  set "isnum="
  for /f "delims=0123456789" %%i in ("%@%\") do if "%%i" == "\" set "isnum=1"
  %return%


::-----------------------------------------------------------------------------
::                           parse_args
::-----------------------------------------------------------------------------
:parse_args

  set /a "rc=0"

  if /i "%~1" == ""        goto :help
  if /i "%~1" == "?"       goto :help
  if /i "%~1" == "/?"      goto :help
  if /i "%~1" == "-?"      goto :help
  if /i "%~1" == "-h"      goto :help
  if /i "%~1" == "--help"  goto :help

  call :load_tools
  if not "%rc%" == "0" %exit%

  goto :parse_options_loop


::-----------------------------------------------------------------------------
::                        parse_options_loop
::-----------------------------------------------------------------------------
:parse_options_loop

  if "%~1" == "" goto :options_loop_end

  set "opt=%~1"
  set "optflag=%opt:~0,1%"
  set "optname=%opt:~1%"

  shift /1

  if "%optflag%" == "-" goto :parse_opt
  if "%optflag%" == "/" goto :parse_opt

  if defined tstname goto :parse_got_tstname
  set       "tstname=%opt%"
  goto :parse_options_loop

:parse_got_tstname

  if defined bitness goto :parse_got_bitness
  set       "bitness=%opt%"
  goto :parse_options_loop

:parse_got_bitness

  if defined build goto :parse_got_build
  set       "build=%opt%"
  goto :parse_options_loop

:parse_got_build

  if not defined repeat goto :parse_bad_opt
  if defined maxruns goto :parse_got_maxruns
  set       "maxruns=%opt%"
  goto :parse_options_loop

:parse_got_maxruns

  if not defined repeat goto :parse_bad_opt
  if defined maxfail goto :parse_got_maxfail
  set       "maxfail=%opt%"
  goto :parse_options_loop

:parse_got_maxfail

  goto :parse_bad_opt

:parse_bad_opt

  echo ERROR: Unknown/unsupported argument '%opt%'. 1>&2
  set /a "rc=1"
  goto :parse_options_loop

:parse_opt

  if /i "%optname:~0,1%" == "r" goto :parse_repeat_opt
  if /i "%optname:~0,1%" == "t" goto :parse_ttof_opt

  echo ERROR: Unrecognized option '%opt%' 1>&2
  set /a "rc=1"
  goto :parse_options_loop

:parse_repeat_opt

  set "repeat=/r"
  goto :parse_options_loop

:parse_ttof_opt

  set "ttof=%~1"
  shift /1
  goto :parse_options_loop

:options_loop_end

  goto :validate_args


::-----------------------------------------------------------------------------
::                          validate_args
::-----------------------------------------------------------------------------
:validate_args

  if not defined tstname set "tstname=%deftstname%"
  if not defined bitness set "bitness=%defbitness%"
  if not defined build   set "build=%defbuild%"
  if not defined repeat  set "repeat=%defrepeat%"
  if not defined maxruns set "maxruns=%defrpts%"
  if not defined maxfail set "maxfail=%deffail%"


  @REM Validate which test(s) to run

  if not exist "%tstname%*.tst" (
    echo ERROR: Test^(s^) "%tstname%*.tst" not found. 1>&2
    set /a "rc=1"
  )


  @REM Validate retail/debug build option (only first char is checked)

  set "dbg=%build%"
  if defined dbg (
    if /i not "%dbg:~0,1%" == "d" (
      if /i not "%dbg:~0,1%" == "r" (
        echo ERROR: Build must be either 'Retail' or 'Debug'. 1>&2
        set /a "rc=1"
      )
    )
    if /i "%dbg:~0,1%" == "d" set "dbg=debug."
    if /i "%dbg:~0,1%" == "r" set "dbg="
  )


  @REM Validate which herc binaries to use (32-bit, 64-bit or .. parent)

  if "%bitness%" == "64" (
    set "hbindir=msvc.%dbg%AMD64.bin"
  ) else (
    if "%bitness%" == "32" (
      set "hbindir=msvc.%dbg%dllmod.bin"
    ) else (
      if "%bitness%" == ".." (
        set "hbindir="
        set "dbg="
      ) else (
        echo ERROR: Which binaries to use must be either '64', '32' or '..' 1>&2
        set /a "rc=1"
      )
    )
  )

  if defined hbindir (
    call :isdir "..\%hbindir%"
    if not defined isdir (
      if defined dbg set "xxx=debug "
      echo ERROR: %bitness%-bit %xxx%directory "..\%hbindir%" does not exist. 1>&2
      set /a "rc=1"
    ) else (
      set "hbindir=%hbindir%\"
    )
  )

  call :isfile "..\%hbindir%hercules.exe"
  if not defined isfile (
    echo ERROR: Hercules executable "..\%hbindir%hercules.exe" not found. 1>&2
    set /a "rc=1"
  )


  @REM Validate repeat option arguments

  if not defined repeat goto :skip

  call :isnum "%maxruns%"
  if not defined isnum (
    echo ERROR: Repeats value '%maxruns%' is not numeric. 1>&2
    set /a "rc=1"
  )

  call :isnum "%maxfail%"
  if not defined isnum (
    echo ERROR: Fails value '%maxfail%' is not numeric. 1>&2
    set /a "rc=1"
  )
:skip


  @REM Validate /t factor option

  if not defined ttof goto :skip

  echo If %ttof% ^< 1.0 Or %ttof% ^> %mttof% Then   >  %wfn%.vbs
  echo Wscript.Echo "0"                             >> %wfn%.vbs
  echo Else                                         >> %wfn%.vbs
  echo Wscript.Echo "1"                             >> %wfn%.vbs
  echo End If                                       >> %wfn%.vbs
  
  for /f %%i in ('cscript //nologo %wfn%.vbs 2^>^&1') do set ok=%%i
  del %wfn%.vbs
  %TRACE% "mttof=%mttof%, ttof=%ttof%: ok=%ok%"

  if not "%ok%" == "1" (
    echo ERROR: Test timeout factor '%ttof%' not within valid range of 1.0 to %mttof%. 1>&2
    set /a "rc=1"
  )
:skip


  @REM Add validation of other arguments here...

  goto :validate_args_done


:validate_args_done

  if not "%rc%" == "0" %exit%
  goto :begin


::-----------------------------------------------------------------------------
::                              BEGIN
::-----------------------------------------------------------------------------
:begin

  set "begin=1"

  if defined cmdargs (
    echo Begin: "%~n0 %cmdargs%" ...
  ) else (
    echo Begin: "%~n0" ...
  )


  @REM Delete any leftover work files from previous run

  if exist %wfn%.tst  del /f %wfn%.tst
  if exist %wfn%.rc   del /f %wfn%.rc
  if exist %wfn%.out  del /f %wfn%.out
  if exist %wfn%.txt  del /f %wfn%.txt



  @REM Build test script consisting of all *.tst files concatenated together

  echo defsym testpath . >> %wfn%.tst
  for %%a in (%tstname%*.tst) do (
    echo msglvl -debug   >> %wfn%.tst
    echo ostailor null   >> %wfn%.tst
    echo numcpu 1        >> %wfn%.tst
    type "%%a"           >> %wfn%.tst
  )
  echo exit              >> %wfn%.tst



  @REM Build startup .rc file which invokes the test script

  echo script %wfn%.tst >> %wfn%.rc



  @REM Initialize counters

  set /a "runs=0"
  set /a "totruns=0"
  set /a "totgood=0"
  set /a "totfail=0"


::-----------------------------------------------------------------------------
::                           Run the test
::-----------------------------------------------------------------------------
:repeat_loop


  @REM Start Hercules using specified .rc file which calls our test script.
  @REM Note: must be '-t', not '/t'! (Hercules doesn't understand '/' opts!)
  @REM Note: must also be all one argument too! (e.g. "-t2.0", not "-t 2.0")

  ..\%hbindir%hercules.exe -t%ttof% -f %cfg% -r %wfn%.rc > %wfn%.out 2>&1



  @REM Call rexx script to parse the log lines and generate the report

  rexx.exe redtest.rexx %wfn%.out > %wfn%.txt 2>&1
  set /a "rc=%errorlevel%"



  @REM If the repeat option wasn't specified then we're done
  @REM Otherwise update the counters and check for failure

  if not defined repeat %exit%

  set /a "runs=runs+1"
  set /a "totruns=totruns+1"

  if %rc% EQU 0 (
    set /a "totgood=totgood+1"
  ) else (
    set /a "totfail=totfail+1"
    if %rc% GTR %maxrc% (
      set /a "maxrc=%rc%"
    )
  )


  @REM Report any failure
  @REM Stop repeating once limit is reached
  @REM Otherwise repeat the same test over again

  if %rc% NEQ 0 call :report_failure

  if %totruns% GEQ %maxruns% %exit%
  if %totfail% GEQ %maxfail% %exit%

  goto :repeat_loop


::-----------------------------------------------------------------------------
::                            report_failure
::-----------------------------------------------------------------------------
:report_failure


  @REM Show them the failure...

  type %wfn%.txt


  @REM Report how many good runs there were before the failure occurred...

  if        %runs% EQU 1 (set "zz=st"
  ) else if %runs% EQU 2 (set "zz=nd"
  ) else if %runs% EQU 3 (set "zz=rd"
  ) else                  set "zz=th"

  @REM Note:  below echo statement is formatted to position
  @REM        the text properly along with other report lines
  echo                                         ** FAILURE #%totfail% **     on %runs%%zz% run
  echo.


  @REM Reset the 'runs' counter and return

  set /a "runs=0"
  %return%


::-----------------------------------------------------------------------------
::                               EXIT
::-----------------------------------------------------------------------------
:exit

  if not defined begin (
    set /a "maxrc=%rc%"
    if not defined hlp echo INFO:  Use "%~n0 /?" to get help. 1>&2
    goto :exitnow
  )

  if not defined repeat (
    type %wfn%.txt
    if %rc% EQU 0 (
      echo End: ** Success! **
    ) else (
      echo End: ** FAILURE! **
    )
    goto :exitnow
  )

  if %totfail% EQU 0 (
    echo ** Successfully completed %maxruns% runs in a row without any failures! **
    goto :exitnow
  )

  set /a "pct=((totfail*1000)+5)/totruns"
  if %pct% GTR 1000 set "pct=1000"
  set    "pct=%pct:~0,-1%.%pct:~-1%"

  echo *** %totfail% FAILURES in %totruns% runs! ^(%pct%%% FAILURE rate!^) ***

:exitnow

  popd
  endlocal & exit /b %maxrc%


::-----------------------------------------------------------------------------
::                              (( EOF ))
::-----------------------------------------------------------------------------

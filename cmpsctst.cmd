@echo off

  REM  If this batch file works, then it was written by Fish.
  REM  If it doesn't then I don't know who the heck wrote it.

  setlocal

  REM                ***   IMPORTANT!   ***
  REM
  REM  Due to issues surrounding the ability to pass arguments
  REM  to REXX scripts in a portable manner, the purpose of this
  REM  shell script (Windows batch (.cmd) file) is simply to
  REM  place each command line argument onto a separate line of
  REM  an "arguments file" which the REXX script can then read.
  REM
  REM  This MAY be needed if any of the arguments being passed
  REM  contain blanks thus requiring them to be surrounded with
  REM  with double quotes (which REXX always removes).
  REM
  REM  If NONE of your arguments have any blanks in them (and
  REM  are thus NOT surrounded with any double quotes) then you
  REM  SHOULD be able to call the REXX script directly yourself
  REM  and use of this batch (.cmd) file then becomes OPTIONAL.
  REM
  REM  If ANY of your arguments *DO* happen to contain blanks
  REM  (thus requiring them to be surrounded by double quotes)
  REM  then you MUST use this batch file to build an arguments
  REM  file to be passed to the REXX script instead. Failure
  REM  to do so WILL cause "cmpsctst.rexx" to not work right.

  set "rexx_script=%~n0%.rexx"
  set "cmpsctst_args_file=%~n0%-args.txt"

  if exist "%cmpsctst_args_file%" del "%cmpsctst_args_file%"

  if not (%1) == () goto :args_loop
  echo "--help" >> "%cmpsctst_args_file%"
  goto :call_rexx

:args_loop

  echo "%~1" >> "%cmpsctst_args_file%"
  shift /1
  if not (%1) == () goto :args_loop

:call_rexx

  rexx "%rexx_script%" "%cmpsctst_args_file%"
  set "rc=%errorlevel%"
  if exist "%cmpsctst_args_file%" del "%cmpsctst_args_file%"
  endlocal & exit /b %rc%

@echo off
REM +-------------------------------------------------------------------------------+
REM !                                                                               !
REM ! Name: _build.bat                                                              !
REM !                                                                               !
REM ! Desc: build Hercules with various options.  Windows bat front end             !
REM !                                                                               !
REM ! Use: _build [i586 | i686] [fthreads | nofthreads]                             !
REM !                                                                               !
REM ! Defaults: _build `uname -m` fthreads                                          !
REM !                                                                               !
REM +-------------------------------------------------------------------------------+
REM !
REM ! Last Change: '$Id$'
REM !
REM +-------------------------------------------------------------------------------
bash _build %1 %2
pause

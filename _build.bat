@echo off
REM +-------------------------------------------------------------------------------+
REM !                                                                               !
REM ! Name: _build.bat                                                              !
REM !                                                                               !
REM ! Desc: build Hercules with various options.  Windows bat front end             !
REM !                                                                               !
REM +-------------------------------------------------------------------------------+
REM !
REM ! Change Log: '$Log$
REM !
REM +-------------------------------------------------------------------------------
echo off
SET ANSI_FLAG=Y
SET BASH_OPT=+e +x
bash %BASH_OPT% _build.tail
bash %BASH_OPT% _build -c p4 -p 1 -f
pause

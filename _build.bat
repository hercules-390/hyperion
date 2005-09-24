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
REM ! Change Log: 'Revision 1.8.4.1  2005/03/01 11:22:05  fish
REM ! Change Log: 'fixing the last(?) of my CRLF screwups
REM ! Change Log: '
REM ! Change Log: 'Revision 1.8  2004/03/28 15:11:58  vbandke
REM ! Change Log: 'no message
REM ! Change Log: '
REM !
REM +-------------------------------------------------------------------------------
echo off
SET ANSI_FLAG=Y
SET BASH_OPT=+e +x
bash %BASH_OPT% _build.tail
bash %BASH_OPT% _build -c p4 -p 1 -f
pause

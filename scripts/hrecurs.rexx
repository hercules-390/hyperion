#! /bin/cat

Trace "O"

parse arg args
args = space(args)
argc = words(args)

parse   version _ver
parse   source  _src
_env    = Address()

parse   var     _src . . _cmd
_who    = filespec("n",_cmd)
parse	var _who  _who "." .

if  ( args = "" ) then ,
    coun = 1
else ,
    coun = args

say _who " started  " coun
if  ( coun > 0 ) then do
    "exec " _who coun-1
end
say _who " ended    " coun

exit 0




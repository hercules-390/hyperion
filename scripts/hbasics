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


say _who " started  "
say _who " version  " _ver
say _who " source   " _src
say _who " HOSTENV  " _env
say _who " date     " date()
say _who " time     " time()
if  args = "" then do
    say _who "          " "No arguments given "
    _ret = 0
end
else do
    say _who " args     " ">>>"args"<<<"
    _ret = args
end
say _who " ended    "

exit _ret



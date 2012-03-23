#! /bin/cat

Trace "O"

parse arg args
args = space(args)
argc = words(args)

parse   version _ver
parse   source  _src
_env    = Address()

parse   var _src . . _cmd
_who    = filespec("n",_cmd)
parse	var _who  _who "." .

say _who "started  "
say _who "version  " _ver
say _who "source   " _src
say _who "HOSTENV  " _env
say _who "date     " date()
say _who "time     " time()

do  i = 1 while ( lines(_cmd) > 0 )
	stmt = linein(_cmd)
    if strip(stmt) = "" then ,
       iterate
    say _who "("right(i,4)") : "stmt
end

say _who "ended"

exit 0

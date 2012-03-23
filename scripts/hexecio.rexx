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

Address HOSTEMU "EXECIO * DISKR '"_cmd"' ( stem stmt. finis "
if RC \= 0 then ,
    say _who " EXECIO RC("RC") "
else ,
    do  i = 1 to stmt.0
        if strip(stmt.i) = "" then ,
            iterate
        say _who "("right(i,4)") : "stmt.i
    end

say _who " ended"

exit 0

::requires hostemu LIBRARY

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


say _who "started  "
say _who "version  " _ver
say _who "source   " _src
say _who "HOSTENV  " _env
say _who "date     " date()
say _who "time     " time()
if  args = "" then do
    say _who "No command entered "
    exit 4
end

--signal on failure name hfailure
--signal on error   name herror

Address HERCULES args "# some useless comment"

say _who "RC = " RC

parse lower var args . "(" opts

parse lower var opts . "stem" stem .

if  strip(stem) = "" then do
    say _who "No output requested "
    exit 0
end

stem = strip(stem,,".") || "."

coun = value(stem || 0)
if  coun = 0 then ,
    say _who "No output returned"
else ,
    do  i = 1 to coun
        say value(stem || i )
    end
say _who "ended  "

exit 0


herror:
say _who "signal on error   trapped"
say _who "ended  "
exit 0

hfailure:
say _who "signal on failure trapped"
say _who "ended  "
exit 0



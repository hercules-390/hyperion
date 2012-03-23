/* Rexx Sample configuration file for Hercules ESA/390 emulator              */
/* tested and working with ooRexx AND Regina                                 */

parse version _RXv
parse var _RXv _RXv "_" .

_PWD = directory()

numcpu      =   1
cnslport    =   3279

Address HERCULES

-- find the number of cpus
"maxcpu (stem retd."
do  i = 1 to retd.0
    if  pos("HHC02203",retd.i) > 0 then do
        parse var retd.i . ":" maxcpu
        maxcpu = strip(maxcpu)
        leave
    end
end

--  CPU section
"cpuserial"     "002623"
"cpumodel"      "3090"
"model"         "EMULATOR"
"plant"         "ZZ"
"manufacturer"  "HRC"
"lparname"      "HERCULES"
"cpuverid"      "FD"
"mainsize"      64
"xpndsize"      0
"maxcpu"        maxcpu
"numcpu"        maxcpu
"numvec"        0
"capping"       0
"archlvl"       "z/Arch"
"archlvl"       "DISABLE ASN_LX_REUSE"

--  misc
"panrate"       "SLOW"
"timerint"      "1000"

--  integrated Hercules I/O Controller
"cnslport"      cnslport

-- message level
"msglevel"      "VERBOSE"


--              .-----------------------Device number
--              |       .-----------------Device type
--              |       |       .---------File name and parameters
--              |       |       |
--              V       V       V
--              ----    ----    --------------------

--integrated console
"attach"        "0009" "3215-C"  "/ noprompt"

-- readers
rdrfile = _PWD"util/zzsacard.bin"
if  \exists(rdrfile) then ,
    rdrfile = "*"

rdrtabl = "000C 001C"
do  i = 1 to words(rdrtabl)
    addr = word(rdrtabl,i)
    "attach"        addr "3505" rdrfile
end

-- card punches
pchtabl = "000D 001D"
do  i = 1 to words(rdrtabl)
    addr = word(pchtabl,i)
    "attach"        addr "3525" "pch"addr".txt"
end

-- printers
prttabl = "0002 0003 000e 000f"
prttype = "3211 3211 1403 1403"
do  i = 1 to words(prttabl)
    addr = word(prttabl,i)
    type = word(prttype,i)
    "attach"        addr type "prt"addr".txt"
end

-- 3270 devices
"attach"            "700.8" "3270"

-- 3270 devices ( to show how to loop thru hex addresses )
do  addr = x2d(708) to x2d(70f)
    "attach"        d2x(addr) "3270"
end


exit


-- ooRexx/Regina compatibility functions

exists:
	if _RXv = "REXX-ooRexx" then do
		if SysIsFile(arg(1)) then return 1
		if SysIsFileDirectory(arg(1)) then return 1
		return 0
	end

	if _RXv = "REXX-Regina" then do
		if stream(arg(1), "c", "query exists") \= "" then return 1
		return 0
	end

	return 0

isFile:
	if _RXv = "REXX-ooRexx" then do
		if SysIsFile(arg(1)) then return 1
		return 0
	end

	if _RXv = "REXX-Regina" then do
		_?fstat = stream(arg(1), "c", "fstat")
		if wordpos("RegularFile",_?fstat) > 0 then return 1
		return 0
	end

	return 0

isPath:
	if _RXv = "REXX-ooRexx" then do
		if SysIsFileDirectory(arg(1)) then return 1
		return 0
	end

	if _RXv = "REXX-Regina" then do
		_?fstat = stream(arg(1), "c", "fstat")
		if wordpos("Directory",_?fstat) > 0 then return 1
		return 0
	end

	return 0

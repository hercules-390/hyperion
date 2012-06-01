/* cmpsc.rexx -- test CMPSC instruction */

--  help:start

--
--  NAME
--
--      <scriptname>  -  Hercules CMPSC instruction test script.
--
--  VERSION
--
--      1.0  (May 23, 2012)
--
--  DESCRIPTION
--
--      Performs the Hercules "cmpsc.txt" instruction test using
--      the specified parameters.
--
--  SYNOPSIS
--
--      <scriptname> inbuff outbuff infile indict [workdir] [-d]
--
--  ARGUMENTS
--
--      inbuff     Input buffer size and offset value specified as
--                 two decimal values separated by a single colon.
--
--      outbuff    Output buffer size and offset in the same format
--                 as the in_buffer_size argument (e.g. "8160:15").
--
--      infile     The name of the file used as input to the test.
--
--      indict     The name of the dictionary used as input to the
--                 test. The file's extention is used to determine
--                 the dictionary format, the compressed data symbol
--                 size and whether this will be a compression test
--                 or an expansion test. If the extension is '.01E'
--                 the test will be an expansion test using a 9-bit
--                 (cdss=1) format-0 expansion dictionary, whereas
--                 '.15C' performs a compression test using 13-bit
--                 format-1 compression and expansion dictionaries.
--
--      workdir    An optional directory name where temporary files
--                 created during processing should be written. If
--                 not specified the current directory is used. The
--                 work files created during processing are called
--                 "cmpout.bin" and "expout.txt" respectively.
--
--      -d         Debug option to force echoing of all internally
--                 issued Hercules commands to the console as well
--                 as whatever message(s) those commands generate.
--                 The normal behavior is for <scriptname> to issue
--                 all internal Hercules commands silently. The -d
--                 option overrides this silent behavior.
--
--  NOTES
--
--      <scriptname> simply performs a series of defsyms to define the
--      test parameters for the Hercules "cmpsc.txt" instruction test
--      program based on the arguments you specified for <scriptname>.
--      It is the Hercules "cmpsc.txt" instruction test program which
--      performs the actual test of the CMPSC instruction. <scriptname>
--      does the needed defsyms, starts the test, and then waits for
--      it to complete by querying the psw at periodic intervals.
--

--  help:end

Trace Off

parse source src
parse var src . mode .                      -- (command or subroutine)
parse var src . . cmdpath
scriptname = filespec("name",cmdpath)       -- (name with extension)
parse var scriptname who "." .              -- (name without extension))

--signal on failure name hfailure           -- (debugging)
--signal on error name herror               -- (debugging)

/* Initialization */

cmp_dict_addr   = x2d("00010000")  -- 64K line   (from CMPSC.txt test program)
in_buffer_addr  = x2d("00100000")  -- 1MB line   (from CMPSC.txt test program)
out_buffer_addr = x2d("00110000")  -- 1MB + 64K  (from CMPSC.txt test program)
out_file_addr   = x2d("00200000")  -- 2MB line   (from CMPSC.txt test program)
in_file_addr    = x2d("00900000")  -- 9MB line   (from CMPSC.txt test program)

cmpout = "cmpout.bin"
expout = "expout.txt"

curdir = directory()
if pos("/",curdir) > 0 then pathsep = "/"
if pos("\",curdir) > 0 then pathsep = "\"

/* Gather arguments */

if  mode = "COMMAND" then do
    parse arg args
    args = space(args)
    arg.0 = words(args)
    do  i = 1 for arg.0
        arg.i = word(args,i)
    end
end
else do -- "SUBROUTINE"
    arg.0 = arg()
    do  i = 1 for arg.0
        arg.i = arg(i)
    end
end

if  arg.0 <= 0 then do
    call help
    exit 1
end

/* Parse arguments */

in_file       = ""
indict        = ""
exp_dict_name = ""
workdir       = ""

in_buffer_size    = 0
in_buffer_offset  = 0
out_buffer_size   = 0
out_buffer_offset = 0

have_inbuff   = 0
have_outbuff  = 0
have_infile   = 0
have_indict   = 0
have_workdir  = 0

debug = 0

do  i = 1 for arg.0
    argv = arg.i

    if  argv = "/?" | ,
        argv = "-?" | ,
        argv = "-h" | ,
        argv = "--help" then do
        signal help
    end

    if \have_inbuff then do
        have_inbuff = 1
        in_buffer_size = argv
        iterate
    end

    if \have_outbuff then do
        have_outbuff = 1
        out_buffer_size = argv
        iterate
    end

    if \have_infile then do
        have_infile = 1
        in_file = argv
        iterate
    end

    if \have_indict then do
        have_indict = 1
        indict = argv
        iterate
    end

    if \have_workdir then do
        have_workdir = 1
        workdir = argv
        iterate
    end

    if  argv = "/d" | ,
        argv = "-d" then do
        debug = 1
        iterate
    end

    call logmsg '** ERROR ** arg 'i': extraneous argument: "'argv'"'
    exit 1
end

/* Make sure we have all needed arguments */

if \have_inbuff then do
    call logmsg "** ERROR ** Required argument 'inbuff' not specified."
    exit 1
end

if \have_outbuff then do
    call logmsg "** ERROR ** Required argument 'outbuff' not specified."
    exit 1
end

if \have_infile then do
    call logmsg "** ERROR ** Required argument 'infile' not specified."
    exit 1
end

if \have_indict then do
    call logmsg "** ERROR ** Required argument 'indict' not specified."
    exit 1
end

if \have_workdir then do
    workdir = curdir
end

/* Validate arguments */

parse var in_buffer_size  ib ":" in_buffer_offset
parse var out_buffer_size ob ":" out_buffer_offset

if  in_buffer_offset  = "" then in_buffer_offset  = 0
if  out_buffer_offset = "" then out_buffer_offset = 0

if  \isnum(ib) | \isnum(in_buffer_offset) then do
    call logmsg '** ERROR ** non-numeric inbuff:offset "'in_buffer_size'"'
    exit 1
end

if  \isnum(ob) | \isnum(out_buffer_offset) then do
    call logmsg '** ERROR ** non-numeric outbuff:offset "'out_buffer_size'"'
    exit 1
end

in_buffer_size  = ib
out_buffer_size = ob

if  fullpath(in_file) = "" then do
    call logmsg '** ERROR ** infile "'in_file'" not found.'
    exit 1
end

if  fullpath(indict) = "" then do
    call logmsg '** ERROR ** indict "'indict'" not found.'
    exit 1
end

if  right(indict,1) = "C" then do
    test_type = "Compression"
    compress = 1
    expand   = 0
    cmp_dict_name = indict
    exp_dict_name = left(cmp_dict_name,length(cmp_dict_name)-1) || "E"
end
else do
    test_type = "Expansion"
    compress = 0
    expand   = 1
    /*
        PROGRAMMING NOTE: for expansion we purposely set our compression
        dictionary name to be the same as our expansion dictionary name
        in order to force the expansion dictionary to get loaded where
        the compression dictionary normally would be. We do this because
        our test program is designed to point GR1 to what it thinks is a
        compression dictionary, but because this is an expansion and not
        a compression, causes it to end up pointing to our expansion
        dictionary instead, which is exactly what we want.
    */
    exp_dict_name = indict
    cmp_dict_name = exp_dict_name
end

if  fullpath(cmp_dict_name) = "" then do
    call logmsg '** ERROR ** cmp_dict_name "'cmp_dict_name'" not found.'
    exit 1
end

if  fullpath(exp_dict_name) = "" then do
    call logmsg '** ERROR ** exp_dict_name "'exp_dict_name'" not found.'
    exit 1
end

if  fullpath(workdir) = "" then do
    call logmsg '** ERROR ** workdir "'workdir'" not found.'
    exit 1
end
workdir = dirnamefmt(workdir)

/* Set needed values */

if compress then out_file = workdir || cmpout
if expand   then out_file = workdir || expout

in_buffer_addr  += in_buffer_offset
out_buffer_addr += out_buffer_offset

cdss = left(right(cmp_dict_name,2),1)
fmt  = left(right(cmp_dict_name,3),1)

dictsize = 2048 * (2 ** cdss)
exp_dict_addr = cmp_dict_addr + dictsize

dicts_size = dictsize
if fmt = "1" then ,
    dicts_size += dictsize

GR0  = 0
GR0 += cdss   * (2 ** 12)
GR0 += fmt    * (2 **  9)
GR0 += expand * (2 **  8)

call SysFileTree in_file, "info", "F"
parse var info.1 . . in_file_size . .

msgs. = ""

call do_test(test_type)

exit 0

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

do_test:

    test_type = arg(1)

    /* The following 'defsym' values are used by */
    /* the "cmpsc.txt" instruction test program  */

    call panel_command "defsym in_file            " || defsym_filename(in_file)
    call panel_command "defsym out_file           " || defsym_filename(out_file)
    call panel_command "defsym cmp_dict_name      " || defsym_filename(cmp_dict_name)
    call panel_command "defsym exp_dict_name      " || defsym_filename(exp_dict_name)
    call panel_command "defsym GR0                " || defsym_value(right("00000000"||d2x(GR0),8))
    call panel_command "defsym dicts_size         " || defsym_value(right("00000000"||d2x(dicts_size),8))
    call panel_command "defsym in_file_size       " || defsym_value(right("00000000"||d2x(in_file_size),8))
    call panel_command "defsym cmp_dict_addr      " || defsym_value(right("00000000"||d2x(cmp_dict_addr),8))
    call panel_command "defsym exp_dict_addr      " || defsym_value(right("00000000"||d2x(exp_dict_addr),8))
    call panel_command "defsym in_buffer_offset   " || defsym_value(right("00000000"||d2x(in_buffer_offset),8))
    call panel_command "defsym in_buffer_addr     " || defsym_value(right("00000000"||d2x(in_buffer_addr),8))
    call panel_command "defsym in_buffer_size     " || defsym_value(right("00000000"||d2x(in_buffer_size),8))
    call panel_command "defsym out_buffer_offset  " || defsym_value(right("00000000"||d2x(out_buffer_offset),8))
    call panel_command "defsym out_buffer_addr    " || defsym_value(right("00000000"||d2x(out_buffer_addr),8))
    call panel_command "defsym out_buffer_size    " || defsym_value(right("00000000"||d2x(out_buffer_size),8))
    call panel_command "defsym out_file_addr      " || defsym_value(right("00000000"||d2x(out_file_addr),8))
    call panel_command "defsym in_file_addr       " || defsym_value(right("00000000"||d2x(in_file_addr),8))

    found_start = 0

    /* The following extracts the "cmpsc.txt" instruction  */
    /* test program code which is embedded into our source */

    do  i = 1 for sourceline()

        srcline = strip(strip(strip(sourceline(i)),"leading","-"))

        if  srcline = "" then ,
            iterate

        if  srcline = "test:end" then ,
            leave

        if  srcline = "test:start" then do
            found_start = 1
            iterate i
        end

        if  \found_start then ,
            iterate i

        call panel_command srcline

    end

    /* Wait for the now running test program to finish */
    /* and then save the output file that it created.  */

    if test_wait(test_type) then do

        -- get o/p file size from gpr 11

        call panel_command "gpr"

        -- HHC01603I gpr
        -- HHC02269I General purpose registers
        -- HHC02269I GR00=00005200 GR01=00000004 GR02=00110AAB GR03=00000261
        -- HHC02269I GR04=00101FD7 GR05=00000000 GR06=00201C01 GR07=00000000
        -- HHC02269I GR08=00110AAB GR09=00000000 GR10=00000000 GR11=00001C02
        -- HHC02269I GR12=80000200 GR13=00110AAB GR14=800002B8 GR15=00000038

        if msgs.0 < 6 then do
            call logmsg ""
            call logmsg "*** "test_type" Test SUCCESS ***"
            call logmsg ""
            call logmsg "** ERROR ** 'gpr' command parsing failure; test aborted."
            exit 1
        end
        parse var msgs.5 . " GR11=" gpr11 .

        -- save output file

        out_file_size = x2d(gpr11)
        out_file_end_addr = out_file_addr + out_file_size - 1
        call delfile(out_file)
        call panel_command "savecore $(out_file) $(out_file_addr) " || d2x(out_file_end_addr)

        call logmsg ""
        call logmsg "    *** "test_type" Test SUCCESS ***"
        call logmsg ""
        call logmsg '    outfile name: "' || out_file || '"'
        call logmsg "    outfile size:  " || out_file_size || " bytes"
        call logmsg ""

    end
    return

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

test_wait: -- (wait for test to complete; return success/failure boolean)

    test_type = arg(1)

    ok = 0      -- (it is safest to presume failure)

    do forever

        /* Query the PSW at regular intervals until we */
        /* see a PSW that indicates the test has ended */

        call SysSleep 1.0
        call panel_command "psw"

        -- HHC01603I psw
        -- HHC02278I Program status word: 0000000000000000 0000000000000000
        -- HHC02300I sm=00 pk=0 cmwp=0 as=pri cc=0 pm=0 am=24 ia=0

        if msgs.0 < 3 then do
            call logmsg "** ERROR ** 'psw' command parsing failure; test aborted."
            exit 1
        end
        parse var msgs.3 . "cmwp=" cmwp " " . "ia=" psw_ia

        if psw_ia = "0" then do
            ok = 1 -- (success)
            leave
        end

        if psw_ia = "C4" then do
            call logmsg ""
            call logmsg "*** "test_type" Test FAILED ***   (Buffer Overflow Detected)"
            call logmsg ""
            leave
        end

        if psw_ia = "BADCC" then do
            call logmsg ""
            call logmsg "*** "test_type" Test FAILED ***   (Invalid cc=2 from CMPSC)"
            call logmsg ""
            leave
        end

        if psw_ia = "DEAD" then do

            call panel_command "r 8e.2" -- (program interrupt code)

            -- HHC01603I r 8e.2
            -- HHC02290I R:000000000000008E:K:00=0000 00000000 00000000 00000000 0000 ................

            if msgs.0 < 2 then do
                call logmsg "** ERROR ** 'r 8e.2' command parsing failure; test aborted."
                exit 1
            end
            parse upper var msgs.2 . "=" code " " .

            if code = "00C4" then reason = "0C4 Protection Exception" ; else ,
            if code = "00C7" then reason = "0C7 Data Exception"       ; else ,
                                  reason = "Program Interrupt Code " || code

            call logmsg ""
            call logmsg "*** "test_type" Test FAILED ***   ("reason")"
            call logmsg ""
            leave
        end

        if cmwp = "A" then do
            call logmsg ""
            call logmsg "*** "test_type" Test FAILED ***   (Unexpected disabled wait PSW)"
            call logmsg ""
            leave
        end
    end
    return ok       -- (true = success, false = failure)

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

defsym_value: -- (wrap value within double quotes if it contains any blanks)

    result = strip(arg(1),,'"')
    if  pos(" ",result) > 0 then
        result = '"' || result || '"'
    return result

defsym_filename: -- (wrap value yet again within apostrophes if needed)

    -- Windows compatibility: wrap the double-quoted value within
    -- single quotes (apostrophes) so that the double quotes are
    -- included as part of the defsym'ed value

    result = defsym_value(arg(1))
    if  left(result,1) = '"' then
        result = "'" || result || "'"
    return result

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

panel_command: procedure expose who msgs. debug -- (issue Hercules command)

    cmdline = arg(1)
    rc = awscmd(cmdline,"msgs")
    if debug then do
        do i=1 for msgs.0
            say msgs.i
        end
    end
    return rc

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

logmsg: procedure expose who -- (say something prefixed with our name)

    say who": "arg(1)
    return

isnum: procedure -- (is it a number?)

    return arg(1) <> "" & datatype(arg(1),"N");

fullpath: procedure -- (locate file or dir, return full path or null)

    return SysSearchPath("PATH",arg(1))

delfile: procedure -- (delete a file if it exists)

    if  SysIsFile(arg(1)) then
        call SysFileDelete(arg(1))
    return

dirnamefmt: procedure expose pathsep -- (convert to dir format)

    result = arg(1)
    do  while pos(pathsep||pathsep,result) > 0
        result = changestr(pathsep||pathsep,result,pathsep)
    end
    if  qualify(result) <> qualify(result||pathsep) then
        result ||= pathsep
    return result

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

help:

    found_start = 0

    do  i = 1 for sourceline()

        srcline = strip(sourceline(i))

        if  srcline = "" then ,
            iterate

        srcline = strip(srcline,"leading","-")

        if  strip(srcline) = "help:end" then ,
            leave

        if  strip(srcline) = "help:start" then do
            found_start = 1
            iterate i
        end

        if  \found_start then ,
            iterate i

        srcline = changestr("<scriptname>",srcline,scriptname)
        say strip(srcline,"T") -- (use 'say' here, *NOT* 'logmsg'!)

    end
    exit 1

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

herror:

    call logmsg "signal on error trapped"
    call logmsg "ended"
    exit 1

hfailure:

    call logmsg "signal on failure trapped"
    call logmsg "ended"
    exit 1

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/*
                            CMPSC instruction test

    This test is designed to be driven by the REXX script called CMPSC.REXX.
    CMPSC.REXX issues the needed defsym commands to define the actual test
    parameters which this test script then uses to perform the actual test.
    The assembler source code for this test is in file "util\CMPSC.txt".
    "CMPSC.txt" is purposely designed to be compression/expansion neutral.
*/
/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

-- test:start

-- archmode esa/390
-- defstore main 16
-- sysclear
-- sysreset

-- loadcore  $(cmp_dict_name)  $(cmp_dict_addr)
-- loadcore  $(exp_dict_name)  $(exp_dict_addr)
-- loadcore  $(in_file)        $(in_file_addr)

-- gpr 0=$(GR0)
-- gpr 1=$(cmp_dict_addr)
-- gpr 2=$(out_buffer_addr)
-- gpr 3=$(out_buffer_size)
-- gpr 4=$(in_buffer_addr)
-- gpr 5=$(in_buffer_size)

-- r 068=000A00008000DEAD
-- r 000=0008000080000200

/* The actual "CMPSC.txt" test program code begins here */

-- r 200=0DC0
-- r 202=06C0
-- r 204=06C0
-- r 206=9805C618
-- r 20A=1862
-- r 20C=41732000
-- r 210=5070C648
-- r 214=4DE0C056
-- r 218=41607FFF
-- r 21C=8860000C
-- r 220=8960000C
-- r 224=1F67
-- r 226=5060C64C
-- r 22A=1864
-- r 22C=41754000
-- r 230=4DE0C056
-- r 234=1861
-- r 236=8860000C
-- r 23A=8960000C
-- r 23E=1876
-- r 240=5E70C630
-- r 244=4DE0C056
-- r 248=58A0C638
-- r 24C=58B0C640
-- r 250=1F55
-- r 252=47F0C088
-- r 256=8860000C
-- r 25A=8960000C
-- r 25E=18D7
-- r 260=88D0000C
-- r 264=89D0000C
-- r 268=41A00010
-- r 26C=41F00038
-- r 270=B22B00A6
-- r 274=41606800
-- r 278=41606800
-- r 27C=156D
-- r 27E=47D0C070
-- r 282=B22B00F6
-- r 286=07FE
-- r 288=4DE0C0F4
-- r 28C=4DE0C154
-- r 290=B20A0010
-- r 294=B2630024
-- r 298=4710C094
-- r 29C=B20A0000
-- r 2A0=4720C0F0
-- r 2A4=9200C655
-- r 2A8=4780C0B0
-- r 2AC=9201C655
-- r 2B0=4DE0C166
-- r 2B4=4DE0C132
-- r 2B8=12AA
-- r 2BA=4770C088
-- r 2BE=9500C655
-- r 2C2=4770C088
-- r 2C6=5400C650
-- r 2CA=4770C0E8
-- r 2CE=5410C644
-- r 2D2=4780C0E8
-- r 2D6=5880C620
-- r 2DA=5860C63C
-- r 2DE=D20060008000
-- r 2E4=41B0B001
-- r 2E8=50B0C640
-- r 2EC=8200C600
-- r 2F0=8200C608
-- r 2F4=129A
-- r 2F6=078E
-- r 2F8=9867C628
-- r 2FC=18D7
-- r 2FE=1275
-- r 300=4780C10A
-- r 304=1884
-- r 306=1897
-- r 308=0E68
-- r 30A=187D
-- r 30C=1F75
-- r 30E=5880C634
-- r 312=189A
-- r 314=1597
-- r 316=47B0C11C
-- r 31A=1879
-- r 31C=0E68
-- r 31E=5080C634
-- r 322=5090C638
-- r 326=18A9
-- r 328=5840C628
-- r 32C=1856
-- r 32E=1F54
-- r 330=07FE
-- r 332=9889C620
-- r 336=1F93
-- r 338=5860C63C
-- r 33C=1879
-- r 33E=1EB9
-- r 340=18D8
-- r 342=0E68
-- r 344=5060C63C
-- r 348=D200D0002000
-- r 34E=9823C620
-- r 352=07FE
-- r 354=9867C648
-- r 358=1277
-- r 35A=078E
-- r 35C=1F99
-- r 35E=BF98C654
-- r 362=0E68
-- r 364=07FE
-- r 366=9867C648
-- r 36A=1277
-- r 36C=078E
-- r 36E=1F99
-- r 370=BF98C654
-- r 374=0F68
-- r 376=4770C17C
-- r 37A=07FE
-- r 37C=8200C610
-- r 800=000A000080000000
-- r 808=000A2000800BADCC
-- r 810=000A0000800000C4
-- r 818=$(GR0)
-- r 81C=$(cmp_dict_addr)
-- r 820=$(out_buffer_addr)
-- r 824=$(out_buffer_size)
-- r 828=$(in_buffer_addr)
-- r 82C=$(in_buffer_size)
-- r 830=$(dicts_size)
-- r 834=$(in_file_addr)
-- r 838=$(in_file_size)
-- r 83C=$(out_file_addr)
-- r 840=00000000
-- r 844=00000007
-- r 848=00000000
-- r 84C=00000000
-- r 850=00000100
-- r 854=CD
-- r 855=00

-- restart

-- test:end

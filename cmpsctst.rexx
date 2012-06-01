/*****************************************************************************\
                              CMPSCTST.REXX

                Front-end driver for CMPSCTST.EXE Testing Tool

                (C) Copyright "Fish" (David B. Trout), 2012

                Released under "The Q Public License Version 1"
                 (http://www.hercules-390.org/herclic.html)
                       as modifications to Hercules.

\*****************************************************************************/

  ver      = "2.1"              -- (version of this script)
  ver_date = "May 22, 2012"     -- (version of this script)

  Trace Off
  signal initialize

/*----------------------------------------------------------------------------
                           check_rexx_support
 ----------------------------------------------------------------------------*/

check_rexx_support: procedure expose  _rc

  parse upper version VERS
  parse var VERS REXX . .
  /*
      PROGRAMMING NOTE: the issue I had with Regina was that I simply
      could not ever get Regina's utilities DLL to load properly or
      register/call the SysLoadFuncs function to provide support for
      the "SysFileTree" and related functions. Perhaps I was doing
      something wrong. Maybe someone else can figure it out, but for
      now, Regina is simply not supported. Sorry!
  */
  if pos(REXX,"REGINA") = 0 then
    return
  call errmsg "Open Object REXX required. (Regina doesn't work right!)"
  exit -- (abort)

/*----------------------------------------------------------------------------
                            platform_unique
 ----------------------------------------------------------------------------*/

platform_unique:          -- (subroutine)

                /* Set platform specific variables */

  rexxutil_dll = "rexxutil"
  cmpsctst_bin = "cmpsctst"
  md5sum_bin   = "md5sum"

  if left(OPSYS,3) = "WIN" then do
    windows = 1
    linux = 0
    pathsep = "\"
    cmpsctst_bin ||= ".exe"
    md5sum_bin   ||= ".exe"
    rexxutil_dll ||= ".dll"
  end; else do
    linux = 1
    windows = 0
    pathsep = "/"
    rexxutil_dll = "lib" || rexxutil_dll || ".so"
  end

  return

/*----------------------------------------------------------------------------
                                HELP
 ----------------------------------------------------------------------------*/

help:

  say ""
  say "    NAME"
  say ""
  say "        "_n0"  -  CMPSC Instruction Testing Tool helper script."
  say ""
  say "    SYNOPSIS"
  say ""
  say "        "_nx0"    filesdir  dictsdir  [workdir]  [options]"
  say ""
  say "    DESCRIPTION"
  say ""
  say "        Uses the "CMPSCTST.EXE" Instruction Test Tool to verify"
  say "        the sanity of the given compression algorithm to within"
  say "        an acceptably high degree of confidence depending on the"
  say "        number and variety of files and dictionaries used."
  say ""
  say "    ARGUMENTS"
  say ""
  say "        filesdir   The name of the directory tree where the set of"
  say "                   test files reside. All of the test files in the"
  say "                   specified directory and all subdirectories will"
  say "                   be processed against all dictionaries found in"
  say "                   the dictsdir directory."
  say ""
  say "        dictsdir   The name of the directory tree where the set of"
  say "                   test dictionaries reside. All of the dictionaries"
  say "                   in the specified directory and all subdirectories"
  say "                   will be processed against all of the test files"
  say "                   found in the filesdir directory."
  say ""
  say "        workdir    The name of a directory where temporary files"
  say "                   created during processing should be written. If"
  say "                   not specified the current directory is used. The"
  say "                   work files created during processing are called"
  say '                   "'cmpout_bin'" and "'expout_txt'".'
  say ""
  say "    OPTIONS"
  say ""
  say "        -t      Translate ASCII/EBCDIC             (default = no)"
  say "        -a      Algorithm to be used               (default = 0)"
  say "        -r      Number of random tests             (default = "num_randoms")"
  say "        -n      No hard-coded test cases           (default = all)"
  say "        -bn     Use only buffer sizes set 'n'      (default = all)"
  say "        -on     Use only buffer offsets set 'n'    (default = all)"
  say "        -speed  Speed test; -r = repeats           (see NOTES below)"
  say ""
  say "    EXAMPLES"
  say ""
  say "        "_n0"  .\files        .\dicts  .\work  > EXTREMELY-long-test.log"
  say "        "_n0"  .\files\small  .\dicts  .\work  -r 0 > non-random-test.log"
  say "        "_n0"  .\files\small  .\dicts  .\work  -n -r 16 > randoms-test.log"
  say "        "_n0"  .\files\large  .\dicts  .\work  -speed -r 1000 > speed-test.log"
  say ""
  say "    NOTES"
  say ""
  say "        Unless a timing run (speed test) is being done, "_n0
  say "        repeatedly calls the CMPSCTST.EXE test tool for every test"
  say "        file and dictionary found in the two specified directories"
  say "        and all of their subdirectories first asking it to compress"
  say "        the test file to a temporary work file, then expanding the"
  say "        work file to a second work file, and finally comparing the"
  say "        MD5 hash of the final expanded output to make sure that it"
  say "        matches bit-for-bit with the original input file."
  say "  "
  say "        A series of several compress/expand cycles are performed,"
  say "        first using a hard-coded range of buffer size and offset"
  say "        values known to have caused problems in the past as well"
  say "        as a series of randomly generated input and output buffer"
  say "        size and offset values. If either of the compression or"
  say "        expansion calls fail or if the expanded output fails to"
  say "        exactly match the original input a diagnostic message is"
  say "        displayed describing the problem but the command does not"
  say "        abort. Rather, it continues on with the next set of files"
  say "        and dictionaries and does not end until after all passes"
  say "        are made against all files and dictionaries using all of"
  say "        of the previously mentioned buffer size and offset values."
  say ""
  say "                             ** WARNING! **"
  say ""
  say "        "_nx0" produces a VERY large number of messages"
  say "        as it runs since the CMPSCTST.EXE tool is always called"
  say "        with the "-v" verbose option specified."
  say ""
  say "        It is therefore HIGHLY RECOMMENDED you redirect its stdout"
  say "        output to a log file. Redirecting stderr to a log file is"
  say "        optional but not recommended since the volume of progress"
  say "        messages written to stderr is quite small, being limited"
  say "        to notification when each new test is about to start and"
  say "        which dictionary it is using. It does NOT notify you when"
  say "        each new buffer size and offset value is used however."
  say ""
  say "                               - Note -"
  say ""
  say "        Because a default test using the many built-in (hard-coded)"
  say "        buffer size and offset values can cause "_n0" to run for"
  say "        an *VERY* long time since there are so many of them ("words(buffsizes)**2 * words(offsets)**2","
  say "        times #of test files, times #of test dictionaries = total"
  say "        number of tests, each of which does a full compression and"
  say "        expansion) the -n (no-non-random) option allows you to skip"
  say "        these tests to allow "_n0" to finish much more quickly."
  say ""
  say "        You can also use the -bn and -on options to choose a smaller"
  say "        subset of buffer size and/or offset values too. There are 3"
  say "        buffer size sets with "words(bs1)", "words(bs2)" and "words(bs3)" values in each of their sets"
  say "        respectively, and 2 sets of offset values with "words(of1)" and "words(bs2)" values"
  say "        in each of their sets, respectively."
  say ""
  say "        The -r value defines either the number of random buffer size"
  say "        and offset tests to perform or else the number of repeats to"
  say "        perform when the -speed option is specified."
  say ""
  say "        If the -speed option is not specified (default) the -r value"
  say "        defines the number of random generated buffer size and offset"
  say "        tests to perform for each file and dictionary pair. Specify 0"
  say "        to skip all randomly generated buffer size and offset tests"
  say "        and do only the built-in (non-random) tests instead."
  say ""
  say "        When the -speed option is specified it forces all hard-coded"
  say "        built-in and randomly generated buffer size and offset tests"
  say "        to be skipped, thereby altering the meaning of the -r option."
  say "        When -speed is specified, the -r option instead defines the"
  say "        number of repeated compress and expand cycles to perform for"
  say "        each file, each of which always uses default buffer size and"
  say "        offset values. To perform a custom timing/speed test using"
  say "        specific buffer size and offset values, you need to call the"
  say "        CMPSCTST.EXE tool yourself (i.e. don't use "_nx0")."
  say ""
  say "        All dictionaries must be in RAW BINARY format and MUST use"
  say "        the following file naming convention:"
  say ""
  say "                     <filename>.FST"
  say ""
  say "        Where:"
  say ""
  say "             F       Dictionary Format     0/1"
  say "             S       Symbol Size           1-5"
  say "             T       Dictionary Type       C/E"
  say ""
  say "        Thus, for a 13-bit CDSS format-1 dictionary there must be"
  say "        two files with whatever name you want, but with filename"
  say "        extensions of:"
  say ""
  say "             .15C        Compression dictionary"
  say "             .15E        Expansion dictonary"
  say ""
  say "        For example:"
  say ""
  say "            mydict.03C"
  say "            mydict.03E"
  say "            foobar.15C"
  say "            foobar.15E"
  say "            ...etc..."
  say ""
  say "        Each dictionary pair MUST have the same name and each MUST"
  say "        be in raw binary format."
  say ""
  say "    EXIT STATUS"
  say ""
  say "        0    Success."
  say "        1    Failure."
  say ""
  say "    AUTHOR"
  say ""
  say "        "Fish" (David B. Trout)"
  say ""
  say "    VERSION"
  say ""
  say "        "ver"  ("ver_date")"
  say ""

  exit -1

/*----------------------------------------------------------------------------
                                INITIALIZE
 ----------------------------------------------------------------------------*/

initialize:

  _rc = 0

  /* Save our name (_0 and _nx0 and _n0) */
  do
    parse source src
    parse upper var src OPSYS . .
    parse       var src . mode .
    parse       var src . . _0
    _0 = strip(_0,,'"')
    _nx0 = filespec("name",_0)
    parse	var _nx0  _n0 "." .
    drop src
  end

  /* Check for required REXX support */
  call check_rexx_support

  /* Set platform specific variables */
  call platform_unique

  /* Define all of our constants and variables... */

  cmpout_bin   = "cmpout.bin"
  expout_txt   = "expout.txt"

  num_algorithms = 3

  MAXSYMLEN    = 260
  PAGESIZE     = 4096
  MIN_OFFSET   = 0
  MAX_OFFSET   = PAGESIZE - 1
  MIN_BUFFSIZE = MAXSYMLEN
  MAX_BUFFSIZE = 32767 + MAX_OFFSET + 1   -- (purely arbitrary)

  sep = "---------------------------------------------------------------------------------------------------"


  /* The following buffer size and offset values
     are known to have caused problems in the past
     with certain files on some implementations. */

  bs1 = "8160 8183"
  bs2 = "8161 8182 8162 8181 8163 8180"
  bs3 = "16964 4665 4243 27881"

  of1 = "0 1 2 3 7 8 9 10 15"
  of2 = "1695 1771 3126 4052"


  /* The following values are modifiable options */

  trans       = ""        -- ("-t" translate option)
  algorithm   = 0         -- ("-a" algorithm option)
  no_nonrand  = ""        -- ("-n" no non-randoms option)
  num_randoms = 100       -- ("-r" random option)
  repeat      = ""        -- ("-r" repeat option)
  speed       = ""        -- ("-speed" option)

  buffsizes   = bs1 || " " || bs2 || " " || bs3     -- (the complete set)
  offsets     = of1 || " " || of2                   -- (the complete set)

  /*
      Gather arguments...

      Due to issues surrounding the ability to pass arguments
      which happen to contain blanks in them (thus requiring
      them to be surrounded with double quotes) to REXX in a
      portable manner, an optional arguments file may be used
      instead. If none of the arguments need to be surrounded
      with double quotes however (which REXX always removes),
      then normal argument parsing can be used.
  */

  if left(OPSYS,3) = "WIN" then do
    args_file = strip(arg(1),,'"')
    if args_file = "" then
      signal help
    call read_args strip(args_file,,'"')
    if _rc <> 0 then
      signal exit
  end

  if mode \= "ARGSFILE" then do
    parse arg args
    args = space(args)
    arg.0 = words(args)
    do  i = 1 for arg.0
      arg.i = word(args,i)
    end
  end
  /*
      Check if Help was requested...

      We need to do this BEFORE we try loading the tools
      so they can always obtain help information regardless
      of whether or not the loading of the tools fails.
      We also need to do this AFTER we set our variables
      since the help function uses them in its help display.
  */

  if  arg.0 <= 0 then
    signal help

  helpflags = "? -? /? -h /h -" || "-help"        -- (careful!)

  do i=1 to arg.0
    do h=1 to words(helpflags)
      if lower(arg.i) = lower(word(helpflags,h)) then
        signal help
    end
  end
  drop helpflags i h


  /* Now we can try loading our required tools... */

  call loadtools
  if _rc <> 0 then
    signal exit

  signal parse_arguments

/*----------------------------------------------------------------------------
                           READ ARGUMENTS FILE
 ----------------------------------------------------------------------------*/

read_args:                -- (subroutine)

  cmdline = qif(reltodir(_0))                       -- (reconstructed cmdline)
  args_file = strip(arg(1),,'"')                    -- (raw name of args file)
  arg.0 = 0;                                        -- (number of arguments)

  rc = stream( args_file, "Command", "OPEN READ" )

  if rc = "READY:" then do                          -- (successfully opened?)

    mode = "ARGSFILE"                               -- (not "COMMAND" mode)

    do i=1 by 1 while lines(args_file)              -- (for each file stmt)
      arg.0 = i                                     -- (count)
      arg.i = strip(strip(linein(args_file)),,'"')  -- (unquoted)
      cmdline ||= " " || qif(reltodir(arg.i))       -- (reconstructed cmdline)
      arg.i = changestr('""',arg.i,'"')             -- (correct escaped quotes)
    end

    call stream args_file, "Command", "CLOSE"       -- (finished! close it)
    call delfile args_file                          -- (and then delete it)

  end; else do
    if rc = "ERROR:2" then
      mode = "COMMAND"                              -- (try parsing normally)
    else do
      call errmsg 'Could not open Arguments file "' || args_file || '"'
      _rc = 1
    end
  end
  return

/*----------------------------------------------------------------------------
                           LOAD REQUIRED TOOLS
 ----------------------------------------------------------------------------*/

loadtools: procedure expose _rc rexxutil_dll cmpsctst_bin md5sum_bin

  /* Load REXX Utilities library */

  If 0 ,
     | RxFuncQuery("SysIsFile") ,
     | RxFuncQuery("SysFileDelete") ,
     | RxFuncQuery("SysIsFileDirectory") ,
     | RxFuncQuery("SysSearchPath") ,
     | RxFuncQuery("SysFileTree") ,
  then do
    if RxFuncAdd("SysLoadFuncs",rexxutil_dll,"SysLoadFuncs") then do
      call errmsg "RxFuncAdd(SysLoadFuncs) failed!"
      _rc = 1
    end; else do
      if SysLoadFuncs() <> 0 then do
        call errmsg "SysLoadFuncs() failed!"
        _rc = 1
      end
    end
  end

  if _rc <> 0 then
    return

  /* Verify all required tools exist */

  call fullpath cmpsctst_bin
  if result = "" then do
    call errmsg '"'cmpsctst_bin'" not found.'
    _rc = 1
  end

  call fullpath md5sum_bin
  if result = "" then do
    call errmsg '"'md5sum_bin'" not found.'
    _rc = 1
  end

  return

/*----------------------------------------------------------------------------
                            PARSE ARGUMENTS
 ----------------------------------------------------------------------------*/

parse_arguments:

  files_dir = ""
  dicts_dir = ""
  work_dir  = ""

  if arg.0 >= 1 then
    files_dir = arg.1

  if arg.0 >= 2 then
    dicts_dir = arg.2

  if arg.0 >= 3 then
    work_dir = arg.3

  /* "work-dir" is optional so check if it was specified
     by checking whether first character is a "-" dash */

  beg_argnum = 3        -- (start here if it wasn't specified)

  if work_dir <> "" then do
    if left(work_dir,1) = "-" then
      work_dir = ""     -- (it wasn't specified; undo mistake)
    else
      beg_argnum = 4    -- (it was specified; start from here)
  end

  do i = beg_argnum to arg.0

    opt = arg.i

    n = i + 1

    if arg.0 >= n then
      val = arg.n
    else
      val = ""

    select

      when opt = "-t" then
        trans = "-t"

      when opt = "-b1" then
        buffsizes = bs1

      when opt = "-b2" then
        buffsizes = bs2

      when opt = "-b3" then
        buffsizes = bs3

      when opt = "-o1" then
        offsets = of1

      when opt = "-o2" then
        offsets = of2

      when opt = "-speed" then do
        speed = "-speed"
        no_nonrand = 1
        repeat = "-r "num_randoms
      end

      when opt = "-n" then do
        no_nonrand = 1
        if speed <> "" then
          call warnmsg "Option '-n' already implied by -speed"
      end

      when opt = "-a" then do

        /* The "-a" option takes an OPTIONAL algorithm number */

        if val = "" then ,
          algorithm = 1     -- (default alternate algorithm)
        else if left(val,1) = "-" | left(val,1) = "/" then
          algorithm = 1     -- ("val" is really next option)
        else do
          i += 1            -- (the algorithm was specified)
          if isnum(val) then do
            if val >= 0 & val < num_algorithms then ,
              algorithm = val
            else do
              call bad_option_value  -- (out of valid range)
              _rc = 1
            end
          end; else do
            call bad_option_value    -- (not a valid number)
            _rc = 1
          end
        end
      end

      when opt = "-r" then do

        /* "-r" requires a value (already placed into "val") */

        i += 1    -- (because we're consuming "val")

        if isnum(val) then do
          if speed <> "" then do

            /* Speed test: -r means #of repeats; must be > 0 */

            if val > 0 then do
              repeat = "-r " || val + 0
            end; else
              call bad_option_value   -- (negative or zero)
          end; else do

            /* Normal non-speed test: -r means #of randoms or 0 for none */

            if val >= 0 then do
              repeat = ""
              num_randoms = val + 0
            end; else
              call bad_option_value   -- (negative invalid)
          end
        end; else
          call bad_option_value       -- (not a valid number)
      end

      otherwise do
        call errmsg "Invalid option '"opt"'"
        _rc = 1
      end

    end /* select */

  end /* do while */

  signal validate_arguments

bad_option_value:
  call errmsg "Invalid '"opt"' value " || '"'val'"'
  _rc = 1
  return

/*----------------------------------------------------------------------------
                         VALIDATE PARSED ARGUMENTS
 ----------------------------------------------------------------------------*/

validate_arguments:

  /* Here we validate our parsed arguments for sanity. Previously all
     we did was syntax check them. Here we check whether their values
     actually make any sense to us (whether directory exists, etc).
  */

  if \SysIsFileDirectory(files_dir) then do
    call errmsg 'Invalid directory or directory not found: "'files_dir'"'
    _rc = 1
  end

  if \SysIsFileDirectory(dicts_dir) then do
    call errmsg 'Invalid directory or directory not found: "'dicts_dir'"'
    _rc = 1
  end

  files_dir = dirnamefmt(files_dir)       -- (appends pathsep if needed)
  dicts_dir = dirnamefmt(dicts_dir)       -- (appends pathsep if needed)
  files_dir = reltodir(files_dir)         -- (make relative to curr dir)
  dicts_dir = reltodir(dicts_dir)         -- (make relative to curr dir)

  if work_dir <> "" then do
    if \SysIsFileDirectory(work_dir) then do
      call errmsg 'Invalid directory or directory not found: "'work_dir'"'
      _rc = 1
    end; else do
      work_dir = dirnamefmt(work_dir)     -- (appends pathsep if needed)
      work_dir = reltodir(work_dir)       -- (make relative to curr dir)
      cmpout_bin = work_dir || cmpout_bin
      expout_txt = work_dir || expout_txt
    end
  end

  if _rc <> 0 then
    signal exit

  -- signal begin

/*----------------------------------------------------------------------------
                                  BEGIN
 ----------------------------------------------------------------------------*/

begin:

  call bothmsg ""
  call bothmsg cmdline    -- (show them the test they are running)
  call bothmsg ""


  /* Gather all of the files that we'll be using and convert all of their
     absolute path values to be relative to the current directory instead. */

  call SysFileTree filespec("location",files_dir) || "*.*",   "files", "SFO"
  call SysFileTree filespec("location",dicts_dir) || "*.??C", "dicts", "SFO"

  do i=1 for files.0
    files.i = reltodir(files.i)   -- (makes the filename much shorter)
  end
  do i=1 for dicts.0
    dicts.i = reltodir(dicts.i)   -- (makes the filename much shorter)
  end


  /* Now perform the test(s) using those files... */

  call bothmsg "Starting CMPSC test; " || files.0 || " files and " || dicts.0 || " dictionaries selected."
  call bothmsg ""

  call progmsg sep
  call progmsg ""

  totcmp = 0; cmpok = 0; cmperr = 0;    -- (compressions)
  totexp = 0; expok = 0; experr = 0;    -- (expansions)
  tottst = 0; tstok = 0; tsterr = 0;    -- (tests)

  rc4err.0 = 0; -- (Protection Exception during Compression)
  rc4err.1 = 0; -- (Protection Exception during Expansion)
  rc7err.0 = 0; -- (Data Exception during Compression)
  rc7err.1 = 0; -- (Data Exception during Expansion)
  md5ok    = 0; -- (MD5 Hash Matches  after Expansion)
  md5err   = 0; -- (MD5 Hash Mismatch after Expansion)


  /* Start timing... */

  call random MIN_OFFSET, MAX_OFFSET, time("Seconds")   -- (seed PRNG)
  call time("Reset")  -- START TIMING
  say sep
  say "Begin: "time()
  say sep


  /* For each file, for each dictionary... */

  do file_num=1 for files.0           -- (for each test file)

    infile = files.file_num
    call progmsg "Processing file: " || infile
    old_md5 = MD5(infile)

    do dict_num=1 for dicts.0     -- (for each compression dict)

      cdict = dicts.dict_num

      n     = length(cdict)
      edict =   left(cdict,n-1) || "E"    -- (expansion dict name)
      fmt   = substr(cdict,n-2,1)         -- (dict type or format)
      cdss  = substr(cdict,n-1,1)         -- (dict symbol size)

      call progmsg "  ...with dictionary: " || cdict


      /* Perform speed test if requested... */

      if speed <> "" then do
        ib = 0
        io = 0
        ob = 0
        oo = 0
        call dotest
        iterate
      end

      /* First use some buffer size and offset values
         known to have caused problems in the past
         with certain file/dictionary combinations
         with some implementations...
      */
      if no_nonrand = "" then do
        if num_randoms > 0 then
          call progmsg "     ...(begin non-random tests)..."
        do  ibn=1 for words(buffsizes)
            ib = word(buffsizes,ibn)
            do  ion=1 for words(offsets)
                io = word(offsets,ion)
                do  obn=1 for words(buffsizes)
                    ob = word(buffsizes,obn)
                    do  oon=1 for words(offsets)
                        oo = word(offsets,oon)
                        call dotest
                    end
                end
            end
        end
      end -- non-randoms

      /*  Now use some randomly generated
          buffer sizes and offset values... */

      if num_randoms > 0 then do
        if no_nonrand = "" then
          call progmsg "     ...(begin random sized tests)..."
        loop num_randoms
            ib = random(MIN_BUFFSIZE,MAX_BUFFSIZE)
            loop num_randoms
                io = random(MIN_OFFSET,MAX_OFFSET)
                loop num_randoms
                    ob = random(MIN_BUFFSIZE,MAX_BUFFSIZE)
                    loop num_randoms
                        oo = random(MIN_OFFSET,MAX_OFFSET)
                        call dotest
                    end
                end
            end
        end
      end -- randoms
    end -- dicts
    call progmsg
  end -- files

  secs = time("Reset")  -- STOP TIMING; GET ELAPSED TIME


  /* Print test duration */

  call duration(secs);

  if left(ddhhmmss,3) = "00:" then do   -- (any days of duration?)
    ddhhmmss = substr(ddhhmmss,4)       -- (no then remove DD: days)
    say         "End: "time()           -- (align HH of End: w/HH of Dur:)
  end; else say "End:    "time()        -- (align HH of End: w/HH of Dur:)


  /* Print TOTALS */

  n = length(format(tottst))

  call bothmsg sep
  call bothmsg ""
  call bothmsg " " || format(totcmp,n) || " Compressions: " || format(cmpok,n) || " Pass, " || format(cmperr,n)   || " Fail = " || format((cmpok/totcmp)*100,3,1) || "% Success, " || format((cmperr/totcmp)*100,3,1) || "% Failure"
  if cmperr <> 0 then do
  call bothmsg ""
  call bothmsg " " || copies(" ",n)    || "               " || copies(" ",n)   || "       " || format(rc7err.0,n) || " Data Exceptions:       " || format((rc7err.0/cmperr)*100,3,1) || "%"
  call bothmsg " " || copies(" ",n)    || "               " || copies(" ",n)   || "       " || format(rc4err.0,n) || " Protection Exceptions: " || format((rc4err.0/cmperr)*100,3,1) || "%"
  end
  call bothmsg ""
  call bothmsg " " || format(totexp,n) || " Expansions:   " || format(expok,n) || " Pass, " || format(experr,n)   || " Fail = " || format((expok/totexp)*100,3,1) || "% Success, " || format((experr/totexp)*100,3,1) || "% Failure"
  if experr <> 0 then do
  call bothmsg ""
  call bothmsg " " || copies(" ",n)    || "               " || copies(" ",n)   || "       " || format(rc7err.1,n) || " Data Exceptions:       " || format((rc7err.1/experr)*100,3,1) || "%"
  call bothmsg " " || copies(" ",n)    || "               " || copies(" ",n)   || "       " || format(rc4err.1,n) || " Protection Exceptions: " || format((rc4err.1/experr)*100,3,1) || "%"
  end
  call bothmsg ""
  call bothmsg " " || format(expok,n)  || " MD5 Compares: " || format(md5ok,n) || " Pass, " || format(md5err,n)   || " Fail = " || format((md5ok/expok)*100,3,1)  || "% Success, " || format((md5err/expok)*100,3,1)  || "% Failure"
  call bothmsg ""
  call bothmsg sep
  call bothmsg ""
  call bothmsg " " || format(tottst,n) || " TOTAL TESTS:  " || format(tstok,n) || " PASS, " || format(tsterr,n)   || " FAIL = " || format((tstok/tottst)*100,3,1) || "% SUCCESS, " || format((tsterr/tottst)*100,3,1) || "% FAILURE"
  call bothmsg ""
  call bothmsg sep
  call bothmsg "Duration:  " || ddhhmmss || "  (" || dur_eng || ")"
  call bothmsg sep


  /* We are done! */

  signal exit

/*----------------------------------------------------------------------------
                                DOTEST
 ----------------------------------------------------------------------------*/

dotest:

  tottst += 1
  fail = 0
  /*--------------------------------------------------*/
  /*              Compress the file                   */
  /*--------------------------------------------------*/
  totcmp += 1
  exp = 0  -- (0 = compression)
  qif(cmpsctst_bin)" -c -a "algorithm" "repeat" "trans" -v -i "qif(ib":"io":"infile)" -o "qif(ob":"oo":"cmpout_bin)" -d "qif(cdict)" -x "qif(edict)" -s "cdss" -"fmt
  if rc <> 0 then do
    err = rc
    call count_errors
    say "** ERROR: Compression failed! **"
    say ""
    _rc = 1
    fail = 1
    cmperr += 1
  end; else do
    cmpok += 1
    /*--------------------------------------------------*/
    /*               Expand it back                     */
    /*--------------------------------------------------*/
    totexp += 1
    exp = 1  -- (1 = expansion)
    qif(cmpsctst_bin)" -e -a "algorithm" "repeat" "trans" -v -i "qif(ib":"io":"cmpout_bin)" -o "qif(ob":"oo":"expout_txt)" -d "qif(cdict)" -x "qif(edict)" -s "cdss" -"fmt
    if rc <> 0 then do
      err = rc
      call count_errors
      say "** ERROR: Expansion failed! **"
      say ""
      _rc = 1
      fail = 1
      experr += 1
    end; else do
      expok += 1
      /*--------------------------------------------------*/
      /*                Check MD5 Hash                    */
      /*--------------------------------------------------*/
      new_md5 = MD5(expout_txt)
      if new_md5 <> old_md5 then do
        say "** ERROR: Expanded file does not match original input file! **"
        say ""
        say "   Expected MD5:  "old_md5
        say "   Actual MD5:    "new_md5
        say ""
        _rc = 1
        fail = 1
        md5err += 1
      end; else
        md5ok += 1
    end
  end

  if fail then
    tsterr += 1
  else
    tstok += 1

  say sep
  return

count_errors:  -- (exp: 0=compression, 1=expansion, err: 4=prot, 7=data, 5=md5)
  select
    when err=4 then rc4err.exp += 1    -- (Protection Exception)
    when err=7 then rc7err.exp += 1    -- (Data Exception)
    otherwise call oops
  end
  return

/*----------------------------------------------------------------------------
                         CALCULATE MD5 HASH
 ----------------------------------------------------------------------------*/

MD5: procedure expose md5sum_bin

  qif(md5sum_bin)" -b "qif(arg(1))" 2>&1 | RXQUEUE /FIFO"

  if \queued() then
    return 0

  parse pull hash .
  "RXQUEUE /CLEAR"

  do while hash <> "" & \ishex(hash)
    hash = substr(hash,2)
  end

  if hash = "" then
    hash = "???"

  return hash

/*----------------------------------------------------------------------------
                                DURATION
 ----------------------------------------------------------------------------*/
/*
            Convert seconds duration to "dd:hh:mm:ss.uuuuuu"
             *AND* to spoken/written English format too...
*/
duration: procedure expose ddhhmmss dur_eng

  ss = arg(1)           -- (duration in seconds)

  sd = 60 * 60 * 24     -- (seconds per day)
  sh = 60 * 60          -- (seconds per hour)
  sm = 60               -- (seconds per minute)

  dd = ss % sd;  ss -= (dd * sd);   -- (days)
  hh = ss % sh;  ss -= (hh * sh);   -- (hours)
  mm = ss % sm;  ss -= (mm * sm);   -- (minutes)

  ddhhmmss = ""              ,
    || right("00"||dd,2) || ":"  ,
    || right("00"||hh,2) || ":"  ,
    || right("00"||mm,2) || ":"  ,
    || changestr(" ",format(ss,2,3),"0");

  dur_eng = "";

  if dd > 0 then do
    if dd > 1 then
      dur_eng = dd || " days, "
    else
      dur_eng = dd || " day, "
  end

  if hh > 0 then do
    if hh > 1 then
      dur_eng ||= hh || " hours, "
    else
      dur_eng ||= hh || " hour, "
  end

  if mm > 0 then do
    if mm > 1 then
      dur_eng ||= mm || " minutes, "
    else
      dur_eng ||= mm || " minute, "
  end

  if ss <> 0 then do
    if ss = 1 then
      dur_eng ||= format(ss,,3) || " second, "
    else
      dur_eng ||= format(ss,,3) || " seconds, "
  end

  if dd=0 & hh=0 & mm=0 then do
    if ss=0 then
      dur_eng = "no time at all!"
    else if ss < 1 then
      dur_eng = "less than 1 second"
    else
      dur_eng = left(dur_eng,length(dur_eng) - 2)
  end; else
    dur_eng = left(dur_eng,length(dur_eng) - 2)

  return

/*----------------------------------------------------------------------------
      Return a path value that is relative to a specified directory
 ----------------------------------------------------------------------------*/

reltodir: procedure expose pathsep

  /* Convert both arguments to absolute path values... */

  if arg(1) = "" then return ""
  result = qualify(arg(1))
  absolute = result

  /* (note: dirnamefmt ensures values ENDS with a pathsep) */

  if arg(2) <> "" then
    other = qualify(dirnamefmt(arg(2)))       -- (always a directory)
  else
    other = qualify(dirnamefmt(directory()))  -- (current dir default)

  /* Find where inequality begins... */

  do i=1 by 1 while left(result,i) = left(other,i) & i <= length(other)
    nop
  end

  /* We're done if paths have everything or nothing in common */

  if i <= 1 then return arg(1)    -- (nothing in common)
  if i >= length(other) then      -- (everything in common)
    return substr(result,length(other)+1)

  /* Backup to start of directory where inequality was found.
     (Needed in case inequality in middle of directory name) */

  do i=i-1 by -1 while i >= 1 & substr(other,i,1) <> pathsep
    nop
  end

  /* Remove common portion from each, leaving just unequal part */

  other  = substr(other,i+1)
  result = substr(result,i+1)

  /* Prefix enough "up directory" sequences to the original path
     based on how many directories remain in the other directory
     that they wanted their path to be relative to. */

  updirs = countstr(pathsep,other)
  result = copies(".."||pathsep,updirs) || result
  if length(result) > length(absolute) then
    result = absolute
  return result

/*----------------------------------------------------------------------------
                         ( helper functions )
 ----------------------------------------------------------------------------*/

isnum: procedure    -- (is it a number?)

  return arg(1) <> "" & datatype(arg(1),"N");

ishex: procedure    -- (is it hexadecimal?)

  return arg(1) <> "" & datatype(arg(1),"X");

errmsg: procedure

  call lineout stderr, "ERROR: " || arg(1)
  return

warnmsg: procedure

  call lineout stderr, "WARNING: " || arg(1)
  return

progmsg: procedure  -- (prefix stderr msg w/HH:MM:SS time)

  call lineout stderr, time() || "  " || arg(1)
  return

bothmsg: procedure      -- (send to both)
  call progmsg arg(1)   -- (send to both)
  say          arg(1)   -- (send to both)
  return

qif: procedure  -- (quote if needed)

  result = strip(arg(1),,'"')
  if pos(" ",result) > 0 then
    result = '"' || result || '"'
  return result

dirnamefmt: procedure expose pathsep  -- (convert to dir format)

  result = arg(1)
  do while pos(pathsep||pathsep,result) > 0
    result = changestr(pathsep||pathsep,result,pathsep)
  end
  if qualify(result) <> qualify(result||pathsep) then
    result ||= pathsep
  return result

fullpath: procedure   -- (locate file or dir, return full path or null)

  return SysSearchPath("PATH",arg(1))

delfile: procedure    -- (delete a file if it exists)

  if RxFuncQuery("SysFileDelete") = 0 then  -- (loadtools may have failed)
    if SysIsFile(arg(1)) then
      call SysFileDelete(arg(1))
  return

oops:

  call bothmsg "** OOPS! **  (line "sigl")"
  raise user oops

/*----------------------------------------------------------------------------
                                EXIT
 ----------------------------------------------------------------------------*/

exit:

  /* Clean up... */

  call delfile cmpout_bin
  call delfile expout_txt

  exit _rc

/*-----------------------------( EOF )---------------------------------------*/

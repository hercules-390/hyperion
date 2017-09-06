 /* runtest.rexx - run a series of test cases in Hercules and analyze results  */

/*
  Copyright 2017 by Stephen Orso.

  Distributed under the Boost Software License, Version 1.0.
  See accompanying file BOOST_LICENSE_1_0.txt or copy at
  http://www.boost.org/LICENSE_1_0.txt)

  Portions of the detailed help in comments at the end of this file are
  a derivative work based on John P. Hartmann's help comments in his
  runtest script, placed by him in the public domain 5 October 2015.

  This runtest.rexx runs Hercules test scripts, uses redtest.rexx to
  analyze the results, and generate a report of the results.  This REXX
  script can be run on both Windows, mac OS, and open source/UNIX-like
  host systems.  Command line options for runtest.rexx are the same
  regardless of the host system upon which it runs.

  This script was inspired by the shell script runtest, written by
  John P. Hartmann, and the Windows batch file runtest.cmd, written
  by Fish, David B. Trout.

*/

/*
Function/Operation
  * runtest.rexx assembles a set of test case files into a single
    Hercules run commands (.rc) file, executes Hercules using a
    test configuration and analyzes the resulting Hercules log
    using the data reduction script redtest.rexx.
  * Command line options are used to identify the tests to be run,
    where the Hercules executable and its loadable modules are
    located, and where the tests scripts are located.

Input
  * Command line options, which control the operation of this script.
    See the help display function at the end of this script for
    details on command line options.
  * A directory of test cases.

Output
  * allTests.testin, a single file of test scripts consolidated from
    command line options or defaults.
  * allTests.out, the Hercules log file created when the allTests.testin
    file is used by Hercules as the .rc file via the -r Hercules command
    line option.
  * allTests.txt, the output from redtest.rexx, the Hercules log file
    analysis script.
  * The file name use by the above three files defaults to allTests, and
    that default can be overridden by a command line option.

External References & Dependencies
  * This script requires the RexxUtil package, which is included in the
    Open Object Rexx interpreter but was not integrated into Regina Rexx
    until v3.5 of Regina Rexx.
  * When used with Regina Rexx, this script must be invoked with the
    regina command to enable dynamic loading of the RexxUtil package and
    its functions.  If started with Regina rexx command, this script
    re-invokes itself using the regina command.
  * While REXX interpreters are pretty agnostic about their host system,
    especially with respect to path separators, this script cannonicalizes
    path separators to conform to the detected host system.
  * This script will use either the 'cat' command (open source and UNIX-like
    systems) or type (Windows) to append each selected test script file
    into a single allTests.testin file.
  * The strings returned by the REXX command parse source provides enough
    information to let this script identify the correct path separator and
    the correct command, 'cat' or 'type', to append scripts to the
    consolidated file.

Exits
  * Normal: return code zero if all tests run succefully.
  * Error: if command line option errors are detected, the return code
    is the number of errors detected.
  * If Hercules exits running the test scripts with a non-zero return code,
    that return code is used as this script's return code.
  * If redtest.rexx returns a non-zero return code, that return code is used
    as this script's return code.

   See the help text in this routine for options and for differences
   in options from the earlier runtest shell script and runtest.cmd
   Windows script.

   N.B., the two scripts that were the basis for this script used
   overlapping command line options with some incompatibilities.  This
   script uses the same command line options for Windows and open source
   systems.  Where conflicts existed, the open source options are
   generally used, but refer to the help documentation for specifics.
*/

/*
  Some notes about OS portability for this REXX script

    1) REXX is host system agnostic; syntax is the same whether on
      Windows or open source.  In particular, REXX handles differences
      in path separators in Windows and open source without much fuss.
      However, because some of the paths used in this script are
      passed to the underlying OS, the separators are translated into
      host-appropriate separators when encountered, typically during
      command line options processing.
    2) Where operating system commands are needed, this application
      uses cmake -E to invoke those commands in a portable manner.
      mkdir/md <dir> becomes cmake -E makedirectory <dir>.  As above,
      open source path separators where needed.
    3) An exception to this is in the creation of the test input script.
      CMake -E does not have a copy command that allows for appending.
      So if running on Windows, the type command is used, and otherwise
      cat is used.

  A note about REXX portability for this script
    1) This script is intended to be run using either Open Object Rexx
       or Regina Rexx.  When it comes to traditional REXX, the two
       interpreters are identical enough for this script.
    2) This script requires the SysFileTree() function, a part of
       Rexxutils.  Rexxutils is included in Open Object Rexx but is,
       in some distros, a separately installable package (cf. FreeBSD).
    3) Regina Rexx requires specific commands to load Rexxutils, and
       Regina Rexx allows the Rexxutils package to be loaded only when a
       script is invoked with the regina command.
    4) So if this script determines that it has been invoked under
       Regina Rexx using the rexx command, it uses address command
       to re-invoke itself using the regina command.
*/



parse arg opts              /* copy command line arguments to opts  */
parse source host invocation script_path  /* get invocation information */

/* the following rxfuncadd always returns rc=0 on ooRexx.  On Regina    */
/* Rexx, rc=60 means the script was invoked using the rexx command,     */
/* which does not support rxfuncadd()or other loadable utility          */
/* So if we get rc=60, re-invoke this script using the regina command.  */
/* To keep CMake simple, and to simplify the life of users running this */
/* runtest.rexx, we assume that reinvocation when using Regina Rexx is  */
/* the normal state of affairs.                                         */

rc = rxfuncadd( 'sysloadfuncs', 'rexxutil', 'sysloadfuncs' )
if rc \== 0 then do
    if rc == 60 then do
        address command 'regina' script_path opts
        return RC
        end
    else do
        say 'Unable to load required REXX utility function SysLoadFuncs(), rc=' rc
        say RxFuncErrMsg()
        return rc
        end
    end

call SysLoadFuncs       /* not needed on ooRexx, required for Regina Rexx   */


exe_name      = 'hercules'      /* name of the Hercules executable          */
ptrsize_name  = 'ptrsize'       /* name of the pointer size -v option       */
platform_name = 'platform'      /* name of platform redtest.rexx variable   */
ptrsize_found = 0               /* no ptrsize= in -v options yet            */
all_pass      = 'All pass.'     /* successful test message completion       */


/* -t   Specify default timeout adjustment factor in the range 1.0 to   */
/* 14.3.  Note that while we require the space between -t and the       */
/* adjustment factor, Hercules requires no space, thus:                 */
/* -t 2.2 to runtest.rexx becomes -t2.2 on its way to redtest.rexx.     */

/* 14.3 is the result, rounded to the nearest 0.1 of a second, of the   */
/* of the constants defined in hconst.h for MAX_RUNTEST_FACTOR and      */
/* MAX_RUNTEST_DUR. If those constants change, then the following       */
/* assignment statement should be changed as well. And do not think     */
/* about removing the commas from the three comment lines that follow   */

timeout_max_adj =  ,
/*                           Round to the nearest tenth-------------------|         */ ,
/*                           MAX_RUNTEST_DUR -----------------------|     |         */ ,
/*         V---------------- MAX_RUNTEST_FACTOR ---------------V    V-V   V----V    */ ,
    trunc( (((4.0 * 1024.0 * 1024.0 * 1024.0) - 1.0) / 1000000.0) / 300 + .05, 1)

/* And of course, should you discover the constants have changed,       */
/* just copy the above say into a temporary REXX script with the        */
/* new values and execute it.                                           */

if upper(left(host,3)) == 'WIN' then do
    path_sep = '\'
    wrong_path_sep = '/'
    type_or_cat = 'type'
    exe_suffix = '.exe'
    loadmod_suffix = '.dll'
    platform="Windows"
    end
else do
    path_sep = '/'
    wrong_path_sep = '\'
    type_or_cat = 'cat'
    exe_suffix = ''
    loadmod_suffix = '.so'
    address command 'uname -s | rxqueue '
    i = 0
    do while queued()
        i = i + 1
        parse pull plat.i
        end
    platform = plat.1
    drop plat.
    end

call parse_command_line

if options.help_ == 1 then  /* If help requested, then that's all one gets  */
    return 0

if options.unknown_ then do    /* Invalid option provided.             */
    call help_text              /* ..Show summary help and exit         */
    return error_count
    end

call set_option_defaults    /* set defaults for options not specified.  */

/*
    if options.d_ \== "" then
        say '-d' options.d_
    if options.e_ \== "" then
        say '-e' options.e_
    if options.h_ \== "" then
        say '-h' options.h_
    if options.p_ \== "" then
        say '-p' options.p_
    if options.q_ \== "" then
        say '-q' options.q_
    if options.r_ \== "" then
        say '-r' options.r_
    if options.t_ \== "" then
        say '-t' options.t_
    if options.v_ \== "" then
        say '-v' options.v_
    if options.w_ \== "" then
        say '-w' options.w_
    if options.x_ \== "" then
        say '-x' options.x_
*/

if error_count > 0 then do      /* any errors?  yes..abort.  */
    say "Testing not performed due to command line errors."
    return 1
    end

call validate_environment
if error_count > 0 then do      /* any errors?  yes..abort.  */
    say "Testing not performed due to above errors."
    return 1
    end

call build_test_script_file


/* After what must seem an eternity of getting things put together,     */
/* we can now run Hercules against the test scripts                     */

hercules_runtest_cmd = options.h_ || 'hercules' ,
                        '-p' options.p_ ,
                        options.l_ ,
                        options.t_ ,
                        '-f' options.d_ || 'tests.conf' ,
                        '-r' input_script ,

/* If Hercules should exit after testing (no -x option) then redirect   */
/* stderr and stdout.  The input test script already has the exit       */
/* if needed.                                                           */

if \options.x_ then
    hercules_runtest_cmd = hercules_runtest_cmd '-d >' output_log '2>&1'
else
    hercules_runtest_cmd = hercules_runtest_cmd '>' output_log


say 'Running Hercules to generate test results...'
hercules_runtest_cmd


/* if Hercules ended successfully, run the data reduction.              */

if RC = 0 then do
    say 'Performing analysis of test results...'
    save_trace = trace()
    if save_trace = 'N' then
        trace O
    'rexx' options.d_ || path_sep || 'redtest.rexx' ,
            output_log ,
            options.v_ ,
            '>' || redtest_log '2>&1'
    trace value save_trace
    end

/* Summarize results of Hercules log file reduction.  Summarize means   */
/* announcing complete success in a single line                         */

if RC == 0 then
    say 'All tests ran successfully.  See' redtest_log 'for a summary and' output_log 'for details.'
else do
    do while lines( redtest_log )
        redtest_log_line =  linein( redtest_log )
        if right( redtest_log_line, length( all_pass ) ) \== all_pass then
            say redtest_log_line
        end /* do while lines( redtest_log ) */
    say 'See file' output_log 'for additional information'
    end /* else, RC \== 0  */

return RC


/*
    Built the input test case file.  test_script_list. has the list of
    file information as returned by SysFileTree.  The -r option in
    option.r_ has the repeat count.  That's it.
*/

build_test_script_file:

/*  Test script preamble: initialize input and output file names, make  */
/*  sure the files are empty by deleting them                           */

    input_script = options.w_ || '.testin'
    output_log   = options.w_ || '.out'
    redtest_log  = options.w_ || '.txt'
    address command 'cmake -E remove' input_script
    address command 'cmake -E remove' output_log
    address command 'cmake -E remove' redtest_log

    Say 'Building test script file' "'" || input_script || "'..."


/*  More preamble: provide the test case directory in a symbol.         */

    rc = lineout( input_script, '* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *' )
    rc = lineout( input_script, '' )
    rc = lineout( input_script, '* Hercules runtest script created' date() time('N') )
    rc = lineout( input_script, '' )
    rc = lineout( input_script, 'defsym testpath "' || strip(options.d_, 'T', path_sep) || '"')
    rc = lineout( input_script, '' )
    rc = lineout( input_script, '* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *' )


/* copy each test script into the input_script file to the Hercules     */
/* execution script the number of times specified or defaulted to by    */
/* the repeat count option -r.                                          */

    do i = 1 to test_script_list.0
        parse var test_script_list.i script_date script_time script_size script_attr script_fullpath
        script_name = filespec('N', script_fullpath)
        script_path = filespec('D', script_fullpath) || filespec('P', script_fullpath)
        /* REXX does not care about path separators matching the host OS.   */
        /* But the OS command we will issue does.  So ensure only correct   */
        /* path separators                                                  */
        script_fullpath = translate( script_fullpath, path_sep, wrong_path_sep)

        rc = lineout( input_script, '' )
        rc = lineout( input_script, '' )
        rc = lineout( input_script, '* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *' )
        rc = lineout( input_script, '*' )
        rc = lineout( input_script,  '* Start of test file' script_name 'date' script_date script_time 'in' script_path )
        rc = lineout( input_script,  '*' )
        rc = lineout( input_script, '* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *' )
        rc = lineout( input_script, '' )
        do j = 1 to options.r_
            rc = lineout(input_script)
            type_or_cat script_fullpath '>>' input_script
            if j < options.r_ then do
                rc = lineout( input_script, '' )
                rc = lineout( input_script, '' )
                rc = lineout( input_script, '* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *' )
                rc = lineout( input_script, '*' )
                rc = lineout( input_script, '* Repetition' j+1 'of test file' script_name )
                rc = lineout( input_script, '*' )
                rc = lineout( input_script, '* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *' )
                rc = lineout( input_script, '' )
                end   /* if j < options.r_  */
            end   /* do j = 1 to options.r_ */

        end  /* do i = 1 to test_script_list.0   */

    rc = lineout( input_script, '' )
    rc = lineout( input_script, '' )
    rc = lineout( input_script, '* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *' )
    rc = lineout( input_script, '' )
    rc = lineout( input_script, '* End of Hercules test case input script'  )
    rc = lineout( input_script, '' )
    rc = lineout( input_script, '* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *' )

    if \options.x_ then do
        rc = lineout( input_script, '' )
        rc = lineout( input_script, '' )
        rc = lineout( input_script, 'exit' )
        end

    call stream input_script, 'C', 'close'

    return 0



call validate_environment
if error_count > 0 then do
    say "Testing not performed due to above errors."
    return 1
    end

call build_herc_command

return 0



/*
    Build the hercules command line such that it can be executed with
    address command.  This includes a full path to the executable and
    all options.
*/

build_herc_command:

return 0


/*
    ********************************************************************
    Parse command line options.

    The -e (extension) and -f (test script name) options may be repeated
    and are processed into a single string of file names.  Test script
    names that lack an extension have the most recent -e value appended.

    The -l option may be repeated and is assembled into a list of
    Hercules loadable modules that will be loaded to run the tests.

    The -v option may be repeated and is assembled into a string of
    variable=value assignments to be passed to redtest.

    Other options may be repeated, but the last one wins.
    ********************************************************************
*/

parse_command_line:

    error_count = 0
    options.    = ""           /* set all options to null               */
    options.e_  = ".tst"       /* set default extension                 */
    options.f_.0 = 0           /* zero count of test scripts            */
    options.l_.0 = 0           /* zero count of loadable modules named  */
    options.q_   = 0           /* set quiet flag to false               */
    options.v_.0 = 0           /* zero count of -v arguments provided   */
    options.unknown_ = 0       /* set unknown option flag to false      */
                               /* set name of dyncrypt on this system   */
    dyncrypt_mod = 'dyncrypt' || loadmod_suffix

    do  while opts \== ''      /* process entire argument string.       */
    parse var opts x opts      /* get the next command line option      */

    select

/*
    -d   Specify test case directory.
*/
        when x == '-d' then do   /* test case directory                 */
            options.d_ = validate_path( "-d test script path" )
            end   /* when x == '-d'   */

/*
    -e  Specify default extension for test case files.  This option may
        be specified multiple times, and each time it is specified, it
        used for all succeeding -f options that do not include an
        extension until the next -e option is specified.  If the option
        is missing the leading '.', no matter: just add it.
*/
                               /* default test case filename extension  */
        when x=='-e' then do
            parse var opts options.e_ opts
            if left( options.e_, 1 ) \== '.' then
                options.e_ = '.' || options.e_
            end


/*
    -f   Specify test case file name.  If no extension is included, then
        the extension specified in the last -e (or the default for -e if
        no -e commmand line option was specified yet) is used as the
        extension.
*/
        when x=='-f' then do   /* provide name of a test script.        */
        /* Note: globs are OK, as are absolute or relative paths        */

            option_value = translate( parse_quoted_string(), path_sep, wrong_path_sep )

            if lastpos('.', option_value) <= lastpos(path_sep, option_value) then
                option_value = option_value || options.e_
            options.f_.0 = options.f_.0 + 1
            i = options.f_.0
            options.f_.i = option_value
            end


/*
    -h  Specify Hercules executable directory.  The default is the
        working directory.  And this directory, specified or defaulted,
        is used as the basis for the default for the Hercules loadable
        modules directory -p.
*/
        when x == '-h' then do   /* Hercules executable directory       */
            options.h_ = validate_path( "-h Hercules executable path" )
            if options.h_ \== '' then do
                rc = SysFileTree( options.h_ || exe_name || exe_suffix, sftresult, 'F' )
                if sftresult.0 == 0 then do
                    say "-h Hercules executable '" || exe_name || exe_suffix ,
                            || "' not found in '" || options.h_ "'"
                    options.h_ = ''
                    error_count = error_count + 1
                    end  /* sftresult.0 == 0   */
                end /* options.h_ \== '' */

            end   /* when x == '-h'   */


/*
    -l  Specify a loadable module.  The module will be loaded from the
        directory specified by or defaulted for the -p command line
        option.  Append the load module suffix if not already provided,
        and accumulate the loadable modules in a stem options.l_.  An
        attempt to load dyncrypt will be rejected; Hercules loads it
        automatically on start-up.
*/
        when x == '-l' then do   /* provide name of a loadable module   */
            options.l_.0 = options.l_.0 + 1
            i = options.l_.0
            options.l_.i = parse_quoted_string()
            if loadmod_suffix \== '' ,
                    & right(options.l_.i, lenght(loadmod_suffix)) \== loadmod_suffix then
                options.l_.i = options.l_.i || loadmod_suffix
            if right( options.i_.i, length( dyncrypt_mod ) ) == dyncrypt_mod then do
                say 'Dyncrypt is loaded by Hercules and cannot be specified in -l'
                error_count = error_count + 1
                end
            end  /* x == '-l'  */


/*
    -p  Specify loadable module directory.  If not specified, the
        default is determined below based on the the Hercules executable
        directory and whether Hecules was built with libtool support.
*/
        when x == '-p' then do   /* loadable module directory           */
            options.p_ = validate_path( "-p loadable module path " )
            end   /* x == '-p' */


/*
    -q   Pass -v QUIET to make redtest.rexx run more quietly.
*/
        when x == '-q' then    /* redtest to run quietly if -q          */
            options.q_ = 1


/*
    -r   Specify the repeat count for the test cases to be run.
*/
        when x == '-r' then do   /* Repeat test cases n times          */
            options.r_ = parse_quoted_string()
            if \datatype(options.r_, 'N') then do
                say "-r test repeat count value '" || options.r_ || "' not numeric"
                error_count = error_count + 1
                end
            end   /* x == '-r' */


/*
    -t   Specify default timeout adjustment factor in the range 1.0 to
         14.3.  Note that while we require the space between -t and the
         adjustment factor, Hercules requires no space.

         -t 2.2 to runtest.rexx becomes -t2.2 on its way to redtest.rexx.

         14.3 is the result, rounded to the nearest 0.1 of a second, of
         the constants defined in hconst.h; see above for the details.
*/
        when x == '-t' then do   /* default timeout value                 */
            options.t_ = parse_quoted_string()
            if \datatype(options.t_, 'N') then do
                say "-t timeout adjustment factor '" || options.t_ || "' not numeric"
                error_count = error_count + 1
                end
            else do
                if options.t_ < 1.0 || options.t_ > timeout_max_adj then do
                    say "-t timeout adjustment factor '" || options.t_ || "' must be between 1.0 and' timeout_max_adj 'inclusive"
                    error_count = error_count + 1
                    end
                else
                    options.t_ = '-t' || options.t_
                end  /* else, options.t_ numeric  */
            end    /* x == '-t' */

/*
    -v  Specify variables and values to be provided to the Hercules
        environment while the test cases are being run. <var>=<value>
        is the syntax.  The -v option may be specified multiple times,
        and each variable/value is passed to Hercules.
*/
        when x == '-v' then do /* provide vars & values to redtest      */
                               /* and if -v ptrsize= found, indicate    */
                               /* this for validate_environment().      */
            option_value = parse_quoted_string()
            if left(option_value,length(ptrsize_name)) == ptrsize_name then
                ptrsize_found = 1
            if left(option_value,length(platform_name)) == platform_name then do
                say 'platform variable for redtest.rexx cannot be overridden: ' ,
                        "'-v " || option_value || "'"
                error_count = error_count + 1
                end
            else
                options.v_ = options.v_ option_value

            end   /* x == '-v' */


/*
    -w  Specify the test case filename of the .rc file used to start
        Hercules.  The test cases specified for this execution of
        are concatenated to this file.
*/
        when x == '-w' then    /* Name the test workfile                */
            options.w_ = parse_quoted_string()


/*
    -x  Leave Hercules running after test case script execution.
*/
        when x == '-x' then    /* Leave Hercules running after tests    */
            options.x_ = 1

/*
    --help   display summary runtest.rexx help.
*/
        when x == '--help'   then do
            options.help_ = 1
            call help_text
            return 0
            end


/*
    --helplong   display detailed runtest.rexx help.
*/
        when x == '--helplong'   then do
            options.help_ = 1
            call help_text
            call helplong_text
            return 0
            end

        otherwise do
            say 'option' x 'is not a known option.'
            options.unknown_ = 1
            error_count = error_count + 1
            end
        end     /*select*/
    end        /*do while*/

    return error_count


/*
    ********************************************************************
    Extract and validate a path name that is the operand of the current
    command line path option, -d, -h, or -p.
    1) Cannonicalize path separators for the target system
    2) Ensure that the path exists and is a directory
    3) Ensure that wildcards were not used in the path name.
    Input: A string to be used in error messages if needed.
    Output passed by return: the validated path name
    ********************************************************************
*/

validate_path:

    parse arg path_desc .

    path_name = translate( parse_quoted_string(), path_sep, wrong_path_sep)

    rc = SysFileTree( path_name, sftresult, 'D' )    /* does path exist?  */

    select
        when sftresult.0 = 1 then do    /* one directory returned */
            if right(path_name,1) \== path_sep then  /* did path end with a separator */
                path_name = path_name || path_sep  /* no, end path with / or \      */
            end  /* sftresult.0 = 1  */

        when sftresult.0 == 0 then do   /* no directories returned; path does not exist */
            say path_desc "'" || path_name|| "' is not a directory"
            path_name = ''
            error_count = error_count + 1
            end  /* sftresult.0 == 0  */

        when sftresult.0 > 1 then do   /* multiple directories returned */
            say path_desc "'" || path_name || "' cannot contain wildcards"
            path_name = ''
            error_count = error_count + 1
            end  /* sftresult.0 > 1  */

        end  /* select  */

    return path_name


/*
    ********************************************************************
    Extract the next value from the command line string.  It may be
    that the next value is a quoted string, which REXX does not have
    too much support for.  So we do it ourselves.
    ********************************************************************
*/

parse_quoted_string:

    delim = left(opts, 1)
    if (delim == '"' | delim == "'") then do
        parse var opts . (delim) option_value (delim) opts
        end
    else
        parse var opts option_value opts

    return  option_value


/*
    ********************************************************************
    set defaults for options that have them.   This must be done after
    options processing because some defaults are based on the defaulted
    _or specified_ value of other options, notably -h.
    ********************************************************************
*/

set_option_defaults:

    /* default test script directory is the directory from which this   */
    /* script runs.  It is assumed that this script is running from     */
    /* the tests directory of the Hercules source directory             */
    if options.d_ == "" then
        options.d_ = filespec( 'D', script_path ) ,
                || filespec( 'P', script_path )

    /* The default Hercules executable directory is the current working */
    /* directory if an executable is present there.  Otherwise, it is   */
    /* the parent of the directory specified or defaulted for -d.       */

    if options.h_ == "" then do  /* try the current working directory   */
        options.h_ = directory() || path_sep
        rc = SysFileTree( options.h_ || exe_name || exe_suffix, sftresult, 'F' )
        if sftresult.0 == 0 then do
            options.h_ = filespec( 'D', options.d_ ) ,
                    || filespec( 'P', left(options.d_, length( options.d_) - 1 ) )
            say options.h_ ' from ' options.d_
            rc = SysFileTree( options.h_ || exe_name || exe_suffix, sftresult, 'F' )
            if sftresult.0 == 0 then do
                say "Hercules executable '" || exe_name || exe_suffix ,
                        || "' not found in default location '" || options.h_ "'"
                options.h_ = ''
                error_count = error_count + 1
                end  /* sftresult.0 == 0 when testing parent of -d  */
            end /* sftresult.0 == 0 when testing the current working directory  */
        end /* options.h_ == "" */


    if options.f_.0 == 0 then do  /* set up default test script name    */
        options.f_.0 = 1
        options.f_.1 = '*' || options.e_
        end

    if options.q_ then        /* if -q, then add -v quiet to redtest    */
        options.v_ = '-v quiet' options.v_      /* add it first         */

    if options.r_ = '' then   /* default repetition count is 1          */
        options.r_ = 1

    if options.t_ = '' then   /* default timeout adjustment is 1        */
        options.t_ = '-t1'

 /* Determine the default for the loadable modules directory (-p).  On  */
 /* On Windows, and on open source/UNIX-like systems when libtool is    */
 /* not used, the default is the Hercules executable directory.  When   */
 /* Hercules was built to use libtool, the default is the .lib          */
 /* subdirectory of the executable directory.                           */

 /* If we find a .libs subdirectory that includes a 'hercules' file,    */
 /* then we assume a libtool build.                                     */

    if options.p_ == "" then   /* default dir for loadable modules.     */
        options.p_ = options.h_
        if platform \== 'Windows' then do
            call SysFileTree options.h_ || '.libs' || path_sep ,
                    || 'hercules' || exe_suffix, sftresult, 'F'
            if sftresult.1 == 1 then
                options.p_ = options.p_ || ".libs" || path_sep
            end

    if options.w_ == "" then        /* use default script file name     */
        options.w_ = "allTests"

    if options.x_ == "" then        /* ensure REXX boolean for -x       */
        options.x_ = 0

    return 0


/*
    ********************************************************************
    Perform checks on the environment to ensure that runtest has a
    chance of working.  This includes:
    1) if not set in a -v option, getting the pointer size from config.h
    2) validating that all specified test scripts exist.
       Note: if there is wildcarding of test script names, the wildcards
       are expanded into a list of specific file names.
    3) validating that all specified load modules exist.
    ********************************************************************
*/

validate_environment:

    /* Ensure the tests directory has a tests.conf file, which is       */
    /* used when running Hercules for test scrpt execution              */
    rc = SysFileTree( options.d_ || 'tests.conf', sftresult, 'F' )
    if sftresult.0 \== 1 then do
        say 'Hercules configuration file tests.conf not found in' ,
                || "'" options.d_ || "'"
        error_count = error_count + 1
        end

    /* Ensure the tests directory has redtest.rexx, the script that     */
    /* does the reduction of the Hercules log into test results.        */
    rc = SysFileTree( options.d_ || 'redtest.rexx', sftresult, 'F' )
    if sftresult.0 \== 1 then do
        say 'Log reduction script redtest.rexx not found in' ,
                || "'" options.d_ || "'"
        error_count = error_count + 1
        end


    /*  If the caller did not provide a -v ptrsize= option, determine   */
    /*  the pointer size by looking for SIZEOF_SIZE_P in config.h.      */
    /*  get_config_h_ptrsize() does all diagnostics and issues any      */
    /*  error messages.                                                 */

    if \ptrsize_found then do
        ptrsize = get_config_h_ptrsize()
        if ptrsize \== '' then
            options.v_ = options.v_ 'ptrsize=' || ptrsize
        else do
            say 'Unable to find SIZEOF_SIZE_P in config.h to set prtsize'
            error_count = error_count + 1
            end
        end

    /* Ensure all named test scripts exist.  We cannot do this as we    */
    /* process the -f options because -d has to be set/defaulted first. */
    /* We also unwind any globbing that exists in the -f option(s)      */
    /* provided so that the copy routine can insert file separator      */
    /* comments between each file.                                      */
    /*                                                                  */
    /* The test script list includes all of the information returned    */
    /* by SysFileTree because we will need some of it later when        */
    /* building the script to be run by Hercules.                       */

    test_script_list. = ''
    test_script_list.0 = 0

    do i = 1 to options.f_.0   /* iterate through script list  */
        if pos(path_sep, options.f_.i) == 0 then
            test_script_path = options.d_ || options.f_.i
        rc = SysFileTree( test_script_path, sftresult, 'FL' )

    /* if there was more than one result, then the -f option included a */
    /* wildcard.  Sort the test script names into alpha sequence, case  */
    /* insensitive.  (Case insensitivity is a kludge; ooRexx and Regina */
    /* Rexx use different parameters for SysStemSort, and case          */
    /* insensitive sorting means the same call parameters can be used.  */
    /* The following also depends on the test script path starting in   */
    /* same column for all results returned by the call to SysFileTree. */

        if sftresult.0 > 1 then do
            fullpath_start = wordindex( sftresult.1, 5)
            call SysStemSort 'sftresult.', 'a', 'i', 1, sftresult.0, fullpath_start
            end

        do j = 1 to sftresult.0
            test_script_list.0 = test_script_list.0 + 1
            k = test_script_list.0
            test_script_list.k = sftresult.j
            end

    /* no matches for -f value.  Issue an appropriate diagnostic.  This */
    /* means checking for a wildcard in the -f option value.            */

        if sftresult.0 == 0 then do
            if pos('*', test_script) > 0 then  /* globbing?  */
                say "No matches for pattern '" || options.f_.i ,
                        || "' in directory '" || options.d_ || "'"
            else     /* yes, make message meaningful   */
                say "Cannot find script '" || options.f_.i ,
                        || "' in test directory '" || options.d_ || "'"
            error_count = error_count + 1
            end  /* if sftresult.0 == 0   */

        end   /* do i = 1 to options.f_.0   */

    /* Ensure all named load modules exist.  We cannot do this as we    */
    /* process the -l options because -p has to be set/defaulted first, */
    /* and -p in turn depends on -d having been set or defaulted.       */
    /*                                                                  */
    /* Note that if loadable modules are required to have a suffix,     */
    /* processing in the -h command line option has ensured it is       */
    /* there.                                                           */
    /*                                                                  */
    /* While validating the existence of the load modules, the Hercules */
    /* command line option identifying loadable modules is built.       */

    options.l_ = ''
    do i = 1 to options.l_.0

        load_module_path = options.p_ || options.l_.i
        rc = SysFileTree( load_module_path, sftresult, 'F' )

        select
            when sftresult.0 == 0 then do
                say "Cannot find loadable module '" || options.l_.i ,
                        || "' in directory '" || options.p_ || "'"
                error_count = error_count + 1
                end  /* if sftresult.0 == 0   */

            when sftresult.0 > 1 then do
                say "Wildcards are not allowed in load module names: '" ,
                        || options.l_.i || "'"
                error_count = error_count + 1
                end  /* if sftresult.0 > 1   */

            otherwise
                opttions.l_ = '-l' options.l_.i

            end /*  select  */

        end   /*  do i = 1 to option.l_.0  */

return error_count


/*
    ********************************************************************
    scan through config.h used to build Hercules looking for the
    #define for SIZEOF_SIZE_T.  When found, return that value.
    Config.h is expected to be in the executable directory.

    The config.h file is assumed to exist in the Hercules executable
    directory, which is also assumed to be the Hercules build directory.
    If these assumptions are not valid, for example when running runtest
    using the system-installed hercules, a -v ptrsize=4 (or 8) will
    avoid any issues.
    ********************************************************************
*/

get_config_h_ptrsize:

    config_h_path = options.h_ || 'config.h'
    rc = SysFileTree( config_h_path, sftresult, 'F' )
    if sftresult.0 \== 1 then do
        say 'unable to find config.h in Hercules executable directory' ,
                "'" || options.h_
        error_count = error_count + 1
        return ''
        end

    /* The following code is subject to false positive detection of     */
    /* SIZEOF_SIZE_P inside of instream comments.  But because we can   */
    /* control how config.h is built, we can document a restriction     */
    /* on starting '#define SIZEOF_SIZE_T n' in column 1 of a comment.  */

    sizeof_size_p = ''
    do while lines(config_h_path) > 0 & sizeof_size_p == ''
        config_h_line = space( strip( linein( config_h_path ) ) )
        if left(config_h_line, 8) == '#define ' then do
            parse var config_h_line define_name sizeof define_value
            if ( define_name == '#define' & sizeof == 'SIZEOF_SIZE_T' ) then
                sizeof_size_p = define_value
            end /* if '#define' */
        end /* while lines(config_h_path) > 0 & sizeof_size_p == '' */

    return sizeof_size_p


/*
    ********************************************************************
    Display summary help for this command.
    ********************************************************************
*/

help_text:

    script_name = filespec( "N", script_path )
    script_pad  = copies(' ', length(script_name) )

    say ""
    say script_name "- run Hercules to process one or more test cases and analyse results"
    say ""
    say script_name "-d <testdir_path>"
    say script_pad  "-e <default_extension>"
    say script_pad  "-f <test_script> [-f <test_script> ]..."
    say script_pad  "-h <herc_bindir_path>"
    say script_pad  "-l <herc_module> [-l <herc_module> ]..."
    say script_pad  "-p <loadmod_path>"
    say script_pad  "-q       # run tests quietly"
    say script_pad  "-r <repeat_count>"
    say script_pad  "-t <timeout_adjustment_factor>"
    say script_pad  "-v <redtest_var>=<redtest_varval>  [-v <redtest_var>=<redtest_varval> ]..."
    say script_pad  "-w <workfile_name>"
    say script_pad  "-x           # do not quit Hercules at end of tests"
    say script_pad  "--help       # display this help and exit."
    say script_pad  "--helplong   # display verbose help and exit."
    say ""
    say ""

    return 0



/*
    ********************************************************************
    Display detailed help for this runtest.rexx.

    Derived from John P. Hartmann's runtest shell script, which he
    placed in the public domain on 5 Oct 2015.

    ********************************************************************
*/

helplong_text:

    say ''
    say ''
    say '-d <testdir_path>'
    say '   Specify the path to the test directory.  The default is the directory'
    say '   that runtest.rexx is running from (not the working directory),'
    say '   perhaps ../hyperion/tests, and currently' ,
                filespec( 'D', script_path ) || filespec( 'P', script_path ) || '.'
    say '   <path> may be absolute or relative to the current working directory.'
    say '   If -d is specified multiple times, the last one is used.'
    say ''
    say ''

    say '-e <default_extension>'
    say '   Specify the extension for test scripts specified by -f that do not'
    say '   have an extension.  ".tst" is the initial default value for -e.'
    say '   The current value of the -e flag is active for all test scripts'
    say '   named in -f flags until the next -e flag.  For example:'
    say ''
    say '      runtest.rexx -f file1 file2.txt -e sptst -f file3 file4'
    say ''
    say '   Will run the following test scripts:'
    say ''
    say '      file1.tst file2.txt file3.sptst file4.sptst'
    say ''
    say '   If you do not include the "." at the beginning of the extension,'
    say '   it will be added for you.  -e may be specified as many times as'
    say '   needed.'
    say ''
    say ''

    say '-f <test_script>'
    say '   Specify the the name of a test script.  If an extension is not'
    say '   included, the extension specified in the most recent -e option,'
    say '   or ".tst" if no -e option preceeds the -f.  Wildcards are allowed.'
    say '   An absolute or relative path may be included.  Wildcards are'
    say '   allowed.  Any relative path is relative to the current working'
    say '   directory.  The default is "-f  *.tst" to run all tests in the'
    say '   test directory.  Multiple -f flags are accepted.'
    say ''
    say ''

    say '-h <herc_bindir_path>'
    say '   Specify the directory that contains the Hercules executable.'
    say '   If -h is specified, then the Hercules executable must exist'
    say '   in that directory, and that executable will be used to run the'
    say '   test scripts.  If -h is not specified and a Hercules executable'
    say '   exists in the current working directory, that executable is'
    say '   used.  Otherwise, the the hercules executable in the parent'
    say '   of the test script directory ("-d" option) is used'
    say ''
    say ''

    say '-l <herc_module>'
    say '   Specify the name of a module to load dynamically into hercules.  This'
    say '   is passed to Hercules.  Hercules loads module dyncrypt at start-up,'
    say '   so dyncrypt cannot be specified here.  You can specify multiple -l'
    say '   flags; they will be accumulated.'
    say ''
    say ''

    say '-p <loadmod_path>'
    say '   Specify the library path for loadable modules.  If the directory'
    say '   from which the Hercules executable is loaded contains a ".libs"'
    say '   subdirectory that also contains a Hercules executable, that ".libs"'
    say '   directory is the default loadable module.  Otherwise the default'
    say '   loadable module path is the directory containing the Hercules'
    say '   executable.'
    say ''
    say ''

    say '-q'
    say '   Pass "-v quiet" to redtest.rexx to suppress details about test cases.'
    say '   -q can be specified multiple times, but the effect is the same whether'
    say '   it is specified once or more than once.'
    say ''
    say ''

    say '-r <repeat_count>'
    say '   Repeat each test script n times.'
    say ''
    say ''

    say '-t <timeout_adjustment_factor>'
    say '   The timeout adjustment factor is passed to Hercules to increase the'
    say '   amount of time a test script can run before being canceled.  The'
    say '   factor is a number between 1.0 and 14.3 inclusive.  The maximum'
    say '   is determined by constants coded in Hercules.  The default is 1.0.'
    say ''
    say ''

    say '-v <redtest_var>=<redtest_varval>'
    say '-v quiet'
    say '   Set a variable for redtest.rexx to a specified value, or pass the'
    say '   flag "quiet" to reduce the number of messages generated by'
    say '   redtest.rexx.  Variables set with -v are available to test scripts'
    say '   when those scripts are run using runtest.rexx and redtest.rexx.'
    say ''
    say '-v ptrsize=4 | 8'
    say '   Provides the size of a host system pointer to redtest.rexx.  One'
    say '   current test,"mainsize.tst," requires ptrsize to interpret'
    say '   messages issued by Hercules to main storage size commands.'
    say '   Specify 4 for a 32-bit host system and 8 for a 64-bit host system.'
    say '   The default is the value of SIZEOF_SIZE_P in the config.h file'
    say '   in the Hercules executable directory.  If config.h cannot be'
    say '   located and -v ptrsize= is not specified, an error message is'
    say '   issued and no tests are run.'
    say ''
    say ''

    say '-w <workfile_name>'
    say '   Specify the file name for the three work files created by this'
    say '   script.  The files are:'
    say '      <workfile_name>.testin - consolidated test script file'
    say '      <workfile_name>.out    - Hercules log from test execution'
    say '      <workfile_name>.txt    - redtest log results.name.'
    say '   The default work file name is "allTests".'
    say ''
    say ''

    say '-x'
    say '   Do not append the quit command the composite test script.'
    say ''

/*
     ####  End of material from John P. Hartmann's runtest shell script.
*/

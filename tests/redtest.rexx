/* Process a test case                                               */
/*                                John Hartmann 22 Sep 2015 16:27:56 */

/*********************************************************************/
/* This  file  was  put into the public domain 2015-10-05 by John P. */
/* Hartmann.   You can use it for anything you like, as long as this */
/* notice remains.                                                   */
/*                                                                   */
/* Process the log file from a Hercules test run.                    */
/*                                                                   */
/* One challenge is that messages are issued from different threads. */
/* Thus the *Done message may overtake, e.g., message 809.           */
/*********************************************************************/

Signal on novalue

parse arg in opts
values. = ''
quiet = 0                             /* Log OK tests too            */

Do while opts\=''
   parse var opts w opts
   parse var w var '=' +0 eq +1 value
   Select
      When w = 'quiet'
         Then quiet = 1
      when eq \= '='
         then say 'Not variable assignment in argument string: ' w
      otherwise
         values.var = value
         If \quiet
            Then say 'Variable' var 'is set to "'value'".'
   End
End

testcase = '<unknown>'
comparing = 0
fails. = 0                            /* .0 is OK .1 is failures     */
catast = 0                            /* Catastrophic failures       */
done = 0                              /* Test cases started          */
stardone = 0                          /* *Done orders met            */
lineno = 0                            /* Line number in input file   */
ifnest=0                              /* If/then/else nesting        */
ifstack = ''
active = 1                            /* We do produce output        */
hadelse = 0

/*********************************************************************/
/* Process the test output.                                          */
/*********************************************************************/

do while lines(in) > 0
   l = readline()
   If l \= ''
      Then call procline l
end

Select
   When fails.1 = 0 & catast = 0
      Then msg = 'All OK.'
   otherwise
      msg = fails.1 'failed;' fails.0 'OK.'
end

say 'Done' done 'tests. ' msg
If catast > 0
   Then say '>>>>>' catast 'test failed catastrophically.  Bug in Hercules or test case is likely. <<<<<'
if done \= stardone then say 'Tests malformed. ' done '*Testcase orders met, but' stardone '*Done orders met.'
exit fails.1

/*********************************************************************/
/* Read a line of input and strip timestamp, if any.                 */
/*********************************************************************/

readline:
do forever
   l = ''
   If 0 = lines(in)
      Then return ''                  /* EOF                         */
   lineno = lineno + 1
   l = linein(in)
   /* Another  way  to  handle  a prefix to the response might be to */
   /* look for HHC.                                                  */
   parse value word(l, 1) with fn '(' line ')' +0 rpar
   If rpar = ')'
      Then
         Do
            parse var l . l           /* Drop debug ID               */
            l = strip(l)
         end
   parse var l ?msg ?verb ?rest
   p = verify(?msg, "0123456789:")    /* Leading timestamp?          */
   If p = 0                           /* Just a timestamp or nothing */
      Then iterate
   If p > 1                           /* Timestamp prefix            */
      Then l = substr(l, p)           /* Toss it                     */
   If left(l, 3) \='HHC'
      Then iterate                    /* Not a message               */
   If ?msg = 'HHC00007I'              /* Where issued                */
      Then iterate
   If ?msg \= 'HHC01603I'
      Then leave                      /* Not a comment               */
   If length(?verb) > 1 & left(?verb, 1) = '*'
      Then leave                      /* Potential order.            */
end
return l

/*********************************************************************/
/* Process the line                                                  */
/*********************************************************************/

procline:
parse arg l
parse var l msg verb rest

Select
   When left(msg, 3) \= 'HHC'
      Then nop                        /* Toss all non-messages.      */
   When msg = 'HHC00801I'
      Then
         Do
            parse var rest ' code ' havepgm . ' ilc'
         End
   When msg = 'HHC00803I'          /* Program interrupt loop      */
      Then havewait = 1
   When msg = 'HHC00809I'
      Then call waitstate
   When msg = 'HHC01405E'
      then call test 0, l
   When msg = 'HHC01417I'
      Then
         If verb = 'Machine'
            Then
               Do
                  parse var rest 'dependent assists:' rest
                  Do while rest \= ''
                     parse var rest fac rest
                     value.fac = 1
                     /* say 'facility' fac */
                  End
               End
   When msg = 'HHC01603I'
      Then
         If left(verb, 1) = '*'
            Then call order
   When msg = 'HHC02269I'
      then call gprs
   When msg = 'HHC02277I'
      Then parse var rest . prefix .
   When (msg = 'HHC02290I' | msg = 'HHC02291I')
      Then
         If comparing
            Then
               Do
                  parse var rest 'K:' key .
                  If key = ''
                     Then
                        Do
                           parse var rest display +36 /* Save for compare order */
                           lastaddr = verb
                        end
                     else
                        Do
                           keyaddr = substr(verb, 3)
                           lastkey = key
                        End
               End
   When left(msg, 8) = 'HHC02332'
      Then
         Do
            If \timeoutOk
               Then catast = catast + 1
            call test timeoutOk, 'Test case timed out in wait.',,
               'This is likely an error in Hercules.  Please report'
         end
   When \comparing /* Toss other messages, particularly during start */
      Then nop
   otherwise                     /* Something else.  Store for *Hmsg */
      ? = lastmsg.0 + 1
      lastmsg.? = l
      lastmsg.0 = ?
end
return

/*********************************************************************/
/* Decode orders.                                                    */
/*********************************************************************/

order:
info = ''
rest = strip(rest)
If left(rest, 1) = '"'
   Then parse var rest '"' info '"' rest
Select
   When verb = '*'                    /* Comment                     */
      Then nop
   When verb = '*Testcase'
      Then call begtest
   When verb = '*If'
      Then call doif
   When verb = '*Else'
      Then call doElse
   When verb = '*Fi'
      Then call endif
   when \active
      Then nop
   When verb = '*Compare'
      Then
         Do
            comparing = 1
            lastmsg.0 = 0
         End
   When verb = '*Want'
      Then call want
   When verb = '*Error'
      Then call msg 'Error'
   When verb = '*Info'
      Then call msg 'Informational'
   When verb = '*Hmsg'
      Then call msg 'Panel message'
   When verb = '*Explain'
      Then call explain
   When verb = '*Gpr'
      Then call wantgpr
   When verb = '*Key'
      Then call wantkey
   When verb = '*Message'
      Then
         If active
            Then say rest
   When verb = '*Prefix'
      Then call wantprefix
   When verb = '*Program'
      Then wantpgm = rest
   When verb = '*Done'
      Then call endtest
   When verb = '*Timeout'
      then timeoutOk = 1
   otherwise
      say 'Bad directive: ' verb
      rv = 16
end
return

/*********************************************************************/
/* Initialise variable store for new test case                       */
/*********************************************************************/

begtest:
done = done + 1
testcase = rest
comparing = 0
havewait = 0
oks = 0
wantpgm = ''
havepgm = ''
rv = 0
pgmok = 0                             /* Program check not expected  */
gpr.=''
lastkey = ''
keyaddr = '<unknown>'
lastaddr = '<unknown>'
prefix = ''
lastmsg.0 = 0
lasterror.0 = 0
lasterror = ''
lastinfo.0 = 0
lastinfo = ''
expl.0 = 0
timeoutOk = 0
lastmsg.0 = 0

return

/*********************************************************************/
/* Wind up a test.                                                   */
/*********************************************************************/

endtest:
stardone = stardone + 1
unprocessed = ''                      /* No read ahead stored        */
nowait = rest = 'nowait'

If \havewait & \nowait
   Then call lookahead             /* Perhaps *Done overtook message */

Select
   When havepgm \= ''
      Then call figurePgm
   otherwise
end

If \havewait & \nowait
   Then
      Do
         say '>>>>> line' lineno': No wait state encountered.'
         rv = rv + 1
      end
Select
   When rv = 0
      Then msg ='All pass.'
   When rv = 1
      Then msg ='One failure.'
   otherwise
      msg = rv 'failures.'
end

If \quiet | rv \= 0
   Then say 'Test' testcase'. ' oks 'OK compares. ' msg

fail = rv \= 0
fails.fail = fails.fail + 1           /* Ok or fail                  */
If unprocessed \= ''
   Then call procline unprocessed
return

/*********************************************************************/
/* Test  case has ended, but we have not seen a disabled PSW message */
/* yet.                                                              */
/*                                                                   */
/* If havepgm is nonblank, we have seen a program check, but not yet */
/* the disabled wait PSW that will ensue.                            */
/*********************************************************************/

lookahead:
do while lines(in) > 0
   unprocessed = readline()
   If wordpos(?msg, 'HHC00809I HHC00803I') > 0
      Then leave
   If havepgm = ''                    /* Saw a program check?        */
      Then return                  /* Return to process *Done if not */
   call procline unprocessed
end
/* say '*** wait leapfrogged. ***' lineno */
call procline unprocessed
unprocessed = ''
return

/*********************************************************************/
/* Compare storage display against wanted contents.                  */
/*********************************************************************/

want:
wantres = rest = display
call test wantres, 'Storage at' lastaddr 'compare mismatch. ' info
If \wantres
   Then
      Do
         say '... Want:' strip(rest)
         say '... Have:' strip(display)
      End
return

/*********************************************************************/
/* Verify contents of a general register.                            */
/*********************************************************************/

wantgpr:
parse upper var rest r want '#' .
call test gpr.r = want, 'Gpr' r 'compare mismatch. ' info 'Want:' want 'got' gpr.r
return

/*********************************************************************/
/* Verify  that  the  key  of  the  last page frame displayed is the */
/* specified one.                                                    */
/*********************************************************************/

wantkey:
parse upper var rest want '#' .
If lastkey = ''
   Then call test 0, 'No key saved from r command.'
   Else call test lastkey = want, 'Key' keyaddr 'compare mismatch. ' info 'Want:' want 'got' lastkey
return

/*********************************************************************/
/* Verify that the prefix register contains the specified value.     */
/*********************************************************************/

wantprefix:
parse upper var rest want '#' .
If prefix = ''
   Then call test 0, 'No prefix register saved from pr command.'
   Else call test prefix = want, 'Prefix register compare mismatch. ' info 'Want:' want 'got' prefix
return

/*********************************************************************/
/* Verify the last issued message.                                   */
/*********************************************************************/

msg:
parse arg msgtype
If lastmsg.0 = 0
   Then
      Do
         call test 0, 'No message saved for' rest
         return
      End
If datatype(word(rest, 1), 'Whole')
   Then parse var rest mnum rest
   Else mnum = 0

ix = lastmsg.0 - mnum

If ix <= 0
   Then call test 0, 'No message stored for number' mnum
   else call test lastmsg.ix = rest, msgtype 'message mismatch. ' info ,,
      'Want:' rest, 'Got: ' lastmsg.ix
return

/*********************************************************************/
/* Verify the last issue informational message                       */
/*********************************************************************/

imsg:
If lastinfo = ''
   Then call test 0, 'No informational message saved for' rest
   Else call test lastinfo = rest, 'Informational message mismatch. ' info ,,
      'Want:' rest, 'Got: ' lastinfo
return

/*********************************************************************/
/* Decode disabled wait state message.                               */
/*********************************************************************/

waitstate:
havewait = 1
parse var rest 'wait state' psw1 psw2
cond = psw2 = 0 | (right(psw2, 4) = 'DEAD' & havepgm \= '' & havepgm = wantpgm)
call test cond, 'Received unexpected wait state: ' psw1 psw2
return

/*********************************************************************/
/* Save general registers for later test.                            */
/*********************************************************************/

gprs:
If verb = 'General'
   Then return
todo = verb rest
Do while todo \= ''
   parse var todo 'R' r '=' val todo
   r = x2d(r)
   gpr.r = val
End
return

/*********************************************************************/
/* Save explain text in explain array.                               */
/*********************************************************************/

explain:
? = expl.0 + 1
expl.? = rest
expl.0 = ?
return

/*********************************************************************/
/* Process  a  program check, which is identified by the IA field of */
/* the disabled wait PSW.                                            */
/*********************************************************************/

figurePgm:
Select
   When havepgm \= '' & wantpgm \= ''
      Then call test havepgm = wantpgm, 'Expect pgm type' wantpgm', but have type' havepgm'.'
   When havepgm \= ''
      Then call test 0, 'Unexpected pgm type' havepgm' havepgm.'
   Otherwise
      call test 0, 'Expect pgm type' wantpgm', but none happened.'
end
return

/*********************************************************************/
/* Evaluate  the  condition  on a If to determine whether to perform */
/* orders or not.                                                    */
/*********************************************************************/

doif:
expr = rest
test = ''
Do while rest \= ''
   parse var rest rl '$' +0 dolvar rest
   test = test rl
   If dolvar = ''
      Then leave
   parse var dolvar +1 vname .
   val = values.vname
   If \datatype(val, "Whole")
      Then val = '"'val'"'
   test = test val
End
signal on syntax
signal off novalue
interpret 'res =' test
signal on  novalue
ifstack = active hadelse ifstack
ifnest = ifnest + 1
If active                             /* In nested suppress?         */
   Then active = res = 1

hadelse = 0                 /* status of "have else" has been stacked   */
                            /* New '*If' means no else yet...           */

return

syntax:
say 'Syntax error in *If test:' condition('D')
return

/*********************************************************************/
/* Process Else.  Flip active if no else has been seen.              */
/*********************************************************************/

doElse:
Select
   When ifnest <= 0
      Then say 'No *If is active.'
   When hadelse
      Then say '*Else already seen on level' ifnest '.  Stack:' ifstack
   otherwise
      parse var ifstack prevact .   /* Nested if in suppressed code? */
      hadelse = 1
      active = prevact & (1 && active)
end
return

/*********************************************************************/
/* End of if.  Unstack.                                              */
/*********************************************************************/

endif:
If ifnest <= 0
   Then
      Do
         say 'No *If is active.'
         return
      End
ifnest = ifnest - 1
parse var ifstack active hadelse ifstack
return

/*********************************************************************/
/* Display message about a failed test.                              */
/*********************************************************************/

test:
If \active                            /* suppress any noise          */
   Then return
! = '>>>>> line' right(lineno, 5)':'
!! = left(' ', length(!))
If arg(1)
   Then oks = oks + 1
   Else
      Do
         say ! arg(2)
         do ? = 3 to arg()
            say !! arg(?)
         end
         rv = rv + 1
         Do ? = 1 to expl.0           /* Further explanation         */
            say !! expl.?
         End
      End
expl.0 = 0                            /* Be quiet next time          */
return

novalue:
parse source . . fn ft fm .
say 'Novalue in' fn ft fm '-- variable' condition('D')
say right(sigl, 6) '>>>' sourceline(sigl)
say '  This is often caused by missing or misspelled *Testcase'
say '  Note that the verbs are case sensitive.  E.g., *testcase is not correct.'
signal value lets_get_a_traceback

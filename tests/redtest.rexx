/* Process a test case                                               */
/*                                John Hartmann 22 Sep 2015 16:27:56 */

/*********************************************************************/
/* This  file  was  put into the public domain 2015-10-05 by John P. */
/* Hartmann.   You can use it for anything you like, as long as this */
/* notice remains.                                                   */
/*********************************************************************/

Signal on novalue

parse arg in opts
quiet = wordpos('quiet', opts) > 0
testcase = '<unknown>'
fails. = 0
catast = 0
done = 0
lineno = 0
facility.=0                           /* No facilities installed     */
active = 1                            /* We do produce output        */
nest=0                                /* If/then/else nesting        */

do while lines(in) > 0
   lineno = lineno + 1
   l = linein(in)
   parse var l msg verb rest
   If left(msg, 3) = 'HHC' & right(msg, 1) = 'E'
      Then lasterror = l
   Select
      When msg = 'HHC00801I'
         Then
            Do
               parse var rest ' code ' havepgm . ' ilc'
            End
      When msg = 'HHC00809I'
         Then call waitstate
      When msg = 'HHC01417I'
         Then
            If verb = 'Machine'
               Then
                  Do
                     parse var rest 'dependent assists:' rest
                     Do while rest \= ''
                        parse var rest fac rest
                        facility.fac = 1
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
                        Then parse var rest display +36 /* Save for compare order */
                        else
                           Do
                              keyaddr = substr(verb, 3)
                              lastkey = key
                           End
                  End
      When left(msg, 8) = 'HHC02332'
         Then
            Do
               catast = catast + 1
               call test 0, 'Test case timed out in wait.',,
                  'This is likely an error in Hercules.  Please report'
            end
      otherwise
   end
end
Select
   When fails.1 = 0
      Then msg = 'All OK.'
   otherwise
      msg = fails.1 'failed;' fails.0 'OK.'
end

say 'Done' done 'tests. ' msg
If catast > 0
   Then say '>>>>>' catast 'test failed catastrophically.  Bug in Hercules or test case is likely. <<<<<'
exit fails.1

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
   When verb = '*Compare'
      Then comparing = 1
   When verb = '*Want'
      Then call want
   When verb = '*Error'
      Then call emsg
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
   otherwise
      say 'Bad order: ' verb
      rv = 16
end
return

/*********************************************************************/
/* Initialise variable store for new test case                       */
/*********************************************************************/

begtest:
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
prefix = ''
lasterror = ''
expl.0 = 0
return

endtest:
Select
   When havepgm \= ''
      Then call figurePgm
   otherwise
end

If \havewait & rest \= 'nowait'
   Then
      Do
         say 'No wait state encountered.'
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
say 'Test' testcase'. ' oks 'OK compares. ' msg
done = done + 1
fail = rv \= 0
fails.fail = fails.fail + 1           /* Ok or fail                  */
return

/*********************************************************************/
/* Compare storage display atainst wanted contents.                  */
/*********************************************************************/

want:
call test rest = display, 'Storage at' lastkey 'compare mismatch. ' info 'Want:' rest 'got' display
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
/* Verify the last issued error message.                             */
/*********************************************************************/

emsg:
If lasterror = ''
   Then call test 0, 'No error message saved for' rest
   Else call test lasterror = rest, 'Error message mismatch. ' info ,,
      'Want:' rest, 'Got: ' lasterror
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

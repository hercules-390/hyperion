/* Process a test case                                               */
/*                                John Hartmann 22 Sep 2015 16:27:56 */

/*********************************************************************/
/* This  file  was  put into the public domain 2015-10-05 by John P. */
/* Hartmann.   You can use it for anything you like, as long as this */
/* notice remains.                                                   */
/*********************************************************************/

Signal on novalue

parse arg in .
comparing = 0
havewait = 0
testcase = '<unknown>'
rv = 0
fails. = 0
done = 0
lineno = 0

do while lines(in) > 0
   lineno = lineno + 1
   l = linein(in)
   parse var l msg verb rest
   Select
      When msg = 'HHC01603I'
         Then
            If left(verb, 1) = '*'
               Then call order
      When msg = 'HHC00801I'
         Then
            Do
               parse var rest ' code ' havepgm . ' ilc'
            End
      When msg = 'HHC00809I'
         Then call waitstate
      When msg = 'HHC02277I'
         Then parse var rest . prefix .
      When msg = 'HHC02290I'
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
      When msg = 'HHC02269I'
         then call gprs
      otherwise
   end
end
say 'Done' done 'tests. ' fails.1 'failed;' fails.0 'OK.'
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
   When verb = '*Gpr'
      Then call wantgpr
   When verb = '*Key'
      Then call wantkey
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

begtest:
testcase = rest
comparing = 0
havewait = 0
oks = 0
notoks = 0
wantpgm = ''
havepgm = ''
rv = 0
pgmok = 0                             /* Program check not expected  */
gpr.=''
lastkey = ''
keyaddr = '<unknown>'
prefix = ''
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
say 'Test' testcase'. ' oks 'OK compares.  rv='rv
done = done + 1
fail = rv \= 0
fails.fail = fails.fail + 1
return

want:
If rest = display
   Then
      Do
         oks = oks + 1
         return
      End
notoks = notoks + 1
call failtest 'Storage at' lastkey 'compare mismatch. ' info 'Want:' rest 'got' display
return

wantgpr:
parse var rest r want
If gpr.r = want
   Then
      Do
         oks = oks + 1
         return
      End
call failtest 'Gpr' r 'compare mismatch. ' info 'Want:' want 'got' gpr.r
return

/*********************************************************************/
/* Verify  that  the  key  of  the  last page frame displayed is the */
/* specified one.                                                    */
/*********************************************************************/

wantkey:
parse upper var rest want

If lastkey = want
   Then
      Do
         oks = oks + 1
         return
      End
If lastkey = ''
   Then call failtest 'No key saved from r command.'
   Else call failtest 'Key' keyaddr 'compare mismatch. ' info 'Want:' want 'got' lastkey
return

/*********************************************************************/
/* Verify that the prefix register contains the specified value.     */
/*********************************************************************/

wantprefix:
parse upper var rest want

If prefix = want
   Then
      Do
         oks = oks + 1
         return
      End
If prefix = ''
   Then call failtest 'No prefix register saved from pr command.'
   Else call failtest 'Prefix register compare mismatch. ' info 'Want:' want 'got' prefix
return

waitstate:
havewait = 1
parse var rest 'wait state' psw1 psw2
If psw2 = 0
   Then return
If psw2 = 'FFFFFFFFDEADDEAD' & havepgm \= '' & havepgm = wantpgm
   Then return
call failtest 'Received unexpected wait state: ' psw1 psw2
return

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

figurePgm:
If havepgm = wantpgm
   Then return
Select
   When havepgm \= '' & wantpgm \= ''
      Then call failtest 'Expect pgm type' wantpgm', but have type' havepgm'.'
   When havepgm \= ''
      Then call failtest 'Unexpected pgm type' havepgm' havepgm.'
   Otherwise
      call failtest 'Expect pgm type' wantpgm', but none happened.'
end
return

failtest:
say '>>>>> line' right(lineno, 5)':'  arg(1)
rv = rv + 1
return

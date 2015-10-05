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

do while lines(in) > 0
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
      When msg = 'HHC02290I'
         Then
            If comparing & left(rest, 2) ^= 'K:'
               then parse var rest display +36 /* Save for compare order */
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
   When verb = '*Program'
      Then
         Do
            wantpgm = rest
         End
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
return

endtest:
Select
   When havepgm ^= ''
      Then call figurePgm
   otherwise
end

If ^havewait
   Then
      Do
         say 'No wait state encountered.'
         rv = rv + 1
      end
say 'Test' testcase'. ' oks 'OK compares.  rv='rv
done = done + 1
fail = rv ^= 0
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
say 'Compare mismatch. ' info 'Want:' rest 'got' display
rv = rv + 1
return

waitstate:
havewait = 1
parse var rest 'wait state' psw1 psw2
If psw2 = 0
   Then return
If psw2 = 'FFFFFFFFDEADDEAD' & havepgm ^= '' & havepgm = wantpgm
   Then return
say 'Received unexpected wait state: ' psw1 psw2
rv = rv + 1
return
say 'have' havepgm 'want' wantpgm (havepgm = wantpgm)

figurePgm:
If havepgm = wantpgm
   Then return
rv = rv + 1
Select
   When havepgm ^= '' & wantpgm ^= ''
      Then say 'Expect pgm type' wantpgm', but have type' havepgm'.'
   When havepgm ^= ''
      Then say 'Unexpected pgm type' havepgm' havepgm.'
   Otherwise
      say 'Expect pgm type' wantpgm', but none happened.'
end
return

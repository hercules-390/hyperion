/* Build a hercules test case from -tgtascii hlasm                   */
/*                                 John Hartmann 7 Oct 2015 15:29:14 */

/* This file  was put into the  public domain 2015-10-07 by  John P. */
/* Hartmann.  You can use it for  anything you like, as long as this */
/* notice remains.                                                   */

exitcode = 0

Signal on novalue

parse arg in out '(' options

logit = wordpos('log', options) > 0

If in = ''
   Then exit 36
parse source . . myfn .
If out = ''
   Then call makeout

call SysFileDelete out
say 'reading' in'; writing' out
parse source .. myname .
call out ,
   '# This test file was generated from offline assembler source',,
   '# by' myname date() time(),,
   '# Treat as object code.  That is, modifications will be lost.',,
   '# assemble and listing files are provided for information only.'
do while lines(in) > 0
   l = charin(in,, 80)
   parse var l verb rest
   rest = strip(rest)

   Select
      When left(l, 4) == '02e3e7e3'x  /* TXT record                  */
         Then call txt
      When left(l, 1) == '02'x        /* Other object                */
         then nop
      When left(verb, 1) = ':'
         Then call macs
      When verb = '*'
         Then call out rest
      When left(verb, 1) = '*'
         Then call out substr(verb, 2) rest
      otherwise
         say 'ignoring' l
         exitcode = 1
exit
   end
end
exit exitcode

txt:
addr = c2d(substr(l, 6, 3))
len = c2d(substr(l, 11, 2))
data = substr(l, 17, 56)
do while len > 0
   toget = min(16, len)
   parse var data w +(toget) data
   call out 'r' right(d2x(addr), 6)'='c2x(w)
   addr = addr + 16
   len = len - 16
end
return

macs:
Select
   When verb = ':test'
      Then call out '*Testcase' rest 'processed' date() time() 'by' myfn
   When verb = ':btst'
      Then call out 'sysclear', 'archmode' rest
   When verb = ':etst'
      Then call out 'runtest' rest
   otherwise
      say 'Bad macro:' l
      exit
end
return

makeout:
p = pos('/', reverse(in))
If p=0
   Then out = in
   Else out = right(in, p - 1)
parse var out fn '.'
out = fn'.tst'
return

out:
do ? = 1 to arg()
   call lineout out, arg(?)
   If logit
      Then say arg(?)
end
return

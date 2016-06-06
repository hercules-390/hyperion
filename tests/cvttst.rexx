/* Convert .tst test case to object deck for CMS                     */
/*                                John Hartmann 19 May 2016 12:51:16 */

/* This  file  was  put into the public domain 2016-05-22 by John P. */
/* Hartmann.   You can use it for anything you like, as long as this */
/* notice remains.                                                   */

Signal on novalue
numeric digits 30

verbose = 0
call init
out = ''
parse arg infiles
parse var infiles w1 w2 rest
If w1 = 'o:'
   Then
      Do
         call getout w2
         infiles = rest
      End
If infiles = ''
   Then infiles = 'ieee-add /usr/data/src/hercules/hyperion/tests/ieee-adds.tst'
csects = ''

do while infiles \= ''
   parse var infiles in infiles
   parse value reverse(in) with '.' fn '/'
   fn = reverse(fn)
   parse var fn fn +8                    /* Truncate                    */
   If out = ''
      Then call getout fn
   call onefile
   If infiles \= ''
      Then call punch 'SPB'
end

"(echo cd zcms; echo bin ; echo site fix 80; echo put" out "; echo quit)",
      "| ftp vm.marist.edu"
exit

getout:
out = arg(1)'.text'
say 'Creating' out
call sysfiledelete(out)
return

onefile:
oix = 0
tix = 0
maxaddr = 0
relocate = ''
inst. = ''
t. = ''
start = x2d(200)                  /* Where a program normally begins */

sect = translate(translate(fn), '_', '-')
If wordpos(sect, csects) > 0
   Then
      Do
         say "Duplicate CSECT" sect '(' csects ')'
         parse var sect sect +7
         sect = sect || '@'
      End
csects = csects sect
call order '*Load' sect

do while lines(in) > 0
   ln = linein(in)
   parse var ln w1 w2 rest
   If w1 \= 'r'
      Then
         Do
            call order ln
            iterate
         End
   parse var w2 addr '=' data
   If data \= ''
      Then call text
      else call display
end

item = a2e(left(sect, 8)) || '0000000007'x || d2c(maxaddr, 3)
call punch 'ESD', , 16, 1, item

do i = 1 to tix
   If t.i = ''
      Then call ckinst
end

do i = 1 to tix
   len = length(d.i)
   If len < 8
      Then pad = left(' ', 10 - len)
      Else pad = '  '
   call punch 'TXT', a.i, len, 1, d.i || a2e(pad || r.i)
end

Do while relocate \= ''
   parse var relocate daddr len relocate
   flag = 0
   If len > 4
      then
         Do
            flag = 4
            len = len - 4
         End
   len = d2x((len - 1) * 4)
   item = '0001'x || '0001'x || x2c(flag || len) || d2c(daddr, 3)
   call punch 'RLD', , length(item), , item
End

do i = 1 to oix
   call charout out, left(orders.i, 80)
end
call punch 'END'
return


order:
oix = oix + 1
orders.oix = '*' arg(1)
return

/*********************************************************************/
/* r order to set contents of storage                                */
/*********************************************************************/

text:
tix = tix + 1
daddr = x2d(addr)
dlen = length(data) / 2
a.tix = daddr
d.tix = x2c(data)
r.tix = strip(rest)
l.tix = ln
ckaddr(addr, dlen)
inst.daddr = tix
op = left(d.tix, 1)
itype = b2x('000000'left(c2b(op), 2))
ilen = word('2 4 4 6', itype + 1)
Select
   When daddr >= x2d('1a0') & daddr < x2d(200)
      Then call epsw
   When daddr >= x2d('18') & daddr < x2d(40)
      Then call psw
   When daddr = 0
      Then call psw
   When op = '00'x | op = 'ff'x | dlen > 6
      Then t.tix = 'dc'
   when ilen \= dlen
      Then t.tix = 'dc'
   when 4 = ilen & x2d(right(data, 3)) < x2d(200)
      Then t.tix = 'dc'
   otherwise
end
return

epsw:
t.tix = 'epsw'
If right(addr, 1) \= '0'
   Then call fixme "Address of new EPSW not divisible by 16."
If dlen \= 16
   Then call fixme 'New EPSW length is not 16.'
If bitand('02'x, substr(d.tix, 2, 1)) == '02'x             /* disabled */
   Then return
relocate = relocate daddr + 8 8
If addr \= x2d('1a0')
   Then return
/* Restart PSW                                                       */
start = c2d(right(d.tix, 8))
return

psw:
t.tix = 'psw'
If pos(right(addr, 1), '08') = 0
   Then call fixme "Address of new PSW not divisible by 8."
If dlen \= 8
   Then call fixme 'New PSW length is not 8.'
If bitand('02'x, substr(d.tix, 2, 1)) == '02'x             /* disabled */
   Then return
relocate = relocate daddr + 4 4
If addr \= 0
   Then return
/* Restart PSW                                                       */
start = c2d(bitand('7fffffff'x, right(d.tix, 4)))
return

/*********************************************************************/
/* r order to display storage.                                       */
/*********************************************************************/

display:
parse var w2 addr '.' len
ckaddr(addr, x2d(len))
call order ln
return

ckaddr:
parse arg addr, len
xend = x2d(addr) + len
If xend > maxaddr
   Then maxaddr = xend
return

punch:
parse arg type, addr, len, esdid, data
odata = '02'x || a2e(type) || d2cx(addr, 3) || d2cx(len, 2) || d2cx(esdid, 2) || data
call charout out, left(odata, 80, '40'x)
return

a2e:
return translate(arg(1), xlate819to1047, xrange())

init:

xlate819to1047 = ,
  "00010203 372D2E2F 1605250B 0C0D0E0F 10111213 3C3D3226 18193F27 1C1D1E1F"x ||,
  "405A7F7B 5B6C507D 4D5D5C4E 6B604B61 F0F1F2F3 F4F5F6F7 F8F97A5E 4C7E6E6F"x ||,
  "7CC1C2C3 C4C5C6C7 C8C9D1D2 D3D4D5D6 D7D8D9E2 E3E4E5E6 E7E8E9AD E0BD5F6D"x ||,
  "79818283 84858687 88899192 93949596 979899A2 A3A4A5A6 A7A8A9C0 4FD0A107"x ||,
  "20212223 24150617 28292A2B 2C090A1B 30311A33 34353608 38393A3B 04143EFF"x ||,
  "41AA4AB1 9FB26AB5 BBB49A8A B0CAAFBC 908FEAFA BEA0B6B3 9DDA9B8B B7B8B9AB"x ||,
  "64656266 63679E68 74717273 78757677 AC69EDEE EBEFECBF 80FDFEFB FCBAAE59"x ||,
  "44454246 43479C48 54515253 58555657 8C49CDCE CBCFCCE1 70DDDEDB DC8D8EDF"x
return

d2cx:
If length(arg(1)) = 0
   Then return '40404040'x
? = d2c(arg(1), arg(2))
return right(?, 4, '40'x)

fixme:
say arg(1)
say ln
exit 17

/*********************************************************************/
/* Check out an instruction.                                         */
/*********************************************************************/

ckinst:
op = left(d.i, 1)
xinst = c2x(d.i)
itype = left(c2b(op), 2)
Select
   When itype = 0                     /* RR                          */
      Then nop
   When itype = 1
      then signal rx
   When itype = 10
      then signal rs
   When itype = 11
      then signal ss
end
return

rx:
parse var xinst +2 reg+1 ix+1 b+1 disp
ddisp = x2d(disp)
Select
   When op = '41'x
      Then signal la
   When d.i == '47000000'x
      Then say 'no op: ' l.i          /* Don't turn no-op into base  */
   otherwise
      signal ckbase
end
return

/* LA  might  load  a  constant  or  an address.  Arbitrarily we say */
/* anything above the program origin is an address.                  */
la:
If ddisp < start
   Then return
ckbase:
If b \= 0
   Then return 0
d.i = x2c(overlay('f', c2x(d.i), 5))
If verbose
   Then say 'Base fixed: ' c2x(d.i) r.i
return 1
say c2x(d.i) r.i
return

rs:
parse var xinst +2 r1 +1 r2 +1 b+1 disp
ix = 0                                /* No index active             */
ddisp = x2d(disp)
dop = c2d(op)
Select
   When op = '84'x | op = '85'x
      Then return                     /* Relative                    */
   when dop >= x2d('88') & dop <= x2d('8f')
      Then return                     /* Shift                       */
   When op = 'a5'x | op = 'a7'x
      Then return                     /* Immediate                   */
   When op = 'b2'x
      Then signal b2
   When op = 'b3'x | op = 'b3'x
      Then return                   /* All B3 and B9 are RRsomething */
   When op = 'b7'x
      Then signal lctl
   when ckbase()
      then return
   otherwise
      say 'rs: ' xinst r.i
end
return

ss:
parse var xinst +2 r1 +1 r2 +1 b+1 disp+3 dhi+2
Select
   When op = 'e3'x | op = 'ed'x
      then call ckbase
   otherwise
      say 'ss: ' l.i
end
return

b2:
parse var xinst +4 b+1 disp           /* If S type                   */
opx = substr(d.i, 2, 1)
Select
   When opx = 'b2'x
      Then signal lpswe
   otherwise
      say 'b2' opx xinst r.i
end
return

lpswe:
call getoperand 'lpswe'
If bitand('02'x, substr(opdata, 2, 1)) == '00'x
   Then call fixmex 'lpswe not wait'
If left(opdata, 1) \= '00'x
   Then call fixmex 'lpswe not disabled'
say 'Replacing with BR 14: ' l.i
d.i = '07fe'x
return

lctl:
call getoperand 'lctl'
If opdata \= '00040000'x
   Then call fixmex 'lctl not for afpr' c2x(opdata)
say 'Noop for lctl: ' l.i
call charout out, left('* *Set afpr', 80)
d.i = '47000000'x                     /* Make no op                  */
return

getoperand:
If b \= 0
   Then call fixmex 'Non-zero base for' arg(1)
dix = inst.ddisp
If dix = ''
   Then call fixmex arg(1) 'operand not found'
opdata = d.dix
return

fixmex:
ln = l.i
call fixme arg(1)

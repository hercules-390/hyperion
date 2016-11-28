* Multiply halfword immediate test

* This file was put into the public domain 2015-10-22
* by John P. Hartmann.  You can use it for anything you like,
* as long as this notice remains.

mhi start 0
 using mhi,0
 org mhi+x'60' Unused bcmode stuff as scratch
stop dc x'00020000',f'0',ad(0)
 org mhi+x'1a0' Restart
 dc x'0000000180000000',ad(go)
 org mhi+x'1d0' Program
 dc x'0002000180000000',ad(x'deaddead')
 org mhi+x'200'
go equ *
 la 2,x'21'
 lr 5,2
 mhi 2,2
 lr 6,2
 ipm 4
 la 3,1(2,2)
 lpswe stop
 ltorg
 end

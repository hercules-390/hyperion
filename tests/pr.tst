* Prefix test.  Hand written.

# This  file  was  put  into  the  public  domain 2015-10-13 by John P.
# Hartmann.   You  can  use  it  for anything you like, as long as this
# notice remains.

*Testcase Prefix register and real/absolute storage set/display
sysclear
archmode z
mainstor 8m
numcpu 1
*Compare
pr
*Prefix 0
r 0=01
r 4000=02
abs 0.4
*Want 01000000
abs 4000.4
*Want 02000000

pr 4000
*Prefix 4000
abs 0.4
*Want 01000000
abs 4000.4
*Want 02000000
r   0.4
*Want 02000000
r   4000.4
*Want 01000000
abs 0.4

pr 800000
*Error HHC02290E A:00801000  Addressing exception
# we get one page more because the prefix area is
# two frames in z/Architecture
pr fffff
*Prefix 00000000000fe000
*Done nowait

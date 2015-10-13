* Prefix test.  Hand written.

# This  file  was  put  into  the  public  domain 2015-10-13 by John P.
# Hartmann.   You  can  use  it  for anything you like, as long as this
# notice remains.

*Testcase Prefix register and real/absolute storage set/display
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
*Done nowait

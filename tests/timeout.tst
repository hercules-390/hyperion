* This test case relies on a PSW of all zeros being valid.
* So the restart goes to 0 and takes an 0c1.  The program new
* PSW also goes to 0 and takes another program check.
* Note that this is not a program interrupt loop since the
* program new PSW is valid.
*Testcase timeout
sysclear
archmode z
*Timeout
runtest .01
*Program 1
*Done nowait

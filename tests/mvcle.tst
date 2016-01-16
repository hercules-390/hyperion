# This test file was generated from offline assembler source
# by bldhtc.rexx 16 Jan 2016 12:28:39
# Treat as object code.  That is, modifications will be lost.
# assemble and listing files are provided for information only.
*Testcase mvcle-370
sysclear
archmode s/370
r      0=0000000000000200
r     68=00020000DEADDEAD
r    1A0=00000001800000000000000000000200
r    1D0=0002000180000000FFFFFFFFDEADDEAD
r    200=412002004130000441100006A8020001
r    210=82000218
r    218=00020000000000000000000000000000
*Program 0001
runtest .1
*Compare
r 0.8
*Want 00000000 00000200
*Done

# This test file was generated from offline assembler source
# by bldhtc.rexx 11 Nov 2015 19:00:06
# Treat as object code.  That is, modifications will be lost.
# assemble and listing files are provided for information only.
*Testcase sske#1 processed 11 Nov 2015 19:00:06 by bldhtc.rexx
* sske#1:  Test for sske being privileged.
sysclear
archmode z
r    1A0=00010001800000000000000000000200
r    1D0=0002000180000000FFFFFFFFDEADDEAD
r    200=A7282004A73800F0B22B0032B2B20210
r    210=00020001800000000000000000000BAD
*Program 0002

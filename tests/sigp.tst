# This test file was generated from offline assembler source
# by bldhtc.rexx 11 Nov 2015 19:00:06
# Treat as object code.  That is, modifications will be lost.
# assemble and listing files are provided for information only.
*Testcase sigp
sysclear
archmode z
r    1A0=00000001800000000000000000000200
r    1D0=0002000180000000FFFFFFFFDEADDEAD
r    200=B212022048200220AE020005B2B20210
r    210=00020000000000000000000000000BAD
runtest .1
psw
runtest .1
psw
*Done nowait

# This test file was generated from offline assembler source
# by bldhtc.rexx 11 Nov 2015 20:20:38
# Treat as object code.  That is, modifications will be lost.
# assemble and listing files are provided for information only.
*Testcase mhi
sysclear
archmode z
r     60=00020000000000000000000000000000
r    1A0=00000001800000000000000000000200
r    1D0=0002000180000000FFFFFFFFDEADDEAD
r    200=412000211852A72C00021862B2220040
r    210=41322001B2B20060
runtest .1
*Compare
gpr
*Gpr 2 0042
*Gpr 3 0085
*Gpr 4 000000000000000
*Done

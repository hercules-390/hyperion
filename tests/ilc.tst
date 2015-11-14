# This test file was generated from offline assembler source
# by bldhtc.rexx 13 Nov 2015 17:12:51
# Treat as object code.  That is, modifications will be lost.
# assemble and listing files are provided for information only.
*Testcase ilc
numcpu 1
sysclear
archmode z
r     60=00020000000000000000000000000000
r     70=0002000000000000000000000000DEAD
r    1A0=00000001800000000000000000000200
r    1D0=00000001800000000000000000000220
r    200=41F00C0041200021EB110240002FEB9B
r    210=0248002FAD440000A72C0002B2B20060
r    220=D213F000008C41F0F020D5010260008E
r    230=47700238B2B20150
r    238=B2B20070000000000000000000000020
r    248=00000000400000000000000000000218
r    258=000000000000021B0080
runtest .1
*Compare
gpr
*Gpr 2 0042
r 8c.14 # last program check for info
r c00.14
r c00.c
*Want "MHI ilc" 00040080 00000000 000040F0
r c20.14
*Done

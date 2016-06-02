# This test file was generated from offline assembler source
# by bldhtc.rexx  2 Jun 2016 09:24:36
# Treat as object code.  That is, modifications will be lost.
# assemble and listing files are provided for information only.
*Testcase csxtr 20160602 09.24
sysclear
archlvl z
r    1A0=00000001800000000000000000000200
r    1C0=00020001800000000000000000000000
r    1D0=00020001800000000000000000000000
*Program 7
r    200=B3EB0040B600F3309604F331B700F330
r    210=6800F3386820F340B3EB0040EB45F310
r    220=0024ED00001F00496000F3006020F308
r    230=B3EA00404050F320
r    238=6840F348B3E30064E360F328002412EE
r    248=077E0AFF
r    338=2608134B9C1E28E56F3C127177823534
r    348=263934B9C1E28E56
runtest .1
runtest program .1
*Compare
r 300.10
r 310.10
*Want "csxtr/srxt low" 45678901 23456789 01234567 8901234C
r 320.2
*Want "csxtr/srxt high" 0123
r 328.8
*Want "csdtr" 23456789 0123456C
*Done

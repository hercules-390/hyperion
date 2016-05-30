# This test file was generated from offline assembler source
# by bldhtc.rexx 30 May 2016 22:35:51
# Treat as object code.  That is, modifications will be lost.
# assemble and listing files are provided for information only.
*Testcase csxtr 20160530 22.35
sysclear
archlvl z
r    1A0=00010001800000000000000000000200
r    1C0=00020001800000000000000000000000
r    1D0=00020001800000000000000000000000
r    200=6800F3186820F318B3EB0040EB45F300
r    210=0024ED00001F0049B3EA00404050F310
r    220=12EE077E0AFF
r    318=2608134B9C1E28E56F3C127177823534
runtest .1
r 300.10
*Want 45678901 23452302 01382834 1780165C
r 310.2
*Want 0123
*Done

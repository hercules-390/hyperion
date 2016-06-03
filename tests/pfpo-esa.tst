# This test file was generated from offline assembler source
# by bldhtc.rexx  5 May 2016 09:49:18
# Treat as object code.  That is, modifications will be lost.
# assemble and listing files are provided for information only.
*Testcase pfpo-esa
sysclear
arclvl esa/390
r      0=0001000000000070
r     68=00020000DEADDEAD5800F090010A8200
r     78=F080
r     80=00020000000000000000000000000000
r     90=80000000
*Program 0001
runtest .1
*Done

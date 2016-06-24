
*Testcase ieee-comps.tst: IEEE Compare, Load and Test, Test Data Class
*Message Testcase ieee-comps.tst: IEEE Compare, Load and Test, Test Data Class
*Message ..Includes COMPARE (5), COMPARE AND SIGNAL (5), LOAD AND TEST (3), 
*Message ..         TEST DATA CLASS (3), 16 instructions total.
*Message ..Test case values include infinities, a subnormal, a QNaN, and an SNaN

# see ieee-comps.asm for test case processing details

sysclear
archmode esame
loadcore $(testpath)/ieee-comps.core
runtest .1

# the following symbol definitions must match the first digit of the corresponding 
# ORG statement in the ieee-comps.asm and .list files

defsym res1 "7"   # Condition codes
defsym res2 "8"   # IEEE flags
defsym res3 "9"   # SNaN->QNaN test results

r $(res1)00.98    # Condition codes
r $(res2)00.90    # IEEE flags
r $(res3)00.20    # SNaN->QNaN test results

# Short instruction expected results

*Compare
r $(res1)00.8  # BFP Short Compare RXE condition codes, expecting 3 0 1 2 1 1 3 3 
*Want "CEB cc" 03000102 01010303
*Compare
r $(res2)00.8  # BFP Short Compare RXE IEEE Flags, expecting 00, 00, 00, 00, 00, 80, 80
*Want "CEB IEEE Flags" 00000000 00008080

*Compare
r $(res1)08.8  # BFP Short Compare RRE condition codes, expecting 3 0 1 2 1 1 3 3 
*Want "CEBR cc" 03000102 01010303
*Compare
r $(res2)08.8  # BFP Short Compare RRE IEEE Flags, expecting 00, 00, 00, 00, 00, 80, 80
*Want "CEBR IEEE Flags" 00000000 00008080

*Compare
r $(res1)28.8  # BFP Short Compare and Signal RXE condition codes, expecting 3 0 1 2 1 1 3 3 
*Want "KEB cc" 03000102 01010303
*Compare
r $(res2)28.8  # BFP Short Compare and Signal RXE IEEE Flags, expecting 80, 00, 00, 00, 00, 80, 80
*Want "KEB IEEE Flags" 80000000 00008080

*Compare
r $(res1)30.8  # BFP Short Compare and Signal RRE condition codes, expecting 3 0 1 2 1 1 3 3 
*Want "KEBR cc" 03000102 01010303
*Compare
r $(res2)30.8  # BFP Short Compare and Signal RRE IEEE Flags, expecting 80, 00, 00, 00, 00, 80, 80
*Want "KEBR IEEE Flags" 80000000 00008080

*Compare
r $(res1)50.8  # BFP Short Load and Test RRE condition codes, expecting 3 2 2 2 1 1 2 3 
*Want "LTEBR cc" 03020202 01010203
*Compare
r $(res2)50.8  # BFP Short Load and Test RRE IEEE Flags, expecting 00, 00, 00, 00, 00, 00, 80
*Want "LTEBR IEEE Flags" 00000000 00000080

*Compare
r $(res3)18.4  # BFP Short Load and Test RRE SNaN->QNaN, expecting 7FC10000
*Want "LTEBR SNaN->QNaN" 7FC10000

*Compare
r $(res1)68.8  # BFP Short Test Data Class condition codes, expecting 1 1 1 1 1 1 1 1
*Want "TCEB cc 1/2" 01010101 01010101
*Compare
r $(res1)70.8  # BFP Short Test Data Class condition codes, expecting 0 0 0 0 0 0 0 0
*Want "TCEB cc 2/2" 00000000 00000000


# Long instruction expected results  (same as the shorts except for the QNaN)

*Compare
r $(res1)10.8  # BFP Long Compare RXE condition codes, expecting 3 0 1 2 1 1 3 3 
*Want "CDB cc" 03000102 01010303
*Compare
r $(res2)10.8  # BFP Long Compare RXE IEEE Flags, expecting 00, 00, 00, 00, 00, 80, 80
*Want "CDB IEEE Flags" 00000000 00008080

*Compare
r $(res1)18.8  # BFP Long Compare RRE condition codes, expecting 3 0 1 2 1 1 3 3 
*Want "CDBR cc" 03000102 01010303
*Compare
r $(res2)18.8  # BFP Long Compare RRE IEEE Flags, expecting 00, 00, 00, 00, 00, 80, 80
*Want "CDBR IEEE Flags" 00000000 00008080

*Compare
r $(res1)38.8  # BFP Long Compare and Signal RXE condition codes, expecting 3 0 1 2 1 1 3 3 
*Want "KDB cc" 03000102 01010303
*Compare
r $(res2)38.8  # BFP Long Compare and Signal RXE IEEE Flags, expecting 00, 00, 00, 00, 00, 80, 80
*Want "KDB IEEE Flags" 80000000 00008080

*Compare
r $(res1)40.8  # BFP Long Compare and Signal RRE condition codes, expecting 3 0 1 2 1 1 3 3 
*Want "KDBR cc" 03000102 01010303
*Compare
r $(res2)40.8  # BFP Long Compare and Signal RRE IEEE Flags, expecting 80, 00, 00, 00, 00, 80, 80
*Want "KDBR IEEE Flags" 80000000 00008080

*Compare
r $(res1)58.8  # BFP Long Load and Test RRE condition codes, expecting 3 2 2 2 1 1 2 3 
*Want "LTDBR cc" 03020202 01010203
*Compare
r $(res2)58.8  # BFP Long Load and Test RRE IEEE Flags, expecting 00, 00, 00, 00, 00, 00, 80
*Want "LTDBR IEEE Flags" 00000000 00000080
*Compare
r $(res3)10.8  # BFP Long Load and Test RRE SNaN->QNaN, expecting 7FF81000 00000000
*Want "LTDBR SNaN->QNaN" 7FF81000 00000000

*Compare
r $(res1)78.8  # BFP Long Test Data Class condition codes, expecting 1 1 1 1 1 1 1 1
*Want "TCDB cc 1/2" 01010101 01010101
*Compare
r $(res1)80.8  # BFP Long Test Data Class condition codes, expecting 0 0 0 0 0 0 0 0
*Want "TCDB cc 2/2" 00000000 00000000



# Extended instruction expected results  (same as the shorts except for the QNaN)

*Compare
r $(res1)20.8  # BFP extended Compare RRE condition codes, expecting 3 0 1 2 1 1 3 3 
*Want "CXBR cc" 03000102 01010303
*Compare
r $(res2)20.8  # BFP extended Compare RRE IEEE Flags, expecting 00, 00, 00, 00, 00, 80, 80
*Want "CXBR IEEE Flags" 00000000 00008080

*Compare
r $(res1)48.8  # BFP extended Compare and Signal RRE condition codes, expecting 3 0 1 2 1 1 3 3 
*Want "KXBR cc" 03000102 01010303
*Compare
r $(res2)48.8  # BFP extended Compare and Signal RRE IEEE Flags, expecting 80, 00, 00, 00, 00, 80, 80
*Want "KXBR IEEE Flags" 80000000 00008080

*Compare
r $(res1)60.8  # BFP extended Load and Test RRE condition codes, expecting 3 2 2 2 1 1 2 3 
*Want "LTXBR cc" 03020202 01010203
*Compare
r $(res2)60.8  # BFP extended Load and Test RRE IEEE Flags, expecting 00, 00, 00, 00, 00, 00, 80
*Want "LTXBR IEEE Flags" 00000000 00000080
*Compare
r $(res3)00.10  # BFP extended Load and Test RRE SNaN->QNaN, expecting 7FFF8100 00000000 00000000 00000000
*Want "LTXBR SNaN->QNaN" 7FFF8100 00000000 00000000 00000000

*Compare
r $(res1)88.8  # BFP extended Test Data Class condition codes, expecting 1 1 1 1 1 1 1 1
*Want "TCXB cc 1/2" 01010101 01010101
*Compare
r $(res1)90.8  # BFP extended Test Data Class condition codes, expecting 0 0 0 0 0 0 0 0
*Want "TCXB cc 2/2" 00000000 00000000


*Done

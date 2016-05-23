
*Testcase IEEE LOAD LENGTHENED (6), MULTIPLY (9), MULTPLY AND ADD (4), MULTPLY AND SUBTRACT (4), 23 instr total

#
#  1. Multiply adjacent pairs of values in the input set (five values means four
#     products).  
#     Test data: 1, 2. 4. -2, -2; expected products 2, 8, -8, 4
#
#  2. Multiply adjacent pairs of values in the input set and add the first value
#     in the pair to the product (five values means four results).  
#     Test data: 1, 2, 4, -2, -2; expected results 3, 10, -4, 2
#
#  3. Multiply adjacent pairs of values in the input set and subtract the first 
#     value in the pair from the product (five values means four results).  
#     Test data: 1, 2, 4, -2, -2; expected results 1, 6, -12, 6
#
# Tests 17  multiplication instructions:
#   MULTIPLY (BFP short x short -> short, RRE) MEEBR
#   MULTIPLY (BFP short x short -> short, RXE) MEEB
#   MULTIPLY (BFP short x short -> long, RRE) MDEBR
#   MULTIPLY (BFP short x short -> long, RXE) MDEB
#   MULTIPLY (BFP long x long -> long, RRE) MDBR
#   MULTIPLY (BFP long x long -> long, RXE) MDB
#   MULTIPLY (BFP long x long -> extended, RRE) MXDBR
#   MULTIPLY (BFP long x long -> extended, RXE) MXDB
#   MULTIPLY (BFP long x long -> extended, RRE) MXDBR
#   MULTIPLY AND ADD (BFP short x short -> short, RRE) MAEBR
#   MULTIPLY AND ADD (BFP short x short -> short, RXE) MAEB
#   MULTIPLY AND ADD (BFP long x long -> long, RRE) MADBR
#   MULTIPLY AND ADD (BFP long x long -> long, RXE) MADB
#   MULTIPLY AND SUBTRACT (BFP short x short -> short, RRE) MSEBR
#   MULTIPLY AND SUBTRACT (BFP short x short -> short, RXE) MSEB
#   MULTIPLY AND SUBTRACT (BFP long x long -> long, RRE) MSDBR
#   MULTIPLY AND SUBTRACT (BFP long x long -> long, RXE) MSDB
#
# Also tests the following six conversion instructions
#   LOAD LENGTHENED (short BFP to long BFP, RRE)
#   LOAD LENGTHENED (short BFP to extended BFP, RRE) 
#   LOAD LENGTHENED (long BFP to extended BFP, RRE)  
#   LOAD LENGTHENED (short BFP to long BFP, RRE)
#   LOAD LENGTHENED (short BFP to extended BFP, RRE) 
#   LOAD LENGTHENED (long BFP to extended BFP, RRE)  
#
# Intermediate results from the Load Lengthened instructions are not saved.  Because
# zero is not present in the input test set, any error in Load Lengthened will appear
# in the resulting products
#
# Also tests the following floating point support instructions
#   LOAD  (Short)
#   LOAD  (Long)
#   LOAD ZERO (Long)
#   STORE (Short)
#   STORE (Long)
#
sysclear
archmode esame
r 1a0=00000001800000000000000000000200 # z/Arch restart PSW
r 1d0=0002000000000000000000000000DEAD # z/Arch pgm chk new PSW disabled wait

#
r 200=B60003EC     #         STCTL R0,R0,CTLR0    Store CR0 to enable AFP
r 204=960403ED     #         OI    CTLR0+1,X'04'  Turn on AFP bit
r 208=B70003EC     #         LCTL  R0,R0,CTLR0    Reload updated CR0
r 20C=45C00600     #         BAL   R12,TESTMULT   Perform multiplications
r 210=B2B203F0     #         LPSWE WAITPSW        All done, load disabled wait PSW



# BFP Multiplication Short x Short -> Short & Short x Short -> Long, RXE and RRE 

#                            ORG   X'600'
                   #TESTMULT DS    0H                
r 600=41200004     #         LA    R2,4         Set count of multiplication operations
r 604=41300420     #         LA    R3,SHORTRES  Point to start of short BFP products
r 608=41400440     #         LA    R4,LONGRES1  Point to start of long BFP products
r 60C=415004C0     #         LA    R5,SHORTMAD  Point to start of short BFP Mult-Add results
r 610=416004E0     #         LA    R6,SHORTMSB   Point to start of short BFP Mult-Sub results
r 614=41700400     #         LA    R7,SHORTBFP  Point to start of short BFP input values
r 618=0DD0         #         BASR  R13,0        Set top of loop
#     Top of loop; clear residuals from FP registers
r 61A=B3750000     #         LZDR  R0           Zero FPR0
r 61E=B3750010     #         LZDR  R1           Zero FPR1
r 622=B3750040     #         LZDR  R4           Zero FPR4
r 626=B3750050     #         LZDR  R5           Zero FPR5
r 62A=B37500D0     #         LZDR  R13          Zero FPR13
#     Collect multiplicand and multiplier, do four multiplications, store results.
r 62E=78007000     #         LE    R0,0(,R7)    Get BFP short multiplicand
r 632=78D07004     #         LE    R13,4(,R7)   Get BFP short multiplier
r 636=3810         #         LER   R1,R0        Copy multiplicand for s*s=s RRE
r 638=3840         #         LER   R4,R0        Copy multiplicand for s*s=l RXE
r 63A=3850         #         LER   R5,R0        Copy multiplicand for s*s=l RRE
r 63C=ED0070040017 #         MEEB  R0,4(,R7)    Generate RXE s*s=s product
r 642=B317001D     #         MEEBR R1,R13       Generate RRE s*s=s product
r 646=ED407004000C #         MDEB  R4,4(,R7)    Generate RXE s*s=l product
r 64C=B30C005D     #         MDEBR R5,R13       Generate RRE s*s=l product
r 650=70003000     #         STE   R0,0(,R3)    Store short BFP RXE s*s=s product
r 654=70103004     #         STE   R1,4(,R3)    Store short BFP RRE s*s=s product
r 658=70404000     #         STE   R4,0(,R4)    Store short BFP RXE s*s=l product
r 65C=70504008     #         STE   R5,8(,R4)    Store short BFP RRE s*s=l product
r 660=78007000     #         LE    R0,0(,R7)    Reload BFP short multiplicand for mult-add RXE
r 664=3810         #         LER   R1,R0        Copy multiplicand for mult-add RRE
r 666=ED007004000E #         MAEB  R0,4(,R7),R0 Multiply third by second, add to first RXE
r 66C=B30E101D     #         MAEBR R1,R13,R1    Multiply third by second, add to the first RRE
r 670=70005000     #         STE   R0,0(,R5)    Store short BFP RXE mult-add result
r 674=70105004     #         STE   R1,4(,R5)    Store short BFP RRE mult-add result
r 678=78007000     #         LE    R0,0(,R7)    Reload BFP short multiplicand for mult-sub RXE
r 67C=3810         #         LER   R1,R0        Copy multiplicand for mult-sub RRE
r 67E=ED007004000F #         MSEB  R0,4(,R7),R0 Multiply third by second, subtract the first RXE
r 684=B30F101D     #         MSEBR R1,R13,R1    Multiply third by second, subtract the first RRE
r 688=70006000     #         STE   R0,0(,R6)    Store short BFP RXE mult-sub result
r 68C=70106004     #         STE   R1,4(,R6)    Store short BFP RRE mult-sub result
r 690=41707004     #         LA    R7,4(,R7)    Point to next short BFP input pair
r 694=41303008     #         LA    R3,8(,R3)    Point to next short BFP product pair
r 698=41404010     #         LA    R4,16(,R4)   Point to next long BFP product pair
r 69C=41505008     #         LA    R5,8(,R5)    Point to next short BFP mult-add result pair
r 6A0=41606008     #         LA    R6,8(,R6)    Point to next short BFP mult-sub result pair
r 6A4=062D         #         BCTR  R2,R13       Loop through all s*s test cases
r 6A6=47F00700     #         B     TESTLONG     Skip across patch area


# BFP Multiplication long x long -> long & long x long -> extended, RXE and RRE 
# Use Load Lengthened to expand Short BFP into the longs needed for this test.

#                            ORG   X'700'
                   #TESTLONG DS    0H
r 700=41200004     #         LA    R2,4         Set count of multiplication operations
r 704=41300480     #         LA    R3,LONGRES2  Point to start of long BFP products
r 708=41400500     #         LA    R4,EXTDRES1  Point to start of extended BFP products
r 70C=41500300     #         LA    R5,LONGMAD   Point to start of long BFP Mult-Add results
r 710=41600340     #         LA    R6,LONGMSB   Point to start of long BFP Mult-Sub results
r 714=41700400     #         LA    R7,SHORTBFP  Point to start of short BFP input values
r 718=0DD0         #         BASR  R13,0        Set top of loop for l*l tests
#     Top of loop; clear residuals from FP registers
r 71A=B3750000     #         LZDR  R0           Zero FPR0
r 71E=B3750010     #         LZDR  R1           Zero FPR1
r 722=B3750040     #         LZDR  R4           Zero FPR4
r 726=B3750050     #         LZDR  R5           Zero FPR5
r 72A=B37500D0     #         LZDR  R13          Zero FPR13
#     Collect multiplicand and multiplier, lengthen them, do four multiplications, store results.
r 72E=78007000     #         LE    R0,0(,R7)    Get BFP short multiplicand
r 732=EDD070040004 #         LDEB  R13,4(,R7)   Lengthen BFP short multiplier into long RXE
r 738=60D03000     #         STD   R13,0(,R3)   Store for use in MDB/MXDB tests
r 73C=60D05000     #         STD   R13,0(,R5)   Store for use in MADB/MADBR tests
r 740=60D06000     #         STD   R13,0(,R6)   Store for use in MSDB/MSDBR tests
r 744=B3040010     #         LDEBR R1,R0        Lengthen multiplicand for l*l=l RRE
r 748=B3040040     #         LDEBR R4,R0        Lengthen multiplicand for l*l=e RXE
r 74C=B3040050     #         LDEBR R5,R0        Lengthen multiplicand for l*l=e RRE
r 750=B3040000     #         LDEBR R0,R0        Lengthen multiplicand for l*l=l RXE (This one must be last in the list)
r 754=28F0         #         LDR   R15,R0       Save long multiplicand for MADB/MADBR/MSDB/MSDBR tests 
r 756=ED003000001C #         MDB   R0,0(,R3)    Generate RXE l*l=l product
r 75C=B31C001D     #         MDBR  R1,R13       Generate RRE l*l=l product
r 760=ED4030000007 #         MXDB  R4,0(,R3)    Generate RXE l*l=e product
r 766=B307005D     #         MXDBR R5,R13       Generate RRE l*l=e product
r 76A=60003000     #         STD   R0,0(,R3)    Store BFP RXE l*l=l product 
r 76E=60103008     #         STD   R1,8(,R3)    Store BFP RRE l*l=l product 
r 772=60404000     #         STD   R4,0(,R4)    Store BFP RXE l*l=e product part 1
r 776=60604008     #         STD   R6,8(,R4)    Store BFP RXE l*l=e product part 2
r 77A=60504010     #         STD   R5,16(,R4)   Store BFP RRE l*l=e product part 1
r 77E=60704018     #         STD   R7,24(,R4)   Store BFP RRE l*l=e product part 2
r 782=280F         #         LDR   R0,R15       Reload BFP long multiplicand for mult-add RXE
r 784=2810         #         LDR   R1,R0        Copy multiplicand for mult-add RRE
r 786=ED005000001E #         MADB  R0,0(,R5),R0 Multiply third by second, add to first RXE
r 78C=B31E101D     #         MADBR R1,R13,R1    Multiply third by second, add to the first RRE
r 790=60005000     #         STD   R0,0(,R5)    Store long BFP RXE mult-add result
r 794=60105008     #         STD   R1,8(,R5)    Store long  BFP RRE mult-add result
r 798=280F         #         LDR   R0,R15       Reload BFP long multiplicand for mult-sub RXE
r 79A=2810         #         LDR   R1,R0        Copy multiplicand for mult-sub RRE
r 79C=ED006000001F #         MSDB  R0,0(,R6),R0 Multiply third by second, subtract the first RXE
r 7A2=B31F101D     #         MSDBR R1,R13,R1    Multiply third by second, subtract the first RRE
r 7A6=60006000     #         STD   R0,0(,R6)    Store long BFP RXE mult-sub result
r 7AA=60106008     #         STD   R1,8(,R6)    Store long BFP RRE mult-sub result
r 7AE=41707004     #         LA    R7,4(,R7)    Point to next short BFP input pair
r 7B2=41303010     #         LA    R3,16(,R3)   Point to next long BFP product pair
r 7B6=41404020     #         LA    R4,32(,R4)   Point to next extended BFP product pair
r 7BA=41505010     #         LA    R5,16(,R5)   Point to next long BFP mult-add result pair
r 7BE=41606010     #         LA    R6,16(,R6)   Point to next long BFP mult-sub result pair
r 7C2=062D         #         BCTR  R2,R13       Loop through all l*l cases
r 7C4=47F00800     #         B     TESTEXT      Skip across patch area


# BFP Multiplication extended x extended -> extended, RRE only.  We do two multiplies
# per input pair so we can test a diversity of Load Lengthened instructions

#                            ORG   X'800'
                   #TESTEXT  DS    0H
r 800=41200004     #         LA    R2,4         Set count of multiplication operations
r 804=41400580     #         LA    R4,EXTDRES2  Point to start of extended BFP results
r 808=41700400     #         LA    R7,SHORTBFP  Point to start of short BFP input values
r 80C=0DD0         #         BASR  R13,0        Set top of loop for l*l tests
#     Top of loop
#     Collect multiplicand and multiplier, lengthen them, do two multiplications, store results.
r 80E=47000000
r 812=78007000     #         LE    R0,0(,R7)    Get BFP short multiplicand
r 816=B3060010     #         LXEBR R1,R0        Lengthen multiplicand into R1-R3 for e*e=e RRE (2)
r 81A=B3040000     #         LDEBR R0,R0        lengthen multiplicand to long
r 81E=70004000     #         STD   R0,0(,R4)    Save for test of LXDB RXE
r 822=ED0040000005 #         LXDB  R0,0(,R4)    Load long multiplicand, lengthen for e*e=e RRE (1)
r 828=EDD070040006 #         LXEB  R13,4(,R7)   Lengthen BFP short multiplier into ext
r 82E=EDC070040004 #         LDEB  R12,4(,R7)   lengthen short multiplier into long
r 834=B30500CC     #         LXDBR R12,R12      Lengthen long multiplier into ext
r 838=B34C000D     #         MXBR  R0,R13       Generate RRE l*l=e
r 83C=B34C001C     #         MXBR  R1,R12       Generate RRE l*l=e
r 840=60004000     #         STD   R0,0(,R4)    Store BFP RXE l*l=l
r 844=60204008     #         STD   R2,8(,R4)    Store BFP RRE l*l=l
r 848=60104010     #         STD   R1,16(,R4)   Store BFP RXE l*l=e part 1
r 84C=60304018     #         STD   R3,24(,R4)   Store BFP RXE l*l=e part 2
r 850=41707004     #         LA    R7,4(,R7)    Point to next short BFP input pair
r 854=41404020     #         LA    R3,32(,R4)   Point to next long BFP result pair
r 858=062D         #         BCTR  R2,R13       Loop through all l*l cases

r 85A=07FC         #         BR    R12          Tests done, return to mainline


#
r 3EC=00040000     # CTLR0             Control register 0 (bit45 AFP control)
r 3F0=00020000000000000000000000000000 # WAITPSW Disabled wait state PSW - normal completion
r 400=3F800000     # 1 in short BFP 
r 404=40000000     # 2 
r 408=40800000     # 4
r 40C=C0000000     # -2
r 410=C0000000     # -2

# 300.40             LONGMAD:   Results from (l*l)+l=l
# 340.40             LONGMSB:   Results from (l*l)-l=l
# 420.20             SHORTRES:  Results from s*s=s
# 440.40             LONGRES1:  Results from s*s=l
# 480.40             LONGRES2:  Results from l*l=l
# 4C0.20             SHORTMAD:  Results from (s*s)+s=s
# 4E0.20             SHORTMSB:  Results from (s*s)-s=s
# 500.80             EXTDRES1:  Results from l*l=e
# 580.80             EXTDRES2:  Results from e*e=e

t+ 77e-792
runtest 

*Compare
r 420.10  # BFP Short Products part 1 Expecting 2 2 8 8
*Want "s*s->s BFP 1/2" 40000000 40000000 41000000 41000000
*Compare
r 430.10  # BFP Short Products part 2 -8 -8 4 4
*Want "s*s->s BFP 2/2" C1000000 C1000000 40800000 40800000

*Compare
r 4C0.10  # BFP Short M-Add part 1 Expecting 3 3 10 10
*Want "Short M-Add BFP 1/2" 40400000 40400000 41200000 41200000
*Compare
r 4D0.10  # BFP Short M-Add part 2 Expecting -4, -4, 2, 2
*Want "Short M-Add BFP 2/2" C0800000 C0800000 40000000 40000000 

*Compare
r 4E0.10  # BFP Short M-Sub part 1 Expecting 1, 1, 6, 6
*Want "Short M-Sub BFP 1/2" 3F800000 3F800000 40C00000 40C00000
*Compare
r 4F0.10  # BFP Short M-Sub part 2 Expecting -12, -12, 6, 6
*Want "Short M-Sub BFP 2/2" C1400000 C1400000 40C00000 40C00000 

*Compare
r 440.10  # BFP Long products from short x short part 1 Expecting 2.0 2.0
*Want "s*s->l 1/4" 40000000 00000000 40000000 00000000
*Compare
r 450.10  # BFP Long products from short x short part 2 Expecting 8.0 8.0
*Want "s*s->l 2/4" 40200000 00000000 40200000 00000000
*Compare
r 460.10  # BFP Long products from short x short part 3 Expecting -8.0 -8.0
*Want "s*s->l 3/4" C0200000 00000000 C0200000 00000000
*Compare
r 470.10  # BFP Long products from short x short part 4 Expecting 4.0 4.0
*Want "s*s->l 4/4" 40100000 00000000 40100000 00000000

*Compare
r 480.10  # BFP Long products from long x long part 1
*Want "l*l->l 1/4" 40000000 00000000 40000000 00000000
*Compare
r 490.10  # BFP Long products from long x long part 2
*Want "l*l->l 2/4" 40200000 00000000 40200000 00000000
*Compare
r 4A0.10  # BFP Long products from long x long part 3
*Want "l*l->l 3/4" C0200000 00000000 C0200000 00000000
*Compare
r 4B0.10  # BFP Long products from long x long part 4
*Want "l*l->l 4/4" 40100000 00000000 40100000 00000000

*Compare
r 300.10  # BFP Long M-Add part 1 Expecting 3 3 
*Want "Long M-Add BFP 1/4" 40080000 00000000 40080000 00000000
*Compare
r 310.10  # BFP Long M-Add part 2 Expecting 10 10
*Want "Long M-Add BFP 2/4" 40240000 00000000 40240000 00000000
*Compare
r 320.10  # BFP Long M-Add part 3 Expecting -4, -4
*Want "Long M-Add BFP 3/4" C0100000 00000000 C0100000 00000000
*Compare
r 330.10  # BFP Long M-Add part 4 Expecting 2, 2
*Want "Long M-Add BFP 4/4" 40000000 00000000 40000000 00000000

*Compare
r 340.10  # BFP Long M-Sub part 1 Expecting 1, 1
*Want "Long M-Sub BFP 1/4" 3FF00000 00000000 3FF00000 00000000
*Compare
r 350.10  # BFP Long M-Sub part 2 Expecting 6, 6
*Want "Long M-Sub BFP 2/4" 40180000 00000000 40180000 00000000
*Compare
r 360.10  # BFP Long M-Sub part 3 Expecting -12, -12
*Want "Long M-Sub BFP 3/4" C0280000 00000000 C0280000 00000000
*Compare
r 370.10  # BFP Long M-Sub part 4 Expecting 6, 6
*Want "Long M-Sub BFP 4/4" 40180000 00000000 40180000 00000000

*Compare
r 500.10  # BFP Extended products from long x long part 1a
*Want "l*l->e 1a/4" 40000000 00000000 00000000 00000000
*Compare
r 510.10  # BFP Extended products from long x long part 1b
*Want "l*l->e 1b/4" 40000000 00000000 00000000 00000000
*Compare
r 520.10  # BFP Extended products from long x long part 2a
*Want "l*l->e 2a/4" 40020000 00000000 00000000 00000000
*Compare
r 530.10  # BFP Extended products from long x long part 2b
*Want "l*l->e 2b/4" 40020000 00000000 00000000 00000000
*Compare
r 540.10  # BFP Extended products from long x long part 3a
*Want "l*l->e 3a/4" C0020000 00000000 00000000 00000000
*Compare
r 550.10  # BFP Extended products from long x long part 3b
*Want "l*l->e 3b/4" C0020000 00000000 00000000 00000000
*Compare
r 560.10  # BFP Extended products from long x long part 4a
*Want "l*l->e 4a/4" 40010000 00000000 00000000 00000000
*Compare
r 570.10  # BFP Extended products from long x long part 4b
*Want "l*l->e 4b/4" 40010000 00000000 00000000 00000000

*Compare
r 580.10  # BFP Extended products from extended x extended part 1a
*Want "e*e->e 1a/4" 40000000 00000000 00000000 00000000
*Compare
r 590.10  # BFP Extended products from extended x extended part 1b
*Want "e*e->e 1b/4" 40000000 00000000 00000000 00000000
*Compare
r 5A0.10  # BFP Extended products from extended x extended part 2a
*Want "e*e->e 2a/4" 40020000 00000000 00000000 00000000
*Compare
r 5B0.10  # BFP Extended products from extended x extended part 2b
*Want "e*e->e 2b/4" 40020000 00000000 00000000 00000000
*Compare
r 5C0.10  # BFP Extended products from extended x extended part 3a
*Want "e*e->e 3a/4" C0020000 00000000 00000000 00000000
*Compare
r 5D0.10  # BFP Extended products from extended x extended part 3b
*Want "e*e->e 3b/4" C0020000 00000000 00000000 00000000
*Compare
r 5E0.10  # BFP Extended products from extended x extended part 4a
*Want "e*e->e 4a/4" 40010000 00000000 00000000 00000000
*Compare
r 5F0.10  # BFP Extended products from extended x extended part 4b
*Want "e*e->e 4b/4" 40010000 00000000 00000000 00000000

# Following useful for script testing; not required for instruction validation
#*Compare
#r 150.10    # No program checks
#*Want "No ProgChk"  00000000 00000000 00000000 00000000
# Comment this back out before you commit.

*Done



*Testcase IEEE-NaNs Test DIVIDE instruction with zero, QNaNs and SNaNs
*Message Testcase ieee-div-nans.tst: IEEE Division with zero and NaNs
*Message ..Test case values include zero, QNaNs, and SNaNs
*Message ..Compare failures from this test are a known issue and 
*Message ..need not be reported.  

#  Divide adjacent pairs of values in the input set (six values means five results).
#
#  Test data:         2,    0,      QNaN(1),      QNaN(2),      SNaN(3),      SNaN(4). 
#  Expected Results    +inf, QNaN(1),      QNaN(1),      SNaN(3),      SNaN(3).
#
#  NaN payload--in parentheses--is used to confirm that the right NaN ends up in the result.  
#
# Tests seven division instructions:
#   DIVIDE (BFP short RRE) DEBR
#   DIVIDE (BFP short RXE) DEB
#   DIVIDE (BFP long, RRE) DDBR
#   DIVIDE (BFP long, RXE) DDB
#   DIVIDE (BFP extended, RRE) DXBR
#
# Interrupts are masked; no program checks are expected
#
# Also tests the following floating point support instructions
#   LOAD  (Short)
#   LOAD  (Long)
#   LOAD ZERO (Long)
#   STORE (Short)
#   STORE (Long)



sysclear
archmode esame

r 1a0=00000001800000000000000000000200 # z/Arch restart PSW
r 1d0=0002000000000000000000000000DEAD # z/Arch pgm chk new PSW disabled wait

# Mainline program.  
r 200=B60003EC     #         STCTL R0,R0,CTLR0    Store CR0 to enable AFP
r 204=960403ED     #         OI    CTLR0+1,X'04'  Turn on AFP bit
r 208=B70003EC     #         LCTL  R0,R0,CTLR0    Reload updated CR0
r 20C=4DC00600     #         BAS   R12,TESTDIV    Perform divisions
r 210=B2B203F0     #         LPSWE WAITPSW        All done, load disabled wait PSW


# BFP Division Short RXE and RRE using NaNs as inputs.

# We cannot use Load Rounded to shrink extended BFP into the shorts needed 
# for this test because Load Rounded will convert the SNaNs into QNaNs

#                            ORG   X'600'
                   #TESTDIV  DS    0H           
r 600=41200005     #         LA    R2,5         Set count of division operations
r 604=41300400     #         LA    R3,SHORTRES  Point to start of short BFP quotients
r 608=41700300     #         LA    R7,SHORTBFP  Point to start of short BFP input values
r 60C=0DD0         #         BASR  R13,0        Set top of loop for short BFP tests

#     Top of loop; clear residuals from FPR0, 4, 5
r 60E=B3750000     #         LZDR  R0           Zero FPR0
r 612=B3750040     #         LZDR  R4           Zero FPR4
#     Collect dividend and divisor, do four divisions, store quotients.
r 616=78007000     #         LE    R0,0(,R7)    Get BFP ext dividend
r 61A=2850         #         LDR   R5,R0        Duplicate dividend for RRE test
r 61C=78407004     #         LE    R4,4(,R7)    Get BFP ext divisor for RRE test
r 620=ED007004000D #         DEB   R0,4(,R7)    Generate RXE
r 626=B30D0054     #         DEBR  R5,R4        Generate RRE
r 62A=70003000     #         STE   R0,0(,R3)    Store short BFP RXE
r 62E=70503004     #         STE   R5,4(,R3)    Store short BFP RRE
r 632=41707004     #         LA    R7,4(,R7)    Point to next short BFP input pair
r 636=41303008     #         LA    R3,8(,R3)    Point to next short BFP result pair
r 63A=062D         #         BCTR  R2,R13       Loop through all short BFP test cases
r 63C=47F00700     #         B     TESTLONG     Go test long BFP division.


# BFP Division long RXE and RRE 

                             ORG   X'700'
                   #TESTLONG DS    0H
r 700=41200004     #         LA    R2,5         Set count of division operations
r 704=41300440     #         LA    R3,LONGRES   Point to start of long BFP quotients
r 708=41700320     #         LA    R7,LONGBFP   Point to start of extended BFP input values
r 70C=0DD0         #         BASR  R13,0        Set top of loop for long BFP tests
#     Top of loop
#     Collect dividend and divisor, do two divisions, store quotients
r 70E=68007000     #         LD    R0,0(,R7)    Get BFP ext dividend
R 712=2850         #         LDR   R5,R0        Save dividend for RRE 
r 714=68407008     #         LD    R4,8(,R7)    Get BFP ext divisor
r 718=ED007008001D #         DDB   R0,8(,R7)    Generate RXE long
r 71E=B31D0054     #         DDBR  R5,R4        Generate RRE long
r 722=60003000     #         STD   R0,0(,R3)    Store long BFP RXE quotient
r 726=60503008     #         STD   R5,8(,R3)    Store long BFP RRE quotient
r 72A=41707008     #         LA    R7,8(,R7)    Point to next long BFP input pair
r 72E=41303010     #         LA    R3,16(,R3)   Point to next long BFP result pair
r 732=062D         #         BCTR  R2,R13       Loop through all long test cases
r 734=47F00800     #         B     TESTEXT      Skip across patch area


# BFP division extended, RRE only.  

#                            ORG   X'800'
                   #TESTEXT  DS    0H
r 800=41200005     #         LA    R2,5         Set count of division operations
r 804=41300500     #         LA    R4,EXTDRES   Point to start of extended BFP quotients
r 808=41700360     #         LA    R7,EXTDBFP   Point to start of extended BFP input values
r 80C=0DD0         #         BASR  R13,0        Set top of loop for l*l tests
#     Top of loop
#     Collect dividend & divisor, do division and Load FP Integer, store results.
r 80E=68007000     #         LD    R0,0(,R7)    Get BFP ext dividend part 1
r 812=68207008     #         LD    R2,8(,R7)    Get BFP ext dividend part 2
r 816=68D07010     #         LD    R13,16(,R7)  Get BFP ext divisor part 1
r 81A=68F07018     #         LD    R15,24(,R7)  Get BFP ext divisor part 2
r 81E=B34D000D     #         DXBR  R0,R13       Generate RRE extended quotient
r 822=60003000     #         STD   R0,0(,R3)    Store extended BFP RRE quotient part 1
r 826=60203008     #         STD   R2,8(,R3)    Store extended BFP RRE quotient part 2
r 82A=41707010     #         LA    R7,16(,R7)   Point to next extended BFP input pair
r 82E=41303010     #         LA    R4,16(,R3)   Point to next extended BFP result
r 832=062D         #         BCTR  R2,R13       Loop through all test cases

r 834=07FC         #         BR    R12          Tests done, return to mainline


#
r 3EC=00040000     # CTLR0             Control register 0 (bit45 AFP control)
r 3F0=00020000000000000000000000000000 # WAITPSW Disabled wait state PSW - normal completion

# 300.20  SHORTBFP   DS  0D    6 short BFP (Room for 8)
r 300=40000000      #    DC    2
r 304=00000000      #    DC    0
r 308=7FC10000      # QNaN(1)
r 30C=7FC20000      # QNaN(2)
r 310=7F830000      # SNaN(3)
r 314=7F840000      # SNaN(4)

# 320.40  LONGBFP   DS  0D    6 long BFP (Room for 8)
r 320=4000000000000000    # 2
r 328=0000000000000000    # 0
r 330=7FFC100000000000    # QNaN(1)
r 338=7FFC200000000000    # QNaN(2)
r 340=7FF8300000000000    # SNaN(3)
r 348=7FF8400000000000    # SNaN(4)

# 360.80  EXTDBFP   DS  0D    6 Extended BFP (Room for 8, but not 9)
r 360=40000000000000000000000000000000    # 2
r 370=00000000000000000000000000000000    # 0
r 380=7FFF8100000000000000000000000000    # QNaN(1)
r 390=7FFF8200000000000000000000000000    # QNaN(2)
r 3A0=7FFF0300000000000000000000000000    # SNaN(3)
r 3B0=7FFF0400000000000000000000000000    # SNaN(4)

# 400.40             SHORTRES DS 8D    Results from short divide, five pairs, room for 8
# 440.80             LONGRES  DS 16D   Results from long divide, five pairs, room for 8
# 500.80             EXTDRES  DS 16D   Results from extended divide, five results, room for 8


runtest 

r 400.40       # Display short BFP results in test log
r 440.80       # Display long BFP results in test log
r 500.80       # Display extended BFP results in test log

*Compare
r 400.10  # BFP Short quotients part 1 Expecting +inf, +inf, QNaN(1), QNaN(1)
*Want "DEB/DEBR NaN 1/4" 7F800000 7F800000 7FC10000 7FC10000
*Compare
r 410.10  # BFP Short quotients part 2 Expecting QNaN(1), QNaN(1), QNaN(3), QNaN(3)
*Want "DEB/DEBR NaN 2/4" 7FC10000 7FC10000 7FC30000 7FC30000
*Compare
r 420.10  # BFP Short quotients part 3 Expecting QNaN(3), QNaN(3), 0, 0
*Want "DEB/DEBR NaN 3/4" 7FC30000 7FC30000 00000000 00000000
*Compare
r 430.10  # BFP Short quotients part 4 Expecting QNaN(3), QNaN(3), 0, 0
*Want "DEB/DEBR NaN 4/4" 00000000 00000000 00000000 00000000

*Compare
r 440.10  # BFP Long quotients part 1 Expecting +inf, +inf
*Want "DDB/DDBR NaN 1/8" 7FF00000 00000000 7FF00000 00000000
*Compare
r 450.10  # BFP Long quotients part 2 Expecting QNaN(1), QNaN(1)
*Want "DDB/DDBR NaN 2/8" 7FFC1000 00000000 7FFC1000 00000000
*Compare
r 460.10  # BFP Long quotients part 3 Expecting QNaN(1), QNaN(1)
*Want "DDB/DDBR NaN 3/8" 7FFC1000 00000000 7FFC1000 00000000
*Compare
r 470.10  # BFP Long quotients part 4 Expecting QNaN(3), QNaN(3)
*Want "DDB/DDBR NaN 4/8" 7FFC3000 00000000 7FFC3000 00000000
*Compare
r 480.10  # BFP Long quotients part 5 Expecting QNaN(3), QNaN(3)
*Want "DDB/DDBR NaN 5/8" 7FFC3000 00000000 7FFC3000 00000000
*Compare
r 490.10  # BFP Long quotients part 6 Expecting 0, 0
*Want "DDB/DDBR NaN 6/8" 00000000 00000000 00000000 00000000
*Compare
r 4A0.10  # BFP Long quotients part 7 Expecting 0, 0
*Want "DDB/DDBR NaN 7/8" 00000000 00000000 00000000 00000000
*Compare
r 4B0.10  # BFP Long quotients part 8 Expecting 0, 0
*Want "DDB/DDBR NaN 8/8" 00000000 00000000 00000000 00000000

*Compare
r 500.10  # BFP Extended quotients part 1 Expecting +inf
*Want "DXBR NaN 1/8" 7FFF0000 00000000 00000000 00000000
*Compare
r 510.10  # BFP Extended quotients part 2 Expecting QNaN(1)
*Want "DXBR NaN 2/8" 7FFF8100 00000000 00000000 00000000
*Compare
r 520.10  # BFP Extended quotients part 3 Expecting QNaN(1)
*Want "DXBR NaN 3/8" 7FFF8100 00000000 00000000 00000000
*Compare
r 530.10  # BFP Extended quotients part 4 Expecting QNaN(3)
*Want "DXBR NaN 4/8" 7FFF8300 00000000 00000000 00000000
*Compare
r 540.10  # BFP Extended quotients part 5 Expecting QNaN(3)
*Want "DXBR NaN 5/8" 7FFF8300 00000000 00000000 00000000
*Compare
r 550.10  # BFP Extended quotients part 6 Expecting 0
*Want "DXBR NaN 6/8" 00000000 00000000 00000000 00000000
*Compare
r 560.10  # BFP Extended quotients part 7 Expecting 0
*Want "DXBR NaN 7/8" 00000000 00000000 00000000 00000000
*Compare
r 570.10  # BFP Extended quotients part 8 Expecting 0
*Want "DXBR NaN 8/8" 00000000 00000000 00000000 00000000
*Done



*Testcase IEEE LOAD ROUNDED (3), LOAD FP INTEGER (3) DIVIDE (5), DIVIDE TO INTEGER (2) 13 instr total


#  Divide adjacent pairs of values in the input set (five values means four
#  quotients).  Test data: 1, 2, 4, -2, -2; expected quotients 0.5, 0.5, -2, 1
#  Load Floating Point Integer of the above result set.  Expected values 0, 0, -2, 1
#  For short and long BFP, divide adjacent pairs using DIVIDE TO INTEGER.
#      Expected quotient/remainder pairs: 0/1, 0/2, 2/0, 1/-0 (Yes, minus zero)
#
# Tests seven division instructions:
#   DIVIDE (BFP short RRE) DEBR
#   DIVIDE (BFP short RXE) DEB
#   DIVIDE (BFP long, RRE) DDBR
#   DIVIDE (BFP long, RXE) DDB
#   DIVIDE (BFP extended, RRE) DXBR
#   DIVIDE TO INTEGER (BFP short RRE) DIEBR
#   DIVIDE TO INTEGER (BFP long RRE) DIDBR
#
# Also tests the following six conversion instructions
#   LOAD ROUNDED (long BFP to short BFP, RRE) LEDBR
#   LOAD ROUNDED (extended BFP to short BFP, RRE) LEXBR
#   LOAD ROUNDED (extended BFP to long BFP, RRE) LDXBR
#   LOAD FP INTEGER (BFP short RRE) FIEBR
#   LOAD FP INTEGER (BFP long RRE) FIDBR
#   LOAD FP INTEGER (BFP extended RRE) FIXBR
#
# Intermediate results from the Load Rounded instructions are not saved.  Because
# zero is not present in the test set, any error in Load Rounded will appear in 
# the resulting quotients, remainders, and integer floating point values.
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


# BFP Division Short RXE and RRE
# Use Load Rounded to shrink extended BFP into the shorts needed for this test.

#                            ORG   X'600'
                   #TESTDIV  DS    0H           
r 600=41200004     #         LA    R2,4         Set count of division operations
r 604=41300420     #         LA    R3,SHORTRES  Point to start of short BFP quotients
r 608=414004C0     #         LA    R4,SHORTDTI  Point to short Divide Integer quotients and remainders
r 60C=415004E0     #         LA    R5,SHORTLFP  Point to short Load Floating Point Integer results
r 610=41700300     #         LA    R7,EXTBFPIN  Point to start of extended BFP input values
r 614=0DD00700     #         BASR  R13,0        Set top of loop for short BFP tests
# ****** Get rid of NOPR above
#     Top of loop; clear residuals from FPR1, 4, 5, 8, 9.
r 618=B3750000     #         LZDR  R1           Zero FPR1
r 61C=B3750040     #         LZDR  R4           Zero FPR4
r 620=B3750050     #         LZDR  R5           Zero FPR5
r 624=B3750080     #         LZDR  R8           Zero FPR8
r 624=B3750090     #         LZDR  R9           Zero FPR9
#     Collect dividend and divisor, do four divisions, store quotients.
r 628=68007000     #         LD    R0,0(,R7)    Get BFP ext dividend part 1
r 62C=68207008     #         LD    R2,8(,R7)    Get BFP ext dividend part 2
r 630=68D07010     #         LD    R13,16(,R7)  Get BFP ext divisor part 1
r 634=68F07018     #         LD    R15,24(,R7)  Get BFP ext divisor part 2
r 638=B3450010     #         LDXBR R1,R0        Round dividend for RXE to long
r 63C=B3440011     #         LEDBR R1,R1        Round dividend for RXE to short
r 640=B3460050     #         LEXBR R5,R0        Round dividend for RRE
r 644=3895         #         LER   R9,R5        Save dividend for Divide to Integer
r 646=B346004D     #         LEXBR R4,R13       Round divisor for RRE
r 64A=70403000     #         STE   R4,0(,R3)    Save divisor for RXE
r 64E=B3538094     #         DIEBR R9,R8,R4     Divide to Integer, quotient in R8, remainder in R9
r 652=ED103000000D #         DEB   R1,0(,R3)    Generate RXE
r 658=B30D0054     #         DEBR  R5,R4        Generate RRE
r 65C=B3570045     #         FIEBR R4,0,R5      Load result as Floating Point Integer
r 660=70103000     #         STE   R1,0(,R3)    Store short BFP RXE
r 664=70503004     #         STE   R5,4(,R3)    Store short BFP RRE
r 668=70804000     #         STE   R8,0(,R4)    Store divide to integer quotient
r 66C=70904004     #         STE   R9,4(,R4)    Store divide to integer remainder
r 670=70405000     #         STE   R4,0(,R5)    Store load FP integer result
r 674=41707010     #         LA    R7,16(,R7)   Point to next extended BFP input pair
r 678=41303008     #         LA    R3,8(,R3)    Point to next short BFP result pair
r 67C=41404008     #         LA    R4,8(,R4)    Point to next Divide to Integer result
r 680=41505004     #         LA    R5,4(,R5)    Point to next Load FP Integer result
r 684=062D         #         BCTR  R2,R13       Loop through all short BFP test cases
r 686=47F00700     #         B     TESTLONG     Go test long BFP division.


# BFP Division long RXE and RRE 
# Use Load Rounded to shrink extended BFP input into the longs needed for this test.

#                            ORG   X'700'
                   #TESTLONG DS    0H
r 700=41200004     #         LA    R2,4         Set count of division operations
r 704=41300440     #         LA    R3,LONGRES   Point to start of long BFP quotients
r 708=41400500     #         LA    R4,LONGDTI   Point to short Divide Integer quotients and remainders
r 70C=41500540     #         LA    R5,LONGLFP   Point to short Load Floating Point Integer results
r 710=41700300     #         LA    R7,EXTBFPIN  Point to start of extended BFP input values
r 714=0DD0         #         BASR  R13,0        Set top of loop for long BFP tests
#     Top of loop
#     Collect dividend and divisor, shorten them, do four divisions, store quotients
r 716=68007000     #         LD    R0,0(,R7)    Get BFP ext dividend part 1
r 71A=68207008     #         LD    R2,8(,R7)    Get BFP ext dividend part 2
r 71E=68D07010     #         LD    R13,16(,R7)  Get BFP ext divisor part 1
r 722=68F07018     #         LD    R15,24(,R7)  Get BFP ext divisor part 2
r 726=B3450010     #         LDXBR R1,R0        Round dividend for RXE 
r 72A=B3450050     #         LDXBR R5,R0        Round dividend for RRE
r 72E=2895         #         LDR   R9,R5        Save dividend for divide to Integer
r 730=B345004D     #         LDXBR R4,R13       Round divisor for RRE
r 734=60403000     #         STD   R4,0(,R3)    Save divisor for RXE
r 738=B35B8094     #         DIDBR R9,R8,R4     Divide to Integer, quotient in R8, remainder in R9
r 73C=ED103000001D #         DDB   R1,0(,R3)    Generate RXE long
r 742=B31D0054     #         DDBR  R5,R4        Generate RRE long
r 746=B35F0045     #         FIEBR R4,0,R5      Load result as Floating Point Integer
r 74A=60103000     #         STD   R1,0(,R3)    Store long BFP RXE quotient
r 74E=60503008     #         STD   R5,8(,R3)    Store long BFP RRE quotient
r 752=60804000     #         STD   R8,0(,R4)    Store long BFP divide to integer quotient
r 756=60904008     #         STD   R9,8(,R4)    Store long BFP divide to integer remainder
r 75A=60405000     #         STD   R5,0(,R5)    Store long BFP load FP integer result
r 75E=41707010     #         LA    R7,16(,R7)   Point to next extended BFP input pair
r 762=41303010     #         LA    R3,16(,R3)   Point to next long BFP result pair
r 766=41404010     #         LA    R4,16(,R4)   Point to next long BFP divide to integer quotient and remainder
r 76A=41505008     #         LA    R5,8(,R5)    Point to next long BFP Load FP Integer result
r 76E=062D         #         BCTR  R2,R13       Loop through all long test cases
r 770=47F00800     #         B     TESTEXT      Skip across patch area

# BFP division extended, RRE only.  We do two divisions per input pair so we can 
# test a diversity of Load Rounded instructions.  

#                            ORG   X'800'
                   #TESTEXT  DS    0H
r 800=41200004     #         LA    R2,4         Set count of division operations
r 804=41400480     #         LA    R4,EXTDRES   Point to start of extended BFP quotients
r 808=41500560     #         LA    R5,EXTLFP    Point to start of extended Load FP Integer results
r 80C=41700300     #         LA    R7,EXTBFPIN  Point to start of extended BFP input values
r 810=0DD00700     #         BASR  R13,0        Set top of loop for l*l tests
# ***********get rid of the nop above..
#     Top of loop
#     Collect dividend & divisor, do division and Load FP Integer, store results.
r 814=68007000     #         LD    R0,0(,R7)    Get BFP ext dividend part 1
r 818=68207008     #         LD    R2,8(,R7)    Get BFP ext dividend part 2
r 81C=68D07010     #         LD    R13,16(,R7)  Get BFP ext divisor part 1
r 820=68F07018     #         LD    R15,24(,R7)  Get BFP ext divisor part 2
r 824=B34D000D     #         DXBR  R0,R13       Generate RRE extended quotient
r 828=B3470010     #         FIXBR R1,0,R0      create Load FP Integer result
r 82C=60004000     #         STD   R0,0(,R4)    Store extended BFP RRE quotient part 1
r 830=60204008     #         STD   R2,8(,R4)    Store extended BFP RRE quotient part 2
r 834=60105000     #         STD   R1,0(,R5)    Store extended BFP Load FP Integer part 1
r 838=60305008     #         STD   R3,8(,R5)    Store extended BFP Load FP Integer part 2
r 83C=41707010     #         LA    R7,16(,R7)   Point to next extended BFP input pair
r 840=41404010     #         LA    R4,16(,R4)   Point to next extended BFP result
r 844=41505010     #         LA    R5,16(,R5)   Point to next extended BFP Load FP Result
r 848=062D         #         BCTR  R2,R13       Loop through all test cases

r 84A=07FC         #         BR    R12          Tests done, return to mainline


#
r 3EC=00040000     # CTLR0             Control register 0 (bit45 AFP control)
r 3F0=00020000000000000000000000000000 # WAITPSW Disabled wait state PSW - normal completion
# 300.50  EXTBFPIN
r 300=3FFF0000000000000000000000000000 # 1.0 BFP Extended
r 310=40000000000000000000000000000000 # 2.0 BFP Extended
r 320=40010000000000000000000000000000 # 4.0 BFP Extended
r 330=C0000000000000000000000000000000 # -2.0 BFP Extended
r 340=C0000000000000000000000000000000 # -2.0 BFP Extended

# 420.20             SHORTRES:  Results from short divide, four pairs
# 440.40             LONGRES:   Results from long divide, four pairs
# 480.40             EXTDRES:   Results from extended divide, four results
# 4C0.20             SHORTDTI:  Results from short Divide to Integer, four quotient/remainder pairs
# 4E0.10             SHORTLFP:  Results from short Load Floating Point Integer, four results
# 500.40             LONGDTI:   Results from long Divide to Integer, four quotient/remainder pairs
# 540.20             LONGLFP:   Results from long Load Floating Point Integer, four results
# 560.40             EXTLFP:    Results from Extended Load Floating Point Integer, four results

runtest 

r 420.20       # Display short BFP results in test log
r 4C0.20       # Display short BFP Divide to Integer results in test log
r 4E0.10       # Display short BFP Load FP Integer results in test log
r 440.40       # Display long BFP results in test log
r 500.40       # Display long BFP Divide to Integer results in test log
r 540.20       # Display long BFP Load FP Integer results in test log
r 480.40       # Display extended BFP results in test log
r 560.40       # Display extended BFP Load FP Integer results in test log

*Compare
r 420.10  # BFP Short quotients part 1 Expecting 0.5 0.5 0.5 0.5
*Want "Short BFP results 1/2" 3F000000 3F000000 3F000000 3F000000
*Compare
r 430.10  # BFP Short quotients part 2 Expecting -2 -2 1 1
*Want "Short BFP results 2/2" C0000000 C0000000 3F800000 3F800000

*Compare
r 4C0.10  # BFP Short Divide to Integer part 1 Expecting 0.0 1.0 0.0 2.0
*Want "Short BFP Divide to Int 1/2" 00000000 3F800000 00000000 40000000
*Compare
r 4D0.10  # BFP Short Divide to Integer part 2 Expecting -2.0 0.0 1.0 -0.0
*Want "Short BFP Divide to Int 2/2" C0000000 00000000 3F800000 80000000
# Yup, -0.  Remainder sign in DIEBR is exclusive or of divisor and dividend signs.  

*Compare
r 4E0.10  # BFP Short Load FP Integer Expecting 0.0 0.0 -2.0 1.0
*Want "Short BFP Divide to Int 1/2" 00000000 00000000 C0000000 3F800000

*Compare
r 440.10  # BFP Long quotients part 1 Expecting 0.5 0.5
*Want "Long BFP results 1/4" 3FE00000 00000000 3FE00000 00000000
*Compare
r 450.10  # BFP Long quotients part 2 Expecting 0.5 0.5
*Want "Long BFP results 2/4" 3FE00000 00000000 3FE00000 00000000
*Compare
r 460.10  # BFP Long quotients part 3 Expecting -2.0 -2.0
*Want "Long BFP results 3/4" C0000000 00000000 C0000000 00000000
*Compare
r 470.10  # BFP Long quotients part 4 Expecting 1.0 1.0
*Want "Long BFP results 4/4" 3FF00000 00000000 3FF00000 00000000

*Compare
r 500.10  # BFP Long divide to integer part 1 Expecting 0.0 1.0
*Want "Long BFP divide to int 1/4" 00000000 00000000 3FF00000 00000000
*Compare
r 510.10  # BFP Long divide to integer part 2 Expecting 0.0 2.0
*Want "Long BFP divide to int 2/4" 00000000 00000000 40000000 00000000
*Compare
r 520.10  # BFP Long divide to integer part 3 Expecting -2.0 0.0
*Want "Long BFP divide to int 3/4" C0000000 00000000 00000000 00000000
*Compare
r 530.10  # BFP Long divide to integer part 4 Expecting 1.0 -0
*Want "Long BFP divide to int 4/4" 3FF00000 00000000 80000000 00000000

*Compare
r 480.10  # BFP Extended quotients part 1
*Want "Ext. BFP Results 1/4" 3FFE0000 00000000 00000000 00000000
*Compare
r 490.10  # BFP Extended quotients part 2
*Want "Ext. BFP Results 2/4" 3FFE0000 00000000 00000000 00000000
*Compare
r 4A0.10  # BFP Extended quotients part 3
*Want "Ext. BFP Results 3/4" C0000000 00000000 00000000 00000000
*Compare
r 4B0.10  # BFP Extended quotients part 4
*Want "Ext. BFP Results 4/4" 3FFF0000 00000000 00000000 00000000

*Compare 
r 560.10  # BFP Extended load FP integer part 1 Expecting 0.0
*Want "Ext. BFP load FP integer 1/4" 00000000 00000000 00000000 00000000
*Compare
r 570.10  # BFP Extended load FP integer part 2 Expecting 0.0
*Want "Ext. BFP load FP integer 2/4" 00000000 00000000 00000000 00000000
*Compare
r 580.10  # BFP Extended load FP integer part 3 Expecting -2.0
*Want "Ext. BFP load FP integer 3/4" C0000000 00000000 00000000 00000000
*Compare
r 590.10  # BFP Extended load FP integer part 4 Expecting 1.0
*Want "Ext. BFP load FP integer 4/4" 3FFF0000 00000000 00000000 00000000

*Done


*
*Testcase IEEE CONVERT FROM/TO FIXED (12), SQUARE ROOT (5), 17 instr total
* SQUARE ROOT tests - Binary Floating Point
*
#
# Tests five square root instructions:
#   SQUARE ROOT (extended BFP, RRE) 
#   SQUARE ROOT (long BFP, RRE) 
#   SQUARE ROOT (long BFP, RXE) 
#   SQUARE ROOT (short BFP, RRE) 
#   SQUARE ROOT (short BFP, RXE) 
#
# Also tests the following twelve conversion instructions
#   CONVERT FROM FIXED (32 to short BFP, RRE)
#   CONVERT FROM FIXED (32 to long BFP, RRE) 
#   CONVERT FROM FIXED (32 to extended BFP, RRE)  
#   CONVERT TO FIXED (32 to short BFP, RRE)
#   CONVERT TO FIXED (32 to long BFP, RRE) 
#   CONVERT TO FIXED (32 to extended BFP, RRE)  
#   CONVERT FROM FIXED (64 to short BFP, RRE)
#   CONVERT FROM FIXED (64 to long BFP, RRE) 
#   CONVERT FROM FIXED (64 to extended BFP, RRE)  
#   CONVERT TO FIXED (64 to short BFP, RRE)
#   CONVERT TO FIXED (64 to long BFP, RRE) 
#   CONVERT TO FIXED (64 to extended BFP, RRE)  
#
# Also tests the following floating point support instructions
#   LOAD  (Short)
#   LOAD  (Long)
#   LOAD ZERO (Long)
#   STORE (Short)
#   STORE (Long)
#
#  Convert integers 1, 2, 4, 16 to each BFP floating point format
#  Take the square root of each in each BFP format
#  Convert back to integers (Note: SQRT(2) will/should round)
#  Because all inputs must be positive, we'll use this rig to test 
#     logical (unsigned) conversions when that support is added to 
#     Hercules.   And we will test 32 and 64 bit logical conversions.  
#     The comments will still say "integers" though. 
#
sysclear
archmode esame
r 1a0=00000001800000000000000000000200 # z/Arch restart PSW
r 1d0=0002000000000000000000000000DEAD # z/Arch pgm chk new PSW disabled wait

#
r 200=B60003EC     #         STCTL R0,R0,CTLR0  Store CR0 to enable AFP
r 204=960403ED     #         OI    CTLR0+1,X'04'  Turn on AFP bit
r 208=B70003EC     #         LCTL  R0,R0,CTLR0  Reload updated CR0
r 20C=45C00800     #         BAL   R12,CVTINPUT Convert inputs to binary formats
r 210=45C00600     #         BAL   R12,TESTSQRT Perform subtractions
r 214=45C00900     #         BAL   R12,CVTOUTS  Convert results to integers
r 218=B2B203F0     #         LPSWE WAITPSW      All done, load enabled wait PSW

# Take square roots of the four test cases in each precision

# 600               TESTSQRT DS    0H                
# Calculate square roots of values 
r 600=41200004     #         LA    R2,4         Set count of square root operations
r 604=41300500     #         LA    R3,SHORTRES  Point to start of short BFP results
r 608=41400520     #         LA    R4,LONGRES   Point to start of long BFP results
r 60C=41500560     #         LA    R5,EXTRES    Point to start of extended BFP results
r 610=41700410     #         LA    R7,SHORTBFP  Point to start of short BFP input values
r 614=41800420     #         LA    R8,LONGBFP   Point to start of long BFP input values
r 618=41900440     #         LA    R9,EXTBFP    Point to start of extended BFP input values
#     Top of loop; clear residuals from FPR0-1
r 61C=B3750000     #SQRTLOOP LZDR  R0           Zero FPR0
r 620=B3750010     #         LZDR  R1           Zero FPR1
#     BFP Short square root RXE and RRE
r 624=ED0070000014 #         SQEB  R0,0(,R7)    Put square root of test case in FPR0
r 62A=70003000     #         STE   R0,0(,R3)    Store short BFP result from RXE
r 62E=78107000     #         LE    R1,0(,R7)    Get test case for RRE instruction
r 632=B3140001     #         SQEBR R0,R1        Add BFP values, result in FPR0
r 636=70003004     #         STE   R0,4(,R3)    Store short BFP from RRE
r 63A=41707004     #         LA    R7,4(,R7)    Point to next short BFP test case
r 63E=41303008     #         LA    R3,8(,R3)    Point to next short BFP result pair
#     BFP Long square root RXE and RRE
r 642=ED0080000015 #         SQDB  R0,0(,R8)    Square root BFP long test case
r 648=60004000     #         STD   R0,0(,R4)    Store long BFP result from RXE
r 64C=47000000     #         NOP   ,            Not used for testing
r 650=68108000     #         LD    R1,0(,R8)    Get BFP long test case
r 654=B3150001     #         SQDBR R0,R1        Take square root, result in FPR0
r 658=60004008     #         STD   R0,8(,R4)    Store long BFP from RRE
r 65C=41808008     #         LA    R8,8(,R8)    Point to next long BFP test case
r 660=41404010     #         LA    R4,16(,R4)   Point to next long BFP result pair
#     BFP Extended square root RRE
r 664=68109000     #         LD    R1,0(,R9)    Get BFP ext. 1st half of test case
r 668=68309008     #         LD    R3,8(,R9)    Get BFP ext. 2nd half of test case
r 66C=B3160001     #         SQXBR R0,R1        Add BFP values, result in FPR0-FPR2
r 670=60005000     #         STD   R0,0(,R5)    Store ext. BFP from RRE
r 674=60205008     #         STD   R2,8(,R5)    Store ext. BFP from RRE
r 678=41909010     #         LA    R9,16(,R9)   Point to next ext. BFP input 
r 67C=41505010     #         LA    R5,16(,R5)   Point to next ext. BFP result
r 680=4620061C     #         BCT   R2,SQRTLOOP   Square root next input pair
r 684=07FC         #         BR    R12          Return to caller

#
r 3EC=00040000     # CTLR0             Control register 0 (bit45 AFP control)
r 3F0=00020000000000000000000000000000 # WAITPSW Disabled wait state PSW - normal completion
#                    INTVALS  DS    0F   Four fullwords
r 480=0000000000000001     # 1 first test integer
r 488=0000000000000002     # 2 second test integer
r 490=0000000000000004     # 4 third test integer
r 498=0000000000000010     # 16 fourth test integer
# 410.10             SHORTBFP DS    4F   four test cases in short bfp
# 420.20             LONGBFP  DS    4D   four test cases in long bfp
# 440.40             EXTBFP   DS    8D   four test cases in extended bfp
# 500.20             Short square root results (4 pairs, RXE & RRE)
# 520.40             Long square root results (4 pairs, RXE & RRE)
# 560.40             Extended square root results (4, RRE only)
# A00.40             Convert short BFP to Integer results
# A40.40             Convert long BFP to Integer results
# A80.20             Convert extended BFP to Integer results


# Convert Integer test cases to each format of BFP.  Half will be converted as
# 32-bit unsigned integers, and the rest as 64-bit unsigned integers.  

# 800               CVTINPUT DS    0H
r 800=41200002     #         LA    R2,2         Set count of test input values
r 804=41300480     #         LA    R3,INTVALS   point to start of input values
r 808=41700410     #         LA    R7,SHORTBFP  Point to start of short BFP input values
r 80C=41800420     #         LA    R8,LONGBFP   Point to start of long BFP input values
r 810=41900440     #         LA    R9,EXTBFP    Point to start of extended BFP input values
# convert integers to three BFP formats.  32 bit first, then 64
r 814=58103004     #CVTLOOP  L     R1,4(,R3)    Get integer test value
r 818=B3940001     #         CEFBR R0,R1       Cvt Int in GPR1 to float in FPR0
r 81C=70007000     #         STE   R0,0(,R7)    Store short BFP
r 820=B3950001     #         CDFBR R0,R1       Cvt Int in GPR1 to float in FPR0
r 824=60008000     #         STD   R0,0(,R8)    Store long BFP
r 828=B3960001     #         CXFBR R0,R1       Cvt Int in GPR1 to float in FPR0-FPR2
r 82C=60009000     #         STD   R0,0(,R9)    Store extended BFP part 1
r 830=60209008     #         STD   R2,8(,R9)    Store extended BFP part 2
r 834=E31030080004 #         LG    R1,8(,R3)    Get integer test value
r 83A=B3A40001     #         CEGBR R0,R1       Cvt Int in GPR1 to float in FPR0
r 83E=70007004     #         STE   R0,4(,R7)    Store short BFP
r 842=B3A50001     #         CDGBR R0,R1       Cvt Int in GPR1 to float in FPR0
r 846=60008008     #         STD   R0,8(,R8)    Store long BFP
r 84A=B3A60001     #         CXGBR R0,R1       Cvt Int in GPR1 to float in FPR0-FPR2
r 84E=60009010     #         STD   R0,16(,R9)   Store extended BFP part 1
r 852=60209008     #         STD   R2,24(,R9)   Store extended BFP part 2
r 856=41303010     #         LA    R3,16(,R3)   point to next input value pair
r 85A=41707008     #         LA    R7,8(,R7)    Point to next pair of short BFP converted values
r 85E=41808010     #         LA    R8,16(,R8)   Point to next pair of long BFP converted values
r 862=41909020     #         LA    R9,32(,R9)   Point to next pair of extended BFP converted values
r 866=46200814     #         BCT   R2,CVTLOOP   Convert next input value pair.  
r 86A=07FC         #         BR    R12          All converted, return to main line.  

# Convert back to integers

# 900               CVTOUTS  DS    0H
r 900=41200002     #         LA    R2,2         Set count of square root result pairs
r 904=41700500     #         LA    R7,SHORTRES  Point to start of short BFP results
r 908=41800520     #         LA    R8,LONGRES   Point to start of long BFP results
r 90C=41900560     #         LA    R9,EXTRES    Point to start of extended BFP results
r 910=41300A00     #         LA    R3,ISHBFP    Point to start of short BFP Integer results
r 914=41400A40     #         LA    R4,ILNBFP    Point to start of long BFP integer results
r 918=41500A80     #         LA    R5,IXTBFP    Point to start of extended BFP integer results
#
r 91C=B3750000     #INTLOOP  LZDR  R0           Zero FPR0
#     Convert shorts back to integer
r 920=78007000     #         LE    R0,0(,R7)    Get BFP short result RXE
r 924=B3980000     #         CFEBR R0,R0       Convert to integer in r0
r 928=50003004     #         ST    R0,4(,R3)    Store integer result
r 92C=78007004     #         LE    R0,4(,R7)    Get BFP short result RRE
r 930=B3980000     #         CFEBR R0,R0       Convert to integer in r0
r 934=5000300C     #         ST    R0,12(,R3)   Store integer result
#
r 938=78007008     #         LE    R0,8(,R7)    Get BFP short result RXE
r 93C=B3A80000     #         CGEBR R0,R0       Convert to integer 64 in r0
r 940=E30030100024 #         STG   R0,16(,R3)   Store integer result
r 946=7800700C     #         LE    R0,12(,R7)   Get BFP short result RRE
r 94A=B3A80000     #         CGEBR R0,R0       Convert to integer 64 in r0
r 94E=E30030180024 #         STG   R0,24(,R3)   Store integer result
#
r 954=68008000     #         LD    R0,0(,R8)    Get BFP long result RXE
r 958=B3990000     #         CFDBR R0,R0       Convert to integer in r0
r 95C=50004004     #         ST    R0,4(,R4)    Store integer result
r 960=68008008     #         LD    R0,8(,R8)    Get BFP long result RRE
r 964=B3990000     #         CFDBR R0,R0       Convert to integer in r0
r 968=5000400C     #         ST    R0,12(,R4)   Store integer result
#
r 96C=68008010     #         LD    R0,16(,R8)   Get BFP long result RXE
r 970=B3A90000     #         CGDBR R0,R0       Convert to integer 64 in r0
r 974=E30040100024 #         STG   R0,16(,R4)   Store integer result
r 97A=68008018     #         LD    R0,24(,R8)   Get BFP long result RRE
r 97E=B3A90000     #         CGDBR R0,R0       Convert to integer 64 in r0
r 982=E30040180024 #         STG   R0,24(,R4)   Store integer result
#
r 988=68009000     #         LD    R0,0(,R9)    Get BFP ext. 1st half
r 98C=68209008     #         LD    R2,8(,R9)    Get BFP ext. 2nd half
r 990=B39A0000     #         CFXBR R0,R0       Convert BFP Ext. to Integer-32
r 994=50005004     #         ST    R0,4(,R5)    Store integer result lower word
#
r 998=68009010     #         LD    R0,16(,R9)   Get BFP ext. 1st half
r 99C=68209018     #         LD    R2,24(,R9)   Get BFP ext. 2nd half
r 9A0=B3AA0000     #         CGXBR R0,R0       Convert BFP Ext. to Integer-64
r 9A4=E30050080024 #         STG   R0,8(,R5)    Store integer result 
#
r 9AA=41707010     #         LA    R7,16(,R7)   Point to next short BFP result pair
r 9AE=41303020     #         LA    R3,32(,R3)   Point to next short BFP integer pair
r 9B2=41808020     #         LA    R8,32(,R8)   Point to next long BFP result pair
r 9B6=41404020     #         LA    R4,32(,R4)   Point to next long BFP integer pair
r 9BA=41909020     #         LA    R9,32(,R9)   Point to next ext. BFP result 
r 9BE=41505010     #         LA    R5,16(,R5)   Point to next ext. BFP integer
#
r 9C2=4620091C     #         BCT   R2,INTLOOP   Convert next pair of BFP values
r 9C6=07FC         #         BR    R12          Return to caller

runtest 

*Compare
r 410.10  # Inputs converted to BFP short 
*Want "Int->Short BFP" 3F800000 40000000 40800000 41800000

*Compare
r 420.10  # Inputs converted to BFP long one of two
*Want "Int->Long BFP 1 of 2" 3FF00000 00000000 40000000 00000000

*Compare
r 430.10  # Inputs converted to BFP long two of two
*Want "Int->Long BFP 2 of 2" 40100000 00000000 40300000 00000000

*Compare
r 440.10  # Inputs converted to BFP ext one of four 
*Want "Int->Extended BFP 1 of 4" 3FFF0000 00000000 00000000 00000000

*Compare
r 450.10  # Inputs converted to BFP ext two of four 
*Want "Int->Extended BFP 2 of 4" 40000000 00000000 00000000 00000000

*Compare
r 460.10  # Inputs converted to BFP ext three of four 
*Want "Int->Extended BFP 3 of 4" 40010000 00000000 00000000 00000000

*Compare
r 470.10  # Inputs converted to BFP ext four of four 
*Want "Int->Extended BFP 4 of 4" 40030000 00000000 00000000 00000000

*Compare
r 500.10  # BFP short square roots RXE and RRE part 1
*Want "Short BFP Roots 1/2" 3F800000 3F800000 3FB504F3 3FB504F3

*Compare
r 510.10  # BFP short square roots RXE and RRE part 2
*Want "Short BFP Roots 2/2" 40000000 40000000 40800000 40800000

*Compare
r 520.10  # BFP long square roots RXE and RRE part 1
*Want "Long BFP Roots 1/4" 3FF00000 00000000 3FF00000 00000000

*Compare
r 530.10  # BFP long square roots RXE and RRE part 2
*Want "Long BFP Roots 2/4" 3FF6A09E 667F3BCD 3FF6A09E 667F3BCD

*Compare
r 540.10  # BFP long square roots RXE and RRE part 3
*Want "Long BFP Roots 3/4" 40000000 00000000 40000000 00000000

*Compare
r 550.10  # BFP long square roots RXE and RRE part 4
*Want "Long BFP Roots 4/4" 40100000 00000000 40100000 00000000

*Compare
r 560.10  # BFP extended square roots RRE part 1
*Want "Ext BFP Roots 1/4" 3FFF0000 00000000 00000000 00000000

*Compare
r 570.10  # BFP extended square roots RRE part 2
*Want "Ext BFP Roots 2/4" 3FFF6A09 E667F3BC C908B2FB 1366EA95

*Compare
r 580.10  # BFP extended square roots RRE part 3
*Want "Ext BFP Roots 3/4" 40000000 00000000 00000000 00000000

*Compare
r 590.10  # BFP extended square roots RRE part 4
*Want "Ext BFP Roots 4/4" 40010000 00000000 00000000 00000000

*Compare
r A00.10   # Short BFP to Integer results part 1
*Want "Short BFP Integer Roots 1/4" 00000000 00000001 00000000 00000001

*Compare
r A10.10   # Short BFP to Integer results part 2
*Want "Short BFP Integer Roots 2/4" 00000000 00000001 00000000 00000001

*Compare
r A20.10   # Short BFP to Integer results part 3
*Want "Short BFP Integer Roots 3/4" 00000000 00000002 00000000 00000002

*Compare
r A30.10   # Short BFP to Integer results part 4
*Want "Short BFP Integer Roots 4/4" 00000000 00000004 00000000 00000004

*Compare
r A40.10   # Convert long BFP to Integer results part 1
*Want "Long BFP Integer Roots 1/4" 00000000 00000001 00000000 00000001

*Compare
r A50.10   # Convert long BFP to Integer results part 2
*Want "Long BFP Integer Roots 2/4" 00000000 00000001 00000000 00000001

*Compare
r A60.10   # Convert long BFP to Integer results part 3
*Want "Long BFP Integer Roots 3/4" 00000000 00000002 00000000 00000002

*Compare
r A70.10   # Convert long BFP to Integer results part 4
*Want "Long BFP Integer Roots 4/4" 00000000 00000004 00000000 00000004

*Compare
r A80.10    # Convert extended BFP to Integer results part 1
*Want "Extended BFP Integer Roots 1/2"  00000000 00000001 00000000 00000001

*Compare
r A90.10    # Convert extended BFP to Integer results part 2
*Want "Extended BFP Integer Roots 2/2"  00000000 00000002 00000000 00000004

# Following useful for script testing; not required for instruction validation
# *Compare
# r 150.10    # No program checks
# *Want "No ProgChk"  00000000 00000000 00000000 00000000
# Comment this back out before you commit.

*Done


*
*Testcase IEEE CONVERT FROM/TO FIXED 64 (6), SUBTRACT (5), 11 instr total
* SUBTRACT tests - Binary Floating Point
*
#
# Tests five addition instructions:
#   SUBTRACT (extended BFP, RRE) 
#   SUBTRACT (long BFP, RRE) 
#   SUBTRACT (long BFP, RXE) 
#   SUBTRACT (short BFP, RRE) 
#   SUBTRACT (short BFP, RXE) 
#
# Also tests the following three conversion instructions
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
#  Convert integers 2, 1, 4, and -2 to each BFP floating point format
#  Subtract the second from the first (2 - 1, 4 - -2) in each format
#  Convert the results back to integers (none should round)
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
r 210=45C00600     #         BAL   R12,TESTSUB  Perform subtractions
r 214=45C00900     #         BAL   R12,CVTOUTS  Convert results to integers
r 218=B2B203F0     #         LPSWE WAITPSW      All done, load enabled wait PSW

#
# Subtract pairs of values from each other
#                   TESTSUB  DS    0H                
r 600=41200002     #         LA    R2,2         Set count of subtraction operations
r 604=41300500     #         LA    R3,SHORTRES  Point to start of short BFP results
r 608=41400510     #         LA    R4,LONGRES   Point to start of long BFP results
r 60C=41500530     #         LA    R5,EXTRES    Point to start of extended BFP results
r 610=41700410     #         LA    R7,SHORTBFP  Point to start of short BFP input values
r 614=41800420     #         LA    R8,LONGBFP   Point to start of long BFP input values
r 618=41900440     #         LA    R9,EXTBFP    Point to start of extended BFP input values
#     Top of loop; clear residuals from FPR0-1
r 61C=B3750000     #SUBLOOP  LZDR  R0           Zero FPR0
r 620=B3750010     #         LZDR  R1           Zero FPR1
#     BFP Short subtraction RXE and RRE
r 624=78007000     #         LE    R0,0(,R7)    Get BFP short first of pair
r 628=ED007004000B #         SEB   R0,4(,R7)    Subtract BFP short second of pair
r 62E=70003000     #         STE   R0,0(,R3)    Store short BFP result from RXE
r 632=78007000     #         LE    R0,0(,R7)    Get BFP short first of pair
r 636=78107004     #         LE    R1,4(,R7)    Get BFP short second of pair
r 63A=B30B0001     #         SEBR  R0,R1        Subtract BFP values, result in FPR0
r 63E=70003004     #         STE   R0,4(,R3)    Store short BFP from RRE
r 642=41707008     #         LA    R7,8(,R7)    Point to next short BFP input pair
r 646=41303008     #         LA    R3,8(,R3)    Point to next short BFP result pair
#     BFP Long subtraction RXE and RRE
r 64A=68008000     #         LD    R0,0(,R8)    Get BFP long first of pair
r 64E=ED008008001B #         SDB   R0,8(,R8)    Subtract BFP long second of pair
r 654=60004000     #         STD   R0,0(,R4)    Store long BFP result from RXE
r 658=68008000     #         LD    R0,0(,R8)    Get BFP long first of pair
r 65C=68108008     #         LD    R1,8(,R8)    Get BFP long second of pair
r 660=B31B0001     #         SDBR  R0,R1        Subtract BFP values, result in FPR0
r 664=60004008     #         STD   R0,8(,R4)    Store long BFP from RRE
r 668=41808010     #         LA    R8,16(,R8)   Point to next long BFP input pair
r 66C=41404010     #         LA    R4,16(,R4)   Point to next long BFP result pair
#     BFP Extended subtraction RRE
r 670=68009000     #         LD    R0,0(,R9)    Get BFP ext. 1st half of first of pair
r 674=68209008     #         LD    R2,8(,R9)    Get BFP ext. 2nd half of first of pair
r 678=68109010     #         LD    R1,16(,R9)   Get BFP ext. 1st half of second of pair
r 67C=68309018     #         LD    R3,24(,R9)   Get BFP ext. 2nd half of second of pair
r 680=B34B0001     #         SXBR  R0,R1        Subtract BFP values, result in FPR0-FPR2
r 684=60005000     #         STD   R0,0(,R5)    Store ext. BFP from RRE
r 688=60205008     #         STD   R2,8(,R5)    Store ext. BFP from RRE
r 68C=41909020     #         LA    R9,32(,R9)   Point to next ext. BFP input pair
r 690=41505010     #         LA    R5,16(,R5)   Point to next ext. BFP result
#
r 694=4620061C     #         BCT   R2,SUBLOOP   Subtract next input pair
r 698=07FC         #         BR    R12          Tests done, return to mainline


#
r 3EC=00040000     # CTLR0             Control register 0 (bit45 AFP control)
r 3F0=00020000000000000000000000000000 # WAITPSW Disabled wait state PSW - normal completion
r 480=0000000000000002     # 1 first test integer
r 488=0000000000000001     # 2 second test integer
r 490=0000000000000004     # 4 third test integer
r 498=FFFFFFFFFFFFFFFE     # -2 fourth test integer
# 410                first test in short bfp
# 414                second test in short bfp
# 418                third test in short bfp
# 41C                fourth test in short bfp
# 420                first test in long bfp
# 428                second test in long bfp
# 430                third test in long bfp
# 438                fourth test in long bfp
# 440                first test in extended bfp
# 450                second test in extended bfp
# 460                third test in extended bfp
# 470                fourth test in extended bfp
# 500.10             Short subtraction results (2 pairs, RXE & RRE)
# 510.20             Long subtraction results (2 pairs, RXE & RRE)
# 530.20             Extended subtraction results (1 pair, RRE only)
# 550.20             Convert short BFP to 64-bit Integer results
# 570.20             Convert long BFP to 64-bit Integer results
# 590.10             Convert extended BFP to 64-bit Integer results

#
# Subroutine: convert 64-bit integers into BFP in all precisions
#
#                   CVTINPUT DS    0H           Input conversion routine
r 800=41200004     #         LA    R2,4         Set count of 64-bit test input values
r 804=47000000
r 808=41300480     #         LA    R3,INTVALS   point to start of input values
r 80C=41700410     #         LA    R7,SHORTBFP  Point to start of short BFP input values
r 810=41800420     #         LA    R8,LONGBFP   Point to start of long BFP input values
r 814=41900440     #         LA    R9,EXTBFP    Point to start of extended BFP input values
# convert integers to three BFP formats
r 818=E31030000004 #CVTLOOP  LG    R1,0(,R3)    Get integer test value
r 81E=B3A40001     #         CEGBR R0,R1        Cvt Int in GPR1 to short float in FPR0
r 822=70007000     #         STE   R0,0(,R7)    Store short BFP
r 826=B3A50001     #         CDGBR R0,R1        Cvt Int in GPR1 to long float in FPR0
r 82A=60008000     #         STD   R0,0(,R8)    Store long BFP
r 82E=B3A60001     #         CXGBR R0,R1        Cvt Int in GPR1 to ext float in FPR0-FPR2
r 832=60009000     #         STD   R0,0(,R9)    Store extended BFP part 1
r 836=60209008     #         STD   R2,8(,R9)    Store extended BFP part 2
r 83A=41303008     #         LA    R3,8(,R3)    point to next input values
r 83E=41707004     #         LA    R7,4(,R7)    Point to next short BFP converted values
r 842=41808008     #         LA    R8,8(,R8)    Point to next long BFP converted values
r 846=41909010     #         LA    R9,16(,R9)   Point to next extended BFP converted values
r 84A=46200818     #         BCT   R2,CVTLOOP   Convert next input value.  
r 84E=07FC         #         BR    R12          Conversion done, return to mainline

#
# Convert back to integers
#
#                   CVTOUTS  DS    0H
r 900=41200002     #         LA    R2,2         Set count of subtraction operations
r 904=41700500     #         LA    R7,SHORTRES  Point to start of short BFP results
r 908=41800510     #         LA    R8,LONGRES   Point to start of long BFP results
r 90C=41900530     #         LA    R9,EXTRES    Point to start of extended BFP results
r 910=41300550     #         LA    R3,ISHBFP    Point to start of short BFP Integer results
r 914=41400570     #         LA    R4,ILNBFP    Point to start of long BFP integer results
r 918=41500590     #         LA    R5,IXTBFP    Point to start of extended BFP integer results
#
r 91C=B3750000     #INTLOOP  LZDR  R0           Zero FPR0
#     Convert shorts back to integer
r 920=78007000     #         LE    R0,0(,R7)    Get BFP short result first of pair 
r 924=B3A80000     #         CGEBR R0,R0        Convert to integer in r0
r 928=E30030000024 #         STG   R0,0(,R3)    Store integer result
r 92E=78007004     #         LE    R0,4(,R7)    Get BFP short result second of pair 

r 932=B3A80000     #         CGEBR R0,R0        Convert to integer in r0
r 936=E30030080024 #         STG   R0,8(,R3)    Store integer result
#
r 93C=68008000     #         LD    R0,0(,R8)    Get BFP long result first of pair 
r 940=B3A90000     #         CGDBR R0,R0        Convert to integer in r0
r 944=E30040000024 #         STG   R0,0(,R4)    Store integer result
r 94A=68008008     #         LD    R0,8(,R8)    Get BFP long result second of pair 
r 94E=B3A90000     #         CGDBR R0,R0        Convert to integer in r0
r 952=E30040080024 #         STG   R0,8(,R4)    Store integer result
#
r 958=68009000     #         LD    R0,0(,R9)    Get BFP ext. 1st half of first of pair
r 95C=68209008     #         LD    R2,8(,R9)    Get BFP ext. 2nd half of first of pair
r 960=B3AA0000     #         CGXBR R0,R0        Convert BFP Ext. to Integer-64
r 964=E30050000024 #         STG   R0,0(,R5)    Store integer result lower word
#
r 96A=41707008     #         LA    R7,8(,R7)    Point to next short BFP input pair
r 96E=41303010     #         LA    R3,16(,R3)   Point to next short BFP integer pair
r 972=41808010     #         LA    R8,16(,R8)   Point to next long BFP input pair
r 976=41404010     #         LA    R4,16(,R4)   Point to next long BFP integer pair
r 97A=41909010     #         LA    R9,16(,R9)   Point to next ext. BFP input
r 97E=41505008     #         LA    R5,8(,R5)    Point to next ext. BFP integer
r 982=4620091C     #         BCT   R2,INTLOOP   Subtract next input pair
r 986=07FC         #         BR    R12          Conversion done, return to mainline

runtest 

*Compare
r 410.10  # Inputs converted to BFP short 
*Want "Int->Short BFP" 40000000 3F800000 40800000 C0000000

*Compare
r 420.10  # Inputs converted to BFP long part 1
*Want "Int->Long BFP 1/2"  40000000 00000000 3FF00000 00000000

*Compare
r 430.10  # Inputs converted to BFP long part 2
*Want "Int->Long BFP 2/2" 40100000 00000000 C0000000 00000000

*Compare
r 440.10  # Inputs converted to BFP ext part 1
*Want "Int->Extended BFP 1/4" 40000000 00000000 00000000 00000000

*Compare
r 450.10  # Inputs converted to BFP ext part 2
*Want "Int->Extended BFP 2/4" 3FFF0000 00000000 00000000 00000000

*Compare
r 460.10  # Inputs converted to BFP ext part 3
*Want "Int->Extended BFP 3/4" 40010000 00000000 00000000 00000000

*Compare
r 470.10  # Inputs converted to BFP ext part 4
*Want "Int->Extended BFP 4/4" C0000000 00000000 00000000 00000000

*Compare
r 500.10  # BFP short differences RXE and RRE 
*Want "Short BFP Sums" 3F800000 3F800000 40C00000 40C00000 

*Compare
r 510.10  # BFP long differences RXE and RRE part 1
*Want "Long BFP Sums 1/2" 3FF00000 00000000 3FF00000 00000000

*Compare
r 520.10  # BFP long differences RXE and RRE part 2
*Want "Long BFP Sums 2/2" 40180000 00000000 40180000 00000000

*Compare
r 530.10  # BFP extended differences RRE part 1
*Want "Ext BFP Sums 1/2" 3FFF0000 00000000 00000000 00000000

*Compare
r 540.10  # BFP extended differences RRE part 2
*Want "Ext BFP Sums 2/2" 40018000 00000000 00000000 00000000

*Compare
r 550.10   # Short BFP to Integer results part 1
*Want "Short BFP Integer Results 1/2"  00000000 00000001 00000000 00000001

*Compare
r 560.10   # Short BFP to Integer results part 2
*Want "Short BFP Integer Results 2/2"  00000000 00000006 00000000 00000006

*Compare
r 570.10   # Convert long BFP to Integer results part 1
*Want "Long BFP Integer Results 1/2"  00000000 00000001 00000000 00000001 


*Compare
r 580.10   # Convert long BFP to Integer results part 2
*Want "Long BFP Integer Results 2/2" 00000000 00000006 00000000 00000006

*Compare
r 590.10    # Convert extended BFP to Integer results
*Want "Extended BFP Integer Results"  00000000 00000001 00000000 00000006

# Following useful for script testing; not required for instruction validation
# *Compare
# r 150.10    # No program checks
# *Want "No ProgChk"  00000000 00000000 00000000 00000000
# Comment this back out before you commit.

*Done


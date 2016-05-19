*
*Testcase Binary Floating Point CONVERT FROM/TO FIXED, ADD, 11 instructions
* ADD tests - Binary Floating Point
*
#
# Tests five addition instructions:
#   ADD (extended BFP, RRE) 
#   ADD (long BFP, RRE) 
#   ADD (long BFP, RXE) 
#   ADD (short BFP, RRE) 
#   ADD (short BFP, RXE) 
#
# Also tests the following three conversion instructions
#   CONVERT FROM FIXED (32 to short BFP, RRE)
#   CONVERT FROM FIXED (32 to long BFP, RRE) 
#   CONVERT FROM FIXED (32 to extended BFP, RRE)  
#   CONVERT TO FIXED (32 to short BFP, RRE)
#   CONVERT TO FIXED (32 to long BFP, RRE) 
#   CONVERT TO FIXED (32 to extended BFP, RRE)  
#
# Also tests the following floating point support instructions
#   LOAD  (Short)
#   LOAD  (Long)
#   LOAD ZERO (Long)
#   STORE (Short)
#   STORE (Long)
#
#  Convert integers 1, 2, 4, -2 to each BFP floating point format
#  Add first and second pairs together (1 + 2, 4 + -2) in each format
#
sysclear
archmode esame
r 1a0=00000001800000000000000000000200 # z/Arch restart PSW
r 1d0=0002000000000000000000000000DEAD # z/Arch pgm chk new PSW disabled wait
r 200=B70003EC     #         LCTL  R0,R0,CTLR0  Set CR0 bit 45 - AFP Control
r 204=41200004     #         LA    R2,4         Set count of test input values
r 208=41300400     #         LA    R3,INTVALS   point to start of input values
r 20C=41700410     #         LA    R7,SHORTBFP  Point to start of short BFP input values
r 210=41800420     #         LA    R8,LONGBFP   Point to start of long BFP input values
r 214=41900440     #         LA    R9,EXTBFP    Point to start of extended BFP input values
# convert integers to three BFP formats
r 218=58103000     #CVTLOOP  L     R1,0(,R3)    Get integer test value
r 21c=B3940001     #         CEFBR R0,R1        Cvt Int in GPR1 to float in FPR0
r 220=70007000     #         STE   R0,0(,R7)    Store short BFP
r 224=B3950001     #         CDFBR R0,R1        Cvt Int in GPR1 to float in FPR0
r 228=60008000     #         STD   R0,0(,R8)    Store long BFP
r 22C=B3960001     #         CXFBR R0,R1        Cvt Int in GPR1 to float in FPR0-FPR2
r 230=60009000     #         STD   R0,0(,R9)    Store extended BFP part 1
r 234=60209008     #         STD   R2,8(,R9)    Store extended BFP part 2
r 238=41303004     #         LA    R3,4(,R3)    point to next input values
r 23C=41707004     #         LA    R7,4(,R7)    Point to next short BFP converted values
r 240=41808008     #         LA    R8,8(,R8)    Point to next long BFP converted values
r 244=41909010     #         LA    R9,16(,R9)   Point to next extended BFP converted values
r 248=46200218     #         BCT   R2,CVTLOOP   Convert next input value.  
# Add pairs of values together
r 24C=41200002     #         LA    R2,2         Set count of addition operations
r 250=41300500     #         LA    R3,SHORTRES  Point to start of short BFP results
r 254=41400510     #         LA    R4,LONGRES   Point to start of long BFP results
r 258=41500530     #         LA    R5,EXTRES    Point to start of extended BFP results
r 25C=41700410     #         LA    R7,SHORTBFP  Point to start of short BFP input values
r 260=41800420     #         LA    R8,LONGBFP   Point to start of long BFP input values
r 264=41900440     #         LA    R9,EXTBFP    Point to start of extended BFP input values
#     Top of loop; clear residuals from FPR0-1
r 268=B3750000     #ADDLOOP  LZDR  R0           Zero FPR0
r 26C=B3750010     #         LZDR  R1           Zero FPR1
#     BFP Short addition RXE and RRE
r 270=78007000     #         LE    R0,0(,R7)    Get BFP short first of pair
r 274=ED007004000A #         AEB   R0,4(,R7)    Add BFP short second of pair
r 27A=70003000     #         STE   R0,0(,R3)    Store short BFP result from RXE
r 27E=78007000     #         LE    R0,0(,R7)    Get BFP short first of pair
r 282=78107004     #         LE    R1,4(,R7)    Get BFP short second of pair
r 286=B30A0001     #         AEBR  R0,R1        Add BFP values, result in FPR0
r 28A=70003004     #         STE   R0,4(,R3)    Store short BFP from RRE
r 28E=41707008     #         LA    R7,8(,R7)    Point to next short BFP input pair
r 292=41303008     #         LA    R3,8(,R3)    Point to next short BFP result pair
#     BFP Long addition RXE and RRE
r 296=68008000     #         LD    R0,0(,R8)    Get BFP long first of pair
r 29A=ED008008001A #         ADB   R0,8(,R8)    Add BFP long second of pair
r 2A0=60004000     #         STD   R0,0(,R4)    Store long BFP result from RXE
r 2A4=68008000     #         LD    R0,0(,R8)    Get BFP long first of pair
r 2A8=68108008     #         LD    R1,8(,R8)    Get BFP long second of pair
r 2AC=B31A0001     #         ADBR  R0,R1        Add BFP values, result in FPR0
r 2B0=60004008     #         STD   R0,8(,R4)    Store long BFP from RRE
r 2B4=41808010     #         LA    R8,16(,R8)   Point to next long BFP input pair
r 2B8=41404010     #         LA    R4,16(,R4)   Point to next long BFP result pair
#     BFP Extended addition RRE
r 2BC=68009000     #         LD    R0,0(,R9)    Get BFP ext. 1st half of first of pair
r 2C0=68209008     #         LD    R2,8(,R9)    Get BFP ext. 2nd half of first of pair
r 2C4=68109010     #         LD    R1,16(,R9)   Get BFP ext. 1st half of second of pair
r 2C8=68309018     #         LD    R3,24(,R9)   Get BFP ext. 2nd half of second of pair
r 2CC=B34A0001     #         AXBR  R0,R1        Add BFP values, result in FPR0-FPR2
r 2D0=60005000     #         STD   R0,0(,R5)    Store ext. BFP from RRE
r 2D4=60205008     #         STD   R2,8(,R5)    Store ext. BFP from RRE
r 2D8=41909020     #         LA    R9,32(,R9)   Point to next ext. BFP input pair
r 2DC=41505010     #         LA    R5,16(,R5)   Point to next ext. BFP result
#
r 2E0=46200268     #         BCT   R2,ADDLOOP   Add next input pair
#
# Convert back to integers
#
r 2E4=41200002     #         LA    R2,2         Set count of addition operations
r 2E8=41700500     #         LA    R7,SHORTRES  Point to start of short BFP results
r 2EC=41800510     #         LA    R8,LONGRES   Point to start of long BFP results
r 2F0=41900530     #         LA    R9,EXTRES    Point to start of extended BFP results
r 2F4=41300550     #         LA    R3,ISHBFP    Point to start of short BFP Integer results
r 2F8=41400560     #         LA    R4,ILNBFP    Point to start of long BFP integer results
r 2FC=41500570     #         LA    R5,IXTBFP    Point to start of extended BFP integer results
#
r 300=B3750000     #INTLOOP  LZDR  R0           Zero FPR0
#     Convert shorts back to integer
r 304=78007000     #         LE    R0,0(,R7)    Get BFP short result first of pair 
r 308=B3980000     #         CFEBR R0,R0        Convert to integer in r0
r 30C=50003000     #         ST    R0,0(,R3)    Store integer result
r 310=78007004     #         LE    R0,4(,R7)    Get BFP short result second of pair 
r 314=B3980000     #         CFEBR R0,R0        Convert to integer in r0
r 318=50003004     #         ST    R0,4(,R3)    Store integer result
#
r 31C=68008000     #         LD    R0,0(,R8)    Get BFP long result first of pair 
r 320=B3990000     #         CFDBR R0,R0        Convert to integer in r0
r 324=50004000     #         ST    R0,0(,R4)    Store integer result
r 328=68008008     #         LD    R0,8(,R8)    Get BFP long result second of pair 
r 32C=B3990000     #         CFDBR R0,R0        Convert to integer in r0
r 330=50004004     #         ST    R0,4(,R4)    Store integer result
#
r 334=68009000     #         LD    R0,0(,R9)    Get BFP ext. 1st half of first of pair
r 338=68209008     #         LD    R2,8(,R9)    Get BFP ext. 2nd half of first of pair
r 33C=B39A0000     #         CFXBR R0,R0        Convert BFP Ext. to Integer-64
r 340=50005000     #         ST    R0,0(,R5)    Store integer result lower word
#
r 344=41707008     #         LA    R7,8(,R7)    Point to next short BFP input pair
r 348=41303008     #         LA    R3,8(,R3)    Point to next short BFP integer pair
r 34C=41808010     #         LA    R8,16(,R8)   Point to next long BFP input pair
r 350=41404008     #         LA    R4,8(,R4)    Point to next long BFP integer pair
r 354=41909010     #         LA    R9,16(,R9)   Point to next ext. BFP input
r 358=41505004     #         LA    R5,4(,R5)    Point to next ext. BFP integer
#
r 35C=46200300     #         BCT   R2,INTLOOP   Add next input pair
#
r 360=B2B203F0     # LPSWE WAITPSW     Load enabled wait PSW
#
r 3EC=00040000     # CTLR0             Control register 0 (bit45 AFP control)
r 3F0=00020000000000000000000000000000 # WAITPSW Disabled wait state PSW - normal completion
r 400=00000001     # 1 first test integer
r 404=00000002     # 2 second test integer
r 408=00000004     # 4 third test integer
r 40C=FFFFFFFE     # -2 fourth test integer
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
# 500.10             Short add results (2 pairs, RXE & RRE)
# 510.20             Long add results (2 pairs, RXE & RRE)
# 530.20             Extended add results (1 pair, RRE only)
# 550.10             Convert short BFP to Integer results
# 560.10             Convert long BFP to Integer results
# 570.8              Convert extended BFP to Integer results

runtest 

*Compare
r 410.10  # Inputs converted to BFP short 
*Want "Int->Short BFP" 3F800000 40000000 40800000 C0000000

*Compare
r 420.10  # Inputs converted to BFP long one of two
*Want "Int->Long BFP 1 of 2" 3FF00000 00000000 40000000 00000000

*Compare
r 430.10  # Inputs converted to BFP long two of two
*Want "Int->Long BFP 2 of 2" 40100000 00000000 C0000000 00000000

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
*Want "Int->Extended BFP 4 of 4" C0000000 00000000 00000000 00000000

*Compare
r 500.10  # BFP short sums RXE and RRE 
*Want "Short BFP Sums" 40400000 40400000 40000000 40000000

*Compare
r 510.10  # BFP long sums RXE and RRE part 1
*Want "Long BFP Sums 1 of 2" 40080000 00000000 40080000 00000000

*Compare
r 520.10  # BFP long sums RXE and RRE part 2
*Want "Long BFP Sums 2 of 2" 40000000 00000000 40000000 00000000

*Compare
r 530.10  # BFP extended sum RRE one of two
*Want "Ext BFP Sums 1 of 2" 40008000 00000000 00000000 00000000

*Compare
r 540.10  # BFP extended sum RRE two of two
*Want "Ext BFP Sums 2 of 2" 40000000 00000000 00000000 00000000

*Compare
r 550.10   # Short BFP to Integer results
*Want "Short BFP Integer Results"  00000003 00000003 00000002 00000002

*Compare
r 560.10   # Convert long BFP to Integer results
*Want "Long BFP Integer Results"  00000003 00000003 00000002 00000002

*Compare
r 570.8    # Convert extended BFP to Integer results
*Want "Extended BFP Integer Results"  00000003 00000002


*Done


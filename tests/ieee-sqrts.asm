*
* Tests five square root instructions:
*   SQUARE ROOT (extended BFP, RRE) 
*   SQUARE ROOT (long BFP, RRE) 
*   SQUARE ROOT (long BFP, RXE) 
*   SQUARE ROOT (short BFP, RRE) 
*   SQUARE ROOT (short BFP, RXE) 
*
* Also tests the following twelve conversion instructions
*   CONVERT FROM FIXED (32 to short BFP, RRE)
*   CONVERT FROM FIXED (32 to long BFP, RRE) 
*   CONVERT FROM FIXED (32 to extended BFP, RRE)  
*   CONVERT TO FIXED (32 to short BFP, RRE)
*   CONVERT TO FIXED (32 to long BFP, RRE) 
*   CONVERT TO FIXED (32 to extended BFP, RRE)  
*   CONVERT FROM FIXED (64 to short BFP, RRE)
*   CONVERT FROM FIXED (64 to long BFP, RRE) 
*   CONVERT FROM FIXED (64 to extended BFP, RRE)  
*   CONVERT TO FIXED (64 to short BFP, RRE)
*   CONVERT TO FIXED (64 to long BFP, RRE) 
*   CONVERT TO FIXED (64 to extended BFP, RRE)  
*
* Also tests the following floating point support instructions
*   LOAD  (Short)
*   LOAD  (Long)
*   LOAD ZERO (Long)
*   STORE (Short)
*   STORE (Long)
*
*  Convert integers 1, 2, 4, 16 to each BFP floating point format
*  Take the square root of each in each BFP format
*  Convert back to integers (Note: SQRT(2) will/should round)
*  Because all inputs must be positive, we'll use this rig to test 
*     logical (unsigned) conversions when that support is added to 
*     Hercules.   And we will test 32 and 64 bit logical conversions.  
*     The comments will still say "integers" though. 
*
BFPSQRTS START 0
R0       EQU   0
R1       EQU   1
R2       EQU   2
R3       EQU   3
R4       EQU   4
R5       EQU   5
R6       EQU   6
R7       EQU   7
R8       EQU   8
R9       EQU   9
R10      EQU   10
R11      EQU   11
R12      EQU   12
R13      EQU   13
R14      EQU   14
R15      EQU   15
         USING *,0
         ORG   BFPSQRTS+X'1A0' 
         DC    X'0000000180000000',AD(STARTNAN)       z/Arch restart PSW
         ORG   BFPSQRTS+X'1D0' 
         DC    X'0002000000000000000000000000DEAD' z/Arch pgm chk new PSW
         ORG   BFPSQRTS+X'200'
*
* Mainline program.
*
STARTNAN STCTL R0,R0,CTLR0    Store CR0 to enable AFP
         OI    CTLR0+1,X'04'  Turn on AFP bit
         LCTL  R0,R0,CTLR0    Reload updated CR0
         BAL   R12,CVTINPUT   Convert inputs to binary formats
         BAL   R12,TESTSQRT   Perform subtractions
         BAL   R12,CVTOUTS    Convert results to integers
         LPSWE WAITPSW        All done, load enabled wait PSW
         DS    0D            Ensure correct alignment for PSW
*
*         
WAITPSW  DC    X'00020000000000000000000000000000' Disabled wait state PSW - normal completion
CTLR0    DS    F             Control register 0 (bit45 AFP control)
*
         ORG   BFPSQRTS+X'480'
* asma does not support doubleword integers, so we'll just do F'0' then F'value'
INTVALS  DS    0D            Four doublewords
         DC    F'0'  
         DC    F'1'          first test integer
         DC    F'0'
         DC    F'2'          second test integer
         DC    F'0'
         DC    F'4'          third test integer
         DC    F'0'
         DC    F'16'         fourth test integer
*       
SHORTBFP EQU   BFPSQRTS+X'410'    4F  410.10  four test cases in short bfp
LONGBFP  EQU   BFPSQRTS+X'420'    4D  420.20  four test cases in long bfp
EXTBFP   EQU   BFPSQRTS+X'440'    8D  440.40  four test cases in extended bfp

SHORTRES EQU   BFPSQRTS+X'500'    8F  500.20 Short square root results (4 pairs, RXE & RRE)
LONGRES  EQU   BFPSQRTS+X'520'    8D  520.40 Long square root results (4 pairs, RXE & RRE)
EXTRES   EQU   BFPSQRTS+X'560'    8D  560.40 Extended square root results (4, RRE only)
*
ISHBFP   EQU   BFPSQRTS+X'A00'    8D  A00.40 Convert short BFP to Integer results
ILNBFP   EQU   BFPSQRTS+X'A40'    8D  A40.40 Convert long BFP to Integer results
IXTBFP   EQU   BFPSQRTS+X'A80'    8F  A80.20 Convert extended BFP to Integer results
         ORG   BFPSQRTS+X'800'
*
*
*
* Take square roots of the four test cases in each precision
*
TESTSQRT DS    0H                
* Calculate square roots of values 
         LA    R2,4          Set count of square root operations
         LA    R3,SHORTRES   Point to start of short BFP results
         LA    R4,LONGRES    Point to start of long BFP results
         LA    R5,EXTRES     Point to start of extended BFP results
         LA    R7,SHORTBFP   Point to start of short BFP input values
         LA    R8,LONGBFP    Point to start of long BFP input values
         LA    R9,EXTBFP     Point to start of extended BFP input values
         BASR  R13,0         Set top of loop
*     Top of loop; clear residuals from FPR0-1
SQRTLOOP LZDR  R0            Zero FPR0
         LZDR  R1            Zero FPR1
*     BFP Short square root RXE and RRE
         SQEB  R0,0(0,R7)    Put square root of test case in FPR0
         STE   R0,0(0,R3)    Store short BFP result from RXE
         LE    R1,0(0,R7)    Get test case for RRE instruction
         SQEBR R0,R1         Add BFP values, result in FPR0
         STE   R0,4(0,R3)    Store short BFP from RRE
         LA    R7,4(0,R7)    Point to next short BFP test case
         LA    R3,8(0,R3)    Point to next short BFP result pair
*     BFP Long square root RXE and RRE
         SQDB  R0,0(0,R8)    Square root BFP long test case
         STD   R0,0(0,R4)    Store long BFP result from RXE
         LD    R1,0(0,R8)    Get BFP long test case
         SQDBR R0,R1         Take square root, result in FPR0
         STD   R0,8(0,R4)    Store long BFP from RRE
         LA    R8,8(0,R8)    Point to next long BFP test case
         LA    R4,16(0,R4)   Point to next long BFP result pair
*     BFP Extended square root RRE
         LD    R1,0(0,R9)    Get BFP ext. 1st half of test case
         LD    R3,8(0,R9)    Get BFP ext. 2nd half of test case
         SQXBR R0,R1         Add BFP values, result in FPR0-FPR2
         STD   R0,0(0,R5)    Store ext. BFP from RRE
         STD   R2,8(0,R5)    Store ext. BFP from RRE
         LA    R9,16(0,R9)   Point to next ext. BFP input 
         LA    R5,16(0,R5)   Point to next ext. BFP result
         BCTR  R2,R13        Square root next input pair
         BR    R12           Return to caller
*
* Convert Integer test cases to each format of BFP.  Half will be converted as
* 32-bit unsigned integers, and the rest as 64-bit unsigned integers.  
*
CVTINPUT DS    0H
         LA    R2,2          Set count of test input values
         LA    R3,INTVALS    point to start of input values
         LA    R7,SHORTBFP   Point to start of short BFP input values
         LA    R8,LONGBFP    Point to start of long BFP input values
         LA    R9,EXTBFP     Point to start of extended BFP input values
         BASR  R13,0         Set top of loop
* convert integers to three BFP formats.  32 bit first, then 64
CVTLOOP  L     R1,4(0,R3)    Get integer test value
         CEFBR R0,R1         Cvt Int in GPR1 to float in FPR0
         STE   R0,0(0,R7)    Store short BFP
         CDFBR R0,R1         Cvt Int in GPR1 to float in FPR0
         STD   R0,0(0,R8)    Store long BFP
         CXFBR R0,R1         Cvt Int in GPR1 to float in FPR0-FPR2
         STD   R0,0(0,R9)    Store extended BFP part 1
         STD   R2,8(0,R9)    Store extended BFP part 2
         LG    R1,8(0,R3)    Get integer test value
         CEGBR R0,R1         Cvt Int in GPR1 to float in FPR0
         STE   R0,4(0,R7)    Store short BFP
         CDGBR R0,R1         Cvt Int in GPR1 to float in FPR0
         STD   R0,8(0,R8)    Store long BFP
         CXGBR R0,R1         Cvt Int in GPR1 to float in FPR0-FPR2
         STD   R0,16(0,R9)   Store extended BFP part 1
         STD   R2,24(0,R9)   Store extended BFP part 2
         LA    R3,16(0,R3)   point to next input value pair
         LA    R7,8(0,R7)    Point to next pair of short BFP converted values
         LA    R8,16(0,R8)   Point to next pair of long BFP converted values
         LA    R9,32(0,R9)   Point to next pair of extended BFP converted values
         BCTR  R2,R13        Convert next input value pair.  
         BR    R12           All converted, return to main line.  
*
* Convert back to integers
*
CVTOUTS  DS    0H
         LA    R2,2          Set count of square root result pairs
         LA    R7,SHORTRES   Point to start of short BFP results
         LA    R8,LONGRES    Point to start of long BFP results
         LA    R9,EXTRES     Point to start of extended BFP results
         LA    R3,ISHBFP     Point to start of short BFP Integer results
         LA    R4,ILNBFP     Point to start of long BFP integer results
         LA    R5,IXTBFP     Point to start of extended BFP integer results
         BASR  R13,0         Set top of loop
*
*     Convert shorts back to integer
*
INTLOOP  LZDR  R0            Zero FPR0
         LE    R0,0(0,R7)    Get BFP short result RXE
         CFEBR R0,R0         Convert to integer in r0
         ST    R0,4(0,R3)    Store integer result
         LE    R0,4(0,R7)    Get BFP short result RRE
         CFEBR R0,R0         Convert to integer in r0
         ST    R0,12(0,R3)   Store integer result
*
         LE    R0,8(0,R7)    Get BFP short result RXE
         CGEBR R0,R0         Convert to integer 64 in r0
         STG   R0,16(0,R3)   Store integer result
         LE    R0,12(0,R7)   Get BFP short result RRE
         CGEBR R0,R0         Convert to integer 64 in r0
         STG   R0,24(0,R3)   Store integer result
*
         LD    R0,0(0,R8)    Get BFP long result RXE
         CFDBR R0,R0         Convert to integer in r0
         ST    R0,4(0,R4)    Store integer result
         LD    R0,8(0,R8)    Get BFP long result RRE
         CFDBR R0,R0         Convert to integer in r0
         ST    R0,12(0,R4)   Store integer result
*
         LD    R0,16(0,R8)   Get BFP long result RXE
         CGDBR R0,R0         Convert to integer 64 in r0
         STG   R0,16(0,R4)   Store integer result
         LD    R0,24(0,R8)   Get BFP long result RRE
         CGDBR R0,R0         Convert to integer 64 in r0
         STG   R0,24(0,R4)   Store integer result
*
         LD    R0,0(0,R9)    Get BFP ext. 1st half
         LD    R2,8(0,R9)    Get BFP ext. 2nd half
         CFXBR R0,R0         Convert BFP Ext. to Integer-32
         ST    R0,4(0,R5)    Store integer result lower word
*
         LD    R0,16(0,R9)   Get BFP ext. 1st half
         LD    R2,24(0,R9)   Get BFP ext. 2nd half
         CGXBR R0,R0         Convert BFP Ext. to Integer-64
         STG   R0,8(0,R5)    Store integer result 
*
         LA    R7,16(0,R7)   Point to next short BFP result pair
         LA    R3,32(0,R3)   Point to next short BFP integer pair
         LA    R8,32(0,R8)   Point to next long BFP result pair
         LA    R4,32(0,R4)   Point to next long BFP integer pair
         LA    R9,32(0,R9)   Point to next ext. BFP result 
         LA    R5,16(0,R5)   Point to next ext. BFP integer
*
         BCTR  R2,R13        Convert next pair of BFP values
         BR    R12           Return to caller
*
         END
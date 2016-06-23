*
* Tests five subtraction instructions:
*   SUBTRACT (extended BFP, RRE) 
*   SUBTRACT (long BFP, RRE) 
*   SUBTRACT (long BFP, RXE) 
*   SUBTRACT (short BFP, RRE) 
*   SUBTRACT (short BFP, RXE) 
*
* Also tests the following six conversion instructions
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
*  Convert integers 2, 1, 4, and -2 to each BFP floating point format
*  Subtract the second from the first (2 - 1, 4 - -2) in each format
*  Convert the results back to integers (none should round)
*
BFPSUBS  START 0
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
         ORG   BFPSUBS+X'1A0' 
         DC    X'0000000180000000',AD(STARTSUB)       z/Arch restart PSW
         ORG   BFPSUBS+X'1D0' 
         DC    X'0002000000000000000000000000DEAD' z/Arch pgm chk new PSW
         ORG   BFPSUBS+X'200'
STARTSUB STCTL R0,R0,CTLR0  Store CR0 to enable AFP
         OI    CTLR0+1,X'04'  Turn on AFP bit
         LCTL  R0,R0,CTLR0  Reload updated CR0
         BAS   R12,CVTINPUT Convert inputs to binary formats
         BAS   R12,TESTSUB  Perform subtractions
         BAS   R12,CVTOUTS  Convert results to integers
         LPSWE WAITPSW      All done, load enabled wait PSW
*
*
* Subtract pairs of values from each other
TESTSUB  DS    0H                
         LA    R2,2          Set count of subtraction operations
         LA    R3,SHORTRES   Point to start of short BFP results
         LA    R4,LONGRES    Point to start of long BFP results
         LA    R5,EXTRES     Point to start of extended BFP results
         LA    R7,SHORTBFP   Point to start of short BFP input values
         LA    R8,LONGBFP    Point to start of long BFP input values
         LA    R9,EXTBFP     Point to start of extended BFP input values
*     Top of loop; clear residuals from FPR0-1
SUBLOOP  LZDR  R0            Zero FPR0
         LZDR  R1            Zero FPR1
*     BFP Short subtraction RXE and RRE
         LE    R0,0(0,R7)    Get BFP short first of pair
         SEB   R0,4(0,R7)    Subtract BFP short second of pair
         STE   R0,0(0,R3)    Store short BFP result from RXE
         LE    R0,0(0,R7)    Get BFP short first of pair
         LE    R1,4(0,R7)    Get BFP short second of pair
         SEBR  R0,R1         Subtract BFP values, result in FPR0
         STE   R0,4(0,R3)    Store short BFP from RRE
         LA    R7,8(0,R7)    Point to next short BFP input pair
         LA    R3,8(0,R3)    Point to next short BFP result pair
*     BFP Long subtraction RXE and RRE
         LD    R0,0(0,R8)    Get BFP long first of pair
         SDB   R0,8(0,R8)    Subtract BFP long second of pair
         STD   R0,0(0,R4)    Store long BFP result from RXE
         LD    R0,0(0,R8)    Get BFP long first of pair
         LD    R1,8(0,R8)    Get BFP long second of pair
         SDBR  R0,R1         Subtract BFP values, result in FPR0
         STD   R0,8(0,R4)    Store long BFP from RRE
         LA    R8,16(0,R8)   Point to next long BFP input pair
         LA    R4,16(0,R4)   Point to next long BFP result pair
*     BFP Extended subtraction RRE
         LD    R0,0(0,R9)    Get BFP ext. 1st half of first of pair
         LD    R2,8(0,R9)    Get BFP ext. 2nd half of first of pair
         LD    R1,16(0,R9)   Get BFP ext. 1st half of second of pair
         LD    R3,24(0,R9)   Get BFP ext. 2nd half of second of pair
         SXBR  R0,R1         Subtract BFP values, result in FPR0-FPR2
         STD   R0,0(0,R5)    Store ext. BFP from RRE
         STD   R2,8(0,R5)    Store ext. BFP from RRE
         LA    R9,32(0,R9)   Point to next ext. BFP input pair
         LA    R5,16(0,R5)   Point to next ext. BFP result
*
         BCT   R2,SUBLOOP    Subtract next input pair
         BR    R12           Tests done, return to mainline
*
*
*
         DS    0D            Ensure correct alignment for PSW
WAITPSW  DC    X'00020000000000000000000000000000'   Disabled wait state PSW - normal completion
CTLR0    DS    F             Control register 0 (bit45 AFP control)
*
         ORG   BFPSUBS+X'480' 
INTVALS  DS    0D            Ensure alignment for 64-bit ints
         DC    F'0',F'2'         2 first test integer
         DC    F'0',F'1'         1 second test integer
         DC    F'0',F'4'         4 third test integer
         DC    F'-1',F'-2'      -2 fourth test integer
*
SHORTBFP EQU   BFPSUBS+X'410'  410.10   test inputs in short bfp

LONGBFP  EQU   BFPSUBS+X'420'  420.20   test inputs in long bfp

EXTBFP   EQU   BFPSUBS+X'440'  440.40   test inputs in extended bfp

SHORTRES EQU   BFPSUBS+X'500'  500.10   Short subtraction results (2 pairs, RXE & RRE)
LONGRES  EQU   BFPSUBS+X'510'  510.20   Long subtraction results (2 pairs, RXE & RRE)
EXTRES   EQU   BFPSUBS+X'530'  530.20   Extended subtraction results (1 pair, RRE only)

ISHBFP   EQU   BFPSUBS+X'550'  550.20   Convert short BFP to 64-bit Integer results
ILNBFP   EQU   BFPSUBS+X'570'  570.20   Convert long BFP to 64-bit Integer results
IXTBFP   EQU   BFPSUBS+X'590'  590.10   Convert extended BFP to 64-bit Integer results
*
*
* Subroutine: convert 64-bit integers into BFP in all precisions
*
         ORG   BFPSUBS+X'800' 
CVTINPUT DS    0H            Input conversion routine
         LA    R2,4          Set count of 64-bit test input values
         LA    R3,INTVALS    point to start of input values
         LA    R7,SHORTBFP   Point to start of short BFP input values
         LA    R8,LONGBFP    Point to start of long BFP input values
         LA    R9,EXTBFP     Point to start of extended BFP input values
         BASR  R13,0         Set top of loop
* convert integers to three BFP formats
CVTLOOP  LG    R1,0(0,R3)    Get integer test value
         CEGBR R0,R1         Cvt Int in GPR1 to short float in FPR0
         STE   R0,0(0,R7)    Store short BFP
         CDGBR R0,R1         Cvt Int in GPR1 to long float in FPR0
         STD   R0,0(0,R8)    Store long BFP
         CXGBR R0,R1         Cvt Int in GPR1 to ext float in FPR0-FPR2
         STD   R0,0(0,R9)    Store extended BFP part 1
         STD   R2,8(0,R9)    Store extended BFP part 2
         LA    R3,8(0,R3)    point to next input values
         LA    R7,4(0,R7)    Point to next short BFP converted values
         LA    R8,8(0,R8)    Point to next long BFP converted values
         LA    R9,16(0,R9)   Point to next extended BFP converted values
         BCTR  R2,R13        Convert next input value.  
         BR    R12           Conversion done, return to mainline
*
*
* Convert back to integers
*
CVTOUTS  DS    0H
         LA    R2,2          Set count of subtraction operations
         LA    R7,SHORTRES   Point to start of short BFP results
         LA    R8,LONGRES    Point to start of long BFP results
         LA    R9,EXTRES     Point to start of extended BFP results
         LA    R3,ISHBFP     Point to start of short BFP Integer results
         LA    R4,ILNBFP     Point to start of long BFP integer results
         LA    R5,IXTBFP     Point to start of extended BFP integer results
         BASR  R13,0         Set top of loop
*
INTLOOP  LZDR  R0            Zero FPR0
*     Convert shorts back to integer
         LE    R0,0(0,R7)    Get BFP short result first of pair 
         CGEBR R0,R0         Convert to integer in r0
         STG   R0,0(0,R3)    Store integer result
         LE    R0,4(0,R7)    Get BFP short result second of pair 
*
         CGEBR R0,R0         Convert to integer in r0
         STG   R0,8(0,R3)    Store integer result
*
         LD    R0,0(0,R8)    Get BFP long result first of pair 
         CGDBR R0,R0         Convert to integer in r0
         STG   R0,0(0,R4)    Store integer result
         LD    R0,8(0,R8)    Get BFP long result second of pair 
         CGDBR R0,R0         Convert to integer in r0
         STG   R0,8(0,R4)    Store integer result
*
         LD    R0,0(0,R9)    Get BFP ext. 1st half of first of pair
         LD    R2,8(0,R9)    Get BFP ext. 2nd half of first of pair
         CGXBR R0,R0         Convert BFP Ext. to Integer-64
         STG   R0,0(0,R5)    Store integer result lower word
*
         LA    R7,8(0,R7)    Point to next short BFP input pair
         LA    R3,16(0,R3)   Point to next short BFP integer pair
         LA    R8,16(0,R8)   Point to next long BFP input pair
         LA    R4,16(0,R4)   Point to next long BFP integer pair
         LA    R9,16(0,R9)   Point to next ext. BFP input
         LA    R5,8(0,R5)    Point to next ext. BFP integer
         BCTR  R2,R13        Subtract next input pair
         BR    R12           Conversion done, return to mainline

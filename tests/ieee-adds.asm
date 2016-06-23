*
*Testcase IEEE CONVERT FROM/TO FIXED 32 (6), ADD (5), 11 instr total
* ADD tests - Binary Floating Point
*
*
* Tests five addition instructions:
*   ADD (extended BFP, RRE) 
*   ADD (long BFP, RRE) 
*   ADD (long BFP, RXE) 
*   ADD (short BFP, RRE) 
*   ADD (short BFP, RXE) 
*
* Also tests the following three conversion instructions
*   CONVERT FROM FIXED (32 to short BFP, RRE)
*   CONVERT FROM FIXED (32 to long BFP, RRE) 
*   CONVERT FROM FIXED (32 to extended BFP, RRE)  
*   CONVERT TO FIXED (32 to short BFP, RRE)
*   CONVERT TO FIXED (32 to long BFP, RRE) 
*   CONVERT TO FIXED (32 to extended BFP, RRE)  
*
* Also tests the following floating point support instructions
*   LOAD  (Short)
*   LOAD  (Long)
*   LOAD ZERO (Long)
*   STORE (Short)
*   STORE (Long)
*
*  Convert integers 1, 2, 4, -2 to each BFP floating point format
*  Add first and second pairs together (1 + 2, 4 + -2) in each format
*
BFPADDS  START 0
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
         ORG   BFPADDS+X'1A0' 
         DC    X'0000000180000000',AD(START)       z/Arch restart PSW
         ORG   BFPADDS+X'1D0' 
         DC    X'0002000000000000000000000000DEAD' z/Arch pgm chk new PSW
         ORG   BFPADDS+X'200'
*
START    STCTL R0,R0,CTLR0    Store CR0 to enable AFP
         OI    CTLR0+1,X'04'  Turn on AFP bit
         LCTL  R0,R0,CTLR0    Reload updated CR0
*
* convert integers to three BFP formats
*
         LA    R2,4         Set count of test input values
         LA    R3,INTVALS   point to start of input values
         LA    R7,SHORTBFP  Point to start of short BFP input values
         LA    R8,LONGBFP   Point to start of long BFP input values
         LA    R9,EXTBFP    Point to start of extended BFP input values
         BASR  R12,0        Set top of loop
*
         L     R1,0(0,R3)    Get integer test value
         CEFBR R0,R1         Cvt Int in GPR1 to float in FPR0
         STE   R0,0(0,R7)    Store short BFP
         CDFBR R0,R1         Cvt Int in GPR1 to float in FPR0
         STD   R0,0(0,R8)    Store long BFP
         CXFBR R0,R1         Cvt Int in GPR1 to float in FPR0-FPR2
         STD   R0,0(0,R9)    Store extended BFP part 1
         STD   R2,8(0,R9)    Store extended BFP part 2
         LA    R3,4(0,R3)    point to next input values
         LA    R7,4(0,R7)    Point to next short BFP converted values
         LA    R8,8(0,R8)    Point to next long BFP converted values
         LA    R9,16(0,R9)   Point to next extended BFP converted values
         BCTR  R2,R12        Convert next input value.  
*
* Add pairs of values together
*
         LA    R2,2          Set count of addition operations
         LA    R3,SHORTRES   Point to start of short BFP results
         LA    R4,LONGRES    Point to start of long BFP results
         LA    R5,EXTRES     Point to start of extended BFP results
         LA    R7,SHORTBFP   Point to start of short BFP input values
         LA    R8,LONGBFP    Point to start of long BFP input values
         LA    R9,EXTBFP     Point to start of extended BFP input values
         BASR  R12,0         Set top of loop
*
* clear residuals from FPR0-1
ADDLOOP  LZDR  R0            Zero FPR0 - clear residuals from FPR0, 1
         LZDR  R1            Zero FPR1
*     BFP Short addition RXE and RRE
         LE    R0,0(0,R7)    Get BFP short first of pair
         AEB   R0,4(0,R7)    Add BFP short second of pair
         STE   R0,0(0,R3)    Store short BFP result from RXE
         LE    R0,0(0,R7)    Get BFP short first of pair
         LE    R1,4(0,R7)    Get BFP short second of pair
         AEBR  R0,R1         Add BFP values, result in FPR0
         STE   R0,4(0,R3)    Store short BFP from RRE
         LA    R7,8(0,R7)    Point to next short BFP input pair
         LA    R3,8(0,R3)    Point to next short BFP result pair
*     BFP Long addition RXE and RRE
         LD    R0,0(0,R8)    Get BFP long first of pair
         ADB   R0,8(0,R8)    Add BFP long second of pair
         STD   R0,0(0,R4)    Store long BFP result from RXE
         LD    R0,0(0,R8)    Get BFP long first of pair
         LD    R1,8(0,R8)    Get BFP long second of pair
         ADBR  R0,R1         Add BFP values, result in FPR0
         STD   R0,8(0,R4)    Store long BFP from RRE
         LA    R8,16(0,R8)   Point to next long BFP input pair
         LA    R4,16(0,R4)   Point to next long BFP result pair
*     BFP Extended addition RRE
         LD    R0,0(0,R9)    Get BFP ext. 1st half of first of pair
         LD    R2,8(0,R9)    Get BFP ext. 2nd half of first of pair
         LD    R1,16(0,R9)   Get BFP ext. 1st half of second of pair
         LD    R3,24(0,R9)   Get BFP ext. 2nd half of second of pair
         AXBR  R0,R1         Add BFP values, result in FPR0-FPR2
         STD   R0,0(0,R5)    Store ext. BFP from RRE
         STD   R2,8(0,R5)    Store ext. BFP from RRE
*
         LA    R9,32(0,R9)   Point to next ext. BFP input pair
         LA    R5,16(0,R5)   Point to next ext. BFP result
         BCTR  R2,R12        Add next input pair
*
* Convert back to integers
*
         LA    R2,2          Set count of addition operations
         LA    R7,SHORTRES   Point to start of short BFP results
         LA    R8,LONGRES    Point to start of long BFP results
         LA    R9,EXTRES     Point to start of extended BFP results
         LA    R3,ISHBFP     Point to start of short BFP Integer results
         LA    R4,ILNBFP     Point to start of long BFP integer results
         LA    R5,IXTBFP     Point to start of extended BFP integer results
         BASR  R12,0         Set top of loop
*
         LZDR  R0            Zero FPR0
*    Convert shorts back to integer
         LE    R0,0(0,R7)    Get BFP short result first of pair 
         CFEBR R0,R0         Convert to integer in r0
         ST    R0,0(0,R3)    Store integer result
         LE    R0,4(0,R7)    Get BFP short result second of pair 
         CFEBR R0,R0         Convert to integer in r0
         ST    R0,4(0,R3)    Store integer result
*
         LD    R0,0(0,R8)    Get BFP long result first of pair 
         CFDBR R0,R0         Convert to integer in r0
         ST    R0,0(0,R4)    Store integer result
         LD    R0,8(0,R8)    Get BFP long result second of pair 
         CFDBR R0,R0         Convert to integer in r0
         ST    R0,4(0,R4)    Store integer result
*
         LD    R0,0(0,R9)    Get BFP ext. 1st half of first of pair
         LD    R2,8(0,R9)    Get BFP ext. 2nd half of first of pair
         CFXBR R0,R0         Convert BFP Ext. to Integer-64
         ST    R0,0(0,R5)    Store integer result lower word
*
         LA    R7,8(0,R7)    Point to next short BFP input pair
         LA    R3,8(0,R3)    Point to next short BFP integer pair
         LA    R8,16(0,R8)   Point to next long BFP input pair
         LA    R4,8(0,R4)    Point to next long BFP integer pair
         LA    R9,16(0,R9)   Point to next ext. BFP input
         LA    R5,4(0,R5)    Point to next ext. BFP integer
*
         BCTR  R2,R12        Add next input pair
*
         LPSWE WAITPSW       Load enabled wait PSW
*
         ORG   BFPADDS+X'3EC'
CTLR0    DS    F
WAITPSW  DC    X'00020000000000000000000000000000'    Disabled wait state PSW - normal completion
         ORG   BFPADDS+X'400'
INTVALS  DS    0F
         DC    F'1'
         DC    F'2'
         DC    F'4'
         DC    F'-2'
*
SHORTBFP DS    4F                  Four short BFP values
LONGBFP  DS    4D                  Four long BFP values
EXTBFP   DS    8D                  Four extended BPF values
*
         ORG   BFPADDS+X'500'
SHORTRES DS    4F                  Four short BFP results
LONGRES  DS    4D                  Four long BFP results
EXTRES   DS    4D                  Two extended BFP results
*
ISHBFP   DS    4F                  Four short BFP results in 32-bit integer form
ILNBFP   DS    4F                  Four long BFP results in 32-bit integer form
IXTBFP   DS    4F                  Four extended BPF results in 32-bit integer form

         END

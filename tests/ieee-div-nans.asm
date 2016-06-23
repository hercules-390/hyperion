*
*  Divide adjacent pairs of values in the input set (six values means five results).
*
*  Test data:         2,    0,      QNaN(1),      QNaN(2),      SNaN(3),      SNaN(4).
*  Expected Results    +inf, QNaN(1),      QNaN(1),      SNaN(3),      SNaN(3).
*
*  NaN payload--in parentheses--is used to confirm that the right NaN ends up in the result.
*
* Tests seven division instructions:
*   DIVIDE (BFP short RRE) DEBR
*   DIVIDE (BFP short RXE) DEB
*   DIVIDE (BFP long, RRE) DDBR
*   DIVIDE (BFP long, RXE) DDB
*   DIVIDE (BFP extended, RRE) DXBR
*
* Interrupts are masked; no program checks are expected
*
* Also tests the following floating point support instructions
*   LOAD  (Short)
*   LOAD  (Long)
*   LOAD ZERO (Long)
*   STORE (Short)
*   STORE (Long)
*
BFPDIVNA START 0
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
         ORG   BFPDIVNA+X'1A0' 
         DC    X'0000000180000000',AD(STARTNAN)       z/Arch restart PSW
         ORG   BFPDIVNA+X'1D0' 
         DC    X'0002000000000000000000000000DEAD' z/Arch pgm chk new PSW
         ORG   BFPDIVNA+X'200'
*
* Mainline program.
*
STARTNAN STCTL R0,R0,CTLR0    Store CR0 to enable AFP
         OI    CTLR0+1,X'04'  Turn on AFP bit
         LCTL  R0,R0,CTLR0    Reload updated CR0
         BAS   R12,TESTDIV    Perform divisions
         LPSWE WAITPSW        All done, load disabled wait PSW
*
* BFP Division Short RXE and RRE using NaNs as inputs.
*
* We cannot use Load Rounded to shrink extended BFP into the shorts needed
* for this test because Load Rounded will convert the SNaNs into QNaNs
*
TESTDIV  DS    0H
         LA    R2,5          Set count of division operations
         LA    R3,SHORTRES   Point to start of short BFP quotients
         LA    R7,SHORTBFP   Point to start of short BFP input values
         BASR  R13,0         Set top of loop for short BFP tests
*
*     Top of loop
*     Clear residuals from FPR0, 4, 5
         LZDR  R0            Zero FPR0
         LZDR  R4            Zero FPR4
*     Collect dividend and divisor, do four divisions, store quotients.
         LE    R0,0(0,R7)    Get BFP ext dividend
         LDR   R5,R0         Duplicate dividend for RRE test
         LE    R4,4(0,R7)    Get BFP ext divisor for RRE test
         DEB   R0,4(0,R7)    Generate RXE
         DEBR  R5,R4         Generate RRE
         STE   R0,0(0,R3)    Store short BFP RXE
         STE   R5,4(0,R3)    Store short BFP RRE
         LA    R7,4(0,R7)    Point to next short BFP input pair
         LA    R3,8(0,R3)    Point to next short BFP result pair
         BCTR  R2,R13        Loop through all short BFP test cases
         B     TESTLONG      Go test long BFP division.
*
* BFP Division long RXE and RRE
*
TESTLONG DS    0H
         LA    R2,5          Set count of division operations
         LA    R3,LONGRES    Point to start of long BFP quotients
         LA    R7,LONGBFP    Point to start of extended BFP input values
         BASR  R13,0         Set top of loop for long BFP tests
*     Top of loop
*     Collect dividend and divisor, do two divisions, store quotients
         LD    R0,0(0,R7)    Get BFP ext dividend
         LDR   R5,R0         Save dividend for RRE
         LD    R4,8(0,R7)    Get BFP ext divisor
         DDB   R0,8(0,R7)    Generate RXE long
         DDBR  R5,R4         Generate RRE long
         STD   R0,0(0,R3)    Store long BFP RXE quotient
         STD   R5,8(0,R3)    Store long BFP RRE quotient
         LA    R7,8(0,R7)    Point to next long BFP input pair
         LA    R3,16(0,R3)   Point to next long BFP result pair
         BCTR  R2,R13        Loop through all long test cases
         B     TESTEXT       Skip across patch area
*
* BFP division extended, RRE only.
*
TESTEXT  DS    0H
         LA    R2,5          Set count of division operations
         LA    R4,EXTDRES    Point to start of extended BFP quotients
         LA    R7,EXTDBFP    Point to start of extended BFP input values
         BASR  R13,0         Set top of loop for l*l tests
*     Top of loop
*     Collect dividend & divisor, do division and Load FP Integer, store results.
         LD    R0,0(0,R7)    Get BFP ext dividend part 1
         LD    R2,8(0,R7)    Get BFP ext dividend part 2
         LD    R13,16(0,R7)  Get BFP ext divisor part 1
         LD    R15,24(0,R7)  Get BFP ext divisor part 2
         DXBR  R0,R13        Generate RRE extended quotient
         STD   R0,0(0,R4)    Store extended BFP RRE quotient part 1
         STD   R2,8(0,R4)    Store extended BFP RRE quotient part 2
         LA    R7,16(0,R7)   Point to next extended BFP input pair
         LA    R4,16(0,R4)   Point to next extended BFP result
         BCTR  R2,R13        Loop through all test cases
*
         BR    R12           Tests done, return to mainline
*
*
*
CTLR0    DS    F             Control register 0 (bit45 AFP control)
WAITPSW  DC    X'00020000000000000000000000000000' Disabled wait state PSW - normal completion
*
         ORG   BFPDIVNA+X'300'
SHORTBFP DS  0D    6 short BFP (Room for 8)
         DC    X'40000000'        DC    2
         DC    X'00000000'        DC    0
         DC    X'7FC10000'     QNaN(1)
         DC    X'7FC20000'     QNaN(2)
         DC    X'7F830000'     SNaN(3)
         DC    X'7F840000'     SNaN(4)
*
         ORG   BFPDIVNA+X'320'
LONGBFP  DS  0D    6 long BFP (Room for 8)
         DC    X'4000000000000000'     2
         DC    X'0000000000000000'     0
         DC    X'7FF8100000000000'     QNaN(1)
         DC    X'7FF8200000000000'     QNaN(2)
         DC    X'7FF0300000000000'     SNaN(3)
         DC    X'7FF0400000000000'     SNaN(4)
*
         ORG   BFPDIVNA+X'360'
EXTDBFP  DS  0D    6 Extended BFP (Room for 8, but not 9)
         DC    X'40000000000000000000000000000000'     2
         DC    X'00000000000000000000000000000000'     0
         DC    X'7FFF8100000000000000000000000000'     QNaN(1)
         DC    X'7FFF8200000000000000000000000000'     QNaN(2)
         DC    X'7FFF0300000000000000000000000000'     SNaN(3)
         DC    X'7FFF0400000000000000000000000000'     SNaN(4)
*
          ORG    BFPDIVNA+X'400'
SHORTRES DS 8D    Results from short divide, five pairs, room for 8
LONGRES  DS 16D   Results from long divide, five pairs, room for 8
          ORG    BFPDIVNA+X'500'
EXTDRES  DS 16D   Results from extended divide, five results, room for 8

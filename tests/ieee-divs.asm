*
*  Divide adjacent pairs of values in the input set (five values means four
*  quotients).  Test data: 1, 2, 4, -2, -2; expected quotients 0.5, 0.5, -2, 1
*  Load Floating Point Integer of the above result set.  Expected values 0, 0, -2, 1
*  For short and long BFP, divide adjacent pairs using DIVIDE TO INTEGER.
*      Expected quotient/remainder pairs: 0/1, 0/2, 2/0, 1/-0 (Yes, minus zero)
*
* Tests seven division instructions:
*   DIVIDE (BFP short RRE) DEBR
*   DIVIDE (BFP short RXE) DEB
*   DIVIDE (BFP long, RRE) DDBR
*   DIVIDE (BFP long, RXE) DDB
*   DIVIDE (BFP extended, RRE) DXBR
*   DIVIDE TO INTEGER (BFP short RRE) DIEBR
*   DIVIDE TO INTEGER (BFP long RRE) DIDBR
*
* Also tests the following six conversion instructions
*   LOAD ROUNDED (long BFP to short BFP, RRE) LEDBR
*   LOAD ROUNDED (extended BFP to short BFP, RRE) LEXBR
*   LOAD ROUNDED (extended BFP to long BFP, RRE) LDXBR
*   LOAD FP INTEGER (BFP short RRE) FIEBR
*   LOAD FP INTEGER (BFP long RRE) FIDBR
*   LOAD FP INTEGER (BFP extended RRE) FIXBR
*
* Intermediate results from the Load Rounded instructions are not saved.  Because
* zero is not present in the test set, any error in Load Rounded will appear in 
* the resulting quotients, remainders, and integer floating point values.
*
* Also tests the following floating point support instructions
*   LOAD  (Short)
*   LOAD  (Long)
*   LOAD ZERO (Long)
*   STORE (Short)
*   STORE (Long)
*
*
*
BFPDIVS  START 0
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
*         
         ORG   BFPDIVS+X'1A0' 
         DC    X'0000000180000000',AD(STARTDIV)       z/Arch restart PSW
         ORG   BFPDIVS+X'1D0' 
         DC    X'0002000000000000000000000000DEAD' z/Arch pgm chk new PSW
         ORG   BFPDIVS+X'200'
*
* Mainline program.  
STARTDIV STCTL R0,R0,CTLR0    Store CR0 to enable AFP
         OI    CTLR0+1,X'04'  Turn on AFP bit
         LCTL  R0,R0,CTLR0    Reload updated CR0
         BAS   R12,TESTDIV    Perform divisions
         LPSWE WAITPSW        All done, load disabled wait PSW
*
* BFP Division Short RXE and RRE
* Use Load Rounded to shrink extended BFP into the shorts needed for this test.
*
         ORG   BFPDIVS+X'600'
TESTDIV  DS    0H           
         LA    R2,4           Set count of division operations
         LA    R3,SHORTRES    Point to start of short BFP quotients
         LA    R4,SHORTDTI    Point to short Divide Integer quotients and remainders
         LA    R5,SHORTLFP    Point to short Load Floating Point Integer results
         LA    R7,EXTBFPIN    Point to start of extended BFP input values
         BASR  R13,0          Set top of loop for short BFP tests
*
*     Top of loop; clear residuals from FPR1, 4, 5, 8, 9.
         LZDR  R1             Zero FPR1
         LZDR  R4             Zero FPR4
         LZDR  R5             Zero FPR5
         LZDR  R8             Zero FPR8
         LZDR  R9             Zero FPR9
*     Collect dividend and divisor, do four divisions, store quotients.
         LD    R0,0(0,R7)     Get BFP ext dividend part 1
         LD    R2,8(0,R7)     Get BFP ext dividend part 2
         LD    R13,16(0,R7)   Get BFP ext divisor part 1
         LD    R15,24(0,R7)   Get BFP ext divisor part 2
         LDXBR R1,R0          Round dividend for RXE to long
         LEDBR R1,R1          Round dividend for RXE to short
         LEXBR R5,R0          Round dividend for RRE
         LER   R9,R5          Save dividend for Divide to Integer
         LEXBR R4,R13         Round divisor for RRE
         STE   R4,0(0,R3)     Save divisor for RXE
         DIEBR R9,R8,R4       Divide to Integer, quotient in R8, remainder in R9
         DEB   R1,0(0,R3)     Generate RXE
         DEBR  R5,R4          Generate RRE
         FIEBR R4,0,R5        Load result as Floating Point Integer
         STE   R1,0(0,R3)     Store short BFP RXE
         STE   R5,4(0,R3)     Store short BFP RRE
         STE   R8,0(0,R4)     Store divide to integer quotient
         STE   R9,4(0,R4)     Store divide to integer remainder
         STE   R4,0(0,R5)     Store load FP integer result
         LA    R7,16(0,R7)    Point to next extended BFP input pair
         LA    R3,8(0,R3)     Point to next short BFP result pair
         LA    R4,8(0,R4)     Point to next Divide to Integer result
         LA    R5,4(0,R5)     Point to next Load FP Integer result
         BCTR  R2,R13         Loop through all short BFP test cases
*
* BFP Division long RXE and RRE 
* Use Load Rounded to shrink extended BFP input into the longs needed for this test.
*
TESTLONG DS    0H
         LA    R2,4           Set count of division operations
         LA    R3,LONGRES     Point to start of long BFP quotients
         LA    R4,LONGDTI     Point to short Divide Integer quotients and remainders
         LA    R5,LONGLFP     Point to short Load Floating Point Integer results
         LA    R7,EXTBFPIN    Point to start of extended BFP input values
         BASR  R13,0          Set top of loop for long BFP tests
*     Top of loop
*     Collect dividend and divisor, shorten them, do four divisions, store quotients
         LD    R0,0(0,R7)     Get BFP ext dividend part 1
         LD    R2,8(0,R7)     Get BFP ext dividend part 2
         LD    R13,16(0,R7)   Get BFP ext divisor part 1
         LD    R15,24(0,R7)   Get BFP ext divisor part 2
         LDXBR R1,R0          Round dividend for RXE 
         LDXBR R5,R0          Round dividend for RRE
         LDR   R9,R5          Save dividend for divide to Integer
         LDXBR R4,R13         Round divisor for RRE
         STD   R4,0(0,R3)     Save divisor for RXE
         DIDBR R9,R8,R4       Divide to Integer, quotient in R8, remainder in R9
         DDB   R1,0(0,R3)     Generate RXE long
         DDBR  R5,R4          Generate RRE long
         FIDBR R4,0,R5        Load result as Floating Point Integer
         STD   R1,0(0,R3)     Store long BFP RXE quotient
         STD   R5,8(0,R3)     Store long BFP RRE quotient
         STD   R8,0(0,R4)     Store long BFP divide to integer quotient
         STD   R9,8(0,R4)     Store long BFP divide to integer remainder
         STD   R4,0(0,R5)     Store long BFP load FP integer result
         LA    R7,16(0,R7)    Point to next extended BFP input pair
         LA    R3,16(0,R3)    Point to next long BFP result pair
         LA    R4,16(0,R4)    Point to next long BFP divide to integer quotient and remainder
         LA    R5,8(0,R5)     Point to next long BFP Load FP Integer result
         BCTR  R2,R13         Loop through all long test cases
*
* BFP division extended, RRE only.  We do two divisions per input pair so we can 
* test a diversity of Load Rounded instructions.  
*
TESTEXT  DS    0H
         LA    R2,4           Set count of division operations
         LA    R4,EXTDRES     Point to start of extended BFP quotients
         LA    R5,EXTLFP      Point to start of extended Load FP Integer results
         LA    R7,EXTBFPIN    Point to start of extended BFP input values
         BASR  R13,0          Set top of loop for l*l tests
*     Top of loop
*     Collect dividend & divisor, do division and Load FP Integer, store results.
         LD    R0,0(0,R7)     Get BFP ext dividend part 1
         LD    R2,8(0,R7)     Get BFP ext dividend part 2
         LD    R13,16(0,R7)   Get BFP ext divisor part 1
         LD    R15,24(0,R7)   Get BFP ext divisor part 2
         DXBR  R0,R13         Generate RRE extended quotient
         FIXBR R1,0,R0        create Load FP Integer result
         STD   R0,0(0,R4)     Store extended BFP RRE quotient part 1
         STD   R2,8(0,R4)     Store extended BFP RRE quotient part 2
         STD   R1,0(0,R5)     Store extended BFP Load FP Integer part 1
         STD   R3,8(0,R5)     Store extended BFP Load FP Integer part 2
         LA    R7,16(0,R7)    Point to next extended BFP input pair
         LA    R4,16(0,R4)    Point to next extended BFP result
         LA    R5,16(0,R5)    Point to next extended BFP Load FP Result
         BCTR  R2,R13         Loop through all test cases
*
         BR    R12            Tests done, return to mainline
*
*
CTLR0    DS    F              Control register 0 (bit45 AFP control)
         DS    0D             Ensure doubleword alignment
WAITPSW  DC    X'00020000000000000000000000000000' Disabled wait state PSW - normal completion
*
         ORG  BFPDIVS+X'300'
EXTBFPIN DS   0D
         DC    X'3FFF0000000000000000000000000000' 1.0 BFP Extended
         DC    X'40000000000000000000000000000000' 2.0 BFP Extended
         DC    X'40010000000000000000000000000000' 4.0 BFP Extended
         DC    X'C0000000000000000000000000000000' -2.0 BFP Extended
         DC    X'C0000000000000000000000000000000' -2.0 BFP Extended
*
         ORG  BFPDIVS+X'420'
SHORTRES DS   8F     420.20   Results from short divide, four pairs
LONGRES  DS   8D     440.40   Results from long divide, four pairs
EXTDRES  DS   8D     480.40   Results from extended divide, four results
SHORTDTI DS   8F     4C0.20   Results from short Divide to Integer, four quotient/remainder pairs
SHORTLFP DS   4F     4E0.10   Results from short Load Floating Point Integer, four results
*
         ORG  BFPDIVS+X'500'
LONGDTI  DS   8D     500.40   Results from long Divide to Integer, four quotient/remainder pairs
LONGLFP  DS   4D     540.20   Results from long Load Floating Point Integer, four results
EXTLFP   DS   8D     560.40   Results from Extended Load Floating Point Integer, four results
*
         END
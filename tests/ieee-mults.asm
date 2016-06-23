*
*  1. Multiply adjacent pairs of values in the input set (five values means four
*     products).  
*     Test data: 1, 2, 4, -2, -2; expected products 2, 8, -8, 4
*
*  2. Multiply adjacent pairs of values in the input set and add the first value
*     in the pair to the product (five values means four results).  
*     Test data: 1, 2, 4, -2, -2; expected results 3, 10, -4, 2
*
*  3. Multiply adjacent pairs of values in the input set and subtract the first 
*     value in the pair from the product (five values means four results).  
*     Test data: 1, 2, 4, -2, -2; expected results 1, 6, -12, 6
*
* Tests 17  multiplication instructions:
*   MULTIPLY (BFP short x short -> short, RRE) MEEBR
*   MULTIPLY (BFP short x short -> short, RXE) MEEB
*   MULTIPLY (BFP short x short -> long, RRE) MDEBR
*   MULTIPLY (BFP short x short -> long, RXE) MDEB
*   MULTIPLY (BFP long x long -> long, RRE) MDBR
*   MULTIPLY (BFP long x long -> long, RXE) MDB
*   MULTIPLY (BFP long x long -> extended, RRE) MXDBR
*   MULTIPLY (BFP long x long -> extended, RXE) MXDB
*   MULTIPLY (BFP long x long -> extended, RRE) MXDBR
*   MULTIPLY AND ADD (BFP short x short -> short, RRE) MAEBR
*   MULTIPLY AND ADD (BFP short x short -> short, RXE) MAEB
*   MULTIPLY AND ADD (BFP long x long -> long, RRE) MADBR
*   MULTIPLY AND ADD (BFP long x long -> long, RXE) MADB
*   MULTIPLY AND SUBTRACT (BFP short x short -> short, RRE) MSEBR
*   MULTIPLY AND SUBTRACT (BFP short x short -> short, RXE) MSEB
*   MULTIPLY AND SUBTRACT (BFP long x long -> long, RRE) MSDBR
*   MULTIPLY AND SUBTRACT (BFP long x long -> long, RXE) MSDB
*
* Also tests the following six conversion instructions
*   LOAD LENGTHENED (short BFP to long BFP, RXE)
*   LOAD LENGTHENED (short BFP to extended BFP, RXE) 
*   LOAD LENGTHENED (long BFP to extended BFP, RXE)  
*   LOAD LENGTHENED (short BFP to long BFP, RRE)
*   LOAD LENGTHENED (short BFP to extended BFP, RRE) 
*   LOAD LENGTHENED (long BFP to extended BFP, RRE)  
*
* Intermediate results from the Load Lengthened instructions are not saved.  Because
* zero is not present in the input test set, any error in Load Lengthened will appear
* in the resulting products
*
* Also tests the following floating point support instructions
*   LOAD  (Short)
*   LOAD  (Long)
*   LOAD ZERO (Long)
*   STORE (Short)
*   STORE (Long)
*
BFPMULTS START 0
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
         ORG   BFPMULTS+X'1A0' 
         DC    X'0000000180000000',AD(STARTMUL)       z/Arch restart PSW
         ORG   BFPMULTS+X'1D0' 
         DC    X'0002000000000000000000000000DEAD' z/Arch pgm chk new PSW
         ORG   BFPMULTS+X'200'
*
*
STARTMUL STCTL R0,R0,CTLR0    Store CR0 to enable AFP
         OI    CTLR0+1,X'04'  Turn on AFP bit
         LCTL  R0,R0,CTLR0    Reload updated CR0
         BAL   R12,TESTMULT   Perform multiplications
         LPSWE WAITPSW        All done, load disabled wait PSW
*
* BFP Multiplication Short x Short -> Short & Short x Short -> Long, RXE and RRE 
*
         ORG   BFPMULTS+X'600'
TESTMULT DS    0H                
         LA    R2,4          Set count of multiplication operations
         LA    R3,SHORTRES   Point to start of short BFP products
         LA    R4,LONGRES1   Point to start of long BFP products
         LA    R5,SHORTMAD   Point to start of short BFP Mult-Add results
         LA    R6,SHORTMSB   Point to start of short BFP Mult-Sub results
         LA    R7,SHORTBFP   Point to start of short BFP input values
         BASR  R13,0         Set top of loop
*     Top of loop; clear residuals from FP registers
         LZDR  R0            Zero FPR0
         LZDR  R1            Zero FPR1
         LZDR  R4            Zero FPR4
         LZDR  R5            Zero FPR5
         LZDR  R13           Zero FPR13
*     Collect multiplicand and multiplier, do four multiplications, store results.
         LE    R0,0(0,R7)    Get BFP short multiplicand
         LE    R13,4(0,R7)   Get BFP short multiplier
         LER   R1,R0         Copy multiplicand for s*s->s RRE
         LER   R4,R0         Copy multiplicand for s*s->l RXE
         LER   R5,R0         Copy multiplicand for s*s->l RRE
         MEEB  R0,4(0,R7)    Generate RXE s*s->s product
         MEEBR R1,R13        Generate RRE s*s->s product
         MDEB  R4,4(0,R7)    Generate RXE s*s->l product
         MDEBR R5,R13        Generate RRE s*s->l product
         STE   R0,0(0,R3)    Store short BFP RXE s*s->s product
         STE   R1,4(0,R3)    Store short BFP RRE s*s->s product
         STE   R4,0(0,R4)    Store short BFP RXE s*s->l product
         STE   R5,8(0,R4)    Store short BFP RRE s*s->l product
         LE    R0,0(0,R7)    Reload BFP short multiplicand for mult-add RXE
         LER   R1,R0         Copy multiplicand for mult-add RRE
         MAEB  R0,R0,4(0,R7) Multiply third by second, add to first RXE
         MAEBR R1,R13,R1     Multiply third by second, add to the first RRE
         STE   R0,0(0,R5)    Store short BFP RXE mult-add result
         STE   R1,4(0,R5)    Store short BFP RRE mult-add result
         LE    R0,0(0,R7)    Reload BFP short multiplicand for mult-sub RXE
         LER   R1,R0         Copy multiplicand for mult-sub RRE
         MSEB  R0,R0,4(0,R7) Multiply third by second, subtract the first RXE
         MSEBR R1,R13,R1     Multiply third by second, subtract the first RRE
         STE   R0,0(0,R6)    Store short BFP RXE mult-sub result
         STE   R1,4(0,R6)    Store short BFP RRE mult-sub result
         LA    R7,4(0,R7)    Point to next short BFP input pair
         LA    R3,8(0,R3)    Point to next short BFP product pair
         LA    R4,16(0,R4)   Point to next long BFP product pair
         LA    R5,8(0,R5)    Point to next short BFP mult-add result pair
         LA    R6,8(0,R6)    Point to next short BFP mult-sub result pair
         BCTR  R2,R13        Loop through all s*s test cases
*
* BFP Multiplication long x long -> long & long x long -> extended, RXE and RRE 
* Use Load Lengthened to expand Short BFP into the longs needed for this test.
*
TESTLONG DS    0H
         LA    R2,4          Set count of multiplication operations
         LA    R3,LONGRES2   Point to start of long BFP products
         LA    R4,EXTDRES1   Point to start of extended BFP products
         LA    R5,LONGMAD    Point to start of long BFP Mult-Add results
         LA    R6,LONGMSB    Point to start of long BFP Mult-Sub results
         LA    R7,SHORTBFP   Point to start of short BFP input values
         BASR  R13,0         Set top of loop for l*l tests
*     Top of loop; clear residuals from FP registers
         LZDR  R0            Zero FPR0
         LZDR  R1            Zero FPR1
         LZDR  R4            Zero FPR4
         LZDR  R5            Zero FPR5
         LZDR  R13           Zero FPR13
*     Collect multiplicand and multiplier, lengthen them, do four multiplications, store results.
         LE    R0,0(0,R7)    Get BFP short multiplicand
         LDEB  R13,4(0,R7)   Lengthen BFP short multiplier into long RXE
         STD   R13,0(0,R3)   Store for use in MDB/MXDB tests
         STD   R13,0(0,R5)   Store for use in MADB/MADBR tests
         STD   R13,0(0,R6)   Store for use in MSDB/MSDBR tests
         LDEBR R1,R0         Lengthen multiplicand for l*l->l RRE
         LDEBR R4,R0         Lengthen multiplicand for l*l->e RXE
         LDEBR R5,R0         Lengthen multiplicand for l*l->e RRE
         LDEBR R0,R0         Lengthen multiplicand for l*l->l RXE (This one must be last in the list)
         LDR   R15,R0        Save long multiplicand for MADB/MADBR/MSDB/MSDBR tests 
         MDB   R0,0(0,R3)    Generate RXE l*l->l product
         MDBR  R1,R13        Generate RRE l*l->l product
         MXDB  R4,0(0,R3)    Generate RXE l*l->e product
         MXDBR R5,R13        Generate RRE l*l->e product
         STD   R0,0(0,R3)    Store BFP RXE l*l->l product 
         STD   R1,8(0,R3)    Store BFP RRE l*l->l product 
         STD   R4,0(0,R4)    Store BFP RXE l*l->e product part 1
         STD   R6,8(0,R4)    Store BFP RXE l*l->e product part 2
         STD   R5,16(0,R4)   Store BFP RRE l*l->e product part 1
         STD   R7,24(0,R4)   Store BFP RRE l*l->e product part 2
         LDR   R0,R15        Reload BFP long multiplicand for mult-add RXE
         LDR   R1,R0         Copy multiplicand for mult-add RRE
         MADB  R0,R0,0(0,R5) Multiply third by second, add to first RXE
         MADBR R1,R13,R1     Multiply third by second, add to the first RRE
         STD   R0,0(0,R5)    Store long BFP RXE mult-add result
         STD   R1,8(0,R5)    Store long  BFP RRE mult-add result
         LDR   R0,R15        Reload BFP long multiplicand for mult-sub RXE
         LDR   R1,R0         Copy multiplicand for mult-sub RRE
         MSDB  R0,R0,0(0,R6) Multiply third by second, subtract the first RXE
         MSDBR R1,R13,R1     Multiply third by second, subtract the first RRE
         STD   R0,0(0,R6)    Store long BFP RXE mult-sub result
         STD   R1,8(0,R6)    Store long BFP RRE mult-sub result
         LA    R7,4(0,R7)    Point to next short BFP input pair
         LA    R3,16(0,R3)   Point to next long BFP product pair
         LA    R4,32(0,R4)   Point to next extended BFP product pair
         LA    R5,16(0,R5)   Point to next long BFP mult-add result pair
         LA    R6,16(0,R6)   Point to next long BFP mult-sub result pair
         BCTR  R2,R13        Loop through all l*l cases
*
* BFP Multiplication extended x extended -> extended, RRE only.  We do two multiplies
* per input pair so we can test a diversity of Load Lengthened instructions
*
TESTEXT  DS    0H
         LA    R2,4          Set count of multiplication operations
         LA    R4,EXTDRES2   Point to start of extended BFP results
         LA    R7,SHORTBFP   Point to start of short BFP input values
         BASR  R13,0         Set top of loop for e*e tests
*     Top of loop
*     Collect multiplicand and multiplier, lengthen them, do two multiplications, store results.
         LE    R0,0(0,R7)    Get BFP short multiplicand
         LXEBR R1,R0         Lengthen multiplicand into R1-R3 for e*e->e RRE (2)
         LDEBR R0,R0         lengthen multiplicand to long
         STD   R0,0(0,R4)    Save for test of LXDB RXE
         LXDB  R0,0(0,R4)    Load long multiplicand, lengthen for e*e->e RRE (1)
         LXEB  R13,4(0,R7)   Lengthen BFP short multiplier into ext
         LDEB  R12,4(0,R7)   lengthen short multiplier into long
         LXDBR R12,R12       Lengthen long multiplier into ext
         MXBR  R0,R13        Generate RRE (s->e) * (s->e) -> e
         MXBR  R1,R12        Generate RRE (s->l->e) * (s->l->e) -> e
         STD   R0,0(0,R4)    Store BFP RRE e*e->e from short part 1
         STD   R2,8(0,R4)    Store BFP RRE e*e->e ..part 2
         STD   R1,16(0,R4)   Store BFP RRE e*e->e from short->long part 1
         STD   R3,24(0,R4)   Store BFP RRE e*e->e ..part 2
         LA    R7,4(0,R7)    Point to next short BFP input pair
         LA    R4,32(0,R4)   Point to next extended BFP result pair
         BCTR  R2,R13        Loop through all e*e cases
*
         BR    R12           Tests done, return to mainline
*
*
*
         DS    0D            Ensure correct alignment for PSW
WAITPSW  DC    X'00020000000000000000000000000000'  Disabled wait state PSW - normal completion
CTLR0    DS    F             Control register 0 (bit45 AFP control)
*
         ORG   BFPMULTS+X'400'
SHORTBFP DS    0F            Ensure alignment
         DC    X'3F800000'     1 in short BFP 
         DC    X'40000000'     2 
         DC    X'40800000'     4
         DC    X'C0000000'     -2
         DC    X'C0000000'     -2
*
LONGMAD  EQU   BFPMULTS+X'300'   300.40     Results from (l*l)+l->l
LONGMSB  EQU   BFPMULTS+X'340'   340.40     Results from (l*l)-l->l
SHORTRES EQU   BFPMULTS+X'420'   420.20     Results from s*s->s
LONGRES1 EQU   BFPMULTS+X'440'   440.40     Results from s*s->l
LONGRES2 EQU   BFPMULTS+X'480'   480.40     Results from l*l->l
SHORTMAD EQU   BFPMULTS+X'4C0'   4C0.20     Results from (s*s)+s->s
SHORTMSB EQU   BFPMULTS+X'4E0'   4E0.20     Results from (s*s)-s->s
EXTDRES1 EQU   BFPMULTS+X'500'   500.80     Results from l*l->e
EXTDRES2 EQU   BFPMULTS+X'580'   580.80     Results from e*e->e

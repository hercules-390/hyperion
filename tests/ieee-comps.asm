*
*  For the following tests, the IEEE exception masks in the FPC register are set
*  to zero, so no program interruptions.  The IEEE status flags are saved and set 
*  to zero after each test.  
*
*  1. COMPARE adjacent pairs of values in the input set (nine values means eight
*     comparisons).  Save condition code and FPC register IEEE flags byte (byte 1).
*
*  2. COMPARE AND SIGNAL adjacent pairs of values in the input set (nine values 
*     means eight comparisons).  Save condition code and FPC register IEEE flags 
*     byte (byte 1).
*
*  3. LOAD AND TEST the first eight values in the input set (the nineth value is
*     not tested).  Save condition code and FPC register IEEE flags byte (byte 1).
*     The result from the last LOAD AND TEST is saved to verify conversion of the
*     SNaN into a QNaN.  
*
*  4. TEST DATA CLASS the first eight values in the input set (the nineth value is
*     not tested) using each of two masks specific to the input value.  The first
*     mask will test true (cc=1)  and the second will test false (cc=0).  Save the
*     condition code; the IEEE flags are not changed by this instruction.
*
*     Test data:             QNaN,    1,    1,    2,  -inf, -SubN,  +inf, SNaN,    0.
*     Test data class masks:  080,  200,  200,  200,   010,  100,    020,  002.
*
*     Expected results in cc/flags format for COMPARE, COMPARE AND SIGNAL, and LOAD AND TEST
*     - COMPARE:            3/00  0/00  1/00  2/00  1/00  1/00  3/80  3/80
*     - COMPARE AND SIGNAL: 3/80  0/00  1/00  2/00  1/00  1/00  3/80  3/80
*     - LOAD AND TEST:      3/00  2/00  2/00  2/00  1/00  1/00  2/00  3/80
*
*     Expected results in cc1/cc2 format for TEST DATA CLASS
*     - TEST DATA CLASS:    1/0   1/0   1/0   1/0   1/0   1/0   1/0   1/0
*
*
* Tests 5 COMPARE, 5 COMPARE AND SIGNAL, 3 LOAD AND TEST, and 3 TEST DATA CLASS instructions
*   COMPARE (BFP short, RRE) CEBR
*   COMPARE (BFP short, RXE) CEB
*   COMPARE (BFP long, RRE) CDBR
*   COMPARE (BFP long, RXE) CDB
*   COMPARE (BFP extended, RRE) CXBR
*   COMPARE AND SIGNAL (BFP short, RRE) KEBR
*   COMPARE AND SIGNAL (BFP short, RXE) KEB
*   COMPARE AND SIGNAL (BFP long, RRE) KDBR
*   COMPARE AND SIGNAL (BFP long, RXE) KDB
*   COMPARE AND SIGNAL (BFP extended, RRE) KXBR
*   LOAD AND TEST (BFP short, RRE) LTEBR
*   LOAD AND TEST (BFP long, RRE) LTDBR
*   LOAD AND TEST (BFP extended, RRE) LTXBR
*   TEST DATA CLASS (BFP short, RRE) LTEBR
*   TEST DATA CLASS (BFP long, RRE) LTDBR
*   TEST DATA CLASS (BFP extended, RRE) LTXBR
*
*
* Also tests the following floating point support instructions
*   EXTRACT FPC 
*   LOAD  (Short)
*   LOAD  (Long)
*   LOAD ZERO (Long)
*   STORE (Short)
*   STORE (Long)
*   SET FPC
*
BFPCOMPS START 0
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
         ORG   BFPCOMPS+X'1A0' 
         DC    X'0000000180000000',AD(STARTTST)    z/Arch restart PSW
         ORG   BFPCOMPS+X'1D0' 
         DC    X'0002000000000000000000000000DEAD' z/Arch pgm chk new PSW
         ORG   BFPCOMPS+X'200'
*
STARTTST STCTL R0,R0,CTLR0    Store CR0 to enable AFP
         OI    CTLR0+1,X'04'  Turn on AFP bit
         LCTL  R0,R0,CTLR0    Reload updated CR0
         EFPC  R0             Save reference copy of FPC
         STG   R11,SAVER11    Save R11 in case we're on z/CMS
         BAS   R12,TESTCOMP   Perform compares and tests
         LG    R11,SAVER11    Restore R11 in case we're on z/CMS
         LTR   R14,R14        Any value in R14?
         BNZR  R14            ..yes, running on z/CMS, take graceful exit
         LPSWE WAITPSW        ..no, on bare iron, load disabled wait PSW
*
CTLR0    DS    F              Control register 0 (bit45 AFP control)
SAVER11  DS    D              Savearea for R11, needed when on z/CMS
WAITPSW  DC    X'00020000000000000000000000000000' Disabled PSW - normal completion
*
* BFP Compares, Load and Test, and Test Data Class main processing loop.
* All tests are performed in each iteration of the loop.
*
TESTCOMP DS    0H             Process one test case
         LA    R2,8           Set count of multiplication operations
         LA    R3,RESULTCC    Point cc table, 8 per precision per instruction
         LA    R4,RESULTFL    Point IEEE flags table, 8 per precision per instruction
         LA    R7,SHORTBFP    Point to start of short BFP input values
         LA    R8,TDCMASKS    Point to start of masks for TDC instruction
         LA    R9,LONGBFP     Point to start of long BFP input values
         LA    R10,EXTDBFP    Point to start of extended BFP input values
         BASR  R13,0          Set top of loop
*
         BAS   R11,SHORTS     Test shorts: CEB/CEBR, KEB/KEBR, LTEBR, and TCEB
         BAS   R11,LONGS      Test longs: CDB/CDBR, KDB/KDBR, LTDBR, and TCDB
         BAS   R11,EXTDS      Test extendeds: CXBR, KXBR, LTXBR, and TCXB
         LA    R3,1(0,R3)     point to next cc table entry
         LA    R4,1(0,R4)     Point to next mask table entry
         LA    R7,4(0,R7)     Point to next short BFP input value
         LA    R8,4(0,R8)     Point to next TDCxx mask pair
         LA    R9,8(0,R9)     Point to next long BFP input value
         LA    R10,16(0,R10)  Point to extended long BFP input value
         BCTR  R2,R13         Loop through all test cases
*
         BR    R12            Tests done, return to mainline
*
*     Next iteration of short BFP instruction testing; clear residuals from FP registers
*
SHORTS   LZDR  R0            Zero FPR0
         LZDR  R1            Zero FPR1
         LZDR  R13           Zero FPR13
         LE    R0,0(0,R7)    Get BFP short first value
         LE    R13,4(0,R7)   Get BFP short second value
         LER   R1,R0         Copy second value for CEBR RRE
*     Compare value pair using short RXE
         SFPC  R0            Load reference copy of FPC
         CEB   R0,4(0,R7)    Compare first and second values
         IPM   R1            Get condition code and program mask
         SRL   R1,28         Isolate CC in low order byte
         STC   R1,0(0,R3)    Save condition code in results table
         EFPC  R1            Get result FPC
         STCM  R1,B'0100',0(R4)  Store IEEE Flags in results table
*     Compare value pair using short RRE
         LFPC  R0            Load reference copy of FPC
         CEBR  R1,R13        Compare first and second values
         IPM   R1            Get condition code and program mask
         SRL   R1,28         Isolate CC in low order byte
         STC   R1,8(0,R3)    Save condition code in results table
         EFPC  R1            Get result FPC
         STCM  R1,B'0100',8(R4)  Store IEEE Flags in results table
*     Compare and Signal value pair using short RXE
         SFPC  R0            Load reference copy of FPC
         KEB   R0,4(0,R7)    Compare first and second values, signal if NaN
         IPM   R1            Get condition code and program mask
         SRL   R1,28         Isolate CC in low order byte
         STC   R1,40(0,R3)   Save condition code in results table
         EFPC  R1            Get result FPC
         STCM  R1,B'0100',40(R4)  Store IEEE Flags in results table
*     Compare and Signal value pair using short RRE
         LFPC  R0            Load reference copy of FPC
         KEBR  R1,R13        Compare first and second values, signal if NaN
         IPM   R1            Get condition code and program mask
         SRL   R1,28         Isolate CC in low order byte
         STC   R1,48(0,R3)   Save condition code in results table
         EFPC  R1            Get result FPC
         STCM  R1,B'0100',48(R4)  Store IEEE Flags in results table
*      Load and Test value short RRE
         LFPC  R0            Load reference copy of FPC
         LTEBR R12,R0        Load and test 
         STE   R12,LTEBRTST  Save tested value for SNaN->QNaN test (last iteration)
         IPM   R1            Get condition code and program mask
         SRL   R1,28         Isolate CC in low order byte
         STC   R1,80(0,R3)   Save condition code in results table
         EFPC  R1            Get result FPC
         STCM  R1,B'0100',80(R4)  Store IEEE Flags in results table
*     Test Data Class short RRE
         LFPC  R0            Load reference copy of FPC
         LH    R1,0(0,R8)    Get test data class mask value for True
         TCEB  R0,0(0,R1)    Test Data Class, set condition code
         IPM   R1            Get condition code and program mask
         SRL   R1,28         Isolate CC in low order byte
         STC   R1,104(0,R3)  Save condition code in results table
         LH    R1,2(0,R8)    Get test data class mask value for False
         TCEB  R0,0(0,R1)    Test Data Class, set condition code
         IPM   R1            Get condition code and program mask
         SRL   R1,28         Isolate CC in low order byte
         STC   R1,112(0,R3)  Save condition code in results table
*
         BR    R11           Tests done, return to loop control
*
*     Next iteration of long BFP instruction testing
*
LONGS    LD    R0,0(0,R9)    Load BFP long first value
         LD    R13,8(0,R9)   Load BFP long second value
         LDR   R1,R0         Copy second value for CDBR RRE
*     Compare value pair using long RXE
         SFPC  R0            Load reference copy of FPC
         CDB   R0,8(0,R9)    Compare first and second values
         IPM   R1            Get condition code and program mask
         SRL   R1,28         Isolate CC in low order byte
         STC   R1,16(0,R3)   Save condition code in results table
         EFPC  R1            Get result FPC
         STCM  R1,B'0100',16(R4)  Store IEEE Flags in results table
*     Compare value pair using long RRE
         LFPC  R0            Load reference copy of FPC
         CDBR  R1,R13        Compare first and second values
         IPM   R1            Get condition code and program mask
         SRL   R1,28         Isolate CC in low order byte
         STC   R1,24(0,R3)   Save condition code in results table
         EFPC  R1            Get result FPC
         STCM  R1,B'0100',24(R4)  Store IEEE Flags in results table
*     Compare and Signal value pair using long RXE
         SFPC  R0            Load reference copy of FPC
         KDB   R0,8(0,R9)    Compare first and second values
         IPM   R1            Get condition code and program mask
         SRL   R1,28         Isolate CC in low order byte
         STC   R1,56(0,R3)   Save condition code in results table
         EFPC  R1            Get result FPC
         STCM  R1,B'0100',56(R4)  Store IEEE Flags in results table
*      Compare and Signal value pair using long RRE
         LFPC  R0            Load reference copy of FPC
         KDBR  R1,R13        Generate RRE s*s=s product
         IPM   R1            Get condition code and program mask
         SRL   R1,28         Isolate CC in low order byte
         STC   R1,64(0,R3)   Save condition code in results table
         EFPC  R1            Get result FPC
         STCM  R1,B'0100',64(R4)  Store IEEE Flags in results table
*     Load and Test value long RRE
         LFPC  R0            Load reference copy of FPC
         LTDBR R12,R0        Load and test 
         STD   R12,LTDBRTST  Save tested value for SNaN->QNaN test (last iteration)
         IPM   R1            Get condition code and program mask
         SRL   R1,28         Isolate CC in low order byte
         STC   R1,88(0,R3)   Save condition code in results table
         EFPC  R1            Get result FPC
         STCM  R1,B'0100',88(R4)  Store IEEE Flags in results table
*     Test Data Class long RRE
         LFPC  R0            Load reference copy of FPC
         LH    R1,0(0,R8)    Get test data class mask value for True
         TCDB  R0,0(0,R1)    Test Data Class, set condition code
         IPM   R1            Get condition code and program mask
         SRL   R1,28         Isolate CC in low order byte
         STC   R1,120(0,R3)  Save condition code in results table
         LH    R1,2(0,R8)    Get test data class mask value for False
         TCDB  R0,0(0,R1)    Test Data Class, set condition code
         IPM   R1            Get condition code and program mask
         SRL   R1,28         Isolate CC in low order byte
         STC   R1,128(0,R3)  Save condition code in results table
*
         BR    R11           Tests done, return to loop control
*
*     Next iteration of long BFP instruction testing
*
EXTDS    LD    R0,0(0,R10)   Load BFP extended first value part 1
         LD    R2,8(0,R10)   Load BFP extended second value part 1
         LD    R13,16(0,R10) Load BFP extended first value part 1
         LD    R15,24(0,R10) Load BFP extended second value part 1
         LXR   R1,R0         Copy second value for CDBR RRE
*     Compare value pair using extended RRE
         LFPC  R0            Load reference copy of FPC
         CXBR  R1,R13        Compare first and second values
         IPM   R1            Get condition code and program mask
         SRL   R1,28         Isolate CC in low order byte
         STC   R1,32(0,R3)   Save condition code in results table
         EFPC  R1            Get result FPC
         STCM  R1,B'0100',32(R4)  Store IEEE Flags in results table
*      Compare and Signal value pair using extended RRE
         LFPC  R0            Load reference copy of FPC
         KXBR  R1,R13        Generate RRE s*s=s product
         IPM   R1            Get condition code and program mask
         SRL   R1,28         Isolate CC in low order byte
         STC   R1,72(0,R3)   Save condition code in results table
         EFPC  R1            Get result FPC
         STCM  R1,B'0100',72(R4)  Store IEEE Flags in results table
*     Load and Test value extended RRE
         LFPC  R0            Load reference copy of FPC
         LTXBR R12,R0        Load and test 
         STD   R12,LTXBRTST  Save tested value for SNaN->QNaN test (last iteration)
*  need 2nd half of store
         IPM   R1            Get condition code and program mask
         SRL   R1,28         Isolate CC in low order byte
         STC   R1,96(0,R3)   Save condition code in results table
         EFPC  R1            Get result FPC
         STCM  R1,B'0100',96(R4)  Store IEEE Flags in results table
*     Test Data Class extended RRE
         LFPC  R0            Load reference copy of FPC
         LH    R1,0(0,R8)    Get test data class mask value for True
         TCXB  R0,0(0,R1)    Test Data Class, set condition code
         IPM   R1            Get condition code and program mask
         SRL   R1,28         Isolate CC in low order byte
         STC   R1,136(0,R3)  Save condition code in results table
         LH    R1,2(0,R8)    Get test data class mask value for False
         TCXB  R0,0(0,R1)    Test Data Class, set condition code
         IPM   R1            Get condition code and program mask
         SRL   R1,28         Isolate CC in low order byte
         STC   R1,144(0,R3)  Save condition code in results table
*
         BR    R11           Tests done, return to loop control
*
*  Short BFP Input values.  QNaN,    1,    1,    2,  -inf,   -2,  +inf, SNaN,    0.
*
SHORTBFP DS    0F            Ensure fullword alignment for input table
         DC    X'FFC00000'      -QNaN
         DC    X'3F800000'      1 
         DC    X'3F800000'      1
         DC    X'40000000'      2
         DC    X'FF800000'      -infinity
         DC    X'807FFFFF'      -subnormal (maximum magnitude)
         DC    X'7F800000'      +infinity
         DC    X'7F810000'      +SNaN
         DC    X'00000000'      0
*
*  Long BFP Input values.  QNaN,    1,    1,    2,  -inf,   -2,  +inf, SNaN,    0.
*
LONGBFP  DS    0D
         DC    X'FFF8000000000000'       -QNaN
         DC    X'3FF0000000000000'      1 
         DC    X'3FF0000000000000'      1
         DC    X'4000000000000000'      2
         DC    X'FFF0000000000000'      -infinity
         DC    X'800FFFFFFFFFFFFF'      -subnormal (maximum magnitude)
         DC    X'7FF0000000000000'      +infinity
         DC    X'7FF0100000000000'      +SNaN
         DC    X'0000000000000000'      0
*
*  Extended BFP Input values.  QNaN,    1,    1,    2,  -inf,   -2,  +inf, SNaN,    0.
*
EXTDBFP  DS    0D
         DC    X'FFFF8000000000000000000000000000' -QNaN
         DC    X'3FFF0000000000000000000000000000' 1 
         DC    X'3FFF0000000000000000000000000000' 1
         DC    X'40000000000000000000000000000000' 2
         DC    X'FFFF0000000000000000000000000000' -infinity
         DC    X'8000FFFFFFFFFFFFFFFFFFFFFFFFFFFF' -subnormal (maximum magnitude)
         DC    X'7FFF0000000000000000000000000000' +infinity
         DC    X'7FFF0100000000000000000000000000' +SNaN
         DC    X'00000000000000000000000000000000' 0
*
* Test data class masks
* Bit meanings are: 0 0 0 0 | +z -z +norm -norm || +subn -sub +inf -inf | +QNaN -QNaN +SNaN -SNaN
*
TDCMASKS DS    0F
         DC    X'00040000'          0 0 0 0 | 0 0 0 0 || 0 0 0 0 | 0 1 0 0  -QNaN 
         DC    X'02000000'          0 0 0 0 | 0 0 1 0 || 0 0 0 0 | 0 0 0 0  +1
         DC    X'02000000'          0 0 0 0 | 0 0 1 0 || 0 0 0 0 | 0 0 0 0  +1
         DC    X'02000000'          0 0 0 0 | 0 0 1 0 || 0 0 0 0 | 0 0 0 0  +2
         DC    X'00100000'          0 0 0 0 | 0 0 0 0 || 0 0 0 1 | 0 0 0 0  -inf
         DC    X'00400000'          0 0 0 0 | 0 0 0 0 || 0 1 0 0 | 0 0 0 0  -2
         DC    X'00200000'          0 0 0 0 | 0 0 0 0 || 0 0 1 0 | 0 0 0 0  +inf
         DC    X'00020000'          0 0 0 0 | 0 0 0 0 || 0 0 0 0 | 0 0 2 0  +SNaN

         ORG   BFPCOMPS+X'700'
RESULTCC DS    19D      8 Condition codes from each of 19 instructions
*                       (  +0)          COMPARE short RXE results
*                       (  +8)          COMPARE short RRE results
*                       ( +16)          COMPARE long RXE results
*                       ( +24)          COMPARE long RRE results
*                       ( +32)          COMPARE extended RRE result
*
*                       ( +40)          COMPARE AND SIGNAL short RXE results
*                       ( +48)          COMPARE AND SIGNAL short RRE results
*                       ( +56)          COMPARE AND SIGNAL long RXE results
*                       ( +64)          COMPARE AND SIGNAL long RRE results
*                       ( +72)          COMPARE AND SIGNAL extended RRE results
*
*                       ( +80)          LOAD AND TEST short RRE results
*                       ( +88)          LOAD AND TEST long RRE results
*                       ( +96)          LOAD AND TEST extended RRE results
*
*                       (+104)          TEST DATA CLASS short RRE results (True results)
*                       (+112)          TEST DATA CLASS short RRE results (False results)
*                       (+120)          TEST DATA CLASS long RRE results (True results)
*                       (+128)          TEST DATA CLASS long RRE results (False results)
*                       (+136)          TEST DATA CLASS extended RRE results (True results)
*                       (+144)          TEST DATA CLASS extended RRE results (False results)
*
         ORG   BFPCOMPS+X'800'
RESULTFL DS    13D      8 IEEE flag bytes from each of 13 instructions
*                       (TEST DATA CLASS does not set IEEE flags)
*                       (  +0)          COMPARE short RXE results
*                       (  +8)          COMPARE short RRE results
*                       ( +16)          COMPARE long RXE results
*                       ( +24)          COMPARE long RRE results
*                       ( +32)          COMPARE extended RRE result
*
*                       ( +40)          COMPARE AND SIGNAL short RXE results
*                       ( +48)          COMPARE AND SIGNAL short RRE results
*                       ( +56)          COMPARE AND SIGNAL long RXE results
*                       ( +64)          COMPARE AND SIGNAL long RRE results
*                       ( +72)          COMPARE AND SIGNAL extended RRE results
*
*                       ( +80)          LOAD AND TEST short RRE results
*                       ( +88)          LOAD AND TEST long RRE results
*                       ( +96)          LOAD AND TEST extended RRE results
*
         ORG   BFPCOMPS+X'900'
LTXBRTST DS    2D   LOAD AND TEST Extended result (to test conversion of SNaN into QNaN)
LTDBRTST DS    D    LOAD AND TEST Long result (to test conversion of SNaN into QNaN)
LTEBRTST DS    F    LOAD AND TEST Short result (to test conversion of SNaN into QNaN)
         END

*Testcase IEEE Compare, Load and Test, Test Data Class
*Message Testcase ieee-comps.tst: IEEE Compare, Load and Test, Test Data Class
*Message ..Includes COMPARE (5), COMPARE AND SIGNAL (5), 
*Message ..         LOAD AND TEST (3), TEST DATA CLASS (3), 16 instructions total
*Message ..Test case values include infinities, a subnormal, a QNaN, and an SNaN

#
#  For the following tests, the IEEE exception masks in the FPC register are set
#  to zero, so no program interruptions.  The IEEE status flags are saved and set 
#  to zero after each test.  
#
#  1. COMPARE adjacent pairs of values in the input set (nine values means eight
#     comparisons).  Save condition code and FPC register IEEE flags byte (byte 1).
#
#  2. COMPARE AND SIGNAL adjacent pairs of values in the input set (nine values 
#     means eight comparisons).  Save condition code and FPC register IEEE flags 
#     byte (byte 1).
#
#  3. LOAD AND TEST the first eight values in the input set (the nineth value is
#     not tested).  Save condition code and FPC register IEEE flags byte (byte 1).
#     The result from the last LOAD AND TEST is saved to verify conversion of the
#     SNaN into a QNaN.  
#
#  4. TEST DATA CLASS the first eight values in the input set (the nineth value is
#     not tested) using each of two masks specific to the input value.  The first
#     mask will test true (cc=1)  and the second will test false (cc=0).  Save the
#     condition code; the IEEE flags are not changed by this instruction.
#
#     Test data:             QNaN,    1,    1,    2,  -inf, -SubN,  +inf, SNaN,    0.
#     Test data class masks:  080,  200,  200,  200,   010,  100,    020,  002.
#
#     Expected results in cc/flags format for COMPARE, COMPARE AND SIGNAL, and LOAD AND TEST
#     - COMPARE:            3/00  0/00  1/00  2/00  1/00  1/00  3/80  3/80
#     - COMPARE AND SIGNAL: 3/80  0/00  1/00  2/00  1/00  1/00  3/80  3/80
#     - LOAD AND TEST:      3/00  2/00  2/00  2/00  1/00  1/00  2/00  3/80
#
#     Expected results in cc1/cc2 format for TEST DATA CLASS
#     - TEST DATA CLASS:    1/0   1/0   1/0   1/0   1/0   1/0   1/0   1/0

#
# Tests 5 COMPARE, 5 COMPARE AND SIGNAL, 3 LOAD AND TEST, and 3 TEST DATA CLASS instructions
#   COMPARE (BFP short, RRE) CEBR
#   COMPARE (BFP short, RXE) CEB
#   COMPARE (BFP long, RRE) CDBR
#   COMPARE (BFP long, RXE) CDB
#   COMPARE (BFP extended, RRE) CXBR
#   COMPARE AND SIGNAL (BFP short, RRE) KEBR
#   COMPARE AND SIGNAL (BFP short, RXE) KEB
#   COMPARE AND SIGNAL (BFP long, RRE) KDBR
#   COMPARE AND SIGNAL (BFP long, RXE) KDB
#   COMPARE AND SIGNAL (BFP extended, RRE) KXBR
#   LOAD AND TEST (BFP short, RRE) LTEBR
#   LOAD AND TEST (BFP long, RRE) LTDBR
#   LOAD AND TEST (BFP extended, RRE) LTXBR
#   TEST DATA CLASS (BFP short, RRE) LTEBR
#   TEST DATA CLASS (BFP long, RRE) LTDBR
#   TEST DATA CLASS (BFP extended, RRE) LTXBR

#
# Also tests the following floating point support instructions
#   EXTRACT FPC 
#   LOAD  (Short)
#   LOAD  (Long)
#   LOAD ZERO (Long)
#   STORE (Short)
#   STORE (Long)
#   SET FPC
#
sysclear
archmode esame
r 1a0=00000001800000000000000000000200 # z/Arch restart PSW
r 1d0=0002000000000000000000000000DEAD # z/Arch pgm chk new PSW disabled wait

#
r 200=B60003D4     #STARTTST STCTL R0,R0,CTLR0    Store CR0 to enable AFP
r 204=960403D5     #         OI    CTLR0+1,X'04'  Turn on AFP bit
r 208=B70003D4     #         LCTL  R0,R0,CTLR0    Reload updated CR0
r 20C=B38C0000     #         EFPC  R0             Save reference copy of FPC
r 210=E3B003D80024 #         STG   R11,SAVER11    Save R11 in case we're on z/CMS
r 216=4DC00600     #         BAS   R12,TESTCOMP   Perform compares and tests
r 21A=E3B003D80004 #         LG    R11,SAVER11    Restore R11 in case we're on z/CMS
r 220=12EE         #         LTR   R14,R14        Any value in R14?
r 222=077E         #         BNZR  R14            ..yes, running on z/CMS, take graceful exit
r 224=B2B203E0     #         LPSWE WAITPSW        ..no, on bare iron, load disabled wait PSW

r 3D4=00040000     #CTLR0    DS    F             Control register 0 (bit45 AFP control)
# 3D8.8            #SAVER11  DS    D             Savearea for R11, needed when on z/CMS
r 3E0=00020000000000000000000000000000 # WAITPSW Disabled wait state PSW - normal completion


# BFP Compares, Load and Test, and Test Data Class main processing loop.
# All tests are performed in each iteration of the loop.

#                            ORG   X'600'
                   #TESTCASE DS    0H           Process one test case
r 600=41200008     #         LA    R2,8         Set count of multiplication operations
r 604=41300400     #         LA    R3,RESULTCC  Point cc table, 8 per precision per instruction
r 608=41400500     #         LA    R4,RESULTMS  Point mask table, 8 per precision per instruction
r 60C=41700300     #         LA    R7,SHORTBFP  Point to start of short BFP input values
r 610=41800330     #         LA    R8,TDCMASKS  Point to start of masks for TDC instruction
r 614=41900350     #         LA    R9,LONGBFP   Point to start of long BFP input values
r 618=41A00270     #         LA    R10,EXTDBFP  Point to start of extended BFP input values
r 61C=0DD0         #         BASR  R13,0        Set top of loop
r 61E=4DB00700     #         BAS   R11,SHORTS   Test shorts: CEB/CEBR, KEB/KEBR, LTEBR, and TCEBR
r 622=4DB00900     #         BAS   R11,LONGS    Test longs: CDB/CDBR, KDB/KDBR, LTDBR, and TCDBR
r 626=4DB00B00     #         BAS   R11,EXTDS    Test extendeds: CXBR, KXBR, LTXBR, and TCXBR
r 62A=41303001     #         LA    R3,1(,R3)    point to next cc table entry
r 62E=41404001     #         LA    R4,1(,R4)    Point to next mask table entry
r 632=41707004     #         LA    R7,4(,R7)    Point to next short BFP input value
r 636=41808004     #         LA    R8,4(,R8)    Point to next TDCxx mask pair
r 63A=41909008     #         LA    R9,8(,R9)    Point to next long BFP input value
r 63E=41A0A010     #         LA    R10,16(,R10) Point to extended long BFP input value

r 642=062D         #         BCTR  R2,R13       Loop through all test cases

r 644=07FC         #         BR    R12          Tests done, return to mainline


#     Next iteration of short BFP instruction testing; clear residuals from FP registers
#                            ORG   X'700'
r 700=B3750000     #SHORTS   LZDR  R0           Zero FPR0
r 704=B3750010     #         LZDR  R1           Zero FPR1
r 708=B37500D0     #         LZDR  R13          Zero FPR13
r 70C=78007000     #         LE    R0,0(,R7)    Get BFP short first value
r 710=78D07004     #         LE    R13,4(,R7)   Get BFP short second value
r 714=3810         #         LER   R1,R0        Copy second value for CEBR RRE
#     Compare value pair using short RXE
r 716=B3840000     #         SFPC  R0           Load reference copy of FPC
r 71A=ED0070040009 #         CEB   R0,4(,R7)    Compare first and second values
r 720=B2220010     #         IPM   R1           Get condition code and program mask
r 724=8810001C     #         SRL   R1,28        Isolate CC in low order byte
r 728=42103000     #         STC   R1,0(,R3)    Save condition code in results table
r 72C=B38C0010     #         EFPC  R1           Get result FPC
r 730=BE144000     #         STCM  R1,B'0100',0(,R4)  Store IEEE Flags in results table
#     Compare value pair using short RRE
r 734=B3840000     #         LFPC  R0           Load reference copy of FPC
r 738=B309001D     #         CEBR  R1,R13       Compare first and second values
r 73C=B2220010     #         IPM   R1           Get condition code and program mask
r 740=8810001C     #         SRL   R1,28        Isolate CC in low order byte
r 744=42103008     #         STC   R1,8(,R3)    Save condition code in results table
r 748=B38C0010     #         EFPC  R1           Get result FPC
r 74C=BE144008     #         STCM  R1,B'0100',8(,R4)  Store IEEE Flags in results table
#     Compare and Signal value pair using short RXE
r 750=B3840000     #         SFPC  R0           Load reference copy of FPC
r 754=ED0070040008 #         KEB   R0,4(,R7)    Compare first and second values, signal if NaN
r 75A=B2220010     #         IPM   R1           Get condition code and program mask
r 75E=8810001C     #         SRL   R1,28        Isolate CC in low order byte
r 762=42103028     #         STC   R1,40(,R3)   Save condition code in results table
r 766=B38C0010     #         EFPC  R1           Get result FPC
r 76A=BE144028     #         STCM  R1,B'0100',40(,R4)  Store IEEE Flags in results table
#     Compare and Signal value pair using short RRE
r 76E=B3840000     #         LFPC  R0           Load reference copy of FPC
r 772=B308001D     #         KEBR  R1,R13       Compare first and second values, signal if NaN
r 776=B2220010     #         IPM   R1           Get condition code and program mask
r 77A=8810001C     #         SRL   R1,28        Isolate CC in low order byte
r 77E=42103030     #         STC   R1,48(,R3)   Save condition code in results table
r 782=B38C0010     #         EFPC  R1           Get result FPC
r 786=BE144030     #         STCM  R1,B'0100',48(,R4)  Store IEEE Flags in results table
#      Load and Test value short RRE
r 78A=B3840000     #         LFPC  R0           Load reference copy of FPC
r 78E=B30200C0     #         LTEBR R12,R0       Load and test 
r 792=70C00570     #         STE   R12,LTEBRTST Save tested value for SNaN->QNaN test (last iteration)
r 796=B2220010     #         IPM   R1           Get condition code and program mask
r 79A=8810001C     #         SRL   R1,28        Isolate CC in low order byte
r 79E=42103050     #         STC   R1,80(,R3)   Save condition code in results table
r 7A2=B38C0010     #         EFPC  R1           Get result FPC
r 7A6=BE144050     #         STCM  R1,B'0100',80(,R4)  Store IEEE Flags in results table
#     Test Data Class short RRE
r 7AA=B3840000     #         LFPC  R0           Load reference copy of FPC
r 7AE=48108000     #         LH    R1,0(,R8)    Get test data class mask value for True
r 7B2=ED0010000010 #         TCEBR R0,0(,R1)    Test Data Class, set condition code
r 7B8=B2220010     #         IPM   R1           Get condition code and program mask
r 7BC=8810001C     #         SRL   R1,28        Isolate CC in low order byte
r 7C0=42103068     #         STC   R1,104(,R3)  Save condition code in results table
r 7C4=48108002     #         LH    R1,2(,R8)    Get test data class mask value for False
r 7C8=ED0010000010 #         TCEBR R0,0(,R1)    Test Data Class, set condition code
r 7CE=B2220010     #         IPM   R1           Get condition code and program mask
r 7D2=8810001C     #         SRL   R1,28        Isolate CC in low order byte
r 7D6=42103070     #         STC   R1,112(,R3)  Save condition code in results table
#
r 7DA=07FB         #         BR    R11          Tests done, return to loop control



#     Next iteration of long BFP instruction testing
#                            ORG   X'900'
r 900=68009000     #LONGS    LD    R0,0(,R9)    Load BFP long first value
r 904=68D09008     #         LD    R13,8(,R9)   Load BFP long second value
r 908=2810         #         LDR   R1,R0        Copy second value for CDBR RRE
#     Compare value pair using long RXE
r 90A=B3840000     #         SFPC  R0           Load reference copy of FPC
r 90E=ED0090080019 #         CDB   R0,8(,R9)    Compare first and second values
r 914=B2220010     #         IPM   R1           Get condition code and program mask
r 918=8810001C     #         SRL   R1,28        Isolate CC in low order byte
r 91C=42103010     #         STC   R1,16(,R3)   Save condition code in results table
r 920=B38C0010     #         EFPC  R1           Get result FPC
r 924=BE144010     #         STCM  R1,B'0100',16(,R4)  Store IEEE Flags in results table
#     Compare value pair using long RRE
r 928=B3840000     #         LFPC  R0           Load reference copy of FPC
r 92C=B319001D     #         CDBR  R1,R13       Compare first and second values
r 930=B2220010     #         IPM   R1           Get condition code and program mask
r 934=8810001C     #         SRL   R1,28        Isolate CC in low order byte
r 938=42103018     #         STC   R1,24(,R3)   Save condition code in results table
r 93C=B38C0010     #         EFPC  R1           Get result FPC
r 940=BE144018     #         STCM  R1,B'0100',24(,R4)  Store IEEE Flags in results table
#     Compare and Signal value pair using long RXE
r 944=B3840000     #         SFPC  R0           Load reference copy of FPC
r 948=ED0090080018 #         KDB   R0,8(,R9)    Compare first and second values
r 94E=B2220010     #         IPM   R1           Get condition code and program mask
r 952=8810001C     #         SRL   R1,28        Isolate CC in low order byte
r 956=42103038     #         STC   R1,56(,R3)   Save condition code in results table
r 95A=B38C0010     #         EFPC  R1           Get result FPC
r 95E=BE144038     #         STCM  R1,B'0100',56(,R4)  Store IEEE Flags in results table
#      Compare and Signal value pair using long RRE
r 962=B3840000     #         LFPC  R0           Load reference copy of FPC
r 966=B318001D     #         KDBR  R1,R13       Generate RRE s*s=s product
r 96A=B2220010     #         IPM   R1           Get condition code and program mask
r 96E=8810001C     #         SRL   R1,28        Isolate CC in low order byte
r 972=42103040     #         STC   R1,64(,R3)   Save condition code in results table
r 976=B38C0010     #         EFPC  R1           Get result FPC
r 97A=BE144040     #         STCM  R1,B'0100',64(,R4)  Store IEEE Flags in results table
#     Load and Test value long RRE
r 97E=B3840000     #         LFPC  R0           Load reference copy of FPC
r 982=B31200C0     #         LTDBR R12,R0       Load and test 
r 986=60C00578     #         STD   R12,LTDBRTST Save tested value for SNaN->QNaN test (last iteration)
r 98A=B2220010     #         IPM   R1           Get condition code and program mask
r 98E=8810001C     #         SRL   R1,28        Isolate CC in low order byte
r 992=42103058     #         STC   R1,88(,R3)   Save condition code in results table
r 996=B38C0010     #         EFPC  R1           Get result FPC
r 99A=BE144058     #         STCM  R1,B'0100',88(,R4)  Store IEEE Flags in results table
#     Test Data Class long RRE
r 99E=B3840000     #         LFPC  R0           Load reference copy of FPC
r 9A2=48108000     #         LH    R1,0(,R8)    Get test data class mask value for True
r 9A6=ED0010000011 #         TCDBR R0,0(,R1)    Test Data Class, set condition code
r 9AC=B2220010     #         IPM   R1           Get condition code and program mask
r 9B0=8810001C     #         SRL   R1,28        Isolate CC in low order byte
r 9B4=42103078     #         STC   R1,120(,R3)  Save condition code in results table
r 9B8=48108002     #         LH    R1,2(,R8)    Get test data class mask value for False
r 9BC=ED0010000011 #         TCDBR R0,0(,R1)    Test Data Class, set condition code
r 9C2=B2220010     #         IPM   R1           Get condition code and program mask
r 9C6=8810001C     #         SRL   R1,28        Isolate CC in low order byte
r 9CA=42103080     #         STC   R1,128(,R3)  Save condition code in results table
#
r 9CE=07FB         #         BR    R11          Tests done, return to loop control


#     Next iteration of long BFP instruction testing
#                            ORG   X'900'
r B00=6800A000     #EXTDS    LD    R0,0(,R10)   Load BFP extended first value part 1
r B04=6820A008     #         LD    R2,8(,R10)   Load BFP extended second value part 1
r B08=68D0A010     #         LD    R13,16(,R10) Load BFP extended first value part 1
r B0C=68F0A018     #         LD    R15,24(,R10) Load BFP extended second value part 1
r B10=B3650010     #         LXR   R1,R0        Copy second value for CDBR RRE
#     Compare value pair using extended RRE
r B14=B3840000     #         LFPC  R0           Load reference copy of FPC
r B18=B349001D     #         CXBR  R1,R13       Compare first and second values
r B1C=B2220010     #         IPM   R1           Get condition code and program mask
r B20=8810001C     #         SRL   R1,28        Isolate CC in low order byte
r B24=42103020     #         STC   R1,32(,R3)   Save condition code in results table
r B28=B38C0010     #         EFPC  R1           Get result FPC
r B2C=BE144020     #         STCM  R1,B'0100',32(,R4)  Store IEEE Flags in results table
#      Compare and Signal value pair using extended RRE
r B30=B3840000     #         LFPC  R0           Load reference copy of FPC
r B34=B348001D     #         KXBR  R1,R13       Generate RRE s*s=s product
r B38=B2220010     #         IPM   R1           Get condition code and program mask
r B3C=8810001C     #         SRL   R1,28        Isolate CC in low order byte
r B40=42103048     #         STC   R1,72(,R3)   Save condition code in results table
r B44=B38C0010     #         EFPC  R1           Get result FPC
r B48=BE144048     #         STCM  R1,B'0100',72(,R4)  Store IEEE Flags in results table
#     Load and Test value extended RRE
r B4C=B3840000     #         LFPC  R0           Load reference copy of FPC
r B50=B34200C0     #         LTXBR R12,R0       Load and test 
r B54=60C00580     #         STD   R12,LTXBRTST Save tested value for SNaN->QNaN test (last iteration)
#  need 2nd half of store
r B58=B2220010     #         IPM   R1           Get condition code and program mask
r B5C=8810001C     #         SRL   R1,28        Isolate CC in low order byte
r B60=42103060     #         STC   R1,96(,R3)   Save condition code in results table
r B64=B38C0010     #         EFPC  R1           Get result FPC
r B68=BE144060     #         STCM  R1,B'0100',96(,R4)  Store IEEE Flags in results table
#     Test Data Class extended RRE
r B6C=B3840000     #         LFPC  R0           Load reference copy of FPC
r B70=48108000     #         LH    R1,0(,R8)    Get test data class mask value for True
r B74=ED0010000012 #         TCXBR R0,0(,R1)    Test Data Class, set condition code
r B7A=B2220010     #         IPM   R1           Get condition code and program mask
r B7E=8810001C     #         SRL   R1,28        Isolate CC in low order byte
r B82=42103088     #         STC   R1,136(,R3)  Save condition code in results table
r B86=48108002     #         LH    R1,2(,R8)    Get test data class mask value for False
r B8A=ED0010000012 #         TCXBR R0,0(,R1)    Test Data Class, set condition code
r B90=B2220010     #         IPM   R1           Get condition code and program mask
r B94=8810001C     #         SRL   R1,28        Isolate CC in low order byte
r B98=42103090     #         STC   R1,144(,R3)  Save condition code in results table
#
r B9C=07FB         #         BR    R11          Tests done, return to loop control


#  Short BFP Input values.  QNaN,    1,    1,    2,  -inf,   -2,  +inf, SNaN,    0.
                   # SHORTBFP  DC    9F
r 300=FFC00000     # -QNaN
r 304=3F800000     # 1 
r 308=3F800000     # 1
r 30C=40000000     # 2
r 310=FF800000     # -infinity
r 314=807FFFFF     # -subnormal (maximum magnitude)
r 318=7F800000     # +infinity
r 31C=7F810000     # +SNaN
r 320=00000000     # 0

#  Long BFP Input values.  QNaN,    1,    1,    2,  -inf,   -2,  +inf, SNaN,    0.
                   # LONGBFP  DC    9D
r 350=FFF8000000000000     # -QNaN
r 358=3FF0000000000000     # 1 
r 360=3FF0000000000000     # 1
r 368=4000000000000000     # 2
r 370=FFF0000000000000     # -infinity
r 378=800FFFFFFFFFFFFF     # -subnormal (maximum magnitude)
r 380=7FF0000000000000     # +infinity
r 388=7FF0100000000000     # +SNaN
r 390=0000000000000000     # 0

#  Extended BFP Input values.  QNaN,    1,    1,    2,  -inf,   -2,  +inf, SNaN,    0.
                   # EXTDBFP   DC    18D
r 270=FFFF8000000000000000000000000000 -QNaN
r 280=3FFF0000000000000000000000000000 1 
r 290=3FFF0000000000000000000000000000 1
r 2A0=40000000000000000000000000000000 2
r 2B0=FFFF0000000000000000000000000000 -infinity
r 2C0=8000FFFFFFFFFFFFFFFFFFFFFFFFFFFF -subnormal (maximum magnitude)
r 2D0=7FFF0000000000000000000000000000 +infinity
r 2E0=7FFF0100000000000000000000000000 +SNaN
r 2F0=00000000000000000000000000000000 0




# Test data class masks
#                        0 0 0 0 | +z -z +norm -norm || +subn -sub +inf -inf | +QNaN -QNaN +SNaN -SNaN
r 330=00040000     #     0 0 0 0 | 0 0 0 0 || 0 0 0 0 | 0 1 0 0  -QNaN 
r 334=02000000     #     0 0 0 0 | 0 0 1 0 || 0 0 0 0 | 0 0 0 0  +1
r 338=02000000     #     0 0 0 0 | 0 0 1 0 || 0 0 0 0 | 0 0 0 0  +1
r 33C=02000000     #     0 0 0 0 | 0 0 1 0 || 0 0 0 0 | 0 0 0 0  +2
r 340=00100000     #     0 0 0 0 | 0 0 0 0 || 0 0 0 1 | 0 0 0 0  -inf
r 344=00400000     #     0 0 0 0 | 0 0 0 0 || 0 1 0 0 | 0 0 0 0  -2
r 348=00200000     #     0 0 0 0 | 0 0 0 0 || 0 0 1 0 | 0 0 0 0  +inf
r 34C=00020000     #     0 0 0 0 | 0 0 0 0 || 0 0 0 0 | 0 0 2 0  +SNaN


#                    RESULTCC: 8 Condition codes from each of 16 instructions
# 400.8  (  +0)          COMPARE short RXE results
# 408.8  (  +8)          COMPARE short RRE results
# 410.8  ( +16)          COMPARE long RXE results
# 418.8  ( +24)          COMPARE long RRE results
# 420.8  ( +32)          COMPARE extended RRE result
#
# 428.8  ( +40)          COMPARE AND SIGNAL short RXE results
# 430.8  ( +48)          COMPARE AND SIGNAL short RRE results
# 438.8  ( +56)          COMPARE AND SIGNAL long RXE results
# 440.8  ( +64)          COMPARE AND SIGNAL long RRE results
# 448.8  ( +72)          COMPARE AND SIGNAL extended RRE results
#
# 450.8  ( +80)          LOAD AND TEST short RRE results
# 458.8  ( +88)          LOAD AND TEST long RRE results
# 460.8  ( +96)          LOAD AND TEST extended RRE results
#
# 468.8  (+104)          TEST DATA CLASS short RRE results (True results)
# 470.8  (+112)          TEST DATA CLASS short RRE results (False results)
# 478.8  (+120)          TEST DATA CLASS long RRE results (True results)
# 480.8  (+128)          TEST DATA CLASS long RRE results (False results)
# 488.8  (+136)          TEST DATA CLASS extended RRE results (True results)
# 490.8  (+144)          TEST DATA CLASS extended RRE results (False results)

#                    RESULTCC: 8 IEEE flag bytes from each of 13 instructions
#                    (TEST DATA CLASS does not set IEEE flags)
# 500.8  (  +0)          COMPARE short RXE results
# 508.8  (  +8)          COMPARE short RRE results
# 510.8  ( +16)          COMPARE long RXE results
# 518.8  ( +24)          COMPARE long RRE results
# 520.8  ( +32)          COMPARE extended RRE result
#
# 528.8  ( +40)          COMPARE AND SIGNAL short RXE results
# 530.8  ( +48)          COMPARE AND SIGNAL short RRE results
# 538.8  ( +56)          COMPARE AND SIGNAL long RXE results
# 540.8  ( +64)          COMPARE AND SIGNAL long RRE results
# 548.8  ( +72)          COMPARE AND SIGNAL extended RRE results
#
# 550.8  ( +80)          LOAD AND TEST short RRE results
# 558.8  ( +88)          LOAD AND TEST long RRE results
# 560.8  ( +96)          LOAD AND TEST extended RRE results

# 570.4                  LTEBRTST  DS F    LOAD AND TEST Short result (to test conversion of SNaN into QNaN)
# 578.8                  LTDBRTST  DS D    LOAD AND TEST Long result (to test conversion of SNaN into QNaN)
# 580.10                 LTXBRTST: DS 2D   LOAD AND TEST Extended result (to test conversion of SNaN into QNaN)

runtest 

r 400.98
r 500.90

# Short instruction expected results

*Compare
r 400.8  # BFP Short Compare RXE condition codes, expecting 3 0 1 2 1 1 3 3 
*Want "CEB cc" 03000102 01010303
*Compare
r 500.8  # BFP Short Compare RXE IEEE Flags, expecting 00, 00, 00, 00, 00, 80, 80
*Want "CEB IEEE Flags" 00000000 00008080

*Compare
r 408.8  # BFP Short Compare RRE condition codes, expecting 3 0 1 2 1 1 3 3 
*Want "CEBR cc" 03000102 01010303
*Compare
r 508.8  # BFP Short Compare RRE IEEE Flags, expecting 00, 00, 00, 00, 00, 80, 80
*Want "CEBR IEEE Flags" 00000000 00008080

*Compare
r 428.8  # BFP Short Compare and Signal RXE condition codes, expecting 3 0 1 2 1 1 3 3 
*Want "KEB cc" 03000102 01010303
*Compare
r 528.8  # BFP Short Compare and Signal RXE IEEE Flags, expecting 80, 00, 00, 00, 00, 80, 80
*Want "KEB IEEE Flags" 80000000 00008080

*Compare
r 430.8  # BFP Short Compare and Signal RRE condition codes, expecting 3 0 1 2 1 1 3 3 
*Want "KEBR cc" 03000102 01010303
*Compare
r 530.8  # BFP Short Compare and Signal RRE IEEE Flags, expecting 80, 00, 00, 00, 00, 80, 80
*Want "KEBR IEEE Flags" 80000000 00008080

*Compare
r 450.8  # BFP Short Load and Test RRE condition codes, expecting 3 2 2 2 1 1 2 3 
*Want "LTEBR cc" 03020202 01010203
*Compare
r 550.8  # BFP Short Load and Test RRE IEEE Flags, expecting 00, 00, 00, 00, 00, 00, 80
*Want "LTEBR IEEE Flags" 00000000 00000080
*Compare
r 570.4  # BFP Short Load and Test RRE SNaN->QNaN, expecting 7FC00001
*Want "LTEBR SNaN->QNaN" 7FC10000

*Compare
r 468.8  # BFP Short Test Data Class condition codes, expecting 1 1 1 1 1 1 1 1
*Want "TCEBR cc 1/2" 01010101 01010101
*Compare
r 470.8  # BFP Short Test Data Class condition codes, expecting 0 0 0 0 0 0 0 0
*Want "TCEBR cc 2/2" 00000000 00000000


# Long instruction expected results  (same as the shorts except for the QNaN)

*Compare
r 410.8  # BFP Long Compare RXE condition codes, expecting 3 0 1 2 1 1 3 3 
*Want "CDB cc" 03000102 01010303
*Compare
r 510.8  # BFP Long Compare RXE IEEE Flags, expecting 00, 00, 00, 00, 00, 80, 80
*Want "CDB IEEE Flags" 00000000 00008080

*Compare
r 418.8  # BFP Long Compare RRE condition codes, expecting 3 0 1 2 1 1 3 3 
*Want "CDBR cc" 03000102 01010303
*Compare
r 518.8  # BFP Long Compare RRE IEEE Flags, expecting 00, 00, 00, 00, 00, 80, 80
*Want "CDBR IEEE Flags" 00000000 00008080

*Compare
r 438.8  # BFP Long Compare and Signal RXE condition codes, expecting 3 0 1 2 1 1 3 3 
*Want "KDB cc" 03000102 01010303
*Compare
r 538.8  # BFP Long Compare and Signal RXE IEEE Flags, expecting 00, 00, 00, 00, 00, 80, 80
*Want "KDB IEEE Flags" 80000000 00008080

*Compare
r 440.8  # BFP Long Compare and Signal RRE condition codes, expecting 3 0 1 2 1 1 3 3 
*Want "KDBR cc" 03000102 01010303
*Compare
r 540.8  # BFP Long Compare and Signal RRE IEEE Flags, expecting 80, 00, 00, 00, 00, 80, 80
*Want "KDBR IEEE Flags" 80000000 00008080

*Compare
r 458.8  # BFP Long Load and Test RRE condition codes, expecting 3 2 2 2 1 1 2 3 
*Want "LTDBR cc" 03020202 01010203
*Compare
r 558.8  # BFP Long Load and Test RRE IEEE Flags, expecting 00, 00, 00, 00, 00, 00, 80
*Want "LTDBR IEEE Flags" 00000000 00000080
*Compare
r 578.8  # BFP Long Load and Test RRE SNaN->QNaN, expecting 7FC00001
*Want "LTDBR SNaN->QNaN" 7FF81000 00000000 

*Compare
r 478.8  # BFP Long Test Data Class condition codes, expecting 1 1 1 1 1 1 1 1
*Want "TCDBR cc 1/2" 01010101 01010101
*Compare
r 480.8  # BFP Long Test Data Class condition codes, expecting 0 0 0 0 0 0 0 0
*Want "TCDBR cc 2/2" 00000000 00000000



# Extended instruction expected results  (same as the shorts except for the QNaN)

*Compare
r 420.8  # BFP extended Compare RRE condition codes, expecting 3 0 1 2 1 1 3 3 
*Want "CXBR cc" 03000102 01010303
*Compare
r 520.8  # BFP extended Compare RRE IEEE Flags, expecting 00, 00, 00, 00, 00, 80, 80
*Want "CXBR IEEE Flags" 00000000 00008080

*Compare
r 448.8  # BFP extended Compare and Signal RRE condition codes, expecting 3 0 1 2 1 1 3 3 
*Want "KXBR cc" 03000102 01010303
*Compare
r 548.8  # BFP extended Compare and Signal RRE IEEE Flags, expecting 80, 00, 00, 00, 00, 80, 80
*Want "KXBR IEEE Flags" 80000000 00008080

*Compare
r 460.8  # BFP extended Load and Test RRE condition codes, expecting 3 2 2 2 1 1 2 3 
*Want "LTXBR cc" 03020202 01010203
*Compare
r 560.8  # BFP extended Load and Test RRE IEEE Flags, expecting 00, 00, 00, 00, 00, 00, 80
*Want "LTXBR IEEE Flags" 00000000 00000080
*Compare
r 580.10  # BFP extended Load and Test RRE SNaN->QNaN, expecting 7FC00001
*Want "LTXBR SNaN->QNaN" 7FFF8100 00000000 00000000 00000000

*Compare
r 488.8  # BFP extended Test Data Class condition codes, expecting 1 1 1 1 1 1 1 1
*Want "TCXBR cc 1/2" 01010101 01010101
*Compare
r 490.8  # BFP extended Test Data Class condition codes, expecting 0 0 0 0 0 0 0 0
*Want "TCXBR cc 2/2" 00000000 00000000


*Done

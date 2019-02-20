  TITLE 'bfp-025-b2h.asm: Test Convert BFP to HFP'
***********************************************************************
*
*Testcase CONVERT BFP TO HFP
*  Test case capability includes verifying the condition code for
*  sundry result cases.  CONVERT BFP TO HFP does not generate IEEE
*  execptions or traps, and does not alter the Floating Point Control
*  Register (FPC).  
*
***********************************************************************
         SPACE 2
***********************************************************************
*
*                        bfp-025-b2h.asm
*
*        This assembly-language source file is part of the
*        Hercules Binary Floating Point Validation Package 
*                        by Stephen R. Orso
*
* Copyright 2019 by Stephen R Orso.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
*
* 1. Redistributions of source code must retain the above copyright
*    notice, this list of conditions and the following disclaimer.
*
* 2. Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in
*    the documentation and/or other materials provided  with the
*    distribution.
*
* 3. The name of the author may not be used to endorse or promote
*    products derived from this software without specific prior written
*    permission.
*
* DISCLAMER: THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
* THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
* PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT
* HOLDER BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
* EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
* PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
* PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
* OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
***********************************************************************
         SPACE 2
***********************************************************************
*
*
*Testcase CONVERT BFP TO HFP
*  Test case capability includes verifying the condition code for
*  sundry result cases.  CONVERT BFP TO HFP does not generate IEEE
*  execptions or traps and does not reference or alter the Floating 
* Point Control Register (FPC). 
*
* Tests the following two conversion instructions
*   CONVERT BFP TO HFP (long BFP to long HFP, RRF-e)
*   CONVERT BFP TO HFP (short BFP to long HFP, RRF-e) 
*
* Both conversions above are exact; there is no rounding needed.  
*
* Test data is compiled into this program.  The test script that runs
* this program can provide alternative test data through Hercules R 
* commands.
* 
* Test Case Order
* 1) Short BFP to long HFP functional tests.  Input values include
*    finite values, subnormal finite values, +/-infinity, and NaNs.
* 1) Long BFP to long HFP functional tests.  Input values include
*    finite values, values that over/underflow long HFP, +/-infinity, 
*    and NaNs.
*
* Note: Subnormal inputs are treated as having an exponent one lower 
* than the minimum exponent of a normalized input, or -127 for short
* BFP and -1023 for long BFP.  While a binary exponent of -127 can
* be converted into an HFP biased characteristic that fits in 7 bits,
* the same is not true for -1023.  Input values are included that 
* test this condition.  
*
* Overflow and underflow are not possible when the source is short BFP.
*
* Test cases are needed as follows:
*   0, +1.5, -1.5
*   +/- Subnormal inputs
*   +/- infinity, QNaN, and SNaN
*
* Two input test data sets are provided, one for short BFP to long 
*   HFP and one for long BFP to long HFP.  
*
* Also tests the following floating point support instructions
*   LOAD  (Short)
*   LOAD  (Long)
*   LFPC  (Load Floating Point Control Register)
*   SRNMB (Set BFP Rounding Mode 3-bit)
*   STFPC (Store Floating Point Control Register)
*   STORE (Short)
*   STORE (Long)
*
         MACRO
         PADCSECT &ENDLABL
         AIF   ('&SYSASM' EQ 'A SMALL MAINFRAME ASSEMBLER').NOPAD
         AGO   .GOODPAD
         AIF   (D'&ENDLABL).GOODPAD
         MNOTE 4,'Missing or invalid CSECT padding label ''&ENDLABL'''
         MNOTE *,'No CSECT padding performed'  
         MEXIT
.GOODPAD ANOP
         ORG   &ENDLABL-1
         MEXIT
.NOPAD   ANOP
         MNOTE *,'asma detected; no CSECT padding performed'  
         MEND
*
BFPB2H   START 0
R0       EQU   0            Work register for condition code retrieval
R1       EQU   1
R2       EQU   2            Loop control for instruction test routines
R3       EQU   3            Address of next input test value
R4       EQU   4
R5       EQU   5
R6       EQU   6
R7       EQU   7            Address of next result storage
R8       EQU   8            Address of next FPCR storage
R9       EQU   9
R10      EQU   10           Address of 4-word test parameter array
R11      EQU   11
R12      EQU   12           Top of loop for instruction test routines
R13      EQU   13           Return address within main test driver
R14      EQU   14           CMS Test rig return address or zero
R15      EQU   15           Base address (CMS) or zero (bare iron)
*
* Floating Point Register equates to keep the cross reference clean
*
FPR0     EQU   0            Input value (operand 2) for THDEBR/THDR
FPR1     EQU   1            Output value (operand 1) for THDEBR/THDR
FPR2     EQU   2
FPR3     EQU   3
FPR4     EQU   4
FPR5     EQU   5
FPR6     EQU   6
FPR7     EQU   7
FPR8     EQU   8
FPR9     EQU   9
FPR10    EQU   10
FPR11    EQU   11
FPR12    EQU   12
FPR13    EQU   13
FPR14    EQU   14
FPR15    EQU   15
*
* Control Register equates (only one used for AFP bit 44)
*
CR0      EQU   0            Contains advanced floating point bit 44
*
         USING *,R15
*
* Above USING works on real iron (R15=0 after sysclear) 
* and in z/CMS (R15 points to start of load module)
*
         ORG   BFPB2H+X'8E'   Program check interrution code
PCINTCD  DS    H
*
PCOLDPSW EQU   BFPB2H+X'150'  z/Arch Program check old PSW
*
         ORG   BFPB2H+X'1A0'  z/Arch Restart PSW
         DC    X'0000000180000000',AD(START)   
*
         ORG   BFPB2H+X'1D0'  z/Arch Program check old PSW
         DC    X'0000000000000000',AD(PROGCHK)
* 
* Program check routine.  If Data Exception, continue execution at
* the instruction following the program check.  Otherwise, hard wait.
* No need to collect data.  All interesting DXC stuff is captured
* in the FPCR.
*
* Note that CONVERT HFP TO BFP is not expected to program check in 
* this test case.  One could consider including the AFP register test,
* which should generate a program check.  
*
         ORG   BFPB2H+X'200'
PROGCHK  DS    0H             Program check occured...
         CLI   PCINTCD+1,X'07'  Data Exception?
         JNE   PCNOTDTA       ..no, hardwait (not sure if R15 is ok)
         LPSWE PCOLDPSW       ..yes, resume program execution
PCNOTDTA DS    0H
         LTR   R14,R14        Return address provided?
         BNZR  R14            Yes, return to z/CMS test rig.  
         LPSWE HARDWAIT       Not data exception, enter disabled wait
         EJECT
***********************************************************************
*
*  Main program.  Enable Advanced Floating Point, process test cases.
*         
START    STCTL CR0,CR0,CTLR0  Store CR0 to enable AFP
         OI    CTLR0+1,X'04'  Turn on AFP bit
*                             ..(Not strictly required; we could write
*                             ..(the test case to use FPR0/2/4/6)
         LCTL  CR0,CR0,CTLR0  Reload updated CR0
*
* Short BFP to long HFP tests
*
         LA    R10,STOHBAS    Short BFP test inputs
         BAS   R13,THDER      Convert short BFP to long HFP
*
* Long BFP to long HFP tests.  
*
         LA    R10,LTOHBAS    Long SFP test inputs
         BAS   R13,THDR       Convert long BFP to long HFP
*
* Two test sets run.  All done.
*
         LTR   R14,R14        Return address provided?
         BNZR  R14            ..Yes, return to z/CMS test rig.  
         LPSWE WAITPSW        ..No, load disabled wait PSW
*
         DS    0D             Ensure correct alignment for psw
WAITPSW  DC    X'0002000000000000',AD(0)  Normal end - disabled wait
HARDWAIT DC    X'0002000000000000',XL6'00',X'DEAD' Abnormal end
*
CTLR0    DS    F
FPCREGNT DC    X'00000000'  FPCR, trap all IEEE exceptions, zero flags
FPCREGTR DC    X'F8000000'  FPCR, trap no IEEE exceptions, zero flags
*
* Input values parameter list, four fullwords: 
*      1) Count, 
*      2) Address of inputs, 
*      3) Address to place results, and
*      4) Address to place DXC/Flags/cc values. 
*
         ORG   BFPB2H+X'300'
STOHBAS  DS    0F           Inputs for short BFP->HFP basic tests
         DC    A(STOHCT/4)
         DC    A(STOHIN)
         DC    A(STOHOUT)
         DC    A(STOHFLGS)
*
LTOHBAS  DS    0F           Inputs for long BFP->HFP basic tests
         DC    A(LTOHCT/8)
         DC    A(LTOHIN)
         DC    A(LTOHOUT)
         DC    A(LTOHFLGS)
*
         EJECT
***********************************************************************
*
* Convert short BFP to long HFP.  One result is generated for each
* input with all exceptions trappable.  Note: this instruction should
* not trap.  The FPCR contents and condition code are stored for each
* input value.  Overflow is not possible.  
*
***********************************************************************
         SPACE 2
THDER    LM    R2,R3,0(R10)  Get count and address of test input values
         LM    R7,R8,8(R10)  Get address of result area and flag area.
         LTR   R2,R2         Any test cases?
         BZR   R13           ..No, return to caller
         BASR  R12,0         Set top of loop
*
         LZDR  FPR1          Zero FRP1 to clear any residual
         LE    FPR0,0(,R3)   Get short BFP test value
         LFPC  FPCREGTR      Set exceptions trappable
         THDER FPR1,FPR0     Cvt short BFP in FPR0 to long HFP in FPR1
         STD   FPR1,0(,R7)   Store long HFP result
         STFPC 0(R8)         Store resulting FPCR flags and DXC
         IPM   R0            Get condition code and program mask
         SRL   R0,28         Isolate CC in low order byte
         STC   R0,3(,R8)     Save condition code in results table
*
         LA    R3,4(,R3)     Point to next input value
         LA    R7,8(,R7)     Point to next result
         LA    R8,4(,R8)     Point to next FPCR result area
         BCTR  R2,R12        Convert next input value.  
         BR    R13           All converted; return.
         EJECT
***********************************************************************
*
* Convert long BFP to long HFP.  One result is generated for each
* input with all exceptions trappable.  Note: this instruction should
* not trap.  The FPCR contents and condition code are stored for each
* input value.  Overflow and underflow are possible but are not 
* reported as traps with a DXC code or in the FPCR.  Overflow returns
* +/-Hmax and underflow returns +/-0.  
*
***********************************************************************
         SPACE 2
THDR     LM    R2,R3,0(R10)  Get count and address of test input values
         LM    R7,R8,8(R10)  Get address of result area and flag area.
         LTR   R2,R2         Any test cases?
         BZR   R13           ..No, return to caller
         BASR  R12,0         Set top of loop
*
         LZDR  FPR1          Zero FRP1 to clear any residual
         LD    FPR0,0(,R3)   Get long BFP test value
         LFPC  FPCREGTR      Set exceptions trappable
         THDR  FPR1,FPR0     Cvt long BFP in FPR0 to long HFP in FPR1
         STD   FPR1,0(,R7)   Store long HFP result
         STFPC 0(R8)         Store resulting FPCR flags and DXC
         IPM   R0            Get condition code and program mask
         SRL   R0,28         Isolate CC in low order byte
         STC   R0,3(,R8)     Save condition code in results table
*
         LA    R3,8(,R3)     Point to next input value
         LA    R7,8(,R7)     Point to next result
         LA    R8,4(,R8)     Point to next FPCR result area
         BCTR  R2,R12        Convert next input value.  
         BR    R13           All converted; return.
         EJECT
***********************************************************************
*
* BFP inputs.  One set each of short and long BFP values are included.
* used for both short BFP and long BFP results.  
*
***********************************************************************
          SPACE 2
*
* Short Binary Floating Point doubleword breakdown
*
*              x'80000000'          sign bit
*              x'7F800000'          8-bit exponent, biased +126
*              x'007FFFFF'          23-bit signficand, plus an 
*              x'00800000'          ..implicit MSB that is not stored
*
STOHIN   DS    0D        Inputs for short BFP to long HFP tests
         DC    X'00000000'          positive zero
         DC    X'80000000'          negative zero
* 
* Test normalization with varying numbers of required leading zero 
* bits in the leading digit of the HFP representation of the fraction.
*
         DC    X'3FC00000'          +1.5
         DC    X'BFC00000'          -1.5
         DC    X'40200000'          +2.5
         DC    X'C0200000'          -2.5
         DC    X'40900000'          +4.5
         DC    X'C0900000'          -4.5
         DC    X'41080000'          +8.5
         DC    X'C1080000'          -8.5
*
* Test subnormals.  These should convert to valid long HFP results.
* This test includes +/-Dmax, +/-Dmin, and their neighbors.
*
         DC    X'00000001'          +Dmin
         DC    X'80000001'          -Dmin
*
         DC    X'00400000'          +Dmax
         DC    X'80400000'          -Dmax
*
* Test +/-Nmax and near Nmax.  These should convert to valid long HFP
* results.  +/-Dmin was tested above as part of subnormal testing.  
* Note that neither +/-Hmax nor +/-Hmin fit in a short BFP, so there
* is no test for them.  
*
         DC    X'7F7FFFFF'          +Nmax
         DC    X'FF7FFFFF'          -Nmax
*
         DC    X'7F7FFFFE'          A teensy bit smaller than +Nmax
         DC    X'FF7FFFFE'          A teensy bit smaller than -Nmax
*
* Test +/-Nmin.  +/-Dmin was tested above.
*
         DC    X'00800000'          +Nmin
         DC    X'80800000'          -Nmin
*
* Test SNaNs, QNaNs, and infinities.  NaNs should return +Hmax, and 
* infinities should return Hmax with the sign of the input infinity.  
*
         DC    X'7F800000'          +infinity
         DC    X'FF800000'          -infinity
*
         DC    X'7FC00000'          +QNaN with no payload
         DC    X'FFC00000'          -QNaN with no payload
*
         DC    X'7F800008'          +SNaN with payload 0x00008
         DC    X'FF800004'          -SNaN with payload 0x00004
*
STOHCT   EQU   *-STOHIN     Count of short BFP in list * 4
         SPACE 3
*
* Long Binary Floating Point doubleword breakdown
*
*              x'8000000000000000'  sign bit
*              x'7FF0000000000000'  11-bit exponent, biased +1023
*              x'000FFFFFFFFFFFFF'  52-bit significand, plus an
*              x'0080000000000000'  ..implicit MSB that is not stored
*
LTOHIN   DS    0D        Inputs for long BFP to long HFP tests
         DC    X'0000000000000000'  positive zero
         DC    X'8000000000000000'  negative zero
* 
* Test normalization with varying numbers of required leading zero 
* bits in the leading digit of the HFP representation of the fraction.
*
         DC    X'3FF8000000000000'  +1.5
         DC    X'BFF8000000000000'  -1.5
         DC    X'4004000000000000'  +2.5
         DC    X'C004000000000000'  -2.5
         DC    X'4012000000000000'  +4.5
         DC    X'C012000000000000'  -4.5
         DC    X'4021000000000000'  +8.5
         DC    X'C021000000000000'  -8.5
*
* Test +/-Hmax and nearby values represented as long BFP inputs.  Hmax
* and smaller magnitude should convert, and larger should overflow to
* a signed Hmax.
*
         DC    X'4FAFFFFFFFFFFFFF'  +Hmax in long BFP
         DC    X'CFAFFFFFFFFFFFFF'  -Hmax in long BFP
*
         DC    X'4FAFFFFFFFFFFFFE'  A teensy bit smaller than +Hmax
         DC    X'CFAFFFFFFFFFFFFE'  A teensy bit smaller than -Hmax
*
         DC    X'4FB0000000000000'  A teensy bit larger than +Hmax
         DC    X'CFB0000000000000'  A teensy bit larger than -Hmax
*
* Test +/-Hmin and nearby values represented as long BFP inputs.  Hmin
* and larger magnitude should convert, and smaller should underflow to
* a signed zero.
*
         DC    X'2FB0000000000000'  +Hmin in long BFP
         DC    X'AFB0000000000000'  -Hmin in long BFP
*
         DC    X'2FB0000000000001'  A teensy bit larger than +Hmin
         DC    X'AFB0000000000001'  A teensy bit larger than -Hmin
*
         DC    X'2FAFFFFFFFFFFFFF'  A teensy bit smaller than +Hmin
         DC    X'AFAFFFFFFFFFFFFF'  A teensy bit smaller than -Hmin
*
*
* Test subnormals.  These should all underflow to a signed zero. These
* are included to ensure an subnormal with its unbiased exponent is 
* handled correctly.  
*
         DC    X'0000000000000001'  +Dmin
         DC    X'8000000000000001'  -Dmin
*
         DC    X'0008000000000000'  +Dmax
         DC    X'8008000000000000'  -Dmax
*
* Test +/-Nmax.  These should overflow to a signed Hmax.
*
         DC    X'7FEFFFFFFFFFFFFF'  +Nmax
         DC    X'FFEFFFFFFFFFFFFF'  -Nmax
*
* Test SNaNs, QNaNs, and infinities.  NaNs should return +Hmax, and 
* infinities should return Hmax with the sign of the input infinity.  
*
         DC    X'7FF0000000000000'  +infinity
         DC    X'FFF0000000000000'  -infinity
*
         DC    X'7FF8000000000000'  +QNaN
         DC    X'FFF8000000000000'  -QNaN
*
         DC    X'7FF0000800000000'  +SNaN with payload 0x00008
         DC    X'FFF0000400000000'  -SNaN with payload 0x00004
*
LTOHCT   EQU   *-LTOHIN     Count of long BFP in list * 8
         SPACE 3
*
*  Locations for long BFP results
*
*                                26 inputs, 1 result/input
STOHOUT  EQU   BFPB2H+X'1000'    Long HFP rounded from Short BFP
*                                  ..26 results used, room for 32
STOHFLGS EQU   BFPB2H+X'1200'    FPCR flags and CC from above
*                                  ..26 results used, room for 32
*
*
*  Locations for long BFP results
*
*                                34 inputs, 1 result/input
LTOHOUT  EQU   BFPB2H+X'1300'    Long HFP rounded from long BFP
*                                  ..34 results used, room for 48
LTOHFLGS EQU   BFPB2H+X'1600'    FPCR flags and DXC from above
*                                  ..10 results used, room for 64
*
*
*
ENDRES   EQU   BFPB2H+X'1800'    next location for results
*
         PADCSECT ENDRES            Pad csect unless asma
*
         END

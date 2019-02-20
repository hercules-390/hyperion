  TITLE 'bfp-024-h2b.asm: Test Convert HFP to BFP'
***********************************************************************
*
*Testcase CONVERT HFP TO BFP
*  Test case capability includes verifying the condition code for
*  sundry result cases.  CONVERT HFP TO BFP does not generate IEEE
*  execptions or traps, and does not alter the Floating Point Control
*  Register (FPC).  
*
***********************************************************************
         SPACE 2
***********************************************************************
*
*                        bfp-024-h2b.asm 
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
*Testcase CONVERT HFP TO BFP
*  Test case capability includes verifying the condition code for
*  sundry result cases.  CONVERT HFP TO BFP does not generate IEEE
*  execptions or traps and does not alter the Floating Point Control
*  Register (FPC). 
*
* Tests the following two conversion instructions
*   CONVERT HFP TO BFP (long HFP to long BFP, RRF-e)
*   CONVERT HFP TO BFP (long HFP to short BFP, RRF-e) 
*
* This routine exhaustively tests rounding to 32- and 64-bit binary 
* floating point.  HFP may have up to 56 bits of significance, so HFP
* long may need to be rounded to fit into a 53-bit BFP significand.  
*
* The rounding mode also affects the short BFP value on overflow.  
* Conversion to long BFP cannot overflow.  
*
* Test data is compiled into this program.  The test script that runs
* this program can provide alternative test data through Hercules R 
* commands.
* 
* Test Case Order
* 1) Long HFP to short BFP basic tests.  Note that HFP does not support
*    non-finite values.  Infinity and NaNs cannot be used as input.
* 2) Long HFP to short BFP overflow and underflow tests using all seven
*    rounding modes.  See Note.
* 3) Long HFP to short BFP rounding mode tests
* 4) Long HFP to long BFP basic tests.  Note that HFP does not support
*    non-finite values.  Infinity and NaNs cannot be used as input.
* 5) Long HFP to long BFP rounding mode tests
*
* Note: When HFP is rounded to short BFP, overflow and underflow are
* possible because an HFP characteristic when converted to a BFP
* exponent may not fit in the 7 bits allowed for a BFP exponent.  For
* overflow, the result is either infinity or Nmax depending on the 
* input sign and rounding mode.  For underflow, the result is a rounded
* subnormal (tiny, exponent zero) BFP.  
*
* Overflow and underflow are not possible when the target is long BFP.
*
* Basic test cases are needed as follows:
*   0, +1.5, -1.5
*   Unnormalized HFP inputs (leading digit(s) of fraction is/are zero).
*
* Overflow/underflow test cases are needed as follows (short BFP only):
*   All cases in this set must over/underflow.  In the ideal case,
*   over/underflow would be triggered on some but not all rounding 
*   modes.  
*   Pure overflows that test return of +/-infinity or +/-Nmax based 
*   on the rounding mode used.  
*   Ties are represented, both even (round down) and odd (round up)
*   Nearest value is toward zero
*   Nearest value is away from zero
*   HFP overflows when converted to short BFP and the effectivve 
*   rounding direction is away from zero.  
*   HFP underflows when converted to short BFP and the effectivve 
*   rounding direction is towards zero.  
*   Each of the above must be represented in positive and negative.  
*   
* Rounding test cases are needed as follows:
*   Exact conversions are represented (no rounding needed)
*   Ties are represented, both even (round down) and odd (round up)
*   Nearest value is toward zero
*   Nearest value is away from zero
*   Each of the above must be represented in positive and negative.  
*   
* Because rounding decisions are based on the binary significand,
*   there is limited value to considering test case inputs in 
*   decimal form.  The binary representations are all that matters.
*
* Four input test data sets are provided.  The same HFP inputs are used
*   for both short and long BFP targets.  Two more sets are provided
*   for long HFP to short BFP (over/underflow, rounding) and one more 
*   for long HFP to long BFP (rounding).  We cannot use the same HFP
*   inputs for long and short BFP rounding tests because the rounding 
*   points differ for the two target precisions.  
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
BFPH2B   START 0
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
FPR0     EQU   0            Input value (operand 2) for TBEDBR/TBDR
FPR1     EQU   1            Output value (operand 1) for TBEDBR/TBDR
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
         ORG   BFPH2B+X'8E'   Program check interrution code
PCINTCD  DS    H
*
PCOLDPSW EQU   BFPH2B+X'150'  z/Arch Program check old PSW
*
         ORG   BFPH2B+X'1A0'  z/Arch Restart PSW
         DC    X'0000000180000000',AD(START)   
*
         ORG   BFPH2B+X'1D0'  z/Arch Program check old PSW
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
         ORG   BFPH2B+X'200'
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
* Long HFP to short BFP basic, over/underflow, and rounding tests
*
         LA    R10,HTOSBAS    Long HFP basic test inputs
         BAS   R13,TBEDR      Convert long HFP to short BFP
         LA    R10,HTOSOU     Long HFP overflow/underflow test inputs
         BAS   R13,TBEDROU    Round long HFP to short BFP 
         LA    R10,HTOSRM     Long HFP rounding mode test inputs
         BAS   R13,TBEDRA     Round long HFP to short BFP 
*
* Long HFP to long BFP basic and rounding tests.  Overflow and
* underflow are not possible when converting long HFP to long BFP.
*
         LA    R10,HTOLBAS    Long HFP basic test inputs
         BAS   R13,TBDR       Convert long HFP to long BFP
         LA    R10,HTOLRM     Long HFP rounding mode test inputs
         BAS   R13,TBDRA      Round long HFP to long BFP 
*
* Five test sets run.  All done.
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
         ORG   BFPH2B+X'300'
HTOSBAS  DS    0F           Inputs for HFP->short BFP basic tests
         DC    A(HTOBCT/8)
         DC    A(HTOBIN)
         DC    A(HTOSOUT)
         DC    A(HTOSFLGS)
*
HTOSOU   DS    0F       Inputs for HFP->short BFP o/uflow tests
         DC    A(HTOBOUCT/8)
         DC    A(HTOBINOU)
         DC    A(HTOSOUO)
         DC    A(HTOSOUOF)
*
HTOSRM   DS    0F       Inputs for HFP->short BFP rounding tests
         DC    A(HTOSRMCT/8)
         DC    A(HTOSINRM)
         DC    A(HTOSRMO)
         DC    A(HTOSRMOF)
*
HTOLBAS  DS    0F           Inputs for HFP->long BFP basic tests
         DC    A(HTOBCT/8)
         DC    A(HTOBIN)
         DC    A(HTOLOUT)
         DC    A(HTOLFLGS)
*
HTOLRM   DS    0F       Inputs for HFP->long BFP rounding tests
         DC    A(HTOLRMCT/8)
         DC    A(HTOLINRM)
         DC    A(HTOLRMO)
         DC    A(HTOLRMOF)
*
         EJECT
***********************************************************************
*
* Convert long HFP to short BFP.  One result is generated for each
* input with all exceptions trappable.  Note: this instruction should
* not trap.  The FPCR contents and condition code are stored for each
* input value.  Overflow testing is done here.  
*
***********************************************************************
         SPACE 2
TBEDR    LM    R2,R3,0(R10)  Get count and address of test input values
         LM    R7,R8,8(R10)  Get address of result area and flag area.
         LTR   R2,R2         Any test cases?
         BZR   R13           ..No, return to caller
         BASR  R12,0         Set top of loop
*
         LZDR  FPR1          Zero FRP1 to clear any residual
         LD    FPR0,0(,R3)   Get long HFP test value
         LFPC  FPCREGTR      Set exceptions trappable
         TBEDR FPR1,0,FPR0   Cvt long HFP in FPR0 to short BFP in FPR1
         STE   FPR1,0(,R7)   Store short BFP result
         STFPC 0(R8)         Store resulting FPCR flags and DXC
         MVI   3(R8),X'FF'   Ensure visibility of returned CC
         IPM   R0            Get condition code and program mask
         SRL   R0,28         Isolate CC in low order byte
         STC   R0,3(,R8)     Save condition code in results table
*
         LA    R3,8(,R3)     Point to next input value
         LA    R7,4(,R7)     Point to next result
         LA    R8,4(,R8)     Point to next FPCR result area
         BCTR  R2,R12        Convert next input value.  
         BR    R13           All converted; return.
         EJECT
***********************************************************************
*
* Short BFP overflow and underflow testing.  
*
* The BFP result returned on overflow is either +/- infinity or +/-
* Nmax depending on the input sign and the rounding mode.
* 
* The BFP result on underflow is a rounded denormalized (tiny) short 
* BFP value.  
*
* Each input will overflow or underflow and is tested using all seven 
* rounding modes that can be specified in the instruction.  
*
* The rounding mode stored in the FPC has no impact on 
* CONVERT HFP TO BFP.
*
***********************************************************************
          SPACE 2
TBEDROU  LM    R2,R3,0(R10)  Get count and address of test input values
         LM    R7,R8,8(R10)  Get address of result area and flag area.
         LTR   R2,R2         Any test cases?
         BZR   R13           ..No, return to caller
         BASR  R12,0         Set top of loop
*
         LD    FPR0,0(,R3)    Get long HFP test value
*
* Test cases using rounding mode specified in the instruction M3 field
*
         LFPC  FPCREGNT      Set exceptions non-trappable, clear flags
         TBEDR FPR1,0,FPR0   RZ, toward zero
         STE   FPR1,0*4(,R7) Store shortened rounded BFP result
         STFPC 0*4(R8)       Store resulting FPCR flags and DXC
         IPM   R0            Get condition code and program mask
         SRL   R0,28         Isolate CC in low order byte
         STC   R0,3+0*4(,R8) Save cc with FPC in results table
*
         LFPC  FPCREGNT      Set exceptions non-trappable, clear flags
         TBEDR FPR1,1,FPR0   RNTA, to nearest, ties away
         STE   FPR1,1*4(,R7) Store shortened rounded BFP result
         STFPC 1*4(R8)       Store resulting FPCR flags and DXC
         IPM   R0            Get condition code and program mask
         SRL   R0,28         Isolate CC in low order byte
         STC   R0,3+1*4(,R8) Save cc with FPC in results table
*
         LFPC  FPCREGNT      Set exceptions non-trappable, clear flags
         TBEDR FPR1,3,FPR0   RFS, prepare for shorter precision
         STE   FPR1,2*4(,R7) Store shortened rounded BFP result
         STFPC 2*4(R8)       Store resulting FPCR flags and DXC
         IPM   R0            Get condition code and program mask
         SRL   R0,28         Isolate CC in low order byte
         STC   R0,3+2*4(,R8) Save cc with FPC in results table
*
         LFPC  FPCREGNT      Set exceptions non-trappable, clear flags
         TBEDR FPR1,4,FPR0   RNTE, to nearest, ties to even
         STE   FPR1,3*4(,R7) Store shortened rounded BFP result
         STFPC 3*4(R8)       Store resulting FPCR flags and DXC
         IPM   R0            Get condition code and program mask
         SRL   R0,28         Isolate CC in low order byte
         STC   R0,3+3*4(,R8) Save cc with FPC in results table
*
         LFPC  FPCREGNT      Set exceptions non-trappable, clear flags
         TBEDR FPR1,5,FPR0   RZ, toward zero
         STE   FPR1,4*4(,R7) Store shortened rounded BFP result
         STFPC 4*4(R8)       Store resulting FPCR flags and DXC
         IPM   R0            Get condition code and program mask
         SRL   R0,28         Isolate CC in low order byte
         STC   R0,3+4*4(,R8) Save cc with FPC in results table
*
         LFPC  FPCREGNT      Set exceptions non-trappable, clear flags
         TBEDR FPR1,6,FPR0   RP, to +inf
         STE   FPR1,5*4(,R7) Store shortened rounded BFP result
         STFPC 5*4(R8)       Store resulting FPCR flags and DXC
         IPM   R0            Get condition code and program mask
         SRL   R0,28         Isolate CC in low order byte
         STC   R0,3+5*4(,R8) Save cc with FPC in results table
*
         LFPC  FPCREGNT      Set exceptions non-trappable, clear flags
         TBEDR FPR1,7,FPR0   RM, to -inf
         STE   FPR1,6*4(,R7) Store shortened rounded BFP result
         STFPC 6*4(R8)       Store resulting FPCR flags and DXC
         IPM   R0            Get condition code and program mask
         SRL   R0,28         Isolate CC in low order byte
         STC   R0,3+6*4(,R8) Save cc with FPC in results table
*
         LA    R3,8(,R3)     Point to next input value
         LA    R7,8*4(,R7)   Point to next short BFP result pair
         LA    R8,8*4(,R8)   Point to next FPCR result area
         BCTR  R2,R12        Convert next input value.  
         BR    R13           All converted; return.
         EJECT
***********************************************************************
*
* Convert long HFP to rounded short BFP using each possible rounding
* mode.  Seven test results are generated for each input.  A 32-byte 
* test result section is used to keep results sets aligned on a 
* quad-double word.
*
* All seven results use instruction-specified rounding modes; the FPC
* rounding mode has no impact on CONVERT HFP TO BFP.
*
***********************************************************************
          SPACE 2
TBEDRA   LM    R2,R3,0(R10)  Get count and address of test input values
         LM    R7,R8,8(R10)  Get address of result area and flag area.
         LTR   R2,R2         Any test cases?
         BZR   R13           ..No, return to caller
         BASR  R12,0         Set top of loop
*
         LD    FPR0,0(,R3)    Get long HFP test value
*
* Test cases using rounding mode specified in the instruction M3 field
*
         LFPC  FPCREGNT      Set exceptions non-trappable, clear flags
         TBEDR FPR1,0,FPR0   RZ, toward zero
         STE   FPR1,0*4(,R7) Store shortened rounded BFP result
         STFPC 0*4(R8)       Store resulting FPCR flags and DXC
         IPM   R0            Get condition code and program mask
         SRL   R0,28         Isolate CC in low order byte
         STC   R0,3+0*4(,R8) Save cc with FPC in results table
*
         LFPC  FPCREGNT      Set exceptions non-trappable, clear flags
         TBEDR FPR1,1,FPR0   RNTA, to nearest, ties away
         STE   FPR1,1*4(,R7) Store shortened rounded BFP result
         STFPC 1*4(R8)       Store resulting FPCR flags and DXC
         IPM   R0            Get condition code and program mask
         SRL   R0,28         Isolate CC in low order byte
         STC   R0,3+1*4(,R8) Save cc with FPC in results table
*
         LFPC  FPCREGNT      Set exceptions non-trappable, clear flags
         TBEDR FPR1,3,FPR0   RFS, prepare for shorter precision
         STE   FPR1,2*4(,R7) Store shortened rounded BFP result
         STFPC 2*4(R8)       Store resulting FPCR flags and DXC
         IPM   R0            Get condition code and program mask
         SRL   R0,28         Isolate CC in low order byte
         STC   R0,3+2*4(,R8) Save cc with FPC in results table
*
         LFPC  FPCREGNT      Set exceptions non-trappable, clear flags
         TBEDR FPR1,4,FPR0   RNTE, to nearest, ties to even
         STE   FPR1,3*4(,R7) Store shortened rounded BFP result
         STFPC 3*4(R8)       Store resulting FPCR flags and DXC
         IPM   R0            Get condition code and program mask
         SRL   R0,28         Isolate CC in low order byte
         STC   R0,3+3*4(,R8) Save cc with FPC in results table
*
         LFPC  FPCREGNT      Set exceptions non-trappable, clear flags
         TBEDR FPR1,5,FPR0   RZ, toward zero
         STE   FPR1,4*4(,R7) Store shortened rounded BFP result
         STFPC 4*4(R8)       Store resulting FPCR flags and DXC
         IPM   R0            Get condition code and program mask
         SRL   R0,28         Isolate CC in low order byte
         STC   R0,3+4*4(,R8) Save cc with FPC in results table
*
         LFPC  FPCREGNT      Set exceptions non-trappable, clear flags
         TBEDR FPR1,6,FPR0   RP, to +inf
         STE   FPR1,5*4(,R7) Store shortened rounded BFP result
         STFPC 5*4(R8)       Store resulting FPCR flags and DXC
         IPM   R0            Get condition code and program mask
         SRL   R0,28         Isolate CC in low order byte
         STC   R0,3+5*4(,R8) Save cc with FPC in results table
*
         LFPC  FPCREGNT      Set exceptions non-trappable, clear flags
         TBEDR FPR1,7,FPR0   RM, to -inf
         STE   FPR1,6*4(,R7) Store shortened rounded BFP result
         STFPC 6*4(R8)       Store resulting FPCR flags and DXC
         IPM   R0            Get condition code and program mask
         SRL   R0,28         Isolate CC in low order byte
         STC   R0,3+6*4(,R8) Save cc with FPC in results table
*
         LA    R3,8(,R3)     Point to next input value
         LA    R7,8*4(,R7)   Point to next short BFP result set
         LA    R8,8*4(,R8)   Point to next FPCR result set
         BCTR  R2,R12        Convert next input value.  
         BR    R13           All converted; return.
         EJECT
***********************************************************************
*
* Convert long HFP to short BFP.  One result is generated for each
* input with all exceptions trappable.  Note: this instruction should
* not trap.  The FPCR contents and condition code are stored for each
* input value.  Overflow testing is done here.  
*
***********************************************************************
         SPACE 2
TBDR     LM    R2,R3,0(R10)  Get count and address of test input values
         LM    R7,R8,8(R10)  Get address of result area and flag area.
         LTR   R2,R2         Any test cases?
         BZR   R13           ..No, return to caller
         BASR  R12,0         Set top of loop
*
         LZDR  FPR1          Zero FRP1 to clear any residual
         LD    FPR0,0(,R3)   Get long HFP test value
         LFPC  FPCREGTR      Set exceptions trappable
         TBDR  FPR1,0,FPR0   Cvt long HFP in FPR0 to long BFP in FPR1
         STD   FPR1,0(,R7)   Store short BFP result
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
* Convert long HFP to rounded short BFP using each possible rounding
* mode.  Seven test results are generated for each input.  A 32-byte 
* test result section is used to keep results sets aligned on a 
* quad-double word.
*
* All seven results use instruction-specified rounding modes; the FPC
* rounding mode has no impact on CONVERT HFP TO BFP.
*
***********************************************************************
          SPACE 2
TBDRA    LM    R2,R3,0(R10)  Get count and address of test input values
         LM    R7,R8,8(R10)  Get address of result area and flag area.
         LTR   R2,R2         Any test cases?
         BZR   R13           ..No, return to caller
         BASR  R12,0         Set top of loop
*
         LD    FPR0,0(,R3)    Get long HFP test value
*
* Test cases using rounding mode specified in the instruction M3 field
*
         LFPC  FPCREGNT      Set exceptions non-trappable, clear flags
         TBDR  FPR1,0,FPR0   RZ, toward zero
         STD   FPR1,0*8(,R7) Store long BFP result
         STFPC 0*4(R8)       Store resulting FPCR flags and DXC
         IPM   R0            Get condition code and program mask
         SRL   R0,28         Isolate CC in low order byte
         STC   R0,3+0*4(,R8) Save cc with FPC in results table
*
         LFPC  FPCREGNT      Set exceptions non-trappable, clear flags
         TBDR  FPR1,1,FPR0   RNTA, to nearest, ties away
         STD   FPR1,1*8(,R7) Store long BFP result
         STFPC 1*4(R8)       Store resulting FPCR flags and DXC
         IPM   R0            Get condition code and program mask
         SRL   R0,28         Isolate CC in low order byte
         STC   R0,3+1*4(,R8) Save cc with FPC in results table
*
         LFPC  FPCREGNT      Set exceptions non-trappable, clear flags
         TBDR  FPR1,3,FPR0   RFS, prepare for shorter precision
         STD   FPR1,2*8(,R7) Store long BFP result
         STFPC 2*4(R8)       Store resulting FPCR flags and DXC
         IPM   R0            Get condition code and program mask
         SRL   R0,28         Isolate CC in low order byte
         STC   R0,3+2*4(,R8) Save cc with FPC in results table
*
         LFPC  FPCREGNT      Set exceptions non-trappable, clear flags
         TBDR  FPR1,4,FPR0   RNTE, to nearest, ties to even
         STD   FPR1,3*8(,R7) Store long BFP result
         STFPC 3*4(R8)       Store resulting FPCR flags and DXC
         IPM   R0            Get condition code and program mask
         SRL   R0,28         Isolate CC in low order byte
         STC   R0,3+3*4(,R8) Save cc with FPC in results table
*
         LFPC  FPCREGNT      Set exceptions non-trappable, clear flags
         TBDR  FPR1,5,FPR0   RZ, toward zero
         STD   FPR1,4*8(,R7) Store long BFP result
         STFPC 4*4(R8)       Store resulting FPCR flags and DXC
         IPM   R0            Get condition code and program mask
         SRL   R0,28         Isolate CC in low order byte
         STC   R0,3+4*4(,R8) Save cc with FPC in results table
*
         LFPC  FPCREGNT      Set exceptions non-trappable, clear flags
         TBDR  FPR1,6,FPR0   RP, to +inf
         STD   FPR1,5*8(,R7) Store long BFP result
         STFPC 5*4(R8)       Store resulting FPCR flags and DXC
         IPM   R0            Get condition code and program mask
         SRL   R0,28         Isolate CC in low order byte
         STC   R0,3+5*4(,R8) Save cc with FPC in results table
*
         LFPC  FPCREGNT      Set exceptions non-trappable, clear flags
         TBDR  FPR1,7,FPR0   RM, to -inf
         STD   FPR1,6*8(,R7) Store long BFP result
         STFPC 6*4(R8)       Store resulting FPCR flags and DXC
         IPM   R0            Get condition code and program mask
         SRL   R0,28         Isolate CC in low order byte
         STC   R0,3+6*4(,R8) Save cc with FPC in results table
*
         LA    R3,8(,R3)     Point to next long HFP input value
         LA    R7,8*8(,R7)   Point to next long BFP result set
         LA    R8,8*4(,R8)   Point to next FPCR result set
         BCTR  R2,R12        Convert next input value.  
         BR    R13           All converted; return.
         EJECT
***********************************************************************
*
* HFP inputs.  The same input set is used for short BFP and long BFP 
* basic tests.  Separate sets are used for overflow/underflow testsing
* (short BFP only) and for rounding testing.  
*
***********************************************************************
          SPACE 2
*
* Long Hex Floating Point doubleword breakdown
*
*              x'8000000000000000'    sign bit
*              x'7F00000000000000'    Biased Characteristic
*              x'00FFFFFFFFFFFFFF'    Fraction digits for long HFP
*              x'00FFFFFF00000000'    Fraction digits for short HFP
*
* Long to BFP basic tests.  The same input values are used when 
* converting to long or short BFP.  These basic tests just make sure
* simple conversions can be handled; there is no attempt to create
* boundary tests.  
*
* Note that the same bias is used for all three HFP formats: +64.
*
HTOBIN   DS    0D        Inputs for long HFP to BFP basic tests
         DC    X'0000000000000000'         +0
         DC    X'4118000000000000'         +1.5
         DC    X'C118000000000000'         -1.5
         DC    X'412DB6DB6DB6DB6D'         +2.857142857142857 (20/7)
         DC    X'C12DB6DB6DB6DB6D'         -2.857142857142857 (20/7)
*
* Non-normalized HFP.  When converted to BFP, the result is always
* exact.  Additional non-normalized inputs are used for BFP rounding
* testing.  The result should be an exact 8.0000 (decimal)
*
         DC    X'4900000000800000'    Positive, non-normalized
         DC    X'C900000000800000'    Negative, non-normalized
*
         DC    X'7FFFFFFFFFFFFFFF'    Positive Hmax
         DC    X'0010000000000000'    Positive Hmin
*
HTOBCT   EQU   *-HTOBIN     Count of long HFP in list * 8
         SPACE 3
*
* The following tests over/underflow on short BFP results only.  Long
* BFP exponent sizes prevent over/underflow when converting from long
* HFP.  Rounding mode affects results; some cases below attempt to
* cause over/underflow only on certain rounding modes.  Each test value
* is converted to short BFP using each rounding mode.  
*
HTOBINOU DS    0D   Inputs for long HFP to short BFP over/underflow
*
*                                   Basic overflow values.  Overflows
*                                   to infinity or Nmax depending on
*                                   rounding mode.  
         DC    X'7EFFFFFF00000000'         +4,523,128E+68 (overflows)
         DC    X'FEFFFFFF00000000'         -4,523,128E+68 (overflows)
*
*                                   Basic underflow values.  Underflow
*                                   may be to zero or to Dmin depending
*                                   on rounding mode.
         DC    X'0010000000000000'         +5,397,605E-85 (underflows)
         DC    X'8010000000000000'         -5,397,605E-85 (underflows)
*
* The following tests underflow during normalization of the short
* BFP result.  On input, the HFP characteristic can be converted to
* a BFP exponent that is within range.  But the left-shift of the 
* characteristic into a significand that has an implicit one bit to 
* the left of the implicit radix point decrements the BFP exponent such
* that it will not fit in a short BFP.  
*
*                                   Underflows only when rounded
*                                   towards zero.
         DC    X'213FFFFFFFFFFFFF'    Positive "maybe" underflow
         DC    X'A13FFFFFFFFFFFFF'    Negative "maybe" underflow
*
*                                   overflows only when rounded away
*                                   from zero.
         DC    X'6B3FFFFFFFFFFFFF'    Positive "maybe" overflow
         DC    X'EB3FFFFFFFFFFFFF'    Negative "maybe" overflow
*
*                                   A tie.  Underflows on RTNE only
*                                   when rounded towards smaller
*                                   magnitude (toward zero).
         DC    X'213FFFFFE0000000'    Positive, does not underflow
         DC    X'A13FFFFFE0000000'    Negative, does not underflow
         DC    X'213FFFFFA0000000'    Positive, underflows
         DC    X'A13FFFFFA0000000'    Negative, underflows
*
*                                   A tie.  Overflows on RTNE only
*                                   when rounded towards larger
*                                   magnitude (away from zero).
         DC    X'6B3FFFFFE0000000'    Positive, does not underflow
         DC    X'EB3FFFFFE0000000'    Negative, does not underflow
         DC    X'6B3FFFFFA0000000'    Positive, underflows
         DC    X'EB3FFFFFA0000000'    Negative, underflows
* 
HTOBOUCT EQU   *-HTOBINOU   Count of long HFP in list * 8
         SPACE 3
*
* Test cases for exhaustive rounding mode tests of HFP to short BFP.
* Different test cases are used for long BFP targets because it is not
* possible to create an input value that is a tie for both long and 
* short BFP results.
*
HTOSINRM DS    0D        Inputs for HFP to short BFP rounding tests
*
* Note: in the comments below, 'up' and 'down' mean 'toward higher
* magnitude' and 'toward lower magnitude' respectively and without
* regard to the sign.  When converting long HFP to long BFP, up to 
* three of the trailing bits control rounding, depending on the value
* of the leading hex digit 'd':
*
* d >= 8        - Three trailing bits rounded
* 4 <= d <= 7   - Two trailing bits rounded
* 2 <= d <= 3   - One trailing bit rounded
* d <= 1        - No rounding (includes non-normalized)
*
* The following two values have 54 bits of significance in the 
* fraction.  These will need to be rounded for both short and long
* BFP results.
*
         DC    X'412DB6DB6DB6DB6B'         +2.857142857142857 (20/7)
         DC    X'C12DB6DB6DB6DB6B'         -2.857142857142857 (20/7)
*
* The following values require rounding on both short and long bfp.  
* Short for obvious reasons, long because the fraction has 56 bits 
* of significance.  When rounded towards zero, the result should be
* 8.0... (0x41000000 or 0x4020000000000000).  When rounded away from 
* zero on short BFP target, 8.00000095367431640625 (0x41000001), and 
* on long BFP, 8.0000000000000017763568394002504646778106689453125 
* (0x4020000000000001).
*
* I/P value       8.000000000000000222044604925031[...]
* When rounded to larger magnitude: 
* Short BFP O/P   8.00000095367431640625
         DC    X'4180000000000001'    Positive, inexact long and short
         DC    X'C180000000000001'    Negative, inexact long and short
*
* A rounding tie when converted to short BFP.  RTNE rounds down.
*
* I/P value       9.000000000000000222044604925031[...]
* When rounded to larger magnitude: 
* Short BFP O/P   9.00000095367431640625
         DC    X'4190000020000000'    Positive, inexact short
         DC    X'C190000020000000'    Negative, inexact short
*
* A rounding tie when converted to long BFP.  RTNE rounds up.
*
* I/P value       10.000000000000000222044604925031[...]
* When rounded to larger magnitude: 
* Short BFP O/P   10.00000095367431640625
         DC    X'41A0000060000000'    Positive, inexact short
         DC    X'C1A0000060000000'    Negative, inexact short
*
* Non-normalized HFP.  When converted to short BFP, the result must be 
* rounded.
*
         DC    X'480000000B000002'    Positive, non-normal inexact
         DC    X'C80000000B000002'    Negative, non-normal inexact
*
HTOSRMCT EQU   *-HTOSINRM   Count of short BFP rounding tests * 8
*
         SPACE 3
*
* Test cases for exhaustive rounding mode tests of HFP to long BFP.
* Different test cases are used for long BFP targets because it is not
* possible to create an input value that is a tie for both long and 
* short BFP results.
*
HTOLINRM DS    0D        Inputs for HFP to short BFP rounding tests
*
*   Note: in the comments below, 'up' and 'down' mean 'toward 
*   higher magnitude' and 'toward lower magnitude' respectively and
*   without regard to the sign, and rounding is to short BFP.  
*
* The following two values have 54 bits of significance in the 
* fraction.  These will need to be rounded for both short and long
* BFP results.
*
         DC    X'412DB6DB6DB6DB6D'         +2.857142857142857 (20/7)
         DC    X'C12DB6DB6DB6DB6D'         -2.857142857142857 (20/7)
*
* The following values require rounding on both short and long bfp.  
* Short for obvious reasons, long because the fraction has 56 bits 
* of significance.  When rounded towards zero, the result should be
* 8.0... (0x41000000 or 0x4020000000000000).  When rounded away from 
* zero on short BFP target, 8.00000095367431640625 (0x41000001), and 
* on long BFP, 8.0000000000000017763568394002504646778106689453125 
* (0x4020000000000001).
*
* I/P value       8.000000000000000222044604925031[...]
* When rounded to larger magnitude: 
* Long BFP O/P    8.0000000000000017763568394002504646778106689453125
* Short BFP O/P   8.00000095367431640625
         DC    X'4180000000000001'    Positive, inexact long and short
         DC    X'C180000000000001'    Negative, inexact long and short
*
* A rounding tie when converted to long BFP.  RTNE rounds down.
*
* I/P value       9.000000000000000222044604925031[...]
* When rounded to larger magnitude: 
* Short BFP O/P   9.00000095367431640625
         DC    X'4190000000000004'    Positive, inexact long
         DC    X'C190000000000004'    Negative, inexact long
*
* A rounding tie when converted to long BFP.  RTNE rounds up.
*
* I/P value       10.000000000000000222044604925031[...]
* When rounded to larger magnitude: 
* Short BFP O/P   10.00000095367431640625
         DC    X'41A000000000000C'    Positive, inexact long
         DC    X'C1A000000000000C'    Negative, inexact long
*
HTOLRMCT EQU   *-HTOLINRM   Count of short BFP rounding tests * 8
*
         SPACE 3
*
*  Locations for long BFP results
*
*                                Seven inputs, 1 results/input
HTOSOUT  EQU   BFPH2B+X'1000'    Short BFP rounded from long HFP
*                                  ..7 results used, room for 32
HTOSFLGS EQU   BFPH2B+X'1080'    FPCR flags and CC from above
*                                  ..7 results used, room for 32
*
*                                Sixteen inputs, 7 results/input;
*                                ...align result set on a quadword
HTOSOUO  EQU   BFPH2B+X'1100'    Short BFP result o/uflow tests
*                                  ..16 sets used, room for 32
HTOSOUOF EQU   BFPH2B+X'1500'    FPCR flags and DXC from above
*                                  ..16 sets used, room for 32

*                                Ten inputs, 7 results/input;
*                                ...align result set on a quadword
HTOSRMO  EQU   BFPH2B+X'1900'    Short BFP result rounding tests
*                                  ..10 sets used, room for 16
HTOSRMOF EQU   BFPH2B+X'1B00'    FPCR flags and DXC from above
*                                  ..10 sets used, room for 16
*
*  Locations for long BFP results
*
*                                Seven inputs, 1 result/input
HTOLOUT  EQU   BFPH2B+X'1D00'    Long BFP rounded from long HFP
*                                  ..7 results used, room for 16
HTOLFLGS EQU   BFPH2B+X'1D80'    FPCR flags and DXC from above
*                                  ..7 results used, room for 32
*
*                                Eight inputs, 7 results/input;
*                                ...align result set on a quadword
HTOLRMO  EQU   BFPH2B+X'2000'    Long BFP rounding tests
*                                  ..8 sets used, room for 16
HTOLRMOF EQU   BFPH2B+X'2400'    FPCR flags and DXC from above
*                                  ..8 sets used, room for 16
*
*
*
ENDRES   EQU   BFPH2B+X'2600'    next location for results
*
         PADCSECT ENDRES            Pad csect unless asma
*
         END

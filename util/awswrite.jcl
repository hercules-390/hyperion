//IBMUSERA JOB CLASS=A,MSGCLASS=A,MSGLEVEL=(1,1)
//ASMCLG  PROC
//IEUASM  EXEC PGM=ASMA90,PARM='NOOBJECT,DECK',REGION=4M
//SYSPRINT DD  SYSOUT=*
//SYSLIB   DD  DSN=SYS1.MACLIB,DISP=SHR
//         DD  DSN=SYS1.MODGEN,DISP=SHR
//SYSUT1   DD  UNIT=SYSDA,SPACE=(CYL,(5,5))
//SYSPUNCH DD  DSN=&&OBJSET,DISP=(,PASS),UNIT=SYSDA,
//             SPACE=(TRK,(5,5)),DCB=(RECFM=FB,LRECL=80,BLKSIZE=3120)
//IEWL    EXEC PGM=IEWL,PARM='LIST,LET,NCAL,MAP',
//             COND=(0,NE,IEUASM),REGION=4M
//SYSPRINT DD  SYSOUT=*
//SYSUT1   DD  UNIT=SYSDA,SPACE=(CYL,(5,5))
//SYSLIN   DD  DSN=&&OBJSET,DISP=(OLD,DELETE)
//SYSLMOD  DD  DSN=&&GOSET(GO),DISP=(,PASS),UNIT=SYSDA,
//             SPACE=(TRK,(5,5,5)),DCB=(RECFM=U,BLKSIZE=6144)
//GO      EXEC PGM=*.IEWL.SYSLMOD,COND=((0,NE,IEUASM),(0,NE,IEWL))
//SYSPRINT DD  SYSOUT=*
//AWSIN    DD  DSN=&SYSUID..XXXXXX.AWSTAPE,DISP=SHR
//TAPEOUT  DD  UNIT=3480,VOL=SER=XXXXXX,LABEL=(1,BLP,EXPDT=98000)
//        PEND
//ASMCLG  EXEC ASMCLG
AWSWRITE TITLE 'Copy AWSTAPE file to physical tape'
AWSWRITE CSECT
*****    PROGRAM DESCRIPTION
*
*        This program copies an AWSTAPE file (a tape image on disk)
*        to a physical tape.  The exact structure of the tape image
*        (all files including data blocks and tape marks) is copied
*        to the physical tape.
*
*        The JCL for running this program is:
*
*        //AWSWRIT EXEC PGM=AWSWRITE
*        //SYSPRINT DD  SYSOUT=*
*        //AWSIN    DD  DSN=file.awstape,DISP=SHR
*        //TAPEOUT  DD  UNIT=3480,VOL=SER=XXXXXX,LABEL=(1,BLP)
*
*        Notes:
*        1. The input file AWSIN can be any record format (fixed,
*           variable, or undefined) and can have any record length
*        2. The output tape is written with BLP, therefore the
*           job must be run under a job class which allows BLP
*           processing. The JES2PARM parameter BLP=YES in the
*           JOBCLASS statement allows a job to use BLP.
*           This can be modified dynamically by using the command
*           $TJOBCLASS(A),BLP=YES
*
*        AWSWRITE was created by Roger Bowler, September 2003
*        and placed in the public domain.
*****
AWSWRITE CSECT
         SAVE  (14,12),,AWSWRITE-Roger-Bowler-2003
         LR    R12,R15             Establish base register
         USING AWSWRITE,R12
         LA    R15,AWSSAVEA        Point to new savearea
         ST    R13,4(,R15)         Establish forward/
         ST    R15,8(,R13)           backward pointers
         LR    R13,R15             Activate new savearea
         L     R1,0(,R1)           Point to PARM area
         LH    R2,0(,R1)           Pick up PARM length
         LA    R3,2(,R1)           Point to PARM text
         CH    R2,=H'4'            Could it be PARM=TEST?
         BNE   NOTTEST             No, skip
         CLC   0(4,R3),=C'TEST'    Is it PARM=TEST?
         BNE   NOTTEST             No, skip
         MVI   TESTFLAG,X'FF'      Yes, set TEST flag
NOTTEST  EQU   *
***
* Open the DCBs
***
         MVC   RETCODE,=F'16'      Prime return code for failure
         OPEN  (SYSPRINT,OUTPUT)   Open listing dataset
         TM    SYSPRINT+48,X'10'   Listing DCB open?
         BZ    EXIT                No, exit with return code 16
         OPEN  (AWSIN,INPUT)       Open input dataset
         TM    AWSIN+48,X'10'      Input DCB open?
         BZ    TERMINE             No, exit with return code 16
         CLI   TESTFLAG,X'FF'      Is it PARM=TEST?
         BE    NOOPENT             Yes, do not open tape
         OPEN  (TAPEOUT,OUTPUT)    Open output dataset
         TM    TAPEOUT+48,X'10'    Output DCB open?
         BZ    TERMINE             No, exit with return code 16
NOOPENT  EQU   *
         MVC   RETCODE,=F'0'       Prime return code for success
***
* Obtain a 64K output buffer
***
         GETMAIN R,LV=MAXBLKL      Obtain 64K storage area
         ST    R1,OUTBUFP          Save address of output buffer
         XC    OUTBLKL,OUTBLKL     Clear output block length
         XC    INDATAP,INDATAP     Clear input data pointer
         XC    INDATAL,INDATAL     Clear input data length
***
* Read a 6-byte AWSTAPE block header from the input file
***
NEXTHDR  EQU   *
         LA    R4,AWSHDR           Address of buffer for header
         LA    R5,AWSHDRL          Length of AWSTAPE header
         BAL   R10,READIN          Read 6-byte header from AWSIN
         CLC   AWSHDR(6),=XL6'00'  Is it all zero?
         BE    LOGEOF              Yes, treat as logical end-of-file
         TM    AWSFLG1,AWSF1TM     Is this a tape mark?
         BO    WRITETM             Yes, go write a tape mark
***
* Obtain length of logical data block which follows
***
         SR    R2,R2               Clear length register
         ICM   R2,B'0001',AWSBLKL  Load input block length...
         ICM   R2,B'0010',AWSBLKL+1  ...in reverse byte order
***
* Determine where to read logical data block into output buffer
***
         TM    AWSFLG1,AWSF1BB     Is it start of a physical block?
         BZ    BEGBLK1             No, append to data in buffer
         XC    OUTBLKL,OUTBLKL     Yes, clear output block length
BEGBLK1  DS    0H
         L     R1,OUTBLKL          Calculate...
         ALR   R1,R2               ...new output block length
         CL    R1,=A(MAXBLKL)      Does data exceed buffer length?
         BNL   BADBLKL             Yes, error
***
* Read a logical data block from the input file
***
         L     R4,OUTBUFP          Point to start of buffer
         AL    R4,OUTBLKL          Point past data already in buffer
         LR    R5,R2               Load logical data block length
         BAL   R10,READIN          Read logical data block
         L     R1,OUTBLKL          Calculate...
         ALR   R1,R5               ...new output block length
         ST    R1,OUTBLKL          Update new output block length
         TM    AWSFLG1,AWSF1EB     End of physical block?
         BZ    NEXTHDR             No, read next input header
***
* Write a physical data block to the tape
***
         L     R1,OUTBLKL          Load output block length
         CVD   R1,DWORK            Convert block length to decimal
         MVC   MSGDBL(6),=X'402020202120'
         ED    MSGDBL(6),DWORK+5   Edit block length into message
         MVC   MSGDBV,MSGDBV-1     Clear label area in message
         CH    R1,=H'80'           Is it an 80-byte block?
         BNE   NOTLABL             No, cannot be a standard label
         L     R1,OUTBUFP          Point to output buffer
         CLC   0(3,R1),=C'VOL'     Could it be a standard label?
         BE    PRTLABL             Yes, list it
         CLC   0(3,R1),=C'HDR'     Could it be a standard label?
         BE    PRTLABL             Yes, list it
         CLC   0(3,R1),=C'EOF'     Could it be a standard label?
         BE    PRTLABL             Yes, list it
         CLC   0(3,R1),=C'UHL'     Could it be a standard label?
         BE    PRTLABL             Yes, list it
         CLC   0(3,R1),=C'UTL'     Could it be a standard label?
         BNE   NOTLABL             No, skip
PRTLABL  EQU   *
         MVC   MSGDBV,0(R1)        Copy standard label to message
NOTLABL  EQU   *
         PUT   SYSPRINT,MSGDB      Write diagnostic message
         MVI   CCW,X'01'           Set CCW command = Write
         L     R1,OUTBUFP          Point to output buffer
         STCM  R1,B'0111',CCW+1    Save 24-bit buffer address in CCW
         MVI   CCW+4,X'20'         Set CCW flags = SLI
         L     R1,OUTBLKL          Load length of data in buffer
         STH   R1,CCW+6            Save 16-bit data length in CCW
         BAL   R10,EXCPIO          Perform I/O via EXCP
         B     NEXTHDR             Read next input header
***
* Write a tape mark to the tape
***
WRITETM  DS    0H
         L     R2,=A(MSGTM)        Point to tape mark message
         PUT   SYSPRINT,(R2)       Write diagnostic message
         MVI   CCW,X'1F'           Set CCW command = Write Tape Mark
         XC    CCW+1(3),CCW+1      Zeroise CCW data address
         MVI   CCW+4,X'20'         Set CCW flags = SLI
         MVC   CCW+6(2),=H'1'      Set CCW data length non-zero
         BAL   R10,EXCPIO          Perform I/O via EXCP
         B     NEXTHDR             Read next input header
***
* Fatal error routines
***
BADBLKL  DS    0H
         L     R2,=A(ERRMSG1)      Data block exceeds 64K-1
         PUT   SYSPRINT,(R2)       Write error message
         MVC   RETCODE,=F'12'      Set bad return code
         B     TERMINE             Exit with bad return code
OUTIOER  DS    0H
         UNPK  ERRM2CCW(9),CCW(5)
         TR    ERRM2CCW(8),HEXTAB-240
         MVI   ERRM2CCW+8,C' '
         UNPK  ERRM2CCW+9(9),CCW+4(5)
         TR    ERRM2CCW+9(8),HEXTAB-240
         MVI   ERRM2CCW+17,C','
         UNPK  ERRM2ECB(3),ECB(2)
         TR    ERRM2CCW(2),HEXTAB-240
         MVI   ERRM2CCW+2,C','
         UNPK  ERRM2CSW(9),IOBCSW(5)
         TR    ERRM2CSW(8),HEXTAB-240
         MVI   ERRM2CSW+8,C' '
         UNPK  ERRM2CSW+9(9),IOBCSW+4(5)
         TR    ERRM2CSW+9(8),HEXTAB-240
         MVI   ERRM2CSW+17,C','
         UNPK  ERRM2SNS(5),IOBSENSE(3)
         TR    ERRM2SNS(4),HEXTAB-240
         MVI   ERRM2SNS+4,C' '
         PUT   SYSPRINT,ERRMSG2    Print I/O error message
         MVC   RETCODE,=F'8'       Set bad return code
         B     TERMINE             Exit with bad return code
***
* Termination routines
***
READEOF  DS    0H
         L     R2,=A(MSGPEOF)      Physical end-of-file on AWSIN
         PUT   SYSPRINT,(R2)       Write diagnostic message
         B     TERMINE
LOGEOF   DS    0H
         L     R2,=A(MSGLEOF)      Logical end-of-file on AWSIN
         PUT   SYSPRINT,(R2)       Write diagnostic message
TERMINE  DS    0H
         ICM   R1,B'1111',OUTBUFP  Load output buffer address
         BZ    NOFREEM             Skip if no buffer allocated
         FREEMAIN R,A=(1),LV=MAXBLKL  Release storage area
NOFREEM  EQU   *
         CLOSE (AWSIN,,TAPEOUT)    Close input/output DCBs
         CLOSE (SYSPRINT)          Close listing dataset
EXIT     EQU   *
         L     R13,4(,R13)         Load HSA pointer
         L     R15,RETCODE         Load return code
         L     R14,12(,R13)        Restore...
         LM    R0,R12,20(R13)      ...registers
         BR    R14                 Exit from AWSWRITE
***
* Subroutine to read a given number of bytes from the input file
*
* Input: R4  = Destination buffer address
*        R5  = Number of bytes to read
***
READIN   DS    0H
         STM   R4,R5,READSAVE      Save work registers
READCONT EQU   *
         CL    R5,INDATAL          Enough data in input buffer?
         BNH   READMOVE            Yes, copy it
* Copy as much data as is available from the input buffer
         LR    R0,R4               R0 = destination buffer address
         L     R1,INDATAL          R1 = length of input data
         L     R14,INDATAP         R14 => data in input buffer
         L     R15,INDATAL         R15 = length of input data
         MVCL  R0,R14              Copy data from input buffer
         LR    R4,R0               R4 = updated destination addr
         SL    R5,INDATAL          R5 = updated length remaining
* Read the next input record into the input buffer
         GET   AWSIN               Get-locate input record
         SR    R0,R0               Clear for insert
         ICM   R0,B'0011',AWSLRECL R0 = record length from DCB
         TM    AWSRECFM,DCBRECU    Is it RECFM=U ?
         BO    READNOTV            Yes, skip
         TM    AWSRECFM,DCBRECV    Is it RECFM=V or RECFM=VB ?
         BNO   READNOTV            No, skip
* For RECFM=V or RECFM=VB there is a 4-byte RDW preceding the data
         ICM   R0,B'0011',0(R1)    Load record length from RDW
         SH    R0,=H'4'            Subtract length of RDW
         LA    R1,4(,R1)           Skip over the RDW
READNOTV EQU   *
         ST    R0,INDATAL          Save input data length
         ST    R1,INDATAP          Save input data pointer
         B     READCONT            Go back and move more data
READMOVE EQU   *
* Copy data from the input buffer to the destination buffer
         L     R14,INDATAP         R14 => data in input buffer
         L     R15,INDATAL         R15 = length of input data
         MVCL  R4,R14              Copy data from input buffer
         ST    R14,INDATAP         Save updated input data pointer
         ST    R15,INDATAL         Save updated input data length
         LM    R4,R5,READSAVE      Restore work registers
         BR    R10                 Return from READIN subroutine
***
* Subroutine to write to tape using EXCP
***
EXCPIO   DS    0H
         CLI   TESTFLAG,X'FF'      Is it PARM=TEST?
         BE    EXCPRET             Yes, bypass tape I/O
         MVI   ECB,X'00'           Clear ECB completion code
         EXCP  IOB                 Start channel program
         WAIT  ECB=ECB             Wait for I/O completion
         CLI   ECB,X'7F'           I/O completed successfully?
         BNE   OUTIOER             No, take error exit
EXCPRET  EQU   *
         BR    R10                 Return from EXCPIO subroutine
         EJECT
*
* AWSTAPE 6-byte logical block header
*
AWSHDR   DS    0H
AWSBLKL  DS    XL2                 Logical block length (reversed)
AWSPRVL  DS    XL2                 Previous block length (reversed)
AWSFLG1  DS    X                   Flags...
AWSF1BB  EQU   X'80'               ...beginning of physical block
AWSF1TM  EQU   X'40'               ...tape mark
AWSF1EB  EQU   X'20'               ...end of physical block
AWSFLG2  DS    X                   Flags (unused)
AWSHDRL  EQU   *-AWSHDR            Length of AWSTAPE block header
MAXBLKL  EQU   65536               Maximum block size 64K
*
* Data areas for EXCP I/O to tape
*
CCW      CCW   X'01',0,X'20',0     Write Data CCW
ECB      DC    F'0'                Event Control Block
IOB      DS    0F                  Input Output Block...
IOBFLAGS DC    XL2'0'              ...IOB flags
IOBSENSE DC    XL2'0'              ...IOB sense bytes
IOBECBPT DC    A(ECB)              ...ECB pointer
IOBCSW   DC    2F'0'               ...CSW after I/O
IOBSTART DC    A(CCW)              ...CCW pointer
IOBDCBPT DC    A(TAPEOUT)          ...DCB pointer
IOBRESTR DC    A(0)
IOBINCAM DC    H'1'                ...Block count increment
IOBERRCT DC    H'0'                ...Error counter
*
* Static data areas
*
         LTORG
DWORK    DC    D'0'                Doubleword work area
RETCODE  DC    F'0'                Program final return code
INDATAP  DC    A(0)                Pointer to next byte of input data
INDATAL  DC    F'0'                No.of input data bytes remaining
OUTBUFP  DC    A(0)                Address of output buffer
OUTBLKL  DC    F'0'                Length of data in output buffer
AWSSAVEA DS    18F                 New savearea
READSAVE DS    2F                  Savearea for READIN subroutine
TESTFLAG DC    X'00'               X'FF' if PARM=TEST specified
         PRINT NOGEN
SYSPRINT DCB   DSORG=PS,MACRF=PM,DDNAME=SYSPRINT,
               RECFM=FBA,LRECL=133,BLKSIZE=133
AWSIN    DCB   DSORG=PS,MACRF=GL,DDNAME=AWSIN,EODAD=READEOF
AWSLRECL EQU   DCBLRECL-IHADCB+AWSIN
AWSRECFM EQU   DCBRECFM-IHADCB+AWSIN
TAPEOUT  DCB   DSORG=PS,MACRF=E,DDNAME=TAPEOUT,DEVD=TA
HEXTAB   DC    C'0123456789ABCDEF'
*
* Messages
*
ERRMSG1  DC    CL133' *** Error *** Data block length exceeds 64K-1'
ERRMSG2  DC    CL133' '
         ORG   ERRMSG2
         DC    C' *** Error *** '
         DC    C'CCW='
ERRM2CCW DC    C'xxxxxxxx xxxxxxxx,'
         DC    C'ECBCC='
ERRM2ECB DC    C'xx,'
         DC    C'CSW='
ERRM2CSW DC    C'xxxxxxxx xxxxxxxx,'
         DC    C'SENSE='
ERRM2SNS DC    C'xxxx '
         ORG
MSGTM    DC    CL133' ** Tape Mark **'
MSGDB    DC    CL133' Data Block: nnnnn bytes'
MSGDBL   EQU   MSGDB+12,6
MSGDBV   EQU   MSGDB+30,80
MSGPEOF  DC    CL133' Terminated at end-of-file on AWSIN'
MSGLEOF  DC    CL133' Terminated by zero header on AWSIN'
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
         DCBD  DSORG=PS,DEVD=TA
         END
//

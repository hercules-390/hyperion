//IBMUSERA JOB CLASS=A,MSGCLASS=A,MSGLEVEL=(1,1)                        00010000
//ASMCLG  PROC                                                          00020000
//IEUASM  EXEC PGM=ASMA90,PARM='NOOBJECT,DECK',REGION=4M                00030000
//SYSPRINT DD  SYSOUT=*                                                 00040000
//SYSLIB   DD  DSN=SYS1.MACLIB,DISP=SHR                                 00050000
//         DD  DSN=SYS1.MODGEN,DISP=SHR                                 00060000
//SYSUT1   DD  UNIT=SYSDA,SPACE=(CYL,(5,5))                             00070000
//SYSPUNCH DD  DSN=&&OBJSET,DISP=(,PASS),UNIT=SYSDA,                    00080000
//             SPACE=(TRK,(5,5)),DCB=(RECFM=FB,LRECL=80,BLKSIZE=3120)   00090000
//IEWL    EXEC PGM=IEWL,PARM='LIST,LET,NCAL,MAP',                       00100000
//             COND=(0,NE,IEUASM),REGION=4M                             00110000
//SYSPRINT DD  SYSOUT=*                                                 00120000
//SYSUT1   DD  UNIT=SYSDA,SPACE=(CYL,(5,5))                             00130000
//SYSLIN   DD  DSN=&&OBJSET,DISP=(OLD,DELETE)                           00140000
//SYSLMOD  DD  DSN=&&GOSET(GO),DISP=(,PASS),UNIT=SYSDA,                 00150000
//             SPACE=(TRK,(5,5,5)),DCB=(RECFM=U,BLKSIZE=6144)           00160000
//GO      EXEC PGM=*.IEWL.SYSLMOD,COND=((0,NE,IEUASM),(0,NE,IEWL))      00170000
//SYSUT1   DD  DSN=TAPE.DATASET,UNIT=3480,VOL=SER=AAAAAA,DISP=OLD       00180000
//SYSUT2   DD  DSN=IBMUSER.AWSTAPE.DATASET,DISP=(,CATLG),               00181000
//             UNIT=SYSDA,VOL=SER=VVVVVV,SPACE=(CYL,(5,5),RLSE)         00181100
//        PEND                                                          00182000
//ASMCLG  EXEC ASMCLG                                                   00183000
TAPECONV TITLE 'Convert file to AWSTAPE format'                         00184000
*---------------------------------------------------------------------* 00185000
* Function:                                                           * 00186000
*        This program converts a tape file to AWSTAPE format.         * 00187000
*        It reads undefined length blocks of data from SYSUT1 and     * 00188000
*        writes each block, prefixed by a 6-byte header, to SYSUT2.   * 00189000
*                                                                     *
* Modification by Charlie Brint:                                      *
*        This program has been modified from its original Hercules    *
*        source format to handle blocks > 32K and < 64k because the   *
*        default blksize for ADRDSSU is 65,520 in most installations  *
*        and the original TAPECONV would just truncate blocks at      *
*        32,760 bytes without giving any error indication.  As a side *
*        benefit, SYSUT2 can reside on disk even if SYSUT1 is an      *
*        ADRDSSU dump tape with blocks longer than 32760 because the  *
*        program never writes blocks longer than 32760 to SYSUT2.     *
* NOTE:  This version uses the Large Block Interface (LBI) and        *
*        thus requires OS/390 V2R10 or z/OS to assemble and run.      *
*        For earlier versions of MVS, the program still assembles and *
*        runs (without LBI) if you replace &LBI SETB 1 by &LBI SETB 0 *
*---------------------------------------------------------------------* 00226000
         GBLB  &LBI
&LBI     SETB  1                        1=use LBI, 0=do not use LBI
TAPECONV CSECT                                                          03740000
         LR    R12,R15                  Load base register              03750000
         USING TAPECONV,R12             Establish addressability        03760000
         OPEN  (SYSUT1,INPUT)           Open input DCB                  03770000
         TM    SYSUT1+48,X'10'          Is DCB open?                    03780000
         BZ    EXIT020                  No, exit with RC=20             03790000
         OPEN  (SYSUT2,OUTPUT)          Open output DCB                 03791000
         TM    SYSUT2+48,X'10'          Is DCB open?                    03792000
         BZ    EXIT020                  No, exit with RC=20             03793000
GENLOOP  EQU   *                                                        03800000
         GET   SYSUT1                   Get input block                 03810000
         LR    R2,R1                    R2=>input block                 03811000
         LH    R4,SYSUT1+82             R4=actual block length          03820000
         AIF   (NOT &LBI).LBIN1
         L     R15,SYSUT1+68            Get IOB address
         SH    R15,=H'4'                Reduce by 4 as per LBI docs
         L     R4,0(R15)                R4 should now be the blk leng
.LBIN1   ANOP  ,
         LR    R5,R4                    Copy length for later use
         C     R4,=F'65520'             Is the block > 65520 ?
         BH    EXIT020                  yes, take error exit
         C     R4,=F'32760'             Is the block > 32760 ?
         BNH   UNDER32                  no, skip
         L     R4,=F'32760'             yes, set write length to max
UNDER32  DS    0H
         MVC   HDRPRVLN,HDRCURLN        Copy previous block length      03830000
         STCM  R5,B'0001',HDRCURLN      Store low-order length byte
         STCM  R5,B'0010',HDRCURLN+1    Store high-order length byte
         MVI   HDRFLAG1,HDRF1BOR+HDRF1EOR  Set complete record flags
         MVC   SYSUT2+82(2),=H'6'       Set header length in DCB        03851001
         PUT   SYSUT2,HEADER            Write block header to SYSUT2    03860000
         STH   R4,SYSUT2+82             Set block length in DCB         03870001
         PUT   SYSUT2,(R2)              Write data block to SYSUT2      03880000
         CR    R4,R5                    Did we write all the data ?
         BE    GENLOOP                  yes, go back for next record
         ALR   R2,R4                    no, bump input block pointer
         SR    R5,R4                    Compute remaining length
         STH   R5,SYSUT2+82             Set remaining length in DCB
         PUT   SYSUT2,(R2)              Write the rest of the block
         B     GENLOOP                  Go back for next record         03890000
GENEOF   DS    0H                                                       03900000
         MVC   HDRPRVLN,HDRCURLN        Copy previous block length      03901000
         XC    HDRCURLN,HDRCURLN        Clear current block length      03901100
         MVI   HDRFLAG1,HDRF1TMK        Set tape mark flag
         MVC   SYSUT2+82(2),=H'6'       Set header length in DCB        03902001
         PUT   SYSUT2,HEADER            Write block header to SYSUT2    03903000
         CLOSE (SYSUT1,,SYSUT2)         Close DCBs                      03910000
         SR    R15,R15                  Zeroize return code             03920000
         SVC   3                        Exit with RC=0                  03930000
EXIT020  DS    0H                                                       04050000
         LA    R15,20                   DD statement missing            04060000
         SVC   3                        Exit with RC=20                 04070000
         DROP  R12                      Drop base register              04080000
*                                                                       04081000
* AWSTAPE block header                                                  04082000
*                                                                       04083000
HEADER   DS    0CL6                     Block header                    04090000
HDRCURLN DC    XL2'0000'                Current block length            04100100
HDRPRVLN DC    XL2'0000'                Previous block length           04100202
HDRFLAG1 DC    X'00'                    Flags byte 1...                 04100300
HDRF1BOR EQU   X'80'                    ...beginning of record
HDRF1TMK EQU   X'40'                    ...tape mark
HDRF1EOR EQU   X'20'                    ...end of record
HDRFLAG2 DC    X'00'                    Flags byte 2
*                                                                       04100400
* Data Control Blocks                                                   04100500
*                                                                       04100600
         AIF   (&LBI).LBID1
SYSUT1   DCB   DSORG=PS,MACRF=GL,DDNAME=SYSUT1,EODAD=GENEOF,           X04110000
               RECFM=U,LRECL=0,BLKSIZE=32760                            04120000
         AGO   .LBID2
.LBID1   ANOP  ,
SYSUT1   DCB   DSORG=PS,MACRF=GL,DDNAME=SYSUT1,EODAD=GENEOF,           X
               RECFM=U,LRECL=0,DCBE=MYDCBE
MYDCBE   DCBE  BLKSIZE=65520  DCB extension, new in OS/390 V2R10
.LBID2   ANOP  ,
SYSUT2   DCB   DSORG=PS,MACRF=PM,DDNAME=SYSUT2,                        X04121000
               RECFM=U,LRECL=0,BLKSIZE=32760                            04122000
         LTORG                                                          04130000
*                                                                       04431000
* Register equates                                                      04432000
*                                                                       04433000
R0       EQU   0                                                        04434000
R1       EQU   1                                                        04435000
R2       EQU   2                                                        04436000
R3       EQU   3                                                        04437000
R4       EQU   4                                                        04438000
R5       EQU   5                                                        04439000
R6       EQU   6                                                        04440000
R7       EQU   7                                                        04450000
R8       EQU   8                                                        04460000
R9       EQU   9                                                        04470000
R10      EQU   10                                                       04480000
R11      EQU   11                                                       04490000
R12      EQU   12                                                       04500000
R13      EQU   13                                                       04510000
R14      EQU   14                                                       04520000
R15      EQU   15                                                       04530000
         END                                                            04540000

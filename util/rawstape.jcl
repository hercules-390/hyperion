//IBMUSERA JOB ,'JAN JAEGER',NOTIFY=IBMUSER,CLASS=A,MSGCLASS=A          00000103
//ASMA90   EXEC PGM=ASMA90,PARM='DECK,NOOBJ,XREF(SHORT)'                00000200
//SYSPRINT DD  SYSOUT=*                                                 00000300
//SYSLIB   DD  DSN=SYS1.MACLIB,DISP=SHR                                 00000400
//         DD  DSN=SYS1.MODGEN,DISP=SHR                                 00000500
//SYSUT1   DD  UNIT=SYSDA,SPACE=(CYL,10)                                00000600
//SYSPUNCH DD  DSN=&&PUNCH,DISP=(NEW,PASS),                             00000700
//          UNIT=SYSALLDA,SPACE=(CYL,10)                                00000800
RAWSTAPE TITLE 'Convert file from AWSTAPE format'                       00000900
*---------------------------------------------------------------------* 00001000
* Function:                                                           * 00001100
*        This program converts an AWSTAPE format file to RECFM=U      * 00001200
*        SYSUT1 is converted reblocked according to the AWS           * 00001300
*        header records.  The blocksize of SYSUT2 can be reset        * 00001400
*        using IEBGENER                                               * 00001500
*        The parm field indicates the filenumber to be extracted,     * 00001600
*        this number has the same value as when using BLP in JCL.     * 00001700
*---------------------------------------------------------------------* 00001800
RAWSTAPE CSECT                                                          00001900
         LR    R12,R15                  Load base register              00002000
         USING RAWSTAPE,R12             Establish addressability        00002100
         L     R2,0(,R1)                PARM=                           00002200
         LH    R3,0(,R2)                L'PARM                          00002300
         LTR   R3,R3                                                    00002400
         BZ    NOPARM                   No parm field                   00002500
         BCTR  R3,0                     Reduce to Machine length        00002600
         EX    R3,EXPACK                                                00002700
         CVB   R3,DWORD                                                 00002800
         ST    R3,FILENUM               Filenumber to be read           00002900
NOPARM   DS    0H                                                       00003000
         OPEN  (SYSUT1,INPUT)           Open input DCB                  00003100
         TM    SYSUT1+48,X'10'          Is DCB open?                    00003200
         BZ    EXIT020                  No, exit with RC=20             00003300
         OPEN  (SYSUT2,OUTPUT)          Open output DCB                 00003400
         TM    SYSUT2+48,X'10'          Is DCB open?                    00003500
         BZ    EXIT020                  No, exit with RC=20             00003600
         LA    R2,BUFFER                                                00003700
         SLR   R3,R3                    Total number of bytes in buffer 00003800
         LA    R5,1                     We start at file 1              00003900
READBLK  DS    0H                                                       00004000
         LA    R2,BUFFER(R3)                                            00004100
         GET   SYSUT1,(2)               Get input block                 00004200
         AH    R3,SYSUT1+82             R3=total bytes in buffer        00004300
GETNEXT  DS    0H                                                       00004400
         CH    R3,=Y(L'HEADER)          Do we at least have the header  00004500
         BM    READBLK                                                  00004600
         TM    HDRFLAG1,HDRF1TMK        Take mark?                      00004700
         BZ    NOEOF                                                    00004800
         LA    R5,1(,R5)                Increment file number           00004900
         C     R5,FILENUM               Beyond requested file?          00005000
         BH    EXIT000                                                  00005100
NOEOF    DS    0H                                                       00005200
         SLR   R4,R4                    Logical block length            00005300
         ICM   R4,B'0001',HDRCURLN      Load low-order length byte      00005400
         ICM   R4,B'0010',HDRCURLN+1    Load high-order length byte     00005500
         LA    R1,L'HEADER(,R4)                                         00005600
         CR    R3,R1                    Full block yet?                 00005700
         BM    READBLK                  No: Fetch another               00005800
         C     R5,FILENUM               Is this the file we want?       00005900
         BNE   NOPUT                                                    00006000
         TM    HDRFLAG1,HDRF1BOR+HDRF1EOR                               00006100
         BM    EXIT016                                                  00006200
         BNO   NOPUT                                                    00006300
         LA    R2,DATA                  Skip AWS header                 00006400
         STH   R4,SYSUT2+82             Store block size                00006500
         PUT   SYSUT2,(2)               Write block                     00006600
NOPUT    DS    0H                                                       00006700
         AH    R4,=Y(L'HEADER)          Remove header                   00006800
         SLR   R3,R4                    Remove block                    00006900
         LA    R2,BUFFER                Back to start                   00007000
         LR    R6,R2                    Move remainder to beginning     00007100
         LA    R8,0(R4,R2)              Point past block                00007200
         LR    R7,R3                    Set length                      00007300
         LR    R9,R3                                                    00007400
         MVCL  R6,R8                    Move block                      00007500
         B     GETNEXT                                                  00007600
*                                                                       00007700
EXIT000  DS    0H                                                       00007800
         CLOSE (SYSUT1,,SYSUT2)         Close DCBs                      00007900
         SR    R15,R15                  Zeroize return code             00008000
         SVC   3                        Exit with RC=0                  00008100
EXIT012  DS    0H                                                       00008200
         LA    R15,12                   Premature EOF                   00008300
         SVC   3                        Exit with RC=12                 00008400
EXIT016  DS    0H                                                       00008500
         LA    R15,16                   Invalid record type             00008600
         SVC   3                        Exit with RC=16                 00008700
EXIT020  DS    0H                                                       00008800
         LA    R15,20                   DD statement missing            00008900
         SVC   3                        Exit with RC=20                 00009000
*                                                                       00009100
* EXecuted                                                              00009200
*                                                                       00009300
EXPACK   PACK  DWORD,2(0,2)                                             00009400
         DROP  R12                      Drop base register              00009500
*                                                                       00009600
* Variables                                                             00009700
*                                                                       00009800
FILENUM  DC    F'1'                                                     00009900
DWORD    DS    D                                                        00010000
*                                                                       00010100
* Data Control Blocks                                                   00010200
*                                                                       00010300
         PRINT NOGEN                                                    00010400
SYSUT1   DCB   DSORG=PS,MACRF=GM,DDNAME=SYSUT1,EODAD=EXIT012,          X00010500
               RECFM=U,LRECL=0,BLKSIZE=32760                            00010600
SYSUT2   DCB   DSORG=PS,MACRF=PM,DDNAME=SYSUT2,                        X00010700
               RECFM=U,LRECL=0,BLKSIZE=32760                            00010800
         LTORG                                                          00010900
*                                                                       00011000
BUFFER   DS    0C                                                       00011100
*                                                                       00011200
* AWSTAPE block header                                                  00011300
*                                                                       00011400
HEADER   DS    0CL6                     Block header                    00011500
HDRCURLN DS    XL2                      Current block length            00011600
HDRPRVLN DS    XL2                      Previous block length           00011700
HDRFLAG1 DS    X'00'                    Flags byte 1...                 00011800
HDRF1BOR EQU   X'80'                    ...beginning of record          00011900
HDRF1TMK EQU   X'40'                    ...tape mark                    00012000
HDRF1EOR EQU   X'20'                    ...end of record                00012100
HDRFLAG2 DS    X'00'                    Flags byte 2                    00012200
*                                                                       00012300
* Data                                                                  00012400
*                                                                       00012500
DATA     DS    0C                                                       00012600
         DS    65536C                                                   00012700
*                                                                       00012800
* Register equates                                                      00012900
*                                                                       00013000
R0       EQU   0                                                        00013100
R1       EQU   1                                                        00013200
R2       EQU   2                                                        00013300
R3       EQU   3                                                        00013400
R4       EQU   4                                                        00013500
R5       EQU   5                                                        00013600
R6       EQU   6                                                        00013700
R7       EQU   7                                                        00013800
R8       EQU   8                                                        00013900
R9       EQU   9                                                        00014000
R10      EQU   10                                                       00014100
R11      EQU   11                                                       00014200
R12      EQU   12                                                       00014300
R13      EQU   13                                                       00014400
R14      EQU   14                                                       00014500
R15      EQU   15                                                       00014600
         END                                                            00014700
//IEWL     EXEC PGM=IEWL                                                00014800
//SYSPRINT DD  SYSOUT=*                                                 00014900
//SYSUT1   DD  UNIT=SYSDA,SPACE=(CYL,10)                                00015000
//SYSLMOD  DD  DSN=IBMUSER.LOAD(RAWSTAPE),DISP=SHR                      00015100
//SYSLIN   DD  DSN=&&PUNCH,DISP=(OLD,DELETE)                            00015200
//*                                                                     00015300
//CONVERT  EXEC PGM=*.IEWL.SYSLMOD,PARM=3                               00016704
//SYSUDUMP DD  SYSOUT=*                                                 00016800
//SYSUT1   DD  DISP=SHR,DSN=IBMUSER.SADUMP.AWS                          00016900
//SYSUT2   DD  DSN=IBMUSER.SADUMP,DISP=(NEW,CATLG),                     00017000
//          UNIT=SYSALLDA,SPACE=(TRK,1200,RLSE)                         00018000
//*                                                                     00019000
//SETDCB   EXEC PGM=IEBGENER                                            00020000
//SYSPRINT DD  SYSOUT=*                                                 00030000
//SYSIN    DD  DUMMY                                                    00040000
//SYSUT1   DD  DUMMY,                                                   00050000
//          DCB=(DSORG=PS,RECFM=FBS,LRECL=4160,BLKSIZE=29120)           00060000
//SYSUT2   DD  DISP=MOD,DCB=(*.SYSUT1),DSN=IBMUSER.SADUMP               00070000

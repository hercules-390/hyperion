#ifndef __ECPSVM_H__
#define __ECPSVM_H__

typedef struct _VMBLOK {
    U32 VMQFPNT;        /* 000 */
    U32 VMQBPNT;        /* 004 */
    U32 VMPNT;          /* 008 */
    U32 VMECEXT;        /* 00C */
    U32 VMSEG;          /* 010 */
    U32 VMSIZE;         /* 014 */
    U32 VMCHSTRT;       /* 018 */ 
    U32 VMCUSTRT;       /* 01C */
    U32 VMDVSTRT;       /* 020 */
    U32 VMTERM;         /* 024 */
    U16 VMVTERM;        /* 028 */
    U16 VMTRMID;        /* 02A */
    BYTE V1;            /* 02C */
#define VMTLEND V1
    BYTE V2;            /* 02D */
#define VMTLDEL V2
    BYTE V3;            /* 02E */
#define VMTCDEL V3
    BYTE V4;            /* 02F */
#define VMTESCP V4
    U16  VMCHCNT;       /* 030 */
    U16  VMCUCNT;       /* 032 */
    U16  VMDVCNT;       /* 034 */
    U16  VMIOACTV;      /* 036 */
    U16  VMCHTBL[16];   /* 038 */
    BYTE V5;            /* 058 */
#define VMRSTAT V5
    BYTE V6;            /* 059 */
#define VMDSTAT V6
    BYTE V7;            /* 05A */
#define VMOSTAT V7
    BYTE V8;            /* 05B */
#define VMQSTAT V8
    BYTE V9;            /* 05C */
#define VMPSTAT V9
    BYTE V10;           /* 05D */
#define VMESTAT V10
    BYTE V11;           /* 05E */
#define VMTRCTL V11
    BYTE V12;           /* 05F */
#define VMMLEVEL V12
    BYTE V13;           /* 060 */
#define VMQLEVEL V13
    BYTE RESERV1;       /* 061 */
    BYTE V14;           /* 062 */
#define VMTLEVEL V14;
    BYTE V15;           /* 063 */
#define VMPEND V15
    U32  VMLOCKER;      /* 064 */
    BYTE V16;           /* 068 */
#define VMFSTAT V16
    BYTE V17;           /* 069 */
#define VMMLVL2 V17
    U16 VMIOINT;        /* 06A */
    U32 VMTIMER;        /* 06C */
    DW  VMVTIME;        /* 070 */
    DW  VMTMOUTQ;       /* 078 */
    DW  VMTTIME;        /* 080 */
    DW  VMTMINQ;        /* 088 */
#define VMTSOUTQ VMTMINQ
    DW VMTODINQ;        /* 090 */
    BYTE VMINST[6];     /* 098 */
    BYTE V18;           /* 09E */
#define VMUPRIOR V18
    BYTE V19;           /* 09F */
#define VMPSWDCT V19
    U32 VMPERCTL;       /* 0A0 */
    U32 VMADSTOP;       /* 0A4 */
    DW  VMPSW;          /* 0A8 */
    U32 VMGPRS[16];     /* 0B0 */
    DW  VMFPRS[4];      /* 0F0 */
    BYTE VMUSER[8];     /* 110 */
    BYTE VMACNT[8];     /* 118 */
    BYTE VMDIST[8];     /* 120 */
    /* The rest is irrelevant */
} VMBLOK;

typedef struct _ECPSVM_STAT
{
    char *name;
    U32  call;
    U32  hit;
} ECPSVM_STAT;

#define ECPSVM_STAT_DCL(_name) ECPSVM_STAT _name
#define ECPSVM_STAT_DEF(_name) ._name = { .name = ""#_name"" ,.call=0,.hit=0}

#endif

#ifndef __ECPSVM_H__
#define __ECPSVM_H__

/* CR6 Definitions */
#define ECPSVM_CR6_VMASSIST 0x80000000         /* DO Privop Sim */
#define ECPSVM_CR6_VIRTPROB 0x40000000         /* Running user in Problem State */
#define ECPSVM_CR6_ISKINHIB 0x20000000         /* Inhibit ISK/SSK Sim */
#define ECPSVM_CR6_S360ONLY 0x10000000         /* Only S/360 Operations */
#define ECPSVM_CR6_SVCINHIB 0x08000000         /* No SVC sim */
#define ECPSVM_CR6_STVINHIB 0x04000000         /* No Shadow Table Validation */
#define ECPSVM_CR6_ECPSVM   0x02000000         /* ECPS:VM Enable */
#define ECPSVM_CR6_VIRTTIMR 0x01000000         /* Virtual Interval Timer update */
#define ECPSVM_CR6_MICBLOK  0x00FFFFF8         /* MICBLOK Address mask */
#define ECPSVM_CR6_VMMVSAS  0x00000004         /* VM Assists for MVS Enable (370E) */

/* MICBLOK */
typedef struct _ECPSVM_MICBLOK
{
    U32 MICRSEG;
    U32 MICCREG;
    U32 MICVPSW;
#define MICVIP MICVPSW
#define MICPEND 0x80
    U32 MICWORK;
    U32 MICVTMR;
    U32 MICACF;
#define MICEVMA MICACF
#define MICLPSW 0x80    /* LPSW SIM */
#define MICPTLB 0x40    /* PTLB SIM */
#define MICSCSP 0x20    /* SCKC, SPT SIM */
#define MICSIO  0x10    /* SIO, SIOF SIM */
#define MICSTSM 0x08    /* SSM, STNSM, STOSM SIM */
#define MICSTPT 0x04    /* STPT SIM */
#define MICTCH  0x02    /* TCH SIM */
#define MICDIAG 0x01    /* DIAG SIM */
} ECPSVM_MICBLOK;

/* PSA + X'3D4' - ASSISTS STUFF */
#define CPCREG0 0x3D4
#define CPCREG6 0x3D8
#define CPCREG8 0x3DC
#define TIMEDISP 0x3E0
#define ASVCLIST 0x3E4
#define AVMALIST 0x3E8
#define LASTUSER 0x3EC

/* CP ASSIST SVC (Not VM Assist SVC) LIST */
/* ASSISTS FOR CP LINK/RETURN SVCs */
/* DMKSVCNS */
/* Address found @ PSA+3E4 */
typedef struct _ECPSVM_SVCLIST
{
    DW NEXTSAVE;        /* Pointer to next Save Area + 8 */
    DW SLCADDR;         /* V=R Start */
    DW DMKSVCHI;        /* DMKFREHI */
    DW DMKSVCLO;        /* DMKFRELO + SAVEAREA LENGTH */
} ECPSVM_SVCLIST;

/* VM ASSIST LISTS */
/* ENTRYPOINT TO VARIOUS PRIVOP SIM FASTPATH */
/* (DMKPRVMA) */
/* Address found @ PSA+3E8 */

typedef struct _ECPSVM_VMALIST
{
    DW VSIVS;   /* EP To DMKVSIVS (Fastpath SIO/SIOF) */
    DW VSIEX;   /* Base addr for VSIVS */
    DW DSPCH;   /* Scheduler - Fast path for LPSW/SSM/STNSM/STOSM */
    DW TMRCC;   /* SCKC EP */
    DW TMR;     /* Timer ops base */
    DW TMRSP;   /* SPT EP */
    DW VATAT;   /* ARCHITECT */
    DW DSPB;    /* Slow Path Dispatcher - PSW Revalidate required */
    DW PRVVS;   /* VSIVS COUNT */
    DW PRVVL;   /* LPSW Count */
    DW PRVVM;   /* SSM/STxSM COUNT */
    DW PRVVC;   /* SCKC COUNT */
    DW RESERVED;
    DW PRVVP;   /* SPT COUNT */
} ECPSVM_VMALIST;

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
    U16  support:1,
        total:1;
} ECPSVM_STAT;

#define ECPSVM_STAT_DCL(_name) ECPSVM_STAT _name
#define ECPSVM_STAT_DEF(_name) ._name = { .name = ""#_name"" ,.call=0,.hit=0,.support=1,.total=0}
#define ECPSVM_STAT_DEFU(_name) ._name = { .name = ""#_name"" ,.call=0,.hit=0,.support=0,.total=0}
#define ECPSVM_STAT_DEFM(_name) ._name = { .name = ""#_name"" ,.call=0,.hit=0,.support=1,.total=1}

#endif

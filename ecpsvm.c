/***********************************************************/
/* ECPS:VM Support                                         */
/* (c) 2003 Ivan Warren                                    */
/*                                                         */
/* General guidelines about E6XX instruction class         */
/* this is an implementation of ECPS:VM Level 20, 21 or 22 */
/*                                                         */
/* General rule is : Only do what is safe to do. In doubt, */
/*                   give control to CP back (and act as   */
/*                   a NO-OP). All instructions have this  */
/*                   behaviour, therefore allowing only    */
/*                   partial implementation, or bypassing  */
/*                   tricky cases                          */
/*                                                         */
/* NOTE : VM/370 Rel 6 seems to have a slight bug in       */
/*        handling CP ASSIST. Although it does report      */
/*        CPASSIST is ON (CP QUERY CPASSIST : CPASSIST ON) */
/*        CR 6, Bit 7 is not set after IPL. However,       */
/*        doing a CP SET CPASSIST ON after IPL does set    */
/*        CR6, Bit 7, thus activating CP ASSIST instrs     */
/*        Therefore, on VM/370 Rel 6, an additional        */
/*        CP¨SET CPASSIST ON (Class A command) should be   */
/*        issued to activate the support                   */
/*                                                         */
/* NOTE : ECPS:VM is only available for S/370 architecture */
/*                                                         */
/* In order for CP ASSIST to be active, a CONFIGURATION    */
/* statement is added : ECPS:VM lvl|no                     */
/* lvl is the ASSIST level (20 is recommended)             */
/* no means CP ASSIST is disabled (default)                */
/*                                                         */
/* Currently supported CP ASSIST instructions :            */
/* +-----+-------+----------------------------------------+*/
/* |opc  | Mnemo | Function                               |*/
/* +-----+-------+----------------------------------------+*/
/* |E602 | LCKPG | Lock Page in core table                |*/
/* |E603 | ULKPG | Unlock page in core table              |*/
/* |E60E | SCNRU | Scan Real Unit control blocks          |*/
/* |E612 | STLVL | Store ECPS:VM Level                    |*/
/* |E614 | FREEX | Allocate CP FREE Storage from subpool  |*/
/* |E615 | FRETX | Release CP FREE Storage to subpool     |*/
/* +-----+-------+----------------------------------------+*/
/*                                                         */
/***********************************************************/

#include "hercules.h"

#include "opcode.h"

#include "inline.h"

#include "ecpsvm.h"

struct _ECPSVM_STATS
{
    ECPSVM_STAT_DCL(SVC);
    ECPSVM_STAT_DCL(SSM);
    ECPSVM_STAT_DCL(SASSIST);
    ECPSVM_STAT_DCL(FREE);
    ECPSVM_STAT_DCL(FRET);
    ECPSVM_STAT_DCL(LCKPG);
    ECPSVM_STAT_DCL(ULKPG);
    ECPSVM_STAT_DCL(SCNRU);
    ECPSVM_STAT_DCL(SCNVU);
    ECPSVM_STAT_DCL(DISP0);
    ECPSVM_STAT_DCL(DISP1);
    ECPSVM_STAT_DCL(DISP2);
    ECPSVM_STAT_DCL(DNCCW);
    ECPSVM_STAT_DCL(DFCCW);
    ECPSVM_STAT_DCL(FCCWS);
    ECPSVM_STAT_DCL(CCWGN);
    ECPSVM_STAT_DCL(UXCCW);
    ECPSVM_STAT_DCL(TRBRG);
    ECPSVM_STAT_DCL(TRLOK);
    ECPSVM_STAT_DCL(VIST);
    ECPSVM_STAT_DCL(VIPT);
    ECPSVM_STAT_DCL(STEVL);
    ECPSVM_STAT_DCL(FREEX);
    ECPSVM_STAT_DCL(FRETX);
    ECPSVM_STAT_DCL(PMASS);
    ECPSVM_STAT_DCL(LCSPG);
    ECPSVM_STAT_DCL(CPASSIST);
} ecpsvm_stats={
    ECPSVM_STAT_DEF(SVC),
    ECPSVM_STAT_DEF(SSM),
    ECPSVM_STAT_DEFM(SASSIST),
    ECPSVM_STAT_DEFU(FREE),
    ECPSVM_STAT_DEFU(FRET),
    ECPSVM_STAT_DEF(LCKPG),
    ECPSVM_STAT_DEF(ULKPG),
    ECPSVM_STAT_DEF(SCNRU),
    ECPSVM_STAT_DEFU(SCNVU),
    ECPSVM_STAT_DEFU(DISP0),
    ECPSVM_STAT_DEFU(DISP1),
    ECPSVM_STAT_DEFU(DISP2),
    ECPSVM_STAT_DEFU(DNCCW),
    ECPSVM_STAT_DEFU(DFCCW),
    ECPSVM_STAT_DEFU(FCCWS),
    ECPSVM_STAT_DEFU(CCWGN),
    ECPSVM_STAT_DEFU(UXCCW),
    ECPSVM_STAT_DEFU(TRBRG),
    ECPSVM_STAT_DEFU(TRLOK),
    ECPSVM_STAT_DEFU(VIST),
    ECPSVM_STAT_DEFU(VIPT),
    ECPSVM_STAT_DEF(STEVL),
    ECPSVM_STAT_DEF(FREEX),
    ECPSVM_STAT_DEF(FRETX),
    ECPSVM_STAT_DEFU(PMASS),
    ECPSVM_STAT_DEFU(LCSPG),
    ECPSVM_STAT_DEFM(CPASSIST),
};

typedef union _ECPSVM_STATARRAY
{
    struct _ECPSVM_STATS;
    ECPSVM_STAT s[1];
} ECPSVM_STATARRAY;
// #define DEBUG_CPASSIST
// #define DEBUG_SASSIST

#ifdef DEBUG_CPASSIST
#define DEBUG_ASSIST(x) ( x )
#else
#define DEBUG_ASSIST(x)
#endif

/* Utility macros because I am very lazy */
#define EVM_IC( x )  ARCH_DEP(vfetchb) ( ( ( x ) & ADDRESS_MAXWRAP(regs) ) , USE_REAL_ADDR , regs )
#define EVM_LH( x )  ARCH_DEP(vfetch2) ( ( ( x ) & ADDRESS_MAXWRAP(regs) ) , USE_REAL_ADDR , regs )
#define EVM_L( x )  ARCH_DEP(vfetch4) ( ( ( x ) & ADDRESS_MAXWRAP(regs) ) , USE_REAL_ADDR , regs )
#define EVM_LD( x )  ARCH_DEP(vfetch8) ( ( ( x ) & ADDRESS_MAXWRAP(regs) ) , USE_REAL_ADDR , regs )
#define EVM_ST( x , y ) ARCH_DEP(vstore4) ( ( x ) , ( ( y ) & ADDRESS_MAXWRAP(regs) ) , USE_REAL_ADDR , regs )
#define EVM_STH( x , y ) ARCH_DEP(vstore2) ( ( x ) , ( ( y ) & ADDRESS_MAXWRAP(regs) ) , USE_REAL_ADDR , regs )
#define EVM_STC( x , y ) ARCH_DEP(vstoreb) ( ( x ) , ( ( y ) & ADDRESS_MAXWRAP(regs) ) , USE_REAL_ADDR , regs )
#define EVM_MVC( x , y , z ) ARCH_DEP(vfetchc) ( ( x ) , ( z ) , ( y ) , USE_REAL_ADDR , regs )

#define BR14 regs->psw.IA=regs->GR_L(14) & ADDRESS_MAXWRAP(regs)

#ifdef _FEATURE_SIE
#define INITSIESTATE(x) (x).sie_state=0;
#else
#define INITSIESTATE(x)
#endif

#define CPASSIST_HIT(_stat) ecpsvm_stats._stat.hit++; \
        ecpsvm_stats.CPASSIST.hit++

#define SASSIST_HIT(_stat) ecpsvm_stats._stat.hit++; \
        ecpsvm_stats.SASSIST.hit++

#define SASSIST_PROLOG( _instname ) \
    VADR micblok; \
    VADR vpswa; \
    REGS vpregs; \
    BYTE  micpend; \
    if(!sysblk.ecpsvm.available) \
    { \
          DEBUG_ASSIST(logmsg("HHCEV300D : SASSIST "#_instname" ECPS:VM Disabled in configuration ")); \
          return(1); \
    } \
    micblok=regs->CR_L(6) & 0x00fffff8; \
    if(!regs->psw.prob) \
    { \
        return(1);      /* Priviledge stats */ \
    } \
    DEBUG_ASSIST(logmsg("HHCEV300D : SASSIST "#_instname" Problem State\n")); \
    if(!(regs->CR_L(6) & 0x80000000)) \
    { \
        return(1);      /* VM Assist not enabled */ \
    } \
    /* Ensure MICBLOK resides on a single page */ \
    /* Then set ref bit by calling LOG_TO_ABS */ \
    if((micblok & PAGEFRAME_BYTEMASK) > 0xfe0) \
    { \
        DEBUG_ASSIST(logmsg("HHCEV300D : SASSIST "#_instname" Micblok @ %6.6X crosses page frame\n",micblok)); \
        return(1); \
    } \
    /* Increment call now (don't count early misses) */ \
    ecpsvm_stats._instname.call++; \
    ecpsvm_stats.SASSIST.call++; \
    micblok=LOGICAL_TO_ABS(micblok,USE_REAL_ADDR,regs,ACCTYPE_READ,0); \
    /* Load the Virtual PSW Address */ \
    FETCH_FW(vpswa,regs->mainstor+micblok+8); \
    /* Load MICPEND */ \
    micpend=vpswa >> 24; \
    /* keep PSW address portion */ \
    vpswa &=ADDRESS_MAXWRAP(regs); \
    /* Set ref bit on page where Virtual PSW is stored */ \
    vpswa=LOGICAL_TO_ABS(vpswa,USE_REAL_ADDR,regs,ACCTYPE_READ,0); \
    /* Load the Virtual PSW in a temporary REGS structure */ \
    ARCH_DEP(load_psw) (&vpregs,&regs->mainstor[vpswa]); \
    DEBUG_ASSIST(logmsg("HHCEV300D : SASSIST "#_instname" Virtual ")); \
    DEBUG_ASSIST(display_psw(&vpregs)); \
    DEBUG_ASSIST(logmsg("HHCEV300D : SASSIST "#_instname" Real ")); \
    DEBUG_ASSIST(display_psw(regs));

#define ECPSVM_PROLOG(_inst) \
int     b1, b2; \
VADR    effective_addr1, \
        effective_addr2; \
     SSE(inst, execflag, regs, b1, effective_addr1, b2, effective_addr2); \
     if(!sysblk.ecpsvm.available) \
     { \
          DEBUG_ASSIST(logmsg("HHCEV300D : CPASSTS "#_inst" ECPS:VM Disabled in configuration ")); \
          ARCH_DEP(program_interrupt) (regs, PGM_OPERATION_EXCEPTION); \
     } \
     PRIV_CHECK(regs); \
     if(!(regs->CR_L(6) & 0x02000000)) \
     { \
        return; \
     } \
     ecpsvm_stats._inst.call++; \
     ecpsvm_stats.CPASSIST.call++;

#ifdef FEATURE_ECPSVM

DEF_INST(ecpsvm_basic_freex)
{
    ECPSVM_PROLOG(FREE);
    DEBUG_ASSIST(logmsg("HHCEV300D : freex called\n"));
}
DEF_INST(ecpsvm_basic_fretx)
{
    ECPSVM_PROLOG(FRET);
    DEBUG_ASSIST(logmsg("HHCEV300D : fretx called\n"));
}
DEF_INST(ecpsvm_lock_page)
{
    VADR ptr_pl;
    VADR pg;
    VADR cortbl;
    VADR corte;
    BYTE corcode;
    U32  lockcount;

    ECPSVM_PROLOG(LCKPG);

    ptr_pl=effective_addr1;
    pg=effective_addr2;

    DEBUG_ASSIST(logmsg("HHCEV300D : LKPG PAGE=%6.6X, PTRPL=%6.6X\n",pg,ptr_pl));

    cortbl=EVM_L(ptr_pl);
    corte=cortbl+((pg & 0xfff000)>>8);
    corcode=EVM_IC(corte+8);
    if(corcode & 0x80)
    {
        lockcount=EVM_L(corte+4);
        lockcount++;
    }
    else
    {
        lockcount=1;
        corcode|=0x80;
        EVM_STC(corcode,corte+8);
    }
    EVM_ST(lockcount,corte+4);
    regs->psw.cc=0;
    BR14;
    DEBUG_ASSIST(logmsg("HHCEV300D : LKPG Page locked. Count = %6.6X\n",lockcount));
    CPASSIST_HIT(LCKPG);
    return;
}
DEF_INST(ecpsvm_unlock_page)
{
    VADR ptr_pl;
    VADR pg;
    VADR corsz;
    VADR cortbl;
    VADR corte;
    BYTE corcode;
    U32  lockcount;

    ECPSVM_PROLOG(ULKPG);

    ptr_pl=effective_addr1;
    pg=effective_addr2;

    DEBUG_ASSIST(logmsg("HHCEV300D : ULKPG PAGE=%6.6X, PTRPL=%6.6X\n",pg,ptr_pl));

    corsz=EVM_L(ptr_pl);
    cortbl=EVM_L(ptr_pl+4);
    if((pg+4095)>corsz)
    {
        DEBUG_ASSIST(logmsg("HHCEV300D : ULKPG Page beyond core size of %6.6X\n",corsz));
        return;
    }
    corte=cortbl+((pg & 0xfff000)>>8);
    corcode=EVM_IC(corte+8);
    if(corcode & 0x80)
    {
        lockcount=EVM_L(corte+4);
        lockcount--;
    }
    else
    {
        DEBUG_ASSIST(logmsg("HHCEV300D : ULKPG Attempting to unlock page that is not locked\n"));
        return;
    }
    if(lockcount==0)
    {
        corcode &= ~(0x80|0x02);
        EVM_STC(corcode,corte+8);
        DEBUG_ASSIST(logmsg("HHCEV300D : ULKPG now unlocked\n"));
    }
    else
    {
        DEBUG_ASSIST(logmsg("HHCEV300D : ULKPG Page still locked. Count = %6.6X\n",lockcount));
    }
    EVM_ST(lockcount,corte+4);
    CPASSIST_HIT(ULKPG);
    BR14;
    return;
}
DEF_INST(ecpsvm_decode_next_ccw)
{
    ECPSVM_PROLOG(DNCCW);
    DEBUG_ASSIST(logmsg("HHCEV300D : DNCCW\n"));
}
DEF_INST(ecpsvm_free_ccwstor)
{
    ECPSVM_PROLOG(FCCWS);
    DEBUG_ASSIST(logmsg("HHCEV300D : FCCWST\n"));
}
DEF_INST(ecpsvm_locate_vblock)
{
    ECPSVM_PROLOG(SCNVU);
    DEBUG_ASSIST(logmsg("HHCEV300D : SCNVU\n"));
}
DEF_INST(ecpsvm_disp1)
{
    ECPSVM_PROLOG(DISP1);
    DEBUG_ASSIST(logmsg("HHCEV300D : DISP 1\n"));
}
DEF_INST(ecpsvm_tpage)
{
    ECPSVM_PROLOG(TRBRG);
    DEBUG_ASSIST(logmsg("HHCEV300D : TRANBRNG\n"));
}
DEF_INST(ecpsvm_tpage_lock)
{
    ECPSVM_PROLOG(TRLOK);
    DEBUG_ASSIST(logmsg("HHCEV300D : TRANLOCK\n"));
}
DEF_INST(ecpsvm_inval_segtab)
{
    ECPSVM_PROLOG(VIST);
    DEBUG_ASSIST(logmsg("HHCEV300D : ISTBL\n"));
}
DEF_INST(ecpsvm_inval_ptable)
{
    ECPSVM_PROLOG(VIPT);
    DEBUG_ASSIST(logmsg("HHCEV300D : IPTBL\n"));
}
DEF_INST(ecpsvm_decode_first_ccw)
{
    ECPSVM_PROLOG(DFCCW);
    DEBUG_ASSIST(logmsg("HHCEV300D : DFCCW\n"));
}
DEF_INST(ecpsvm_dispatch_main)
{
    VMBLOK      *vmb;
    ECPSVM_PROLOG(DISP0);
    DEBUG_ASSIST(logmsg("HHCEV300D : DISP 0\n"));
}

/******************************************************/
/* SCNRU Instruction : Scan Real Unit                 */
/* Invoked by DMKSCN                                  */
/* E60D                                               */
/* SCNRU D1(B1,I1),D2(B2,I2)                          */
/*                                                    */
/* Operations                                         */
/* The Device Address specified in operand 1 is       */
/* the real device address for which control block    */
/* addresses are to be returned. The storage area     */
/* designated as the 2nd operand is a list of 4       */
/* consecutive fullword (note : the 2nd operand is    */
/* treated as a Real Address, regardless of any       */
/* translation mode that may be in effect)            */
/* +--------------------+-------------------+         */
/* | CHNLINDEX          | RCHTABLE          |         */
/* +--------------------+-------------------+         */
/* | RCUTABLE           | RDVTABLE          |         */
/* +--------------------+-------------------+         */
/* CHNLINDEX is an array of 16 halfwords, each        */
/* representing the offset of the target's device     */
/* channel's RCHBLOCK. If the channel is not defined  */
/* in the RIO table, the index has bit 0, byte 0 set  */
/* to 1                                               */
/*                                                    */
/* The RCHBLOK has at offset +X'20' a table of        */
/* 32 possible control unit indices                   */
/* the device's address bit 8 to 12 are used to fetch */
/* the index specified at the table. If it has        */
/* bit 0 byte 0 set, then the same operation is       */
/* attempted with bits 8 to 11                        */
/* The RCUBLOK then fetched from RCUTABLE + the index */
/* fetched from the RCHBLOK has a DEVICE index table  */
/* at offset +X'28' which can be fetched by using     */
/* bits 5-7                                           */
/* If the RCUBLOK designates an alternate control     */
/* unit block, (Offset +X'5' bit 1 set), then         */
/* the primary RCUBLOK is fetched from offset X'10'   */
/*                                                    */
/* If no RCHBLOK is found, R6,R7 and R8 contain -1    */
/* and Condition Code 3 is set                        */
/*                                                    */
/* if no RCUBLOK is found, R6 contains the RCHBLOK,   */
/* R7 and R8 contain -1, and Condition Code 2 is set  */
/*                                                    */
/* if no RDVBLOK is found, R6 contains the RCHBLOK,   */
/* R7 contains the RCUBLOK and R8 contains -1, and    */
/* condition code 1 is set                            */
/*                                                    */
/* If all 3 control blocks are found, R6 contains the */
/* the RCHBLOK, R7 Contains the RCUBLOK and R8 cont-  */
/* ains the RDVBLOK. Condition code 0 is set          */
/*                                                    */
/* If the instruction is sucesfull, control is        */
/* returned at the address specified by GPR14.        */
/* Otherwise, the next sequential instruction is      */
/* executed, and no GPR or condition code is changed. */
/*                                                    */
/* Exceptions :                                       */
/*       Operation Exception : ECPS:VM Disabled       */
/*       Priviledged Exception : PSW in problem state */
/*                                                    */
/* Note : no access exception is generated for        */
/*        the second operand.                         */
/*                                                    */
/* Note : as of yet, for any other situation than     */
/*        finding all 3 control blocks, SCNRU acts    */
/*        as a NO-OP                                  */
/******************************************************/
DEF_INST(ecpsvm_locate_rblock)
{
    U16 chix;           /* offset of RCH in RCH Array */
    U16 cuix;           /* Offset of RCU in RCU Array */
    U16 dvix;           /* Offset of RDV in RDV Array */
    VADR rchixtbl;      /* RCH Index Table */
    VADR rchtbl;        /* RCH Array */
    VADR rcutbl;        /* RCU Array */
    VADR rdvtbl;        /* RDV Array */
    VADR arioct;        /* Data list for SCNRU */

    VADR rchblk;        /* Effective RCHBLOK Address */
    VADR rcublk;        /* Effective RCUBLOK Address */
    VADR rdvblk;        /* Effective RDVBLOK Address */
    U16 rdev;

    ECPSVM_PROLOG(SCNRU);

    /* Obtain the Device address */
    rdev=(effective_addr1 & 0xfff);
    /* And the DMKRIO tables addresses */
    arioct=effective_addr2;

    DEBUG_ASSIST(logmsg("HHCEV300D : ECPS:VM SCNRU called; RDEV=%4.4X ARIOCT=%6.6X\n",effective_addr1,arioct));

    /* Get the Channel Index Table */
    rchixtbl= EVM_L(effective_addr2);

    /* Obtain the RCH offset */
    chix=EVM_LH(rchixtbl+((rdev & 0xf00) >> 7));

    // logmsg("HHCEV300D : ECPS:VM SCNRU : RCH IX = %x\n",chix);

    /* Check if Bit 0 set (no RCH) */
    if(chix & 0x8000)
    {
        // logmsg("HHCEV300D : ECPS:VM SCNRU : NO CHANNEL\n");
        /*
        regs->GR_L(6)=~0;
        regs->GR_L(7)=~0;
        regs->GR_L(8)=~0;
        regs->psw.IA=regs->GR_L(14) & ADDRESS_MAXWREP(regs);
        regs->psw.cc=1;
        */
        /* Right now, let CP handle the case */
        return;
    }

    /* Obtain the RCH Table pointer */
    rchtbl=EVM_L(arioct+4);

    /* Add the RCH Index offset */
    rchblk=rchtbl+chix;

    /* Try to obtain RCU index with bits 8-12 of the device */
    cuix=EVM_LH(rchblk+0x20+((rdev & 0xf8)>>2));
    if(cuix & 0x8000)
    {
        /* Try with bits 8-11 */
        cuix=EVM_LH(rchblk+0x20+((rdev & 0xf0)>>2));
        if(cuix & 0x8000)
        {
            // logmsg("HHCEV300D : ECPS:VM SCNRU : NO CONTROL UNIT\n");
            /*
            regs->GR_L(6)=rchblk;
            regs->GR_L(7)=~0;
            regs->GR_L(8)=~0;
            regs->psw.IA=regs->GR_L(14) & ADDRESS_MAXWREP(regs);
            regs->psw.cc=2;
            */
            return;
        }
    }
    // logmsg("HHCEV300D : ECPS:VM SCNRU : RCU IX = %x\n",cuix);
    rcutbl=EVM_L(arioct+8);
    rcublk=rcutbl+cuix;
    dvix=EVM_LH(rcublk+0x28+((rdev & 0x00f)<<1));
    dvix<<=3;
    // logmsg("HHCEV300D : ECPS:VM SCNRU : RDV IX = %x\n",dvix);
    if(EVM_IC(rcublk+5)&0x40)
    {
        rcublk=EVM_L(rcublk+0x10);
    }
    if(dvix & 0x8000)
    {
        // logmsg("HHCEV300D : ECPS:VM SCNRU : NO RDEVBLOK\n");
        /*
        regs->GR_L(6)=rchblk;
        regs->GR_L(7)=rcublk;
        regs->GR_L(8)=~0;
        regs->psw.IA=regs->GR_L(14) & ADDRESS_MAXWREP(regs);
        regs->psw.cc=3;
        */
        return;
    }
    rdvtbl=EVM_L(arioct+12);
    rdvblk=rdvtbl+dvix;
    DEBUG_ASSIST(logmsg("HHCEV300D : ECPS:VM SCNRU : RCH = %6.6X, RCU = %6.6X, RDV = %6.6X\n",rchblk,rcublk,rdvblk));
    regs->GR_L(6)=rchblk;
    regs->GR_L(7)=rcublk;
    regs->GR_L(8)=rdvblk;
    regs->psw.cc=0;
    regs->GR_L(15)=0;
    BR14;
    CPASSIST_HIT(SCNRU);
}
DEF_INST(ecpsvm_comm_ccwproc)
{
    ECPSVM_PROLOG(CCWGN);
    DEBUG_ASSIST(logmsg("HHCEV300D : CCWGN\n"));
}
DEF_INST(ecpsvm_unxlate_ccw)
{
    ECPSVM_PROLOG(UXCCW);
    DEBUG_ASSIST(logmsg("HHCEV300D : UNCCW\n"));
}
DEF_INST(ecpsvm_disp2)
{
    ECPSVM_PROLOG(DISP2);
    DEBUG_ASSIST(logmsg("HHCEV300D : DISP 2\n"));
}
DEF_INST(ecpsvm_store_level)
{
    ECPSVM_PROLOG(STEVL);
    EVM_ST(sysblk.ecpsvm.level,effective_addr1);
    DEBUG_ASSIST(logmsg("HHCEV300D : ECPS:VM STORE LEVEL %d called\n",sysblk.ecpsvm.level));
}
DEF_INST(ecpsvm_loc_chgshrpg)
{
    ECPSVM_PROLOG(LCSPG);
    DEBUG_ASSIST(logmsg("HHCEV300D : LCHSHPG\n"));
}
DEF_INST(ecpsvm_extended_freex)
{
    U32 maxdw;
    U32 numdw;
    U32 maxsztbl;
    U32 spixtbl;
    BYTE spix;
    U32 freeblock;
    U32 nextblk;
    ECPSVM_PROLOG(FREEX);
    numdw=regs->GR_L(0);
    spixtbl=effective_addr2;
    maxsztbl=effective_addr1;
    DEBUG_ASSIST(logmsg("HHCEV300D : ECPS:VM FREEX DW = %4.4X\n",numdw));
    if(numdw==0)
    {
        return;
    }
    DEBUG_ASSIST(logmsg("HHCEV300D : MAXSIZE ADDR = %6.6X, SUBPOOL INDEX TABLE = %6.6X\n",maxsztbl,spixtbl));
    /* E1 = @ of MAXSIZE (maximum # of DW allocatable by FREEX from subpools) */
    /*      followed by subpool pointers                                      */
    /* E2 = @ of subpool indices                                              */
    maxdw=EVM_L(maxsztbl);
    if(regs->GR_L(0)>maxdw)
    {
        DEBUG_ASSIST(logmsg("HHCEV300D : FREEX request beyond subpool capacity\n"));
        return;
    }
    /* Fetch subpool index */
    spix=EVM_IC(spixtbl+numdw);
    DEBUG_ASSIST(logmsg("HHCEV300D : Subpool index = %X\n",spix));
    /* Fetch value */
    freeblock=EVM_L(maxsztbl+4+spix);
    DEBUG_ASSIST(logmsg("HHCEV300D : Value in subpool table = %6.6X\n",freeblock));
    if(freeblock==0)
    {
        /* Can't fullfill request here */
        return;
    }
    nextblk=EVM_L(freeblock);
    EVM_ST(nextblk,maxsztbl+4+spix);
    DEBUG_ASSIST(logmsg("HHCEV300D : New Value in subpool table = %6.6X\n",nextblk));
    regs->GR_L(1)=freeblock;
    regs->psw.cc=0;
    BR14;
    CPASSIST_HIT(FREEX);
    return;
}
DEF_INST(ecpsvm_extended_fretx)
{
    U32 fretl;
    U32 cortbl;
    U32 maxsztbl;
    U32 maxdw;
    U32 numdw;
    U32 block;
    U32 cortbe; /* Core table Page entry for fretted block */
    U32 prevblk;
    BYTE spix;
    ECPSVM_PROLOG(FRETX);
    numdw=regs->GR_L(0);
    block=regs->GR_L(1) & ADDRESS_MAXWRAP(regs);
    maxsztbl=effective_addr1 & ADDRESS_MAXWRAP(regs);
    fretl=effective_addr2 & ADDRESS_MAXWRAP(regs);
    DEBUG_ASSIST(logmsg("HHCEV300D : X fretx called AREA=%6.6X, DW=%4.4X\n",regs->GR_L(1),regs->GR_L(0)));
    if(numdw==0)
    {
        DEBUG_ASSIST(logmsg("HHCEV300D : ECPS:VM Cannot FRETX : DWORDS = 0\n"));
        return;
    }
    maxdw=EVM_L(maxsztbl);
    if(numdw>maxdw)
    {
        DEBUG_ASSIST(logmsg("HHCEV300D : ECPS:VM Cannot FRETX : DWORDS = %d > MAXDW %d\n",numdw,maxdw));
        return;
    }
    cortbl=EVM_L(fretl);
    cortbe=cortbl+((block & 0xfff000)>>8);
    if(EVM_L(cortbe)!=EVM_L(fretl+4))
    {
        DEBUG_ASSIST(logmsg("HHCEV300D : ECPS:VM Cannot FRETX : Area not in Core Free area\n"));
        return;
    }
    if(EVM_IC(cortbe+8)!=0x02)
    {
        DEBUG_ASSIST(logmsg("HHCEV300D : ECPS:VM Cannot FRETX : Area flag != 0x02\n"));
        return;
    }
    spix=EVM_IC(fretl+11+numdw);
    prevblk=EVM_L(maxsztbl+4+spix);
    if(prevblk==block)
    {
        DEBUG_ASSIST(logmsg("HHCEV300D : ECPS:VM Cannot FRETX : fretted block already on subpool chain\n"));
        return;
    }
    EVM_ST(block,maxsztbl+4+spix);
    EVM_ST(prevblk,block);
    BR14;
    CPASSIST_HIT(FRETX);
    return;
}
DEF_INST(ecpsvm_prefmach_assist)
{
    ECPSVM_PROLOG(PMASS);
    DEBUG_ASSIST(logmsg("HHCEV300D : PMA\n"));
}

#undef DEBUG_ASSIST
#ifdef DEBUG_SASSIST
#define DEBUG_ASSIST(x) ( x )
#else
#define DEBUG_ASSIST(x)
#endif
/**********************************/
/* VM ASSIST                      */
/**********************************/

/********************************************************/
/* Why I can't use vfetchX/vstoreX accesses from here : */
/*   The PSW key is set according to the virtual machine*/
/*   provided PSW Key. But the assist is going to access*/
/*   real storage for it's own purpose.                 */
/*   if the Virtual Machine has set a certain key, then */
/*   attempting to fetch from a storage area with fetch */
/*   control protection or to store from an area with   */
/*   a different storage key will yield a protection    */
/*   exception.                                         */
/*   Protection exception should only occur             */
/*   when the target addresses of the instruction       */
/*   are really protected                               */
/********************************************************/

/* Check psw Transition validity */
/* NOTE : oldr/newr Only have the PSW field valid (the rest is not initialised) */
int     ecpsvm_check_pswtrans(REGS *regs,VADR micblok, BYTE micpend, REGS *oldr, REGS *newr)
{
    /* Check for a switch from BC->EC or EC->BC */
    if(oldr->psw.ecmode!=newr->psw.ecmode)
    {
        return(1);
    }
    /* Check if PER or DAT is being changed */
    if(newr->psw.ecmode)
    {
        if((newr->psw.sysmask & 0x44) != (oldr->psw.sysmask & 0x44))
        {
            return(1);
        }
    }
    /* Check if a Virtual interrupt is pending and new interrupts are being enabled */
    if(micpend & 0x80)
    {
        if(newr->psw.ecmode)
        {
            if(((~oldr->psw.sysmask) & 0x03) & newr->psw.sysmask)
            {
                return(1);
            }
        }
        else
        {
            if(~oldr->psw.sysmask & newr->psw.sysmask)
            {
                return(1);
            }
        }
    }
    /* Check NEW PSW Validity */
    if(newr->psw.ecmode)
    {
        if(newr->psw.sysmask & 0xb8)
        {
            return(1);
        }
    }
    if(newr->psw.IA & 0x01)
    {
        return(1);
    }
    return(0);
}
int     ecpsvm_dossm(REGS *regs,VADR effective_addr2,int b2)
{
    BYTE  reqmask;
    RADR  cregs;
    U32   creg0;
    REGS  npregs;

    SASSIST_PROLOG(SSM);


    /* Reject if V PSW is in problem state */
    if(vpregs.psw.prob)
    {
        DEBUG_ASSIST(logmsg("HHCEV300D : SASSIST SSM reject : V PB State\n"));
        return(1);
    }
    /* Get CR0 - set ref bit on  fetched CR0 (already done in prolog for MICBLOK) */
    FETCH_FW(cregs,&regs->mainstor[micblok+4]);
    LOGICAL_TO_ABS(cregs,USE_REAL_ADDR,regs,ACCTYPE_READ,0);
    FETCH_FW(creg0,&regs->mainstor[cregs]);

    /* Reject if V CR0 specifies SSM Suppression */
    if(creg0 & 0x40000000)
    {
        DEBUG_ASSIST(logmsg("HHCEV300D : SASSIST SSM reject : V SSM Suppr\n"));
        return(1);
    }
    /* Load the requested SSM Mask */
    /* USE Normal vfetchb here ! not only do we want tranlsation */
    /* but also fetch protection control, ref bit, etc.. */
    reqmask=ARCH_DEP(vfetchb) (effective_addr2,b2,regs);

    INITSIESTATE(npregs);
    /* Load the virtual PSW AGAIN in a new structure */

    ARCH_DEP(load_psw) (&npregs,&regs->mainstor[vpswa]);

    npregs.psw.sysmask=reqmask;
    /* While we are at it, set the IA in the V PSW */
    npregs.psw.IA=regs->psw.IA;

    if(ecpsvm_check_pswtrans(regs,micblok,micpend,&vpregs,&npregs))       /* Check PSW transition capability */
    {
        DEBUG_ASSIST(logmsg("HHCEV300D : SASSIST SSM Reject : New PSW too complex\n"));
        return(1); /* Something in the NEW PSW we can't handle.. let CP do it */
    }

    /* Set the change bit */
    LOGICAL_TO_ABS(vpswa,USE_REAL_ADDR,regs,ACCTYPE_WRITE,0);
    /* store the new PSW */
    ARCH_DEP(store_psw) (&npregs,&regs->mainstor[vpswa]);
    DEBUG_ASSIST(logmsg("HHCEV300D : SASSIST SSM Complete : new SM = %2.2X\n",reqmask));
    SASSIST_HIT(SSM);
    return(0);
}

int     ecpsvm_dosvc(REGS *regs,int svccode)
{
    PSA_3XX *psa;
    REGS newr;

    SASSIST_PROLOG(SVC);

    if(svccode==76)     /* NEVER trap SVC 76 */
    {
        DEBUG_ASSIST(logmsg("HHCEV300D : SASSIST SVC Reject : SVC 76\n"));
        return(1);
    }
#if defined(_FEATURE_SIE)
    newr.sie_state=0;
#endif
    if(regs->CR_L(6) & 0x08000000)
    {
        DEBUG_ASSIST(logmsg("HHCEV300D : SASSIST SVC Reject : SVC Assist Inhibit\n"));
        return(1);      /* SVC SASSIST INHIBIT ON */
    }
    /* Get what the NEW PSW should be */

    psa=(PSA_3XX *)&regs->mainstor[LOGICAL_TO_ABS((VADR)0 , USE_PRIMARY_SPACE, regs, ACCTYPE_READ, 0)];
                                                                                         /* Use all around access key 0 */
                                                                                         /* Also sets reference bit     */
    ARCH_DEP(load_psw) (&newr, (BYTE *)&psa->svcnew);   /* Ref bit set above */
    DEBUG_ASSIST(logmsg("HHCEV300D : SASSIST SVC NEW VIRT "));
    DEBUG_ASSIST(display_psw(&newr));
    /* Get some stuff from the REAL Running PSW to put in OLD SVC PSW */
    vpregs.psw.IA=regs->psw.IA;                   /* Instruction Address */
    vpregs.psw.cc=regs->psw.cc;                   /* Condition Code      */
    vpregs.psw.pkey=regs->psw.pkey;               /* Protection Key      */
    vpregs.psw.domask=regs->psw.domask;           /* Dec Overflow        */
    vpregs.psw.fomask=regs->psw.fomask;           /* Fixed Pt Overflow   */
    vpregs.psw.eumask=regs->psw.eumask;           /* Exponent Underflow  */
    vpregs.psw.sgmask=regs->psw.sgmask;           /* Significance        */
    vpregs.psw.intcode=svccode;                   /* SVC Interrupt code  */
    DEBUG_ASSIST(logmsg("HHCEV300D : SASSIST SVC OLD VIRT "));
    DEBUG_ASSIST(display_psw(&vpregs));

    if(ecpsvm_check_pswtrans(regs,micblok,micpend,&vpregs,&newr))       /* Check PSW transition capability */
    {
        DEBUG_ASSIST(logmsg("HHCEV300D : SASSIST SVC Reject : Cannot make transition to new PSW from microcode\n"));
        return(1); /* Something in the NEW PSW we can't handle.. let CP do it */
    }
    /* Store the OLD SVC PSW */
    psa=(PSA_3XX *)&regs->mainstor[LOGICAL_TO_ABS((VADR)0, USE_PRIMARY_SPACE, regs, ACCTYPE_WRITE, 0)];
                                                                                         /* Use all around access key 0 */
                                                                                         /* Also sets change bit        */
    /* Set intcode in PSW (for BC mode) */


    ARCH_DEP(store_psw) (&vpregs, (BYTE *)&psa->svcold);

    if(vpregs.psw.ecmode)
    {
        /* Also set SVC interrupt code */
        /* and ILC                     */
        STORE_FW((BYTE *)&psa->svcint,0x00020000 | svccode);
    }
    /* 
     * Now, update some stuff in the REAL PSW
     */
    regs->psw.cc=newr.psw.cc;           /* Update Condition code  */
    regs->psw.pkey=newr.psw.pkey;       /* Update PSW Storage Key */
    regs->psw.IA=newr.psw.IA;           /* Update IA              */
    regs->psw.fomask=newr.psw.fomask;   /* Fixed Point Overflow   */
    regs->psw.domask=newr.psw.domask;   /* Decimal Overflow       */
    regs->psw.eumask=newr.psw.eumask;   /* Exponent Underflow     */
    regs->psw.sgmask=newr.psw.sgmask;   /* Significance           */
    /*
     * Now store the new PSW in the area pointed by the MICBLOK
     */
    ARCH_DEP(store_psw) (&newr,regs->mainstor+vpswa);
    DEBUG_ASSIST(logmsg("HHCEV300D : SASSIST SVC Done\n"));
    SASSIST_HIT(SVC);
    return(0);
}

/* SHOW STATS */

void ecpsvm_showstats(void)
{
    char *sep="HHCEV003I +-----------+----------+----------+-------+\n";
    char nname[32];
    size_t  nument;
    int  havedisp=0;
    int  notshown=0;
    size_t unsupcc=0;
    ECPSVM_STATARRAY *ar;
    size_t i;
    ar=(ECPSVM_STATARRAY *)&ecpsvm_stats;
    logmsg(sep);
    logmsg("HHCEV002I | %-9s | %-8s | %-8s | %-5s |\n","Function","Calls","Hits","Ratio");
    logmsg(sep);
    nument=(sizeof(struct _ECPSVM_STATS)/sizeof(ECPSVM_STAT));
    for(i=0;i<nument;i++)
    {
        if(ar->s[i].total && havedisp)
        {
                logmsg(sep);
        }
        if(ar->s[i].call || ar->s[i].total)
        {
            if(!ar->s[i].support)
            {
                unsupcc+=ar->s[i].call;
            }
            havedisp=1;
            snprintf(nname,32,"%s%s",ar->s[i].name,ar->s[i].support ? "" : "*");
            if(ar->s[i].total)
            {
                strcat(nname,"+");
            }
            logmsg("HHCEV001I | %-9s | %8d | %8d |  %3d%% |\n",
                    nname,
                    ar->s[i].call,
                    ar->s[i].hit,
                    ar->s[i].call ?
                            (ar->s[i].hit*100)/ar->s[i].call :
                            100);
        }
        else
        {
            notshown++;
        }
        if(ar->s[i].total && i!=(nument-1))
        {
                logmsg(sep);
        }
    }
    logmsg(sep);
    logmsg("HHCEV004I (*) Unsupported; (+) Category summary.\n");
    if(notshown)
    {
        logmsg("HHCEV005I %d Entr%s not shown (never invoked)\n",notshown,notshown==1?"y":"ies");
    }
    if(unsupcc)
    {
        if(unsupcc==1)
        {
                logmsg("HHCEV006I 1 call was made to an unsupported function\n");
        }
        else
        {
            logmsg("HHCEV006I %d calls where made to unsupported functions\n",unsupcc);
        }
    }
    return;
}

#endif /* ifdef FEATURE_ECPSVM */

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

static ECPSVM_CMDENT ecpsvm_cmdtab[];
ECPSVM_CMDENT *ecpsvm_getcmdent(char *cmd);

struct _ECPSVM_CPSTATS
{
    ECPSVM_STAT_DCL(SVC);
    ECPSVM_STAT_DCL(SSM);
    ECPSVM_STAT_DCL(LPSW);
    ECPSVM_STAT_DCL(STNSM);
    ECPSVM_STAT_DCL(STOSM);
    ECPSVM_STAT_DCL(SIO);
} ecpsvm_sastats={
    ECPSVM_STAT_DEF(SVC),
    ECPSVM_STAT_DEF(SSM),
    ECPSVM_STAT_DEF(LPSW),
    ECPSVM_STAT_DEFU(STNSM),
    ECPSVM_STAT_DEFU(STOSM),
    ECPSVM_STAT_DEFU(SIO),
};
struct _ECPSVM_SASTATS
{
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
} ecpsvm_cpstats={
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
    ECPSVM_STAT_DEF(TRBRG),
    ECPSVM_STAT_DEF(TRLOK),
    ECPSVM_STAT_DEFU(VIST),
    ECPSVM_STAT_DEFU(VIPT),
    ECPSVM_STAT_DEF(STEVL),
    ECPSVM_STAT_DEF(FREEX),
    ECPSVM_STAT_DEF(FRETX),
    ECPSVM_STAT_DEFU(PMASS),
    ECPSVM_STAT_DEFU(LCSPG),
};

#define DEBUG_CPASSIST
#define DEBUG_SASSIST

#define DODEBUG_ASSIST(_cond,x)  \
{ \
    if((_cond)) \
    { \
        (x); \
    }\
}

#if defined(DEBUG_SASSIST)
#define DEBUG_SASSISTX(_inst,x) \
{ \
    DODEBUG_ASSIST(ecpsvm_sastats._inst.debug,x) \
}
#else
#define DEBUG_SASSISTX(_cond,x)
#endif
#if defined(DEBUG_CPASSIST)
#define DEBUG_CPASSISTX(_inst,x) \
{ \
    DODEBUG_ASSIST(ecpsvm_cpstats._inst.debug,x) \
}
#else
#define DEBUG_CPASSISTX(_cond,x)
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

#define CPASSIST_HIT(_stat) ecpsvm_cpstats._stat.hit++

#define SASSIST_HIT(_stat) ecpsvm_sastats._stat.hit++

#define SASSIST_LPSW(_regs) \
        regs->psw.IA=_regs.psw.IA; \
        regs->psw.cc=_regs.psw.cc; \
        regs->psw.pkey=_regs.psw.pkey; \
        regs->psw.domask=_regs.psw.domask; \
        regs->psw.fomask=_regs.psw.fomask; \
        regs->psw.eumask=_regs.psw.eumask; \
        regs->psw.sgmask=_regs.psw.sgmask;

#define SASSIST_PROLOG( _instname ) \
    VADR amicblok; \
    VADR vpswa; \
    REGS vpregs; \
    BYTE  micpend; \
    U32   CR6; \
    ECPSVM_MICBLOK micblok; \
    BYTE micevma; \
    if(!sysblk.ecpsvm.available) \
    { \
          DEBUG_SASSISTX(_instname,logmsg("HHCEV300D : SASSIST "#_instname" ECPS:VM Disabled in configuration\n")); \
          return(1); \
    } \
    if(!ecpsvm_sastats._instname.enabled) \
    { \
          DEBUG_SASSISTX(_instname,logmsg("HHCEV300D : SASSIST "#_instname" ECPS:VM Disabled by command\n")); \
          return(1); \
    } \
    if(!regs->psw.prob) \
    { \
          return(1); \
    } \
    CR6=regs->CR_L(6); \
    if(!(CR6 & ECPSVM_CR6_VMASSIST)) \
    { \
        DEBUG_SASSISTX(_instname,logmsg("HHCEV300D : EVMA Disabled by guest\n")); \
        return(1); \
    } \
    amicblok=CR6 & ECPSVM_CR6_MICBLOK; \
    /* Ensure MICBLOK resides on a single page */ \
    /* Then set ref bit by calling LOG_TO_ABS */ \
    if((amicblok & 0x007ff) > 0x7e0) \
    { \
        DEBUG_SASSISTX(_instname,logmsg("HHCEV300D : SASSIST "#_instname" Micblok @ %6.6X crosses page frame\n",amicblok)); \
        return(1); \
    } \
    /* Increment call now (don't count early misses) */ \
    ecpsvm_sastats._instname.call++; \
    amicblok=LOGICAL_TO_ABS(amicblok,USE_REAL_ADDR,regs,ACCTYPE_READ,0); \
    /* Load the micblok copy */ \
    FETCH_FW(micblok.MICRSEG,regs->mainstor+amicblok); \
    FETCH_FW(micblok.MICCREG,regs->mainstor+amicblok+4); \
    FETCH_FW(micblok.MICVPSW,regs->mainstor+amicblok+8); \
    FETCH_FW(micblok.MICACF,regs->mainstor+amicblok+12); \
    micpend=(micblok.MICVPSW >> 24); \
    vpswa=micblok.MICVPSW & ADDRESS_MAXWRAP(regs); \
    micevma=(micblok.MICACF >> 24); \
    /* Set ref bit on page where Virtual PSW is stored */ \
    vpswa=LOGICAL_TO_ABS(vpswa,USE_REAL_ADDR,regs,ACCTYPE_READ,0); \
    /* Load the Virtual PSW in a temporary REGS structure */ \
    INITSIESTATE(vpregs); \
    ARCH_DEP(load_psw) (&vpregs,&regs->mainstor[vpswa]); \
    DEBUG_SASSISTX(_instname,logmsg("HHCEV300D : SASSIST "#_instname" VPSWA= %8.8X Virtual ",vpswa)); \
    DEBUG_SASSISTX(_instname,display_psw(&vpregs)); \
    DEBUG_SASSISTX(_instname,logmsg("HHCEV300D : SASSIST "#_instname" Real ")); \
    DEBUG_SASSISTX(_instname,display_psw(regs));

#define ECPSVM_PROLOG(_inst) \
int     b1, b2; \
VADR    effective_addr1, \
        effective_addr2; \
     SSE(inst, execflag, regs, b1, effective_addr1, b2, effective_addr2); \
     if(!sysblk.ecpsvm.available) \
     { \
          DEBUG_CPASSISTX(_inst,logmsg("HHCEV300D : CPASSTS "#_inst" ECPS:VM Disabled in configuration ")); \
          ARCH_DEP(program_interrupt) (regs, PGM_OPERATION_EXCEPTION); \
     } \
     if(!ecpsvm_cpstats._inst.enabled) \
     { \
          DEBUG_CPASSISTX(_inst,logmsg("HHCEV300D : CPASSTS "#_inst" Disabled by command")); \
          return; \
     } \
     PRIV_CHECK(regs); \
     if(!(regs->CR_L(6) & 0x02000000)) \
     { \
        return; \
     } \
     ecpsvm_cpstats._inst.call++; \
    DEBUG_CPASSISTX(_inst,logmsg("HHCEV300D : "#_inst" called\n"));

#ifdef FEATURE_ECPSVM

DEF_INST(ecpsvm_basic_freex)
{
    ECPSVM_PROLOG(FREE);
}
DEF_INST(ecpsvm_basic_fretx)
{
    ECPSVM_PROLOG(FRET);
}

static void ecpsvm_lockpage1(REGS *regs,RADR cortab,RADR pg)
{
    BYTE corcode;
    VADR corte;
    U32  lockcount;
    RADR cortbl;

    DEBUG_CPASSISTX(LCKPG,logmsg("HHCEV300D : LKPG coreptr = "F_RADR" Frame = "F_RADR"\n",cortab,pg));
    cortbl=EVM_L(cortab);
    corte=cortbl+((pg & 0xfff000)>>8);
    DEBUG_CPASSISTX(LCKPG,logmsg("HHCEV300D : LKPG corete = %6.6X\n",corte));
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
    DEBUG_CPASSISTX(LCKPG,logmsg("HHCEV300D : LKPG Page locked. Count = %6.6X\n",lockcount));
    return;
}
DEF_INST(ecpsvm_lock_page)
{
    VADR ptr_pl;
    VADR pg;

    ECPSVM_PROLOG(LCKPG);

    ptr_pl=effective_addr1;
    pg=effective_addr2;

    DEBUG_CPASSISTX(LCKPG,logmsg("HHCEV300D : LKPG PAGE=%6.6X, PTRPL=%6.6X\n",pg,ptr_pl));

    ecpsvm_lockpage1(regs,ptr_pl,pg);
    regs->psw.cc=0;
    BR14;
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

    DEBUG_CPASSISTX(ULKPG,logmsg("HHCEV300D : ULKPG PAGE=%6.6X, PTRPL=%6.6X\n",pg,ptr_pl));

    corsz=EVM_L(ptr_pl);
    cortbl=EVM_L(ptr_pl+4);
    if((pg+4095)>corsz)
    {
        DEBUG_CPASSISTX(ULKPG,logmsg("HHCEV300D : ULKPG Page beyond core size of %6.6X\n",corsz));
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
        DEBUG_CPASSISTX(ULKPG,logmsg("HHCEV300D : ULKPG Attempting to unlock page that is not locked\n"));
        return;
    }
    if(lockcount==0)
    {
        corcode &= ~(0x80|0x02);
        EVM_STC(corcode,corte+8);
        DEBUG_CPASSISTX(ULKPG,logmsg("HHCEV300D : ULKPG now unlocked\n"));
    }
    else
    {
        DEBUG_CPASSISTX(ULKPG,logmsg("HHCEV300D : ULKPG Page still locked. Count = %6.6X\n",lockcount));
    }
    EVM_ST(lockcount,corte+4);
    CPASSIST_HIT(ULKPG);
    BR14;
    return;
}
DEF_INST(ecpsvm_decode_next_ccw)
{
    ECPSVM_PROLOG(DNCCW);
}
DEF_INST(ecpsvm_free_ccwstor)
{
    ECPSVM_PROLOG(FCCWS);
}
DEF_INST(ecpsvm_locate_vblock)
{
    ECPSVM_PROLOG(SCNVU);
}
DEF_INST(ecpsvm_disp1)
{
    ECPSVM_PROLOG(DISP1);
}
static int ecpsvm_tranbrng(REGS *regs,VADR cortabad,VADR pgadd,RADR *raddr)
{
    int cc;
    U16 xcode;
    int private;
    int protect;
    int stid;
    int corcode;
#if defined(FEATURE_2K_STORAGE_KEYS)
    RADR pg1,pg2;
#endif
    VADR cortab;
    cc = ARCH_DEP(translate_addr) (pgadd , USE_PRIMARY_SPACE,  regs,
                  ACCTYPE_LRA, raddr , &xcode, &private, &protect, &stid);
    if(cc!=0)
    {
        DEBUG_CPASSISTX(TRBRG,logmsg("HHCEV300D : Tranbring : LRA cc = %d\n",cc));
        return(1);
    }
    /* Get the core table entry from the Real address */
    cortab=EVM_L( cortabad );
    cortab+=((*raddr) & 0xfff000) >> 8;
    corcode=EVM_IC(cortab+8);
    if(!(corcode & 0x08))
    {
        DEBUG_CPASSISTX(TRBRG,logmsg("HHCEV300D : Page not shared - OK %d\n",cc));
        return(0);      /* Page is NOT shared.. All OK */
    }
#if defined(FEATURE_2K_STORAGE_KEYS)
    pg1=(*raddr & 0xfff000);
    pg2=pg1+0x800;
    DEBUG_CPASSISTX(TRBRG,logmsg("HHCEV300D : Checking 2K Storage keys @"F_RADR" & "F_RADR"\n",pg1,pg2));
    if((STORAGE_KEY(pg1,regs) & STORKEY_CHANGE) ||
            (STORAGE_KEY(pg2,regs) & STORKEY_CHANGE))
    {
#else
    DEBUG_CPASSISTX(TRBRG,logmsg("HHCEV300D : Checking 4K Storage keys @"F_RADR"\n",*raddr));
    if(STORAGE_KEY(*raddr,regs) & STORKEY_CHANGE)
    {
#endif
        DEBUG_CPASSISTX(TRBRG,logmsg("HHCEV300D : Page shared and changed\n"));
        return(1);      /* Page shared AND changed */
    }
    DEBUG_CPASSISTX(TRBRG,logmsg("HHCEV300D : Page shared but not changed\n"));
    return(0);  /* All done */
}
DEF_INST(ecpsvm_tpage)
{
    int rc;
    RADR raddr;
    ECPSVM_PROLOG(TRBRG);
    DEBUG_CPASSISTX(TRBRG,logmsg("HHCEV300D : TRANBRNG\n"));
    rc=ecpsvm_tranbrng(regs,effective_addr1,regs->GR_L(1),&raddr);
    if(rc)
    {
        DEBUG_CPASSISTX(TRBRG,logmsg("HHCEV300D : TRANBRNG - Back to CP\n"));
        return; /* Something not right : NO OP */
    }
    regs->psw.cc=0;
    regs->GR_L(2)=raddr;
    regs->psw.IA = effective_addr2 & ADDRESS_MAXWRAP(regs);
    CPASSIST_HIT(TRBRG);
    return;
}
DEF_INST(ecpsvm_tpage_lock)
{
    int rc;
    RADR raddr;
    ECPSVM_PROLOG(TRLOK);
    DEBUG_CPASSISTX(TRLOK,logmsg("HHCEV300D : TRANLOCK\n"));
    rc=ecpsvm_tranbrng(regs,effective_addr1,regs->GR_L(1),&raddr);
    if(rc)
    {
        DEBUG_CPASSISTX(TRLOK,logmsg("HHCEV300D : TRANLOCK - Back to CP\n"));
        return; /* Something not right : NO OP */
    }
    /*
     * Lock the page in Core Table
     */
    ecpsvm_lockpage1(regs,effective_addr1,raddr);
    regs->psw.cc=0;
    regs->GR_L(2)=raddr;
    regs->psw.IA = effective_addr2 & ADDRESS_MAXWRAP(regs);
    CPASSIST_HIT(TRLOK);
    return;
}
DEF_INST(ecpsvm_inval_segtab)
{
    ECPSVM_PROLOG(VIST);
}
DEF_INST(ecpsvm_inval_ptable)
{
    ECPSVM_PROLOG(VIPT);
}
DEF_INST(ecpsvm_decode_first_ccw)
{
    ECPSVM_PROLOG(DFCCW);
}
DEF_INST(ecpsvm_dispatch_main)
{
    ECPSVM_PROLOG(DISP0);
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

    DEBUG_CPASSISTX(SCNRU,logmsg("HHCEV300D : ECPS:VM SCNRU called; RDEV=%4.4X ARIOCT=%6.6X\n",effective_addr1,arioct));

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
    DEBUG_CPASSISTX(SCNRU,logmsg("HHCEV300D : ECPS:VM SCNRU : RCH = %6.6X, RCU = %6.6X, RDV = %6.6X\n",rchblk,rcublk,rdvblk));
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
}
DEF_INST(ecpsvm_unxlate_ccw)
{
    ECPSVM_PROLOG(UXCCW);
}
DEF_INST(ecpsvm_disp2)
{
    ECPSVM_PROLOG(DISP2);
}
DEF_INST(ecpsvm_store_level)
{
    ECPSVM_PROLOG(STEVL);
    EVM_ST(sysblk.ecpsvm.level,effective_addr1);
    DEBUG_CPASSISTX(STEVL,logmsg("HHCEV300D : ECPS:VM STORE LEVEL %d called\n",sysblk.ecpsvm.level));
    CPASSIST_HIT(STEVL);
}
DEF_INST(ecpsvm_loc_chgshrpg)
{
    ECPSVM_PROLOG(LCSPG);
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
    DEBUG_CPASSISTX(FREEX,logmsg("HHCEV300D : ECPS:VM FREEX DW = %4.4X\n",numdw));
    if(numdw==0)
    {
        return;
    }
    DEBUG_CPASSISTX(FREEX,logmsg("HHCEV300D : MAXSIZE ADDR = %6.6X, SUBPOOL INDEX TABLE = %6.6X\n",maxsztbl,spixtbl));
    /* E1 = @ of MAXSIZE (maximum # of DW allocatable by FREEX from subpools) */
    /*      followed by subpool pointers                                      */
    /* E2 = @ of subpool indices                                              */
    maxdw=EVM_L(maxsztbl);
    if(regs->GR_L(0)>maxdw)
    {
        DEBUG_CPASSISTX(FREEX,logmsg("HHCEV300D : FREEX request beyond subpool capacity\n"));
        return;
    }
    /* Fetch subpool index */
    spix=EVM_IC(spixtbl+numdw);
    DEBUG_CPASSISTX(FREEX,logmsg("HHCEV300D : Subpool index = %X\n",spix));
    /* Fetch value */
    freeblock=EVM_L(maxsztbl+4+spix);
    DEBUG_CPASSISTX(FREEX,logmsg("HHCEV300D : Value in subpool table = %6.6X\n",freeblock));
    if(freeblock==0)
    {
        /* Can't fullfill request here */
        return;
    }
    nextblk=EVM_L(freeblock);
    EVM_ST(nextblk,maxsztbl+4+spix);
    DEBUG_CPASSISTX(FREEX,logmsg("HHCEV300D : New Value in subpool table = %6.6X\n",nextblk));
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
    DEBUG_CPASSISTX(FRETX,logmsg("HHCEV300D : X fretx called AREA=%6.6X, DW=%4.4X\n",regs->GR_L(1),regs->GR_L(0)));
    if(numdw==0)
    {
        DEBUG_CPASSISTX(FRETX,logmsg("HHCEV300D : ECPS:VM Cannot FRETX : DWORDS = 0\n"));
        return;
    }
    maxdw=EVM_L(maxsztbl);
    if(numdw>maxdw)
    {
        DEBUG_CPASSISTX(FRETX,logmsg("HHCEV300D : ECPS:VM Cannot FRETX : DWORDS = %d > MAXDW %d\n",numdw,maxdw));
        return;
    }
    cortbl=EVM_L(fretl);
    cortbe=cortbl+((block & 0xfff000)>>8);
    if(EVM_L(cortbe)!=EVM_L(fretl+4))
    {
        DEBUG_CPASSISTX(FRETX,logmsg("HHCEV300D : ECPS:VM Cannot FRETX : Area not in Core Free area\n"));
        return;
    }
    if(EVM_IC(cortbe+8)!=0x02)
    {
        DEBUG_CPASSISTX(FRETX,logmsg("HHCEV300D : ECPS:VM Cannot FRETX : Area flag != 0x02\n"));
        return;
    }
    spix=EVM_IC(fretl+11+numdw);
    prevblk=EVM_L(maxsztbl+4+spix);
    if(prevblk==block)
    {
        DEBUG_CPASSISTX(FRETX,logmsg("HHCEV300D : ECPS:VM Cannot FRETX : fretted block already on subpool chain\n"));
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
}

#undef DEBUG_ASSIST
#if defined(DEBUG_SASSIST)
#define DEBUG_ASSIST(x) DODEBUG_ASSIST(1,x)
#else
#define DEBUG_ASSIST(x)
#endif

/**********************************/
/* VM ASSISTS                     */
/**********************************/

/******************************************************************/
/* LPSW/SSM/STxSM :                                               */
/* Not sure about the current processing ..                       */
/* *MAYBE* we need to invoke DMKDSPCH when the newly loaded PSW   */
/* does not need further checking. Now.. I wonder what the point  */
/* is to return to CP anyway, as we have entirelly validated the  */
/* new PSW (i.e. for most of it, this is essentially a BRANCH     */
/* However...                                                     */
/* Maybe we should call DMKDSPCH (from the DMKPRVMA list)         */
/* only if re-enabling bits (and no Int pending)                  */
/*                                                                */
/* For the time being, we do THIS :                               */
/* If the new PSW 'disables' bits or enables bit but MICPEND=0    */
/* we just update the VPSW and continue                           */
/* Same for LPSW.. But we also update the IA                      */
/* If we encounter ANY issue, we just return to caller (which will*/
/* generate a PRIVOP) thus invoking CP like for non-EVMA          */
/******************************************************************/

/******************************************************************/
/* Check psw Transition validity                                  */
/******************************************************************/
/* NOTE : oldr/newr Only have the PSW field valid (the rest is not initialised) */
int     ecpsvm_check_pswtrans(REGS *regs,ECPSVM_MICBLOK *micblok, BYTE micpend, REGS *oldr, REGS *newr)
{
    UNREFERENCED(micblok);
    UNREFERENCED(regs);
    /* Check for a switch from BC->EC or EC->BC */
    if(oldr->psw.ecmode!=newr->psw.ecmode)
    {
        DEBUG_SASSISTX(LPSW,logmsg("HHCEV300D : New and Old PSW have a EC/BC transition\n"));
        return(1);
    }
    /* Check if PER or DAT is being changed */
    if(newr->psw.ecmode)
    {
        if((newr->psw.sysmask & 0x44) != (oldr->psw.sysmask & 0x44))
        {
            DEBUG_SASSISTX(LPSW,logmsg("HHCEV300D : New PSW Enables DAT or PER\n"));
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
                DEBUG_SASSISTX(LPSW,logmsg("HHCEV300D : New PSW Enables interrupts and MICPEND (EC)\n"));
                return(1);
            }
        }
        else
        {
            if(~oldr->psw.sysmask & newr->psw.sysmask)
            {
                DEBUG_SASSISTX(LPSW,logmsg("HHCEV300D : New PSW Enables interrupts and MICPEND (BC)\n"));
                return(1);
            }
        }
    }
    if(newr->psw.wait)
    {
        DEBUG_SASSISTX(LPSW,logmsg("HHCEV300D : New PSW is a WAIT PSW\n"));
        return(1);
    }
    if(newr->psw.ecmode)
    {
        if(newr->psw.sysmask & 0xb8)
        {
            DEBUG_SASSISTX(LPSW,logmsg("HHCEV300D : New PSW sysmask incorrect\n"));
            return(1);
        }
    }
    if(newr->psw.IA & 0x01)
    {
        DEBUG_SASSISTX(LPSW,logmsg("HHCEV300D : New PSW has ODD IA\n"));
        return(1);
    }
    return(0);
}
int     ecpsvm_dossm(REGS *regs,int b2,VADR effective_addr2)
{
    BYTE  reqmask;
    RADR  cregs;
    U32   creg0;
    REGS  npregs;

    SASSIST_PROLOG(SSM);


    /* Reject if V PSW is in problem state */
    if(CR6 & ECPSVM_CR6_VIRTPROB)
    {
        DEBUG_SASSISTX(SSM,logmsg("HHCEV300D : SASSIST SSM reject : V PB State\n"));
        return(1);
    }
    /* Get CR0 - set ref bit on  fetched CR0 (already done in prolog for MICBLOK) */
    cregs=LOGICAL_TO_ABS(micblok.MICCREG,USE_REAL_ADDR,regs,ACCTYPE_READ,0);
    FETCH_FW(creg0,&regs->mainstor[cregs]);

    /* Reject if V CR0 specifies SSM Suppression */
    if(creg0 & 0x40000000)
    {
        DEBUG_SASSISTX(SSM,logmsg("HHCEV300D : SASSIST SSM reject : V SSM Suppr\n"));
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

    if(ecpsvm_check_pswtrans(regs,&micblok,micpend,&vpregs,&npregs))       /* Check PSW transition capability */
    {
        DEBUG_SASSISTX(SSM,logmsg("HHCEV300D : SASSIST SSM Reject : New PSW too complex\n"));
        return(1); /* Something in the NEW PSW we can't handle.. let CP do it */
    }

    /* While we are at it, set the IA in the V PSW */
    npregs.psw.IA=regs->psw.IA & ADDRESS_MAXWRAP(regs);

    /* Set the change bit */
    LOGICAL_TO_ABS(vpswa,USE_REAL_ADDR,regs,ACCTYPE_WRITE,0);
    /* store the new PSW */
    ARCH_DEP(store_psw) (&npregs,&regs->mainstor[vpswa]);
    DEBUG_SASSISTX(SSM,logmsg("HHCEV300D : SASSIST SSM Complete : new SM = %2.2X\n",reqmask));
    DEBUG_SASSISTX(LPSW,logmsg("HHCEV300D : SASSIST SSM New VIRT "));
    DEBUG_SASSISTX(LPSW,display_psw(&npregs));
    DEBUG_SASSISTX(LPSW,logmsg("HHCEV300D : SASSIST SSM New REAL "));
    DEBUG_SASSISTX(LPSW,display_psw(regs));
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
        DEBUG_SASSISTX(SVC,logmsg("HHCEV300D : SASSIST SVC Reject : SVC 76\n"));
        return(1);
    }
    if(CR6 & ECPSVM_CR6_SVCINHIB)
    {
        DEBUG_SASSISTX(SVC,logmsg("HHCEV300D : SASSIST SVC Reject : SVC Assist Inhibit\n"));
        return(1);      /* SVC SASSIST INHIBIT ON */
    }
    /* Get what the NEW PSW should be */

    psa=(PSA_3XX *)&regs->mainstor[LOGICAL_TO_ABS((VADR)0 , USE_PRIMARY_SPACE, regs, ACCTYPE_READ, 0)];
                                                                                         /* Use all around access key 0 */
                                                                                         /* Also sets reference bit     */
    INITSIESTATE(newr);
    ARCH_DEP(load_psw) (&newr, (BYTE *)&psa->svcnew);   /* Ref bit set above */
    DEBUG_SASSISTX(SVC,logmsg("HHCEV300D : SASSIST SVC NEW VIRT "));
    DEBUG_SASSISTX(SVC,display_psw(&newr));
    /* Get some stuff from the REAL Running PSW to put in OLD SVC PSW */
    vpregs.psw.IA=regs->psw.IA;                   /* Instruction Address */
    vpregs.psw.cc=regs->psw.cc;                   /* Condition Code      */
    vpregs.psw.pkey=regs->psw.pkey;               /* Protection Key      */
    vpregs.psw.domask=regs->psw.domask;           /* Dec Overflow        */
    vpregs.psw.fomask=regs->psw.fomask;           /* Fixed Pt Overflow   */
    vpregs.psw.eumask=regs->psw.eumask;           /* Exponent Underflow  */
    vpregs.psw.sgmask=regs->psw.sgmask;           /* Significance        */
    vpregs.psw.intcode=svccode;                   /* SVC Interrupt code  */
    DEBUG_SASSISTX(SVC,logmsg("HHCEV300D : SASSIST SVC OLD VIRT "));
    DEBUG_SASSISTX(SVC,display_psw(&vpregs));

    if(ecpsvm_check_pswtrans(regs,&micblok,micpend,&vpregs,&newr))       /* Check PSW transition capability */
    {
        DEBUG_SASSISTX(SVC,logmsg("HHCEV300D : SASSIST SVC Reject : Cannot make transition to new PSW\n"));
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
    SASSIST_LPSW(newr);
    /*
     * Now store the new PSW in the area pointed by the MICBLOK
     */
    ARCH_DEP(store_psw) (&newr,regs->mainstor+vpswa);
    DEBUG_SASSISTX(SVC,logmsg("HHCEV300D : SASSIST SVC Done\n"));
    SASSIST_HIT(SVC);
    return(0);
}
/* LPSW Assist */
int ecpsvm_dolpsw(REGS *regs,int b2,VADR e2)
{
    VADR nlpsw;
    REGS nregs;

    SASSIST_PROLOG(LPSW);
    /* Reject if V PSW is in problem state */
    if(CR6 & ECPSVM_CR6_VIRTPROB)
    {
        DEBUG_SASSISTX(LPSW,logmsg("HHCEV300D : SASSIST LPSW reject : V PB State\n"));
        return(1);
    }
    if(e2&0x03)
    {
        DEBUG_SASSISTX(LPSW,logmsg("HHCEV300D : SASSIST LPSW %6.6X - Alignement error\n",e2));
        return(1);

    }
    nlpsw=LOGICAL_TO_ABS(e2,b2,regs,ACCTYPE_READ,regs->psw.pkey);
    INITSIESTATE(nregs);
    ARCH_DEP(load_psw) (&nregs,regs->mainstor+nlpsw);
    if(ecpsvm_check_pswtrans(regs,&micblok,micpend,&vpregs,&nregs))
    {
        DEBUG_SASSISTX(LPSW,logmsg("HHCEV300D : SASSIST LPSW Rejected - Cannot make PSW transition\n"));
        return(1);

    }
    SASSIST_LPSW(nregs);
    LOGICAL_TO_ABS(vpswa,USE_REAL_ADDR,regs,ACCTYPE_WRITE,0);
                        /* Set ref bit in address pointed by MICBLOK */
    ARCH_DEP(store_psw) (&nregs,regs->mainstor+vpswa);
    DEBUG_SASSISTX(LPSW,logmsg("HHCEV300D : SASSIST LPSW New VIRT "));
    DEBUG_SASSISTX(LPSW,display_psw(&nregs));
    DEBUG_SASSISTX(LPSW,logmsg("HHCEV300D : SASSIST LPSW New REAL "));
    DEBUG_SASSISTX(LPSW,display_psw(regs));
    SASSIST_HIT(LPSW);
    return(0);
}

/* SIO/SIOF Assist */
int ecpsvm_dosio(BYTE *inst,REGS *regs,int b2,VADR e2)
{
    SASSIST_PROLOG(SIO);
    UNREFERENCED(b2);
    UNREFERENCED(e2);
    /* Reject if V PSW is in problem state */
    if(CR6 & ECPSVM_CR6_VIRTPROB)
    {
        DEBUG_SASSISTX(SIO,logmsg("HHCEV300D : SASSIST SSM reject : V PB State\n"));
        return(1);
    }
    if(inst[1]>1)
    {
        DEBUG_SASSISTX(SIO,logmsg("HHCEV300D : SASSIST - RIO Not supported\n"));
        return(1);      /* RIO is a NOGO */
    }
    return(1);
}
int ecpsvm_dostnsm(REGS *regs,int b1,VADR effective_addr1,int imm2)
{
    UNREFERENCED(b1);
    UNREFERENCED(effective_addr1);
    UNREFERENCED(imm2);
    SASSIST_PROLOG(STNSM);
    return(1);
}
int ecpsvm_dostosm(REGS *regs,int b1,VADR effective_addr1,int imm2)
{
    UNREFERENCED(b1);
    UNREFERENCED(effective_addr1);
    UNREFERENCED(imm2);
    SASSIST_PROLOG(STOSM);
    return(1);
}
static char *ecpsvm_stat_sep="HHCEV003I +-----------+----------+----------+-------+\n";

static int ecpsvm_sortstats(const void *a,const void *b)
{
    ECPSVM_STAT *ea,*eb;
    ea=(ECPSVM_STAT *)a;
    eb=(ECPSVM_STAT *)b;
    return(eb->call-ea->call);
}

static void ecpsvm_showstats2(ECPSVM_STAT *ar,size_t count)
{
    char *sep=ecpsvm_stat_sep;
    char nname[32];
    int  havedisp=0;
    int  notshown=0;
    size_t unsupcc=0;
    int haveunsup=0;
    int callt=0;
    int hitt=0;
    size_t i;
    for(i=0;i<count;i++)
    {
        if(ar[i].call)
        {
            callt+=ar[i].call;
            hitt+=ar[i].hit;
            if(!ar[i].support)
            {
                unsupcc+=ar[i].call;
                haveunsup++;
            }
            havedisp=1;
            snprintf(nname,32,"%s%s",ar[i].name,ar[i].support ? "" : "*");
            if(!ar[i].enabled)
            {
                strcat(nname,"-");
            }
            if(ar[i].debug)
            {
                strcat(nname,"%");
            }
            if(ar[i].total)
            {
                strcat(nname,"+");
            }
            logmsg("HHCEV001I | %-9s | %8d | %8d |  %3d%% |\n",
                    nname,
                    ar[i].call,
                    ar[i].hit,
                    ar[i].call ?
                            (ar[i].hit*100)/ar[i].call :
                            100);
        }
        else
        {
            notshown++;
        }
    }
    if(havedisp)
    {
        logmsg(sep);
    }
    logmsg("HHCEV001I | %-9s | %8d | %8d |  %3d%% |\n",
            "Total",
            callt,
            hitt,
            callt ?
                    (hitt*100)/callt :
                    100);
    logmsg(sep);
    if(haveunsup)
    {
        logmsg("HHCEV004I * : Unsupported, - : Disabled, %% - Debug\n");
    }
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
/* SHOW STATS */
void ecpsvm_showstats(int ac,char **av)
{
    size_t      asize;
    ECPSVM_STAT *ar;
    char *sep=ecpsvm_stat_sep;

    UNREFERENCED(ac);
    UNREFERENCED(av);
    logmsg(sep);
    logmsg("HHCEV002I | %-9s | %-8s | %-8s | %-5s |\n","VM ASSIST","Calls","Hits","Ratio");
    logmsg(sep);
    ar=malloc(sizeof(ecpsvm_sastats));
    memcpy(ar,&ecpsvm_sastats,sizeof(ecpsvm_sastats));
    asize=sizeof(ecpsvm_sastats)/sizeof(ECPSVM_STAT);
    qsort(ar,asize,sizeof(ECPSVM_STAT),ecpsvm_sortstats);
    ecpsvm_showstats2(ar,asize);
    free(ar);
    logmsg(sep);
    logmsg("HHCEV002I | %-9s | %-8s | %-8s | %-5s |\n","CP ASSIST","Calls","Hits","Ratio");
    logmsg(sep);
    ar=malloc(sizeof(ecpsvm_cpstats));
    memcpy(ar,&ecpsvm_cpstats,sizeof(ecpsvm_cpstats));
    asize=sizeof(ecpsvm_cpstats)/sizeof(ECPSVM_STAT);
    qsort(ar,asize,sizeof(ECPSVM_STAT),ecpsvm_sortstats);
    ecpsvm_showstats2(ar,asize);
    free(ar);
}

static void ecpsvm_helpcmdlist(void)
{
    int i;
    ECPSVM_CMDENT *ce;

    for(i=0;ecpsvm_cmdtab[i].name;i++)
    {
        ce=&ecpsvm_cmdtab[i];
        logmsg("HHCEV010I : %s : %s\n",ce->name,ce->expl);
    }
    return;
}
void ecpsvm_helpcmd(int ac,char **av)
{
    ECPSVM_CMDENT *ce;
    if(ac==1)
    {
        ecpsvm_helpcmdlist();
        return;
    }
    ce=ecpsvm_getcmdent(av[1]);
    if(ce==NULL)
    {
        logmsg("HHCEV011E Unknown subcommand %s - valid subcommands are :\n",av[1]);
        ecpsvm_helpcmdlist();
        return;
    }
    logmsg("HHCEV012I : %s : %s",ce->name,ce->help);
    return;
}

ECPSVM_STAT *ecpsvm_findstat(char *feature,char **fclass)
{
    ECPSVM_STAT *es;
    ECPSVM_STAT *esl;
    int i;
    int fcount;
    fcount=sizeof(ecpsvm_sastats)/sizeof(ECPSVM_STAT);
    esl=(ECPSVM_STAT *)&ecpsvm_sastats;
    for(i=0;i<fcount;i++)
    {
        es=&esl[i];
        if(strcasecmp(feature,es->name)==0)
        {
            *fclass="VM ASSIST";
            return(es);
        }
    }
    esl=(ECPSVM_STAT *)&ecpsvm_cpstats;
    fcount=sizeof(ecpsvm_cpstats)/sizeof(ECPSVM_STAT);
    for(i=0;i<fcount;i++)
    {
        es=&esl[i];
        if(strcasecmp(feature,es->name)==0)
        {
            *fclass="CP ASSIST";
            return(es);
        }
    }
    return(NULL);
}

void ecpsvm_enadisaall(char *fclass,ECPSVM_STAT *tbl,size_t count,int onoff,int debug)
{
    ECPSVM_STAT *es;
    size_t i;
    char *enadisa,*debugonoff;
    enadisa=onoff?"Enabled":"Disabled";
    debugonoff=debug?"On":"Off";
    for(i=0;i<count;i++)
    {
        es=&tbl[i];
        if(onoff>=0)
        {
            es->enabled=onoff;
            logmsg("HHCEV015I ECPS:VM %s feature %s %s\n",fclass,es->name,enadisa);
        }
        if(debug>=0)
        {
            es->debug=debug;
            logmsg("HHCEV015I ECPS:VM %s feature %s Debug %s\n",fclass,es->name,debugonoff);
        }
    }
    if(onoff>=0)
    {
        logmsg("HHCEV016I All ECPS:VM %s features %s\n",fclass,enadisa);
    }
    if(debug>=0)
    {
        logmsg("HHCEV016I All ECPS:VM %s features Debug %s\n",fclass,debugonoff);
    }
}

void ecpsvm_enable_disable(int ac,char **av,int onoff,int debug)
{
    char        *fclass;
    char        *enadisa,*debugonoff;
    int i;

    size_t      sacount,cpcount;
    ECPSVM_STAT *es;
    ECPSVM_STAT *sal,*cpl;

    sal=(ECPSVM_STAT *)&ecpsvm_sastats;
    cpl=(ECPSVM_STAT *)&ecpsvm_cpstats;
    sacount=sizeof(ecpsvm_sastats)/sizeof(ECPSVM_STAT);
    cpcount=sizeof(ecpsvm_cpstats)/sizeof(ECPSVM_STAT);

    enadisa=onoff?"Enabled":"Disabled";
    debugonoff=debug?"On":"Off";
    if(ac==1)
    {
        ecpsvm_enadisaall("VM ASSIST",sal,sacount,onoff,debug);
        ecpsvm_enadisaall("CP ASSIST",cpl,cpcount,onoff,debug);
        if(onoff>=0)
        {
            sysblk.ecpsvm.available=onoff;
            logmsg("HHCEV013I ECPS:VM Globaly %s\n",enadisa);
        }
        if(debug>=0)
        {
            sysblk.ecpsvm.debug=debug;
            logmsg("HHCEV013I ECPS:VM Global Debug %s\n",debugonoff);
        }
        return;
    }
    for(i=1;i<ac;i++)
    {
        if(strcasecmp(av[i],"ALL")==0)
        {
            ecpsvm_enadisaall("VM ASSIST",sal,sacount,onoff,debug);
            ecpsvm_enadisaall("CP ASSIST",cpl,cpcount,onoff,debug);
            return;
        }
        if(strcasecmp(av[i],"VMA")==0)
        {
            ecpsvm_enadisaall("VM ASSIST",sal,sacount,onoff,debug);
            return;
        }
        if(strcasecmp(av[i],"CPA")==0)
        {
            ecpsvm_enadisaall("CP ASSIST",cpl,cpcount,onoff,debug);
            return;
        }
        es=ecpsvm_findstat(av[i],&fclass);
        if(es!=NULL)
        {
            if(onoff>=0)
            {
                es->enabled=onoff;
                logmsg("HHCEV014I ECPS:VM %s feature %s %s\n",fclass,es->name,enadisa);
            }
            if(debug>=0)
            {
                es->debug=onoff;
                logmsg("HHCEV014I ECPS:VM %s feature %s Debug %s\n",fclass,es->name,debugonoff);
            }
        }
        else
        {
            logmsg("HHCEV014I Unknown ECPS:VM feature %s; Ignored\n",av[i]);
        }
    }
}

void ecpsvm_disable(int ac,char **av)
{
    ecpsvm_enable_disable(ac,av,0,-1);
}
void ecpsvm_enable(int ac,char **av)
{
    ecpsvm_enable_disable(ac,av,1,-1);
}
void ecpsvm_debug(int ac,char **av)
{
    ecpsvm_enable_disable(ac,av,-1,1);
}
void ecpsvm_nodebug(int ac,char **av)
{
    ecpsvm_enable_disable(ac,av,-1,0);
}

void ecpsvm_level(int ac,char **av)
{
    int lvl;
    if(sysblk.ecpsvm.available)
    {
        logmsg("HHCEV016I Current reported ECPS:VM Level is %d\n",sysblk.ecpsvm.level);
    }
    else
    {
        logmsg("HHCEV016I Current reported ECPS:VM Level is %d\n",sysblk.ecpsvm.level);
        logmsg("HHCEV017I But ECPS:VM is currently disabled\n");
    }
    if(ac>1)
    {
        lvl=atoi(av[1]);
        logmsg("HHCEV016I Level reported to guest program is now %d\n",lvl);
        sysblk.ecpsvm.level=lvl;
    }
    if(sysblk.ecpsvm.level!=20)
    {
          logmsg("HHCEV017W WARNING ! current level (%d) is not supported\n",sysblk.ecpsvm.level);
          logmsg("HHCEV018W WARNING ! Unpredictable results may occur\n");
          logmsg("HHCEV019I The microcode support level is 20\n");
    }
}

static ECPSVM_CMDENT ecpsvm_cmdtab[]={
    {"Help",1,ecpsvm_helpcmd,"Show help","format : \"evm help [cmd]\" Shows help on the specified\n"
                                                        "        ECPSVM subcommand\n"},
    {"STats",2,ecpsvm_showstats,"Show statistical counters","format : evm stats : Shows various ECPS:VM Counters\n"},
    {"DIsable",2,ecpsvm_disable,"Disable ECPS:VM Features","format : evm disable [ALL|feat1[ feat2|...]\n"},
    {"ENable",2,ecpsvm_enable,"Enable ECPS:VM Features","format : evm disable [ALL|feat1[ feat2|...]\n"},
#if defined(DEBUG_SASSIST) || defined(DEBUG_CPASSIST)
    {"DEBUG",5,ecpsvm_debug,"Debug ECPS:VM Features","format : evm debug [ALL|feat1[ feat2|...]\n"},
    {"NODebug",3,ecpsvm_nodebug,"Turn Debug off for ECPS:VM Features","format : evm NODebug [ALL|feat1[ feat2|...]\n"},
#endif
    {"Level",1,ecpsvm_level,"Set/Show ECPS:VM level","format : evm Level [nn]\n"},
    {NULL,0,NULL,NULL,NULL}};

ECPSVM_CMDENT *ecpsvm_getcmdent(char *cmd)
{
    ECPSVM_CMDENT *ce;
    int i;
    int clen;
    for(i=0;ecpsvm_cmdtab[i].name;i++)
    {
        ce=&ecpsvm_cmdtab[i];
        if(strlen(cmd)<=strlen(ce->name) && strlen(cmd)>=(size_t)ce->abbrev)
        {
            clen=strlen(cmd);
            if(strncasecmp(cmd,ce->name,clen)==0)
            {
                return(ce);
            }
        }
    }
    return(NULL);
}
void ecpsvm_command(int ac,char **av)
{
    logmsg("HHCEV011I ECPS:VM Command processor invoked\n");
    if(ac==1)
    {
        logmsg("HHCEV008E NO EVM subcommand. Type \"evm help\" for a list of valid subcommands\n");
        return;
    }
    ECPSVM_CMDENT *ce;
    ce=ecpsvm_getcmdent(av[1]);
    if(ce==NULL)
    {
        logmsg("HHCEV008E Unknown EVM subcommand %s\n",av[1]);
        return;
    }
    ce->fun(ac-1,av+1);
    logmsg("HHCEV011I ECPS:VM Command processor complete\n");
}
#endif /* ifdef FEATURE_ECPSVM */

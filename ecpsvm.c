/* ECPS:VM Implementation */
/* (c) Ivan Warren 2003   */

#include "hercules.h"

#include "opcode.h"

#include "inline.h"

#include "ecpsvm.h"

// #define DEBUG_ASSIST(x) ( x )
#define DEBUG_ASSIST(x)

#define BR14 regs->psw.IA=regs->GR_L(14) & ADDRESS_MAXWRAP(regs)

#define ECPSVM_PROLOG \
int     b1, b2; \
VADR    effective_addr1, \
        effective_addr2; \
     SSE(inst, execflag, regs, b1, effective_addr1, b2, effective_addr2); \
     if(!sysblk.ecpsvm.available) \
     { \
          ARCH_DEP(program_interrupt) (regs, PGM_OPERATION_EXCEPTION); \
     } \
     PRIV_CHECK(regs); \
     if(!(regs->CR_L(6) & 0x02000000)) \
     { \
        return; \
     } \

DEF_INST(ecpsvm_basic_freex)
{
    ECPSVM_PROLOG
    DEBUG_ASSIST(logmsg("HHCEV300D : freex called\n"));
}
DEF_INST(ecpsvm_basic_fretx)
{
    ECPSVM_PROLOG
    DEBUG_ASSIST(logmsg("HHCEV300D : fretx called\n"));
}
DEF_INST(ecpsvm_lock_page)
{
    ECPSVM_PROLOG
    DEBUG_ASSIST(logmsg("HHCEV300D : LKPG\n"));
}
DEF_INST(ecpsvm_unlock_page)
{
    ECPSVM_PROLOG
    DEBUG_ASSIST(logmsg("HHCEV300D : ULKPG\n"));
}
DEF_INST(ecpsvm_decode_next_ccw)
{
    ECPSVM_PROLOG
    DEBUG_ASSIST(logmsg("HHCEV300D : DNCCW\n"));
}
DEF_INST(ecpsvm_free_ccwstor)
{
    ECPSVM_PROLOG
    DEBUG_ASSIST(logmsg("HHCEV300D : FCCWST\n"));
}
DEF_INST(ecpsvm_locate_vblock)
{
    ECPSVM_PROLOG
    DEBUG_ASSIST(logmsg("HHCEV300D : SCNVU\n"));
}
DEF_INST(ecpsvm_disp1)
{
    ECPSVM_PROLOG
    DEBUG_ASSIST(logmsg("HHCEV300D : DISP 1\n"));
}
DEF_INST(ecpsvm_tpage)
{
    ECPSVM_PROLOG
    DEBUG_ASSIST(logmsg("HHCEV300D : TRANBRNG\n"));
}
DEF_INST(ecpsvm_tpage_lock)
{
    ECPSVM_PROLOG
    DEBUG_ASSIST(logmsg("HHCEV300D : TRANLOCK\n"));
}
DEF_INST(ecpsvm_inval_segtab)
{
    ECPSVM_PROLOG
    DEBUG_ASSIST(logmsg("HHCEV300D : ISTBL\n"));
}
DEF_INST(ecpsvm_inval_ptable)
{
    ECPSVM_PROLOG
    DEBUG_ASSIST(logmsg("HHCEV300D : IPTBL\n"));
}
DEF_INST(ecpsvm_decode_first_ccw)
{
    ECPSVM_PROLOG
    DEBUG_ASSIST(logmsg("HHCEV300D : DFCCW\n"));
}
DEF_INST(ecpsvm_dispatch_main)
{
    VMBLOK      *vmb;
    ECPSVM_PROLOG
    DEBUG_ASSIST(logmsg("HHCEV300D : DISP 0\n"));
}
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
    ECPSVM_PROLOG
    arioct=APPLY_PREFIXING(effective_addr2,regs->PX);
    rdev=(effective_addr1 & 0xfff);
    DEBUG_ASSIST(logmsg("HHCEV300D : ECPS:VM SCNRU called; RDEV=%4.4X ARIOCT=%6.6X\n",effective_addr1,arioct));
    FETCH_FW(rchixtbl,sysblk.mainstor+arioct);
    FETCH_HW(chix,sysblk.mainstor+rchixtbl+((rdev & 0xf00) >> 7 ));
    // logmsg("HHCEV300D : ECPS:VM SCNRU : RCH IX = %x\n",chix);
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
        return;
    }
    FETCH_FW(rchtbl,sysblk.mainstor+arioct+4);
    rchblk=rchtbl+chix;
    /* RCHBLK now contains RCHBLOK address */
    FETCH_HW(cuix,sysblk.mainstor+rchblk+0x20+((rdev & 0xf8 )>> 2));
    if(cuix & 0x8000)
    {
        /* Try with 0xF0 */
        FETCH_HW(cuix,sysblk.mainstor+rchblk+0x20+((rdev & 0xf0) >> 2));
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
    FETCH_FW(rcutbl,sysblk.mainstor+arioct+8);
    rcublk=rcutbl+cuix;
    FETCH_HW(dvix,sysblk.mainstor+rcublk+0x28+((rdev & 0xf)<<1));
    dvix<<=3;
    // logmsg("HHCEV300D : ECPS:VM SCNRU : RDV IX = %x\n",dvix);
    if(sysblk.mainstor[rcublk+5]&0x40)
    {
        FETCH_FW(rcublk,sysblk.mainstor+rcublk+0x10);
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
    FETCH_FW(rdvtbl,sysblk.mainstor+arioct+12);
    rdvblk=rdvtbl+dvix;
    DEBUG_ASSIST(logmsg("HHCEV300D : ECPS:VM SCNRU : RCH = %6.6X, RCU = %6.6X, RDV = %6.6X\n",rchblk,rcublk,rdvblk));
    regs->GR_L(6)=rchblk;
    regs->GR_L(7)=rcublk;
    regs->GR_L(8)=rdvblk;
    regs->psw.cc=0;
    regs->psw.IA=regs->GR_L(14) & ADDRESS_MAXWRAP(regs);
    regs->GR_L(15)=0;
}
DEF_INST(ecpsvm_comm_ccwproc)
{
    ECPSVM_PROLOG
    DEBUG_ASSIST(logmsg("HHCEV300D : CCWGN\n"));
}
DEF_INST(ecpsvm_unxlate_ccw)
{
    ECPSVM_PROLOG
    DEBUG_ASSIST(logmsg("HHCEV300D : UNCCW\n"));
}
DEF_INST(ecpsvm_disp2)
{
    ECPSVM_PROLOG
    DEBUG_ASSIST(logmsg("HHCEV300D : DISP 2\n"));
}
DEF_INST(ecpsvm_store_level)
{
    ECPSVM_PROLOG
    ARCH_DEP(vstore4) (sysblk.ecpsvm.level,effective_addr1,b1,regs);
    DEBUG_ASSIST(logmsg("HHCEV300D : ECPS:VM STORE LEVEL %d called\n",sysblk.ecpsvm.level));
}
DEF_INST(ecpsvm_loc_chgshrpg)
{
    ECPSVM_PROLOG
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
    ECPSVM_PROLOG
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
    FETCH_FW(maxdw,sysblk.mainstor+effective_addr1);
    if(regs->GR_L(0)>maxdw)
    {
        DEBUG_ASSIST(logmsg("HHCEV300D : FREEX request beyond subpool capacity\n"));
        return;
    }
    /* Fetch subpool index */
    spix=sysblk.mainstor[spixtbl+numdw];
    DEBUG_ASSIST(logmsg("HHCEV300D : Subpool index = %X\n",spix));
    /* Fetch value */
    FETCH_FW(freeblock,sysblk.mainstor+maxsztbl+4+spix);
    DEBUG_ASSIST(logmsg("HHCEV300D : Value in subpool table = %6.6X\n",freeblock));
    if(freeblock==0)
    {
        /* Can't fullfill request here */
        return;
    }
    FETCH_FW(nextblk,sysblk.mainstor+freeblock);
    STORE_FW(sysblk.mainstor+maxsztbl+4+spix,nextblk);
    DEBUG_ASSIST(logmsg("HHCEV300D : New Value in subpool table = %6.6X\n",nextblk));
    regs->GR_L(1)=freeblock;
    regs->psw.cc=0;
    BR14;
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
    ECPSVM_PROLOG
    numdw=regs->GR_L(0);
    block=regs->GR_L(1);
    maxsztbl=effective_addr1;
    fretl=effective_addr2;
    DEBUG_ASSIST(logmsg("HHCEV300D : X fretx called AREA=%6.6X, DW=%4.4X\n",regs->GR_L(1),regs->GR_L(0)));
    if(numdw==0)
    {
        DEBUG_ASSIST(logmsg("HHCEV300D : ECPS:VM Cannot FRETX : DWORDS = 0\n"));
        return;
    }
    FETCH_FW(maxdw,sysblk.mainstor+maxsztbl);
    if(numdw>maxdw)
    {
        DEBUG_ASSIST(logmsg("HHCEV300D : ECPS:VM Cannot FRETX : DWORDS = %d > MAXDW %d\n",numdw,maxdw));
        return;
    }
    FETCH_FW(cortbl,sysblk.mainstor+fretl);
    cortbe=cortbl+((block & 0xfff000)>>8);
    if(memcmp(sysblk.mainstor+cortbe,sysblk.mainstor+fretl+4,4)!=0)

    {
        DEBUG_ASSIST(logmsg("HHCEV300D : ECPS:VM Cannot FRETX : Area not in Core Free area\n"));
        return;
    }
    if(sysblk.mainstor[cortbe+8]!=0x02)
    {
        DEBUG_ASSIST(logmsg("HHCEV300D : ECPS:VM Cannot FRETX : Area flag != 0x02\n"));
        return;
    }
    spix=sysblk.mainstor[fretl+11+numdw];
    FETCH_FW(prevblk,sysblk.mainstor+maxsztbl+4+spix);
    if(prevblk==block)
    {
        DEBUG_ASSIST(logmsg("HHCEV300D : ECPS:VM Cannot FRETX : fretted block already on subpool chain\n"));
        return;
    }
    STORE_FW(sysblk.mainstor+maxsztbl+4+spix,block);
    STORE_FW(sysblk.mainstor+block,prevblk);
    BR14;
    return;
}
DEF_INST(ecpsvm_prefmach_assist)
{
    ECPSVM_PROLOG
    DEBUG_ASSIST(logmsg("HHCEV300D : PMA\n"));
}

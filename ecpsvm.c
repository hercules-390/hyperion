/* ECPS:VM Implementation */
/* (c) Ivan Warren 2003   */

#include "hercules.h"

#include "opcode.h"

#include "inline.h"

#include "ecpsvm.h"

// #define DEBUG_ASSIST(x) ( x )
#define DEBUG_ASSIST(x)

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
    DEBUG_ASSIST(logmsg("HHCEV300D : ECPS:VM SCNRU called; RDEV=%4.4X ARIOCT=%8.8X\n",effective_addr1,arioct));
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
    // logmsg("HHCEV300D : ECPS:VM SCNRU RCHBLOK @ %8.8X\n",rchblk);
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
    // logmsg("HHCEV300D : ECPS:VM SCNRU RCUBLOK @ %8.8X\n",rcublk);
    FETCH_HW(dvix,sysblk.mainstor+rcublk+0x28+((rdev & 0xf)<<1));
    dvix<<=3;
    // logmsg("HHCEV300D : ECPS:VM SCNRU : RDV IX = %x\n",dvix);
    if(sysblk.mainstor[rcublk+5]&0x40)
    {
        FETCH_FW(rcublk,sysblk.mainstor+rcublk+0x10);
        // logmsg("HHCEV300D : ECPS:VM SCNRU RCUBLOK ACTUALLY @ %8.8X\n",rcublk);
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
    // logmsg("HHCEV300D : ECPS:VM SCNRU RDVBLOK @ %8.8X\n",rdvblk);
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
    ECPSVM_PROLOG
    DEBUG_ASSIST(logmsg("HHCEV300D : X freex called CR6 = %8.8X\n",regs->CR_L(6)));
}
DEF_INST(ecpsvm_extended_fretx)
{
    ECPSVM_PROLOG
    DEBUG_ASSIST(logmsg("HHCEV300D : X fretx called\n"));
}
DEF_INST(ecpsvm_prefmach_assist)
{
    ECPSVM_PROLOG
    DEBUG_ASSIST(logmsg("HHCEV300D : PMA\n"));
}

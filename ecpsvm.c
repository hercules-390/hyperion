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

    ECPSVM_PROLOG

    /* Get the address specified for the 2nd operand */
    arioct=APPLY_PREFIXING(effective_addr2,regs->PX);

    /* Obtain the Device address */
    rdev=(effective_addr1 & 0xfff);
    DEBUG_ASSIST(logmsg("HHCEV300D : ECPS:VM SCNRU called; RDEV=%4.4X ARIOCT=%6.6X\n",effective_addr1,arioct));

    /* Get the Channel Index Table */
    FETCH_FW(rchixtbl,regs->mainstor+arioct);

    /* Obtain the RCH offset */
    FETCH_HW(chix,regs->mainstor+rchixtbl+((rdev & 0xf00) >> 7 ));

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
    FETCH_FW(rchtbl,regs->mainstor+arioct+4);

    /* Add the RCH Index offset */
    rchblk=rchtbl+chix;

    /* Try to obtain RCU index with bits 8-12 of the device */
    FETCH_HW(cuix,regs->mainstor+rchblk+0x20+((rdev & 0xf8 )>> 2));
    if(cuix & 0x8000)
    {
        /* Try with bits 8-11 */
        FETCH_HW(cuix,regs->mainstor+rchblk+0x20+((rdev & 0xf0) >> 2));
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
    FETCH_FW(rcutbl,regs->mainstor+arioct+8);
    rcublk=rcutbl+cuix;
    FETCH_HW(dvix,regs->mainstor+rcublk+0x28+((rdev & 0xf)<<1));
    dvix<<=3;
    // logmsg("HHCEV300D : ECPS:VM SCNRU : RDV IX = %x\n",dvix);
    if(regs->mainstor[rcublk+5]&0x40)
    {
        FETCH_FW(rcublk,regs->mainstor+rcublk+0x10);
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
    FETCH_FW(rdvtbl,regs->mainstor+arioct+12);
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
    FETCH_FW(maxdw,regs->mainstor+effective_addr1);
    if(regs->GR_L(0)>maxdw)
    {
        DEBUG_ASSIST(logmsg("HHCEV300D : FREEX request beyond subpool capacity\n"));
        return;
    }
    /* Fetch subpool index */
    spix=regs->mainstor[spixtbl+numdw];
    DEBUG_ASSIST(logmsg("HHCEV300D : Subpool index = %X\n",spix));
    /* Fetch value */
    FETCH_FW(freeblock,regs->mainstor+maxsztbl+4+spix);
    DEBUG_ASSIST(logmsg("HHCEV300D : Value in subpool table = %6.6X\n",freeblock));
    if(freeblock==0)
    {
        /* Can't fullfill request here */
        return;
    }
    FETCH_FW(nextblk,regs->mainstor+freeblock);
    STORE_FW(regs->mainstor+maxsztbl+4+spix,nextblk);
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
    FETCH_FW(maxdw,regs->mainstor+maxsztbl);
    if(numdw>maxdw)
    {
        DEBUG_ASSIST(logmsg("HHCEV300D : ECPS:VM Cannot FRETX : DWORDS = %d > MAXDW %d\n",numdw,maxdw));
        return;
    }
    FETCH_FW(cortbl,regs->mainstor+fretl);
    cortbe=cortbl+((block & 0xfff000)>>8);
    if(memcmp(regs->mainstor+cortbe,regs->mainstor+fretl+4,4)!=0)

    {
        DEBUG_ASSIST(logmsg("HHCEV300D : ECPS:VM Cannot FRETX : Area not in Core Free area\n"));
        return;
    }
    if(regs->mainstor[cortbe+8]!=0x02)
    {
        DEBUG_ASSIST(logmsg("HHCEV300D : ECPS:VM Cannot FRETX : Area flag != 0x02\n"));
        return;
    }
    spix=regs->mainstor[fretl+11+numdw];
    FETCH_FW(prevblk,regs->mainstor+maxsztbl+4+spix);
    if(prevblk==block)
    {
        DEBUG_ASSIST(logmsg("HHCEV300D : ECPS:VM Cannot FRETX : fretted block already on subpool chain\n"));
        return;
    }
    STORE_FW(regs->mainstor+maxsztbl+4+spix,block);
    STORE_FW(regs->mainstor+block,prevblk);
    BR14;
    return;
}
DEF_INST(ecpsvm_prefmach_assist)
{
    ECPSVM_PROLOG
    DEBUG_ASSIST(logmsg("HHCEV300D : PMA\n"));
}

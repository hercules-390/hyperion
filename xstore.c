/* XSTORE.C   Expanded storage related instructions - Jan Jaeger     */

/* Interpretive Execution - (c) Copyright Jan Jaeger, 1999-2001      */
/* z/Architecture support - (c) Copyright Jan Jaeger, 1999-2001      */

/* MVPG moved from cpu.c to xstore.c   05/07/00 Jan Jaeger */

#include "hercules.h"

#include "opcode.h"

#include "inline.h"

#if defined(FEATURE_EXPANDED_STORAGE)
/*-------------------------------------------------------------------*/
/* B22E PGIN  - Page in from expanded storage                  [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(page_in)
{
int     r1, r2;                         /* Values of R fields        */
RADR    maddr;                          /* Main storage address      */
U32     xaddr;                          /* Expanded storage address  */

    RRE(inst, execflag, regs, r1, r2);

    PRIV_CHECK(regs);

#if defined(_FEATURE_SIE)
    if(regs->sie_state && (regs->siebk->ic[3] & SIE_IC3_PGX))
        longjmp(regs->progjmp, SIE_INTERCEPT_INST);
#endif /*defined(_FEATURE_SIE)*/

#if defined(FEATURE_MULTIPLE_CONTROLLED_DATA_SPACE)
    /* Cannot perform xstore page movement in XC mode */
    if(regs->sie_state && (regs->siebk->mx & SIE_MX_XC))
        longjmp(regs->progjmp, SIE_INTERCEPT_INST);
#endif /*defined(FEATURE_MULTIPLE_CONTROLLED_DATA_SPACE)*/

    /* expanded storage block number */
    xaddr = regs->GR_L(r2);

#if defined(_FEATURE_SIE)
    if(regs->sie_state)
    {
        xaddr += regs->sie_xso;
        if(xaddr >= regs->sie_xsl)
        {
            regs->psw.cc = 3;
            return;
        }
    }
#endif /*defined(_FEATURE_SIE)*/

    /* If the expanded storage block is not configured then
       terminate with cc3 */
    if (xaddr >= sysblk.xpndsize)
    {
        regs->psw.cc = 3;
        return;
    }

    /* Byte offset in expanded storage */
    xaddr <<= XSTORE_PAGESHIFT;

    /* Obtain abs address, verify access and set ref/change bits */
    maddr = LOGICAL_TO_ABS (regs->GR(r1) & ADDRESS_MAXWRAP(regs),
         USE_REAL_ADDR, regs, ACCTYPE_WRITE, 0);
    maddr &= XSTORE_PAGEMASK;

    /* Copy data from expanded to main */
    memcpy (sysblk.mainstor + maddr, sysblk.xpndstor + xaddr,
            XSTORE_PAGESIZE);

    /* cc0 means pgin ok */
    regs->psw.cc = 0;

} 
#endif /*defined(FEATURE_EXPANDED_STORAGE)*/


#if defined(FEATURE_EXPANDED_STORAGE)
/*-------------------------------------------------------------------*/
/* B22F PGOUT - Page out to expanded storage                   [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(page_out)
{
int     r1, r2;                         /* Values of R fields        */
RADR    maddr;                          /* Main storage address      */
U32     xaddr;                          /* Expanded storage address  */

    RRE(inst, execflag, regs, r1, r2);

    PRIV_CHECK(regs);

#if defined(_FEATURE_SIE)
    if(regs->sie_state && (regs->siebk->ic[3] & SIE_IC3_PGX))
        longjmp(regs->progjmp, SIE_INTERCEPT_INST);
#endif /*defined(_FEATURE_SIE)*/

#if defined(FEATURE_MULTIPLE_CONTROLLED_DATA_SPACE)
    /* Cannot perform xstore page movement in XC mode */
    if(regs->sie_state && (regs->siebk->mx & SIE_MX_XC))
        longjmp(regs->progjmp, SIE_INTERCEPT_INST);
#endif /*defined(FEATURE_MULTIPLE_CONTROLLED_DATA_SPACE)*/

    /* expanded storage block number */
    xaddr = regs->GR_L(r2);

#if defined(_FEATURE_SIE)
    if(regs->sie_state)
    {
        xaddr += regs->sie_xso;
        if(xaddr >= regs->sie_xsl)
        {
            regs->psw.cc = 3;
            return;
        }
    }
#endif /*defined(_FEATURE_SIE)*/

    /* If the expanded storage block is not configured then
       terminate with cc3 */
    if (xaddr >= sysblk.xpndsize)
    {
        regs->psw.cc = 3;
        return;
    }

    /* Byte offset in expanded storage */
    xaddr <<= XSTORE_PAGESHIFT;

    /* Obtain abs address, verify access and set ref/change bits */
    maddr = LOGICAL_TO_ABS (regs->GR(r1) & ADDRESS_MAXWRAP(regs),
         USE_REAL_ADDR, regs, ACCTYPE_READ, regs->psw.pkey);
    maddr &= XSTORE_PAGEMASK;

    /* Copy data from main to expanded */
    memcpy (sysblk.xpndstor + xaddr, sysblk.mainstor + maddr,
            XSTORE_PAGESIZE);

    /* cc0 means pgout ok */
    regs->psw.cc = 0;

}
#endif /*defined(FEATURE_EXPANDED_STORAGE)*/


#if defined(FEATURE_MOVE_PAGE_FACILITY_2) && defined(FEATURE_EXPANDED_STORAGE)
/*-------------------------------------------------------------------*/
/* B259 IESBE - Invalidate Expanded Storage Blk Entry          [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(invalidate_expanded_storage_block_entry)
{
int     r1, r2;                         /* Values of R fields        */

    RRE(inst, execflag, regs, r1, r2);

    PRIV_CHECK(regs);

    /* Perform serialization before operation */
    PERFORM_SERIALIZATION (regs);

    /* Update page table entry interlocked */
    OBTAIN_MAINLOCK(regs);

    /* Invalidate page table entry */
    ARCH_DEP(invalidate_pte) (inst[1], r1, r2, regs);

    /* Release mainstore interlock */
    RELEASE_MAINLOCK(regs);

    /* Perform serialization after operation */
    PERFORM_SERIALIZATION (regs);

}
#endif /*defined(FEATURE_EXPANDED_STORAGE)*/


#if defined(FEATURE_MOVE_PAGE_FACILITY_2)
/*-------------------------------------------------------------------*/
/* Move Page (Facility 2)                                            */
/*                                                                   */
/* Input:                                                            */
/*      r1      First operand register number                        */
/*      r2      Second operand register number                       */
/*      regs    Pointer to the CPU register context                  */
/* Return value:                                                     */
/*      Returns the condition code for the MVPG instruction.         */
/*                                                                   */
/*      This function does not return if a program check occurs.     */
/*-------------------------------------------------------------------*/

/*-------------------------------------------------------------------*/
/* B254 MVPG  - Move Page                                      [RRE] */
/*-------------------------------------------------------------------*/
DEF_INST(move_page)
{
int     r1, r2;                         /* Register values           */
int     rc;                             /* Return code               */
int     cc = 0;				/* Condition code            */
VADR    vaddr1, vaddr2;                 /* Virtual addresses         */
RADR    raddr1, raddr2;                 /* Real addresses            */
RADR    aaddr1 = 0, aaddr2 = 0;         /* Absolute addresses        */
int     priv = 0;                       /* 1=Private address space   */
int     prot = 0;                       /* 1=Protected page          */
int     stid;                           /* Segment table indication  */
U16     xcode;                          /* Exception code            */
BYTE    akey;                           /* Access key in register 0  */
BYTE    akey1, akey2;                   /* Access keys for operands  */
#if defined(FEATURE_EXPANDED_STORAGE)
int     xpvalid1 = 0, xpvalid2 = 0;     /* 1=Operand in expanded stg */
CREG    pte1 = 0, pte2 = 0;             /* Page table entry          */
U32     xpblk1 = 0, xpblk2 = 0;         /* Expanded storage block#   */
BYTE    xpkey1 = 0, xpkey2 = 0;         /* Expanded storage keys     */
#endif /*defined(FEATURE_EXPANDED_STORAGE)*/

    RRE(inst, execflag, regs, r1, r2);

    /* Specification exception if register 0 bits 16-19 are
       not all zero, or if bits 20 and 21 are both ones */
    if ((regs->GR_L(0) & 0x0000F000) != 0
        || (regs->GR_L(0) & 0x00000C00) == 0x00000C00)
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIFICATION_EXCEPTION);

    /* Use PSW key as access key for both operands */
    akey1 = akey2 = regs->psw.pkey;

    /* If register 0 bit 20 or 21 is one, get access key from R0 */
    if (regs->GR_L(0) & 0x00000C00)
    {
        /* Extract the access key from register 0 bits 24-27 */
        akey = regs->GR_L(0) & 0x000000F0;

        /* Priviliged operation exception if in problem state, and
           the specified key is not permitted by the PSW key mask */
        if ( regs->psw.prob
            && ((regs->CR(3) << (akey >> 4)) & 0x80000000) == 0 )
            ARCH_DEP(program_interrupt) (regs, PGM_PRIVILEGED_OPERATION_EXCEPTION);

        /* If register 0 bit 20 is one, use R0 key for operand 1 */
        if (regs->GR_L(0) & 0x00000800)
            akey1 = akey;

        /* If register 0 bit 21 is one, use R0 key for operand 2 */
        if (regs->GR_L(0) & 0x00000400)
            akey2 = akey;
    }

    /* Determine the logical addresses of each operand */
    vaddr1 = regs->GR(r1) & ADDRESS_MAXWRAP(regs);
    vaddr2 = regs->GR(r2) & ADDRESS_MAXWRAP(regs);

    /* Isolate the page addresses of each operand */
    vaddr1 &= XSTORE_PAGEMASK;
    vaddr2 &= XSTORE_PAGEMASK;

    /* Obtain the real or expanded address of each operand */
    if (!REAL_MODE(&regs->psw))
    {
        /* Translate the second operand address to a real address */
        rc = ARCH_DEP(translate_addr) (vaddr2, r2, regs, ACCTYPE_READ, &raddr2,
                        &xcode, &priv, &prot, &stid);

	raddr2 = APPLY_PREFIXING (raddr2, regs->PX);

        SIE_TRANSLATE(&raddr2,ACCTYPE_READ,regs);

#if defined(FEATURE_EXPANDED_STORAGE)
        if(rc == 2)
        {
	    FETCH_W(pte2,sysblk.mainstor + raddr2);
            /* If page is invalid in real storage but valid in expanded
               storage then xpblk2 now contains expanded storage block# */
            if(pte2 & PAGETAB_ESVALID)
            {
                xpblk2 = pte2 & ZPGETAB_PFRA >> 12;
#if defined(_FEATURE_SIE)
                if(regs->sie_state)
                {
                    /* Add expanded storage origin for this guest */
                    xpblk2 += regs->sie_xso;
                    /* If the block lies beyond this guests limit
                       then we must terminate the instruction */
                    if(xpblk2 >= regs->sie_xsl)
                    {
                        cc = 2;
                        goto mvpg_progck;
                    }
                }
#endif /*defined(_FEATURE_SIE)*/

                /* If the expanded storage block does not exist
                   then terminate the instruction */
                if (xpblk2 >= sysblk.xpndsize)
                {
                    cc = 2;
                    goto mvpg_progck;
                }

                rc = 0;
                xpvalid2 = 1;
                xpkey2 = sysblk.mainstor[raddr2 +
#if defined(FEATURE_ESAME)
                                                  2048
#else /*!defined(FEATURE_ESAME)*/
                /* For ESA/390 mode, the XPTE lies directly beyond 
                   the PTE, and each entry is 12 bytes long, we must
                   therefor add 1024 + 8 times the page index */
	                             1024 + ((vaddr1 & 0x000FF000) >> 9)
#endif /*!defined(FEATURE_ESAME)*/
                                                      ];
/*DEBUG*/ logmsg("MVPG pte2 = " F_CREG ", xkey2 = %2.2X\n",pte2,xpkey2);
            }
            else
            {
                cc = 2;
                goto mvpg_progck;
            }
        }
#endif /*defined(FEATURE_EXPANDED_STORAGE)*/

        /* Program check if second operand is not valid
           in either main storage or expanded storage */
        if (rc)
        {
	    cc = 2;
            goto mvpg_progck;
	}

        /* Translate the first operand address to a real address */
        rc = ARCH_DEP(translate_addr) (vaddr1, r1, regs, ACCTYPE_WRITE, &raddr1,
                        &xcode, &priv, &prot, &stid);

	raddr1 = APPLY_PREFIXING (raddr1, regs->PX);

        SIE_TRANSLATE(&raddr1,ACCTYPE_READ,regs);

#if defined(FEATURE_EXPANDED_STORAGE)
        if(rc == 2)
        {
	    FETCH_W(pte1,sysblk.mainstor + raddr1);
            /* If page is invalid in real storage but valid in expanded
               storage then xpblk1 now contains expanded storage block# */
            if(pte1 & PAGETAB_ESVALID)
            {
                xpblk1 = pte1 & ZPGETAB_PFRA >> 12;
#if defined(_FEATURE_SIE)
                if(regs->sie_state)
                {
                    /* Add expanded storage origin for this guest */
                    xpblk1 += regs->sie_xso;
                    /* If the block lies beyond this guests limit
                       then we must terminate the instruction */
                    if(xpblk1 >= regs->sie_xsl)
                    {
                        cc = 1;
                        goto mvpg_progck;
                    }
                }
#endif /*defined(_FEATURE_SIE)*/

                /* If the expanded storage block does not exist
                   then terminate the instruction */
                if (xpblk1 >= sysblk.xpndsize)
                {
                    cc = 1;
                    goto mvpg_progck;
                }

                rc = 0;
                xpvalid1 = 1;
                xpkey1 = sysblk.mainstor[raddr2 +
#if defined(FEATURE_ESAME)
                                                  2048
#else /*!defined(FEATURE_ESAME)*/
                /* For ESA/390 mode, the XPTE lies directly beyond 
                   the PTE, and each entry is 12 bytes long, we must
                   therefor add 1024 + 8 times the page index */
	                             1024 + ((vaddr1 & 0x000FF000) >> 9)
#endif /*!defined(FEATURE_ESAME)*/
                                                      ];
/*DEBUG*/ logmsg("MVPG pte1 = " F_CREG ", xkey1 = %2.2X\n",pte1,xpkey1);
            }
            else
            {
                cc = 1;
                goto mvpg_progck;
            }
        }
#endif /*defined(FEATURE_EXPANDED_STORAGE)*/

        /* Program check if operand not valid in main or expanded */
        if (rc)
        {
	    cc = 1;
            goto mvpg_progck;
	}

        /* Program check if page protection or access-list controlled
           protection applies to the first operand */
        if (prot)
            ARCH_DEP(program_interrupt) (regs, PGM_PROTECTION_EXCEPTION);

    } /* end if(!REAL_MODE) */

#if defined(FEATURE_EXPANDED_STORAGE)
    /* Program check if both operands are in expanded storage, or
       if first operand is in expanded storage and the destination
       reference intention (register 0 bit 22) is set to one */
    if ((xpvalid1 && xpvalid2)
        || (xpvalid1 && (regs->GR_L(0) & 0x00000200)))
    {
        xcode = PGM_PAGE_TRANSLATION_EXCEPTION;
        goto mvpg_progck;
    }

    /* Perform addressing and protection checks */
    if (xpvalid1)
    {
        /* Perform protection check on expanded storage block */
        if (akey1 != 0 && akey1 != (xpkey1 & STORKEY_KEY))
            ARCH_DEP(program_interrupt) (regs, PGM_PROTECTION_EXCEPTION);
    }
    else
#endif /*defined(FEATURE_EXPANDED_STORAGE)*/
    {
        /* Obtain absolute address of main storage block,
           check protection, and set reference and change bits */
        aaddr1 = LOGICAL_TO_ABS (vaddr1, r1, regs,
                                ACCTYPE_WRITE, akey1);
    }

#if defined(FEATURE_EXPANDED_STORAGE)
    if (xpvalid2)
    {
        /* Perform protection check on expanded storage block */
        if (akey2 != 0 && (xpkey2 & STORKEY_FETCH)
            && akey2 != (xpkey2 & STORKEY_KEY))
            ARCH_DEP(program_interrupt) (regs, PGM_PROTECTION_EXCEPTION);
    }
    else
#endif /*defined(FEATURE_EXPANDED_STORAGE)*/
    {
        /* Obtain absolute address of main storage block,
           check protection, and set reference bit */
        aaddr2 = LOGICAL_TO_ABS (vaddr2, r2, regs,
                                ACCTYPE_READ, akey2);
    }

#if defined(FEATURE_EXPANDED_STORAGE)
    /* Perform page movement */
    if (xpvalid2)
    {
        /* Set the main storage reference and change bits */
        STORAGE_KEY(aaddr1) |= (STORKEY_REF | STORKEY_CHANGE);

	/* Set Expanded Storage reference bit in the PTE */
        STORE_W(sysblk.mainstor + raddr2, pte2 | PAGETAB_ESREF);
        

        /* Move 4K bytes from expanded storage to main storage */
        memcpy (sysblk.mainstor + aaddr1,
                sysblk.xpndstor + (xpblk2 << XSTORE_PAGESHIFT),
                XSTORE_PAGESIZE);
    }
    else if (xpvalid1)
    {
        /* Set the main storage reference bit */
        STORAGE_KEY(aaddr2) |= STORKEY_REF;

	/* Set Expanded Storage reference and change bits in the PTE */
        STORE_W(sysblk.mainstor + raddr1, pte1 | PAGETAB_ESREF | PAGETAB_ESCHA);

        /* Move 4K bytes from main storage to expanded storage */
        memcpy (sysblk.xpndstor + (xpblk1 << XSTORE_PAGESHIFT),
                sysblk.mainstor + aaddr2,
                XSTORE_PAGESIZE);
    }
    else
#endif /*defined(FEATURE_EXPANDED_STORAGE)*/
    {
        /* Set the main storage reference and change bits */
        STORAGE_KEY(aaddr1) |= (STORKEY_REF | STORKEY_CHANGE);
        STORAGE_KEY(aaddr2) |= STORKEY_REF;

        /* Move 4K bytes from main storage to main storage */
        memcpy (sysblk.mainstor + aaddr1,
                sysblk.mainstor + aaddr2,
                XSTORE_PAGESIZE);
    }

    /* Return condition code zero */
    regs->psw.cc = 0;
    return;

mvpg_progck:
    /* If page translation exception and condition code option
       in register 0 bit 23 is set, return condition code */
    if ((regs->GR_L(0) & 0x00000100)
        && xcode == PGM_PAGE_TRANSLATION_EXCEPTION)
    {
        regs->psw.cc = cc;
        return;
    }

    /* Otherwise generate program check */
    ARCH_DEP(program_interrupt) (regs, xcode);
} /* end function move_page */

#endif /*defined(FEATURE_MOVE_PAGE_FACILITY_2)*/


#if !defined(_GEN_ARCH)

#define  _GEN_ARCH 390
#include "xstore.c"

#undef   _GEN_ARCH
#define  _GEN_ARCH 370
#include "xstore.c"

#endif /*!defined(_GEN_ARCH)*/

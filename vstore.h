/* VSTORE.H     (c) Copyright Roger Bowler, 1999-2001                */
/*              ESA/390 Dynamic Address Translation                  */

/* Interpretive Execution - (c) Copyright Jan Jaeger, 1999-2001      */
/* z/Architecture support - (c) Copyright Jan Jaeger, 1999-2001      */

/*-------------------------------------------------------------------*/
/* This module implements the DAT, ALET, and ASN translation         */
/* functions of the ESA/390 architecture, described in the manual    */
/* SA22-7201-04 ESA/390 Principles of Operation.  The numbers in     */
/* square brackets in the comments refer to sections in the manual.  */
/*-------------------------------------------------------------------*/

/*-------------------------------------------------------------------*/
/* Additional credits:                                               */
/*      S/370 DAT support by Jay Maynard (as described in            */
/*      GA22-7000 System/370 Principles of Operation)                */
/*      Clear remainder of ASTE when ASF=0 - Jan Jaeger              */
/*      S/370 DAT support when running under SIE - Jan Jaeger        */
/*-------------------------------------------------------------------*/

#if !defined(OPTION_NO_INLINE_VSTORE) || defined(_VSTORE_C)

/*-------------------------------------------------------------------*/
/* Store 1 to 256 characters into virtual storage operand            */
/*                                                                   */
/* Input:                                                            */
/*      src     1 to 256 byte input buffer                           */
/*      len     Size of operand minus 1                              */
/*      addr    Logical address of leftmost character of operand     */
/*      arn     Access register number                               */
/*      regs    CPU register context                                 */
/*                                                                   */
/*      A program check may be generated if the logical address      */
/*      range causes an addressing, translation, or protection       */
/*      exception, and in this case no real storage locations are    */
/*      updated, and the function does not return.                   */
/*-------------------------------------------------------------------*/
_VSTORE_C_STATIC void ARCH_DEP(vstorec) (void *src, BYTE len,
                                        VADR addr, int arn, REGS *regs)
{
VADR    addr2;                          /* Page address of last byte */
BYTE    len1;                           /* Length to end of page     */
BYTE    len2;                           /* Length after next page    */
BYTE    akey;                           /* Bits 0-3=key, 4-7=zeroes  */

    /* Obtain current access key from PSW */
    akey = regs->psw.pkey;

    /* Calculate page address of last byte of operand */
    addr2 = (addr + len) & ADDRESS_MAXWRAP(regs);
    addr2 &= PAGEFRAME_PAGEMASK;

    /* Copy data to real storage in either one or two parts
       depending on whether operand crosses a page boundary */
    if (addr2 == (addr & PAGEFRAME_PAGEMASK)) {
        addr = LOGICAL_TO_ABS (addr, arn, regs, ACCTYPE_WRITE, akey);
        memcpy (sysblk.mainstor+addr, src, len+1);
    } else {
        len1 = addr2 - addr;
        len2 = len - len1 + 1;
        addr = LOGICAL_TO_ABS_SKP (addr, arn, regs, ACCTYPE_WRITE_SKP, akey);
        addr2 = LOGICAL_TO_ABS_SKP (addr2, arn, regs, ACCTYPE_WRITE_SKP, akey);
        /* both pages are accessable, so set ref & change bits */
        STORAGE_KEY(addr) |= (STORKEY_REF | STORKEY_CHANGE);
        STORAGE_KEY(addr2) |= (STORKEY_REF | STORKEY_CHANGE);
        memcpy (sysblk.mainstor+addr, src, len1);
        memcpy (sysblk.mainstor+addr2, src+len1, len2);
    }
} /* end function ARCH_DEP(vstorec) */

/*-------------------------------------------------------------------*/
/* Store a single byte into virtual storage operand                  */
/*                                                                   */
/* Input:                                                            */
/*      value   Byte value to be stored                              */
/*      addr    Logical address of operand byte                      */
/*      arn     Access register number                               */
/*      regs    CPU register context                                 */
/*                                                                   */
/*      A program check may be generated if the logical address      */
/*      causes an addressing, translation, or protection             */
/*      exception, and in this case the function does not return.    */
/*-------------------------------------------------------------------*/
_VSTORE_C_STATIC void ARCH_DEP(vstoreb) (BYTE value, VADR addr,
                                                   int arn, REGS *regs)
{
    addr = LOGICAL_TO_ABS (addr, arn, regs, ACCTYPE_WRITE,
                                regs->psw.pkey);
    sysblk.mainstor[addr] = value;
} /* end function ARCH_DEP(vstoreb) */

/*-------------------------------------------------------------------*/
/* Store a two-byte integer into virtual storage operand             */
/*                                                                   */
/* Input:                                                            */
/*      value   16-bit integer value to be stored                    */
/*      addr    Logical address of leftmost operand byte             */
/*      arn     Access register number                               */
/*      regs    CPU register context                                 */
/*                                                                   */
/*      A program check may be generated if the logical address      */
/*      causes an addressing, translation, or protection             */
/*      exception, and in this case the function does not return.    */
/*-------------------------------------------------------------------*/
_VSTORE_C_STATIC void ARCH_DEP(vstore2) (U16 value, VADR addr, int arn,
                                                            REGS *regs)
{
VADR    addr2;                          /* Address of second byte    */
RADR    abs1, abs2;                     /* Absolute addresses        */
BYTE    akey;                           /* Bits 0-3=key, 4-7=zeroes  */

    /* Obtain current access key from PSW */
    akey = regs->psw.pkey;

    abs1 = LOGICAL_TO_ABS_SKP (addr, arn, regs, ACCTYPE_WRITE_SKP, regs->psw.pkey);

    /* Check if store crosses page or not */
    if (!(addr & 1) || (addr & PAGEFRAME_BYTEMASK) <= (PAGEFRAME_PAGESIZE - 2))
    {
        STORAGE_KEY(abs1) |= (STORKEY_REF | STORKEY_CHANGE);
        STORE_HW(sysblk.mainstor + abs1, value);
        return;
    }

    /* Page Crosser - get absolute address of both pages skipping change bit */
    addr2 = (addr + 1) & ADDRESS_MAXWRAP(regs);
    abs2 = LOGICAL_TO_ABS_SKP (addr2, arn, regs, ACCTYPE_WRITE_SKP, regs->psw.pkey);

    /* Both pages are accessable, now safe to set Reference and Change bits */
    STORAGE_KEY(abs1) |= (STORKEY_REF | STORKEY_CHANGE);
    STORAGE_KEY(abs2) |= (STORKEY_REF | STORKEY_CHANGE);

    /* Store integer value at operand location */
    sysblk.mainstor[abs1] = value >> 8;
    sysblk.mainstor[abs2] = value & 0xFF;

} /* end function ARCH_DEP(vstore2) */

/*-------------------------------------------------------------------*/
/* Store a four-byte integer into virtual storage operand            */
/*                                                                   */
/* Input:                                                            */
/*      value   32-bit integer value to be stored                    */
/*      addr    Logical address of leftmost operand byte             */
/*      arn     Access register number                               */
/*      regs    CPU register context                                 */
/*                                                                   */
/*      A program check may be generated if the logical address      */
/*      causes an addressing, translation, or protection             */
/*      exception, and in this case the function does not return.    */
/*-------------------------------------------------------------------*/
_VSTORE_C_STATIC void ARCH_DEP(vstore4) (U32 value, VADR addr, int arn,
                                                            REGS *regs)
{
int     i;                              /* Loop counter              */
int     k;                              /* Shift counter             */
VADR    addr2;                          /* Page address of last byte */
RADR    abs, abs2;                      /* Absolute addresses        */
BYTE    akey;                           /* Bits 0-3=key, 4-7=zeroes  */

    /* Obtain current access key from PSW */
    akey = regs->psw.pkey;

    abs = LOGICAL_TO_ABS_SKP (addr, arn, regs, ACCTYPE_WRITE_SKP, regs->psw.pkey);

    /* Check if store crosses page or not */
    if (!(addr & 3) || (addr & PAGEFRAME_BYTEMASK) <= (PAGEFRAME_PAGESIZE - 4))
    {
        STORAGE_KEY(abs) |= (STORKEY_REF | STORKEY_CHANGE);
        STORE_FW(sysblk.mainstor + abs, value);
        return;
    }

    /* Page Crosser - get absolute address of both pages skipping change bit */
    addr2 = ((addr + 3) & ADDRESS_MAXWRAP(regs)) & PAGEFRAME_PAGEMASK;
    abs2 = LOGICAL_TO_ABS_SKP (addr2, arn, regs, ACCTYPE_WRITE_SKP, regs->psw.pkey);

    /* Both pages are accessable, now safe to set Reference and Change bits */
    STORAGE_KEY(abs) |= (STORKEY_REF | STORKEY_CHANGE);
    STORAGE_KEY(abs2) |= (STORKEY_REF | STORKEY_CHANGE);

    /* Store integer value byte by byte at operand location */
    for (i=0, k=24; i < 4; i++, k -= 8) {

        /* Store byte in absolute storage */
        sysblk.mainstor[abs] = (value >> k) & 0xFF;

        /* Increment absolute address and virtual address */
        abs++;
        addr++;
        addr &= ADDRESS_MAXWRAP(regs);

        /* Adjust absolute address if page boundary crossed */
        if (addr == addr2)
            abs = abs2;

    } /* end for */

} /* end function ARCH_DEP(vstore4) */

/*-------------------------------------------------------------------*/
/* Store an eight-byte integer into virtual storage operand          */
/*                                                                   */
/* Input:                                                            */
/*      value   64-bit integer value to be stored                    */
/*      addr    Logical address of leftmost operand byte             */
/*      arn     Access register number                               */
/*      regs    CPU register context                                 */
/*                                                                   */
/*      A program check may be generated if the logical address      */
/*      causes an addressing, translation, or protection             */
/*      exception, and in this case the function does not return.    */
/*-------------------------------------------------------------------*/
_VSTORE_C_STATIC void ARCH_DEP(vstore8) (U64 value, VADR addr, int arn,
                                                            REGS *regs)
{
int     i;                              /* Loop counter              */
int     k;                              /* Shift counter             */
VADR    addr2;                          /* Page address of last byte */
RADR    abs, abs2;                      /* Absolute addresses        */
BYTE    akey;                           /* Bits 0-3=key, 4-7=zeroes  */

    /* Obtain current access key from PSW */
    akey = regs->psw.pkey;

    abs = LOGICAL_TO_ABS_SKP (addr, arn, regs, ACCTYPE_WRITE_SKP, regs->psw.pkey);
    /* Check if store crosses page or not */
    if (!(addr & 7) || (addr & PAGEFRAME_BYTEMASK) <= (PAGEFRAME_PAGESIZE - 8))
    {
        STORAGE_KEY(abs) |= (STORKEY_REF | STORKEY_CHANGE);
        STORE_DW(sysblk.mainstor + abs, value);
        return;
    }

    /* Page Crosser - get absolute address of both pages skipping change bit */
    addr2 = ((addr + 7) & ADDRESS_MAXWRAP(regs)) & PAGEFRAME_PAGEMASK;
    abs2 = LOGICAL_TO_ABS_SKP (addr2, arn, regs, ACCTYPE_WRITE_SKP, regs->psw.pkey);

    /* Both pages are accessable, now safe to set Reference and Change bits */
    STORAGE_KEY(abs) |= (STORKEY_REF | STORKEY_CHANGE);
    STORAGE_KEY(abs2) |= (STORKEY_REF | STORKEY_CHANGE);

    /* Store integer value byte by byte at operand location */
    for (i=0, k=56; i < 8; i++, k -= 8) {

        /* Store byte in absolute storage */
        sysblk.mainstor[abs] = (value >> k) & 0xFF;

        /* Increment absolute address and virtual address */
        abs++;
        addr++;
        addr &= ADDRESS_MAXWRAP(regs);

        /* Adjust absolute address if page boundary crossed */
        if (addr == addr2)
            abs = abs2;

    } /* end for */

} /* end function ARCH_DEP(vstore8) */

/*-------------------------------------------------------------------*/
/* Fetch a 1 to 256 character operand from virtual storage           */
/*                                                                   */
/* Input:                                                            */
/*      len     Size of operand minus 1                              */
/*      addr    Logical address of leftmost character of operand     */
/*      arn     Access register number                               */
/*      regs    CPU register context                                 */
/* Output:                                                           */
/*      dest    1 to 256 byte output buffer                          */
/*                                                                   */
/*      A program check may be generated if the logical address      */
/*      causes an addressing, translation, or fetch protection       */
/*      exception, and in this case the function does not return.    */
/*-------------------------------------------------------------------*/
_VSTORE_C_STATIC void ARCH_DEP(vfetchc) (void *dest, BYTE len,
                                        VADR addr, int arn, REGS *regs)
{
VADR    addr2;                          /* Page address of last byte */
BYTE    len1;                           /* Length to end of page     */
BYTE    len2;                           /* Length after next page    */
BYTE    akey;                           /* Bits 0-3=key, 4-7=zeroes  */

    /* Obtain current access key from PSW */
    akey = regs->psw.pkey;

    /* Calculate page address of last byte of operand */
    addr2 = (addr + len) & ADDRESS_MAXWRAP(regs);
    addr2 &= ~0x7FF;

    /* Copy data from real storage in either one or two parts
       depending on whether operand crosses a page boundary
       (Page boundary set at 800 to catch FPO crosser too) */
    if (addr2 == (addr & ~0x7FF)) {
        addr = LOGICAL_TO_ABS (addr, arn, regs, ACCTYPE_READ, akey);
        memcpy (dest, sysblk.mainstor+addr, len+1);
    } else {
        len1 = addr2 - addr;
        len2 = len - len1 + 1;
        addr = LOGICAL_TO_ABS (addr, arn, regs, ACCTYPE_READ, akey);
        addr2 = LOGICAL_TO_ABS (addr2, arn, regs, ACCTYPE_READ, akey);
        memcpy (dest, sysblk.mainstor+addr, len1);
        memcpy (dest+len1, sysblk.mainstor+addr2, len2);
    }
} /* end function ARCH_DEP(vfetchc) */

/*-------------------------------------------------------------------*/
/* Fetch a single byte operand from virtual storage                  */
/*                                                                   */
/* Input:                                                            */
/*      addr    Logical address of operand character                 */
/*      arn     Access register number                               */
/*      regs    CPU register context                                 */
/* Returns:                                                          */
/*      Operand byte                                                 */
/*                                                                   */
/*      A program check may be generated if the logical address      */
/*      causes an addressing, translation, or fetch protection       */
/*      exception, and in this case the function does not return.    */
/*-------------------------------------------------------------------*/
_VSTORE_C_STATIC BYTE ARCH_DEP(vfetchb) (VADR addr, int arn,
                                                            REGS *regs)
{
    addr = LOGICAL_TO_ABS (addr, arn, regs, ACCTYPE_READ,
                                regs->psw.pkey);
    return sysblk.mainstor[addr];
} /* end function ARCH_DEP(vfetchb) */

/*-------------------------------------------------------------------*/
/* Fetch a two-byte integer operand from virtual storage             */
/*                                                                   */
/* Input:                                                            */
/*      addr    Logical address of leftmost byte of operand          */
/*      arn     Access register number                               */
/*      regs    CPU register context                                 */
/* Returns:                                                          */
/*      Operand in 16-bit integer format                             */
/*                                                                   */
/*      A program check may be generated if the logical address      */
/*      causes an addressing, translation, or fetch protection       */
/*      exception, and in this case the function does not return.    */
/*-------------------------------------------------------------------*/
_VSTORE_C_STATIC U16 ARCH_DEP(vfetch2) (VADR addr, int arn, REGS *regs)
{
VADR    addr2;                          /* Address of second byte    */
RADR    abs1, abs2;                     /* Absolute addresses        */
BYTE    akey;                           /* Bits 0-3=key, 4-7=zeroes  */

    /* Obtain current access key from PSW */
    akey = regs->psw.pkey;

    /* Get absolute address of first byte of operand */
    abs1 = LOGICAL_TO_ABS (addr, arn, regs, ACCTYPE_READ, akey);

    /* Fetch 2 bytes when operand does not cross page boundary
       (Page boundary test at 800 to catch FPO crosser too) */
    if(!(addr & 1) || (abs1 & 0x000007FF) <= (2048 - 2))
        return fetch_hw(sysblk.mainstor + abs1);

    /* Calculate address of second byte of operand */
    addr2 = (addr + 1) & ADDRESS_MAXWRAP(regs);

    /* Repeat address translation for 2nd page */
    abs2 = LOGICAL_TO_ABS (addr2, arn, regs, ACCTYPE_READ, akey);

    /* Return integer value of operand */
    return (sysblk.mainstor[abs1] << 8) | sysblk.mainstor[abs2];
} /* end function ARCH_DEP(vfetch2) */

/*-------------------------------------------------------------------*/
/* Fetch a four-byte integer operand from virtual storage            */
/*                                                                   */
/* Input:                                                            */
/*      addr    Logical address of leftmost byte of operand          */
/*      arn     Access register number                               */
/*      regs    CPU register context                                 */
/* Returns:                                                          */
/*      Operand in 32-bit integer format                             */
/*                                                                   */
/*      A program check may be generated if the logical address      */
/*      causes an addressing, translation, or fetch protection       */
/*      exception, and in this case the function does not return.    */
/*-------------------------------------------------------------------*/
_VSTORE_C_STATIC U32 ARCH_DEP(vfetch4) (VADR addr, int arn, REGS *regs)
{
int     i;                              /* Loop counter              */
U32     value;                          /* Accumulated value         */
VADR    addr2;                          /* Page address of last byte */
RADR    abs, abs2;                      /* Absolute addresses        */
BYTE    akey;                           /* Bits 0-3=key, 4-7=zeroes  */

    /* Obtain current access key from PSW */
    akey = regs->psw.pkey;

    /* Get absolute address of first byte of operand */
    abs = LOGICAL_TO_ABS (addr, arn, regs, ACCTYPE_READ, akey);

    /* Fetch 4 bytes when operand does not cross page boundary
       (Page boundary test at 800 to catch FPO crosser too) */
    if(!(addr & 3) || (abs & 0x000007FF) <= (2048 - 4))
        return fetch_fw(sysblk.mainstor + abs);

    /* Operand is not fullword aligned and may cross a page boundary */

    /* Calculate page address of last byte of operand */
    addr2 = (addr + 3) & ADDRESS_MAXWRAP(regs);
    addr2 &= ~0x7FF;
    abs2 = LOGICAL_TO_ABS (addr2, arn, regs, ACCTYPE_READ, akey);

    /* Fetch integer value byte by byte from operand location */
    for (i=0, value=0; i < 4; i++) {

        /* Fetch byte from absolute storage */
        value <<= 8;
        value |= sysblk.mainstor[abs];

        /* Increment absolute address and virtual address */
        abs++;
        addr++;
        addr &= ADDRESS_MAXWRAP(regs);

        /* Adjust absolute address if page boundary crossed */
        if (addr == addr2)
            abs = abs2;

    } /* end for */
    return value;

} /* end function ARCH_DEP(vfetch4) */

/*-------------------------------------------------------------------*/
/* Fetch an eight-byte integer operand from virtual storage          */
/*                                                                   */
/* Input:                                                            */
/*      addr    Logical address of leftmost byte of operand          */
/*      arn     Access register number                               */
/*      regs    CPU register context                                 */
/* Returns:                                                          */
/*      Operand in 64-bit integer format                             */
/*                                                                   */
/*      A program check may be generated if the logical address      */
/*      causes an addressing, translation, or fetch protection       */
/*      exception, and in this case the function does not return.    */
/*-------------------------------------------------------------------*/
_VSTORE_C_STATIC U64 ARCH_DEP(vfetch8) (VADR addr, int arn, REGS *regs)
{
int     i;                              /* Loop counter              */
U64     value;                          /* Accumulated value         */
VADR    addr2;                          /* Page address of last byte */
RADR    abs, abs2;                      /* Absolute addresses        */
BYTE    akey;                           /* Bits 0-3=key, 4-7=zeroes  */

    /* Obtain current access key from PSW */
    akey = regs->psw.pkey;

    /* Get absolute address of first byte of operand */
    abs = LOGICAL_TO_ABS (addr, arn, regs, ACCTYPE_READ, akey);

    /* Fetch 8 bytes when operand does not cross page boundary
       (Page boundary test at 800 to catch FPO crosser too) */
    if(!(addr & 7) || (abs & 0x000007FF) <= (2048 - 8))
        return fetch_dw(sysblk.mainstor + abs);

    /* Calculate page address of last byte of operand */
    addr2 = (addr + 7) & ADDRESS_MAXWRAP(regs);
    addr2 &= ~0x7FF;
    abs2 = LOGICAL_TO_ABS (addr2, arn, regs, ACCTYPE_READ, akey);

    /* Fetch integer value byte by byte from operand location */
    for (i=0, value=0; i < 8; i++) {

        /* Fetch byte from absolute storage */
        value <<= 8;
        value |= sysblk.mainstor[abs];

        /* Increment absolute address and virtual address */
        abs++;
        addr++;
        addr &= ADDRESS_MAXWRAP(regs);

        /* Adjust absolute address if page boundary crossed */
        if (addr == addr2)
            abs = abs2;

    } /* end for */
    return value;

} /* end function ARCH_DEP(vfetch8) */

#endif

#if !defined(OPTION_NO_INLINE_IFETCH) | defined(_VSTORE_C)
/*-------------------------------------------------------------------*/
/* Fetch instruction from halfword-aligned virtual storage location  */
/*                                                                   */
/* Input:                                                            */
/*      dest    Pointer to 6-byte area to receive instruction bytes  */
/*      addr    Logical address of leftmost instruction halfword     */
/*      regs    Pointer to the CPU register context                  */
/*                                                                   */
/* Output:                                                           */
/*      If successful, from one to three instruction halfwords will  */
/*      be fetched from main storage and stored into the 6-byte area */
/*      pointed to by dest.                                          */
/*                                                                   */
/*      A program check may be generated if the instruction address  */
/*      is odd, or causes an addressing or translation exception,    */
/*      and in this case the function does not return.               */
/*-------------------------------------------------------------------*/
_VFETCH_C_STATIC void ARCH_DEP(instfetch) (BYTE *dest, VADR addr,
                                                            REGS *regs)
{
RADR    abs;                            /* Absolute storage address  */
BYTE    akey;                           /* Bits 0-3=key, 4-7=zeroes  */

    /* Obtain current access key from PSW */
    akey = regs->psw.pkey;

    /* Program check if instruction address is odd */
    if (addr & 0x01)
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIFICATION_EXCEPTION);

    /* Fetch six bytes if instruction cannot cross a page boundary */
    if ((addr & 0x7FF) <= (0x800 - 6))
    {
        abs = LOGICAL_TO_ABS (addr, 0, regs, ACCTYPE_INSTFETCH, akey);
#if defined(OPTION_AIA_BUFFER)
        regs->AI = abs & PAGEFRAME_PAGEMASK;
        regs->VI = addr & PAGEFRAME_PAGEMASK;
#endif /*defined(OPTION_AIA_BUFFER)*/
        memcpy (dest, sysblk.mainstor+abs, 6);
        return;
    }

    /* Fetch first two bytes of instruction */
    abs = LOGICAL_TO_ABS (addr, 0, regs, ACCTYPE_INSTFETCH, akey);
#if defined(OPTION_AIA_BUFFER)
    regs->AI = abs & PAGEFRAME_PAGEMASK;
    regs->VI = addr & PAGEFRAME_PAGEMASK;
#endif /*defined(OPTION_AIA_BUFFER)*/
    memcpy (dest, sysblk.mainstor+abs, 2);

    /* Return if two-byte instruction */
    if (dest[0] < 0x40) return;

    /* Fetch next two bytes of instruction */
    abs += 2;
    addr += 2;
    addr &= ADDRESS_MAXWRAP(regs);
    if ((addr & 0x7FF) == 0x000) {
        abs = LOGICAL_TO_ABS (addr, 0, regs, ACCTYPE_INSTFETCH, akey);
    }
    memcpy (dest+2, sysblk.mainstor+abs, 2);

    /* Return if four-byte instruction */
    if (dest[0] < 0xC0) return;

    /* Fetch next two bytes of instruction */
    abs += 2;
    addr += 2;
    addr &= ADDRESS_MAXWRAP(regs);
    if ((addr & 0x7FF) == 0x000) {
        abs = LOGICAL_TO_ABS (addr, 0, regs, ACCTYPE_INSTFETCH, akey);
    }
    memcpy (dest+4, sysblk.mainstor+abs, 2);

} /* end function ARCH_DEP(instfetch) */
#endif

#if !defined(OPTION_NO_INLINE_VSTORE) | defined(_VSTORE_C)
/*-------------------------------------------------------------------*/
/* Move characters using specified keys and address spaces           */
/*                                                                   */
/* Input:                                                            */
/*      addr1   Effective address of first operand                   */
/*      arn1    Access register number for first operand,            */
/*              or USE_PRIMARY_SPACE or USE_SECONDARY_SPACE          */
/*      key1    Bits 0-3=first operand access key, 4-7=zeroes        */
/*      addr2   Effective address of second operand                  */
/*      arn2    Access register number for second operand,           */
/*              or USE_PRIMARY_SPACE or USE_SECONDARY_SPACE          */
/*      key2    Bits 0-3=second operand access key, 4-7=zeroes       */
/*      len     Operand length minus 1 (range 0-255)                 */
/*      regs    Pointer to the CPU register context                  */
/*                                                                   */
/*      This function implements the MVC, MVCP, MVCS, MVCK, MVCSK,   */
/*      and MVCDK instructions.  These instructions move up to 256   */
/*      characters using the address space and key specified by      */
/*      the caller for each operand.  Operands are moved byte by     */
/*      byte to ensure correct processing of overlapping operands.   */
/*                                                                   */
/*      The arn parameter for each operand may be an access          */
/*      register number, in which case the operand is in the         */
/*      primary, secondary, or home space, or in the space           */
/*      designated by the specified access register, according to    */
/*      the current PSW addressing mode.                             */
/*                                                                   */
/*      Alternatively the arn parameter may be one of the special    */
/*      values USE_PRIMARY_SPACE or USE_SECONDARY_SPACE in which     */
/*      case the operand is in the specified space regardless of     */
/*      the current PSW addressing mode.                             */
/*                                                                   */
/*      A program check may be generated if either logical address   */
/*      causes an addressing, protection, or translation exception,  */
/*      and in this case the function does not return.               */
/*-------------------------------------------------------------------*/
_VSTORE_C_STATIC void ARCH_DEP(move_chars) (VADR addr1, int arn1,
       BYTE key1, VADR addr2, int arn2, BYTE key2, int len, REGS *regs)
{
RADR    abs1, abs2;                     /* Absolute addresses        */
VADR    npv1, npv2;                     /* Next page virtual addrs   */
RADR    npa1 = 0, npa2 = 0;             /* Next page absolute addrs  */
int     i;                              /* Loop counter              */
BYTE    obyte;                          /* Operand byte              */
#ifdef OPTION_FAST_MOVECHAR
BYTE    slow = 0;
#endif

    /* Translate addresses of leftmost operand bytes */
    abs1 = LOGICAL_TO_ABS_SKP (addr1, arn1, regs, ACCTYPE_WRITE_SKP, key1);
    abs2 = LOGICAL_TO_ABS (addr2, arn2, regs, ACCTYPE_READ, key2);

    /* Calculate page addresses of rightmost operand bytes */
    npv1 = (addr1 + len) & ADDRESS_MAXWRAP(regs);
    npv1 &= PAGEFRAME_PAGEMASK;
    npv2 = (addr2 + len) & ADDRESS_MAXWRAP(regs);
    npv2 &= PAGEFRAME_PAGEMASK;

    /* If FPO crosser, check last byte for protection */
    if (addr2 < 0x800 && addr2 + len >= 0x800)
        npa2 = LOGICAL_TO_ABS (addr2 + len, arn2, regs, ACCTYPE_READ, key2);

    /* Translate next page addresses if page boundary crossed */
    if (npv1 != (addr1 & PAGEFRAME_PAGEMASK))
    {
        npa1 = LOGICAL_TO_ABS_SKP (npv1, arn1, regs, ACCTYPE_WRITE_SKP, key1);
#ifdef OPTION_FAST_MOVECHAR
        slow = 1;
#endif
    }
    if (npv2 != (addr2 & PAGEFRAME_PAGEMASK))
    {
        npa2 = LOGICAL_TO_ABS (npv2, arn2, regs, ACCTYPE_READ, key2);
#ifdef OPTION_FAST_MOVECHAR
        slow = 1;
#endif
    }

    /* all operands and page crossers valid, now alter ref & chg bits */
    STORAGE_KEY(abs1) |= (STORKEY_REF | STORKEY_CHANGE);
    if (npa1)
        STORAGE_KEY(npa1) |= (STORKEY_REF | STORKEY_CHANGE);

#ifdef FEATURE_INTERVAL_TIMER
    /* Special case for mvc to/from interval timer */
    if ( (len == 3) && ((abs1 == 0x50) || (abs2 == 0x50)) && ~((abs1 | abs2) & 3) )
    {
        /* We've got a 4-byte wide, 4-byte aligned access of the interval timer */
        obtain_lock( &sysblk.todlock );

        *(U32 *)&sysblk.mainstor[abs1] = *(U32 *)&sysblk.mainstor[abs2];

        release_lock( &sysblk.todlock );
    } else {
#endif /* FEATURE_INTERVAL_TIMER */

#ifdef OPTION_FAST_MOVECHAR
    if (!slow)
    {
        for (i = 0; i < len + 1; i++)
            sysblk.mainstor[abs1++] = sysblk.mainstor[abs2++];
        return;
    }
#endif

    /* Process operands from left to right */
    for ( i = 0; i < len+1; i++ )
    {
        /* Fetch a byte from the source operand */
        obyte = sysblk.mainstor[abs2];

        /* Store the byte in the destination operand */
        sysblk.mainstor[abs1] = obyte;

        /* Increment first operand address */
        addr1++;
        addr1 &= ADDRESS_MAXWRAP(regs);
        abs1++;

        /* Adjust absolute address if page boundary crossed */
        if ((addr1 & PAGEFRAME_BYTEMASK) == 0x000)
            abs1 = npa1;

        /* Increment second operand address */
        addr2++;
        addr2 &= ADDRESS_MAXWRAP(regs);
        abs2++;

        /* Adjust absolute address if page boundary crossed */
        if ((addr2 & PAGEFRAME_BYTEMASK) == 0x000)
            abs2 = npa2;

    } /* end for(i) */
#ifdef FEATURE_INTERVAL_TIMER
    }
#endif /* FEATURE_INTERVAL_TIMER */

} /* end function ARCH_DEP(move_chars) */


/*-------------------------------------------------------------------*/
/* Validate operand                                                  */
/*                                                                   */
/* Input:                                                            */
/*      addr    Effective address of operand                         */
/*      arn     Access register number                               */
/*      len     Operand length minus 1 (range 0-255)                 */
/*      acctype Type of access requested: READ or WRITE              */
/*      regs    Pointer to the CPU register context                  */
/*                                                                   */
/*      The purpose of this function is to allow an instruction      */
/*      operand to be validated for addressing, protection, and      */
/*      translation exceptions, thus allowing the instruction to     */
/*      be nullified or suppressed before any updates occur.         */
/*                                                                   */
/*      A program check is generated if the operand causes an        */
/*      addressing, protection, or translation exception, and        */
/*      in this case the function does not return.                   */
/*-------------------------------------------------------------------*/
_VSTORE_C_STATIC void ARCH_DEP(validate_operand) (VADR addr, int arn,
                                      int len, int acctype, REGS *regs)
{
VADR    npv;                            /* Next page virtual address */
BYTE    akey;                           /* Bits 0-3=key, 4-7=zeroes  */

    /* Obtain current access key from PSW */
    akey = regs->psw.pkey;

    /* Translate address of leftmost operand byte */
    LOGICAL_TO_ABS (addr, arn, regs, acctype, akey);

    /* Calculate page address of rightmost operand byte */
    npv = (addr + len) & ADDRESS_MAXWRAP(regs);
    npv &= PAGEFRAME_PAGEMASK;

    /* Translate next page address if page boundary crossed */
    if (npv != (addr & PAGEFRAME_PAGEMASK))
        LOGICAL_TO_ABS (npv, arn, regs, acctype, akey);

} /* end function ARCH_DEP(validate_operand) */

#endif /*!defined(OPTION_NO_INLINE_VSTORE) || defined(_VSTORE_C)*/

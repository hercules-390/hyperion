/* VSTORE.H     (c) Copyright Roger Bowler, 1999-2004                */
/*              ESA/390 Dynamic Address Translation                  */

/* Interpretive Execution - (c) Copyright Jan Jaeger, 1999-2004      */
/* z/Architecture support - (c) Copyright Jan Jaeger, 1999-2004      */

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
    if (likely(addr2 == (addr & PAGEFRAME_PAGEMASK))) {
        addr = LOGICAL_TO_ABS (addr, arn, regs, ACCTYPE_WRITE, akey);
        MEMCPY (regs->mainstor+addr, src, len+1);
    } else {
        len1 = addr2 - addr;
        len2 = len - len1 + 1;
        addr = LOGICAL_TO_ABS (addr, arn, regs, ACCTYPE_WRITE_SKP, akey);
        addr2 = LOGICAL_TO_ABS (addr2, arn, regs, ACCTYPE_WRITE_SKP, akey);
        /* both pages are accessable, so set ref & change bits */
        STORAGE_KEY(addr, regs) |= (STORKEY_REF | STORKEY_CHANGE);
        STORAGE_KEY(addr2, regs) |= (STORKEY_REF | STORKEY_CHANGE);
        MEMCPY (regs->mainstor+addr, src, len1);
        MEMCPY (regs->mainstor+addr2, src+len1, len2);
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
    regs->mainstor[addr] = value;
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
_VSTORE_C_STATIC void ARCH_DEP(vstore2_full) (U16 value, VADR addr, int arn,
                                                            REGS *regs)
{
VADR    addr2;                          /* Address of second byte    */
RADR    abs1, abs2;                     /* Absolute addresses        */
BYTE    akey;                           /* Bits 0-3=key, 4-7=zeroes  */

    /* Obtain current access key from PSW */
    akey = regs->psw.pkey;

    abs1 = LOGICAL_TO_ABS (addr, arn, regs, ACCTYPE_WRITE_SKP, regs->psw.pkey);

    /* Check if store crosses page or not */
    if ((addr & PAGEFRAME_BYTEMASK) <= (PAGEFRAME_PAGESIZE - 2))
    {
        STORAGE_KEY(abs1, regs) |= (STORKEY_REF | STORKEY_CHANGE);
        STORE_HW(regs->mainstor + abs1, value);
        return;
    }

    /* Page Crosser - get absolute address of both pages skipping change bit */
    addr2 = (addr + 1) & ADDRESS_MAXWRAP(regs);
    abs2 = LOGICAL_TO_ABS (addr2, arn, regs, ACCTYPE_WRITE_SKP, regs->psw.pkey);

    /* Both pages are accessable, now safe to set Reference and Change bits */
    STORAGE_KEY(abs1, regs) |= (STORKEY_REF | STORKEY_CHANGE);
    STORAGE_KEY(abs2, regs) |= (STORKEY_REF | STORKEY_CHANGE);

    /* Store integer value at operand location */
    regs->mainstor[abs1] = value >> 8;
    regs->mainstor[abs2] = value & 0xFF;

} /* end function ARCH_DEP(vstore2) */
/* vstore2 accelerator - Simple case only (better inline candidate) */
_VSTORE_C_STATIC void ARCH_DEP(vstore2) (U16 value, VADR addr, int arn,
                                                            REGS *regs)
{
    /* Most common case : Aligned & not crossing page boundary */
    if(likely(!(addr & 0x01)) || ((addr & 0x7ff) <= 0x7fe))
    {
        register RADR abs;
        abs=LOGICAL_TO_ABS(addr,arn,regs,ACCTYPE_WRITE,regs->psw.pkey);
        STORE_HW(regs->mainstor + abs, value);
        return;
    }
    ARCH_DEP(vstore2_full)(value,addr,arn,regs);
}

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
_VSTORE_C_STATIC void ARCH_DEP(vstore4_full) (U32 value, VADR addr, int arn,
                                                            REGS *regs)
{
int     i;                              /* Loop counter              */
int     k;                              /* Shift counter             */
VADR    addr2;                          /* Page address of last byte */
RADR    abs, abs2;                      /* Absolute addresses        */
BYTE    akey;                           /* Bits 0-3=key, 4-7=zeroes  */

    /* Obtain current access key from PSW */
    akey = regs->psw.pkey;

    abs = LOGICAL_TO_ABS (addr, arn, regs, ACCTYPE_WRITE_SKP, regs->psw.pkey);

    /* Check if store crosses page or not */
    if ((addr & PAGEFRAME_BYTEMASK) <= (PAGEFRAME_PAGESIZE - 4))
    {
        STORAGE_KEY(abs, regs) |= (STORKEY_REF | STORKEY_CHANGE);
        STORE_FW(regs->mainstor + abs, value);
        return;
    }

    /* Page Crosser - get absolute address of both pages skipping change bit */
    addr2 = ((addr + 3) & ADDRESS_MAXWRAP(regs)) & PAGEFRAME_PAGEMASK;
    abs2 = LOGICAL_TO_ABS (addr2, arn, regs, ACCTYPE_WRITE_SKP, regs->psw.pkey);

    /* Both pages are accessable, now safe to set Reference and Change bits */
    STORAGE_KEY(abs, regs) |= (STORKEY_REF | STORKEY_CHANGE);
    STORAGE_KEY(abs2, regs) |= (STORKEY_REF | STORKEY_CHANGE);

    /* Store integer value byte by byte at operand location */
    for (i=0, k=24; i < 4; i++, k -= 8) {

        /* Store byte in absolute storage */
        regs->mainstor[abs] = (value >> k) & 0xFF;

        /* Increment absolute address and virtual address */
        abs++;
        addr++;
        addr &= ADDRESS_MAXWRAP(regs);

        /* Adjust absolute address if page boundary crossed */
        if (addr == addr2)
            abs = abs2;

    } /* end for */

} /* end function ARCH_DEP(vstore4_full) */

/* vstore4 accelerator - Simple case only (better inline candidate) */
_VSTORE_C_STATIC void ARCH_DEP(vstore4) (U32 value, VADR addr, int arn,
                                                            REGS *regs)
{
    /* Most common case : Aligned & not crossing page boundary */
    if(likely(!(addr & 0x03)) || ((addr & 0x7ff) <= 0x7fc))
    {
        register RADR abs;
        abs=LOGICAL_TO_ABS(addr,arn,regs,ACCTYPE_WRITE,regs->psw.pkey);
        STORE_FW(regs->mainstor + abs, value);
        return;
    }
    ARCH_DEP(vstore4_full)(value,addr,arn,regs);
}

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
_VSTORE_C_STATIC void ARCH_DEP(vstore8_full) (U64 value, VADR addr, int arn,
                                                            REGS *regs)
{
int     i;                              /* Loop counter              */
int     k;                              /* Shift counter             */
VADR    addr2;                          /* Page address of last byte */
RADR    abs, abs2;                      /* Absolute addresses        */
BYTE    akey;                           /* Bits 0-3=key, 4-7=zeroes  */

    /* Obtain current access key from PSW */
    akey = regs->psw.pkey;

    abs = LOGICAL_TO_ABS (addr, arn, regs, ACCTYPE_WRITE_SKP, regs->psw.pkey);
    /* Check if store crosses page or not */
    if ((addr & PAGEFRAME_BYTEMASK) <= (PAGEFRAME_PAGESIZE - 8))
    {
        STORAGE_KEY(abs, regs) |= (STORKEY_REF | STORKEY_CHANGE);
        STORE_DW(regs->mainstor + abs, value);
        return;
    }

    /* Page Crosser - get absolute address of both pages skipping change bit */
    addr2 = ((addr + 7) & ADDRESS_MAXWRAP(regs)) & PAGEFRAME_PAGEMASK;
    abs2 = LOGICAL_TO_ABS (addr2, arn, regs, ACCTYPE_WRITE_SKP, regs->psw.pkey);

    /* Both pages are accessable, now safe to set Reference and Change bits */
    STORAGE_KEY(abs, regs) |= (STORKEY_REF | STORKEY_CHANGE);
    STORAGE_KEY(abs2, regs) |= (STORKEY_REF | STORKEY_CHANGE);

    /* Store integer value byte by byte at operand location */
    for (i=0, k=56; i < 8; i++, k -= 8) {

        /* Store byte in absolute storage */
        regs->mainstor[abs] = (value >> k) & 0xFF;

        /* Increment absolute address and virtual address */
        abs++;
        addr++;
        addr &= ADDRESS_MAXWRAP(regs);

        /* Adjust absolute address if page boundary crossed */
        if (addr == addr2)
            abs = abs2;

    } /* end for */

} /* end function ARCH_DEP(vstore8) */
_VSTORE_C_STATIC void ARCH_DEP(vstore8) (U64 value, VADR addr, int arn,
                                                            REGS *regs)
{
    /* Most common case : Aligned & not crossing page boundary */
    if(likely(!(addr & 0x07)) || ((addr & 0x7ff) <= 0x7f8))
    {
        register RADR abs;
        abs=LOGICAL_TO_ABS(addr,arn,regs,ACCTYPE_WRITE,regs->psw.pkey);
        STORE_DW(regs->mainstor + abs, value);
        return;
    }
    ARCH_DEP(vstore8_full)(value,addr,arn,regs);
}

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
#if 1
BYTE   *main;                           /* Main storage addresses    */
VADR    addr2;                          /* Address next page         */
int     len2;                           /* Length to copy on page    */

    main = LOGICAL_TO_ABS(addr,arn,regs,ACCTYPE_READ,regs->psw.pkey)
         + regs->mainstor;

    if ( likely((addr & 0x7FF) <= 0x7FF - len) )
    {
#ifdef FEATURE_INTERVAL_TIMER
        if (unlikely(addr == 80))
        {
            obtain_lock( &sysblk.todlock );
            update_TOD_clock ();
        }
#endif /*FEATURE_INTERVAL_TIMER*/

        MEMCPY (dest, main, len + 1);

#ifdef FEATURE_INTERVAL_TIMER
        if (unlikely(addr == 80))
            release_lock( &sysblk.todlock );
#endif /*FEATURE_INTERVAL_TIMER*/
    }
    else
    {
        /* Possibly crossing page or FPO boundary */
        len2 = 0x800 - (addr & 0x7FF);
        MEMCPY (dest, main, len2);
        addr = (addr + len2) & ADDRESS_MAXWRAP(regs);
        main = LOGICAL_TO_ABS(addr,arn,regs,ACCTYPE_READ,regs->psw.pkey)
             + regs->mainstor;
        MEMCPY (dest + len2, main, len + 1 - len2);
    }

#elif 0
BYTE   *main;                           /* Main storage addresses    */
int     len2;                           /* Length to copy on page    */

    main = LOGICAL_TO_ABS(addr,arn,regs,ACCTYPE_READ,regs->psw.pkey)
         + regs->mainstor;

    switch (len) {

    /* 4 bytes - 33.2% */
//NOTE: we could do `( !(addr & len) && (addr & 0x7FF) <= 0x7FC) )' too
//      since `len' is the actual length - 1
    case 3:  if ( likely((addr & 0x7FF) <= 0x7FC) )
             {
#ifdef FEATURE_INTERVAL_TIMER
                 if (unlikely(addr == 80))
                 {
                     obtain_lock( &sysblk.todlock );
                     update_TOD_clock ();
                 }
#endif /*FEATURE_INTERVAL_TIMER*/

                 memcpy (dest, main, 4);

#ifdef FEATURE_INTERVAL_TIMER
                 if (unlikely(addr == 80))
                     release_lock( &sysblk.todlock );
#endif /*FEATURE_INTERVAL_TIMER*/

                 return;
             }
             break;

    /* 8 bytes - 17.1% */
    case 7:  if ( likely((addr & 0x7FF) <= 0x7F8) )
             {
                 memcpy (dest, main, 8);
                 return;
             }
             break;

    /* 3 bytes - 14.8% */
    case 2:  if ( likely((addr & 0x7FF) <= 0x7FD) )
             {
                 memcpy (dest, main, 3);
                 return;
             }
             break;

    /* 1 byte  -  2.9% (but not a page crosser) */
    case 0:  memcpy (dest, main, 1);
             return;
             break;

    default: if ( likely((addr & 0x7FF) <= 0x7FF - len) )
             {
                 MEMCPY (dest, main, len + 1);
                 return;
             }
             break;
    } /* switch (len) */

    /* If here then a page is being crossed */
    len2 = 0x800 - (addr & 0x7FF);
    MEMCPY (dest, main, len2);
    addr = (addr + len2) & ADDRESS_MAXWRAP(regs);
    main = LOGICAL_TO_ABS(addr,arn,regs,ACCTYPE_READ,regs->psw.pkey)
          + regs->mainstor;
    MEMCPY (dest + len2, main, len + 1 - len2);

#else
VADR    addr2;                          /* Page address of last byte */
BYTE    len1;                           /* Length to end of page     */
BYTE    len2;                           /* Length after next page    */
BYTE    akey;                           /* Bits 0-3=key, 4-7=zeroes  */

#ifdef FEATURE_INTERVAL_TIMER
int     intaccess = 0;                  /* Access interval timer     */
#endif /*FEATURE_INTERVAL_TIMER*/

    /* Obtain current access key from PSW */
    akey = regs->psw.pkey;

    /* Calculate page address of last byte of operand */
    addr2 = (addr + len) & ADDRESS_MAXWRAP(regs);
    addr2 &= ~0x7FF;

#ifdef FEATURE_INTERVAL_TIMER
    /* Check for interval timer access */
    if (unlikely(addr == 80))
        if (len == 3)
          {
            intaccess = 1;
            obtain_lock( &sysblk.todlock );
            update_TOD_clock ();
          }
#endif /*FEATURE_INTERVAL_TIMER*/

    /* Copy data from real storage in either one or two parts
       depending on whether operand crosses a page boundary
       (Page boundary set at 800 to catch FPO crosser too) */
    if (likely(addr2 == (addr & ~0x7FF))) {
        addr = LOGICAL_TO_ABS (addr, arn, regs, ACCTYPE_READ, akey);
        MEMCPY (dest, regs->mainstor+addr, len+1);
    } else {
        len1 = addr2 - addr;
        len2 = len - len1 + 1;
        addr = LOGICAL_TO_ABS (addr, arn, regs, ACCTYPE_READ, akey);
        addr2 = LOGICAL_TO_ABS (addr2, arn, regs, ACCTYPE_READ, akey);
        MEMCPY (dest, regs->mainstor+addr, len1);
        MEMCPY (dest+len1, regs->mainstor+addr2, len2);
    }

#ifdef FEATURE_INTERVAL_TIMER
    /* Release todlock, if held */
    if (unlikely(intaccess))
      {
        release_lock( &sysblk.todlock );
        intaccess = 0;
      }
#endif /*FEATURE_INTERVAL_TIMER*/

#endif
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
    return regs->mainstor[addr];
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
_VSTORE_C_STATIC U16 ARCH_DEP(vfetch2_full) (VADR addr, int arn, REGS *regs)
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
    if((abs1 & 0x000007FF) <= (2048 - 2))
        return fetch_hw(regs->mainstor + abs1);

    /* Calculate address of second byte of operand */
    addr2 = (addr + 1) & ADDRESS_MAXWRAP(regs);

    /* Repeat address translation for 2nd page */
    abs2 = LOGICAL_TO_ABS (addr2, arn, regs, ACCTYPE_READ, akey);

    /* Return integer value of operand */
    return (regs->mainstor[abs1] << 8) | regs->mainstor[abs2];
} /* end function ARCH_DEP(vfetch2) */
_VSTORE_C_STATIC U16 ARCH_DEP(vfetch2) (VADR addr, int arn, REGS *regs)
{
    if(likely(!(addr & 0x01)) || ((addr & 0x7ff) !=0x7ff ))
    {
        register RADR abs;
        abs=LOGICAL_TO_ABS(addr,arn,regs,ACCTYPE_READ,regs->psw.pkey);
        return(fetch_hw(regs->mainstor + abs));
    }
    return(ARCH_DEP(vfetch2_full)(addr,arn,regs));
}

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
_VSTORE_C_STATIC U32 ARCH_DEP(vfetch4_full) (VADR addr, int arn, REGS *regs)
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
    if((abs & 0x000007FF) <= (2048 - 4))
#ifdef FEATURE_INTERVAL_TIMER
      {
        if (abs == 80)
          {
            obtain_lock( &sysblk.todlock );
            update_TOD_clock ();
            release_lock( &sysblk.todlock );
          }
#endif /*FEATURE_INTERVAL_TIMER*/
        return fetch_fw(regs->mainstor + abs);
#ifdef FEATURE_INTERVAL_TIMER
      }
#endif /*FEATURE_INTERVAL_TIMER*/

    /* Operand is not fullword aligned and may cross a page boundary */

    /* Calculate page address of last byte of operand */
    addr2 = (addr + 3) & ADDRESS_MAXWRAP(regs);
    addr2 &= ~0x7FF;
    abs2 = LOGICAL_TO_ABS (addr2, arn, regs, ACCTYPE_READ, akey);

    /* Fetch integer value byte by byte from operand location */
    for (i=0, value=0; i < 4; i++) {

        /* Fetch byte from absolute storage */
        value <<= 8;
        value |= regs->mainstor[abs];

        /* Increment absolute address and virtual address */
        abs++;
        addr++;
        addr &= ADDRESS_MAXWRAP(regs);

        /* Adjust absolute address if page boundary crossed */
        if (addr == addr2)
            abs = abs2;

    } /* end for */
    return value;

} /* end function ARCH_DEP(vfetch4_full) */
_VSTORE_C_STATIC U32 ARCH_DEP(vfetch4) (VADR addr, int arn, REGS *regs)
{
    if(likely(!(addr & 0x03)) || ((addr & 0x7ff) <= 0x7fc ))
    {
        register RADR abs;
        abs=LOGICAL_TO_ABS(addr,arn,regs,ACCTYPE_READ,regs->psw.pkey);
#if defined(FEATURE_INTERVAL_TIMER)
        if(abs!=80)
        {
#endif
            return(fetch_fw(regs->mainstor + abs));
#if defined(FEATURE_INTERVAL_TIMER)
        }
        return(ARCH_DEP(vfetch4_full)(addr,arn,regs));
#endif
    }
    return(ARCH_DEP(vfetch4_full)(addr,arn,regs));
}

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
_VSTORE_C_STATIC U64 ARCH_DEP(vfetch8_full) (VADR addr, int arn, REGS *regs)
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
    if((abs & 0x000007FF) <= (2048 - 8))
        return fetch_dw(regs->mainstor + abs);

    /* Calculate page address of last byte of operand */
    addr2 = (addr + 7) & ADDRESS_MAXWRAP(regs);
    addr2 &= ~0x7FF;
    abs2 = LOGICAL_TO_ABS (addr2, arn, regs, ACCTYPE_READ, akey);

    /* Fetch integer value byte by byte from operand location */
    for (i=0, value=0; i < 8; i++) {

        /* Fetch byte from absolute storage */
        value <<= 8;
        value |= regs->mainstor[abs];

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
_VSTORE_C_STATIC U64 ARCH_DEP(vfetch8) (VADR addr, int arn, REGS *regs)
{
    if(likely(!(addr & 0x07)) || ((addr & 0x7ff) <= 0x7f8 ))
    {
        register RADR abs;
        abs = LOGICAL_TO_ABS (addr, arn, regs, ACCTYPE_READ, regs->psw.pkey);
        return fetch_dw(regs->mainstor + abs);
    }
    return ARCH_DEP(vfetch8_full)(addr,arn,regs);
}
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
_VFETCH_C_STATIC BYTE * ARCH_DEP(instfetch) (BYTE *dest, VADR addr,
                                                            REGS *regs)
{
RADR    abs;                            /* Absolute storage address  */
BYTE   *ia;                             /* Instruction address       */
int     ilc, len = 0;                   /* Lengths for page crossing */
VADR    mask;                           /* Mask for page crossing    */

    /* Program check if instruction address is odd */
    if ( unlikely(addr & 0x01) )
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIFICATION_EXCEPTION);

#if defined(FEATURE_PER)
    /* Save the address address used to fetch the instruction */
    if( unlikely(EN_IC_PER(regs)) )
    {
#if defined(FEATURE_PER2)
        regs->perc = 0x40    /* ATMID-validity */
                   | (regs->psw.amode64 << 7)
                   | (regs->psw.amode << 5)
                   | (!REAL_MODE(&regs->psw) ? 0x10 : 0)
                   | (SPACE_BIT(&regs->psw) << 3)
                   | (AR_BIT(&regs->psw) << 2);
#else /*!defined(FEATURE_PER2)*/
        regs->perc = 0;
#endif /*!defined(FEATURE_PER2)*/

        /* For EXecute instvalid will be true */
        if(!regs->instvalid)
            regs->peradr = addr;

        if( EN_IC_PER_IF(regs)
          && PER_RANGE_CHECK(addr,regs->CR(10),regs->CR(11)) )
            ON_IC_PER_IF(regs);
    }
#endif /*defined(FEATURE_PER)*/

    /* Get instruction address */
    abs = LOGICAL_TO_ABS (addr, 0, regs, ACCTYPE_INSTFETCH, regs->psw.pkey);
    ia = abs + regs->mainstor;

    /* If page is crossed then copy instruction to the destination */
    ilc = ILC(ia[0]);
    mask = addr < PSA_SIZE ? 0x7FF : PAGEFRAME_BYTEMASK;
    if ( unlikely((addr & mask) > mask + 1 - ilc) )
    {
     // NOTE: `dest' must be at least 8 bytes
        len = (mask + 1) - (addr & mask);
        memcpy (dest, ia, 4);
        addr += len;
        addr &= ADDRESS_MAXWRAP(regs);
        abs = LOGICAL_TO_ABS (addr, 0, regs, ACCTYPE_INSTFETCH, regs->psw.pkey);
        ia = abs + regs->mainstor;
        memcpy(dest + len, ia, 4);
    }

    /* Update the AIA */
    if (!regs->instvalid
#if defined(FEATURE_PER)
      && !EN_IC_PER(regs)
#endif /*defined(FEATURE_PER)*/
       )
    {
        regs->VI = addr & PAGEFRAME_PAGEMASK;
        regs->VIE = (addr | mask) - 5;
        abs &= PAGEFRAME_PAGEMASK;
        regs->AI = abs;
        regs->mi = NEW_IADDR(regs, addr, abs + regs->mainstor);
    }

    regs->instvalid = 1;
    return len ? dest : ia;

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
BYTE   *dest1, *dest2;                  /* Destination addresses     */
RADR    abs1, abs2;                     /* Destination abs addresses */
BYTE   *source1, *source2;              /* Source addresses          */
int     len2, len3;                     /* Lengths to copy           */
int     i;                              /* Loop counter              */

    /* Translate addresses of leftmost operand bytes */
    abs1 = LOGICAL_TO_ABS (addr1, arn1, regs, ACCTYPE_WRITE_SKP, key1);
    dest1 = abs1 + regs->mainstor;
    source1 = LOGICAL_TO_ABS (addr2, arn2, regs, ACCTYPE_READ, key2)
            + regs->mainstor;

    /* Quick out if copying just 1 byte */
    if (unlikely(len == 0))
    {
        *dest1 = *source1;
        STORAGE_KEY(abs1, regs) |= (STORKEY_REF | STORKEY_CHANGE);
        return;
    }

    /* There are several scenarios (in optimal order):
     * (1) dest boundary and source boundary not crossed
     *     (a) no operand overlap
     *     (b) operand overlap
     * (2) dest boundary not crossed and source boundary crossed
     * (3) dest boundary crossed and source boundary not crossed
     * (4) dest boundary and source boundary are crossed
     *     (a) dest and source boundary cross at the same time
     *     (b) dest boundary crossed first
     *     (c) source boundary crossed first
     * NOTE: for scenarios (2), (3) and (4) we don't check for overlap
     *     but just perform the copy we would for overlap.  Testing
     *     shows that less than 4% of the move_chars calls uses one of
     *     these scenarios.  Over 95% of the calls are scenario (1a).
     */

#ifdef FEATURE_INTERVAL_TIMER
    if (addr1 == 80 || addr2 == 80)
    {
        obtain_lock (&sysblk.todlock);
        /* If a program check occurs during address translation
         * (when a boundary is crossed), then program_interrupt
         * must release the todlock for us.
         */
        regs->todlock = 1;
        update_TOD_clock ();
    }
#endif /* FEATURE_INTERVAL_TIMER */

    if ( (addr1 & 0x7FF) <= 0x7FF - len )
    {
        if ( (addr2 & 0x7FF) <= 0x7FF - len )
        {
             /* (1) - No boundaries are crossed */
             if ( (dest1 < source1 && dest1 + len < source1)
               || (source1 < dest1 && source1 + len < dest1) )
             {
                 /* (1a) - No overlap */
                 MEMCPY (dest1, source1, len + 1);
             }
             else
             {
                 /* (1b) - Overlap */
                 for ( i = 0; i <= len; i++) *dest1++ = *source1++;
             }
        }
        else
        {
             /* (2) - Second operand crosses a boundary */
             len2 = 0x800 - (addr2 & 0x7FF);
             source2 = LOGICAL_TO_ABS ((addr2 + len2) & ADDRESS_MAXWRAP(regs),
                                    arn2, regs, ACCTYPE_READ, key2)
                     + regs->mainstor;

             for ( i = 0; i < len2; i++) *dest1++ = *source1++;
             len2 = len - len2;
             for ( i = 0; i <= len2; i++) *dest1++ = *source2++;
        }
        STORAGE_KEY(abs1, regs) |= (STORKEY_REF | STORKEY_CHANGE);
    }
    else
    {
        /* First operand crosses a boundary */
        len2 = 0x800 - (addr1 & 0x7FF);
        abs2 = LOGICAL_TO_ABS ((addr1 + len2) & ADDRESS_MAXWRAP(regs),
                            arn1, regs, ACCTYPE_WRITE_SKP, key1);
        dest2 = abs2 + regs->mainstor;

        if ( (addr2 & 0x7FF) <= 0x7FF - len )
        {
             /* (3) - First operand crosses a boundary */
             for ( i = 0; i < len2; i++) *dest1++ = *source1++;
             len2 = len - len2;
             for ( i = 0; i <= len2; i++) *dest2++ = *source1++;
        }
        else
        {
            /* (4) - Both operands cross a boundary */
            len3 = 0x800 - (addr2 & 0x7FF);
            source2 = LOGICAL_TO_ABS ((addr2 + len3) & ADDRESS_MAXWRAP(regs),
                                    arn2, regs, ACCTYPE_READ, key2)
                    + regs->mainstor;
            if (len2 == len3)
            {
                /* (4a) - Both operands cross at the same time */
                for ( i = 0; i < len2; i++) *dest1++ = *source1++;
                len2 = len - len2;
                for ( i = 0; i <= len2; i++) *dest2++ = *source2++;
            }
            else if (len2 < len3)
            {
                /* (4b) - First operand crosses first */
                for ( i = 0; i < len2; i++) *dest1++ = *source1++;
                len2 = len3 - len2;
                for ( i = 0; i < len2; i++) *dest2++ = *source1++;
                len2 = len - len3;
                for ( i = 0; i <= len2; i++) *dest2++ = *source2++;
            }
            else
            {
                /* (4c) - Second operand crosses first */
                for ( i = 0; i < len3; i++) *dest1++ = *source1++;
                len3 = len2 - len3;
                for ( i = 0; i < len3; i++) *dest1++ = *source2++;
                len3 = len - len2;
                for ( i = 0; i <= len3; i++) *dest2++ = *source2++;
            }
        }
        STORAGE_KEY(abs1, regs) |= (STORKEY_REF | STORKEY_CHANGE);
        STORAGE_KEY(abs2, regs) |= (STORKEY_REF | STORKEY_CHANGE);
    }

#ifdef FEATURE_INTERVAL_TIMER
    if (addr1 == 80 || addr2 == 80)
    {
        update_TOD_clock ();
        regs->todlock = 0;
        release_lock (&sysblk.todlock);
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

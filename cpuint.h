/**********************************************************************
 Interrupts_State & Interrupts_Mask bits definition (Initial_Mask=800E)
 Machine check, PER and external interrupt subclass bit positions
 are fixed by the architecture and cannot be changed
 Floating interrupts are made pending to all CPUs, and are 
 recorded in the sysblk structure, CPU specific interrupts
 are recorded in the regs structure.
 hi0m mmmm pppp p000 xxxx xxx0 xxxx hhhs : type U32
 || | |||| |||| |--- |||| |||- |||| ||||   h:mask  is always '1'
 || | |||| |||| |    |||| |||  |||| ||||   s:state is always '1'
 || | |||| |||| |    |||| |||  |||| |||+--> '1' : PSW_WAIT
 || | |||| |||| |    |||| |||  |||| ||+---> '1' : RESTART
 || | |||| |||| |    |||| |||  |||| |+----> '1' : BROADCAST
 || | |||| |||| |    |||| |||  |||| +-----> '1' : STORSTAT
 || | |||| |||| |    |||| |||  ||||
 || | |||| |||| |    |||| |||  |||+-------> '1' : ETR
 || | |||| |||| |    |||| |||  ||+--------> '1' : EXTSIG
 || | |||| |||| |    |||| |||  |+---------> '1' : INTKEY
 || | |||| |||| |    |||| |||  +----------> '1' : ITIMER
 || | |||| |||| |    |||| |||
 || | |||| |||| |    |||| |||
 || | |||| |||| |    |||| ||+-------------> '1' : SERVSIG
 || | |||| |||| |    |||| |+--------------> '1' : PTIMER
 || | |||| |||| |    |||| +---------------> '1' : CLKC
 || | |||| |||| |    |||| 
 || | |||| |||| |    |||+-----------------> '1' : TODSYNC
 || | |||| |||| |    ||+------------------> '1' : EXTCALL
 || | |||| |||| |    |+-------------------> '1' : EMERSIG
 || | |||| |||| |    +--------------------> '1' : MALFALT
 || | |||| |||| |
 || | |||| |||| |    
 || | |||| |||| |    
 || | |||| |||| +-------------------------> '1' : PER STURA
 || | |||| |||| 
 || | |||| |||+---------------------------> '1' : PER GRA
 || | |||| ||+----------------------------> '1' : PER SA
 || | |||| |+-----------------------------> '1' : PER IF
 || | |||| +------------------------------> '1' : PER SB
 || | |||| 
 || | |||+--------------------------------> '1' : WARNING
 || | ||+---------------------------------> '1' : XDMGRPT
 || | |+----------------------------------> '1' : DGRDRPT
 || | +-----------------------------------> '1' : RCVYRPT
 || |
 || +-------------------------------------> '1' : CHANRPT
 ||
 |+---------------------------------------> '1' : IO
 +----------------------------------------> '1' : INTERRUPT possible
**********************************************************************/

/* Interrupt bit numbers */
#define IC_INTERRUPT       31 /* 0x80000000 */
#define IC_IO              30 /* 0x40000000 */
#define IC_UNUSED_29       29 /* 0x20000000 */
#define IC_CHANRPT         28 /* 0x10000000 - Architecture dependent (CR14) */
#define IC_RCVYRPT         27 /* 0x08000000 - Architecture dependent (CR14) */
#define IC_DGRDRPT         26 /* 0x04000000 - Architecture dependent (CR14) */
#define IC_XDMGRPT         25 /* 0x02000000 - Architecture dependent (CR14) */
#define IC_WARNING         24 /* 0x01000000 - Architecture dependent (CR14) */
#define IC_PER_SB          23 /* 0x00800000 - Architecture dependent (CR9 >> 8) */
#define IC_PER_IF          22 /* 0x00400000 - Architecture dependent (CR9 >> 8) */
#define IC_PER_SA          21 /* 0x00200000 - Architecture dependent (CR9 >> 8) */
#define IC_PER_GRA         20 /* 0x00100000 - Architecture dependent (CR9 >> 8) */
#define IC_PER_STURA       19 /* 0x00080000 - Architecture dependent (CR9 >> 8) */
#define IC_UNUSED_18       18 /* 0x00040000 */
#define IC_UNUSED_17       17 /* 0x00020000 */
#define IC_UNUSED_16       16 /* 0x00010000 */
#define IC_MALFALT         15 /* 0x00008000 - Architecture dependent (CR0) */
#define IC_EMERSIG         14 /* 0x00004000 - Architecture dependent (CR0) */
#define IC_EXTCALL         13 /* 0x00002000 - Architecture dependent (CR0) */
#define IC_TODSYNC         12 /* 0x00001000 - Architecture dependent (CR0) */
#define IC_CLKC            11 /* 0x00000800 - Architecture dependent (CR0) */
#define IC_PTIMER          10 /* 0x00000400 - Architecture dependent (CR0) */
#define IC_SERVSIG          9 /* 0x00000200 - Architecture dependent (CR0) */
#define IC_UNUSED_8         8 /* 0x00000100 */
#define IC_ITIMER           7 /* 0x00000080 - Architecture dependent (CR0) */
#define IC_INTKEY           6 /* 0x00000040 - Architecture dependent (CR0) */
#define IC_EXTSIG           5 /* 0x00000020 - Architecture dependent (CR0) */
#define IC_ETR              4 /* 0x00000010 - Architecture dependent (CR0) */
#define IC_STORSTAT         3 /* 0x00000008 */
#define IC_BROADCAST        2 /* 0x00000004 */
#define IC_RESTART          1 /* 0x00000002 */
#define IC_PSW_WAIT         0 /* 0x00000001 */

/* Initial values */
#define IC_INITIAL_STATE   BIT(IC_PSW_WAIT)
#define IC_INITIAL_MASK  ( BIT(IC_INTERRUPT) \
                         | BIT(IC_RESTART) \
                         | BIT(IC_BROADCAST) \
                         | BIT(IC_STORSTAT) \
                         )

#define SET_IC_INITIAL_MASK(_regs)  (_regs)->ints_mask = IC_INITIAL_MASK
#define SET_IC_INITIAL_STATE(_regs) (_regs)->ints_state = IC_INITIAL_STATE

/* I/O interrupt subclasses */
#define IC_IOPENDING     ( BIT(IC_IO) )

/* External interrupt subclasses */
#define IC_EXTPENDING    ( BIT(IC_MALFALT) \
                         | BIT(IC_EMERSIG) \
                         | BIT(IC_EXTCALL) \
                         | BIT(IC_TODSYNC) \
                         | BIT(IC_CLKC) \
                         | BIT(IC_PTIMER) \
                         | BIT(IC_SERVSIG) \
                         | BIT(IC_ITIMER) \
                         | BIT(IC_INTKEY) \
                         | BIT(IC_EXTSIG) \
                         | BIT(IC_ETR) \
                         )

/* Machine check subclasses */
#define IC_MCKPENDING    ( BIT(IC_CHANRPT) \
                         | BIT(IC_RCVYRPT) \
                         | BIT(IC_DGRDRPT) \
                         | BIT(IC_XDMGRPT) \
                         | BIT(IC_WARNING) \
                         )

/* Not disabled mask */
#define IC_OPEN_MASK     ( IC_MCKPENDING \
                         | IC_EXTPENDING \
                         | IC_IOPENDING \
                         )

#define IC_PER_MASK      ( BIT(IC_PER_SB) \
                         | BIT(IC_PER_IF) \
                         | BIT(IC_PER_SA) \
                         | BIT(IC_PER_GRA) \
                         | BIT(IC_PER_STURA) \
                         )

/* SIE & Assist supported events */
#define IC_SIE_INT       ( BIT(IC_BROADCAST) \
                         | BIT(IC_IO) \
                         | BIT(IC_CLKC) \
                         | BIT(IC_PTIMER) \
                         | BIT(IC_ITIMER) \
                         )

#define IC_CR9_SHIFT 8

/* Mask bits are ONLY set by the associated cpu thread and
 * therefore don't have to be serialized.  The mask bits
 * indicate which interrupts the CPU is willing to take at
 * this time.  We set the mask bits below in one fell swoop
 * to avoid multiple updates to `ints_mask'.
 */

#define IC_MASK(_regs) \
 ( ( IC_INITIAL_MASK ) \
 | ( ECMODE(&(_regs)->psw) \
     ? ( ((_regs)->psw.sysmask & PSW_IOMASK) ? BIT(IC_IO) : 0 ) \
     : ( ((_regs)->psw.sysmask & 0xFE) ? BIT(IC_IO) : 0 ) \
   ) \
 | ( MACHMASK(&(_regs)->psw) ? ((_regs)->CR(14) & IC_MCKPENDING) : 0 ) \
 | ( PER_MODE(_regs) ? (((_regs)->CR(9) >> IC_CR9_SHIFT) & IC_PER_MASK) : 0 ) \
 | ( ((_regs)->psw.sysmask & PSW_EXTMASK) ? ((_regs)->CR(0) & IC_EXTPENDING) : 0 ) \
 | ( WAITSTATE(&(_regs)->psw) ? BIT(IC_PSW_WAIT) : 0 ) \
 )

#define IC_ECMODE_MASK(_regs) \
 ( ( IC_INITIAL_MASK ) \
 | ( ((_regs)->psw.sysmask & PSW_IOMASK) ? BIT(IC_IO) : 0 ) \
 | ( MACHMASK(&(_regs)->psw) ? ((_regs)->CR(14) & IC_MCKPENDING) : 0 ) \
 | ( PER_MODE(_regs) ? (((_regs)->CR(9) >> IC_CR9_SHIFT) & IC_PER_MASK) : 0 ) \
 | ( ((_regs)->psw.sysmask & PSW_EXTMASK) ? ((_regs)->CR(0) & IC_EXTPENDING) : 0 ) \
 | ( WAITSTATE(&(_regs)->psw) ? BIT(IC_PSW_WAIT) : 0 ) \
 )

#define IC_BCMODE_MASK(_regs) \
 ( ( IC_INITIAL_MASK ) \
 | ( ((_regs)->psw.sysmask & 0xFE) ? BIT(IC_IO) : 0 ) \
 | ( MACHMASK(&(_regs)->psw) ? ((_regs)->CR(14) & IC_MCKPENDING) : 0 ) \
 | ( PER_MODE(_regs) ? (((_regs)->CR(9) >> IC_CR9_SHIFT) & IC_PER_MASK) : 0 ) \
 | ( ((_regs)->psw.sysmask & PSW_EXTMASK) ? ((_regs)->CR(0) & IC_EXTPENDING) : 0 ) \
 | ( WAITSTATE(&(_regs)->psw) ? BIT(IC_PSW_WAIT) : 0 ) \
 )

#define SET_IC_ECMODE_MASK(_regs) \
 do { \
   (_regs)->ints_mask = IC_ECMODE_MASK((_regs)); \
   (_regs)->permode = ((IC_PER_MASK & (_regs)->ints_mask) != 0); \
 } while (0)

#define SET_IC_BCMODE_MASK(_regs) \
 do { \
   (_regs)->ints_mask = IC_BCMODE_MASK((_regs)); \
   (_regs)->permode = ((IC_PER_MASK & (_regs)->ints_mask) != 0); \
 } while (0)

#undef SET_IC_MASK
#ifdef FEATURE_BCMODE
 #define SET_IC_MASK(_regs) \
  do { \
    (_regs)->ints_mask = IC_MASK((_regs)); \
    (_regs)->permode = ((IC_PER_MASK & (_regs)->ints_mask) != 0); \
  } while (0)
#else
 #define SET_IC_MASK(_regs) SET_IC_ECMODE_MASK(_regs)
#endif

/* State bits indicate what interrupts are possibly pending
 * for a CPU.  These bits can be set by any thread and therefore
 * are serialized by the `intlock'.  An exception is for PER
 * events.  Updates then use macros that provide serialization
 * on supported architectures.
 */

#define SET_IC_TRACE \
 do { \
   int i = 0; \
   int tracing = (sysblk.instbreak || sysblk.inststep || sysblk.insttrace); \
   U32 mask = sysblk.started_mask; \
   while (mask) { \
     i += ffs(mask); \
     sysblk.regs[i]->tracing = tracing; \
     set_bit (4, IC_INTERRUPT, &sysblk.regs[i]->ints_state); \
     mask >>= ++i; \
   } \
 } while (0)


  /* * * * * * * * * * * * * *
   * Set state bit to '1'    *
   * * * * * * * * * * * * * */

#define ON_IC_INTERRUPT(_regs) \
 do { \
   set_bit (4, IC_INTERRUPT, &(_regs)->ints_state); \
 } while (0)

#define ON_IC_RESTART(_regs) \
 do { \
   or_bits (4, BIT(IC_RESTART)|BIT(IC_INTERRUPT), &(_regs)->ints_state); \
 } while (0)

#define ON_IC_BROADCAST(_regs) \
 do { \
   or_bits (4, BIT(IC_BROADCAST)|BIT(IC_INTERRUPT), &(_regs)->ints_state); \
 } while (0)

#define ON_IC_STORSTAT(_regs) \
 do { \
   or_bits (4, BIT(IC_STORSTAT)|BIT(IC_INTERRUPT), &(_regs)->ints_state); \
 } while (0)

#define ON_IC_IOPENDING \
 do { \
   int i; U32 mask; \
   if ( !test_bit (4, IC_IO, &sysblk.ints_state) ) { \
     set_bit (4, IC_IO, &sysblk.ints_state); \
     i = 0; \
     mask = sysblk.started_mask; \
     while (mask) { \
       i += ffs(mask); \
       if (test_bit (4, IC_IO, &sysblk.regs[i]->ints_mask)) \
         or_bits (4, BIT(IC_IO)|BIT(IC_INTERRUPT), &sysblk.regs[i]->ints_state); \
       else \
         set_bit (4, IC_IO, &sysblk.regs[i]->ints_state); \
       mask >>= ++i; \
     } \
   } \
 } while (0)

#define ON_IC_CHANRPT \
 do { \
   int i; U32 mask; \
   if ( !test_bit (4, IC_CHANRPT, &sysblk.ints_state) ) { \
     set_bit (4, IC_CHANRPT, &sysblk.ints_state); \
     i = 0; \
     mask = sysblk.started_mask; \
     while (mask) { \
       i += ffs(mask); \
       if (test_bit (4, IC_CHANRPT, &sysblk.regs[i]->ints_mask)) \
         or_bits (4, BIT(IC_CHANRPT)|BIT(IC_INTERRUPT), &sysblk.regs[i]->ints_state); \
       else \
         set_bit (4, IC_CHANRPT, &sysblk.regs[i]->ints_state); \
       mask >>= ++i; \
     } \
   } \
 } while (0)

#define ON_IC_INTKEY \
 do { \
   int i; U32 mask; \
   if ( !test_bit (4, IC_INTKEY, &sysblk.ints_state) ) { \
     set_bit (4, IC_INTKEY, &sysblk.ints_state); \
     i = 0; \
     mask = sysblk.started_mask; \
     while (mask) { \
       i += ffs(mask); \
       if (test_bit (4, IC_INTKEY, &sysblk.regs[i]->ints_mask)) \
         or_bits (4, BIT(IC_INTKEY)|BIT(IC_INTERRUPT), &sysblk.regs[i]->ints_state); \
       else \
         set_bit (4, IC_INTKEY, &sysblk.regs[i]->ints_state); \
       mask >>= ++i; \
     } \
   } \
 } while (0)

#define ON_IC_SERVSIG \
 do { \
   int i; U32 mask; \
   if ( !test_bit (4, IC_SERVSIG, &sysblk.ints_state) ) { \
     set_bit (4, IC_SERVSIG, &sysblk.ints_state); \
     i = 0; \
     mask = sysblk.started_mask; \
     while (mask) { \
       i += ffs(mask); \
       if (test_bit (4, IC_SERVSIG, &sysblk.regs[i]->ints_mask)) \
         or_bits (4, BIT(IC_SERVSIG)|BIT(IC_INTERRUPT), &sysblk.regs[i]->ints_state); \
       else \
         set_bit (4, IC_SERVSIG, &sysblk.regs[i]->ints_state); \
       mask >>= ++i; \
     } \
   } \
 } while (0)

#define ON_IC_ITIMER(_regs) \
 do { \
   if (test_bit (4, IC_ITIMER, &(_regs)->ints_mask)) \
     or_bits (4, BIT(IC_ITIMER)|BIT(IC_INTERRUPT), &(_regs)->ints_state); \
   else \
     set_bit (4, IC_ITIMER, &(_regs)->ints_state); \
 } while (0)

#define ON_IC_PTIMER(_regs) \
 do { \
   if (test_bit (4, IC_PTIMER, &(_regs)->ints_mask)) \
     or_bits (4, BIT(IC_PTIMER)|BIT(IC_INTERRUPT), &(_regs)->ints_state); \
   else \
     set_bit (4, IC_PTIMER, &(_regs)->ints_state); \
 } while (0)

#define ON_IC_CLKC(_regs) \
 do { \
   if (test_bit (4, IC_CLKC, &(_regs)->ints_mask)) \
     or_bits (4, BIT(IC_CLKC)|BIT(IC_INTERRUPT), &(_regs)->ints_state); \
   else \
     set_bit (4, IC_CLKC, &(_regs)->ints_state); \
 } while (0)

#define ON_IC_EXTCALL(_regs) \
 do { \
   if (test_bit (4, IC_EXTCALL, &(_regs)->ints_mask)) \
     or_bits (4, BIT(IC_EXTCALL)|BIT(IC_INTERRUPT), &(_regs)->ints_state); \
   else \
     set_bit (4, IC_EXTCALL, &(_regs)->ints_state); \
 } while (0)

#define ON_IC_MALFALT(_regs) \
 do { \
   if (test_bit (4, IC_MALFALT, &(_regs)->ints_mask)) \
     or_bits (4, BIT(IC_MALFALT)|BIT(IC_INTERRUPT), &(_regs)->ints_state); \
   else \
     set_bit (4, IC_MALFALT, &(_regs)->ints_state); \
 } while (0)

#define ON_IC_EMERSIG(_regs) \
 do { \
   if (test_bit (4, IC_EMERSIG, &(_regs)->ints_mask)) \
     or_bits (4, BIT(IC_EMERSIG)|BIT(IC_INTERRUPT), &(_regs)->ints_state); \
   else \
     set_bit (4, IC_EMERSIG, &(_regs)->ints_state); \
 } while (0)

    /* PER bits are a little different in that we only set the
     * state bit to one if the mask bit is on.  There is no need
     * to set the IC_INTERRUPT bit.
     */

#define ON_IC_PER_SB(_regs) \
 do { \
   if (test_bit (4, IC_PER_SB, &(_regs)->ints_mask)) \
     set_bit (4, IC_PER_SB, &(_regs)->ints_state); \
 } while (0)

#define ON_IC_PER_IF(_regs) \
 do { \
   if (test_bit (4, IC_PER_IF, &(_regs)->ints_mask)) \
     set_bit (4, IC_PER_IF, &(_regs)->ints_state); \
 } while (0)

#define ON_IC_PER_SA(_regs) \
 do { \
   if (test_bit (4, IC_PER_SA, &(_regs)->ints_mask)) \
     set_bit (4, IC_PER_SA, &(_regs)->ints_state); \
 } while (0)

#define ON_IC_PER_GRA(_regs) \
 do { \
   if (test_bit (4, IC_PER_GRA, &(_regs)->ints_mask)) \
     set_bit (4, IC_PER_GRA, &(_regs)->ints_state); \
 } while (0)

#define ON_IC_PER_STURA(_regs) \
 do { \
   if (test_bit (4, IC_PER_STURA, &(_regs)->ints_mask)) \
     set_bit (4, IC_PER_STURA, &(_regs)->ints_state); \
 } while (0)


  /* * * * * * * * * * * * * *
   * Set state bit to '0'    *
   * * * * * * * * * * * * * */

#define OFF_IC_INTERRUPT(_regs) \
 do { \
   clear_bit (4, IC_INTERRUPT, &(_regs)->ints_state); \
 } while (0)

#define OFF_IC_RESTART(_regs) \
 do { \
   clear_bit (4, IC_RESTART, &(_regs)->ints_state); \
 } while (0)

#define OFF_IC_BROADCAST(_regs) \
 do { \
   clear_bit (4, IC_BROADCAST, &(_regs)->ints_state); \
 } while (0)

#define OFF_IC_STORSTAT(_regs) \
 do { \
   clear_bit (4, IC_STORSTAT, &(_regs)->ints_state); \
 } while (0)

#define OFF_IC_IOPENDING \
 do { \
   int i; U32 mask; \
   if ( test_bit (4, IC_IO, &sysblk.ints_state) ) { \
     clear_bit (4, IC_IO, &sysblk.ints_state); \
     i = 0; \
     mask = sysblk.started_mask; \
     while (mask) { \
       i += ffs(mask); \
       clear_bit (4, IC_IO, &sysblk.regs[i]->ints_state); \
       mask >>= ++i; \
     } \
   } \
 } while (0)

#define OFF_IC_CHANRPT \
 do { \
   int i; U32 mask; \
   if ( test_bit (4, IC_CHANRPT, &sysblk.ints_state) ) { \
     clear_bit (4, IC_CHANRPT, &sysblk.ints_state); \
     i = 0; \
     mask = sysblk.started_mask; \
     while (mask) { \
       i += ffs(mask); \
       clear_bit (4, IC_CHANRPT, &sysblk.regs[i]->ints_state); \
       mask >>= ++i; \
     } \
   } \
 } while (0)

#define OFF_IC_INTKEY \
 do { \
   int i; U32 mask; \
   if ( test_bit (4, IC_INTKEY, &sysblk.ints_state) ) { \
     clear_bit (4, IC_INTKEY, &sysblk.ints_state); \
     i = 0; \
     mask = sysblk.started_mask; \
     while (mask) { \
       i += ffs(mask); \
       clear_bit (4, IC_INTKEY, &sysblk.regs[i]->ints_state); \
       mask >>= ++i; \
     } \
   } \
 } while (0)

#define OFF_IC_SERVSIG \
 do { \
   int i; U32 mask; \
   if ( test_bit (4, IC_SERVSIG, &sysblk.ints_state) ) { \
     clear_bit (4, IC_SERVSIG, &sysblk.ints_state); \
     i = 0; \
     mask = sysblk.started_mask; \
     while (mask) { \
       i += ffs(mask); \
       clear_bit (4, IC_SERVSIG, &sysblk.regs[i]->ints_state); \
       mask >>= ++i; \
     } \
   } \
 } while (0)

#define OFF_IC_ITIMER(_regs) \
 do { \
   clear_bit (4, IC_ITIMER, &(_regs)->ints_state); \
 } while (0)

#define OFF_IC_PTIMER(_regs) \
 do { \
   clear_bit (4, IC_PTIMER, &(_regs)->ints_state); \
 } while (0)

#define OFF_IC_CLKC(_regs) \
 do { \
   clear_bit (4, IC_CLKC, &(_regs)->ints_state); \
 } while (0)

#define OFF_IC_EXTCALL(_regs) \
 do { \
   clear_bit (4, IC_EXTCALL, &(_regs)->ints_state); \
 } while (0)

#define OFF_IC_MALFALT(_regs) \
 do { \
   clear_bit (4, IC_MALFALT, &(_regs)->ints_state); \
 } while (0)

#define OFF_IC_EMERSIG(_regs) \
 do { \
   clear_bit (4, IC_EMERSIG, &(_regs)->ints_state); \
 } while (0)

#define OFF_IC_PER(_regs) \
 do { \
   and_bits (4, ~IC_PER_MASK, &(_regs)->ints_state); \
 } while (0)

#define OFF_IC_PER_SB(_regs) \
 do { \
   clear_bit (4, IC_PER_SB, &(_regs)->ints_state); \
 } while (0)

#define OFF_IC_PER_IF(_regs) \
 do { \
   clear_bit (4, IC_PER_IF, &(_regs)->ints_state); \
 } while (0)

#define OFF_IC_PER_SA(_regs) \
 do { \
   clear_bit (4, IC_PER_SA, &(_regs)->ints_state); \
 } while (0)

#define OFF_IC_PER_GRA(_regs) \
 do { \
   clear_bit (4, IC_PER_GRA, &(_regs)->ints_state); \
 } while (0)

#define OFF_IC_PER_STURA(_regs) \
 do { \
   clear_bit (4, IC_PER_STURA, &(_regs)->ints_state); \
 } while (0)


  /* * * * * * * * * * * * * *
   * Test interrupt state    *
   * * * * * * * * * * * * * */

#define IS_IC_INTERRUPT(_regs)  ( test_bit (4, IC_INTERRUPT, &(_regs)->ints_state) )
#define IS_IC_RESTART(_regs)    ( test_bit (4, IC_RESTART,   &(_regs)->ints_state) )
#define IS_IC_BROADCAST(_regs)  ( test_bit (4, IC_BROADCAST, &(_regs)->ints_state) )
#define IS_IC_STORSTAT(_regs)   ( test_bit (4, IC_STORSTAT,  &(_regs)->ints_state) )
#define IS_IC_IOPENDING         ( test_bit (4, IC_IO,          &sysblk.ints_state) )
#define IS_IC_MCKPENDING(_regs) (          (   IC_MCKPENDING &(_regs)->ints_state) )
#define IS_IC_CHANRPT           ( test_bit (4, IC_CHANRPT,     &sysblk.ints_state) )
#define IS_IC_INTKEY            ( test_bit (4, IC_INTKEY,      &sysblk.ints_state) )
#define IS_IC_SERVSIG           ( test_bit (4, IC_SERVSIG,     &sysblk.ints_state) )
#define IS_IC_ITIMER(_regs)     ( test_bit (4, IC_ITIMER,    &(_regs)->ints_state) )
#define IS_IC_PTIMER(_regs)     ( test_bit (4, IC_PTIMER,    &(_regs)->ints_state) )
#define IS_IC_CLKC(_regs)       ( test_bit (4, IC_CLKC,      &(_regs)->ints_state) )
#define IS_IC_EXTCALL(_regs)    ( test_bit (4, IC_EXTCALL,   &(_regs)->ints_state) )
#define IS_IC_MALFALT(_regs)    ( test_bit (4, IC_MALFALT,   &(_regs)->ints_state) )
#define IS_IC_EMERSIG(_regs)    ( test_bit (4, IC_EMERSIG,   &(_regs)->ints_state) )
#define IS_IC_PER(_regs)        (          (   IC_PER_MASK &  (_regs)->ints_state) )
#define IS_IC_PER_SB(_regs)     ( test_bit (4, IC_PER_SB,    &(_regs)->ints_state) )
#define IS_IC_PER_IF(_regs)     ( test_bit (4, IC_PER_IF,    &(_regs)->ints_state) )
#define IS_IC_PER_SA(_regs)     ( test_bit (4, IC_PER_SA,    &(_regs)->ints_state) )
#define IS_IC_PER_GRA(_regs)    ( test_bit (4, IC_PER_GRA,   &(_regs)->ints_state) )
#define IS_IC_PER_STURA(_regs)  ( test_bit (4, IC_PER_STURA, &(_regs)->ints_state) )


  /* * * * * * * * * * * * * *
   * Disabled wait check     *
   * * * * * * * * * * * * * */

#define IS_IC_DISABLED_WAIT_PSW(_regs) \
                                ( ((_regs)->ints_mask & IC_OPEN_MASK) == 0 )


  /* * * * * * * * * * * * * *
   * Test PER mask bits      *
   * * * * * * * * * * * * * */

#define EN_IC_PER(_regs)        unlikely( (_regs)->permode )
#define EN_IC_PER_SB(_regs)     (EN_IC_PER(_regs) && test_bit(4, IC_PER_SB,    &(_regs)->ints_mask))
#define EN_IC_PER_IF(_regs)     (EN_IC_PER(_regs) && test_bit(4, IC_PER_IF,    &(_regs)->ints_mask))
#define EN_IC_PER_SA(_regs)     (EN_IC_PER(_regs) && test_bit(4, IC_PER_SA,    &(_regs)->ints_mask))
#define EN_IC_PER_GRA(_regs)    (EN_IC_PER(_regs) && test_bit(4, IC_PER_GRA,   &(_regs)->ints_mask))
#define EN_IC_PER_STURA(_regs)  (EN_IC_PER(_regs) && test_bit(4, IC_PER_STURA, &(_regs)->ints_mask))


  /* * * * * * * * * * * * * * * * * * * * * * * * *
   * Check for specific enabled pending interrupt  *
   * * * * * * * * * * * * * * * * * * * * * * * * */

#define OPEN_IC_MCKPENDING(_regs) \
                        ( (_regs)->ints_state & (_regs)->ints_mask & IC_MCKPENDING )

#define OPEN_IC_IOPENDING(_regs) \
                        ( (_regs)->ints_state & (_regs)->ints_mask & IC_IOPENDING )

#define OPEN_IC_CHANRPT(_regs) \
                        ( (_regs)->ints_state & (_regs)->ints_mask & BIT(IC_CHANRPT) )

#define OPEN_IC_EXTPENDING(_regs) \
                        ( (_regs)->ints_state & (_regs)->ints_mask & IC_EXTPENDING )

#define OPEN_IC_ITIMER(_regs) \
                        ( (_regs)->ints_state & (_regs)->ints_mask & BIT(IC_ITIMER) )

#define OPEN_IC_PTIMER(_regs) \
                        ( (_regs)->ints_state & (_regs)->ints_mask & BIT(IC_PTIMER) )

#define OPEN_IC_CLKC(_regs) \
                        ( (_regs)->ints_state & (_regs)->ints_mask & BIT(IC_CLKC) )

#define OPEN_IC_INTKEY(_regs) \
                        ( (_regs)->ints_state & (_regs)->ints_mask & BIT(IC_INTKEY) )

#define OPEN_IC_SERVSIG(_regs) \
                        ( (_regs)->ints_state & (_regs)->ints_mask & BIT(IC_SERVSIG) )

#define OPEN_IC_EXTCALL(_regs) \
                        ( (_regs)->ints_state & (_regs)->ints_mask & BIT(IC_EXTCALL) )

#define OPEN_IC_MALFALT(_regs) \
                        ( (_regs)->ints_state & (_regs)->ints_mask & BIT(IC_MALFALT) )

#define OPEN_IC_EMERSIG(_regs) \
                        ( (_regs)->ints_state & (_regs)->ints_mask & BIT(IC_EMERSIG) )

#define OPEN_IC_PERINT(_regs) \
                        ( (_regs)->ints_state                      & IC_PER_MASK)


  /* * * * * * * * * * * * * * * * * * * * * * * * *
   * Check for general enabled pending interrupt   *
   * * * * * * * * * * * * * * * * * * * * * * * * */

#define IC_INTERRUPT_CPU(_regs) \
   ( (_regs)->ints_state & ((_regs)->ints_mask | IC_PER_MASK) )

#define IC_INTERRUPT_CPU_NO_PER(_regs) \
   ( (_regs)->ints_state & (_regs)->ints_mask )

#define SIE_IC_INTERRUPT_CPU(_regs) \
   (((_regs)->ints_state|((_regs)->hostregs->ints_state&IC_SIE_INT)) & ((_regs)->ints_mask|IC_PER_MASK))

/**********************************************************************
 Interrupts_State & Interrupts_Mask bits definition (Initial_Mask=C00E)
 Machine check, PER and external interrupt subclass bit positions
 are fixed by the architecture and cannot be changed
 Floating interrupts are made pending to all CPUs, and are 
 recorded in the sysblk structure, CPU specific interrupts
 are recorded in the regs structure.
 hhim mmmm pppp p000 xxxx xxx0 xxxx hhhs : type U32
 |||| |||| |||| |--- |||| |||- |||| ||||   h:mask  is always '1'
 |||| |||| |||| |    |||| |||  |||| ||||   s:state is always '1'
 |||| |||| |||| |    |||| |||  |||| |||+--> '1' : PSW_WAIT
 |||| |||| |||| |    |||| |||  |||| ||+---> '1' : RESTART
 |||| |||| |||| |    |||| |||  |||| |+----> '1' : BROADCAST
 |||| |||| |||| |    |||| |||  |||| +-----> '1' : STORSTAT
 |||| |||| |||| |    |||| |||  ||||
 |||| |||| |||| |    |||| |||  |||+-------> '1' : ETR
 |||| |||| |||| |    |||| |||  ||+--------> '1' : EXTSIG
 |||| |||| |||| |    |||| |||  |+---------> '1' : INTKEY
 |||| |||| |||| |    |||| |||  +----------> '1' : ITIMER
 |||| |||| |||| |    |||| |||
 |||| |||| |||| |    |||| |||
 |||| |||| |||| |    |||| ||+-------------> '1' : SERVSIG
 |||| |||| |||| |    |||| |+--------------> '1' : PTIMER
 |||| |||| |||| |    |||| +---------------> '1' : CLKC
 |||| |||| |||| |    |||| 
 |||| |||| |||| |    |||+-----------------> '1' : TODSYNC
 |||| |||| |||| |    ||+------------------> '1' : EXTCALL
 |||| |||| |||| |    |+-------------------> '1' : EMERSIG
 |||| |||| |||| |    +--------------------> '1' : MALFALT
 |||| |||| |||| |
 |||| |||| |||| |    
 |||| |||| |||| |    
 |||| |||| |||| +-------------------------> '1' : PER STURA
 |||| |||| |||| 
 |||| |||| |||+---------------------------> '1' : PER GRA
 |||| |||| ||+----------------------------> '1' : PER SA
 |||| |||| |+-----------------------------> '1' : PER IF
 |||| |||| +------------------------------> '1' : PER SB
 |||| |||| 
 |||| |||+--------------------------------> '1' : WARNING
 |||| ||+---------------------------------> '1' : XDMGRPT
 |||| |+----------------------------------> '1' : DGRDRPT
 |||| +-----------------------------------> '1' : RCVYRPT
 ||||
 |||+-------------------------------------> '1' : CHANRPT
 ||+--------------------------------------> '1' : IOPENDING
 |+---------------------------------------> '1' : DEBUG or TRACE
 +----------------------------------------> '1' : INTERRUPT possible
**********************************************************************/

/* Initial values */
#define IC_INITIAL_STATE   IC_PSW_WAIT
#define IC_INITIAL_MASK  ( IC_INTERRUPT \
                         | IC_DEBUG_BIT \
                         | IC_RESTART \
                         | IC_BROADCAST \
                         | IC_STORSTAT )

#define SET_IC_INITIAL_MASK(_regs)  (_regs)->ints_mask = IC_INITIAL_MASK
#define SET_IC_INITIAL_STATE         sysblk.ints_state = IC_INITIAL_STATE

/* Hercules related or nonmaskable interrupts */
#define IC_INTERRUPT       0x80000000
#define IC_DEBUG_BIT       0x40000000
#define IC_IOPENDING       0x20000000
#define IC_STORSTAT        0x00000008
#define IC_BROADCAST       0x00000004
#define IC_RESTART         0x00000002
#define IC_PSW_WAIT        0x00000001

/* External interrupt subclasses */
#define IC_EXTPENDING    ( CR0_XM_MALFALT \
                         | CR0_XM_EMERSIG \
                         | CR0_XM_EXTCALL \
                         | CR0_XM_TODSYNC \
                         | CR0_XM_CLKC \
                         | CR0_XM_PTIMER \
                         | CR0_XM_SERVSIG \
                         | CR0_XM_ITIMER \
                         | CR0_XM_INTKEY \
                         | CR0_XM_EXTSIG \
                         | CR0_XM_ETR )

/* Machine check subclasses */
#define IC_MCKPENDING    ( CR14_CHANRPT \
                         | CR14_RCVYRPT \
                         | CR14_DGRDRPT \
                         | CR14_XDMGRPT \
                         | CR14_WARNING )

/* Not disabled mask */
#define IC_OPEN_MASK     ( IC_MCKPENDING \
                         | IC_EXTPENDING \
                         | IC_IOPENDING )

#define IC_CR9_SHIFT     8
#define IC_PER_SB        (CR9_SB >> IC_CR9_SHIFT)
#define IC_PER_IF        (CR9_IF >> IC_CR9_SHIFT)
#define IC_PER_SA        (CR9_SA >> IC_CR9_SHIFT)
#define IC_PER_GRA       (CR9_GRA >> IC_CR9_SHIFT)
#define IC_PER_STURA     (CR9_STURA >> IC_CR9_SHIFT)

#define IC_PER_MASK      ( IC_PER_SB \
                         | IC_PER_IF \
                         | IC_PER_SA \
                         | IC_PER_GRA \
                         | IC_PER_STURA )

/* SIE & Assist supported events */
#define IC_SIE_INT       ( IC_BROADCAST \
                         | IC_IOPENDING \
                         | CR0_XM_CLKC \
                         | CR0_XM_PTIMER \
                         | CR0_XM_ITIMER )

/* Conditionally turn bits on or off */
#define SET_IC_PER_MASK(_regs) \
do { \
  (_regs)->ints_mask &= ~IC_PER_MASK; \
  if( PER_MODE((_regs)) ) \
    (_regs)->ints_mask |= ((_regs)->CR(9) >> IC_CR9_SHIFT) & IC_PER_MASK; \
} while (0)

#define SET_IC_EXTERNAL_MASK(_regs) \
do { \
  (_regs)->ints_mask &= ~IC_EXTPENDING; \
  if( (_regs)->psw.sysmask & PSW_EXTMASK ) \
    (_regs)->ints_mask |= (_regs)->CR(0) & IC_EXTPENDING; \
} while (0)

#undef SET_IC_IO_MASK
#ifdef FEATURE_BCMODE
#define SET_IC_IO_MASK(_regs) \
do { \
  (_regs)->ints_mask &= ~IC_IOPENDING; \
  if( ((_regs)->psw.ecmode ? ((_regs)->psw.sysmask&PSW_IOMASK) : \
                             ((_regs)->psw.sysmask&0xFE)) ) \
    (_regs)->ints_mask |= IC_IOPENDING; \
} while (0)
#else /*!FEATURE_BCMODE*/
  #define SET_IC_IO_MASK(_regs) SET_IC_ECIO_MASK(_regs)
#endif /*FEATURE_BCMODE*/

#define SET_IC_ECIO_MASK(_regs) \
do { \
  (_regs)->ints_mask &= ~IC_IOPENDING; \
  if( (_regs)->psw.sysmask & PSW_IOMASK ) \
    (_regs)->ints_mask |= IC_IOPENDING; \
} while (0)

#define SET_IC_BCIO_MASK(_regs) \
do { \
  (_regs)->ints_mask &= ~IC_IOPENDING; \
  if( (_regs)->psw.sysmask & 0xFE ) \
    (_regs)->ints_mask |= IC_IOPENDING; \
} while (0)

#define SET_IC_MCK_MASK(_regs) \
do { \
  (_regs)->ints_mask &= ~IC_MCKPENDING; \
  if( (_regs)->psw.mach ) \
    (_regs)->ints_mask |= (_regs)->CR(14) & IC_MCKPENDING; \
} while (0)

#define SET_IC_PSW_WAIT_MASK(_regs) \
do { \
  (_regs)->ints_mask &= ~IC_PSW_WAIT; \
  if((_regs)->psw.wait) \
    (_regs)->ints_mask |= IC_PSW_WAIT; \
} while (0)

#define SET_IC_TRACE \
do { \
  if(sysblk.instbreak || sysblk.inststep || sysblk.insttrace) \
    ON_IC_TRACE; \
  else \
    and_bits(&sysblk.ints_state,~IC_DEBUG_BIT); \
} while (0)

#define OFF_IC_CPUINT(_regs)
#define RESET_IC_CPUINT(_regs)

/* Set state bit to '1' */
#define ON_IC_INTERRUPT(_regs)  or_bits( &(_regs)->ints_state, IC_INTERRUPT)
#define ON_IC_RESTART(_regs)    or_bits( &(_regs)->ints_state, IC_RESTART|IC_INTERRUPT)
#define ON_IC_BROADCAST(_regs)  or_bits( &(_regs)->ints_state, IC_BROADCAST|IC_INTERRUPT)
#define ON_IC_STORSTAT(_regs)   or_bits( &(_regs)->ints_state, IC_STORSTAT|IC_INTERRUPT)
#define ON_IC_IOPENDING         do { \
                                 int i; \
                                 or_bits(   &sysblk.ints_state, IC_IOPENDING); \
                                 for (i = 0; i < HI_CPU; i++) \
                                  if (IS_CPU_ONLINE(i) \
                                   && sysblk.regs[i]->ints_mask & IC_IOPENDING) \
                                   or_bits(  &sysblk.regs[i]->ints_state, IC_INTERRUPT); \
                                } while (0)
#define ON_IC_CHANRPT           do { \
                                 int i; \
                                 or_bits(   &sysblk.ints_state, CR14_CHANRPT); \
                                 for (i = 0; i < HI_CPU; i++) \
                                  if (IS_CPU_ONLINE(i) \
                                   && sysblk.regs[i]->ints_mask & CR14_CHANRPT) \
                                   or_bits(  &sysblk.regs[i]->ints_state, IC_INTERRUPT); \
                                } while (0)
#define ON_IC_INTKEY            do { \
                                 int i; \
                                 or_bits(   &sysblk.ints_state, CR0_XM_INTKEY); \
                                 for (i = 0; i < HI_CPU; i++) \
                                  if (IS_CPU_ONLINE(i) \
                                   && sysblk.regs[i]->ints_mask & CR0_XM_INTKEY) \
                                   or_bits(  &sysblk.regs[i]->ints_state, IC_INTERRUPT); \
                                } while (0)
#define ON_IC_SERVSIG           do { \
                                 int i; \
                                 or_bits(   &sysblk.ints_state, CR0_XM_SERVSIG); \
                                 for (i = 0; i < HI_CPU; i++) \
                                  if (IS_CPU_ONLINE(i) \
                                   && sysblk.regs[i]->ints_mask & CR0_XM_SERVSIG) \
                                   or_bits(  &sysblk.regs[i]->ints_state, IC_INTERRUPT); \
                                } while (0)
#define ON_IC_ITIMER(_regs)     do { \
                                  U32 state = CR0_XM_ITIMER; \
                                  if ((_regs)->ints_mask & CR0_XM_ITIMER) \
                                    state |= IC_INTERRUPT; \
                                  or_bits( &(_regs)->ints_state, state); \
                                } while (0)
#define ON_IC_PTIMER(_regs)     do { \
                                  U32 state = CR0_XM_PTIMER; \
                                  if ((_regs)->ints_mask & CR0_XM_PTIMER) \
                                    state |= IC_INTERRUPT; \
                                  or_bits( &(_regs)->ints_state, state); \
                                } while (0)
#define ON_IC_CLKC(_regs)       do { \
                                  U32 state = CR0_XM_CLKC; \
                                  if ((_regs)->ints_mask & CR0_XM_CLKC) \
                                    state |= IC_INTERRUPT; \
                                  or_bits( &(_regs)->ints_state, state); \
                                } while (0)
#define ON_IC_EXTCALL(_regs)    do { \
                                  U32 state = CR0_XM_EXTCALL; \
                                  if ((_regs)->ints_mask & CR0_XM_EXTCALL) \
                                    state |= IC_INTERRUPT; \
                                  or_bits( &(_regs)->ints_state, state); \
                                } while (0)
#define ON_IC_MALFALT(_regs)    do { \
                                  U32 state = CR0_XM_MALFALT; \
                                  if ((_regs)->ints_mask & CR0_XM_MALFALT) \
                                    state |= IC_INTERRUPT; \
                                  or_bits( &(_regs)->ints_state, state); \
                                } while (0)
#define ON_IC_EMERSIG(_regs)    do { \
                                  U32 state = CR0_XM_EMERSIG; \
                                  if ((_regs)->ints_mask & CR0_XM_EMERSIG) \
                                    state |= IC_INTERRUPT; \
                                  or_bits( &(_regs)->ints_state, state); \
                                } while (0)
#define ON_IC_TRACE             do { \
                                 int i; \
                                 or_bits(   &sysblk.ints_state, IC_DEBUG_BIT); \
                                 for (i = 0; i < HI_CPU; i++) \
                                  if (IS_CPU_ONLINE(i)) \
                                   or_bits(  &sysblk.regs[i]->ints_state, IC_INTERRUPT); \
                                } while (0)
#define ON_IC_DEBUG(_regs)      or_bits( &(_regs)->ints_state, IC_DEBUG_BIT)
#define ON_IC_PER_SB(_regs)     or_bits( &(_regs)->ints_state, IC_PER_SB&(_regs)->ints_mask)
#define ON_IC_PER_IF(_regs)     or_bits( &(_regs)->ints_state, IC_PER_IF&(_regs)->ints_mask)
#define ON_IC_PER_SA(_regs)     or_bits( &(_regs)->ints_state, IC_PER_SA&(_regs)->ints_mask)
#define ON_IC_PER_GRA(_regs)    or_bits( &(_regs)->ints_state, IC_PER_GRA&(_regs)->ints_mask)
#define ON_IC_PER_STURA(_regs)  or_bits( &(_regs)->ints_state, IC_PER_STURA&(_regs)->ints_mask)

/* Set state bit to '0' */
#define OFF_IC_INTERRUPT(_regs) and_bits(&(_regs)->ints_state,~IC_INTERRUPT)
#define OFF_IC_RESTART(_regs)   and_bits(&(_regs)->ints_state,~IC_RESTART)
#define OFF_IC_BROADCAST(_regs) and_bits(&(_regs)->ints_state,~IC_BROADCAST)
#define OFF_IC_STORSTAT(_regs)  and_bits(&(_regs)->ints_state,~IC_STORSTAT)
#define OFF_IC_IOPENDING        and_bits(  &sysblk.ints_state,~IC_IOPENDING)
#define OFF_IC_CHANRPT          and_bits(  &sysblk.ints_state,~CR14_CHANRPT)
#define OFF_IC_INTKEY           and_bits(  &sysblk.ints_state,~CR0_XM_INTKEY)
#define OFF_IC_SERVSIG          and_bits(  &sysblk.ints_state,~CR0_XM_SERVSIG)
#define OFF_IC_ITIMER(_regs)    and_bits(&(_regs)->ints_state,~CR0_XM_ITIMER)
#define OFF_IC_PTIMER(_regs)    and_bits(&(_regs)->ints_state,~CR0_XM_PTIMER)
#define OFF_IC_CLKC(_regs)      and_bits(&(_regs)->ints_state,~CR0_XM_CLKC)
#define OFF_IC_EXTCALL(_regs)   and_bits(&(_regs)->ints_state,~CR0_XM_EXTCALL)
#define OFF_IC_MALFALT(_regs)   and_bits(&(_regs)->ints_state,~CR0_XM_MALFALT)
#define OFF_IC_EMERSIG(_regs)   and_bits(&(_regs)->ints_state,~CR0_XM_EMERSIG)
#define OFF_IC_TRACE            and_bits(  &sysblk.ints_state,~IC_DEBUG_BIT)
#define OFF_IC_DEBUG(_regs)     and_bits(&(_regs)->ints_state,~IC_DEBUG_BIT)
#define OFF_IC_PER(_regs)       and_bits(&(_regs)->ints_state,~IC_PER_MASK)
#define OFF_IC_PER_SB(_regs)    and_bits(&(_regs)->ints_state,~IC_PER_SB)
#define OFF_IC_PER_IF(_regs)    and_bits(&(_regs)->ints_state,~IC_PER_IF)
#define OFF_IC_PER_SA(_regs)    and_bits(&(_regs)->ints_state,~IC_PER_SA)
#define OFF_IC_PER_GRA(_regs)   and_bits(&(_regs)->ints_state,~IC_PER_GRA)
#define OFF_IC_PER_STURA(_regs) and_bits(&(_regs)->ints_state,~IC_PER_STURA)

/* Check Interrupt State */
#define IS_IC_INTERRUPT(_regs)  ((_regs)->ints_state&IC_INTERRUPT)
#define IS_IC_DISABLED_WAIT_PSW(_regs) \
             ( ((_regs)->ints_mask & IC_OPEN_MASK) == 0 )
#define IS_IC_RESTART(_regs)    ((_regs)->ints_state&IC_RESTART)
#define IS_IC_BROADCAST(_regs)  ((_regs)->ints_state&IC_BROADCAST)
#define IS_IC_STORSTAT(_regs)   ((_regs)->ints_state&IC_STORSTAT)
#define IS_IC_IOPENDING           (sysblk.ints_state&IC_IOPENDING)
#define IS_IC_MCKPENDING          (sysblk.ints_state&IC_MCKPENDING)
#define IS_IC_CHANRPT             (sysblk.ints_state&CR14_CHANRPT)
#define IS_IC_EXTPENDING(_regs) \
            (((_regs)->ints_state|sysblk.ints_state)&IC_EXTPENDING )
#define IS_IC_INTKEY              (sysblk.ints_state&CR0_XM_INTKEY)
#define IS_IC_SERVSIG             (sysblk.ints_state&CR0_XM_SERVSIG)
#define IS_IC_ITIMER(_regs)     ((_regs)->ints_state&CR0_XM_ITIMER)
#define IS_IC_PTIMER(_regs)     ((_regs)->ints_state&CR0_XM_PTIMER)
#define IS_IC_CLKC(_regs)       ((_regs)->ints_state&CR0_XM_CLKC)
#define IS_IC_EXTCALL(_regs)    ((_regs)->ints_state&CR0_XM_EXTCALL)
#define IS_IC_MALFALT(_regs)    ((_regs)->ints_state&CR0_XM_MALFALT)
#define IS_IC_EMERSIG(_regs)    ((_regs)->ints_state&CR0_XM_EMERSIG)
#define IS_IC_TRACE               (sysblk.ints_state&IC_DEBUG_BIT)
#define IS_IC_PER(_regs)        ((_regs)->ints_state&IC_PER_MASK)
#define IS_IC_PER_SB(_regs)     ((_regs)->ints_state&IC_PER_SB)
#define IS_IC_PER_IF(_regs)     ((_regs)->ints_state&IC_PER_IF)
#define IS_IC_PER_SA(_regs)     ((_regs)->ints_state&IC_PER_SA)
#define IS_IC_PER_GRA(_regs)    ((_regs)->ints_state&IC_PER_GRA)
#define IS_IC_PER_STURA(_regs)  ((_regs)->ints_state&IC_PER_STURA)

#define EN_IC_PER(_regs)        ((_regs)->ints_mask&IC_PER_MASK)
#define EN_IC_PER_SB(_regs)     ((_regs)->ints_mask&IC_PER_SB)
#define EN_IC_PER_IF(_regs)     ((_regs)->ints_mask&IC_PER_IF)
#define EN_IC_PER_SA(_regs)     ((_regs)->ints_mask&IC_PER_SA)
#define EN_IC_PER_GRA(_regs)    ((_regs)->ints_mask&IC_PER_GRA)
#define EN_IC_PER_STURA(_regs)  ((_regs)->ints_mask&IC_PER_STURA)

/* Advanced checks macros */
#define OPEN_IC_MCKPENDING(_regs) \
              (sysblk.ints_state&(_regs)->ints_mask&IC_MCKPENDING)

#define OPEN_IC_IOPENDING(_regs) \
              (sysblk.ints_state&(_regs)->ints_mask&IC_IOPENDING)

#define OPEN_IC_CHANRPT(_regs) \
              (sysblk.ints_state&(_regs)->ints_mask&CR14_CHANRPT)

#define OPEN_IC_EXTPENDING(_regs) \
             ((sysblk.ints_state|(_regs)->ints_state) \
                                &(_regs)->ints_mask&IC_EXTPENDING)

#define OPEN_IC_ITIMER(_regs) \
            ((_regs)->ints_state&(_regs)->ints_mask&CR0_XM_ITIMER)

#define OPEN_IC_PTIMER(_regs) \
            ((_regs)->ints_state&(_regs)->ints_mask&CR0_XM_PTIMER)

#define OPEN_IC_CLKC(_regs) \
            ((_regs)->ints_state&(_regs)->ints_mask&CR0_XM_CLKC)

#define OPEN_IC_INTKEY(_regs) \
              (sysblk.ints_state&(_regs)->ints_mask&CR0_XM_INTKEY)

#define OPEN_IC_SERVSIG(_regs) \
              (sysblk.ints_state&(_regs)->ints_mask&CR0_XM_SERVSIG)

#define OPEN_IC_EXTCALL(_regs) \
            ((_regs)->ints_state&(_regs)->ints_mask&CR0_XM_EXTCALL)

#define OPEN_IC_MALFALT(_regs) \
            ((_regs)->ints_state&(_regs)->ints_mask&CR0_XM_MALFALT)

#define OPEN_IC_EMERSIG(_regs) \
            ((_regs)->ints_state&(_regs)->ints_mask&CR0_XM_EMERSIG)

#define OPEN_IC_TRACE(_regs) \
              (sysblk.ints_state&(_regs)->ints_mask&IC_DEBUG_BIT)

#define OPEN_IC_DEBUG(_regs) \
            ((_regs)->ints_state&(_regs)->ints_mask&IC_DEBUG_BIT)

#define OPEN_IC_PERINT(_regs) \
                               ((_regs)->ints_state&IC_PER_MASK)

#define IC_INTERRUPT_CPU(_regs) \
   (((_regs)->ints_state|sysblk.ints_state) & ((_regs)->ints_mask|IC_PER_MASK))

#define IC_INTERRUPT_CPU_NO_PER(_regs) \
   (((_regs)->ints_state|sysblk.ints_state) & (_regs)->ints_mask)

#define SIE_IC_INTERRUPT_CPU(_regs) \
   (((_regs)->ints_state|(sysblk.ints_state&IC_SIE_INT)) & ((_regs)->ints_mask|IC_PER_MASK))

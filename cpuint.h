#define INTERRUPTS_FAST_CHECK

#ifdef INTERRUPTS_FAST_CHECK
/**********************************************************************
 Interrupts_State & Interrupts_Mask bits definition (Initial_Mask=400F)
 Machine check, PER and external interrupt subclass bit positions
 are fixed by the architecture and cannot be changed
 Floating interrupts are made pending to all CPUs, and are 
 recorded in the sysblk structure, CPU specific interrupts
 are recorded in the regs structure.
 dhim mmmm pppp p000 xxxx xxx0 xxxx hhhh : type U32
 |||| |||| |||| |--- |||| |||- |||| ||||   h:mask is always '1'
 |||| |||| |||| |    |||| |||  |||| |||+--> '1' : (regs)->psw.wait == 1
 |||| |||| |||| |    |||| |||  |||| ||+---> '1' : RESTART
 |||| |||| |||| |    |||| |||  |||| |+----> '1' : sysblk.brdcstncpu > 0
 |||| |||| |||| |    |||| |||  |||| +-----> '1' : STORSTAT
 |||| |||| |||| |    |||| |||  ||||
 |||| |||| |||| |    |||| |||  |||+-------> '1' : ETR timer 
 |||| |||| |||| |    |||| |||  ||+--------> '1' : External Signal
 |||| |||| |||| |    |||| |||  |+---------> '1' : INTKEY     (floating)
 |||| |||| |||| |    |||| |||  +----------> '1' : ITIMER
 |||| |||| |||| |    |||| |||
 |||| |||| |||| |    |||| |||
 |||| |||| |||| |    |||| ||+-------------> '1' : SERVSIG    (floating)
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
 |||| |||+--------------------------------> '1' : WARNING    (floating)
 |||| ||+---------------------------------> '1' : XDMGRPT    (floating)
 |||| |+----------------------------------> '1' : DGRDRPT    (floating)
 |||| +-----------------------------------> '1' : RCVYRPT    (floating)
 ||||
 |||+-------------------------------------> '1' : CHANRPT    (floating)
 ||+--------------------------------------> '1' : IOPENDING  (floating)
 |+---------------------------------------> '1' : CPUSTATE!=STARTED
 +----------------------------------------> '1' : IC Debug Mode
**********************************************************************/

/* Hercules related or nonmaskable interrupts */
#define IC_DEBUG_BIT       0x80000000
#define IC_CPU_NOT_STARTED 0x40000000
#define IC_IOPENDING       0x20000000
#define IC_STORSTAT        0x00000008
#define IC_BRDCSTNCPU      0x00000004
#define IC_RESTART         0x00000002
#define IC_PSW_WAIT        0x00000001
#define IC_INITIAL_MASK  ( IC_CPU_NOT_STARTED \
                         | IC_PSW_WAIT \
                         | IC_RESTART \
                         | IC_BRDCSTNCPU \
                         | IC_STORSTAT )

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

#define IC_CR9_SHIFT
#define IC_PER_SB       (CR9_SB >> IC_CR9_SHIFT)
#define IC_PER_IF       (CR9_IF >> IC_CR9_SHIFT)
#define IC_PER_SA       (CR9_SA >> IC_CR9_SHIFT)
#define IC_PER_GRA      (CR9_GRA >> IC_CR9_SHIFT)
#define IC_PER_STURA    (CR9_STURA >> IC_CR9_SHIFT)

#define IC_PER_MASK     ( IC_PER_SB \
                        | IC_PER_IF \
                        | IC_PER_SA \
                        | IC_PER_GRA \
                        | IC_PER_STURA )

/* Bit manipulation macros */
#define SET_IC_INITIAL_MASK(_regs) (_regs)->ints_mask= \
              (IC_INITIAL_MASK|((_regs)->ints_state&IC_DEBUG_BIT))

#define SET_IC_EXTERNAL_MASK(_regs) \
  if( (_regs)->psw.sysmask & PSW_EXTMASK ) \
     (_regs)->ints_mask = (((_regs)->ints_mask&~IC_EXTPENDING) \
                          | ((_regs)->CR(0)&IC_EXTPENDING)); \
   else \
     (_regs)->ints_mask &= ~IC_EXTPENDING

#ifdef FEATURE_BCMODE
  #define SET_IC_IO_MASK(_regs) \
  if( ((_regs)->psw.ecmode ? ((_regs)->psw.sysmask&PSW_IOMASK) : \
                             ((_regs)->psw.sysmask&0xFE)) ) \
     (_regs)->ints_mask |= IC_IOPENDING; \
   else \
     (_regs)->ints_mask &= ~IC_IOPENDING
#else /*!FEATURE_BCMODE*/
  #define SET_IC_IO_MASK(_regs) SET_IC_ECIO_MASK(_regs)
#endif /*FEATURE_BCMODE*/

#define SET_IC_ECIO_MASK(_regs) \
  if( (_regs)->psw.sysmask & PSW_IOMASK ) \
     (_regs)->ints_mask |= IC_IOPENDING; \
   else \
     (_regs)->ints_mask &= ~IC_IOPENDING

#define SET_IC_BCIO_MASK(_regs) \
  if( (_regs)->psw.sysmask & 0xFE ) \
     (_regs)->ints_mask |= IC_IOPENDING; \
   else \
     (_regs)->ints_mask &= ~IC_IOPENDING

#define SET_IC_MCK_MASK(_regs) \
  if( (_regs)->psw.mach ) \
     (_regs)->ints_mask = (((_regs)->ints_mask&~IC_MCKPENDING) \
                          | ((_regs)->CR(14)&IC_MCKPENDING)); \
   else \
     (_regs)->ints_mask &= ~IC_MCKPENDING

/* Set('1') or Reset('0') bit according to condition */

#define SET_IC_PSW_WAIT(_regs) \
  if((_regs)->psw.wait) \
    (_regs)->ints_state|=IC_PSW_WAIT; \
   else \
    (_regs)->ints_state&=~IC_PSW_WAIT

#define SET_IC_TRACE \
  if(sysblk.instbreak || sysblk.inststep || sysblk.insttrace) \
    sysblk.ints_state|=IC_DEBUG_BIT; \
  else \
    sysblk.ints_state&=~IC_DEBUG_BIT;

#define RESET_IC_CPUINT(_regs) 

/* Set bit to '1' */
#define ON_IC_CPU_NOT_STARTED(_regs) \
                        ((_regs)->ints_state|=IC_CPU_NOT_STARTED)
#define ON_IC_RESTART(_regs)    ((_regs)->ints_state|=IC_RESTART)
#define ON_IC_BRDCSTNCPU        (sysblk.ints_state|=IC_BRDCSTNCPU)
#define ON_IC_STORSTAT(_regs)   ((_regs)->ints_state|=IC_STORSTAT)
#define ON_IC_IOPENDING         (sysblk.ints_state|=IC_IOPENDING)
#define ON_IC_CHANRPT           (sysblk.ints_state|=CR14_CHANRPT)
#define ON_IC_INTKEY            (sysblk.ints_state|=CR0_XM_INTKEY)
#define ON_IC_SERVSIG           (sysblk.ints_state|=CR0_XM_SERVSIG)
#define ON_IC_ITIMER(_regs)     ((_regs)->ints_state|=CR0_XM_ITIMER)
#define ON_IC_PTIMER(_regs)     ((_regs)->ints_state|=CR0_XM_PTIMER)
#define ON_IC_CLKC(_regs)       ((_regs)->ints_state|=CR0_XM_CLKC)
#define ON_IC_EXTCALL(_regs)    ((_regs)->ints_state|=CR0_XM_EXTCALL)
#define ON_IC_EMERSIG(_regs)    ((_regs)->ints_state|=CR0_XM_EMERSIG)
#define ON_IC_TRACE             (sysblk.ints_state|=IC_DEBUG_BIT)
#define ON_IC_DEBUG(_regs) \
    { (_regs)->ints_mask|=IC_DEBUG_BIT; (_regs)->ints_state|=IC_DEBUG_BIT; }

/* Re-Set bit to '0' */
#define OFF_IC_CPU_NOT_STARTED(_regs) \
                        ((_regs)->ints_state&=~IC_CPU_NOT_STARTED)
#define OFF_IC_RESTART(_regs)  ((_regs)->ints_state&=~IC_RESTART)
#define OFF_IC_BRDCSTNCPU      (sysblk.ints_state&=~IC_BRDCSTNCPU)
#define OFF_IC_STORSTAT(_regs) ((_regs)->ints_state&=~IC_STORSTAT)
#define OFF_IC_IOPENDING       (sysblk.ints_state&=~IC_IOPENDING)
#define OFF_IC_CHANRPT         (sysblk.ints_state&=~CR14_CHANRPT)
#define OFF_IC_INTKEY          (sysblk.ints_state&=~CR0_XM_INTKEY)
#define OFF_IC_SERVSIG         (sysblk.ints_state&=~CR0_XM_SERVSIG)
#define OFF_IC_ITIMER(_regs)   ((_regs)->ints_state&=~CR0_XM_ITIMER)
#define OFF_IC_PTIMER(_regs)   ((_regs)->ints_state&=~CR0_XM_PTIMER)
#define OFF_IC_CLKC(_regs)     ((_regs)->ints_state&=~CR0_XM_CLKC)
#define OFF_IC_EXTCALL(_regs)  ((_regs)->ints_state&=~CR0_XM_EXTCALL)
#define OFF_IC_EMERSIG(_regs)  ((_regs)->ints_state&=~CR0_XM_EMERSIG)
//#define OFF_IC_TRACE           (sysblk.ints_state&=~IC_DEBUG_BIT)
#define OFF_IC_DEBUG(_regs) \
    { (_regs)->ints_mask&=~IC_DEBUG_BIT; (_regs)->ints_state&=~IC_DEBUG_BIT; }
#define OFF_IC_CPUINT(_regs)
/* Check Interrupt State */
//#define IS_IC_PSW_WAIT(_regs) ((_regs)->ints_state&IC_PSW_WAIT)
#define IS_IC_DISABLED_WAIT_PSW(_regs) \
             ( ((_regs)->ints_mask & IC_OPEN_MASK) == 0 )
#define IS_IC_RESTART(_regs)  ((_regs)->ints_state&IC_RESTART)
//#define IS_IC_BRDCSTNCPU      (sysblk.ints_state&IC_BRDCSTNCPU)
#define IS_IC_STORSTAT(_regs) ((_regs)->ints_state&IC_STORSTAT)
#define IS_IC_IOPENDING       (sysblk.ints_state&IC_IOPENDING)
#define IS_IC_MCKPENDING      (sysblk.ints_state&IC_MCKPENDING)
#define IS_IC_CHANRPT         (sysblk.ints_state&CR14_CHANRPT)
#define IS_IC_EXTPENDING(_regs) \
       ( ((_regs)->ints_state|sysblk.ints_state) & IC_EXTPENDING )
//#define IS_IC_INTKEY          (sysblk.ints_state&CR0_XM_INTKEY)
#define IS_IC_SERVSIG         (sysblk.ints_state&CR0_XM_SERVSIG)
#define IS_IC_ITIMER(_regs)   ((_regs)->ints_state&CR0_XM_ITIMER)
#define IS_IC_PTIMER(_regs)   ((_regs)->ints_state&CR0_XM_PTIMER)
#define IS_IC_CLKC(_regs)     ((_regs)->ints_state&CR0_XM_CLKC)
#define IS_IC_EXTCALL(_regs)  ((_regs)->ints_state&CR0_XM_EXTCALL)
#define IS_IC_EMERSIG(_regs)  ((_regs)->ints_state&CR0_XM_EMERSIG)
#define IS_IC_TRACE           (sysblk.ints_state&IC_DEBUG_BIT)

/* Advanced checks macros */
#define OPEN_IC_MCKPENDING(_regs) \
       ((sysblk.ints_state&IC_MCKPENDING)&(_regs)->ints_mask)

#define OPEN_IC_IOPENDING(_regs) \
       ((sysblk.ints_state&IC_IOPENDING)&(_regs)->ints_mask)

#define OPEN_IC_CHANRPT(_regs) \
       ((sysblk.ints_state&CR14_CHANRPT)&(_regs)->ints_mask)

#define OPEN_IC_EXTPENDING(_regs) \
       ( ((_regs)->ints_state|sysblk.ints_state) \
         & ((_regs)->ints_mask&IC_EXTPENDING) )

#define OPEN_IC_ITIMER(_regs) \
       (((_regs)->ints_state&CR0_XM_ITIMER)&(_regs)->ints_mask)

#define OPEN_IC_PTIMER(_regs) \
       (((_regs)->ints_state&CR0_XM_PTIMER)&(_regs)->ints_mask)

#define OPEN_IC_CLKC(_regs) \
       (((_regs)->ints_state&CR0_XM_CLKC)&(_regs)->ints_mask)

#define OPEN_IC_INTKEY(_regs) \
       (((_regs)->ints_state&CR0_XM_INTKEY)&(_regs)->ints_mask)

#define OPEN_IC_SERVSIG(_regs)   ((sysblk.ints_state&CR0_XM_SERVSIG) \
                                            & (_regs)->ints_mask)

#define OPEN_IC_EXTCALL(_regs)  (((_regs)->ints_state&CR0_XM_EXTCALL) \
                                            & (_regs)->ints_mask)

#define OPEN_IC_EMERSIG(_regs)  (((_regs)->ints_state&CR0_XM_EMERSIG) \
                                            & (_regs)->ints_mask)

#define OPEN_IC_DEBUG(_regs) \
           ((_regs)->ints_state & IC_DEBUG_BIT & (_regs)->ints_mask)

#define IC_EXT_BUT_IT_OR_STORSTAT \
              (IC_STORSTAT | (IC_EXTPENDING & ~CR0_XM_ITIMER))

#define OPEN_IC_CPUINT(_regs) \
   ( ((_regs)->ints_state&IC_EXT_BUT_IT_OR_STORSTAT&(_regs)->ints_mask) \
     || OPEN_IC_ITIMER(_regs) )

#define IC_INTERRUPT_CPU(_regs) \
   (((_regs)->ints_state|sysblk.ints_state) & (_regs)->ints_mask)

#define SIE_IC_INTERRUPT_CPU(_regs) \
   (((_regs)->ints_state|sysblk.ints_state) & (_regs)->ints_mask)

#else /*!INTERRUPTS_FAST_CHECK*/

#error INTERRUPTS_FAST_CHECK must be defined

#endif /*!INTERRUPTS_FAST_CHECK*/

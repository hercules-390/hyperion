#define INTERRUPTS_FAST_CHECK

#ifdef INTERRUPTS_FAST_CHECK
/**********************************************************************
 Interrupts_State & Interrupts_Mask bits definition (Initial_Mask=001F)
 0000 0000 0000 0000 dxxm xxxi xxxh hhhh : type U32
 ---- ---- ---- ---- |||| |||| |||| ||||   h:mask is always '1'
          V          |||| |||| |||| |||+--> '1' : CPUSTATE!=STARTED
          |          |||| |||| |||| ||+---> '1' : (regs)->psw.wait == 1
          |          |||| |||| |||| |+----> '1' : RESTART        (regs)
          |          |||| |||| |||| +-----> '1' : sysblk.brdcstncpu > 0
          |          |||| |||| |||+-------> '1' : STORSTAT       (regs)
          |          |||| |||| ||+--------> '1' : Ext.Signal Int./Mask 
          |          |||| |||| |+---------> '1' : INTKEY       (sysblk)
          |          |||| |||| +----------> '1' : INTERVAL_TIMER (regs)
          |          |||| |||+------------> '1' : IOPENDING    (sysblk)
          |          |||| ||+-------------> '1' : SERVSIG      (sysblk)
          |          |||| |+--------------> '1' : PTPEND         (regs) 
          |          |||| +---------------> '1' : CKPEND         (regs)
          |          |||+-----------------> '1' : Mach.Check Int./Mask
          |          ||+------------------> '1' : EXTCALL        (regs)
          |          |+-------------------> '1' : EMERSIG        (regs)
          |          +--------------------> '1' : IC Debug Mode  (regs)
          |                                     OR Trace active(sysblk)
          +-------------------------------> RESERVED
**********************************************************************/
#define IC_INITIAL_MASK    0x001F
#define IC_DEBUG_BIT       0x8000
#define IC_OPEN_MASK       0x7FE0
/* Hercules related or nonmaskable interrupts */
#define IC_CPU_NOT_STARTED 0x0001
#define IC_PSW_WAIT        0x0002
#define IC_RESTART         0x0004
#define IC_BRDCSTNCPU      0x0008
#define IC_STORSTAT        0x0010
/* Interrupts common to all CPU's */
#define IC_IOPENDING       0x0100
#define IC_MCKPENDING      0x1000
#define IC_INTKEY          0x0040
#define IC_SERVSIG         0x0200
/* CPU related [external] interrupts */
#define IC_ITIMER_PENDING  0x0080
#define IC_PTPEND          0x0400
#define IC_CKPEND          0x0800
#define IC_EXTCALL         0x2000
#define IC_EMERSIG         0x4000
/* Group bits : External */
#define IC_EXTPENDING      0x6EE0
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
  if( (_regs)->psw.mach) \
     (_regs)->ints_mask |= IC_MCKPENDING; \
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
#define ON_IC_MCKPENDING        (sysblk.ints_state|=IC_MCKPENDING)
#define ON_IC_INTKEY            (sysblk.ints_state|=IC_INTKEY)
#define ON_IC_SERVSIG           (sysblk.ints_state|=IC_SERVSIG)
#define ON_IC_ITIMER_PENDING(_regs) \
                         ((_regs)->ints_state|=IC_ITIMER_PENDING)
#define ON_IC_PTPEND(_regs)     ((_regs)->ints_state|=IC_PTPEND)
#define ON_IC_CKPEND(_regs)     ((_regs)->ints_state|=IC_CKPEND)
#define ON_IC_EXTCALL(_regs)    ((_regs)->ints_state|=IC_EXTCALL)
#define ON_IC_EMERSIG(_regs)    ((_regs)->ints_state|=IC_EMERSIG)
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
#define OFF_IC_MCKPENDING      (sysblk.ints_state&=~IC_MCKPENDING)
#define OFF_IC_INTKEY          (sysblk.ints_state&=~IC_INTKEY)
#define OFF_IC_SERVSIG         (sysblk.ints_state&=~IC_SERVSIG)
#define OFF_IC_ITIMER_PENDING(_regs) \
                         ((_regs)->ints_state&=~IC_ITIMER_PENDING)
#define OFF_IC_PTPEND(_regs)   ((_regs)->ints_state&=~IC_PTPEND)
#define OFF_IC_CKPEND(_regs)   ((_regs)->ints_state&=~IC_CKPEND)
#define OFF_IC_EXTCALL(_regs)  ((_regs)->ints_state&=~IC_EXTCALL)
#define OFF_IC_EMERSIG(_regs)  ((_regs)->ints_state&=~IC_EMERSIG)
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
#define IS_IC_EXTPENDING(_regs) \
       ( ((_regs)->ints_state|sysblk.ints_state) & IC_EXTPENDING )
//#define IS_IC_INTKEY          (sysblk.ints_state&IC_INTKEY)
#define IS_IC_SERVSIG         (sysblk.ints_state&IC_SERVSIG)
#define IS_IC_ITIMER_PENDING(_regs) \
             ((_regs)->ints_state&IC_ITIMER_PENDING)
#define IS_IC_PTPEND(_regs)   ((_regs)->ints_state&IC_PTPEND)
#define IS_IC_CKPEND(_regs)   ((_regs)->ints_state&IC_CKPEND)
#define IS_IC_EXTCALL(_regs)  ((_regs)->ints_state&IC_EXTCALL)
#define IS_IC_EMERSIG(_regs)  ((_regs)->ints_state&IC_EMERSIG)
#define IS_IC_TRACE           (sysblk.ints_state&IC_DEBUG_BIT)
/* Advanced checks macros */
#define OPEN_IC_MCKPENDING(_regs) \
       ((sysblk.ints_state&IC_MCKPENDING)&(_regs)->ints_mask)
#define OPEN_IC_IOPENDING(_regs) \
       ((sysblk.ints_state&IC_IOPENDING)&(_regs)->ints_mask)
#define OPEN_IC_EXTPENDING(_regs) \
       ( ((_regs)->ints_state|sysblk.ints_state) \
         & ((_regs)->ints_mask&IC_EXTPENDING) )
#define OPEN_IC_ITIMER_PENDING(_regs) \
       (((_regs)->ints_state&IC_ITIMER_PENDING)&(_regs)->ints_mask)
#define OPEN_IC_INTKEY(_regs) \
       (((_regs)->ints_state&IC_INTKEY)&(_regs)->ints_mask)
#define OPEN_IC_SERVSIG(_regs)   ((sysblk.ints_state&IC_SERVSIG) \
                                            & (_regs)->ints_mask)
#define OPEN_IC_EXTCALL(_regs)  (((_regs)->ints_state&IC_EXTCALL) \
                                            & (_regs)->ints_mask)
#define OPEN_IC_EMERSIG(_regs)  (((_regs)->ints_state&IC_EMERSIG) \
                                            & (_regs)->ints_mask)
#define OPEN_IC_DEBUG(_regs) \
           ((_regs)->ints_state & IC_DEBUG_BIT & (_regs)->ints_mask)
#define IC_EXT_BUT_IT_OR_STORSTAT \
              (IC_STORSTAT | (IC_EXTPENDING & ~IC_ITIMER_PENDING))
#define OPEN_IC_CPUINT(_regs) \
   ( ((_regs)->ints_state&IC_EXT_BUT_IT_OR_STORSTAT&(_regs)->ints_mask) \
     || OPEN_IC_ITIMER_PENDING(_regs) )
#define IC_INTERRUPT_CPU(_regs) \
   (((_regs)->ints_state|sysblk.ints_state) & (_regs)->ints_mask)
#define SIE_IC_INTERRUPT_CPU(_regs) \
   (((_regs)->ints_state|sysblk.ints_state) & (_regs)->ints_mask)

#else /*!INTERRUPTS_FAST_CHECK*/

#error INTERRUPTS_FAST_CHECK must be defined

#endif /*!INTERRUPTS_FAST_CHECK*/

/* MACHDEP.H  Machine specific code                                  */

/*-------------------------------------------------------------------*/
/* Header file containing machine specific macros                    */
/*                                                      	     */
/*-------------------------------------------------------------------*/

#undef  HERCULES_MACHINE
/*-------------------------------------------------------------------*/
/* Intel pentiumpro/i686                                             */
/*-------------------------------------------------------------------*/
#if defined(__i686__) | defined(__pentiumpro__)
#define  HERCULES_MACHINE i686

/* Fetching instruction bytes                                        */
#if defined(OPTION_FETCHIBYTE)
#define FETCHIBYTE1(_ib, _inst) \
        { \
            __asm__("movzbl 1(%%esi),%%eax" : \
                    "=a" (_ib) : "S" (_inst)); \
        }
#define FETCHIBYTE2(_ib, _inst) \
        { \
            __asm__("movzbl 2(%%esi),%%ebx" : \
                    "=b" (_ib) : "S" (_inst)); \
        }
#define FETCHIBYTE3(_ib, _inst) \
        { \
            __asm__("movzbl 3(%%esi),%%eax" : \
                    "=a" (_ib) : "S" (_inst)); \
        }
#endif /*defined(OPTION_FETCHIBYTE)*/

#if 1
#define COMPARE_AND_SWAP(r1, r3, addr, arn, regs) \
        { \
            RADR  abs; \
            U32   prev=0, old, new; \
            abs = LOGICAL_TO_ABS_SKP ((addr), (arn), (regs), \
                                ACCTYPE_WRITE_SKP, (regs)->psw.pkey); \
            old = CSWAP32((regs)->GR_L((r1))); \
            new = CSWAP32((regs)->GR_L((r3))); \
            __asm__("lock; cmpxchgl %1,%2" \
                    :"=a"(prev) \
                    : "q"(new), \
                      "m"(sysblk.mainstor[abs]), "0"(old) \
                    : "memory"); \
            prev = CSWAP32(prev); \
            if ((regs)->GR_L((r1)) == prev) \
            { \
                (regs)->psw.cc = 0; \
                STORAGE_KEY(abs) |= (STORKEY_REF | STORKEY_CHANGE); \
            } \
            else \
            { \
                (regs)->psw.cc = 1; \
                STORAGE_KEY(abs) |= STORKEY_REF; \
                (regs)->GR_L((r1)) = prev; \
            } \
        }
#endif

#endif /*defined(__i686__) | defined(__pentiumpro__)*/

/*-------------------------------------------------------------------*/
/* Default for all other architectures                               */
/*-------------------------------------------------------------------*/
#if !defined(HERCULES_MACHINE)

/* Fetching instruction bytes                                        */
#if defined(OPTION_FETCHIBYTE)
#define FETCHIBYTE1(_ib, _inst) \
        { \
            (_ib) = (_inst)[1]; \
        }
#define FETCHIBYTE2(_ib, _inst) \
        { \
            (_ib) = (_inst)[2]; \
        }
#define FETCHIBYTE3(_ib, _inst) \
        { \
            (_ib) = (_inst)[3]; \
        }
#endif /*defined(OPTION_FETCHIBYTE)*/

#endif /*!defined(HERCULES_MACHINE)*/

#if !defined(COMPARE_AND_SWAP) 
#define COMPARE_AND_SWAP(r1, r3, addr, arn, regs) \
        { \
            RADR  abs; \
            U32   temp; \
            abs = LOGICAL_TO_ABS_SKP ((addr), (arn), (regs), \
                                ACCTYPE_WRITE_SKP, (regs)->psw.pkey); \
            memcpy (&temp, &sysblk.mainstor[abs], 4); \
            temp = CSWAP32(temp); \
            if (temp == regs->GR_L((r1))) \
            { \
                temp = regs->GR_L((r3)); \
                temp = CSWAP32(temp); \
                memcpy (&sysblk.mainstor[abs], &temp, 4); \
                (regs)->psw.cc = 0; \
                STORAGE_KEY(abs) |= (STORKEY_REF | STORKEY_CHANGE); \
            } \
            else \
            { \
                (regs)->psw.cc = 1; \
                STORAGE_KEY(abs) |= STORKEY_REF; \
                regs->GR_L((r1)) = temp; \
            } \
        }
#endif /*!defined(COMPARE_AND_SWAP)*/


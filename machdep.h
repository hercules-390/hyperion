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
#define FETCHIBYTE1(_ib, _inst) \
        { \
            __asm__("movzbl 1(%%esi),%%eax" : \
                    "=a" (_ib) : "S" (_inst)); \
        }
#define FETCHIBYTE4(_ib, _inst) \
        { \
            __asm__("movzbl 4(%%esi),%%ebx" : \
                    "=b" (_ib) : "S" (_inst)); \
        }
#define FETCHIBYTE5(_ib, _inst) \
        { \
            __asm__("movzbl 5(%%esi),%%eax" : \
                    "=a" (_ib) : "S" (_inst)); \
        }

#if 1
#define COMPARE_AND_SWAP(r1, r3, addr, arn, regs) \
        { \
            RADR  abs; \
            U32   prev=0, old, new; \
            abs = LOGICAL_TO_ABS ((addr), (arn), (regs), \
                                ACCTYPE_WRITE, (regs)->psw.pkey); \
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
            } \
            else \
            { \
                (regs)->psw.cc = 1; \
                (regs)->GR_L((r1)) = prev; \
            } \
        }

#define COMPARE_DOUBLE_AND_SWAP(r1, r3, addr, arn, regs) \
        { \
            RADR  abs; \
            void *ptr; \
            U32   temp[4]; \
            abs = LOGICAL_TO_ABS ((addr), (arn), (regs), \
                                ACCTYPE_WRITE, (regs)->psw.pkey); \
            ptr = sysblk.mainstor + abs; \
            temp[0] = (regs)->GR_L((r1)); \
            temp[1] = (regs)->GR_L((r1)+1); \
            temp[2] = (regs)->GR_L((r3)); \
            temp[3] = (regs)->GR_L((r3)+1); \
            __asm__("movl (%1),%%eax\n\t" \
                    "bswap %%eax\n\t" \
                    "movl 4(%1),%%edx\n\t" \
                    "bswap %%edx\n\t" \
                    "movl 8(%1),%%ebx\n\t" \
                    "bswap %%ebx\n\t" \
                    "movl 12(%1),%%ecx\n\t" \
                    "bswap %%ecx\n\t" \
                    "lock; cmpxchg8b (%0)\n\t" \
                    "bswap %%eax\n\t" \
                    "movl %%eax,(%1)\n\t" \
                    "bswap %%edx\n\t" \
                    "movl %%edx,4(%1)\t" \
                    : /* no output */ \
                    : "D"(ptr), \
                      "S"(temp) \
                    : "ax","dx","bx","cx","memory"); \
            if ((regs)->GR_L((r1)) == temp[0] && (regs)->GR_L((r1)+1) == temp[1]) \
            { \
                (regs)->psw.cc = 0; \
            } \
            else \
            { \
                (regs)->psw.cc = 1; \
                (regs)->GR_L((r1)) = temp[0]; \
                (regs)->GR_L((r1)+1) = temp[1]; \
            } \
        }

#define TEST_AND_SET(addr, arn, regs) \
        { \
            RADR  abs; \
            BYTE  old, new=255; \
            abs = LOGICAL_TO_ABS((addr), (arn), (regs), \
                                ACCTYPE_WRITE, (regs)->psw.pkey); \
            old = sysblk.mainstor[abs]; \
            __asm__("1:\t" \
                    "lock; cmpxchgb %b1,%2\n\t" \
                    "jnz 1b" \
                    :"=a"(old) \
                    : "q"(new), "m"(sysblk.mainstor[abs]), "0"(old) \
                    : "memory"); \
            (regs)->psw.cc = old >> 7; \
        }

#endif

#endif /*defined(__i686__) | defined(__pentiumpro__)*/

/*-------------------------------------------------------------------*/
/* Default for all other architectures                               */
/*-------------------------------------------------------------------*/
#if !defined(HERCULES_MACHINE)

/* Fetching instruction bytes                                        */
#define FETCHIBYTE1(_ib, _inst) \
        { \
            (_ib) = (_inst)[1]; \
        }
#define FETCHIBYTE4(_ib, _inst) \
        { \
            (_ib) = (_inst)[4]; \
        }
#define FETCHIBYTE5(_ib, _inst) \
        { \
            (_ib) = (_inst)[5]; \
        }

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
            if (temp == (regs)->GR_L((r1))) \
            { \
                temp = (regs)->GR_L((r3)); \
                temp = CSWAP32(temp); \
                memcpy (&sysblk.mainstor[abs], &temp, 4); \
                (regs)->psw.cc = 0; \
                STORAGE_KEY(abs) |= (STORKEY_REF | STORKEY_CHANGE); \
            } \
            else \
            { \
                (regs)->psw.cc = 1; \
                STORAGE_KEY(abs) |= STORKEY_REF; \
                (regs)->GR_L((r1)) = temp; \
            } \
        }
#endif /*!defined(COMPARE_AND_SWAP)*/

#if !defined(COMPARE_DOUBLE_AND_SWAP) 
#define COMPARE_DOUBLE_AND_SWAP(r1, r3, addr, arn, regs) \
        { \
            RADR  abs; \
            U32   temp1, temp2; \
            abs = LOGICAL_TO_ABS_SKP ((addr), (arn), (regs), \
                                ACCTYPE_WRITE_SKP, (regs)->psw.pkey); \
            memcpy (&temp1, &sysblk.mainstor[abs], 4); \
            memcpy (&temp2, &sysblk.mainstor[abs+4], 4); \
            temp1 = CSWAP32(temp1); \
            temp2 = CSWAP32(temp2); \
            if (temp1 == (regs)->GR_L((r1)) && temp2 == (regs)->GR_L((r1)+1)) \
            { \
                temp1 = CSWAP32((regs)->GR_L((r3))); \
                temp2 = CSWAP32((regs)->GR_L((r3)+1)); \
                memcpy (&sysblk.mainstor[abs], &temp1, 4); \
                memcpy (&sysblk.mainstor[abs+4], &temp2, 4); \
                (regs)->psw.cc = 0; \
                STORAGE_KEY(abs) |= (STORKEY_REF | STORKEY_CHANGE); \
            } \
            else \
            { \
                (regs)->psw.cc = 1; \
                STORAGE_KEY(abs) |= STORKEY_REF; \
                (regs)->GR_L((r1)) = temp1; \
                (regs)->GR_L((r1)+1) = temp2; \
            } \
        }
#endif /*!defined(COMPARE_AND_SWAP)*/

#if !defined(TEST_AND_SET)
#define TEST_AND_SET(addr, arn, regs) \
        { \
            RADR  abs; \
            BYTE  obyte; \
            abs = LOGICAL_TO_ABS((addr), (arn), (regs), \
                                ACCTYPE_WRITE, (regs)->psw.pkey); \
            if ((obyte = sysblk.mainstor[abs]) != 255) \
                sysblk.mainstor[abs] = 255; \
            (regs)->psw.cc = obyte >> 7; \
        }
#endif /*!defined(TEST_AND_SET)*/

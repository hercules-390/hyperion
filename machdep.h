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


/* PTTRACE.H    (C) Copyright Greg Smith, 2003-2013                  */
/*              Threading and locking trace debugger                 */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

/*-------------------------------------------------------------------*/
/* Pthread tracing structures and prototypes                         */
/*-------------------------------------------------------------------*/

#ifndef _PTTHREAD_H_
#define _PTTHREAD_H_

/*-------------------------------------------------------------------*/
/*           Standard Module Import/Export Definitions               */
/*-------------------------------------------------------------------*/
#ifndef _PTTRACE_C_
  #ifndef _HUTIL_DLL_
    #define PTT_DLL_IMPORT      DLL_IMPORT
  #else
    #define PTT_DLL_IMPORT      extern
  #endif
#else
  #define   PTT_DLL_IMPORT      DLL_EXPORT
#endif

/*-------------------------------------------------------------------*/
/*                     PTT Trace Classes                             */
/*-------------------------------------------------------------------*/
#define PTT_CL_LOG      0x00000001      /* Logger records            */
#define PTT_CL_TMR      0x00000002      /* Timer/Clock records       */
#define PTT_CL_THR      0x00000004      /* Thread records            */
#define PTT_CL_INF      0x00000008      /* Instruction info          */
#define PTT_CL_ERR      0x00000010      /* Instruction error/unsup   */
#define PTT_CL_PGM      0x00000020      /* Program interrupt         */
#define PTT_CL_CSF      0x00000040      /* Compare&Swap failure      */
#define PTT_CL_SIE      0x00000080      /* Interpretive Execution    */
#define PTT_CL_SIG      0x00000100      /* SIGP signalling           */
#define PTT_CL_IO       0x00000200      /* IO                        */
//efine PTT_CL_...      0x00000400      /* (Reserved)                */
//efine PTT_CL_...      0x00000800      /* (Reserved)                */
//efine PTT_CL_...      0x00001000      /* (Reserved)                */
//efine PTT_CL_...      0x00002000      /* (Reserved)                */
//efine PTT_CL_...      0x00004000      /* (Reserved)                */
//efine PTT_CL_...      0x00008000      /* (Reserved)                */
#define PTT_CL_USR1     0x00010000      /* User defined class 1      */
#define PTT_CL_USR2     0x00020000      /* User defined class 2      */
#define PTT_CL_USR3     0x00040000      /* User defined class 3      */
#define PTT_CL_USR4     0x00080000      /* User defined class 4      */
#define PTT_CL_USR5     0x00100000      /* User defined class 5      */
#define PTT_CL_USR6     0x00200000      /* User defined class 6      */
#define PTT_CL_USR7     0x00400000      /* User defined class 7      */
#define PTT_CL_USR8     0x00800000      /* User defined class 8      */
#define PTT_CL_USR9     0x01000000      /* User defined class 9      */
#define PTT_CL_USR10    0x02000000      /* User defined class 10     */
#define PTT_CL_USR11    0x04000000      /* User defined class 11     */
#define PTT_CL_USR12    0x08000000      /* User defined class 12     */
#define PTT_CL_USR13    0x10000000      /* User defined class 13     */
#define PTT_CL_USR14    0x20000000      /* User defined class 14     */
#define PTT_CL_USR15    0x40000000      /* User defined class 15     */
#define PTT_CL_USR16    0x80000000      /* User defined class 16     */

/*-------------------------------------------------------------------*/
/*                  Primary PTT Tracing macro                        */
/*-------------------------------------------------------------------*/
#define PTT( _class, _msg, _data1, _data2, _rc )                     \
do {                                                                 \
  if (pttclass & (_class))                                           \
    ptt_pthread_trace( (_class), (_msg),(void*)(uintptr_t)(_data1),  \
                                         (void*)(uintptr_t)(_data2), \
                                         PTT_LOC,                    \
                                         (int)(_rc));                \
} while(0)

/*-------------------------------------------------------------------*/
/*               Exported Function Definitions                       */
/*-------------------------------------------------------------------*/
PTT_DLL_IMPORT void ptt_trace_init    ( int n, int init );
PTT_DLL_IMPORT int  ptt_cmd           ( int argc, const char* argv[], const char* cmdline );
PTT_DLL_IMPORT void ptt_pthread_trace ( int, const char*, const void*, const void*, const char*, int );
PTT_DLL_IMPORT int  ptt_pthread_print ();
PTT_DLL_IMPORT int  pttclass;
PTT_DLL_IMPORT int  pttthread;

/*-------------------------------------------------------------------*/
/*                      Misc Helper Macro                            */
/*-------------------------------------------------------------------*/
#define PTT_MAGIC           -99

#endif /* _PTTHREAD_H_ */

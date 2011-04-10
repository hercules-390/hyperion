/* PTTRACE.H    (c) Copyright Greg Smith, 2003-2010                  */
/*              Header file for pthreads trace debugger              */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

/*-------------------------------------------------------------------*/
/* Pthread tracing structures and prototypes                         */
/*-------------------------------------------------------------------*/

#if !defined( _PTTHREAD_H_ )
#define _PTTHREAD_H_

#ifndef _PTTRACE_C_
#ifndef _HUTIL_DLL_
#define PTT_DLL_IMPORT DLL_IMPORT
#else   /* _HUTIL_DLL_ */
#define PTT_DLL_IMPORT extern
#endif  /* _HUTIL_DLL_ */
#else   /* _PTTRACE_C_ */
#define PTT_DLL_IMPORT DLL_EXPORT
#endif /* _PTTRACE_C_ */

#if defined(OPTION_FTHREADS)
#define OBTAIN_PTTLOCK \
 do { \
   if (!pttnolock) fthread_mutex_lock(&pttlock); \
 } while (0)
#define RELEASE_PTTLOCK \
 do { \
   if (!pttnolock) fthread_mutex_unlock(&pttlock); \
 } while (0)
PTT_DLL_IMPORT int ptt_pthread_mutex_init(LOCK *, void *, char *);
PTT_DLL_IMPORT int ptt_pthread_mutex_lock(LOCK *, char *);
PTT_DLL_IMPORT int ptt_pthread_mutex_trylock(LOCK *, char *);
PTT_DLL_IMPORT int ptt_pthread_mutex_unlock(LOCK *, char *);
PTT_DLL_IMPORT int ptt_pthread_rwlock_init(RWLOCK *, void *, char *);
PTT_DLL_IMPORT int ptt_pthread_rwlock_rdlock(RWLOCK *, char *);
PTT_DLL_IMPORT int ptt_pthread_rwlock_wrlock(RWLOCK *, char *);
PTT_DLL_IMPORT int ptt_pthread_rwlock_tryrdlock(RWLOCK *, char *);
PTT_DLL_IMPORT int ptt_pthread_rwlock_trywrlock(RWLOCK *, char *);
PTT_DLL_IMPORT int ptt_pthread_rwlock_unlock(RWLOCK *, char *);
PTT_DLL_IMPORT int ptt_pthread_cond_init(COND *, void *, char *);
PTT_DLL_IMPORT int ptt_pthread_cond_signal(COND *, char *);
PTT_DLL_IMPORT int ptt_pthread_cond_broadcast(COND *, char *);
PTT_DLL_IMPORT int ptt_pthread_cond_wait(COND *, LOCK *, char *);
PTT_DLL_IMPORT int ptt_pthread_cond_timedwait(COND *, LOCK *, struct timespec *, char *);
PTT_DLL_IMPORT int ptt_pthread_create(TID *, ATTR *, PFT_THREAD_FUNC, void *, char *, char *);
PTT_DLL_IMPORT int ptt_pthread_join(TID, void **, char *);
PTT_DLL_IMPORT int ptt_pthread_detach(TID, char *);
PTT_DLL_IMPORT int ptt_pthread_kill(TID, int, char *);
#else
#define OBTAIN_PTTLOCK \
 do { \
   if (!pttnolock) pthread_mutex_lock(&pttlock); \
 } while (0)
#define RELEASE_PTTLOCK \
 do { \
   if (!pttnolock) pthread_mutex_unlock(&pttlock); \
 } while (0)
PTT_DLL_IMPORT int ptt_pthread_mutex_init(LOCK *, pthread_mutexattr_t *, char *);
PTT_DLL_IMPORT int ptt_pthread_mutex_lock(LOCK *, char *);
PTT_DLL_IMPORT int ptt_pthread_mutex_trylock(LOCK *, char *);
PTT_DLL_IMPORT int ptt_pthread_mutex_unlock(LOCK *, char *);
PTT_DLL_IMPORT int ptt_pthread_rwlock_init(RWLOCK *, pthread_rwlockattr_t *, char *);
PTT_DLL_IMPORT int ptt_pthread_rwlock_rdlock(RWLOCK *, char *);
PTT_DLL_IMPORT int ptt_pthread_rwlock_wrlock(RWLOCK *, char *);
PTT_DLL_IMPORT int ptt_pthread_rwlock_tryrdlock(RWLOCK *, char *);
PTT_DLL_IMPORT int ptt_pthread_rwlock_trywrlock(RWLOCK *, char *);
PTT_DLL_IMPORT int ptt_pthread_rwlock_unlock(RWLOCK *, char *);
PTT_DLL_IMPORT int ptt_pthread_cond_init(COND *, pthread_condattr_t *, char *);
PTT_DLL_IMPORT int ptt_pthread_cond_signal(COND *, char *);
PTT_DLL_IMPORT int ptt_pthread_cond_broadcast(COND *, char *);
PTT_DLL_IMPORT int ptt_pthread_cond_wait(COND *, LOCK *, char *);
PTT_DLL_IMPORT int ptt_pthread_cond_timedwait(COND *, LOCK *, const struct timespec *, char *);
PTT_DLL_IMPORT int ptt_pthread_create(TID *, ATTR *, void *(*)(), void *, char *, char *);
PTT_DLL_IMPORT int ptt_pthread_join(TID, void **, char *);
PTT_DLL_IMPORT int ptt_pthread_detach(TID, char *);
PTT_DLL_IMPORT int ptt_pthread_kill(TID, int, char *);
#endif

PTT_DLL_IMPORT void ptt_trace_init (int n, int init);
PTT_DLL_IMPORT int  ptt_cmd(int argc, char *argv[], char*cmdline);
PTT_DLL_IMPORT void ptt_pthread_trace (int, char *, void *, void *, char *, int);
PTT_DLL_IMPORT int  ptt_pthread_print ();
PTT_DLL_IMPORT int  pttclass;
void *ptt_timeout();

typedef struct _PTT_TRACE {
        TID          tid;               /* Thread id                  */
        int          trclass;           /* Trace record class         */
#define PTT_CL_LOG  0x0001              /* Logger records             */
#define PTT_CL_TMR  0x0002              /* Timer/Clock records        */
#define PTT_CL_THR  0x0004              /* Thread records             */
#define PTT_CL_INF  0x0100              /* Instruction info           */
#define PTT_CL_ERR  0x0200              /* Instruction error/unsup    */
#define PTT_CL_PGM  0x0400              /* Program interrupt          */
#define PTT_CL_CSF  0x0800              /* Compare&Swap failure       */
#define PTT_CL_SIE  0x1000              /* Interpretive Execution     */
#define PTT_CL_SIG  0x2000              /* SIGP signalling            */
#define PTT_CL_IO   0x4000              /* IO                         */
        char        *type;              /* Trace type                 */
        void        *data1;             /* Data 1                     */
        void        *data2;             /* Data 2                     */
        char        *loc;               /* File name:line number      */
        struct timeval tv;              /* Time of day                */
        int          result;            /* Result                     */
      } PTT_TRACE;

#define PTT_LOC_Q( _string ) #_string
#define PTT_LOC_M( _string ) PTT_LOC_Q( _string )
#define PTT_LOC __FILE__ ":" PTT_LOC_M( __LINE__ ) 

#define PTT_TRACE_SIZE sizeof(PTT_TRACE)

#define PTT_MAGIC -99

#define PTT(_class,_type,_data1,_data2,_result) \
do { \
  if (pttclass & (_class)) \
        ptt_pthread_trace(_class,_type,(void *)(uintptr_t)(_data1),(void *)(uintptr_t)(_data2),PTT_LOC,(int)(_result)); \
} while(0)

#define PTTRACE(_type,_data1,_data2,_loc,_result) \
do { \
  if (pttclass & PTT_CL_THR) \
    ptt_pthread_trace(PTT_CL_THR,_type,_data1,_data2,_loc,_result); \
} while(0)

#endif /* defined( _PTTHREAD_H_ ) */

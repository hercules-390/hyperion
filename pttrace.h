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
/*--------------------------------------------------------------------*/
/* Pthread tracing                                                    */
/*--------------------------------------------------------------------*/

#if defined(OPTION_FTHREADS)
#define OBTAIN_PTTLOCK \
 do { \
   if (!pttnolock) fthread_mutex_lock(&pttlock); \
 } while (0)
#define RELEASE_PTTLOCK \
 do { \
   if (!pttnolock) fthread_mutex_unlock(&pttlock); \
 } while (0)
PTT_DLL_IMPORT int ptt_pthread_mutex_init(LOCK *, void *, char *, int);
PTT_DLL_IMPORT int ptt_pthread_mutex_lock(LOCK *, char *, int);
PTT_DLL_IMPORT int ptt_pthread_mutex_trylock(LOCK *, char *, int);
PTT_DLL_IMPORT int ptt_pthread_mutex_unlock(LOCK *, char *, int);
PTT_DLL_IMPORT int ptt_pthread_cond_init(COND *, void *, char *, int);
PTT_DLL_IMPORT int ptt_pthread_cond_signal(COND *, char *, int);
PTT_DLL_IMPORT int ptt_pthread_cond_broadcast(COND *, char *, int);
PTT_DLL_IMPORT int ptt_pthread_cond_wait(COND *, LOCK *, char *, int);
PTT_DLL_IMPORT int ptt_pthread_cond_timedwait(COND *, LOCK *, struct timespec *, char *, int);
PTT_DLL_IMPORT int ptt_pthread_create(TID *, ATTR *, PFT_THREAD_FUNC, void *, char *, int);
PTT_DLL_IMPORT int ptt_pthread_join(TID, void **, char *, int);
PTT_DLL_IMPORT int ptt_pthread_detach(TID, char *, int);
PTT_DLL_IMPORT int ptt_pthread_kill(TID, int, char *, int);
#else
#define OBTAIN_PTTLOCK \
 do { \
   if (!pttnolock) pthread_mutex_lock(&pttlock); \
 } while (0)
#define RELEASE_PTTLOCK \
 do { \
   if (!pttnolock) pthread_mutex_unlock(&pttlock); \
 } while (0)
PTT_DLL_IMPORT int ptt_pthread_mutex_init(LOCK *, pthread_mutexattr_t *, char *, int);
PTT_DLL_IMPORT int ptt_pthread_mutex_lock(LOCK *, char *, int);
PTT_DLL_IMPORT int ptt_pthread_mutex_trylock(LOCK *, char *, int);
PTT_DLL_IMPORT int ptt_pthread_mutex_unlock(LOCK *, char *, int);
PTT_DLL_IMPORT int ptt_pthread_cond_init(COND *, pthread_condattr_t *, char *, int);
PTT_DLL_IMPORT int ptt_pthread_cond_signal(COND *, char *, int);
PTT_DLL_IMPORT int ptt_pthread_cond_broadcast(COND *, char *, int);
PTT_DLL_IMPORT int ptt_pthread_cond_wait(COND *, LOCK *, char *, int);
PTT_DLL_IMPORT int ptt_pthread_cond_timedwait(COND *, LOCK *, const struct timespec *, char *, int);
PTT_DLL_IMPORT int ptt_pthread_create(TID *, ATTR *, void *(*)(), void *, char *, int);
PTT_DLL_IMPORT int ptt_pthread_join(TID, void **, char *, int);
PTT_DLL_IMPORT int ptt_pthread_detach(TID, char *, int);
PTT_DLL_IMPORT int ptt_pthread_kill(TID, int, char *, int);
#endif

PTT_DLL_IMPORT void ptt_trace_init (int n, int init);
PTT_DLL_IMPORT int  ptt_cmd(int argc, char *argv[], char*cmdline);
PTT_DLL_IMPORT void ptt_pthread_trace (char *, void *, void *, char *, int, int);
PTT_DLL_IMPORT void ptt_pthread_print ();

typedef struct _PTT_TRACE {
        TID          tid;               /* Thead id                   */
        char        *type;              /* Trace type                 */
        void        *data1;             /* Data 1                     */
        void        *data2;             /* Data 2                     */
        char        *file;              /* File name                  */
        int          line;              /* Line number                */
        struct timeval tv;              /* Time of day                */
        int          result;            /* Result                     */
      } PTT_TRACE;
#define PTT_TRACE_SIZE sizeof(PTT_TRACE)
#define PTT_MAGIC -99
#define PTT(type,data1,data2,result) \
        ptt_pthread_trace(type,(void *)(data1),(void *)(data2),__FILE__,__LINE__,(int)(result))
#define PTTRACE(type,data1,data2,file,line,result) \
  if (!pttnothreads) \
    ptt_pthread_trace(type,data1,data2,file,line,result)
#endif /* defined( _PTTHREAD_H_ ) */

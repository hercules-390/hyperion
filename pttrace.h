#if !defined( _PTTHREAD_H_ )
#define _PTTHREAD_H_

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
int ptt_pthread_mutex_init(LOCK *, void *, char *, int);
int ptt_pthread_mutex_lock(LOCK *, char *, int);
int ptt_pthread_mutex_trylock(LOCK *, char *, int);
int ptt_pthread_mutex_unlock(LOCK *, char *, int);
int ptt_pthread_cond_init(COND *, void *, char *, int);
int ptt_pthread_cond_signal(COND *, char *, int);
int ptt_pthread_cond_broadcast(COND *, char *, int);
int ptt_pthread_cond_wait(COND *, LOCK *, char *, int);
int ptt_pthread_cond_timedwait(COND *, LOCK *, struct timespec *, char *, int);
int ptt_pthread_create(TID *, ATTR *, PFT_THREAD_FUNC, void *, char *, int);
int ptt_pthread_join(TID, void **, char *, int);
int ptt_pthread_detach(TID, char *, int);
int ptt_pthread_kill(TID, int, char *, int);
#else
#define OBTAIN_PTTLOCK \
 do { \
   if (!pttnolock) pthread_mutex_lock(&pttlock); \
 } while (0)
#define RELEASE_PTTLOCK \
 do { \
   if (!pttnolock) pthread_mutex_unlock(&pttlock); \
 } while (0)
int ptt_pthread_mutex_init(LOCK *, pthread_mutexattr_t *, char *, int);
int ptt_pthread_mutex_lock(LOCK *, char *, int);
int ptt_pthread_mutex_trylock(LOCK *, char *, int);
int ptt_pthread_mutex_unlock(LOCK *, char *, int);
int ptt_pthread_cond_init(COND *, pthread_condattr_t *, char *, int);
int ptt_pthread_cond_signal(COND *, char *, int);
int ptt_pthread_cond_broadcast(COND *, char *, int);
int ptt_pthread_cond_wait(COND *, LOCK *, char *, int);
int ptt_pthread_cond_timedwait(COND *, LOCK *, const struct timespec *, char *, int);
int ptt_pthread_create(TID *, ATTR *, void *(*)(), void *, char *, int);
int ptt_pthread_join(TID, void **, char *, int);
int ptt_pthread_detach(TID, char *, int);
int ptt_pthread_kill(TID, int, char *, int);
#endif

void ptt_trace_init (int n, int init);
int  ptt_cmd(int argc, char *argv[], char*cmdline);
void ptt_pthread_trace (char *, void *, void *, char *, int, int);
void ptt_pthread_print ();

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
#define PTT(_a,_b,_c,_d) \
        ptt_pthread_trace(_a,(void *)(_b),(void *)(_c),__FILE__,__LINE__,(int)(_d))
#define PTTRACE(_a,_b,_c,_d,_e,_f) \
  if (!pttnothreads) \
    ptt_pthread_trace(_a,_b,_c,_d,_e,_f)
#endif /* defined( _PTTHREAD_H_ ) */

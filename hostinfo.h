/* HOSTINFO.H   (c) Copyright "Fish" (David B. Trout), 2002-2004     */

/*   Released under the Q Public License                             */
/*      (http://www.conmicro.cx/hercules/herclic.html)               */
/*   as modifications to Hercules.                                   */

/*-------------------------------------------------------------------*/
/* Header file contains host system information                      */
/*-------------------------------------------------------------------*/

#if !defined(_HOSTINFO_H_)

#define _HOSTINFO_H_

#if defined(HAVE_CONFIG_H)
#include <config.h>                     /* (need WIN32 flag defined) */
#endif /*defined(HAVE_CONFIG_H)*/

#include <stdio.h>                      /* (need FILE type defined)  */

typedef struct _HOST_INFO               /* Host system info          */
{
#if defined(WIN32)
    int     trycritsec_avail;           /* 1=TryEnterCriticalSection */
    int     multi_proc;                 /* 1=multi-CPU               */
#else /*!defined(WIN32)*/
    int     dummy;                      /* (not defined yet)         */
#endif /*defined(WIN32)*/
}
HOST_INFO;

extern  HOST_INFO  hostinfo;
extern  void  init_hostinfo ();
extern  void  display_hostinfo (FILE *f);

#if defined(WIN32)
extern  int   get_process_directory(char* dirbuf, size_t bufsiz);
extern  int is_win32_directory(char* dir);
extern void convert_win32_directory_to_posix_directory(const char *win32_dir, char *posix_dir);
#endif /*defined(WIN32)*/

#endif /*!defined(_HOSTINFO_H_)*/

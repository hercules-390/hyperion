/* HOSTINFO.C   (c) Copyright "Fish" (David B. Trout), 2002.         */

/*   Released under the Q Public License                             */
/*      (http://www.conmicro.cx/hercules/herclic.html)               */
/*   as modifications to Hercules.                                   */

/*-------------------------------------------------------------------*/
/* functions to set/query host system information                    */
/*-------------------------------------------------------------------*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "hercnls.h"

#if defined(WIN32)
#define _WIN32_WINNT 0x0403
#include <windows.h>
#include <sys/cygwin.h>
#endif /*defined(WIN32)*/

#include <sys/utsname.h>

#include "hostinfo.h"

HOST_INFO  hostinfo;                /* Host system information       */

/*-------------------------------------------------------------------*/
/* initialize host system information                                */
/*-------------------------------------------------------------------*/
void init_hostinfo ()
{
#if defined(WIN32)
    {
        CRITICAL_SECTION cs;
        InitializeCriticalSection(&cs);
        if (!TryEnterCriticalSection(&cs))
            hostinfo.trycritsec_avail = 0;
        else
        {
            hostinfo.trycritsec_avail = 1;
            LeaveCriticalSection(&cs);
        }
        DeleteCriticalSection(&cs);
    }
    {
        SYSTEM_INFO   si;
        GetSystemInfo(&si);
        hostinfo.multi_proc = (si.dwNumberOfProcessors > 1);
    }
#endif /*defined(WIN32)*/
}

/*-------------------------------------------------------------------*/
/* Display host system information                                   */
/*-------------------------------------------------------------------*/
void display_hostinfo (FILE *f)
{
    struct utsname uname_info;

    uname(&uname_info);

    fprintf(f,_("Running on %s %s%s %s %s\n"),
        uname_info.sysname,
        uname_info.machine,
#if defined(WIN32)
        hostinfo.multi_proc ? " MP" :
#endif /*defined(WIN32)*/
        "",
        uname_info.release,
        uname_info.version
        );
}

#if defined(WIN32)
/*-------------------------------------------------------------------*/
/* Retrieve directory where process was loaded from                  */
/*-------------------------------------------------------------------*/
int get_process_directory(char* dirbuf, size_t bufsiz)
{
    /* (returns >0 == success, 0 == failure) */
    char win32_dirbuf[MAX_PATH];
    char posix_dirbuf[MAX_PATH];
    char* p;
    DWORD dwDirBytes =
        GetModuleFileName(GetModuleHandle(NULL),win32_dirbuf,MAX_PATH);
    if (!dwDirBytes || dwDirBytes >= MAX_PATH)
        return 0;
    p = strrchr(win32_dirbuf,'\\'); if (p) *p = 0;
    /* (note: we assume win32 == cygwin) */
    cygwin_conv_to_full_posix_path(win32_dirbuf, posix_dirbuf);
    if ('/' != *(p+strlen(posix_dirbuf)-1))
        strncat(posix_dirbuf,"/",MAX_PATH);
    if (strlen(posix_dirbuf) >= bufsiz)
        return 0;
    strncpy(dirbuf,posix_dirbuf,bufsiz);
    return strlen(dirbuf);
}
#endif /*defined(WIN32)*/

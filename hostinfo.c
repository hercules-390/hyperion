/* HOSTINFO.C   (c) Copyright "Fish" (David B. Trout), 2002-2005     */

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
    static int bDidThisOnceAlready = 0;
    if (bDidThisOnceAlready) return;
    bDidThisOnceAlready = 1;
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

    init_hostinfo();    /* (ensure hostinfo is initialized first!) */

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

#if defined(WIN32)     /* (we assume win32 == cygwin) */

/* ===================================================================================

                           "CYGWIN API REFERENCE" Notes:

--------------------------------------------------------------------------------------
extern "C" void cygwin32_conv_to_full_posix_path(const char *path, char *posix_path);

Converts a Win32 path to a POSIX path. If path is already a POSIX path, leaves it alone.
If path is relative, then posix_path will be converted to an absolute path. Note that
posix_path must point to a buffer of sufficient size; use MAX_PATH if needed.

--------------------------------------------------------------------------------------
extern "C" void cygwin32_conv_to_full_win32_path(const char *path, char *win32_path);

Converts a POSIX path to a Win32 path. If path is already a Win32 path, leaves it alone.
If path is relative, then win32_path will be converted to an absolute path. Note that
win32_path must point to a buffer of sufficient size; use MAX_PATH if needed.

--------------------------------------------------------------------------------------
extern "C" void cygwin32_conv_to_posix_path(const char *path, char *posix_path);

Converts a Win32 path to a POSIX path. If path is already a POSIX path, leaves it alone.
If path is relative, then posix_path will also be relative. Note that posix_path must
point to a buffer of sufficient size; use MAX_PATH if needed.

--------------------------------------------------------------------------------------
extern "C" void cygwin32_conv_to_win32_path(const char *path, char *win32_path);

Converts a POSIX path to a Win32 path. If path is already a Win32 path, leaves it alone.
If path is relative, then win32_path will also be relative. Note that win32_path must
point to a buffer of sufficient size; use MAX_PATH if needed.

--------------------------------------------------------------------------------------
extern "C" int cygwin32_posix_path_list_p(const char *path);

This function tells you if the supplied path is a POSIX-style path (i.e. posix names,
forward slashes, colon delimiters) or a Win32-style path (drive letters, reverse slashes,
semicolon delimiters. The return value is true if the path is a POSIX path. Note that
"_p" means "predicate", a lisp term meaning that the function tells you something about
the parameter.

Rather than use a mode to say what the "proper" path list format is, we allow any, and
give apps the tools they need to convert between the two. If a ';' is present in the
path list it's a Win32 path list. Otherwise, if the first path begins with [letter]:
(in which case it can be the only element since if it wasn't a ';' would be present)
it's a Win32 path list. Otherwise, it's a POSIX path list.

========================================================================================= */

/*-------------------------------------------------------------------*/
/* Determine if directory is a Win32 directory or a Posix directory  */
/*-------------------------------------------------------------------*/
int is_win32_directory(char* dir)
{
    /* (returns: !0 (TRUE) == is Win32 direcory,
                 0 (FALSE) == is Poxix direcory) */
    return !cygwin32_posix_path_list_p(dir);
}

/*-------------------------------------------------------------------*/
/* Convert a Win32 directory to a Posix directory                    */
/*-------------------------------------------------------------------*/
void convert_win32_directory_to_posix_directory(const char *win32_dir, char *posix_dir)
{
    cygwin32_conv_to_full_posix_path(win32_dir, posix_dir);
}

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
    p = strrchr(win32_dirbuf,'\\'); if (p) *(p+1) = 0;
    cygwin_conv_to_full_posix_path(win32_dirbuf, posix_dirbuf);
    if (strlen(posix_dirbuf) >= bufsiz)
        return 0;
    strncpy(dirbuf,posix_dirbuf,bufsiz);
    return strlen(dirbuf);
}
#endif /*defined(WIN32)*/

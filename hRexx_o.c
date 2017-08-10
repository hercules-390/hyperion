/* HREXX_O.C    (c)Copyright Enrico Sorichetti, 2012                 */
/*              Rexx Interpreter Support (Object  Rexx )             */
/*                                                                   */
/*  Released under "The Q Public License Version 1"                  */
/*  (http://www.hercules-390.org/herclic.html) as modifications to   */
/*  Hercules.                                                        */

/*  inspired by the previous Rexx implementation by Jan Jaeger       */

#include "hstdinc.h"

#ifndef _HREXX_O_C_
#define _HREXX_O_C_

#if defined(ENABLE_OBJECT_REXX)

#define HREXXDROPVAR            ObjectRexxDropVar
#define HREXXFETCHVAR           ObjectRexxFetchVar
#define HREXXSETVAR             ObjectRexxSetVar
#define HREXXEXECCMD            ObjectRexxExecCmd
#define HREXXEXECINSTORECMD     ObjectRexxExecInstoreCmd
#define HREXXEXECSUB            ObjectRexxExecSub

#define HREXXREGISTERFUNCTIONS  ObjectRexxRegisterFunctions
#define HREXXEXTERNALFUNCTION_T size_t REXXENTRY

#define HREXXAWSCMD             ObjectRexxawscmd

#define HREXXREGISTERHANDLERS   ObjectRexxRegisterHandlers
#define HREXXPFN                REXXPFN
#define HREXXSTRING             CONSTRXSTRING

#define HREXXEXITHANDLER_T      RexxReturnCode REXXENTRY
#define HREXXLONG               int

#define HREXXSUBCOMHANDLER_T    RexxReturnCode REXXENTRY
#define HREXXPSTRING            PCONSTRXSTRING

#define _HENGINE_DLL_
#include "hercules.h"

#include "hRexx.h"

#include "rexx.h"
#include "oorexxapi.h"

extern void *hRexxLibHandle;        /* Library handle */
extern void *hRexxApiLibHandle;     /* Api Library handle */
extern void *hRexxUtilLibHandle;    /* Utility Lbrary handle ooRexx */

extern char *RexxPackage;

extern char *RexxLibrary;
extern char *RexxApiLibrary;

extern int   MessageLevel;
extern char *MessagePrefix;
extern char *ErrorPrefix;

extern int (*RexxDynamicLoader)();
extern int (*RexxRegisterFunctions)();
extern int (*RexxRegisterHandlers)();
extern int (*RexxExecCmd)();
extern int (*RexxExecInstoreCmd)();
extern int (*RexxExecSub)();

static PFNREXXSTART                 hRexxStart;
static PFNREXXREGISTERFUNCTIONEXE   hRexxRegisterFunction;
static PFNREXXDEREGISTERFUNCTION    hRexxDeregisterFunction;
static PFNREXXREGISTERSUBCOMEXE     hRexxRegisterSubcom;
static PFNREXXDEREGISTERSUBCOM      hRexxDeregisterSubcom;
static PFNREXXREGISTEREXITEXE       hRexxRegisterExit;
static PFNREXXDEREGISTEREXIT        hRexxDeregisterExit;
static PFNREXXALLOCATEMEMORY        hRexxAllocateMemory;
static PFNREXXFREEMEMORY            hRexxFreeMemory;
static PFNREXXVARIABLEPOOL          hRexxVariablePool;

/*
    TAKE NOTE, TAKE NOTE:  The ooRexx libraries, all three of them, must be
    opened, in the order librexxapi.so, librexx.so, and librexxutil.so, and 
    with the RTLD_GLOBAL flag set.  The first two libraries have dependencies
    on each other.  When built with libtool, these dependencies seem to be
    addressed but it could be good luck.  Without libtool, not so much luck.  
    See David Ashley's post dated 3 Nov 2010 in the ooRexx bug tracker at
    https://sourceforge.net/p/oorexx/mailman/message/26544162/.  The entire
    thread is worth a look.  (David Ashley is an ooRexx developer.)
*/

int ObjectRexxDynamicLoader()
{
    HDLOPEN( hRexxApiLibHandle, OOREXX_API_LIBRARY, RTLD_LAZY | RTLD_GLOBAL);

    HDLOPEN(hRexxLibHandle, OOREXX_LIBRARY, RTLD_LAZY | RTLD_GLOBAL);

    HDLOPEN(hRexxUtilLibHandle, OOREXX_UTIL_LIBRARY, RTLD_LAZY | RTLD_GLOBAL);

    HDLSYM ( hRexxStart, hRexxLibHandle, REXX_START);

    HDLSYM ( hRexxRegisterFunction, hRexxApiLibHandle, REXX_REGISTER_FUNCTION);

    HDLSYM ( hRexxDeregisterFunction, hRexxApiLibHandle, REXX_DEREGISTER_FUNCTION);

    HDLSYM ( hRexxRegisterSubcom, hRexxApiLibHandle, REXX_REGISTER_SUBCOM);

    HDLSYM ( hRexxDeregisterSubcom, hRexxApiLibHandle, REXX_DEREGISTER_SUBCOM);

    HDLSYM ( hRexxRegisterExit, hRexxApiLibHandle, REXX_REGISTER_EXIT);

    HDLSYM ( hRexxDeregisterExit, hRexxApiLibHandle, REXX_DEREGISTER_EXIT);

    HDLSYM ( hRexxAllocateMemory, hRexxApiLibHandle, REXX_ALLOCATE_MEMORY);

    HDLSYM ( hRexxFreeMemory, hRexxApiLibHandle, REXX_FREE_MEMORY);

    HDLSYM ( hRexxVariablePool, hRexxLibHandle, REXX_VARIABLE_POOL);

    return 0;
}

#include "hRexxapi.h"

#endif /* defined(ENABLE_OBJECT_REXX) */
#endif /* #ifndef _HREXX_O_C_  */

/* HREXX_R.C    (c)Copyright Enrico Sorichetti, 2012                 */
/*              REXX Interpreter Support ( Regina Rexx )             */
/*                                                                   */
/*  Released under "The Q Public License Version 1"                  */
/*  (http://www.hercules-390.org/herclic.html) as modifications to   */
/*  Hercules.                                                        */

/*  inspired by the previous Rexx implementation by Jan Jaeger       */

// $Id: $

#include "hstdinc.h"
#define _HENGINE_DLL_
#include "hercules.h"

#ifndef _HREXX_R_C_
#define _HREXX_R_C_

#if defined(ENABLE_REGINA_REXX)

#define LOG_CAPTURE( _RETC_ , _RESP_, _FCNM_, _BUFF_) do { \
_RESP_ = log_capture( _FCNM_ , _BUFF_ ); \
_RETC_ = 0;\
} while (0)

#define INCL_REXXSAA
#if defined(HAVE_REGINA_REXXSAA_H)
 #include "regina/rexxsaa.h"
#else
 #include "rexxsaa.h"
#endif

#define RXAPI_MEMFAIL   1002

#include "hRexx.h"

extern void *hRexxLibHandle;        /* Library handle */
extern void *hRexxApiLibHandle;     /* Api Library handle */

extern char *RexxPackage;

extern char *RexxLibrary;
extern char *RexxApiLibrary;

extern int   MessageLevel;
extern char *MessagePrefix;
extern char *ErrorPrefix;

extern int (*RexxDynamicLoader)();
extern int (*RexxRegisterHandlers)();
extern int (*RexxExecCmd)();
extern int (*RexxExecInstoreCmd)();

typedef APIRET APIENTRY PFNREXXSTART( LONG, PRXSTRING, PCSZ, PRXSTRING, PCSZ, LONG, PRXSYSEXIT, PSHORT, PRXSTRING );
typedef APIRET APIENTRY PFNREXXREGISTERSUBCOMEXE( PCSZ, PFN, PUCHAR);
typedef APIRET APIENTRY PFNREXXDEREGISTERSUBCOM( PCSZ, PCSZ);
typedef APIRET APIENTRY PFNREXXREGISTEREXITEXE( PSZ, PFN , PUCHAR );
typedef APIRET APIENTRY PFNREXXDEREGISTEREXIT( PCSZ, PCSZ);
typedef PVOID  APIENTRY PFNREXXALLOCATEMEMORY(ULONG);
typedef APIRET APIENTRY PFNREXXFREEMEMORY( PVOID);

typedef APIRET APIENTRY PFNREXXVARIABLEPOOL( PSHVBLOCK );


static PFNREXXSTART             *hRexxStart;
static PFNREXXREGISTERSUBCOMEXE *hRexxRegisterSubcom;
static PFNREXXDEREGISTERSUBCOM  *hRexxDeregisterSubcom;
static PFNREXXREGISTEREXITEXE   *hRexxRegisterExit;
static PFNREXXDEREGISTEREXIT    *hRexxDeregisterExit;
static PFNREXXALLOCATEMEMORY    *hRexxAllocateMemory;
static PFNREXXFREEMEMORY        *hRexxFreeMemory;

static PFNREXXVARIABLEPOOL      *hRexxVariablePool;

int ReginaRexxFetchVar (
   char *    pszVar,             /* Variable name                     */
   PRXSTRING prxVar)             /* REXX variable contents            */
   {

   /* local function variables */
   SHVBLOCK      RxVarBlock;
   unsigned long ulRetc;
   char *        pszTemp;

   /* initialize the shared variable block */
   RxVarBlock.shvnext = NULL;
   RxVarBlock.shvname.strptr = pszVar;
   RxVarBlock.shvname.strlength = strlen(pszVar);
   RxVarBlock.shvnamelen = RxVarBlock.shvname.strlength;
   RxVarBlock.shvvalue.strptr = NULL;
   RxVarBlock.shvvalue.strlength = 0;
   RxVarBlock.shvvaluelen = 0;
   RxVarBlock.shvcode = RXSHV_SYFET;
   RxVarBlock.shvret = RXSHV_OK;

   /* fetch variable from pool */
   ulRetc = (*hRexxVariablePool)(&RxVarBlock);

   /* test return code */
   if (ulRetc != RXSHV_OK && ulRetc != RXSHV_NEWV) {
      prxVar -> strptr = NULL;
      prxVar -> strlength = 0;
      }
   else {
      /* allocate a new buffer for the Rexx variable pool value */
      pszTemp = (char *) (*hRexxAllocateMemory)(RxVarBlock.shvvalue.strlength + 1);
      if (pszTemp == NULL) {
         /* no buffer available so return a NULL Rexx value */
         prxVar -> strptr = NULL;
         prxVar -> strlength = 0;
         ulRetc = RXSHV_MEMFL;
         }
      else {
         /* copy to new buffer and zero-terminate */
         memmove(pszTemp, RxVarBlock.shvvalue.strptr,
                  RxVarBlock.shvvalue.strlength);
         *(pszTemp + RxVarBlock.shvvalue.strlength) = '\0';
         prxVar -> strptr = pszTemp;
         prxVar -> strlength = RxVarBlock.shvvalue.strlength;
         }
      // free memory returned from RexxVariablePool API
      (*hRexxFreeMemory)(RxVarBlock.shvvalue.strptr);
      }

   return (int)ulRetc;
   }

int ReginaRexxSetVar (
  char *        pszVar,         /* Variable name to be set           */
  char *        pValue,         /* Ptr to new value                  */
  size_t        ulLen)          /* Value length                      */
{
SHVBLOCK      RxVarBlock;
unsigned long ulRetc;

    /* initialize RxVarBlock */
    RxVarBlock.shvnext = NULL;
    RxVarBlock.shvname.strptr = pszVar;
    RxVarBlock.shvname.strlength = strlen(pszVar);
    RxVarBlock.shvnamelen = RxVarBlock.shvname.strlength;
    RxVarBlock.shvvalue.strptr = pValue;
    RxVarBlock.shvvalue.strlength = ulLen;
    RxVarBlock.shvvaluelen = ulLen;
    RxVarBlock.shvcode = RXSHV_SYSET;
    RxVarBlock.shvret = RXSHV_OK;

    /* set variable in pool */
    ulRetc = (*hRexxVariablePool)(&RxVarBlock);

    /* test return code */
    if (ulRetc == RXSHV_NEWV)
        ulRetc = RXSHV_OK;

    return (int)ulRetc;

}

static APIRET APIENTRY hSubcomHandler(
  PRXSTRING Command,                    /* Command string from Rexx           */
  PUSHORT Flags,                        /* Returned Error/Failure Flags       */
  PRXSTRING RetValue )                  /* Returned RC string                 */
{
#define MAX_OPTS 16

short rc;
char *line = NULL;
int   coun;
char *zCommand = NULL;
char *wCommand = NULL;
char *wResp = NULL;

int   iarg,argc;
char *argv[MAX_OPTS];

#if 0
int   haveecho;
int   echo;
#endif
int   havestem;
int   needname;
int   havename;

char *RespStemName;
int   RespStemLength;
char *RespStemOffset;

char temp[33];

    zCommand = malloc( RXSTRLEN( *Command) +1);
    strncpy( zCommand, RXSTRPTR( *Command), RXSTRLEN( *Command));
    zCommand[RXSTRLEN(*Command)] = '\0';
    parse_command( zCommand, MAX_OPTS, argv, &argc);

    if ( argc == 0 )
    {
        free(zCommand);
        sprintf(RetValue->strptr, "%d", -1);
        RetValue->strlength = strlen(RetValue->strptr);
        *Flags = RXSUBCOM_ERROR;
        return -1;
    }
#if 0
    haveecho = 0;
    echo = 0;
#endif
    havestem = 0;
    needname = 0;
    havename = 0;

    for (iarg = 1; iarg < argc; iarg++)
    {
        if ( !havename && havestem )
        {
            havename = 1;
            RespStemName = malloc(strlen(argv[iarg]) + 33);
            strcpy(RespStemName, argv[iarg]);
            RespStemLength = strlen(RespStemName);
            if  ( RespStemName[RespStemLength-1] != '.' )
            {
                RespStemName[RespStemLength]   = '.';
                RespStemName[RespStemLength+1] = '\0';
            }
            RespStemOffset = RespStemName + strlen(RespStemName);
            sprintf(RespStemOffset,"%d",0);
            sprintf(temp,"%d",0);
            ReginaRexxSetVar(RespStemName, temp, strlen(temp));
            continue;
        }

#if 0
        if ( !haveecho && ( strcasecmp(argv[iarg], "echo" ) == 0 ) )
        {
            haveecho = 1;
            echo = 1;
            continue;
        }
        if ( !haveecho && ( strcasecmp(argv[iarg], "noecho" ) == 0 ) )
        {
            haveecho = 1;
            echo = 0;
            continue;
        }
#endif
        if ( !havestem && ( strcasecmp(argv[iarg], "stem" ) == 0 ) )
        {
            havestem = 1;
            needname = 1;
            continue;
        }

        if ( havename )
        {
            free( RespStemName);
        }

        free( zCommand);
        sprintf( RetValue->strptr, "%d", -2);
        RetValue->strlength = strlen( RetValue->strptr);
        *Flags = RXSUBCOM_ERROR;
        return -2;

    }

    wCommand = NULL;
    wCommand = malloc(strlen(argv[0])+33);
    if ( wCommand )
    {
        strcpy(wCommand, argv[0]);
#if 0
        if ( echo )
        {
            wCommand[0] = '\0';
        }
        else
        {
            wCommand[0] = '-';
            wCommand[1] = '\0';
        }
        strcat( wCommand, argv[0] );
#endif

        if ( havename )
        {
            LOG_CAPTURE( rc, wResp, panel_command, wCommand);
            coun = 0;
            if (wResp )
            {
                for (coun=0, line=strtok(wResp, "\n"); line; line = strtok(NULL, "\n"))
                {
                    coun++;
                    sprintf(RespStemOffset,"%d",coun);
                    ReginaRexxSetVar(RespStemName, line, strlen(line));
                }
                free(wResp);
            }
            sprintf(RespStemOffset,"%d",0);
            sprintf(temp,"%d",coun);
            ReginaRexxSetVar(RespStemName, temp, strlen(temp));
            free(RespStemName);
            *Flags = RXSUBCOM_OK;

        }
        else
        {
            rc = HercCmdLine(wCommand);
            *Flags = rc < 0 ? RXSUBCOM_ERROR : rc > 0 ? RXSUBCOM_FAILURE : RXSUBCOM_OK;
        }
        free(wCommand);
        free(zCommand);
    }
    else
    {
        rc = -4;
        *Flags = RXAPI_MEMFAIL;
    }

    sprintf(RetValue->strptr, "%d", rc);
    RetValue->strlength = strlen(RetValue->strptr);

    return rc;
}

static LONG APIENTRY hExitHandler(
    LONG ExitNumber,                    /* code defining the exit function    */
    LONG SubFunction,                   /* code defining the exit SubFunction */
    PEXIT ParmBlock )                   /* function-dependent control block   */
{
    switch( ExitNumber )
    {
        case RXSIO:
            switch( SubFunction ) {
                case RXSIOSAY:
                    if (MessagePrefix)
                        logmsg("%-9s %s\n", MessagePrefix, RXSTRPTR(((RXSIOSAY_PARM *)ParmBlock)->rxsio_string));
                    else
                        logmsg("%s\n", RXSTRPTR(((RXSIOSAY_PARM *)ParmBlock)->rxsio_string));
                    return RXEXIT_HANDLED;
                    break;
                case RXSIOTRC:
                    if (MessagePrefix)
                        logmsg("%-9s %s\n", MessagePrefix, RXSTRPTR(((RXSIOTRC_PARM *)ParmBlock)->rxsio_string));
                    else
                        logmsg("%s\n", RXSTRPTR(((RXSIOTRC_PARM *)ParmBlock)->rxsio_string));
                    return RXEXIT_HANDLED;
                    break;
                case RXSIOTRD:
                    MAKERXSTRING(((RXSIOTRD_PARM *)ParmBlock)->rxsiotrd_retc, NULL, 0);
                    return RXEXIT_HANDLED;
                    break;
                case RXSIODTR:
                    MAKERXSTRING(((RXSIODTR_PARM *)ParmBlock)->rxsiodtr_retc, NULL, 0);
                    return RXEXIT_HANDLED;
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
    return RXEXIT_NOT_HANDLED;
}

int ReginaRexxDynamicLoader()
{
    HDLOPEN( hRexxLibHandle, REGINA_LIBRARY, RTLD_LAZY);

    HDLSYM ( hRexxStart, hRexxLibHandle, REXX_START);

    HDLSYM ( hRexxRegisterSubcom, hRexxLibHandle, REXX_REGISTER_SUBCOM);

    HDLSYM ( hRexxDeregisterSubcom, hRexxLibHandle, REXX_DEREGISTER_SUBCOM);

    HDLSYM ( hRexxRegisterExit, hRexxLibHandle, REXX_REGISTER_EXIT);

    HDLSYM ( hRexxDeregisterExit, hRexxLibHandle, REXX_DEREGISTER_EXIT);

    HDLSYM ( hRexxAllocateMemory, hRexxLibHandle, REXX_ALLOCATE_MEMORY);

    HDLSYM ( hRexxFreeMemory, hRexxLibHandle, REXX_FREE_MEMORY);

    HDLSYM ( hRexxFreeMemory, hRexxLibHandle, REXX_FREE_MEMORY);

    HDLSYM ( hRexxVariablePool, hRexxLibHandle, REXX_VARIABLE_POOL);

    return 0;
}

int ReginaRexxRegisterHandlers()
{
int rc=0;

    if (!hRexxLibHandle)
    {
        if ( (*RexxDynamicLoader)() != 0 )
           return -1;
    }

    rc = (*hRexxRegisterExit)( hSIOExit, (PFN)hExitHandler, NULL );
    if (rc != RXEXIT_OK && rc != RXEXIT_NOTREG)
    {
        WRMSG( HHC17534, "E", RexxPackage, "Exit Handler", rc);
        return -1;
    }

    rc = (*hRexxRegisterSubcom)( hSubcom, (PFN)hSubcomHandler, NULL );
    if (rc != RXSUBCOM_OK && rc != RXSUBCOM_NOTREG)
    {
        WRMSG( HHC17534, "E", RexxPackage, "Subcom Handler", rc);
        return -1;
    }
    return 0;

}

/*-------------------------------------------------------------------*/
/* exec Command - execute a rexx script ( regina rexx )              */
/*-------------------------------------------------------------------*/
int ReginaRexxExecCmd(char *Command, char *Args, int argc, char *argv[] )
{
int   rc;
short RetRC;
RXSTRING       wArgs;
RXSYSEXIT      ExitList[2];
RXSTRING       RetValue;

    UNREFERENCED(argc);

    if ( (*RexxRegisterHandlers)() != 0 )
        return -1;

    if ( Args )
    {
        MAKERXSTRING(wArgs, Args, strlen(Args));
    }
    else
    {
        MAKERXSTRING(wArgs, NULL, 0 );
    }

    ExitList[0].sysexit_name = hSIOExit;
    ExitList[0].sysexit_code = RXSIO;
    ExitList[1].sysexit_code = RXENDLST;

    RetValue.strptr = NULL;       /* initialize return-pointer to empty */
    RetValue.strlength = 0;       /* initialize return-length to zero   */

    rc= (*hRexxStart)((Args) ? 1 : 0 ,               /* number of arguments   */
                       &wArgs,                       /* array of arguments    */
                       Command,                      /* name of Rexx file     */
                       NULL,                         /* No Instore used       */
                       hSubcom,                      /* Command env. name     */
                       RXCOMMAND,                    /* Code for how invoked  */
                       ExitList,                     /* EXITs on this call    */
                       &RetRC,                       /* converted return code */
                       &RetValue);                   /* Rexx program output   */

    if ( rc !=  0 )
    {
        WRMSG( HHC17502, "E", RexxPackage, RexxPackage, rc);
    }
    else
    {
        if ( MessageLevel > 0)
        {
            WRMSG( HHC17503, "I", RexxPackage, argv[1], RetRC);
            if (RetValue.strptr != NULL)
            {
                WRMSG( HHC17504, "I", RexxPackage, argv[1], RetValue.strptr);
            }
        }
        if (RetValue.strptr != NULL)
        {
            (*hRexxFreeMemory)(RetValue.strptr);
        }

    }
    return rc;
}

/*-------------------------------------------------------------------*/
/* exec Command - execute an Instore rexx script ( regina rexx )     */
/*-------------------------------------------------------------------*/
int ReginaRexxExecInstoreCmd(char *Command, char *Args,
                            short RetRC, char *Result )
{
int rc;

RXSTRING       wArgs;
RXSTRING       Instore[2];
RXSYSEXIT      ExitList[2];
RXSTRING       RetValue;

    if ( (*RexxRegisterHandlers)() != 0 )
        return -1;

    if ( Args )
    {
        MAKERXSTRING(wArgs, Args, strlen(Args));
    }
    else
    {
        MAKERXSTRING(wArgs, NULL, 0 );
    }

    Instore[0].strptr=Command;
    Instore[0].strlength=strlen(Instore[0].strptr);
    Instore[1].strptr=NULL;
    Instore[1].strlength=0;

    ExitList[0].sysexit_name = hSIOExit;
    ExitList[0].sysexit_code = RXSIO;
    ExitList[1].sysexit_code = RXENDLST;

    RetValue.strptr=NULL;
    RetValue.strlength=0;

/*  regina rexx clumsily needs a rexx file name also for Instore scripts;
    the issue could be fixed with about five lines of code
    it would also fix a wrong message issued, but ...
    */
    rc= (*hRexxStart)((Args) ? 1 : 0 ,               /* number of arguments   */
                       &wArgs,                       /* array of arguments    */
                       "Instore",                    /* Fix Regina poor coding*/
                       Instore,                      /* Instore used          */
                       hSubcom,                      /* Command env. name     */
                       RXFUNCTION,                   /* Code for how invoked  */
                       ExitList,                     /* EXITs on this call    */
                       &RetRC,                       /* converted return code */
                       &RetValue);                   /* Rexx program output   */

    if ( rc !=  0 )
    {
        WRMSG( HHC17502, "E", RexxPackage, RexxPackage, rc);
        if (RetValue.strptr != NULL)
            (*hRexxFreeMemory)(RetValue.strptr);
    }
    else
    if (RetValue.strptr != NULL)
    {
        strcpy(Result,RetValue.strptr);
        (*hRexxFreeMemory)(RetValue.strptr);
    }
    return rc;

}
#endif /* defined(ENABLE_REGINA_REXX) */
#endif /* #ifndef _HREXX_R_C_  */

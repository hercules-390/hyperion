/* REXX.C       (c)Copyright Jan Jaeger, 2010                        */
/*              REXX Interpreter Support                             */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

#include "hstdinc.h"

#define _REXX_C_
#define _HENGINE_DLL_

#include "hercules.h"

#if defined(HAVE_REGINA_REXXSAA_H)

#define REXX_PACKAGE "Regina"

#define INCL_REXXSAA
#define RX_STRONGTYPING

#if defined( _MSVC_ )
#include "rexxsaa.h"
#else
#include <regina/rexxsaa.h>
#endif

#define hSubcom  "HERCULES"
#define hSIOExit "HERCSIOE"

static int rexx_initialised = FALSE;
static TID rexx_tid=0;
static char *pszCmdline = NULL;     /* save pointer to cmdline */

#if defined(OPTION_DYNAMIC_RESOLVE_REXX)

#if defined ( _MSVC_ )
#define REGINA_LIBRARY "regina.dll"
#else
#define REGINA_LIBRARY "libregina.so"
#endif

#define REXX_REGISTER_SUBCOM "RexxRegisterSubcomExe"
#define REXX_REGISTER_EXIT   "RexxRegisterExitExe"
#define REXX_START           "RexxStart"

typedef APIRET APIENTRY rRexxStart( LONG, PRXSTRING, PCSZ, PRXSTRING, PCSZ, LONG, PRXSYSEXIT, PSHORT, PRXSTRING ) ;
typedef APIRET APIENTRY rRexxRegisterSubcomExe( PCSZ, RexxSubcomHandler *, PUCHAR ) ; 
typedef APIRET APIENTRY rRexxRegisterExitExe( PSZ, RexxExitHandler *, PUCHAR ) ;

static rRexxRegisterSubcomExe *hRexxRegisterSubcomExe = NULL;
static rRexxStart             *hRexxStart = NULL;
static rRexxRegisterExitExe   *hRexxRegisterExitExe = NULL;

#else /*defined(OPTION_DYNAMIC_RESOLVE_REXX)*/

#define hRexxStart             RexxStart
#define hRexxRegisterSubcomExe RexxRegisterSubcomExe
#define hRexxRegisterExitExe   RexxRegisterExitExe  

#endif /*defined(OPTION_DYNAMIC_RESOLVE_REXX)*/

LONG APIENTRY hExitHnd( LONG ExitNumber, LONG Subfunction, PEXIT ParmBlock )
{
    switch( ExitNumber ) {

        case RXSIO:

            switch( Subfunction ) {

                case RXSIOSAY:
                    logmsg("%s\n",RXSTRPTR(((RXSIOSAY_PARM *)ParmBlock)->rxsio_string));
                    return RXEXIT_HANDLED;
                    break;
             
                case RXSIOTRC:
                    logmsg("%s\n",RXSTRPTR(((RXSIOTRC_PARM *)ParmBlock)->rxsio_string));
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


APIRET APIENTRY hSubCmd( PRXSTRING command, PUSHORT flags, PRXSTRING retval ) 
{
SHORT rc;

    rc = (sysblk.config_done) 
       ? ProcessPanelCommand(RXSTRPTR(*command))
       : ProcessConfigCmdLine(RXSTRPTR(*command));

    *flags = rc < 0 ? RXSUBCOM_ERROR : rc > 0 ? RXSUBCOM_FAILURE : RXSUBCOM_OK;

    snprintf(RXSTRPTR(*retval), RXSTRLEN(*retval), "%hd", rc);

#if defined(_MSVC_)
    RXSTRPTR(*retval)[ (RXSTRLEN(*retval)) - 1 ] = '\0';    /* VS does not protect overruns */
#endif

    MAKERXSTRING(*retval, RXSTRPTR(*retval), (ULONG)strlen(RXSTRPTR(*retval)));

    return 0;
}

int init_rexx()
{
int rc;

#if defined(OPTION_DYNAMIC_RESOLVE_REXX)
    if ( !rexx_initialised )
    {
        void *addr;

        if (!(addr = dlopen(REGINA_LIBRARY,RTLD_LAZY)))
        {
            WRMSG( HHC17504, "E", REXX_PACKAGE, REGINA_LIBRARY, dlerror());
            return -1;
        }

        if (!(hRexxRegisterSubcomExe = (rRexxRegisterSubcomExe *)dlsym(addr, REXX_REGISTER_SUBCOM )))
        {
            WRMSG( HHC17505, "E", REXX_PACKAGE, REXX_REGISTER_SUBCOM, dlerror());
            return -1;
        }

        if (!(hRexxRegisterExitExe = (rRexxRegisterExitExe *)dlsym( addr, REXX_REGISTER_EXIT )))
        {
            WRMSG( HHC17505, "E", REXX_PACKAGE, REXX_REGISTER_EXIT, dlerror());
            return -1;
        }

        if (!(hRexxStart = (rRexxStart *)dlsym(addr, REXX_START )))
        {
            WRMSG( HHC17505, "E", REXX_PACKAGE, REXX_START, dlerror());
            return -1;
        }

    }
#endif /*defined(OPTION_DYNAMIC_RESOLVE_REXX)*/

    if ((rc = hRexxRegisterExitExe( hSIOExit, (RexxExitHandler *)hExitHnd, NULL )) != RXEXIT_OK
      && !(rexx_initialised && rc == RXEXIT_NOTREG))
    {
        WRMSG( HHC17506, "E", REXX_PACKAGE, "Exit Handler", rc);
        return -1;
    }

    if ((rc = hRexxRegisterSubcomExe( hSubcom, (RexxSubcomHandler *)hSubCmd, NULL)) != RXSUBCOM_OK
      && !(rexx_initialised && rc == RXSUBCOM_NOTREG))
    {
        WRMSG( HHC17506, "E", REXX_PACKAGE, "Subcom Handler", rc);
        return -1;
    }

    rexx_initialised = TRUE;

    return 0;
}

/*-------------------------------------------------------------------*/
/* exec command - execute a rexx script                              */
/*-------------------------------------------------------------------*/
int exec_cmd(int argc, char *argv[], char *cmdline)
{
int rc;
SHORT ret;
RXSTRING retval;
char buffer[250];
char pathname[MAX_PATH];
RXSTRING arg;
RXSYSEXIT ExitList[2];

    UNREFERENCED(cmdline);

    if (argc < 2)
    {
        WRMSG( HHC17501, "E", REXX_PACKAGE );
        return -1;
    }

    if (init_rexx())
    {
        WRMSG( HHC17500, "E", REXX_PACKAGE );
        return -1;
    }

    hostpath( pathname, argv[1], sizeof(pathname) );

    if (access( pathname, R_OK ) != 0 && strcmp(basename(argv[1]),argv[1]) == 0 )
    {   /* try $(MODPATH)\rexx\filename if not found and no pathing information */
        char *p = malloc(sizeof(pathname)+1);
        if ( p != NULL )
        {
            bzero(p,sizeof(pathname)+1);
            strlcpy( p, get_symbol("MODPATH"), sizeof(pathname) );
            strlcat( p, PATHSEPS,              sizeof(pathname) );
            strlcat( p, "rexx",                sizeof(pathname) );
            strlcat( p, PATHSEPS,              sizeof(pathname) );
            strlcat( p, argv[1],               sizeof(pathname) );

            if (access( pathname, R_OK ) == 0)
                hostpath( pathname, p, sizeof(pathname));
        }
        else
        {
            char msgbuf[64];
            MSGBUF( msgbuf, "malloc(%lu)", sizeof(pathname)+1 );
            WRMSG(HHC02219, "E", msgbuf, strerror(errno) );
            return -1;
        }
    }

    if (argc > 2)
    {
        int i,len;

        for (len = 0, i = 2; i < argc; i++)
            len += (int)strlen(argv[i]) + 1;

        MAKERXSTRING(arg, malloc(len), len - 1);

        strlcpy( RXSTRPTR(arg), argv[2], len );
        for ( i = 3; i < argc; i++ )
        {
            strlcat( RXSTRPTR(arg), " ", len );
            strlcat( RXSTRPTR(arg), argv[i], len );
        }
    }
    else
        MAKERXSTRING( arg, NULL, 0 );
    
    MAKERXSTRING( retval, buffer, sizeof(buffer) );

    ExitList[0].sysexit_name = hSIOExit;
    ExitList[0].sysexit_code = RXSIO;
    ExitList[1].sysexit_code = RXENDLST;

    if ((rc = hRexxStart ((argc > 2) ? 1 : 0, &arg, pathname, NULL, hSubcom, RXCOMMAND, ExitList, &ret, &retval )))
        WRMSG( HHC17503, "E", REXX_PACKAGE, rc );
    else
        if (ret)
            WRMSG( HHC17502, "E", REXX_PACKAGE, RXSTRPTR(retval) );

    if (RXSTRPTR(arg))
        free(RXSTRPTR(arg));

    return rc ? rc : ret;
}

void *exec_process_thread( void *cmd)
{
int     cmd_argc;
char*   cmd_argv[MAX_ARGS];
int     rc;

    UNREFERENCED(cmd);

    /* Parse the command line into its individual arguments...
       Note: original command line now sprinkled with nulls */
    parse_args (pszCmdline, MAX_ARGS, cmd_argv, &cmd_argc);

    cmd_argv[0] = "exec";

    rc = exec_cmd(cmd_argc, cmd_argv, pszCmdline);
    rexx_tid = 0;
    WRMSG( HHC02264, "I", cmdargv[1] );
    free(pszCmdline);
    pszCmdline = NULL;
    return rc;
}

/*-------------------------------------------------------------------*/
/* execb command - REXX BACKGROUND                                   */
/*-------------------------------------------------------------------*/
int execb_cmd(int argc, char *argv[], char *cmdline)
{
    int rc;
    UNREFERENCED(argv);

    if (argc<2)
    {
        WRMSG( HHC02299, "E", argv[0] );
        return 1;
    }

    if (rexx_tid==0)
    {
        pszCmdline = strdup(cmdline);

        rc = create_thread(&rexx_tid,DETACHED,
                  exec_process_thread,NULL,"rexx processing");
        if (rc)
        {
            if (pszCmdline != NULL)
            {
                free(pszCmdline);
                pszCmdline = NULL;
            }
            WRMSG(HHC00102, "E", strerror(rc));
            rexx_tid = 0;
        }
    }
    else
    {
        WRMSG( HHC02258, "E", "rexx script" );
        return -1;
    }

    return 0;
}
#endif /*defined(HAVE_REGINA_REXXSAA_H)*/

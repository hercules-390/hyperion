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

#if defined( _MSVC_ )
#include "rexxsaa.h"
#else
#include <regina/rexxsaa.h>
#endif

#define hSubcom  "HERCULES"
#define hSIOExit "HERCSIOE"

static int rexx_initialised = FALSE;

#if !defined(NO_DYNAMIC_RESOLVE_REXX)

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

#else

#define hRexxStart             RexxStart
#define hRexxRegisterSubcomExe RexxRegisterSubcomExe
#define hRexxRegisterExitExe   RexxRegisterExitExe  

#endif

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
       ? ProcessPanelCommand(command[0].strptr)
       : ProcessConfigCmdLine(command[0].strptr);

    if( rc )
        *flags = rc < 0 ? RXSUBCOM_ERROR : RXSUBCOM_FAILURE;
    else
        *flags = RXSUBCOM_OK;

    sprintf(retval->strptr,"%hd",rc);
    retval->strlength = (ULONG)strlen(retval->strptr);

    return 0;
}

int init_rexx()
{
int rc;

#if !defined(NO_DYNAMIC_RESOLVE_REXX)
    if( !rexx_initialised )
    {
        void *addr;

        if(!(addr = dlopen(REGINA_LIBRARY,RTLD_LAZY)))
        {
            WRMSG( HHC17504, "E", REXX_PACKAGE, REGINA_LIBRARY, strerror(errno));
            return -1;
        }

        if(!(hRexxRegisterSubcomExe = (rRexxRegisterSubcomExe *)dlsym(addr, REXX_REGISTER_SUBCOM )))
        {
            WRMSG( HHC17505, "E", REXX_PACKAGE, REXX_REGISTER_SUBCOM, strerror(errno));
            return -1;
        }

        if(!(hRexxRegisterExitExe = (rRexxRegisterExitExe *)dlsym( addr, REXX_REGISTER_EXIT )))
        {
            WRMSG( HHC17505, "E", REXX_PACKAGE, REXX_REGISTER_EXIT, strerror(errno));
            return -1;
        }

        if(!(hRexxStart = (rRexxStart *)dlsym(addr, REXX_START )))
        {
            WRMSG( HHC17505, "E", REXX_PACKAGE, REXX_START, strerror(errno));
            return -1;
        }

    }
#endif

    if((rc = hRexxRegisterExitExe( hSIOExit, (RexxExitHandler *)hExitHnd, NULL )) != RXEXIT_OK
      && !(rexx_initialised && rc == RXEXIT_NOTREG))
    {
        WRMSG( HHC17506, "E", REXX_PACKAGE, REXX_REGISTER_EXIT, rc);
        return -1;
    }

    if((rc = hRexxRegisterSubcomExe( hSubcom, (RexxSubcomHandler *)hSubCmd, NULL)) != RXSUBCOM_OK
      && !(rexx_initialised && rc == RXSUBCOM_NOTREG))
    {
        WRMSG( HHC17506, "E", REXX_PACKAGE, REXX_REGISTER_SUBCOM, rc);
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

    if(argc < 2)
    {
        WRMSG( HHC17501, "E", REXX_PACKAGE );
        return -1;
    }

    if(init_rexx())
    {
        WRMSG( HHC17500, "E", REXX_PACKAGE );
        return -1;
    }

    hostpath(pathname, argv[1], sizeof(pathname));

    if ( argc > 2 )
    {
        int i,len;

        for (len = 0, i = 2; i < argc; i++ )
            len += (int)strlen( (char *)argv[i] ) + 1;

        arg.strptr = (char *)malloc( len );

        strcpy( arg.strptr, argv[2] );
        for ( i = 3; i < argc; i++ )
        {
            strcat( arg.strptr, " " );
            strcat( arg.strptr, argv[i] );
        }
        arg.strlength = len - 1;
    }
    else
        MAKERXSTRING(arg, NULL, 0);
    
    MAKERXSTRING(retval, buffer, sizeof(buffer));

    ExitList[0].sysexit_name = hSIOExit;
    ExitList[0].sysexit_code = RXSIO;
    ExitList[1].sysexit_code = RXENDLST;

    if((rc = hRexxStart ((argc > 2) ? 1 : 0, &arg, pathname, NULL, hSubcom, RXCOMMAND, ExitList, &ret, &retval )))
        WRMSG( HHC17503, "E", REXX_PACKAGE, rc );
    else
        if(ret)
            WRMSG( HHC17502, "E", REXX_PACKAGE, RXSTRPTR(retval) );

    if(RXSTRPTR(arg))
        free(RXSTRPTR(arg));

    return rc ? rc : ret;
}
#endif /*defined(HAVE_REGINA_REXXSAA_H)*/

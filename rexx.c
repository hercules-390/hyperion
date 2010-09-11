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

#define INCL_REXXSAA

#if defined( _MSVC_ )
#include "rexxsaa.h"
#else
#include <regina/rexxsaa.h>
#endif

typedef LONG APIENTRY RexxExitHandler( LONG, LONG, /* CONST */ PEXIT ) ;

#define hSubcom "HERCULES"

static int rexx_initialised = FALSE;

#if !defined(NO_DYNAMIC_RESOLVE_REXX)

#if defined ( _MSVC_ )
#define REGINA_LIBRARY "regina.dll"
#else
#define REGINA_LIBRARY "libregina.so"
#endif

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

LONG exit_handler( LONG ExitNumber, LONG Subfunction, PEXIT ParmBlock )
{
RXSIOSAY_PARM *sayparm;
RXSIOTRC_PARM *trcparm;

    switch( ExitNumber ) {
        case RXSIO:
            switch( Subfunction ) {
                case RXSIOSAY:
                    sayparm = (RXSIOSAY_PARM *)ParmBlock;
                    logmsg("%s\n",RXSTRPTR(sayparm->rxsio_string));
                    return RXEXIT_HANDLED;
                    break;
             
                case RXSIOTRC:
                    trcparm = (RXSIOTRC_PARM *)ParmBlock;
                    logmsg("%s\n",RXSTRPTR(trcparm->rxsio_string));
                    return RXEXIT_HANDLED;
                    break;

//  ZZFIXME:  Need to add RXSIO I/O Exit to handle trace and stack reads
//  RexxRegisterExitExe( hSubcom, hSubExit, NULL);
//  
                default:
                    break;
            }
            break;
        default:
            break;
    }
    return RXEXIT_NOT_HANDLED;
}


RexxSubcomHandler *hSubCmd( PRXSTRING command, PUSHORT flags, PRXSTRING retval ) 
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
    if( rexx_initialised )
        return 0;

#if !defined(NO_DYNAMIC_RESOLVE_REXX)
    {
        void *addr;

        if(!(addr = dlopen(REGINA_LIBRARY,RTLD_LAZY)))
            return -1;
        if(!(hRexxRegisterSubcomExe = (rRexxRegisterSubcomExe *)dlsym(addr, "RexxRegisterSubcomExe")))
            return -1;
        if(!(hRexxRegisterExitExe = (rRexxRegisterExitExe *)dlsym( addr, "RexxRegisterExitExe" )))
            return -1;
        if(!(hRexxStart = (rRexxStart *)dlsym(addr,"RexxStart")))
            return -1;
    }
#endif

    if(hRexxRegisterExitExe( "HERC_SIO", (RexxExitHandler *)exit_handler, NULL ))
        return -1;

    if(hRexxRegisterSubcomExe( hSubcom, (RexxSubcomHandler *)hSubCmd, NULL) != RXSUBCOM_OK)
        return -1;

    rexx_initialised = TRUE;

    return 0;
}

/*-------------------------------------------------------------------*/
/* exec command - execute a rexx script                              */
/*-------------------------------------------------------------------*/
int exec_cmd(int argc, char *argv[], char *cmdline)
{
    SHORT rc;
    RXSTRING retval;
    char buffer[250];
    RXSTRING arg;
    RXSYSEXIT Exits[2];

    UNREFERENCED(cmdline);

    if(argc < 2)
    {
        WRMSG( HHC17501, "E", "Regina" );
        return -1;
    }

    if(init_rexx())
    {
        WRMSG( HHC17500, "E", "Regina" );
        return -1;
    }

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

    Exits[0].sysexit_name = "HERC_SIO";
    Exits[0].sysexit_code = RXSIO;
    Exits[1].sysexit_code = RXENDLST;

    hRexxStart ((argc > 2) ? 1 : 0, &arg, argv[1], NULL, hSubcom, RXCOMMAND, Exits, &rc, &retval );

    if(rc)
    {
        WRMSG( HHC17502, "E", "Regina", RXSTRPTR(retval) );
    }

    if(RXSTRPTR(arg))
        free(RXSTRPTR(arg));

    return rc;
}
#endif /*defined(HAVE_REGINA_REXXSAA_H)*/

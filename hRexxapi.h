/* HREXXAPI.H   (c)Copyright Enrico Sorichetti, 2012                  */
/*              Rexx Interpreter Support                              */
/*                                                                    */
/*  Released under "The Q Public License Version 1"                   */
/*  (http://www.hercules-390.org/herclic.html) as modifications to    */
/*  Hercules.                                                         */

/*  inspired by the previous Rexx implementation by Jan Jaeger        */

#ifndef _HREXXAPI_H_
#define _HREXXAPI_H_

#define LOG_CAPTURE( _RETC_ , _RESP_, _FCNM_, _BUFF_) do { \
_RESP_ = log_capture( _FCNM_ , _BUFF_ ); \
_RETC_ = 0; \
} while (0)

/*--------------------------------------------------------------------*/
/* Fetch a Variable form the Rexx varable pool                        */
/*--------------------------------------------------------------------*/
int HREXXFETCHVAR (
   char *    pszVar,            /* Variable name                      */
   PRXSTRING prxVar)            /* Rexx variable contents             */
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
      if ( !pszTemp ) {
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

/*--------------------------------------------------------------------*/
/* Set/update a Variable in the Rexx variable pool                    */
/*--------------------------------------------------------------------*/
int HREXXSETVAR (
    char *        pszVar,       /* Variable name to be set            */
    char *        pValue,       /* Ptr to new value                   */
    size_t        ulLen)        /* Value length                       */
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

static int hSubcomError (
    int rc,
    int flag,
    unsigned short *Flags,
    PRXSTRING RetValue )
{
    sprintf( RetValue->strptr, "%d", rc );
    RetValue->strlength = strlen( RetValue->strptr );
    *Flags = flag;
    return rc;
}

/*--------------------------------------------------------------------*/
/* the core of the whole shebang, the subcommand handler              */
/*--------------------------------------------------------------------*/
static HREXXSUBCOMHANDLER_T hSubcomHandler (
    HREXXPSTRING Command,       /* Command string from Rexx           */
    unsigned short *Flags,      /* Returned Error/Failure Flags       */
    PRXSTRING RetValue )        /* Returned RC string                 */
{

short rc;
char *line = NULL;
int   coun;
char *zCommand = NULL;
char *wCommand = NULL;
char *wResp = NULL;

int   iarg,argc,argl;
char *argv[MAX_ARGS_TO_SUBCOMHANDLER + 1 ];

int   haveEchoFlag;
int   haveStemKeyw;
int   haveStemName;

int   EchoFlag = 0;
char *RespStemName = NULL;
int   RespStemLength = 0;
char *RespStemOffset = NULL;

char temp[WRK_AREA_SIZE];

    zCommand = malloc( RXSTRLEN( *Command ) + 1 );
    if ( !zCommand )
        return hSubcomError( -4, RXAPI_MEMFAIL, Flags, RetValue );

    strncpy( zCommand, RXSTRPTR( *Command ), RXSTRLEN( *Command ) );
    zCommand[RXSTRLEN(*Command)] = '\0';
    parse_command( zCommand, MAX_ARGS_TO_SUBCOMHANDLER, argv, &argc );

    if ( argc == 0 )
    {
        free( zCommand );
        return hSubcomError( -1, RXSUBCOM_ERROR, Flags, RetValue );
    }

    haveEchoFlag = 0;
    haveStemKeyw = 0;
    haveStemName = 0;

    for ( iarg = 1; iarg < argc; iarg++ )
    {
        argl = strlen( argv[iarg] );

        if ( !haveStemName && haveStemKeyw )
        {
            haveStemName = 1;
            RespStemName = malloc( strlen(argv[iarg]) + WRK_AREA_SIZE );
            if ( !RespStemName )
                return hSubcomError( -4, RXAPI_MEMFAIL, Flags, RetValue );

            strcpy( RespStemName, argv[iarg] );
            RespStemLength = strlen( RespStemName );
            if  ( RespStemName[RespStemLength-1] != '.' )
            {
                RespStemName[RespStemLength]   = '.';
                RespStemName[RespStemLength+1] = '\0';
            }
            RespStemOffset = RespStemName + strlen( RespStemName );
            sprintf(RespStemOffset,"%d",0);
            sprintf(temp,"%d",0);
            HREXXSETVAR( RespStemName, temp, strlen(temp) );
            continue;
        }

        if ( !haveEchoFlag && HAVKEYW( "echo" ) )
        {
            haveEchoFlag = 1;
            EchoFlag = 1 ;
            continue;
        }
        if ( !haveEchoFlag && HAVKEYW( "noecho" ) )
        {
            haveEchoFlag = 1;
            EchoFlag = 0 ;
            continue;
        }

        if ( !haveStemKeyw && HAVKEYW( "stem" ) )
        {
            haveStemKeyw = 1;
            continue;
        }

        if ( RespStemName )
            free( RespStemName );
        free( zCommand );
        return hSubcomError( -2, RXSUBCOM_ERROR, Flags, RetValue );

    }
    if ( haveStemKeyw && !haveStemName )
    {
        if ( zCommand )
            free( zCommand );
        return hSubcomError( -2, RXSUBCOM_ERROR, Flags, RetValue );
    }

    wCommand = NULL;
    wCommand = malloc( strlen(argv[0]) + WRK_AREA_SIZE );
    if ( wCommand )
    {
        if ( haveStemName )
        {
            if ( EchoFlag )
                strcpy( wCommand, argv[0] );
            else
            {
                strcpy( wCommand, "-" );
                strcat( wCommand, argv[0] );
            }
            LOG_CAPTURE( rc, wResp, panel_command, wCommand);
//            rc = command_capture( HercCmdLine, wCommand, &wResp );
            coun = 0;
            if ( wResp )
            {
                for ( coun=0, line=strtok(wResp, "\n" ); line; line = strtok(NULL, "\n") )
                {
                    coun++;
                    sprintf( RespStemOffset, "%d", coun );
                    HREXXSETVAR( RespStemName, line, strlen(line) );
                }
                free( wResp );
            }
            sprintf( RespStemOffset, "%d", 0 );
            sprintf( temp, "%d", coun );
            HREXXSETVAR( RespStemName, temp, strlen(temp) );
            free( RespStemName );
            *Flags = rc < 0 ? RXSUBCOM_ERROR : rc > 0 ? RXSUBCOM_FAILURE : RXSUBCOM_OK;
        }
        else
        {
            strcpy( wCommand, argv[0] );
            rc = HercCmdLine( wCommand );
            *Flags = rc < 0 ? RXSUBCOM_ERROR : rc > 0 ? RXSUBCOM_FAILURE : RXSUBCOM_OK;
        }
        free( wCommand );
        free( zCommand );
    }
    else
    {
        free( zCommand );
        rc = -4;
        *Flags = RXAPI_MEMFAIL;
    }

    sprintf( RetValue->strptr, "%d", rc );
    RetValue->strlength = strlen( RetValue->strptr );

    return rc;
}

/*--------------------------------------------------------------------*/
/* the core of the whole shebang, the exit handler                    */
/*--------------------------------------------------------------------*/
static HREXXEXITHANDLER_T hExitHandler(
    HREXXLONG ExitNumber,       /* code defining the exit function    */
    HREXXLONG SubFunction,      /* code defining the exit SubFunction */
    PEXIT ParmBlock )           /* function-dependent control block   */
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

/*--------------------------------------------------------------------*/
/*                                                                    */
/*--------------------------------------------------------------------*/
int HREXXREGISTERHANDLERS()
{
int rc=0;

    if ( !hRexxLibHandle )
    {
        if ( (*RexxDynamicLoader)() != 0 )
           return -1;
    }

    rc = (*hRexxRegisterExit)( hSIOExit, (HREXXPFN)hExitHandler, NULL );
    if (rc != RXEXIT_OK && rc != RXEXIT_NOTREG)
    {
        WRMSG( HHC17534, "E", RexxPackage, "Exit Handler", rc);
        return -1;
    }

    rc = (*hRexxRegisterSubcom)( hSubcom, (HREXXPFN)hSubcomHandler, NULL );
    if (rc != RXSUBCOM_OK && rc != RXSUBCOM_NOTREG)
    {
        WRMSG( HHC17534, "E", RexxPackage, "Subcom Handler", rc);
        return -1;
    }
    return 0;

}

/*--------------------------------------------------------------------*/
/* exec Command - execute a rexx script                               */
/*--------------------------------------------------------------------*/
int HREXXEXECCMD (
    char *Command,
    char *args,
    int   argc,
    char *argv[] )
{
int   rc;
short RetRC;
HREXXSTRING    wArgs;
RXSYSEXIT      ExitList[2];
RXSTRING       RetValue;

    UNREFERENCED(argc);

    if ( (*RexxRegisterHandlers)() != 0 )
        return -1;

    if ( args )
    {
        MAKERXSTRING( wArgs, args, strlen(args) );
    }
    else
    {
        MAKERXSTRING( wArgs, NULL, 0 );
    }

    ExitList[0].sysexit_name = hSIOExit;
    ExitList[0].sysexit_code = RXSIO;
    ExitList[1].sysexit_code = RXENDLST;

    RetValue.strptr = NULL;       /* initialize return-pointer to empty */
    RetValue.strlength = 0;       /* initialize return-length to zero   */

    rc= (*hRexxStart)((args) ? 1 : 0 ,               /* number of arguments   */
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
        if ( MessageLevel > 0 )
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
/* exec Command - execute an Instore rexx script                     */
/*-------------------------------------------------------------------*/
int HREXXEXECINSTORECMD (
    char *Command,
    char *args,
    short RetRC,
    char *Result )
{
int rc;

HREXXSTRING    wArgs;
RXSTRING       Instore[2];
RXSYSEXIT      ExitList[2];
RXSTRING       RetValue;

    if ( (*RexxRegisterHandlers)() != 0 )
        return -1;

    if ( args )
    {
        MAKERXSTRING( wArgs, args, strlen(args) );
    }
    else
    {
        MAKERXSTRING( wArgs, NULL, 0 );
    }

    Instore[0].strptr=Command;
    Instore[0].strlength=strlen( Instore[0].strptr );
    Instore[1].strptr=NULL;
    Instore[1].strlength=0;

    ExitList[0].sysexit_name = hSIOExit;
    ExitList[0].sysexit_code = RXSIO;
    ExitList[1].sysexit_code = RXENDLST;

    RetValue.strptr=NULL;
    RetValue.strlength=0;

/*  object rexx note
    the file name is used as an eye catcher even if none needed
    for source look alike with regina rexx;

    regina rexx note
    clumsily needs a rexx file name also for Instore scripts;
    the issue could be fixed with about five lines of code
    it would also fix a wrong message issued, but ...
    */
    rc= (*hRexxStart)((args) ? 1 : 0 ,               /* number of arguments   */
                       &wArgs,                       /* array of arguments    */
                       "Instore",                    /* placeholder, see note */
                       Instore,                      /* Instore used          */
                       hSubcom,                      /* Command env. name     */
                       RXFUNCTION,                   /* Code for how invoked  */
                       ExitList,                     /* EXITs on this call    */
                       &RetRC,                       /* converted return code */
                       &RetValue);                   /* Rexx program output   */

    if ( rc !=  0 )
    {
        WRMSG( HHC17502, "E", RexxPackage, RexxPackage, rc );
        if (RetValue.strptr != NULL)
            (*hRexxFreeMemory)( RetValue.strptr );
    }
    else
    if (RetValue.strptr != NULL)
    {
        strcpy( Result, RetValue.strptr );
        (*hRexxFreeMemory)( RetValue.strptr );
    }
    return rc;

}

/*-------------------------------------------------------------------*/
/* exec Command - execute a rexx script AS SUBROUTINE                */
/*-------------------------------------------------------------------*/
int HREXXEXECSUB (
    char *Command,
    char *args,
    int   argc,
    char *argv[] )
{
int   iarg ;
int   rc;
short RetRC;
long  ArgCount;

HREXXSTRING    wArgs[MAX_ARGS_TO_REXXSTART + 1];
RXSYSEXIT      ExitList[2];
RXSTRING       RetValue;

    UNREFERENCED( args );
    UNREFERENCED( argc );

    if ( (*RexxRegisterHandlers)() != 0 )
        return -1;

    for (ArgCount=0, iarg=2;
         ArgCount < MAX_ARGS_TO_REXXSTART && iarg < argc;
         ArgCount++, iarg++)
        MAKERXSTRING( wArgs[ArgCount], argv[iarg], strlen( argv[iarg] ) );

    ExitList[0].sysexit_name = hSIOExit;
    ExitList[0].sysexit_code = RXSIO;
    ExitList[1].sysexit_code = RXENDLST;

    RetValue.strptr = NULL;       /* initialize return-pointer to empty */
    RetValue.strlength = 0;       /* initialize return-length to zero   */

    rc= (*hRexxStart)( ArgCount,                     /* number of arguments   */
                       wArgs,                        /* array of arguments    */
                       Command,                      /* name of Rexx file     */
                       NULL,                         /* No Instore used       */
                       hSubcom,                      /* Command env. name     */
                       RXSUBROUTINE,                 /* Code for how invoked  */
                       ExitList,                     /* EXITs on this call    */
                       &RetRC,                       /* converted return code */
                       &RetValue);                   /* Rexx program output   */

    if ( rc !=  0 )
    {
        WRMSG( HHC17502, "E", RexxPackage, RexxPackage, rc );
    }
    else
    {
        if ( MessageLevel > 0)
        {
            WRMSG( HHC17503, "I", RexxPackage, argv[1], RetRC );
            if (RetValue.strptr != NULL)
            {
                WRMSG( HHC17504, "I", RexxPackage, argv[1], RetValue.strptr );
            }
        }
        if (RetValue.strptr != NULL )
        {
            (*hRexxFreeMemory)( RetValue.strptr );
        }

    }
    return rc;
}

#endif /* #ifndef _HREXXAPI_H_  */

/* HREXX.C      (c)Copyright Enrico Sorichetti, 2012                 */
/*              REXX Interpreter Support                             */
/*                                                                   */
/*  Released under "The Q Public License Version 1"                  */
/*  (http://www.hercules-390.org/herclic.html) as modifications to   */
/*  Hercules.                                                        */

/*  inspired by the previous Rexx implementation by Jan Jaeger       */

#include "hstdinc.h"
#define _HENGINE_DLL_
#include "hercules.h"

#ifndef _HREXX_C_
#define _HREXX_C_

#define  CMPARG( _VALUE_ )  strcasecmp( _VALUE_, argv[iarg])
#define CMPARGL( _VALUE_ ) strncasecmp( _VALUE_, argv[iarg], argl)


#if defined(ENABLE_OBJECT_REXX) || defined(ENABLE_REGINA_REXX)

#if defined(ENABLE_OBJECT_REXX) && defined(ENABLE_REGINA_REXX)

#define REXX_TRY_OOREXX( _FOUND_ ) do \
{ \
    RexxPackage = OOREXX_PACKAGE; \
    RexxDynamicLoader = &ObjectRexxDynamicLoader; \
    RexxRegisterHandlers = &ObjectRexxRegisterHandlers; \
    RexxExecCmd = &ObjectRexxExecCmd; \
    RexxExecInstoreCmd = &ObjectRexxExecInstoreCmd; \
    rc = (*RexxDynamicLoader)(); \
    if ( rc == 0 ) \
    { \
        RexxStatus = _WAITING_ ;\
        WRMSG( HHC17525, "I", RexxPackage); \
        goto _FOUND_ ; \
    } \
} while(0) ;

#define REXX_TRY_REGINA( _FOUND_ ) do \
{ \
    RexxPackage = REGINA_PACKAGE; \
    RexxDynamicLoader = &ReginaRexxDynamicLoader; \
    RexxRegisterHandlers = &ReginaRexxRegisterHandlers; \
    RexxExecCmd = &ReginaRexxExecCmd; \
    RexxExecInstoreCmd = &ReginaRexxExecInstoreCmd; \
    rc = (*RexxDynamicLoader)(); \
    if ( rc == 0 ) \
    { \
        RexxStatus = _WAITING_ ;\
        WRMSG( HHC17525, "I", RexxPackage); \
        goto _FOUND_ ; \
    } \
} while(0) ;

#define SETREXX_RESET() do \
{ \
    RexxPackage = ""; \
    RexxDynamicLoader = NULL; \
    RexxRegisterHandlers = NULL; \
    RexxExecCmd = NULL; \
    RexxExecInstoreCmd = NULL; \
    RexxStatus = _STOPPED_ ; \
} while (0) ;

#endif /* defined(ENABLE_OBJECT_REXX) && defined(ENABLE_REGINA_REXX) */

#include "hRexx.h"

// forward references
int  exec_cmd(int, char  **, char* );
int  exec_instore_cmd(char *, char *);
void InitializePaths(char *);
void InitializeExtensions(char *);
void InitializeOptions(void);

#if defined(ENABLE_OBJECT_REXX)
int ObjectRexxDynamicLoader();
int ObjectRexxRegisterHandlers();
int ObjectRexxExecCmd();
int ObjectRexxExecInstoreCmd();
#endif
#if defined(ENABLE_REGINA_REXX)
int ReginaRexxDynamicLoader();
int ReginaRexxRegisterHandlers();
int ReginaRexxExecCmd();
int ReginaRexxExecInstoreCmd();
#endif

void *hRexxLibHandle = NULL;       /* Library handle */
void *hRexxApiLibHandle = NULL;    /* Api Library handle ooRexx*/

#if defined(ENABLE_OBJECT_REXX)  && !defined(ENABLE_REGINA_REXX)
char *RexxPackage = OOREXX_PACKAGE;
char *RexxLibrary = OOREXX_LIBRARY;
char *RexxApiLibrary = OOREXX_API_LIBRARY;
int (*RexxDynamicLoader)() = &ObjectRexxDynamicLoader;
int (*RexxRegisterHandlers)() = &ObjectRexxRegisterHandlers;
int (*RexxExecCmd)() = &ObjectRexxExecCmd;
int (*RexxExecInstoreCmd)() = &ObjectRexxExecInstoreCmd;

#elif !defined(ENABLE_OBJECT_REXX) &&  defined(ENABLE_REGINA_REXX)
char *RexxPackage = REGINA_PACKAGE;
char *RexxLibrary = REGINA_LIBRARY;
char *RexxApiLibrary = "";
int (*RexxDynamicLoader)() = &ReginaRexxDynamicLoader;
int (*RexxRegisterHandlers)() = &ReginaRexxRegisterHandlers;
int (*RexxExecCmd)() = &ReginaRexxExecCmd;
int (*RexxExecInstoreCmd)() = &ReginaRexxExecInstoreCmd;

#else
char *RexxPackage = "";
char *RexxLibrary = "";
char *RexxApiLibrary = "";
int (*RexxDynamicLoader)() = NULL;
int (*RexxRegisterHandlers)() = NULL;
int (*RexxExecCmd)() = NULL;
int (*RexxExecInstoreCmd)() = NULL;

#endif

int   PathsInitialized = FALSE;
int   useRexxPath = TRUE;
char *RexxPath = NULL;
char *RexxPathArray[32];
int   RexxPathCount = 0;
int   useSysPath = TRUE;
char *SysPath = NULL;
char *SysPathArray[32];
int   SysPathCount = 0;

int   ExtensionsInitialized = FALSE;
char *Extensions = NULL;
char *ExtensionsArray[32];
int   ExtensionsCount = 0;

int   OptionsInitialized = TRUE;
int   useResolver = TRUE;
int   MessageLevel = 0;
char *MessagePrefix = NULL;
char *ErrorPrefix = NULL;

#define _WAITING_ 0
#define _STOPPED_ 1
int   RexxStatus = _WAITING_ ;

/*-------------------------------------------------------------------*/
/* rexx Command - manage the Rexx environment                        */
/*-------------------------------------------------------------------*/
int rexx_cmd(int argc, char *argv[], char *CommandLine)
{
char *ARGSDESC[] = {"", "Start/Enable", "Path", "SysPath", "Extensions", "Resolver", "MsgLevel", "MsgPrefix", "ErrPrefix"};
char *PACKAGES[] = {"", "ooRexx", "Regina" };

enum
{
    _NEEDPACKAGE = 1 ,
    _NEEDPATH ,
    _NEEDSYSPATH ,
    _NEEDEXTNS ,
    _NEEDRESOLVER ,
    _NEEDMSGLEVL ,
    _NEEDMSGPREF ,
    _NEEDERRPREF ,
    _NEEDz
} ;

int   i;
int   rc = 0;
short RetRC = 0;
char  Result[1024];
char *ptr,*nxt;
char temp[33];
int  iarg,argl;

int  whatValue = 0;

int  haveStop = FALSE;
int  havePath = FALSE;
int  haveSysPath = FALSE;
int  haveExtns = FALSE;
int  haveResolver = FALSE;
int  haveMsgLevl = FALSE;
int  haveMsgPref = FALSE;
int  haveErrPref = FALSE;
int  haveStart = FALSE;

char *wPath = NULL;
int   wSysPath = TRUE;
char *wExtns = NULL;
int   wResolver = TRUE;
int   wMsgLevl = 0;
char *wMsgPref = NULL;
char *wErrPref = NULL;
char *wPackage = NULL;

    UNREFERENCED(argc);
    UNREFERENCED(argv);
    UNREFERENCED(CommandLine);

    UNREFERENCED(haveStop);
    UNREFERENCED(haveStart);
    UNREFERENCED(*PACKAGES);

    UNREFERENCED(rc);
    UNREFERENCED(RetRC);

    if ( argc >= 2 )
    for (iarg=1; iarg<argc; iarg++)
    {
        argl = strlen(argv[iarg]);

        if ( whatValue > 0 )
        {
            switch ( whatValue )
            {
                case _NEEDPACKAGE :
                {
                    if ( CMPARG( REGINA_PACKAGE ) == 0 || CMPARG( OOREXX_PACKAGE ) == 0 )
                        wPackage = strdup(argv[iarg]);
                    else
                    {
                        WRMSG( HHC17509, "W", RexxPackage, argv[iarg], ARGSDESC[_NEEDPACKAGE]);
                        return -1;
                    }
                }
                break;

                case _NEEDPATH :
                {
                    wPath = strdup(argv[iarg]);
                }
                break;

                case _NEEDSYSPATH :
                {
                    if ( CMPARG( "reset" ) == 0 )
                    {
                        wSysPath = TRUE;
                    }
                    else if ( CMPARG( "enable" ) == 0 || CMPARG( "on" ) == 0 )
                    {
                        wSysPath = TRUE;
                    }
                    else if ( CMPARG( "disable" ) == 0 || CMPARG( "off" ) == 0 )
                    {
                        wSysPath = FALSE;
                    }
                    else
                    {
                        WRMSG( HHC17510, "W", RexxPackage, argv[iarg], strlen(argv[iarg]), ARGSDESC[_NEEDSYSPATH]);
                        return -1;
                    }
                }
                break;

                case _NEEDEXTNS :
                {
                    wExtns = strdup(argv[iarg]);
                }
                break;

                case _NEEDRESOLVER :
                {
                    if ( CMPARG( "reset" ) == 0 )
                    {
                        wResolver = TRUE;
                    }
                    else if ( CMPARG( "enable" ) == 0 || CMPARG( "on" ) == 0 )
                    {
                        wResolver = TRUE;
                    }
                    else if ( CMPARG( "disable" ) == 0 || CMPARG( "off" ) == 0 )
                    {
                        wResolver = FALSE;
                    }
                    else
                    {
                        WRMSG( HHC17510, "W", RexxPackage, argv[iarg], strlen(argv[iarg]), ARGSDESC[_NEEDRESOLVER]);
                        return -1;
                    }
                }
                break;

                case _NEEDMSGLEVL :
                {
                    if ( CMPARG( "reset" ) == 0 )
                    {
                        wMsgLevl = 0;
                    }
                    else
                    {
                        ptr = argv[iarg];
                        errno = 0;
                        wMsgLevl = (int) strtoul(argv[iarg],&nxt,10);
                        if (errno != 0 || nxt == ptr || *nxt != 0 || ( wMsgLevl < 0 || wMsgLevl > 9 ) )
                        {
                            WRMSG( HHC17510, "W", RexxPackage, argv[iarg], strlen(argv[iarg]), ARGSDESC[_NEEDMSGLEVL]);
                            return -1;
                        }
                    }
                }
                break;

                case _NEEDMSGPREF :
                {
                    if ( strlen(argv[iarg]) > 9 )
                    {
                        WRMSG( HHC17510, "W", RexxPackage, argv[iarg], strlen(argv[iarg]), ARGSDESC[_NEEDMSGPREF]);
                        return -1;
                    }
                    wMsgPref = strdup(argv[iarg]);
                }
                break;

                case _NEEDERRPREF :
                {
                    if ( strlen(argv[iarg]) > 9 )
                    {
                        WRMSG( HHC17510, "W", RexxPackage, argv[iarg], strlen(argv[iarg]), ARGSDESC[_NEEDERRPREF]);
                        return -1;
                    }
                    wErrPref = strdup(argv[iarg]);
                }
                break;

            }
            whatValue = 0;
            continue;
        }

        if ( !haveStop &&
             ( ( argl >= 3 && argl <= 4 && CMPARGL( "disable" ) == 0 ) ||
               ( argl >= 3 && argl <= 7 && CMPARGL( "stop" ) == 0 ) ))
        {
#if defined(ENABLE_OBJECT_REXX) && defined(ENABLE_REGINA_REXX)
            haveStop = TRUE;
            continue;
#else  /* defined(ENABLE_OBJECT_REXX) && defined(ENABLE_REGINA_REXX) */
            WRMSG( HHC17524, "I", RexxPackage);
            return 0;
#endif /* defined(ENABLE_OBJECT_REXX) && defined(ENABLE_REGINA_REXX) */
        }

        if ( !havePath &&
           ( ( argl >= 4 && argl <=  5 && CMPARGL( "paths" ) == 0 ) ||
             ( argl >= 5 && argl <=  9 && CMPARGL( "rexxpaths" ) == 0 ) ) )
        {
            havePath = TRUE;
            whatValue = _NEEDPATH;
            continue;
        }

        if ( !haveSysPath &&
             ( argl >= 4 && argl <=  7 && CMPARGL( "syspath" ) == 0 ) )
        {
            haveSysPath = TRUE;
            whatValue = _NEEDSYSPATH;
            continue;
        }

        if ( !haveExtns &&
           ( ( argl >= 3 && argl <= 10 && CMPARGL( "extensions" ) == 0 ) ||
             ( argl >= 3 && argl <=  8 && CMPARGL( "suffixes" ) == 0 ) ) )
        {
            haveExtns = TRUE;
            whatValue = _NEEDEXTNS;
            continue;
        }

        if ( !haveResolver &&
             ( argl >= 5 && argl <=  8 && CMPARGL( "resolver" ) == 0 ) )
        {
            haveResolver = TRUE;
            whatValue = _NEEDRESOLVER;
            continue;
        }

        if ( !haveMsgLevl &&
             ( argl >= 4 && argl <=  8 && CMPARGL( "msglevel" ) == 0 ) )
        {
            haveMsgLevl = TRUE;
            whatValue = _NEEDMSGLEVL;
            continue;
        }

        if ( !haveMsgPref &&
             ( argl >= 4 && argl <=  9 && CMPARGL( "msgprefix" ) == 0 ) )
        {
            haveMsgPref = TRUE;
            whatValue = _NEEDMSGPREF;
            continue;
        }

        if ( !haveErrPref &&
             ( argl >= 4 && argl <=  9 && CMPARGL( "errprefix" ) == 0 ) )
        {
            haveErrPref = TRUE;
            whatValue = _NEEDERRPREF;
            continue;
        }

        if ( !haveStart &&
           ( ( argl >= 3 && argl <=  5 && CMPARGL( "enable" ) == 0 ) ||
             ( argl >= 3 && argl <=  6 && CMPARGL( "start" ) == 0 ) ) )
        {
#if defined(ENABLE_OBJECT_REXX) && defined(ENABLE_REGINA_REXX)
            if ( hRexxLibHandle )
            {
                WRMSG( HHC17522, "I", RexxPackage);
                return -1;
            }
            haveStart= TRUE;
            whatValue = _NEEDPACKAGE;
            continue;
#else /* defined(ENABLE_OBJECT_REXX) && defined(ENABLE_REGINA_REXX) */
            WRMSG( HHC17523, "I", RexxPackage);
            return 0;
#endif /* defined(ENABLE_OBJECT_REXX) && defined(ENABLE_REGINA_REXX) */
        }

        WRMSG( HHC17508, "W", RexxPackage, argv[iarg], iarg + 1 );
        return 0;
    }

    if ( whatValue == _NEEDPACKAGE )
    {
        wPackage = "auto";
    }
    else if ( whatValue > 0 )
    {
        WRMSG( HHC17507, "W", RexxPackage, ARGSDESC[whatValue] );
        return -1;
    }

#if defined(ENABLE_OBJECT_REXX) && defined(ENABLE_REGINA_REXX)
    if ( haveStop )
    {
        if  ( hRexxLibHandle )
            HDLCLOSE( hRexxLibHandle);

        if  ( hRexxApiLibHandle )
            HDLCLOSE( hRexxApiLibHandle);

        RexxStatus = _STOPPED_ ;

        SETREXX_RESET()

        WRMSG( HHC17526, "I", RexxPackage);

    }
#endif /* defined(ENABLE_OBJECT_REXX) && defined(ENABLE_REGINA_REXX) */

    if ( havePath )
    {
        if ( strcasecmp( "reset", wPath) == 0 )
        {
            if (wPath)
            {
                free(wPath);
                wPath = NULL;
            }
            if (RexxPath)
            {
                free(RexxPath);
                RexxPath = NULL;
            }
            InitializePaths(NULL);
        }
        else
        {
            InitializePaths(wPath);
        }
    }

    if ( haveSysPath )
    {
        useSysPath = wSysPath;
    }

    if ( haveExtns )
    {
        if ( strcasecmp( "reset", wExtns) == 0 )
            InitializeExtensions(NULL);
        else
            InitializeExtensions(wExtns);
    }

    if ( haveResolver )
    {
        useResolver = wResolver;
    }

    if ( haveMsgLevl )
    {
        MessageLevel = wMsgLevl;
    }

    if ( haveMsgPref )
    {
        if ( strcasecmp( "off", wMsgPref) == 0 || strcasecmp( "reset", wMsgPref) == 0 )
        {
            if ( MessagePrefix )
            {
                free(MessagePrefix);
                MessagePrefix = NULL;
            }
        }
        else
        {
            MessagePrefix = strdup(wMsgPref);
        }
        free(wMsgPref);
    }

    if ( haveErrPref )
    {
        if ( strcasecmp( "off", wErrPref) == 0 || strcasecmp( "reset", wErrPref) == 0 )
        {
            if ( ErrorPrefix )
            {
                free(ErrorPrefix);
                ErrorPrefix = NULL;
            }
        }
        else
        {
            ErrorPrefix = strdup(wErrPref);
        }
        free(wErrPref);
    }

#if defined(ENABLE_OBJECT_REXX) && defined(ENABLE_REGINA_REXX)
    if ( haveStart )
    {
        if ( strcasecmp(wPackage, "auto" ) == 0  )
        {
            char *envvar;

            if ( !( envvar = getenv("HREXX_PACKAGE") ) )
            {
                REXX_TRY_OOREXX( Start_Rexx_Loaded )
                REXX_TRY_REGINA( Start_Rexx_Loaded )
                SETREXX_RESET()
                RexxStatus = _STOPPED_ ;
                WRMSG( HHC17526, "I", RexxPackage);
            }
            if ( strcasecmp(envvar, "auto" ) == 0 )
            {
                REXX_TRY_OOREXX( Start_Rexx_Loaded )
                REXX_TRY_REGINA( Start_Rexx_Loaded )
                SETREXX_RESET()
                RexxStatus = _STOPPED_ ;
                WRMSG( HHC17526, "I", RexxPackage);
            }

            if ( strcasecmp(envvar, "none" ) == 0 )
            {
                WRMSG( HHC17521, "I", "");
                return 0;
            }

            wPackage = envvar ;

        }

        if ( strcasecmp(wPackage, OOREXX_PACKAGE ) == 0  )
        {
            REXX_TRY_OOREXX( Start_Rexx_Loaded )
            SETREXX_RESET()
            RexxStatus = _STOPPED_ ;
            WRMSG( HHC17526, "I", RexxPackage);
            return -1 ;
        }
        if ( strcasecmp(wPackage, REGINA_PACKAGE ) == 0  )
        {
            REXX_TRY_REGINA( Start_Rexx_Loaded )
            SETREXX_RESET()
            RexxStatus = _STOPPED_ ;
            WRMSG( HHC17526, "I", RexxPackage);
            return -1 ;
        }

        WRMSG( HHC17501, "E", wPackage, "Oh shit, I should not be here");
        return -1;

    }
Start_Rexx_Loaded:
#endif /* defined(ENABLE_OBJECT_REXX) && defined(ENABLE_REGINA_REXX) */

    if ( !PathsInitialized )
        InitializePaths(NULL);
    strcpy(Result, "Rexx Path : " );
    for (i=0; i<RexxPathCount; i++)
    {
        strcat(Result,RexxPathArray[i]);
        strcat(Result,i == RexxPathCount-1 ? "" : PATHDELIM );
    }
    WRMSG( HHC17500, "I", RexxPackage,Result);

    strcpy(Result, "Sys Path  : ");
    strcat(Result, useSysPath ? "(ON)" : "(OFF)" );
    WRMSG( HHC17500, "I", RexxPackage,Result);

    if ( !ExtensionsInitialized )
        InitializeExtensions(NULL);
    strcpy(Result, "Extensions: ");
    for (i=0; i<ExtensionsCount; i++)
    {
        strcat(Result, ExtensionsArray[i]);
        strcat(Result, i == ExtensionsCount-1 ? "" : EXTNDELIM );
    }
    WRMSG( HHC17500, "I", RexxPackage,Result);

    strcpy(Result, "Resolver  : ");
    strcat(Result, useResolver ? "(ON)" : "(OFF)" );
    WRMSG( HHC17500, "I", RexxPackage,Result);

    strcpy(Result, "Msg Level : ");
    sprintf(temp,"%d",MessageLevel);
    strcat(Result,temp);
    WRMSG( HHC17500, "I", RexxPackage,Result);

    strcpy(Result, "Msg Prefix: ");
    strcat(Result, MessagePrefix ? MessagePrefix : "(OFF)" );
        WRMSG( HHC17500, "I", RexxPackage,Result);

    strcpy(Result, "Err Prefix: ");
    strcat(Result, ErrorPrefix ? ErrorPrefix : "(OFF)" );
    WRMSG( HHC17500, "I", RexxPackage,Result);

#if defined(ENABLE_OBJECT_REXX) && defined(ENABLE_REGINA_REXX)
    if ( RexxStatus == _STOPPED_ )
    {
        WRMSG( HHC17521, "I","");
        return -1;
    }
#endif /* defined(ENABLE_OBJECT_REXX) && defined(ENABLE_REGINA_REXX) */

    rc = exec_instore_cmd("parse version _v;parse source  _s;return _v || '0a'x || _s;\n",Result);
    if  ( rc == 0 )
    {
        for (ptr = strtok(Result,"\n"); ptr; ptr = strtok(NULL, "\n"))
        {
            WRMSG( HHC17500, "I", RexxPackage, ptr);
        }
    }
    else
    {
        WRMSG( HHC17501, "I", RexxPackage, "Unexpected RC from instore script");
    }

    return 0;

}

/*-------------------------------------------------------------------*/
/* exec Command - execute a rexx script                              */
/*-------------------------------------------------------------------*/
int exec_cmd(int argc, char *argv[], char *CommandLine)
{
int   iarg,argl;
int   rc=0;
short RetRC = 0;
int   iPath,iExtn;
int   haveResolvedCommand = FALSE;

char  wCommand[1024];
char *wArgs;

    UNREFERENCED(rc);
    UNREFERENCED(RetRC);
    UNREFERENCED(CommandLine);

#if defined(ENABLE_OBJECT_REXX) && defined(ENABLE_REGINA_REXX)

    if ( RexxStatus == _STOPPED_ )
    {
        WRMSG( HHC17521, "I", "");
        return -1 ;
    }
    if ( !hRexxLibHandle )
    {
        char *envvar;
        if ( !( envvar = getenv("HREXX_PACKAGE") ) )
        {
            REXX_TRY_OOREXX( exec_cmd_Rexx_Loaded )
            REXX_TRY_REGINA( exec_cmd_Rexx_Loaded )
            SETREXX_RESET()
            RexxStatus = _STOPPED_ ;
            WRMSG( HHC17526, "I", RexxPackage);
        }
        if ( strcasecmp(envvar, "auto" ) == 0 )
        {
            REXX_TRY_OOREXX( exec_cmd_Rexx_Loaded )
            REXX_TRY_REGINA( exec_cmd_Rexx_Loaded )
            SETREXX_RESET()
            RexxStatus = _STOPPED_ ;
            WRMSG( HHC17526, "I", RexxPackage);
        }
        if ( strcasecmp(envvar, "none" ) == 0 )
        {
            SETREXX_RESET()
            RexxStatus = _STOPPED_ ;
            WRMSG( HHC17521, "I", "");
            return 0;
        }
        if ( strcasecmp(envvar, OOREXX_PACKAGE ) == 0  )
        {
            REXX_TRY_OOREXX( exec_cmd_Rexx_Loaded )
            SETREXX_RESET()
            RexxStatus = _STOPPED_ ;
            WRMSG( HHC17526, "I", RexxPackage);
        }
        else if ( strcasecmp(envvar, REGINA_PACKAGE ) == 0  )
        {
            REXX_TRY_REGINA( exec_cmd_Rexx_Loaded )
            SETREXX_RESET()
            RexxStatus = _STOPPED_ ;
            WRMSG( HHC17526, "I", RexxPackage);
        }
        else
        {
            SETREXX_RESET()
            RexxStatus = _STOPPED_ ;
            WRMSG( HHC17501, "E", envvar,"Unknown REXX Package");
            return -1;
        }

    }
exec_cmd_Rexx_Loaded:
#endif /* defined(ENABLE_OBJECT_REXX) && defined(ENABLE_REGINA_REXX) */

    if (argc < 2)
    {
        WRMSG( HHC17505, "E", RexxPackage  );
        return -1;
    }

    strcpy(wCommand,argv[1]);

    if ( ! useResolver )
        goto skipResolver;

    haveResolvedCommand = FALSE;

    if ( access(wCommand, R_OK ) == 0 )
    {
        haveResolvedCommand = TRUE;
        goto endResolver;
    }

    if (strcmp(basename(wCommand),wCommand) != 0)
        goto endResolver;

    if ( !PathsInitialized )
        InitializePaths(NULL);
    if ( !ExtensionsInitialized )
        InitializeExtensions(NULL);
    if ( !OptionsInitialized )
        InitializeOptions();

    if ( useRexxPath )
    {
        for( iPath = 0; iPath<RexxPathCount; iPath++)
        {
            if ( access( RexxPathArray[iPath], R_OK ) != 0 )
                continue ;

            for( iExtn = 0; iExtn<ExtensionsCount; iExtn++)
            {
                sprintf(wCommand, PATHFORMAT, RexxPathArray[iPath], argv[1], ExtensionsArray[iExtn]);
                if ( access(wCommand, R_OK ) == 0)
                {
                    haveResolvedCommand = TRUE;
                    goto endResolver;
                }
            }
        }
    }
    if ( useSysPath )
    {
        for( iPath = 0; iPath<SysPathCount; iPath++)
        {
            if ( access( SysPathArray[iPath], R_OK ) != 0 )
                continue ;

            for( iExtn = 0;iExtn<ExtensionsCount;iExtn++)
            {
                sprintf(wCommand, PATHFORMAT, SysPathArray[iPath], argv[1],ExtensionsArray[iExtn]);
                if ( access(wCommand, R_OK ) == 0)
                {
                    haveResolvedCommand = TRUE;
                    goto endResolver;
                }
            }
        }
    }

endResolver:
    if ( !haveResolvedCommand )
    {
        WRMSG( HHC17506, "I", RexxPackage, argv[1]);
        return -1;
    }

skipResolver:
    if (argc > 2)
    {
        for (argl = 0, iarg = 2; iarg < argc; iarg++)
            argl += (int)strlen(argv[iarg]) + 1;
        wArgs = malloc(argl);
        strcpy(wArgs, argv[2]);
        for ( iarg = 3; iarg < argc; iarg++ )
        {
            strcat( wArgs, " " );
            strcat( wArgs, argv[iarg] );
        }
    }
    else
        wArgs = NULL;

    return (*RexxExecCmd)(wCommand, wArgs, argc, argv);

}

/*-------------------------------------------------------------------*/
/* exec Command - execute an Instore rexx script                     */
/*-------------------------------------------------------------------*/
int exec_instore_cmd( char *Command,  char *Result)
{
int rc=0;
short RetRC=0;
    UNREFERENCED(rc);
    UNREFERENCED(RetRC);

#if defined(ENABLE_OBJECT_REXX) && defined(ENABLE_REGINA_REXX)

    if ( RexxStatus == _STOPPED_ )
    {
        WRMSG( HHC17521, "I", "");
        return -1 ;
    }
    if ( !hRexxLibHandle )
    {
        char *envvar;
        if ( !( envvar = getenv("HREXX_PACKAGE") ) )
        {
            REXX_TRY_OOREXX( exec_instore_cmd_Rexx_Loaded )
            REXX_TRY_REGINA( exec_instore_cmd_Rexx_Loaded )
            SETREXX_RESET()
            RexxStatus = _STOPPED_ ;
            WRMSG( HHC17526, "I", RexxPackage);
        }
        if ( strcasecmp(envvar, "auto" ) == 0 )
        {
            REXX_TRY_OOREXX( exec_instore_cmd_Rexx_Loaded )
            REXX_TRY_REGINA( exec_instore_cmd_Rexx_Loaded )
            SETREXX_RESET()
            RexxStatus = _STOPPED_ ;
            WRMSG( HHC17526, "I", RexxPackage);
        }
        if ( strcasecmp(envvar, "none" ) == 0 )
        {
            SETREXX_RESET()
            RexxStatus = _STOPPED_ ;
            WRMSG( HHC17521, "I", "");
            return -1;
        }
        if ( strcasecmp(envvar, OOREXX_PACKAGE ) == 0  )
        {
            REXX_TRY_OOREXX( exec_instore_cmd_Rexx_Loaded )
            SETREXX_RESET()
            RexxStatus = _STOPPED_ ;
            WRMSG( HHC17526, "I", RexxPackage);
        }
        else if ( strcasecmp(envvar, REGINA_PACKAGE ) == 0  )
        {
            REXX_TRY_REGINA( exec_instore_cmd_Rexx_Loaded )
            SETREXX_RESET()
            RexxStatus = _STOPPED_ ;
            WRMSG( HHC17526, "I", RexxPackage);
        }
        else
        {
            SETREXX_RESET()
            RexxStatus = _STOPPED_ ;
            WRMSG( HHC17501, "E", envvar,"Unknown REXX Package");
            return -1;
        }

    }
exec_instore_cmd_Rexx_Loaded:
#endif /* defined(ENABLE_OBJECT_REXX) && defined(ENABLE_REGINA_REXX) */

    return (*RexxExecInstoreCmd)( Command, NULL, RetRC, Result);

}

void InitializePaths(char * wPath)
{
char *ptr;
char *envvar;
    if ( wPath )
    {
        RexxPath = strdup(wPath);
        free(wPath);
    }
    else
    {
        if ( ( envvar = getenv("HREXX_PATH") ) )
        {
            RexxPath = strdup(envvar);
        }
        else if ( ( envvar = getenv("HREXX_PATHS") ) )
        {
            RexxPath = strdup(envvar);
        }

    }

    if ( RexxPath )
    {
        for (RexxPathCount = 0, ptr = strtok(RexxPath,PATHDELIM); ptr; ptr = strtok(NULL, PATHDELIM))
        {
            RexxPathArray[RexxPathCount] = ptr;
            RexxPathCount++;
        }
    }
    else
    {
        RexxPathCount = 0 ;
    }

    envvar = getenv("PATH");
    SysPath = strdup(envvar);
    for (SysPathCount= 0,ptr = strtok(SysPath,PATHDELIM); ptr; ptr = strtok(NULL, PATHDELIM))
    {
        SysPathArray[SysPathCount] = ptr;
        SysPathCount++;
    }

    PathsInitialized = TRUE;
    return;
}

void InitializeExtensions(char * wExtns)
{
char *ptr;
char *envvar;

    if ( wExtns )
    {
        Extensions = strdup(wExtns);
        free(wExtns);
    }
    else
    {
        if ( ( envvar = getenv("HREXX_EXTENSIONS") ) )
        {
            Extensions = strdup(envvar);
        }
        else if ( ( envvar = getenv("HREXX_SUFFIXES") ) )
        {
            Extensions = strdup(envvar);
        }
        else
            Extensions = strdup(EXTENSIONS);
    }

    ExtensionsArray[0] = "";
    ExtensionsCount = 1 ;
    for (ptr = strtok(Extensions, EXTNDELIM ); ptr; ptr = strtok(NULL, EXTNDELIM))
    {
        ExtensionsArray[ExtensionsCount] = ptr;
        ExtensionsCount++;
    }
    ExtensionsInitialized = TRUE;
    return;
}

void InitializeOptions( void )
{
    useResolver = TRUE;
    MessageLevel = 0;
    MessagePrefix = NULL;
    ErrorPrefix = NULL;
}

#endif /* defined(ENABLE_OBJECT_REXX) || defined(ENABLE_REGINA_REXX)  */

#endif /* #ifndef _HREXX_C_  */

/* HREXX.C      (c)Copyright Enrico Sorichetti, 2012                 */
/*              REXX Interpreter Support                             */
/*                                                                   */
/*  Released under "The Q Public License Version 1"                  */
/*  (http://www.hercules-390.org/herclic.html) as modifications to   */
/*  Hercules.                                                        */

/*  inspired by the previous Rexx implementation by Jan Jaeger       */

// $Id: $

#include "hstdinc.h"
#define _HENGINE_DLL_
#include "hercules.h"

#ifndef _HREXX_C_
#define _HREXX_C_

#if defined(ENABLE_OBJECT_REXX) || defined(ENABLE_REGINA_REXX)
#if defined(ENABLE_OBJECT_REXX) && defined(ENABLE_REGINA_REXX)

#define REXX_TRY_OOREXX(_FOUND_) do \
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

#define REXX_TRY_REGINA(_FOUND_) do \
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
void InitializeRexxPaths(char *);
void InitializeRexxExtensions(char *);

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
void *hRexxApiLibHandle = NULL;     /* Api Library handle */

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

int   RexxPathsInitialized=FALSE;
char *RexxPaths = NULL;
char *RexxPathsArray[32];
int   RexxPathsCount=0;

int   RexxExtensionsInitialized=FALSE;
char *RexxExtensions = NULL;
char *RexxExtensionsArray[32];
int   RexxExtensionsCount=0;

int   MessageLevel;
char *MessagePrefix;
char *ErrorPrefix;


#define _WAITING_ 0
#define _STOPPED_ 1
int   RexxStatus = _WAITING_ ;

/*-------------------------------------------------------------------*/
/* rexx Command - manage the Rexx environment                        */
/*-------------------------------------------------------------------*/
int rexx_cmd(int argc, char *argv[], char *CommandLine)
{
char *ARGSDESC[] = {"", "Paths", "Extensions", "MsgLevel", "MsgPrefix", "ErrPrefix", "Start/Enable"};
char *PACKAGES[] = {"", "ooRexx", "Regina" };

#define _NEEDPATHS 1
#define _NEEDEXTNS 2
#define _NEEDMSGLEVL 3
#define _NEEDMSGPREF 4
#define _NEEDERRPREF 5
#define _NEEDPACKAGE 6

int   i;
int   rc = 0;
short RetRC = 0;
char  Result[1024];
char *ptr,*nxt;
char temp[33];
int  iarg,argl;

int  whatValue   = 0;

int  haveStop  = FALSE;
int  havePaths = FALSE;
int  haveExtns = FALSE;
int  haveMsgLevl= FALSE;
int  haveMsgPref = FALSE;
int  haveErrPref = FALSE;
int  haveStart = FALSE;

char *wPaths = NULL;
char *wExtns = NULL;
int   wMsgLevl = 0;
char *wMsgPref = NULL;
char *wErrPref = NULL;
char *wPackage  = NULL;

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
                case _NEEDPATHS :
                {
                    wPaths = strdup(argv[iarg]);
                }
                break;

                case _NEEDEXTNS :
                {
                    wExtns = strdup(argv[iarg]);
                }
                break;

                case _NEEDMSGLEVL :
                {
                    if ( strcasecmp( argv[iarg], "reset") == 0 )
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

                case _NEEDPACKAGE :
                {
                    if ( strcasecmp( argv[iarg], REGINA_PACKAGE) == 0 || strcasecmp( argv[iarg], OOREXX_PACKAGE) == 0 )
                        wPackage = strdup(argv[iarg]);
                    else
                    {
                        WRMSG( HHC17509, "W", RexxPackage, argv[iarg], ARGSDESC[_NEEDPACKAGE]);
                        return -1;
                    }
                }
                break;

            }
            whatValue = 0;
            continue;
        }

        if ( !haveStop &&
             ( ( argl >= 3 && argl <= 4 && strncasecmp("stop",    argv[iarg], argl) == 0 ) ||
               ( argl >= 3 && argl <= 7 && strncasecmp("disable", argv[iarg], argl) == 0 ) ))
        {
#if defined(ENABLE_OBJECT_REXX) && defined(ENABLE_REGINA_REXX)
            haveStop= TRUE;
            continue;
#else  /* defined(ENABLE_OBJECT_REXX) && defined(ENABLE_REGINA_REXX) */
            WRMSG( HHC17524, "I", RexxPackage);
            return 0;
#endif /* defined(ENABLE_OBJECT_REXX) && defined(ENABLE_REGINA_REXX) */
        }

        if ( !havePaths &&
             ( argl >= 4 && argl <= 5 && strncasecmp("paths", argv[iarg], argl) == 0 ) )
        {
            havePaths= TRUE;
            whatValue = _NEEDPATHS;
            continue;
        }

        if ( !haveExtns &&
             ( ( argl >= 3 && argl <=  8 && strncasecmp("suffixes",   argv[iarg], argl) == 0 ) ||
               ( argl >= 3 && argl <= 10 && strncasecmp("extensions", argv[iarg], argl) == 0 ) ) )
        {
            haveExtns= TRUE;
            whatValue = _NEEDEXTNS;
            continue;
        }

        if ( !haveMsgLevl &&
             ( argl >= 4 && argl <= 10 && strncasecmp("msglevel", argv[iarg], argl) == 0 ) )
        {
            haveMsgLevl= TRUE;
            whatValue = _NEEDMSGLEVL;
            continue;
        }

        if ( !haveMsgPref &&
             ( argl >= 4 && argl <=  9 && strncasecmp("msgprefix", argv[iarg], argl) == 0 ) )
        {
            haveMsgPref= TRUE;
            whatValue = _NEEDMSGPREF;
            continue;
        }

        if ( !haveErrPref &&
             ( argl >= 4 && argl <=  9 && strncasecmp("errprefix", argv[iarg], argl) == 0 ) )
        {
            haveErrPref= TRUE;
            whatValue = _NEEDERRPREF;
            continue;
        }

        if ( !haveStart &&
             ( ( argl >= 3 && argl <= 5 && strncasecmp("start",  argv[iarg], argl) == 0 ) ||
               ( argl >= 3 && argl <= 6 && strncasecmp("enable", argv[iarg], argl) == 0 ) ) )
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

    if ( havePaths )
    {
        if ( strcasecmp("reset", wPaths) == 0 )
        {
            InitializeRexxPaths(NULL);
        }
        else
            InitializeRexxPaths(wPaths);
    }

    if ( haveExtns )
    {
        if ( strcasecmp("reset", wExtns) == 0 )
            InitializeRexxExtensions(NULL);
        else
            InitializeRexxExtensions(wExtns);
    }

    if ( haveMsgLevl )
    {
        MessageLevel = wMsgLevl;
    }

    if ( haveMsgPref )
    {
        if ( strcasecmp("reset", wMsgPref) == 0 )
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
        if ( strcasecmp("reset", wErrPref) == 0 )
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

    if ( !RexxPathsInitialized )
        InitializeRexxPaths(NULL);
    strcpy(Result,"Paths: " );
    for (i=0;i<RexxPathsCount;i++)
    {
        strcat(Result,RexxPathsArray[i]);
        strcat(Result,i == RexxPathsCount-1 ? "" : PATHDELIM );
    }
    WRMSG( HHC17500, "I", RexxPackage,Result);

    if ( !RexxExtensionsInitialized )
        InitializeRexxExtensions(NULL);
    strcpy(Result,"Extensions: ");
    for (i=0;i<RexxExtensionsCount;i++)
    {
        strcat(Result,RexxExtensionsArray[i]);
        strcat(Result,i == RexxExtensionsCount-1 ? "" : EXTNDELIM );
    }
    WRMSG( HHC17500, "I", RexxPackage,Result);

    strcpy(Result,"Msg Level : ");
    sprintf(temp,"%d",MessageLevel);
    strcat(Result,temp);
    WRMSG( HHC17500, "I", RexxPackage,Result);

    strcpy(Result,"Msg Prefix: ");
    if ( MessagePrefix )
        strcat(Result,MessagePrefix);
    else
        strcat(Result,"(OFF)");
    WRMSG( HHC17500, "I", RexxPackage,Result);

    strcpy(Result,"Err Prefix: ");
    if ( ErrorPrefix )
        strcat(Result,ErrorPrefix);
    else
        strcat(Result,"(OFF)");
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

    haveResolvedCommand = FALSE;

    if ( access(wCommand, R_OK ) == 0 )
    {
        haveResolvedCommand = TRUE;
        goto endResolver;
    }

    if (strcmp(basename(wCommand),wCommand) != 0)
        goto endResolver;

    if ( !RexxPathsInitialized )
        InitializeRexxPaths(NULL);
    if ( !RexxExtensionsInitialized )
        InitializeRexxExtensions(NULL);

    for( iPath = 0;iPath<RexxPathsCount;iPath++)
    {
        if ( access( RexxPathsArray[iPath], R_OK ) != 0 )
            continue;

        sprintf(wCommand, PATHFORMAT, RexxPathsArray[iPath], argv[1],"");
        if ( access(wCommand, R_OK ) == 0)
        {
            haveResolvedCommand = TRUE;
            goto endResolver;
        }
        if  ( !strstr(argv[1],".") )
        {
            for( iExtn = 0;iExtn<RexxExtensionsCount;iExtn++)
            {
                sprintf(wCommand, PATHFORMAT, RexxPathsArray[iPath], argv[1],RexxExtensionsArray[iExtn]);
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

void InitializeRexxPaths(char * wPaths)
{
char *ptr;
char *envvar;
    if ( wPaths )
    {
        RexxPaths = strdup(wPaths);
        free(wPaths);
    }
    else
    {
        if ( ( envvar = getenv("HREXX_PATH") ) )
        {
            RexxPaths = strdup(envvar);
        }
        else if ( ( envvar = getenv("HREXX_PATHS") ) )
        {
            RexxPaths = strdup(envvar);
        }
        else
            RexxPaths = strdup(getenv("PATH"));
    }
    for (RexxPathsCount= 0,ptr = strtok(RexxPaths,PATHDELIM); ptr; ptr = strtok(NULL, PATHDELIM))
    {
        RexxPathsArray[RexxPathsCount] = ptr;
        RexxPathsCount++;
    }
    RexxPathsInitialized = TRUE;
    return;
}

void InitializeRexxExtensions(char * wExtns)
{
char *ptr;
char *envvar;

    if ( wExtns )
    {
        RexxExtensions = strdup(wExtns);
        free(wExtns);
    }
    else
    {
        if ( ( envvar = getenv("HREXX_EXTENSIONS") ) )
        {
            RexxExtensions = strdup(envvar);
        }
        else if ( ( envvar = getenv("HREXX_SUFFIXES") ) )
        {
            RexxExtensions = strdup(envvar);
        }
        else
            RexxExtensions = strdup(EXTENSIONS);
    }

    for (RexxExtensionsCount= 0,ptr = strtok(RexxExtensions, EXTNDELIM ); ptr; ptr = strtok(NULL, EXTNDELIM))
    {
        RexxExtensionsArray[RexxExtensionsCount] = ptr;
        RexxExtensionsCount++;
    }
    RexxExtensionsInitialized = TRUE;
    return;
}

#endif /* defined(ENABLE_OBJECT_REXX) || defined(ENABLE_REGINA_REXX)  */

#endif /* #ifndef _HREXX_C_  */

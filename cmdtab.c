/* CMDTAB.C     (c) Copyright Roger Bowler, 1999-2010                */
/*              (c) Copyright "Fish" (David B. Trout), 2002-2009     */
/*              (c) Copyright Jan Jaeger, 2003-2009                  */
/*              (c) Copyright TurboHercules, SAS 2010                */
/*              Route all Hercules configuration statements          */
/*              and panel commands to the appropriate functions      */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

#include "hstdinc.h"

#define _CMDTAB_C_
#define _HENGINE_DLL_

#include "hercules.h"
#include "history.h"


// Handle externally defined commands...

// (for use in CMDTAB COMMAND entry further below)
#define      EXT_CMD(xxx_cmd)  call_ ## xxx_cmd

// (for defining routing function immediately below)
#define CALL_EXT_CMD(xxx_cmd)  \
int call_ ## xxx_cmd ( int argc, char *argv[], char *cmdline )  { \
    return   xxx_cmd (     argc,       argv,         cmdline ); }

// Externally defined commands routing functions...

#if defined(OPTION_PTTRACE)
CALL_EXT_CMD ( ptt_cmd    )
#endif // OPTION_PTTRACE
CALL_EXT_CMD ( cache_cmd  )
CALL_EXT_CMD ( shared_cmd )

/*-------------------------------------------------------------------*/
/* Create forward references for all commands in the command table   */
/*-------------------------------------------------------------------*/
#define _FW_REF
#define COMMAND(_stmt, _type, _group, _func, _sdesc, _ldesc)        \
int (_func)(int argc, char *argv[], char *cmdline);
#define CMDABBR(_stmt, _abbr, _type, _group, _func, _sdesc, _ldesc) \
int (_func)(int argc, char *argv[], char *cmdline);
#include "cmdtab.h"
#undef COMMAND
#undef CMDABBR
#undef _FW_REF


typedef int CMDFUNC(int argc, char *argv[], char *cmdline);

/* Layout of command routing table                        */
typedef struct _CMDTAB
{
    const char  *statement;        /* statement           */
    const size_t statminlen;       /* min abbreviation    */
          BYTE    type;            /* statement type      */
#define DISABLED   0x00            /* disabled statement  */
#define CONFIG     0x01            /* config statement    */
#define PANEL      0x02            /* command statement   */
          BYTE    group;           /* grouping commands   */
#define SYSOPER    0x01          /* System Operator     */
#define SYSMAINT   0x02          /* System Maintainer   */
#define SYSPROG    0x04          /* Systems Programmer  */
#define SYSDEVEL   0x20          /* System Developer    */
#define SYSDEBUG   0x40          /* Enable Debugging    */
#define SYSNONE    0x80          /* Enable for any lvls */
#define SYSCMDALL  ( SYSOPER + SYSMAINT + SYSPROG + SYSDEVEL + SYSDEBUG ) /* Valid in all states */ 
    CMDFUNC    *function;          /* handler function    */
    const char *shortdesc;         /* description         */
    const char *longdesc;          /* detaled description */
} CMDTAB;

#define COMMAND(_stmt, _type, _group, _func, _sdesc, _ldesc) \
{ (_stmt),     (0), (_type), (_group), (_func), (_sdesc), (_ldesc) },
#define CMDABBR(_stmt, _abbr, _type, _group, _func, _sdesc, _ldesc) \
{ (_stmt), (_abbr), (_type), (_group), (_func), (_sdesc), (_ldesc) },

static CMDTAB cmdtab[] =
{
#include "cmdtab.h"
COMMAND ( NULL, 0, 0, NULL, NULL, NULL ) /* End of table */
};

/* internal functions */
int HelpMessage(char *);

/*-------------------------------------------------------------------*/
/* $zapcmd - internal debug - may cause havoc - use with caution     */
/*-------------------------------------------------------------------*/
int zapcmd_cmd(int argc, char *argv[], char *cmdline)
{
CMDTAB* cmdent;
int i;

    UNREFERENCED(cmdline);

    if (argc > 1)
    {
        for (cmdent = cmdtab; cmdent->statement; cmdent++)
        {
            if(!strcasecmp(argv[1], cmdent->statement))
            {
                if(argc > 2)
                    for(i = 2; i < argc; i++)
                    {
                        if(!strcasecmp(argv[i],"Cfg"))
                            cmdent->type |= CONFIG;
                        else
                        if(!strcasecmp(argv[i],"NoCfg"))
                            cmdent->type &= ~CONFIG;
                        else
                        if(!strcasecmp(argv[i],"Cmd"))
                            cmdent->type |= PANEL;
                        else
                        if(!strcasecmp(argv[i],"NoCmd"))
                            cmdent->type &= ~PANEL;
                        else
                        {
                            logmsg(_("Invalid arg: %s: %s %s [(No)Cfg|(No)Cmd]\n"),argv[i],argv[0],argv[1]);
                            return -1;
                        }
                    }
                else
                    logmsg(_("%s: %s(%sCfg,%sCmd)\n"),argv[0],cmdent->statement,
                      (cmdent->type&CONFIG)?"":"No",(cmdent->type&PANEL)?"":"No");
                return 0;
            }
        }
        logmsg(_("%s: %s not in command table\n"),argv[0],argv[1]);
        return -1;
    }
    else
        logmsg(_("Usage: %s <command> [(No)Cfg|(No)Cmd]\n"),argv[0]);
    return -1;
}

CMDT_DLL_IMPORT
int ProcessConfigCommand (int argc, char **argv, char *cmdline)
{
CMDTAB* cmdent;

    if (!argc)
        return -1;

#if defined(OPTION_DYNAMIC_LOAD)
    if(config_command)
        if( !config_command(argc, argv, cmdline) )
            return 0;
#endif /*defined(OPTION_DYNAMIC_LOAD)*/

    for (cmdent = cmdtab; cmdent->statement; cmdent++)
        if(cmdent->function && (cmdent->type & CONFIG))
            if( !strncasecmp(argv[0], cmdent->statement, 
                cmdent->statminlen == 0 ? 
                MAX( strlen(argv[0]), strlen(cmdent->statement) ) : 
                MAX( cmdent->statminlen, strlen(argv[0]) ) ) )
            {
                char cmd[256];
                strlcpy( cmd, cmdent->statement, sizeof(cmd) );
                argv[0] = cmd;             
                return cmdent->function(argc, argv, cmdline);
            }

    return -1;
}


CMDT_DLL_IMPORT
int ProcessConfigCmdLine(char* pszCmdLine)
{
    int      rc, argc;
    char*    argv[MAX_ARGS];
    char*    pszSaveCmdLine;

    pszSaveCmdLine = strdup(pszCmdLine);

    parse_args (pszCmdLine, MAX_ARGS, argv, &argc);

    rc = ProcessConfigCommand(argc, argv, pszSaveCmdLine);

    free(pszSaveCmdLine);

    return rc;
}


/*-------------------------------------------------------------------*/
/* Main panel command processing function                            */
/*-------------------------------------------------------------------*/
int OnOffCommand(int argc, char *argv[], char *cmdline);
int ShadowFile_cmd(int argc, char *argv[], char *cmdline);

int    cmd_argc;
char*  cmd_argv[MAX_ARGS];

int ProcessPanelCommand (char* pszCmdLine)
{
    CMDTAB*  pCmdTab         = NULL;
    char*    pszSaveCmdLine  = NULL;
    int      rc              = -1;

    if (!pszCmdLine || !*pszCmdLine)
    {
        /* [enter key] by itself: start the CPU
           (ignore if not instruction stepping) */
        if (sysblk.inststep)
            rc = start_cmd(0,NULL,NULL);
        goto ProcessPanelCommandExit;
    }

    /* Save unmodified copy of the command line in case
       its format is unusual and needs customized parsing. */
    pszSaveCmdLine = strdup(pszCmdLine);

    /* Parse the command line into its individual arguments...
       Note: original command line now sprinkled with nulls */
    parse_args (pszCmdLine, MAX_ARGS, cmd_argv, &cmd_argc);

    /* If no command was entered (i.e. they entered just a comment
       (e.g. "# comment")) then ignore their input */
    if ( !cmd_argv[0] )
        goto ProcessPanelCommandExit;

#if defined(OPTION_DYNAMIC_LOAD)
    if(system_command)
        if(!(rc = system_command(cmd_argc, (char**)cmd_argv, pszSaveCmdLine)))
            goto ProcessPanelCommandExit;
#endif

    /* Route standard formatted commands from our routing table... */
    if (cmd_argc)
        for (pCmdTab = cmdtab; pCmdTab->function; pCmdTab++)
        {
            if ( pCmdTab->function && (pCmdTab->type & PANEL) &&
                 ( (sysblk.diag8cmd & DIAG8CMD_RUNNING) || 
                   (pCmdTab->group & sysblk.sysgroup) ) )
            {
                if(!strncasecmp(cmd_argv[0],pCmdTab->statement,
                    pCmdTab->statminlen == 0 ?
                    MAX( strlen(cmd_argv[0]), strlen(pCmdTab->statement) ) : 
                    MAX( pCmdTab->statminlen, strlen(cmd_argv[0]) ) ) )
                {
                    char cmd[256];
                    strlcpy(cmd, pCmdTab->statement, sizeof(cmd));
                    cmd_argv[0] = cmd;             
                    rc = pCmdTab->function(cmd_argc, (char**)cmd_argv, pszSaveCmdLine);
                    goto ProcessPanelCommandExit;
                }
            }
        }

    /* Route non-standard formatted commands... */

    /* sf commands - shadow file add/remove/set/compress/display */
    if (0
        || !strncasecmp(pszSaveCmdLine,"sf+",3)
        || !strncasecmp(pszSaveCmdLine,"sf-",3)
        || !strncasecmp(pszSaveCmdLine,"sfc",3)
        || !strncasecmp(pszSaveCmdLine,"sfd",3)
        || !strncasecmp(pszSaveCmdLine,"sfk",3)
    )
    {
        rc = ShadowFile_cmd(cmd_argc,(char**)cmd_argv,pszSaveCmdLine);
        goto ProcessPanelCommandExit;
    }

    /* x+ and x- commands - turn switches on or off */
    if ('+' == pszSaveCmdLine[1] || '-' == pszSaveCmdLine[1])
    {
        rc = OnOffCommand(cmd_argc,(char**)cmd_argv,pszSaveCmdLine);
        goto ProcessPanelCommandExit;
    }

    /* Error: unknown/unsupported command... */
    ASSERT( cmd_argv[0] );

    if ( sysblk.scpimply && can_send_command() )
        scp_command(pszSaveCmdLine, FALSE, sysblk.scpecho ? TRUE : FALSE);
    else
        WRMSG( HHC01600, "E", cmd_argv[0] );

ProcessPanelCommandExit:

    /* Free our saved copy */
    free(pszSaveCmdLine);

    return rc;
}


/*-------------------------------------------------------------------*/
/* help command - display additional help for a given command        */
/*-------------------------------------------------------------------*/
int HelpCommand(int argc, char *argv[], char *cmdline)
{
    CMDTAB* pCmdTab;
    int     rc = 1;
    int     len = -1;
    char   *p = NULL;
          
    if ( argc == 2 ) 
    {
        p = strchr(argv[1], '*');
        if ( p != NULL )
        {
            len = (int)(p - argv[1]);
            if ( strlen( argv[1] ) > (unsigned) ( len + 1) )
            {
                WRMSG( HHC02299, "E", argv[0] );
                return -1;
            }
        }
    }

    UNREFERENCED(cmdline);

    if (argc < 2 || ( argc == 2 && len >= 0 ) )
    {
        int first = TRUE;
        if ( argc < 2) 
            len = 0;

        /* List standard formatted commands from our routing table... */

        for (pCmdTab = cmdtab; pCmdTab->statement; pCmdTab++)
        {
            if (  (pCmdTab->type & PANEL)
               && ( (sysblk.diag8cmd & DIAG8CMD_RUNNING) || 
                    (pCmdTab->group & sysblk.sysgroup) 
                  )  
               && (pCmdTab->shortdesc) 
               && ( len == 0 || ( len > 0 && !strncasecmp(argv[1],pCmdTab->statement,len) ) )
               )
            {
                if (first)
                {
                    first = FALSE;
                    WRMSG( HHC01602, "I", "Command", "Description" );
                    WRMSG( HHC01602, "I", "-------", "-----------------------------------------------" );
                }
                WRMSG( HHC01602, "I", pCmdTab->statement, pCmdTab->shortdesc );
            }
        }
        if (first && len > 0)
        {
            WRMSG( HHC01609, "E", argv[1] );
            rc = -1;
        }
        else
            rc = 0;
    }
    else
    {
        for (pCmdTab = cmdtab; pCmdTab->statement; pCmdTab++)
        {
            if ( (pCmdTab->type & PANEL) && 
                 ( (sysblk.diag8cmd & DIAG8CMD_RUNNING) || 
                   (pCmdTab->group & sysblk.sysgroup) ) && 
                 ( !strncasecmp(argv[1], pCmdTab->statement,
                                         pCmdTab->statminlen == 0 ?
                                MAX( strlen(argv[1]), strlen(pCmdTab->statement) ) : 
                                MAX( pCmdTab->statminlen, strlen(argv[1]) ) ) )
               )

            {
                WRMSG( HHC01602, "I", "Command", "Description" );
                WRMSG( HHC01602, "I", "-------", "-----------------------------------------------" );
                WRMSG( HHC01602, "I",pCmdTab->statement,pCmdTab->shortdesc);
                if(pCmdTab->longdesc)
                {
                    char buf[257];
                    int i = 0; 
                    int j;

		            WRMSG ( HHC01603, "I", "Long description:");
                    while(pCmdTab->longdesc[i])
                    {
                        for(j = 0; j < (int)sizeof(buf)      && 
                                   pCmdTab->longdesc[i] && 
                                   pCmdTab->longdesc[i] != '\n'; i++, j++)
                        {
                            buf[j] = pCmdTab->longdesc[i];
                        }

                        buf[j] = '\0';
                        if(pCmdTab->longdesc[i] == '\n')
                            i++;
                        WRMSG( HHC01603, "I", buf );
                    }
                }
                rc = 0;
            }
        }
        if ( rc == 1 )
        {
            /* detect hercules message HHCnnnnn or HHCnnnnns */
            if ( argc == 2 && ( strlen(argv[1]) == 8 || strlen(argv[1]) == 9 ) &&
                 ( !strncasecmp(argv[1], "HHC", 3)))
            { 
                rc = (HelpMessage(argv[1]));
            }
            else
            {
                WRMSG( HHC01604, "I", argv[1]);
                rc = -1;
            }
        }
    }
    return rc;
}

/*-------------------------------------------------------------------*/
/* HelpMessage - print help text for message hhcnnnnna               */
/*-------------------------------------------------------------------*/
int HelpMessage(char *msg)
{
    char id[9];

    strlcpy(id, "HHC",sizeof(id));
    strncpy(&id[3], &msg[3], 5);
    id[8] = 0;

    WRMSG(HHC01607, "I", id);
    return(-1);
}
    
int scr_recursion_level();

#if defined(OPTION_DYNAMIC_LOAD)
DLL_EXPORT void *panel_command_r (void *cmdline)
#else
void *panel_command (void *cmdline)
#endif
{
#define MAX_CMD_LEN (32768)
    char  cmd[MAX_CMD_LEN];             /* Copy of panel command     */
    char *pCmdLine;
    char *cl;
    unsigned i;
    int noredisp;

    pCmdLine = cmdline; ASSERT(pCmdLine);
    /* every command will be stored in history list */
    /* except null commands, script commands, scp input and noredisplay */
    if (*pCmdLine != 0 && scr_recursion_level()  == 0)
    {
        if (!(*pCmdLine == '-'))
        {
            if (*pCmdLine == '.' || *pCmdLine == '!')
            {
                if (sysblk.scpecho)
                    history_add(cmdline);
            }
            else
                history_add(cmdline);
        }
    }

    /* Copy panel command to work area, skipping leading blanks */

    /* If the command starts with a -, then strip it and indicate
     * we do not want command redisplay
     */

    noredisp=0;
    while (*pCmdLine && isspace(*pCmdLine)) pCmdLine++;
    i = 0;
    while (*pCmdLine && i < (MAX_CMD_LEN-1))
    {
        if(i==0 && *pCmdLine=='-')
        {
            noredisp=1;
            /* and remove blanks again.. */
            while (*pCmdLine && isspace(*pCmdLine)) pCmdLine++;
        }
        else
        {
            cmd[i] = *pCmdLine;
            i++;
        }
        pCmdLine++;
    }
    cmd[i] = 0;

    /* Ignore null commands (just pressing enter)
       unless instruction stepping is enabled or
       commands are being sent to the SCP by default. */
    if (!sysblk.inststep
#ifdef OPTION_CMDTGT
                         && (sysblk.cmdtgt == 0)
#endif
                                                 && (0 == cmd[0]))
        return NULL;

#ifdef OPTION_CMDTGT
    /* check for herc, scp or pscp command */
    /* Please note that cmdtgt is a hercules command! */
    /* Changing the target to her in scp mode is done by using herc cmdtgt herc */
    if(!strncasecmp(cmd, "herc ", 5) || !strncasecmp(cmd, "scp ", 4) || !strncasecmp(cmd, "pscp ", 5))
    {
        if (!noredisp) WRMSG(HHC00013, "I", cmd);     // Echo command to the control panel
        ProcessPanelCommand(cmd);
        return NULL;
    }

    /* Send command to the selected command target */
    switch(sysblk.cmdtgt)
    {
        case 0: // cmdtgt herc
        {
        /* Stay compatible */
#ifdef _FEATURE_SYSTEM_CONSOLE
            if(cmd[0] == '.' || cmd[0] == '!')
            {
                if(!cmd[1])
                {
                    cmd[1] = ' ';
                    cmd[2] = 0;
                }
                scp_command(cmd + 1, cmd[0] == '!', sysblk.scpecho ? TRUE: FALSE);
            }
            else
#endif /*_FEATURE_SYSTEM_CONSOLE*/
            {
                if (!noredisp) WRMSG(HHC00013, "I", cmd);     // Echo command to the control panel
#if defined(OPTION_CONFIG_SYMBOLS)
                /* Perform variable substitution */
                /* First, set some 'dynamic' symbols to their own values */
                set_symbol("CUU","$(CUU)");
                set_symbol("CCUU","$(CCUU)");
                cl=resolve_symbol_string(cmd);
                ProcessPanelCommand(cl);
                free(cl);
#else
                ProcessPanelCommand(cmd);
#endif
            }
            break;
        }
        case 1: // cmdtgt scp
        {
            if(!cmd[0])
            {
                cmd[0] = ' ';
                cmd[1] = 0;
            }
            scp_command(cmd, 0, TRUE);      // echo command
            break;
        }
        case 2: // cmdtgt pscp
        {
            if(!cmd[0])
            {
                cmd[0] = ' ';
                cmd[1] = 0;
            }
            scp_command(cmd, 1, TRUE);      // echo command
            break;
        }
    }
#else // OPTION_CMDTGT
#ifdef _FEATURE_SYSTEM_CONSOLE
    if ('.' == cmd[0] || '!' == cmd[0])
    {
        if (!cmd[1]) { cmd[1]=' '; cmd[2]=0; }
        scp_command (cmd+1, cmd[0] == '!', sysblk.scpecho ? TRUE: FALSE);  
        return NULL;
    }
#endif /*_FEATURE_SYSTEM_CONSOLE*/
    if (!noredisp) WRMSG(HHC00013, "I", cmd);     // Echo command to the control panel
    ProcessPanelCommand(cmd);

#endif // OPTION_CMDTGT

    return NULL;
}

/*---------------------------------------------------------------------------*/
/* file: hao.c                                                               */
/*                                                                           */
/* Implementation of the automatic operator withing the Hercules z/Series    */
/* emulator. This implementation couldn't be done without the advise of      */
/* Jan Jaeger, who always has an answer to any question. Thanks for the      */
/* regex advice!                                                             */
/*                                                                           */
/*                            (c) Copyright Bernard van der Helm, 2002-2007  */
/*                            Noordwijkerhout, The Netherlands.              */
/*                                                                           */
/*---------------------------------------------------------------------------*/

// $Id$
//
// $Log$
// Revision 1.4  2007/06/23 00:04:10  ivan
// Update copyright notices to include current year (2007)
//
// Revision 1.3  2006/12/08 09:43:21  jj
// Add CVS message log
//

#include "hstdinc.h"

#define _HAO_C_
#define _HENGINE_DLL_

#include "hercules.h"

#if defined(OPTION_HAO)

/*---------------------------------------------------------------------------*/
/* constants                                                                 */
/*---------------------------------------------------------------------------*/
#define HHCAO001I "HHCAO001I Hercules Automatic Operator thread started;\n" \
                  "          tid="TIDPAT", pri=%d, pid=%d\n"
#define HHCAO002I "HHCAO002I Hercules Automatic Operator thread ended\n"
#define HHCAO003I "HHCAO003I Firing command: '%s'\n"
#define HHCAO004I "HHCAO004I The defined Automatic Operator rule(s) are:\n"
#define HHCAO005I "HHCAO005I %02d: '%s' -> '%s'\n"
#define HHCAO006I "HHCAO006I %d rule(s) displayed\n"
#define HHCAO007E "HHCAO007E Unknown hao command, valid commands are:\n" \
                  "  hao tgt <tgt> : define target rule (pattern) to react on\n" \
                  "  hao cmd <cmd> : define command for previously defined rule\n" \
                  "  hao list <n>  : list all rules/commands or only at index <n>\n" \
                  "  hao del <n>   : delete the rule at index <n>\n" \
                  "  hao clear     : delete all rules (stops automatic operator)\n"
#define HHCAO008E "HHCAO008E No rule defined at index %d\n"
#define HHCAO009E "HHCAO009E Invalid index, index must be between 0 and %d\n"
#define HHCAO010E "HHCAO010E Target not added, table full\n"
#define HHCAO011E "HHCAO011E Tgt command given, but cmd command expected\n"
#define HHCAO012E "HHCAO012E Empty target specified\n"
#define HHCAO013E "HHCAO013E Target not added, duplicate found in table\n"
#define HHCAO014E "HHCAO014E %s\n"
#define HHCAO015E "HHCAO015E %s\n"
#define HHCAO016I "HHCAO016I Target placed at index %d\n"
#define HHCAO017E "HHCAO017E Cmd command given, but tgt command expected\n"
#define HHCAO018E "HHCAO018E Empty command specified\n"
#define HHCAO019E "HHCAO019E Command not added; causes loop with target at index %d\n"
#define HHCAO020I "HHCAO020I Command placed at index %d\n"
#define HHCAO021E "HHCAO021E Target not added, causes loop with command at index %d\n"
#define HHCAO022I "HHCAO022I All automatic operation rules cleared\n"
#define HHCAO023E "HHCAO023E hao del command given without a valid index\n"
#define HHCAO024E "HHCAO024E Rule at index %d not deleted, already empty\n"
#define HHCAO025I "HHCAO025I Rule at index %d succesfully deleted\n"
#define HHCA0026E "HHCA0026E Command not added, may cause dead locks\n"

#define HAO_WKLEN    256    /* (maximum message length able to tolerate) */
#define HAO_MAXRULE  10     /* (purely arbitrary and easily increasable) */

/*---------------------------------------------------------------------------*/
/* local variables                                                           */
/*---------------------------------------------------------------------------*/
static LOCK     ao_lock;
static regex_t  ao_preg[HAO_MAXRULE];
static char    *ao_cmd[HAO_MAXRULE];
static char    *ao_tgt[HAO_MAXRULE];
static char     ao_msgbuf[LOG_DEFSIZE+1];   /* (plus+1 for NULL termination) */

/*---------------------------------------------------------------------------*/
/* function prototypes                                                       */
/*---------------------------------------------------------------------------*/
DLL_EXPORT void  hao_initialize(void);
DLL_EXPORT void  hao_command(char *cmd);
DLL_EXPORT void  hao_message(char *buf);
static     void  hao_clear(void);
static     void  hao_cmd(char *arg);
static     void  hao_cpstrp(char *dest, char *src);
static     void  hao_del(char *arg);
static     void  hao_list(char *arg);
static     void  hao_tgt(char *arg);
static     void* hao_thread(void* dummy);

/*---------------------------------------------------------------------------*/
/* void hao_initialize(void)                                                 */
/*                                                                           */
/* This function is called at system startup by impl.c 'process_rc_file'     */
/* It initializes all global variables.                                      */
/*---------------------------------------------------------------------------*/
DLL_EXPORT void hao_initialize(void)
{
  int i = 0;

  initialize_lock(&ao_lock);

  /* serialize */
  obtain_lock(&ao_lock);

  /* initialize variables */
  for(i = 0; i < HAO_MAXRULE; i++)
  {
    ao_cmd[i] = NULL;
    ao_tgt[i] = NULL;
  }

  /* initialize message buffer */
  memset(ao_msgbuf, 0, sizeof(ao_msgbuf));

  /* Start message monitoring thread */
  if ( create_thread (&sysblk.haotid, &sysblk.joinattr,
    hao_thread, NULL, "hao_thread") )
  {
    logmsg(_("HHCIN004S Cannot create HAO thread: %s\n"),
      strerror(errno));
  }

  release_lock(&ao_lock);
}

/*---------------------------------------------------------------------------*/
/* void hao_command(char *cmd)                                               */
/*                                                                           */
/* Within panel this function is called when a command is given that starts  */
/* with the string hao. Here we check if a correct hao command is requested, */
/* otherwise a help is printed.                                              */
/*---------------------------------------------------------------------------*/
DLL_EXPORT void hao_command(char *cmd)
{
  char work[HAO_WKLEN];
  char work2[HAO_WKLEN];

  /* copy and strip spaces */
  hao_cpstrp(work, cmd);

  /* again without starting hao */
  hao_cpstrp(work2, &work[3]);

  if(!strncasecmp(work2, "tgt", 3))
  {
    /* again without starting tgt */
    hao_cpstrp(work, &work2[3]);
    hao_tgt(work);
    return;
  }

  if(!strncasecmp(work2, "cmd", 3))
  {
    /* again without starting cmd */
    hao_cpstrp(work, &work2[3]);
    hao_cmd(work);
    return;
  }

  if(!strncasecmp(work2, "del", 3))
  {
    /* again without starting del */
    hao_cpstrp(work, &work2[3]);
    hao_del(work);
    return;
  }

  if(!strncasecmp(work2, "list", 4))
  {
    /* again without starting list */
    hao_cpstrp(work, &work2[4]);
    hao_list(work);
    return;
  }

  if(!strncasecmp(work2, "clear", 4))
  {
    hao_clear();
    return;
  }

  logmsg(HHCAO007E);
}

/*---------------------------------------------------------------------------*/
/* hao_cpstrp(char *dest, char *src)                                         */
/*                                                                           */
/* This function copies the string from src to dest, without the trailing    */
/* and ending spaces.                                                        */
/*---------------------------------------------------------------------------*/
static void hao_cpstrp(char *dest, char *src)
{
  int i;

  for(i = 0; src[i] == ' '; i++);
  strncpy(dest, &src[i], HAO_WKLEN);
  dest[HAO_WKLEN-1] = 0;
  for(i = strlen(dest); i && dest[i - 1] == ' '; i--);
  dest[i] = 0;
}

/*---------------------------------------------------------------------------*/
/* void hao_tgt(char *arg)                                                   */
/*                                                                           */
/* This function is given when the hao tgt command is given. A free slot is  */
/* to be found and filled with the rule. There will be loop checking.        */
/*---------------------------------------------------------------------------*/
static void hao_tgt(char *arg)
{
  int i;
  int j;
  int rc;
  char work[HAO_WKLEN];

  /* serialize */
  obtain_lock(&ao_lock);

  /* find a free slot */
  for(i = 0; ao_tgt[i] && i < HAO_MAXRULE; i++)

  /* check for table full */
  if(i == HAO_MAXRULE)
  {
    release_lock(&ao_lock);
    logmsg(HHCAO010E);
    return;
  }

  /* check if not command is expected */
  for(j = 0; j < HAO_MAXRULE; j++)
  {
    if(ao_tgt[j] && !ao_cmd[j])
    {
      release_lock(&ao_lock);
      logmsg(HHCAO011E);
      return;
    }
  }

  /* check for empty target */
  if(!strlen(arg))
  {
    release_lock(&ao_lock);
    logmsg(HHCAO012E);
    return;
  }

  /* check for duplicate targets */
  for(j = 0; j < HAO_MAXRULE; j++)
  {
    if(ao_tgt[j] && !strcmp(arg, ao_tgt[j]))
    {
      release_lock(&ao_lock);
      logmsg(HHCAO013E);
      return;
    }
  }

  /* compile the target string */
  rc = regcomp(&ao_preg[i], arg, 0);

  /* check for error */
  if(rc)
  {
    release_lock(&ao_lock);

    /* place error in work */
    regerror(rc, (const regex_t *) &ao_preg[i], work, HAO_WKLEN);
    logmsg(HHCAO014E, work);
    return;
  }

  /* check for possible loop */
  for(j = 0; j < HAO_MAXRULE; j++)
  {
    if(ao_cmd[j] && !regexec(&ao_preg[i], ao_cmd[j], 0, NULL, 0))
    {
      release_lock(&ao_lock);
      regfree(&ao_preg[i]);
      logmsg(HHCAO021E, i);
      return;
    }
  }

  /* duplicate the target */
  ao_tgt[i] = strdup(arg);

  /* check duplication */
  if(!ao_tgt[i])
  {
    release_lock(&ao_lock);
    regfree(&ao_preg[i]);
    logmsg(HHCAO015E, strerror(ENOMEM));
    return;
  }

  release_lock(&ao_lock);
  logmsg(HHCAO016I, i);
}

/*---------------------------------------------------------------------------*/
/* void hao_cmd(char *arg)                                                   */
/*                                                                           */
/* This function is called when the hao cmd command is given. It searches    */
/* the index of the last given hao tgt command. Does some checking and fills */
/* the entry with the given command. There will be loop checking             */
/*---------------------------------------------------------------------------*/
static void hao_cmd(char *arg)
{
  int i;
  int j;

  /* serialize */
  obtain_lock(&ao_lock);

  /* find the free slot */
  for(i = 0; ao_cmd[i] && i < HAO_MAXRULE; i++);

  /* check for table full -> so tgt cmd expected */
  if(i == HAO_MAXRULE)
  {
    release_lock(&ao_lock);
    logmsg(HHCAO017E);
    return;
  }

  /* check if target is given */
  if(!ao_tgt[i])
  {
    release_lock(&ao_lock);
    logmsg(HHCAO017E);
    return;
  }

  /* check for empty cmd string */
  if(!strlen(arg))
  {
    release_lock(&ao_lock);
    logmsg(HHCAO018E);
    return;
  }

  /* check for hao command, prevent deadlock */
  for(i = 0; !strncasecmp(&arg[i], "herc ", 4); i += 5);
  if(!strcasecmp(&arg[i], "hao") || !strncasecmp(&arg[i], "hao ", 4))
  {
    release_lock(&ao_lock);
    logmsg(HHCA0026E);
    return;
  }

  /* check for possible loop */
  for(j = 0; j < HAO_MAXRULE; j++)
  {
    if(ao_tgt[j] && !regexec(&ao_preg[j], arg, 0, NULL, 0))
    {
      release_lock(&ao_lock);
      logmsg(HHCAO019E, j);
      return;
    }
  }

  /* duplicate the string */
  ao_cmd[i] = strdup(arg);

  /* check duplication */
  if(!ao_cmd[i])
  {
    release_lock(&ao_lock);
    logmsg(HHCAO015E, strerror(ENOMEM));
    return;
  }

  release_lock(&ao_lock);
  logmsg(HHCAO020I, i);
}

/*---------------------------------------------------------------------------*/
/* void hao_del(char *arg)                                                   */
/*                                                                           */
/* This function is called when the command hao del is given. the rule in    */
/* the given index is cleared.                                               */
/*---------------------------------------------------------------------------*/
static void hao_del(char *arg)
{
  int i;
  int rc;

  /* read the index number to delete */
  rc = sscanf(arg, "%d", &i);
  if(!rc || rc == -1)
  {
    logmsg(HHCAO023E);
    return;
  }

  /* check if index is valid */
  if(i < 0 || i >= HAO_MAXRULE)
  {
    logmsg(HHCAO009E, HAO_MAXRULE - 1);
    return;
  }

  /* serialize */
  obtain_lock(&ao_lock);

  /* check if entry exists */
  if(!ao_tgt[i])
  {
    release_lock(&ao_lock);
    logmsg(HHCAO024E, i);
    return;
  }

  /* delete the entry */
  free(ao_tgt[i]);
  ao_tgt[i] = NULL;
  regfree(&ao_preg[i]);
  if(ao_cmd[i])
  {
    free(ao_cmd[i]);
    ao_cmd[i] = NULL;
  }

  release_lock(&ao_lock);
  logmsg(HHCAO025I, i);
}

/*---------------------------------------------------------------------------*/
/* void hao_list(char *arg)                                                  */
/*                                                                           */
/* this function is called when the hao list command is given. It lists all  */
/* rules. When given an index, only that index will be showed.               */
/*---------------------------------------------------------------------------*/
static void hao_list(char *arg)
{
  int i;
  int rc;
  int size;

  rc = sscanf(arg, "%d", &i);
  if(!rc || rc == -1)
  {
    /* list all rules */
    logmsg(HHCAO004I);
    size = 0;

    /* serialize */
    obtain_lock(&ao_lock);

    for(i = 0; i < HAO_MAXRULE; i++)
    {
      if(ao_tgt[i])
      {
        logmsg(HHCAO005I, i, ao_tgt[i], (ao_cmd[i] ? ao_cmd[i] : "<not specified>"));
        size++;
      }
    }
    release_lock(&ao_lock);
    logmsg(HHCAO006I, size);
  }
  else
  {
    /* list specific index */
    if(i < 0 || i >= HAO_MAXRULE)
      logmsg(HHCAO009E, HAO_MAXRULE - 1);
    else
    {
      /* serialize */
      obtain_lock(&ao_lock);

      if(!ao_tgt[i])
        logmsg(HHCAO008E, i);
      else
        logmsg(HHCAO005I, i, ao_tgt[i], (ao_cmd[i] ? ao_cmd[i] : "not specified"));

      release_lock(&ao_lock);
    }
  }
}

/*---------------------------------------------------------------------------*/
/* void hao_clear(void)                                                      */
/*                                                                           */
/* This function is called when the hao clear command is given. This         */
/* function just clears all defined rules. Handy command for panic           */
/* situations.                                                               */
/*---------------------------------------------------------------------------*/
static void hao_clear(void)
{
  int i;

  /* serialize */
  obtain_lock(&ao_lock);

  /* clear all defined rules */
  for(i = 0; i < HAO_MAXRULE; i++)
  {
    if(ao_tgt[i])
    {
      free(ao_tgt[i]);
      ao_tgt[i] = NULL;
      regfree(&ao_preg[i]);
    }
    if(ao_cmd[i])
    {
      free(ao_cmd[i]);
      ao_cmd[i] = NULL;
    }
  }

  release_lock(&ao_lock);
  logmsg(HHCAO022I);
}

/*---------------------------------------------------------------------------*/
/* void* hao_thread(void* dummy)                                             */
/*                                                                           */
/* This thread is created by hao_initialize. It examines every message       */
/* printed. Here we check if a rule applies to the message. If so we fire    */
/* the command within the rule.                                              */
/*---------------------------------------------------------------------------*/
static void* hao_thread(void* dummy)
{
  char*  msgbuf  = NULL;
  int    msgidx  = -1;
  int    msgamt  = 0;
  char*  msgend  = NULL;
  char   svchar  = 0;

  UNREFERENCED(dummy);

  logmsg(HHCAO001I, thread_id(), getpriority(PRIO_PROCESS,0), getpid());

  /* Wait for panel thread to engage */
  while (!sysblk.panel_init && !sysblk.shutdown)
    usleep( 10 * 1000 );

  /* initialize message buffer */
  memset(ao_msgbuf, 0, sizeof(ao_msgbuf));

  /* Do until shutdown */
  while (!sysblk.shutdown && msgamt >= 0)
  {
    /* wait for message data */
    if ((msgamt = log_read(&msgbuf, &msgidx, LOG_BLOCK)) > 0 )
    {
      /* append to existing data */
      strlcat( ao_msgbuf, msgbuf, sizeof(ao_msgbuf) );
      msgbuf = ao_msgbuf;

      /* process only complete messages */
      while (NULL != (msgend = strchr(msgbuf,'\n')))
      {
        /* null terminate message */
        svchar = *(msgend+1);
        *(msgend+1) = 0;

        /* process message */
        hao_message(msgbuf);

        /* restore destroyed byte */
        *(msgend+1) = svchar;
        msgbuf = msgend+1;
      }

      /* shift message buffer */
      msgamt = (ao_msgbuf + sizeof(ao_msgbuf)) - msgbuf;
      memmove( ao_msgbuf, msgbuf, msgamt );
      ao_msgbuf[msgamt] = 0;
    }
  }

  logmsg(HHCAO002I);
  return NULL;
}

/*---------------------------------------------------------------------------*/
/* void hao_message(char *buf)                                               */
/*                                                                           */
/* This function is called by hao_thread whenever a message is about to be   */
/* printed. Here we check if a rule applies to the message. If so we fire    */
/* the command within the rule.                                              */
/*---------------------------------------------------------------------------*/
DLL_EXPORT void hao_message(char *buf)
{
  char work[HAO_WKLEN];
  regmatch_t rm;
  int i;

  /* copy and strip spaces */
  hao_cpstrp(work, buf);

  /* don't react on own messages */
  if(!strncmp(work, "HHCAO", 5))
    return;

  /* don't react on own commands */
  if(!strncasecmp(work, "hao", 3))
    return;

  /* also from the .rc file */
  if(!strncasecmp(work, "> hao", 5))
    return;

  /* serialize */
  obtain_lock(&ao_lock);

  /* check all defined rules */
  for(i = 0; i < HAO_MAXRULE; i++)
  {
    if(ao_tgt[i] && ao_cmd[i])  /* complete rule defined in this slot? */
    {
      /* does this rule match our message? */
      if (regexec(&ao_preg[i], work, 1, &rm, 0) == 0)
      {
        /* issue command for this rule */
        logmsg(HHCAO003I, ao_cmd[i]);
        panel_command(ao_cmd[i]);
      }
    }
  }
  release_lock(&ao_lock);
}

#endif /* defined(OPTION_HAO) */

///////////////////////////////////////////////////////////////////////////////
// Fish notes for possible future enhancement: I was thinking we should
// probably at some point support command rules that allow a custom
// command to be constructed based on the string(s) that was/were found.

#if 0
    /*
        The following demonstrates how the REG_NOTBOL flag could be used
        with regexec() to find all substrings in a line that match a pattern
        supplied by a user. (For simplicity of the example, very little
        error checking is done.) 
    */

    regex_t     re;         /*  regular expression  */
    regmatch_t  pm;         /*  match info          */
    int         error;      /*  rc from regexec     */

    char        buffer[]   =  "The quick brown dog jumps over the lazy fox";
    char        pattern[]  =  "lazy";

    /*  Compile the search pattern  */

    (void)regcomp( &re, pattern, 0 );

    /*  This call to regexec() finds the first match on the line  */

    error = regexec( &re, &buffer[0], 1, &pm, 0 );

    while (error == 0)      /* while matches found */
    {
        /*  Substring found between 'pm.rm_so' and 'pm.rm_eo'  */
        /*  This call to regexec() finds the next match    */

        error = regexec( &re, buffer + pm.rm_eo, 1, &pm, REG_NOTBOL );
    }

#endif
///////////////////////////////////////////////////////////////////////////////

/* HAO.C        (c) Copyright Bernard van der Helm, 2002-2011        */
/*             Hercules Automatic Operator Implementation            */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

/*---------------------------------------------------------------------------*/
/* file: hao.c                                                               */
/*                                                                           */
/* Implementation of the automatic operator withing the Hercules emulator.   */
/*                                                                           */
/*                            (c) Copyright Bernard van der Helm, 2002-2011  */
/*                            Noordwijkerhout, The Netherlands.              */
/*---------------------------------------------------------------------------*/



#include "hstdinc.h"

#define _HAO_C_
#define _HENGINE_DLL_

#include "hercules.h"

#if defined(OPTION_HAO)

/*---------------------------------------------------------------------------*/
/* constants                                                                 */
/*---------------------------------------------------------------------------*/
#define HAO_WKLEN    256    /* (maximum message length able to tolerate) */
#define HAO_MAXRULE  64     /* (purely arbitrary and easily increasable) */

/*---------------------------------------------------------------------------*/
/* local variables                                                           */
/*---------------------------------------------------------------------------*/
static TID      haotid;                         /* Herc Auto-Oper thread-id  */
static LOCK     ao_lock;
static regex_t  ao_preg[HAO_MAXRULE];
static char    *ao_cmd[HAO_MAXRULE];
static char    *ao_tgt[HAO_MAXRULE];
static char     ao_msgbuf[LOG_DEFSIZE+1];   /* (plus+1 for NULL termination) */

/*---------------------------------------------------------------------------*/
/* function prototypes                                                       */
/*---------------------------------------------------------------------------*/
DLL_EXPORT void  hao_command(char *cmd);
static     int   hao_initialize(void);
static     void  hao_message(char *buf);
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
static int hao_initialize(void)
{
  int i = 0;
  int rc;

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
  rc = create_thread (&haotid, JOINABLE, hao_thread, NULL, "hao_thread");
  if(rc)
  {
    i = FALSE;
    WRMSG(HHC00102, "E", strerror(rc));
  }
  else
    i = TRUE;

  release_lock(&ao_lock);

  return(i);
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

  /* initialise hao */
  if(!haotid && !hao_initialize())
      WRMSG(HHC01404, "S");

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

  WRMSG(HHC00070, "E");
}

/*---------------------------------------------------------------------------*/
/* hao_cpstrp(char *dest, char *src)                                         */
/*                                                                           */
/* This function copies the string from src to dest, without the leading     */
/* or trailing spaces.                                                       */
/*---------------------------------------------------------------------------*/
static void hao_cpstrp(char *dest, char *src)
{
  int i;

  for(i = 0; src[i] == ' '; i++);
  strncpy(dest, &src[i], HAO_WKLEN);
  dest[HAO_WKLEN-1] = 0;
  for(i = (int)strlen(dest); i && dest[i - 1] == ' '; i--);
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
    WRMSG(HHC00071, "E", "target", HAO_MAXRULE);
    return;
  }

  /* check if not command is expected */
  for(j = 0; j < HAO_MAXRULE; j++)
  {
    if(ao_tgt[j] && !ao_cmd[j])
    {
      release_lock(&ao_lock);
      WRMSG(HHC00072, "E", "tgt", "cmd");
      return;
    }
  }

  /* check for empty target */
  if(!strlen(arg))
  {
    release_lock(&ao_lock);
    WRMSG(HHC00073, "E", "target");
    return;
  }

  /* check for duplicate targets */
  for(j = 0; j < HAO_MAXRULE; j++)
  {
    if(ao_tgt[j] && !strcmp(arg, ao_tgt[j]))
    {
      release_lock(&ao_lock);
      WRMSG(HHC00074, "E", j);
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
    WRMSG(HHC00075, "E", "regcomp()", work);
    return;
  }

  /* check for possible loop */
  for(j = 0; j < HAO_MAXRULE; j++)
  {
    if(ao_cmd[j] && !regexec(&ao_preg[i], ao_cmd[j], 0, NULL, 0))
    {
      release_lock(&ao_lock);
      regfree(&ao_preg[i]);
      WRMSG(HHC00076, "E", "target", "command", i);
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
    WRMSG(HHC00075, "E", "strdup()", strerror(ENOMEM));
    return;
  }

  release_lock(&ao_lock);
  WRMSG(HHC00077, "I", "target", i);
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
    WRMSG(HHC00071, "E", "command", HAO_MAXRULE);
    return;
  }

  /* check if target is given */
  if(!ao_tgt[i])
  {
    release_lock(&ao_lock);
    WRMSG(HHC00072, "E", "cmd", "tgt");
    return;
  }

  /* check for empty cmd string */
  if(!strlen(arg))
  {
    release_lock(&ao_lock);
    WRMSG(HHC00073, "E", "command");
    return;
  }

  /* check for hao command, prevent deadlock */
  for(j = 0; !strncasecmp(&arg[j], "herc ", 4); j += 5);
  if(!strcasecmp(&arg[j], "hao") || !strncasecmp(&arg[j], "hao ", 4))
  {
    release_lock(&ao_lock);
    WRMSG(HHC00078, "E");
    return;
  }

  /* check for possible loop */
  for(j = 0; j < HAO_MAXRULE; j++)
  {
    if(ao_tgt[j] && !regexec(&ao_preg[j], arg, 0, NULL, 0))
    {
      release_lock(&ao_lock);
      WRMSG(HHC00076, "E", "command", "target", j);
      return;
    }
  }

  /* duplicate the string */
  ao_cmd[i] = strdup(arg);

  /* check duplication */
  if(!ao_cmd[i])
  {
    release_lock(&ao_lock);
    WRMSG(HHC00075, "E", "strdup()", strerror(ENOMEM));
    return;
  }

  release_lock(&ao_lock);
  WRMSG(HHC00077, "I", "command", i);
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
    WRMSG(HHC00083, "E");
    return;
  }

  /* check if index is valid */
  if(i < 0 || i >= HAO_MAXRULE)
  {
    WRMSG(HHC00084, "E", HAO_MAXRULE - 1);
    return;
  }

  /* serialize */
  obtain_lock(&ao_lock);

  /* check if entry exists */
  if(!ao_tgt[i])
  {
    release_lock(&ao_lock);
    WRMSG(HHC00085, "E", i);
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
  WRMSG(HHC00086, "I", i);
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
    size = 0;

    /* serialize */
    obtain_lock(&ao_lock);

    for(i = 0; i < HAO_MAXRULE; i++)
    {
      if(ao_tgt[i])
      {
        if(!size)
          WRMSG(HHC00087, "I");
        WRMSG(HHC00088, "I", i, ao_tgt[i], (ao_cmd[i] ? ao_cmd[i] : "not specified"));
        size++;
      }
    }
    release_lock(&ao_lock);
    if(!size)
        WRMSG(HHC00089, "I");
    else
        WRMSG(HHC00082, "I", size);
  }
  else
  {
    /* list specific index */
    if(i < 0 || i >= HAO_MAXRULE)
      WRMSG(HHC00084, "E", HAO_MAXRULE - 1);
    else
    {
      /* serialize */
      obtain_lock(&ao_lock);

      if(!ao_tgt[i])
        WRMSG(HHC00079, "E", i);
      else
        WRMSG(HHC00088, "I", i, ao_tgt[i], (ao_cmd[i] ? ao_cmd[i] : "not specified"));

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
  WRMSG(HHC00080, "I");
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
  int    bufamt  = 0;

  UNREFERENCED(dummy);

  WRMSG(HHC00100, "I", (u_long)thread_id(), getpriority(PRIO_PROCESS,0), "Hercules Automatic Operator");

  /* Wait for panel thread to engage */
  while(!sysblk.panel_init && !sysblk.shutdown && !logger_status() )
    usleep( 10 * 1000 );

  /* Do until shutdown */
  while(!sysblk.shutdown && msgamt >= 0)
  {
    /* wait for message data */
    msgamt = log_read(&msgbuf, &msgidx, LOG_BLOCK);
    if(msgamt > 0)
    {
      /* append to existing data */
      if(msgamt > (int)((sizeof(ao_msgbuf) - 1) - bufamt))
        msgamt = (int)((sizeof(ao_msgbuf) - 1) - bufamt);
      strncpy( &ao_msgbuf[bufamt], msgbuf, msgamt );
      ao_msgbuf[bufamt += msgamt] = 0;
      msgbuf = ao_msgbuf;

      /* process only complete messages */
      msgend = strchr(msgbuf,'\n');
      while(msgend)
      {
        /* null terminate message */
        svchar = *(msgend+1);
        *(msgend+1) = 0;

        /* process message */
        hao_message(msgbuf);

        /* restore destroyed byte */
        *(msgend+1) = svchar;
        msgbuf = msgend+1;
        msgend = strchr(msgbuf,'\n');
      }

      /* shift message buffer */
      memmove( ao_msgbuf, msgbuf, bufamt -= (msgbuf - ao_msgbuf) );
    }
  }

  WRMSG(HHC00101, "I", (u_long)thread_id(), getpriority(PRIO_PROCESS,0), "Hercules Automatic Operator");
  return NULL;
}

/*---------------------------------------------------------------------------*/
/* int hao_ignoremsg(char *msg)                                              */
/*                                                                           */
/* This function determines whether the message being processed should be    */
/* ignored or not (so we don't react to it).  Returns 1 (TRUE) if message    */
/* should be ignored, or 0 == FALSE if message should be processed.          */
/*---------------------------------------------------------------------------*/
static int hao_ignoremsg(char *msg)
{
#if defined(OPTION_MSGCLR) || defined(OPTION_MSGHLD)
  static int debuglen = 0;
#endif
  char* nocolor = msg;
  int msglen;

#if defined( OPTION_MSGCLR )
  /* Get past color string if there is one */
  if (!(msglen = skippnlpfx( &nocolor )))       /* Skip past <pnl pfx  */
      return TRUE;                              /* Ignore if now empty */
  if (nocolor > msg)                            /* Color prefix found? */
    memmove( msg, nocolor, msglen+1 );          /* Remove color prefix */
#else /* defined( OPTION_MSGCLR ) */
  msglen = strlen(msg);
#endif /* defined( OPTION_MSGCLR ) */

#if defined(OPTION_MSGCLR) || defined(OPTION_MSGHLD)
  if (!debuglen)
  {
    char prefix[64] = {0};
    MSGBUF( prefix, MLVL_DEBUG_PRINTF_PATTERN, "foo", 999 );
    debuglen = (int)strlen( prefix );
  }

  /* Get past debug prefix if msglevel DEBUG is active */
  if (MLVL( DEBUG ) && msglen >= debuglen)
    memmove( msg, msg + debuglen, (msglen -= debuglen)+1 );
#endif

  /* Ignore our own messages (HHC0007xx, HHC0008xx and HHC0009xx
     are reserved so that hao.c can recognize its own messages) */
  if (0
      || !strncasecmp( msg, "HHC0007", 7 )
      || !strncasecmp( msg, "HHC0008", 7 )
      || !strncasecmp( msg, "HHC0009", 7 )
  )
    return TRUE;  /* (it's one of our hao messages; ignore it) */

  /* To be extra safe, ignore any messages with the string "hao" in them */
  if (0
      || !strncasecmp( msg, "HHC00013I Herc command: 'hao ",      29 )
      || !strncasecmp( msg, "HHC00013I Herc command: 'herc hao ", 34 )
      || !strncasecmp( msg, "HHC01603I hao ",                     14 )
      || !strncasecmp( msg, "HHC01603I herc hao ",                19 )
  )
    return TRUE;  /* (it's one of our hao messages; ignore it) */

  /* Same idea but for messages logged as coming from the .rc file */
  if (!strncasecmp( msg, "> hao ", 6 ))
    return TRUE;

  return FALSE;   /* (message appears to be one we should process) */
}

/*---------------------------------------------------------------------------*/
/* void hao_message(char *buf)                                               */
/*                                                                           */
/* This function is called by hao_thread whenever a message is about to be   */
/* printed. Here we check if a rule applies to the message. If so we fire    */
/* the command within the rule.                                              */
/*---------------------------------------------------------------------------*/
static void hao_message(char *buf)
{
  char work[HAO_WKLEN];
  regmatch_t rm;
  int i;

  /* copy and strip spaces */
  hao_cpstrp(work, buf);

  /* strip the herc prefix */
  while(!strncmp(work, "herc", 4))
    hao_cpstrp(work, &work[4]);

  /* Ignore the message if we should (e.g. if one of our own!) */
  if (hao_ignoremsg( work ))
      return;

  /* serialize */
  obtain_lock(&ao_lock);

  /* check all defined rules */
  for(i = 0; i < HAO_MAXRULE; i++)
  {
    if(ao_tgt[i] && ao_cmd[i])  /* complete rule defined in this slot? */
    {
      /* does this rule match our message? */
      if(!regexec(&ao_preg[i], work, 1, &rm, 0))
      {
        /* issue command for this rule */
        WRMSG(HHC00081, "I", i, ao_cmd[i]);
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

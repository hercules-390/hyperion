/* HERCLIN.C     (c) Copyright Roger Bowler, 2005-2009               */
/*              Hercules Interface Control Program                   */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

/************************************************/
/* (C) Copyright 2005-2009 Roger Bowler & Others*/
/* Initial author : Ivan Warren                 */
/*                                              */
/* HERCLIN.C                                    */
/*                                              */
/* THIS IS SAMPLE CODE DEMONSTRATING            */
/* THE USE OF THE INITIAL HARDWARE PANEL        */
/* INTERFACE FEATURE.                           */
/************************************************/

#if 1 // fish test: we SHOULD be doing it this way! Why aren't we?!

  #include "hstdinc.h"
  #include "hercules.h"

#else

  #ifdef _MSVC_
  #include <windows.h>
  #include <conio.h>
  #endif
  #ifdef HAVE_CONFIG_H
    #include <config.h> // Hercules build configuration options/settings
  #endif
  #include <stdio.h>
  #include <stdlib.h>
  #include <string.h>
  #include <sys/mman.h>

#endif
#include "hextapi.h"

/**********************************************/
/* The following function is the LOG callback */
/* function. It gets called by the engine     */
/* whenever a log message needs to be         */
/* displayed. This function may therefore be  */
/* invoked from a separate thread             */
/*                                            */
/* This simple version simply writes to STDOUT*/
/**********************************************/

#ifdef _MSVC_
static HANDLE hLogCallbackThread = NULL;
#endif

void mywrite(const char *a,size_t b)
{
#ifdef _MSVC_
    if (!hLogCallbackThread)
        hLogCallbackThread = OpenThread(
            SYNCHRONIZE, FALSE,
            GetCurrentThreadId());
#endif

    fflush(stdout);
    fwrite(a,b,1,stdout);
    fflush(stdout);
}


int main(int ac,char **av)
{
    /*****************************************/
    /* COMMANDHANDLER is the function type   */
    /* of the engine's panel command handler */
    /* this MUST be resolved at run time     */
    /* since some HDL module might have      */
    /* redirected the initial engine function*/
    /*****************************************/

    COMMANDHANDLER  ch;
    char *str,*bfr;

#if defined( OPTION_DYNAMIC_LOAD ) && defined( HDL_USE_LIBTOOL )
    /* LTDL Preloaded symbols for HDL using libtool */
    LTDL_SET_PRELOADED_SYMBOLS();
#endif

    /******************************************/
    /* Register the 'mywrite' function as the */
    /* log callback routine                   */
    /******************************************/
    registerLogCallback(mywrite);

    /******************************************/
    /* Initialize the HERCULE Engine          */
    /******************************************/
    impl(ac,av);

    /******************************************/
    /* Get the command handler function       */
    /* This MUST be done after IML            */
    /******************************************/
    ch=getCommandHandler();

    /******************************************/
    /* Read STDIN and pass to Command Handler */
    /******************************************/
    bfr=(char *)malloc(1024);
    while
    (
#ifdef _MSVC_
        !hLogCallbackThread ||
        WaitForSingleObject(hLogCallbackThread,0)
            != WAIT_OBJECT_0
#else
        1
#endif
    )
    {
#ifdef _MSVC_
        if (!kbhit()) Sleep(50); else
#endif
        if ((str=fgets(bfr,1024,stdin)))
        {
            str[strlen(str)-1]=0;
            ch(str);
        }
    }
#ifdef _MSVC_
    CloseHandle(hLogCallbackThread);
#endif
    return 0;
}


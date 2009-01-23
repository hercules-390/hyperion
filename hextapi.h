/********************************************************/
/* HEXTAPI.H : Definition of Hercules External (public) */
/*             APIs                                     */
/*  (c) Copyright 2005-2009 Roger Bowler & Others       */
/*  This file originally written by Ivan Warren         */
/*  THE STATE OF THIS API IS NOT YET FINALIZED          */
/*  AND THEREFORE, THE INTERFACE MAY CHANGE             */
/********************************************************/

// $Id$
//
// $Log$
// Revision 1.5  2007/06/23 00:04:11  ivan
// Update copyright notices to include current year (2007)
//
// Revision 1.4  2006/12/08 09:43:26  jj
// Add CVS message log
//

#ifndef _HEXTAPI_H_
#define _HEXTAPI_H_

#if defined(_MSVC_) && defined(HERC_DLL_BUILD)
#define DLL_IMPORT __declspec(dllimport)
#else
#define DLL_IMPORT extern
#endif

typedef void (*LOGCALLBACK)(const char *,size_t);
typedef void (*COMMANDHANDLER)(void *);

#ifdef __cplusplus
extern "C" {
#endif

    /* LOG Callback */
    DLL_IMPORT void registerLogCallback(LOGCALLBACK);
    /* Panel Commands */
    DLL_IMPORT COMMANDHANDLER getCommandHandler(void);
    /* IMPL */
    DLL_IMPORT int impl(int ac,char **av);
#ifdef __cplusplus
}
#endif

#endif

/********************************************************/
/* HEXTAPI.H : Definition of Hercules External (public) */
/*             APIs                                     */
/*  (c) Copyright 2005-2006 Roger Bowler & Others       */
/*  This file originally written by Ivan Warren         */
/*  THE STATE OF THIS API IS NOT YET FINALIZED          */
/*  AND THEREFORE, THE INTERFACE MAY CHANGE             */
/********************************************************/

// $Id$
//
// $Log$

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

/////////////////////////////////////////////////////////////////////////////////////////
// dynguiv.h  --  defines the build version for the product itself
//                (c) Copyright "Fish" (David B. Trout), 2003-2005
/////////////////////////////////////////////////////////////////////////////////////////

#ifndef _DYNGUIV_H_
#define _DYNGUIV_H_

///////////////////////////////////////////////////////////////////////////////////////

#include "dynguib.h"        // (where BUILDCOUNT_STR and BUILDCOUNT_NUM are defined)

///////////////////////////////////////////////////////////////////////////////////////

#define DYNGUI_VERMAJOR_NUM   1    // MAJOR Release (program radically changed)
#define DYNGUI_VERMAJOR_STR  "1"

#define DYNGUI_VERINTER_NUM   0    // Minor Enhancements (new features, etc)
#define DYNGUI_VERINTER_STR  "0"

#define DYNGUI_VERMINOR_NUM   0    // Bug Fix
#define DYNGUI_VERMINOR_STR  "0"

#if defined(DEBUG) || defined(_DEBUG)
#define DYNGUI_VERSION  DYNGUI_VERMAJOR_STR "." DYNGUI_VERINTER_STR "." DYNGUI_VERMINOR_STR "." BUILDCOUNT_STR "-D"
#else
#define DYNGUI_VERSION  DYNGUI_VERMAJOR_STR "." DYNGUI_VERINTER_STR "." DYNGUI_VERMINOR_STR "." BUILDCOUNT_STR
#endif

///////////////////////////////////////////////////////////////////////////////////////

#endif // _DYNGUIV_H_

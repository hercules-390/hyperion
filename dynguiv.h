/////////////////////////////////////////////////////////////////////////////////////////
// dynguiv.h  --  defines the build version for the product itself
//                (c) Copyright "Fish" (David B. Trout), 2003
/////////////////////////////////////////////////////////////////////////////////////////

#ifndef _DYNGUIV_H_
#define _DYNGUIV_H_

///////////////////////////////////////////////////////////////////////////////////////

#include "dynguib.h"        // (where BUILDCOUNT_STR and BUILDCOUNT_NUM are defined)

///////////////////////////////////////////////////////////////////////////////////////

#define VERMAJOR_NUM   1    // MAJOR Release (program radically changed)
#define VERMAJOR_STR  "1"

#define VERINTER_NUM   0    // Minor Enhancements (new features, etc)
#define VERINTER_STR  "0"

#define VERMINOR_NUM   0    // Bug Fix
#define VERMINOR_STR  "0"

#if defined(DEBUG) || defined(_DEBUG)
#define VERSION  VERMAJOR_STR "." VERINTER_STR "." VERMINOR_STR "." BUILDCOUNT_STR "-D"
#else
#define VERSION  VERMAJOR_STR "." VERINTER_STR "." VERMINOR_STR "." BUILDCOUNT_STR
#endif

///////////////////////////////////////////////////////////////////////////////////////

#endif // _DYNGUIV_H_

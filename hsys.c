#include "hercules.h"

SYSBLK sysblk;

#if defined(EXTERNALGUI)
int extgui;
#endif

#if defined(OPTION_W32_CTCI)
int (*debug_tt32_stats)(int);
#endif

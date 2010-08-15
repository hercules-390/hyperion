/* S37X.C  (c) Copyright Ivan S. Warren 2010                         */
/*            Optional S370 Extensions                               */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id: s37x.c 6313 2010-08-14 09:27:49Z bernard $

#include "hstdinc.h"
#include "hercules.h"
#include "hdl.h"
#include "opcode.h"

#if defined(OPTION_370_EXTENSION)

DLL_IMPORT void s37x_replace_opcode_scan(int x);

HDL_REGISTER_SECTION;
{
    s37x_replace_opcode_scan(1);
}
END_REGISTER_SECTION

/* Module end : restore the instruction functions */
HDL_FINAL_SECTION;
{
    s37x_replace_opcode_scan(0);
}
END_FINAL_SECTION

#if 0
/* Only need this to get the actual resolver entry point */
/* resolution will be done later on */
HDL_RESOLVER_SECTION;
{
    {
        rep_opcode_scan(1);
    }
    else
    {
        logmsg("the S/370 Extension instruction replacement entry point was not found\n");
    }
}
END_RESOLVER_SECTION;
#endif

#else /* defined(OPTION_370_EXTENSION) */

HDL_REGISTER_SECTION;
{
    logmsg("the S/370 Extension module requires the OPTION_S370_EXTENSION build option\n");
    return -1;
}
END_REGISTER_SECTION


#endif /* defined(OPTION_370_EXTENSION) */

HDL_DEPENDENCY_SECTION;
{
     HDL_DEPENDENCY (HERCULES);

} END_DEPENDENCY_SECTION

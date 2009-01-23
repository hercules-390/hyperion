/* LOADPARM.C   (c) Copyright Jan Jaeger, 2004-2009                  */
/*              SCLP / MSSF loadparm                                 */

// $Id$

/*-------------------------------------------------------------------*/
/* This module contains functions which set, copy, and retrieve the  */
/* values of the LOADPARM and LPARNAME parameters                    */
/*-------------------------------------------------------------------*/

// $Log$
// Revision 1.13  2008/12/30 15:40:01  rbowler
// Allow $(LPARNAME) in herclogo file
//
// Revision 1.12  2007/06/23 00:04:14  ivan
// Update copyright notices to include current year (2007)
//
// Revision 1.11  2006/12/08 09:43:28  jj
// Add CVS message log
//

#include "hstdinc.h"

#define _HENGINE_DLL_
#define _LOADPARM_C_

#include "hercules.h"


static BYTE loadparm[8] = {0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40};


void set_loadparm(char *name)
{
    size_t i;

    for(i = 0; name && i < strlen(name) && i < sizeof(loadparm); i++)
    if(isprint(name[i]))
            loadparm[i] = host_to_guest((int)(islower(name[i]) ? toupper(name[i]) : name[i]));
        else
            loadparm[i] = 0x40;
    for(; i < sizeof(loadparm); i++)
        loadparm[i] = 0x40;
}


void get_loadparm(BYTE *dest)
{
    memcpy(dest, loadparm, sizeof(loadparm));
}


char *str_loadparm()
{
    static char ret_loadparm[sizeof(loadparm)+1];
    int i;

    ret_loadparm[sizeof(loadparm)] = '\0';
    for(i = sizeof(loadparm) - 1; i >= 0; i--)
    {
        ret_loadparm[i] = guest_to_host((int)loadparm[i]);

        if(isspace(ret_loadparm[i]) && !ret_loadparm[i+1])
            ret_loadparm[i] = '\0';
    }

    return ret_loadparm;
}


static BYTE lparname[8] = {0xC8, 0xC5, 0xD9, 0xC3, 0xE4, 0xD3, 0xC5, 0xE2};
                          /* HERCULES */

void set_lparname(char *name)
{
    size_t i;

    for(i = 0; name && i < strlen(name) && i < sizeof(lparname); i++)
        if(isprint(name[i]))
            lparname[i] = host_to_guest((int)(islower(name[i]) ? toupper(name[i]) : name[i]));
        else
            lparname[i] = 0x40;
    for(; i < sizeof(lparname); i++)
        lparname[i] = 0x40;
}


void get_lparname(BYTE *dest)
{
    memcpy(dest, lparname, sizeof(lparname));
}


LOADPARM_DLL_IMPORT
char *str_lparname()
{
    static char ret_lparname[sizeof(lparname)+1];
    int i;

    ret_lparname[sizeof(lparname)] = '\0';
    for(i = sizeof(lparname) - 1; i >= 0; i--)
    {
        ret_lparname[i] = guest_to_host((int)lparname[i]);

        if(isspace(ret_lparname[i]) && !ret_lparname[i+1])
            ret_lparname[i] = '\0';
    }

    return ret_lparname;
}

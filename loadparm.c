/* LOADPARM.C   (c) Copyright Jan Jaeger, 2004                       */
/*              SCLP / MSSF loadparm                                 */

#include "hercules.h"


static BYTE loadparm[8] = {0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40};


void set_loadparm(char *loadpstr)
{
    int i;

    for(i = 0; loadpstr && i < strlen(loadpstr) && i < 8; i++)
	if(isprint(loadpstr[i]))
            loadparm[i] = host_to_guest((int)(islower(loadpstr[i]) ? toupper(loadpstr[i]) : loadpstr[i]));
        else
            loadparm[i] = 0x40;
    for(; i < 8; i++)
        loadparm[i] = 0x40;
}


void get_loadparm(BYTE *dest)
{
    memcpy(dest,loadparm, 8);
}


char *str_loadparm()
{
    static char ret_loadparm[9];
    int i;

    ret_loadparm[8] = '\0';
    for(i = 7; i >= 0; i--)
        ret_loadparm[i] = guest_to_host((int)loadparm[i]);

    return ret_loadparm;
}

/* LOGMSG2.C    (c) Copyright Roger Bowler, 2005                     */
/*              Message log function for batch programs              */

/* This file and the executable program(s) generated from it are     */
/* subject to the terms of the Hercules Public Licence Version 2     */

#include "hstdinc.h"

#include "hercules.h"

/*-------------------------------------------------------------------*/
/* Log message: Device trace                                         */
/*-------------------------------------------------------------------*/
void logdevtr(DEVBLK *dev,char *msg,...)
{
va_list vl;

    if(dev->ccwtrace||dev->ccwstep) 
    { 
        logmsg("%4.4X:",dev->devnum); 
        va_start(vl,msg);
        vprintf(msg,vl);
    } 
} /* end function logdevtr */ 

/*-------------------------------------------------------------------*/
/* Log message: Normal routing to stdout                             */
/*-------------------------------------------------------------------*/
void logmsg(char *msg,...)
{
va_list vl;

    va_start(vl,msg);
    vprintf(msg,vl); 
} /* end function logmsg */

/*-------------------------------------------------------------------*/
/* Log message: Normal routing with vararg pointer                   */
/*-------------------------------------------------------------------*/
void vlogmsg(char *msg, va_list vl)
{
    vprintf(msg,vl); 
} /* end function vlogmsg */ 


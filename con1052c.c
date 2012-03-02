/* CON1052.C    (c)Copyright Jan Jaeger, 2004-2011                   */
/*              Emulated 1052 on hercules console                    */

// $Id$

#include "hstdinc.h"

#include "hercules.h"

#include "devtype.h"

#include "opcode.h"

#include "sr.h"

#undef MOD
#define MOD "CON"

#if defined(OPTION_DYNAMIC_LOAD) && defined(WIN32) && !defined(HDL_USE_LIBTOOL) && !defined(_MSVC_)
 SYSBLK *psysblk;
 #define sysblk (*psysblk)
void* (*panel_command) (void*);
#endif

#if defined(OPTION_DYNAMIC_LOAD)
static void* con1052_panel_command  (char *cmd);
#endif

#define BUFLEN_1052     150             /* 1052 Send/Receive buffer  */

/*-------------------------------------------------------------------*/
/* Ivan Warren 20040227                                              */
/* This table is used by channel.c to determine if a CCW code is an  */
/* immediate command or not                                          */
/* The tape is addressed in the DEVHND structure as 'DEVIMM immed'   */
/* 0 : Command is NOT an immediate command                           */
/* 1 : Command is an immediate command                               */
/* Note : An immediate command is defined as a command which returns */
/* CE (channel end) during initialisation (that is, no data is       */
/* actually transfered. In this case, IL is not indicated for a CCW  */
/* Format 0 or for a CCW Format 1 when IL Suppression Mode is in     */
/* effect                                                            */
/*-------------------------------------------------------------------*/
static BYTE con1052_immed[256]=
 /* 0 1 2 3 4 5 6 7 8 9 A B C D E F */
  { 0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,  /* 00 */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* 10 */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* 20 */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* 30 */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* 40 */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* 50 */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* 60 */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* 70 */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* 80 */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* 90 */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* A0 */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* B0 */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* C0 */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* D0 */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* E0 */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}; /* F0 */

/*-------------------------------------------------------------------*/
/* INITIALIZE THE 1052/3215 DEVICE HANDLER                           */
/*-------------------------------------------------------------------*/
static int
con1052_init_handler ( DEVBLK *dev, int argc, char *argv[] )
{
    int ac=0;

    /* reset excp count */
    dev->excps = 0;

    /* Integrated console is always connected */
    dev->connected = 1;

    /* Set number of sense bytes */
    dev->numsense = 1;

    /* Initialize device dependent fields */
    dev->keybdrem = 0;

    /* Set length of print buffer */
    dev->bufsize = BUFLEN_1052;

    /* Assume we want to prompt */
    dev->prompt1052 = 1;

    /* Default command character is "/" */
    strlcpy(dev->filename,"/",sizeof(dev->filename));

    /* Is there an argument? */
    if (argc > 0)
    {
        /* Look at the argument and set noprompt flag if specified. */
        if (strcasecmp(argv[ac], "noprompt") == 0)
        {
            dev->prompt1052 = 0;
            ac++; argc--;
        }
        else
            strlcpy(dev->filename,argv[ac],sizeof(dev->filename));
    }

    if(!sscanf(dev->typname,"%hx",&(dev->devtype)))
        dev->devtype = 0x1052;

    /* Initialize the device identifier bytes */
    dev->devid[0] = 0xFF;
    dev->devid[1] = dev->devtype >> 8;
    dev->devid[2] = dev->devtype & 0xFF;
    dev->devid[3] = 0x00;
    dev->devid[4] = dev->devtype >> 8;
    dev->devid[5] = dev->devtype & 0xFF;
    dev->devid[6] = 0x00;
    dev->numdevid = 7;

    return 0;
} /* end function con1052_init_handler */


/*-------------------------------------------------------------------*/
/* QUERY THE 1052/3215 DEVICE DEFINITION                             */
/*-------------------------------------------------------------------*/
static void
con1052_query_device (DEVBLK *dev, char **devclass,
                int buflen, char *buffer)
{
    BEGIN_DEVICE_CLASS_QUERY( "CON", dev, devclass, buflen, buffer );

    snprintf(buffer, buflen-1,
        "*syscons cmdpref(%s)%s IO[%" I64_FMT "u]",
        dev->filename,
        !dev->prompt1052 ? " noprompt" : "",
        dev->excps );

} /* end function con1052_query_device */


/*-------------------------------------------------------------------*/
/* CLOSE THE 1052/3215 DEVICE HANDLER                                */
/*-------------------------------------------------------------------*/
static int
con1052_close_device ( DEVBLK *dev )
{
    UNREFERENCED(dev);

    return 0;
} /* end function con1052_close_device */


/*-------------------------------------------------------------------*/
/* EXECUTE A 1052/3215 CHANNEL COMMAND WORD                          */
/*-------------------------------------------------------------------*/
static void
con1052_execute_ccw ( DEVBLK *dev, BYTE code, BYTE flags,
        BYTE chained, U16 count, BYTE prevcode, int ccwseq,
        BYTE *iobuf, BYTE *more, BYTE *unitstat, U16 *residual )
{
int     len;                            /* Length of data            */
int     num;                            /* Number of bytes to move   */
BYTE    c;                              /* Print character           */

    UNREFERENCED(chained);
    UNREFERENCED(prevcode);
    UNREFERENCED(ccwseq);

    /* Unit check with intervention required if no client connected */
    if (dev->connected == 0 && !IS_CCW_SENSE(code))
    {
        dev->sense[0] = SENSE_IR;
        *unitstat = CSW_UC;
        return;
    }

    /* Process depending on CCW opcode */
    switch (code) {

    case 0x01:
    /*---------------------------------------------------------------*/
    /* WRITE NO CARRIER RETURN                                       */
    /*---------------------------------------------------------------*/

    case 0x09:
    /*---------------------------------------------------------------*/
    /* WRITE AUTO CARRIER RETURN                                     */
    /*---------------------------------------------------------------*/

        /* Calculate number of bytes to write and set residual count */
        num = (count < BUFLEN_1052) ? count : BUFLEN_1052;
        *residual = count - num;

        /* Translate data in channel buffer to ASCII */
        for (len = 0; len < num; len++)
        {
            c = guest_to_host(iobuf[len]);
            if (!isprint(c) && c != 0x0a && c != 0x0d) c = SPACE;
            iobuf[len] = c;
        } /* end for(len) */

        /* Perform end of record processing if not data-chaining,
           and append carriage return and newline if required */
        if ((flags & CCW_FLAGS_CD) == 0
          && len < BUFLEN_1052)
            iobuf[len++] = '\n';

        iobuf[len] = '\0';

        /* process multiline messages */
        {
            char    *strtok_str = NULL;
            char    *str = strtok_r ((char *)iobuf,"\n", &strtok_str);

            while (str != NULL)
            {
#ifdef OPTION_MSGCLR
    #ifdef OPTION_SCP_MSG_PREFIX
                WRCMSG ("<pnl,color(green,black)>", HHC00001, "I", str);
    #else /*!OPTION_SCP_MSG_PREFIX*/
                logmsg ("<pnl,color(green,black)>%s\n", str);
    #endif /*OPTION_SCP_MSG_PREFIX*/
#else /*!OPTION_MSGCLR*/
    #ifdef OPTION_SCP_MSG_PREFIX
                WRMSG (HHC00001, "I", str);
    #else /*!OPTION_SCP_MSG_PREFIX*/
                logmsg ("%s\n", str);
    #endif /*OPTION_SCP_MSG_PREFIX*/
#endif /*OPTION_MSGCLR*/
                str = strtok_r (NULL, "\n", &strtok_str);
            }

        }

        /* Return normal status */
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x03:
    /*---------------------------------------------------------------*/
    /* CONTROL NO-OPERATION                                          */
    /*---------------------------------------------------------------*/
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x0A:
    /*---------------------------------------------------------------*/
    /* READ INQUIRY                                                  */
    /*---------------------------------------------------------------*/

        /* Solicit console input if no data in the device buffer */
        if (!dev->keybdrem)
        {
            /* Display prompting message on console if allowed */
            if (dev->prompt1052)
                WRCMSG ("<pnl,color(lightyellow,black)>", HHC00010, "A", SSID_TO_LCSS(dev->ssid), dev->devnum);

            obtain_lock(&dev->lock);
            dev->iowaiters++;
            wait_condition(&dev->iocond, &dev->lock);
            dev->iowaiters--;
            release_lock(&dev->lock);
        }

        /* Calculate number of bytes to move and residual byte count */
        len = dev->keybdrem;
        num = (count < len) ? count : len;
        *residual = count - num;
        if (count < len) *more = 1;

        /* Copy data from device buffer to channel buffer */
        memcpy (iobuf, dev->buf, num);

        /* If data chaining is specified, save remaining data */
        if ((flags & CCW_FLAGS_CD) && len > count)
        {
            memmove (dev->buf, dev->buf + count, len - count);
            dev->keybdrem = len - count;
        }
        else
        {
            dev->keybdrem = 0;
        }

        /* Return normal status */
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x0B:
    /*---------------------------------------------------------------*/
    /* AUDIBLE ALARM                                                 */
    /*---------------------------------------------------------------*/
        WRCMSG ("<pnl,color(lightred,black)>", HHC00009, "I");
    /*
        *residual = 0;
    */
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x04:
    /*---------------------------------------------------------------*/
    /* SENSE                                                         */
    /*---------------------------------------------------------------*/
        /* Calculate residual byte count */
        num = (count < dev->numsense) ? count : dev->numsense;
        *residual = count - num;
        if (count < dev->numsense) *more = 1;

        /* Copy device sense bytes to channel I/O buffer */
        memcpy (iobuf, dev->sense, num);

        /* Clear the device sense bytes */
        memset( dev->sense, 0, sizeof(dev->sense) );

        /* Return unit status */
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0xE4:
    /*---------------------------------------------------------------*/
    /* SENSE ID                                                      */
    /*---------------------------------------------------------------*/
        /* Calculate residual byte count */
        num = (count < dev->numdevid) ? count : dev->numdevid;
        *residual = count - num;
        if (count < dev->numdevid) *more = 1;

        /* Copy device identifier bytes to channel I/O buffer */
        memcpy (iobuf, dev->devid, num);

        /* Return unit status */
        *unitstat = CSW_CE | CSW_DE;
        break;

    default:
    /*---------------------------------------------------------------*/
    /* INVALID OPERATION                                             */
    /*---------------------------------------------------------------*/
        /* Set command reject sense byte, and unit check status */
        dev->sense[0] = SENSE_CR;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;

    } /* end switch(code) */

} /* end function con1052_execute_ccw */

#if defined(OPTION_DYNAMIC_LOAD)
static
#endif
DEVHND con1052_device_hndinfo = {
        &con1052_init_handler,         /* Device Initialisation      */
        &con1052_execute_ccw,          /* Device CCW execute         */
        &con1052_close_device,         /* Device Close               */
        &con1052_query_device,         /* Device Query               */
        NULL,                          /* Device Extended Query      */
        NULL,                          /* Device Start channel pgm   */
        NULL,                          /* Device End channel pgm     */
        NULL,                          /* Device Resume channel pgm  */
        NULL,                          /* Device Suspend channel pgm */
        NULL,                          /* Device Halt channel pgm    */
        NULL,                          /* Device Read                */
        NULL,                          /* Device Write               */
        NULL,                          /* Device Query used          */
        NULL,                          /* Device Reserve             */
        NULL,                          /* Device Release             */
        NULL,                          /* Device Attention           */
        con1052_immed,                 /* Immediate CCW Codes        */
        NULL,                          /* Signal Adapter Input       */
        NULL,                          /* Signal Adapter Output      */
        NULL,                          /* Signal Adapter Sync        */
        NULL,                          /* Signal Adapter Output Mult */
        NULL,                          /* QDIO subsys desc           */
        NULL,                          /* QDIO set subchan ind       */
        NULL,                          /* Hercules suspend           */
        NULL                           /* Hercules resume            */
};


#if defined(OPTION_DYNAMIC_LOAD)
static void*
con1052_panel_command (char *cmd)
{
DEVBLK *dev;
char *input;
int  i;

    void* (*next_panel_command_handler)(char *cmd);

    for(dev = sysblk.firstdev; dev; dev = dev->nextdev)
    {
        if(dev->allocated
          && dev->hnd == &con1052_device_hndinfo
          && !strncasecmp(cmd,dev->filename,strlen(dev->filename)) )
        {
            input = cmd + strlen(dev->filename);
            WRCMSG ("<pnl,color(lightyellow,black)>", HHC00008, "I", 
                        dev->filename, cmd+strlen(dev->filename) );
            for(i = 0; i < dev->bufsize && input[i] != '\0'; i++)
                dev->buf[i] = isprint(input[i]) ? host_to_guest(input[i]) : SPACE;
            dev->keybdrem = dev->buflen = i;
            obtain_lock(&dev->lock);
            if(dev->iowaiters)
            {
                signal_condition(&dev->iocond);
                release_lock(&dev->lock);
            }
            else
            {
                release_lock(&dev->lock);
                device_attention (dev, CSW_ATTN);
            }
            return NULL;
        }
    }

    next_panel_command_handler = HDL_FINDNXT(con1052_panel_command);

    if (!next_panel_command_handler)
        return NULL;

    return  next_panel_command_handler(cmd);
}
#endif

/* Libtool static name colision resolution */
/* note : lt_dlopen will look for symbol & modulename_LTX_symbol */
#if !defined(HDL_BUILD_SHARED) && defined(HDL_USE_LIBTOOL)
#define hdl_ddev hdt1052c_LTX_hdl_ddev
#define hdl_depc hdt1052c_LTX_hdl_depc
#define hdl_reso hdt1052c_LTX_hdl_reso
#define hdl_init hdt1052c_LTX_hdl_init
#define hdl_fini hdt1052c_LTX_hdl_fini
#endif


#if defined(OPTION_DYNAMIC_LOAD)
HDL_DEPENDENCY_SECTION;
{
     HDL_DEPENDENCY(HERCULES);
     HDL_DEPENDENCY(DEVBLK);
     HDL_DEPENDENCY(SYSBLK);
}
END_DEPENDENCY_SECTION


#if defined(WIN32) && !defined(HDL_USE_LIBTOOL) && !defined(_MSVC_)
#undef sysblk
HDL_RESOLVER_SECTION;
{
    HDL_RESOLVE_PTRVAR(psysblk, sysblk);
    HDL_RESOLVE(panel_command);
}
END_RESOLVER_SECTION
#endif


HDL_DEVICE_SECTION;
{
    HDL_DEVICE(1052-C, con1052_device_hndinfo);
    HDL_DEVICE(3215-C, con1052_device_hndinfo);
}
END_DEVICE_SECTION


HDL_REGISTER_SECTION;
{
   HDL_REGISTER (panel_command, con1052_panel_command);
}
END_REGISTER_SECTION

#endif

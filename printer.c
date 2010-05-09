/* PRINTER.C    (c) Copyright Roger Bowler, 1999-2010                */
/*              ESA/390 Line Printer Device Handler                  */

// $Id$

/*-------------------------------------------------------------------*/
/* This module contains device handling functions for emulated       */
/* System/370 line printer devices with fcb support and more         */
/*-------------------------------------------------------------------*/

#include "hstdinc.h"
#include "hercules.h"
#include "devtype.h"
#include "opcode.h"

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

/* Printer Specific : 1403 */
/* The following are considered IMMEDIATE commands : */
/* CTL-NOOP, Skip Channel 'n' Immediate, Block Data check , Allow Data Check
 * Space 1,2,3 Lines Immediate, UCS Gate Load, Load UCS Buffer & Fold,
 * Load UCS Buffer (No Fold)
 */

static BYTE printer_immed_commands[256]=
/*
 *0 1 2 3 4 5 6 7 8 9 A B C D E F
*/

{ 0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,
  0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,
  0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,
  0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,
  0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,
  0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,
  0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,
  0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,
  0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,
  0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0};

/*-------------------------------------------------------------------*/
/* Internal macro definitions                                        */
/*-------------------------------------------------------------------*/
//#define LINE_LENGTH     150
#define BUFF_SIZE       1500
#define BUFF_OVFL       150

int line;
int coun;
int chan;
int FCB_MASK[] = {66,1,7,13,19,25,31,37,43,61,49,55,61} ;

#define WRITE_LINE() \
do { \
    /* Start a new record if not data-chained from previous CCW */ \
    if ((chained & CCW_FLAGS_CD) == 0) \
    { \
        dev->bufoff = 0; \
        dev->bufres = BUFF_SIZE; \
    } /* end if(!data-chained) */ \
    /* Calculate number of bytes to write and set residual count */ \
    num = (count < dev->bufres) ? count : dev->bufres; \
    *residual = count - num; \
    /* Copy data from channel buffer to print buffer */ \
    for (i = 0; i < num; i++) \
    { \
        c = guest_to_host(iobuf[i]); \
        if (dev->fold) c = toupper(c); \
        if (c == 0) c = SPACE; \
        dev->buf[dev->bufoff] = c; \
        dev->bufoff++; \
        dev->bufres--; \
    } /* end for(i) */ \
    /* Perform end of record processing if not data-chaining */ \
    if ((flags & CCW_FLAGS_CD) == 0) \
    { \
        /* Truncate trailing blanks from print line */ \
        for (i = dev->bufoff; i > 0; i--) \
            if (dev->buf[i-1] != SPACE) break; \
        /* Write print line */ \
        write_buffer (dev, (char *)dev->buf, i, unitstat); \
        if (*unitstat != 0) return; \
        if ( dev->crlf ) \
        { \
            write_buffer (dev, "\r", 1, unitstat); \
            if (*unitstat != 0) return;	\
        } \
    } /* end if(!data-chaining) */ \
    /* Return normal status */ \
} while(0)

//  printf("At  chan ... MC %02x, Ch %02d, Curr %02d, Dest %02d\n", code,chan,dev->currline,dev->destline);
#define SKIP_TO_CHAN() \
do { \
    chan = ( code - 128 ) / 8 ; \
    /* test for good channel */ \
    if ( dev->fcb[chan] == 0 ) \
    /* channel not found */ \
    { \
        if ( dev->nofcbcheck ) \
        { \
            if ( ( code & 0x02 ) != 0 ) \
            { \
                write_buffer (dev, "\n", 1, unitstat); \
                if (*unitstat != 0) return; \
            } \
        } \
        else \
        { \
            dev->sense[0] = (dev->devtype == 0x1403 ) ? SENSE_EC :SENSE_EC ; \
            *unitstat = CSW_CE | CSW_DE | CSW_UC; \
            return; \
        } \
    } \
    else \
    { \
        dev->destline = dev->fcb[chan]; \
        if ( ( chan == 1 ) || ( dev->destline < dev->currline ) ) \
        { \
            write_buffer (dev, "\f", 1, unitstat); \
            if (*unitstat != 0) return; \
            dev->currline = 1 ; \
        } \
        for ( ; dev->currline < dev->destline ; dev->currline++ ) \
        { \
            write_buffer (dev, "\n", 1, unitstat); \
            if (*unitstat != 0) return; \
        } \
    } \
} while (0)


static void* spthread (DEVBLK* dev);        /*  (forward reference)  */

/*-------------------------------------------------------------------*/
/* Sockdev "OnConnection" callback function                          */
/*-------------------------------------------------------------------*/
static int onconnect_callback (DEVBLK* dev)
{
    TID tid;
    int rc;

    rc = create_thread( &tid, DETACHED, spthread, dev, NULL );
    if(rc)
    {
        WRMSG( HHC00102, "E", strerror( rc ) );
        return 0;
    }
    return 1;
}

/*-------------------------------------------------------------------*/
/* Thread to monitor the sockdev remote print spooler connection     */
/*-------------------------------------------------------------------*/
static void* spthread (DEVBLK* dev)
{
    BYTE byte;
    fd_set readset, errorset;
    struct timeval tv;
    int rc, fd = dev->fd;           // (save original fd)

    /* Fix thread name */
    {
        char thread_name[32];
        thread_name[sizeof(thread_name)-1] = 0;
        snprintf( thread_name, sizeof(thread_name)-1,
            "spthread %1d:%04X", SSID_TO_LCSS(dev->ssid), dev->devnum );
        SET_THREAD_NAME( thread_name );
    }

    // Looooop...  until shutdown or disconnect...

    // PROGRAMMING NOTE: we do our select specifying an immediate
    // timeout to prevent our select from holding up (slowing down)
    // the device thread (which does the actual writing of data to
    // the client). The only purpose for our thread even existing
    // is to detect a severed connection (i.e. to detect when the
    // client disconnects)...

    while ( !sysblk.shutdown && dev->fd == fd )
    {
        if (dev->busy)
        {
            SLEEP(3);
            continue;
        }

        FD_ZERO( &readset );
        FD_ZERO( &errorset );

        FD_SET( fd, &readset );
        FD_SET( fd, &errorset );

        tv.tv_sec = 0;
        tv.tv_usec = 0;

        rc = select( fd+1, &readset, NULL, &errorset, &tv );

        if (rc < 0)
            break;

        if (rc == 0)
        {
            SLEEP(3);
            continue;
        }

        if (FD_ISSET( fd, &errorset ))
            break;

        // Read and ignore any data they send us...
        // Note: recv should complete immediately
        // as we know data is waiting to be read.

        ASSERT( FD_ISSET( fd, &readset ) );

        rc = recv( fd, &byte, sizeof(byte), 0 );

        if (rc <= 0)
            break;
    }

    obtain_lock( &dev->lock );

    // PROGRAMMING NOTE: the following tells us whether we detected
    // the error or if the device thread already did. If the device
    // thread detected it while we were sleeping (and subsequently
    // closed the connection) then we don't need to do anything at
    // all; just exit. If we were the ones that detected the error
    // however, then we need to close the connection so the device
    // thread can learn of it...

    if (dev->fd == fd)
    {
        dev->fd = -1;
        close_socket( fd );
        WRMSG (HHC01100, "I", SSID_TO_LCSS(dev->ssid), dev->devnum, 
               dev->bs->clientname, dev->bs->clientip, dev->bs->spec);
    }

    release_lock( &dev->lock );

    return NULL;

} /* end function spthread */

/*-------------------------------------------------------------------*/
/* Initialize the device handler                                     */
/*-------------------------------------------------------------------*/
static int printer_init_handler (DEVBLK *dev, int argc, char *argv[])
{
int     i,j;                            /* Array subscript           */
int     chan;
char    wrk[3];                         /* for atoi conversion       */
int     sockdev = 0;                    /* 1 == is socket device     */
    /* Forcibly disconnect anyone already currently connected */
    if (dev->bs && !unbind_device_ex(dev,1))
        return -1; // (error msg already issued)

    /* The first argument is the file name */
    if (argc == 0 || strlen(argv[0]) >= sizeof(dev->filename))
    {
        WRMSG (HHC01101, "E", SSID_TO_LCSS(dev->ssid), dev->devnum);
        return -1;
    }

    /* Save the file name in the device block */
    hostpath(dev->filename, argv[0], sizeof(dev->filename));

    if(!sscanf(dev->typname,"%hx",&(dev->devtype)))
        dev->devtype = 0x3211;

    /* Initialize device dependent fields */
    dev->fd = -1;
//  dev->printpos = 0;
//  dev->printrem = LINE_LENGTH;
    dev->diaggate = 0;
    dev->fold = 0;
    dev->crlf = 0;
    dev->stopprt = 0;
    dev->notrunc = 0;

    /* initialize the new fields for FCB+ support */
    dev->fcbsupp = 1;
    dev->rawcc = 0;
    dev->nofcbcheck = 0;
//  dev->usenoopcc  = 0;
    dev->ccpend = 0;
    dev->prevline = 1;
    dev->currline = 1;
    dev->destline = 1;

    for (i = 0; i < 13; i++) dev->fcb[i] = FCB_MASK[i];

    dev->ispiped = (dev->filename[0] == '|');

    /* Process the driver arguments */
    for (i = 1; i < argc; i++)
    {
        if (strcasecmp(argv[i], "crlf") == 0)
        {
            dev->crlf = 1;
            continue;
        }

        /* sockdev means the device file is actually
           a connected socket instead of a disk file.
           The file name is the socket_spec (host:port)
           to listen for connections on.
        */
        if (!dev->ispiped && strcasecmp(argv[i], "sockdev") == 0)
        {
            sockdev = 1;
            continue;
        }

        if (strcasecmp(argv[i], "noclear") == 0)
        {
            dev->notrunc = 1;
            continue;
        }

        if (strcasecmp(argv[i], "rawcc") == 0)
        {
            dev->rawcc = 1;
            continue;
        }

        if (strcasecmp(argv[i], "nofcbcheck") == 0)
        {
            dev->nofcbcheck = 1;
            continue;
        }

        if (strcasecmp(argv[i], "fcbcheck") == 0)
        {
            dev->nofcbcheck = 0;
            continue;
        }

        if (strncasecmp("fcb=", argv[i], 4) == 0)
        {
            if (strlen (argv[i]) != 30)
            {
                WRMSG (HHC01102, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, argv[i], i + 1);
                return -1;
            }
            for (j = 4 ; j < 30 ; j++)
            {
                if ((argv[i][j] < '0') || (argv[i][j] > '9'))
                {
                    WRMSG (HHC01103, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, argv[i], i + 1, j);
                    return -1;
                }
            }

            /* build the fcb image */
            chan = 0;
            for (j = 4; j < 30; j += 2)
            {
                wrk[0] = argv[i][j];
                wrk[1] = argv[i][j+1];
                wrk[2] = '\0';
                dev->fcb[chan] = atoi(wrk);
                chan++;
            }
//          printf("FCB ");
//          for (chan = 0; chan < 13; printf("/%02d",dev->FCB[chan++]));
//          printf("/\n");

            continue;
        }

        WRMSG (HHC01102, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, argv[i], i + 1);
        return -1;
    }

    /* Check for incompatible options */
    if (sockdev && dev->crlf)
    {
        WRMSG (HHC01104, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, "crlf");
        return -1;
    }

    if (sockdev && dev->notrunc)
    {
        WRMSG (HHC01104, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, "noclear");
        return -1;
    }

    /* If socket device, create a listening socket
       to accept connections on.
    */
    if (sockdev && !bind_device_ex( dev,
        dev->filename, onconnect_callback, dev ))
    {
        return -1;  // (error msg already issued)
    }

    /* Set length of print buffer */
//  dev->bufsize = LINE_LENGTH + 8;
    dev->bufsize = BUFF_SIZE + BUFF_OVFL;
    dev->bufres = BUFF_SIZE;
    dev->bufoff = 0;

    /* Set number of sense bytes */
    dev->numsense = 1;

    /* Initialize the device identifier bytes */
    dev->devid[0] = 0xFF;
    dev->devid[1] = 0x28; /* Control unit type is 2821-1 */
    dev->devid[2] = 0x21;
    dev->devid[3] = 0x01;
    dev->devid[4] = dev->devtype >> 8;
    dev->devid[5] = dev->devtype & 0xFF;
    dev->devid[6] = 0x01;
    dev->numdevid = 7;

    /* Activate I/O tracing */
//  dev->ccwtrace = 1;

    return 0;
} /* end function printer_init_handler */

/*-------------------------------------------------------------------*/
/* Query the device definition                                       */
/*-------------------------------------------------------------------*/
static void printer_query_device (DEVBLK *dev, char **class,
                int buflen, char *buffer)
{
    BEGIN_DEVICE_CLASS_QUERY( "PRT", dev, class, buflen, buffer );

    snprintf (buffer, buflen, "%s%s%s%s%s%s%s",
                 dev->filename,
                (dev->bs         ? " sockdev"      : ""),
                (dev->crlf       ? " crlf"         : ""),
                (dev->notrunc    ? " noclear"      : ""),
                (dev->rawcc      ? " rawcc"        : ""),
                (dev->nofcbcheck ? " nofcbcheck"   : " fcbcheck"),
                (dev->stopprt    ? " (stopped)"    : ""));

} /* end function printer_query_device */

/*-------------------------------------------------------------------*/
/* Subroutine to open the printer file or pipe                       */
/*-------------------------------------------------------------------*/
static int
open_printer (DEVBLK *dev)
{
pid_t           pid;                    /* Child process identifier  */
int             open_flags;             /* File open flags           */
#if !defined( _MSVC_ )
int             pipefd[2];              /* Pipe descriptors          */
int             rc;                     /* Return code               */
#endif

    /* Regular open if 1st char of filename is not vertical bar */
    if (!dev->ispiped)
    {
        int fd;

        /* Socket printer? */
        if (dev->bs)
            return (dev->fd < 0 ? -1 : 0);

        /* Normal printer */
        open_flags = O_BINARY | O_WRONLY | O_CREAT /* | O_SYNC */;
        if (dev->notrunc != 1)
        {
            open_flags |= O_TRUNC;
        }
        fd = open (dev->filename, open_flags,
                    S_IRUSR | S_IWUSR | S_IRGRP);
        if (fd < 0)
        {            
            WRMSG (HHC01105, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, "open()", strerror(errno));
            return -1;
        }

        /* Save file descriptor in device block and return */
        dev->fd = fd;
        return 0;
    }

    /* Filename is in format |xxx, set up pipe to program xxx */

#if defined( _MSVC_ )

    /* "Poor man's" fork... */
    pid = w32_poor_mans_fork ( dev->filename+1, &dev->fd );
    if (pid < 0)
    {
        WRMSG (HHC01105, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, "fork()", strerror(errno));
        return -1;
    }

    /* Log start of child process */
    WRMSG (HHC01106, "I", SSID_TO_LCSS(dev->ssid), dev->devnum, pid);
    dev->ptpcpid = pid;

#else /* !defined( _MSVC_ ) */

    /* Create a pipe */
    rc = create_pipe (pipefd);
    if (rc < 0)
    {
        WRMSG (HHC01105, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, "create_pipe()", strerror(errno));
        return -1;
    }

    /* Fork a child process to receive the pipe data */
    pid = fork();
    if (pid < 0)
    {
        WRMSG (HHC01005, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, "fork()", strerror(errno));
        close_pipe ( pipefd[0] );
        close_pipe ( pipefd[1] );
        return -1;
    }

    /* The child process executes the pipe receiver program... */
    if (pid == 0)
    {
        /* Log start of child process */
        WRMSG (HHC01106, "I", SSID_TO_LCSS(dev->ssid), dev->devnum, getpid(), getpid());

        /* Close the write end of the pipe */
        close_pipe ( pipefd[1] );

        /* Duplicate the read end of the pipe onto STDIN */
        if (pipefd[0] != STDIN_FILENO)
        {
            rc = dup2 (pipefd[0], STDIN_FILENO);
            if (rc != STDIN_FILENO)
            {
                WRMSG (HHC01105, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, "dup2()", strerror(errno));
                close_pipe ( pipefd[0] );
                _exit(127);
            }
        } /* end if(pipefd[0] != STDIN_FILENO) */

        /* Close the original descriptor now duplicated to STDIN */
        close_pipe ( pipefd[0] );

        /* Redirect stderr (screen) to hercules log task */
        dup2(STDOUT_FILENO, STDERR_FILENO);

        /* Relinquish any ROOT authority before calling shell */
        SETMODE(TERM);

        /* Execute the specified pipe receiver program */
        rc = system (dev->filename+1);

        if (rc == 0)
        {
            /* Log end of child process */
            WRMSG (HHC01107, "I", SSID_TO_LCSS(dev->ssid), dev->devnum, getpid());
        }
        else
        {
            /* Log error */
            WRMSG (HHC01108, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename+1, strerror(errno));
        }

        /* The child process terminates using _exit instead of exit
           to avoid invoking the panel atexit cleanup routine */
        _exit(rc);

    } /* end if(pid==0) */

    /* The parent process continues as the pipe sender */

    /* Close the read end of the pipe */
    close_pipe ( pipefd[0] );

    /* Save pipe write descriptor in the device block */
    dev->fd = pipefd[1];
    dev->ptpcpid = pid;

#endif /* defined( _MSVC_ ) */

    return 0;

} /* end function open_printer */

/*-------------------------------------------------------------------*/
/* Subroutine to write data to the printer                           */
/*-------------------------------------------------------------------*/
static void
write_buffer (DEVBLK *dev, char *buf, int len, BYTE *unitstat)
{
int             rc;                     /* Return code               */

    /* Write data to the printer file */
    if (dev->bs)
    {
        /* (socket printer) */
        rc = write_socket (dev->fd, buf, len);

        /* Check for socket error */
        if (rc < len)
        {
            /* Close the connection */
            if (dev->fd != -1)
            {
                int fd = dev->fd;
                dev->fd = -1;
                close_socket( fd );
                WRMSG (HHC01100, "I", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->bs->clientname, dev->bs->clientip, dev->bs->spec);
            }

            /* Set unit check with intervention required */
            dev->sense[0] = SENSE_IR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
        }
    }
    else
    {
        /* Write data to the printer file */
        rc = write (dev->fd, buf, len);

        /* Equipment check if error writing to printer file */
        if (rc < len)
        {
            WRMSG (HHC01105, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, "write()", 
                   rc < 0 ? strerror(errno) : "incomplete record written");
            dev->sense[0] = SENSE_EC;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
        }
    }

} /* end function write_buffer */

/*-------------------------------------------------------------------*/
/* Close the device                                                  */
/*-------------------------------------------------------------------*/
static int printer_close_device ( DEVBLK *dev )
{
int fd = dev->fd;

    if (fd == -1)
        return 0;

    dev->fd      = -1;
    dev->stopprt =  0;

    /* Close the device file */
    if ( dev->ispiped )
    {
#if !defined( _MSVC_ )
        close_pipe (fd);
#else /* defined( _MSVC_ ) */
        close (fd);
        /* Log end of child process */
        WRMSG (HHC01107, "I", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->ptpcpid);
#endif /* defined( _MSVC_ ) */
        dev->ptpcpid = 0;
    }
    else
    {
        if (dev->bs)
        {
            /* Socket printer */
            close_socket (fd);
            WRMSG (HHC01100, "I", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->bs->clientname, dev->bs->clientip, dev->bs->spec);
        }
        else
        {
            /* Regular printer */
            close (fd);
        }
    }

    return 0;
} /* end function printer_close_device */

/*-------------------------------------------------------------------*/
/* Execute a Channel Command Word                                    */
/*-------------------------------------------------------------------*/
static void printer_execute_ccw (DEVBLK *dev, BYTE code, BYTE flags,
        BYTE chained, U16 count, BYTE prevcode, int ccwseq,
        BYTE *iobuf, BYTE *more, BYTE *unitstat, U16 *residual)
{
int             rc = 0;                 /* Return code               */
int             i;                      /* Loop counter              */
int             num;                    /* Number of bytes to move   */
char           *eor;                    /* -> end of record string   */
char           *nls = "\n\n\n";         /* -> new lines              */
BYTE            c;                      /* Print character           */
char            hex[3];                 /* for hex conversion        */

    /* Reset flags at start of CCW chain */
    if (chained == 0)
    {
        dev->diaggate = 0;
    }

    /* Open the device file if necessary */
    if (dev->fd < 0 && !IS_CCW_SENSE(code))
        rc = open_printer (dev);
    else
    {
        /* If printer stopped, return intervention required */
        if (dev->stopprt && !IS_CCW_SENSE(code))
            rc = -1;
        else
            rc = 0;
    }

    if (rc < 0)
    {
        /* Set unit check with intervention required */
        dev->sense[0] = SENSE_IR;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return;
    }

    /* Process depending on CCW opcode */
//  printf("ctlchar(%02x) count(%d)\n",code,count);
    switch (code) {
    case 0x03: /* No Operation                   */
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x0B: /* Space 1 Line  Immediate        */
    case 0x13: /* Space 2 Lines Immediate        */
    case 0x1B: /* Space 3 Lines Immediate        */
        if (dev->rawcc)
        {
            sprintf(hex,"%02x",code);
            write_buffer(dev, hex, 2, unitstat);
            if (*unitstat != 0) return;
            eor = (dev->crlf) ? "\r\n" : "\n";
            write_buffer(dev, eor, strlen(eor), unitstat);
            if (*unitstat != 0) return;
            *unitstat = CSW_CE | CSW_DE;
            return;
        }
        dev->ccpend = 0;
        coun = code / 8;
        dev->currline += coun;
        write_buffer(dev, nls, coun, unitstat);
        if (*unitstat != 0) return;
        *unitstat = CSW_CE | CSW_DE;
        return;

    case 0x8B: /* Skip to Channel 1 Immediate    */
    case 0x93: /* Skip to Channel 2 Immediate    */
    case 0x9B: /* Skip to Channel 3 Immediate    */
    case 0xA3: /* Skip to Channel 4 Immediate    */
    case 0xAB: /* Skip to Channel 5 Immediate    */
    case 0xB3: /* Skip to Channel 6 Immediate    */
    case 0xBB: /* Skip to Channel 7 Immediate    */
    case 0xC3: /* Skip to Channel 8 Immediate    */
    case 0xCB: /* Skip to Channel 9 Immediate    */
    case 0xD3: /* Skip to Channel 10 Immediate   */
    case 0xDB: /* Skip to Channel 11 Immediate   */
    case 0xE3: /* Skip to Channel 12 Immediate   */
        if (dev->rawcc)
        {
            sprintf(hex,"%02x",code);
            write_buffer (dev, hex, 2, unitstat);
            if (*unitstat != 0) return;
            eor = (dev->crlf) ? "\r\n" : "\n";
            write_buffer (dev, eor, strlen(eor), unitstat);
            if (*unitstat != 0) return;
            *unitstat = CSW_CE | CSW_DE;
            return;
        }
        if (dev->ccpend)
        {
            write_buffer(dev, "\n", 1, unitstat);
            if (*unitstat != 0) return;
            dev->ccpend = 0;
            dev->currline++;
        }
        SKIP_TO_CHAN();
        *unitstat = CSW_CE | CSW_DE;
        return;

    case 0x01: /* Write Without Spacing           */
    case 0x09: /* Write and Space 1 Line          */
    case 0x11: /* Write and Space 2 Lines         */
    case 0x19: /* Write and Space 3 Lines         */
        if (dev->rawcc)
        {
            sprintf(hex,"%02x",code);
            write_buffer(dev, hex, 2, unitstat);
            if (*unitstat != 0) return;
            WRITE_LINE();
            write_buffer(dev, "\n", 1, unitstat);
            if (*unitstat != 0) return;
            *unitstat = CSW_CE | CSW_DE;
            return;
        }

        if (((chained & CCW_FLAGS_CD) == 0) && dev->ccpend)
        {
            write_buffer(dev, "\n", 1, unitstat);
            if (*unitstat != 0) return;
            dev->ccpend = 0;
            dev->currline++;
        }
        WRITE_LINE();
        if (code == 0x01)
        {
            /* for a write no space set the cc pending indicator */
            /* and return */
            dev->ccpend = 1;
            *unitstat = CSW_CE | CSW_DE;
            return;
        }
        if ((flags & CCW_FLAGS_CD) == 0)
        {
            coun = code / 8;
            dev->currline += coun;
            write_buffer(dev, nls, coun, unitstat);
            if (*unitstat != 0) return;
        }
        *unitstat = CSW_CE | CSW_DE;
        return;

    case 0x89: /* Write and Skip to Channel 1    */
    case 0x91: /* Write and Skip to Channel 2    */
    case 0x99: /* Write and Skip to Channel 3    */
    case 0xA1: /* Write and Skip to Channel 4    */
    case 0xA9: /* Write and Skip to Channel 5    */
    case 0xB1: /* Write and Skip to Channel 6    */
    case 0xB9: /* Write and Skip to Channel 7    */
    case 0xC1: /* Write and Skip to Channel 8    */
    case 0xC9: /* Write and Skip to Channel 9    */
    case 0xD1: /* Write and Skip to Channel 10   */
    case 0xD9: /* Write and Skip to Channel 11   */
    case 0xE1: /* Write and Skip to Channel 12   */
        if (dev->rawcc)
        {
            sprintf(hex,"%02x",code);
            write_buffer(dev, hex, 2, unitstat);
            if (*unitstat != 0) return;
            WRITE_LINE();
            eor = (dev->crlf) ? "\r\n" : "\n";
            write_buffer(dev, eor, strlen(eor), unitstat);
            if (*unitstat != 0) return;
            *unitstat = CSW_CE | CSW_DE;
            return;
        }
        if (((chained & CCW_FLAGS_CD) == 0) && dev->ccpend)
        {
            write_buffer(dev, "\n", 1, unitstat);
            if (*unitstat != 0) return;
            dev->currline++;
            dev->ccpend = 0;
        }
        WRITE_LINE();
        if ((flags & CCW_FLAGS_CD) == 0)
        {
            write_buffer(dev, "\n", 1, unitstat);
            if (*unitstat != 0) return;
            dev->currline++;
            SKIP_TO_CHAN();
        }
        *unitstat = CSW_CE | CSW_DE;
        return;

    case 0x63:
    /*---------------------------------------------------------------*/
    /* LOAD FORMS CONTROL BUFFER                                     */
    /*---------------------------------------------------------------*/
        if (dev->rawcc)
        {
            sprintf(hex,"%02x",code);
            write_buffer(dev, hex, 2, unitstat);
            if (*unitstat != 0) return;
            for (i = 0; i < count; i++)
            {
                sprintf(hex,"%02x",iobuf[i]);
                dev->buf[i*2] = hex[0];
                dev->buf[i*2+1] = hex[1];
            } /* end for(i) */
            write_buffer(dev, (char *)dev->buf, i*2, unitstat);
            if (*unitstat != 0) return;
            eor = (dev->crlf) ? "\r\n" : "\n";
            write_buffer(dev, eor, strlen(eor), unitstat);
            if (*unitstat != 0) return;
        }
        else
        {
            /* i index to the DATA */
            for (i = 0; i < 13; dev->fcb[i++] = 0);
            for (i=1, line=1; i < count && !(iobuf[i] & 0x10); i++, line++)
            {
                if (!(chan = iobuf[i])) continue;
                if (chan == 1) line = 1;
                if (chan > 12) chan = 12;
                dev->fcb[chan] = line;
            }
        }
        /* Return normal status */
        *residual = 0;
        *unitstat = CSW_CE | CSW_DE;
        break;


    case 0x06:
    /*---------------------------------------------------------------*/
    /* DIAGNOSTIC CHECK READ                                         */
    /*---------------------------------------------------------------*/
        /* If not 1403, reject if not preceded by DIAGNOSTIC GATE */
        if (dev->devtype != 0x1403 && dev->diaggate == 0)
        {
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Return normal status */
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x07:
    /*---------------------------------------------------------------*/
    /* DIAGNOSTIC GATE                                               */
    /*---------------------------------------------------------------*/
        /* Command reject if 1403, or if chained to another CCW
           except a no-operation at the start of the CCW chain */
        if (dev->devtype == 0x1403 || ccwseq > 1
            || (chained && prevcode != 0x03))
        {
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Set diagnostic gate flag */
        dev->diaggate = 1;

        /* Return normal status */
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x0A:
    /*---------------------------------------------------------------*/
    /* DIAGNOSTIC READ UCS BUFFER                                    */
    /*---------------------------------------------------------------*/
        /* Reject if 1403 or not preceded by DIAGNOSTIC GATE */
        if (dev->devtype == 0x1403 || dev->diaggate == 0)
        {
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Return normal status */
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x12:
    /*---------------------------------------------------------------*/
    /* DIAGNOSTIC READ fcb                                           */
    /*---------------------------------------------------------------*/
        /* Reject if 1403 or not preceded by DIAGNOSTIC GATE */
        if (dev->devtype == 0x1403 || dev->diaggate == 0)
        {
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Return normal status */
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x23:
    /*---------------------------------------------------------------*/
    /* UNFOLD                                                        */
    /*---------------------------------------------------------------*/
        dev->fold = 0;
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x43:
    /*---------------------------------------------------------------*/
    /* FOLD                                                          */
    /*---------------------------------------------------------------*/
        dev->fold = 1;
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x73:
    /*---------------------------------------------------------------*/
    /* BLOCK DATA CHECK                                              */
    /*---------------------------------------------------------------*/
    /*
        *residual = 0;
    */
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x7B:
    /*---------------------------------------------------------------*/
    /* ALLOW DATA CHECK                                              */
    /*---------------------------------------------------------------*/
    /*
        *residual = 0;
    */
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0xEB:
    /*---------------------------------------------------------------*/
    /* UCS GATE LOAD                                                 */
    /*---------------------------------------------------------------*/
        /* Command reject if not first command in chain */
        if (chained != 0)
        {
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Return normal status */
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0xF3:
    /*---------------------------------------------------------------*/
    /* LOAD UCS BUFFER AND FOLD                                      */
    /*---------------------------------------------------------------*/
        /* For 1403, command reject if not chained to UCS GATE */
        /* Also allow ALLOW DATA CHECK to get TSS/370 working  */
        /* -- JRM 11/28/2007 */
        if (dev->devtype == 0x1403 &&
            ((prevcode != 0xEB) && (prevcode != 0x7B)))
        {
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Set fold indicator and return normal status */
        dev->fold = 1;
    /*
        *residual = 0;
    */
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0xFB:
    /*---------------------------------------------------------------*/
    /* LOAD UCS BUFFER (NO FOLD)                                     */
    /*---------------------------------------------------------------*/
        /* For 1403, command reject if not chained to UCS GATE */
        if (dev->devtype == 0x1403 && prevcode != 0xEB)
        {
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Reset fold indicator and return normal status */
        dev->fold = 0;
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
        memset (dev->sense, 0, sizeof(dev->sense));

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

} /* end function printer_execute_ccw */


#if defined(OPTION_DYNAMIC_LOAD)
static
#endif
DEVHND printer_device_hndinfo = {
        &printer_init_handler,         /* Device Initialisation      */
        &printer_execute_ccw,          /* Device CCW execute         */
        &printer_close_device,         /* Device Close               */
        &printer_query_device,         /* Device Query               */
        NULL,                          /* Device Start channel pgm   */
        NULL,                          /* Device End channel pgm     */
        NULL,                          /* Device Resume channel pgm  */
        NULL,                          /* Device Suspend channel pgm */
        NULL,                          /* Device Read                */
        NULL,                          /* Device Write               */
        NULL,                          /* Device Query used          */
        NULL,                          /* Device Reserve             */
        NULL,                          /* Device Release             */
        NULL,                          /* Device Attention           */
        printer_immed_commands,        /* Immediate CCW Codes        */
        NULL,                          /* Signal Adapter Input       */
        NULL,                          /* Signal Adapter Output      */
        NULL,                          /* Hercules suspend           */
        NULL                           /* Hercules resume            */
};

/* Libtool static name colision resolution */
/* note : lt_dlopen will look for symbol & modulename_LTX_symbol */
#if !defined(HDL_BUILD_SHARED) && defined(HDL_USE_LIBTOOL)
#define hdl_ddev hdt1403_LTX_hdl_ddev
#define hdl_depc hdt1403_LTX_hdl_depc
#define hdl_reso hdt1403_LTX_hdl_reso
#define hdl_init hdt1403_LTX_hdl_init
#define hdl_fini hdt1403_LTX_hdl_fini
#endif

#if defined(OPTION_DYNAMIC_LOAD)
HDL_DEPENDENCY_SECTION;
{
     HDL_DEPENDENCY(HERCULES);
     HDL_DEPENDENCY(DEVBLK);
}
END_DEPENDENCY_SECTION


HDL_DEVICE_SECTION;
{
    HDL_DEVICE(1403, printer_device_hndinfo );
    HDL_DEVICE(3211, printer_device_hndinfo );
}
END_DEVICE_SECTION
#endif

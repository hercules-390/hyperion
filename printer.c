/* PRINTER.C    (c) Copyright Roger Bowler, 1999-2006                */
/*              ESA/390 Line Printer Device Handler                  */

// $Id$

/*-------------------------------------------------------------------*/
/* This module contains device handling functions for emulated       */
/* System/370 line printer devices.                                  */
/*-------------------------------------------------------------------*/

// $Log$
// Revision 1.38  2007/02/26 13:38:51  rbowler
// Messages HHCPR001E,HHCPR002E not logged to control panel
//
// Revision 1.37  2006/12/08 09:43:29  jj
// Add CVS message log
//

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
  0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,
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
#define LINE_LENGTH     150

/*-------------------------------------------------------------------*/
/* Subroutine to open the printer file or pipe                       */
/*-------------------------------------------------------------------*/
static int
open_printer (DEVBLK *dev)
{
pid_t           pid;                    /* Child process identifier  */
char            pathname[MAX_PATH];     /* file path in host format  */
char*           pszCmdLine;
size_t          nCmdLineBuffSize;
int             save_errno;
#if !defined( _MSVC_ )
int             pipefd[2];              /* Pipe descriptors          */
int             rc;                     /* Return code               */
#endif

    if (dev->ispiped)
        hostpath(pathname, dev->filename+1, sizeof(pathname));
    else
        hostpath(pathname, dev->filename,   sizeof(pathname));

    /* Regular open if 1st char of filename is not vertical bar */
    if (!dev->ispiped)
    {
        int fd;
        fd = open (pathname, O_BINARY |
                    O_WRONLY | O_CREAT | O_TRUNC /* | O_SYNC */,
                    S_IRUSR | S_IWUSR | S_IRGRP);
        if (fd < 0)
        {
            logmsg (_("HHCPR004E Error opening file %s: %s\n"),
                    dev->filename, strerror(errno));
            return -1;
        }

        /* Save file descriptor in device block and return */
        dev->fd = fd;
        return 0;
    }

    /* Filename is in format |xxx, set up pipe to program xxx */

    // (allocate room for command-line)

    nCmdLineBuffSize = ( 1 + strlen(pathname) + 1 + dev->siz_ptpargs );
    if (!(pszCmdLine = malloc( nCmdLineBuffSize )))
    {
        logmsg (_("HHCPR013E Out of memory\n"));
        return -1;
    }
    *pszCmdLine = 0;

#if defined( _MSVC_ )

    // (quotes might not be needed but it doesn't hurt to have them anyway)
    // (if dev->ptpargs is non-empty, it should already start with a blank)

    strlcat( pszCmdLine, "\"",            nCmdLineBuffSize );
    strlcat( pszCmdLine, pathname,        nCmdLineBuffSize ); // (skip leading '|')
    strlcat( pszCmdLine, "\"",            nCmdLineBuffSize );
    strlcat( pszCmdLine, dev->ptpargs,    nCmdLineBuffSize );

#else /* !defined( _MSVC_ ) */

    // (if dev->ptpargs is non-empty, it should already start with a blank)

    strlcat( pszCmdLine, pathname,        nCmdLineBuffSize ); // (skip leading '|')
    strlcat( pszCmdLine, dev->ptpargs,    nCmdLineBuffSize );

#endif /* defined( _MSVC_ ) */

#if defined( _MSVC_ )

    /* "Poor man's" fork... */
    pid = w32_poor_mans_fork ( pszCmdLine, &dev->fd );
    save_errno = errno;
    free( pszCmdLine );
    errno = save_errno;

    if (pid < 0)
    {
        logmsg (_("HHCPR006E %4.4X device initialization error: fork: %s\n"),
                dev->devnum, strerror(errno));
        return -1;
    }

    /* Log start of child process */
    logmsg (_("HHCPR007I pipe receiver (pid=%d) starting for %4.4X\n"),
            pid, dev->devnum);
    dev->ptpcpid = pid;

#else /* !defined( _MSVC_ ) */

    /* Create a pipe */
    rc = create_pipe (pipefd);
    if (rc < 0)
    {
        logmsg (_("HHCPR005E %4.4X device initialization error: pipe: %s\n"),
                dev->devnum, strerror(errno));
        return -1;
    }

    /* Fork a child process to receive the pipe data */
    pid = fork();
    if (pid < 0)
    {
        logmsg (_("HHCPR006E %4.4X device initialization error: fork: %s\n"),
                dev->devnum, strerror(errno));
        close_pipe ( pipefd[0] );
        close_pipe ( pipefd[1] );
        return -1;
    }

    /* The child process executes the pipe receiver program... */
    if (pid == 0)
    {
        /* Log start of child process */
        logmsg (_("HHCPR007I pipe receiver (pid=%d) starting for %4.4X\n"),
                getpid(), dev->devnum);

        /* Close the write end of the pipe */
        close_pipe ( pipefd[1] );

        /* Duplicate the read end of the pipe onto STDIN */
        if (pipefd[0] != STDIN_FILENO)
        {
            rc = dup2 (pipefd[0], STDIN_FILENO);
            if (rc != STDIN_FILENO)
            {
                logmsg (_("HHCPR008E %4.4X dup2 error: %s\n"),
                        dev->devnum, strerror(errno));
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

        rc = system (pszCmdLine);
        save_errno = errno;
        free( pszCmdLine );
        errno = save_errno;

        if (rc == 0)
        {
            /* Log end of child process */
            logmsg (_("HHCPR011I pipe receiver (pid=%d) terminating for %4.4X\n"),
                    getpid(), dev->devnum);
        }
        else
        {
            /* Log error */
            logmsg (_("HHCPR012E %4.4X Unable to execute %s: %s\n"),
                    dev->devnum, pathname, strerror(errno));
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
    rc = write (dev->fd, buf, len);

    /* Equipment check if error writing to printer file */
    if (rc < len)
    {
        logmsg (_("HHCPR003E %4.4X Error writing to %s: %s\n"),
                dev->devnum, dev->filename,
                (errno == 0 ? _("incomplete"): strerror(errno)));
        dev->sense[0] = SENSE_EC;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return;
    }

} /* end function write_buffer */

/*-------------------------------------------------------------------*/
/* Initialize the device handler                                     */
/*-------------------------------------------------------------------*/
static int printer_init_handler (DEVBLK *dev, int argc, char *argv[])
{
int     i;                              /* Array subscript           */

    /* The first argument is the file name */
    if (argc == 0 || strlen(argv[0]) > sizeof(dev->filename)-1)
    {
        logmsg (_("HHCPR001E File name missing or invalid for printer %4.4X\n"),
                 dev->devnum);
        return -1;
    }

    /* Save the file name in the device block */
    strncpy (dev->filename, argv[0], sizeof(dev->filename));

    if(!sscanf(dev->typname,"%hx",&(dev->devtype)))
        dev->devtype = 0x1403;

    /* Initialize device dependent fields */
    dev->fd = -1;
    dev->printpos = 0;
    dev->printrem = LINE_LENGTH;
    dev->diaggate = 0;
    dev->fold = 0;
    dev->crlf = 0;
    dev->stopprt = 0;
    dev->ptpcpid = 0;
    dev->ispiped = 0;
    dev->ptpargs = NULL;
    dev->siz_ptpargs = 0;

    /* Process the driver arguments */
    for (i = 1; i < argc; i++)
    {
        if (dev->ispiped)
        {
            size_t siz_ptpargs = dev->siz_ptpargs + 1 + strlen(argv[i]);
            if (!realloc( dev->ptpargs, siz_ptpargs ))
            {
                logmsg (_("HHCPR013E Out of memory\n"));
                free( dev->ptpargs );
                dev->ptpargs = NULL;
                dev->siz_ptpargs = 0;
                dev->ispiped = 0;
                return -1;
            }
            dev->siz_ptpargs = siz_ptpargs;
            strlcat( dev->ptpargs, " ",     dev->siz_ptpargs );
            strlcat( dev->ptpargs, argv[i], dev->siz_ptpargs );
            continue;
        }

        if (strcmp(argv[i], "--") == 0)
        {
            if (dev->filename[0] == '|')
            {
                dev->ispiped = 1;
                if (!(dev->ptpargs = malloc( dev->siz_ptpargs = 1 )))
                {
                    logmsg (_("HHCPR013E Out of memory\n"));
                    return -1;
                }
                *dev->ptpargs = 0;
                continue;
            }
        }

        if (strcasecmp(argv[i], "crlf") == 0)
        {
            dev->crlf = 1;
            continue;
        }

        logmsg (_("HHCPR002E Invalid argument for printer %4.4X: %s\n"),
                dev->devnum, argv[i]);
        return -1;
    }

    /* Set length of print buffer */
    dev->bufsize = LINE_LENGTH + 8;

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
    *class = "PRT";
    snprintf (buffer, buflen, "%s%s%s",
                dev->filename,
                (dev->crlf ? " crlf" : ""),
                (dev->stopprt ? " (stopped)" : ""));

} /* end function printer_query_device */

/*-------------------------------------------------------------------*/
/* Close the device                                                  */
/*-------------------------------------------------------------------*/
static int printer_close_device ( DEVBLK *dev )
{
    if (dev->fd < 0) return 0;  /* (nothing to close!) */

    /* Close the device file */
    if ( dev->ispiped )
    {
#if !defined( _MSVC_ )
        close_pipe ( dev->fd );
#else /* defined( _MSVC_ ) */
        close (dev->fd);
        /* Log end of child process */
        logmsg (_("HHCPR011I pipe receiver (pid=%d) terminating for %4.4X\n"),
                dev->ptpcpid, dev->devnum);
#endif /* defined( _MSVC_ ) */
        dev->ptpcpid = 0;
        free( dev->ptpargs );
        dev->ptpargs = NULL;
        dev->siz_ptpargs = 0;
        dev->ispiped = 0;
    }
    else
        close (dev->fd);
    dev->fd = -1;
    dev->stopprt = 0;

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
BYTE            c;                      /* Print character           */

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
    switch (code) {

    case 0x01:
    /*---------------------------------------------------------------*/
    /* WRITE WITHOUT SPACING                                         */
    /*---------------------------------------------------------------*/
        eor = "\r";
        goto write;

    case 0x09:
    /*---------------------------------------------------------------*/
    /* WRITE AND SPACE 1 LINE                                        */
    /*---------------------------------------------------------------*/
        eor = dev->crlf ? "\r\n" : "\n";
        goto write;

    case 0x11:
    /*---------------------------------------------------------------*/
    /* WRITE AND SPACE 2 LINES                                       */
    /*---------------------------------------------------------------*/
        eor = dev->crlf ? "\r\n\n" : "\n\n";
        goto write;

    case 0x19:
    /*---------------------------------------------------------------*/
    /* WRITE AND SPACE 3 LINES                                       */
    /*---------------------------------------------------------------*/
        eor = dev->crlf ? "\r\n\n\n" : "\n\n\n";
        goto write;

    case 0x89:
    /*---------------------------------------------------------------*/
    /* WRITE AND SKIP TO CHANNEL 1                                   */
    /*---------------------------------------------------------------*/
        eor = dev->crlf ? "\r\f" : "\f";
        goto write;

    case 0xC9:
    /*---------------------------------------------------------------*/
    /* WRITE AND SKIP TO CHANNEL 9                                   */
    /*---------------------------------------------------------------*/
        eor = dev->crlf ? "\r\n" : "\n";
        goto write;

    case 0xE1:
    /*---------------------------------------------------------------*/
    /* WRITE AND SKIP TO CHANNEL 12                                  */
    /*---------------------------------------------------------------*/
        eor = dev->crlf ? "\r\n" : "\n";
        goto write;

    write:
        /* Start a new record if not data-chained from previous CCW */
        if ((chained & CCW_FLAGS_CD) == 0)
        {
            dev->printpos = 0;
            dev->printrem = LINE_LENGTH;

        } /* end if(!data-chained) */

        /* Calculate number of bytes to write and set residual count */
        num = (count < dev->printrem) ? count : dev->printrem;
        *residual = count - num;

        /* Copy data from channel buffer to print buffer */
        for (i = 0; i < num; i++)
        {
            c = guest_to_host(iobuf[i]);

            if (dev->fold) c = toupper(c);
            if (c == 0) c = SPACE;

            dev->buf[dev->printpos] = c;
            dev->printpos++;
            dev->printrem--;
        } /* end for(i) */

        /* Perform end of record processing if not data-chaining */
        if ((flags & CCW_FLAGS_CD) == 0)
        {
            /* Truncate trailing blanks from print line */
            for (i = dev->printpos; i > 0; i--)
                if (dev->buf[i-1] != SPACE) break;

            /* Append carriage return and line feed(s) */
            strcpy ((char *)(dev->buf + i), eor);
            i += strlen(eor);

            /* Write print line */
            write_buffer (dev, (char *)dev->buf, i, unitstat);
            if (*unitstat != 0) break;

        } /* end if(!data-chaining) */

        /* Return normal status */
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x03:
    /*---------------------------------------------------------------*/
    /* CONTROL NO-OPERATION                                          */
    /*---------------------------------------------------------------*/
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
        if (dev->devtype == 1403 || ccwseq > 1
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
    /* DIAGNOSTIC READ FCB                                           */
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

    case 0x0B:
    /*---------------------------------------------------------------*/
    /* SPACE 1 LINE IMMEDIATE                                        */
    /*---------------------------------------------------------------*/
        eor = dev->crlf ? "\r\n" : "\n";
        write_buffer (dev, eor, strlen(eor), unitstat);
        if (*unitstat != 0) break;

    /*
        *residual = 0;
    */
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x13:
    /*---------------------------------------------------------------*/
    /* SPACE 2 LINES IMMEDIATE                                       */
    /*---------------------------------------------------------------*/
        eor = dev->crlf ? "\r\n\n" : "\n\n";
        write_buffer (dev, eor, strlen(eor), unitstat);
        if (*unitstat != 0) break;

    /*
        *residual = 0;
    */
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x1B:
    /*---------------------------------------------------------------*/
    /* SPACE 3 LINES IMMEDIATE                                       */
    /*---------------------------------------------------------------*/
        eor = dev->crlf ? "\r\n\n\n" : "\n\n\n";
        write_buffer (dev, eor, strlen(eor), unitstat);
        if (*unitstat != 0) break;

    /*
        *residual = 0;
    */
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

    case 0x8B:
    /*---------------------------------------------------------------*/
    /* SKIP TO CHANNEL 1 IMMEDIATE                                   */
    /*---------------------------------------------------------------*/
        eor = dev->crlf ? "\r\f" : "\f";
        write_buffer (dev, eor, strlen(eor), unitstat);
        if (*unitstat != 0) break;

    /*
        *residual = 0;
    */
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0xCB:
    /*---------------------------------------------------------------*/
    /* SKIP TO CHANNEL 9 IMMEDIATE                                   */
    /*---------------------------------------------------------------*/
        eor = dev->crlf ? "\r\n" : "\n";
        write_buffer (dev, eor, strlen(eor), unitstat);
        if (*unitstat != 0) break;

    /*
        *residual = 0;
    */
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0xE3: case 0xDB:
    /*---------------------------------------------------------------*/
    /* SKIP TO CHANNEL 12 IMMEDIATE (or 11)                          */
    /*---------------------------------------------------------------*/
        eor = dev->crlf ? "\r\n" : "\n";
        write_buffer (dev, eor, strlen(eor), unitstat);
        if (*unitstat != 0) break;

    /*
        *residual = 0;
    */
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x63:
    /*---------------------------------------------------------------*/
    /* LOAD FORMS CONTROL BUFFER                                     */
    /*---------------------------------------------------------------*/
        /* Return normal status */
        *residual = 0;
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
        if (dev->devtype == 0x1403 && prevcode != 0xEB)
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

/* PRINTER.C    (c) Copyright Roger Bowler, 1999-2001                */
/*              ESA/390 Line Printer Device Handler                  */

/*-------------------------------------------------------------------*/
/* This module contains device handling functions for emulated       */
/* System/370 line printer devices.                                  */
/*-------------------------------------------------------------------*/

#include "hercules.h"

/*-------------------------------------------------------------------*/
/* Internal macro definitions                                        */
/*-------------------------------------------------------------------*/
#define LINE_LENGTH     150
#define SPACE           ((BYTE)' ')

/*-------------------------------------------------------------------*/
/* Subroutine to write data to the printer                           */
/*-------------------------------------------------------------------*/
static void
write_buffer (DEVBLK *dev, BYTE *buf, int len, BYTE *unitstat)
{
int             rc;                     /* Return code               */

    /* Write data to the printer file */
    rc = write (dev->fd, buf, len);

    /* Equipment check if error writing to printer file */
    if (rc < len)
    {
        logmsg ("HHC414I Error writing to %s: %s\n",
                dev->filename,
                (errno == 0 ? "incomplete": strerror(errno)));
        dev->sense[0] = SENSE_EC;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return;
    }

} /* end function write_buffer */

/*-------------------------------------------------------------------*/
/* Initialize the device handler                                     */
/*-------------------------------------------------------------------*/
int printer_init_handler (DEVBLK *dev, int argc, BYTE *argv[])
{
int     i;                              /* Array subscript           */

    /* The first argument is the file name */
    if (argc == 0 || strlen(argv[0]) > sizeof(dev->filename)-1)
    {
        fprintf (stderr,
                "HHC411I File name missing or invalid\n");
        return -1;
    }

    /* Save the file name in the device block */
    strcpy (dev->filename, argv[0]);

    /* Initialize device dependent fields */
    dev->fd = -1;
    dev->printpos = 0;
    dev->printrem = LINE_LENGTH;
    dev->diaggate = 0;
    dev->fold = 0;
    dev->crlf = 0;

    /* Process the driver arguments */
    for (i = 1; i < argc; i++)
    {
        if (strcasecmp(argv[i], "crlf") == 0)
        {
            dev->crlf = 1;
            continue;
        }

        fprintf (stderr, "HHC412I Invalid argument: %s\n",
                argv[i]);
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
void printer_query_device (DEVBLK *dev, BYTE **class,
                int buflen, BYTE *buffer)
{

    *class = "PRT";
    snprintf (buffer, buflen, "%s%s",
                dev->filename,
                (dev->crlf ? " crlf" : ""));

} /* end function printer_query_device */

/*-------------------------------------------------------------------*/
/* Close the device                                                  */
/*-------------------------------------------------------------------*/
int printer_close_device ( DEVBLK *dev )
{
    /* Close the device file */
    close (dev->fd);
    dev->fd = -1;

    return 0;
} /* end function printer_close_device */

/*-------------------------------------------------------------------*/
/* Execute a Channel Command Word                                    */
/*-------------------------------------------------------------------*/
void printer_execute_ccw (DEVBLK *dev, BYTE code, BYTE flags,
        BYTE chained, U16 count, BYTE prevcode, int ccwseq,
        BYTE *iobuf, BYTE *more, BYTE *unitstat, U16 *residual)
{
int             rc;                     /* Return code               */
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
    {
        rc = open (dev->filename,
                    O_WRONLY | O_CREAT | O_TRUNC /* | O_SYNC */,
                    S_IRUSR | S_IWUSR | S_IRGRP);
        if (rc < 0)
        {
            /* Handle open failure */
            logmsg ("HHC413I Error opening file %s: %s\n",
                    dev->filename, strerror(errno));

            /* Set unit check with intervention required */
            dev->sense[0] = SENSE_IR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            return;
        }
        dev->fd = rc;
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
            c = ebcdic_to_ascii[iobuf[i]];

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
            strcpy (dev->buf + i, eor);
            i += strlen(eor);

            /* Write print line */
            write_buffer (dev, dev->buf, i, unitstat);
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

        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x13:
    /*---------------------------------------------------------------*/
    /* SPACE 2 LINES IMMEDIATE                                       */
    /*---------------------------------------------------------------*/
        eor = dev->crlf ? "\r\n\n" : "\n\n";
        write_buffer (dev, eor, strlen(eor), unitstat);
        if (*unitstat != 0) break;

        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x1B:
    /*---------------------------------------------------------------*/
    /* SPACE 3 LINES IMMEDIATE                                       */
    /*---------------------------------------------------------------*/
        eor = dev->crlf ? "\r\n\n\n" : "\n\n\n";
        write_buffer (dev, eor, strlen(eor), unitstat);
        if (*unitstat != 0) break;

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
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x7B:
    /*---------------------------------------------------------------*/
    /* ALLOW DATA CHECK                                              */
    /*---------------------------------------------------------------*/
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x8B:
    /*---------------------------------------------------------------*/
    /* SKIP TO CHANNEL 1 IMMEDIATE                                   */
    /*---------------------------------------------------------------*/
        eor = dev->crlf ? "\r\f" : "\f";
        write_buffer (dev, eor, strlen(eor), unitstat);
        if (*unitstat != 0) break;

        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0xCB:
    /*---------------------------------------------------------------*/
    /* SKIP TO CHANNEL 9 IMMEDIATE                                   */
    /*---------------------------------------------------------------*/
        eor = dev->crlf ? "\r\n" : "\n";
        write_buffer (dev, eor, strlen(eor), unitstat);
        if (*unitstat != 0) break;

        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0xE3:
    /*---------------------------------------------------------------*/
    /* SKIP TO CHANNEL 12 IMMEDIATE                                  */
    /*---------------------------------------------------------------*/
        eor = dev->crlf ? "\r\n" : "\n";
        write_buffer (dev, eor, strlen(eor), unitstat);
        if (*unitstat != 0) break;

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
        *residual = 0;
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
        *residual = 0;
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


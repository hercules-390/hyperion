/* CARDRDR.C    (c) Copyright Roger Bowler, 1999-2001                */
/*              ESA/390 Card Reader Device Handler                   */

/*-------------------------------------------------------------------*/
/* This module contains device handling functions for emulated       */
/* card reader devices.                                              */
/*-------------------------------------------------------------------*/

#include "hercules.h"

/*-------------------------------------------------------------------*/
/* Internal macro definitions                                        */
/*-------------------------------------------------------------------*/
#define CARD_SIZE       80
#define HEX40           ((BYTE)0x40)

/*-------------------------------------------------------------------*/
/* Initialize the device handler                                     */
/*-------------------------------------------------------------------*/
int cardrdr_init_handler ( DEVBLK *dev, int argc, BYTE *argv[] )
{
int     i;                              /* Array subscript           */
int     fc;                             /* File counter              */

    /* The first argument is the file name */
    if (argc > 0)
    {
        /* Check for valid file name */
        if (strlen(argv[0]) > sizeof(dev->filename)-1)
        {
            logmsg ("HHC401I File name too long\n");
            return -1;
        }
        /* Added by VB, after suggestion by Richard Snow - Aug 26, 2001   */
        /* Check if reader file exists, to avoid gazillion error messages */
        dev->fd = open(argv[0], O_RDONLY);
        if(dev->fd < 0)
        {       /* file does NOT exist                              */
                /* issue message, and insert empty filename instead */
                logmsg ("HHC402I File %s does not exist\n", argv[0]);
                strcpy(argv[0], "");
        }
        else
        {
                /* if file DID exist, we need to close it now */
                close(dev->fd);
        }
        /* end of change VB/RSS Aug 2001                            */

        /* Save the file name in the device block */
        strcpy (dev->filename, argv[0]);
    }
    else
        dev->filename[0] = '\0';

    /* Initialize device dependent fields */
    dev->fd = -1;
    dev->multifile = 0;
    dev->rdreof = 0;
    dev->ebcdic = 0;
    dev->ascii = 0;
    dev->trunc = 0;
    dev->cardpos = 0;
    dev->cardrem = 0;
    dev->autopad = 0;
    fc = 0;

    dev->more_files = malloc(sizeof(char *) * (fc + 1));
    if (!dev->more_files)
    {
        logmsg ("HHC403I Out of memory\n");
        return -1;
    }
    dev->more_files[fc] = NULL;
    /* Process the driver arguments */
    for (i = 1; i < argc; i++)
    {
        /* multifile means to automatically open the next
           i/p file if multiple i/p files are defined.   */
        if (strcasecmp(argv[i], "multifile") == 0)
        {
            dev->multifile = 1;
            continue;
        }

        /* eof means that unit exception will be returned at
           end of file, instead of intervention required */
        if (strcasecmp(argv[i], "eof") == 0)
        {
            dev->rdreof = 1;
            continue;
        }

        /* ebcdic means that the card image file consists of
           fixed length 80-byte EBCDIC card images with no
           line-end delimiters */
        if (strcasecmp(argv[i], "ebcdic") == 0)
        {
            dev->ebcdic = 1;
            continue;
        }

        /* ascii means that the card image file consists of
           variable length ASCII records delimited by either
           line-feed or carriage-return line-feed sequences */
        if (strcasecmp(argv[i], "ascii") == 0)
        {
            dev->ascii = 1;
            continue;
        }

        /* trunc means that records longer than 80 bytes will
           be silently truncated to 80 bytes when processing a
           variable length ASCII file.  The default behaviour
           is to present a data check if an overlength record
           is encountered.  The trunc option is ignored except
           when processing an ASCII card image file. */
        if (strcasecmp(argv[i], "trunc") == 0)
        {
            dev->trunc = 1;
            continue;
        }

        /* autopad means that if reading fixed sized records
         * (ebcdic) and end of file is reached in the middle of
         * a record, the record is automatically padded to 80 bytes.
         */
        if (strcasecmp(argv[i], "autopad") == 0)
        {
            dev->autopad = 1;
            continue;
        }
        // add additional file arguments
        dev->more_files[fc++] = strdup(argv[i]);
        dev->more_files = realloc(dev->more_files, sizeof(char *) * (fc + 1));
        if (!dev->more_files)
        {
            logmsg ("HHC403I Out of memory\n");
            return -1;
        }
        dev->more_files[fc] = NULL;
    }
    dev->current_file = dev->more_files;
    /* Check for conflicting arguments */
    if (dev->ebcdic && dev->ascii)
    {
        logmsg ("HHC403I Specify ASCII or EBCDIC but not both\n");
        return -1;
    }

    /* Set length of card image buffer */
    dev->bufsize = CARD_SIZE;

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
} /* end function cardrdr_init_handler */

/*-------------------------------------------------------------------*/
/* Query the device definition                                       */
/*-------------------------------------------------------------------*/
void cardrdr_query_device (DEVBLK *dev, BYTE **class,
                int buflen, BYTE *buffer)
{

    *class = "RDR";
    snprintf (buffer, buflen, "%s%s%s%s%s%s%s",
                dev->filename,
                (dev->multifile ? " multifile" : ""),
                (dev->ascii ? " ascii" : ""),
                (dev->ebcdic ? " ebcdic" : ""),
                (dev->autopad ? " autopad" : ""),
                (dev->rdreof ? " eof" : ""),
                ((dev->ascii && dev->trunc) ? " trunc" : ""));

} /* end function cardrdr_query_device */

/*-------------------------------------------------------------------*/
/* Close the device                                                  */
/*-------------------------------------------------------------------*/
int cardrdr_close_device ( DEVBLK *dev )
{
    /* Close the device file */
    close (dev->fd);
    dev->fd = -1;

    return 0;
} /* end function cardrdr_close_device */


/*-------------------------------------------------------------------*/
/* Clear the card reader                                             */
/*-------------------------------------------------------------------*/
static void clear_cardrdr ( DEVBLK *dev )
{
    /* Close the card image file */
    cardrdr_close_device (dev);

    /* Clear the file name */
    dev->filename[0] = '\0';

    /* If next file is available, open it */
    if (dev->current_file && *(dev->current_file))
    {
        strcpy(dev->filename, *(dev->current_file++));
    } else {

        /* Reset the device dependent flags */
        dev->multifile = 0;
        dev->ascii = 0;
        dev->ebcdic = 0;
        dev->rdreof = 0;
        dev->trunc = 0;
        dev->autopad = 0;
    }
} /* end function clear_cardrdr */


/*-------------------------------------------------------------------*/
/* Open the card image file                                          */
/*-------------------------------------------------------------------*/
static int open_cardrdr ( DEVBLK *dev, BYTE *unitstat )
{
int     rc;                             /* Return code               */
int     i;                              /* Array subscript           */
int     len;                            /* Length of data            */
BYTE    buf[160];                       /* Auto-detection buffer     */

    /* Intervention required if device has no file name */
    if (dev->filename[0] == '\0')
    {
        dev->sense[0] = SENSE_IR;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return -1;
    }
    *unitstat=0;
    /* Open the device file */
#ifdef WIN32
    rc = open (dev->filename, O_RDONLY | O_BINARY);
#else /* WIN32 */
    rc = open (dev->filename, O_RDONLY);
#endif /* WIN32 */
    if (rc < 0)
    {
        /* Handle open failure */
        logmsg ("HHC404I Error opening file %s: %s\n",
                dev->filename, strerror(errno));

        /* Set unit check with equipment check */
        dev->sense[0] = SENSE_EC;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return -1;
    }

    /* Save the file descriptor in the device block */
    dev->fd = rc;

    /* If neither EBCDIC nor ASCII was specified, attempt to
       detect the format by inspecting the first 160 bytes */
    if (dev->ebcdic == 0 && dev->ascii == 0)
    {
        /* Read first 160 bytes of file into the buffer */
        len = read (dev->fd, buf, sizeof(buf));
        if (len < 0)
        {
            /* Handle read error condition */
            logmsg ("HHC405I Error reading file %s: %s\n",
                    dev->filename, strerror(errno));

            /* Close the file */
            close (dev->fd);
            dev->fd = -1;

            /* Set unit check with equipment check */
            dev->sense[0] = SENSE_EC;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            return -1;
        }

        /* Assume ASCII format if first 160 bytes contain only ASCII
           characters, carriage return, line feed, tab, or EOF */
        for (i = 0, dev->ascii = 1; i < len && buf[i] != '\x1A'; i++)
        {
            if ((buf[i] < 0x20 || buf[i] > 0x7F)
                && buf[i] != '\r' && buf[i] != '\n'
                && buf[i] != '\t')
            {
                dev->ascii = 0;
                dev->ebcdic = 1;
                break;
            }
        } /* end for(i) */

        /* Rewind to start of file */
        rc = lseek (dev->fd, 0, SEEK_SET);
        if (rc < 0)
        {
            /* Handle seek error condition */
            logmsg ("HHC406I Seek error in file %s: %s\n",
                    dev->filename, strerror(errno));

            /* Close the file */
            close (dev->fd);
            dev->fd = -1;

            /* Set unit check with equipment check */
            dev->sense[0] = SENSE_EC;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            return -1;
        }

    } /* end if(auto-detect) */

    return 0;
} /* end function open_cardrdr */


/*-------------------------------------------------------------------*/
/* Read an 80-byte EBCDIC card image into the device buffer          */
/*-------------------------------------------------------------------*/
static int read_ebcdic ( DEVBLK *dev, BYTE *unitstat )
{
int     rc;                             /* Return code               */

    /* Read 80 bytes of card image data into the device buffer */
    rc = read (dev->fd, dev->buf, CARD_SIZE);

    if ((rc > 0) && (rc < CARD_SIZE) && dev->autopad)
        {
        memset(&dev->buf[rc], 0, CARD_SIZE - rc);
                rc = CARD_SIZE;
    }

    /* Handle end-of-file condition */
    if (rc == 0)
    {
        /* Return unit exception or intervention required */
        if (dev->rdreof)
        {
            *unitstat = CSW_CE | CSW_DE | CSW_UX;
        }
        else
        {
            dev->sense[0] = SENSE_IR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
        }

        /* Close the file and clear the file name and flags */
        clear_cardrdr (dev);

        return -2;
    }

    /* Handle read error condition */
    if (rc < CARD_SIZE)
    {
        if (rc < 0)
            logmsg ("HHC407I Error reading file %s: %s\n",
                    dev->filename, strerror(errno))
        else
            logmsg ("HHC408I Unexpected end of file on %s\n",
                    dev->filename);

        /* Set unit check with equipment check */
        dev->sense[0] = SENSE_EC;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return -1;
    }

    return 0;
} /* end function read_ebcdic */


/*-------------------------------------------------------------------*/
/* Read a variable length ASCII card image into the device buffer    */
/*-------------------------------------------------------------------*/
static int read_ascii ( DEVBLK *dev, BYTE *unitstat )
{
int     rc;                             /* Return code               */
int     i;                              /* Array subscript           */
BYTE    c;                              /* Input character           */

    /* Prefill the card image with EBCDIC blanks */
    memset (dev->buf, HEX40, CARD_SIZE);

    /* Read up to 80 bytes into device buffer */
    for (i = 0; ; )
    {
        /* Read next byte of card image */
        rc = read (dev->fd, &c, 1);

        /* Handle end-of-file condition */
        if (rc == 0 || c == '\x1A')
        {
            /* End of record if there is any data in buffer */
            if (i > 0) break;

            /* Return unit exception or intervention required */
            if (dev->rdreof)
            {
                *unitstat = CSW_CE | CSW_DE | CSW_UX;
            }
            else
            {
                dev->sense[0] = SENSE_IR;
                *unitstat = CSW_CE | CSW_DE | CSW_UC;
            }

            /* Close the file and clear the file name and flags */
            clear_cardrdr (dev);

            return -2;
        }

        /* Handle read error condition */
        if (rc < 0)
        {
            logmsg ("HHC409I Error reading file %s: %s\n",
                    dev->filename, strerror(errno));

            /* Set unit check with equipment check */
            dev->sense[0] = SENSE_EC;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            return -1;
        }

        /* Ignore carriage return */
        if (c == '\r') continue;

        /* Line-feed indicates end of variable length record */
        if (c == '\n') break;

        /* Expand tabs to spaces */
        if (c == '\t')
        {
            do {i++;} while ((i & 7) && (i < CARD_SIZE));
            continue;
        }

        /* Test for overlength record */
        if (i >= CARD_SIZE)
        {
            /* Ignore excess characters if trunc option specified */
            if (dev->trunc) continue;

            logmsg ("HHC410I Card image exceeds %d bytes in file %s\n",
                    CARD_SIZE, dev->filename);

            /* Set unit check with data check */
            dev->sense[0] = SENSE_DC;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            return -1;
        }

        /* Convert character to EBCDIC and store in device buffer */
        dev->buf[i++] = ascii_to_ebcdic[c];

    } /* end for(i) */

    return 0;
} /* end function read_ascii */


/*-------------------------------------------------------------------*/
/* Execute a Channel Command Word                                    */
/*-------------------------------------------------------------------*/
void cardrdr_execute_ccw ( DEVBLK *dev, BYTE code, BYTE flags,
        BYTE chained, U16 count, BYTE prevcode, int ccwseq,
        BYTE *iobuf, BYTE *more, BYTE *unitstat, U16 *residual )
{
int     rc;                             /* Return code               */
int     num;                            /* Number of bytes to move   */

    /* Open the device file if necessary */
    if (dev->fd < 0 && !IS_CCW_SENSE(code))
    {
        rc = open_cardrdr (dev, unitstat);
        if (rc) return;
    }

    /* Turn all read/feed commands into read, feed, select stacker 1 */
    if ((code & 0x17) == 0x02) code = 0x02;

    /* Process depending on CCW opcode */
    switch (code) {

    case 0x02:
    /*---------------------------------------------------------------*/
    /* READ                                                          */
    /*---------------------------------------------------------------*/
        /* Read next card if not data-chained from previous CCW */
        if ((chained & CCW_FLAGS_CD) == 0)
        {
                        for (;;)
                        {
                                /* Read ASCII or EBCDIC card image */
                                if (dev->ascii)
                                        rc = read_ascii (dev, unitstat);
                                else
                                        rc = read_ebcdic (dev, unitstat);

                                if (0
                                        || rc != -2
                                        || !dev->multifile
                                        || open_cardrdr (dev, unitstat) != 0
                                        )
                                break;
                        }

            /* Return error status if read was unsuccessful */
            if (rc) break;

            /* Initialize number of bytes in current card */
            dev->cardpos = 0;
            dev->cardrem = CARD_SIZE;

        } /* end if(!data-chained) */

        /* Calculate number of bytes to read and set residual count */
        num = (count < dev->cardrem) ? count : dev->cardrem;
        *residual = count - num;
        if (count < dev->cardrem) *more = 1;

        /* Copy data from card image buffer into channel buffer */
        memcpy (iobuf, dev->buf + dev->cardpos, num);

        /* Update number of bytes remaining in card image buffer */
        dev->cardpos += num;
        dev->cardrem -= num;

        /* Return normal status */
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x03:
    /*---------------------------------------------------------------*/
    /* CONTROL NO-OPERATION                                          */
    /*---------------------------------------------------------------*/
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

} /* end function cardrdr_execute_ccw */


/* FBADASD.C    (c) Copyright Roger Bowler, 1999-2002                */
/*              ESA/390 FBA Direct Access Storage Device Handler     */

/*-------------------------------------------------------------------*/
/* This module contains device handling functions for emulated       */
/* fixed block architecture direct access storage devices.           */
/*-------------------------------------------------------------------*/

/*-------------------------------------------------------------------*/
/* Additional credits:                                               */
/*      0671 device support by Jay Maynard                           */
/*-------------------------------------------------------------------*/

#include "hercules.h"

#include "devtype.h"

/*-------------------------------------------------------------------*/
/* Bit definitions for Define Extent file mask                       */
/*-------------------------------------------------------------------*/
#define FBAMASK_CTL             0xC0    /* Operation control bits... */
#define FBAMASK_CTL_INHFMT      0x00    /* ...inhibit format writes  */
#define FBAMASK_CTL_INHWRT      0x40    /* ...inhibit all writes     */
#define FBAMASK_CTL_ALLWRT      0xC0    /* ...allow all writes       */
#define FBAMASK_CTL_RESV        0x80    /* ...reserved bit setting   */
#define FBAMASK_CE              0x08    /* CE field extent           */
#define FBAMASK_DIAG            0x04    /* Permit diagnostic command */
#define FBAMASK_RESV            0x33    /* Reserved bits - must be 0 */

/*-------------------------------------------------------------------*/
/* Bit definitions for Locate operation byte                         */
/*-------------------------------------------------------------------*/
#define FBAOPER_RESV            0xF0    /* Reserved bits - must be 0 */
#define FBAOPER_CODE            0x0F    /* Operation code bits...    */
#define FBAOPER_WRITE           0x01    /* ...write data             */
#define FBAOPER_READREP         0x02    /* ...read replicated data   */
#define FBAOPER_FMTDEFC         0x04    /* ...format defective block */
#define FBAOPER_WRTVRFY         0x05    /* ...write data and verify  */
#define FBAOPER_READ            0x06    /* ...read data              */

static int fbadasd_read_block (DEVBLK *, BYTE *, int, BYTE *);
static int fbadasd_write_block (DEVBLK *, BYTE *, int, BYTE *);

/*-------------------------------------------------------------------*/
/* Initialize the device handler                                     */
/*-------------------------------------------------------------------*/
int fbadasd_init_handler ( DEVBLK *dev, int argc, BYTE *argv[] )
{
int     rc;                             /* Return code               */
struct  stat statbuf;                   /* File information          */
U32     startblk;                       /* Device origin block number*/
U32     numblks;                        /* Device block count        */
BYTE    c;                              /* Character work area       */
int     cfba = 0;                       /* 1 = Compressed fba        */
int     i;                              /* Loop index                */
CKDDASD_DEVHDR  devhdr;                 /* Device header             */
CCKDDASD_DEVHDR cdevhdr;                /* Compressed device header  */

    /* The first argument is the file name */
    if (argc == 0 || strlen(argv[0]) > sizeof(dev->filename)-1)
    {
        devmsg ("HHC301I File name missing or invalid\n");
        return -1;
    }

    /* Save the file name in the device block */
    strcpy (dev->filename, argv[0]);

    /* Open the device file */
    dev->fd = open (dev->filename, O_RDWR|O_BINARY);
    if (dev->fd < 0)
    {
        dev->fd = open (dev->filename, O_RDONLY|O_BINARY);
        if (dev->fd < 0)
        {
            devmsg ("HHC302I File %s open error: %s\n",
                    dev->filename, strerror(errno));
            return -1;
        }
    }

    /* Read the first block to see if it's compressed */
    rc = read (dev->fd, &devhdr, CKDDASD_DEVHDR_SIZE);
    if (rc < CKDDASD_DEVHDR_SIZE)
    {
        /* Handle read error condition */
        if (rc < 0)
            devmsg (_("HHC303I Read error in file %s: %s\n"),
                    dev->filename, strerror(errno));
        else
            devmsg (_("HHC304I Unexpected end of file in %s\n"),
                    dev->filename);
        close (dev->fd);
        dev->fd = -1;
        return -1;
    }

    /* Processing for compressed fba dasd */
    if (memcmp (&devhdr.devid, "FBA_C370", 8) == 0)
    {
        cfba = 1;

        /* Read the compressed device header */
        rc = read (dev->fd, &cdevhdr, CCKDDASD_DEVHDR_SIZE);
        if (rc < CKDDASD_DEVHDR_SIZE)
        {
            /* Handle read error condition */
            if (rc < 0)
                devmsg (_("HHC303I Read error in file %s: %s\n"),
                        dev->filename, strerror(errno));
            else
                devmsg (_("HHC304I Unexpected end of file in %s\n"),
                        dev->filename);
            close (dev->fd);
            dev->fd = -1;
            return -1;
        }

        /* Set block size, device origin, and device size in blocks */
        dev->fbablksiz = 512;
        dev->fbaorigin = 0;
        dev->fbanumblk = ((U32)(cdevhdr.cyls[3]) << 24)
                       | ((U32)(cdevhdr.cyls[2]) << 16)
                       | ((U32)(cdevhdr.cyls[1]) << 8)
                       |  (U32)(cdevhdr.cyls[0]);

        /* Default to synchronous I/O */
        dev->syncio = 1;

        /* process the remaining arguments */
        for (i = 1; i < argc; i++)
        {
            if (strlen (argv[i]) > 3
             && memcmp ("sf=", argv[i], 3) == 0)
            {
                if (strlen(argv[i]+3) < 256)
                    strcpy (dev->dasdsfn, argv[i]+3);
                continue;
            }
            if (strcasecmp ("nosyncio", argv[i]) == 0
             || strcasecmp ("nosyio",   argv[i]) == 0)
            {
                dev->syncio = 0;
                continue;
            }
            if (strcasecmp ("syncio", argv[i]) == 0
             || strcasecmp ("syio",   argv[i]) == 0)
            {
                dev->syncio = 1;
                continue;
            }

            devmsg (_("HHC351I parameter %d is invalid: %s\n"),
                    i + 1, argv[i]);
            return -1;
        }
    }

    /* Processing for regullar fba dasd */
    else
    {
        /* Determine the device size */
        rc = fstat (dev->fd, &statbuf);
        if (rc < 0)
        {
            devmsg ("HHC305I File %s fstat error: %s\n",
                    dev->filename, strerror(errno));
            close (dev->fd);
            dev->fd = -1;
            return -1;
        }

        /* Set block size, device origin, and device size in blocks */
        dev->fbablksiz = 512;
        dev->fbaorigin = 0;
        dev->fbanumblk = statbuf.st_size / dev->fbablksiz;

        /* The second argument is the device origin block number */
        if (argc >= 2)
        {
            if (sscanf(argv[1], "%u%c", &startblk, &c) != 1
             || startblk >= dev->fbanumblk)
            {
                devmsg ("HHC306I Invalid device origin block number %s\n",
                        argv[1]);
                close (dev->fd);
                dev->fd = -1;
                return -1;
            }
            dev->fbaorigin = startblk;
            dev->fbanumblk -= startblk;
        }

        /* The third argument is the device block count */
        if (argc >= 3 && strcmp(argv[2],"*") != 0)
        {
            if (sscanf(argv[2], "%u%c", &numblks, &c) != 1
             || numblks > dev->fbanumblk)
            {
                devmsg ("HHC307I Invalid device block count %s\n",
                        argv[2]);
                close (dev->fd);
                dev->fd = -1;
                return -1;
            }
            dev->fbanumblk = numblks;
        }
    }

    devmsg ("fbadasd: %s origin=%d blks=%d\n",
            dev->filename, dev->fbaorigin, dev->fbanumblk);

    /* Set number of sense bytes */
    dev->numsense = 24;

    /* Locate the FBA dasd table entry */
    dev->fbatab = dasd_lookup (DASD_FBADEV, NULL, dev->devtype, dev->fbanumblk);
    if (dev->fbatab == NULL)
    {
        devmsg ("HHC308I %4.4X device type %4.4X not found in dasd table\n",
                dev->devnum, dev->devtype);
        close (dev->fd);
        dev->fd = -1;
        return -1;
    }

    /* Build the devid area */
    dev->numdevid = dasd_build_fba_devid (dev->fbatab,(BYTE *)&dev->devid);

    /* Build the devchar area */
    dev->numdevchar = dasd_build_fba_devchar (dev->fbatab,
                                 (BYTE *)&dev->devchar,dev->fbanumblk);

    /* Set the routine addresses for read_block and write_block */
    dev->fbardblk = &fbadasd_read_block;
    dev->fbawrblk = &fbadasd_write_block;

    /* Activate I/O tracing */
//  dev->ccwtrace = 1;

    /* Call the compressed init handler if compressed fba */
    if (cfba)
        return cckddasd_init_handler (dev, argc, argv);

    return 0;
} /* end function fbadasd_init_handler */

/*-------------------------------------------------------------------*/
/* Query the device definition                                       */
/*-------------------------------------------------------------------*/
void fbadasd_query_device (DEVBLK *dev, BYTE **class,
                int buflen, BYTE *buffer)
{

    *class = "DASD";
    snprintf (buffer, buflen, "%s [%d,%d]",
            dev->filename,
            dev->fbaorigin, dev->fbanumblk);

} /* end function fbadasd_query_device */

/*-------------------------------------------------------------------*/
/* Close the device                                                  */
/*-------------------------------------------------------------------*/
int fbadasd_close_device ( DEVBLK *dev )
{
    /* Close the device file */
    close (dev->fd);
    dev->fd = -1;

    return 0;
} /* end function fbadasd_close_device */

/*-------------------------------------------------------------------*/
/* Read an fba block                                                 */
/*-------------------------------------------------------------------*/
static int fbadasd_read_block (DEVBLK *dev, BYTE *buf, int len,
                               BYTE *unitstat)
{
int     rc;                             /* Return code               */
off_t   rcoff;                          /* Return value from lseek() */

    rcoff = lseek (dev->fd, dev->fbarba, SEEK_SET);
    if (rcoff < 0)
    {
        /* Handle seek error condition */
        devmsg (_("HHC311I Seek error in file %s: %s\n"),
                dev->filename, strerror(errno));

        /* Set unit check with equipment check */
        dev->sense[0] = SENSE_EC;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return -1;
    }

    rc = read (dev->fd, buf, len);
    if (rc < len)
    {
        /* Handle read error condition */
        if (rc < 0)
            devmsg (_("HHC312I Read error in file %s: %s\n"),
                    dev->filename, strerror(errno));
        else
            devmsg (_("HHC313I Unexpected end of file in %s\n"),
                    dev->filename);

        /* Set unit check with equipment check */
        dev->sense[0] = SENSE_EC;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return -1;
    }

    dev->fbarba += len;

    return len;
} /* end function fbadasd_read_block */

/*-------------------------------------------------------------------*/
/* Write an fba block                                                */
/*-------------------------------------------------------------------*/
static int fbadasd_write_block (DEVBLK *dev, BYTE *buf, int len,
                                BYTE *unitstat)
{
int     rc;                             /* Return code               */
off_t   rcoff;                          /* Return value from lseek() */

    rcoff = lseek (dev->fd, dev->fbarba, SEEK_SET);
    if (rcoff < 0)
    {
        /* Handle seek error condition */
        devmsg (_("HHC314I Seek error in file %s: %s\n"),
                dev->filename, strerror(errno));

        /* Set unit check with equipment check */
        dev->sense[0] = SENSE_EC;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return -1;
    }

    rc = write (dev->fd, buf, len);
    if (rc < len)
    {
        /* Handle write error condition */
        devmsg (_("HHC315I Write error in file %s: %s\n"),
                dev->filename, strerror(errno));

        /* Set unit check with equipment check */
        dev->sense[0] = SENSE_EC;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return -1;
    }

    dev->fbarba += len;

    return len;
} /* end function fbadasd_write_block */

/*-------------------------------------------------------------------*/
/* Execute a Channel Command Word                                    */
/*-------------------------------------------------------------------*/
void fbadasd_execute_ccw ( DEVBLK *dev, BYTE code, BYTE flags,
        BYTE chained, U16 count, BYTE prevcode, int ccwseq,
        BYTE *iobuf, BYTE *more, BYTE *unitstat, U16 *residual )
{
int     rc;                             /* Return code               */
int     num;                            /* Number of bytes to move   */
BYTE    hexzeroes[512];                 /* Bytes for zero fill       */
int     rem;                            /* Byte count for zero fill  */
int     repcnt;                         /* Replication count         */

    /* Reset extent flag at start of CCW chain */
    if (chained == 0)
        dev->fbaxtdef = 0;

    /* Process depending on CCW opcode */
    switch (code) {

    case 0x02:
    /*---------------------------------------------------------------*/
    /* READ IPL                                                      */
    /*---------------------------------------------------------------*/
        /* Must be first CCW or chained from a previous READ IPL CCW */
        if (chained && prevcode != 0x02)
        {
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Zeroize the file mask and set extent for entire device */
        dev->fbamask = 0;
        dev->fbaxblkn = 0;
        dev->fbaxfirst = 0;
        dev->fbaxlast = dev->fbanumblk - 1;

        /* Seek to start of block zero */
        dev->fbarba = dev->fbaorigin * dev->fbablksiz;

        /* Overrun if data chaining */
        if ((flags & CCW_FLAGS_CD))
        {
            dev->sense[0] = SENSE_OR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Calculate number of bytes to read and set residual count */
        num = (count < dev->fbablksiz) ? count : dev->fbablksiz;
        *residual = count - num;
        if (count < dev->fbablksiz) *more = 1;

        /* Read physical block into channel buffer */
        rc = (dev->fbardblk) (dev, iobuf, num, unitstat);
        if (rc < num) break;

        /* Set extent defined flag */
        dev->fbaxtdef = 1;

        /* Set normal status */
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x03:
    /*---------------------------------------------------------------*/
    /* CONTROL NO-OPERATION                                          */
    /*---------------------------------------------------------------*/
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x41:
    /*---------------------------------------------------------------*/
    /* WRITE                                                         */
    /*---------------------------------------------------------------*/
        /* Reject if previous command was not LOCATE */
        if (prevcode != 0x43)
        {
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Reject if locate command did not specify write operation */
        if ((dev->fbaoper & FBAOPER_CODE) != FBAOPER_WRITE
            && (dev->fbaoper & FBAOPER_CODE) != FBAOPER_WRTVRFY)
        {
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Prepare a block of zeroes for write padding */
        memset (hexzeroes, 0, sizeof(hexzeroes));

        /* Write physical blocks of data to the device */
        while (dev->fbalcnum > 0)
        {
            /* Exit if data chaining and this CCW is exhausted */
            if ((flags & CCW_FLAGS_CD) && count == 0)
                break;

            /* Overrun if data chaining within a physical block */
            if ((flags & CCW_FLAGS_CD) && count < dev->fbablksiz)
            {
                dev->sense[0] = SENSE_OR;
                *unitstat = CSW_CE | CSW_DE | CSW_UC;
                break;
            }

            /* Calculate number of bytes to write */
            num = (count < dev->fbablksiz) ? count : dev->fbablksiz;

            /* Write physical block from channel buffer */
            if (num > 0)
            {
                rc = (dev->fbawrblk) (dev, iobuf, num, unitstat);
                if (rc < num) break;
            }

            /* Fill remainder of block with zeroes */
            if (num < dev->fbablksiz)
            {
                rem = dev->fbablksiz - num;
                rc = (dev->fbawrblk) (dev, hexzeroes, rem, unitstat);
                if (rc < rem) break;
            }

            /* Prepare to write next block */
            count -= num;
            iobuf += num;
            dev->fbalcnum--;

        } /* end while */

        /* Set residual byte count */
        *residual = count;
        if (dev->fbalcnum > 0) *more = 1;

        /* Set ending status */
        *unitstat |= CSW_CE | CSW_DE;
        break;

    case 0x42:
    /*---------------------------------------------------------------*/
    /* READ                                                          */
    /*---------------------------------------------------------------*/
        /* Reject if previous command was not LOCATE or READ IPL */
        if (prevcode != 0x43 && prevcode != 0x02)
        {
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Reject if locate command did not specify read operation */
        if (prevcode != 0x02
            && (dev->fbaoper & FBAOPER_CODE) != FBAOPER_READ
            && (dev->fbaoper & FBAOPER_CODE) != FBAOPER_READREP)
        {
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Read physical blocks of data from device */
        while (dev->fbalcnum > 0 && count > 0)
        {
            /* Overrun if data chaining within a physical block */
            if ((flags & CCW_FLAGS_CD) && count < dev->fbablksiz)
            {
                dev->sense[0] = SENSE_OR;
                *unitstat = CSW_CE | CSW_DE | CSW_UC;
                break;
            }

            /* Calculate number of bytes to read */
            num = (count < dev->fbablksiz) ? count : dev->fbablksiz;
            if (num < dev->fbablksiz) *more = 1;

            /* Read physical block into channel buffer */
            rc = (dev->fbardblk) (dev, iobuf, num, unitstat);
            if (rc < num) break;

            /* Prepare to read next block */
            count -= num;
            iobuf += num;
            dev->fbalcnum--;

        } /* end while */

        /* Set residual byte count */
        *residual = count;
        if (dev->fbalcnum > 0) *more = 1;

        /* Set ending status */
        *unitstat |= CSW_CE | CSW_DE;

        break;

    case 0x43:
    /*---------------------------------------------------------------*/
    /* LOCATE                                                        */
    /*---------------------------------------------------------------*/
        /* Calculate residual byte count */
        num = (count < 8) ? count : 8;
        *residual = count - num;

        /* Control information length must be at least 8 bytes */
        if (count < 8)
        {
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* LOCATE must be preceded by DEFINE EXTENT or READ IPL */
        if (dev->fbaxtdef == 0)
        {
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Save and validate the operation byte */
        dev->fbaoper = iobuf[0];
        if (dev->fbaoper & FBAOPER_RESV)
        {
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Validate and process operation code */
        if ((dev->fbaoper & FBAOPER_CODE) == FBAOPER_WRITE
            || (dev->fbaoper & FBAOPER_CODE) == FBAOPER_WRTVRFY)
        {
            /* Reject command if file mask inhibits all writes */
            if ((dev->fbamask & FBAMASK_CTL) == FBAMASK_CTL_INHWRT)
            {
                dev->sense[0] = SENSE_CR;
                *unitstat = CSW_CE | CSW_DE | CSW_UC;
                break;
            }
        }
        else if ((dev->fbaoper & FBAOPER_CODE) == FBAOPER_READ
                || (dev->fbaoper & FBAOPER_CODE) == FBAOPER_READREP)
        {
        }
        else if ((dev->fbaoper & FBAOPER_CODE) == FBAOPER_FMTDEFC)
        {
            /* Reject command if file mask inhibits format writes */
            if ((dev->fbamask & FBAMASK_CTL) == FBAMASK_CTL_INHFMT)
            {
                dev->sense[0] = SENSE_CR;
                *unitstat = CSW_CE | CSW_DE | CSW_UC;
                break;
            }
        }
        else /* Operation code is invalid */
        {
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Byte 1 contains the replication count */
        repcnt = iobuf[1];

        /* Bytes 2-3 contain the block count */
        dev->fbalcnum = (iobuf[2] << 8) | iobuf[3];

        /* Bytes 4-7 contain the displacement of the first block
           relative to the start of the dataset */
        dev->fbalcblk = (iobuf[4] << 24) | (iobuf[5] << 16)
                        | (iobuf[6] << 8) | iobuf[7];

        /* Verify that the block count is non-zero, and that
           the starting and ending blocks fall within the extent */
        if (dev->fbalcnum == 0
            || dev->fbalcnum - 1 > dev->fbaxlast
            || dev->fbalcblk < dev->fbaxfirst
            || dev->fbalcblk > dev->fbaxlast - (dev->fbalcnum - 1))
        {
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* For replicated data, verify that the replication count
           is non-zero and is a multiple of the block count */
        if ((dev->fbaoper & FBAOPER_CODE) == FBAOPER_READREP)
        {
            if (repcnt == 0 || repcnt % dev->fbalcnum != 0)
            {
                dev->sense[0] = SENSE_CR;
                *unitstat = CSW_CE | CSW_DE | CSW_UC;
                break;
            }
        }

        /* Position device to start of block */
        dev->fbarba = (dev->fbalcblk - dev->fbaxfirst
                     + dev->fbaorigin
                     + dev->fbaxblkn) * dev->fbablksiz;

        DEVTRACE("fbadasd: Positioning to %8.8llX (%llu)\n",
                 (long long unsigned int)dev->fbarba, (long long unsigned int)dev->fbarba);

        /* Return normal status */
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x63:
    /*---------------------------------------------------------------*/
    /* DEFINE EXTENT                                                 */
    /*---------------------------------------------------------------*/
        /* Calculate residual byte count */
        num = (count < 16) ? count : 16;
        *residual = count - num;

        /* Control information length must be at least 16 bytes */
        if (count < 16)
        {
            devmsg(_("fbadasd: define extent data too short: %d bytes\n"),
                    count);
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Reject if extent previously defined in this CCW chain */
        if (dev->fbaxtdef)
        {
            devmsg(_("fbadasd: second define extent in chain\n"));
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Save and validate the file mask */
        dev->fbamask = iobuf[0];
        if ((dev->fbamask & (FBAMASK_RESV | FBAMASK_CE))
            || (dev->fbamask & FBAMASK_CTL) == FBAMASK_CTL_RESV)
        {
            devmsg(_("fbadasd: invalid file mask %2.2X\n"),
                    dev->fbamask);
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

//      VM/ESA sends 0x00000200 0x00000000 0x00000000 0x0001404F
//      /* Verify that bytes 1-3 are zeroes */
//      if (iobuf[1] != 0 || iobuf[2] != 0 || iobuf[3] != 0)
//      {
//          devmsg(_("fbadasd: invalid reserved bytes %2.2X %2.2X %2.2X\n"),
//                  iobuf[1], iobuf[2], iobuf[3]);
//          dev->sense[0] = SENSE_CR;
//          *unitstat = CSW_CE | CSW_DE | CSW_UC;
//          break;
//      }

        /* Bytes 4-7 contain the block number of the first block
           of the extent relative to the start of the device */
        dev->fbaxblkn = (iobuf[4] << 24) | (iobuf[5] << 16)
                        | (iobuf[6] << 8) | iobuf[7];

        /* Bytes 8-11 contain the block number of the first block
           of the extent relative to the start of the dataset */
        dev->fbaxfirst = (iobuf[8] << 24) | (iobuf[9] << 16)
                        | (iobuf[10] << 8) | iobuf[11];

        /* Bytes 12-15 contain the block number of the last block
           of the extent relative to the start of the dataset */
        dev->fbaxlast = (iobuf[12] << 24) | (iobuf[13] << 16)
                        | (iobuf[14] << 8) | iobuf[15];

        /* Validate the extent description by checking that the
           ending block is not less than the starting block and
           that the ending block does not exceed the device size */
        if (dev->fbaxlast < dev->fbaxfirst
            || dev->fbaxblkn > dev->fbanumblk
            || dev->fbaxlast - dev->fbaxfirst
                >= dev->fbanumblk - dev->fbaxblkn)
        {
            devmsg(_("fbadasd: invalid extent: first block %d, last block %d,\n"),
                    dev->fbaxfirst, dev->fbaxlast);
            devmsg(_("         numblks %d, device size %d\n"),
                    dev->fbaxblkn, dev->fbanumblk);
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Set extent defined flag and return normal status */
        dev->fbaxtdef = 1;
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x64:
    /*---------------------------------------------------------------*/
    /* READ DEVICE CHARACTERISTICS                                   */
    /*---------------------------------------------------------------*/
        /* Calculate residual byte count */
        num = (count < dev->numdevchar) ? count : dev->numdevchar;
        *residual = count - num;
        if (count < dev->numdevchar) *more = 1;

        /* Copy device characteristics bytes to channel buffer */
        memcpy (iobuf, dev->devchar, num);

        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x94:
    /*---------------------------------------------------------------*/
    /* DEVICE RELEASE                                                */
    /*---------------------------------------------------------------*/
        /* Reject if extent previously defined in this CCW chain */
        if (dev->fbaxtdef)
        {
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Return sense information */
        goto sense;

    case 0xB4:
    /*---------------------------------------------------------------*/
    /* DEVICE RESERVE                                                */
    /*---------------------------------------------------------------*/
        /* Reject if extent previously defined in this CCW chain */
        if (dev->fbaxtdef)
        {
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Return sense information */
        goto sense;

    case 0x14:
    /*---------------------------------------------------------------*/
    /* UNCONDITIONAL RESERVE                                         */
    /*---------------------------------------------------------------*/
        /* Reject if this is not the first CCW in the chain */
        if (ccwseq > 0)
        {
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Return sense information */
        goto sense;

    case 0x04:
    /*---------------------------------------------------------------*/
    /* SENSE                                                         */
    /*---------------------------------------------------------------*/
    sense:
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

    case 0xA4:
    /*---------------------------------------------------------------*/
    /* READ AND RESET BUFFERED LOG                                   */
    /*---------------------------------------------------------------*/
        /* Calculate residual byte count */
        num = (count < 24) ? count : 24;
        *residual = count - num;
        if (count < 24) *more = 1;

        /* Copy device identifier bytes to channel I/O buffer */
        memset (iobuf, 0, num);

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

} /* end function fbadasd_execute_ccw */

/*-------------------------------------------------------------------*/
/* Synchronous Fixed Block I/O (used by Diagnose instruction)        */
/*-------------------------------------------------------------------*/
void fbadasd_syncblk_io ( DEVBLK *dev, BYTE type, U32 blknum,
        U32 blksize, BYTE *iobuf, BYTE *unitstat, U16 *residual )
{
int     rc;                             /* Return code               */
int     blkfactor;                      /* Number of device blocks
                                           per logical block         */

    /* Calculate the blocking factor */
    blkfactor = blksize / dev->fbablksiz;

    /* Unit check if block number is invalid */
    if (blknum * blkfactor >= dev->fbanumblk)
    {
        dev->sense[0] = SENSE_CR;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return;
    }

    /* Seek to start of desired block */
    dev->fbarba = dev->fbaorigin * dev->fbablksiz;
    dev->fbarba += blknum * blksize;

    /* Process depending on operation type */
    switch (type) {

    case 0x01:
        /* Write block from I/O buffer */
        rc = (dev->fbawrblk) (dev, iobuf, blksize, unitstat);
        if (rc < blksize) return;
        break;

    case 0x02:
        /* Read block into I/O buffer */
        rc = (dev->fbardblk) (dev, iobuf, blksize, unitstat);
        if (rc < blksize) return;
        break;

    } /* end switch(type) */

    /* Return unit status and residual byte count */
    *unitstat = CSW_CE | CSW_DE;
    *residual = 0;

} /* end function fbadasd_syncblk_io */


DEVHND fbadasd_device_hndinfo = {
        &fbadasd_init_handler,
        &fbadasd_execute_ccw,
        &fbadasd_close_device,
        &fbadasd_query_device,
        NULL, NULL, NULL, NULL
};

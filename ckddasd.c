/* CKDDASD.C    (c) Copyright Roger Bowler, 1999-2004                */
/*              ESA/390 CKD Direct Access Storage Device Handler     */

/*-------------------------------------------------------------------*/
/* This module contains device handling functions for emulated       */
/* count-key-data direct access storage devices.                     */
/*-------------------------------------------------------------------*/

/*-------------------------------------------------------------------*/
/* Additional credits:                                               */
/*      Write Update Key and Data CCW by Jan Jaeger                  */
/*      Track overflow support added by Jay Maynard                  */
/*      Track overflow fixes by Jay Maynard, suggested by Valery     */
/*        Pogonchenko                                                */
/*      Track overflow write fix by Roger Bowler, thanks to Valery   */
/*        Pogonchenko and Volker Bandke             V1.71 16/01/2001 */
/*-------------------------------------------------------------------*/

#include "hercules.h"
#include "devtype.h"

/*-------------------------------------------------------------------*/
/* Bit definitions for File Mask                                     */
/*-------------------------------------------------------------------*/
#define CKDMASK_WRCTL           0xC0    /* Write control bits...     */
#define CKDMASK_WRCTL_INHWR0    0x00    /* ...inhibit write HA/R0    */
#define CKDMASK_WRCTL_INHWRT    0x40    /* ...inhibit all writes     */
#define CKDMASK_WRCTL_ALLWRU    0x80    /* ...write update only      */
#define CKDMASK_WRCTL_ALLWRT    0xC0    /* ...allow all writes       */
#define CKDMASK_RESV            0x20    /* Reserved bits - must be 0 */
#define CKDMASK_SKCTL           0x18    /* Seek control bits...      */
#define CKDMASK_SKCTL_ALLSKR    0x00    /* ...allow all seek/recalib */
#define CKDMASK_SKCTL_CYLHD     0x08    /* ...allow seek cyl/head    */
#define CKDMASK_SKCTL_HEAD      0x10    /* ...allow seek head only   */
#define CKDMASK_SKCTL_INHSMT    0x18    /* ...inhibit seek and MT    */
#define CKDMASK_AAUTH           0x06    /* Access auth bits...       */
#define CKDMASK_AAUTH_NORMAL    0x00    /* ...normal authorization   */
#define CKDMASK_AAUTH_DSF       0x02    /* ...device support auth    */
#define CKDMASK_AAUTH_DIAG      0x04    /* ...diagnostic auth        */
#define CKDMASK_AAUTH_DSFNCR    0x06    /* ...device support with no
                                              correction or retry    */
#define CKDMASK_PCI_FETCH       0x01    /* PCI fetch mode            */

/*-------------------------------------------------------------------*/
/* Bit definitions for Define Extent global attributes byte          */
/*-------------------------------------------------------------------*/
#define CKDGATR_ARCH            0xC0    /* Architecture mode...      */
#define CKDGATR_ARCH_ECKD       0xC0    /* ...extended CKD mode      */
#define CKDGATR_CKDCONV         0x20    /* CKD conversion mode       */
#define CKDGATR_SSOP            0x1C    /* Subsystem operation mode..*/
#define CKDGATR_SSOP_NORMAL     0x00    /* ...normal cache           */
#define CKDGATR_SSOP_BYPASS     0x04    /* ...bypass cache           */
#define CKDGATR_SSOP_INHIBIT    0x08    /* ...inhibit cache loading  */
#define CKDGATR_SSOP_SEQACC     0x0C    /* ...sequential access      */
#define CKDGATR_SSOP_LOGGING    0x10    /* ...logging mode           */
#define CKDGATR_USE_CACHE_FW    0x02    /* Use cache fast write      */
#define CKDGATR_INH_DASD_FW     0x01    /* Inhibit DASD fast write   */

/*-------------------------------------------------------------------*/
/* Bit definitions for Locate operation byte                         */
/*-------------------------------------------------------------------*/
#define CKDOPER_ORIENTATION     0xC0    /* Orientation bits...       */
#define CKDOPER_ORIENT_COUNT    0x00    /* ...orient to count        */
#define CKDOPER_ORIENT_HOME     0x40    /* ...orient to home address */
#define CKDOPER_ORIENT_DATA     0x80    /* ...orient to data area    */
#define CKDOPER_ORIENT_INDEX    0xC0    /* ...orient to index        */
#define CKDOPER_CODE            0x3F    /* Operation code bits...    */
#define CKDOPER_ORIENT          0x00    /* ...orient                 */
#define CKDOPER_WRITE           0x01    /* ...write data             */
#define CKDOPER_FORMAT          0x03    /* ...format write           */
#define CKDOPER_RDDATA          0x06    /* ...read data              */
#define CKDOPER_RDANY           0x0A    /* ...read any               */
#define CKDOPER_WRTTRK          0x0B    /* ...write track            */
#define CKDOPER_RDTRKS          0x0C    /* ...read tracks            */
#define CKDOPER_READ            0x16    /* ...read                   */

/*-------------------------------------------------------------------*/
/* Bit definitions for Locate auxiliary byte                         */
/*-------------------------------------------------------------------*/
#define CKDLAUX_TLFVALID        0x80    /* TLF field is valid        */
#define CKDLAUX_RESV            0x7E    /* Reserved bits - must be 0 */
#define CKDLAUX_RDCNTSUF        0x01    /* Suffixed read count CCW   */

/*-------------------------------------------------------------------*/
/* Definitions for ckdorient field in device block                   */
/*-------------------------------------------------------------------*/
#define CKDORIENT_NONE          0       /* Orientation unknown       */
#define CKDORIENT_INDEX         1       /* Oriented after track hdr  */
#define CKDORIENT_COUNT         2       /* Oriented after count field*/
#define CKDORIENT_KEY           3       /* Oriented after key field  */
#define CKDORIENT_DATA          4       /* Oriented after data field */
#define CKDORIENT_EOT           5       /* Oriented after end of trk */

/* Path state byte for Sense Path Group ID command */
#define SPG_PATHSTAT            0xC0    /* Pathing status bits...    */
#define SPG_PATHSTAT_RESET      0x00    /* ...reset                  */
#define SPG_PATHSTAT_RESV       0x40    /* ...reserved bit setting   */
#define SPG_PATHSTAT_UNGROUPED  0x80    /* ...ungrouped              */
#define SPG_PATHSTAT_GROUPED    0xC0    /* ...grouped                */
#define SPG_PARTSTAT            0x30    /* Partitioning status bits..*/
#define SPG_PARTSTAT_IENABLED   0x00    /* ...implicitly enabled     */
#define SPG_PARTSTAT_RESV       0x10    /* ...reserved bit setting   */
#define SPG_PARTSTAT_DISABLED   0x20    /* ...disabled               */
#define SPG_PARTSTAT_XENABLED   0x30    /* ...explicitly enabled     */
#define SPG_PATHMODE            0x08    /* Path mode bit...          */
#define SPG_PATHMODE_SINGLE     0x00    /* ...single path mode       */
#define SPG_PATHMODE_RESV       0x08    /* ...reserved bit setting   */
#define SPG_RESERVED            0x07    /* Reserved bits, must be 0  */

/* Function control byte for Set Path Group ID command */
#define SPG_SET_MULTIPATH       0x80    /* Set multipath mode        */
#define SPG_SET_COMMAND         0x60    /* Set path command bits...  */
#define SPG_SET_ESTABLISH       0x00    /* ...establish group        */
#define SPG_SET_DISBAND         0x20    /* ...disband group          */
#define SPG_SET_RESIGN          0x40    /* ...resign from group      */
#define SPG_SET_COMMAND_RESV    0x60    /* ...reserved bit setting   */
#define SPG_SET_RESV            0x1F    /* Reserved bits, must be 0  */

/*-------------------------------------------------------------------*/
/* Bit definitions for Diagnostic Control subcommand byte            */
/*-------------------------------------------------------------------*/
#define DIAGCTL_INHIBIT_WRITE   0x02    /* Inhibit Write             */
#define DIAGCTL_SET_GUAR_PATH   0x04    /* Set Guaranteed Path       */
#define DIAGCTL_ENABLE_WRITE    0x08    /* Enable Write              */
#define DIAGCTL_3380_TC_MODE    0x09    /* 3380 Track Compat Mode    */
#define DIAGCTL_INIT_SUBSYS     0x0B    /* Diagnostic Init Subsys    */
#define DIAGCTL_UNFENCE         0x0C    /* Unfence                   */
#define DIAGCTL_ACCDEV_UNKCOND  0x0F    /* Access Device Unknown Cond*/
#define DIAGCTL_MAINT_RESERVE   0x10    /* Media Maintenance Reserve */
#define DIAGCTL_MAINT_RELEASE   0x11    /* Media Maintenance Release */
#define DIAGCTL_MAINT_QUERY     0x12    /* Media Maintenance Query   */

/*-------------------------------------------------------------------*/
/* Definitions for sense data format codes and message codes         */
/*-------------------------------------------------------------------*/
#define FORMAT_0                0       /* Program or System Checks  */
#define FORMAT_1                1       /* Device Equipment Checks   */
#define FORMAT_2                2       /* 3990 Equipment Checks     */
#define FORMAT_3                3       /* 3990 Control Checks       */
#define FORMAT_4                4       /* Data Checks               */
#define FORMAT_5                5       /* Data Check + Displacement */
#define FORMAT_6                6       /* Usage Stats/Overrun Errors*/
#define FORMAT_7                7       /* Device Control Checks     */
#define FORMAT_8                8       /* Device Equipment Checks   */
#define FORMAT_9                9       /* Device Rd/Wrt/Seek Checks */
#define FORMAT_F                15      /* Cache Storage Checks      */
#define MESSAGE_0               0       /* Message 0                 */
#define MESSAGE_1               1       /* Message 1                 */
#define MESSAGE_2               2       /* Message 2                 */
#define MESSAGE_3               3       /* Message 3                 */
#define MESSAGE_4               4       /* Message 4                 */
#define MESSAGE_5               5       /* Message 5                 */
#define MESSAGE_6               6       /* Message 6                 */
#define MESSAGE_7               7       /* Message 7                 */
#define MESSAGE_8               8       /* Message 8                 */
#define MESSAGE_9               9       /* Message 9                 */
#define MESSAGE_A               10      /* Message A                 */
#define MESSAGE_B               11      /* Message B                 */
#define MESSAGE_C               12      /* Message C                 */
#define MESSAGE_D               13      /* Message D                 */
#define MESSAGE_E               14      /* Message E                 */
#define MESSAGE_F               15      /* Message F                 */

/*-------------------------------------------------------------------*/
/* Definitions for Read Configuration Data command                   */
/*-------------------------------------------------------------------*/
#define CONFIG_DATA_SIZE        256     /* Number of bytes returned
                                           by Read Config Data CCW   */

/*-------------------------------------------------------------------*/
/* Static data areas                                                 */
/*-------------------------------------------------------------------*/
static  BYTE eighthexFF[] = {0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff};

/*-------------------------------------------------------------------*/
/* Initialize the device handler                                     */
/*-------------------------------------------------------------------*/
int ckddasd_init_handler ( DEVBLK *dev, int argc, BYTE *argv[] )
{
int             rc;                     /* Return code               */
struct stat     statbuf;                /* File information          */
CKDDASD_DEVHDR  devhdr;                 /* Device header             */
CCKDDASD_DEVHDR cdevhdr;                /* Compressed device header  */
int             i;                      /* Loop index                */
int             fileseq;                /* File sequence number      */
BYTE           *sfxptr;                 /* -> Last char of file name */
BYTE            sfxchar;                /* Last char of file name    */
int             heads;                  /* #of heads in CKD file     */
int             trksize;                /* Track size of CKD file    */
int             trks;                   /* #of tracks in CKD file    */
int             cyls;                   /* #of cylinders in CKD file */
int             highcyl;                /* Highest cyl# in CKD file  */
BYTE           *cu = NULL;              /* Specified control unit    */
char           *kw;                     /* Argument keyword          */
int             cckd=0;                 /* 1 if compressed CKD       */

    if(!sscanf(dev->typname,"%hx",&(dev->devtype)))
        dev->devtype = 0x3380;

    /* The first argument is the file name */
    if (argc == 0 || strlen(argv[0]) > sizeof(dev->filename)-1)
    {
        logmsg (_("HHCDA001E File name missing or invalid\n"));
        return -1;
    }

    /* Save the file name in the device block */
    strcpy (dev->filename, argv[0]);

    /* Device is shareable */
    dev->shared = 1;

    /* Check for possible remote device */
    if (stat(dev->filename, &statbuf) < 0)
    {
        rc = shared_ckd_init ( dev, argc, argv);
        if (rc < 0)
        {
            logmsg (_("HHCDA002E %4.4X:File not found or invalid\n"),
                    dev->devnum);
            return -1;
        }
        else
            return rc;
    }

    /* Default to synchronous I/O */
    dev->syncio = 1;

    /* No active track or cache entry */
    dev->bufcur = dev->cache = -1;

    /* Locate and save the last character of the file name */
    sfxptr = strrchr (dev->filename, '/');
    if (sfxptr == NULL) sfxptr = dev->filename + 1;
    sfxptr = strchr (sfxptr, '.');
    if (sfxptr == NULL) sfxptr = dev->filename + strlen(dev->filename);
    sfxptr--;
    sfxchar = *sfxptr;

    /* process the remaining arguments */
    for (i = 1; i < argc; i++)
    {
        if (strcasecmp ("lazywrite", argv[i]) == 0)
        {
            dev->ckdnolazywr = 0;
            continue;
        }
        if (strcasecmp ("nolazywrite", argv[i]) == 0)
        {
            dev->ckdnolazywr = 1;
            continue;
        }
        if (strcasecmp ("fulltrackio", argv[i]) == 0 ||
            strcasecmp ("fulltrkio",   argv[i]) == 0 ||
            strcasecmp ("ftio",        argv[i]) == 0)
        {
            dev->ckdnolazywr = 0;
            continue;
        }
        if (strcasecmp ("nofulltrackio", argv[i]) == 0 ||
            strcasecmp ("nofulltrkio",   argv[i]) == 0 ||
            strcasecmp ("noftio",        argv[i]) == 0)
        {
            dev->ckdnolazywr = 1;
            continue;
        }
        if (strcasecmp ("readonly", argv[i]) == 0 ||
            strcasecmp ("rdonly",   argv[i]) == 0 ||
            strcasecmp ("ro",       argv[i]) == 0)
        {
            dev->ckdrdonly = 1;
            continue;
        }
        if (strcasecmp ("fakewrite", argv[i]) == 0 ||
            strcasecmp ("fakewrt",   argv[i]) == 0 ||
            strcasecmp ("fw",        argv[i]) == 0)
        {
            dev->ckdfakewr = 1;
            continue;
        }
        if (strlen (argv[i]) > 3 &&
            memcmp ("sf=", argv[i], 3) == 0)
        {
            if ('\"' == argv[i][3]) argv[i]++;
            if (strlen(argv[i]+3) < 256)
                strcpy (dev->dasdsfn, argv[i]+3);
            continue;
        }
        if (strlen (argv[i]) > 3
         && memcmp("cu=", argv[i], 3) == 0)
        {
            kw = strtok (argv[i], "=");
            cu = strtok (NULL, " \t");
            continue;
        }
        if (strcasecmp ("nosyncio", argv[i]) == 0 ||
            strcasecmp ("nosyio",   argv[i]) == 0)
        {
            dev->syncio = 0;
            continue;
        }
        if (strcasecmp ("syncio", argv[i]) == 0 ||
            strcasecmp ("syio",   argv[i]) == 0)
        {
            dev->syncio = 1;
            continue;
        }

        logmsg (_("HHCDA003E parameter %d is invalid: %s\n"),
                i + 1, argv[i]);
        return -1;
    }

    /* Initialize the total tracks and cylinders */
    dev->ckdtrks = 0;
    dev->ckdcyls = 0;

    /* Open all of the CKD image files which comprise this volume */
    if (dev->ckdrdonly)
        logmsg (_("HHCDA004I opening %s readonly%s\n"), dev->filename,
                dev->ckdfakewr ? " with fake writing" : "");
    for (fileseq = 1;;)
    {
        /* Open the CKD image file */
        dev->fd = open (dev->filename, dev->ckdrdonly ?
                        O_RDONLY|O_BINARY : O_RDWR|O_BINARY);
        if (dev->fd < 0)
        {   /* Try read-only if shadow file present */
            if (!dev->ckdrdonly && dev->dasdsfn[0] != '\0')
                dev->fd = open (dev->filename, O_RDONLY|O_BINARY);
            if (dev->fd < 0)
            {
                logmsg (_("HHCDA005E %s open error: %s\n"),
                        dev->filename, strerror(errno));
                return -1;
            }
        }

        /* If `readonly' and shadow files (`sf=') were specified,
           then turn off the readonly bit.  Might as well make
           sure the `fakewrite' bit is off, too.               */
        if (dev->dasdsfn[0] != '\0' && !dev->batch)
            dev->ckdrdonly = dev->ckdfakewr = 0;

        /* If shadow file, only one base file is allowed */
        if (fileseq > 1 && dev->dasdsfn[0] != '\0')
        {
            logmsg (_("HHCDA006E %s not in a single file for shadowing\n"),
                    dev->filename);
            return -1;
        }

        /* Determine the device size */
        rc = fstat (dev->fd, &statbuf);
        if (rc < 0)
        {
            logmsg (_("HHCDA007E %s fstat error: %s\n"),
                    dev->filename, strerror(errno));
            return -1;
        }

        /* Read the device header */
        rc = read (dev->fd, &devhdr, CKDDASD_DEVHDR_SIZE);
        if (rc < (int)CKDDASD_DEVHDR_SIZE)
        {
            if (rc < 0)
                logmsg (_("HHCDA008E %s read error: %s\n"),
                        dev->filename, strerror(errno));
            else
                logmsg (_("HHCDA09E %s CKD header incomplete\n"),
                        dev->filename);
            return -1;
        }

        /* Check the device header identifier */
        if (memcmp(devhdr.devid, "CKD_P370", 8) != 0)
        {
            if (memcmp(devhdr.devid, "CKD_C370", 8) != 0)
            {
                logmsg (_("HHCDA010E %s CKD header invalid\n"),
                        dev->filename);
                return -1;
            }
            else
            {
                cckd = 1;
                if (fileseq != 1)
                {
                    logmsg (_("HHCDA011E %s Only 1 CCKD file allowed\n"),
                            dev->filename);
                    return -1;
                }
            }
        }

        /* Read the compressed device header */
        if ( cckd )
        {
            rc = read (dev->fd, &cdevhdr, CCKDDASD_DEVHDR_SIZE);
            if (rc < (int)CCKDDASD_DEVHDR_SIZE)
            {
                if (rc < 0)
                {
                    logmsg (_("HHCDA012E %s read error: %s\n"),
                            dev->filename, strerror(errno));
                }
                else
                {
                    logmsg (_("HHCDA013E %s CCKD header incomplete\n"),
                            dev->filename);
                }
                return -1;
            }
        }

        /* Check for correct file sequence number */
        if (devhdr.fileseq != fileseq
            && !(devhdr.fileseq == 0 && fileseq == 1))
        {
            logmsg (_("HHCDA014E %s CKD file out of sequence\n"),
                    dev->filename);
            return -1;
        }

        /* Extract fields from device header */
        heads   = ((U32)(devhdr.heads[3]) << 24)
                | ((U32)(devhdr.heads[2]) << 16)
                | ((U32)(devhdr.heads[1]) << 8)
                | (U32)(devhdr.heads[0]);
        trksize = ((U32)(devhdr.trksize[3]) << 24)
                | ((U32)(devhdr.trksize[2]) << 16)
                | ((U32)(devhdr.trksize[1]) << 8)
                | (U32)(devhdr.trksize[0]);
        highcyl = ((U32)(devhdr.highcyl[1]) << 8)
                | (U32)(devhdr.highcyl[0]);
        if (cckd == 0)
        {
            trks = (statbuf.st_size - CKDDASD_DEVHDR_SIZE) / trksize;
            cyls = trks / heads;
        }
        else
        {
            cyls = ((U32)(cdevhdr.cyls[3]) << 24)
                 | ((U32)(cdevhdr.cyls[2]) << 16)
                 | ((U32)(cdevhdr.cyls[1]) << 8)
                 |  (U32)(cdevhdr.cyls[0]);
            trks = cyls * heads;
        }

        if (devhdr.fileseq > 0)
        {
            logmsg (_("HHCDA015I %s seq=%d cyls=%d-%d\n"),
                    dev->filename, devhdr.fileseq, dev->ckdcyls,
                    (highcyl > 0 ? highcyl : dev->ckdcyls + cyls - 1));
        }

        /* Save device geometry of first file, or check that device
           geometry of subsequent files matches that of first file */
        if (fileseq == 1)
        {
            dev->ckdheads = heads;
            dev->ckdtrksz = trksize;
        }
        else if (heads != dev->ckdheads || trksize != dev->ckdtrksz)
        {
            logmsg (_("HHCDA016E %s heads=%d trklen=%d, "
                    "expected heads=%d trklen=%d\n"),
                    dev->filename, heads, trksize,
                    dev->ckdheads, dev->ckdtrksz);
            return -1;
        }

        /* Consistency check device header */
        if (cckd == 0 && (cyls * heads != trks
            || ((off_t)trks * trksize) + CKDDASD_DEVHDR_SIZE
                            != statbuf.st_size
            || (highcyl != 0 && highcyl != dev->ckdcyls + cyls - 1)))
        {
            logmsg (_("HHCDA017E %s CKD header inconsistent with file size\n"),
                    dev->filename);
            return -1;
        }

        /* Check for correct high cylinder number */
        if (highcyl != 0 && highcyl != dev->ckdcyls + cyls - 1)
        {
            logmsg (_("HHCDA018E %s CKD header high cylinder incorrect\n"),
                    dev->filename);
            return -1;
        }

        /* Accumulate total volume size */
        dev->ckdtrks += trks;
        dev->ckdcyls += cyls;

        /* Save file descriptor and high track number */
        dev->ckdfd[fileseq-1] = dev->fd;
        dev->ckdhitrk[fileseq-1] = dev->ckdtrks;
        dev->ckdnumfd = fileseq;

        /* Exit loop if this is the last file */
        if (highcyl == 0) break;

        /* Increment the file sequence number */
        fileseq++;

        /* Alter the file name suffix ready for the next file */
        *sfxptr = '0' + fileseq;

        /* Check that maximum files has not been exceeded */
        if (fileseq > CKD_MAXFILES)
        {
            logmsg (_("HHCDA019E %s exceeds maximum %d CKD files\n"),
                    dev->filename, CKD_MAXFILES);
            return -1;
        }

    } /* end for(fileseq) */

    /* Restore the last character of the file name */
    *sfxptr = sfxchar;

    /* Log the device geometry */
    logmsg (_("HHCDA020I %s cyls=%d heads=%d tracks=%d trklen=%d\n"),
            dev->filename, dev->ckdcyls,
            dev->ckdheads, dev->ckdtrks, dev->ckdtrksz);

    /* Set number of sense bytes */
    dev->numsense = 32;

    /* Locate the CKD dasd table entry */
    dev->ckdtab = dasd_lookup (DASD_CKDDEV, NULL, dev->devtype, dev->ckdcyls);
    if (dev->ckdtab == NULL)
    {
        logmsg (_("HHCDA021E %4.4X device type %4.4X not found in dasd table\n"),
                dev->devnum, dev->devtype);
        return -1;
    }

    /* Locate the CKD control unit dasd table entry */
    dev->ckdcu = dasd_lookup (DASD_CKDCU, cu ? cu : dev->ckdtab->cu, 0, 0);
    if (dev->ckdcu == NULL)
    {
        logmsg (_("HHCDA022E %4.4X control unit %s not found in dasd table\n"),
                dev->devnum, cu ? cu : dev->ckdtab->cu);
        return -1;
    }

    /* Set flag bit if 3990 controller */
    if (dev->ckdcu->devt == 0x3990)
        dev->ckd3990 = 1;

    /* Build the devid area */
    dev->numdevid = dasd_build_ckd_devid (dev->ckdtab, dev->ckdcu,
                                          (BYTE *)&dev->devid);

    /* Build the devchar area */
    dev->numdevchar = dasd_build_ckd_devchar (dev->ckdtab, dev->ckdcu,
                                  (BYTE *)&dev->devchar, dev->ckdcyls);

    /* Clear the DPA */
    memset(dev->pgid, 0, sizeof(dev->pgid));

    /* Activate I/O tracing */
//  dev->ccwtrace = 1;

    /* Request the channel to merge data chained write CCWs into
       a single buffer before passing data to the device handler */
    dev->cdwmerge = 1;

    if (!cckd) return 0;
    else return cckddasd_init_handler(dev, argc, argv);

} /* end function ckddasd_init_handler */


/*-------------------------------------------------------------------*/
/* Query the device definition                                       */
/*-------------------------------------------------------------------*/
void ckddasd_query_device (DEVBLK *dev, BYTE **class,
                int buflen, BYTE *buffer)
{

    *class = "DASD";
    snprintf (buffer, buflen, "%s [%d cyls]",
            dev->filename,
            dev->ckdcyls);

} /* end function ckddasd_query_device */

/*-------------------------------------------------------------------*/
/* Release cache entries                                             */
/*-------------------------------------------------------------------*/
int ckddasd_purge_cache (int *answer, int ix, int i, void *data)
{
U16             devnum;                 /* Cached device number      */
int             trk;                    /* Cached track              */
DEVBLK         *dev = data;             /* -> device block           */

    UNREFERENCED(answer);
    CKD_CACHE_GETKEY(i, devnum, trk);
    if (dev->devnum == devnum)
        cache_release (ix, i, CACHE_FREEBUF);
    return 0;
}


static int ckddasd_read_track (DEVBLK *dev, int trk, BYTE *unitstat);
/*-------------------------------------------------------------------*/
/* Close the device                                                  */
/*-------------------------------------------------------------------*/
int ckddasd_close_device ( DEVBLK *dev )
{
int     i;                              /* Index                     */
BYTE    unitstat;                       /* Unit Status               */

    /* Write the last track image if it's modified */
    ckddasd_read_track (dev, -1, &unitstat);

    /* Free the cache */
    cache_lock(CACHE_DEVBUF);
    cache_scan(CACHE_DEVBUF, ckddasd_purge_cache, dev);
    cache_unlock(CACHE_DEVBUF);

    if (!dev->batch)
        logmsg (_("HHCDA023I %4.4X cache hits %d, misses %d, waits %d\n"),
                dev->devnum, dev->cachehits, dev->cachemisses,
                dev->cachewaits);

    /* Close all of the CKD image files */
    for (i = 0; i < dev->ckdnumfd; i++)
        if (dev->ckdfd[i] > 2)
            close (dev->ckdfd[i]);

    dev->buf = NULL;
    dev->bufsize = 0;

    return 0;
} /* end function ckddasd_close_device */


/*-------------------------------------------------------------------*/
/* Read a track image at CCHH                                        */
/*-------------------------------------------------------------------*/
static
int ckd_read_cchh (DEVBLK *dev, int cyl, int head, BYTE *unitstat)
{
int             rc;                     /* Return code               */
int             trk;                    /* Track number              */

    /* Command reject if seek position is outside volume */
    if (cyl >= dev->ckdcyls || head >= dev->ckdheads)
    {
        ckd_build_sense (dev, SENSE_CR, 0, 0,
                        FORMAT_0, MESSAGE_4);
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return -1;
    }

    /* Calculate the track number */
    trk = cyl * dev->ckdheads + head;

    /* Call the read exit */
    rc = (dev->hnd->read) (dev, trk, unitstat);

    return rc;
} /* end function ckd_read_cchh */

/*-------------------------------------------------------------------*/
/* Return track image length                                         */
/*-------------------------------------------------------------------*/
static int ckd_trklen (DEVBLK *dev, BYTE *buf)
{
int             sz;                     /* Size so far               */

    for (sz = CKDDASD_TRKHDR_SIZE;
         memcmp (buf + sz, &eighthexFF, 8) != 0; )
    {
        /* add length of count, key, and data fields */
        sz += CKDDASD_RECHDR_SIZE +
                buf[sz+5] +
                (buf[sz+6] << 8) + buf[sz+7];
        if (sz > dev->ckdtrksz - 8) break;
    }

    /* add length for end-of-track indicator */
    sz += CKDDASD_RECHDR_SIZE;

    if (sz > dev->ckdtrksz)
        sz = dev->ckdtrksz;

    return sz;
}

/*-------------------------------------------------------------------*/
/* Read a track image                                                */
/*-------------------------------------------------------------------*/
static
int ckddasd_read_track (DEVBLK *dev, int trk, BYTE *unitstat)
{
int             rc;                     /* Return code               */
int             cyl;                    /* Cylinder                  */
int             head;                   /* Head                      */
off_t           offset;                 /* File offsets              */
int             i,o,f;                  /* Indexes                   */
int             active;                 /* 1=Synchronous I/O active  */
CKDDASD_TRKHDR *trkhdr;                 /* -> New track header       */

    DEVTRACE (_("HHCDA024I read trk %d cur trk %d\n"), trk, dev->bufcur);

    /* Calculate cylinder and head */
    cyl = trk / dev->ckdheads;
    head = trk % dev->ckdheads;

    /* Reset buffer offsets */
    dev->bufoff = 0;
    dev->bufoffhi = dev->ckdtrksz;

    /* Return if reading the same track image */
    if (trk >= 0 && trk == dev->bufcur)
        return 0;

    /* Turn off the synchronous I/O bit if trk overflow or trk 0 */
    active = dev->syncio_active;
    if (dev->ckdtrkof || trk <= 0)
        dev->syncio_active = 0;

    /* Write the previous track image if modified */
    if (dev->bufupd)
    {
        /* Retry if synchronous I/O */
        if (dev->syncio_active)
        {
            dev->syncio_retry = 1;
            return -1;
        }

        DEVTRACE (_("HHCDA025I read track: updating track %d\n"),
                  dev->bufcur);

        dev->bufupd = 0;

        /* Seek to the old track image offset */
        offset = (off_t)(dev->ckdtrkoff + dev->bufupdlo);
        offset = lseek (dev->fd, offset, SEEK_SET);
        if (offset < 0)
        {
            /* Handle seek error condition */
            logmsg (_("HHCDA026E error writing trk %d: lseek error: %s\n"),
                    dev->bufcur, strerror(errno));
            ckd_build_sense (dev, SENSE_EC, 0, 0,
                            FORMAT_1, MESSAGE_0);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            cache_lock(CACHE_DEVBUF);
            cache_setflag(CACHE_DEVBUF, dev->cache, ~CKD_CACHE_ACTIVE, 0);
            cache_unlock(CACHE_DEVBUF);
            dev->bufupdlo = dev->bufupdhi = 0;
            dev->bufcur = dev->cache = -1;
            return -1;
        }

        /* Write the portion of the track image that was modified */
        rc = write (dev->fd, &dev->buf[dev->bufupdlo],
                    dev->bufupdhi - dev->bufupdlo);
        if (rc < dev->bufupdhi - dev->bufupdlo)
        {
            /* Handle seek error condition */
            logmsg (_("HHCDA027E error writing trk %d: write error: %s\n"),
                    dev->bufcur, strerror(errno));
            ckd_build_sense (dev, SENSE_EC, 0, 0,
                            FORMAT_1, MESSAGE_0);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            cache_lock(CACHE_DEVBUF);
            cache_setflag(CACHE_DEVBUF, dev->cache, ~CKD_CACHE_ACTIVE, 0);
            cache_unlock(CACHE_DEVBUF);
            dev->bufupdlo = dev->bufupdhi = 0;
            dev->bufcur = dev->cache = -1;
            return -1;
        }

        dev->bufupdlo = dev->bufupdhi = 0;
    }

    cache_lock (CACHE_DEVBUF);

    /* Make the previous cache entry inactive */
    if (dev->cache >= 0)
        cache_setflag(CACHE_DEVBUF, dev->cache, ~CKD_CACHE_ACTIVE, 0);
    dev->bufcur = dev->cache = -1;

    /* Return on special case when called by the close handler */
    if (trk < 0)
    {
        cache_unlock (CACHE_DEVBUF);
        return 0;
    }

ckd_read_track_retry:

    /* Search the cache */
    i = cache_lookup (CACHE_DEVBUF, CKD_CACHE_SETKEY(dev->devnum, trk), &o);

    /* Cache hit */
    if (i >= 0)
    {
        cache_setflag(CACHE_DEVBUF, i, ~0, CKD_CACHE_ACTIVE);
        cache_setage(CACHE_DEVBUF, i);
        cache_unlock(CACHE_DEVBUF);

        DEVTRACE (_("HHCDA028I read trk %d cache hit, using cache[%d]\n"),
                  trk, i);

        dev->cachehits++;
        dev->cache = i;
        dev->buf = cache_getbuf(CACHE_DEVBUF, dev->cache, 0);
        dev->bufcur = trk;
        dev->bufoff = 0;
        dev->bufoffhi = dev->ckdtrksz; 
        dev->buflen = ckd_trklen (dev, dev->buf);
        dev->bufsize = cache_getlen(CACHE_DEVBUF, dev->cache);

        /* Set the file descriptor */
        for (f = 0; f < dev->ckdnumfd; f++)
            if (trk < dev->ckdhitrk[f]) break;
        dev->fd = dev->ckdfd[f];

        /* Calculate the track offset */
        dev->ckdtrkoff = CKDDASD_DEVHDR_SIZE +
             (off_t)(trk - (f ? dev->ckdhitrk[f-1] : 0)) * dev->ckdtrksz;

        dev->syncio_active = active;

        return 0;
     }

    /* Retry if synchronous I/O */
    if (dev->syncio_active)
    {
        cache_unlock(CACHE_DEVBUF);
        dev->syncio_retry = 1;
        return -1;
    }

    /* Wait if no available cache entry */
    if (o < 0)
    {
        DEVTRACE (_("HHCDA029I read trk %d no available cache entry, waiting\n"),
                  trk); 
        dev->cachewaits++;
        cache_wait(CACHE_DEVBUF);
        goto ckd_read_track_retry;
    }

    /* Cache miss */
    DEVTRACE (_("HHCDA030I read trk %d cache miss, using cache[%d]\n"),
              trk, o);

    dev->cachemisses++;

    /* Make this cache entry active */
    cache_setkey (CACHE_DEVBUF, o, CKD_CACHE_SETKEY(dev->devnum, trk));
    cache_setflag(CACHE_DEVBUF, o, 0, CKD_CACHE_ACTIVE|DEVBUF_TYPE_CKD);
    cache_setage (CACHE_DEVBUF, o);
    dev->buf = cache_getbuf(CACHE_DEVBUF, o, dev->ckdtrksz);
    cache_unlock (CACHE_DEVBUF);
  
    /* Set the file descriptor */
    for (f = 0; f < dev->ckdnumfd; f++)
        if (trk < dev->ckdhitrk[f]) break;
    dev->fd = dev->ckdfd[f];

    /* Calculate the track offset */
    dev->ckdtrkoff = CKDDASD_DEVHDR_SIZE +
         (off_t)(trk - (f ? dev->ckdhitrk[f-1] : 0)) * dev->ckdtrksz;

    dev->syncio_active = active;

    DEVTRACE (_("HHCDA031I read trk %d reading file %d offset %lld len %d\n"),
              trk, f+1, (long long)dev->ckdtrkoff, dev->ckdtrksz);  

    /* Seek to the track image offset */
    offset = (off_t)dev->ckdtrkoff;
    offset = lseek (dev->fd, offset, SEEK_SET);
    if (offset < 0)
    {
        /* Handle seek error condition */
        logmsg (_("HHCDA032E error reading trk %d: lseek error: %s\n"),
                trk, strerror(errno));
        ckd_build_sense (dev, SENSE_EC, 0, 0, FORMAT_1, MESSAGE_0);
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        dev->bufcur = dev->cache = -1;
        cache_lock(CACHE_DEVBUF);
        cache_release(CACHE_DEVBUF, o, 0);
        cache_unlock(CACHE_DEVBUF);
        return -1;
    }

    /* Read the track image */
    rc = read (dev->fd, dev->buf, dev->ckdtrksz);
    if (rc < dev->ckdtrksz)
    {
        /* Handle read error condition */
        logmsg (_("HHCDA033E error reading trk %d: read error: %s\n"),
           trk, (rc < 0 ? strerror(errno) : "unexpected end of file"));
        ckd_build_sense (dev, SENSE_EC, 0, 0, FORMAT_1, MESSAGE_0);
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        dev->bufcur = dev->cache = -1;
        cache_lock(CACHE_DEVBUF);
        cache_release(CACHE_DEVBUF, o, 0);
        cache_unlock(CACHE_DEVBUF);
        return -1;
    }

    /* Validate the track header */
    DEVTRACE (_("HHCDA034I read trk %d trkhdr %2.2x %2.2x%2.2x %2.2x%2.2x\n"),
       trk, dev->buf[0], dev->buf[1], dev->buf[2], dev->buf[3], dev->buf[4]);
    trkhdr = (CKDDASD_TRKHDR *)dev->buf;
    if ((trkhdr->bin != 0
      || trkhdr->cyl[0] != (cyl >> 8)
      || trkhdr->cyl[1] != (cyl & 0xFF)
      || trkhdr->head[0] != (head >> 8)
      || trkhdr->head[1] != (head & 0xFF))
     && !dev->dasdcopy)
    {
        logmsg (_("HHCDA035E %4.4X invalid track header for cyl %d head %d "
                " %2.2x%2.2x%2.2x%2.2x%2.2x\n"), dev->devnum, cyl, head,
                trkhdr->bin,trkhdr->cyl[0],trkhdr->cyl[1],trkhdr->head[0],trkhdr->head[1]);
        ckd_build_sense (dev, 0, SENSE1_ITF, 0, 0, 0);
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        dev->bufcur = dev->cache = -1;
        cache_lock(CACHE_DEVBUF);
        cache_release(CACHE_DEVBUF, o, 0);
        cache_unlock(CACHE_DEVBUF);
        return -1;
    }

    dev->cache = o;
    dev->buf = cache_getbuf(CACHE_DEVBUF, dev->cache, 0);
    dev->bufcur = trk;
    dev->bufoff = 0;
    dev->bufoffhi = dev->ckdtrksz; 
    dev->buflen = ckd_trklen (dev, dev->buf);
    dev->bufsize = cache_getlen(CACHE_DEVBUF, dev->cache);

    return 0;
} /* end function ckdread_read_track */


/*-------------------------------------------------------------------*/
/* Update a track image                                              */
/*-------------------------------------------------------------------*/
static
int ckddasd_update_track (DEVBLK *dev, int trk, int off,
                      BYTE *buf, int len, BYTE *unitstat)
{
int             rc;                     /* Return code               */

    /* Immediately return if fake writing */
    if (dev->ckdfakewr)
        return len;

    /* Error if opened read-only */
    if (dev->ckdrdonly)
    {
        ckd_build_sense (dev, SENSE_EC, SENSE1_WRI, 0,
                        FORMAT_1, MESSAGE_0);
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return -1;
    }

    /* Read the track if it's not current */
    if (trk != dev->bufcur)
    {
        rc = (dev->hnd->read) (dev, trk, unitstat);
        if (rc < 0)
        {
            dev->bufcur = dev->cache = -1;
            return -1;
        }
    }

    /* Invalid track format if going past buffer end */
    if (off + len > dev->bufoffhi)
    {
        ckd_build_sense (dev, 0, SENSE1_ITF, 0, 0, 0);
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return -1;
    }

    /* Copy the data into the buffer */
    if (buf) memcpy (dev->buf + off, buf, len);

    /* Set low and high updated offsets */
    if (!dev->bufupd || off < dev->bufupdlo)
        dev->bufupdlo = off;
    if (off + len > dev->bufupdhi)
        dev->bufupdhi = off + len;

    /* Indicate track image has been modified */
    if (!dev->bufupd)
    {
        dev->bufupd = 1;
        shared_update_notify (dev, trk);
    }

    return len;
} /* end function ckd_update_track */

/*-------------------------------------------------------------------*/
/* CKD start/resume channel program                                  */
/*-------------------------------------------------------------------*/
void ckddasd_start (DEVBLK *dev)
{
    /* Reset buffer offsets */
    dev->bufoff = 0;
    dev->bufoffhi = dev->ckdtrksz;
}

/*-------------------------------------------------------------------*/
/* CKD end/suspend channel program                                   */
/*-------------------------------------------------------------------*/
void ckddasd_end (DEVBLK *dev)
{
BYTE    unitstat;                       /* Unit Status               */

    /* Write the last track image if it's modified */
    ckddasd_read_track (dev, -1, &unitstat);
}

/*-------------------------------------------------------------------*/
/* Return used cylinders                                             */
/*-------------------------------------------------------------------*/
static
int ckddasd_used (DEVBLK *dev)
{
    return dev->ckdcyls;
}

/*-------------------------------------------------------------------*/
/* Build sense data                                                  */
/*-------------------------------------------------------------------*/
void ckd_build_sense ( DEVBLK *dev, BYTE sense0, BYTE sense1,
                       BYTE sense2, BYTE format, BYTE message )
{
    /* Clear the sense bytes */
    memset (dev->sense, 0, sizeof(dev->sense));

    /* Sense bytes 0-2 are specified by caller */
    dev->sense[0] = sense0;
    dev->sense[1] = sense1;
    dev->sense[2] = sense2;

    /* Sense byte 3 contains the residual locate record count
       if imprecise ending is indicated in sense byte 1 */
    if (sense1 & SENSE1_IE)
    {
        if (dev->ckdtrkof)
            dev->sense[3] = dev->ckdcuroper;
        else
            dev->sense[3] = dev->ckdlcount;
    }

    /* Sense byte 4 is the physical device address */
    dev->sense[4] = 0;

    /* Sense byte 5 contains bits 8-15 of the cylinder address
       and sense byte 6 contains bits 4-7 of the cylinder
       address followed by bits 12-15 of the head address,
       unless the device has more than 4095 cylinders, in
       which case sense bytes 5 and 6 both contain X'FF' */
    if (dev->ckdcyls > 4095)
    {
        dev->sense[5] = 0xFF;
        dev->sense[6] = 0xFF;
    }
    else
    {
        dev->sense[5] = dev->ckdcurcyl & 0xFF;
        dev->sense[6] = ((dev->ckdcurcyl >> 4) & 0xF0)
                        | (dev->ckdcurhead & 0x0F);
    }

    /* Sense byte 7 contains the format code and message type */
    dev->sense[7] = (format << 4) | (message & 0x0F);

    /* Sense bytes 8-23 depend on the format code */
    switch (format) {

    case FORMAT_4: /* Data check */
    case FORMAT_5: /* Data check with displacement information */
        /* Sense bytes 8-12 contain the CCHHR of the record in error */
        dev->sense[8] = dev->ckdcurcyl >> 8;
        dev->sense[9] = dev->ckdcurcyl & 0xFF;
        dev->sense[10] = dev->ckdcurhead >> 8;
        dev->sense[11] = dev->ckdcurhead & 0xFF;
        dev->sense[12] = dev->ckdcurrec;
        break;

    } /* end switch(format) */

    /* Sense byte 27 bit 0 indicates 24-byte compatability sense data*/
    dev->sense[27] = 0x80;

    /* Sense bytes 29-30 contain the cylinder address */
    dev->sense[29] = dev->ckdcurcyl >> 8;
    dev->sense[30] = dev->ckdcurcyl & 0xFF;

    /* Sense byte 31 contains the head address */
    dev->sense[31] = dev->ckdcurhead & 0xFF;

} /* end function ckd_build_sense */

/*-------------------------------------------------------------------*/
/* Seek to a specified cylinder and head                             */
/*-------------------------------------------------------------------*/
static int ckd_seek ( DEVBLK *dev, int cyl, int head,
                      CKDDASD_TRKHDR *trkhdr, BYTE *unitstat )
{
int             rc;                     /* Return code               */

    DEVTRACE(_("HHCDA038I seeking to cyl %d head %d\n"), cyl, head);

    /* Read the track image */
    rc = ckd_read_cchh (dev, cyl, head, unitstat);
    if (rc < 0) return -1;

    /* Set device orientation fields */
    dev->ckdcurcyl = cyl;
    dev->ckdcurhead = head;
    dev->ckdcurrec = 0;
    dev->ckdcurkl = 0;
    dev->ckdcurdl = 0;
    dev->ckdrem = 0;
    dev->ckdorient = CKDORIENT_INDEX;

    /* Copy the track header */
    if (trkhdr) memcpy (trkhdr, &dev->buf[dev->bufoff], CKDDASD_TRKHDR_SIZE);

    /* Increment offset past the track header */
    dev->bufoff += CKDDASD_TRKHDR_SIZE;

    return 0;
} /* end function ckd_seek */


/*-------------------------------------------------------------------*/
/* Advance to next track for multitrack operation                    */
/*-------------------------------------------------------------------*/
static int mt_advance ( DEVBLK *dev, BYTE *unitstat )
{
int             rc;                     /* Return code               */
int             cyl;                    /* Next cyl for multitrack   */
int             head;                   /* Next head for multitrack  */

    /* File protect error if not within domain of Locate Record
       and file mask inhibits seek and multitrack operations */
    if (dev->ckdlcount == 0 &&
        (dev->ckdfmask & CKDMASK_SKCTL) == CKDMASK_SKCTL_INHSMT)
    {
        DEVTRACE(_("HHCDA039E MT advance error: "
                 "locate record %d file mask %2.2X\n"),
                 dev->ckdlcount, dev->ckdfmask);
       if (dev->ckdtrkof)
            ckd_build_sense (dev, 0, SENSE1_FP | SENSE1_IE, 0, 0, 0);
        else
           ckd_build_sense (dev, 0, SENSE1_FP, 0, 0, 0);
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return -1;
    }

    /* End of cylinder error if not within domain of Locate Record
       and current track is last track of cylinder */
    if (dev->ckdlcount == 0
        && dev->ckdcurhead >= dev->ckdheads - 1)
    {
    if (dev->ckdtrkof)
            ckd_build_sense (dev, 0, SENSE1_EOC | SENSE1_IE, 0, 0, 0);
        else
            ckd_build_sense (dev, 0, SENSE1_EOC, 0, 0, 0);
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return -1;
    }

    /* Advance to next track */
    cyl = dev->ckdcurcyl;
    head = dev->ckdcurhead + 1;
    if (head >= dev->ckdheads)
    {
        head = 0;
        cyl++;
    }
    DEVTRACE(_("HHCDA040I MT advance to cyl %d head %d\n"), cyl, head);

    /* File protect error if next track is outside the
       limits of the device or outside the defined extent */
    if (cyl >= dev->ckdcyls
        || (dev->ckdxtdef
            && (cyl < dev->ckdxbcyl || cyl > dev->ckdxecyl
                || (cyl == dev->ckdxbcyl && head < dev->ckdxbhead)
                || (cyl == dev->ckdxecyl && head > dev->ckdxehead)
            )))
    {
    if (dev->ckdtrkof)
            ckd_build_sense (dev, 0, SENSE1_FP | SENSE1_IE, 0, 0, 0);
        else
            ckd_build_sense (dev, 0, SENSE1_FP, 0, 0, 0);
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return -1;
    }

    /* Seek to next track */
    rc = ckd_seek (dev, cyl, head, NULL, unitstat);
    if (rc < 0) return -1;

    /* Successful return */
    return 0;

} /* end function mt_advance */

/*-------------------------------------------------------------------*/
/* Read count field                                                  */
/*-------------------------------------------------------------------*/
static int ckd_read_count ( DEVBLK *dev, BYTE code,
                CKDDASD_RECHDR *rechdr, BYTE *unitstat)
{
int             rc;                     /* Return code               */
int             skipr0 = 0;             /* 1=Skip record zero        */
int             cyl;                    /* Cylinder number for seek  */
int             head;                   /* Head number for seek      */
char           *orient[] = {"none", "index", "count", "key", "data", "eot"};

    /* Skip record 0 for all operations except READ TRACK, READ R0,
       SEARCH ID EQUAL, SEARCH ID HIGH, SEARCH ID EQUAL OR HIGH,
       LOCATE RECORD, and WRITE CKD NEXT TRACK */
    if (code != 0xDE
        && (code & 0x7F) != 0x16
        && (code & 0x7F) != 0x31
        && (code & 0x7F) != 0x51
        && (code & 0x7F) != 0x71
        && code != 0x47
        && code != 0x9D)
        skipr0 = 1;

    DEVTRACE (_("HHCDA041I read count orientation is %s\n"),
              orient[dev->ckdorient]);

    /* If orientation is at End-Of_Track then a multi-track advance
       failed previously during synchronous I/O */
    if (dev->ckdorient == CKDORIENT_EOT)
    {
        rc = mt_advance (dev, unitstat);
        if (rc < 0) return -1;
    }

    /* Search for next count field */
    for ( ; ; )
    {
        /* If oriented to count or key field, skip key and data */
        if (dev->ckdorient == CKDORIENT_COUNT)
            dev->bufoff += dev->ckdcurkl + dev->ckdcurdl;
        else if (dev->ckdorient == CKDORIENT_KEY)
            dev->bufoff += dev->ckdcurdl;

        /* Make sure we don't copy past the end of the buffer */
        if (dev->bufoff + CKDDASD_RECHDR_SIZE >= dev->bufoffhi)
        {
            /* Handle error condition */
            logmsg (_("HHCDA042E attempt to read past end of track %d %d\n"),
                    dev->bufoff, dev->bufoffhi);

            /* Set unit check with equipment check */
            ckd_build_sense (dev, SENSE_EC, 0, 0,
                            FORMAT_1, MESSAGE_0);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            return -1;
        }

        /* Copy the record header (count field) */
        memcpy (rechdr, &dev->buf[dev->bufoff], CKDDASD_RECHDR_SIZE);
        dev->bufoff += CKDDASD_RECHDR_SIZE;

        /* Set the device orientation fields */
        dev->ckdcurrec = rechdr->rec;
        dev->ckdrem = 0;
        dev->ckdorient = CKDORIENT_COUNT;
        dev->ckdcurkl = rechdr->klen;
        dev->ckdcurdl = (rechdr->dlen[0] << 8) + rechdr->dlen[1];
        dev->ckdtrkof = (rechdr->cyl[0] == 0xFF) ? 0 : rechdr->cyl[0] >> 7;

        DEVTRACE(_("HHCDA043I cyl %d head %d record %d kl %d dl %d of %d\n"),
                dev->ckdcurcyl, dev->ckdcurhead, dev->ckdcurrec,
                dev->ckdcurkl, dev->ckdcurdl, dev->ckdtrkof);

        /* Skip record zero if user data record required */
        if (skipr0 && rechdr->rec == 0)
            continue;

        /* Test for logical end of track and exit if not */
        if (memcmp(rechdr, eighthexFF, 8) != 0)
            break;
        dev->ckdorient = CKDORIENT_EOT;

        /* For READ TRACK or READ MULTIPLE CKD, return with the
           end of track marker in the record header field */
        if (code == 0xDE || code == 0x5E)
            break;

        /* End of track found, so terminate with no record found
           error if this is a LOCATE RECORD or WRITE CKD NEXT TRACK
           command; or if this is the second end of track in this
           channel program without an intervening read of the home
           address or data area and without an intervening write,
           sense, or control command --
           -- except when multitrack READ or SEARCH [KEY?] command
           operates outside the domain of a locate record */
        if (code == 0x47 || code == 0x9D
            || (dev->ckdxmark
                && !((dev->ckdlcount == 0)
                     && ( (IS_CCW_READ(code) && (code&0x80))
                         || code==0xA9 || code==0xC9 || code==0xE9) )))
        {
            ckd_build_sense (dev, 0, SENSE1_NRF, 0, 0, 0);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            return -1;
        }

        /* Test for multitrack operation */
        if ((code & 0x80) == 0)
        {
            /* If non-multitrack, return to start of current track */
            cyl = dev->ckdcurcyl;
            head = dev->ckdcurhead;
            rc = ckd_seek (dev, cyl, head, NULL, unitstat);
            if (rc < 0) return -1;

            /* Set index marker found flag */
            dev->ckdxmark = 1;
        }
        else
        {
            /* If multitrack, attempt to advance to next track */
            rc = mt_advance (dev, unitstat);
            if (rc < 0) return -1;

            /* Set index marker flag if non-search command */
            if ((code & 0x7F) != 0x31
                && (code & 0x7F) != 0x51
                && (code & 0x7F) != 0x71
                && (code & 0x7F) != 0x29
                && (code & 0x7F) != 0x49
                && (code & 0x7F) != 0x69)
                dev->ckdxmark = 1;
        }

    } /* end for */

    return 0;

} /* end function ckd_read_count */


/*-------------------------------------------------------------------*/
/* Read key field                                                    */
/*-------------------------------------------------------------------*/
static int ckd_read_key ( DEVBLK *dev, BYTE code,
                BYTE *buf, BYTE *unitstat)
{
int             rc;                     /* Return code               */
CKDDASD_RECHDR  rechdr;                 /* CKD record header         */

    /* If not oriented to count field, read next count field */
    if (dev->ckdorient != CKDORIENT_COUNT)
    {
        rc = ckd_read_count (dev, code, &rechdr, unitstat);
        if (rc < 0) return rc;
    }

    DEVTRACE(_("HHCDA044I read key %d bytes\n"), dev->ckdcurkl);

    /* Read key field */
    if (dev->ckdcurkl > 0)
    {
        if (dev->bufoffhi - dev->bufoff < dev->ckdcurkl)
        {
            /* Handle error condition */
            logmsg (_("ckddasd: attempt to read past end of track\n"));

            /* Set unit check with equipment check */
            ckd_build_sense (dev, SENSE_EC, 0, 0,
                            FORMAT_1, MESSAGE_0);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            return -1;
        }

        memcpy (buf, &dev->buf[dev->bufoff], dev->ckdcurkl);
        dev->bufoff += dev->ckdcurkl;
    }

    /* Set the device orientation fields */
    dev->ckdrem = 0;
    dev->ckdorient = CKDORIENT_KEY;

    return 0;
} /* end function ckd_read_key */


/*-------------------------------------------------------------------*/
/* Read data field                                                   */
/*-------------------------------------------------------------------*/
static int ckd_read_data ( DEVBLK *dev, BYTE code,
                BYTE *buf, BYTE *unitstat)
{
int             rc;                     /* Return code               */
CKDDASD_RECHDR  rechdr;                 /* Record header             */

    /* If not oriented to count or key field, read next count field */
    if (dev->ckdorient != CKDORIENT_COUNT
        && dev->ckdorient != CKDORIENT_KEY)
    {
        rc = ckd_read_count (dev, code, &rechdr, unitstat);
        if (rc < 0) return rc;
    }

    /* If oriented to count field, skip the key field */
    if (dev->ckdorient == CKDORIENT_COUNT)
        dev->bufoff += dev->ckdcurkl;

    DEVTRACE(_("HHCDA045I read data %d bytes\n"), dev->ckdcurdl);

    /* Read data field */
    if (dev->ckdcurdl > 0)
    {
        if (dev->bufoff + dev->ckdcurdl >= dev->bufoffhi)
        {
            /* Handle error condition */
            logmsg (_("HHCDA046E attempt to read past end of track\n"));

            /* Set unit check with equipment check */
            ckd_build_sense (dev, SENSE_EC, 0, 0,
                            FORMAT_1, MESSAGE_0);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            return -1;
        }
        memcpy (buf, &dev->buf[dev->bufoff], dev->ckdcurdl);
        dev->bufoff += dev->ckdcurdl;
    }

    /* Set the device orientation fields */
    dev->ckdrem = 0;
    dev->ckdorient = CKDORIENT_DATA;

    return 0;
} /* end function ckd_read_data */


/*-------------------------------------------------------------------*/
/* Erase remainder of track                                          */
/*-------------------------------------------------------------------*/
static int ckd_erase ( DEVBLK *dev, BYTE *buf, int len, int *size,
                BYTE *unitstat)
{
int             rc;                     /* Return code               */
CKDDASD_RECHDR  rechdr;                 /* CKD record header         */
int             keylen;                 /* Key length                */
int             datalen;                /* Data length               */
int             ckdlen;                 /* Count+key+data length     */

    /* If oriented to count or key field, skip key and data */
    if (dev->ckdorient == CKDORIENT_COUNT)
        dev->bufoff += dev->ckdcurkl + dev->ckdcurdl;
    else if (dev->ckdorient == CKDORIENT_KEY)
        dev->bufoff += dev->ckdcurdl;

    /* Copy the count field from the buffer */
    memset (&rechdr, 0, CKDDASD_RECHDR_SIZE);
    memcpy (&rechdr, buf, (len < CKDDASD_RECHDR_SIZE) ?
                                len : CKDDASD_RECHDR_SIZE);

    /* Extract the key length and data length */
    keylen = rechdr.klen;
    datalen = (rechdr.dlen[0] << 8) + rechdr.dlen[1];

    /* Calculate total count key and data size */
    ckdlen = CKDDASD_RECHDR_SIZE + keylen + datalen;

    /* Check that there is enough space on the current track to
       contain the complete erase plus an end of track marker */
    if (dev->bufoff + ckdlen + 8 >= dev->bufoffhi)
    {
        /* Unit check with invalid track format */
        ckd_build_sense (dev, 0, SENSE1_ITF, 0, 0, 0);
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return -1;
    }

    /* Logically erase rest of track by writing end of track marker */
    rc = (dev->hnd->write) (dev, dev->bufcur, dev->bufoff, eighthexFF, 8, unitstat);
    if (rc < 0) return -1;

    /* Return total count key and data size */
    *size = ckdlen;

    /* Set the device orientation fields */
    dev->ckdrem = 0;
    dev->ckdorient = CKDORIENT_DATA;

    return 0;
} /* end function ckd_erase */


/*-------------------------------------------------------------------*/
/* Write count key and data fields                                   */
/*-------------------------------------------------------------------*/
static int ckd_write_ckd ( DEVBLK *dev, BYTE *buf, int len,
                BYTE *unitstat, BYTE trk_ovfl)
{
int             rc;                     /* Return code               */
CKDDASD_RECHDR  rechdr;                 /* CKD record header         */
int             recnum;                 /* Record number             */
int             keylen;                 /* Key length                */
int             datalen;                /* Data length               */
int             ckdlen;                 /* Count+key+data length     */

    /* If oriented to count or key field, skip key and data */
    if (dev->ckdorient == CKDORIENT_COUNT)
        dev->bufoff += dev->ckdcurkl + dev->ckdcurdl;
    else if (dev->ckdorient == CKDORIENT_KEY)
        dev->bufoff += dev->ckdcurdl;

    /* Copy the count field from the buffer */
    memset (&rechdr, 0, CKDDASD_RECHDR_SIZE);
    memcpy (&rechdr, buf, (len < CKDDASD_RECHDR_SIZE) ?
                                len : CKDDASD_RECHDR_SIZE);

    /* Extract the record number, key length and data length */
    recnum = rechdr.rec;
    keylen = rechdr.klen;
    datalen = (rechdr.dlen[0] << 8) + rechdr.dlen[1];

    /* Calculate total count key and data size */
    ckdlen = CKDDASD_RECHDR_SIZE + keylen + datalen;

    if (dev->bufoff + ckdlen + 8 >= dev->bufoffhi)
    {
        /* Unit check with invalid track format */
        ckd_build_sense (dev, 0, SENSE1_ITF, 0, 0, 0);
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return -1;
    }

    /* Pad the I/O buffer with zeroes if necessary */
    while (len < ckdlen) buf[len++] = '\0';

    DEVTRACE(_("HHCDA047I writing cyl %d head %d record %d kl %d dl %d\n"),
            dev->ckdcurcyl, dev->ckdcurhead, recnum, keylen, datalen);

    /* Set track overflow flag if called for */
    if (trk_ovfl)
    {
        DEVTRACE(_("HHCDA048I setting track overflow flag for "
                 "cyl %d head %d record %d\n"),
                 dev->ckdcurcyl, dev->ckdcurhead, recnum);
        buf[0] |= 0x80;
    }

    /* Write count key and data */
    rc = (dev->hnd->write) (dev, dev->bufcur, dev->bufoff, buf, ckdlen, unitstat);
    if (rc < 0) return -1;
    dev->bufoff += ckdlen;

    /* Clear track overflow flag if we set it above */
    if (trk_ovfl)
    {
        buf[0] &= 0x7F;
    }

    /* Logically erase rest of track by writing end of track marker */
    rc = (dev->hnd->write) (dev, dev->bufcur, dev->bufoff, eighthexFF, 8, unitstat);
    if (rc < 0) return -1;

    /* Set the device orientation fields */
    dev->ckdcurrec = recnum;
    dev->ckdcurkl = keylen;
    dev->ckdcurdl = datalen;
    dev->ckdrem = 0;
    dev->ckdorient = CKDORIENT_DATA;
    dev->ckdtrkof = trk_ovfl & 1;

    return 0;
} /* end function ckd_write_ckd */


/*-------------------------------------------------------------------*/
/* Write key and data fields                                         */
/*-------------------------------------------------------------------*/
static int ckd_write_kd ( DEVBLK *dev, BYTE *buf, int len,
                BYTE *unitstat)
{
int             rc;                     /* Return code               */
int             kdlen;                  /* Key+data length           */

    /* Unit check if not oriented to count area */
    if (dev->ckdorient != CKDORIENT_COUNT)
    {
        logmsg (_("HHCDA049E Write KD orientation error\n"));
        ckd_build_sense (dev, SENSE_CR, 0, 0,
                        FORMAT_0, MESSAGE_2);
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return -1;
    }

    /* Calculate total key and data size */
    kdlen = dev->ckdcurkl + dev->ckdcurdl;

    /* Pad the I/O buffer with zeroes if necessary */
    while (len < kdlen) buf[len++] = '\0';

    DEVTRACE(_("HHCDA050I updating cyl %d head %d record %d kl %d dl %d\n"),
            dev->ckdcurcyl, dev->ckdcurhead, dev->ckdcurrec,
            dev->ckdcurkl, dev->ckdcurdl);

    /* Write key and data */
    rc = (dev->hnd->write) (dev, dev->bufcur, dev->bufoff, buf, kdlen, unitstat);
    if (rc < 0) return -1;
    dev->bufoff += kdlen;

    /* Set the device orientation fields */
    dev->ckdrem = 0;
    dev->ckdorient = CKDORIENT_DATA;

    return 0;
} /* end function ckd_write_kd */


/*-------------------------------------------------------------------*/
/* Write data field                                                  */
/*-------------------------------------------------------------------*/
static int ckd_write_data ( DEVBLK *dev, BYTE *buf, int len,
                BYTE *unitstat)
{
int             rc;                     /* Return code               */

    /* Unit check if not oriented to count or key areas */
    if (dev->ckdorient != CKDORIENT_COUNT
        && dev->ckdorient != CKDORIENT_KEY)
    {
        logmsg (_("HHCDA051E Write data orientation error\n"));
        ckd_build_sense (dev, SENSE_CR, 0, 0,
                        FORMAT_0, MESSAGE_2);
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return -1;
    }

    /* If oriented to count field, skip the key field */
    if (dev->ckdorient == CKDORIENT_COUNT)
        dev->bufoff += dev->ckdcurkl;

    /* Pad the I/O buffer with zeroes if necessary */
    while (len < dev->ckdcurdl) buf[len++] = '\0';

    DEVTRACE(_("HHCDA052I updating cyl %d head %d record %d dl %d\n"),
            dev->ckdcurcyl, dev->ckdcurhead, dev->ckdcurrec,
            dev->ckdcurdl);

    /* Write data */
    rc = (dev->hnd->write) (dev, dev->bufcur, dev->bufoff, buf, dev->ckdcurdl, unitstat);
    if (rc < 0) return -1;
    dev->bufoff += dev->ckdcurdl;

    /* Set the device orientation fields */
    dev->ckdrem = 0;
    dev->ckdorient = CKDORIENT_DATA;

    return 0;
} /* end function ckd_write_data */


/*-------------------------------------------------------------------*/
/* Execute a Channel Command Word                                    */
/*-------------------------------------------------------------------*/
void ckddasd_execute_ccw ( DEVBLK *dev, BYTE code, BYTE flags,
        BYTE chained, U16 count, BYTE prevcode, int ccwseq,
        BYTE *iobuf, BYTE *more, BYTE *unitstat, U16 *residual )
{
int             rc;                     /* Return code               */
int             i;                      /* Loop index                */
CKDDASD_TRKHDR  trkhdr;                 /* CKD track header (HA)     */
CKDDASD_RECHDR  rechdr;                 /* CKD record header (count) */
int             size;                   /* Number of bytes available */
int             num;                    /* Number of bytes to move   */
int     offset;                 /* Offset into buf for I/O   */
int             bin;                    /* Bin number                */
int             cyl;                    /* Cylinder number           */
int             head;                   /* Head number               */
BYTE            cchhr[5];               /* Search argument           */
BYTE            sector;                 /* Sector number             */
BYTE            key[256];               /* Key for search operations */
BYTE            trk_ovfl;               /* == 1 if track ovfl write  */

    /* If this is a data-chained READ, then return any data remaining
       in the buffer which was not used by the previous CCW */
    if (chained & CCW_FLAGS_CD)
    {
        memmove (iobuf, iobuf + dev->ckdpos, dev->ckdrem);
        num = (count < dev->ckdrem) ? count : dev->ckdrem;
        *residual = count - num;
        if (count < dev->ckdrem) *more = 1;
        dev->ckdrem -= num;
        dev->ckdpos = num;
        *unitstat = CSW_CE | CSW_DE;
        return;
    }

    /* Command reject if data chaining and command is not READ */
    if ((flags & CCW_FLAGS_CD) && code != 0x02 && code != 0x5E
        && (code & 0x7F) != 0x1E && (code & 0x7F) != 0x1A
        && (code & 0x7F) != 0x16 && (code & 0x7F) != 0x12
        && (code & 0x7F) != 0x0E && (code & 0x7F) != 0x06)
    {
        logmsg(_("HHCDA053E Data chaining not supported for CCW %2.2X\n"),
                code);
        ckd_build_sense (dev, SENSE_CR, 0, 0,
                        FORMAT_0, MESSAGE_1);
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return;
    }

    /* Reset flags at start of CCW chain */
    if (chained == 0 && !dev->syncio_retry)
    {
        dev->ckdxtdef = 0;
        dev->ckdsetfm = 0;
        dev->ckdlocat = 0;
        dev->ckdspcnt = 0;
        dev->ckdseek = 0;
        dev->ckdskcyl = 0;
        dev->ckdrecal = 0;
        dev->ckdrdipl = 0;
        dev->ckdfmask = 0;
        dev->ckdxmark = 0;
        dev->ckdhaeq = 0;
        dev->ckdideq = 0;
        dev->ckdkyeq = 0;
        dev->ckdwckd = 0;
        dev->ckdlcount = 0;
        dev->ckdtrkof = 0;
        /* ISW20030819-1 : Clear Write HA flag */
        dev->ckdwrha = 0;
    }
    dev->syncio_retry = 0;

    /* Reset index marker flag if sense or control command,
       or any write command (other search ID or search key),
       or any read command except read sector --
       -- and except single track Read Count */
    if (IS_CCW_SENSE(code) || IS_CCW_CONTROL(code)
        || (IS_CCW_WRITE(code)
            && (code & 0x7F) != 0x31
            && (code & 0x7F) != 0x51
            && (code & 0x7F) != 0x71
            && (code & 0x7F) != 0x29
            && (code & 0x7F) != 0x49
            && (code & 0x7F) != 0x69)
        || (IS_CCW_READ(code)
            &&  code         != 0x12
            && (code & 0x7F) != 0x22))
        dev->ckdxmark = 0;

    /* Note current operation for track overflow sense byte 3 */
    dev->ckdcuroper = (IS_CCW_READ(code)) ? 6 :
        ((IS_CCW_WRITE(code)) ? 5 : 0);

    /* Process depending on CCW opcode */
    switch (code) {

    case 0x02:
    /*---------------------------------------------------------------*/
    /* READ IPL                                                      */
    /*---------------------------------------------------------------*/
        /* Command reject if preceded by a Define Extent or
           Set File Mask, or within the domain of a Locate Record */
        if (dev->ckdxtdef || dev->ckdsetfm || dev->ckdlcount > 0)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_2);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Set define extent parameters */
        dev->ckdxtdef = 1;
        dev->ckdsetfm = 1;
        dev->ckdfmask = 0;
        dev->ckdxgattr = 0;
        dev->ckdxblksz = 0;
        dev->ckdxbcyl = 0;
        dev->ckdxbhead = 0;
        dev->ckdxecyl = dev->ckdcyls - 1;
        dev->ckdxehead = dev->ckdheads - 1;

        /* Set locate record parameters */
        dev->ckdloper = CKDOPER_ORIENT_DATA | CKDOPER_RDDATA;
        dev->ckdlaux = 0;
        dev->ckdlcount = 2;
        dev->ckdltranlf = 0;

        /* Seek to start of cylinder zero track zero */
        rc = ckd_seek (dev, 0, 0, &trkhdr, unitstat);
        if (rc < 0) break;

        /* Read count field for first record following R0 */
        rc = ckd_read_count (dev, code, &rechdr, unitstat);
        if (rc < 0) break;

        /* Calculate number of bytes to read and set residual count */
        size = dev->ckdcurdl;
        num = (count < size) ? count : size;
        *residual = count - num;
        if (count < size) *more = 1;

        /* Read data field */
        rc = ckd_read_data (dev, code, iobuf, unitstat);
        if (rc < 0) break;

        /* Set command processed flag */
        dev->ckdrdipl = 1;

        /* Save size and offset of data not used by this CCW */
        dev->ckdrem = size - num;
        dev->ckdpos = num;

        /* Return unit exception if data length is zero */
        if (dev->ckdcurdl == 0)
            *unitstat = CSW_CE | CSW_DE | CSW_UX;
        else
            *unitstat = CSW_CE | CSW_DE;

        break;

    case 0x03:
    /*---------------------------------------------------------------*/
    /* CONTROL NO-OPERATION                                          */
    /*---------------------------------------------------------------*/
        /* Command reject if within the domain of a Locate Record */
        if (dev->ckdlcount > 0)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_2);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Return normal status */
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x17:
    /*---------------------------------------------------------------*/
    /* RESTORE                                                       */
    /*---------------------------------------------------------------*/
        /* Command reject if within the domain of a Locate Record */
        if (dev->ckdlcount > 0)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_2);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Return normal status */
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x06:
    case 0x86:
    /*---------------------------------------------------------------*/
    /* READ DATA                                                     */
    /*---------------------------------------------------------------*/
        /* For 3990, command reject if not preceded by Seek, Seek Cyl,
           Locate Record, Read IPL, or Recalibrate command */
        if (dev->ckd3990
            && dev->ckdseek == 0 && dev->ckdskcyl == 0
            && dev->ckdlocat == 0 && dev->ckdrdipl == 0
            && dev->ckdrecal == 0)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_2);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Check operation code if within domain of a Locate Record */
        if (dev->ckdlcount > 0)
        {
            if (!((dev->ckdloper & CKDOPER_CODE) == CKDOPER_RDDATA
                  || (dev->ckdloper & CKDOPER_CODE) == CKDOPER_RDANY
                  || (dev->ckdloper & CKDOPER_CODE) == CKDOPER_READ))
            {
                ckd_build_sense (dev, SENSE_CR, 0, 0,
                                FORMAT_0, MESSAGE_2);
                *unitstat = CSW_CE | CSW_DE | CSW_UC;
                break;
            }
        }

        /* If not oriented to count or key field, read next count */
        if (dev->ckdorient != CKDORIENT_COUNT
            && dev->ckdorient != CKDORIENT_KEY)
        {
            rc = ckd_read_count (dev, code, &rechdr, unitstat);
            if (rc < 0) break;
        }

        /* Calculate number of bytes to read and set residual count */
        size = dev->ckdcurdl;
        num = (count < size) ? count : size;
        *residual = count - num;
        if (count < size) *more = 1;
        offset = 0;

        /* Read data field */
        rc = ckd_read_data (dev, code, iobuf, unitstat);
        if (rc < 0) break;

    /* If track overflow, keep reading */
    while (dev->ckdtrkof)
    {
        /* Advance to next track */
        rc = mt_advance (dev, unitstat);
        if (rc < 0) break;
    
        /* Read the first count field */
        rc = ckd_read_count (dev, code, &rechdr, unitstat);
        if (rc < 0) break;
    
        /* Skip the key field if present */
        if (dev->ckdcurkl > 0)
                dev->bufoff += dev->ckdcurkl;

        /* Set offset into buffer for this read */
        offset += num;

        /* Account for size of this overflow record */
        size = dev->ckdcurdl;
        num = (*residual < size) ? *residual : size;
        if (*residual < size) *more = 1;
        else *more = 0;
        *residual -= num;
    
        /* Read the next data field */
        rc = ckd_read_data (dev, code, iobuf+offset, unitstat);
        if (rc < 0) break;
    }
    
    /* Bail out if track overflow produced I/O error */
    if (rc < 0) break;

        /* Save size and offset of data not used by this CCW */
        dev->ckdrem = size - num;
        dev->ckdpos = num;

        /* Return unit exception if data length is zero */
        if (dev->ckdcurdl == 0)
            *unitstat = CSW_CE | CSW_DE | CSW_UX;
        else
            *unitstat = CSW_CE | CSW_DE;

        break;

    case 0x0E:
    case 0x8E:
    /*---------------------------------------------------------------*/
    /* READ KEY AND DATA                                             */
    /*---------------------------------------------------------------*/
        /* For 3990, command reject if not preceded by Seek, Seek Cyl,
           Locate Record, Read IPL, or Recalibrate command */
        if (dev->ckd3990
            && dev->ckdseek == 0 && dev->ckdskcyl == 0
            && dev->ckdlocat == 0 && dev->ckdrdipl == 0
            && dev->ckdrecal == 0)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_2);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Check operation code if within domain of a Locate Record */
        if (dev->ckdlcount > 0)
        {
            if (!((dev->ckdloper & CKDOPER_CODE) == CKDOPER_RDDATA
                  || (dev->ckdloper & CKDOPER_CODE) == CKDOPER_RDANY
                  || (dev->ckdloper & CKDOPER_CODE) == CKDOPER_READ))
            {
                ckd_build_sense (dev, SENSE_CR, 0, 0,
                                FORMAT_0, MESSAGE_2);
                *unitstat = CSW_CE | CSW_DE | CSW_UC;
                break;
            }
        }

        /* If not oriented to count field, read next count */
        if (dev->ckdorient != CKDORIENT_COUNT)
        {
            rc = ckd_read_count (dev, code, &rechdr, unitstat);
            if (rc < 0) break;
        }

        /* Calculate number of bytes to read and set residual count */
        size = dev->ckdcurkl + dev->ckdcurdl;
        num = (count < size) ? count : size;
        *residual = count - num;
        if (count < size) *more = 1;
        offset = dev->ckdcurkl;

        /* Read key field */
        rc = ckd_read_key (dev, code, iobuf, unitstat);
        if (rc < 0) break;

        /* Read data field */
        rc = ckd_read_data (dev, code, iobuf+dev->ckdcurkl, unitstat);
        if (rc < 0) break;

    /* If track overflow, keep reading */
    while (dev->ckdtrkof)
    {
        /* Advance to next track */
        rc = mt_advance (dev, unitstat);
        if (rc < 0) break;
    
        /* Read the first count field */
        rc = ckd_read_count (dev, code, &rechdr, unitstat);
        if (rc < 0) break;
    
        /* Skip the key field if present */
        if (dev->ckdcurkl > 0)
                dev->bufoff += dev->ckdcurkl;

        /* Set offset into buffer for this read */
        offset += num;

        /* Account for size of this overflow record */
        size = dev->ckdcurdl;
        num = (*residual < size) ? *residual : size;
        if (*residual < size) *more = 1;
        else *more = 0;
        *residual -= num;
    
        /* Read the next data field */
        rc = ckd_read_data (dev, code, iobuf+offset, unitstat);
        if (rc < 0) break;
    }

    /* Bail out if track overflow produced I/O error */
    if (rc < 0) break;

        /* Save size and offset of data not used by this CCW */
        dev->ckdrem = size - num;
        dev->ckdpos = num;

        /* Return unit exception if data length is zero */
        if (dev->ckdcurdl == 0)
            *unitstat = CSW_CE | CSW_DE | CSW_UX;
        else
            *unitstat = CSW_CE | CSW_DE;

        break;

    case 0x12:
    case 0x92:
    /*---------------------------------------------------------------*/
    /* READ COUNT                                                    */
    /*---------------------------------------------------------------*/
        /* For 3990, command reject if not preceded by Seek, Seek Cyl,
           Locate Record, Read IPL, or Recalibrate command */
        if (dev->ckd3990
            && dev->ckdseek == 0 && dev->ckdskcyl == 0
            && dev->ckdlocat == 0 && dev->ckdrdipl == 0
            && dev->ckdrecal == 0)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_2);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Check operation code if within domain of a Locate Record */
        if (dev->ckdlcount > 0)
        {
            if (!((dev->ckdloper & CKDOPER_CODE) == CKDOPER_RDDATA
                  || (dev->ckdloper & CKDOPER_CODE) == CKDOPER_RDANY
                  || (dev->ckdloper & CKDOPER_CODE) == CKDOPER_READ
                  || ((dev->ckdloper & CKDOPER_CODE) == CKDOPER_WRITE
                      && (dev->ckdlaux & CKDLAUX_RDCNTSUF)
                      && dev->ckdlcount == 1)))
            {
                ckd_build_sense (dev, SENSE_CR, 0, 0,
                                FORMAT_0, MESSAGE_2);
                *unitstat = CSW_CE | CSW_DE | CSW_UC;
                break;
            }
        }

        /* Read next count field */
        rc = ckd_read_count (dev, code, &rechdr, unitstat);
        if (rc < 0) break;

        /* Calculate number of bytes to read and set residual count */
        size = CKDDASD_RECHDR_SIZE;
        num = (count < size) ? count : size;
        *residual = count - num;
        if (count < size) *more = 1;

        /* Copy count field to I/O buffer */
        memcpy (iobuf, &rechdr, CKDDASD_RECHDR_SIZE);

    /* Turn off track overflow flag in read record header */
    *iobuf &= 0x7F;

        /* Save size and offset of data not used by this CCW */
        dev->ckdrem = size - num;
        dev->ckdpos = num;

        /* Return normal status */
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x16:
    case 0x96:
    /*---------------------------------------------------------------*/
    /* READ RECORD ZERO                                              */
    /*---------------------------------------------------------------*/
        /* For 3990, command reject if not preceded by Seek, Seek Cyl,
           Locate Record, Read IPL, or Recalibrate command */
        if (dev->ckd3990
            && dev->ckdseek == 0 && dev->ckdskcyl == 0
            && dev->ckdlocat == 0 && dev->ckdrdipl == 0
            && dev->ckdrecal == 0)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_2);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Check operation code if within domain of a Locate Record */
        if (dev->ckdlcount > 0)
        {
            if (!((dev->ckdloper & CKDOPER_CODE) == CKDOPER_RDDATA
                  || ((dev->ckdloper & CKDOPER_CODE) == CKDOPER_READ
                      && ((dev->ckdloper & CKDOPER_ORIENTATION)
                                == CKDOPER_ORIENT_HOME
                          || (dev->ckdloper & CKDOPER_ORIENTATION)
                                == CKDOPER_ORIENT_INDEX
                        ))))
            {
                ckd_build_sense (dev, SENSE_CR, 0, 0,
                                FORMAT_0, MESSAGE_2);
                *unitstat = CSW_CE | CSW_DE | CSW_UC;
                break;
            }
        }

        /* For multitrack operation outside domain of a Locate Record,
           attempt to advance to the next track before reading R0 */
        if ((code & 0x80) && dev->ckdlcount == 0)
        {
            rc = mt_advance (dev, unitstat);
            if (rc < 0) break;
        }

        /* Seek to beginning of track */
        rc = ckd_seek (dev, dev->ckdcurcyl, dev->ckdcurhead, &trkhdr, unitstat);
        if (rc < 0) break;

        /* Read the count field for record zero */
        rc = ckd_read_count (dev, code, &rechdr, unitstat);
        if (rc < 0) break;

        /* Calculate number of bytes to read and set residual count */
        size = CKDDASD_RECHDR_SIZE + dev->ckdcurkl + dev->ckdcurdl;
        num = (count < size) ? count : size;
        *residual = count - num;
        if (count < size) *more = 1;

        /* Copy count field to I/O buffer */
        memcpy (iobuf, &rechdr, CKDDASD_RECHDR_SIZE);

    /* Turn off track overflow flag in read record header */
    *iobuf &= 0x7F;

        /* Read key field */
        rc = ckd_read_key (dev, code,
                            iobuf + CKDDASD_RECHDR_SIZE, unitstat);
        if (rc < 0) break;

        /* Read data field */
        rc = ckd_read_data (dev, code,
                            iobuf + CKDDASD_RECHDR_SIZE + dev->ckdcurkl,
                            unitstat);
        if (rc < 0) break;

        /* Save size and offset of data not used by this CCW */
        dev->ckdrem = size - num;
        dev->ckdpos = num;

        /* Return unit exception if data length is zero */
        if (dev->ckdcurdl == 0)
            *unitstat = CSW_CE | CSW_DE | CSW_UX;
        else
            *unitstat = CSW_CE | CSW_DE;

        break;

    case 0x1A:
    case 0x9A:
    /*---------------------------------------------------------------*/
    /* READ HOME ADDRESS                                             */
    /*---------------------------------------------------------------*/
        /* For 3990, command reject if not preceded by Seek, Seek Cyl,
           Locate Record, Read IPL, or Recalibrate command */
        if (dev->ckd3990
            && dev->ckdseek == 0 && dev->ckdskcyl == 0
            && dev->ckdlocat == 0 && dev->ckdrdipl == 0
            && dev->ckdrecal == 0)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_2);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Check operation code if within domain of a Locate Record */
        if (dev->ckdlcount > 0)
        {
            if (!((dev->ckdloper & CKDOPER_CODE) == CKDOPER_RDDATA
                  || ((dev->ckdloper & CKDOPER_CODE) == CKDOPER_READ
                      && (dev->ckdloper & CKDOPER_ORIENTATION)
                                == CKDOPER_ORIENT_INDEX
                    )))
            {
                ckd_build_sense (dev, SENSE_CR, 0, 0,
                                FORMAT_0, MESSAGE_2);
                *unitstat = CSW_CE | CSW_DE | CSW_UC;
                break;
            }
        }

        /* For multitrack operation outside domain of a Locate Record,
           attempt to advance to the next track before reading HA */
        if ((code & 0x80) && dev->ckdlcount == 0)
        {
            rc = mt_advance (dev, unitstat);
            if (rc < 0) break;
        }

        /* Seek to beginning of track */
        rc = ckd_seek (dev, dev->ckdcurcyl, dev->ckdcurhead, &trkhdr, unitstat);
        if (rc < 0) break;

        /* Calculate number of bytes to read and set residual count */
        size = CKDDASD_TRKHDR_SIZE;
        num = (count < size) ? count : size;
        *residual = count - num;
        if (count < size) *more = 1;

        /* Copy home address field to I/O buffer */
        memcpy (iobuf, &trkhdr, CKDDASD_TRKHDR_SIZE);

        /* Save size and offset of data not used by this CCW */
        dev->ckdrem = size - num;
        dev->ckdpos = num;

        /* Return normal status */
        *unitstat = CSW_CE | CSW_DE;

        break;

    case 0x19:
    /*---------------------------------------------------------------*/
    /* WRITE HOME ADDRESS                                            */
    /*---------------------------------------------------------------*/
        /* For 3990, command reject if not preceded by Seek, Seek Cyl,
           Locate Record, Read IPL, or Recalibrate command */
        if (dev->ckd3990
            && dev->ckdseek == 0 && dev->ckdskcyl == 0
            && dev->ckdlocat == 0 && dev->ckdrdipl == 0
            && dev->ckdrecal == 0)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_2);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Check operation code if within domain of a Locate Record */
        if (dev->ckdlcount > 0)
        {
            if (!((dev->ckdloper & CKDOPER_CODE) == CKDOPER_RDDATA
                  || ((dev->ckdloper & CKDOPER_CODE) == CKDOPER_READ
                      && (dev->ckdloper & CKDOPER_ORIENTATION)
                                == CKDOPER_ORIENT_INDEX
                    )))
            {
                ckd_build_sense (dev, SENSE_CR, 0, 0,
                                FORMAT_0, MESSAGE_2);
                *unitstat = CSW_CE | CSW_DE | CSW_UC;
                break;
            }
        }

        /* File protected if file mask does not allow Write HA */
        if ((dev->ckdfmask & CKDMASK_WRCTL) != CKDMASK_WRCTL_ALLWRT)
        {
            ckd_build_sense (dev, 0, SENSE1_FP, 0, 0, 0);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Seek to beginning of track */
        rc = ckd_seek (dev, dev->ckdcurcyl, dev->ckdcurhead, &trkhdr, unitstat);
        if (rc < 0) break;

        /* Calculate number of bytes to write and set residual count */
        size = CKDDASD_TRKHDR_SIZE;
        num = (count < size) ? count : size;
    /* FIXME: what devices want 5 bytes, what ones want 7, and what
        ones want 11? Do this right when we figure that out */
        /* ISW20030819-1 Indicate WRHA performed */
        dev->ckdwrha=1;
        *residual = 0;

        /* Return normal status */
        *unitstat = CSW_CE | CSW_DE;

        break;

    case 0x1E:
    case 0x9E:
    /*---------------------------------------------------------------*/
    /* READ COUNT KEY AND DATA                                       */
    /*---------------------------------------------------------------*/
        /* For 3990, command reject if not preceded by Seek, Seek Cyl,
           Locate Record, Read IPL, or Recalibrate command */
        if (dev->ckd3990
            && dev->ckdseek == 0 && dev->ckdskcyl == 0
            && dev->ckdlocat == 0 && dev->ckdrdipl == 0
            && dev->ckdrecal == 0)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_2);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Check operation code if within domain of a Locate Record */
        if (dev->ckdlcount > 0)
        {
            if (!((dev->ckdloper & CKDOPER_CODE) == CKDOPER_RDDATA
                  || (dev->ckdloper & CKDOPER_CODE) == CKDOPER_RDANY
                  || (dev->ckdloper & CKDOPER_CODE) == CKDOPER_READ))
            {
                ckd_build_sense (dev, SENSE_CR, 0, 0,
                                FORMAT_0, MESSAGE_2);
                *unitstat = CSW_CE | CSW_DE | CSW_UC;
                break;
            }
        }

        /* Read next count field */
        rc = ckd_read_count (dev, code, &rechdr, unitstat);
        if (rc < 0) break;

        /* Calculate number of bytes to read and set residual count */
        size = CKDDASD_RECHDR_SIZE + dev->ckdcurkl + dev->ckdcurdl;
        num = (count < size) ? count : size;
        *residual = count - num;
        if (count < size) *more = 1;
        offset = CKDDASD_RECHDR_SIZE + dev->ckdcurkl;

        /* Copy count field to I/O buffer */
        memcpy (iobuf, &rechdr, CKDDASD_RECHDR_SIZE);

        /* Turn off track overflow flag in read record header */
        *iobuf &= 0x7F;

        /* Read key field */
        rc = ckd_read_key (dev, code,
                            iobuf + CKDDASD_RECHDR_SIZE, unitstat);
        if (rc < 0) break;

        /* Read data field */
        rc = ckd_read_data (dev, code,
                            iobuf + CKDDASD_RECHDR_SIZE + dev->ckdcurkl,
                            unitstat);
        if (rc < 0) break;

    /* If track overflow, keep reading */
    while (dev->ckdtrkof)
    {
        /* Advance to next track */
        rc = mt_advance (dev, unitstat);
        if (rc < 0) break;
    
        /* Read the first count field */
        rc = ckd_read_count (dev, code, &rechdr, unitstat);
        if (rc < 0) break;
    
        /* Skip the key field if present */
        if (dev->ckdcurkl > 0)
                dev->bufoff += dev->ckdcurkl;

        /* Set offset into buffer for this read */
        offset += num;

        /* Account for size of this overflow record */
        size = dev->ckdcurdl;
        num = (*residual < size) ? *residual : size;
        if (*residual < size) *more = 1;
        else *more = 0;
        *residual -= num;
    
        /* Read the next data field */
        rc = ckd_read_data (dev, code, iobuf+offset, unitstat);
        if (rc < 0) break;
    }

    /* Bail out if track overflow produced I/O error */
    if (rc < 0) break;

        /* Save size and offset of data not used by this CCW */
        dev->ckdrem = size - num;
        dev->ckdpos = num;

        /* Return unit exception if data length is zero */
        if (dev->ckdcurdl == 0)
            *unitstat = CSW_CE | CSW_DE | CSW_UX;
        else
            *unitstat = CSW_CE | CSW_DE;

        break;

    case 0x5E:
    /*---------------------------------------------------------------*/
    /* READ MULTIPLE COUNT KEY AND DATA                              */
    /*---------------------------------------------------------------*/
        /* For 3990, command reject if not preceded by Seek, Seek Cyl,
           Locate Record, Read IPL, or Recalibrate */
        if (dev->ckd3990
            && dev->ckdseek == 0 && dev->ckdskcyl == 0
            && dev->ckdlocat == 0 && dev->ckdrdipl == 0
            && dev->ckdrecal == 0)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_2);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Command reject if within the domain of a Locate Record */
        if (dev->ckdlcount > 0)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_2);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Read records into the I/O buffer until end of track */
        for (size = 0; ; )
        {
            /* Read next count field */
            rc = ckd_read_count (dev, code, &rechdr, unitstat);
            if (rc < 0) break;

            /* Exit if end of track marker was read */
            if (memcmp (&rechdr, eighthexFF, 8) == 0)
                break;

            /* Copy count field to I/O buffer */
            memcpy (iobuf + size, &rechdr, CKDDASD_RECHDR_SIZE);
            size += CKDDASD_RECHDR_SIZE;

            /* Turn off track overflow flag */
            *(iobuf + size) &= 0x7F;

            /* Read key field */
            rc = ckd_read_key (dev, code, iobuf + size, unitstat);
            if (rc < 0) break;
            size += dev->ckdcurkl;

            /* Read data field */
            rc = ckd_read_data (dev, code, iobuf + size, unitstat);
            if (rc < 0) break;
            size += dev->ckdcurdl;

        } /* end for(size) */

        /* Set the residual count */
        num = (count < size) ? count : size;
        *residual = count - num;
        if (count < size) *more = 1;

        /* Save size and offset of data not used by this CCW */
        dev->ckdrem = size - num;
        dev->ckdpos = num;

        /* Return normal status */
        *unitstat = CSW_CE | CSW_DE;

        break;

    case 0xDE:
    /*---------------------------------------------------------------*/
    /* READ TRACK                                                    */
    /*---------------------------------------------------------------*/
        /* Command reject if not within the domain of a Locate Record
           that specifies a read tracks operation */
        if (dev->ckdlcount == 0
            || (dev->ckdloper & CKDOPER_CODE) != CKDOPER_RDTRKS)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_2);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Command reject if not chained from a Locate Record
           command or from another Read Track command */
        if (chained == 0
            || (prevcode != 0x47 && prevcode != 0xDE))
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_2);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Advance to next track if chained from previous read track */
        if (prevcode == 0xDE)
        {
            rc = mt_advance (dev, unitstat);
            if (rc < 0) break;
        }

        /* Read each record on the track into the I/O buffer */
        for (size = 0; ; )
        {
            /* Read next count field */
            rc = ckd_read_count (dev, code, &rechdr, unitstat);
            if (rc < 0) break;

            /* Copy count field to I/O buffer */
            memcpy (iobuf + size, &rechdr, CKDDASD_RECHDR_SIZE);
            size += CKDDASD_RECHDR_SIZE;

            /* Turn off track overflow flag */
            *(iobuf+size) &= 0x7F;

            /* Exit if end of track marker was read */
            if (memcmp (&rechdr, eighthexFF, 8) == 0)
                break;

            /* Read key field */
            rc = ckd_read_key (dev, code, iobuf + size, unitstat);
            if (rc < 0) break;
            size += dev->ckdcurkl;

            /* Read data field */
            rc = ckd_read_data (dev, code, iobuf + size, unitstat);
            if (rc < 0) break;
            size += dev->ckdcurdl;

        } /* end for(size) */

        /* Set the residual count */
        num = (count < size) ? count : size;
        *residual = count - num;
        if (count < size) *more = 1;

        /* Save size and offset of data not used by this CCW */
        dev->ckdrem = size - num;
        dev->ckdpos = num;

        /* Return normal status */
        *unitstat = CSW_CE | CSW_DE;

        break;

    case 0x27: /* SEEK AND SET SECTOR (Itel 7330 controller only) */
/*  case 0x27:    Perform Subsystem Function (3990)               */
    case 0x07: /* SEEK */
    case 0x0B: /* SEEK CYLINDER */
    case 0x1B: /* SEEK HEAD */
    /*---------------------------------------------------------------*/
    /* SEEK                                                          */
    /* PERFORM SUBSYSTEM FUNCTION                                    */
    /*---------------------------------------------------------------*/

        /* Process Perform Subsystem Function - Set Special Intercept */
        if(code == 0x27 && dev->ckd3990)
        {
#if 0
            /* Command reject if SSI active */
            if(dev->ckdssi)
            {
                /* Reset SSI condition */
                dev->ckdssi = 0;
                ckd_build_sense (dev, SENSE_CR, 0, 0,
                                FORMAT_0, MESSAGE_F);
                *unitstat = CSW_CE | CSW_DE | CSW_UC;
                break;
            }
#endif

            /* Command reject if within the domain of a Locate Record,
               or indeed if preceded by any command at all apart from
               Suspend Multipath Reconnection */
            if (dev->ckdlcount > 0
                || ccwseq > 1
                || (chained && prevcode != 0x5B))
            {
                ckd_build_sense (dev, SENSE_CR, 0, 0,
                                FORMAT_0, MESSAGE_2);
                *unitstat = CSW_CE | CSW_DE | CSW_UC;
                break;
            }

            /* Command reject if not Set Special Intercept order, 
               with zero flag byte */
            if (iobuf[0] != 0x1B || iobuf[1] != 0x00)
            {
                ckd_build_sense (dev, SENSE_CR, 0, 0,
                                FORMAT_0, MESSAGE_4);
                *unitstat = CSW_CE | CSW_DE | CSW_UC;
                break;
            }

            /* Command reject if any command is chained from this command */
            if (flags & CCW_FLAGS_CC)
            {
                ckd_build_sense (dev, SENSE_CR, 0, 0,
                                FORMAT_0, MESSAGE_2);
                *unitstat = CSW_CE | CSW_DE | CSW_UC;
                break;
            }

            /* Command reject if count is less than 2 */
            if (count < 2)
            {
                ckd_build_sense (dev, SENSE_CR, 0, 0,
                                FORMAT_0, MESSAGE_3);
                *unitstat = CSW_CE | CSW_DE | CSW_UC;
                break;
            }

            /* Mark Set Special Intercept inactive */
            dev->ckdssi = 1;

            /* Set residual count */
            num = (count < 2) ? count : 2;
            *residual = count - num;

            /* No further processing required */

            /* Return normal status */
            *unitstat = CSW_CE | CSW_DE;

            break;
        }
     

        /* Command reject if within the domain of a Locate Record */
        if (dev->ckdlcount > 0)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_2);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* For 3990, command reject if Seek Head not preceded by Seek,
           Seek Cylinder, Locate Record, Read IPL, or Recalibrate */
        if (code == 0x1B && dev->ckd3990
            && dev->ckdseek == 0 && dev->ckdskcyl == 0
            && dev->ckdlocat == 0 && dev->ckdrdipl == 0
            && dev->ckdrecal == 0)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_2);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* File protected if file mask does not allow requested seek */
        if (((code == 0x07 || code == 0x27)
            && (dev->ckdfmask & CKDMASK_SKCTL) != CKDMASK_SKCTL_ALLSKR)
           || (code == 0x0B
            && (dev->ckdfmask & CKDMASK_SKCTL) != CKDMASK_SKCTL_ALLSKR
            && (dev->ckdfmask & CKDMASK_SKCTL) != CKDMASK_SKCTL_CYLHD)
           || (code == 0x1B
            && (dev->ckdfmask & CKDMASK_SKCTL) == CKDMASK_SKCTL_INHSMT))
        {
            ckd_build_sense (dev, 0, SENSE1_FP, 0, 0, 0);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Set residual count */
        num = (count < 6) ? count : 6;
        *residual = count - num;

        /* Command reject if count is less than 6 */
        if (count < 6)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_3);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Extract the BBCCHH seek address from the I/O buffer */
        bin = (iobuf[0] << 8) | iobuf[1];
        cyl = (iobuf[2] << 8) | iobuf[3];
        head = (iobuf[4] << 8) | iobuf[5];

        /* For Seek Head, use the current cylinder number */
        if (code == 0x1B)
            cyl = dev->ckdcurcyl;

        /* Command reject if seek address is invalid */
        if (bin != 0 || cyl >= dev->ckdcyls || head >= dev->ckdheads)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_4);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* File protected if outside defined extent */
        if (dev->ckdxtdef
            && (cyl < dev->ckdxbcyl || cyl > dev->ckdxecyl
                || (cyl == dev->ckdxbcyl && head < dev->ckdxbhead)
                || (cyl == dev->ckdxecyl && head > dev->ckdxehead)))
        {
            ckd_build_sense (dev, 0, SENSE1_FP, 0, 0, 0);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Seek to specified cylinder and head */
        rc = ckd_seek (dev, cyl, head, &trkhdr, unitstat);
        if (rc < 0) break;

        /* Set command processed flag */
        dev->ckdseek = 1;

        /* Return normal status */
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x13:
    /*---------------------------------------------------------------*/
    /* RECALIBRATE                                                   */
    /*---------------------------------------------------------------*/
        /* Command reject if recalibrate is issued to a 3390 */
        if (dev->devtype == 0x3390)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_1);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Command reject if within the domain of a Locate Record */
        if (dev->ckdlcount > 0)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_2);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* File protected if the file mask does not allow recalibrate,
           or if the file mask specifies diagnostic authorization */
        if ((dev->ckdfmask & CKDMASK_SKCTL) != CKDMASK_SKCTL_ALLSKR
            || (dev->ckdfmask & CKDMASK_AAUTH) == CKDMASK_AAUTH_DIAG)
        {
            ckd_build_sense (dev, 0, SENSE1_FP, 0, 0, 0);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* File protected if cyl 0 head 0 is outside defined extent */
        if (dev->ckdxtdef
            && (dev->ckdxbcyl > 0 || dev->ckdxbhead > 0))
        {
            ckd_build_sense (dev, 0, SENSE1_FP, 0, 0, 0);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Seek to cylinder 0 head 0 */
        rc = ckd_seek (dev, 0, 0, &trkhdr, unitstat);
        if (rc < 0) break;

        /* Set command processed flag */
        dev->ckdrecal = 1;

        /* Return normal status */
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x1F:
    /*---------------------------------------------------------------*/
    /* SET FILE MASK                                                 */
    /*---------------------------------------------------------------*/
        /* Command reject if preceded by a Define Extent or
           Set File Mask, or within the domain of a Locate Record */
        if (dev->ckdxtdef || dev->ckdsetfm || dev->ckdlcount > 0)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_2);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Set residual count */
        num = (count < 1) ? count : 1;
        *residual = count - num;

        /* Command reject if count is less than 1 */
        if (count < 1)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_3);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Extract the file mask from the I/O buffer */
        dev->ckdfmask = iobuf[0];
        DEVTRACE(_("HHCDA054I set file mask %2.2X\n"), dev->ckdfmask);

        /* Command reject if file mask is invalid */
        if ((dev->ckdfmask & CKDMASK_RESV) != 0)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_4);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Set command processed flag */
        dev->ckdsetfm = 1;

        /* Return normal status */
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x22:
    /*---------------------------------------------------------------*/
    /* READ SECTOR                                                   */
    /*---------------------------------------------------------------*/
        /* Command reject if non-RPS device */
        if (dev->ckdtab->sectors == 0)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_1);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Command reject if within the domain of a Locate Record */
        if (dev->ckdlcount > 0)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_2);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Set residual count */
        num = (count < 1) ? count : 1;
        *residual = count - num;
        if (count < 1) *more = 1;

        /* Return sector number in I/O buffer */
        iobuf[0] = 0;

        /* Return normal status */
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x23:
    /*---------------------------------------------------------------*/
    /* SET SECTOR                                                    */
    /*---------------------------------------------------------------*/
        /* Command reject if non-RPS device */
        if (dev->ckdtab->sectors == 0)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_1);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Command reject if within the domain of a Locate Record */
        if (dev->ckdlcount > 0)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_2);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* For 3990, command reject if not preceded by Seek, Seek Cyl,
           Locate Record, Read IPL, or Recalibrate command */
        if (dev->ckd3990
            && dev->ckdseek == 0 && dev->ckdskcyl == 0
            && dev->ckdlocat == 0 && dev->ckdrdipl == 0
            && dev->ckdrecal == 0)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_2);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Set residual count */
        num = (count < 1) ? count : 1;
        *residual = count - num;

        /* Command reject if count is less than 1 */
        if (count < 1)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_3);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Return normal status */
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x29: case 0xA9: /* SEARCH KEY EQUAL */
    case 0x49: case 0xC9: /* SEARCH KEY HIGH */
    case 0x69: case 0xE9: /* SEARCH KEY EQUAL OR HIGH */
    /*---------------------------------------------------------------*/
    /* SEARCH KEY                                                    */
    /*---------------------------------------------------------------*/
        /* For 3990, command reject if not preceded by Seek, Seek Cyl,
           Locate Record, Read IPL, or Recalibrate command */
        if (dev->ckd3990
            && dev->ckdseek == 0 && dev->ckdskcyl == 0
            && dev->ckdlocat == 0 && dev->ckdrdipl == 0
            && dev->ckdrecal == 0)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_2);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Command reject if within the domain of a Locate Record */
        if (dev->ckdlcount > 0)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_2);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Read next key */
        rc = ckd_read_key (dev, code, key, unitstat);
        if (rc < 0) break;

        /* Calculate number of compare bytes and set residual count */
        num = (count < dev->ckdcurkl) ? count : dev->ckdcurkl;
        *residual = count - num;

        /* Nothing to compare if key length is zero */
        if (dev->ckdcurkl == 0)
        {
            *unitstat = CSW_CE | CSW_DE;
            break;
        }

        /* Compare key with search argument */
        rc = memcmp(key, iobuf, num);

        /* Return status modifier if compare result matches */
        if (((code & 0x20) && (rc == 0))
            || ((code & 0x40) && (rc > 0)))
            *unitstat = CSW_SM | CSW_CE | CSW_DE;
        else
            *unitstat = CSW_CE | CSW_DE;

#ifdef OPTION_CKD_KEY_TRACING
        /* If the search was successful, trace the first 8 bytes of
           the key, which will usually be a dataset name or member
           name and can provide useful debugging information */
        if ((*unitstat & CSW_SM) && dev->ckdkeytrace
            && isprint(guest_to_host(iobuf[0])))
        {
            BYTE module[45]; int i;
            for (i=0; i < (ssize_t)sizeof(module)-1 && i < num; i++)
                module[i] = guest_to_host(iobuf[i]);
            module[i] = '\0';
            logmsg (_("HHCDA055I search key %s\n"), module);
        }
#endif /*OPTION_CKD_KEY_TRACING*/

        /* Set flag if entire key was equal for SEARCH KEY EQUAL */
        if (rc == 0 && num == dev->ckdcurkl && (code & 0x7F) == 0x29)
            dev->ckdkyeq = 1;
        else
            dev->ckdkyeq = 0;

        break;

    case 0x31: case 0xB1: /* SEARCH ID EQUAL */
    case 0x51: case 0xD1: /* SEARCH ID HIGH */
    case 0x71: case 0xF1: /* SEARCH ID EQUAL OR HIGH */
    /*---------------------------------------------------------------*/
    /* SEARCH ID                                                     */
    /*---------------------------------------------------------------*/
        /* For 3990, command reject if not preceded by Seek, Seek Cyl,
           Locate Record, Read IPL, or Recalibrate command */
        if (dev->ckd3990
            && dev->ckdseek == 0 && dev->ckdskcyl == 0
            && dev->ckdlocat == 0 && dev->ckdrdipl == 0
            && dev->ckdrecal == 0)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_2);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Command reject if within the domain of a Locate Record */
        if (dev->ckdlcount > 0)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_2);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Read next count field */
        rc = ckd_read_count (dev, code, &rechdr, unitstat);
        if (rc < 0) break;

        /* Calculate number of compare bytes and set residual count */
        num = (count < 5) ? count : 5;
        *residual = count - num;

    /* Turn off track overflow flag in record header if present */
    rechdr.cyl[0] &= 0x7F;

        /* Compare count with search argument */
        rc = memcmp(&rechdr, iobuf, num);

        /* Return status modifier if compare result matches */
        if (((code & 0x20) && (rc == 0))
            || ((code & 0x40) && (rc > 0)))
            *unitstat = CSW_SM | CSW_CE | CSW_DE;
        else
            *unitstat = CSW_CE | CSW_DE;

        /* Set flag if entire id compared equal for SEARCH ID EQUAL */
        if (rc == 0 && num == 5 && (code & 0x7F) == 0x31)
            dev->ckdideq = 1;
        else
            dev->ckdideq = 0;

        break;

    case 0x39:
    case 0xB9:
    /*---------------------------------------------------------------*/
    /* SEARCH HOME ADDRESS EQUAL                                     */
    /*---------------------------------------------------------------*/
        /* For 3990, command reject if not preceded by Seek, Seek Cyl,
           Locate Record, Read IPL, or Recalibrate command */
        if (dev->ckd3990
            && dev->ckdseek == 0 && dev->ckdskcyl == 0
            && dev->ckdlocat == 0 && dev->ckdrdipl == 0
            && dev->ckdrecal == 0)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_2);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Command reject if within the domain of a Locate Record */
        if (dev->ckdlcount > 0)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_2);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* For multitrack operation, advance to next track */
        if (code & 0x80)
        {
            rc = mt_advance (dev, unitstat);
            if (rc < 0) break;
        }

        /* Seek to beginning of track */
        rc = ckd_seek (dev, dev->ckdcurcyl, dev->ckdcurhead, &trkhdr, unitstat);
        if (rc < 0) break;

        /* Calculate number of compare bytes and set residual count */
        num = (count < 4) ? count : 4;
        *residual = count - num;

        /* Compare CCHH portion of track header with search argument */
        rc = memcmp(&(trkhdr.cyl), iobuf, num);

        /* Return status modifier if compare result matches */
        if (rc == 0)
            *unitstat = CSW_SM | CSW_CE | CSW_DE;
        else
            *unitstat = CSW_CE | CSW_DE;

        /* Set flag if entire home address compared equal */
        if (rc == 0 && num == 4)
            dev->ckdhaeq = 1;
        else
            dev->ckdhaeq = 0;

        break;

    case 0x05:
    /*---------------------------------------------------------------*/
    /* WRITE DATA                                                    */
    /*---------------------------------------------------------------*/
        /* Command reject if the current track is in the DSF area */
        if (dev->ckdcurcyl >= dev->ckdcyls)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_2);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Command reject if not within the domain of a Locate Record
           and not preceded by either a Search ID Equal or Search Key
           Equal that compared equal on all bytes */
           /*INCOMPLETE*/ /*Write CKD allows intervening Read/Write
             key and data commands, Write Data does not!!! Rethink
             the handling of these flags*/
        if (dev->ckdlcount == 0 && dev->ckdideq == 0
            && dev->ckdkyeq == 0)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_2);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Command reject if file mask inhibits all write commands */
        if ((dev->ckdfmask & CKDMASK_WRCTL) == CKDMASK_WRCTL_INHWRT)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_2);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Check operation code if within domain of a Locate Record */
        if (dev->ckdlcount > 0)
        {
            if (!(((dev->ckdloper & CKDOPER_CODE) == CKDOPER_WRITE
                       && dev->ckdlcount ==
                            (dev->ckdlaux & CKDLAUX_RDCNTSUF) ? 2 : 1)
                  || (dev->ckdloper & CKDOPER_CODE) == CKDOPER_WRTTRK))
            {
                ckd_build_sense (dev, SENSE_CR, 0, 0,
                                FORMAT_0, MESSAGE_2);
                *unitstat = CSW_CE | CSW_DE | CSW_UC;
                break;
            }

            /* If not operating in CKD conversion mode, check that the
               data length is equal to the transfer length factor,
               except when writing a R0 data area under the control
               of a Locate Record Write Track operation, in which
               case a transfer length factor of 8 is used instead */
            if ((dev->ckdxgattr & CKDGATR_CKDCONV) == 0)
            {
                if ((dev->ckdloper & CKDOPER_CODE) == CKDOPER_WRTTRK
                    && dev->ckdcurrec == 0)
                    num = 8;
                else
                    num = dev->ckdltranlf;

                if (dev->ckdcurdl != num)
                {
                    /* Unit check with invalid track format */
                    ckd_build_sense (dev, 0, SENSE1_ITF, 0, 0, 0);
                    *unitstat = CSW_CE | CSW_DE | CSW_UC;
                    break;
                }
            }
        } /* end if(ckdlcount) */

        /* If data length is zero, terminate with unit exception */
        if (dev->ckdcurdl == 0)
        {
            *unitstat = CSW_CE | CSW_DE | CSW_UX;
            break;
        }

        /* Calculate number of bytes written and set residual count */
        size = dev->ckdcurdl;
        num = (count < size) ? count : size;
        *residual = count - num;

        /* Write data */
        rc = ckd_write_data (dev, iobuf, num, unitstat);
        if (rc < 0) break;

    /* If track overflow, keep writing */
        offset = 0;
    while (dev->ckdtrkof)
    {
        /* Advance to next track */
        rc = mt_advance (dev, unitstat);
        if (rc < 0) break;
    
        /* Read the first count field */
        rc = ckd_read_count (dev, code, &rechdr, unitstat);
        if (rc < 0) break;
    
            /* Set offset into buffer for this write */
            offset += size;

        /* Account for size of this overflow record */
        size = dev->ckdcurdl;
        num = (*residual < size) ? *residual : size;
        *residual -= num;
    
        /* Write the next data field */
            rc = ckd_write_data (dev, iobuf+offset, num, unitstat);
        if (rc < 0) break;
    }

    /* Bail out if track overflow produced I/O error */
    if (rc < 0) break;

        /* Return normal status */
        *unitstat = CSW_CE | CSW_DE;

        break;

    case 0x85:
    /*---------------------------------------------------------------*/
    /* WRITE UPDATE DATA                                             */
    /*---------------------------------------------------------------*/
        /* Command reject if not within the domain of a Locate Record
           that specifies the Write Data operation code */
        if (dev->ckdlcount == 0
            || (dev->ckdloper & CKDOPER_CODE) != CKDOPER_WRITE)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_2);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Orient to next user record count field */
        if (dev->ckdorient != CKDORIENT_COUNT
            || dev->ckdcurrec == 0)
        {
            /* Read next count field */
            rc = ckd_read_count (dev, code, &rechdr, unitstat);
            if (rc < 0) break;
        }

        /* If not operating in CKD conversion mode, check that the
           data length is equal to the transfer length factor */
        if ((dev->ckdxgattr & CKDGATR_CKDCONV) == 0)
        {
            if (dev->ckdcurdl != dev->ckdltranlf)
            {
                /* Unit check with invalid track format */
                ckd_build_sense (dev, 0, SENSE1_ITF, 0, 0, 0);
                *unitstat = CSW_CE | CSW_DE | CSW_UC;
                break;
            }
        }

        /* If data length is zero, terminate with unit exception */
        if (dev->ckdcurdl == 0)
        {
            *unitstat = CSW_CE | CSW_DE | CSW_UX;
            break;
        }

        /* Calculate number of bytes written and set residual count */
        size = dev->ckdcurdl;
        num = (count < size) ? count : size;
        *residual = count - num;

        /* Write data */
        rc = ckd_write_data (dev, iobuf, num, unitstat);
        if (rc < 0) break;

    /* If track overflow, keep writing */
        offset = 0;
    while (dev->ckdtrkof)
    {
        /* Advance to next track */
        rc = mt_advance (dev, unitstat);
        if (rc < 0) break;
    
        /* Read the first count field */
        rc = ckd_read_count (dev, code, &rechdr, unitstat);
        if (rc < 0) break;
    
            /* Set offset into buffer for this write */
            offset += size;

        /* Account for size of this overflow record */
        size = dev->ckdcurdl;
        num = (*residual < size) ? *residual : size;
        *residual -= num;
    
        /* Write the next data field */
            rc = ckd_write_data (dev, iobuf+offset, num, unitstat);
        if (rc < 0) break;
    }

    /* Bail out if track overflow produced I/O error */
    if (rc < 0) break;

        /* Return normal status */
        *unitstat = CSW_CE | CSW_DE;

        break;

    case 0x0D:
    /*---------------------------------------------------------------*/
    /* WRITE KEY AND DATA                                            */
    /*---------------------------------------------------------------*/
        /* Command reject if the current track is in the DSF area */
        if (dev->ckdcurcyl >= dev->ckdcyls)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_2);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Command reject if not within the domain of a Locate Record
           and not preceded by a Search ID Equal that compared equal
           on all bytes */
           /*INCOMPLETE*/ /*Write CKD allows intervening Read/Write
             key and data commands, Write Key Data does not!!! Rethink
             the handling of these flags*/
        if (dev->ckdlcount == 0 && dev->ckdideq == 0)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_2);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Command reject if file mask inhibits all write commands */
        if ((dev->ckdfmask & CKDMASK_WRCTL) == CKDMASK_WRCTL_INHWRT)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_2);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Check operation code if within domain of a Locate Record */
        if (dev->ckdlcount > 0)
        {
            if (!(((dev->ckdloper & CKDOPER_CODE) == CKDOPER_WRITE
                       && dev->ckdlcount ==
                            (dev->ckdlaux & CKDLAUX_RDCNTSUF) ? 2 : 1)
                  || (dev->ckdloper & CKDOPER_CODE) == CKDOPER_WRTTRK))
            {
                ckd_build_sense (dev, SENSE_CR, 0, 0,
                                FORMAT_0, MESSAGE_2);
                *unitstat = CSW_CE | CSW_DE | CSW_UC;
                break;
            }

            /* If not operating in CKD conversion mode, check that the
               key + data length equals the transfer length factor */
            if ((dev->ckdxgattr & CKDGATR_CKDCONV) == 0
                && dev->ckdcurkl + dev->ckdcurdl != dev->ckdltranlf)
            {
                /* Unit check with invalid track format */
                ckd_build_sense (dev, 0, SENSE1_ITF, 0, 0, 0);
                *unitstat = CSW_CE | CSW_DE | CSW_UC;
                break;
            }
        } /* end if(ckdlcount) */

        /* If data length is zero, terminate with unit exception */
        if (dev->ckdcurdl == 0)
        {
            *unitstat = CSW_CE | CSW_DE | CSW_UX;
            break;
        }

        /* Calculate number of bytes written and set residual count */
        size = dev->ckdcurkl + dev->ckdcurdl;
        num = (count < size) ? count : size;
        *residual = count - num;

        /* Write key and data */
        rc = ckd_write_kd (dev, iobuf, num, unitstat);
        if (rc < 0) break;

    /* If track overflow, keep writing */
        offset = dev->ckdcurkl;
    while (dev->ckdtrkof)
    {
        /* Advance to next track */
        rc = mt_advance (dev, unitstat);
        if (rc < 0) break;
    
        /* Read the first count field */
        rc = ckd_read_count (dev, code, &rechdr, unitstat);
        if (rc < 0) break;
    
            /* Set offset into buffer for this write */
            offset += size;

        /* Account for size of this overflow record */
        size = dev->ckdcurdl;
        num = (*residual < size) ? *residual : size;
        *residual -= num;
    
        /* Write the next data field */
            rc = ckd_write_data (dev, iobuf+offset, num, unitstat);
        if (rc < 0) break;
    }

    /* Bail out if track overflow produced I/O error */
    if (rc < 0) break;

        /* Return normal status */
        *unitstat = CSW_CE | CSW_DE;

        break;

    case 0x8D:
    /*---------------------------------------------------------------*/
    /* WRITE UPDATE KEY AND DATA                                     */
    /*---------------------------------------------------------------*/
        /* Command reject if not within the domain of a Locate Record
           that specifies the Write Data operation code */
        if (dev->ckdlcount == 0
            || (dev->ckdloper & CKDOPER_CODE) != CKDOPER_WRITE)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_2);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Orient to next user record count field */
        if (dev->ckdorient != CKDORIENT_COUNT
            || dev->ckdcurrec == 0)
        {
            /* Read next count field */
            rc = ckd_read_count (dev, code, &rechdr, unitstat);
            if (rc < 0) break;
        }

        /* If not operating in CKD conversion mode, check that the
           data length is equal to the transfer length factor */
        if ((dev->ckdxgattr & CKDGATR_CKDCONV) == 0)
        {
            if ((dev->ckdcurkl + dev->ckdcurdl) != dev->ckdltranlf)
            {
                /* Unit check with invalid track format */
                ckd_build_sense (dev, 0, SENSE1_ITF, 0, 0, 0);
                *unitstat = CSW_CE | CSW_DE | CSW_UC;
                break;
            }
        }

        /* If data length is zero, terminate with unit exception */
        if (dev->ckdcurdl == 0)
        {
            *unitstat = CSW_CE | CSW_DE | CSW_UX;
            break;
        }

        /* Calculate number of bytes written and set residual count */
        size = dev->ckdcurkl + dev->ckdcurdl;
        num = (count < size) ? count : size;
        *residual = count - num;

        /* Write key and data */
        rc = ckd_write_kd (dev, iobuf, num, unitstat);
        if (rc < 0) break;

    /* If track overflow, keep writing */
        offset = dev->ckdcurkl;
    while (dev->ckdtrkof)
    {
        /* Advance to next track */
        rc = mt_advance (dev, unitstat);
        if (rc < 0) break;
    
        /* Read the first count field */
        rc = ckd_read_count (dev, code, &rechdr, unitstat);
        if (rc < 0) break;
    
            /* Set offset into buffer for this write */
            offset += size;

        /* Account for size of this overflow record */
        size = dev->ckdcurdl;
        num = (*residual < size) ? *residual : size;
        *residual -= num;
    
        /* Write the next data field */
            rc = ckd_write_data (dev, iobuf+offset, num, unitstat);
        if (rc < 0) break;
    }

    /* Bail out if track overflow produced I/O error */
    if (rc < 0) break;

        /* Return normal status */
        *unitstat = CSW_CE | CSW_DE;

        break;

    case 0x11:
    /*---------------------------------------------------------------*/
    /* ERASE                                                         */
    /*---------------------------------------------------------------*/
        /* Command reject if the current track is in the DSF area */
        if (dev->ckdcurcyl >= dev->ckdcyls)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_2);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Command reject if not within the domain of a Locate Record
           and not preceded by either a Search ID Equal or Search Key
           Equal that compared equal on all bytes, or a Write R0 or
           Write CKD not within the domain of a Locate Record */
        if (dev->ckdlcount == 0 && dev->ckdideq == 0
            && dev->ckdkyeq == 0 && dev->ckdwckd == 0)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_2);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Command reject if file mask does not permit Write CKD */
        if ((dev->ckdfmask & CKDMASK_WRCTL) != CKDMASK_WRCTL_ALLWRT
            && (dev->ckdfmask & CKDMASK_WRCTL) != CKDMASK_WRCTL_INHWR0)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_2);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Check operation code if within domain of a Locate Record */
        if (dev->ckdlcount > 0)
        {
            if ((dev->ckdloper & CKDOPER_CODE) != CKDOPER_WRTTRK)
            {
                ckd_build_sense (dev, SENSE_CR, 0, 0,
                                FORMAT_0, MESSAGE_2);
                *unitstat = CSW_CE | CSW_DE | CSW_UC;
                break;
            }
        }

        /* Write end of track marker */
        rc = ckd_erase (dev, iobuf, count, &size, unitstat);
        if (rc < 0) break;

        /* Calculate number of bytes used and set residual count */
        num = (count < size) ? count : size;
        *residual = count - num;

        /* Return normal status */
        *unitstat = CSW_CE | CSW_DE;

        break;

    case 0x15:
    /*---------------------------------------------------------------*/
    /* WRITE RECORD ZERO                                             */
    /*---------------------------------------------------------------*/
        /* Command reject if the current track is in the DSF area */
        if (dev->ckdcurcyl >= dev->ckdcyls)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_2);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            logmsg("DEBUG : WR0 OUTSIDE PACK\n");
            break;
        }

        /* Command reject if not within the domain of a Locate Record
           and not preceded by either a Search Home Address that
           compared equal on all 4 bytes, or a Write Home Address not
           within the domain of a Locate Record */
        /* ISW20030819-1 : Added check for previously issued WRHA */
        if (dev->ckdlcount == 0 && dev->ckdhaeq == 0 && dev->ckdwrha==0)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_2);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            logmsg("DEBUG : WR0 CASE 2\n");
            break;
        }

        /* Command reject if file mask does not permit Write R0 */
        if ((dev->ckdfmask & CKDMASK_WRCTL) != CKDMASK_WRCTL_ALLWRT)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_2);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            logmsg("DEBUG : WR0 BAD FM\n");
            break;
        }

        /* Check operation code if within domain of a Locate Record */
        if (dev->ckdlcount > 0)
        {
            if (!((dev->ckdloper & CKDOPER_CODE) == CKDOPER_FORMAT
                    && ((dev->ckdloper & CKDOPER_ORIENTATION)
                                == CKDOPER_ORIENT_HOME
                          || (dev->ckdloper & CKDOPER_ORIENTATION)
                                == CKDOPER_ORIENT_INDEX
                       )))
            {
                ckd_build_sense (dev, SENSE_CR, 0, 0,
                                FORMAT_0, MESSAGE_2);
                *unitstat = CSW_CE | CSW_DE | CSW_UC;
                logmsg("DEBUG : LOC REC 2\n");
                break;
            }
        }

        /* Write R0 count key and data */
        rc = ckd_write_ckd (dev, iobuf, count, unitstat, 0);
        if (rc < 0) break;

        /* Calculate number of bytes written and set residual count */
        size = CKDDASD_RECHDR_SIZE + dev->ckdcurkl + dev->ckdcurdl;
        num = (count < size) ? count : size;
        *residual = count - num;

        /* Return normal status */
        *unitstat = CSW_CE | CSW_DE;

        /* Set flag if Write R0 outside domain of a locate record */
        if (dev->ckdlcount == 0)
            dev->ckdwckd = 1;
        else
            dev->ckdwckd = 0;

        break;

    case 0x1D: /* WRITE CKD */
    case 0x01: /* WRITE SPECIAL CKD */
    /*---------------------------------------------------------------*/
    /* WRITE COUNT KEY AND DATA                                      */
    /*---------------------------------------------------------------*/
        /* Command reject if the current track is in the DSF area */
        if (dev->ckdcurcyl >= dev->ckdcyls)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_2);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Command reject if previous command was a Write R0 that
           assigned an alternate track - not implemented */

        /* Command reject if not within the domain of a Locate Record
           and not preceded by either a Search ID Equal or Search Key
           Equal that compared equal on all bytes, or a Write R0 or
           Write CKD not within the domain of a Locate Record */
        if (dev->ckdlcount == 0 && dev->ckdideq == 0
            && dev->ckdkyeq == 0 && dev->ckdwckd == 0)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_2);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Command reject if file mask does not permit Write CKD */
        if ((dev->ckdfmask & CKDMASK_WRCTL) != CKDMASK_WRCTL_ALLWRT
            && (dev->ckdfmask & CKDMASK_WRCTL) != CKDMASK_WRCTL_INHWR0)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_2);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Command reject if WRITE SPECIAL CKD to a 3380 or 3390 */
        if ((code == 0x01)
            && ((dev->devtype == 0x3380) || (dev->devtype == 0x3390)))
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_2);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Check operation code if within domain of a Locate Record */
        if (dev->ckdlcount > 0)
        {
            if (!((dev->ckdloper & CKDOPER_CODE) == CKDOPER_FORMAT
                  || (dev->ckdloper & CKDOPER_CODE) == CKDOPER_WRTTRK))
            {
                ckd_build_sense (dev, SENSE_CR, 0, 0,
                                FORMAT_0, MESSAGE_2);
                *unitstat = CSW_CE | CSW_DE | CSW_UC;
                break;
            }
        }

        /* Set track overflow flag if WRITE SPECIAL CKD */
        trk_ovfl = (code==0x01) ? 1 : 0;

        /* Write count key and data */
        rc = ckd_write_ckd (dev, iobuf, count, unitstat, trk_ovfl);
        if (rc < 0) break;

        /* Calculate number of bytes written and set residual count */
        size = CKDDASD_RECHDR_SIZE + dev->ckdcurkl + dev->ckdcurdl;
        num = (count < size) ? count : size;
        *residual = count - num;

        /* Return normal status */
        *unitstat = CSW_CE | CSW_DE;

        /* Set flag if Write CKD outside domain of a locate record */
        if (dev->ckdlcount == 0)
            dev->ckdwckd = 1;
        else
            dev->ckdwckd = 0;

        break;

    case 0x9D:
    /*---------------------------------------------------------------*/
    /* WRITE COUNT KEY AND DATA NEXT TRACK                           */
    /*---------------------------------------------------------------*/
        /* Command reject if not within the domain of a Locate Record
           that specifies a format write operation */
        if (dev->ckdlcount == 0
            || (dev->ckdloper & CKDOPER_CODE) != CKDOPER_FORMAT)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_2);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Command reject if not chained from a Write CKD or
           another Write CKD Next Track command */
        if (chained == 0
            || (prevcode != 0x1D && prevcode != 0x9D))
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_2);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Advance to next track */
        rc = mt_advance (dev, unitstat);
        if (rc < 0) break;

        /* Read the count field for record zero */
        rc = ckd_read_count (dev, code, &rechdr, unitstat);
        if (rc < 0) break;

        /* Write count key and data */
        rc = ckd_write_ckd (dev, iobuf, count, unitstat, 0);
        if (rc < 0) break;

        /* Calculate number of bytes written and set residual count */
        size = CKDDASD_RECHDR_SIZE + dev->ckdcurkl + dev->ckdcurdl;
        num = (count < size) ? count : size;
        *residual = count - num;

        /* Return normal status */
        *unitstat = CSW_CE | CSW_DE;

        break;

    case 0x47:
    /*---------------------------------------------------------------*/
    /* LOCATE RECORD                                                 */
    /*---------------------------------------------------------------*/
        /* Calculate residual byte count */
        num = (count < 16) ? count : 16;
        *residual = count - num;

        /* Control information length must be at least 16 bytes */
        if (count < 16)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_3);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Command reject if within the domain of a Locate Record,
           or not preceded by a Define Extent or Read IPL command */
        if (dev->ckdlcount > 0
            || (dev->ckdxtdef == 0 && dev->ckdrdipl == 0))
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_2);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Byte 0 contains the locate record operation byte */
        dev->ckdloper = iobuf[0];

        /* Validate the locate record operation code (bits 2-7) */
        if (!((dev->ckdloper & CKDOPER_CODE) == CKDOPER_ORIENT
              || (dev->ckdloper & CKDOPER_CODE) == CKDOPER_WRITE
              || (dev->ckdloper & CKDOPER_CODE) == CKDOPER_FORMAT
              || (dev->ckdloper & CKDOPER_CODE) == CKDOPER_RDDATA
              || (dev->ckdloper & CKDOPER_CODE) == CKDOPER_WRTTRK
              || (dev->ckdloper & CKDOPER_CODE) == CKDOPER_RDTRKS
              || (dev->ckdloper & CKDOPER_CODE) == CKDOPER_READ))
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_4);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Check for valid combination of orientation and opcode */
        if (   ((dev->ckdloper & CKDOPER_ORIENTATION)
                                        == CKDOPER_ORIENT_HOME
                && !((dev->ckdloper & CKDOPER_CODE) == CKDOPER_ORIENT
                  || (dev->ckdloper & CKDOPER_CODE) == CKDOPER_FORMAT
                  || (dev->ckdloper & CKDOPER_CODE) == CKDOPER_RDDATA
                  || (dev->ckdloper & CKDOPER_CODE) == CKDOPER_RDTRKS
                  || (dev->ckdloper & CKDOPER_CODE) == CKDOPER_READ))
            ||
               ((dev->ckdloper & CKDOPER_ORIENTATION)
                                        == CKDOPER_ORIENT_DATA
                && !((dev->ckdloper & CKDOPER_CODE) == CKDOPER_ORIENT
                  || (dev->ckdloper & CKDOPER_CODE) == CKDOPER_WRITE
                  || (dev->ckdloper & CKDOPER_CODE) == CKDOPER_RDDATA
                  || (dev->ckdloper & CKDOPER_CODE) == CKDOPER_READ))
            ||
               ((dev->ckdloper & CKDOPER_ORIENTATION)
                                        == CKDOPER_ORIENT_INDEX
                && !((dev->ckdloper & CKDOPER_CODE) == CKDOPER_FORMAT
                  || (dev->ckdloper & CKDOPER_CODE) == CKDOPER_READ))
            )
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_4);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Check for write operation on a read only disk */
        if ( (dev->ckdrdonly && !dev->ckdfakewr)
             &&  ((dev->ckdloper & CKDOPER_CODE) == CKDOPER_WRITE
               || (dev->ckdloper & CKDOPER_CODE) == CKDOPER_FORMAT
               || (dev->ckdloper & CKDOPER_CODE) == CKDOPER_WRTTRK)
           )
        {
            ckd_build_sense (dev, SENSE_EC, SENSE1_WRI, 0,
                            FORMAT_0, MESSAGE_4);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }


        /* Byte 1 contains the locate record auxiliary byte */
        dev->ckdlaux = iobuf[1];

        /* Validate the auxiliary byte */
        if ((dev->ckdlaux & CKDLAUX_RESV) != 0
            || ((dev->ckdlaux & CKDLAUX_RDCNTSUF)
                && !((dev->ckdloper & CKDOPER_CODE) == CKDOPER_WRITE
                  || (dev->ckdloper & CKDOPER_CODE) == CKDOPER_READ))
            )
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_4);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Byte 2 must contain zeroes */
        if (iobuf[2] != 0)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_4);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Byte 3 contains the locate record domain count */
        dev->ckdlcount = iobuf[3];

        /* Validate the locate record domain count */
        if (   ((dev->ckdloper & CKDOPER_CODE) == CKDOPER_ORIENT
                && dev->ckdlcount != 0)
            || ((dev->ckdloper & CKDOPER_CODE) != CKDOPER_ORIENT
                && dev->ckdlcount == 0)
            || ((dev->ckdlaux & CKDLAUX_RDCNTSUF)
                && dev->ckdlcount < 2)
            )
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_4);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Bytes 4-7 contain the seek address */
        cyl = (iobuf[4] << 8) | iobuf[5];
        head = (iobuf[6] << 8) | iobuf[7];

        /* Command reject if seek address is not valid */
        if (cyl >= dev->ckdcyls || head >= dev->ckdheads)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_4);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* File protect error if seek address is outside extent */
        if (cyl < dev->ckdxbcyl || cyl > dev->ckdxecyl
            || (cyl == dev->ckdxbcyl && head < dev->ckdxbhead)
            || (cyl == dev->ckdxecyl && head > dev->ckdxehead))
        {
            ckd_build_sense (dev, 0, SENSE1_FP, 0, 0, 0);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Bytes 8-12 contain the search argument */
        memcpy (cchhr, iobuf+8, 5);

        /* Byte 13 contains the sector number */
        sector = iobuf[13];

        /* Command reject if sector number is not valid */
        if (sector != 0xFF && sector >= dev->ckdtab->sectors)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_4);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Bytes 14-15 contain the transfer length factor */
        dev->ckdltranlf = (iobuf[14] << 8) | iobuf[15];

        /* Validate the transfer length factor */
        if (   ((dev->ckdlaux & CKDLAUX_TLFVALID) == 0
                && dev->ckdltranlf != 0)
            || ((dev->ckdlaux & CKDLAUX_TLFVALID)
                && dev->ckdltranlf == 0)
            || dev->ckdltranlf > dev->ckdxblksz)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_4);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* If transfer length factor is not supplied then use
           the blocksize from the define extent command */
        if ((dev->ckdlaux & CKDLAUX_TLFVALID) == 0)
            dev->ckdltranlf = dev->ckdxblksz;

        /* Seek to the required track */
        rc = ckd_seek (dev, cyl, head, &trkhdr, unitstat);
        if (rc < 0)
        {
            if (dev->syncio_retry) dev->ckdlcount = 0;
            break;
        }

        /* Set normal status */
        *unitstat = CSW_CE | CSW_DE;

        /* Perform search according to specified orientation */
        switch ((dev->ckdloper & CKDOPER_ORIENTATION)) {

        case CKDOPER_ORIENT_HOME:
            /* For home orientation, compare the search CCHH
               with the CCHH in the track header */
            if (memcmp (&(trkhdr.cyl), cchhr, 4) != 0)
            {
                ckd_build_sense (dev, 0, SENSE1_NRF, 0, 0, 0);
                *unitstat = CSW_CE | CSW_DE | CSW_UC;
            }
            break;

        case CKDOPER_ORIENT_COUNT:
        case CKDOPER_ORIENT_DATA:
            /* For count or data orientation, search the track
               for a count field matching the specified CCHHR */
            while (1)
            {
                /* Read next count field and exit at end of track
                   with sense data indicating no record found */
                rc = ckd_read_count (dev, code, &rechdr, unitstat);
                if (rc < 0) break;

                /* Turn off track overflow flag */
                rechdr.cyl[0] &= 0x7F;

                /* Compare the count field with the search CCHHR */
                if (memcmp (&rechdr, cchhr, 5) == 0)
                    break;

// NOTE: Code like this breaks VM mini-disks !!!
#if 0
                if (memcmp (&rechdr, cchhr, 4) != 0)
                {
    	            logmsg ("HHCDA999E wrong recordheader: cc hh r=%d %d %d,"
		                    "should be:cc hh r=%d %d %d\n",
                            (rechdr.cyl[0] << 8) | rechdr.cyl[1],
                            (rechdr.head[0] << 8) | rechdr.head[1],
                            rechdr.rec,
                            (cchhr[0] << 8) | cchhr[1],
                            (cchhr[2] << 8) | cchhr[3],
                            cchhr[4]);
                    break;
                }
#endif
            } /* end while */

        } /* end switch(CKDOPER_ORIENTATION) */

        /* Exit if search ended with error status */
        if (*unitstat != (CSW_CE | CSW_DE))
            break;

        /* Reorient past data if data orientation is specified */
        if ((dev->ckdloper & CKDOPER_ORIENTATION)
                        == CKDOPER_ORIENT_DATA)
        {
            /* Skip past key and data fields */
            dev->bufoff += dev->ckdcurkl + dev->ckdcurdl;

            /* Set the device orientation fields */
            dev->ckdrem = 0;
            dev->ckdorient = CKDORIENT_DATA;
        }

        /* Set locate record flag and return normal status */
        dev->ckdlocat = 1;
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
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_3);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Command reject if within the domain of a Locate Record, or
           preceded by Define Extent, Space Count, or Set File Mask,
           or (for 3390 only) preceded by Read IPL */
        if (dev->ckdlcount > 0
            || dev->ckdxtdef || dev->ckdspcnt || dev->ckdsetfm
            || (dev->ckdrdipl && dev->devtype == 0x3390))
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_2);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Bytes 0-1 contain the file mask and global attributes */
        dev->ckdfmask = iobuf[0];
        dev->ckdxgattr = iobuf[1];

        /* Validate the global attributes byte bits 0-1 */
        if ((dev->ckdxgattr & CKDGATR_ARCH) != CKDGATR_ARCH_ECKD)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_4);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Validate the file mask */
        if ((dev->ckdfmask & CKDMASK_RESV) != 0)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_4);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Bytes 2-3 contain the extent block size */
        dev->ckdxblksz = (iobuf[2] << 8) | iobuf[3];

        /* If extent block size is zero then use the maximum R0
           record length (as returned in device characteristics
           bytes 44 and 45) plus 8 */
        if (dev->ckdxblksz == 0)
            dev->ckdxblksz = dev->ckdtab->r0 + 8;

        /* Validate the extent block */
        if (dev->ckdxblksz > dev->ckdtab->r0 + 8)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_4);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Bytes 4-6 must contain zeroes */
        if (iobuf[4] != 0 || iobuf[5] != 0 || iobuf[6] != 0)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_4);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Bytes 8-11 contain the extent begin cylinder and head */
        dev->ckdxbcyl = (iobuf[8] << 8) | iobuf[9];
        dev->ckdxbhead = (iobuf[10] << 8) | iobuf[11];

        /* Bytes 12-15 contain the extent end cylinder and head */
        dev->ckdxecyl = (iobuf[12] << 8) | iobuf[13];
        dev->ckdxehead = (iobuf[14] << 8) | iobuf[15];

        /* Validate the extent description by checking that the
           ending track is not less than the starting track and
           that the extent does not exceed the device size */
        if (dev->ckdxecyl < dev->ckdxbcyl
            || (dev->ckdxecyl == dev->ckdxbcyl
                && dev->ckdxehead < dev->ckdxbhead)
//          || dev->ckdxecyl >= dev->ckdcyls
            || dev->ckdxbhead >= dev->ckdheads
            || dev->ckdxehead >= dev->ckdheads)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_4);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Set extent defined flag and return normal status */
        dev->ckdxtdef = 1;
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x64:
    /*---------------------------------------------------------------*/
    /* READ DEVICE CHARACTERISTICS                                   */
    /*---------------------------------------------------------------*/
        /* Command reject if within the domain of a Locate Record */
        if (dev->ckdlcount > 0)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_2);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Command reject if not 3380 or 3390 or 9345 */
        if ((dev->devtype != 0x3380)
         && (dev->devtype != 0x3390)
         && (dev->devtype != 0x9345))
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_2);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Calculate residual byte count */
        num = (count < dev->numdevchar) ? count : dev->numdevchar;
        *residual = count - num;
        if (count < dev->numdevchar) *more = 1;

        /* Copy device characteristics bytes to channel buffer */
        memcpy (iobuf, dev->devchar, num);

        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x5B:
    /*---------------------------------------------------------------*/
    /* SUSPEND MULTIPATH RECONNECTION                                */
    /*---------------------------------------------------------------*/
        /* Command reject if within the domain of a Locate Record
           or if chained from any other CCW */
        if (dev->ckdlcount > 0 || chained)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_2);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Return normal status */
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0xF3:
    /*---------------------------------------------------------------*/
    /* DIAGNOSTIC CONTROL                                            */
    /*---------------------------------------------------------------*/
        /* Command reject if SSI active */
        if(dev->ckdssi)
        {
            /* Mark Set Special Intercept inactive */
            dev->ckdssi = 0;
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_F);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Calculate residual byte count */
        num = (count < 4) ? count : 4;
        *residual = count - num;

        /* Control information length must be at least 4 bytes */
        if (count < 4)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_3);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Command reject if within the domain of a Locate Record */
        if (dev->ckdlcount > 0)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_2);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Command reject if byte 0 does not contain a valid
           subcommand code, or if bytes 2-3 are not zero */
        if (!(iobuf[0] == DIAGCTL_MAINT_QUERY
//            || iobuf[0] == DIAGCTL_MAINT_RESERVE
//            || iobuf[0] == DIAGCTL_MAINT_RELEASE
//            || iobuf[0] == DIAGCTL_INHIBIT_WRITE
//            || iobuf[0] == DIAGCTL_SET_GUAR_PATH
//            || iobuf[0] == DIAGCTL_ENABLE_WRITE
//            || iobuf[0] == DIAGCTL_3380_TC_MODE
//            || iobuf[0] == DIAGCTL_INIT_SUBSYS
//            || iobuf[0] == DIAGCTL_UNFENCE
//            || iobuf[0] == DIAGCTL_ACCDEV_UNKCOND
             ) || iobuf[2] != 0 || iobuf[3] != 0)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_4);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Command reject if file mask does not specify
           diagnostic or device support authorization */
        if ((dev->ckdfmask & CKDMASK_AAUTH) == CKDMASK_AAUTH_NORMAL)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_5);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Return normal status */
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x94:
    /*---------------------------------------------------------------*/
    /* DEVICE RELEASE                                                */
    /*---------------------------------------------------------------*/
        /* Command reject if within the domain of a Locate Record, or
           preceded by Define Extent, Space Count, or Set File Mask,
           or (for 3390 only) preceded by Read IPL */
        if (dev->ckdlcount > 0
            || dev->ckdxtdef || dev->ckdspcnt || dev->ckdsetfm
            || (dev->ckdrdipl && dev->devtype == 0x3390))
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_2);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Call the release exit and mark the device not reserved */
        if (dev->hnd->release) (dev->hnd->release) (dev);

        obtain_lock (&dev->lock);
        dev->reserved = 0;
        release_lock (&dev->lock);

        /* Perform the operation of a sense command */
        goto sense;

    case 0x14: /* UNCONDITIONAL RESERVE */
    case 0xB4: /* DEVICE RESERVE */
    /*---------------------------------------------------------------*/
    /* DEVICE RESERVE                                                */
    /*---------------------------------------------------------------*/
        /* Command reject if within the domain of a Locate Record,
           or indeed if preceded by any command at all apart from
           Suspend Multipath Reconnection */
        if (dev->ckdlcount > 0
            || ccwseq > 1
            || (chained && prevcode != 0x5B))
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_2);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Mark the device reserved and call the reserve exit */

        obtain_lock (&dev->lock);
        dev->reserved = 1;
        release_lock (&dev->lock);

        if (dev->hnd->reserve) (dev->hnd->reserve) (dev);

        /* Perform the operation of a sense command */
        goto sense;

    case 0x04:
    /*---------------------------------------------------------------*/
    /* SENSE                                                         */
    /*---------------------------------------------------------------*/
        /* Command reject if within the domain of a Locate Record */
        if (dev->ckdlcount > 0)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_2);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

    sense:
        /* Calculate residual byte count */
        num = (count < dev->numsense) ? count : dev->numsense;
        *residual = count - num;
        if (count < dev->numsense) *more = 1;

        /* Copy device sense bytes to channel I/O buffer */
        memcpy (iobuf, dev->sense, num);

        /* Clear the device sense bytes */
        memset (dev->sense, 0, sizeof(dev->sense));

        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0xE4:
    /*---------------------------------------------------------------*/
    /* SENSE ID                                                      */
    /*---------------------------------------------------------------*/
        /* Command reject if within the domain of a Locate Record */
        if (dev->ckdlcount > 0)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_2);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Calculate residual byte count */
        num = (count < dev->numdevid) ? count : dev->numdevid;
        *residual = count - num;
        if (count < dev->numdevid) *more = 1;

        /* Copy device identifier bytes to channel I/O buffer */
        memcpy (iobuf, dev->devid, num);

        /* Return unit status */
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x34:
    /*---------------------------------------------------------------*/
    /* SENSE PATH GROUP ID                                           */
    /*---------------------------------------------------------------*/

        /* Calculate residual byte count */
        num = (count < 12) ? count : 12;
        *residual = count - num;
        if (count < 12) *more = 1;

        /* Byte 0 is the path group state byte */
        iobuf[0] = SPG_PATHSTAT_RESET
                | SPG_PARTSTAT_IENABLED
                | SPG_PATHMODE_SINGLE;

        /* Bytes 1-11 contain the path group identifier */
        memcpy (iobuf+1, dev->pgid, 11);

        /* Return unit status */
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0xAF:
    /*---------------------------------------------------------------*/
    /* SET PATH GROUP ID                                             */
    /*---------------------------------------------------------------*/

        /* Calculate residual byte count */
        num = (count < 12) ? count : 12;
        *residual = count - num;

        /* Control information length must be at least 12 bytes */
        if (count < 12)
        {
            dev->sense[0] = SENSE_CR;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Byte 0 is the path group state byte */
        if ((iobuf[0] & SPG_SET_COMMAND) == SPG_SET_ESTABLISH)
        {
            /* Only accept the new pathgroup id when
               1) it has not yet been set (ie contains zeros) or
               2) It is set, but we are setting the same value */
            if(memcmp(dev->pgid,
                 "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 11)
              && memcmp(dev->pgid, iobuf+1, 11))
            {
                dev->sense[0] = SENSE_CR;
                *unitstat = CSW_CE | CSW_DE | CSW_UC;
                break;
            }

            /* Bytes 1-11 contain the path group identifier */
            memcpy (dev->pgid, iobuf+1, 11);
        }

        /* Return unit status */
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x54:
    /*---------------------------------------------------------------*/
    /* SENSE SUBSYSTEM STATUS                                        */
    /*---------------------------------------------------------------*/
        /* Command reject if within the domain of a Locate Record,
           or if chained from any command unless the preceding command
           is Read Device Characteristics, Read Configuration Data, or
           a Suspend Multipath Reconnection command that was the first
           command in the chain */
        if (dev->ckdlcount > 0
            || (chained && prevcode != 0x64 && prevcode != 0xFA
                && prevcode != 0x5B)
            || (chained && prevcode == 0x5B && ccwseq > 1))
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_2);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Calculate residual byte count */
        num = (count < 40) ? count : 40;
        *residual = count - num;
        if (count < 40) *more = 1;

        /* Build the subsystem status data in the I/O area */
        memset (iobuf, 0x00, 40);
        iobuf[1] = dev->devnum & 0xFF;

        /* Return unit status */
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0xA4:
    /*---------------------------------------------------------------*/
    /* READ AND RESET BUFFERED LOG                                   */
    /*---------------------------------------------------------------*/
        /* Command reject if within the domain of a Locate Record,
           or if chained from any command unless the preceding command
           is Read Device Characteristics, Read Configuration Data, or
           a Suspend Multipath Reconnection command that was the first
           command in the chain */
        if (dev->ckdlcount > 0
            || (chained && prevcode != 0x64 && prevcode != 0xFA
                && prevcode != 0x5B)
            || (chained && prevcode == 0x5B && ccwseq > 1))
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_2);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Calculate residual byte count */
        num = (count < 32) ? count : 32;
        *residual = count - num;
        if (count < 32) *more = 1;

        /* Build the buffered error log in the I/O area */
        memset (iobuf, 0x00, 32);

        /* Return unit status */
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0xFA:
    /*---------------------------------------------------------------*/
    /* READ CONFIGURATION DATA                                       */
    /*---------------------------------------------------------------*/
        /* Command reject if within the domain of a Locate Record */
        if (dev->ckdlcount > 0)
        {
            ckd_build_sense (dev, SENSE_CR, 0, 0,
                            FORMAT_0, MESSAGE_2);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Calculate residual byte count */
        num = (count < CONFIG_DATA_SIZE) ? count : CONFIG_DATA_SIZE;
        *residual = count - num;
        if (count < CONFIG_DATA_SIZE) *more = 1;

        /* Clear the configuration data area */
        memset (iobuf, 0x00, CONFIG_DATA_SIZE);

        /* Bytes 0-31 contain node element descriptor 1 (HDA) data */
        iobuf[0] = 0xCC;
        iobuf[1] = 0x01;
        iobuf[2] = 0x01;
        memcpy (&iobuf[4],"  0000000HRC01000000000001", 26);
        for (i = 4; i < 30; i++)
            iobuf[i] = host_to_guest(iobuf[i]);
        iobuf[31] = 0x30;

        /* Bytes 32-63 contain node element descriptor 2 (unit) data */
        memcpy (&iobuf[32], &iobuf[0], 32);
        iobuf[33] = 0x00;
        iobuf[34] = 0x00;

        /* Bytes 64-95 contain node element descriptor 3 (CU) data */
        memcpy (&iobuf[64], &iobuf[0], 32);
        iobuf[64] = 0xD4;
        iobuf[65] = 0x02;
        iobuf[66] = 0x00;
        iobuf[95] = 0x91;

       /* Bytes 96-127 contain node element desc 4 (subsystem) data */
        memcpy (&iobuf[96], &iobuf[0], 32);
        iobuf[96] = 0xF0;
        iobuf[97] = 0x00;
        iobuf[98] = 0x00;
        iobuf[99] = 0x01;
        memcpy (&iobuf[102], "0000   ", 7);
        iobuf[102] += ((dev->ckdcu->devt >> 12) & 0x0f);
        iobuf[103] += ((dev->ckdcu->devt >>  8) & 0x0f);
        iobuf[104] += ((dev->ckdcu->devt >>  4) & 0x0f);
        iobuf[105] += ((dev->ckdcu->devt >>  0) & 0x0f);
        for (i = 102; i < 109; i++)
            iobuf[i] = host_to_guest(iobuf[i]);

        /* Bytes 128-223 contain zeroes */

        /* Bytes 224-255 contain node element qualifier data */
        iobuf[224] = 0x80;
        iobuf[230] = 0x3c;
        iobuf[233] = (dev->devtype >> 8) & 0xff;
        iobuf[234] = 0x80;
        iobuf[235] = dev->devtype & 0xff;
        iobuf[236] = dev->devtype & 0xff;
        iobuf[237] = dev->devtype & 0xff;
        iobuf[241] = 0x80;
        iobuf[243] = dev->devtype & 0xff;

        /* Return unit status */
        *unitstat = CSW_CE | CSW_DE;
        break;

    default:
    /*---------------------------------------------------------------*/
    /* INVALID OPERATION                                             */
    /*---------------------------------------------------------------*/
        /* Set command reject sense byte, and unit check status */
        ckd_build_sense (dev, SENSE_CR, 0, 0,
                        FORMAT_0, MESSAGE_1);
        *unitstat = CSW_CE | CSW_DE | CSW_UC;

    } /* end switch(code) */

    /* Return if synchronous I/O needs to be retried asynchronously */
    if (dev->syncio_retry) return;

    /* Reset the flags which ensure correct positioning for write
       commands */

    /* Reset search HA flag if command was not SEARCH HA EQUAL
       or WRITE HA */
    if ((code & 0x7F) != 0x39 && (code & 0x7F) != 0x19)
        dev->ckdhaeq = 0;

    /* Reset search id flag if command was not SEARCH ID EQUAL,
       READ/WRITE KEY AND DATA, or READ/WRITE DATA */
    if ((code & 0x7F) != 0x31
        && (code & 0x7F) != 0x0E && (code & 0x7F) != 0x0D
        && (code & 0x7F) != 0x06 && (code & 0x7F) != 0x05)
        dev->ckdideq = 0;

    /* Reset search key flag if command was not SEARCH KEY EQUAL
       or READ/WRITE DATA */
    if ((code & 0x7F) != 0x29
        && (code & 0x7F) != 0x06 && (code & 0x7F) != 0x05)
        dev->ckdkyeq = 0;

    /* Reset write CKD flag if command was not WRITE R0 or WRITE CKD */
    if (code != 0x15 && code != 0x1D)
        dev->ckdwckd = 0;

    /* If within the domain of a locate record then decrement the
       count of CCWs remaining to be processed within the domain */
    if (dev->ckdlcount > 0 && code != 0x047)
    {
        /* Decrement the count of CCWs remaining in the domain */
        dev->ckdlcount--;

        /* Command reject with incomplete domain if CCWs remain
           but command chaining is not specified */
        if (dev->ckdlcount > 0 && (flags & CCW_FLAGS_CC) == 0 &&
            code != 0x02)
        {
            ckd_build_sense (dev, SENSE_CR | SENSE_OC, 0, 0, 0, 0);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
        }
    } /* end if(ckdlcount) */

} /* end function ckddasd_execute_ccw */


DEVHND ckddasd_device_hndinfo = {
        &ckddasd_init_handler,
        &ckddasd_execute_ccw,
        &ckddasd_close_device,
        &ckddasd_query_device,
        &ckddasd_start,
        &ckddasd_end,
        &ckddasd_start,
        &ckddasd_end,
        &ckddasd_read_track,
        &ckddasd_update_track,
        &ckddasd_used,
        NULL,
        NULL
};

/* OMATAPE.C    (c) Copyright Roger Bowler, 1999-2012                */
/*              Hercules Tape Device Handler for OMATAPE             */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

/* Original Author: Roger Bowler                                     */
/* Prime Maintainer: Ivan Warren                                     */
/* Secondary Maintainer: "Fish" (David B. Trout)                     */

/*-------------------------------------------------------------------*/
/* This module contains the OMATAPE emulated tape format support.    */
/* The subroutines in this module are called by the general tape     */
/* device handler (tapedev.c) when the tape format is OMATAPE.       */
/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/
/* Reference information:                                            */
/* SC53-1200 S/370 and S/390 Optical Media Attach/2 User's Guide     */
/* SC53-1201 S/370 and S/390 Optical Media Attach/2 Technical Ref    */
/*-------------------------------------------------------------------*/

#include "hstdinc.h"
#include "hercules.h"  /* need Hercules control blocks               */
#include "tapedev.h"   /* Main tape handler header file              */

//#define  ENABLE_TRACING_STMTS   1       // (Fish: DEBUGGING)
//#include "dbgtrace.h"                   // (Fish: DEBUGGING)

/*-------------------------------------------------------------------*/
/* Read the OMA tape descriptor file                                 */
/*-------------------------------------------------------------------*/
int read_omadesc (DEVBLK *dev)
{
int             rc;                     /* Return code               */
int             i;                      /* Array subscript           */
size_t          pathlen;                /* Length of TDF path name   */
int             tdfsize;                /* Size of TDF file in bytes */
int             filecount;              /* Number of files           */
int             stmt;                   /* TDF file statement number */
int             fd;                     /* TDF file descriptor       */
struct stat     statbuf;                /* TDF file information      */
U32             blklen;                 /* Fixed block length        */
int             tdfpos;                 /* Position in TDF buffer    */
char           *tdfbuf;                 /* -> TDF file buffer        */
char           *tdfrec;                 /* -> TDF record             */
char           *tdffilenm;              /* -> Filename in TDF record */
char           *tdfformat;              /* -> Format in TDF record   */
char           *tdfreckwd;              /* -> Keyword in TDF record  */
char           *tdfblklen;              /* -> Length in TDF record   */
OMATAPE_DESC   *tdftab;                 /* -> Tape descriptor array  */
BYTE            c;                      /* Work area for sscanf      */
char            pathname[MAX_PATH];     /* file path in host format  */
char           *strtok_str = NULL;      /* last token position       */

    /* Isolate the base path name of the TDF file */
    for (pathlen = strlen(dev->filename); pathlen > 0; )
    {
        pathlen--;
        if (dev->filename[pathlen-1] == '/') break;

    }
#if 0
    // JCS thinks this is bad
    if (pathlen < 7
        || strncasecmp(dev->filename+pathlen-7, "/tapes/", 7) != 0)
    {
        WRITEMSG ("%1d:%04X TDF File '%s': invalid filename: TDF files must be in the TAPES subdirectory", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename+pathlen, "oma");
        return -1;
    }
    pathlen -= 7;
#endif

    /* Open the tape descriptor file */
    hostpath(pathname, dev->filename, sizeof(pathname));
    fd = HOPEN (pathname, O_RDONLY | O_BINARY);
    if (fd < 0)
    {
        WRMSG (HHC00205, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename, "oma", "open()", strerror(errno));
        return -1;
    }

    /* Determine the size of the tape descriptor file */
    rc = fstat (fd, &statbuf);
    if (rc < 0)
    {
        WRMSG (HHC00205, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename, "oma", "fstat()", strerror(errno));
        close (fd);
        return -1;
    }
    tdfsize = statbuf.st_size;

    /* Obtain a buffer for the tape descriptor file */
    tdfbuf = malloc (tdfsize);
    if (tdfbuf == NULL)
    {
        WRMSG (HHC00205, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename, "oma", "malloc()", strerror(errno));
        close (fd);
        return -1;
    }

    /* Read the tape descriptor file into the buffer */
    rc = read (fd, tdfbuf, tdfsize);
    if (rc < tdfsize)
    {
        WRMSG (HHC00205, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename, "oma", "read()", strerror(errno));
        free (tdfbuf);
        close (fd);
        return -1;
    }

    /* Close the tape descriptor file */
    close (fd); fd = -1;

    /* Check that the first record is a TDF header */
    if (memcmp(tdfbuf, "@TDF", 4) != 0)
    {
        WRMSG (HHC00206, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename, "oma");
        free (tdfbuf);
        return -1;
    }

    /* Count the number of linefeeds in the tape descriptor file
       to determine the size of the descriptor array required */
    for (i = 0, filecount = 0; i < tdfsize; i++)
    {
        if (tdfbuf[i] == '\n') filecount++;
    } /* end for(i) */
    /* ISW Add 1 to filecount to add an extra EOT marker */
    filecount++;

    /* Obtain storage for the tape descriptor array */
    tdftab = (OMATAPE_DESC*)malloc (filecount * sizeof(OMATAPE_DESC));
    if (tdftab == NULL)
    {
        WRMSG (HHC00205, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename, "oma", "malloc()", strerror(errno));
        free (tdfbuf);
        return -1;
    }

    /* Build the tape descriptor array */
    for (filecount = 0, tdfpos = 0, stmt = 1; ; filecount++)
    {
        /* Clear the tape descriptor array entry */
        memset (&(tdftab[filecount]), 0, sizeof(OMATAPE_DESC));

        /* Point past the next linefeed in the TDF file */
        while (tdfpos < tdfsize && tdfbuf[tdfpos++] != '\n');
        stmt++;

        /* Exit at end of TDF file */
        if (tdfpos >= tdfsize) break;

        /* Mark the end of the TDF record with a null terminator */
        tdfrec = tdfbuf + tdfpos;
        while (tdfpos < tdfsize && tdfbuf[tdfpos]!='\r'
            && tdfbuf[tdfpos]!='\n') tdfpos++;
        c = tdfbuf[tdfpos];
        if (tdfpos >= tdfsize) break;
        tdfbuf[tdfpos] = '\0';

        /* Exit if TM or EOT record */
        if (strcasecmp(tdfrec, "TM") == 0)
        {
            tdftab[filecount].format='X';
            tdfbuf[tdfpos] = c;
            continue;
        }
        if(strcasecmp(tdfrec, "EOT") == 0)
        {
            tdftab[filecount].format='E';
            break;
        }

        /* Parse the TDF record */
        tdffilenm = strtok_r (tdfrec, " \t", &strtok_str);
        tdfformat = strtok_r (NULL,   " \t", &strtok_str);
        tdfreckwd = strtok_r (NULL,   " \t", &strtok_str);
        tdfblklen = strtok_r (NULL,   " \t", &strtok_str);

        /* Check for missing fields */
        if (tdffilenm == NULL || tdfformat == NULL)
        {
            WRMSG (HHC00207, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename, "oma", stmt, "filename or format missing");
            free (tdftab);
            free (tdfbuf);
            return -1;
        }

        /* Check that the file name is not too long */
        if (pathlen + 1 + strlen(tdffilenm)
                > sizeof(tdftab[filecount].filename) - 1)
        {
            char buf[MAX_PATH+32];

            if ( strchr(tdffilenm, SPACE) == NULL)
                MSGBUF(buf, "filename %s too long", tdffilenm);
            else
                MSGBUF(buf, "filename '%s' too long", tdffilenm);

            WRMSG (HHC00207, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename, "oma", stmt, buf);
            free (tdftab);
            free (tdfbuf);
            return -1;
        }

        /* Convert the file name to Unix format */
        for (i = 0; i < (int)strlen(tdffilenm); i++)
        {
            if (tdffilenm[i] == '\\')
                tdffilenm[i] = '/';
/* JCS */
//            else
//                tdffilenm[i] = Tolower(tdffilenm[i]);
        } /* end for(i) */

        /* Prefix the file name with the base path name and
           save it in the tape descriptor array */
        /* but only if the filename lacks a leading slash - JCS  */
/*
        strncpy (tdftab[filecount].filename, dev->filename, pathlen);
        if (tdffilenm[0] != '/')
            stlrcat ( tdftab[filecount].filename, "/", sizeof(tdftab[filecount].filename) );
        strlcat ( tdftab[filecount].filename, tdffilenm, sizeof(tdftab[filecount].filename) );
*/
        tdftab[filecount].filename[0] = 0;

        if ((tdffilenm[0] != '/') && (tdffilenm[1] != ':'))
        {
            strncpy (tdftab[filecount].filename, dev->filename, pathlen);
            strlcat (tdftab[filecount].filename, "/", sizeof(tdftab[filecount].filename) );
        }

        strlcat (tdftab[filecount].filename, tdffilenm, sizeof(tdftab[filecount].filename) );

        /* Check for valid file format code */
        if (strcasecmp(tdfformat, "HEADERS") == 0)
        {
            tdftab[filecount].format = 'H';
        }
        else if (strcasecmp(tdfformat, "TEXT") == 0)
        {
            tdftab[filecount].format = 'T';
        }
        else if (strcasecmp(tdfformat, "FIXED") == 0)
        {
            /* Check for RECSIZE keyword */
            if (tdfreckwd == NULL
                || strcasecmp(tdfreckwd, "RECSIZE") != 0)
            {
                WRMSG (HHC00207, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename, "oma", stmt, "keyword RECSIZE missing");
                free (tdftab);
                free (tdfbuf);
                return -1;
            }

            /* Check for valid fixed block length */
            if (tdfblklen == NULL
                || sscanf(tdfblklen, "%u%c", &blklen, &c) != 1
                || blklen < 1 || blklen > MAX_BLKLEN)
            {
                char buf[40];
                MSGBUF(buf, "invalid record size %s", tdfblklen);
                WRMSG (HHC00207, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename, "oma", stmt, buf);
                free (tdftab);
                free (tdfbuf);
                return -1;
            }

            /* Set format and block length in descriptor array */
            tdftab[filecount].format = 'F';
            tdftab[filecount].blklen = blklen;
        }
        else
        {
            char buf[40];
            MSGBUF(buf, "invalid record format '%s'", tdfformat);
            WRMSG (HHC00207, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename, "oma", stmt, buf);
            free (tdftab);
            free (tdfbuf);
            return -1;
        }
        tdfbuf[tdfpos] = c;
    } /* end for(filecount) */
    /* Force an EOT as last entry (filecount is correctly adjusted here) */
    tdftab[filecount].format='E';

    /* Save the file count and TDF array pointer in the device block */
    dev->omafiles = filecount+1;
    dev->omadesc = tdftab;

    /* Release the TDF file buffer and exit */
    free (tdfbuf);
    return 0;

} /* end function read_omadesc */

/*-------------------------------------------------------------------*/
/* Open the OMATAPE file defined by the current file number          */
/*                                                                   */
/* The OMA tape descriptor file is read if necessary.                */
/* If successful, the file descriptor is stored in the device block  */
/* and the return value is zero.  Otherwise the return value is -1.  */
/*-------------------------------------------------------------------*/
int open_omatape (DEVBLK *dev, BYTE *unitstat,BYTE code)
{
int             fd;                     /* File descriptor integer   */
int             rc;                     /* Return code               */
OMATAPE_DESC   *omadesc;                /* -> OMA descriptor entry   */
char            pathname[MAX_PATH];     /* file path in host format  */

     /* Check for no tape in drive */
     if (!strcmp (dev->filename, TAPE_UNLOADED))
     {
         build_senseX(TAPE_BSENSE_TAPEUNLOADED,dev,unitstat,code);
         return -1;
     }

    /* Read the OMA descriptor file if necessary */
    if (dev->omadesc == NULL)
    {
        rc = read_omadesc (dev);
        if (rc < 0)
        {
            build_senseX(TAPE_BSENSE_TAPELOADFAIL,dev,unitstat,code);
            return -1;
        }
        dev->blockid = 0;
    }

    dev->fenced = 0;

    /* Unit exception if beyond end of tape */
    /* ISW: CHANGED PROCESSING - RETURN UNDEFINITE Tape Marks  */
    /*       NOTE: The last entry in the TDF table is ALWAYS   */
    /*              an EOT Condition                           */
    /*              This is ensured by the TDF reading routine */
#if 0
    if (dev->curfilen >= dev->omafiles)
    {
        WRITEMSG (HHC00000E, SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename);

        build_senseX(TAPE_BSENSE_ENDOFTAPE,dev,unitstat,code);
        return -1;
    }
#else
    if(dev->curfilen>dev->omafiles)
    {
        dev->curfilen=dev->omafiles;
        return(0);
    }
#endif

    /* Point to the current file entry in the OMA descriptor table */
    omadesc = (OMATAPE_DESC*)(dev->omadesc);
    omadesc += (dev->curfilen-1);
    if(omadesc->format=='X')
    {
        return 0;
    }
    if(omadesc->format=='E')
    {
        return 0;
    }

    /* Open the OMATAPE file */
    hostpath(pathname, omadesc->filename, sizeof(pathname));
    fd = HOPEN (pathname, O_RDONLY | O_BINARY);

    /* Check for successful open */
    if (fd < 0 || lseek (fd, 0, SEEK_END) > LONG_MAX)
    {
        if (fd >= 0)            /* (if open was successful, then it) */
            errno = EOVERFLOW;  /* (must have been a lseek overflow) */

        WRMSG (HHC00205, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, omadesc->filename, "oma", "open()", strerror(errno));

        if (fd >= 0)
            close(fd);          /* (close the file if it was opened) */

        build_senseX(TAPE_BSENSE_TAPELOADFAIL,dev,unitstat,code);
        return -1;
    }

    /* OMA tapes are always read-only */
    dev->readonly = 1;

    /* Store the file descriptor in the device block */
    dev->fd = fd;
    return 0;

} /* end function open_omatape */

/*-------------------------------------------------------------------*/
/* Read a block header from an OMA tape file in OMA headers format   */
/*                                                                   */
/* If successful, return value is zero, and the current block        */
/* length and previous and next header offsets are returned.         */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*-------------------------------------------------------------------*/
int readhdr_omaheaders (DEVBLK *dev, OMATAPE_DESC *omadesc,
                        long blkpos, S32 *pcurblkl, S32 *pprvhdro,
                        S32 *pnxthdro, BYTE *unitstat,BYTE code)
{
int             rc;                     /* Return code               */
off_t           rcoff;                  /* Return code from lseek()  */
int             padding;                /* Number of padding bytes   */
OMATAPE_BLKHDR  omahdr;                 /* OMATAPE block header      */
S32             curblkl;                /* Length of current block   */
S32             prvhdro;                /* Offset of previous header */
S32             nxthdro;                /* Offset of next header     */

    /* Seek to start of block header */
    rcoff = lseek (dev->fd, blkpos, SEEK_SET);
    if (rcoff < 0)
    {
        /* Handle seek error condition */
        WRMSG (HHC00204, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, omadesc->filename, "oma", "lseek()", (off_t)blkpos, strerror(errno));

        /* Set unit check with equipment check */
        build_senseX(TAPE_BSENSE_LOCATEERR,dev,unitstat,code);
        return -1;
    }

    /* Read the 16-byte block header */
    rc = read (dev->fd, &omahdr, sizeof(omahdr));

    /* Handle read error condition */
    if (rc < 0)
    {
        WRMSG (HHC00204, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, omadesc->filename, "oma", "read()", (off_t)blkpos,
                strerror(errno));

        /* Set unit check with equipment check */
        build_senseX(TAPE_BSENSE_READFAIL,dev,unitstat,code);
        return -1;
    }

    /* Handle end of file within block header */
    if (rc < (int)sizeof(omahdr))
    {
        WRMSG (HHC00204, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, omadesc->filename, "oma", "readhdr_omaheaders()", (off_t)blkpos, "unexpected end of file");

        /* Set unit check with data check and partial record */
        build_senseX(TAPE_BSENSE_BLOCKSHORT,dev,unitstat,code);
        return -1;
    }

    /* Extract the current block length and previous header offset */
    curblkl = (S32)(((U32)(omahdr.curblkl[3]) << 24)
                    | ((U32)(omahdr.curblkl[2]) << 16)
                    | ((U32)(omahdr.curblkl[1]) << 8)
                    | omahdr.curblkl[0]);
    prvhdro = (S32)((U32)(omahdr.prvhdro[3]) << 24)
                    | ((U32)(omahdr.prvhdro[2]) << 16)
                    | ((U32)(omahdr.prvhdro[1]) << 8)
                    | omahdr.prvhdro[0];

    /* Check for valid block header */
    if (curblkl < -1 || curblkl == 0 || curblkl > MAX_BLKLEN
        || memcmp(omahdr.omaid, "@HDF", 4) != 0)
    {
        WRMSG (HHC00204, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, omadesc->filename, "oma", "readhdr_omaheaders()", (off_t)blkpos, "invalid block header");

        build_senseX(TAPE_BSENSE_READFAIL,dev,unitstat,code);
        return -1;
    }

    /* Calculate the number of padding bytes which follow the data */
    padding = (16 - (curblkl & 15)) & 15;

    /* Calculate the offset of the next block header */
    nxthdro = blkpos + sizeof(OMATAPE_BLKHDR) + curblkl + padding;

    /* Return current block length and previous/next header offsets */
    *pcurblkl = curblkl;
    *pprvhdro = prvhdro;
    *pnxthdro = nxthdro;
    return 0;

} /* end function readhdr_omaheaders */

/*-------------------------------------------------------------------*/
/* Read a block from an OMA tape file in OMA headers format          */
/*                                                                   */
/* If successful, return value is block length read.                 */
/* If a tapemark was read, the file is closed, the current file      */
/* number in the device block is incremented so that the next file   */
/* will be opened by the next CCW, and the return value is zero.     */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*-------------------------------------------------------------------*/
int read_omaheaders (DEVBLK *dev, OMATAPE_DESC *omadesc,
                        BYTE *buf, BYTE *unitstat,BYTE code)
{
int             rc;                     /* Return code               */
long            blkpos;                 /* Offset to block header    */
S32             curblkl;                /* Length of current block   */
S32             prvhdro;                /* Offset of previous header */
S32             nxthdro;                /* Offset of next header     */

    /* Read the 16-byte block header */
    blkpos = dev->nxtblkpos;
    rc = readhdr_omaheaders (dev, omadesc, blkpos, &curblkl,
                            &prvhdro, &nxthdro, unitstat,code);
    if (rc < 0) return -1;

    /* Update the offsets of the next and previous blocks */
    dev->nxtblkpos = nxthdro;
    dev->prvblkpos = blkpos;

    /* Increment file number and return zero if tapemark */
    if (curblkl == -1)
    {
        close (dev->fd);
        dev->fd = -1;
        dev->curfilen++;
        dev->nxtblkpos = 0;
        dev->prvblkpos = -1;
        return 0;
    }

    /* Read data block from tape file */
    rc = read (dev->fd, buf, curblkl);

    /* Handle read error condition */
    if (rc < 0)
    {
        WRMSG (HHC00204, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, omadesc->filename, "oma", "read()", (off_t)blkpos,
                strerror(errno));

        /* Set unit check with equipment check */
        build_senseX(TAPE_BSENSE_READFAIL,dev,unitstat,code);
        return -1;
    }

    /* Handle end of file within data block */
    if (rc < curblkl)
    {
        WRMSG(HHC00204, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, omadesc->filename, "oma", "read_omaheaders()", (off_t)blkpos, "unexpected end of file");

        /* Set unit check with data check and partial record */
        build_senseX(TAPE_BSENSE_BLOCKSHORT,dev,unitstat,code);
        return -1;
    }

    /* Return block length */
    return curblkl;

} /* end function read_omaheaders */

/*-------------------------------------------------------------------*/
/* Read a block from an OMA tape file in fixed block format          */
/*                                                                   */
/* If successful, return value is block length read.                 */
/* If a tapemark was read, the file is closed, the current file      */
/* number in the device block is incremented so that the next file   */
/* will be opened by the next CCW, and the return value is zero.     */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*-------------------------------------------------------------------*/
int read_omafixed (DEVBLK *dev, OMATAPE_DESC *omadesc,
                        BYTE *buf, BYTE *unitstat,BYTE code)
{
off_t           rcoff;                  /* Return code from lseek()  */
int             blklen;                 /* Block length              */
long            blkpos;                 /* Offset of block in file   */

    /* Initialize current block position */
    blkpos = dev->nxtblkpos;

    /* Seek to new current block position */
    rcoff = lseek (dev->fd, blkpos, SEEK_SET);
    if (rcoff < 0)
    {
        /* Handle seek error condition */
        WRMSG (HHC00204, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, omadesc->filename, "oma", "lseek()", (off_t)blkpos, strerror(errno));

        /* Set unit check with equipment check */
        build_senseX(TAPE_BSENSE_LOCATEERR,dev,unitstat,code);
        return -1;
    }

    /* Read fixed length block or short final block */
    blklen = read (dev->fd, buf, omadesc->blklen);

    /* Handle read error condition */
    if (blklen < 0)
    {
        WRMSG (HHC00204, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, omadesc->filename, "oma", "read()", (off_t)blkpos,
                strerror(errno));

        build_senseX(TAPE_BSENSE_READFAIL,dev,unitstat,code);
        return -1;
    }

    /* At end of file return zero to indicate tapemark */
    if (blklen == 0)
    {
        close (dev->fd);
        dev->fd = -1;
        dev->curfilen++;
        dev->nxtblkpos = 0;
        dev->prvblkpos = -1;
        return 0;
    }

    /* Calculate the offsets of the next and previous blocks */
    dev->nxtblkpos = blkpos + blklen;
    dev->prvblkpos = blkpos;

    /* Return block length, or zero to indicate tapemark */
    return blklen;

} /* end function read_omafixed */

/*-------------------------------------------------------------------*/
/* Read a block from an OMA tape file in ASCII text format           */
/*                                                                   */
/* If successful, return value is block length read.                 */
/* If a tapemark was read, the file is closed, the current file      */
/* number in the device block is incremented so that the next file   */
/* will be opened by the next CCW, and the return value is zero.     */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*                                                                   */
/* The buf parameter points to the I/O buffer during a read          */
/* operation, or is NULL for a forward space block operation.        */
/*-------------------------------------------------------------------*/
int read_omatext (DEVBLK *dev, OMATAPE_DESC *omadesc,
                        BYTE *buf, BYTE *unitstat,BYTE code)
{
int             rc;                     /* Return code               */
off_t           rcoff;                  /* Return code from lseek()  */
int             num;                    /* Number of characters read */
int             pos;                    /* Position in I/O buffer    */
long            blkpos;                 /* Offset of block in file   */
BYTE            c;                      /* Character work area       */

    /* Initialize current block position */
    blkpos = dev->nxtblkpos;

    /* Seek to new current block position */
    rcoff = lseek (dev->fd, blkpos, SEEK_SET);
    if (rcoff < 0)
    {
        /* Handle seek error condition */
        WRMSG (HHC00204, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, omadesc->filename, "oma", "lseek()", (off_t)blkpos, strerror(errno));

        /* Set unit check with equipment check */
        build_senseX(TAPE_BSENSE_LOCATEERR,dev,unitstat,code);
        return -1;
    }

    /* Read data from tape file until end of line */
    for (num = 0, pos = 0; ; )
    {
        rc = read (dev->fd, &c, 1);
        if (rc < 1) break;

        /* Treat X'1A' as end of file */
        if (c == '\x1A')
        {
            rc = 0;
            break;
        }

        /* Count characters read */
        num++;

        /* Ignore carriage return character */
        if (c == '\r') continue;

        /* Exit if newline character */
        if (c == '\n') break;

        /* Ignore characters in excess of I/O buffer length */
        if (pos >= MAX_BLKLEN) continue;

        /* Translate character to EBCDIC and copy to I/O buffer */
        if (buf != NULL)
            buf[pos] = host_to_guest(c);

        /* Count characters copied or skipped */
        pos++;

    } /* end for(num) */

    /* At end of file return zero to indicate tapemark */
    if (rc == 0 && num == 0)
    {
        close (dev->fd);
        dev->fd = -1;
        dev->curfilen++;
        dev->nxtblkpos = 0;
        dev->prvblkpos = -1;
        return 0;
    }

    /* Handle read error condition */
    if (rc < 0)
    {
        WRMSG (HHC00204, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, omadesc->filename, "oma", "read()", (off_t)blkpos,
                strerror(errno));

        build_senseX(TAPE_BSENSE_READFAIL,dev,unitstat,code);
        return -1;
    }

    /* Check for block not terminated by newline */
    if (rc < 1)
    {
        WRMSG (HHC00204, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, omadesc->filename, "oma", "read_omatext()", (off_t)blkpos, "unexpected end of file");

        /* Set unit check with data check and partial record */
        build_senseX(TAPE_BSENSE_BLOCKSHORT,dev,unitstat,code);
        return -1;
    }

    /* Check for invalid zero length block */
    if (pos == 0)
    {
        WRMSG (HHC00204, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, omadesc->filename, "oma", "read_omatext()", (off_t)blkpos, "invalid block header");

        /* Set unit check with equipment check */
        build_senseX(TAPE_BSENSE_BLOCKSHORT,dev,unitstat,code);
        return -1;
    }

    /* Calculate the offsets of the next and previous blocks */
    dev->nxtblkpos = blkpos + num;
    dev->prvblkpos = blkpos;

    /* Return block length */
    return pos;

} /* end function read_omatext */

/*-------------------------------------------------------------------*/
/* Read a block from an OMA - Selection of format done here          */
/*                                                                   */
/* If successful, return value is block length read.                 */
/* If a tapemark was read, the file is closed, the current file      */
/* number in the device block is incremented so that the next file   */
/* will be opened by the next CCW, and the return value is zero.     */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*                                                                   */
/* The buf parameter points to the I/O buffer during a read          */
/* operation, or is NULL for a forward space block operation.        */
/*-------------------------------------------------------------------*/
int read_omatape (DEVBLK *dev,
                        BYTE *buf, BYTE *unitstat,BYTE code)
{
int len;
OMATAPE_DESC *omadesc;
    omadesc = (OMATAPE_DESC*)(dev->omadesc);
    omadesc += (dev->curfilen-1);

    switch (omadesc->format)
    {
    default:
    case 'H':
        len = read_omaheaders (dev, omadesc, buf, unitstat,code);
        break;
    case 'F':
        len = read_omafixed (dev, omadesc, buf, unitstat,code);
        break;
    case 'T':
        len = read_omatext (dev, omadesc, buf, unitstat,code);
        break;
    case 'X':
        len=0;
        dev->curfilen++;
        break;
    case 'E':
        len=0;
        break;
    } /* end switch(omadesc->format) */

    if (len >= 0)
        dev->blockid++;

    return len;
}

/*-------------------------------------------------------------------*/
/* Forward space to next file of OMA tape device                     */
/*                                                                   */
/* For OMA tape devices, the forward space file operation is         */
/* achieved by closing the current file, and incrementing the        */
/* current file number in the device block, which causes the         */
/* next file will be opened when the next CCW is processed.          */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*-------------------------------------------------------------------*/
int fsf_omatape (DEVBLK *dev, BYTE *unitstat,BYTE code)
{
    UNREFERENCED(unitstat);
    UNREFERENCED(code);

    /* Close the current OMA file */
    if (dev->fd >= 0)
        close (dev->fd);
    dev->fd = -1;
    dev->nxtblkpos = 0;
    dev->prvblkpos = -1;

    /* Increment the current file number */
    dev->curfilen++;

    /* Return normal status */
    return 0;

} /* end function fsf_omatape */

/*-------------------------------------------------------------------*/
/* Forward space over next block of OMA file in OMA headers format   */
/*                                                                   */
/* If successful, return value is the length of the block skipped.   */
/* If the block skipped was a tapemark, the return value is zero,    */
/* the file is closed, and the current file number in the device     */
/* block is incremented so that the next file belonging to the OMA   */
/* tape will be opened when the next CCW is executed.                */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*-------------------------------------------------------------------*/
int fsb_omaheaders (DEVBLK *dev, OMATAPE_DESC *omadesc,
                        BYTE *unitstat,BYTE code)
{
int             rc;                     /* Return code               */
long            blkpos;                 /* Offset of block header    */
S32             curblkl;                /* Length of current block   */
S32             prvhdro;                /* Offset of previous header */
S32             nxthdro;                /* Offset of next header     */

    /* Initialize current block position */
    blkpos = dev->nxtblkpos;

    /* Read the 16-byte block header */
    rc = readhdr_omaheaders (dev, omadesc, blkpos, &curblkl,
                            &prvhdro, &nxthdro, unitstat,code);
    if (rc < 0) return -1;

    /* Check if tapemark was skipped */
    if (curblkl == -1)
    {
        /* Close the current OMA file */
        if (dev->fd >= 0)
            close (dev->fd);
        dev->fd = -1;
        dev->nxtblkpos = 0;
        dev->prvblkpos = -1;

        /* Increment the file number */
        dev->curfilen++;

        /* Return zero to indicate tapemark */
        return 0;
    }

    /* Update the offsets of the next and previous blocks */
    dev->nxtblkpos = nxthdro;
    dev->prvblkpos = blkpos;

    /* Return block length */
    return curblkl;

} /* end function fsb_omaheaders */

/*-------------------------------------------------------------------*/
/* Forward space over next block of OMA file in fixed block format   */
/*                                                                   */
/* If successful, return value is the length of the block skipped.   */
/* If already at end of file, the return value is zero,              */
/* the file is closed, and the current file number in the device     */
/* block is incremented so that the next file belonging to the OMA   */
/* tape will be opened when the next CCW is executed.                */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*-------------------------------------------------------------------*/
int fsb_omafixed (DEVBLK *dev, OMATAPE_DESC *omadesc,
                        BYTE *unitstat,BYTE code)
{
off_t           eofpos;                 /* Offset of end of file     */
off_t           blkpos;                 /* Offset of current block   */
int             curblkl;                /* Length of current block   */

    /* Initialize current block position */
    blkpos = dev->nxtblkpos;

    /* Seek to end of file to determine file size */
    eofpos = lseek (dev->fd, 0, SEEK_END);
    if (eofpos < 0 || eofpos >= LONG_MAX)
    {
        /* Handle seek error condition */
        if ( eofpos >= LONG_MAX) errno = EOVERFLOW;
        WRMSG (HHC00205, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, omadesc->filename, "oma", "lseek()", strerror(errno));

        /* Set unit check with equipment check */
        build_senseX(TAPE_BSENSE_LOCATEERR,dev,unitstat,code);
        return -1;
    }

    /* Check if already at end of file */
    if (blkpos >= eofpos)
    {
        /* Close the current OMA file */
        if (dev->fd >= 0)
            close (dev->fd);
        dev->fd = -1;
        dev->nxtblkpos = 0;
        dev->prvblkpos = -1;

        /* Increment the file number */
        dev->curfilen++;

        /* Return zero to indicate tapemark */
        return 0;
    }

    /* Calculate current block length */
    curblkl = (int)(eofpos - blkpos);
    if (curblkl > omadesc->blklen)
        curblkl = omadesc->blklen;

    /* Update the offsets of the next and previous blocks */
    dev->nxtblkpos = (long)(blkpos + curblkl);
    dev->prvblkpos = (long)(blkpos);

    /* Return block length */
    return curblkl;

} /* end function fsb_omafixed */

/*-------------------------------------------------------------------*/
/* Forward space to next block of OMA file                           */
/*                                                                   */
/* If successful, return value is the length of the block skipped.   */
/* If forward spaced over end of file, return value is 0.            */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*-------------------------------------------------------------------*/
int fsb_omatape (DEVBLK *dev, BYTE *unitstat,BYTE code)
{
int             rc;                     /* Return code               */
OMATAPE_DESC   *omadesc;                /* -> OMA descriptor entry   */

    /* Point to the current file entry in the OMA descriptor table */
    omadesc = (OMATAPE_DESC*)(dev->omadesc);
    omadesc += (dev->curfilen-1);

    /* Forward space block depending on OMA file type */
    switch (omadesc->format)
    {
    default:
    case 'H':
        rc = fsb_omaheaders (dev, omadesc, unitstat,code);
        break;
    case 'F':
        rc = fsb_omafixed (dev, omadesc, unitstat,code);
        break;
    case 'T':
        rc = read_omatext (dev, omadesc, NULL, unitstat,code);
        break;
    } /* end switch(omadesc->format) */

    if (rc >= 0) dev->blockid++;

    return rc;

} /* end function fsb_omatape */

/*-------------------------------------------------------------------*/
/* Backspace to previous file of OMA tape device                     */
/*                                                                   */
/* If the current file number is 1, then backspace file simply       */
/* closes the file, setting the current position to start of tape.   */
/* Otherwise, the current file is closed, the current file number    */
/* is decremented, the new file is opened, and the new file is       */
/* repositioned to just before the tape mark at the end of the file. */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*-------------------------------------------------------------------*/
int bsf_omatape (DEVBLK *dev, BYTE *unitstat,BYTE code)
{
int             rc;                     /* Return code               */
off_t           pos;                    /* File position             */
OMATAPE_DESC   *omadesc;                /* -> OMA descriptor entry   */
S32             curblkl;                /* Length of current block   */
S32             prvhdro;                /* Offset of previous header */
S32             nxthdro;                /* Offset of next header     */

    /* Close the current OMA file */
    if (dev->fd >= 0)
        close (dev->fd);
    dev->fd = -1;
    dev->nxtblkpos = 0;
    dev->prvblkpos = -1;

    /* Exit with tape at load point if currently on first file */
    if (dev->curfilen <= 1)
    {
        build_senseX(TAPE_BSENSE_LOADPTERR,dev,unitstat,code);
        return -1;
    }

    /* Decrement current file number */
    dev->curfilen--;

    /* Point to the current file entry in the OMA descriptor table */
    omadesc = (OMATAPE_DESC*)(dev->omadesc);
    omadesc += (dev->curfilen-1);

    /* Open the new current file */
    rc = open_omatape (dev, unitstat,code);
    if (rc < 0) return rc;

    /* Reposition before tapemark header at end of file, or
       to end of file for fixed block or ASCII text files */
    pos = 0;
    if ( 'H' == omadesc->format )
        pos -= sizeof(OMATAPE_BLKHDR);

    pos = lseek (dev->fd, pos, SEEK_END);
    if (pos < 0)
    {
        /* Handle seek error condition */
        WRMSG (HHC00205, "E", SSID_TO_LCSS(dev->ssid), dev->devnum, omadesc->filename, "oma", "lseek()", strerror(errno));

        /* Set unit check with equipment check */
        build_senseX(TAPE_BSENSE_LOCATEERR,dev,unitstat,code);
        dev->sense[0] = SENSE_EC;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return -1;
    }
    dev->nxtblkpos = pos;
    dev->prvblkpos = -1;

    /* Determine the offset of the previous block */
    switch (omadesc->format)
    {
    case 'H':
        /* For OMA headers files, read the tapemark header
           and extract the previous block offset */
        rc = readhdr_omaheaders (dev, omadesc, pos, &curblkl,
                                &prvhdro, &nxthdro, unitstat,code);
        if (rc < 0) return -1;
        dev->prvblkpos = prvhdro;
        break;
    case 'F':
        /* For OMA fixed block files, calculate the previous block
           offset allowing for a possible short final block */
        pos = (pos + omadesc->blklen - 1) / omadesc->blklen;
        dev->prvblkpos = (pos > 0 ? (pos - 1) * omadesc->blklen : -1);
        break;
    case 'T':
        /* For OMA ASCII text files, the previous block is unknown */
        dev->prvblkpos = -1;
        break;
    } /* end switch(omadesc->format) */

    /* Return normal status */
    return 0;

} /* end function bsf_omatape */

/*-------------------------------------------------------------------*/
/* Backspace to previous block of OMA file                           */
/*                                                                   */
/* If successful, return value is +1.                                */
/* If current position is at start of a file, then a backspace file  */
/* operation is performed to reset the position to the end of the    */
/* previous file, and the return value is zero.                      */
/* If error, return value is -1 and unitstat is set to CE+DE+UC      */
/*                                                                   */
/* Note that for ASCII text files, the previous block position is    */
/* known only if the previous CCW was a read or a write, so any      */
/* attempt to issue more than one consecutive backspace block on     */
/* an ASCII text file will fail with unit check status.              */
/*-------------------------------------------------------------------*/
int bsb_omatape (DEVBLK *dev, BYTE *unitstat,BYTE code)
{
int             rc;                     /* Return code               */
OMATAPE_DESC   *omadesc;                /* -> OMA descriptor entry   */
long            blkpos;                 /* Offset of block header    */
S32             curblkl;                /* Length of current block   */
S32             prvhdro = 0;            /* Offset of previous header */
S32             nxthdro;                /* Offset of next header     */

    /* Point to the current file entry in the OMA descriptor table */
    omadesc = (OMATAPE_DESC*)(dev->omadesc);
    omadesc += (dev->curfilen-1);

    /* Backspace file if current position is at start of file */
    if (dev->nxtblkpos == 0)
    {
        /* Unit check if already at start of tape */
        if (dev->curfilen <= 1)
        {
            build_senseX(TAPE_BSENSE_LOADPTERR,dev,unitstat,code);
            return -1;
        }

        /* Perform backspace file operation */
        rc = bsf_omatape (dev, unitstat,code);
        if (rc < 0) return -1;

        dev->blockid--;

        /* Return zero to indicate tapemark detected */
        return 0;
    }

    /* Unit check if previous block position is unknown */
    if (dev->prvblkpos < 0)
    {
        build_senseX(TAPE_BSENSE_LOADPTERR,dev,unitstat,code);
        return -1;
    }

    /* Backspace to previous block position */
    blkpos = dev->prvblkpos;

    /* Determine new previous block position */
    switch (omadesc->format)
    {
    case 'H':
        /* For OMA headers files, read the previous block header to
           extract the block length and new previous block offset */
        rc = readhdr_omaheaders (dev, omadesc, blkpos, &curblkl,
                                &prvhdro, &nxthdro, unitstat,code);
        if (rc < 0) return -1;
        break;
    case 'F':
        /* For OMA fixed block files, calculate the new previous
           block offset by subtracting the fixed block length */
        if (blkpos >= omadesc->blklen)
            prvhdro = blkpos - omadesc->blklen;
        else
            prvhdro = -1;
        break;
    case 'T':
        /* For OMA ASCII text files, new previous block is unknown */
        prvhdro = -1;
        break;
    } /* end switch(omadesc->format) */

    /* Update the offsets of the next and previous blocks */
    dev->nxtblkpos = blkpos;
    dev->prvblkpos = prvhdro;

    dev->blockid--;

    /* Return +1 to indicate backspace successful */
    return +1;

} /* end function bsb_omatape */

/*-------------------------------------------------------------------*/
/* Close an OMA tape file set                                        */
/*                                                                   */
/* All errors are ignored                                            */
/*-------------------------------------------------------------------*/
void close_omatape2(DEVBLK *dev)
{
    if (dev->fd >= 0)
    {
        WRMSG (HHC00201, "I", SSID_TO_LCSS(dev->ssid), dev->devnum, dev->filename, "oma");
        close (dev->fd);
    }
    dev->fd=-1;
    if (dev->omadesc != NULL)
    {
        free (dev->omadesc);
        dev->omadesc = NULL;
    }

    /* Reset the device dependent fields */
    dev->nxtblkpos=0;
    dev->prvblkpos=-1;
    dev->curfilen=1;
    dev->blockid=0;
    dev->fenced = 0;
    dev->omafiles = 0;
    return;
}

/*-------------------------------------------------------------------*/
/* Close an OMA tape file set                                        */
/*                                                                   */
/* All errors are ignored                                            */
/* Change the filename to '*' - unloaded                             */
/* TAPE REALLY UNLOADED                                              */
/*-------------------------------------------------------------------*/
void close_omatape(DEVBLK *dev)
{
    close_omatape2(dev);
    strlcpy( dev->filename, TAPE_UNLOADED, sizeof(dev->filename) );
    dev->blockid = 0;
    dev->fenced = 0;
    return;
}

/*-------------------------------------------------------------------*/
/* Rewind an OMA tape file set                                       */
/*                                                                   */
/* All errors are ignored                                            */
/*-------------------------------------------------------------------*/
int rewind_omatape(DEVBLK *dev,BYTE *unitstat,BYTE code)
{
    UNREFERENCED(unitstat);
    UNREFERENCED(code);
    close_omatape2(dev);
    dev->fenced = 0;
    return 0;
}

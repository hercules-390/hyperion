/* OMATAPE.C    (c) Copyright Roger Bowler, 1999-2007                */
/*              ESA/390 Tape Device Handler                          */

/* Original Author: Roger Bowler                                     */
/* Prime Maintainer: Ivan Warren                                     */
/* Secondary Maintainer: "Fish" (David B. Trout)                     */

// $Id$

/*-------------------------------------------------------------------*/
/* This module contains the OMATAPE emulated tape format support.    */
/*-------------------------------------------------------------------*/
/*                                                                   */
/* Five emulated tape formats are supported:                         */
/*                                                                   */
/* 1. AWSTAPE   This is the format used by the P/390.                */
/*              The entire tape is contained in a single flat file.  */
/*              A tape block consists of one or more block segments. */
/*              Each block segment is preceded by a 6-byte header.   */
/*              Files are separated by tapemarks, which consist      */
/*              of headers with zero block length.                   */
/*              AWSTAPE files are readable and writable.             */
/*                                                                   */
/*              Support for AWSTAPE is in the "AWSTAPE.C" member.    */
/*                                                                   */
/*                                                                   */
/* 2. OMATAPE   This is the Optical Media Attach device format.      */
/*              Each physical file on the tape is represented by     */
/*              a separate flat file.  The collection of files that  */
/*              make up the physical tape is obtained from an ASCII  */
/*              text file called the "tape description file", whose  */
/*              file name is always tapes/xxxxxx.tdf (where xxxxxx   */
/*              is the volume serial number of the tape).            */
/*              Three formats of tape files are supported:           */
/*              * FIXED files contain fixed length EBCDIC blocks     */
/*                with no headers or delimiters. The block length    */
/*                is specified in the TDF file.                      */
/*              * TEXT files contain variable length ASCII blocks    */
/*                delimited by carriage return line feed sequences.  */
/*                The data is translated to EBCDIC by this module.   */
/*              * HEADER files contain variable length blocks of     */
/*                EBCDIC data prefixed by a 16-byte header.          */
/*              The TDF file and all of the tape files must reside   */
/*              reside under the same directory which is normally    */
/*              on CDROM but can be on disk.                         */
/*              OMATAPE files are supported as read-only media.      */
/*                                                                   */
/*              OMATAPE tape Support is in the "OMATAPE.C" member.   */
/*                                                                   */
/*                                                                   */
/* 3. SCSITAPE  This format allows reading and writing of 4mm or     */
/*              8mm DAT tape, 9-track open-reel tape, or 3480-type   */
/*              cartridge on an appropriate SCSI-attached drive.     */
/*              All SCSI tapes are processed using the generalized   */
/*              SCSI tape driver (st.c) which is controlled using    */
/*              the MTIOCxxx set of IOCTL commands.                  */
/*              PROGRAMMING NOTE: the 'tape' portability macros for  */
/*              physical (SCSI) tapes MUST be used for all tape i/o! */
/*                                                                   */
/*              SCSI tape Support is in the "SCSITAPE.C" member.     */
/*                                                                   */
/*                                                                   */
/* 4. HET       This format is based on the AWSTAPE format but has   */
/*              been extended to support compression.  Since the     */
/*              basic file format has remained the same, AWSTAPEs    */
/*              can be read/written using the HET routines.          */
/*                                                                   */
/*              Support for HET is in the "HETTAPE.C" member.        */
/*                                                                   */
/*                                                                   */
/* 5. FAKETAPE  This is the format used by Fundamental Software      */
/*              on their FLEX-ES systems. It it similar to the AWS   */
/*              format. The entire tape is contained in a single     */
/*              flat file. A tape block is preceded by a 12-ASCII-   */
/*              hex-characters header which indicate the size of     */
/*              the previous and next blocks. Files are separated    */
/*              by tapemarks which consist of headers with a zero    */
/*              current block length. FakeTapes are both readable    */
/*              and writable.                                        */
/*                                                                   */
/*              Support for FAKETAPE is in the "FAKETAPE.C" member.  */
/*                                                                   */
/*-------------------------------------------------------------------*/

/*-------------------------------------------------------------------*/
/* Additional credits:                                               */
/*      3480 commands contributed by Jan Jaeger                      */
/*      Sense byte improvements by Jan Jaeger                        */
/*      3480 Read Block ID and Locate CCWs by Brandon Hill           */
/*      Unloaded tape support by Brandon Hill                    v209*/
/*      HET format support by Leland Lucius                      v209*/
/*      JCS - minor changes by John Summerfield                  2003*/
/*      PERFORM SUBSYSTEM FUNCTION / CONTROL ACCESS support by       */
/*      Adrian Trenkwalder (with futher enhancements by Fish)        */
/*      **INCOMPLETE** 3590 support by Fish (David B. Trout)         */
/*-------------------------------------------------------------------*/

/*-------------------------------------------------------------------*/
/* Reference information:                                            */
/* SC53-1200 S/370 and S/390 Optical Media Attach/2 User's Guide     */
/* SC53-1201 S/370 and S/390 Optical Media Attach/2 Technical Ref    */
/* SG24-2506 IBM 3590 Tape Subsystem Technical Guide                 */
/* GA32-0331 IBM 3590 Hardware Reference                             */
/* GA32-0329 IBM 3590 Introduction and Planning Guide                */
/* SG24-2594 IBM 3590 Multiplatform Implementation                   */
/* ANSI INCITS 131-1994 (R1999) SCSI-2 Reference                     */
/* GA32-0127 IBM 3490E Hardware Reference                            */
/* GC35-0152 EREP Release 3.5.0 Reference                            */
/* SA22-7204 ESA/390 Common I/O-Device Commands                      */
/* Flex FakeTape format (http://preview.tinyurl.com/67rgnp)          */
/*-------------------------------------------------------------------*/

// $Log$
// Revision 1.6  2008/05/22 19:25:58  fish
// Flex FakeTape support
//
// Revision 1.5  2008/03/30 02:51:33  fish
// Fix SCSI tape EOV (end of volume) processing
//
// Revision 1.4  2008/03/29 08:36:46  fish
// More complete/extensive 3490/3590 tape support
//
// Revision 1.3  2008/03/28 02:09:42  fish
// Add --blkid-24 option support, poserror flag renamed to fenced,
// added 'generic', 'readblkid' and 'locateblk' tape media handler
// call vectors.
//
// Revision 1.2  2008/03/26 07:23:51  fish
// SCSI MODS part 2: split tapedev.c: aws, het, oma processing moved
// to separate modules, CCW processing moved to separate module.
//
// Revision 1.1  2008/03/25 18:42:36  fish
// AWS, HET and OMA processing logic moved to separate modules.
// Tape device CCW processing logic also moved to separate module.
// (tapedev.c was becoming too large and unwieldy)
//
// Revision 1.133  2008/03/13 01:44:17  kleonard
// Fix residual read-only setting for tape device
//
// Revision 1.132  2008/03/04 01:10:29  ivan
// Add LEGACYSENSEID config statement to allow X'E4' Sense ID on devices
// that originally didn't support it. Defaults to off for compatibility reasons
//
// Revision 1.131  2008/03/04 00:25:25  ivan
// Ooops.. finger check on 8809 case for numdevid.. Thanks Roger !
//
// Revision 1.130  2008/03/02 12:00:04  ivan
// Re-disable Sense ID on 3410, 3420, 8809 : report came in that it breaks MTS
//
// Revision 1.129  2007/12/14 17:48:52  rbowler
// Enable SENSE ID CCW for 2703,3410,3420
//
// Revision 1.128  2007/11/29 03:36:40  fish
// Re-sequence CCW opcode 'case' statements to be in ascending order.
// COSMETIC CHANGE ONLY. NO ACTUAL LOGIC WAS CHANGED.
//
// Revision 1.127  2007/11/13 15:10:52  rbowler
// fsb_awstape support for segmented blocks
//
// Revision 1.126  2007/11/11 20:46:50  rbowler
// read_awstape support for segmented blocks
//
// Revision 1.125  2007/11/09 14:59:34  rbowler
// Move misplaced comment and restore original programming style
//
// Revision 1.124  2007/11/02 16:04:15  jmaynard
// Removing redundant #if !(defined OPTION_SCSI_TAPE).
//
// Revision 1.123  2007/09/01 06:32:24  fish
// Surround 3590 SCSI test w/#ifdef (OPTION_SCSI_TAPE)
//
// Revision 1.122  2007/08/26 14:37:17  fish
// Fix missed unfixed 31 Aug 2006 non-SCSI tape Locate bug
//
// Revision 1.121  2007/07/24 23:06:32  fish
// Force command-reject for 3590 Medium Sense and Mode Sense
//
// Revision 1.120  2007/07/24 22:54:49  fish
// (comment changes only)
//
// Revision 1.119  2007/07/24 22:46:09  fish
// Default to --blkid-32 and --no-erg for 3590 SCSI
//
// Revision 1.118  2007/07/24 22:36:33  fish
// Fix tape Synchronize CCW (x'43') to do actual commit
//
// Revision 1.117  2007/07/24 21:57:29  fish
// Fix Win32 SCSI tape "Locate" and "ReadBlockId" SNAFU
//
// Revision 1.116  2007/06/23 00:04:18  ivan
// Update copyright notices to include current year (2007)
//
// Revision 1.115  2007/04/06 15:40:25  fish
// Fix Locate Block & Read BlockId for SCSI tape broken by 31 Aug 2006 preliminary-3590-support change
//
// Revision 1.114  2007/02/25 21:10:44  fish
// Fix het_locate to continue on tapemark
//
// Revision 1.113  2007/02/03 18:58:06  gsmith
// Fix MVT tape CMDREJ error
//
// Revision 1.112  2006/12/28 03:04:17  fish
// PR# tape/100: Fix crash in "open_omatape()" in tapedev.c if bad filespec entered in OMA (TDF)  file
//
// Revision 1.111  2006/12/11 17:25:59  rbowler
// Change locblock from long to U32 to correspond with dev->blockid
//
// Revision 1.110  2006/12/08 09:43:30  jj
// Add CVS message log
//
/*-------------------------------------------------------------------*/

#include "hstdinc.h"
#include "hercules.h"  /* need Hercules control blocks               */
#include "tapedev.h"   /* Main tape handler header file              */

/*-------------------------------------------------------------------*/
//#define  ENABLE_TRACING_STMTS     // (Fish: DEBUGGING)

#ifdef ENABLE_TRACING_STMTS
  #if !defined(DEBUG)
    #warning DEBUG required for ENABLE_TRACING_STMTS
  #endif
  // (TRACE, ASSERT, and VERIFY macros are #defined in hmacros.h)
#else
  #undef  TRACE
  #undef  ASSERT
  #undef  VERIFY
  #define TRACE       1 ? ((void)0) : logmsg
  #define ASSERT(a)
  #define VERIFY(a)   ((void)(a))
#endif

/*-------------------------------------------------------------------*/
/* Read the OMA tape descriptor file                                 */
/*-------------------------------------------------------------------*/
int read_omadesc (DEVBLK *dev)
{
int             rc;                     /* Return code               */
int             i;                      /* Array subscript           */
int             pathlen;                /* Length of TDF path name   */
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
        logmsg (_("HHC232I Invalid filename %s: "
                "TDF files must be in the TAPES subdirectory\n"),
                dev->filename+pathlen);
        return -1;
    }
    pathlen -= 7;
#endif

    /* Open the tape descriptor file */
    hostpath(pathname, dev->filename, sizeof(pathname));
    fd = open (pathname, O_RDONLY | O_BINARY);
    if (fd < 0)
    {
        logmsg (_("HHCTA039E Error opening TDF file %s: %s\n"),
                dev->filename, strerror(errno));
        return -1;
    }

    /* Determine the size of the tape descriptor file */
    rc = fstat (fd, &statbuf);
    if (rc < 0)
    {
        logmsg (_("HHCTA040E %s fstat error: %s\n"),
                dev->filename, strerror(errno));
        close (fd);
        return -1;
    }
    tdfsize = statbuf.st_size;

    /* Obtain a buffer for the tape descriptor file */
    tdfbuf = malloc (tdfsize);
    if (tdfbuf == NULL)
    {
        logmsg (_("HHCTA041E Cannot obtain buffer for TDF file %s: %s\n"),
                dev->filename, strerror(errno));
        close (fd);
        return -1;
    }

    /* Read the tape descriptor file into the buffer */
    rc = read (fd, tdfbuf, tdfsize);
    if (rc < tdfsize)
    {
        logmsg (_("HHCTA042E Error reading TDF file %s: %s\n"),
                dev->filename, strerror(errno));
        free (tdfbuf);
        close (fd);
        return -1;
    }

    /* Close the tape descriptor file */
    close (fd); fd = -1;

    /* Check that the first record is a TDF header */
    if (memcmp(tdfbuf, "@TDF", 4) != 0)
    {
        logmsg (_("HHCTA043E %s is not a valid TDF file\n"),
                dev->filename);
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
        logmsg (_("HHCTA044E Cannot obtain buffer for TDF array: %s\n"),
                strerror(errno));
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
        tdffilenm = strtok (tdfrec, " \t");
        tdfformat = strtok (NULL, " \t");
        tdfreckwd = strtok (NULL, " \t");
        tdfblklen = strtok (NULL, " \t");

        /* Check for missing fields */
        if (tdffilenm == NULL || tdfformat == NULL)
        {
            logmsg (_("HHCTA045E Filename or format missing in "
                    "line %d of file %s\n"),
                    stmt, dev->filename);
            free (tdftab);
            free (tdfbuf);
            return -1;
        }

        /* Check that the file name is not too long */
        if (pathlen + 1 + strlen(tdffilenm)
                > sizeof(tdftab[filecount].filename) - 1)
        {
            logmsg (_("HHCTA046E Filename %s too long in "
                    "line %d of file %s\n"),
                    tdffilenm, stmt, dev->filename);
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
//                tdffilenm[i] = tolower(tdffilenm[i]);
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
                logmsg (_("HHCTA047E RECSIZE keyword missing in "
                        "line %d of file %s\n"),
                        stmt, dev->filename);
                free (tdftab);
                free (tdfbuf);
                return -1;
            }

            /* Check for valid fixed block length */
            if (tdfblklen == NULL
                || sscanf(tdfblklen, "%u%c", &blklen, &c) != 1
                || blklen < 1 || blklen > MAX_BLKLEN)
            {
                logmsg (_("HHCTA048E Invalid record size %s in "
                        "line %d of file %s\n"),
                        tdfblklen, stmt, dev->filename);
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
            logmsg (_("HHCTA049E Invalid record format %s in "
                    "line %d of file %s\n"),
                    tdfformat, stmt, dev->filename);
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
        logmsg (_("HHCTA050E Attempt to access beyond end of tape %s\n"),
                dev->filename);

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
    fd = open (pathname, O_RDONLY | O_BINARY);

    /* Check for successful open */
    if (fd < 0 || lseek (fd, 0, SEEK_END) > LONG_MAX)
    {
        if (fd >= 0)            /* (if open was successful, then it) */
            errno = EOVERFLOW;  /* (must have been a lseek overflow) */

        logmsg (_("HHCTA051E Error opening %s: %s\n"),
                omadesc->filename, strerror(errno));

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
        logmsg (_("HHCTA052E Error seeking to offset "I32_FMTX" "
                "in file %s: %s\n"),
                blkpos, omadesc->filename, strerror(errno));

        /* Set unit check with equipment check */
        build_senseX(TAPE_BSENSE_LOCATEERR,dev,unitstat,code);
        return -1;
    }

    /* Read the 16-byte block header */
    rc = read (dev->fd, &omahdr, sizeof(omahdr));

    /* Handle read error condition */
    if (rc < 0)
    {
        logmsg (_("HHCTA053E Error reading block header "
                "at offset "I32_FMTX" in file %s: %s\n"),
                blkpos, omadesc->filename,
                strerror(errno));

        /* Set unit check with equipment check */
        build_senseX(TAPE_BSENSE_READFAIL,dev,unitstat,code);
        return -1;
    }

    /* Handle end of file within block header */
    if (rc < (int)sizeof(omahdr))
    {
        logmsg (_("HHCTA054E Unexpected end of file in block header "
                "at offset "I32_FMTX" in file %s\n"),
                blkpos, omadesc->filename);

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
        logmsg (_("HHCTA055E Invalid block header "
                "at offset "I32_FMTX" in file %s\n"),
                blkpos, omadesc->filename);

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
        logmsg (_("HHCTA056E Error reading data block "
                "at offset "I32_FMTX" in file %s: %s\n"),
                blkpos, omadesc->filename,
                strerror(errno));

        /* Set unit check with equipment check */
        build_senseX(TAPE_BSENSE_READFAIL,dev,unitstat,code);
        return -1;
    }

    /* Handle end of file within data block */
    if (rc < curblkl)
    {
        logmsg (_("HHCTA057E Unexpected end of file in data block "
                "at offset "I32_FMTX" in file %s\n"),
                blkpos, omadesc->filename);

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
        logmsg (_("HHCTA058E Error seeking to offset "I32_FMTX" "
                "in file %s: %s\n"),
                blkpos, omadesc->filename, strerror(errno));

        /* Set unit check with equipment check */
        build_senseX(TAPE_BSENSE_LOCATEERR,dev,unitstat,code);
        return -1;
    }

    /* Read fixed length block or short final block */
    blklen = read (dev->fd, buf, omadesc->blklen);

    /* Handle read error condition */
    if (blklen < 0)
    {
        logmsg (_("HHCTA059E Error reading data block "
                "at offset "I32_FMTX" in file %s: %s\n"),
                blkpos, omadesc->filename,
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
        logmsg (_("HHCTA060E Error seeking to offset "I32_FMTX" "
                "in file %s: %s\n"),
                blkpos, omadesc->filename, strerror(errno));

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
        logmsg (_("HHCTA061E Error reading data block "
                "at offset "I32_FMTX" in file %s: %s\n"),
                blkpos, omadesc->filename,
                strerror(errno));

        build_senseX(TAPE_BSENSE_READFAIL,dev,unitstat,code);
        return -1;
    }

    /* Check for block not terminated by newline */
    if (rc < 1)
    {
        logmsg (_("HHCTA062E Unexpected end of file in data block "
                "at offset "I32_FMTX" in file %s\n"),
                blkpos, omadesc->filename);

        /* Set unit check with data check and partial record */
        build_senseX(TAPE_BSENSE_BLOCKSHORT,dev,unitstat,code);
        return -1;
    }

    /* Check for invalid zero length block */
    if (pos == 0)
    {
        logmsg (_("HHCTA063E Invalid zero length block "
                "at offset "I32_FMTX" in file %s\n"),
                blkpos, omadesc->filename);

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
        logmsg (_("HHCTA064E Error seeking to end of file %s: %s\n"),
                omadesc->filename, strerror(errno));

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
        logmsg (_("HHCTA065E Error seeking to end of file %s: %s\n"),
                omadesc->filename, strerror(errno));

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
S32             prvhdro;                /* Offset of previous header */
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
        close (dev->fd);
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
    strcpy(dev->filename,TAPE_UNLOADED);
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

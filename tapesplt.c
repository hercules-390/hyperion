/* tapesplt.C  (c) Copyright Jay Maynard, 2000-2009                 */
/*              Split AWSTAPE format tape image                      */

// $Id$

/*-------------------------------------------------------------------*/
/* This program reads an AWSTAPE format tape image file and produces */
/* output files containing pieces of it, controlled by command line  */
/* options.                                                          */
/*-------------------------------------------------------------------*/

// $Log$
// Revision 1.27  2008/11/04 04:50:46  fish
// Ensure consistent utility startup
//
// Revision 1.26  2007/06/23 00:04:18  ivan
// Update copyright notices to include current year (2007)
//
// Revision 1.25  2006/12/08 09:43:31  jj
// Add CVS message log
//

#include "hstdinc.h"

#include "hercules.h"

/*-------------------------------------------------------------------*/
/* Structure definition for AWSTAPE block header                     */
/*-------------------------------------------------------------------*/
typedef struct _AWSTAPE_BLKHDR {
        HWORD   curblkl;                /* Length of this block      */
        HWORD   prvblkl;                /* Length of previous block  */
        BYTE    flags1;                 /* Flags byte 1              */
        BYTE    flags2;                 /* Flags byte 2              */
    } AWSTAPE_BLKHDR;

/* Definitions for AWSTAPE_BLKHDR flags byte 1 */
#define AWSTAPE_FLAG1_NEWREC    0x80    /* Start of new record       */
#define AWSTAPE_FLAG1_TAPEMARK  0x40    /* Tape mark                 */
#define AWSTAPE_FLAG1_ENDREC    0x20    /* End of record             */

/*-------------------------------------------------------------------*/
/* Static data areas                                                 */
/*-------------------------------------------------------------------*/
static BYTE vollbl[] = "\xE5\xD6\xD3";  /* EBCDIC characters "VOL"   */
static BYTE hdrlbl[] = "\xC8\xC4\xD9";  /* EBCDIC characters "HDR"   */
static BYTE eoflbl[] = "\xC5\xD6\xC6";  /* EBCDIC characters "EOF"   */
static BYTE eovlbl[] = "\xC5\xD6\xE5";  /* EBCDIC characters "EOV"   */
static BYTE buf[65536];

#ifdef EXTERNALGUI
/* Report progress every this many bytes */
#define PROGRESS_MASK (~0x3FFFF /* 256K */)
/* How many bytes we've read so far. */
long  curpos = 0;
long  prevpos = 0;
#endif /*EXTERNALGUI*/

/*-------------------------------------------------------------------*/
/* tapesplt main entry point                                        */
/*-------------------------------------------------------------------*/
int main (int argc, char *argv[])
{
int             rc;                     /* Return code               */
int             i;                      /* Array subscript           */
int             len;                    /* Block length              */
int             prevlen;                /* Previous block length     */
char           *infilename;             /* -> Input file name        */
char           *outfilename;            /* -> Current out file name  */
int             infd = -1;              /* Input file descriptor     */
int             outfd = -1;             /* Current out file desc     */
int             fileno;                 /* Tape file number          */
int             blkcount;               /* Block count               */
int             curblkl;                /* Current block length      */
int             minblksz;               /* Minimum block size        */
int             maxblksz;               /* Maximum block size        */
int             outfilenum;             /* Current out file# in argv */
int             outfilecount;           /* Current # files copied    */
int             files2copy;             /* Current # files to copy   */
BYTE            labelrec[81];           /* Standard label (ASCIIZ)   */
AWSTAPE_BLKHDR  awshdr;                 /* AWSTAPE block header      */
char            pathname[MAX_PATH];     /* file path in host format  */

    INITIALIZE_UTILITY("tapesplt");

    /* Display the program identification message */
    display_version (stderr, "Hercules tape split program", FALSE);

    /* The only argument is the tape image file name */
    if (argc > 3 && argv[1] != NULL)
    {
        infilename = argv[1];
    }
    else
    {
        printf ("Usage: tapesplt infilename outfilename count [...]\n");
        exit (1);
    }

    /* Open the tape device */
    hostpath(pathname, infilename, sizeof(pathname));
    infd = open (pathname, O_RDONLY | O_BINARY);
    if (infd < 0)
    {
        printf ("tapesplt: error opening input file %s: %s\n",
                infilename, strerror(errno));
        exit (2);
    }

    /* Copy blocks from input to output files */
    fileno = 1;
    blkcount = 0;
    minblksz = 0;
    maxblksz = 0;
    len = 0;

    for (outfilenum = 2; outfilenum < argc; outfilenum += 2)
    {
        outfilename = argv[outfilenum];
        printf ("Writing output file %s.\n", outfilename);
        hostpath(pathname, outfilename, sizeof(pathname));
        outfd = open (pathname, O_WRONLY | O_CREAT | O_BINARY,
                        S_IRUSR | S_IWUSR | S_IRGRP);

        if (outfd < 0)
        {
            printf ("tapesplt: error opening output file %s: %s\n",
                    outfilename, strerror(errno));
            exit (3);
        }
        
        if (outfilenum == argc)
        {
            /* count not specified for last file, so use big number */
            files2copy = 32767;
        }
        else
        {
            files2copy = atoi(argv[outfilenum + 1]);
        }

        /* Copy just that many files */
        for (outfilecount = 0; outfilecount < files2copy; )
        {

            /* Save previous block length */
            prevlen = len;

            /* Read a block from the tape */
            len = read (infd, buf, sizeof(AWSTAPE_BLKHDR));
            if (len < 0)
            {
                printf ("tapesplt: error reading header block from %s: %s\n",
                        infilename, strerror(errno));
                exit (4);
            }

            /* Did we finish too soon? */
            if ((len > 0) && (len < (int)sizeof(AWSTAPE_BLKHDR)))
            {
                printf ("tapesplt: incomplete block header on %s\n",
                        infilename);
                exit(5);
            }

#ifdef EXTERNALGUI
            if (extgui)
            {
                curpos += len;
                /* Report progress every nnnK */
                if( ( curpos & PROGRESS_MASK ) != ( prevpos & PROGRESS_MASK ) )
                {
                    prevpos = curpos;
                    fprintf( stderr, "IPOS=%ld\n", curpos );
                }
            }
#endif /*EXTERNALGUI*/

            /* Check for end of tape. */
            if (len == 0)
            {
                printf ("End of input tape.\n");
                break;
            }

            /* Copy the header to the output file. */
            rc = write(outfd, buf, sizeof(AWSTAPE_BLKHDR));
            if (rc < (int)sizeof(AWSTAPE_BLKHDR))
            {
                printf ("tapesplt: error writing block header to %s: %s\n",
                        outfilename, strerror(errno));
                exit(6);
            }

            /* Parse the block header */
            memcpy(&awshdr, buf, sizeof(AWSTAPE_BLKHDR));

            /* Tapemark? */
            if ((awshdr.flags1 & AWSTAPE_FLAG1_TAPEMARK) != 0)
            {
                /* Print summary of current file */
                printf ("File %u: Blocks=%u, block size min=%u, max=%u\n",
                        fileno, blkcount, minblksz, maxblksz);

                /* Reset counters for next file */
                fileno++;
                minblksz = 0;
                maxblksz = 0;
                blkcount = 0;
                
                /* Count the file we just copied. */
                outfilecount++;

            }
            else /* if(tapemark) */
            {
                /* Count blocks and block sizes */
                blkcount++;
                curblkl = awshdr.curblkl[0] + (awshdr.curblkl[1] << 8);
                if (curblkl > maxblksz) maxblksz = curblkl;
                if (minblksz == 0 || curblkl < minblksz) minblksz = curblkl;

                /* Read the data block. */
                len = read (infd, buf, curblkl);
                if (len < 0)
                {
                    printf ("tapesplt: error reading data block from %s: %s\n",
                            infilename, strerror(errno));
                    exit (7);
                }

                /* Did we finish too soon? */
                if ((len > 0) && (len < curblkl))
                {
                    printf ("tapesplt: incomplete final data block on %s: "
                            "expected %d bytes, got %d\n",
                            infilename, curblkl, len);
                    exit(8);
                }

                /* Check for end of tape */
                if (len == 0)
                {
                    printf ("tapesplt: header block with no data on %s\n",
                            infilename);
                    exit(9);
                }

#ifdef EXTERNALGUI
                if (extgui)
                {
                    curpos += len;
                    /* Report progress every nnnK */
                    if( ( curpos & PROGRESS_MASK ) != ( prevpos & PROGRESS_MASK ) )
                    {
                        prevpos = curpos;
                        fprintf( stderr, "IPOS=%ld\n", curpos );
                    }
                }
#endif /*EXTERNALGUI*/

                /* Copy the header to the output file. */
                rc = write(outfd, buf, len);
                if (rc < len)
                {
                    printf ("tapesplt: error writing data block to %s: %s\n",
                            outfilename, strerror(errno));
                    exit(10);
                }

                /* Print standard labels */
                if (len == 80 && blkcount < 4
                    && (memcmp(buf, vollbl, 3) == 0
                        || memcmp(buf, hdrlbl, 3) == 0
                        || memcmp(buf, eoflbl, 3) == 0
                        || memcmp(buf, eovlbl, 3) == 0))
                {
                    for (i=0; i < 80; i++)
                        labelrec[i] = guest_to_host(buf[i]);
                    labelrec[i] = '\0';
                    printf ("%s\n", labelrec);
                }

            } /* end if(tapemark) */

        } /* end for(outfilecount) */
        
        close(outfd);

    } /* end for(outfilenum) */

    /* Close files and exit */
    close (infd);

    return 0;

} /* end function main */

/* TAPECOPY.C   (c) Copyright Roger Bowler, 1999-2005                */
/*              Convert SCSI tape into AWSTAPE format                */

/*-------------------------------------------------------------------*/
/* This program reads a SCSI tape and produces a disk file with      */
/* each block of the tape prefixed by an AWSTAPE block header.       */
/* If no disk file name is supplied, then the program simply         */
/* prints a summary of the tape files and blocksizes.                */
/*-------------------------------------------------------------------*/

#if defined(HAVE_CONFIG_H)
#include <config.h>   /* (need 1st to set build flags appropriately) */
#endif
#include "hercules.h"
#include "featall.h"  /* (need 2nd to set OPTION_SCSI_TAPE correctly)*/

/*-------------------------------------------------------------------*/
/* (if no SCSI tape support generated, do nothing)                   */
/*-------------------------------------------------------------------*/
#if !defined(OPTION_SCSI_TAPE)
int main ()
{
    printf( _("HHCTC017E SCSI tape not supported with this build\n") );
    return 0;
}
#else

/*-------------------------------------------------------------------*/
/* External GUI flag...                                              */
/*-------------------------------------------------------------------*/
#if defined(EXTERNALGUI)
/* Special flag to indicate whether or not we're being
   run under the control of the external GUI facility. */
int  extgui = 0;
time_t curr_progress_time = 0;
time_t prev_progress_time = 0;
#define PROGRESS_INTERVAL_SECS  (   3   )     /* (just what it says) */
#define MAX_BLKS                ( 13000 )     /* (just a wild guess) */
#endif /*defined(EXTERNALGUI)*/

/*-------------------------------------------------------------------*/
/* Tape ioctl areas                                                  */
/*-------------------------------------------------------------------*/
struct mtget mtget;                     /* Area for MTIOCGET ioctl   */

/*-------------------------------------------------------------------*/
/* Return Codes...                                                   */
/*-------------------------------------------------------------------*/
#define  RC_SUCCESS                                ( 0)
#define  RC_ERROR_BAD_ARGUMENTS                    ( 1)
#define  RC_ERROR_OPENING_SCSI_INPUT               ( 3)
#define  RC_ERROR_OPENING_AWS_OUTPUT               ( 4)
#define  RC_ERROR_SETTING_SCSI_VARBLK_PROCESSING   ( 5)
#define  RC_ERROR_REWINDING_SCSI                   ( 6)
#define  RC_ERROR_OBTAINING_SCSI_STATUS            ( 7)
#define  RC_ERROR_READING_SCSI_INPUT               ( 8)
#define  RC_ERROR_WRITING_OUTPUT_AWS_TM_BLOCK      ( 9)
#define  RC_ERROR_WRITING_OUTPUT_AWS_HEADER_BLOCK  (10)
#define  RC_ERROR_WRITING_OUTPUT_AWS_DATA_BLOCK    (11)

/*-------------------------------------------------------------------*/
/* Structure definition for AWSTAPE block header                     */
/*-------------------------------------------------------------------*/
typedef struct _AWSTAPE_BLKHDR
{
        HWORD   curblkl;                /* Length of this block      */
        HWORD   prvblkl;                /* Length of previous block  */
        BYTE    flags1;                 /* Flags byte 1              */
        BYTE    flags2;                 /* Flags byte 2              */
}
AWSTAPE_BLKHDR;

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

static struct mt_tape_info tapeinfo[] = MT_TAPE_INFO;

static struct mt_tape_info densinfo[] =
{
    {0x01, "NRZI (800 bpi)"              },
    {0x02, "PE (1600 bpi)"               },
    {0x03, "GCR (6250 bpi)"              },
    {0x05, "QIC-45/60 (GCR, 8000 bpi)"   },
    {0x06, "PE (3200 bpi)"               },
    {0x07, "IMFM (6400 bpi)"             },
    {0x08, "GCR (8000 bpi)"              },
    {0x09, "GCR /37871 bpi)"             },
    {0x0A, "MFM (6667 bpi)"              },
    {0x0B, "PE (1600 bpi)"               },
    {0x0C, "GCR (12960 bpi)"             },
    {0x0D, "GCR (25380 bpi)"             },
    {0x0F, "QIC-120 (GCR 10000 bpi)"     },
    {0x10, "QIC-150/250 (GCR 10000 bpi)" },
    {0x11, "QIC-320/525 (GCR 16000 bpi)" },
    {0x12, "QIC-1350 (RLL 51667 bpi)"    },
    {0x13, "DDS (61000 bpi)"             },
    {0x14, "EXB-8200 (RLL 43245 bpi)"    },
    {0x15, "EXB-8500 (RLL 45434 bpi)"    },
    {0x16, "MFM 10000 bpi"               },
    {0x17, "MFM 42500 bpi"               },
    {0x24, "DDS-2"                       },
    {0x8C, "EXB-8505 compressed"         },
    {0x90, "EXB-8205 compressed"         },
    {0,     NULL                         },
};

/*-------------------------------------------------------------------*/
/* Maximum blocksized SCSI tape I/O buffer...                        */
/*-------------------------------------------------------------------*/
static BYTE buf[ 65535 ];

/*-------------------------------------------------------------------*/
/* Subroutine to print tape status                                   */
/*-------------------------------------------------------------------*/
static void print_status (char *devname, long stat)
{
    printf (_("HHCTC015I %s status: %8.8lX"), devname, stat);

    if (GMT_EOF    ( stat )) printf (" EOF"    );
    if (GMT_BOT    ( stat )) printf (" BOT"    );
    if (GMT_EOT    ( stat )) printf (" EOT"    );
    if (GMT_SM     ( stat )) printf (" SETMARK");
    if (GMT_EOD    ( stat )) printf (" EOD"    );
    if (GMT_WR_PROT( stat )) printf (" WRPROT" );
    if (GMT_ONLINE ( stat )) printf (" ONLINE" );
    if (GMT_D_6250 ( stat )) printf (" 6250"   );
    if (GMT_D_1600 ( stat )) printf (" 1600"   );
    if (GMT_D_800  ( stat )) printf (" 800"    );
    if (GMT_DR_OPEN( stat )) printf (" NOTAPE" );

    printf ("\n");

} /* end function print_status */

/*-------------------------------------------------------------------*/
/* Subroutine to obtain and print tape status...                     */
/*                                                                   */
/* Return value:     0  ==  normal                                   */
/*                  +1  ==  end-of-tape                              */
/*                  -1  ==  error                                    */
/*-------------------------------------------------------------------*/
static int obtain_status (char *devname, int devfd)
{
int rc;                                 /* Return code               */

    rc = ioctl (devfd, MTIOCGET, (char*)&mtget);
    if (rc < 0)
    {
        if (1
            && EIO == errno
            && (0
                || GMT_EOD( mtget.mt_gstat )
                || GMT_EOT( mtget.mt_gstat )
            )
        )
            return +1;

        printf (_("HHCTC016E Error reading status of %s: rc=%d, errno=%d: %s\n"),
                devname, rc, errno, strerror(errno));
        return -1;
    }

    if (GMT_EOD( mtget.mt_gstat ) ||
        GMT_EOT( mtget.mt_gstat ))
        return +1;

    return 0;
} /* end function obtain_status */

/*-------------------------------------------------------------------*/
/* Custom exit function...                                           */
/*-------------------------------------------------------------------*/
void delayed_exit (int exit_code)
{
    /* Delay exiting is to give the system
     * time to display the error message. */
    usleep(100000);
    exit(exit_code);
}
#define  EXIT(rc)   delayed_exit(rc)   /* (use this macro to exit)   */

/*-------------------------------------------------------------------*/
/* TAPECOPY main entry point                                         */
/*-------------------------------------------------------------------*/
int main (int argc, char *argv[])
{
int             rc;                     /* Return code               */
int             i;                      /* Array subscript           */
int             len;                    /* Block length              */
int             prevlen;                /* Previous block length     */
char           *devname;                /* -> Tape device name       */
char           *filename;               /* -> Output file name       */
int             devfd;                  /* Tape file descriptor      */
int             outfd = -1;             /* Output file descriptor    */
int             fileno;                 /* Tape file number          */
int             blkcount;               /* Block count               */
int             totalblks = 0;          /* Block count               */
int             minblksz;               /* Minimum block size        */
int             maxblksz;               /* Maximum block size        */
struct mtop     opblk;                  /* Area for MTIOCTOP ioctl   */
long            density;                /* Tape density code         */
BYTE            labelrec[81];           /* Standard label (ASCIIZ)   */
AWSTAPE_BLKHDR  awshdr;                 /* AWSTAPE block header      */
int             save_errno;             /* (saved errno value)       */
int64_t         bytes_written;          /* Bytes written to o/p file */
int64_t         bytes_read;             /* Bytes read from i/p file  */
int64_t         file_bytes;             /* Byte count for curr file  */

#if defined(ENABLE_NLS)
    setlocale(LC_ALL, "");
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
#endif

    set_codepage(NULL);

#ifdef EXTERNALGUI
    if (argc >= 1 && strncmp(argv[argc-1],"EXTERNALGUI",11) == 0)
    {
        extgui = 1;
        argc--;
        setvbuf(stderr, NULL, _IONBF, 0);
        setvbuf(stdout, NULL, _IONBF, 0);
    }
#endif /*EXTERNALGUI*/

   /* Display the program identification message */
    display_version (stderr, "Hercules tape copy program ", FALSE);

    /* The first argument is the tape device name */
    if (1
        && argc > 1
        &&         argv[1]
        && strlen( argv[1]             ) >  5
        && memcmp( argv[1], "/dev/", 5 ) == 0
    )
    {
        devname = argv[1];
    }
    else
    {
        printf
        ( _(
            "\n"
//           1...5...10...15...20...25...30...35...40...45...50...55...60...65...70...75...80
            "Creates a .AWS disk file from a SCSI input tape.\n\n"

            "Tapecopy reads a SCSI i/p tape and either outputs a .aws file representation\n"
            "of the tape (if an o/p filename is given), or else simply prints a summary of\n"
            "the files and blocksizes found on the tape (if no o/p filename is specified).\n\n"

            "Usage:\n\n"

            "   tapecopy  /dev/st?  [outfile]\n\n"

            "Where:\n\n"

            "   /dev/st0     specifies the device filename of the i/p SCSI tape.\n"
            "   outfile      specifies the filename of the optional o/p .aws disk file\n"
            "                that is to be created.\n\n"

            "The i/p SCSI tape is read and processed until physical EOD (end-of-data) is\n"
            "reached (i.e. it does not stop whenever multiple tapemarks/filemarks are read;\n"
            "it continues processing until the SCSI tape drive says there is no more data\n"
            "on the tape). The resulting .aws o/p disk file, when specified for the filename\n"
            "on a Hercules tape device configuration statement, can then be used instead in\n"
            "order for the Hercules guest o/s to read the exact same data without having to\n"
            "have a SCSI tape drive physically attached to the host system. This allows you\n"
            "to easily transfer SCSI tape data to other systems that may not have SCSI tape\n"
            "drives attached to them.\n\n"

            "The possible return codes and their meaning are:\n\n"

            "   %2d           Successful completion.\n"
            "   %2d           Invalid arguments or no arguments given.\n"
            "   %2d           Unable to open I/P SCSI tape file.\n"
            "   %2d           Unable to open O/P .AWS disk file.\n"
            "   %2d           Unrecoverable I/O error setting variable length block\n"
            "                processing for i/p SCSI tape device.\n"
            "   %2d           Unrecoverable I/O error rewinding i/p SCSI tape device.\n"
            "   %2d           Unrecoverable I/O error obtaining status of i/p SCSI device.\n"
            "   %2d           Unrecoverable I/O error reading i/p SCSI tape device.\n"
            "   %2d           Unrecoverable I/O error writing 'tapemark' block\n"
            "                to o/p .AWS disk file.\n"
            "   %2d           Unrecoverable I/O error writing 'AWS block header'\n"
            "                to o/p .AWS disk file.\n"
            "   %2d           Unrecoverable I/O error writing 'data' block\n"
            "                to o/p .AWS disk file.\n"
            "\n"
            )

            ,RC_SUCCESS
            ,RC_ERROR_BAD_ARGUMENTS
            ,RC_ERROR_OPENING_SCSI_INPUT
            ,RC_ERROR_OPENING_AWS_OUTPUT
            ,RC_ERROR_SETTING_SCSI_VARBLK_PROCESSING

            ,RC_ERROR_REWINDING_SCSI
            ,RC_ERROR_OBTAINING_SCSI_STATUS
            ,RC_ERROR_READING_SCSI_INPUT
            ,RC_ERROR_WRITING_OUTPUT_AWS_TM_BLOCK

            ,RC_ERROR_WRITING_OUTPUT_AWS_HEADER_BLOCK

            ,RC_ERROR_WRITING_OUTPUT_AWS_DATA_BLOCK
        );

        EXIT( RC_ERROR_BAD_ARGUMENTS );
        return(0); /* Make gcc -Wall happy */
    }

    /* The second argument is the output file name */
    if (argc > 2 && argv[2] )
        filename = argv[2];
    else
        filename = NULL;

    /* Open the tape device */
    devfd = open (devname, O_RDONLY|O_BINARY);
    if (devfd < 0)
    {
        printf (_("HHCTC001E Error opening %s: errno=%d: %s\n"),
                devname, errno, strerror(errno));
        EXIT( RC_ERROR_OPENING_SCSI_INPUT );
    }

    SLEEP(1);

    /* Set the tape device to process variable length blocks */
    opblk.mt_op = MTSETBLK;
    opblk.mt_count = 0;
    rc = ioctl (devfd, MTIOCTOP, (char*)&opblk);
    if (rc < 0)
    {
        printf (_("HHCTC005E Error setting attributes for %s: rc=%d, errno=%d: %s\n"),
                devname, rc, errno, strerror(errno));
        EXIT( RC_ERROR_SETTING_SCSI_VARBLK_PROCESSING );
    }

    SLEEP(1);

    /* Rewind the tape to the beginning */
    opblk.mt_op = MTREW;
    opblk.mt_count = 1;
    rc = ioctl (devfd, MTIOCTOP, (char*)&opblk);
    if (rc < 0)
    {
        printf (_("HHCTC006E Error rewinding %s: rc=%d, errno=%d: %s\n"),
                devname, rc, errno, strerror(errno));
        EXIT( RC_ERROR_REWINDING_SCSI );
    }

    SLEEP(1);

    /* Obtain the tape status */
    rc = obtain_status (devname, devfd);
    if (rc < 0)
        EXIT( RC_ERROR_OBTAINING_SCSI_STATUS );

    /* Display tape status information */
    for (i = 0; tapeinfo[i].t_type != 0
                && tapeinfo[i].t_type != mtget.mt_type; i++);

    if (tapeinfo[i].t_name)
        printf (_("HHCTC003I %s device type: %s\n"), devname, tapeinfo[i].t_name);
    else
        printf (_("HHCTC003I %s device type: 0x%lX\n"), devname, mtget.mt_type);

    density = (mtget.mt_dsreg & MT_ST_DENSITY_MASK)
                >> MT_ST_DENSITY_SHIFT;

    for (i = 0; densinfo[i].t_type != 0
                && densinfo[i].t_type != density; i++);

    if (densinfo[i].t_name)
        printf (_("HHCTC004I %s tape density: %s\n"),
                devname, densinfo[i].t_name);
    else
        printf (_("HHCTC004I %s tape density code: 0x%lX\n"), devname, density);

    if (mtget.mt_gstat != 0)
        print_status (devname, mtget.mt_gstat);

    /* Open the output file */
    if (filename)
    {
        outfd = open (filename, O_WRONLY | O_CREAT | O_BINARY,
                        S_IRUSR | S_IWUSR | S_IRGRP);
        if (outfd < 0)
        {
            printf (_("HHCTC007E Error opening %s: errno=%d: %s\n"),
                    filename, errno, strerror(errno));
            EXIT( RC_ERROR_OPENING_AWS_OUTPUT );
        }
    }

    /* Copy blocks from tape to the output file */
    fileno = 1;
    blkcount = 0;
    totalblks = 0;
    minblksz = 0;
    maxblksz = 0;
    len = 0;
    bytes_read = 0;
    bytes_written = 0;
    file_bytes = 0;

#if defined(EXTERNALGUI)
    if ( extgui )
    {
        fprintf( stderr, "BLKS=%d\n", MAX_BLKS );
        prev_progress_time = time( NULL );
    }
#endif /*defined(EXTERNALGUI)*/

    while (1)
    {
#if defined(EXTERNALGUI)
        /* Issue a progress message every few seconds... */
        if ( extgui )
        {
            if ( ( curr_progress_time = time( NULL ) ) >=
                ( prev_progress_time + PROGRESS_INTERVAL_SECS ) )
            {
                prev_progress_time = curr_progress_time;
                fprintf( stderr, "BLK=%d\n", totalblks );
            }
        }
#endif /*defined(EXTERNALGUI)*/

        /* Save previous block length */
        prevlen = len;

        /* Read a block from the tape */
        len = read (devfd, buf, sizeof(buf));
        if (len < 0)
        {
            /* Determine whether end-of-tape has been read */
            save_errno = errno;
            {
                rc = obtain_status (devname, devfd);
                if (rc == +1)
                {
                    printf (_("HHCTC011I End of tape.\n"));
                    break;
                }
            }
            errno = save_errno;

            printf (_("HHCTC008E Error reading %s: errno=%d: %s\n"),
                    devname, errno, strerror(errno));
            EXIT( RC_ERROR_READING_SCSI_INPUT );
        }

        /* Check for tape mark */
        if (len == 0)
        {
            /* Write tape mark to output file */
            if (outfd >= 0)
            {
                /* Build block header for tape mark */
                awshdr.curblkl[0] = 0;
                awshdr.curblkl[1] = 0;
                awshdr.prvblkl[0] =   prevlen        & 0xFF;
                awshdr.prvblkl[1] = ( prevlen >> 8 ) & 0xFF;
                awshdr.flags1     = AWSTAPE_FLAG1_TAPEMARK;
                awshdr.flags2     = 0;

                /* Write block header to output file */
                rc = write (outfd, &awshdr, sizeof(AWSTAPE_BLKHDR));
                if (rc < (int)sizeof(AWSTAPE_BLKHDR))
                {
                    printf (_("HHCTC010E Error writing %s: rc=%d, errno=%d, %s\n"),
                            filename, rc, errno, strerror(errno));
                    EXIT( RC_ERROR_WRITING_OUTPUT_AWS_TM_BLOCK );
                } /* end if(rc) */

                bytes_written += rc;

            } /* end if(outfd) */

            /* Print summary of current file */
            if (blkcount)
            {
                ASSERT( file_bytes ); // (sanity check)

#if SIZEOF_LONG == 8
                printf (_("HHCTC009I File %u: Blocks=%u, Bytes=%ld, Block size min=%u, "
                        "max=%u, avg=%u\n"),
                        fileno, blkcount, file_bytes, minblksz, maxblksz,
                        (int)file_bytes/blkcount);
#else
                printf (_("HHCTC009I File %u: Blocks=%u, Bytes=%lld, Block size min=%u, "
                        "max=%u, avg=%u\n"),
                        fileno, blkcount, file_bytes, minblksz, maxblksz,
                        (int)file_bytes/blkcount);
#endif
            }
            else
            {
                ASSERT( !file_bytes ); // (sanity check)
            }

            /* Show the 'tapemark' AFTER the above file summary since
               that's the actual physical sequence of events; i.e. the
               file data came first THEN it was followed by a tapemark */
            printf(_("(tapemark)\n"));

            /* Reset counters for next file */
            if (blkcount)
                fileno++;
            minblksz = 0;
            maxblksz = 0;
            blkcount = 0;
            file_bytes = 0;
            continue;
        }

        /* Count blocks and block sizes */
        blkcount++;
        totalblks++;
        bytes_read += len;
        file_bytes += len;
        if (len > maxblksz) maxblksz = len;
        if (minblksz == 0 || len < minblksz) minblksz = len;

        /* Print standard labels */
        if (1
            && blkcount < 4
            && len == 80
            && (0
                || memcmp( buf, vollbl, 3 ) == 0
                || memcmp( buf, hdrlbl, 3 ) == 0
                || memcmp( buf, eoflbl, 3 ) == 0
                || memcmp( buf, eovlbl, 3 ) == 0
               )
        )
        {
            for (i=0; i < 80; i++)
                labelrec[i] = guest_to_host(buf[i]);
            labelrec[i] = '\0';
            printf (_("HHCTC012I %s\n"), labelrec);
        }
        else
        {
            ASSERT(blkcount);

#if defined(EXTERNALGUI)
            if ( !extgui )
#endif
                printf( _("File %u: Block %u\r"),
                    fileno, blkcount );
        }

        /* Write block to output file */
        if (outfd >= 0)
        {
            /* Build the block header */
            awshdr.curblkl[0] =   len            & 0xFF;
            awshdr.curblkl[1] = ( len     >> 8 ) & 0xFF;
            awshdr.prvblkl[0] =   prevlen        & 0xFF;
            awshdr.prvblkl[1] = ( prevlen >> 8 ) & 0xFF;
            awshdr.flags1     = 0
                                | AWSTAPE_FLAG1_NEWREC
                                | AWSTAPE_FLAG1_ENDREC
                                ;
            awshdr.flags2     = 0;

            /* Write block header to output file */
            rc = write (outfd, &awshdr, sizeof(AWSTAPE_BLKHDR));
            if (rc < (int)sizeof(AWSTAPE_BLKHDR))
            {
                printf (_("HHCTC013I Error writing %s: rc=%d, errno=%d: %s\n"),
                        filename, rc, errno, strerror(errno));
                EXIT( RC_ERROR_WRITING_OUTPUT_AWS_HEADER_BLOCK );
            } /* end if(rc) */

            bytes_written += rc;

            /* Write data block to output file */
            rc = write (outfd, buf, len);
            if (rc < len)
            {
                printf (_("HHCTC014I Error writing %s: rc=%d, errno=%d: %s\n"),
                        filename, rc, errno, strerror(errno));
                EXIT( RC_ERROR_WRITING_OUTPUT_AWS_DATA_BLOCK );
            } /* end if(rc) */

            bytes_written += rc;

        } /* end if(outfd) */

    } /* end while */

    /* Print run totals, close files, and exit... */

#define  ONE_MEGABYTE    ( 1024 * 1024 )
#define  HALF_MEGABYTE   ( ONE_MEGABYTE / 2 )

    printf
    (
#if SIZEOF_LONG == 8
        _(
            "HHCTC000I Successful completion;\n"
            "          Bytes read: %ld (%3.1f MB), Blocks=%u, avg=%u\n"
         )
#else
        _(
            "HHCTC000I Successful completion;\n"
            "          Bytes read: %lld (%3.1f MB), Blocks=%u, avg=%u\n"
        )
#endif
        ,           bytes_read
        ,(double) ( bytes_read    + HALF_MEGABYTE ) / (double) ONE_MEGABYTE
        ,totalblks
        ,totalblks ? (int)bytes_read/totalblks : -1
    );

    if (filename)
    {
    printf
    (
#if SIZEOF_LONG == 8
        _(
            "          Bytes written: %ld (%3.1f MB)\n"
         )
#else
        _(
            "          Bytes written: %lld (%3.1f MB)\n"
         )
#endif
        ,           bytes_written
        ,(double) ( bytes_written + HALF_MEGABYTE ) / (double) ONE_MEGABYTE
    );
    close (outfd);
    }

    close (devfd);

    EXIT( RC_SUCCESS );
    return(0);  /* Make -Wall happy */

} /* end function main */

#endif /* defined(OPTION_SCSI_TAPE) */

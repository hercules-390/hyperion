/* TAPECOPY.C   (c) Copyright Roger Bowler, 1999-2010                */
/*              Convert SCSI tape into AWSTAPE format                */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

/*              Read from AWSTAPE and write to SCSI tape mods        */
/*              Copyright 2005-2009 James R. Maynard III             */

/*-------------------------------------------------------------------*/
/* This program reads a SCSI tape and produces a disk file with      */
/* each block of the tape prefixed by an AWSTAPE block header.       */
/* If no disk file name is supplied, then the program simply         */
/* prints a summary of the tape files and blocksizes.                */
/*-------------------------------------------------------------------*/

#include "hstdinc.h"

#include "hercules.h"
#include "tapedev.h"
#include "scsitape.h"

#define UTILITY_NAME    "tapecopy"

/*-------------------------------------------------------------------*/
/* (if no SCSI tape support generated, do nothing)                   */
/*-------------------------------------------------------------------*/
#if !defined(OPTION_SCSI_TAPE)
// SYSBLK sysblk;
int main (int argc, char *argv[])
{
    UNREFERENCED(argc);
    UNREFERENCED(argv);
    printf( MSG( HHC02700, "I" ) );
    return 0;
}
#else

/*-------------------------------------------------------------------*/
/* External GUI flag...                                              */
/*-------------------------------------------------------------------*/
#if defined(EXTERNALGUI)
time_t curr_progress_time = 0;
time_t prev_progress_time = 0;
#define PROGRESS_INTERVAL_SECS  (   3   )     /* (just what it says) */
#endif /*defined(EXTERNALGUI)*/


/*-------------------------------------------------------------------*/
/* Return Codes...                                                   */
/*-------------------------------------------------------------------*/
#define  RC_SUCCESS                                ( 0)
#define  RC_ERROR_BAD_ARGUMENTS                    ( 1)
#define  RC_ERROR_OPENING_SCSI_DEVICE              ( 3)
#define  RC_ERROR_OPENING_AWS_FILE                 ( 4)
#define  RC_ERROR_SETTING_SCSI_VARBLK_PROCESSING   ( 5)
#define  RC_ERROR_REWINDING_SCSI                   ( 6)
#define  RC_ERROR_OBTAINING_SCSI_STATUS            ( 7)
#define  RC_ERROR_READING_AWS_HEADER               ( 8)
#define  RC_ERROR_READING_DATA                     ( 9)
#define  RC_ERROR_AWSTAPE_BLOCK_TOO_LARGE          (10)
#define  RC_ERROR_WRITING_TAPEMARK                 (11)
#define  RC_ERROR_WRITING_OUTPUT_AWS_HEADER_BLOCK  (12)
#define  RC_ERROR_WRITING_DATA                     (13)

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
    {0x09, "GCR (37871 bpi)"             },
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
/* Global variables used by main and the read/write functions        */
/*-------------------------------------------------------------------*/
int             len;                    /* Block length              */
int             prevlen;                /* Previous block length     */
int64_t         bytes_written;          /* Bytes written to o/p file */
char           *devnamein;              /* -> Input tape device name */
char           *devnameout;             /* -> Output tape device name*/
char           *filenamein;             /* -> Input AWS file name    */
char           *filenameout;            /* -> Output AWS file name   */

/*-------------------------------------------------------------------*/
/* Custom exit function...                                           */
/*-------------------------------------------------------------------*/
static void delayed_exit (int exit_code)
{
    if (RC_SUCCESS != exit_code)
        printf( MSG( HHC02701, "E" ) );

    /* Delay exiting is to give the system
     * time to display the error message. */
    usleep(100000);
    exit(exit_code);
}
#define  EXIT(rc)   delayed_exit(rc)   /* (use this macro to exit)   */

/*-------------------------------------------------------------------*/
/* Subroutine to print tape status                                   */
/*-------------------------------------------------------------------*/
static void print_status (char *devname, long stat)
{
    char msgbuf[256];

    MSGBUF( msgbuf, MSG( HHC02702, "I", devname, stat,
        (GMT_EOF    ( stat )) ? " EOF" : "",
        (GMT_BOT    ( stat )) ? " BOT" : "",
        (GMT_EOT    ( stat )) ? " EOT" : "",
        (GMT_SM     ( stat )) ? " SETMARK" : "",
        (GMT_EOD    ( stat )) ? " EOD" : "",
        (GMT_WR_PROT( stat )) ? " WRPROT" : "",
        (GMT_ONLINE ( stat )) ? " ONLINE" : "",
        (GMT_D_6250 ( stat )) ? " 6250" : "",
        (GMT_D_1600 ( stat )) ? " 1600" : "",
        (GMT_D_800  ( stat )) ? " 800" : "",
        (GMT_DR_OPEN( stat )) ? " NOTAPE" : "" ) );

    printf ("%s", msgbuf);

} /* end function print_status */

/*-------------------------------------------------------------------*/
/* Subroutine to print usage message                                 */
/*-------------------------------------------------------------------*/
static void print_usage (void)
{
    printf
    ( _(
        "\n"
//       1...5...10...15...20...25...30...35...40...45...50...55...60...65...70...75...80
        "Copies a SCSI tape to or from an AWSTAPE disk file.\n\n"

        "Tapecopy reads a SCSI tape and outputs an AWSTAPE file representation\n"
        "of the tape, or else reads an AWSTAPE file and creates an identical copy\n"
        "of its contents on a tape mounted on a SCSI tape drive.\n\n"

        "Usage:\n\n"

        "   tapecopy  [tapedrive] [awsfile] or\n"
        "   tapecopy  [awsfile] [tapedrive]\n\n"

        "Where:\n\n"

        "   tapedrive    specifies the device filename of the SCSI tape drive.\n"
        "                Must begin with /dev%s to be recognized.\n"
        "   awsfile      specifies the filename of the AWSTAPE disk file.\n\n"

        "The first filename is the input; the second is the output.\n\n"

        "If the input file is a SCSI tape, it is read and processed until physical EOD\n"
        "(end-of-data) is reached (i.e. it does not stop whenever multiple tapemarks or\n"
        "filemarks are read; it continues processing until the SCSI tape drive says\n"
        "there is no more data on the tape). The resulting AWSTAPE output disk file,\n"
        "when specified for the filename on a Hercules tape device configuration\n"
        "statement, can then be used instead in order for the Hercules guest O/S to\n"
        "read the exact same data without having to have a SCSI tape drive physically\n"
        "attached to the host system. This allows you to easily transfer SCSI tape data\n"
        "to other systems that may not have SCSI tape drives attached to them.\n\n"

        "The possible return codes and their meaning are:\n\n"

        "   %2d           Successful completion.\n"
        "   %2d           Invalid arguments or no arguments given.\n"
        "   %2d           Unable to open SCSI tape drive device file.\n"
        "   %2d           Unable to open AWSTAPE disk file.\n"
        "   %2d           Unrecoverable I/O error setting variable length block\n"
        "                processing for SCSI tape device.\n"
        "   %2d           Unrecoverable I/O error rewinding SCSI tape device.\n"
        "   %2d           Unrecoverable I/O error obtaining status of SCSI device.\n"
        "   %2d           Unrecoverable I/O error reading block header\n"
        "                from AWSTAPE disk file.\n"
        "   %2d           Unrecoverable I/O error reading data block.\n"
        "   %2d           AWSTAPE block size too large.\n"
        "   %2d           Unrecoverable I/O error writing tapemark.\n"
        "   %2d           Unrecoverable I/O error writing block header\n"
        "                to AWSTAPE disk file.\n"
        "   %2d           Unrecoverable I/O error writing data block.\n"
        "\n"
        )

#if defined(_MSVC_)
        ," or \\\\.\\Tape"
#else
        ,""
#endif
        ,RC_SUCCESS
        ,RC_ERROR_BAD_ARGUMENTS
        ,RC_ERROR_OPENING_SCSI_DEVICE
        ,RC_ERROR_OPENING_AWS_FILE
        ,RC_ERROR_SETTING_SCSI_VARBLK_PROCESSING

        ,RC_ERROR_REWINDING_SCSI
        ,RC_ERROR_OBTAINING_SCSI_STATUS
        ,RC_ERROR_READING_AWS_HEADER
        ,RC_ERROR_READING_DATA
        ,RC_ERROR_AWSTAPE_BLOCK_TOO_LARGE
        ,RC_ERROR_WRITING_TAPEMARK

        ,RC_ERROR_WRITING_OUTPUT_AWS_HEADER_BLOCK

        ,RC_ERROR_WRITING_DATA
    );

} /* end function print_usage */

/*-------------------------------------------------------------------*/
/* Subroutine to obtain SCSI tape status...                          */
/*                                                                   */
/* Return value:     0  ==  normal                                   */
/*                  +1  ==  end-of-tape                              */
/*                  -1  ==  error                                    */
/*-------------------------------------------------------------------*/
static int obtain_status (char *devname, int devfd, struct mtget* mtget)
{
int rc;                                 /* Return code               */

    rc = ioctl_tape (devfd, MTIOCGET, (char*)mtget);
    if (rc < 0)
    {
        if (1
            && EIO == errno
            && (0
                || GMT_EOD( mtget->mt_gstat )
                || GMT_EOT( mtget->mt_gstat )
            )
        )
            return +1;

        printf ( MSG( HHC02703, "E", devname, rc, errno, strerror(errno) ) );
        return -1;
    }

    if (GMT_EOD( mtget->mt_gstat ) ||
        GMT_EOT( mtget->mt_gstat ))
        return +1;

    return 0;
} /* end function obtain_status */

/*-------------------------------------------------------------------*/
/* Read a block from SCSI tape                                       */
/*-------------------------------------------------------------------*/
int read_scsi_tape (int devfd, void *buf, size_t bufsize, struct mtget* mtget)
{
    int rc;
    int save_errno;

    len = read_tape (devfd, buf, bufsize);
    if (len < 0)
    {
        /* Determine whether end-of-tape has been read */
        save_errno = errno;
        ASSERT( devnamein );
        rc = obtain_status (devnamein, devfd, mtget);
        if (rc == +1)
        {
            printf ( MSG( HHC02704, "I" ) );
            errno = save_errno;
            return(-1);
        }
        printf ( MSG( HHC02705, "E", devnamein, errno, strerror(errno) ) );
        EXIT( RC_ERROR_READING_DATA );
    }

    return(len);
} /* end function read_scsi_tape */

/*-------------------------------------------------------------------*/
/* Read a block from AWSTAPE disk file                               */
/*-------------------------------------------------------------------*/
int read_aws_disk (int diskfd, void *buf, size_t bufsize)
{
    AWSTAPE_BLKHDR  awshdr;                 /* AWSTAPE block header      */
    int             rc;
    unsigned int    count_read = 0;
    unsigned int    blksize;
    int             end_block;
    BYTE           *bufptr = buf;

    while (1)
    {
        /* Read block header */
        rc = read (diskfd, &awshdr, sizeof(AWSTAPE_BLKHDR));
        if (rc == 0)
        {
            printf ( MSG( HHC02706, "I", filenamein, "AWSTAPE" ) );
            return (-1);
        }
        if (rc < (int)sizeof(AWSTAPE_BLKHDR))
        {
            printf ( MSG( HHC02707, "E", filenamein, "AWSTAPE", rc, errno, strerror(errno) ) );
            EXIT( RC_ERROR_READING_AWS_HEADER );
        } /* end if(rc) */

        /* Interpret the block header */
        blksize = ((int)awshdr.curblkl[1] << 8) + awshdr.curblkl[0];
        end_block = (awshdr.flags1 & AWSTAPE_FLAG1_ENDREC) != 0;

        /* If this is a tapemark, return immediately */
        if (blksize == 0)
            return (0);

        /* Check maximum block length */
        if ((count_read + blksize) > bufsize)
        {
            printf ( MSG( HHC02708, "E", filenamein, "AWSTAPE", count_read+blksize, (int)bufsize) );
            EXIT( RC_ERROR_AWSTAPE_BLOCK_TOO_LARGE );
        } /* end if(count) */

        /* Read data block */
        rc = read (diskfd, bufptr, blksize);
        if (rc < (int)blksize)
        {
            printf ( MSG( HHC02709, "E", filenamein, "AWSTAPE", rc, errno, strerror(errno) ) );
            EXIT( RC_ERROR_READING_DATA );
        } /* end if(rc) */

        bufptr += blksize;
        count_read += blksize;
        if (end_block)
            break;
    }

    return(count_read);

} /* end function read_aws_disk */

/*-------------------------------------------------------------------*/
/* Write a block to SCSI tape                                        */
/*-------------------------------------------------------------------*/
int write_scsi_tape (int devfd, void *buf, size_t len)
{
    int                 rc;

    rc = write_tape (devfd, buf, len);
    if (rc < (int)len)
    {
        printf ( MSG( HHC02710, "E", devnameout, rc, errno, strerror(errno) ) );
        EXIT( RC_ERROR_WRITING_DATA );
    } /* end if(rc) */

    bytes_written += rc;

    return(rc);

} /* end function write_scsi_tape */

/*-------------------------------------------------------------------*/
/* Write a block to AWSTAPE disk file                                */
/*-------------------------------------------------------------------*/
int write_aws_disk (int diskfd, void *buf, size_t len)
{
    AWSTAPE_BLKHDR  awshdr;                 /* AWSTAPE block header      */
    int             rc;

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
    rc = write (diskfd, &awshdr, sizeof(AWSTAPE_BLKHDR));
    if (rc < (int)sizeof(AWSTAPE_BLKHDR))
    {
        printf ( MSG( HHC02711, "E", filenameout, "AWSTAPE", rc, errno, strerror(errno) ) );
        EXIT( RC_ERROR_WRITING_OUTPUT_AWS_HEADER_BLOCK );
    } /* end if(rc) */

    bytes_written += rc;

    /* Write data block to output file */
#if defined( _MSVC_ )
    rc = write (diskfd, buf, (u_int)len);
#else
    rc = write (diskfd, buf, len);
#endif
    if (rc < (int)len)
    {
        printf ( MSG( HHC02712, "E", filenameout, "AWSTAPE", rc, errno, strerror(errno) ) );
        EXIT( RC_ERROR_WRITING_DATA );
    } /* end if(rc) */

    bytes_written += rc;

    return(rc);
} /* end function write_aws_disk */

/*-------------------------------------------------------------------*/
/* Write a tapemark to SCSI tape                                     */
/*-------------------------------------------------------------------*/
int write_tapemark_scsi_tape (int devfd)
{
    struct mtop     opblk;                  /* Area for MTIOCTOP ioctl   */
    int             rc;

    opblk.mt_op = MTWEOF;
    opblk.mt_count = 1;
    rc = ioctl_tape (devfd, MTIOCTOP, (char*)&opblk);
    if (rc < 0)
    {
        printf ( MSG( HHC02713, "E", devnameout, rc, errno, strerror(errno) ) );
        EXIT( RC_ERROR_WRITING_TAPEMARK );
    }
    return(rc);

} /* end function write_tapemark_scsi_tape */

/*-------------------------------------------------------------------*/
/* Write a tapemark to AWSTAPE disk file                             */
/*-------------------------------------------------------------------*/
int write_tapemark_aws_disk (int diskfd)
{
    AWSTAPE_BLKHDR  awshdr;                 /* AWSTAPE block header      */
    int             rc;

    /* Build block header for tape mark */
    awshdr.curblkl[0] = 0;
    awshdr.curblkl[1] = 0;
    awshdr.prvblkl[0] =   prevlen        & 0xFF;
    awshdr.prvblkl[1] = ( prevlen >> 8 ) & 0xFF;
    awshdr.flags1     = AWSTAPE_FLAG1_TAPEMARK;
    awshdr.flags2     = 0;

    /* Write block header to output file */
    rc = write (diskfd, &awshdr, sizeof(AWSTAPE_BLKHDR));
    if (rc < (int)sizeof(AWSTAPE_BLKHDR))
    {
        printf ( MSG( HHC02714, "E", filenameout, "AWSTAPE", rc, errno, strerror(errno) ) );
       EXIT( RC_ERROR_WRITING_TAPEMARK );
    } /* end if(rc) */

    bytes_written += rc;
    return(rc);
} /* end function write_tapemark_aws_disk */

/*-------------------------------------------------------------------*/
/* TAPECOPY main entry point                                         */
/*-------------------------------------------------------------------*/
int main (int argc, char *argv[])
{
char           *pgmname;                /* prog name in host format  */
char           *pgm;                    /* less any extension (.ext) */
char           *pgmpath;                /* prog path in host format  */
char            msgbuf[512];            /* message build work area   */
int             rc;                     /* Return code               */
int             i;                      /* Array subscript           */
int             devfd;                  /* Tape file descriptor      */
int             diskfd = -1;            /* Disk file descriptor      */
int             fileno;                 /* Tape file number          */
int             blkcount;               /* Block count               */
int             totalblks = 0;          /* Block count               */
int             minblksz;               /* Minimum block size        */
int             maxblksz;               /* Maximum block size        */
struct mtop     opblk;                  /* Area for MTIOCTOP ioctl   */
long            density;                /* Tape density code         */
BYTE            labelrec[81];           /* Standard label (ASCIIZ)   */
int64_t         bytes_read;             /* Bytes read from i/p file  */
int64_t         file_bytes;             /* Byte count for curr file  */
char            pathname[MAX_PATH];     /* file name in host format  */
struct mtget    mtget;                  /* Area for MTIOCGET ioctl   */
char           *strtok_str = NULL;

#if defined(EXTERNALGUI)
struct mtpos    mtpos;                  /* Area for MTIOCPOS ioctl   */
int             is3590 = 0;             /* 1 == 3590, 0 == 3480/3490 */
#endif /*defined(EXTERNALGUI)*/

    /* Set program name */
    if ( argc > 0 )
    {
        if ( strlen(argv[0]) == 0 )
        {
            pgmname = strdup( UTILITY_NAME );
            pgmpath = strdup( "" );
        }
        else
        {
            char path[MAX_PATH];
#if defined( _MSVC_ )
            GetModuleFileName( NULL, path, MAX_PATH );
#else
            strncpy( path, argv[0], sizeof( path ) );
#endif
            pgmname = strdup(basename(path));
#if !defined( _MSVC_ )
            strncpy( path, argv[0], sizeof(path) );
#endif
            pgmpath = strdup( dirname( path  ));
        }
    }
    else
    {
            pgmname = strdup( UTILITY_NAME );
            pgmpath = strdup( "" );
    }

    pgm = strtok_r( strdup(pgmname), ".", &strtok_str);
    INITIALIZE_UTILITY( pgmname );

    /* Display the program identification message */
    MSGBUF( msgbuf, MSG_C( HHC02499, "I", pgm, "tape copy" ) );
    display_version (stderr, msgbuf+10, FALSE);

    /* The first argument is the input file name
       (either AWS disk file or SCSI tape device)
    */
    if ((argc < 2) || (argv[1] == NULL))
    {
        print_usage();
        EXIT( RC_ERROR_BAD_ARGUMENTS );
        return(0); /* Make gcc -Wall happy */
    }

    if (0
        || ( strlen( argv[1] ) > 5 && strnfilenamecmp( argv[1], "/dev/",   5 ) == 0 )
#if defined(_MSVC_)
        || ( strlen( argv[1] ) > 5 && strnfilenamecmp( argv[1], "\\dev\\", 5 ) == 0 )
#endif
        || ( strlen( argv[1] ) > 4 && strnfilenamecmp( argv[1], "\\\\.\\", 4 ) == 0 )
    )
    {
        devnamein = argv[1];
        filenamein = NULL;
    }
    else
    {
        filenamein = argv[1];
        devnamein = NULL;
    }

    /* The second argument is the output file name
       (either AWS disk file or SCSI tape device)
    */
    if (argc > 2 && argv[2] )
    {
        if (0
            || ( strlen( argv[2] ) > 5 && strnfilenamecmp( argv[2], "/dev/",   5 ) == 0 )
#if defined(_MSVC_)
            || ( strlen( argv[2] ) > 5 && strnfilenamecmp( argv[2], "\\dev\\",   5 ) == 0 )
#endif
            || ( strlen( argv[2] ) > 4 && strnfilenamecmp( argv[2], "\\\\.\\", 4 ) == 0 )
        )
        {
            devnameout = argv[2];
            filenameout = NULL;
        }
        else
        {
            filenameout = argv[2];
            devnameout = NULL;
        }
    }
    else
    {
        print_usage();
        EXIT( RC_ERROR_BAD_ARGUMENTS );
    }

    /* Check input arguments and disallow tape-to-tape or disk-to-disk copy */
    if ((!devnamein && !devnameout) || (!filenamein && !filenameout))
    {
        print_usage();
        EXIT( RC_ERROR_BAD_ARGUMENTS );
    }

    /* Open the SCSI tape device */
    if (devnamein)
    {
        hostpath( pathname, devnamein, sizeof(pathname) );
        devfd = open_tape (pathname, O_RDONLY|O_BINARY);
    }
    else // (devnameout)
    {
        hostpath( pathname, devnameout, sizeof(pathname) );
        devfd = open_tape (pathname, O_RDWR|O_BINARY);
    }
    if (devfd < 0)
    {
        printf ( MSG( HHC02715, "E", (devnamein ? devnamein : devnameout), errno, strerror(errno) ) );
        EXIT( RC_ERROR_OPENING_SCSI_DEVICE );
    }

    usleep(50000);

    /* Set the tape device to process variable length blocks */
    opblk.mt_op = MTSETBLK;
    opblk.mt_count = 0;
    rc = ioctl_tape (devfd, MTIOCTOP, (char*)&opblk);
    if (rc < 0)
    {
        printf ( MSG( HHC02716, "E", (devnamein ? devnamein : devnameout), rc, errno, strerror(errno) ) );
        EXIT( RC_ERROR_SETTING_SCSI_VARBLK_PROCESSING );
    }

    usleep(50000);

    /* Rewind the tape to the beginning */
    opblk.mt_op = MTREW;
    opblk.mt_count = 1;
    rc = ioctl_tape (devfd, MTIOCTOP, (char*)&opblk);
    if (rc < 0)
    {
        printf ( MSG( HHC02717, "E", (devnamein ? devnamein : devnameout), rc, errno, strerror(errno) ) );
        EXIT( RC_ERROR_REWINDING_SCSI );
    }

    usleep(50000);

    /* Obtain the tape status */
    rc = obtain_status ((devnamein ? devnamein : devnameout), devfd, &mtget);
    if (rc < 0)
        EXIT( RC_ERROR_OBTAINING_SCSI_STATUS );

    /* Display tape status information */
    for (i = 0; tapeinfo[i].t_type != 0
                && tapeinfo[i].t_type != mtget.mt_type; i++);

    {
        char msgbuf[64];

        if (tapeinfo[i].t_name)
            MSGBUF( msgbuf, ": %s", tapeinfo[i].t_name);
        else
            MSGBUF( msgbuf, " code: 0x%lX", mtget.mt_type);

        printf ( MSG( HHC02718, "I", (devnamein ? devnamein : devnameout), msgbuf ) );
    }

    density = (mtget.mt_dsreg & MT_ST_DENSITY_MASK)
                >> MT_ST_DENSITY_SHIFT;

    for (i = 0; densinfo[i].t_type != 0
                && densinfo[i].t_type != density; i++);

    {
        char msgbuf[64];

        if (densinfo[i].t_name)
            MSGBUF( msgbuf, ": %s", densinfo[i].t_name );
        else
            MSGBUF( msgbuf, " code: 0x%lX", density );

        printf ( MSG( HHC02719, "I", (devnamein ? devnamein : devnameout), msgbuf ) );
    }

    if (mtget.mt_gstat != 0)
        print_status ((devnamein ? devnamein : devnameout), mtget.mt_gstat);

    /* Open the disk file */
    if (filenamein)
    {
        hostpath( pathname, filenamein, sizeof(pathname) );
        diskfd = HOPEN (pathname, O_RDONLY | O_BINARY);
    }
    else
    {
        hostpath( pathname, filenameout, sizeof(pathname) );
        diskfd = HOPEN (pathname, O_WRONLY | O_CREAT | O_BINARY,
                        S_IRUSR | S_IWUSR | S_IRGRP);
    }
    if (diskfd < 0)
    {
        printf ( MSG( HHC02720, "E", (filenamein ? filenamein : filenameout), errno, strerror(errno) ) );
        EXIT( RC_ERROR_OPENING_AWS_FILE );
    }

    /* Copy blocks from input to output */
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
    // Notify the GUI of the high-end of the copy-progress range...
    if ( extgui )
    {
        // Retrieve BOT block-id...
        VERIFY( 0 == ioctl_tape( devfd, MTIOCPOS, (char*)&mtpos ) );

        is3590 = ((mtpos.mt_blkno & 0x7F000000) != 0x01000000) ? 1 : 0;

        if (!is3590)
        {
            // The seg# portion the SCSI tape physical
            // block-id number values ranges from 1 to 95...
            fprintf( stderr, "BLKS=%d\n", 95 );
        }
        else
        {
            // FIXME: 3590s (e.g. Magstar) use 32-bit block addressing,
            // and thus its block-id does not contain a seg# value, so
            // we must use some other technique. For now, we'll simply
            // presume the last block on the tape is block# 0x003FFFFF
            // (just to keep things simple).

            fprintf( stderr, "BLKS=%d\n", 0x003FFFFF );
        }

        // Init time of last issued progress message
        prev_progress_time = time( NULL );
    }
#endif /*defined(EXTERNALGUI)*/

    /* Perform the copy... */

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
                if ( ioctl_tape( devfd, MTIOCPOS, (char*)&mtpos ) == 0 )
                {
                    if (!is3590)
                        fprintf( stderr, "BLK=%ld\n", (mtpos.mt_blkno >> 24) & 0x0000007F );
                    else
                        fprintf( stderr, "BLK=%ld\n", mtpos.mt_blkno );
                }
            }
        }
#endif /*defined(EXTERNALGUI)*/

        /* Save previous block length */
        prevlen = len;

        /* Read a block */
        if (devnamein)
            len = read_scsi_tape(devfd, buf, sizeof(buf), &mtget);
        else
            len = read_aws_disk(diskfd, buf, sizeof(buf));

        /* If returned with -1, end of tape; errors are handled by the
            read functions themselves */
        if (len < 0)
            break;

        /* Check for tape mark */
        if (len == 0)
        {
            /* Write tape mark to output file */
            if (filenameout)
                write_tapemark_aws_disk(diskfd);
            else
                write_tapemark_scsi_tape(devfd);

            /* Print summary of current file */
            if (blkcount)
            {
                ASSERT( file_bytes ); // (sanity check)

                printf ( MSG( HHC02721, "I", fileno, blkcount, file_bytes, minblksz, maxblksz, (int)file_bytes/blkcount ) );
            }
            else
            {
                ASSERT( !file_bytes ); // (sanity check)
            }

            /* Show the 'tapemark' AFTER the above file summary since
               that's the actual physical sequence of events; i.e. the
               file data came first THEN it was followed by a tapemark */
            printf(_("          (tapemark)\n"));  // (align past HHCmsg#)

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
            printf ( MSG( HHC02722, "I", labelrec ) );
        }
        else
        {
            ASSERT(blkcount);

#if defined(EXTERNALGUI)
            if ( !extgui )
#endif
                printf( MSG_C( HHC02723, "I", fileno, blkcount ) );
                printf("\r");
        }

        /* Write block to output file */
        if (filenameout)
            write_aws_disk(diskfd, buf, len);
        else
            write_scsi_tape(devfd, buf, len);

    } /* end while */

    /* Print run totals, close files, and exit... */

    printf
    (
        MSG( HHC02724, "I"
        ,           bytes_read
        ,(double) ( bytes_read    + HALF_MEGABYTE ) / (double) ONE_MEGABYTE
        ,totalblks
        ,totalblks ? (int)bytes_read/totalblks : -1
        ,           bytes_written
        ,(double) ( bytes_written + HALF_MEGABYTE ) / (double) ONE_MEGABYTE
        )
    );
    close (diskfd);

    /* Rewind the tape back to the beginning again before exiting */

    opblk.mt_op = MTREW;
    opblk.mt_count = 1;

    rc = ioctl_tape (devfd, MTIOCTOP, (char*)&opblk);

    if (rc < 0)
    {
        printf ( MSG( HHC02717, "E", (devnamein ? devnamein : devnameout), rc, errno, strerror(errno) ) );
        EXIT( RC_ERROR_REWINDING_SCSI );
    }

    close_tape (devfd);

    EXIT( RC_SUCCESS );
    return(0);  /* Make -Wall happy */

} /* end function main */

#endif /* defined(OPTION_SCSI_TAPE) */

/* DEVMAP2CNF.C   (c) Copyright Jay Maynard, 2001                    */
/*              Convert P/390 DEVMAP to Hercules .CNF file           */

/*-------------------------------------------------------------------*/
/* This program reads a P/390 DEVMAP file and extracts the device    */
/* definitions from it, then writes them to the standard output in   */
/* the format Hercules uses for its .cnf file.                       */
/*-------------------------------------------------------------------*/

#include "hercules.h"

/*-------------------------------------------------------------------*/
/* Structure definition for DEVMAP controller record                 */
/*-------------------------------------------------------------------*/
typedef struct _DEVMAP_CTLR {
        BYTE    channel;                /* High order dev addr byte  */
        BYTE    name[8];                /* Name of controller program*/
        BYTE    lowdev;                 /* Low addr byte first dev   */
        BYTE    highdev;		/* Low addr byte last dev    */
        BYTE    filler1;		/* Fill byte                 */
        BYTE    type[4];		/* Type of controller        */
        BYTE    flags;                  /* Flag byte                 */
        BYTE    filler2[47];		/* More filler bytes         */
    } DEVMAP_CTLR;

/*-------------------------------------------------------------------*/
/* Structure definition for DEVMAP device record                     */
/*-------------------------------------------------------------------*/
typedef struct _DEVMAP_DEV {
        BYTE    highaddr;               /* High order dev addr byte  */
        BYTE    lowaddr;                /* Low order dev addr byte   */
        BYTE    type[4];		/* Type of device            */
	union {
	    struct {                    /* Disk devices:             */
	        BYTE filler1[4];        /* filler                    */
	        BYTE volser[6];         /* Volume serial             */
	        BYTE filler2[2];        /* more filler               */
	        BYTE filename[45];      /* name of file on disk      */
	        BYTE flags;             /* flag byte                 */
	    } disk;
	    struct {                    /* Other devices:            */
	        BYTE filler1[7];        /* fill bytes                */
	        BYTE filename[50];      /* device filename           */
	        BYTE flags;             /* flag byte                 */
	    } other;
	} parms;
    } DEVMAP_DEV;

/*-------------------------------------------------------------------*/
/* DEVMAP2CNF main entry point                                       */
/*-------------------------------------------------------------------*/
int main (int argc, char *argv[])
{
int             i;                      /* Array subscript           */
int             len;                    /* Length of actual read     */
BYTE           *filename;               /* -> Input file name        */
int             infd = -1;              /* Input file descriptor     */
DEVMAP_CTLR     controller;             /* Controller record         */
DEVMAP_DEV      device;			/* Device record             */

    /* The only argument is the DEVMAP file name */
    if (argc == 2 && argv[1] != NULL)
    {
        filename = argv[1];
    }
    else
    {
        printf ("Usage: devmap2cnf filename\n");
        exit (1);
    }

    /* Open the devmap file */
    infd = open (filename, O_RDONLY | O_BINARY);
    if (infd < 0)
    {
        printf ("devmap2cnf: Error opening %s: %s\n",
                filename, strerror(errno));
        exit (2);
    }

    /* Skip the file header */
    for (i = 0; i < 8; i++)
    {
        len = read (infd, controller, sizeof(DEVMAP_CTLR));
        if (len < 0)
        {
            printf ("devmap2cnf: error reading header records from %s: %s\n",
                    filename, strerror(errno));
            exit (3);
        }
    }

    /* Read records from the input file and convert them */
    while (1)
    {
        /* Read a controller record */
        len = read (infd, controller, sizeof(DEVMAP_CTLR));
        if (len < 0)
        {
            printf ("tapemap: error reading header block from %s: %s\n",
                    filename, strerror(errno));
            exit (3);
        }

        /* Did we finish too soon? */
        if ((len > 0) && (len < sizeof(AWSTAPE_BLKHDR)))
        {
            printf ("tapemap: incomplete block header on %s\n",
                    filename);
            exit(4);
        }
        
        /* Check for end of tape. */
        if (len == 0)
        {
            printf ("End of tape.\n");
            break;
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
                printf ("tapemap: error reading data block from %s: %s\n",
                        filename, strerror(errno));
                exit (5);
            }

            /* Did we finish too soon? */
            if ((len > 0) && (len < curblkl))
            {
                printf ("tapemap: incomplete final data block on %s, "
                        "expected %d bytes, got %d\n",
                        filename, curblkl, len);
                exit(6);
            }
        
            /* Check for end of tape */
            if (len == 0)
            {
                printf ("tapemap: header block with no data on %s\n",
                        filename);
                exit(7);
            }
        
            /* Print standard labels */
            if (len == 80 && blkcount < 4
                && (memcmp(buf, vollbl, 3) == 0
                    || memcmp(buf, hdrlbl, 3) == 0
                    || memcmp(buf, eoflbl, 3) == 0
                    || memcmp(buf, eovlbl, 3) == 0))
            {
                for (i=0; i < 80; i++)
                    labelrec[i] = ebcdic_to_ascii[buf[i]];
                labelrec[i] = '\0';
                printf ("%s\n", labelrec);
            }
            
        } /* end if(tapemark) */

    } /* end while */

    /* Close files and exit */
    close (infd);

    return 0;

} /* end function main */

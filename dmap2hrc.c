/* DMAP2HRC.C   (c) Copyright Jay Maynard, 2001-2012                 */
/*              Convert P/390 DEVMAP to Hercules config file         */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

/*-------------------------------------------------------------------*/
/* This program reads a P/390 DEVMAP file and extracts the device    */
/* definitions from it, then writes them to the standard output in   */
/* the format Hercules uses for its .cnf file.                       */
/*-------------------------------------------------------------------*/

#include "hstdinc.h"

#include "hercules.h"

#define UTILITY_NAME    "dmap2hrc"

/*-------------------------------------------------------------------*/
/* Structure definition for DEVMAP controller record                 */
/*-------------------------------------------------------------------*/
typedef struct _DEVMAP_CTLR {
        BYTE    channel;                /* High order dev addr byte  */
        BYTE    name[8];                /* Name of controller program*/
        BYTE    lowdev;                 /* Low addr byte first dev   */
        BYTE    highdev;        /* Low addr byte last dev    */
        BYTE    filler1;        /* Fill byte                 */
        BYTE    type[4];        /* Type of controller        */
        BYTE    flags;                  /* Flag byte                 */
        BYTE    filler2[47];        /* More filler bytes         */
    } DEVMAP_CTLR;

/*-------------------------------------------------------------------*/
/* Structure definition for DEVMAP device record                     */
/*-------------------------------------------------------------------*/
typedef struct _DEVMAP_DEV {
        BYTE    highaddr;               /* High order dev addr byte  */
        BYTE    lowaddr;                /* Low order dev addr byte   */
        char    type[4];        /* Type of device            */
    union {
        struct {                    /* Disk devices:             */
            BYTE filler1[4];        /* filler                    */
            BYTE volser[6];         /* Volume serial             */
            BYTE filler2[2];        /* more filler               */
            char filename[45];      /* name of file on disk      */
            BYTE flags;             /* flag byte                 */
        } disk;
        struct {                    /* Other devices:            */
            BYTE filler1[7];        /* fill bytes                */
            char filename[50];      /* device filename           */
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
char           *filename;               /* -> Input file name        */
int             infd = -1;              /* Input file descriptor     */
DEVMAP_CTLR     controller;             /* Controller record         */
DEVMAP_DEV      device;                 /* Device record             */
char            output_type[5];         /* Device type to print      */
char           *output_filename;        /* -> filename to print      */
int             more_devices;           /* More devices this ctlr?   */
char            pathname[MAX_PATH];     /* file path in host format  */

    INITIALIZE_UTILITY( UTILITY_NAME,
        "P/390 DEVMAP to Hercules conversion program", NULL );

    /* The only argument is the DEVMAP file name */
    if (argc == 2 && argv[1] != NULL)
    {
        filename = argv[1];
    }
    else
    {
        fprintf (stderr,"Usage: dmap2hrc filename\n");
        exit (1);
    }

    /* Open the devmap file */
    hostpath(pathname, filename, sizeof(pathname));
    infd = HOPEN (pathname, O_RDONLY | O_BINARY);
    if (infd < 0)
    {
        fprintf (stderr,"dmap2hrc: Error opening %s: %s\n",
                 filename, strerror(errno));
        exit (2);
    }

    /* Skip the file header */
    for (i = 0; i < 9; i++)
    {
        len = read (infd, (void *)&controller, sizeof(DEVMAP_CTLR));
        if (len < 0)
        {
            fprintf (stderr,
                     "dmap2hrc: error reading header records from %s: %s\n",
                     filename, strerror(errno));
            exit (3);
        }
    }

    /* Read records from the input file and convert them */
    while (1)
    {
        /* Read a controller record. */
        len = read (infd, (void *)&controller, sizeof(DEVMAP_CTLR));
        if (len < 0)
        {
            fprintf (stderr,
                     "dmap2hrc: error reading controller record from %s:"
                     " %s\n",
                     filename, strerror(errno));
            exit (4);
        }

        /* Did we finish too soon? */
        if ((len > 0) && (len < (int)sizeof(DEVMAP_CTLR)))
        {
            fprintf (stderr,
                     "dmap2hrc: incomplete controller record on %s\n",
                     filename);
            exit(5);
        }

        /* Check for end of file. */
        if (len == 0)
        {
            fprintf(stderr, "End of input file.\n");
            break;
        }

        /* Read devices on this controller. */
        more_devices = 1;
        while (more_devices)
        {

            /* Read a device record. */
            len = read (infd, (void *)&device, sizeof(DEVMAP_DEV));
            if (len < 0)
            {
                fprintf (stderr,
                         "dmap2hrc: error reading device record from %s:"
                         " %s\n",
                         filename, strerror(errno));
                exit (6);
            }

            /* Did we finish too soon? */
            if ((len > 0) && (len < (int)sizeof(DEVMAP_DEV)))
            {
                fprintf (stderr,
                         "dmap2hrc: incomplete device record on %s\n",
                         filename);
                exit(7);
            }

            /* Check for end of file. */
            if (len == 0)
            {
                fprintf (stderr,"dmap2hrc: premature end of input file\n");
                exit(8);
            }

        /* Is this the dummy device record at the end of the controller's
           set of devices? */
        if (strncmp(device.type,"    ",4) == 0)
        {
            more_devices = 0;
            break;
        }

        /* It's a real device. Fix the type so Hercules can use it and
           locate the output filename. */
        strncpy(output_type, device.type, 4);
        output_type[4] = '\0';
        if (isprint(device.parms.disk.volser[0]))
            output_filename = device.parms.disk.filename;
        else output_filename = device.parms.other.filename;

        if (strncmp(device.type, "3278", 4) == 0)
        {
            strcpy(output_type, "3270");
            output_filename = "";
        }
        if (strncmp(device.type, "2540", 4) == 0)
            strcpy(output_type, "3505");

        /* Emit the Hercules config file entry. */
        printf("%02X%02X    %s",
               device.highaddr, device.lowaddr,
               output_type);
        if (strlen(output_filename) > 0)
            printf("    %s", output_filename);
        puts("");   /* newline */

        } /* end while more_devices) */

    } /* end while (1) */

    /* Close files and exit */
    close (infd);

    return 0;

} /* end function main */

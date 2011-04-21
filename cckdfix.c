/* CCKDFIX.C    (c) Copyright Roger Bowler, 1999-2010                */
/*       Correct compressed CKD file.                                */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$
//
// $Log$

#include "hercules.h"
int main ( int argc, char *argv[])
{
int             fd;
CKDDASD_DEVHDR  devhdr;
CCKDDASD_DEVHDR cdevhdr;
int             heads, cyls, devt;
char            pathname[MAX_PATH];

        hostpath(pathname, argv[1], sizeof(pathname));

        fd = HOPEN (pathname, O_RDWR|O_BINARY);
        if (fd < 0) return 1;

        read (fd, &devhdr, CKDDASD_DEVHDR_SIZE);
        read (fd, &cdevhdr, CCKDDASD_DEVHDR_SIZE);

        /* --------------------------------------- */
        /* Device header updates                   */
        /* --------------------------------------- */

        /* device identifier */

//      memcpy (devhdr.devid, "CKD_C370", 8);

        /* number of heads per cylinder
           must be in little-endian byte order */

//      devhdr.heads[3] = (heads >> 24) & 0xFF;
//      devhdr.heads[2] = (heads >> 16) & 0xFF;
//      devhdr.heads[1] = (heads >>  8) & 0xFF;
//      devhdr.heads[0] = heads & 0xFF;

        /* device type -- last two digits */

//      devhdr.devtype = devt;      /* eg 0x90 for 3390 */

        /* file sequence number; must be zero for
           compressed ckd dasd emulation */

//      devhdr.fileseq = 0;

        /* highest cylinder on this file; must be zero for
           compressed ckd dasd emulation */

//      devhdr.highcyl[0] = 0;
//      devhdr.highcyl[1] = 0;

//      memset (&devhdr.resv, 0, 492);


        /* --------------------------------------- */
        /* Compressed device header updates        */
        /* --------------------------------------- */

        /* version-release-modification level */

//      cdevhdr.vrm[0] = CCKD_VERSION;
//      cdevhdr.vrm[0] = CCKD_RELEASE;
//      cdevhdr.vrm[0] = CCKD_MODLVL;

        /* options byte */

//      cdevhdr.options = 0;
//      cdevhdr.options |= CCKD_NOFUDGE;
//      cdevhdr.options |= CCKD_BIGENDIAN;
//      cdevhdr.options |= CCKD_OPENED;

        /* lookup table sizes*/

//      cdevhdr.numl1tab = (cyls * heads) >> 8;
//      if ((cyls * heads) & 0xff != 0)
//         cdevhdr.numl1tab++;
//      cdevhdr.numl2tab = 256;

        /* free space header -- set to zeroes to force
           cckdcdsk to rebuild the free space */

//      cdevhdr.size = cdevhdr.used = cdevhdr.free =
//      cdevhdr.free_total = cdevhdr.free_largest =
//      cdevhdr.free_number = cdevhdr.free_imbed = 0;


        /* number of cylinders on the emulated device
           must be in little-endian byte order */

//      cdevhdr.cyls[3] = (cyls >> 24) & 0xFF;
//      cdevhdr.cyls[2] = (cyls >> 16) & 0xFF;
//      cdevhdr.cyls[1] = (cyls >>    8) & 0xFF;
//      cdevhdr.cyls[0] = cyls & 0xFF;

//      cdevhdr.resv1 = 0;

        /* compression algorithm and parameter */

//      cdevhdr.compress = CCKD_COMPRESS_NONE;
//      cdevhdr.compress_parm = 0;

//      cdevhdr.compress = CCKD_COMPRESS_ZLIB;
//      cdevhdr.compress_parm = Z_DEFAULT_COMPRESSION;

//      cdevhdr.compress = CCKD_COMPRESS_BZIP2;
//      cdevhdr.compress_parm = 5;

//      memset (&cdevhdr.resv2, 0, 464);

        lseek (fd, 0, SEEK_SET);
        write (fd, &devhdr, CKDDASD_DEVHDR_SIZE);
        write (fd, &cdevhdr, CCKDDASD_DEVHDR_SIZE);

        close (fd);
        return 0;
}

/* CCKDCOMP.C   (c) Copyright Roger Bowler, 1999-2001                */
/*       Perform chkdsk for a Compressed CKD Direct Access Storage   */
/*       Device file.                                                */

/*-------------------------------------------------------------------*/
/* Remove all free space on a compressed ckd file                    */
/*-------------------------------------------------------------------*/

#include "hercules.h"

#ifndef NO_CCKD

#define CCKD_COMP_MAIN

#define compmsg(m, format, a...) \
 if(m>=0) fprintf (m, "cckdcomp: " format, ## a)

int cckd_chkdsk(int, FILE *, int);

int cckd_comp (int, FILE *);
#ifdef CCKD_COMP_MAIN
int syntax ();
BYTE eighthexFF[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

#ifdef EXTERNALGUI
/* Special flag to indicate whether or not we're being
   run under the control of the external GUI facility. */
int  extgui = 0;
#endif /*EXTERNALGUI*/

/*-------------------------------------------------------------------*/
/* Main function for stand-alone compress                            */
/*-------------------------------------------------------------------*/

int main (int argc, char *argv[])
{
int             rc;                     /* Return code               */
char           *fn;                     /* File name                 */
int             fd;                     /* File descriptor           */
int             level=-1;               /* Level for chkdsk          */

    /* Display the program identification message */
    display_version (stderr, "Hercules cckd compress program ");

    /* parse the arguments */
#ifdef EXTERNALGUI
    if (argc >= 1 && strncmp(argv[argc-1],"EXTERNALGUI",11) == 0)
    {
        extgui = 1;
        argc--;
    }
#endif /*EXTERNALGUI*/
    for (argc--, argv++ ; argc > 0 ; argc--, argv++)
    {
        if(**argv != '-') break;

        switch(argv[0][1])
        {
            case '0':
            case '1':
            case '3':  if (argv[0][2] != '\0') return syntax ();
                       level = (argv[0][1] & 0xf);
                       break;
            default:   return syntax ();
        }
    }
    if (argc != 1) return syntax ();
    fn = argv[0];

    /* open the file */
    fd = open (fn, O_RDWR|O_BINARY);
    if (fd < 0)
    {
        fprintf (stderr,
                 "cckdcomp: error opening file %s: %s\n",
                 fn, strerror(errno));
        return -1;
    }

    /* call chkdsk() if level was specified */
    if (level >= 0)
    {
        rc = cckd_chkdsk (fd, stderr, level);
        if (rc < 0)
        {
            fprintf (stderr,
               "cckdcomp: terminating due to chkdsk errors%s\n", "");
            return -1;
        }
    }

    /* call the actual compress function */
    rc = cckd_comp (fd, stderr);

    close (fd);

    return rc;

}

/*-------------------------------------------------------------------*/
/* print syntax                                                      */
/*-------------------------------------------------------------------*/

int syntax()
{
    fprintf (stderr, "cckdcomp [-level] file-name\n"
                "\n"
                "       where level is a digit 0 - 3\n"
                "       specifying the cckdcdsk level:\n"
                "         0  --  minimal checking\n"
                "         1  --  normal  checking\n"
                "         3  --  maximal checking\n");
    return -1;
}
#endif

/*-------------------------------------------------------------------*/
/* Remove all free space from a compressed ckd file                  */
/*-------------------------------------------------------------------*/

int cckd_comp (int fd, FILE *m)
{
int             rc;                     /* Return code               */
off_t           pos;                    /* Current file offset       */
int             i;                      /* Loop index                */
CKDDASD_DEVHDR  devhdr;                 /* CKD device header         */
CCKDDASD_DEVHDR cdevhdr;                /* Compressed CKD device hdr */
CCKD_L1ENT     *l1=NULL;                /* -> Primary lookup table   */
int             l1tabsz;                /* Primary lookup table size */
CCKD_L2ENT      l2;                     /* Secondary lookup table    */
CCKD_FREEBLK    fb;                     /* Free space block          */
BYTE           *buf=NULL;               /* Buffers for track image   */
int             len;                    /* Space  length             */
int             trksz;                  /* Maximum track size        */
int             heads;                  /* Heads per cylinder        */
int             trk;                    /* Track number              */
int             l1x,l2x;                /* Lookup indices            */
int             freed=0;                /* Total space freed         */
int             imbedded=0;             /* Imbedded space freed      */
int             moved=0;                /* Total space moved         */

/*-------------------------------------------------------------------*/
/* Read the headers and level 1 table                                */
/*-------------------------------------------------------------------*/

    rc = lseek (fd, 0, SEEK_SET);
    rc = read (fd, &devhdr, CKDDASD_DEVHDR_SIZE);
    rc = read (fd, &cdevhdr, CCKDDASD_DEVHDR_SIZE);

    /* Check the endianess of the file */
    if (((cdevhdr.options & CCKD_BIGENDIAN) != 0 && cckd_endian() == 0) ||
        ((cdevhdr.options & CCKD_BIGENDIAN) == 0 && cckd_endian() != 0))
    {
        rc = cckd_swapend (fd, m);
        rc = lseek (fd, CCKD_L1TAB_POS, SEEK_SET);
    }

    if (cdevhdr.free_number == 0)
    {
        compmsg (m, "file has no free space%s\n", "");
        return 0;
    }
    l1tabsz = cdevhdr.numl1tab * CCKD_L1ENT_SIZE;
    l1 = malloc (l1tabsz);
    rc = read (fd, l1, l1tabsz);

    trksz = ((U32)(devhdr.trksize[3]) << 24)
          | ((U32)(devhdr.trksize[2]) << 16)
          | ((U32)(devhdr.trksize[1]) << 8)
          | (U32)(devhdr.trksize[0]);

    heads = ((U32)(devhdr.heads[3]) << 24)
          | ((U32)(devhdr.heads[2]) << 16)
          | ((U32)(devhdr.heads[1]) << 8)
          | (U32)(devhdr.heads[0]);

    buf = malloc(trksz);

/*-------------------------------------------------------------------*/
/* Relocate spaces                                                   */
/*-------------------------------------------------------------------*/

    /* figure out where to start; if imbedded free space
       (ie old format), start right after the level 1 table,
       otherwise start at the first free space                       */
    if (cdevhdr.free_imbed) pos = CCKD_L1TAB_POS + l1tabsz;
    else pos = cdevhdr.free;

#ifdef EXTERNALGUI
    if (extgui) fprintf (stderr,"SIZE=%d\n",cdevhdr.size);
#endif /*EXTERNALGUI*/

    /* process each space in file sequence; the only spaces we expect
       are free blocks, level 2 tables, and track images             */
    for ( ; pos + freed < cdevhdr.size; pos += len)
    {
#ifdef EXTERNALGUI
        if (extgui) fprintf (stderr,"POS=%lu\n",pos);
#endif /*EXTERNALGUI*/

        /* check for free space */
        if (pos + freed == cdevhdr.free)
        { /* space is free space */
            rc = lseek (fd, pos + freed, SEEK_SET);
            rc = read (fd, &fb, CCKD_FREEBLK_SIZE);
            cdevhdr.free = fb.pos;
            cdevhdr.free_number--;
            cdevhdr.free_total -= fb.len;
            freed += fb.len;
            len = 0;
            continue;
        }

        /* check for l2 table */
        for (i = 0; i < cdevhdr.numl1tab; i++)
            if (l1[i] == pos + freed) break;
        if (i < cdevhdr.numl1tab)
        { /* space is a l2 table */
            len = CCKD_L2TAB_SIZE;
            if (freed)
            {
                rc = lseek (fd, l1[i], SEEK_SET);
                rc = read  (fd, buf, len);
                l1[i] -= freed;
                rc = lseek (fd, l1[i], SEEK_SET);
                rc = write (fd, buf, len);
                moved += len;
            }
            continue;
        }

        /* check for track image */
        rc = lseek (fd, pos + freed, SEEK_SET);
        rc = read  (fd, buf, 8);
        trk = ((buf[1] << 8) + buf[2]) * heads
            + ((buf[3] << 8) + buf[4]);
        l1x = trk >> 8;
        l2x = trk & 0xff;
        l2.pos = l2.len = l2.size = 0;
        if (l1[l1x])
        {
            rc = lseek (fd, l1[l1x] + l2x * CCKD_L2ENT_SIZE, SEEK_SET);
            rc = read (fd, &l2, CCKD_L2ENT_SIZE);
        }
        if (l2.pos == pos + freed)
        { /* space is a track image */
            len = l2.len;
            imbedded = l2.size - l2.len;
            if (freed)
            {
                rc = lseek (fd, l2.pos, SEEK_SET);
                rc = read  (fd, buf, len);
                l2.pos -= freed;
                rc = lseek (fd, l2.pos, SEEK_SET);
                rc = write (fd, buf, len);
            }
            if (freed || imbedded )
            {
                l2.size = l2.len;
                rc = lseek (fd, l1[l1x] + l2x * CCKD_L2ENT_SIZE, SEEK_SET);
                rc = write (fd, &l2, CCKD_L2ENT_SIZE);
            }
            cdevhdr.free_imbed -= imbedded;
            cdevhdr.free_total -= imbedded;
            freed += imbedded;
            moved += len;
            continue;
         }

         /* space is unknown -- have to punt
            if we have freed some space, then add a free space */
         compmsg (m, "unknown space at offset 0x%lx : "
                  "%2.2x%2.2x%2.2x%2.2x %2.2x%2.2x%2.2x%2.2x\n",
                  pos + freed, buf[0], buf[1], buf[2], buf[3],
                  buf[4], buf[5], buf[6], buf[7]);
         if (freed)
         {
             fb.pos = cdevhdr.free;
             fb.len = freed;
             rc = lseek (fd, pos, SEEK_SET);
             rc = write (fd, &fb, CCKD_FREEBLK_SIZE);
             cdevhdr.free = pos;
             cdevhdr.free_number++;
             cdevhdr.free_total += freed;
             freed = 0;
         }
         break;
    }

    /* update the largest free size -- will be zero unless we punted */
    cdevhdr.free_largest = 0;
    for (pos = cdevhdr.free; pos; pos = fb.pos)
    {
#ifdef EXTERNALGUI
        if (extgui) fprintf (stderr,"POS=%lu\n",pos);
#endif /*EXTERNALGUI*/
        rc = lseek (fd, pos, SEEK_SET);
        rc = read (fd, &fb, CCKD_FREEBLK_SIZE);
        if (fb.len > cdevhdr.free_largest)
            cdevhdr.free_largest = fb.len;
    }

    /* write the compressed header and l1 table and truncate the file */
    cdevhdr.options |= CCKD_NOFUDGE;
    if (cdevhdr.free_imbed == 0)
    {
        cdevhdr.vrm[0] = CCKD_VERSION;
        cdevhdr.vrm[1] = CCKD_RELEASE;
        cdevhdr.vrm[2] = CCKD_MODLVL;
    }
    cdevhdr.size -= freed;
    rc = lseek (fd, CKDDASD_DEVHDR_SIZE, SEEK_SET);
    rc = write (fd, &cdevhdr, CCKDDASD_DEVHDR_SIZE);
    rc = write (fd, l1, l1tabsz);
    rc = ftruncate (fd, cdevhdr.size);

    compmsg (m, "file %ssuccessfully compacted, %d freed and %d moved\n",
             freed ? "" : "un", freed, moved);
   free (buf);
   free (l1);

   return (freed ? freed : -1);
}

#else /* NO_CCKD */

#ifdef CCKD_COMP_MAIN
int main ( int argc, char *argv[])
{
    fprintf (stderr, "cckdcomp support not generated\n");
    return -1;
}
#endif

#endif

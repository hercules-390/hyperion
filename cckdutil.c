/* CCKDUTIL.C   (c) Copyright Roger Bowler, 1999-2004                */
/*       ESA/390 Compressed CKD Common routines                      */

/*-------------------------------------------------------------------*/
/* This module contains functions for compressed CKD devices         */
/* used by more than 1 main program.                                 */
/*-------------------------------------------------------------------*/

#include "hercules.h"
#include "opcode.h"

#define ENDMSG(m, format, a...) \
   if(m!=NULL) fprintf (m, "cckdend: " format, ## a)

#define COMPMSG(m, format, a...) \
 if(m!=NULL) fprintf (m, "cckdcomp: " format, ## a)

#define CDSKMSG(m, format, a...) \
 if(m!=NULL) fprintf (m, "cckdcdsk: " format, ## a)

typedef struct _SPCTAB {                /* Space table               */
off_t           pos;                    /* Space offset              */
long long       len;                    /* Space length              */
long long       siz;                    /* Space size                */
int             val;                    /* Value for space           */
void           *ptr;                    /* Pointer to recovered space*/
int             typ;                    /* Type of space             */
               } SPCTAB;

#define SPCTAB_NONE     0               /* Ignore this space entry   */
#define SPCTAB_DEVHDR   1               /* Space is device header    */
#define SPCTAB_CDEVHDR  2               /* Space is compressed hdr   */
#define SPCTAB_L1TAB    3               /* Space is level 1 table    */
#define SPCTAB_L2TAB    4               /* Space is level 2 table    */
#define SPCTAB_TRKIMG   5               /* Space is track image      */
#define SPCTAB_FREE     6               /* Space is free block       */
#define SPCTAB_END      7               /* Space is end-of-file      */

/*-------------------------------------------------------------------*/
/* Internal functions                                                */
/*-------------------------------------------------------------------*/

int cdsk_spctab_comp(const void *, const void *);
int cdsk_rcvtab_comp(const void *, const void *);
int cdsk_valid_trk (int, BYTE *, int, int, int, BYTE *);
int cdsk_recover_trk (int, BYTE *, int, int, int, int, int, int *);
int cdsk_build_gap (SPCTAB *, int *, SPCTAB *);
int cdsk_build_gap_long (SPCTAB *, int *, SPCTAB *);

/*-------------------------------------------------------------------*/
/* Static data areas                                                 */
/*-------------------------------------------------------------------*/
static  BYTE eighthexFF[] = {0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff};

/*-------------------------------------------------------------------*/
/* Change the endianess of a compressed file                         */
/*-------------------------------------------------------------------*/
int cckd_swapend (int fd, FILE *m)
{
int               rc;                   /* Return code               */
int               i;                    /* Index                     */
CCKDDASD_DEVHDR   cdevhdr;              /* Compressed ckd header     */
CCKD_L1ENT       *l1;                   /* Level 1 table             */
CCKD_L2ENT        l2[256];              /* Level 2 table             */
CCKD_FREEBLK      fb;                   /* Free block                */
int               swapend;              /* 1 = New endianess doesn't
                                             match machine endianess */
int               n;                    /* Level 1 table entries     */
U32               o;                    /* Level 2 table offset      */

    /* fix the compressed ckd header */

    rc = lseek (fd, CKDDASD_DEVHDR_SIZE, SEEK_SET);
    if (rc == -1)
    {
        ENDMSG (m, "lseek error fd %d offset %lld: %s\n",
                fd, (long long)CKDDASD_DEVHDR_SIZE, strerror(errno));
        return -1;
    }

    rc = read (fd, &cdevhdr, CCKDDASD_DEVHDR_SIZE);
    if (rc != CCKDDASD_DEVHDR_SIZE)
    {
        ENDMSG (m, "cdevhdr read error fd %d: %s\n",
                fd, strerror(errno));
        return -1;
    }

    cckd_swapend_chdr (&cdevhdr);

    rc = lseek (fd, CKDDASD_DEVHDR_SIZE, SEEK_SET);
    if (rc == -1)
    {
        ENDMSG (m, "lseek error fd %d offset %lld: %s\n",
                fd, (long long)CKDDASD_DEVHDR_SIZE, strerror(errno));
        return -1;
    }

    cdevhdr.options |= CCKD_ORDWR;
    rc = write (fd, &cdevhdr, CCKDDASD_DEVHDR_SIZE);
    if (rc != CCKDDASD_DEVHDR_SIZE)
    {
        ENDMSG (m, "cdevhdr write error fd %d: %s\n",
                fd, strerror(errno));
        return -1;
    }

    /* Determine need to swap */

    swapend = (cckd_endian() !=
               ((cdevhdr.options & CCKD_BIGENDIAN) != 0));

    /* fix the level 1 table */

    n = cdevhdr.numl1tab;
    if (swapend) cckd_swapend4((char *)&n);

    l1 = malloc (n * CCKD_L1ENT_SIZE);
    if (l1 == NULL)
    {
        ENDMSG (m, "l1tab malloc error fd %d size %ud: %s\n",
               fd, (unsigned int)(n * CCKD_L1ENT_SIZE), strerror(errno));
        return -1;
    }

    rc = lseek (fd, CCKD_L1TAB_POS, SEEK_SET);
    if (rc == -1)
    {
        ENDMSG (m, "lseek error fd %d offset %lld: %s\n",
                fd, (long long)CCKD_L1TAB_POS, strerror(errno));
        free (l1);
        return -1;
    }

    rc = read (fd, l1, n * CCKD_L1ENT_SIZE);
    if (rc != (int)(n * CCKD_L1ENT_SIZE))
    {
        ENDMSG (m, "l1tab read error fd %d: %s\n",
                fd, strerror(errno));
        free (l1);
        return -1;
    }

    cckd_swapend_l1 (l1, n);

    rc = lseek (fd, CCKD_L1TAB_POS, SEEK_SET);
    if (rc == -1)
    {
        ENDMSG (m, "lseek error fd %d offset %lld: %s\n",
                fd, (long long)CCKD_L1TAB_POS, strerror(errno));
        free (l1);
        return -1;
    }

    rc = write (fd, l1, n * CCKD_L1ENT_SIZE);
    if (rc != (int)(n * CCKD_L1ENT_SIZE))
    {
        ENDMSG (m, "l1tab write error fd %d: %s\n",
                fd, strerror(errno));
        free (l1);
        return -1;
    }

    /* fix the level 2 tables */

    for (i=0; i < n; i++)
    {
        o = l1[i];
        if (swapend) cckd_swapend4((char *)&o);

        if (o != 0 && o != 0xffffffff)
        {
            rc = lseek (fd, (off_t)o, SEEK_SET);
            if (rc == -1)
            {
                ENDMSG (m, "lseek error fd %d offset %lld: %s\n",
                        fd, (long long)o, strerror(errno));
                free (l1);
                return -1;
            }

            rc = read (fd, &l2, CCKD_L2TAB_SIZE);
            if (rc != CCKD_L2TAB_SIZE)
            {
                ENDMSG (m, "l2[%d] read error, offset %lld bytes read %d : %s\n",
                        i, (long long)o, rc, strerror(errno));
                free (l1);
                return -1;
            }

            cckd_swapend_l2 ((CCKD_L2ENT *)&l2);

            rc = lseek (fd, (off_t)o, SEEK_SET);
            if (rc == -1)
            {
                ENDMSG (m, "lseek error fd %d offset %lld: %s\n",
                        fd, (long long)o, strerror(errno));
                free (l1);
                return -1;
            }

            rc = write (fd, &l2, CCKD_L2TAB_SIZE);
            if (rc != CCKD_L2TAB_SIZE)
            {
                ENDMSG (m, "l2[%d] write error fd %d (%d): %s\n",
                        i, fd, rc, strerror(errno));
                free (l1);
                return -1;
            }
        }
    }
    free (l1);

    /* fix the free chain */
    for (o = cdevhdr.free; o != 0; o = fb.pos)
    {
        if (swapend) cckd_swapend4 ((char *)&o);

        rc = lseek (fd, o, SEEK_SET);
        if (rc == -1)
        {
            ENDMSG (m, "lseek error fd %d offset %lld: %s\n",
                    fd, (long long)o, strerror(errno));
            return -1;
        }

        rc = read (fd, &fb, CCKD_FREEBLK_SIZE);
        if (rc != CCKD_FREEBLK_SIZE)
        {
            ENDMSG (m, "free block read error fd %d: %s\n",
                    fd, strerror(errno));
            return -1;
        }

        cckd_swapend_free (&fb);

        rc = lseek (fd, (off_t)o, SEEK_SET);
        if (rc == -1)
        {
            ENDMSG (m, "lseek error fd %d offset %lld: %s\n",
                    fd, (long long)o, strerror(errno));
            return -1;
        }

        rc = write (fd, &fb, CCKD_FREEBLK_SIZE);
        if (rc != CCKD_FREEBLK_SIZE)
        {
            ENDMSG (m, "free block write error fd %d: %s\n",
                    fd, strerror(errno));
            return -1;
        }
    }
    return 0;
}


/*-------------------------------------------------------------------*/
/* Swap endian - compressed device header                            */
/*-------------------------------------------------------------------*/
void cckd_swapend_chdr (CCKDDASD_DEVHDR *cdevhdr)
{
    /* fix the compressed ckd header */
    cdevhdr->options ^= CCKD_BIGENDIAN;
    cckd_swapend4 ((char *) &cdevhdr->numl1tab);
    cckd_swapend4 ((char *) &cdevhdr->numl2tab);
    cckd_swapend4 ((char *) &cdevhdr->size);
    cckd_swapend4 ((char *) &cdevhdr->used);
    cckd_swapend4 ((char *) &cdevhdr->free);
    cckd_swapend4 ((char *) &cdevhdr->free_total);
    cckd_swapend4 ((char *) &cdevhdr->free_largest);
    cckd_swapend4 ((char *) &cdevhdr->free_number);
    cckd_swapend4 ((char *) &cdevhdr->free_imbed);
    cckd_swapend2 ((char *) &cdevhdr->compress_parm);
}


/*-------------------------------------------------------------------*/
/* Swap endian - level 1 table                                       */
/*-------------------------------------------------------------------*/
void cckd_swapend_l1 (CCKD_L1ENT *l1, int n)
{
int i;                                  /* Index                     */

    for (i=0; i < n; i++)
        cckd_swapend4 ((char *) &l1[i]);
}


/*-------------------------------------------------------------------*/
/* Swap endian - level 2 table                                       */
/*-------------------------------------------------------------------*/
void cckd_swapend_l2 (CCKD_L2ENT *l2)
{
int i;                                  /* Index                     */

    for (i=0; i < 256; i++)
    {
        cckd_swapend4 ((char *) &l2[i].pos);
        cckd_swapend2 ((char *) &l2[i].len);
        cckd_swapend2 ((char *) &l2[i].size);
    }
}


/*-------------------------------------------------------------------*/
/* Swap endian - free space entry                                    */
/*-------------------------------------------------------------------*/
void cckd_swapend_free (CCKD_FREEBLK *fb)
{
    cckd_swapend4 ((char *) &fb->pos);
    cckd_swapend4 ((char *) &fb->len);
}


/*-------------------------------------------------------------------*/
/* Swap endian - 4 bytes                                             */
/*-------------------------------------------------------------------*/
void cckd_swapend4 (char *c)
{
 char temp[4];

    memcpy (&temp, c, 4);
    c[0] = temp[3];
    c[1] = temp[2];
    c[2] = temp[1];
    c[3] = temp[0];
}


/*-------------------------------------------------------------------*/
/* Swap endian - 2 bytes                                             */
/*-------------------------------------------------------------------*/
void cckd_swapend2 (char *c)
{
 char temp[2];

    memcpy (&temp, c, 2);
    c[0] = temp[1];
    c[1] = temp[0];
}


/*-------------------------------------------------------------------*/
/* Are we little or big endian?  From Harbison&Steele.               */
/*-------------------------------------------------------------------*/
int cckd_endian()
{
union
{
    long l;
    char c[sizeof (long)];
}   u;

    u.l = 1;
    return (u.c[sizeof (long) - 1] == 1);
}

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
int             len;                    /* Space  length             */
int             trksz;                  /* Maximum track size        */
int             heads;                  /* Heads per cylinder        */
int             blks;                   /* Number fba blocks         */
int             trk;                    /* Track number              */
int             l1x,l2x;                /* Lookup indices            */
int             freed=0;                /* Total space freed         */
int             imbedded=0;             /* Imbedded space freed      */
int             moved=0;                /* Total space moved         */
int             ckddasd;                /* 1=CKD dasd  0=FBA dasd    */
BYTE            buf[65536];             /* Buffer                    */
#ifdef EXTERNALGUI
int extgui2 = (m!= NULL && fileno(m) == fileno(stderr) && extgui);
#endif

/*-------------------------------------------------------------------*/
/* Read the headers and level 1 table                                */
/*-------------------------------------------------------------------*/

restart:
    /* Read the headers */
    rc = lseek (fd, 0, SEEK_SET);
    if (rc < 0)
    {
        COMPMSG (m, "lseek error offset 0: %s\n",strerror(errno));
        return -1;
    }
    rc = read (fd, &devhdr, CKDDASD_DEVHDR_SIZE);
    if (rc < (int)CKDDASD_DEVHDR_SIZE)
    {
        COMPMSG (m, "devhdr read error: %s\n",strerror(errno));
        return -1;
    }
    if (memcmp (devhdr.devid, "CKD_C370", 8) == 0
     || memcmp (devhdr.devid, "CKD_S370", 8) == 0)
        ckddasd = 1;
    else if (memcmp (devhdr.devid, "FBA_C370", 8) == 0
          || memcmp (devhdr.devid, "FBA_S370", 8) == 0)
             ckddasd = 0;
    else
    {
        COMPMSG (m, "incorrect header id\n");
        return -1;
    }
    rc = read (fd, &cdevhdr, CCKDDASD_DEVHDR_SIZE);
    if (rc < (int)CCKDDASD_DEVHDR_SIZE)
    {
        COMPMSG (m, "cdevhdr read error: %s\n",strerror(errno));
        return -1;
    }
    cdevhdr.options |= CCKD_ORDWR;

    /* Check the endianess of the file */
    if (((cdevhdr.options & CCKD_BIGENDIAN) != 0 && cckd_endian() == 0) ||
        ((cdevhdr.options & CCKD_BIGENDIAN) == 0 && cckd_endian() != 0))
    {
        rc = cckd_swapend (fd, m);
        if (rc < 0) return -1;
        goto restart;
    }

    if (cdevhdr.free_number == 0)
    {
        COMPMSG (m, "file has no free space%s\n", "");
        return 0;
    }
    l1tabsz = cdevhdr.numl1tab * CCKD_L1ENT_SIZE;
    l1 = malloc (l1tabsz);
    if (l1 == NULL)
    {
        COMPMSG (m, "l1 table malloc error: %s\n",strerror(errno));
        return -1;
    }
    rc = read (fd, l1, l1tabsz);
    if (rc < (int)l1tabsz)
    {
        COMPMSG (m, "l1 table read error: %s\n",strerror(errno));
        return -1;
    }

    if (ckddasd)
    {
        trksz = ((U32)(devhdr.trksize[3]) << 24)
              | ((U32)(devhdr.trksize[2]) << 16)
              | ((U32)(devhdr.trksize[1]) << 8)
              | (U32)(devhdr.trksize[0]);

        heads = ((U32)(devhdr.heads[3]) << 24)
              | ((U32)(devhdr.heads[2]) << 16)
              | ((U32)(devhdr.heads[1]) << 8)
              | (U32)(devhdr.heads[0]);
        blks = -1;
    }
    else
    {
        trksz = CFBA_BLOCK_SIZE + CKDDASD_TRKHDR_SIZE;
        heads = -1;
        blks  = ((U32)(cdevhdr.cyls[0]) << 24)
              | ((U32)(cdevhdr.cyls[2]) << 16)
              | ((U32)(cdevhdr.cyls[1]) << 8)
              | (U32)(cdevhdr.cyls[0]);
    }

/*-------------------------------------------------------------------*/
/* Relocate spaces                                                   */
/*-------------------------------------------------------------------*/

    /* figure out where to start; if imbedded free space
       (ie old format), start right after the level 1 table,
       otherwise start at the first free space                       */
    if (cdevhdr.free_imbed) pos = (off_t)(CCKD_L1TAB_POS + l1tabsz);
    else pos = (off_t)cdevhdr.free;

#ifdef EXTERNALGUI
    if (extgui2) fprintf (stderr,"SIZE=%d\n",cdevhdr.size);
#endif /*EXTERNALGUI*/

    /* process each space in file sequence; the only spaces we expect
       are free blocks, level 2 tables, and track images             */
    for ( ; pos + freed < (off_t)cdevhdr.size; pos += len)
    {
#ifdef EXTERNALGUI
        if (extgui2) fprintf (stderr,"POS=%lu\n",pos);
#endif /*EXTERNALGUI*/

        /* check for free space */
        if ((U32)(pos + freed) == cdevhdr.free)
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
            if (l1[i] == (U32)(pos + freed)) break;
        if (i < cdevhdr.numl1tab)
        { /* space is a l2 table */
            len = CCKD_L2TAB_SIZE;
            if (freed)
            {
                rc = lseek (fd, (off_t)l1[i], SEEK_SET);
                rc = read  (fd, buf, len);
                l1[i] -= freed;
                rc = lseek (fd, (off_t)l1[i], SEEK_SET);
                rc = write (fd, buf, len);
                moved += len;
            }
            continue;
        }

        /* check for track image */
        rc = lseek (fd, pos + freed, SEEK_SET);
        rc = read  (fd, buf, 8);
        if (ckddasd)
        {
            trk = ((buf[1] << 8) + buf[2]) * heads
                + ((buf[3] << 8) + buf[4]);
        }
        else
        {
            trk = (buf[1] << 24)
                | (buf[2] << 16)
                | (buf[3] <<  8)
                | buf[4];
        }
        l1x = trk >> 8;
        l2x = trk & 0xff;
        l2.pos = l2.len = l2.size = 0;
        if (l1[l1x])
        {
            rc = lseek (fd, (off_t)(l1[l1x] + l2x * CCKD_L2ENT_SIZE), SEEK_SET);
            rc = read (fd, &l2, CCKD_L2ENT_SIZE);
        }
        if ((off_t)l2.pos == pos + freed)
        { /* space is a track image */
            len = l2.len;
            imbedded = l2.size - l2.len;
            if (freed)
            {
                rc = lseek (fd, (off_t)l2.pos, SEEK_SET);
                rc = read  (fd, buf, len);
                l2.pos -= freed;
                rc = lseek (fd, (off_t)l2.pos, SEEK_SET);
                rc = write (fd, buf, len);
            }
            if (freed || imbedded )
            {
                l2.size = l2.len;
                rc = lseek (fd, (off_t)(l1[l1x] + l2x * CCKD_L2ENT_SIZE), SEEK_SET);
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
         COMPMSG (m, "unknown space at offset 0x%llx : "
                  "%2.2x%2.2x%2.2x%2.2x %2.2x%2.2x%2.2x%2.2x\n",
                  (long long)(pos + freed), buf[0], buf[1], buf[2],
                  buf[3], buf[4], buf[5], buf[6], buf[7]);
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
        if (extgui2) fprintf (stderr,"POS=%lu\n",pos);
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

    COMPMSG (m, "file %ssuccessfully compacted, %d freed and %d moved\n",
             freed ? "" : "un", freed, moved);
   free (l1);

   return (freed ? freed : -1);
}

/*-------------------------------------------------------------------*/
/* Perform check function on a compressed ckd file                   */
/*-------------------------------------------------------------------*/
int cckd_chkdsk(int fd, FILE *m, int level)
{
CKDDASD_DEVHDR  devhdr;                 /* CKD device header         */
CCKDDASD_DEVHDR cdevhdr;                /* Compressed CKD device hdr */
CCKDDASD_DEVHDR cdevhdr2;               /* CCKD header 2             */
CCKD_L1ENT     *l1=NULL;                /* -> Primary lookup table   */
int             l1tabsz;                /* Primary lookup table size */
CCKD_L2ENT      l2[256], *l2p=NULL;     /* Secondary lookup table    */
CKDDEV         *ckd=NULL;               /* -> CKD DASD table entry   */
BYTE           *buf=NULL;               /* Buffer for track image    */
int             readlen;                /* Length for buffer read    */
long            trklen=0;               /* Length of track image     */
int             rc;                     /* Return code               */
int             crc=-1;                 /* Chdsk return code         */
int             i,j,k;                  /* Indexes                   */
struct stat     fst;                    /* File status information   */
int             cyls=-1, hdrcyls=-1;    /* Total cylinders           */
int             trks;                   /* Total tracks              */
int             heads=-1, hdrheads=-1;  /* Heads per cylinder        */
int             rec0len=8;              /* R0 length                 */
int             maxdlen=-1;             /* Max data length for device*/
int             trksz=-1, hdrtrksz=-1;  /* Track size                */
off_t           hipos, lopos;           /* Valid high/low offsets    */
off_t           pos;                    /* File offset               */
int             hdrerr=0, fsperr=0, l1errs=0, l2errs=0, trkerrs=0,
                othererrs = 0;          /* Error indicators          */
off_t           fsp;                    /* Free space offset         */
CCKD_FREEBLK    fb;                     /* Free space block          */
int             n;                      /* Size of space tables      */
SPCTAB         *spc=NULL, *rcv=NULL,    /* Space/Recovery tables     */
               *gap=NULL;               /* Gaps in the file          */
int             s=0, r=0;               /* Space/Recovery indices    */
int             trk;                    /* Current track number      */
int             maxlen;                 /* Max length of track image */
int             gaps=0, gapsize=0;      /* Gaps and size in file     */
BYTE           *gapbuf=NULL;            /* Buffer containing gap data*/
int             x,y;                    /* Lookup table indices      */
int             rcvtrks;                /* Nbr trks to be recovered  */
int             fdflags;                /* File flags                */
int             shadow=0;               /* 1=Shadow file             */
int             swapend=0;              /* 1=Wrong endianess         */
int             fend,mend;              /* Byte order indicators     */
int             ckddasd;                /* 1=CKD dasd   0=FBA dasd   */
U32             trkavg=0, trksum=0, trknum=0;
                                        /* Used to compute avg trk sz*/
int             trys;                   /* Nbr recovery trys for trk */
int             comps = 0;              /* Supported compressions    */
int             badcomps[] = {0, 0, 0}; /* Bad compression counts    */
char            msg[256];               /* Message                   */
BYTE *space[]     = {"none", "devhdr", "cdevhdr", "l1tab", "l2tab",
                     "trkimg", "free_blk", "file_end"};
BYTE *compression[] = {"none", "zlib", "bzip2"};

#if defined(HAVE_LIBZ)
    comps |= CCKD_COMPRESS_ZLIB;
#endif
#if defined(CCKD_BZIP2)
    comps |= CCKD_COMPRESS_BZIP2;
#endif

#ifdef EXTERNALGUI
    /* The number of "steps" (checks) we'll be doing... */
    if (extgui) fprintf (stderr,"STEPS=10\n");
#endif /*EXTERNALGUI*/

#ifdef EXTERNALGUI
    /* Beginning first step... */
    if (extgui) fprintf (stderr,"STEP=1\n");
#endif /*EXTERNALGUI*/

/*-------------------------------------------------------------------*/
/* Read the device header.                                           */
/*-------------------------------------------------------------------*/
    rc = fstat (fd, &fst);
    if (rc < 0)
    {
        CDSKMSG (m, "fstat(%d) error: %s\n",
                 fd, strerror(errno));
        return -1;
    }
    fdflags = fcntl (fd, F_GETFL);

    rc = lseek (fd, 0, SEEK_SET);
    if (rc < 0)
    {
        CDSKMSG (m, "devhdr lseek error: %s\n",
                 strerror(errno));
    }
    rc = read (fd, &devhdr, CKDDASD_DEVHDR_SIZE);
    if (rc != CKDDASD_DEVHDR_SIZE)
    {
        CDSKMSG (m, "devhdr read error: %s\n",
                 strerror(errno));
        return -1;
    }

/*-------------------------------------------------------------------*/
/* Perform checks on the device header.  The following fields        */
/* *must* be correct or the chkdsk function terminates:              */
/*      devid                                                        */
/* In this circumstance, the header will need to be                  */
/* manually repaired -- see cckdfix.c for a sample                   */
/*-------------------------------------------------------------------*/

    /* Check the identifier */
    if (memcmp(devhdr.devid, "CKD_C370", 8) == 0
     || memcmp(devhdr.devid, "CKD_S370", 8) == 0)
        ckddasd = 1;
    else if (memcmp(devhdr.devid, "FBA_C370", 8) == 0
          || memcmp(devhdr.devid, "FBA_S370", 8) == 0)
        ckddasd = 0;
    else
    {
        CDSKMSG (m, "file is not a compressed dasd file\n");
        goto cdsk_return;
    }
    if (memcmp(devhdr.devid, "CKD_S370", 8) == 0
     || memcmp(devhdr.devid, "FBA_S370", 8) == 0)
        shadow = 1;

/*-------------------------------------------------------------------*/
/* read the compressed CKD device header                             */
/*-------------------------------------------------------------------*/
    rc = read (fd, &cdevhdr, CCKDDASD_DEVHDR_SIZE);
    if (rc != CCKDDASD_DEVHDR_SIZE)
    {
        CDSKMSG (m, "cdevhdr read error: %s\n",
                 strerror(errno));
        goto cdsk_return;
    }

/*-------------------------------------------------------------------*/
/* Check the endianess of the compressed CKD file                    */
/*-------------------------------------------------------------------*/
    fend = ((cdevhdr.options & CCKD_BIGENDIAN) != 0);
    mend = cckd_endian ();
    if (fend != mend)
    {
        if (fdflags & O_RDWR)
        {
            CDSKMSG (m, "converting file(%d) to %s\n",
               fd, mend ? "big-endian" : "little-endian");
            rc = cckd_swapend (fd, m);
            if (rc == -1) goto cdsk_return;
            rc = lseek (fd, CKDDASD_DEVHDR_SIZE, SEEK_SET);
            rc = read (fd, &cdevhdr, CCKDDASD_DEVHDR_SIZE);
            cdevhdr.options |= CCKD_ORDWR;
        }
        else
        {
            swapend = 1;
            cckd_swapend_chdr (&cdevhdr);
        }
    }

/*-------------------------------------------------------------------*/
/* Lookup the DASD table entry for the device                        */
/*-------------------------------------------------------------------*/
    if (ckddasd)
    {
        hdrcyls = ((U32)(cdevhdr.cyls[3]) << 24)
                | ((U32)(cdevhdr.cyls[2]) << 16)
                | ((U32)(cdevhdr.cyls[1]) << 8)
                | (U32)(cdevhdr.cyls[0]);

        ckd = dasd_lookup (DASD_CKDDEV, NULL, devhdr.devtype, hdrcyls);
        if (ckd == NULL)
        {
            CDSKMSG (m, "DASD table entry not found for 0x%2.2X cyls %d\n",
                     devhdr.devtype, hdrcyls);
            goto cdsk_return;
        }
        maxdlen = ckd->r1;
        heads = ckd->heads;
    }

/*-------------------------------------------------------------------*/
/* Perform checks on the headers.  The following fields must be      */
/* correct or the chkdsk function terminates:                        */
/*   devhdr:                                                         */
/*      trksz                                                        */
/*      heads                                                        */
/*   cdevhdr:                                                        */
/*      cyls                                                         */
/*      numl1tab                                                     */
/* In this circumstance, the headers will need to be manually        */
/* repaired -- see cckdfix.c for a sample                            */
/*-------------------------------------------------------------------*/

    if (ckddasd)
    {
        /* Check track size */
        trksz = sizeof(CKDDASD_TRKHDR)
              + sizeof(CKDDASD_RECHDR) + rec0len
              + sizeof(CKDDASD_RECHDR) + maxdlen
              + sizeof(eighthexFF);
        trksz = ((trksz+511)/512)*512;

        hdrtrksz = ((U32)(devhdr.trksize[3]) << 24)
                 | ((U32)(devhdr.trksize[2]) << 16)
                 | ((U32)(devhdr.trksize[1]) << 8)
                 | (U32)(devhdr.trksize[0]);

        if (trksz != hdrtrksz)
        {
            CDSKMSG (m, "Invalid track size in header: "
                     "0x%x, expecting 0x%x\n",
                     hdrtrksz, trksz);
            goto cdsk_return;
        }

        /* Check number of heads */
        hdrheads = ((U32)(devhdr.heads[3]) << 24)
                 | ((U32)(devhdr.heads[2]) << 16)
                 | ((U32)(devhdr.heads[1]) << 8)
                 | (U32)(devhdr.heads[0]);

        if (heads != hdrheads)
        {
            CDSKMSG (m, "Invalid number of heads in header: "
                     "%d, expecting %d\n",
                     hdrheads, heads);
            goto cdsk_return;
        }

        if (cdevhdr.numl1tab < ((hdrcyls * heads) + 255) / 256
         || cdevhdr.numl1tab > ((hdrcyls * heads) + 255) / 256 + 1)
        {
            CDSKMSG (m, "Invalid number of l1 table entries in header: "
                     "%d, expecting %d\n",
                     cdevhdr.numl1tab, ((hdrcyls * heads) + 255) / 256);
            goto cdsk_return;
        }
        cyls = hdrcyls;
        trks = cyls * heads;
    }
    else
    {
        trks = ((U32)(cdevhdr.cyls[3]) << 24)
             | ((U32)(cdevhdr.cyls[2]) << 16)
             | ((U32)(cdevhdr.cyls[1]) << 8)
             | (U32)(cdevhdr.cyls[0]);
        trks = (trks / CFBA_BLOCK_NUM) + 1;
        trksz = CFBA_BLOCK_SIZE + CKDDASD_TRKHDR_SIZE;
        cyls = heads = -1;
        if (cdevhdr.numl1tab < (trks + 255) / 256
         || cdevhdr.numl1tab > (trks + 255) / 256 + 1)
        {
            CDSKMSG (m, "Invalid number of l1 table entries in header: "
                     "%d, expecting %d\n", cdevhdr.numl1tab, (trks + 255) / 256);
            goto cdsk_return;
        }
    }

    l1tabsz = cdevhdr.numl1tab * CCKD_L1ENT_SIZE;

    /* Perform space checks */
    if ((U32)cdevhdr.size != (U32)fst.st_size
     || (U32)(cdevhdr.used + cdevhdr.free_total) != (U32)fst.st_size
     || (U32)cdevhdr.free_largest > (U32)cdevhdr.free_total
     || (!cdevhdr.free
      && (cdevhdr.free_total || (U32)cdevhdr.free_number != (U32)0))
     || (cdevhdr.free
      && (!cdevhdr.free_total || !cdevhdr.free_number))
     || (!cdevhdr.free_number && cdevhdr.free_total)
     || (cdevhdr.free_number && !cdevhdr.free_total))
    {
        CDSKMSG (m, "Recoverable header errors found: "
                 "%d %d %d %d %d %d %d\n",
                 (unsigned int)fst.st_size,
                 (unsigned int)cdevhdr.size,
                 (unsigned int)cdevhdr.used,
                 (unsigned int)cdevhdr.free_total,
                 (unsigned int)cdevhdr.free_largest,
                 (unsigned int)cdevhdr.free,
                 (unsigned int)cdevhdr.free_number);
        hdrerr = 1;
        if (level < 1)
        {   level = 1;
            CDSKMSG (m, "forcing check level %d\n", level);
        }
    }

/*-------------------------------------------------------------------*/
/* Read the primary lookup table                                     */
/* We *must* be able to successfully read this table, too            */
/*-------------------------------------------------------------------*/

    l1 = malloc (l1tabsz);
    if (l1 == NULL)
    {
        CDSKMSG (m, "malloc() l1tab error, size %d: %s\n", l1tabsz,
            strerror(errno));
        goto cdsk_return;
     }

    rc = read (fd, l1, l1tabsz);
    if (rc != l1tabsz)
    {
        CDSKMSG (m, "read(%d) l1tab error, size %d: %s\n",
             (int) fd, l1tabsz, strerror(errno));
        goto cdsk_return;
    }
    if (swapend) cckd_swapend_l1 (l1, cdevhdr.numl1tab);

/*-------------------------------------------------------------------*/
/* If minimal checking is specified then                             */
/*    If the file was not closed then perform level 1 checking       */
/*    Else if the file hasn't been written to since the last check   */
/*       Then perform very minimal checking.                         */
/*-------------------------------------------------------------------*/

    if (level == 0)
    {
        if (cdevhdr.options & CCKD_OPENED)
        {
            CDSKMSG (m, "forcing check level 1; file not closed\n");
            level = 1;
        }
        else if ((cdevhdr.options & (CCKD_OPENED | CCKD_ORDWR)) == 0)
            level = -1;
    }

/*-------------------------------------------------------------------*/
/* Set space boundaries                                              */
/*-------------------------------------------------------------------*/

    lopos = (off_t)CCKD_L1TAB_POS + l1tabsz;
    hipos = (off_t)fst.st_size;

/*-------------------------------------------------------------------*/
/* Get a buffer                                                      */
/*-------------------------------------------------------------------*/

    buf = malloc(trksz);
    if (buf == NULL)
    {
        CDSKMSG (m, "malloc() failed for buffer, size %d: %s\n",
                trksz, strerror(errno));
        goto cdsk_return;
    }

/*-------------------------------------------------------------------*/
/* get the space/recovery/gap tables                                 */
/*-------------------------------------------------------------------*/

    n = 4 + (cdevhdr.numl1tab + trks) * 2;

    spc = malloc (n * sizeof(SPCTAB));
    if (spc == NULL)
    {   CDSKMSG (m, "malloc() failed for space table size %ud: %s\n",
                 (unsigned int) (n * sizeof(SPCTAB)), strerror(errno));
        goto cdsk_return;
    }

    rcv = malloc (n * sizeof(SPCTAB));
    if (rcv == NULL)
    {   CDSKMSG (m, "malloc() failed for recovery table size %ud: %s\n",
                 (unsigned int) (n * sizeof(SPCTAB)), strerror(errno));
        goto cdsk_return;
    }

    gap = malloc (n * sizeof(SPCTAB));
    if (gap == NULL)
    {   CDSKMSG (m, "malloc() failed for gap table size %ud: %s\n",
                 (unsigned int) (n * sizeof(SPCTAB)), strerror(errno));
        goto cdsk_return;
    }

/*-------------------------------------------------------------------*/
/* Check free space chain                                            */
/*                                                                   */
/* Things we check for :                                             */
/*    (1) free space offset is within a valid position in the file   */
/*    (2) free space block can be lseek()ed and read() without error */
/*    (3) length of the free space is at least the size of a free    */
/*        space block                                                */
/*    (4) next free space does not precede this free space           */
/*    (5) free space does not extend beyond the end of the file      */
/*                                                                   */
/*-------------------------------------------------------------------*/

free_space_check:
    if (level >= 0)
    {
    memset (&cdevhdr2, 0, CCKDDASD_DEVHDR_SIZE);
    cdevhdr2.size = (U32)hipos;
    cdevhdr2.used = CKDDASD_DEVHDR_SIZE + CCKDDASD_DEVHDR_SIZE +
                    l1tabsz;
    cdevhdr2.free = cdevhdr.free;

    /* if the file wasn't closed then rebuild the free space */
    if (cdevhdr.options & CCKD_OPENED)
    {   fsperr = 1;
        sprintf (msg, "file not closed");
    }
    else if (hdrerr)
    {   fsperr = 1;
        sprintf (msg, "header errors");
    }
    else fsperr = 0;

    #ifdef EXTERNALGUI
    /* Beginning next step... */
    if (extgui) fprintf (stderr,"STEP=2\n");
    #endif /*EXTERNALGUI*/

    for (fsp = (off_t)cdevhdr.free; fsp && !fsperr; fsp = (off_t)fb.pos)
    {
        fsperr = 1;               /* turn on error indicator */
        memset (&fb, 0, CCKD_FREEBLK_SIZE);
        sprintf (msg, "pos=0x%llx nxt=0x%llx len=%d\n",
                 (long long)fsp, (long long)fb.pos, fb.len);
        if (fsp < lopos || fsp > hipos - CCKD_FREEBLK_SIZE) break;
        rc = lseek (fd, fsp, SEEK_SET);
        if (rc == -1) break;
        rc = read (fd, &fb, CCKD_FREEBLK_SIZE);
        if (rc != CCKD_FREEBLK_SIZE) break;
        if (swapend) cckd_swapend_free (&fb);
        if ((U32)fb.len < (U32)CCKD_FREEBLK_SIZE || (U32)(fsp + fb.len) > (U32)hipos) break;
        if (fb.pos && (fb.pos <= fsp + fb.len ||
                       (U32)fb.pos > (U32)(hipos - CCKD_FREEBLK_SIZE))) break;
        sprintf (msg, "free space at end of the file");
        if ((U32)(fsp + fb.len) == (U32)hipos && (fdflags & O_RDWR)) break;
        fsperr = 0;                       /* reset error indicator   */
        cdevhdr2.free_number++;
        cdevhdr2.free_total += fb.len;
        if (fb.len > cdevhdr2.free_largest)
            cdevhdr2.free_largest = fb.len;
    }
    }

    if (fsperr)
    {   CDSKMSG (m, "free space errors found: %s\n", msg);
        if (level < 1)
        {   level = 1;
            CDSKMSG (m, "forcing check level %d\n", level);
        }
    }

/*-------------------------------------------------------------------*/
/* populate the space/recovery tables; perform space checks          */
/*-------------------------------------------------------------------*/

space_check:
    memset (spc, 0, n * sizeof(SPCTAB));
    memset (rcv, 0, n * sizeof(SPCTAB));
    memset (gap, 0, n * sizeof(SPCTAB));

    s = r = 0;

    /* ckddasd device header space */
    spc[s].pos = 0;
    spc[s].len = spc[s].siz = CKDDASD_DEVHDR_SIZE;
    spc[s++].typ = SPCTAB_DEVHDR;

    /* compressed ckddasd device header space */
    spc[s].pos = CKDDASD_DEVHDR_SIZE;
    spc[s].len = spc[s].siz = CCKDDASD_DEVHDR_SIZE;
    spc[s++].typ = SPCTAB_CDEVHDR;

    /* level 1 lookup table space */
    spc[s].pos = CCKD_L1TAB_POS;
    spc[s].len = spc[s].siz = l1tabsz;
    spc[s++].typ = SPCTAB_L1TAB;

    /* end-of-file */
    spc[s].pos = hipos;
    spc[s++].typ = SPCTAB_END;

#ifdef EXTERNALGUI
    /* Beginning next step... */
    if (extgui) fprintf (stderr,"STEP=3\n");
#endif /*EXTERNALGUI*/

    /* free spaces */
    if (level >= 0)
    {
    for (fsp = (off_t)cdevhdr.free, i=0; i < (int)cdevhdr2.free_number;
         fsp = (off_t)fb.pos, i++)
    {
        rc = lseek (fd, fsp, SEEK_SET);
        rc = read (fd, &fb, CCKD_FREEBLK_SIZE);
        if (swapend) cckd_swapend_free (&fb);
        spc[s].pos = fsp;
        spc[s].len = spc[s].siz = fb.len;
        spc[s++].typ = SPCTAB_FREE;
    }
    }

    l1errs = l2errs = trkerrs = 0;

#ifdef EXTERNALGUI
    /* Beginning next step... */
    if (extgui) fprintf (stderr,"STEP=4\n");
#endif /*EXTERNALGUI*/

    /* level 2 lookup table and track image spaces */
    for (i = 0; i < (int)cdevhdr.numl1tab; i++)
    {
        int valid_l2, valid_trks, invalid_trks;

        if ((l1[i] == 0)
         || (shadow && l1[i] == 0xffffffff)) continue;

        /* check for valid offset in l1tab entry */
        if ((off_t)l1[i] < lopos || (off_t)l1[i] > (hipos - CCKD_L2TAB_SIZE))
        {
            CDSKMSG (m, "l1[%d] has bad offset 0x%x\n", i, l1[i]);
            l1errs++;

        /* recover all tracks for the level 2 table entry */
        bad_l2:
            if (level <= 0)
            {   int orig_lvl = level;
                level = 1;
                CDSKMSG (m, "forcing check level %d\n", level);
                if (orig_lvl < 0) goto free_space_check;
                goto space_check;
            }
            CDSKMSG (m, "tracks %d thru %d will be recovered\n",
                     i * 256, i * 256 + 255 < trks ?
                              i * 256 + 255 : trks - 1);
            rcv[r].typ = SPCTAB_L2TAB;
            rcv[r].len = rcv[r].siz = CCKD_L2TAB_SIZE;
            rcv[r++].val = i;
            for (j = 0; j < 256; j++)
            {
                if (i * 256 + j >= trks) break;
                rcv[r].typ = SPCTAB_TRKIMG;
                rcv[r++].val = i * 256 + j;
            }
            continue;
        }

        /* continue if very minimal checking */
        if (level < 0)
            continue;

        /* read the level 2 table */
        rc = lseek (fd, (off_t)l1[i], SEEK_SET);
        if (rc == -1)
        {
            CDSKMSG (m, "l1[%d] lseek error offset 0x%x: %s\n",
                    i, l1[i], strerror(errno));
            l2errs++;
            goto bad_l2;
        }

        rc = read (fd, &l2, CCKD_L2TAB_SIZE);
        if (rc != CCKD_L2TAB_SIZE)
        {
            CDSKMSG (m, "l1[%d] read error offset 0x%x: %s\n",
                    i , l1[i], strerror(errno));
            l2errs++;
            goto bad_l2;
        }

        if (swapend) cckd_swapend_l2 ((CCKD_L2ENT *)&l2);

        /* validate each level 2 entry */
        valid_l2 = 1;
    validate_l2:
        valid_trks = invalid_trks = 0;
        for (j = 0; j < 256; j++)
        {
            if ((l2[j].pos == 0)
             || (shadow && l2[j].pos == 0xffffffff)) continue;
            trk = i * 256 + j;

            /* consistency check on level 2 table entry */
            if (trk >= trks ||  (off_t)(l2[j].pos) < lopos ||
                (off_t)(l2[j].pos + l2[j].len) > hipos ||
                l2[j].len <= CKDDASD_TRKHDR_SIZE ||
                l2[j].len > trksz)
            {
                sprintf(msg, "l2tab inconsistency track %d", trk);
            bad_trk:
                if (level == 0)
                {
                    level = 1;
                    CDSKMSG (m, "forcing check level %d\n", level);
                    goto space_check;
                }
                if (valid_l2)
                {   /* start over but with valid_l2 false */
                    valid_l2 = 0;
                    for ( ;
                         spc[s - 1].typ == SPCTAB_TRKIMG &&
                         spc[s - 1].val >=  (i * 256) &&
                         spc[s - 1].val <= ((i * 256) + 255);
                         s--)
                        memset (&spc[s - 1], 0, sizeof (SPCTAB));
                    goto validate_l2;
                }
                CDSKMSG (m, "%s\n"
                   "  l2[%d,%d] offset 0x%x len %d\n",
                   msg, i, j, l2[j].pos, l2[j].len);
                trkerrs++;
                invalid_trks++;
                if (fdflags & O_RDWR)
                    CDSKMSG (m, " track %d will be recovered\n", trk);
                rcv[r].pos = l2[j].pos;
                rcv[r].len = l2[j].len;
                rcv[r].siz = l2[j].size;
                rcv[r].val = trk;
                rcv[r++].typ = SPCTAB_TRKIMG;
                continue;
            }

            /* Keep stats to compute average track size */
            trksum += l2[j].len;
            trknum++;

            /* read the track header */
            if (level >= 1)
            {
                /* Calculate length to read */
                if (level < 3 && valid_l2) readlen = CKDDASD_TRKHDR_SIZE;
                else readlen = l2[j].len;

                /* Read the track header and possibly the track image */
                rc = lseek (fd, (off_t)l2[j].pos, SEEK_SET);
                if (rc == -1)
                {
                    sprintf (msg, "lseek error track %d: %s", trk,
                             strerror(errno));
                    goto bad_trk;
                }
                rc = read (fd, buf, readlen);
                if (rc != readlen)
                {
                    sprintf (msg, "read error track %d: %s", trk,
                             strerror(errno));
                    goto bad_trk;
                }

                /* consistency check on track header */
                if (ckddasd)
                {
                  if ((buf[0] & ~CCKD_COMPRESS_MASK)
                   || fetch_hw (buf + 1) >= cyls
                   || fetch_hw (buf + 3) >= heads
                   || fetch_hw (buf + 1) * heads + fetch_hw (buf + 3) != trk)
                  {
                      sprintf (msg, "track %d invalid header "
                               "0x%2.2x%2.2x%2.2x%2.2x%2.2x", trk,
                               buf[0], buf[1], buf[2], buf[3], buf[4]);
                      goto bad_trk;
                  }
                }
                else
                {
                  if ((buf[0] & ~CCKD_COMPRESS_MASK)
                     || (int)fetch_fw (buf + 1) != trk)
                  {
                      sprintf (msg, "block %d invalid header "
                               "0x%2.2x%2.2x%2.2x%2.2x%2.2x", trk,
                               buf[0], buf[1], buf[2], buf[3], buf[4]);
                      goto bad_trk;
                  }
                }

                /* Check for unsupported compression */
                if (buf[0] & ~comps)
                {
                      sprintf (msg, "%s %d compressed using %s"
                               " which is not configured",
                               ckddasd ? "track" : "block", trk,
                               compression[buf[0]]);
                      if (badcomps[buf[0]]++ < 10)
                          goto bad_trk;
                      CDSKMSG (m, "Unsupported compression threshold exceeded, aborting\n");
                      goto cdsk_return;
                }
            }

            /* if we've had problems with the level 2 table
               then validate the entire track */
            if (!valid_l2 || level >= 3)
            {
                rc = cdsk_valid_trk (trk, buf, heads, l2[j].len, trksz, msg);
                if (rc != l2[j].len)
                    goto bad_trk;
            }

            /* add track to the space table */
            valid_trks++;
            cdevhdr2.used += l2[j].len;
            if ((l2[j].size - l2[j].len) && (fdflags & O_RDWR))
            {
                if (!fsperr)
                    CDSKMSG (m, "imbedded free space will be removed%s\n","");
                fsperr = 1;
                l2[j].size = l2[j].len;
                rc = lseek (fd, (off_t)l1[i], SEEK_SET);
                rc = write (fd, &l2, CCKD_L2TAB_SIZE);
            }
            cdevhdr2.free_imbed += l2[j].size - l2[j].len;
            cdevhdr2.free_total += l2[j].size - l2[j].len;
            spc[s].pos = l2[j].pos;
            spc[s].len = l2[j].len;
            spc[s].siz = l2[j].size;
            spc[s].val = trk;
            spc[s++].typ = SPCTAB_TRKIMG;
        }

        /* if the level 2 table appears valid, add the table to the
           space table; otherwise add the table to the recover table */
//FIXME: problem recovering l2 table
#if 0
        if (valid_l2 || valid_trks > 0)
#else
        if (1)
#endif
        {

            cdevhdr2.used += CCKD_L2TAB_SIZE;
            spc[s].pos = l1[i];
            spc[s].len = spc[s].siz = CCKD_L2TAB_SIZE;
            spc[s].typ = SPCTAB_L2TAB;
            spc[s++].val = i;
        }
        else
        {
            CDSKMSG (m, "l2[%d] will be recovered due to errors\n", i);
            l2errs++;
            for ( ;
                 spc[s - 1].typ == SPCTAB_TRKIMG &&
                 spc[s - 1].val >=  (i * 256) &&
                 spc[s - 1].val <= ((i * 256) + 255);
                 s--)
                memset (&spc[s - 1], 0, sizeof (SPCTAB));
            for ( ;
                 rcv[r - 1].typ == SPCTAB_TRKIMG &&
                 rcv[r - 1].val >=  (i * 256) &&
                 rcv[r - 1].val <= ((i * 256) + 255);
                 r--)
                memset (&rcv[r - 1], 0, sizeof (SPCTAB));
            goto bad_l2;
        }
    }

    /* Calculate average track size */
    if (trknum) trkavg = trksum / trknum;
    if (!trkavg) trkavg = trksz >> 1;
    trksum = trkavg;
    trknum = 1;

/*-------------------------------------------------------------------*/
/* we will rebuild free space on any kind of error                   */
/*-------------------------------------------------------------------*/

    othererrs = r || l1errs || l2errs || trkerrs;
    if ((level >= 0
      && memcmp (&cdevhdr.CCKD_FREEHDR, &cdevhdr2.CCKD_FREEHDR, CCKD_FREEHDR_SIZE))
     || othererrs)
        fsperr = 1;

/*-------------------------------------------------------------------*/
/* look for gaps and overlaps                                        */
/*                                                                   */
/* Overlaps aren't handled particularly elegantly, but then,         */
/* overlaps aren't exactly expected either.                          */
/*                                                                   */
/*-------------------------------------------------------------------*/

overlap:
    qsort ((void *)spc, s, sizeof(SPCTAB), cdsk_spctab_comp);
    for ( ; spc[s-1].typ == SPCTAB_NONE; s--);
#ifdef EXTERNALGUI
    /* Beginning next step... */
    if (extgui) fprintf (stderr,"STEP=5\n");
#endif /*EXTERNALGUI*/
    for (i = 0; i < s - 1; i++)
    {
//      CDSKMSG (m, "%s[%d] pos 0x%x length %d\n",
//              space[spc[i].typ], spc[i].val, spc[i].pos, spc[i].len);
        if (spc[i].pos + spc[i].siz < spc[i+1].pos)
        {
//          if (othererrs)
//          CDSKMSG (m, "gap at pos 0x%llx length %d\n",
//                  (long long)(spc[i].pos + spc[i].siz),
//                  (int)(spc[i+1].pos - (spc[i].pos + spc[i].siz)));
        }
        else if (spc[i].pos + spc[i].len > spc[i+1].pos)
        {
            CDSKMSG (m, "%s at pos 0x%llx length %lld overlaps "
                    "%s at pos 0x%llx\n",
                    space[spc[i].typ], (long long)spc[i].pos, spc[i].len,
                    space[spc[i+1].typ], (long long)spc[i+1].pos);
            for (j = i; j < s; j++)
            {
                if (spc[i].pos + spc[i].siz <= spc[j].pos) break;
                switch (spc[j].typ) {
                    case SPCTAB_FREE:
                        fsperr = 1;
                        memset (&spc[j], 0, sizeof(SPCTAB));
                        break;
                    case SPCTAB_L2TAB:
                        l2errs++;
                        memcpy (&rcv[r++], &spc[j], sizeof(SPCTAB));
                        for (k = 0; k < s - 1; k++)
                    {   /* move any tracks for the l2tab from  the
                           space table to the recovery table */
                            if (spc[k].typ == SPCTAB_TRKIMG &&
                                spc[k].val >= spc[j].val * 256 &&
                                spc[k].val <= spc[j].val * 256 + 255)
                            {
                                memcpy (&rcv[r++], &spc[k], sizeof(SPCTAB));
                                memset (&spc[k], 0, sizeof(SPCTAB));
                            }
                        }
                        memset (&spc[j], 0, sizeof(SPCTAB));
                        break;
                    case SPCTAB_TRKIMG:
                        trkerrs++;
                        memcpy (&rcv[r++], &spc[j], sizeof(SPCTAB));
                        memset (&spc[j], 0, sizeof(SPCTAB));
                        break;
                    case SPCTAB_END:
                        break;
                } /* switch */
            }
            goto overlap;
        }
    }

    /* return if we are still doing `very minimal' checking */
    if (level < 0)
    {
        crc = 0;
        goto cdsk_return;
    }

    /* build the gap table */
    gaps = cdsk_build_gap (spc, &s, gap);

    /* sort the recovery table by track */
    if (r > 0) qsort ((void *)rcv, r, sizeof(SPCTAB), cdsk_rcvtab_comp);

    /* if any kind of error, indicate free space error */
    if (gaps || r || l1errs || l2errs || trkerrs) fsperr = 1;

#if 0
/*-------------------------------------------------------------------*/
/* some debugging stuff                                              */
/*-------------------------------------------------------------------*/

    CDSKMSG (m, "gap table  size %d\n               position   size   data\n", gaps);
    for (i = 0; i < gaps; i++)
    {char buf[5];
        lseek (fd, gap[i].pos, SEEK_SET);
        read (fd, &buf, 5);
        CDSKMSG (m, "%3d 0x%8.8x %5lld %2.2x%2.2x%2.2x%2.2x%2.2x\n",
                 i+1, (int)gap[i].pos, gap[i].siz, buf[0], buf[1], buf[2], buf[3], buf[4]);
    }
    CDSKMSG (m,"recovery table  size %d\n                  type value\n", r);
    for (i = 0; i < r; i++)
        CDSKMSG (m, "%3d %8s %5d\n", i+1, space[rcv[i].typ], rcv[i].val);
//  if (gaps || r) goto cdsk_return;
#endif

/*-------------------------------------------------------------------*/
/* return if file opened read-only                                   */
/*-------------------------------------------------------------------*/

    if (!(fdflags & O_RDWR))
    {
        crc = fsperr;
        if (l1errs || l2errs || trkerrs) crc = -1;
        if (crc)
            CDSKMSG (m, "errors detected on read-only file%s\n","");
        goto cdsk_return;
    }

/*-------------------------------------------------------------------*/
/* perform track image space recovery                                */
/*-------------------------------------------------------------------*/

    /* count the number of tracks to be recovered */
    for (i = rcvtrks = 0; i < r; i++)
        if (rcv[i].typ == SPCTAB_TRKIMG) rcvtrks++;

#ifdef EXTERNALGUI
    /* Beginning next step... */
    if (extgui) fprintf (stderr,"STEP=6\n");
#endif /*EXTERNALGUI*/

    for (gapsize = i = 0; r > 0 && i < gaps && rcvtrks > 0; i++)
    {
        /* get new buffer if previous isn't large enough */
        if (gap[i].siz > gapsize)
        {
            if (gapbuf != NULL) free (gapbuf);
            gapsize = gap[i].siz;
            gapbuf = malloc (gapsize);
            if (gapbuf ==NULL)
            {
                CDSKMSG (m, "malloc failed for gap buf, size %d:%s\n",
                        gapsize, strerror(errno));
                goto cdsk_return;
            }
        }

        /* read the gap */
        rc = lseek (fd, gap[i].pos, SEEK_SET);
        if (rc == -1)
        {
            CDSKMSG (m, "lseek failed for gap at pos 0x%llx: %s\n",
                    (long long)gap[i].pos, strerror(errno));
            goto cdsk_return;
        }
        rc = read (fd, gapbuf, gap[i].siz);
        if (rc != gap[i].siz)
        {
            CDSKMSG (m, "read failed for gap at pos 0x%llx length %lld: %s\n",
                    (long long)gap[i].pos, gap[i].siz, strerror(errno));
            goto cdsk_return;
        }
//      CDSKMSG (m, "recovery for gap at pos 0x%llx length %lld\n",
//               (long long)gap[i].pos, gap[i].siz);

        /* search for track images in the gap */
        n = gap[i].siz;
        for (j = n; j >= 0; j--)
        {
            if (n - j < (int)(CKDDASD_TRKHDR_SIZE + 8))
                continue;

            /* test for possible track header */
            if (ckddasd)
            {
              if (!((gapbuf[j+1] << 8) + gapbuf[j+2] < cyls
                 && (gapbuf[j+3] << 8) + gapbuf[j+4] < heads))
                  continue;
              /* get track number */
              trk = ((gapbuf[j+1] << 8) + gapbuf[j+2]) * heads +
                     (gapbuf[j+3] << 8) + gapbuf[j+4];
            }
            /* test for possible block header */
            else
            {
              /* get block number */
              trk = (gapbuf[j+1] << 24) + (gapbuf[j+2] << 16) +
                    (gapbuf[j+3] <<  8) +  gapbuf[j+4];
            }
            if (trk >= trks)
                continue;

            /* see if track is to be recovered */
            for (k = 0; k < r; k++)
                if (rcv[k].typ == SPCTAB_TRKIMG &&
                    trk <= rcv[k].val) break;

            /* if track is not being recovered, continue */
            if (!(k < r && rcv[k].typ == SPCTAB_TRKIMG &&
                  rcv[k].val == trk))
                continue;

            /* calculate maximum track length */
            if (n - j < trksz) maxlen = n - j;
            else maxlen = trksz;

            CDSKMSG (m, "%s %d at 0x%llx maxlen %d: ",
                     ckddasd ? "trk" : "blk",
                     trk, (long long)(gap[i].pos + j), maxlen);

            /* Try to recover the track at the space in the gap */
            trklen = cdsk_recover_trk (trk, &gapbuf[j], heads,
                         maxlen, spc[s].len, trkavg, trksz, &trys);

            /* continue if track couldn't be uncompressed or isn't valid */
            if (trklen < 0)
            {
                if (m) fprintf (m,"not recovered here\n");
                continue;
            }

            rc = lseek (fd, gap[i].pos + j, SEEK_SET);
            rc = write (fd, &gapbuf[j], 1);

            if (m) fprintf (m,"recovered!! len %ld comp %s trys %d\n",
                trklen, compression[gapbuf[j]], trys);

            /* enter the recovered track into the space table */
            spc[s].pos = gap[i].pos + j;
            spc[s].len = spc[s].siz = trklen;
            spc[s].val = trk;
            spc[s++].typ = SPCTAB_TRKIMG;

            /* remove the entry from the recovery table */
            memset (&rcv[k], 0, sizeof(SPCTAB));

            /* update the level 2 table entry */
            x = trk / 256; y = trk % 256;
            for (k = 0; k < r; k++)
            {
                if (rcv[k].typ == SPCTAB_L2TAB && rcv[k].val == x)
                    break;
            }
            if (k < r) /* level 2 table is also being recovered */
            {
                if (rcv[k].ptr == NULL)
                {
                    rcv[k].ptr = calloc (256, CCKD_L2ENT_SIZE);
                    if (rcv[k].ptr == NULL)
                    {
                        CDSKMSG (m, "calloc failed l2 recovery buf, size %ud: %s\n",
                                (unsigned int) (256*CCKD_L2ENT_SIZE),
                                strerror(errno));
                        goto cdsk_return;
                    }
                }
                l2p = (CCKD_L2ENT *) rcv[k].ptr;
                l2p[y].pos = spc[s-1].pos;
                l2p[y].len = l2p[y].size = spc[s-1].len;
            }
            else /* level 2 table entry is in the file */
            {
                rc = lseek (fd, (off_t)(l1[x] + y * CCKD_L2ENT_SIZE), SEEK_SET);
                rc = read (fd, &l2, CCKD_L2ENT_SIZE);
                l2[0].pos = (U32)spc[s-1].pos;
                l2[0].len = l2[0].size = spc[s-1].len;
                rc = lseek (fd, (off_t)(l1[x] + y * CCKD_L2ENT_SIZE), SEEK_SET);
                rc = write (fd, &l2, CCKD_L2ENT_SIZE);
            }

            /* reset ending offset */
            n = j;

            /* decrement trks to be recovered; exit if none left */
            rcvtrks--;
            if (rcvtrks == 0) break;

            /* Compute new average */
            trksum += trklen;
            trknum++;
            trkavg = trksum / trknum;
        } /* for each byte in the gap */

    } /* for each gap */

/*-------------------------------------------------------------------*/
/* Handle unrecovered tracks                                         */
/*-------------------------------------------------------------------*/

    if (r > 0)
    {   qsort ((void *)rcv, r, sizeof(SPCTAB), cdsk_rcvtab_comp);
        for ( ; rcv[r-1].typ == SPCTAB_NONE; r--);
    }
#ifdef EXTERNALGUI
    /* Beginning next step... */
    if (extgui) fprintf (stderr,"STEP=7\n");
#endif /*EXTERNALGUI*/
    for (i = 0; i < r; i++)
    {
        if (rcv[i].typ != SPCTAB_TRKIMG) continue;

        CDSKMSG (m, "*** track %d was not recovered\n", rcv[i].val);
        x = rcv[i].val / 256; y = rcv[i].val % 256;

        /* Check if level 2 table is being recovered */
        for (j = 1; j < r; j++)
            if (rcv[j].typ == SPCTAB_L2TAB && rcv[j].val == x) break;

        /* Level 2 table is being recovered */
        if (j < r)
        {
            if (rcv[j].ptr == NULL)
            {
                rcv[j].ptr = calloc (256, CCKD_L2ENT_SIZE);
                if (rcv[j].ptr == NULL)
                {
                    CDSKMSG (m, "calloc failed l2 recovery buf, size %ud: %s\n",
                            (unsigned int) (256*CCKD_L2ENT_SIZE),
                            strerror(errno));
                    goto cdsk_return;
                }
            }
            l2p = (CCKD_L2ENT *) rcv[j].ptr;
            l2p[y].pos = shadow ? 0xffffffff : 0;
            l2p[y].len = l2p[y].size = 0;
        }
        /* Level 2 table entry is in the file */
        else
        {
            rc = lseek (fd, (off_t)(l1[x] + y * CCKD_L2ENT_SIZE), SEEK_SET);
            rc = read (fd, &l2, CCKD_L2ENT_SIZE);
            l2[0].pos = shadow ? 0xffffffff : 0;
            l2[0].len = l2[0].size = 0;
            rc = lseek (fd, (off_t)(l1[x] + y * CCKD_L2ENT_SIZE), SEEK_SET);
            rc = write (fd, &l2, CCKD_L2ENT_SIZE);
        }

        /* remove the entry from the recovery table */
        memset (&rcv[i], 0, sizeof(SPCTAB));
    }

/*-------------------------------------------------------------------*/
/* remove free space if any errors were encountered                  */
/*-------------------------------------------------------------------*/

    if (fsperr)
        for (i = 0; i < s; i++)
            if (spc[i].typ == SPCTAB_FREE)
                memset (&spc[i], 0, sizeof(SPCTAB));

/*-------------------------------------------------------------------*/
/* rebuild the gap/space tables                                      */
/*-------------------------------------------------------------------*/

    gaps = cdsk_build_gap (spc, &s, gap);

/*-------------------------------------------------------------------*/
/* recover any level 2 tables                                        */
/*-------------------------------------------------------------------*/

    if (r > 0)
    {   qsort ((void *)rcv, r, sizeof(SPCTAB), cdsk_rcvtab_comp);
        for ( ; rcv[r-1].typ == SPCTAB_NONE; r--);
    }

#ifdef EXTERNALGUI
    /* Beginning next step... */
    if (extgui) fprintf (stderr,"STEP=8\n");
#endif /*EXTERNALGUI*/

    for (i = 0; i < r; i++)
    {
        if (rcv[i].typ != SPCTAB_L2TAB) continue;

        if (rcv[i].ptr == NULL)
        {
            CDSKMSG (m, "l2[%d] not recovered\n", rcv[i].val);
            l1[rcv[i].val] = 0;
        }
        else
        {   /* find a space for the recovered level 2 table */
            for (j = 0; j < gaps; j++)
                if (gap[j].siz == CCKD_L2TAB_SIZE ||
                    gap[j].siz >= CCKD_L2TAB_SIZE + CCKD_FREEBLK_SIZE)
                    break;
            if (j < gaps)
            {   /* found a place for the recovered level 2 table */
                pos = gap[j].pos;
                gap[j].pos += CCKD_L2TAB_SIZE;
                gap[j].len = gap[j].siz -= CCKD_L2TAB_SIZE;
            }
            else
            {   /* don't think this should ever happen */
                pos = hipos;
                rc = ftruncate (fd, pos + CCKD_L2TAB_SIZE);
                rc = fstat (fd, &fst);
                hipos = fst.st_size;
                spc[s-1].pos = hipos; /* should be SPCTAB_END */
            }
            /* write the recovered level 2 table to the file */
            l2p = (CCKD_L2ENT *) rcv[i].ptr;
            rc = lseek (fd, pos, SEEK_SET);
            rc = write (fd, l2p, CCKD_L2TAB_SIZE);
            CDSKMSG (m, "l2[%d] recovered\n", rcv[i].val);
            l1[rcv[i].val] = pos;
            spc[s].typ = SPCTAB_L2TAB;
            spc[s].pos = pos;
            spc[s].len = spc[s].siz = CCKD_L2TAB_SIZE;
            ++s;
        }
    }


/*-------------------------------------------------------------------*/
/* if any errors at all occurred then we rebuild the free space      */
/*-------------------------------------------------------------------*/

    if (fsperr)
    {
/*-------------------------------------------------------------------*/
/* Remove short gaps...                                              */
/* Short gaps should either be preceded by a l2 table or a trk img;  */
/* in this case, we relocate the l2 tab or trk img somewhere else.   */
/* Otherwise, the short gap is ignored.                              */
/*-------------------------------------------------------------------*/

    short_gap:
        gaps = cdsk_build_gap (spc, &s, gap);
#ifdef EXTERNALGUI
        /* Beginning next step... */
        if (extgui) fprintf (stderr,"STEP=9\n");
#endif /*EXTERNALGUI*/
        for (i = j = 0; i < gaps; i++)
        {
            if (gap[i].siz == 0 || gap[i].siz >= CCKD_FREEBLK_SIZE)
                continue; /* continue if not a short gap */

            /* find the space preceding the gap, it should
               always be a l2tab or a trkimg */
                for ( ; j < s; j++)
                    if (spc[j].pos + spc[j].siz == gap[i].pos) break;
            if (spc[j].typ == SPCTAB_TRKIMG || spc[j].typ == SPCTAB_L2TAB)
            {   /* move the preceding space somewhere else */
                for (k = 0; k < gaps; k++)
                    if (gap[k].siz == spc[j].siz ||
                        gap[k].siz >= spc[j].siz + CCKD_FREEBLK_SIZE)
                        break;
                if (k < gaps) /* use space in an existing gap */
                    pos = gap[k].pos;
                else
                {   /* use space at the end of the file */
                    pos = hipos;
                    rc = ftruncate (fd, hipos + spc[j].siz);
                        rc = fstat (fd, &fst);
                        hipos = fst.st_size;
                    spc[s-1].pos = hipos; /* should be SPCTAB_END */
                }
                /* write the preceding space to its new location */
                rc = lseek (fd, spc[j].pos, SEEK_SET);
                rc = read (fd, buf, spc[j].siz);
                spc[j].pos = pos;
                rc = lseek (fd, spc[j].pos, SEEK_SET);
                rc = write (fd, buf, spc[j].siz);
                if (spc[j].typ == SPCTAB_L2TAB) l1[spc[j].val] = spc[j].pos;
                else
                {   /* update level 2 table entry for track image */
                    x = spc[j].val / 256; y = spc[j].val % 256;
                    rc = lseek (fd, (off_t)(l1[x] + CCKD_L2ENT_SIZE * y), SEEK_SET);
                    rc = read (fd, &l2, CCKD_L2ENT_SIZE);
                    l2[0].pos = spc[j].pos;
                    rc = lseek (fd, (off_t)(l1[x] + CCKD_L2ENT_SIZE * y), SEEK_SET);
                    rc = write (fd, &l2, CCKD_L2ENT_SIZE);
            }
                goto short_gap;
            }
            CDSKMSG (m, "short gap pos 0x%llx preceded by %s pos 0x%llx\n",
                     (long long)gap[i].pos, space[spc[j].typ],
                     (long long)spc[j].pos);
        }

/*-------------------------------------------------------------------*/
/* Finally, we are ready to rebuild the free space                   */
/*-------------------------------------------------------------------*/

        gaps = cdsk_build_gap_long (spc, &s, gap);

        /* if the last gap is at the end of the file, then
           truncate the file */
        if (gaps > 0 && gap[gaps-1].pos + gap[gaps-1].siz == hipos)
        {
            hipos -= gap[gaps-1].siz;
            rc = ftruncate (fd, hipos);
            gaps--;
        }

        memset (&cdevhdr.CCKD_FREEHDR, 0, CCKD_FREEHDR_SIZE);
        cdevhdr.size = hipos;
        if (gaps) cdevhdr.free = gap[0].pos;
        cdevhdr.free_number = gaps;
#ifdef EXTERNALGUI
        /* Beginning next step... */
        if (extgui) fprintf (stderr,"STEP=10\n");
#endif /*EXTERNALGUI*/
        for (i = 0; i < gaps; i++)
        {
            if (i < gaps - 1) fb.pos = gap[i+1].pos;
            else fb.pos = 0;
            fb.len = gap[i].siz;
            rc = lseek (fd, gap[i].pos, SEEK_SET);
            rc = write (fd, &fb, CCKD_FREEBLK_SIZE);
            cdevhdr.free_total += gap[i].siz;
            if (gap[i].siz > cdevhdr.free_largest)
                cdevhdr.free_largest = gap[i].siz;
        }
        for (i = 0; i < s - 1; i++)
        {
            cdevhdr.free_total += spc[i].siz - spc[i].len;
            cdevhdr.free_imbed += spc[i].siz - spc[i].len;
            cdevhdr.used += spc[i].len;
        }

/*-------------------------------------------------------------------*/
/* Update the header and l1 table                                    */
/*-------------------------------------------------------------------*/

        if (cdevhdr.free_imbed == 0)
        {
            cdevhdr.vrm[0] = CCKD_VERSION;
            cdevhdr.vrm[1] = CCKD_RELEASE;
            cdevhdr.vrm[2] = CCKD_MODLVL;
        }
        cdevhdr.options |= CCKD_NOFUDGE;

        rc = lseek (fd, CKDDASD_DEVHDR_SIZE, SEEK_SET);
        rc = write (fd, &cdevhdr, CCKDDASD_DEVHDR_SIZE);
        rc = write (fd, l1, l1tabsz);

        CDSKMSG (m, "free space rebuilt: 1st 0x%x nbr %d\n   "
                    "total %d imbed %d largest %d\n",
                    cdevhdr.free, cdevhdr.free_number, cdevhdr.free_total,
                    cdevhdr.free_imbed, cdevhdr.free_largest);
    }

    crc = fsperr;

/*-------------------------------------------------------------------*/
/* Free all space and return                                         */
/*-------------------------------------------------------------------*/

cdsk_return:

    /* free all malloc()ed storage */
    for (i = 0; i < r; i++) if (rcv[i].ptr) free (rcv[i].ptr);
    if (spc)    free (spc);
    if (rcv)    free (rcv);
    if (gap)    free (gap);
    if (gapbuf) free (gapbuf);
    if (buf)    free (buf);
    if (l1)     free (l1);

    /* if file is ok or has been repaired, turn off the
       opened bit if it's on */
    if (crc >= 0
     && (cdevhdr.options & (CCKD_OPENED | CCKD_ORDWR)) != 0
     && (fdflags & O_RDWR))
    {
        cdevhdr.options &= ~(CCKD_OPENED | CCKD_ORDWR);
        rc = lseek (fd, CKDDASD_DEVHDR_SIZE + 3, SEEK_SET);
        rc = write (fd, &cdevhdr.options, 1);
    }

    return crc;

} /* end function cckd_chkdsk */

/*-------------------------------------------------------------------*/
/* Sort Space Table by pos.  Always return entry type NONE behind    */
/* any other entries                                                 */
/*-------------------------------------------------------------------*/

int cdsk_spctab_comp(const void *a, const void *b)
{
const SPCTAB   *x = a, *y = b;          /* Entries to be sorted      */

    if (x->typ == SPCTAB_NONE) return  1;
    if (y->typ == SPCTAB_NONE) return -1;
    if (x->pos < y->pos) return -1;
    else return 1;
} /* end function cdsk_spctab_comp */


/*-------------------------------------------------------------------*/
/* Sort Recovery Table by val.  Always return entry type NONE behind */
/* any other entries                                                 */
/*-------------------------------------------------------------------*/

int cdsk_rcvtab_comp(const void *a, const void *b)
{
const SPCTAB   *x = a, *y = b;          /* Entries to be sorted      */
unsigned int    v1, v2;                 /* Value for entry           */

    if (x->typ == SPCTAB_NONE) return  1;
    if (y->typ == SPCTAB_NONE) return -1;

    if (x->typ == SPCTAB_L2TAB) v1 = x->val * 256;
    else v1 = x->val;

    if (y->typ == SPCTAB_L2TAB) v2 = y->val * 256;
    else v2 = y->val;

    if (v1 < v2) return -1;
    if (v1 > v2) return  1;

    if (x->typ == SPCTAB_L2TAB) return -1;
    else return 1;
} /* end function cdsk_rcvtab_comp */

/*-------------------------------------------------------------------*/
/* Validate a track image                                            */
/*-------------------------------------------------------------------*/
int cdsk_valid_trk (int trk, BYTE *buf, int heads, int len, int trksz,
                    BYTE *msg)
{
int             rc;                     /* Return code               */
int             cyl;                    /* Cylinder                  */
int             head;                   /* Head                      */
char            cchh[4],cchh2[4];       /* Cyl, head big-endian      */
int             r;                      /* Record number             */
int             sz;                     /* Track size                */
int             kl,dl;                  /* Key/Data lengths          */
BYTE           *bufp;                   /* Buffer pointer            */
int             bufl;                   /* Buffer length             */
BYTE            buf2[65536];            /* Uncompressed buffer       */
int             comps = 0;              /* Supported compressions    */
BYTE           *compression[] = {"none", "zlib", "bzip2", "????"};

#if defined(HAVE_LIBZ)
    comps |= CCKD_COMPRESS_ZLIB;
#endif
#if defined(CCKD_BZIP2)
    comps |= CCKD_COMPRESS_BZIP2;
#endif

    /* Check for extraneous bits in the first byte */
    if (buf[0] & ~CCKD_COMPRESS_MASK)
    {
        if (msg)
            sprintf(msg,"%s %d invalid byte[0]: "
                   "%2.2x%2.2x%2.2x%2.2x%2.2x",
                   heads >= 0 ? "trk" : "blk", trk,
                   buf[0], buf[1], buf[2], buf[3], buf[4]);
        return -1;
    }

    /* Check for unsupported compression */
    if (buf[0] & ~comps)
    {
        if (msg)
            sprintf(msg,"%s %d compression %s not configured",
                   heads >= 0 ? "trk" : "blk", trk,
                   compression[buf[0]]);
        return -1;
    }

    /* Uncompress the track/block image */
    switch (buf[0] & CCKD_COMPRESS_MASK) {

    case CCKD_COMPRESS_NONE:
        bufp = buf;
        bufl = len;
        break;

#ifdef HAVE_LIBZ
    case CCKD_COMPRESS_ZLIB:
        bufp = (BYTE *)&buf2;
        memcpy (&buf2, buf, CKDDASD_TRKHDR_SIZE);
        bufl = sizeof(buf2) - CKDDASD_TRKHDR_SIZE;
        rc = uncompress (&buf2[CKDDASD_TRKHDR_SIZE], (uLongf *)&bufl,
                         &buf[CKDDASD_TRKHDR_SIZE], len);
        if (rc != Z_OK)
        {
            if (msg)
                sprintf (msg, "%s %d uncompress error, rc=%d;"
                         "%2.2x%2.2x%2.2x%2.2x%2.2x",
                         heads >= 0 ? "trk" : "blk", trk, rc,
                         buf[0], buf[1], buf[2], buf[3], buf[4]);
            return -1;
        }
        bufl += CKDDASD_TRKHDR_SIZE;
        break;
#endif

#ifdef CCKD_BZIP2
    case CCKD_COMPRESS_BZIP2:
        bufp = (BYTE *)&buf2;
        memcpy (&buf2, buf, CKDDASD_TRKHDR_SIZE);
        bufl = sizeof(buf2) - CKDDASD_TRKHDR_SIZE;
        rc = BZ2_bzBuffToBuffDecompress ( &buf2[CKDDASD_TRKHDR_SIZE], &bufl,
                         &buf[CKDDASD_TRKHDR_SIZE], len, 0, 0);
        if (rc != BZ_OK)
        {
            if (msg)
                sprintf (msg, "%s %d decompress error, rc=%d;"
                         "%2.2x%2.2x%2.2x%2.2x%2.2x",
                         heads >= 0 ? "trk" : "blk", trk, rc,
                         buf[0], buf[1], buf[2], buf[3], buf[4]);
            return -1;
        }
        bufl += CKDDASD_TRKHDR_SIZE;
        break;
#endif

    default:
        return -1;

    } /* switch (buf[0] & CCKD_COMPRESS_MASK) */

    /* FBA dasd check */
    if (heads == -1)
    {
        if (bufl == trksz) return len;
        if (msg)
            sprintf (msg, "block %d length %d expected %d validation error: "
                     "%2.2x%2.2x%2.2x%2.2x%2.2x",
                     trk, len, trksz, 
                     bufp[0], bufp[1], bufp[2], bufp[3], bufp[4]);
        return -1;
    }

    /* cylinder and head calculations */
    cyl = trk / heads;
    head = trk % heads;
    cchh[0] = cyl >> 8;
    cchh[1] = cyl & 0xFF;
    cchh[2] = head >> 8;
    cchh[3] = head & 0xFF;

    /* validate home address */
    if (memcmp (&bufp[1], cchh, 4) != 0)
    {
        if (msg)
            sprintf (msg, "track %d HA validation error: "
                 "%2.2x%2.2x%2.2x%2.2x%2.2x",
                 trk, bufp[0], bufp[1], bufp[2], bufp[3], bufp[4]);
        return -1;
    }

    /* validate record 0 */
    memcpy (cchh2, &bufp[5], 4); cchh2[0] &= 0x7f; /* fix for ovflow */
    if (/* memcmp (cchh, cchh2, 4) != 0 ||*/  bufp[9] != 0 ||
        bufp[10] != 0 || bufp[11] != 0 || bufp[12] != 8)
    {
        if (msg)
            sprintf (msg, "track %d R0 validation error: "
                 "%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x",
                 trk, bufp[5], bufp[6], bufp[7], bufp[8], bufp[9],
                 bufp[10], bufp[11], bufp[12]);
        return -1;
    }

    /* validate records 1 thru n */
    for (r = 1, sz = 21; sz + 8 <= trksz; sz += 8 + kl + dl, r++)
    {
        if (memcmp (&bufp[sz], eighthexFF, 8) == 0) break;
        kl = bufp[sz+5];
        dl = bufp[sz+6] * 256 + bufp[sz+7];
        /* fix for track overflow bit */
        memcpy (cchh2, &bufp[sz], 4); cchh2[0] &= 0x7f;

        /* fix for funny formatted vm disks */
        /*
        if (r == 1) memcpy (cchh, cchh2, 4);
        */

        if (/*memcmp (cchh, cchh2, 4) != 0 ||*/ bufp[sz+4] == 0 ||
            sz + 8 + kl + dl >= bufl)
        {
             if (msg)
                 sprintf (msg, "track %d R%d validation error: "
                     "%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x",
                     trk, r, bufp[sz], bufp[sz+1], bufp[sz+2], bufp[sz+3],
                     bufp[sz+4], bufp[sz+5], bufp[sz+6], bufp[sz+7]);
             return -1;
        }
    }
    sz += 8;

    if (sz > trksz)
    {
        if (msg)
            sprintf (msg, "track %d R%d validation error, no EOT: "
                "%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x",
                trk, r, bufp[sz], bufp[sz+1], bufp[sz+2], bufp[sz+3],
                bufp[sz+4], bufp[sz+5], bufp[sz+6], bufp[sz+7]);
        return -1;
    }

    /* Special processing for uncompressed track image */
    if ((buf[0] & CCKD_COMPRESS_MASK) == 0)
    {
        if (sz > len)
        {
            if (msg)
                sprintf (msg, "track %d size %d exceeds %d: "
                    "%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x",
                    trk, sz, len, bufp[sz], bufp[sz+1], bufp[sz+2], bufp[sz+3],
                    bufp[sz+4], bufp[sz+5], bufp[sz+6], bufp[sz+7]);
            return -1;
        }
        return sz;
    }

    if (sz != bufl)
    {
        if (msg)
            sprintf (msg, "track %d size mismatch, expected %d found %d",
                     trk, bufl, sz);
        return -1;
    }
    
    return len;

} /* end function cdsk_valid_trk */


/*-------------------------------------------------------------------*/
/* Recover a track image                                             */
/*-------------------------------------------------------------------*/
int cdsk_recover_trk (int trk, BYTE *buf, int heads, int maxlen,
                      int origlen, int avglen, int trksz, int *trys)
{
int             rc;                     /* Return code               */
int             startlen;               /* Start length              */
int             incr;                   /* Increment                 */
int             adjust;                 /* Adjustment value          */
BYTE            b0;                     /* First buffer byte         */

    if (trys) *trys = 0;

    b0 = buf[0];

    /* Special case if not compressed */
    buf[0] = 0;
    rc = cdsk_valid_trk (trk, buf, heads, maxlen, trksz, NULL);
    if (rc > 0)
    {
        if (trys) (*trys)++;
        return rc;
    }

    /* If the maxlen falls within range, try the maxlen */
    if (maxlen <= trksz)
    {
#ifdef CCKD_COMPRESS_ZLIB
        buf[0] = CCKD_COMPRESS_ZLIB;
        if (trys) (*trys)++;
        rc = cdsk_valid_trk (trk, buf, heads, maxlen, trksz, NULL);
        if (rc > 0) return rc;
#endif
#ifdef CCKD_COMPRESS_BZIP2
        buf[0] = CCKD_COMPRESS_BZIP2;
        if (trys) (*trys)++;
        rc = cdsk_valid_trk (trk, buf, heads, maxlen, trksz, NULL);
        if (rc > 0) return rc;
#endif
    }

    /* Try the original length if it fits */
    if (origlen && origlen <= maxlen)
    {
#ifdef CCKD_COMPRESS_ZLIB
        buf[0] = CCKD_COMPRESS_ZLIB;
        if (trys) (*trys)++;
        rc = cdsk_valid_trk (trk, buf, heads, origlen, trksz, NULL);
        if (rc > 0) return rc;
#endif
#ifdef CCKD_COMPRESS_BZIP2
        buf[0] = CCKD_COMPRESS_BZIP2;
        if (trys) (*trys)++;
        rc = cdsk_valid_trk (trk, buf, heads, origlen, trksz, NULL);
        if (rc > 0) return rc;
#endif
    }

    /* Figure out the offset to start recovering */
    if (origlen && origlen < maxlen
     && origlen <= avglen + (avglen >> 1)
     && origlen >= avglen - (avglen >> 1)) startlen = origlen;
    else if (avglen < maxlen) startlen = avglen;
    else startlen = maxlen;
    incr = 1;

    /* Try the start length */
#ifdef CCKD_COMPRESS_ZLIB
    buf[0] = CCKD_COMPRESS_ZLIB;
    if (trys) (*trys)++;
    rc = cdsk_valid_trk (trk, buf, heads, startlen, trksz, NULL);
    if (rc > 0) return rc;
#endif
#ifdef CCKD_COMPRESS_BZIP2
    buf[0] = CCKD_COMPRESS_BZIP2;
    if (trys) (*trys)++;
    rc = cdsk_valid_trk (trk, buf, heads, startlen, trksz, NULL);
    if (rc > 0) return rc;
#endif

    /* Now we start trying on both sides of the start length,
       adjusting by `incr' each iteration */
    adjust = incr;
    while (startlen - adjust >= 8 || startlen + adjust <= maxlen)
    {
        if (startlen - adjust >= 8)
        {
#ifdef CCKD_COMPRESS_ZLIB
            buf[0] = CCKD_COMPRESS_ZLIB;
            if (trys) (*trys)++;
            rc = cdsk_valid_trk (trk, buf, heads, startlen - adjust, trksz, NULL);
            if (rc > 0) return rc;
#endif
#ifdef CCKD_COMPRESS_BZIP2
            buf[0] = CCKD_COMPRESS_BZIP2;
            if (trys) (*trys)++;
            rc = cdsk_valid_trk (trk, buf, heads, startlen - adjust, trksz, NULL);
            if (rc > 0) return rc;
#endif
        }
        if (startlen + adjust <= maxlen)
        {
#ifdef CCKD_COMPRESS_ZLIB
            buf[0] = CCKD_COMPRESS_ZLIB;
            if (trys) (*trys)++;
            rc = cdsk_valid_trk (trk, buf, heads, startlen + adjust, trksz, NULL);
            if (rc > 0) return rc;
#endif
#ifdef CCKD_COMPRESS_BZIP2
            buf[0] = CCKD_COMPRESS_BZIP2;
            if (trys) (*trys)++;
            rc = cdsk_valid_trk (trk, buf, heads, startlen + adjust, trksz, NULL);
            if (rc > 0) return rc;
#endif
        }
        adjust += incr;
    }

    /* Track image not found */
    buf[0] = b0;
    return -1;

} /* end function cdsk_recover_trk */

/*-------------------------------------------------------------------*/
/* build gap table                                                   */
/*-------------------------------------------------------------------*/

int cdsk_build_gap (SPCTAB *spc, int *n, SPCTAB *gap)
{
int i, gaps, s;

    s = *n; /* size of space table */

    /* sort the space table by offset */
    qsort ((void *)spc, s, sizeof(SPCTAB), cdsk_spctab_comp);

    /* remove null entries from the end */
    for ( ; spc[s-1].typ == SPCTAB_NONE; s--);

    /* build the gap table */
    for (i = gaps = 0; i < s - 1; i++)
    {
       if (spc[i].pos + spc[i].siz < spc[i+1].pos)
        {
            gap[gaps].pos = spc[i].pos + spc[i].siz;
            gap[gaps++].siz = spc[i+1].pos - (spc[i].pos + spc[i].siz);
        }
    }
    *n = s;
    return gaps;
}

/*-------------------------------------------------------------------*/
/* build gap table (but ignore short gaps)                           */
/*-------------------------------------------------------------------*/

int cdsk_build_gap_long (SPCTAB *spc, int *n, SPCTAB *gap)
{
int i, gaps, s, siz;

    s = *n; /* size of space table */

    /* sort the space table by offset */
    qsort ((void *)spc, s, sizeof(SPCTAB), cdsk_spctab_comp);

    /* remove null entries from the end */
    for ( ; spc[s-1].typ == SPCTAB_NONE; s--);

    /* build the gap table */
    for (i = gaps = 0; i < s - 1; i++)
    {
        if (spc[i].pos + spc[i].siz < spc[i+1].pos)
        {
            siz = spc[i+1].pos - (spc[i].pos + spc[i].siz);
            if (siz >= CCKD_FREEBLK_SIZE)
            {
                gap[gaps].pos = spc[i].pos + spc[i].siz;
                gap[gaps++].siz = siz;
            }
        }
    }
    *n = s;
    return gaps;
}

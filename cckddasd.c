/* CCKDDASD.C   (c) Copyright Roger Bowler, 1999-2002                */
/*       ESA/390 Compressed CKD Direct Access Storage Device Handler */

/*-------------------------------------------------------------------*/
/* This module contains device functions for compressed emulated     */
/* count-key-data direct access storage devices.                     */
/*-------------------------------------------------------------------*/

//#define CCKD_ITRACEMAX 10000

#include "hercules.h"
#include "devtype.h"

#include <zlib.h>
#ifdef CCKD_BZIP2
#include <bzlib.h>
#endif

#ifdef CCKD_ITRACEMAX
#undef DEVTRACE
#define DEVTRACE(format, a...) \
 if (cckd->itracex >= 0) \
  {int n; \
   if (cckd->itracex >= 128 * CCKD_ITRACEMAX) cckd->itracex = 0; \
   n = cckd->itracex; cckd->itracex += 128; \
   sprintf(&cckd->itrace[n], "%4.4X:" format, dev->devnum, a); }
#endif

/*-------------------------------------------------------------------*/
/* Internal functions                                                */
/*-------------------------------------------------------------------*/
int     cckddasd_init_handler (DEVBLK *, int, BYTE **);
int     cckddasd_close_device (DEVBLK *);
void    cckddasd_start (DEVBLK *);
void    cckddasd_end (DEVBLK *);
int     cckd_read_track (DEVBLK *, int, int, BYTE *);
int     cckd_update_track (DEVBLK *, BYTE *, int, BYTE *);
CCKD_CACHE *cckd_read_trk (DEVBLK *, int, int, BYTE *);
void    cckd_readahead (DEVBLK *, int);
void    cckd_ra (DEVBLK *);
void    cckd_flush_cache(DEVBLK *);
void    cckd_writer(DEVBLK *);
off_t   cckd_get_space (DEVBLK *, int);
void    cckd_rel_space (DEVBLK *, unsigned int, int);
void    cckd_rel_free_atend (DEVBLK *, unsigned int, int, int);
int     cckd_read_chdr (DEVBLK *);
void    cckd_write_chdr (DEVBLK *);
int     cckd_read_l1 (DEVBLK *);
void    cckd_write_l1 (DEVBLK *);
void    cckd_write_l1ent (DEVBLK *, int);
int     cckd_read_fsp (DEVBLK *);
void    cckd_write_fsp (DEVBLK *);
int     cckd_read_l2 (DEVBLK *, int, int);
void    cckd_purge_l2 (DEVBLK *, int);
void    cckd_write_l2 (DEVBLK *);
int     cckd_read_l2ent (DEVBLK *, CCKD_L2ENT *, int);
void    cckd_write_l2ent (DEVBLK *, CCKD_L2ENT *, int);
int     cckd_read_trkimg (DEVBLK *, BYTE *, int, BYTE *);
int     cckd_write_trkimg (DEVBLK *, BYTE *, int, int);
void    cckd_harden (DEVBLK *);
int     cckd_trklen (DEVBLK *, BYTE *);
int     cckd_null_trk (DEVBLK *, BYTE *, int, int);
int     cckd_cchh (DEVBLK *, BYTE *, int);
int     cckd_sf_name (DEVBLK *, int, char *);
int     cckd_sf_init (DEVBLK *);
int     cckd_sf_new (DEVBLK *);
void    cckd_sf_add (DEVBLK *);
void    cckd_sf_remove (DEVBLK *, int);
void    cckd_sf_newname (DEVBLK *, BYTE *);
void    cckd_sf_stats (DEVBLK *);
void    cckd_sf_comp (DEVBLK *);
void    cckd_gcol (DEVBLK *);
int     cckd_gc_percolate (DEVBLK *, int, int);
void    cckd_print_itrace(DEVBLK *);

/*-------------------------------------------------------------------*/
/* Definitions for sense data format codes and message codes         */
/*-------------------------------------------------------------------*/
#define FORMAT_0                0       /* Program or System Checks  */
#define FORMAT_1                1       /* Device Equipment Checks   */
#define FORMAT_2                2       /* 3990 Equipment Checks     */
#define FORMAT_3                3       /* 3990 Control Checks       */
#define FORMAT_4                4       /* Data Checks               */
#define FORMAT_5                5       /* Data Check + Displacement */
#define FORMAT_6                6       /* Usage Stats/Overrun Errors*/
#define FORMAT_7                7       /* Device Control Checks     */
#define FORMAT_8                8       /* Device Equipment Checks   */
#define FORMAT_9                9       /* Device Rd/Wrt/Seek Checks */
#define FORMAT_F                15      /* Cache Storage Checks      */
#define MESSAGE_0               0       /* Message 0                 */
#define MESSAGE_1               1       /* Message 1                 */
#define MESSAGE_2               2       /* Message 2                 */
#define MESSAGE_3               3       /* Message 3                 */
#define MESSAGE_4               4       /* Message 4                 */
#define MESSAGE_5               5       /* Message 5                 */
#define MESSAGE_6               6       /* Message 6                 */
#define MESSAGE_7               7       /* Message 7                 */
#define MESSAGE_8               8       /* Message 8                 */
#define MESSAGE_9               9       /* Message 9                 */
#define MESSAGE_A               10      /* Message A                 */
#define MESSAGE_B               11      /* Message B                 */
#define MESSAGE_C               12      /* Message C                 */
#define MESSAGE_D               13      /* Message D                 */
#define MESSAGE_E               14      /* Message E                 */
#define MESSAGE_F               15      /* Message F                 */

/*-------------------------------------------------------------------*/
/* Static data areas                                                 */
/*-------------------------------------------------------------------*/
static  BYTE cckd_empty_l2tab[CCKD_L2TAB_SIZE];  /* Empty L2 table   */
static  BYTE eighthexFF[] = {0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff};
DEVHND  cckddasd_device_hndinfo; 

/*-------------------------------------------------------------------*/
/* Initialize the compressed device handler                          */
/*-------------------------------------------------------------------*/
int cckddasd_init_handler ( DEVBLK *dev, int argc, BYTE *argv[] )
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             i;                      /* Index                     */
int             rc;                     /* Return code               */
int             fdflags;                /* File flags                */

    /* Obtain area for cckd extension */
    dev->cckd_ext = cckd = malloc(sizeof(CCKDDASD_EXT));
    memset(cckd, 0, sizeof(CCKDDASD_EXT));
    memset(&cckd_empty_l2tab, 0, CCKD_L2TAB_SIZE);

#ifdef CCKD_ITRACEMAX
    /* get internal trace table */
    cckd->itrace = calloc (CCKD_ITRACEMAX, 128);
#endif

    /* Initialize locks and conditions */
    initialize_lock (&cckd->filelock);
    initialize_lock (&cckd->gclock);
    initialize_lock (&cckd->cachelock);
    initialize_lock (&cckd->ralock);
    initialize_condition (&cckd->readcond);
    initialize_condition (&cckd->writecond);
    initialize_condition (&cckd->cachecond);
    initialize_condition (&cckd->gccond);
    initialize_condition (&cckd->termcond);
    initialize_condition (&cckd->racond);
    initialize_condition (&cckd->writercond);

    /* Initialize some variables */
    cckd->l1x = cckd->sfx = -1;
    cckd->gcolsmax = CCKD_DEFAULT_GCOL;
    cckd->writersmax = CCKD_DEFAULT_WRITER;
    cckd->ramax = CCKD_DEFAULT_RA;
    cckd->l2cachenbr = CCKD_L2CACHE_NBR;
    dev->ckdcachenbr = dev->ckdheads + cckd->ramax;

    /* Initialize the readahead queue */
    cckd->ra1st = cckd->ralast = -1;
    cckd->rafree = 0;
    for (i = 0; i < CCKD_RA_SIZE; i++) cckd->ra[i].next = i + 1;
    cckd->ra[CCKD_RA_SIZE - 1].next = -1;

    /* Read the compressed device header */
    cckd->fd[0] = dev->fd;
    fdflags = fcntl (dev->fd, F_GETFL);
    cckd->open[0] = (fdflags & O_RDWR) ? CCKD_OPEN_RW : CCKD_OPEN_RO;
    rc = cckd_read_chdr (dev);
    if (rc < 0) return -1;

    /* Read the level 1 table */
    rc = cckd_read_l1 (dev);
    if (rc < 0) return -1;

    /* call the chkdsk function */
    rc = cckd_chkdsk (cckd->fd[0], dev->msgpipew, 0);
    if (rc < 0) return -1;

    /* re-read the compressed device header */
    rc = cckd_read_chdr (dev);
    if (rc < 0) return -1;

    /* open the shadow files */
    rc = cckd_sf_init (dev);
    if (rc < 0)
    {   devmsg ("cckddasd: error initializing shadow files: %s\n",
                strerror (errno));
        return -1;
    }

    /* display statistics */
//  cckd_sf_stats (dev);

    /* Update the routines */
    dev->hnd = &cckddasd_device_hndinfo;
    dev->ckdrdtrk = &cckd_read_track;
    dev->ckdupdtrk = &cckd_update_track;

    return 0;
} /* end function cckddasd_init_handler */


/*-------------------------------------------------------------------*/
/* Close a Compressed CKD Device                                     */
/*-------------------------------------------------------------------*/
int cckddasd_close_device (DEVBLK *dev)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             i;                      /* Index                     */

    cckd = dev->cckd_ext;

    /* Terminate the readahead threads */
    obtain_lock (&cckd->ralock);
    cckd->ramax = 0;
    if (cckd->ras)
    {
        broadcast_condition (&cckd->racond);
        wait_condition (&cckd->termcond, &cckd->ralock);
    }
    release_lock (&cckd->ralock);

    /* Terminate the garbage collection threads */
    obtain_lock (&cckd->gclock);
    cckd->gcolsmax = 0;
    if (cckd->gcols)
    {
        broadcast_condition (&cckd->gccond);
        wait_condition (&cckd->termcond, &cckd->gclock);
    }
    release_lock (&cckd->gclock);

    /* write any updated track images and wait for writers to stop */
    obtain_lock (&cckd->cachelock);
    cckd_flush_cache (dev);
    cckd->writersmax = 0;
    if (cckd->writepending || cckd->writers)
    {
        broadcast_condition (&cckd->writercond);
        wait_condition (&cckd->termcond, &cckd->cachelock);
    }
    release_lock (&cckd->cachelock);

    /* harden the file */
    cckd_harden (dev);

    /* close the shadow files */
    for (i = 1; i <= cckd->sfn; i++)
    {
        close (cckd->fd[i]);
        cckd->open[i] = 0;
    }

    /* free the level 1 tables */
    for (i = 0; i <= cckd->sfn; i++)
        free (cckd->l1[i]);

    /* free the cache */
    if (cckd->cache)
    {   for (i = 0; i < dev->ckdcachenbr; i++)
            if (cckd->cache[i].buf)
                free (cckd->cache[i].buf);
        free (cckd->cache);
    }

    /* free the level 2 cache */
    if (cckd->l2cache)
    {
        for (i = 0; i < cckd->l2cachenbr; i++)
            if (cckd->l2cache[i].buf)
                free (cckd->l2cache[i].buf);
        free (cckd->l2cache);
    }

    /* write some statistics */
    if (!dev->batch)
        cckd_sf_stats (dev);

    /* free the cckd extension */
    dev->cckd_ext= NULL;
    memset (&dev->ckdsfn, 0, sizeof(dev->ckdsfn));
    free (cckd);

    close (dev->fd);

    return 0;
} /* end function cckddasd_close_device */


/*-------------------------------------------------------------------*/
/* Compressed ckd start/resume channel program                       */
/*-------------------------------------------------------------------*/
void cckddasd_start (DEVBLK *dev)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */

    cckd = dev->cckd_ext;

    obtain_lock (&cckd->cachelock);

    /* If previous active cache entry has been stolen or is busy
       then set it to null and return */
    if (!cckd->active || dev->ckdcurtrk != cckd->active->trk
     || (cckd->active->flags & (CCKD_CACHE_READING | CCKD_CACHE_WRITING)))
    {
        dev->ckdcurtrk = -1;
        cckd->active = NULL;
        release_lock (&cckd->cachelock);
        return;
    }

    /* Make the previous cache entry active again */
    cckd->active->flags |= CCKD_CACHE_ACTIVE;

    /* If the entry is pending write then change it to `updated' */
    if (cckd->active->flags & CCKD_CACHE_WRITE)
    {
        cckd->active->flags &= ~CCKD_CACHE_WRITE;
        cckd->active->flags |= CCKD_CACHE_UPDATED;
        cckd->writepending--;
    }

    release_lock (&cckd->cachelock);
}


/*-------------------------------------------------------------------*/
/* Compressed ckd end/suspend channel program                        */
/*-------------------------------------------------------------------*/
void cckddasd_end (DEVBLK *dev)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */

    cckd = dev->cckd_ext;
    if (cckd->active) cckd->active->flags &= ~CCKD_CACHE_ACTIVE;
}


/*-------------------------------------------------------------------*/
/* Compressed ckd read track image                                   */
/*-------------------------------------------------------------------*/
int cckd_read_track (DEVBLK *dev, int cyl, int head, BYTE *unitstat)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             trk;                    /* New track                 */
CCKD_CACHE     *active;                 /* New active cache entry    */
int             act;                    /* Syncio indicator          */

    cckd = dev->cckd_ext;

    /* Turn off the synchronous I/O bit if trk overflow or trk 0 */
    act = dev->syncio_active;
    if (dev->ckdtrkof || (cyl == 0 && head == 0))
        dev->syncio_active = 0;

    /* Calculate the track number */
    trk = cyl * dev->ckdheads + head;

    /* Command reject if seek position is outside volume */
    if (cyl >= dev->ckdcyls || head >= dev->ckdheads)
    {
        ckd_build_sense (dev, SENSE_CR, 0, 0,
                        FORMAT_0, MESSAGE_4);
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return -1;
    }

    /* Reset buffer offsets */
    dev->bufoff = 0;
    dev->bufoffhi = dev->ckdtrksz;

    /* Return if reading the same track image and not trk 0 */
    if (trk == dev->ckdcurtrk && dev->buf && cckd->active) return 0;

    DEVTRACE ("cckddasd: read  trk   %d (%s)\n", trk,
              dev->syncio_active ? "synchronous" : "asynchronous");
    cckd->switches++;

    /* read the new track */
    *unitstat = 0;
    active = cckd_read_trk (dev, trk, 0, unitstat);
    if (dev->syncio_retry) return -1;
    dev->syncio_active = act;
    cckd->active = active;
    dev->ckdcurtrk = trk;
    dev->buf = active->buf;

    if (*unitstat != 0) return -1;

    return 0;
} /* end function cckd_read_track */


/*-------------------------------------------------------------------*/
/* Compressed ckd update track image                                 */
/*-------------------------------------------------------------------*/
int cckd_update_track (DEVBLK *dev, BYTE *buf, int len, BYTE *unitstat)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */

    cckd = dev->cckd_ext;

    /* Immediately return if fake writing */
    if (dev->ckdfakewr)
        return len;

    /* Error if opened read-only */
    if (dev->ckdrdonly)
    {
        ckd_build_sense (dev, SENSE_EC, SENSE1_WRI, 0,
                        FORMAT_1, MESSAGE_0);
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return -1;
    }

    /* Invalid track format if going past buffer end */
    if (dev->bufoff + len > dev->ckdtrksz)
    {
        ckd_build_sense (dev, 0, SENSE1_ITF, 0, 0, 0);
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return -1;
    }

    /* Copy the data into the buffer */
    if (buf) memcpy (&dev->buf[dev->bufoff], buf, len);

    /* Update the cache entry */
    cckd->active->flags |= CCKD_CACHE_UPDATED | CCKD_CACHE_USED;

    return len;

} /* end function cckd_update_track */


/*-------------------------------------------------------------------*/
/* Read a track image                                                */
/*                                                                   */
/* This routine can be called by the i/o thread (`ra' == 0) or       */
/* by readahead threads (0 < `ra' <= cckd->ramax).                   */
/*                                                                   */
/*-------------------------------------------------------------------*/
CCKD_CACHE *cckd_read_trk(DEVBLK *dev, int trk, int ra, BYTE *unitstat)
{
int             rc;                     /* Return code               */
int             i;                      /* Index variable            */
int             fnd;                    /* Cache index for hit       */
int             lru;                    /* Least-Recently-Used cache
                                            index                    */
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
unsigned long   len,len2;               /* Lengths                   */
BYTE           *buf, buf2[65536];       /* Buffers                   */

    cckd = dev->cckd_ext;

    DEVTRACE("cckddasd: %d rdtrk     %d\n", ra, trk);

    obtain_lock (&cckd->cachelock);

    /* get the cache array if it doesn't exist yet */
    if (cckd->cache == NULL)
        cckd->cache = calloc (dev->ckdcachenbr, CCKD_CACHE_SIZE);

cckd_read_trk_retry:

    /* scan the cache array for the track */
    for (fnd = lru = -1, i = 0; i < dev->ckdcachenbr; i++)
    {
        if (trk == cckd->cache[i].trk && cckd->cache[i].buf)
        {   fnd = i;
            break;
        }
        /* find the oldest entry that isn't busy */
        if (!(cckd->cache[i].flags & CCKD_CACHE_BUSY)
         && (lru == - 1 || cckd->cache[i].age < cckd->cache[lru].age))
            lru = i;
    }

    /* check for cache hit */
    if (fnd >= 0)
    {
        if (ra) /* readahead doesn't care about a cache hit */
        {   release_lock (&cckd->cachelock);
            return NULL;
        }

        /* If synchronous I/O and we need to wait for a read or
           write to complete then return with syncio_retry bit on */
        if (dev->syncio_active
         && cckd->cache[fnd].flags & (CCKD_CACHE_READING | CCKD_CACHE_WRITING))
        {
            DEVTRACE ("cckddasd: %d rdtrk[%2.2d] %d syncio %s\n",
                      ra, lru, trk,
                      cckd->cache[fnd].flags & CCKD_CACHE_READING ?
                      "reading" : "writing");
            dev->syncio_retry = 1;
            release_lock (&cckd->cachelock);
            return NULL;
        }

        /* Mark the entry active */
        DEVTRACE ("cckddasd: %d rdtrk[%2.2d] %d cache hit buf %p\n",
                  ra, fnd, trk, cckd->cache[fnd].buf);
        if (cckd->active) cckd->active->flags &= ~CCKD_CACHE_ACTIVE;
        cckd->cache[fnd].flags |= CCKD_CACHE_ACTIVE | CCKD_CACHE_USED;
        cckd->cache[fnd].age = ++cckd->cacheage;
        cckd->cachehits++;

        /* if read is in progress then wait for it to finish */
        if (cckd->cache[fnd].flags & CCKD_CACHE_READING)
        {
            DEVTRACE ("cckddasd: %d rdtrk[%2.2d] %d waiting for read\n",
                      ra, fnd, trk);
            cckd->cache[fnd].flags |= CCKD_CACHE_READWAIT;
            wait_condition (&cckd->readcond, &cckd->cachelock);
            cckd->cache[fnd].flags &= ~CCKD_CACHE_READWAIT;
            DEVTRACE ("cckddasd: %d rdtrk[%2.2d] %d read wait complete buf %p\n",
                      ra, fnd, trk, cckd->cache[fnd].buf);
        }

        /* if write is in progress then wait for it to finish */
        if (cckd->cache[fnd].flags & CCKD_CACHE_WRITING)
        {
            DEVTRACE ("cckddasd: %d rdtrk[%2.2d] %d waiting for write\n",
                      ra, fnd, trk);
            cckd->cache[fnd].flags |= CCKD_CACHE_WRITEWAIT;
            wait_condition (&cckd->writecond, &cckd->cachelock);
            cckd->cache[fnd].flags &= ~CCKD_CACHE_WRITEWAIT;
            DEVTRACE ("cckddasd: %d rdtrk[%2.2d] %d write wait complete buf %p\n",
                      ra, fnd, trk, cckd->cache[fnd].buf);
        }

        /* If the entry is pending write then change it to `updated' */
        if (cckd->cache[fnd].flags & CCKD_CACHE_WRITE)
        {
            cckd->cache[fnd].flags &= ~CCKD_CACHE_WRITE;
            cckd->cache[fnd].flags |= CCKD_CACHE_UPDATED;
            cckd->writepending--;
        }

        /* Check for readahead */
        if (trk == dev->ckdcurtrk + 1) cckd_readahead (dev, trk);

        release_lock (&cckd->cachelock);
        return &cckd->cache[fnd];
    }

    /* If not readahead and synchronous I/O then return with
       the `syio_retry' bit set */
    if (!ra && dev->syncio_active)
    {
        DEVTRACE ("cckddasd: %d rdtrk[%2.2d] %d syncio cache miss\n",
                   ra, lru, trk);
        dev->syncio_retry = 1;
        release_lock (&cckd->cachelock);
        return NULL;
    }

    /* If no cache entry was stolen, then schedule all updated entries
       to be written, and wait for a cache entry unless readahead */
    if (lru < 0)
    {
        DEVTRACE ("cckddasd: %d rdtrk[%2.2d] %d no available cache entry\n",
                   ra, lru, trk);
        cckd_flush_cache (dev);
        if (ra)
        {
            release_lock (&cckd->cachelock);
            return NULL;
        }
        cckd->cachewaits++;
        wait_condition (&cckd->cachecond, &cckd->cachelock);
        cckd->cachewaits--;
        goto cckd_read_trk_retry;
    }

    /* get buffer if there isn't one */
    if (cckd->cache[lru].buf == NULL)
    {
        cckd->cache[lru].buf = malloc (dev->ckdtrksz);
        DEVTRACE ("cckddasd: %d rdtrk[%2.2d] %d get buf %p\n",
                  ra, lru, trk, cckd->cache[lru].buf);
        if (!cckd->cache[lru].buf)
        {
            devmsg ("%4.4X cckddasd: %d rdtrk[%2.2d] %d malloc failed: %s\n",
                    dev->devnum, ra, lru, trk, strerror(errno));
            cckd->cache[lru].flags |= CCKD_CACHE_INVALID;
            goto cckd_read_trk_retry;
        }
    }
    else
    {
        DEVTRACE ("cckddasd: %d rdtrk[%2.2d] %d dropping %d from cache buf %p\n",
                  ra, lru, trk, cckd->cache[lru].trk, cckd->cache[lru].buf);
        if (!(cckd->cache[lru].flags & CCKD_CACHE_USED)) cckd->misses++;
    }

    /* Mark the entry active if not readahead */
    if (ra == 0)
    {
        if (cckd->active) cckd->active->flags &= ~CCKD_CACHE_ACTIVE;
        cckd->cache[lru].flags = CCKD_CACHE_ACTIVE | CCKD_CACHE_USED;
    }
    else cckd->cache[lru].flags = 0;

    buf = cckd->cache[lru].buf;
    cckd->cache[lru].trk = trk;
    if (ra) cckd->readaheads++;
    cckd->cache[lru].flags |= CCKD_CACHE_READING;
    cckd->cache[lru].age = ++cckd->cacheage;

    /* asynchrously schedule readaheads */
    if (!ra && trk == dev->ckdcurtrk + 1)
        cckd_readahead (dev, trk);

    release_lock (&cckd->cachelock);

    /* read the track image */
    obtain_lock (&cckd->filelock);
    len2 = cckd_read_trkimg (dev, (BYTE *)&buf2, trk, unitstat);
    release_lock (&cckd->filelock);
    DEVTRACE ("cckddasd: %d rdtrk[%2.2d] %d read len %ld\n",
              ra, lru, trk, len2);

    /* uncompress the track image */
    switch (buf2[0]) {

    case CCKD_COMPRESS_NONE:
        memcpy (buf, &buf2, len2);
        len = len2;
        DEVTRACE("cckddasd: %d rdtrk[%2.2d] %d not compressed\n",
                 ra, lru, trk);
        break;

    case CCKD_COMPRESS_ZLIB:
        /* Uncompress the track image using zlib */
        memcpy (buf, &buf2, CKDDASD_TRKHDR_SIZE);
        len = dev->ckdtrksz - CKDDASD_TRKHDR_SIZE;
        rc = uncompress(&buf[CKDDASD_TRKHDR_SIZE],
                        &len, &buf2[CKDDASD_TRKHDR_SIZE],
                        len2 - CKDDASD_TRKHDR_SIZE);
        len += CKDDASD_TRKHDR_SIZE;
        DEVTRACE("cckddasd: %d rdtrk[%2.2d] %d uncompressed len %ld code %d\n",
                 ra, lru, trk, len, rc);
        if (rc != Z_OK)
        {   devmsg ("%4.4X cckddasd: rdtrk %d uncompress error: %d\n",
                        dev->devnum, trk, rc);
            len = cckd_null_trk (dev, buf, trk, 0);
            if (unitstat)
            {
                ckd_build_sense (dev, SENSE_EC, 0, 0, FORMAT_1, MESSAGE_0);
                *unitstat = CSW_CE | CSW_DE | CSW_UC;
            }
        }
        break;

#ifdef CCKD_BZIP2
    case CCKD_COMPRESS_BZIP2:
        /* Decompress the track image using bzip2 */
        memcpy (buf, &buf2, CKDDASD_TRKHDR_SIZE);
        len = dev->ckdtrksz - CKDDASD_TRKHDR_SIZE;
        rc = BZ2_bzBuffToBuffDecompress (
                        &buf[CKDDASD_TRKHDR_SIZE],
                        (unsigned int *)&len,
                        &buf2[CKDDASD_TRKHDR_SIZE],
                        len2 - CKDDASD_TRKHDR_SIZE, 0, 0);
        len += CKDDASD_TRKHDR_SIZE;
        DEVTRACE("cckddasd: %d rdtrk[%2.2d] %d decompressed len %ld code %d\n",
                 ra, lru, trk, len, rc);
        if (rc != BZ_OK)
        {   devmsg ("cckddasd: decompress error for trk %d: %d\n",
                    trk, rc);
            len = cckd_null_trk (dev, buf, trk, 0);
            if (unitstat)
            {
                ckd_build_sense (dev, SENSE_EC, 0, 0, FORMAT_1, MESSAGE_0);
                *unitstat = CSW_CE | CSW_DE | CSW_UC;
            }
        }
        break;
#endif

    default:
        devmsg ("cckddasd: %4.4x unknown compression for trk %d: %d\n",
                dev->devnum, trk, buf2[0]);
        len = cckd_null_trk (dev, buf, trk, 0);
        if (unitstat)
        {
            ckd_build_sense (dev, SENSE_EC, 0, 0, FORMAT_1, MESSAGE_0);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
        }
        break;
    }

    obtain_lock (&cckd->cachelock);

    cckd->cache[lru].flags &= ~CCKD_CACHE_READING;

    /* wakeup other thread waiting for this read */
    if (cckd->cache[lru].flags & CCKD_CACHE_READWAIT)
    {   DEVTRACE("cckddasd: %d rdtrk[%2.2d] %d signalling read complete buf %p\n",
                 ra, lru, trk, cckd->cache[lru].buf);
        signal_condition (&cckd->readcond);
    }

    /* wakeup cache entry waiters if readahead */
    if (ra && cckd->cachewaits)
    {   DEVTRACE("cckddasd: %d rdtrk[%2.2d] %d signalling cache entry available\n",
                 ra, lru, trk);
        signal_condition (&cckd->cachecond);
    }

    DEVTRACE("cckddasd: %d rdtrk[%2.2d] %d complete buf %p\n",
              ra, lru, trk, buf);

    release_lock (&cckd->cachelock);

    return &cckd->cache[lru];

} /* end function cckd_read_trk */


/*-------------------------------------------------------------------*/
/* Schedule asynchronous readaheads                                  */
/*-------------------------------------------------------------------*/
void cckd_readahead (DEVBLK *dev, int trk)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             ra[CCKD_RA_SIZE];       /* Thread create switches    */
int             i, r;                   /* Indexes                   */
TID             tid;                    /* Readahead thread id       */

    cckd = dev->cckd_ext;

    if (dev->ckdcachenbr < cckd->ramax + 2 || cckd->ramax < 1 || dev->batch)
        return;

    /* Initialize */
    memset ( ra, 0, sizeof (ra));

    /* make sure readahead tracks aren't already cached */
    for (i = 0; i < dev->ckdcachenbr; i++)
        if (cckd->cache[i].trk > trk
         && cckd->cache[i].trk <= trk + cckd->ramax)
            ra[cckd->cache[i].trk - (trk+1)] = 1;

    /* Queue the tracks to the readahead queue */
    obtain_lock (&cckd->ralock);
    for (i = 0; i < cckd->ramax && cckd->rafree >= 0; i++)
    {
        if (ra[i] || trk + 1 + i >= dev->ckdtrks) continue;
        r = cckd->rafree;
        cckd->rafree = cckd->ra[r].next;
        if (cckd->ralast < 0)
        {
            cckd->ra1st = cckd->ralast = r;
            cckd->ra[r].prev = cckd->ra[r].next = -1;
        }
        else
        {
            cckd->ra[cckd->ralast].next = r;
            cckd->ra[r].prev = cckd->ralast;
            cckd->ra[r].next = -1;
            cckd->ralast = r;
        }
        cckd->ra[r].trk = trk + 1 + i;
    }

    /* Schedule the readahead if any are pending */
    if (cckd->ra1st >= 0)
    {
        if (cckd->rawaiting)
            signal_condition (&cckd->racond);
        else if (cckd->ras < cckd->ramax)
            create_thread (&tid, NULL, cckd_ra, dev);
    }
    release_lock (&cckd->ralock);
} /* end function cckd_readahead */


/*-------------------------------------------------------------------*/
/* Asynchronous readahead thread                                     */
/*-------------------------------------------------------------------*/
void cckd_ra (DEVBLK *dev)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             trk;                    /* Readahead track           */
int             ra;                     /* Readahead index           */
int             r;                      /* Readahead queue index     */
TID             tid;                    /* Readahead thread id       */

    cckd = dev->cckd_ext;

    obtain_lock (&cckd->ralock);
    ra = ++cckd->ras;
    
    /* Return without messages if too many already started */
    if (ra > cckd->ramax)
    {
        --cckd->ras;
        release_lock (&cckd->ralock);
        return;
    }

    devmsg ("%4.4X: cckd_readahead[%d] thread started: tid="TIDPAT", pid = %d\n",
            dev->devnum, ra, thread_id(), getpid());

    while (ra <= cckd->ramax)
    {
        if (cckd->ra1st < 0)
        {
            cckd->rawaiting++;
            wait_condition (&cckd->racond, &cckd->ralock);
            cckd->rawaiting--;
        }

        /* Possibly shutting down if no writes pending */
        if (cckd->ra1st < 0) continue;

        /* Requeue the 1st entry to the readahead free queue */
        r = cckd->ra1st;
        trk = cckd->ra[r].trk;
        cckd->ra1st = cckd->ra[r].next;
        if (cckd->ra[r].next > -1)
            cckd->ra[cckd->ra[r].next].prev = -1;
        else cckd->ralast = -1;
        cckd->ra[r].next = cckd->rafree;
        cckd->rafree = r;

        /* Schedule the other readaheads if any are still pending */
        if (cckd->ra1st)
        {
            if (cckd->rawaiting)
                signal_condition (&cckd->racond);
            else if (cckd->ras < cckd->ramax)
                create_thread (&tid, NULL, cckd_ra, dev);
        }
        release_lock (&cckd->ralock);

        /* Read the readahead track */
        cckd_read_trk (dev, trk, ra, NULL);

        obtain_lock (&cckd->ralock);
    }

    devmsg ("%4.4X cckd_readahead[%d] thread stopping: tid="TIDPAT", pid = %d\n",
            dev->devnum, ra, thread_id(), getpid());
    --cckd->ras;
    if (!cckd->ras) signal_condition(&cckd->termcond);
    release_lock(&cckd->ralock);

} /* end thread cckd_ra_thread */


/*-------------------------------------------------------------------*/
/* Flush updated cache entries                                       */
/*-------------------------------------------------------------------*/
void cckd_flush_cache(DEVBLK *dev)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             i;                      /* Index                     */
TID             tid;                    /* Writer thread id          */

    cckd = dev->cckd_ext;
    if (cckd->cache == NULL) return;

    /* Scan cache for updated cache entries */
    for (i = 0; i < dev->ckdcachenbr; i++)
        if ((cckd->cache[i].flags & CCKD_CACHE_BUSY) == CCKD_CACHE_UPDATED)
        {
            cckd->cache[i].flags &= ~CCKD_CACHE_UPDATED;
            cckd->cache[i].flags |= CCKD_CACHE_WRITE;
            cckd->writepending++;
        }

    /* Schedule the writer if any writes are pending */
    if (cckd->writepending)
    {
        if (cckd->writewaiting)
            signal_condition (&cckd->writercond);
        else if (cckd->writers < cckd->writersmax)
            create_thread (&tid, NULL, cckd_writer, dev);
    }
}


/*-------------------------------------------------------------------*/
/* Writer thread                                                     */
/*-------------------------------------------------------------------*/
void cckd_writer(DEVBLK *dev)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             writer;                 /* Writer identifier         */
int             i, o;                   /* Indexes                   */
BYTE           *buf;                    /* Buffer                    */
BYTE            buf2[65536];            /* Compress buffer           */
BYTE           *bufp;                   /* Buffer to be written      */
unsigned long   len, len2, bufl;        /* Buffer lengths            */
int             trk;                    /* Track number              */
BYTE            compress;               /* Compression algorithm     */
TID             tid;                    /* Writer thead id           */
int             rc;                     /* Return code               */

    cckd = dev->cckd_ext;

#ifndef WIN32
    /* Set writer priority just below cpu priority to mimimize the
       compression effect */
    if(dev->cpuprio >= 0)
        setpriority(PRIO_PROCESS, 0, dev->cpuprio+1);
#endif

    obtain_lock (&cckd->cachelock);
    writer = ++cckd->writers;

    /* Return without messages if too many already started */
    if (writer > cckd->writersmax
     && !(cckd->writersmax == 0 && cckd->writepending))
    {
        --cckd->writers;
        release_lock (&cckd->cachelock);
        return;
    }

    devmsg ("%4.4X:cckddasd: %d writer thread started: tid="TIDPAT", pid = %d\n",
            dev->devnum, writer, thread_id(), getpid());

    while (writer <= cckd->writersmax || cckd->writepending)
    {
        /* Wait for work */
        if (!cckd->writepending)
        {
            cckd->writewaiting++;
            wait_condition (&cckd->writercond, &cckd->cachelock);
            cckd->writewaiting--;
        }

        /* Scan the cache for the oldest pending entry */
        for (o = -1, i = 0; i < dev->ckdcachenbr; i++)
            if ((cckd->cache[i].flags & CCKD_CACHE_WRITE)
             && (o == -1 || cckd->cache[i].age < cckd->cache[o].age))
                o = i;

        /* Possibly shutting down if no writes pending */
        if (o < 0)
        {
            cckd->writepending = 0;
            continue;
        }

        cckd->writepending--;
        cckd->cache[o].flags &= ~CCKD_CACHE_WRITE;
        cckd->cache[o].flags |= CCKD_CACHE_WRITING;

        /* Schedule the other writers if any writes are still pending */
        if (cckd->writepending)
        {
            if (cckd->writewaiting)
                signal_condition (&cckd->writercond);
            else if (cckd->writers < cckd->writersmax)
                create_thread (&tid, NULL, cckd_writer, dev);
        }

        release_lock (&cckd->cachelock);

        /* Compress the track image */

        buf = cckd->cache[o].buf;
        len = cckd_trklen (dev, buf);
        trk = cckd->cache[o].trk;
        compress = (len < CCKD_COMPRESS_MIN ?
                    CCKD_COMPRESS_NONE : cckd->cdevhdr[cckd->sfn].compress);

        DEVTRACE ("cckddasd: %d wrtrk[%2.2d] %d\n", writer, o, cckd->cache[o].trk);

        switch (compress) {

        case CCKD_COMPRESS_ZLIB:
            memcpy (&buf2, buf, CKDDASD_TRKHDR_SIZE);
            buf2[0] = CCKD_COMPRESS_ZLIB;
            len2 = 65535 - CKDDASD_TRKHDR_SIZE;
            rc = compress2 (&buf2[CKDDASD_TRKHDR_SIZE], &len2,
                            &buf[CKDDASD_TRKHDR_SIZE], len - CKDDASD_TRKHDR_SIZE,
                            cckd->cdevhdr[cckd->sfn].compress_parm);
            len2 += CKDDASD_TRKHDR_SIZE;
            DEVTRACE("cckddasd: writer[%d] compress trk %d len %lu rc=%d\n",
                     writer, trk, len2, rc);
            if (rc != Z_OK || len2 >= len)
            {
                bufp = buf;
                bufl = len;
            }
            else
            {
                bufp = (BYTE *)&buf2;
                bufl = len2;
            }
            break;

#ifdef CCKD_BZIP2
        case CCKD_COMPRESS_BZIP2:
            memcpy (&buf2, buf, CKDDASD_TRKHDR_SIZE);
            buf2[0] = CCKD_COMPRESS_BZIP2;
            len2 = 65535 - CKDDASD_TRKHDR_SIZE;
            rc = BZ2_bzBuffToBuffCompress (
                            &buf2[CKDDASD_TRKHDR_SIZE], (unsigned int *)&len2,
                            &buf[CKDDASD_TRKHDR_SIZE], len - CKDDASD_TRKHDR_SIZE,
                            cckd->cdevhdr[cckd->sfn].compress_parm >= 1 &&
                            cckd->cdevhdr[cckd->sfn].compress_parm <= 9 ?
                            cckd->cdevhdr[cckd->sfn].compress_parm : 5, 0, 0);
            len2 += CKDDASD_TRKHDR_SIZE;
            DEVTRACE("cckddasd: %d writer compress trk %d len %lu rc=%d\n",
                     writer, trk, len2, rc);
            if (rc != BZ_OK || len2 >= len)
            {
                bufp = buf;
                bufl = len;
            }
            else
            {
                bufp = (BYTE *)&buf2;
                bufl = len2;
            }
            break;
#endif

        default:
        case CCKD_COMPRESS_NONE:
            bufp = buf;
            bufl = len;
            break;
        } /* switch (compress) */

        obtain_lock (&cckd->filelock);

        /* Turn on read-write header bits if not already on */
        if (!(cckd->cdevhdr[cckd->sfn].options & CCKD_OPENED))
        {
            cckd->cdevhdr[cckd->sfn].options |= (CCKD_OPENED | CCKD_ORDWR);
            cckd_write_chdr (dev);
        }

        /* Write the track image */
        cckd_write_trkimg (dev, bufp, bufl, trk);

        release_lock (&cckd->filelock);

        /* Schedule the garbage collector */
        if (cckd->cdevhdr[cckd->sfn].free_number
         && cckd->gcols < cckd->gcolsmax)
            create_thread (&tid, NULL, cckd_gcol, dev);

        obtain_lock (&cckd->cachelock);
        cckd->cache[o].flags &= ~CCKD_CACHE_WRITING;
        if (cckd->cache[o].flags & CCKD_CACHE_WRITEWAIT)
        {   DEVTRACE("cckddasd: writer[%d] cache[%2.2d] %d signalling write complete\n",
                 writer, o, trk);
            signal_condition (&cckd->writecond);
        }
        else if (cckd->cachewaits)
        {   DEVTRACE("cckddasd: writer[%d] cache[%2.2d] %d signalling"
            " cache entry available\n", writer, o, trk);
            signal_condition (&cckd->cachecond);
        }

        DEVTRACE ("cckddasd: %d wrtrk[%2.2d] %d complete flags:%8.8x\n",
                  writer, o, cckd->cache[o].trk, cckd->cache[o].flags);

    }

    devmsg ("%4.4X:cckddasd: %d writer thread stopping: tid="TIDPAT", pid = %d\n",
            dev->devnum, writer, thread_id(), getpid());
    cckd->writers--;
    if (!cckd->writers) signal_condition(&cckd->termcond);
    release_lock(&cckd->cachelock);
} /* end thread cckd_writer */


/*-------------------------------------------------------------------*/
/* Get file space                                                    */
/*-------------------------------------------------------------------*/
off_t cckd_get_space(DEVBLK *dev, int len)
{
int             rc;                     /* Return code               */
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             i,p,n;                  /* Free space indices        */
off_t           fpos;                   /* Free space offset         */
int             flen;                   /* Free space size           */
int             sfx;                    /* Shadow file index         */

    cckd = dev->cckd_ext;
    sfx = cckd->sfn;

    DEVTRACE("cckddasd: get_space len %d\n", len);

    if (len == 0 || len == 0xffff) return 0;
    if (!cckd->free) cckd_read_fsp (dev);

    if (!(len == cckd->cdevhdr[sfx].free_largest ||
          len + CCKD_FREEBLK_SIZE <= cckd->cdevhdr[sfx].free_largest))
    { /* no free space big enough; add space to end of the file */
        fpos = cckd->cdevhdr[sfx].size;
        cckd->cdevhdr[sfx].size += len;
        rc = ftruncate (dev->fd, cckd->cdevhdr[sfx].size);
        cckd->cdevhdr[sfx].used += len;
        DEVTRACE("cckddasd: get_space_at_end pos 0x%llx len %d\n",
                 (long long)fpos, len);
        return fpos;
    }

    /* scan free space chain */
    for (i = cckd->free1st; i >= 0; i = cckd->free[i].next)
        if (cckd->free[i].len == len ||
            cckd->free[i].len >= len + CCKD_FREEBLK_SIZE) break;

    p = cckd->free[i].prev;
    n = cckd->free[i].next;

    /* found a free space, obtain its file position and length */
    if (p >= 0) fpos = cckd->free[p].pos;
    else fpos = cckd->cdevhdr[sfx].free;
    flen = cckd->free[i].len;

    /* remove the new space from free space */
    if (cckd->free[i].len >= len + CCKD_FREEBLK_SIZE)
    { /* only use a portion of the free space */
        cckd->free[i].len -= len;
        if (p >= 0) cckd->free[p].pos += len;
        else cckd->cdevhdr[sfx].free += len;
    }
    else
    { /* otherwise use the entire free space */
        cckd->cdevhdr[sfx].free_number--;
        /* remove the free space entry from the chain */
        if (p >= 0)
        {
            cckd->free[p].pos = cckd->free[i].pos;
            cckd->free[p].next = n;
        }
        else
        {
            cckd->cdevhdr[sfx].free = cckd->free[i].pos;
            cckd->free1st = n;
        }
        if (n >= 0) cckd->free[n].prev = p;

        /* make this free space entry available */
        cckd->free[i].next = cckd->freeavail;
        cckd->freeavail = i;
    }

    /* find the largest free space if we got the largest */
    if (flen >= cckd->cdevhdr[sfx].free_largest)
    {
        cckd->cdevhdr[sfx].free_largest = 0;
        for (i = cckd->free1st; i >= 0; i = cckd->free[i].next)
            if (cckd->free[i].len > cckd->cdevhdr[sfx].free_largest)
                cckd->cdevhdr[sfx].free_largest = cckd->free[i].len;
     }

    /* update free space stats */
    cckd->cdevhdr[sfx].used += len;
    cckd->cdevhdr[sfx].free_total -= len;

    DEVTRACE("cckddasd: get_space found pos 0x%llx len %d\n",
             (long long)fpos, len);

    return fpos;

} /* end function cckd_get_space */


/*-------------------------------------------------------------------*/
/* Release file space                                                */
/*-------------------------------------------------------------------*/
void cckd_rel_space(DEVBLK *dev, unsigned int pos, int len)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
off_t           ppos,npos;              /* Prev/next offsets         */
int             p2,p,i,n,n2;            /* Free space indices        */
int             sfx;                    /* Shadow file index         */

    if (len == 0 || len == 0xffff) return;

    cckd = dev->cckd_ext;
    sfx = cckd->sfn;

    DEVTRACE("cckddasd: rel_space pos 0x%x len %d %s\n",
            pos, len, pos + len == cckd->cdevhdr[sfx].size ? "at end" : "");

    if (!cckd->free) cckd_read_fsp (dev);

    /* increase the size of the free space array if necessary */
    if (cckd->freeavail < 0)
    {
        cckd->freeavail = cckd->freenbr;
        cckd->freenbr += 1024;
        cckd->free =
             realloc ( cckd->free, cckd->freenbr * CCKD_FREEBLK_ISIZE);
        for (i = cckd->freeavail; i < cckd->freenbr; i++)
            cckd->free[i].next = i + 1;
        cckd->free[i-1].next = -1;
    }

    /* get a free space entry */
    i = cckd->freeavail;
    cckd->freeavail = cckd->free[i].next;
    cckd->free[i].len = len;

    /* update the free space statistics */
    cckd->cdevhdr[sfx].free_number++;
    cckd->cdevhdr[sfx].used -= len;
    cckd->cdevhdr[sfx].free_total += len;

    /* scan free space chain */
    p = p2 = n2 = -1; npos = cckd->cdevhdr[sfx].free; ppos = 0;
    for (n = cckd->free1st; n >= 0; n = cckd->free[n].next)
    {
        if (pos < npos) break;
        p2 = p;
        p = n;
        ppos = npos;
        npos = cckd->free[n].pos;
    }
    if (n >= 0) n2 = cckd->free[n].next;

    /* insert the new entry into the chain */
    cckd->free[i].prev = p;
    cckd->free[i].next = n;
    if (p >= 0)
    {
        cckd->free[i].pos = cckd->free[p].pos;
        cckd->free[p].pos = pos;
        cckd->free[p].next = i;
    }
    else
    {
        cckd->free[i].pos = cckd->cdevhdr[sfx].free;
        cckd->cdevhdr[sfx].free = pos;
        cckd->free1st = i;
    }
    if (n >= 0)
        cckd->free[n].prev = i;

    /* if the new free space is adjacent to the previous free
       space then combine the two */
    if (p >= 0 && ppos + cckd->free[p].len == pos)
    {
        cckd->cdevhdr[sfx].free_number--;
        cckd->free[p].pos = cckd->free[i].pos;
        cckd->free[p].len += cckd->free[i].len;
        cckd->free[p].next = cckd->free[i].next;
        cckd->free[i].next = cckd->freeavail;
        cckd->freeavail = i;
        if (n >= 0) cckd->free[n].prev = p;
        i = p;
        pos = ppos;
        p = p2;
    }

    /* if the new free space is adjacent to the following free
       space then combine the two */
    if (n >= 0 && pos + cckd->free[i].len == npos)
        {
        cckd->cdevhdr[sfx].free_number--;
        cckd->free[i].pos = cckd->free[n].pos;
        cckd->free[i].len += cckd->free[n].len;
        cckd->free[i].next = cckd->free[n].next;
        cckd->free[n].next = cckd->freeavail;
        cckd->freeavail = n;
        n = n2;
        if (n >= 0) cckd->free[n].prev = i;
    }

    /* update the largest free space */
    if (cckd->free[i].len > cckd->cdevhdr[sfx].free_largest)
        cckd->cdevhdr[sfx].free_largest = cckd->free[i].len;

    /* truncate the file if the free space is at the end */
    if (pos + cckd->free[i].len == cckd->cdevhdr[sfx].size)
        cckd_rel_free_atend (dev, pos, cckd->free[i].len, i);

} /* end function cckd_rel_space */


/*-------------------------------------------------------------------*/
/* Release free space at end of file                                 */
/*-------------------------------------------------------------------*/
void cckd_rel_free_atend(DEVBLK *dev, unsigned int pos, int len, int i)
{
int             rc;                     /* Return code               */
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             p;                      /* Prev free space index     */
int             sfx;                    /* Shadow file index         */

    cckd = dev->cckd_ext;
    sfx = cckd->sfn;

    DEVTRACE("cckddasd: rel_free_atend ix %d pos %x len %d sz %d\n",
            i, pos, len, cckd->cdevhdr[sfx].size);

    cckd->cdevhdr[sfx].free_number--;
    cckd->cdevhdr[sfx].size -= cckd->free[i].len;
    cckd->cdevhdr[sfx].free_total -= cckd->free[i].len;
    p = cckd->free[i].prev;
    if (p >= 0)
    {
        cckd->free[p].pos = 0;
        cckd->free[p].next = -1;
    }
    else
    {
        cckd->cdevhdr[sfx].free = 0;
        cckd->free1st = -1;
    }
    cckd->free[i].next = cckd->freeavail;
    cckd->freeavail = i;
    rc = ftruncate (cckd->fd[sfx], cckd->cdevhdr[sfx].size);
    if (cckd->free[i].len >= cckd->cdevhdr[sfx].free_largest)
    {   /* find the next largest free space */
        cckd->cdevhdr[sfx].free_largest = 0;
        for (i = cckd->free1st; i >= 0; i = cckd->free[i].next)
            if (cckd->free[i].len > cckd->cdevhdr[sfx].free_largest)
                cckd->cdevhdr[sfx].free_largest = cckd->free[i].len;
    }

} /* end function cckd_rel_free_atend */


/*-------------------------------------------------------------------*/
/* Read compressed dasd header                                       */
/*-------------------------------------------------------------------*/
int cckd_read_chdr (DEVBLK *dev)
{
CKDDASD_DEVHDR  devhdr;                 /* Device header             */
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             rc;                     /* Return code               */
int             sfx;                    /* File index                */
int             fend,mend;              /* Byte order indicators     */

    cckd = dev->cckd_ext;
    sfx = cckd->sfn;

    memset (&cckd->cdevhdr[sfx], 0, CCKDDASD_DEVHDR_SIZE);

    /* read the device header */
    rc = lseek(cckd->fd[sfx], 0, SEEK_SET);
    if (rc == -1) return -1;
    rc = read (cckd->fd[sfx], &devhdr, CKDDASD_DEVHDR_SIZE);
    if (rc != CKDDASD_DEVHDR_SIZE) return -1;

    /* if the file is a regular ckd file, return */
    if (!memcmp (devhdr.devid, "CKD_P370", 8) && !sfx) return 0;

    /* check the device id for shadow files */
    if (sfx && memcmp (devhdr.devid, "CKD_S370", 8))
    {
        devmsg ("file [%d] is not a shadow file\n", cckd->sfn);
        return -1;
    }

    /* read the compressed device header */
    rc = read (cckd->fd[sfx], &cckd->cdevhdr[sfx], CCKDDASD_DEVHDR_SIZE);
    if (rc != CCKDDASD_DEVHDR_SIZE) return -1;

    /* check endian format */
    cckd->swapend[sfx] = 0;
    fend = ((cckd->cdevhdr[sfx].options & CCKD_BIGENDIAN) != 0);
    mend = cckd_endian ();
    if (fend != mend)
    {
        if (cckd->open[sfx] == CCKD_OPEN_RW)
        {
            rc = cckd_swapend (cckd->fd[sfx], dev->msgpipew);
            rc = lseek (cckd->fd[sfx], CKDDASD_DEVHDR_SIZE, SEEK_SET);
            rc = read (cckd->fd[sfx], &cckd->cdevhdr[sfx], CCKDDASD_DEVHDR_SIZE);
        }
        else
        {
            cckd->swapend[sfx] = 1;
            cckd_swapend_chdr (&cckd->cdevhdr[sfx]);
        }
    }

    return 0;

} /* end function cckd_read_chdr */


/*-------------------------------------------------------------------*/
/* Write compressed dasd header                                      */
/*-------------------------------------------------------------------*/
void cckd_write_chdr (DEVBLK *dev)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             rc;                     /* Return code               */
int             sfx;                    /* File index                */

    cckd = dev->cckd_ext;
    sfx = cckd->sfn;

    /* return if a regular ckd file */
    if (!cckd->cdevhdr[sfx].numl1tab) return;

    rc = lseek(cckd->fd[sfx], CKDDASD_DEVHDR_SIZE, SEEK_SET);
    rc = write (cckd->fd[sfx], &cckd->cdevhdr[sfx], CCKDDASD_DEVHDR_SIZE);

} /* end function cckd_write_chdr */


/*-------------------------------------------------------------------*/
/* Read the level 1 table                                            */
/*-------------------------------------------------------------------*/
int cckd_read_l1 (DEVBLK *dev)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             rc;                     /* Return code               */
int             sfx;                    /* File index                */
int             n;                      /* Number entries            */

    cckd = dev->cckd_ext;
    sfx = cckd->sfn;

    /* calculate size of the level 1 table */
    if (cckd->cdevhdr[sfx].numl1tab)
        n = cckd->cdevhdr[sfx].numl1tab;
    else n = (dev->ckdtrks + 255) >> 8;

    /* Free the old level 1 table if it exists */
    if (cckd->l1[sfx] != NULL) free (cckd->l1[sfx]);

    /* get the level 1 table */
    cckd->l1[sfx] = malloc (n * CCKD_L1ENT_SIZE);
    if (cckd->l1[sfx] == NULL) return -1;

    if (cckd->cdevhdr[sfx].numl1tab)
    { /* read the level 1 table */
        rc = lseek (cckd->fd[sfx], CCKD_L1TAB_POS, SEEK_SET);
        if (rc == -1) return -1;
        rc = read(cckd->fd[sfx], cckd->l1[sfx], n * CCKD_L1ENT_SIZE);
        if (rc != n * CCKD_L1ENT_SIZE) return -1;
        if (cckd->swapend[sfx]) cckd_swapend_l1 (cckd->l1[sfx], n);
    }
    else memset (cckd->l1[sfx], 0xff, n * CCKD_L1ENT_SIZE);

    return 0;

} /* end function cckd_read_l1 */


/*-------------------------------------------------------------------*/
/* Write the level 1 table                                           */
/*-------------------------------------------------------------------*/
void cckd_write_l1 (DEVBLK *dev)
{
CCKDDASD_EXT    *cckd;                  /* -> cckd extension         */
int             rc;                     /* Return code               */
int             sfx;                    /* File index                */

    cckd = dev->cckd_ext;
    sfx = cckd->sfn;

    if (cckd->l1updated)
    {
        rc = lseek (cckd->fd[sfx], CCKD_L1TAB_POS, SEEK_SET);
        rc = write (cckd->fd[sfx], cckd->l1[sfx],
                    cckd->cdevhdr[sfx].numl1tab*CCKD_L1ENT_SIZE);
        cckd->l1updated = 0;
        DEVTRACE ("cckddasd: l1tab written pos 0x%lx\n",
        	  (unsigned long) CCKD_L1TAB_POS);
    }

} /* end function cckd_write_l1 */


/*-------------------------------------------------------------------*/
/* Update a level 1 table entry                                      */
/*-------------------------------------------------------------------*/
void cckd_write_l1ent (DEVBLK *dev, int l1x)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             rc;                     /* Return code               */
int             sfx;                    /* File index                */

    cckd = dev->cckd_ext;
    sfx = cckd->sfn;

    rc = lseek (cckd->fd[sfx], (off_t)(CCKD_L1TAB_POS + l1x * CCKD_L1ENT_SIZE),
                SEEK_SET);
    rc = write (cckd->fd[sfx], &cckd->l1[sfx][l1x], CCKD_L1ENT_SIZE);

    DEVTRACE("cckddasd: l1[%d,%d] updated pos 0x%x\n",
             sfx, l1x, cckd->l1[sfx][l1x]);

} /* end function cckd_write_l1ent */


/*-------------------------------------------------------------------*/
/* Read  the free space                                              */
/*-------------------------------------------------------------------*/
int cckd_read_fsp (DEVBLK *dev)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             rc;                     /* Return code               */
off_t           fpos;                   /* Free space offset         */
int             sfx;                    /* File index                */
int             i;                      /* Index                     */

    cckd = dev->cckd_ext;
    sfx = cckd->sfn;

    DEVTRACE ("cckddasd: reading free space, number %d\n",
              cckd->cdevhdr[sfx].free_number);

    /* get storage for the internal free space chain;
       get a multiple of 1024 entries. */
    cckd->freenbr = ((cckd->cdevhdr[sfx].free_number << 10) + 1) >> 10;
    cckd->free = calloc (cckd->freenbr, CCKD_FREEBLK_ISIZE);

    /* if the only free space is at the end of the file,
       then remove it.  this should only happen for a file
       built by the cckddump os/390 utility */
    if (cckd->cdevhdr[sfx].free_number == 1)
    {
        fpos = (off_t)cckd->cdevhdr[sfx].free;
        rc = lseek (cckd->fd[sfx], fpos, SEEK_SET);
        rc = read (cckd->fd[sfx], &cckd->free[0], CCKD_FREEBLK_SIZE);
        if (fpos + cckd->free[0].len == cckd->cdevhdr[sfx].size)
        {
            cckd->cdevhdr[sfx].free_number = 
            cckd->cdevhdr[sfx].free_total = 
            cckd->cdevhdr[sfx].free_largest = 0;
            cckd->cdevhdr[sfx].size -= cckd->free[0].len;
            rc = ftruncate (cckd->fd[sfx], cckd->cdevhdr[sfx].size);
        }
    }

    /* build the doubly linked internal free space chain */
    if (cckd->cdevhdr[sfx].free_number)
    {
        cckd->free1st = 0;
        fpos = (off_t)cckd->cdevhdr[sfx].free;
        for (i = 0; i < cckd->cdevhdr[sfx].free_number; i++)
        {
            rc = lseek (cckd->fd[sfx], fpos, SEEK_SET);
            rc = read (cckd->fd[sfx], &cckd->free[i], CCKD_FREEBLK_SIZE);
            cckd->free[i].prev = i - 1;
            cckd->free[i].next = i + 1;
            fpos = cckd->free[i].pos;
        }
        cckd->free[i-1].next = -1;
    }
    else cckd->free1st = -1;

    /* build singly linked chain of available free space entries */
    if (cckd->cdevhdr[sfx].free_number < cckd->freenbr)
    {
        cckd->freeavail = cckd->cdevhdr[sfx].free_number;
        for (i = cckd->freeavail; i < cckd->freenbr; i++)
            cckd->free[i].next = i + 1;
        cckd->free[i-1].next = -1;
    }
    else cckd->freeavail = -1;

    return 0;

} /* end function cckd_read_fsp */

/*-------------------------------------------------------------------*/
/* Write the free space                                              */
/*-------------------------------------------------------------------*/
void cckd_write_fsp (DEVBLK *dev)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             rc;                     /* Return code               */
off_t           fpos;                   /* Free space offset         */
int             sfx;                    /* File index                */
int             i;                      /* Index                     */

    cckd = dev->cckd_ext;
    sfx = cckd->sfn;

    if (!cckd->free) return;
    DEVTRACE ("cckddasd: writing free space, number %d\n",
              cckd->cdevhdr[sfx].free_number);

    fpos = (off_t)cckd->cdevhdr[sfx].free;
    for (i = cckd->free1st; i >= 0; i = cckd->free[i].next)
    {
        rc = lseek (cckd->fd[sfx], fpos, SEEK_SET);
        rc = write (cckd->fd[sfx], &cckd->free[i], CCKD_FREEBLK_SIZE);
        fpos = (off_t)cckd->free[i].pos;
    }

    if (cckd->free) free (cckd->free);
    cckd->free = NULL;
    cckd->freenbr = 0;
    cckd->free1st = cckd->freeavail = -1;

} /* end function cckd_write_fsp */


/*-------------------------------------------------------------------*/
/* Read a new level 2 table                                          */
/*-------------------------------------------------------------------*/
int cckd_read_l2 (DEVBLK *dev, int sfx, int l1x)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             rc=0;                   /* Return code               */
int             i;                      /* Index variable            */
int             fnd=-1;                 /* Cache index for hit       */
int             lru=-1;                 /* Least-Recently-Used cache
                                            index                    */

    cckd = dev->cckd_ext;

    /* return if table is already active */
    if (sfx == cckd->sfx && l1x == cckd->l1x) return 0;

    /* write the old table if it has been updated */
    if (cckd->l2updated)
        cckd_write_l2 (dev);

    cckd->sfx = sfx;
    cckd->l1x = l1x;

    /* obtain the level 2 cache if it doesn't exist */
    if (!cckd->l2cache)
        cckd->l2cache = calloc (cckd->l2cachenbr, CCKD_CACHE_SIZE);

    /* scan the cache array for the l2tab */
    for (i = 0; i < cckd->l2cachenbr; i++)
    {
        if (sfx == cckd->l2cache[i].sfx
         && l1x == cckd->l2cache[i].l1x
         && cckd->l2cache[i].buf)
        {   fnd = i;
            break;
        }
        /* find the oldest entry */
        if  (lru == - 1 || cckd->l2cache[i].age < cckd->l2cache[lru].age)
            lru = i;
    }

    /* check for level 2 cache hit */
    if (fnd >= 0)
    {
        DEVTRACE ("cckddasd: l2[%d,%d] cache[%d] hit\n", sfx, l1x, fnd);
        cckd->l2 = (CCKD_L2ENT *)cckd->l2cache[fnd].buf;
        cckd->l2cache[i].age = ++cckd->cacheage; 
        return 0;
    }
    DEVTRACE ("cckddasd: l2[%d,%d] cache[%d] miss\n", sfx, l1x, lru);

    /* get buffer for level 2 table if there isn't one */
    if (!cckd->l2cache[lru].buf)
        cckd->l2cache[lru].buf = malloc (CCKD_L2TAB_SIZE);

    cckd->l2cache[lru].sfx = sfx;
    cckd->l2cache[lru].l1x = l1x;
    cckd->l2cache[lru].age = ++cckd->cacheage;
    cckd->l2 = (CCKD_L2ENT *)cckd->l2cache[lru].buf;

    /* check for null table */
    if (!cckd->l1[sfx][l1x] || cckd->l1[sfx][l1x] == 0xffffffff)
    {
        if (cckd->l1[sfx][l1x])
            memset (cckd->l2, 0xff, CCKD_L2TAB_SIZE);
        else memset (cckd->l2, 0, CCKD_L2TAB_SIZE);
        DEVTRACE("cckddasd: l2[%d,%d] cache[%d] null\n", sfx, l1x, lru);
        rc = 0;
    }
    /* read the new level 2 table */
    else
    {
        rc = lseek (cckd->fd[sfx], (off_t)cckd->l1[sfx][cckd->l1x], SEEK_SET);
        rc = read (cckd->fd[sfx], cckd->l2, CCKD_L2TAB_SIZE);
        if (cckd->swapend[sfx]) cckd_swapend_l2 (cckd->l2);
        DEVTRACE("cckddasd: l2[%d,%d] cache[%d] read pos 0x%x\n",
                 sfx, l1x, lru, cckd->l1[sfx][l1x]);
        cckd->l2reads[sfx]++;
        cckd->totl2reads++;
    }

    return rc;

} /* end function cckd_read_l2 */


/*-------------------------------------------------------------------*/
/* Purge all l2tab cache entries for a given index                   */
/*-------------------------------------------------------------------*/
void cckd_purge_l2 (DEVBLK *dev, int sfx)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             i;                      /* Index variable            */

    cckd = dev->cckd_ext;

    /* Return if the level 2 cache doesn't exist */
    if (!cckd->l2cache) return;

    /* write the old table if it has been updated */
    if (cckd->l2updated)
        cckd_write_l2 (dev);

    /* Invalidate the active entry if it is for this index */
    if (sfx == cckd->sfx)
        cckd->sfx = cckd->l1x = -1;

    /* scan the cache array for l2tabs matching this index */
    for (i = 0; i < cckd->l2cachenbr; i++)
    {
        if (sfx == cckd->l2cache[i].sfx)
        {
            /* Invalidate the cache entry */
            cckd->l2cache[i].sfx = cckd->l2cache[i].l1x = -1;
            cckd->l2cache[i].age = 0;
        }
    }

} /* end function cckd_purge_l2 */


/*-------------------------------------------------------------------*/
/* Write the current level 2 table                                   */
/*-------------------------------------------------------------------*/
void cckd_write_l2 (DEVBLK *dev)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             rc;                     /* Return code               */
int             sfx,l1x;                /* Lookup table indices      */

    cckd = dev->cckd_ext;
    sfx = cckd->sfn;
    l1x = cckd->l1x;

    if (cckd->l2updated)
    {
        cckd->l2updated = 0;

        if (!cckd->l1[sfx][l1x] || cckd->l1[sfx][l1x] == 0xffffffff)
        { /* new level 2 table */
            cckd->l1[sfx][l1x] = cckd_get_space (dev, CCKD_L2TAB_SIZE);
            cckd_write_l1ent (dev, l1x);
            DEVTRACE ("cckddasd: new l2[%d,%d] pos 0x%x\n",
                      sfx, l1x, cckd->l1[sfx][l1x]);
        }

        /* write the level 2 table */
        rc = lseek (cckd->fd[sfx], (off_t)cckd->l1[sfx][l1x], SEEK_SET);
        rc = write (cckd->fd[sfx], cckd->l2, CCKD_L2TAB_SIZE);
        DEVTRACE("cckddasd: l2[%d,%d] written pos 0x%x\n",
                 sfx, l1x, cckd->l1[sfx][l1x]);
    }

} /* end function cckd_write_l2 */


/*-------------------------------------------------------------------*/
/* Return a level 2 entry                                            */
/*-------------------------------------------------------------------*/
int cckd_read_l2ent (DEVBLK *dev, CCKD_L2ENT *l2, int trk)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             rc;                     /* Return code               */
int             sfx,l1x,l2x;            /* Lookup table indices      */

    cckd = dev->cckd_ext;

    l1x = trk >> 8;
    l2x = trk & 0xff;

    for (sfx = cckd->sfn; sfx >= 0; sfx--)
    {
        DEVTRACE("cckddasd: rdl2ent trk %d l2[%d,%d] pos 0x%x\n",
                 trk, sfx, l1x, cckd->l1[sfx][l1x]);
        if (cckd->l1[sfx][l1x] == 0xffffffff) continue;
        rc = cckd_read_l2 (dev, sfx, l1x);
        if (cckd->l2[l2x].pos != 0xffffffff) break;
    }

    if (sfx >= 0)
        memcpy (l2, &cckd->l2[l2x], CCKD_L2ENT_SIZE);
    else
    { /* base file must be a regular ckd file */
        sfx = 0;
        memset (l2, 0xff, CCKD_L2ENT_SIZE);
    }

    DEVTRACE ("cckddasd: l2[%d,%d,%d] entry read trk %d pos 0x%x len %d\n",
              sfx, l1x, l2x, trk, l2->pos, l2->len);

    return sfx;

} /* end function cckd_read_l2ent */


/*-------------------------------------------------------------------*/
/* Update a level 2 entry                                            */
/*-------------------------------------------------------------------*/
void cckd_write_l2ent (DEVBLK *dev, CCKD_L2ENT *l2, int trk)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             rc;                     /* Return code               */
int             sfx,l1x,l2x;            /* Lookup table indices      */

    cckd = dev->cckd_ext;

    sfx = cckd->sfn;
    l1x = trk >> 8;
    l2x = trk & 0xff;

    memcpy (&cckd->l2[l2x], l2, CCKD_L2ENT_SIZE);
    if (cckd->l1[sfx][l1x] && cckd->l1[sfx][l1x] != 0xffffffff)
    {
        rc = lseek (cckd->fd[sfx],
                    (off_t)(cckd->l1[sfx][l1x] + l2x * CCKD_L2ENT_SIZE),
                    SEEK_SET);
        rc = write (cckd->fd[sfx], &cckd->l2[l2x], CCKD_L2ENT_SIZE);
        DEVTRACE ("cckddasd: l2[%d,%d,%d] updated pos 0x%lx\n",
                  sfx, l1x, l2x,
                  (unsigned long) (cckd->l1[sfx][l1x] +
                  l2x * CCKD_L2ENT_SIZE));
    }
    else
    {
        cckd->l2updated = 1;
        cckd_write_l2 (dev);
    }

    return;
} /* end function cckd_write_l2ent */


/*-------------------------------------------------------------------*/
/* Read a track image                                                */
/*-------------------------------------------------------------------*/
int cckd_read_trkimg (DEVBLK *dev, BYTE *buf, int trk, BYTE *unitstat)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             rc;                     /* Return code               */
int             sfx,l1x,l2x;            /* Lookup table indices      */
CCKD_L2ENT      l2;                     /* Level 2 entry             */

    cckd = dev->cckd_ext;

    sfx = cckd_read_l2ent (dev, &l2, trk);
    l1x = trk >> 8;
    l2x = trk & 0xff;

    if (l2.pos && l2.pos != 0xffffffff)
    {
        rc = lseek (cckd->fd[sfx], (off_t)l2.pos, SEEK_SET);
        rc = read (cckd->fd[sfx], buf, l2.len);
        cckd->reads[sfx]++;
        cckd->totreads++;
    }
    else rc = cckd_null_trk (dev, buf, trk, l2.len);

    /* if sfx is zero and l2.pos is all 0xff's, then we are reading
       from a regular ckd file (ie not a compressed one) */
    if (!sfx && l2.pos == 0xffffffff)
    {
        if (trk < dev->ckdtrks)
        {
            rc = lseek (cckd->fd[sfx],
                        (off_t)(trk * dev->ckdtrksz + CKDDASD_DEVHDR_SIZE),
                        SEEK_SET);
            rc = read (cckd->fd[sfx], buf, dev->ckdtrksz);
            rc = cckd_trklen (dev, buf);
            cckd->reads[sfx]++;
            cckd->totreads++;
        }
        else
        {
            devmsg ("%4.4x: cckddasd: trkimg %d beyond end of "
                    "regular ckd file: trks %d\n",
                    dev->devnum, trk, dev->ckdtrks);
            rc = cckd_null_trk (dev, buf, trk, 0);
        }
    }

    DEVTRACE ("cckddasd: trkimg %d read sfx %d pos 0x%x len %d "
              "%2.2x%2.2x%2.2x%2.2x%2.2x\n",
              trk, sfx, l2.pos, rc,
              buf[0], buf[1], buf[2], buf[3], buf[4]);

    if (cckd_cchh (dev, buf, trk) < 0)
    {
        rc = cckd_null_trk (dev, buf, trk, 0);
        if (*unitstat)
        {
            ckd_build_sense (dev, SENSE_EC, 0, 0,
                             FORMAT_1, MESSAGE_0);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
        }
    }

    return rc;
} /* end function cckd_read_trkimg */


/*-------------------------------------------------------------------*/
/* Write a track image                                               */
/*-------------------------------------------------------------------*/
int cckd_write_trkimg (DEVBLK *dev, BYTE *buf, int len, int trk)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             rc;                     /* Return code               */
CCKD_L2ENT      l2, oldl2;              /* Level 2 entries           */
int             sfx,l1x,l2x;            /* Lookup table indices      */

    cckd = dev->cckd_ext;

    sfx = cckd->sfn;
    l1x = trk >> 8;
    l2x = trk & 0xff;

    /* take care of the special instance where we are writing
       to a regular ckd file (ie not a compressed one) */
    if (!sfx && cckd->l1[sfx][l1x] == 0xffffffff)
    {
        if (trk < dev->ckdtrks)
        {   rc = lseek (cckd->fd[sfx],
                        (off_t)(trk * dev->ckdtrksz + CKDDASD_DEVHDR_SIZE),
                        SEEK_SET);
            rc = write (cckd->fd[sfx], buf,
                        (len == 0) ? CCKD_NULLTRK_SIZE1 :
                        (len == 0xffff) ? CCKD_NULLTRK_SIZE0 : len);
            cckd->writes[sfx]++;
            cckd->totwrites++;
            DEVTRACE ("cckddasd: trkimg %d write regular len %d "
              "%2.2x%2.2x%2.2x%2.2x%2.2x\n",
              trk, len, buf[0], buf[1], buf[2], buf[3], buf[4]);
        }
        else
        {   devmsg ("%4.4x: cckddasd: wrtrk %d beyond end of file "
                    "for regular ckd file: trks %d\n",
                    dev->devnum, trk, dev->ckdtrks);
            rc = 0;
        }
    }
    /* write to a compressed file */
    else
    {
        /* get the level 2 entry for the track in the active file */
        rc = cckd_read_l2 (dev, sfx, l1x);
        memcpy (&oldl2, &cckd->l2[l2x], CCKD_L2ENT_SIZE);

        /* get offset and length for the track image */
        l2.pos = cckd_get_space (dev, len);
        l2.len = l2.size = len;

        /* write the track image */
        if (l2.pos)
        {   rc = lseek (cckd->fd[sfx], (off_t)l2.pos, SEEK_SET);
            rc = write (cckd->fd[sfx], buf, len);
            cckd->writes[sfx]++;
            cckd->totwrites++;
        } else rc = 0;

        /* update the level 2 entry */
        cckd_write_l2ent (dev, &l2, trk);

        /* release the old space if we got a new space */
        cckd_rel_space (dev, oldl2.pos, oldl2.len);

        DEVTRACE ("cckddasd: trkimg %d write sfx %d pos 0x%x len %d "
                  "%2.2x%2.2x%2.2x%2.2x%2.2x\n",
                  trk, sfx, l2.pos, l2.len,
                  buf[0], buf[1], buf[2], buf[3], buf[4]);
    }

    cckd_cchh (dev, buf, trk);

    return rc;

} /* end function cckd_write_trkimg */


/*-------------------------------------------------------------------*/
/* Harden the file                                                   */
/*-------------------------------------------------------------------*/
void cckd_harden(DEVBLK *dev)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */

    cckd = dev->cckd_ext;

    if ((cckd->cdevhdr[cckd->sfn].options & CCKD_OPENED) != 0)
    {
        /* write the lookup tables in case they were updated */
        cckd_write_l2 (dev);
        cckd_write_l1 (dev);

        /* write the free space chain */
        cckd_write_fsp (dev);

        /* write the compressed device header */
        cckd->cdevhdr[cckd->sfn].options &= ~CCKD_OPENED;
        cckd_write_chdr (dev);
    }
}


/*-------------------------------------------------------------------*/
/* Return length of an uncompressed track image                      */
/*-------------------------------------------------------------------*/
int cckd_trklen (DEVBLK *dev, BYTE *buf)
{
int             size;                   /* Track size                */

    for (size = CKDDASD_TRKHDR_SIZE;
         memcmp (&buf[size], &eighthexFF, 8) != 0; )
    {
        if (size > dev->ckdtrksz) break;

        /* add length of count, key, and data fields */
        size += CKDDASD_RECHDR_SIZE +
                buf[size+5] +
                (buf[size+6] << 8) + buf[size+7];
    }

    /* add length for end-of-track indicator */
    size += CKDDASD_RECHDR_SIZE;

    /* check for missing end-of-track indicator */
    if (size > dev->ckdtrksz ||
        memcmp (&buf[size-CKDDASD_RECHDR_SIZE], &eighthexFF, 8) != 0)
    {
        devmsg ("%4.4X:cckddasd trklen err for %2.2x%2.2x%2.2x%2.2x%2.2x\n",
                dev->devnum, buf[0], buf[1], buf[2], buf[3], buf[4]);
        size = dev->ckdtrksz;
    }

    return size;
}


/*-------------------------------------------------------------------*/
/* Build a null track                                                */
/*-------------------------------------------------------------------*/
int cckd_null_trk(DEVBLK *dev, BYTE *buf, int trk, int len)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
U16             cyl;                    /* Cylinder                  */
U16             head;                   /* Head                      */
FWORD           cchh;                   /* Cyl, head big-endian      */
int             size;                   /* Size of null record       */

    cckd = dev->cckd_ext;

    DEVTRACE("cckddasd: null_trk trk %d\n", trk);

    /* cylinder and head calculations */
    cyl = trk / dev->ckdheads;
    head = trk % dev->ckdheads;
    cchh[0] = cyl >> 8;
    cchh[1] = cyl & 0xFF;
    cchh[2] = head >> 8;
    cchh[3] = head & 0xFF;

    /* a null track has a 5 byte track hdr, 8 byte r0 count,
       8 byte r0 data, 8 byte r1 count and 8 ff's */
    memset(buf, 0, 37);
    memcpy (&buf[1], cchh, sizeof(cchh));
    memcpy (&buf[5], cchh, sizeof(cchh));
    buf[12] = 8;
    if (len == 0)
    {
        memcpy (&buf[21], cchh, sizeof(cchh));
        buf[25] = 1;
        memcpy (&buf[29], eighthexFF, 8);
        size = CCKD_NULLTRK_SIZE1;
    }
    else
    {
        memcpy (&buf[21], eighthexFF, 8);
        size = CCKD_NULLTRK_SIZE0;
    }
    return size;

} /* end function cckd_null_trk */


/*-------------------------------------------------------------------*/
/* Verify a track header and return track number                     */
/*-------------------------------------------------------------------*/
int cckd_cchh (DEVBLK *dev, BYTE *buf, int trk)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
U16             cyl;                    /* Cylinder                  */
U16             head;                   /* Head                      */
int             t;                      /* Calculated track          */

    cckd = dev->cckd_ext;

    cyl = (buf[1] << 8) + buf[2];
    head = (buf[3] << 8) + buf[4];
    t = cyl * dev->ckdheads + head;

    if (buf[0] <= CCKD_COMPRESS_MAX && cyl < dev->ckdcyls &&
        head < dev->ckdheads && (trk == -1 || t == trk))
            return t;

    devmsg ("%4.4X:cckddasd: invalid trk hdr trk %d "
            "buf %p %2.2x%2.2x%2.2x%2.2x%2.2x\n",
            dev->devnum, trk,
            buf, buf[0], buf[1], buf[2], buf[3], buf[4]);

    cckd_print_itrace (dev);

    return -1;
} /* end function cckd_cchh_rcd */


/*-------------------------------------------------------------------*/
/* Create a shadow file name                                         */
/*-------------------------------------------------------------------*/
int cckd_sf_name (DEVBLK *dev, int sfx, char *sfn)
{
BYTE           *sfxptr;                 /* -> Last char of file name */

    /* return base file name if index is 0 */
    if (!sfx)
    {
        strcpy (sfn, (const char *)&dev->filename);
        return 0;
    }

    /* Error if no shadow file name specified */
    if (dev->ckdsfn[0] == '\0')
    {
        devmsg ("cckddasd: no shadow file name specified%s\n", "");
        return -1;
    }

    /* Error if number shadow files exceeded */
    if (sfx > CCKD_MAX_SF)
    {
        devmsg ("cckddasd: [%d] number of shadow files exceeded: %d\n",
                sfx, CCKD_MAX_SF);
        return -1;
    }

    /* copy the shadow file name */
    strcpy (sfn, (const char *)&dev->ckdsfn);
    if (sfx == 1) return 0;

    /* Locate and change the last character of the file name */
    sfxptr = strrchr (sfn, '/');
    if (sfxptr == NULL) sfxptr = sfn + 1;
    sfxptr = strchr (sfxptr, '.');
    if (sfxptr == NULL) sfxptr = sfn + strlen(sfn);
    sfxptr--;
    if (sfx > 0)
        *sfxptr = '0' + sfx;
    else *sfxptr = '*';

    return 0;

} /* end function cckd_sf_name */


/*-------------------------------------------------------------------*/
/* Initialize shadow files                                           */
/*-------------------------------------------------------------------*/
int cckd_sf_init (DEVBLK *dev)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             rc;                     /* Return code               */
int             i;                      /* Index                     */
char            sfn[256];               /* Shadow file name          */

    cckd = dev->cckd_ext;

    /* return if no shadow files */
    if (dev->ckdsfn[0] == '\0') return 0;

    /* open all existing shadow files */
    for (cckd->sfn = 1; cckd->sfn <= CCKD_MAX_SF; cckd->sfn++)
    {
        /* get the shadow file name */
        rc = cckd_sf_name (dev, cckd->sfn, (char *)&sfn);
        if (rc < 0) return -1;

        /* try to open the shadow file read-write then read-only */
        cckd->fd[cckd->sfn] = open (sfn, O_RDWR|O_BINARY);
        if (cckd->fd[cckd->sfn] < 0)
        {
            cckd->fd[cckd->sfn] = open (sfn, O_RDONLY|O_BINARY);
            if (cckd->fd[cckd->sfn] < 0) break;
            cckd->open[cckd->sfn] = CCKD_OPEN_RO;
        }
        else cckd->open[cckd->sfn] = CCKD_OPEN_RW;

        /* read the compressed device header */
        rc = cckd_read_chdr (dev);
        if (rc < 0) return -1;

        /* call the chkdsk function */
        rc = cckd_chkdsk (cckd->fd[cckd->sfn], dev->msgpipew, 0);
        if (rc < 0) return -1;

        /* re-read the compressed header */
        rc = cckd_read_chdr (dev);
        if (rc < 0) return -1;

        /* read the 1 table */
        rc = cckd_read_l1 (dev);
        if (rc < 0) return -1;
    }

    /* Backup to the last opened file number */
    cckd->sfn--;

    /* If the last file was opened read-only then create a new one   */
    if (cckd->open[cckd->sfn] == CCKD_OPEN_RO)
    {
        rc = cckd_sf_new (dev);
        if (rc < 0) return -1;
    }

    /* re-open previous rdwr files rdonly */
    for (i = 0; i < cckd->sfn; i++)
    {
        if (cckd->open[i] == CCKD_OPEN_RO) continue;

        /* close the file */
        close (cckd->fd[i]);

        /* get the file name */
        rc = cckd_sf_name (dev, i, (char *)&sfn);
        if (rc < 0) return -1;

        /* open the file read-only */
        cckd->fd[i] = open (sfn, O_RDONLY|O_BINARY);
        if (cckd->fd[i] < 0)
        {
            devmsg ("cckddasd: error re-opening %s readonly\n   %s\n",
                    sfn, strerror(errno));
            return -1;
        }
        if (!i) dev->fd = cckd->fd[i];
        cckd->open[i] = CCKD_OPEN_RD;
    }

    return 0;

} /* end function cckd_sf_init */


/*-------------------------------------------------------------------*/
/* Create a new shadow file                                          */
/*-------------------------------------------------------------------*/
int cckd_sf_new (DEVBLK *dev)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
CKDDASD_DEVHDR  devhdr;                 /* Device header             */
CCKD_L2TAB      l2;                     /* Level 2 table             */
int             rc;                     /* Return code               */
BYTE            sfn[256];               /* Shadow file name          */
int             sfx;                    /* Shadow file index         */
int             sfd;                    /* Shadow file descriptor    */
int             i;                      /* Loop index                */
int             cyls;                   /* Number cylinders for devt */
int             l1size;                 /* Size of level 1 table     */

    cckd = dev->cckd_ext;

    /* get new shadow file index */
    sfx = cckd->sfn + 1;

    /* get new shadow file name */
    rc = cckd_sf_name (dev, sfx, (char *)&sfn);
    if (rc < 0) return -1;

    /* Open the new shadow file */
    sfd = open (sfn, O_RDWR|O_CREAT|O_EXCL|O_BINARY,
                     S_IRUSR | S_IWUSR | S_IRGRP);
    if (sfd < 0)
    {    devmsg ("cckddasd: shadow file open error: %s\n",
                 strerror (errno));
         return -1;
    }

    /* build the device header */
    rc = lseek (cckd->fd[sfx-1], 0, SEEK_SET);
    rc = read (cckd->fd[sfx-1], &devhdr, CKDDASD_DEVHDR_SIZE);
    memcpy (&devhdr.devid, "CKD_S370", 8);
    rc = write (sfd, &devhdr, CKDDASD_DEVHDR_SIZE);

    /* build the compressed device header */
    memset (&cckd->cdevhdr[sfx], 0, CCKDDASD_DEVHDR_SIZE);
    cckd->cdevhdr[sfx].vrm[0] = CCKD_VERSION;
    cckd->cdevhdr[sfx].vrm[1] = CCKD_RELEASE;
    cckd->cdevhdr[sfx].vrm[2] = CCKD_MODLVL;
    cckd->cdevhdr[sfx].options |= CCKD_NOFUDGE;
    if (cckd_endian()) cckd->cdevhdr[sfx].options |= CCKD_BIGENDIAN;

    /* Need to figure out the number of cylinders for the device;
       the problem we have here is that the base device may be a
       regular ckd file and not contain all the cylinders that
       the device allows while a compressed ckd file (including
       shadow files) must contain all the cylinders.           */
    cyls = dev->ckdtab->cyls;
    if (cyls < dev->ckdcyls) /* must have alternate cylinders */
        cyls = dev->ckdcyls;

    cckd->cdevhdr[sfx].numl1tab = (cyls * dev->ckdheads + 255) >> 8;
    l1size = cckd->cdevhdr[sfx].numl1tab * CCKD_L1ENT_SIZE;
    cckd->cdevhdr[sfx].numl2tab = 256;
    cckd->cdevhdr[sfx].size = cckd->cdevhdr[sfx].used =
               CKDDASD_DEVHDR_SIZE + CCKDDASD_DEVHDR_SIZE + l1size;
    if (cckd->cdevhdr[sfx].numl1tab != cckd->cdevhdr[sfx-1].numl1tab)
    { /* previous file is base regular ckd file */
        cckd->cdevhdr[sfx].size += CCKD_L2TAB_SIZE;
        cckd->cdevhdr[sfx].used += CCKD_L2TAB_SIZE;
    }
    cckd->cdevhdr[sfx].cyls[3] = (cyls >> 24) & 0xff;
    cckd->cdevhdr[sfx].cyls[2] = (cyls >> 16) & 0xff;
    cckd->cdevhdr[sfx].cyls[1] = (cyls >>  8) & 0xff;
    cckd->cdevhdr[sfx].cyls[0] = (cyls      ) & 0xff;
    if (cckd->cdevhdr[sfx-1].compress)
        cckd->cdevhdr[sfx].compress = cckd->cdevhdr[sfx-1].compress;
    else cckd->cdevhdr[sfx].compress = CCKD_COMPRESS_MAX;
    if (cckd->cdevhdr[sfx-1].compress_parm)
        cckd->cdevhdr[sfx].compress_parm =
                                   cckd->cdevhdr[sfx-1].compress_parm;
    else cckd->cdevhdr[sfx].compress_parm = -1;
    rc = write (sfd, &cckd->cdevhdr[sfx], CCKDDASD_DEVHDR_SIZE);

    /* Build the level 1 table */
    cckd->l1[sfx] = malloc (l1size);
    memset (cckd->l1[sfx], 0xff, l1size);
    if (cckd->cdevhdr[sfx].numl1tab != cckd->cdevhdr[sfx-1].numl1tab)
    { /* previous file is base regular ckd file */
        i = (dev->ckdtrks >> 8);
        cckd->l1[sfx][i] = CKDDASD_DEVHDR_SIZE + CCKDDASD_DEVHDR_SIZE
                         + l1size;
        for (i = i + 1; i < cckd->cdevhdr[sfx].numl1tab; i++)
            cckd->l1[sfx][i] = 0;
    }
    rc = write (sfd, cckd->l1[sfx], l1size);

    /* Build the level 2 table, if necessary */
    if (cckd->cdevhdr[sfx].numl1tab != cckd->cdevhdr[sfx-1].numl1tab)
    {
        memset (&l2, 0xff, CCKD_L2TAB_SIZE);
        for (i = dev->ckdtrks % 256; i < 256; i++)
            l2[i].pos = l2[i].len = l2[i].size = 0;
        rc = write (sfd, &l2, CCKD_L2TAB_SIZE);
    }

    /* everything is successful */
    cckd->sfn = sfx;
    cckd->fd[sfx] = sfd;
    cckd->open[sfx] = CCKD_OPEN_RW;
    if (cyls != dev->ckdcyls)
    { /* base regular file has increased in size */
        dev->ckdcyls = cyls;
        dev->ckdtrks = dev->ckdcyls * dev->ckdheads;
        dev->devchar[12] = (dev->ckdcyls >> 8) & 0xff;
        dev->devchar[13] = dev->ckdcyls & 0xff;
    }

    return 0;

} /* end function cckd_sf_new */


/*-------------------------------------------------------------------*/
/* Add a shadow file  (sf+)                                          */
/*-------------------------------------------------------------------*/
void cckd_sf_add (DEVBLK *dev)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             rc;                     /* Return code               */
BYTE            sfn[256];               /* Shadow file name          */

    cckd = dev->cckd_ext;
    if (!cckd)
    {
        devmsg ("%4.4X: cckddasd: device is not a shadow file\n",
                dev->devnum);
        return;
    }

    /* schedule updated track entries to be written */
    obtain_lock (&cckd->cachelock);
    cckd_flush_cache (dev);
    release_lock (&cckd->cachelock);

    /* wait a bit */
    while (cckd->writepending) usleep (5000);

    /* obtain control of the file */
    obtain_lock (&cckd->filelock);

    /* harden the current file */
    cckd_harden (dev);

    /* create a new shadow file */
    rc = cckd_sf_new (dev);
    if (rc < 0)
    {
        devmsg ("cckddasd: error adding shadow file: %s\n",
                strerror(errno));
        release_lock (&cckd->filelock);
        return;
    }

    /* re-open the previous file if opened read-write */
    if (cckd->open[cckd->sfn-1] == CCKD_OPEN_RW)
    {
        close (cckd->fd[cckd->sfn-1]);
        rc = cckd_sf_name (dev, cckd->sfn-1, (char *)&sfn);
        cckd->fd[cckd->sfn-1] = open (sfn, O_RDONLY|O_BINARY);
        cckd->open[cckd->sfn-1] = CCKD_OPEN_RD;
        if (!(cckd->sfn-1)) dev->fd = cckd->fd[cckd->sfn-1];
    }

    rc = cckd_sf_name (dev, cckd->sfn, (char *)&sfn);
    devmsg ("cckddasd: shadow file [%d] %s added\n",
            cckd->sfn, sfn);
    release_lock (&cckd->filelock);

    cckd_sf_stats (dev);

} /* end function cckd_sf_add */


/*-------------------------------------------------------------------*/
/* Remove a shadow file  (sf-)                                       */
/*-------------------------------------------------------------------*/
void cckd_sf_remove (DEVBLK *dev, int merge)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             rc;                     /* Return code               */
int             i,j,sfx;                /* Indices                   */
BYTE            sfn[256];               /* Shadow file name          */
BYTE           *buf, *buf2;             /* Buffers                   */
CCKD_L2TAB      l2;                     /* Level 2 table             */
struct stat     st;                     /* File status information   */
int             cyls, trks;             /* Size of regular file      */
int             lasttrk;                /* Last trk in regular file  */
long            len;                    /* Uncompressed trk length   */
int             add = 0;                /* Add the shadow file back  */

    cckd = dev->cckd_ext;
    if (!cckd)
    {
        devmsg ("%4.4X: cckddasd: device is not a shadow file\n",
                dev->devnum);
        return;
    }

    if (!cckd->sfn)
    {
        devmsg ("cckddasd: cannot remove base file\n");
        return;
    }

    obtain_lock (&cckd->filelock);
    sfx = cckd->sfn;

    /* attempt to re-open the previous file read-write */
    close (cckd->fd[sfx-1]);
    cckd_sf_name (dev, sfx-1, (char *)&sfn);
    cckd->fd[sfx-1] = open (sfn, O_RDWR|O_BINARY);
    if (cckd->fd[sfx-1] < 0)
    {
        cckd->fd[sfx-1] = open (sfn, O_RDONLY|O_BINARY);
        if (merge)
        {
            devmsg ("cckddasd: cannot remove shadow file [%d], "
                    "file [%d] cannot be opened read-write\n",
                    sfx, sfx-1);
            release_lock (&cckd->filelock);
            return;
        }
        else add = 1;
    }
    else
    {
        cckd->open[sfx-1] = CCKD_OPEN_RW;
        DEVTRACE ("cckddasd: sfrem [%d] %s opened r/w\n", sfx-1, sfn);
    }

    /* harden the current file */
    cckd_harden (dev);

    /* make the previous file active */
    cckd->sfn--;

    /* Verify the integrity of the new active file by calling the
       chkdsk function.  This is especially important if the
       preceding file was created by cckddump program and made
       writable for the merge */
    rc = cckd_chkdsk (cckd->fd[sfx-1], dev->msgpipew, 0);
    if (rc < 0)
    {
        devmsg ("cckddasd: cannot remove shadow file [%d], "
                "file [%d] failed chkdsk\n",
                sfx, sfx-1);
        cckd->sfn++;
        release_lock (&cckd->filelock);
        return;         
    }

    /* Re-read the compressed device header, in case chkdsk
       rebuilt the free space */
    rc = cckd_read_chdr (dev);
    if (rc < 0)
    {
        devmsg ("cckddasd: cannot remove shadow file [%d], "
                "file [%d] read cckd devhdr failed\n",
                sfx, sfx-1);
        cckd->sfn++;
        release_lock (&cckd->filelock);
        return;         
    }

    /* perform backwards merge to a compressed file */
    if (merge && cckd->cdevhdr[sfx-1].size)
    {
        DEVTRACE ("cckddasd: sfrem merging to compressed file [%d] %s\n",
                  sfx-1, sfn);
        cckd->cdevhdr[cckd->sfn].options |= (CCKD_OPENED | CCKD_ORDWR);
        buf = malloc (dev->ckdtrksz);
        for (i = 0; i < cckd->cdevhdr[sfx].numl1tab; i++)
        {
            /* get the level 2 tables */
            rc = cckd_read_l2 (dev, sfx, i);
            memcpy (&l2, cckd->l2, CCKD_L2TAB_SIZE);
            rc = cckd_read_l2 (dev, sfx-1, i);

            /* loop for each level 2 table entry */
            for (j = 0; j < 256; j++)
            {
                /* continue if `pass-thru' */
                if (l2[j].pos == 0xffffffff) continue;

                /* continue if both l2 entries are zero */
                if (!l2[j].pos && !cckd->l2[j].pos) continue;

                /* current entry is 0 and previous is `pass-thru' */
                if (!l2[j].pos && cckd->l2[j].pos == 0xffffffff)
                {
                    cckd->l2[j].pos = cckd->l2[j].len = cckd->l2[j].size = 0;
                    cckd->l2updated = 1;
                    continue;
                }

                /* remove track image for a zero l2 entry */
                if (!l2[j].pos)
                {
                    DEVTRACE ("cckddasd: sfrem erasing trk %d\n", i * 256 + j);
                    cckd_rel_space (dev, cckd->l2[j].pos, cckd->l2[j].len);
                    cckd->l2[j].pos = cckd->l2[j].len = cckd->l2[j].size = 0;
                    cckd->l2updated = 1;
                    continue;
                }

                /* read the track image and write it to the new file */
                rc = lseek (cckd->fd[sfx], (off_t)l2[j].pos, SEEK_SET);
                rc = read (cckd->fd[sfx], buf, l2[j].len);
                DEVTRACE ("cckddasd: sfrem trk %d read sfx %d pos 0x%x len %d "
                          "%2.2x%2.2x%2.2x%2.2x%2.2x\n",
                          i * 256 + j, sfx, l2[j].pos, l2[j].len,
                          buf[0], buf[1], buf[2], buf[3], buf[4]);
                rc = cckd_write_trkimg (dev, buf, l2[j].len, i * 256 + j);

            } /* for each level 2 entry */

            /* check for empty l2 */
            if (!memcmp (&cckd->l2, &cckd_empty_l2tab, CCKD_L2TAB_SIZE))
            {
                cckd->l2updated = 0;
                cckd->l1[sfx-1][i] = 0;
                cckd->l1updated = 1;
            }
        } /* for each level 1 entry */
        free (buf);
        cckd_write_l2 (dev);
        cckd_write_l1 (dev);
        cckd_write_chdr (dev);
        cckd_harden (dev);
        cckd_chkdsk (cckd->fd[sfx-1], dev->msgpipew, 1);
        cckd_read_chdr (dev);
        cckd_harden (dev);
        cckd->cdevhdr[cckd->sfn].options |= CCKD_OPENED;
        cckd_write_chdr (dev);
    } /* merge to compressed file */

    /* merge to a regular ckd file */
    if (merge && !cckd->cdevhdr[sfx-1].size)
    {
        /* find the last used track */
        lasttrk = -1;
        for (i = cckd->cdevhdr[sfx].numl1tab - 1; i >= 0 && lasttrk < 0; i--)
            if (cckd->l1[sfx][i] && cckd->l1[sfx][i] != 0xffffffff)
            {
                rc = cckd_read_l2 (dev, sfx, i);
                for (j = 255; j >= 0 && lasttrk < 0; j--)
                    if (cckd->l2[j].pos && cckd->l2[j].pos != 0xffffffff)
                        lasttrk = i * 256 + j;
            }

        /* calculate number regular ckd file tracks */
        rc = fstat (cckd->fd[sfx-1], &st);
        trks = (st.st_size - CKDDASD_DEVHDR_SIZE) / dev->ckdtrksz;

        /* can't merge if we exceeded the regular file size */
        if (lasttrk >= trks)
        { /* backout and punt */
             devmsg ("cckddasd: cannot merge, last used track %d exceeds "
                     " ckd file size %d tracks\n", lasttrk, trks);
             cckd->sfn++;
             close (cckd->fd[sfx-1]);
             cckd->fd[sfx-1] = open (sfn, O_RDWR|O_BINARY);
             release_lock (&cckd->filelock);
             return;
        }

        buf = malloc (dev->ckdtrksz); buf2 = malloc (dev->ckdtrksz);
        for (i = 0; i < cckd->cdevhdr[sfx].numl1tab; i++)
        {
            if (i * 256 >= trks) break;
            rc = cckd_read_l2 (dev, sfx, i);
            for (j = 0; j < 256; j++)
            {
                if (i * 256 + j >= trks) break;
                if (cckd->l2[j].pos == 0xffffffff) continue;
                if (!cckd->l2[j].pos) cckd_null_trk (dev, buf, i * 256 + j, cckd->l2[j].len);
                else
                {   rc = lseek (cckd->fd[sfx], (off_t)cckd->l2[j].pos, SEEK_SET);
                    rc = read (cckd->fd[sfx], buf, cckd->l2[j].len);
                }

                /* uncompress the track image */
                switch (buf[0]) {

                case CCKD_COMPRESS_NONE:
                    len = cckd_trklen (dev, buf);
                    memcpy (buf2, buf, len);
                    break;

                case CCKD_COMPRESS_ZLIB:
                    memcpy (buf2, buf, CKDDASD_TRKHDR_SIZE);
                    len = dev->ckdtrksz - CKDDASD_TRKHDR_SIZE;
                    rc = uncompress(&buf2[CKDDASD_TRKHDR_SIZE],
                        &len, &buf[CKDDASD_TRKHDR_SIZE],
                        (long)cckd->l2[j].len - CKDDASD_TRKHDR_SIZE);
                    len += CKDDASD_TRKHDR_SIZE;
                    if (rc != Z_OK)
                        len = cckd_null_trk (dev, buf2, i * 256 + j, 0);
                    break;

#ifdef CCKD_BZIP2
                case CCKD_COMPRESS_BZIP2:
                    memcpy (buf2, buf, CKDDASD_TRKHDR_SIZE);
                    len = dev->ckdtrksz - CKDDASD_TRKHDR_SIZE;
                    rc = BZ2_bzBuffToBuffDecompress (
                        &buf2[CKDDASD_TRKHDR_SIZE],
                        (unsigned int *)&len,
                        &buf[CKDDASD_TRKHDR_SIZE],
                        cckd->l2[j].len - CKDDASD_TRKHDR_SIZE, 0, 0);
                    len += CKDDASD_TRKHDR_SIZE;
                    if (rc != BZ_OK)
                        len = cckd_null_trk (dev, buf2, i * 256 + j, 0);
                    break;
#endif
                default:
                    len = cckd_null_trk (dev, buf2, i * 256 + j, 0);
                    break;
                }

                buf2[0] = 0;
                rc = cckd_write_trkimg (dev, buf2, len, i * 256 + j);

            } /* for each level 2 entry */
        } /* for each level 1 entry */
        free (buf); free (buf2);

        /* readjust number of cylinders */
        cyls = trks / dev->ckdheads;
        if (cyls != dev->ckdcyls)
        { /* base regular file has decreased in size */
            dev->ckdcyls = cyls;
            dev->ckdtrks = dev->ckdcyls * dev->ckdheads;
            dev->devchar[12] = (dev->ckdcyls >> 8) & 0xff;
            dev->devchar[13] = dev->ckdcyls & 0xff;
        }

    } /* merge into regular ckd file */

    /* remove the old file */
    cckd_purge_l2 (dev, sfx);
    close (cckd->fd[sfx]);
    free (cckd->l1[sfx]);
    cckd->l1[sfx] = NULL;
    memset (&cckd->cdevhdr[sfx], 0, CCKDDASD_DEVHDR_SIZE); 
    cckd_sf_name (dev, sfx, (char *)&sfn);
    rc = unlink ((char *)&sfn);

    /* Add the file back if necessary */
    if (add) rc = cckd_sf_new (dev) ;

    devmsg ("cckddasd: shadow file [%d] successfully %s\n",
            sfx, merge ? "merged" : add ? "re-added" : "removed");

    release_lock (&cckd->filelock);

    cckd_sf_stats (dev);
} /* end function cckd_sf_remove */


/*-------------------------------------------------------------------*/
/* Set shadow file name   (sf=)                                      */
/*-------------------------------------------------------------------*/
void cckd_sf_newname (DEVBLK *dev, BYTE *sfn)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */

    cckd = dev->cckd_ext;
    if (!cckd)
    {
        devmsg ("%4.4X: cckddasd: device is not a shadow file\n",
                dev->devnum);
        return;
    }
    if (CCKD_MAX_SF == 0)
    {
        devmsg ("%4.4X: cckddasd: file shadowing not activated\n",
                dev->devnum);
        return;
    }

    obtain_lock (&cckd->filelock);

    if (cckd->sfn)
    {
        devmsg ("%4.4X: cckddasd: shadowing is already active\n",
                dev->devnum);
        release_lock (&cckd->filelock);
        return;
    }

    strcpy ((char *)&dev->ckdsfn, (const char *)sfn);
    devmsg ("cckddasd: shadow file name set to %s\n", sfn);
    release_lock (&cckd->filelock);

} /* end function cckd_sf_newname */


/*-------------------------------------------------------------------*/
/* Check and compress a shadow file  (sfc)                           */
/*-------------------------------------------------------------------*/
void cckd_sf_comp (DEVBLK *dev)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             rc;                     /* Return code               */

    cckd = dev->cckd_ext;
    if (!cckd)
    {
        devmsg ("%4.4X: cckddasd: device is not a shadow file\n",
                dev->devnum);
        return;
    }

    /* schedule updated track entries to be written */
    obtain_lock (&cckd->cachelock);
    cckd_flush_cache (dev);
    release_lock (&cckd->cachelock);

    /* wait a bit */
    while (cckd->writepending) usleep (5000);

    /* obtain control of the file */
    obtain_lock (&cckd->filelock);

    /* harden the current file */
    cckd_harden (dev);

    /* call the chkdsk function */
    rc = cckd_chkdsk (cckd->fd[cckd->sfn], dev->msgpipew, 0);

    /* Call the compress function */
    if (rc >= 0)
        rc = cckd_comp (cckd->fd[cckd->sfn], dev->msgpipew);

    /* Read the header */
    rc = cckd_read_chdr (dev);

    /* Read the Level 1 table */
    rc = cckd_read_l1 (dev);

    /* Purge the Level 2 tables */
    cckd_purge_l2 (dev, cckd->sfn);

    release_lock (&cckd->filelock);

    /* Display the shadow file statistics */
    cckd_sf_stats (dev);

    return;
} /* end function cckd_sf_comp */


/*-------------------------------------------------------------------*/
/* Display shadow file statistics   (sfd)                            */
/*-------------------------------------------------------------------*/
void cckd_sf_stats (DEVBLK *dev)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
struct stat     st;                     /* File information          */
int             i;                      /* Index                     */
int             rc;                     /* Return code               */
BYTE           *ost[] = {"  ", "ro", "rd", "rw"};
unsigned long long size=0,free=0;       /* Total size, free space    */
int             freenbr=0;              /* Total number free spaces  */
BYTE            sfn[256];               /* Shadow file name          */


    cckd = dev->cckd_ext;
    if (!cckd)
    {
        devmsg ("%4.4X: cckddasd: device is not a shadow file\n",
                dev->devnum);
        return;
    }

    obtain_lock (&cckd->filelock);

    /* Calculate totals */
    rc = fstat (cckd->fd[0], &st);
    for (i = 0; i <= cckd->sfn; i++)
    {
        if (!i) size = st.st_size;
        else size += cckd->cdevhdr[i].size;
        free += cckd->cdevhdr[i].free_total;
        freenbr += cckd->cdevhdr[i].free_number;
    }

    /* header */
    devmsg ("cckddasd:           size free  nbr st   reads  writes l2reads    hits switches\n");
    if (cckd->readaheads || cckd->misses)
    devmsg ("cckddasd:                                                  readaheads   misses\n");
    devmsg ("cckddasd: --------------------------------------------------------------------\n");

    /* total statistics */
    devmsg ("cckddasd: [*] %10lld %3lld%% %4d    %7d %7d %7d %7d  %7d\n",
            size, (free * 100) / size, freenbr,
            cckd->totreads, cckd->totwrites, cckd->totl2reads,
            cckd->cachehits, cckd->switches);
    if (cckd->readaheads || cckd->misses)
    devmsg ("cckddasd:                                                     %7d  %7d\n",
            cckd->readaheads, cckd->misses);

    /* base file statistics */
    devmsg ("cckddasd: %s\n", dev->filename);
    devmsg ("cckddasd: [0] %10lld %3lld%% %4d %s %7d %7d %7d\n",
            (long long)st.st_size,
            (long long)((long long)((long long)cckd->cdevhdr[0].free_total * 100) / st.st_size),
            cckd->cdevhdr[0].free_number, ost[cckd->open[0]],
            cckd->reads[0], cckd->writes[0], cckd->l2reads[0]);

    if (dev->ckdsfn[0] && CCKD_MAX_SF > 0)
    {
        cckd_sf_name ( dev, -1, (char *)&sfn);
        devmsg ("cckddasd: %s\n", sfn);
    }

    /* shadow file statistics */
    for (i = 1; i <= cckd->sfn; i++)
    {
        devmsg ("cckddasd: [%d] %10lld %3lld%% %4d %s %7d %7d %7d\n",
                i, (long long)cckd->cdevhdr[i].size,
                (long long)((long long)((long long)cckd->cdevhdr[i].free_total * 100) / cckd->cdevhdr[i].size),
                cckd->cdevhdr[i].free_number, ost[cckd->open[i]],
                cckd->reads[i], cckd->writes[i], cckd->l2reads[i]);
    }
    release_lock (&cckd->filelock);
} /* end function cckd_sf_stats */


/*-------------------------------------------------------------------*/
/* Garbage Collection thread                                         */
/*-------------------------------------------------------------------*/
void cckd_gcol(DEVBLK *dev)
{
int             gcol;                   /* Identifier                */
int             rc;                     /* Return code               */
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
long long       size, free;             /* File size, free           */
int             wait;                   /* Seconds to wait           */
struct timeval  now;                    /* Time-of-day               */
struct timespec tm;                     /* Time-of-day to wait       */
int             gc, oldgc = -1;         /* Garbage states            */
struct          CCKD_GCOL {             /* Garbage collection parms  */
int             hi;                     /* 1 = start after last      */
int             interval;               /* Collection interval (sec) */
int             size;                   /* K bytes per iteration     */
    }           gctab[5]= {             /* default gcol parameters   */
               {FALSE, 5,4096},         /* critical  50%   - 100%    */
               {FALSE, 5,2048},         /* severe    25%   -  50%    */
               {FALSE, 5,1024},         /* moderate  12.5% -  25%    */
               {FALSE, 5, 512},         /* light      6.3% -  12.5%  */
               {FALSE, 5, 256}};        /* none       0%   -   6.3%  */
char *gcstates[] = {"critical","severe","moderate","light","none"}; 

    cckd = dev->cckd_ext;

    obtain_lock (&cckd->gclock);
    gcol = ++cckd->gcols;
    
    /* Return without messages if too many already started */
    if (gcol > cckd->gcolsmax)
    {
        --cckd->gcols;
        release_lock (&cckd->gclock);
        return;
    }

    devmsg ("%4.4x:cckddasd: %d garbage collector starting: pid %d tid "TIDPAT"\n",
            dev->devnum, gcol, getpid(), thread_id());

    while (gcol <= cckd->gcolsmax)
    {
        release_lock (&cckd->gclock);

        /* schedule any updated tracks to be written */
        obtain_lock (&cckd->cachelock);
        cckd_flush_cache (dev);
        release_lock (&cckd->cachelock);

        /* determine garbage state */
        size = (long long)cckd->cdevhdr[cckd->sfn].size;
        free = (long long)cckd->cdevhdr[cckd->sfn].free_total;
        if      (free >= (size = size/2)) gc = 0;
        else if (free >= (size = size/2)) gc = 1;
        else if (free >= (size = size/2)) gc = 2;
        else if (free >= (size = size/2)) gc = 3;
        else gc = 4;

        /* Adjust the state based on the number of free spaces */
        if (cckd->cdevhdr[cckd->sfn].free_number >  800 && gc > 0) gc--;
        if (cckd->cdevhdr[cckd->sfn].free_number > 1800 && gc > 0) gc--;

        /* Issue garbage state message */
        if (gc != oldgc)
            DEVTRACE ("%4.4X:cckddasd: %d gcol state is %s\n", dev->devnum, gcol, gcstates[gc]);
        oldgc = gc;

        /* call the garbage collector */
        rc = cckd_gc_percolate(dev, gctab[gc].size, gctab[gc].hi);

        /* wait a bit */
        wait = gctab[gc].interval;
        if (wait < 1) wait = 5;

        DEVTRACE ("cckddasd: %d gcol waiting %d seconds\n", gcol, wait);

        obtain_lock (&cckd->gclock);
        gettimeofday (&now, NULL);
        tm.tv_sec = now.tv_sec + wait;
        tm.tv_nsec = now.tv_usec * 1000;
        timed_wait_condition (&cckd->gccond, &cckd->gclock, &tm);
    }

    devmsg ("%4.4x:cckddasd: %d garbage collector stopping: pid %d tid "TIDPAT"\n",
            dev->devnum, gcol, getpid(), thread_id());

    cckd->gcols--;
    if (!cckd->gcols) signal_condition (&cckd->termcond);
    release_lock (&cckd->gclock);
} /* end thread cckd_gcol */


/*-------------------------------------------------------------------*/
/* Garbage Collection -- Percolate algorithm                         */
/*                                                                   */
/* A kinder gentler algorithm                                        */
/*                                                                   */
/*-------------------------------------------------------------------*/
int cckd_gc_percolate(DEVBLK *dev, int size, int hi)
{
int             rc;                     /* Return code               */
int             i;                      /* Index                     */
int             fd;                     /* Current file descriptor   */
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             sfx,l1x,l2x;            /* Table Indices             */
int             trk;                    /* Track number              */
off_t           pos = 0, fpos, oldpos;  /* File offsets              */
int             len = 0, flen;          /* Space lengths             */
int             moved = 0;              /* Space moved               */
CCKD_L2ENT      l2;                     /* Copied level 2 entry      */
int             b, blen;                /* Buffer index, length      */
BYTE            buf[65536];             /* Buffer                    */

    cckd = dev->cckd_ext;
    sfx = cckd->sfn;
    size = size << 10;

    DEVTRACE("cckddasd: gcperc size %d 1st 0x%x nbr %d\n",
             size, cckd->cdevhdr[sfx].free, cckd->cdevhdr[sfx].free_number);

    while (moved < size)
    {
        /* get the file lock */
        obtain_lock (&cckd->filelock);
        sfx = cckd->sfn;
        fd = cckd->fd[sfx];

        /* exit if no more free space */
        if (cckd->cdevhdr[sfx].free == 0 || cckd->cdevhdr[sfx].free_number == 0)
        {
            release_lock (&cckd->filelock);
            return 0;
        }

        /* make sure the free space chain is built */
        if (!cckd->free) cckd_read_fsp (dev);

        /* Move either the first or last free space */
        fpos = (off_t)cckd->cdevhdr[sfx].free;
        for (i = cckd->free1st; hi && cckd->free[i].next >= 0; i = cckd->free[i].next)
            fpos = (off_t)cckd->free[i].pos;
        flen = cckd->free[i].len;
        DEVTRACE ("cckddasd: gcperc free pos 0x%llx len %d\n",
                  (long long)fpos, flen);

        /* Read to the next free space or to end-of-file but don't exceed 64K */
        if (cckd->free[i].pos)
            blen = cckd->free[i].pos - (fpos + flen);
        else blen = cckd->cdevhdr[sfx].size - (fpos + flen);
        if (blen > 65536) blen = 65536;

        /* Read l2tab and trk spaces */
        DEVTRACE ("cckddasd: gcperc buf read file[%d] offset 0x%llx len %d\n",
                  sfx, (long long)(fpos + flen), blen);
        oldpos = lseek (fd, (off_t)(fpos + flen), SEEK_SET);
        if (oldpos < 0)
        {
            devmsg ("%4.4X cckddasd: gcperc lseek error file[%d] offset 0x%llx: %s\n",
                    dev->devnum, sfx, (long long)(fpos + flen), strerror(errno));
            release_lock (&cckd->filelock);
            return moved;
        }
        rc = read (fd, &buf, blen);
        if (rc < blen)
        {
            devmsg ("%4.4X cckddasd: gcperc read error file[%d] offset 0x%llx: %s\n",
                    dev->devnum, sfx, (long long)(fpos + flen), strerror(errno));
            release_lock (&cckd->filelock);
            return moved;
        }

        /* Process each space in the buffer */
        for (b = 0; b + CKDDASD_TRKHDR_SIZE <= blen; b += len, moved += len)
        {
            /* Check for level 2 table */
            for (i = 0; i < cckd->cdevhdr[sfx].numl1tab; i++)
                if (cckd->l1[sfx][i] == (U32)(fpos + flen + b)) break;

            if (i < cckd->cdevhdr[sfx].numl1tab)
            {
                /* Moving a level 2 table */
                len = CCKD_L2TAB_SIZE;
                if (b + len > blen) break;
                DEVTRACE ("cckddasd: gcperc move l2tab[%d] at pos 0x%llx len %d\n",
                          i, (unsigned long long)(fpos + flen + b), len);

                /* Read the level 2 table again because it might have been updated */
                oldpos = lseek (fd, (off_t)(fpos + flen + b), SEEK_SET);
                rc = read (fd, &buf[b], len);

                /* Relocate the level 2 table somewhere else */
                pos = cckd_get_space (dev, len);
                oldpos = lseek (fd, pos, SEEK_SET);
                rc = write (fd, &buf[b], len);

                /* Update the level 1 table entry */
                cckd->l1[sfx][i] = (U32)pos;
                oldpos = lseek (fd, CCKD_L1TAB_POS + (i * CCKD_L1ENT_SIZE), SEEK_SET);
                rc = write (fd, &cckd->l1[sfx][i], CCKD_L1ENT_SIZE);
            }
            else
            {
                /* Moving a track image */
                trk = cckd_cchh (dev, &buf[b], -1);
                if (trk < 0)
                {
                    release_lock (&cckd->filelock);
                    return -1;
                }
                l1x = trk >> 8;
                l2x = trk & 0xff;

                /* Read the lookup entry for the track */
                rc = cckd_read_l2ent (dev, &l2, trk);
                if (l2.pos != (U32)(fpos + flen + b))
                {
                    devmsg ("cckddasd: %4.4x unknown space at offset 0x%llx\n",
                            dev->devnum, (long long)(fpos + flen + b));
                    devmsg ("cckddasd: %4.4x %2.2x%2.2x%2.2x%2.2x%2.2x\n",
                            dev->devnum, buf[b+0], buf[b+1],buf[b+2], buf[b+3], buf[b+4]);
                    cckd_print_itrace (dev);
                    release_lock (&cckd->filelock);
                    return -1;
                }
                len = l2.len;
                if (b + len > blen) break;
                DEVTRACE ("cckddasd: gcperc move trk %d at pos 0x%llx len %d\n",
                          trk, (long long)(fpos + flen + b), len);

                /* Relocate the track image somewhere else */
                pos = cckd_get_space (dev, len);
                oldpos = lseek (fd, pos, SEEK_SET);
                rc = write (fd, &buf[b], len);

                /* Update the level 2 table entry */
                l2.pos = (U32)pos;
                cckd_write_l2ent (dev, &l2, trk);
            }

            /* Release the space occupied by the l2tab or track image */
            cckd_rel_space (dev, (unsigned int)(fpos + flen + b), len);

        } /* for each space in the buffer */

        release_lock (&cckd->filelock);
    } /* while (moved < size) */

    sfx = cckd->sfn;
    DEVTRACE("cckddasd: gcperc moved %d 1st 0x%x nbr %d\n",
             moved, cckd->cdevhdr[sfx].free, cckd->cdevhdr[sfx].free_number);

    return moved;

} /* end function cckd_gc_percolate */


/*-------------------------------------------------------------------*/
/* Print internal trace                                              */
/*-------------------------------------------------------------------*/
void cckd_print_itrace(DEVBLK *dev)
{
#ifdef CCKD_ITRACEMAX
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             start, i;               /* Start index, index        */

    if (cckd->itracex < 0) return;
    devmsg ("%4.4X:cckddasd: print_itrace\n", dev->devnum);
    cckd = dev->cckd_ext;
    i = start = cckd->itracex;
    cckd->itracex = -1;
    do
    {
        if (i >= 128*CCKD_ITRACEMAX) i = 0;
        if (cckd->itrace[i] != '\0')
            devmsg ("%s", &cckd->itrace[i]);
        i+=128;
    } while (i != start);
    fflush (dev->msgpipew);
    sleep (2);
#endif
}


DEVHND cckddasd_device_hndinfo = {
        &ckddasd_init_handler,
        &ckddasd_execute_ccw,
        &cckddasd_close_device,
        &ckddasd_query_device,
        &cckddasd_start,
        &cckddasd_end,
        &cckddasd_start,
        &cckddasd_end
};

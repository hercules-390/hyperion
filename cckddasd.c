/* CCKDDASD.C   (c) Copyright Roger Bowler, 1999-2003                */
/*       ESA/390 Compressed CKD Direct Access Storage Device Handler */

/*-------------------------------------------------------------------*/
/* This module contains device functions for compressed emulated     */
/* count-key-data direct access storage devices.                     */
/*-------------------------------------------------------------------*/

#include "hercules.h"
#include "devtype.h"

#define cckdtrc(format, a...) \
do { \
 if (dev && (dev->ccwtrace||dev->ccwstep)) \
  fprintf((dev)->msgpipew, "%4.4X:" format, dev->devnum, a); \
 if (cckdblk.itracen > 0) \
 {int n; \
  if (cckdblk.itracex >= 128 * cckdblk.itracen) \
  { \
   n = 0; \
   cckdblk.itracex = 128; \
  } \
  else \
  { \
    n = cckdblk.itracex; \
    cckdblk.itracex += 128; \
  } \
  sprintf(&cckdblk.itrace[n], "%4.4X:" format, dev ? dev->devnum : 0, a); \
 } \
} while (0)


/*-------------------------------------------------------------------*/
/* Internal functions                                                */
/*-------------------------------------------------------------------*/
int     cckddasd_init (int, BYTE **);
int     cckddasd_term ();
int     cckddasd_init_handler (DEVBLK *, int, BYTE **);
int     cckddasd_close_device (DEVBLK *);
void    cckddasd_start (DEVBLK *);
void    cckddasd_end (DEVBLK *);
int     cckd_read_track (DEVBLK *, int, int, BYTE *);
int     cckd_update_track (DEVBLK *, BYTE *, int, BYTE *);
int     cckd_used (DEVBLK *);
int     cfba_read_block (DEVBLK *, BYTE *, int, BYTE *);
int     cfba_write_block (DEVBLK *, BYTE *, int, BYTE *);
int     cfba_used (DEVBLK *);
CCKD_CACHE *cckd_read_trk (DEVBLK *, int, int, BYTE *);
void    cckd_readahead (DEVBLK *, int);
void    cckd_ra ();
void    cckd_flush_cache(DEVBLK *);
void    cckd_purge_cache(DEVBLK *);
void    cckd_writer();
off_t   cckd_get_space (DEVBLK *, unsigned int);
void    cckd_rel_space (DEVBLK *, off_t, int);
void    cckd_rel_free_atend (DEVBLK *, unsigned int, int, int);
void    cckd_flush_space(DEVBLK *);
int     cckd_read_chdr (DEVBLK *);
int     cckd_write_chdr (DEVBLK *);
int     cckd_read_l1 (DEVBLK *);
int     cckd_write_l1 (DEVBLK *);
int     cckd_write_l1ent (DEVBLK *, int);
int     cckd_read_init (DEVBLK *);
int     cckd_read_fsp (DEVBLK *);
int     cckd_write_fsp (DEVBLK *);
int     cckd_read_l2 (DEVBLK *, int, int);
void    cckd_purge_l2 (DEVBLK *);
int     cckd_write_l2 (DEVBLK *);
int     cckd_read_l2ent (DEVBLK *, CCKD_L2ENT *, int);
int     cckd_write_l2ent (DEVBLK *, CCKD_L2ENT *, int);
int     cckd_read_trkimg (DEVBLK *, BYTE *, int, BYTE *);
int     cckd_write_trkimg (DEVBLK *, BYTE *, int, int);
int     cckd_harden (DEVBLK *);
void    cckd_truncate (DEVBLK *, int);
int     cckd_trklen (DEVBLK *, BYTE *);
int     cckd_null_trk (DEVBLK *, BYTE *, int, int);
int     cckd_cchh (DEVBLK *, BYTE *, int);
int     cckd_validate (DEVBLK *, BYTE *, int, int);
int     cckd_sf_name (DEVBLK *, int, char *);
int     cckd_sf_init (DEVBLK *);
int     cckd_sf_new (DEVBLK *);
void    cckd_sf_add (DEVBLK *);
void    cckd_sf_remove (DEVBLK *, int);
void    cckd_sf_newname (DEVBLK *, BYTE *);
void    cckd_sf_stats (DEVBLK *);
void    cckd_sf_comp (DEVBLK *);
void    cckd_gcol ();
int     cckd_gc_percolate (DEVBLK *, unsigned int);
void    cckd_print_itrace();

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
/* Data areas                                                        */
/*-------------------------------------------------------------------*/
static  BYTE cckd_empty_l2tab[CCKD_L2TAB_SIZE];  /* Empty L2 table   */
static  BYTE eighthexFF[] = {0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff};
DEVHND  cckddasd_device_hndinfo;
DEVHND  cfbadasd_device_hndinfo;
CCKDBLK cckdblk;                        /* cckd global area          */

/*-------------------------------------------------------------------*/
/* CCKD global initialization                                        */
/*-------------------------------------------------------------------*/
int cckddasd_init (int argc, BYTE *argv[])
{
int             i;                      /* Index                     */

    UNREFERENCED(argc);
    UNREFERENCED(argv);

    if (memcmp (&cckdblk.id, "CCKDBLK ", sizeof(cckdblk.id)) == 0)
        return 0;

    /* Clear the cckdblk */
    memset(&cckdblk, 0, sizeof(CCKDBLK));

    /* Initialize locks and conditions */
    memcpy (&cckdblk.id, "CCKDBLK ", sizeof(cckdblk.id));
    initialize_lock (&cckdblk.l2cachelock);
    initialize_lock (&cckdblk.cachelock);
    initialize_lock (&cckdblk.gclock);
    initialize_lock (&cckdblk.ralock);
    initialize_condition (&cckdblk.cachecond);
    initialize_condition (&cckdblk.gccond);
    initialize_condition (&cckdblk.termcond);
    initialize_condition (&cckdblk.racond);
    initialize_condition (&cckdblk.writercond);
    initialize_condition (&cckdblk.writecond);

    /* Initialize some variables */
    cckdblk.msgpipew   = stderr;
    cckdblk.writerprio = 16;
    cckdblk.ranbr      = CCKD_DEFAULT_RA_SIZE;
    cckdblk.ramax      = CCKD_DEFAULT_RA;
    cckdblk.writermax  = CCKD_DEFAULT_WRITER;
    cckdblk.gcolmax    = CCKD_DEFAULT_GCOL;
    cckdblk.cachenbr   = CCKD_DEFAULT_CACHE;
    cckdblk.l2cachenbr = CCKD_DEFAULT_L2CACHE;
    cckdblk.gcolwait   = CCKD_DEFAULT_GCOLWAIT;
    cckdblk.gcolparm   = CCKD_DEFAULT_GCOLPARM;
    cckdblk.readaheads = CCKD_DEFAULT_READAHEADS;
    cckdblk.freepend   = CCKD_DEFAULT_FREEPEND;

    /* Initialize the readahead queue */
    cckdblk.ra1st = cckdblk.ralast = -1;
    cckdblk.rafree = 0;
    for (i = 0; i < cckdblk.ranbr; i++) cckdblk.ra[i].next = i + 1;
    cckdblk.ra[cckdblk.ranbr - 1].next = -1;

    return 0;

} /* end function cckddasd_init */


/*-------------------------------------------------------------------*/
/* CCKD dasd global termination                                      */
/*-------------------------------------------------------------------*/
int cckddasd_term ()
{
    /* Terminate the readahead threads */
    obtain_lock (&cckdblk.ralock);
    cckdblk.ramax = 0;
    if (cckdblk.ras)
    {
        broadcast_condition (&cckdblk.racond);
        wait_condition (&cckdblk.termcond, &cckdblk.ralock);
    }
    release_lock (&cckdblk.ralock);

    /* Terminate the garbage collection threads */
    obtain_lock (&cckdblk.gclock);
    cckdblk.gcolmax = 0;
    if (cckdblk.gcols)
    {
        broadcast_condition (&cckdblk.gccond);
        wait_condition (&cckdblk.termcond, &cckdblk.gclock);
    }
    release_lock (&cckdblk.gclock);

    /* write any updated track images and wait for writers to stop */
    obtain_lock (&cckdblk.cachelock);
    cckd_flush_cache (NULL);
    cckdblk.writermax = 0;
    if (cckdblk.writepending || cckdblk.writers)
    {
        broadcast_condition (&cckdblk.writercond);
        wait_condition (&cckdblk.termcond, &cckdblk.cachelock);
    }
    release_lock (&cckdblk.cachelock);

    /* free the cache */
    cckd_purge_cache (NULL);
    cckd_purge_l2 (NULL);

    memset(&cckdblk, 0, sizeof(CCKDBLK));

    return 0;

} /* end function cckddasd_term */


/*-------------------------------------------------------------------*/
/* CKD dasd initialization                                           */
/*-------------------------------------------------------------------*/
int cckddasd_init_handler ( DEVBLK *dev, int argc, BYTE *argv[] )
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
DEVBLK         *dev2;                   /* -> device in cckd queue   */
int             rc;                     /* Return code               */
int             fdflags;                /* File flags                */

    UNREFERENCED(argc);
    UNREFERENCED(argv);

    /* Obtain area for cckd extension */
    dev->cckd_ext = cckd = malloc(sizeof(CCKDDASD_EXT));
    if (cckd == NULL)
    {
        devmsg ("%4.4X:cckddasd: malloc failed for cckd extension: %s\n",
                dev->devnum, strerror(errno));
        return -1;
    }
    memset(cckd, 0, sizeof(CCKDDASD_EXT));
    memset(&cckd_empty_l2tab, 0, CCKD_L2TAB_SIZE);

    /* Initialize the global cckd block if necessary */
    if (memcmp (&cckdblk.id, "CCKDBLK ", sizeof(cckdblk.id)))
        cckddasd_init (0, NULL);
    if (dev->msgpipew != cckdblk.msgpipew)
        cckdblk.msgpipew = dev->msgpipew;

    /* Initialize locks and conditions */
    initialize_lock (&cckd->filelock);
    initialize_condition (&cckd->readcond);
    initialize_condition (&cckd->writecond);

    /* Initialize some variables */
    obtain_lock (&cckd->filelock);
    cckd->l1x = cckd->sfx = -1;
    cckd->fd[0] = dev->fd;
    fdflags = fcntl (dev->fd, F_GETFL);
    cckd->open[0] = (fdflags & O_RDWR) ? CCKD_OPEN_RW : CCKD_OPEN_RO;

    /* call the chkdsk function */
    rc = cckd_chkdsk (cckd->fd[0], dev->msgpipew, 0);
    if (rc < 0) return -1;

    /* Perform initial read */
    rc = cckd_read_init (dev);
    if (rc < 0) return -1;

    /* open the shadow files */
    rc = cckd_sf_init (dev);
    if (rc < 0)
    {   devmsg ("%4.4X:cckddasd: error initializing shadow files\n",
                dev->devnum);
        return -1;
    }

    /* Update the routines */
    if (cckd->ckddasd)
    {
        dev->hnd = &cckddasd_device_hndinfo;
        dev->ckdrdtrk = &cckd_read_track;
        dev->ckdupdtrk = &cckd_update_track;
        dev->ckdused = &cckd_used;
    }
    else
    {
        dev->hnd = &cfbadasd_device_hndinfo;
        dev->fbardblk = &cfba_read_block;
        dev->fbawrblk = &cfba_write_block;
        dev->fbaused = &cfba_used;
    }
    release_lock (&cckd->filelock);

    /* Insert the device into the cckd device queue */
    obtain_lock (&cckdblk.gclock);
    for (cckd = NULL, dev2 = cckdblk.dev1st; dev2; dev2 = cckd->devnext)
        cckd = dev2->cckd_ext;
    if (cckd) cckd->devnext = dev;
    else cckdblk.dev1st = dev;
    release_lock (&cckdblk.gclock);

    cckdblk.batch = dev->batch;
    if (cckdblk.batch)
    {
        cckdblk.nostress = 1;
        cckdblk.freepend = 0;
    }

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

    /* Remove the device from the cckd queue */
    obtain_lock (&cckdblk.gclock);
    if (dev == cckdblk.dev1st) cckdblk.dev1st = cckd->devnext;
    else
    {
        DEVBLK *dev2 = cckdblk.dev1st;
        CCKDDASD_EXT *cckd2 = dev2->cckd_ext;
        while (cckd2->devnext != dev)
        {
           dev2 = cckd2->devnext; cckd2 = dev2->cckd_ext;
        }
        cckd2->devnext = cckd->devnext;
    }
    release_lock (&cckdblk.gclock);

    /* Flush the cache and wait for the writes to complete */
    obtain_lock (&cckdblk.cachelock);
    cckd_flush_cache (dev);
    while (cckdblk.writepending || cckd->ioactive)
    {
        cckdblk.writewaiting++;
        wait_condition (&cckdblk.writecond, &cckdblk.cachelock);
        cckdblk.writewaiting--;
        cckd_flush_cache (dev);
    }

    /* Purge the device entries from the cache */
    cckd_purge_cache (dev); cckd_purge_l2 (dev);
    cckd->stopping = 1;
    release_lock (&cckdblk.cachelock);

    /* harden the file */
    obtain_lock (&cckd->filelock);
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

    /* write some statistics */
    if (!dev->batch)
        cckd_sf_stats (dev);
    release_lock (&cckd->filelock);

    /* free the cckd extension */
    dev->cckd_ext= NULL;
    free (cckd);
    memset (&dev->dasdsfn, 0, sizeof(dev->dasdsfn));

    close (dev->fd);

    /* If no more devices then perform global termination */
    if (cckdblk.dev1st == NULL) cckddasd_term ();

    return 0;
} /* end function cckddasd_close_device */


/*-------------------------------------------------------------------*/
/* Compressed ckd start/resume channel program                       */
/*-------------------------------------------------------------------*/
void cckddasd_start (DEVBLK *dev)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */

    cckd = dev->cckd_ext;
    if (!cckd || cckd->stopping)
    {
        dev->dasdcur = -1;
        return;
    }

    obtain_lock (&cckdblk.cachelock);
    cckd->ioactive = 1;

    /* If previous active cache entry has been stolen or is busy
       then set it to null and return.  This forces `cckd_read_trk'
       to be called.                                                  */
    if (!cckd->active || dev != cckd->active->dev
     || dev->dasdcur != cckd->active->trk || cckd->merging
     || (cckd->active->flags & (CCKD_CACHE_READING | CCKD_CACHE_WRITING)))
    {
        dev->dasdcur = -1;
        cckd->active = NULL;
        release_lock (&cckdblk.cachelock);
        return;
    }

    /* Make the previous cache entry active again */
    cckd->active->flags |= CCKD_CACHE_ACTIVE;

    /* If the entry is pending write then change it to `updated' */
    if (cckd->active->flags & CCKD_CACHE_WRITE)
    {
        cckd->active->flags &= ~CCKD_CACHE_WRITE;
        cckd->active->flags |= CCKD_CACHE_UPDATED;
        cckdblk.writepending--;
    }

    release_lock (&cckdblk.cachelock);
} /* end function cckddasd_start */


/*-------------------------------------------------------------------*/
/* Compressed ckd end/suspend channel program                        */
/*-------------------------------------------------------------------*/
void cckddasd_end (DEVBLK *dev)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */

    cckd = dev->cckd_ext;
    obtain_lock (&cckdblk.cachelock);
    if (cckd)
    {
        if (cckd->active)
        {
            cckd->active->flags &= ~CCKD_CACHE_ACTIVE;
            if (!(cckd->active->flags & CCKD_CACHE_BUSY) && cckdblk.cachewaiting)
                signal_condition (&cckdblk.cachecond);
            else if ((cckd->active->flags & CCKD_CACHE_BUSY) == CCKD_CACHE_UPDATED
                  && !cckdblk.writers)
                cckd_flush_cache (dev);
        }
        if (cckd->ioactive)
        {
            cckd->ioactive = 0;
            if (cckdblk.writewaiting)
                broadcast_condition (&cckdblk.writecond);
        } 
    }
    release_lock (&cckdblk.cachelock);
} /* end function cckddasd_end */


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
    if (cyl >= dev->ckdcyls || head >= dev->ckdheads || cckd->stopping)
    {
        ckd_build_sense (dev, SENSE_CR, 0, 0, FORMAT_0, MESSAGE_4);
        if (unitstat) *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return -1;
    }

    /* Reset buffer offsets */
    dev->bufoff = 0;
    dev->bufoffhi = dev->ckdtrksz;

    /* Return if reading the same track image and not trk 0 */
    if (trk == dev->dasdcur && dev->buf && cckd->active) return 0;

    cckdtrc ("cckddasd: read  trk   %d (%s)\n", trk,
              dev->syncio_active ? "synchronous" : "asynchronous");

    /* read the new track */
    *unitstat = 0;
    active = cckd_read_trk (dev, trk, 0, unitstat);
    if (active == NULL && dev->syncio_active && dev->syncio_retry)
        return -1;
    dev->syncio_active = act;
    cckd->active = active;
    dev->dasdcur = trk;
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
        ckd_build_sense (dev, SENSE_EC, SENSE1_WRI, 0,FORMAT_1, MESSAGE_0);
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
    cckd->updated = 1;

    return len;

} /* end function cckd_update_track */


/*-------------------------------------------------------------------*/
/* Return used cylinders                                             */
/*-------------------------------------------------------------------*/
int cckd_used (DEVBLK *dev)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             rc;                     /* Return code               */
int             l1x, l2x;               /* Lookup table indexes      */
int             sfx;                    /* Shadow file suffix        */
CCKD_L2ENT      l2;                     /* Copied level 2 entry      */

    cckd = dev->cckd_ext;
    obtain_lock (&cckd->filelock);

    /* Find the last used level 1 table entry */
    for (l1x = cckd->cdevhdr[0].numl1tab - 1; l1x > 0; l1x--)
    {
        sfx = cckd->sfn;
        while (cckd->l1[sfx][l1x] == 0xffffffff && sfx > 0) sfx--;
        if (cckd->l1[sfx][l1x]) break;
    }

    /* Find the last used level 2 table entry */
    for (l2x = 255; l2x >= 0; l2x--)
    {
        rc = cckd_read_l2ent (dev, &l2, l1x * 256 + l2x);
        if (rc < 0 || l2.pos != 0) break;
    }

    release_lock (&cckd->filelock);
    return (l1x * 256 + l2x + dev->ckdheads) / dev->ckdheads;
}


/*-------------------------------------------------------------------*/
/* Compressed fba read block(s)                                      */
/*-------------------------------------------------------------------*/
int cfba_read_block (DEVBLK *dev, BYTE *buf, int rdlen, BYTE *unitstat)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             ix;                     /* New block group index     */
int             off;                    /* Offset into block group   */
CCKD_CACHE     *active;                 /* New active cache entry    */
int             syncio;                 /* Saved `syncio_active' bit */
int             bufoff;                 /* Offset into caller's buf  */
int             copylen;                /* Length to copy            */
int             len;                    /* Length left to copy       */

    cckd = dev->cckd_ext;
    len = rdlen;

    /* Command reject if seek position is outside volume */
    if ((dev->fbarba + len - 1) / dev->fbablksiz >= dev->fbanumblk
     || cckd->stopping)
    {
        ckd_build_sense (dev, SENSE_CR, 0, 0, FORMAT_0, MESSAGE_4);
        if (unitstat) *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return -1;
    }

    /* Calculate the block index and offset */
    ix = dev->fbarba / CFBA_BLOCK_SIZE;
    off = dev->fbarba % CFBA_BLOCK_SIZE + CKDDASD_TRKHDR_SIZE;

    /* Read the block group if it's not currently active */
    if (!cckd->active || ix != dev->dasdcur)
    {
        *unitstat = 0;
        active = cckd_read_trk (dev, ix, 0, unitstat);
        if (active == NULL)
        {
            if (cckd->active)
                cckd->active->flags &= ~CCKD_CACHE_ACTIVE;
            cckd->active = NULL;
            dev->dasdcur = -1;
            return -1;
        }
        cckd->active = active;
        dev->dasdcur = ix;
        if (*unitstat) return -1;
    }

    /* Mask synchronous i/o.  We don't support interrupting
       the channel command in mid-stream if more than one
       block is being read and spans a block group that may
       have to be read. */
    syncio = dev->syncio_active;
    dev->syncio_active = 0;

    /* Copy the blocks to the caller's buffer */
    for (bufoff = 0; len; len -= copylen, bufoff += copylen)
    {
        /* Calculate the length we can copy */
        if (CFBA_BLOCK_SIZE + CKDDASD_TRKHDR_SIZE - off >= len)
            copylen = len;
        else
            copylen = CFBA_BLOCK_SIZE + CKDDASD_TRKHDR_SIZE - off;

        /* Copy from the cache buffer to the caller's buffer */
        memcpy (&buf[bufoff], &cckd->active->buf[off], copylen);
        off += copylen;

        /* Read the next block group if necessary */
        if (copylen < len)
        {
            ix++;
            *unitstat = 0;
            active = cckd_read_trk (dev, ix, 0, unitstat);
            cckd->active = active;
            dev->dasdcur = ix;
            if (*unitstat) return -1;
            off = CKDDASD_TRKHDR_SIZE;
        }
    }

    /* Restore the synchronous i/o flag */
    dev->syncio_active = syncio;

    /* Update the current fba rba */
    dev->fbarba += rdlen;

    return rdlen;
} /* end function cfba_read_block */


/*-------------------------------------------------------------------*/
/* Compressed fba write block(s)                                     */
/*-------------------------------------------------------------------*/
int cfba_write_block (DEVBLK *dev, BYTE *buf, int wrlen, BYTE *unitstat)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             ix;                     /* New block group index     */
int             off;                    /* Offset into block group   */
CCKD_CACHE     *active;                 /* New active cache entry    */
int             syncio;                 /* Saved `syncio_active' bit */
int             bufoff;                 /* Offset into caller's buf  */
int             copylen;                /* Length to copy            */
int             len;                    /* Length left to copy       */

    cckd = dev->cckd_ext;
    len = wrlen;

    /* Command reject if seek position is outside volume */
    if ((dev->fbarba + len - 1) / dev->fbablksiz >= dev->fbanumblk)
    {
        ckd_build_sense (dev, SENSE_CR, 0, 0, FORMAT_0, MESSAGE_4);
        if (unitstat) *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return -1;
    }

    /* Calculate the block index and offset */
    ix = dev->fbarba / CFBA_BLOCK_SIZE;
    off = dev->fbarba % CFBA_BLOCK_SIZE + CKDDASD_TRKHDR_SIZE;

    /* Read the block group if it's not currently active */
    if (!cckd->active || ix != dev->dasdcur)
    {
        *unitstat = 0;
        active = cckd_read_trk (dev, ix, 0, unitstat);
        if (active == NULL)
        {
            if (cckd->active)
                cckd->active->flags &= ~CCKD_CACHE_ACTIVE;
            cckd->active = NULL;
            dev->dasdcur = -1;
            return -1;
        }
        cckd->active = active;
        dev->dasdcur = ix;
        if (*unitstat) return -1;
    }

    /* Mask synchronous i/o.  We don't support interrupting
       the channel command in mid-stream if more than one
       block is being written and spans a block group that may
       have to be read. */
    syncio = dev->syncio_active;
    dev->syncio_active = 0;

    /* Copy the blocks from the caller's buffer */
    for (bufoff = 0; len; len -= copylen, bufoff += copylen)
    {
        /* Calculate the length we can copy */
        if (CFBA_BLOCK_SIZE + CKDDASD_TRKHDR_SIZE - off >= len)
            copylen = len;
        else
            copylen = CFBA_BLOCK_SIZE + CKDDASD_TRKHDR_SIZE - off;

        /* Copy to the cache buffer from the caller's buffer */
        memcpy (&cckd->active->buf[off], &buf[bufoff], copylen);
        off += copylen;

        /* Update the cache entry flags */
        cckd->active->flags |= CCKD_CACHE_UPDATED | CCKD_CACHE_USED;

        /* Read the next block group if necessary */
        if (copylen < len)
        {
            ix++;
            *unitstat = 0;
            active = cckd_read_trk (dev, ix, 0, unitstat);
            if (*unitstat) return -1;
            cckd->active = active;
            dev->dasdcur = ix;
            off = CKDDASD_TRKHDR_SIZE;
        }
    }

    /* Restore the synchronous i/o flag */
    dev->syncio_active = syncio;

    /* Update the current fba rba */
    dev->fbarba += wrlen;

    cckd->updated = 1;

    return wrlen;
} /* end function cfba_write_block */


/*-------------------------------------------------------------------*/
/* Return used blocks                                                */
/*-------------------------------------------------------------------*/
int cfba_used (DEVBLK *dev)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             rc;                     /* Return code               */
int             l1x, l2x;               /* Lookup table indexes      */
int             sfx;                    /* Shadow file suffix        */
CCKD_L2ENT      l2;                     /* Copied level 2 entry      */

    cckd = dev->cckd_ext;
    obtain_lock (&cckd->filelock);

    /* Find the last used level 1 table entry */
    for (l1x = cckd->cdevhdr[0].numl1tab - 1; l1x > 0; l1x--)
    {
        sfx = cckd->sfn;
        while (cckd->l1[sfx][l1x] == 0xffffffff && sfx > 0) sfx--;
        if (cckd->l1[sfx][l1x]) break;
    }

    /* Find the last used level 2 table entry */
    for (l2x = 255; l2x >= 0; l2x--)
    {
        rc = cckd_read_l2ent (dev, &l2, l1x * 256 + l2x);
        if (rc < 0 || l2.pos != 0) break;
    }

    release_lock (&cckd->filelock);
    return (l1x * 256 + l2x + CFBA_BLOCK_NUM) / CFBA_BLOCK_NUM;
}



/*-------------------------------------------------------------------*/
/* Read a track image                                                */
/*                                                                   */
/* This routine can be called by the i/o thread (`ra' == 0) or       */
/* by readahead threads (0 < `ra' <= cckdblk.ramax).                 */
/*                                                                   */
/*-------------------------------------------------------------------*/
CCKD_CACHE *cckd_read_trk(DEVBLK *dev, int trk, int ra, BYTE *unitstat)
{
int             rc, urc=0, vrc=0;       /* Return code               */
int             i;                      /* Index variable            */
int             fnd;                    /* Cache index for hit       */
int             lru;                    /* Least-Recently-Used cache
                                            index                    */
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
unsigned long   len,len2;               /* Lengths                   */
BYTE           *buf, buf2[65536];       /* Buffers                   */

    cckd = dev->cckd_ext;

    cckdtrc ("cckddasd: %d rdtrk     %d\n", ra, trk);

    obtain_lock (&cckdblk.cachelock);

    /* Check for merge in progress */
    if (cckd->merging)
    {
        /* Return if readahead */
        if (ra > 0)
        {   release_lock (&cckdblk.cachelock);
            return NULL;
        }
        /* If asynchronous i/o then wait while merging.
           Note that synchronous i/o will be redriven
           below since the cache has been purged and a
           cache miss will therefore occur */
        while (dev->syncio_active == 0 && cckd->merging)
        {
            cckdblk.writewaiting++;
            wait_condition (&cckdblk.writecond, &cckdblk.cachelock);
            cckdblk.writewaiting--;
        }
    }

cckd_read_trk_retry:

    /* scan the cache array for the track */
    for (fnd = lru = -1, i = 0; i < cckdblk.cachenbr; i++)
    {
        if (dev == cckdblk.cache[i].dev && trk == cckdblk.cache[i].trk)
        {   fnd = i;
            break;
        }
        /* find the oldest entry that isn't busy */
        if (!(cckdblk.cache[i].flags & CCKD_CACHE_BUSY)
         && (lru == - 1 || cckdblk.cache[i].age < cckdblk.cache[lru].age))
            lru = i;
    }

    /* check for cache hit */
    if (fnd >= 0)
    {
        if (ra) /* readahead doesn't care about a cache hit */
        {   release_lock (&cckdblk.cachelock);
            return &cckdblk.cache[fnd];
        }

        /* If synchronous I/O and we need to wait for a read or
           write to complete then return with syncio_retry bit on */
        if (dev->syncio_active)
        {
            if(cckdblk.cache[fnd].flags & (CCKD_CACHE_READING | CCKD_CACHE_WRITING))
            {
                cckdtrc ("cckddasd: %d rdtrk[%d] %d syncio %s\n",
                         ra, fnd, trk,
                         cckdblk.cache[fnd].flags & CCKD_CACHE_READING ?
                         "reading" : "writing");
                cckdblk.stats_synciomisses++;
                dev->syncio_retry = 1;
                release_lock (&cckdblk.cachelock);
                return NULL;
            }
            else cckdblk.stats_syncios++;
        }    

        /* Inactivate the old entry */
        if (cckd->active && cckd->active != &cckdblk.cache[fnd])
        {
            cckd->active->flags &= ~CCKD_CACHE_ACTIVE;
            if (!(cckd->active->flags & CCKD_CACHE_BUSY) && cckdblk.cachewaiting)
                signal_condition (&cckdblk.cachecond);
        }
        cckd->active = NULL;

        /* Mark the new entry active */
        cckdblk.cache[fnd].flags |= CCKD_CACHE_ACTIVE | CCKD_CACHE_USED;
        cckdblk.cache[fnd].age = ++cckdblk.cacheage;
        cckdblk.stats_switches++; cckd->switches++;
        cckdblk.stats_cachehits++; cckd->cachehits++;
        cckdtrc ("cckddasd: %d rdtrk[%d] %d cache hit\n",ra, fnd, trk);

        /* if read is in progress then wait for it to finish */
        if (cckdblk.cache[fnd].flags & CCKD_CACHE_READING)
        {
            cckdblk.stats_readwaits++;
            cckdtrc ("cckddasd: %d rdtrk[%d] %d waiting for read\n",
                      ra, fnd, trk);
            cckdblk.cache[fnd].flags |= CCKD_CACHE_READWAIT;
            wait_condition (&cckd->readcond, &cckdblk.cachelock);
            cckdblk.cache[fnd].flags &= ~CCKD_CACHE_READWAIT;
            cckdtrc ("cckddasd: %d rdtrk[%d] %d read wait complete\n",
                      ra, fnd, trk);
        }

        /* if write is in progress then wait for it to finish */
        if (cckdblk.cache[fnd].flags & CCKD_CACHE_WRITING)
        {
            cckdblk.stats_writewaits++;
            cckdtrc ("cckddasd: %d rdtrk[%d] %d waiting for write\n",
                      ra, fnd, trk);
            cckdblk.cache[fnd].flags |= CCKD_CACHE_WRITEWAIT;
            wait_condition (&cckd->writecond, &cckdblk.cachelock);
            cckdblk.cache[fnd].flags &= ~CCKD_CACHE_WRITEWAIT;
            cckdtrc ("cckddasd: %d rdtrk[%d] %d write wait complete\n",
                      ra, fnd, trk);
        }

        /* If the entry is pending write then change it to `updated' */
        if (cckdblk.cache[fnd].flags & CCKD_CACHE_WRITE)
        {
            cckdblk.cache[fnd].flags &= ~CCKD_CACHE_WRITE;
            cckdblk.cache[fnd].flags |= CCKD_CACHE_UPDATED;
            cckdblk.writepending--;
        }

        /* Check for readahead */
        if (trk == dev->dasdcur + 1 && dev->dasdcur > 0)
            cckd_readahead (dev, trk);

        release_lock (&cckdblk.cachelock);
        return &cckdblk.cache[fnd];
    } /* cache hit */

    cckdtrc ("cckddasd: %d rdtrk[%d] %d cache miss\n",ra, lru, trk);

    /* If not readahead and synchronous I/O then return with
       the `syio_retry' bit set */
    if (!ra && dev->syncio_active)
    {
        cckdtrc ("cckddasd: %d rdtrk[%d] %d syncio cache miss\n",
                   ra, lru, trk);
        cckdblk.stats_synciomisses++;
        dev->syncio_retry = 1;
        release_lock (&cckdblk.cachelock);
        return NULL;
    }

    /* If no cache entry was stolen, then schedule all updated entries
       to be written, and wait for a cache entry unless readahead */
    if (lru < 0)
    {
        cckdtrc ("cckddasd: %d rdtrk[%d] %d no available cache entry\n",
                   ra, lru, trk);
        cckd_flush_cache (NULL);
        if (ra)
        {
            release_lock (&cckdblk.cachelock);
            return NULL;
        }
        cckdblk.stats_cachewaits++;
        cckdblk.cachewaiting++;
        wait_condition (&cckdblk.cachecond, &cckdblk.cachelock);
        cckdblk.cachewaiting--;
        goto cckd_read_trk_retry;
    }

    /* get buffer if there isn't one */
    if (cckdblk.cache[lru].buf == NULL)
    {
        cckdblk.cache[lru].buf = malloc (65536);
        cckdtrc ("cckddasd: %d rdtrk[%d] %d get buf %p\n",
                  ra, lru, trk, cckdblk.cache[lru].buf);
        if (!cckdblk.cache[lru].buf)
        {
            devmsg ("%4.4X cckddasd: %d rdtrk[%d] %d malloc failed: %s\n",
                    dev->devnum, ra, lru, trk, strerror(errno));
            cckdblk.cache[lru].flags |= CCKD_CACHE_INVALID;
            goto cckd_read_trk_retry;
        }
    }
    else if (cckdblk.cache[lru].dev != NULL)
    {
        cckdtrc ("cckddasd: %d rdtrk[%d] %d dropping %4.4X:%d from cache\n",
           ra, lru, trk, cckdblk.cache[lru].dev->devnum, cckdblk.cache[lru].trk);
        if (!(cckdblk.cache[lru].flags & CCKD_CACHE_USED))
        {
            cckdblk.stats_readaheadmisses++;  cckd->misses++;
        }
    }

    /* Mark the entry active if not readahead */
    if (ra == 0)
    {
        cckdblk.stats_switches++; cckd->switches++;
        cckdblk.stats_cachemisses++;
        if (cckd->active) cckd->active->flags &= ~CCKD_CACHE_ACTIVE;
        cckdblk.cache[lru].flags = CCKD_CACHE_ACTIVE | CCKD_CACHE_USED;
    }
    else cckdblk.cache[lru].flags = 0;

    buf = cckdblk.cache[lru].buf;
    cckdblk.cache[lru].dev = dev;
    cckdblk.cache[lru].trk = trk;
    cckdblk.cache[lru].flags |= CCKD_CACHE_READING;
    cckdblk.cache[lru].age = ++cckdblk.cacheage;

    /* asynchrously schedule readaheads */
    if (!ra && trk == dev->dasdcur + 1 && dev->dasdcur > 0)
        cckd_readahead (dev, trk);

    release_lock (&cckdblk.cachelock);

    /* read the track image */
    obtain_lock (&cckd->filelock);
    len2 = cckd_read_trkimg (dev, (BYTE *)&buf2, trk, unitstat);
    release_lock (&cckd->filelock);
    cckdtrc ("cckddasd: %d rdtrk[%d] %d read len %ld\n",
              ra, lru, trk, len2);

    /* uncompress the track image */
    switch (buf2[0] & CCKD_COMPRESS_MASK) {

    case CCKD_COMPRESS_NONE:
        memcpy (buf, &buf2, len2);
        len = len2;
        urc = 0;
        cckdtrc ("cckddasd: %d rdtrk[%d] %d not compressed\n",
                 ra, lru, trk);
        break;

#ifdef CCKD_COMPRESS_ZLIB
    case CCKD_COMPRESS_ZLIB:
        /* Uncompress the track image using zlib */
        memcpy (buf, &buf2, CKDDASD_TRKHDR_SIZE);
        len = dev->ckdtrksz - CKDDASD_TRKHDR_SIZE;
        rc = uncompress(&buf[CKDDASD_TRKHDR_SIZE],
                        &len, &buf2[CKDDASD_TRKHDR_SIZE],
                        len2 - CKDDASD_TRKHDR_SIZE);
        len += CKDDASD_TRKHDR_SIZE;
        cckdtrc ("cckddasd: %d rdtrk[%d] %d uncompressed len %ld code %d\n",
                 ra, lru, trk, len, rc);
        urc = (rc != Z_OK);
        break;
#endif

#ifdef CCKD_COMPRESS_BZIP2
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
        cckdtrc ("cckddasd: %d rdtrk[%d] %d decompressed len %ld code %d\n",
                 ra, lru, trk, len, rc);
        urc = (rc != BZ_OK);
        break;
#endif

    default:
        urc = 1;
        break;
    } /* switch (compression type) */

    if (urc == 0) vrc = cckd_validate (dev, buf, trk, len);
    cckdtrc ("cckddasd: %d rdtrk[%d] %d urc=%d vrc=%d\n",
             ra, lru, trk, urc, vrc);

    /* If an uncompress or validation error occurred then the
       compression algorithm may be wrong and the image recoverable */
    for (i = 0; i <= CCKD_COMPRESS_MAX && (urc || vrc); i++)
    {
        if (i == (buf2[0] & CCKD_COMPRESS_MASK)) continue;
        switch (i) {
        case CCKD_COMPRESS_NONE:
            memcpy (buf, &buf2, len2);
            len = len2;
            urc = 0;
            break;

#ifdef CCKD_COMPRESS_ZLIB
        case CCKD_COMPRESS_ZLIB:
            memcpy (buf, &buf2, CKDDASD_TRKHDR_SIZE);
            len = dev->ckdtrksz - CKDDASD_TRKHDR_SIZE;
            rc = uncompress(&buf[CKDDASD_TRKHDR_SIZE],
                        &len, &buf2[CKDDASD_TRKHDR_SIZE],
                        len2 - CKDDASD_TRKHDR_SIZE);
            len += CKDDASD_TRKHDR_SIZE;
            urc = (rc != Z_OK);
            break;
#endif

#ifdef CCKD_COMPRESS_BZIP2
        case CCKD_COMPRESS_BZIP2:
            memcpy (buf, &buf2, CKDDASD_TRKHDR_SIZE);
            len = dev->ckdtrksz - CKDDASD_TRKHDR_SIZE;
            rc = BZ2_bzBuffToBuffDecompress (
                        &buf[CKDDASD_TRKHDR_SIZE],
                        (unsigned int *)&len,
                        &buf2[CKDDASD_TRKHDR_SIZE],
                        len2 - CKDDASD_TRKHDR_SIZE, 0, 0);
            len += CKDDASD_TRKHDR_SIZE;
            urc = (rc != BZ_OK);
            break;
#endif
        } /* switch (compression type) */
        if (urc == 0) vrc = cckd_validate (dev, buf, trk, len);
        if (urc == 0 && vrc == 0)
        {
            devmsg ("%4.4X cckddasd: rdtrk %d recovered, comp %d !! "
                    "%2.2x%2.2x%2.2x%2.2x%2.2x\n",
                    dev->devnum, trk, i, buf[0], buf[1], buf[2], buf[3], buf[4]);
//          cckd_print_itrace ();
        }
    }
    buf[0] = 0;

    /* Check for image error */
    if (urc || vrc)
    {
        devmsg ("%4.4X cckddasd: rdtrk %d not valid !! %2.2x%2.2x%2.2x%2.2x%2.2x\n",
                dev->devnum, trk, buf[0], buf[1], buf[2], buf[3], buf[4]);
        cckd_print_itrace ();
        len = cckd_null_trk (dev, buf, trk, 0);
        if (unitstat)
        {
            ckd_build_sense (dev, SENSE_EC, 0, 0, FORMAT_1, MESSAGE_0);
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
        }
    }

    obtain_lock (&cckdblk.cachelock);

    cckdblk.cache[lru].flags &= ~CCKD_CACHE_READING;

    /* wakeup other thread waiting for this read */
    if (cckdblk.cache[lru].flags & CCKD_CACHE_READWAIT)
    {   cckdtrc ("cckddasd: %d rdtrk[%d] %d signalling read complete\n",
                 ra, lru, trk);
        signal_condition (&cckd->readcond);
    }

    if (ra)
    {
        cckdblk.stats_readaheads++; cckd->readaheads++;

        /* wakeup cache entry waiters if readahead */
        if (cckdblk.cachewaiting)
        {
            cckdtrc ("cckddasd: %d rdtrk[%d] %d signalling cache entry available\n",
                     ra, lru, trk);
            signal_condition (&cckdblk.cachecond);
        }
    }

    cckdtrc ("cckddasd: %d rdtrk[%d] %d complete\n",
              ra, lru, trk);

    release_lock (&cckdblk.cachelock);

    return &cckdblk.cache[lru];

} /* end function cckd_read_trk */


/*-------------------------------------------------------------------*/
/* Schedule asynchronous readaheads                                  */
/*-------------------------------------------------------------------*/
void cckd_readahead (DEVBLK *dev, int trk)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             ra[CCKD_MAX_RA_SIZE];   /* Thread create switches    */
int             i, r;                   /* Indexes                   */
TID             tid;                    /* Readahead thread id       */

    cckd = dev->cckd_ext;

    if (cckdblk.ramax < 1 || cckdblk.readaheads < 1
     || cckdblk.cachewaiting)
        return;

    /* Initialize */
    memset (ra, 0, sizeof (ra));

    /* make sure readahead tracks aren't already cached */
    for (i = 0; i < cckdblk.cachenbr; i++)
        if (cckdblk.cache[i].trk > trk
         && cckdblk.cache[i].trk <= trk + cckdblk.readaheads)
            ra[cckdblk.cache[i].trk - (trk+1)] = 1;

    /* Queue the tracks to the readahead queue */
    obtain_lock (&cckdblk.ralock);
    for (i = 0; i < cckdblk.readaheads && cckdblk.rafree >= 0; i++)
    {
        if (ra[i] || trk + 1 + i >= dev->ckdtrks) continue;
        r = cckdblk.rafree;
        cckdblk.rafree = cckdblk.ra[r].next;
        if (cckdblk.ralast < 0)
        {
            cckdblk.ra1st = cckdblk.ralast = r;
            cckdblk.ra[r].prev = cckdblk.ra[r].next = -1;
        }
        else
        {
            cckdblk.ra[cckdblk.ralast].next = r;
            cckdblk.ra[r].prev = cckdblk.ralast;
            cckdblk.ra[r].next = -1;
            cckdblk.ralast = r;
        }
        cckdblk.ra[r].trk = trk + 1 + i;
        cckdblk.ra[r].dev = dev;
    }

    /* Schedule the readahead if any are pending */
    if (cckdblk.ra1st >= 0)
    {
        if (cckdblk.rawaiting)
            signal_condition (&cckdblk.racond);
        else if (cckdblk.ras < cckdblk.ramax)
            create_thread (&tid, NULL, cckd_ra, NULL);
    }
    release_lock (&cckdblk.ralock);
} /* end function cckd_readahead */


/*-------------------------------------------------------------------*/
/* Asynchronous readahead thread                                     */
/*-------------------------------------------------------------------*/
void cckd_ra ()
{
DEVBLK         *dev;                    /* Readahead devblk          */
int             trk;                    /* Readahead track           */
int             ra;                     /* Readahead index           */
int             r;                      /* Readahead queue index     */
TID             tid;                    /* Readahead thread id       */

    obtain_lock (&cckdblk.ralock);
    ra = ++cckdblk.ras;
    
    /* Return without messages if too many already started */
    if (ra > cckdblk.ramax)
    {
        --cckdblk.ras;
        release_lock (&cckdblk.ralock);
        return;
    }

    if (!cckdblk.batch)
    cckdmsg ("cckddasd: readahead thread %d started: tid="TIDPAT", pid=%d\n",
            ra, thread_id(), getpid());

    while (ra <= cckdblk.ramax)
    {
        if (cckdblk.ra1st < 0)
        {
            cckdblk.rawaiting++;
            wait_condition (&cckdblk.racond, &cckdblk.ralock);
            cckdblk.rawaiting--;
        }

        /* Possibly shutting down if no writes pending */
        if (cckdblk.ra1st < 0) continue;

        /* Requeue the 1st entry to the readahead free queue */
        r = cckdblk.ra1st;
        trk = cckdblk.ra[r].trk;
        dev = cckdblk.ra[r].dev;
        cckdblk.ra1st = cckdblk.ra[r].next;
        if (cckdblk.ra[r].next > -1)
            cckdblk.ra[cckdblk.ra[r].next].prev = -1;
        else cckdblk.ralast = -1;
        cckdblk.ra[r].next = cckdblk.rafree;
        cckdblk.rafree = r;

        /* Schedule the other readaheads if any are still pending */
        if (cckdblk.ra1st)
        {
            if (cckdblk.rawaiting)
                signal_condition (&cckdblk.racond);
            else if (cckdblk.ras < cckdblk.ramax)
                create_thread (&tid, NULL, cckd_ra, dev);
        }
        release_lock (&cckdblk.ralock);

        /* Read the readahead track */
        cckd_read_trk (dev, trk, ra, NULL);

        obtain_lock (&cckdblk.ralock);
    }

    if (!cckdblk.batch)
    cckdmsg ("cckddasd: readahead thread %d stopping: tid="TIDPAT", pid=%d\n",
            ra, thread_id(), getpid());
    --cckdblk.ras;
    if (!cckdblk.ras) signal_condition(&cckdblk.termcond);
    release_lock(&cckdblk.ralock);

} /* end thread cckd_ra_thread */


/*-------------------------------------------------------------------*/
/* Flush updated cache entries                                       */
/*-------------------------------------------------------------------*/
void cckd_flush_cache(DEVBLK *dev)
{
int             i;                      /* Index                     */
TID             tid;                    /* Writer thread id          */

    /* Scan cache for updated cache entries */
    for (i = 0; i < cckdblk.cachenbr; i++)
        if ((cckdblk.cache[i].flags & CCKD_CACHE_BUSY) == CCKD_CACHE_UPDATED
         && (dev == NULL || dev == cckdblk.cache[i].dev))
        {
            cckdblk.cache[i].flags &= ~CCKD_CACHE_UPDATED;
            cckdblk.cache[i].flags |= CCKD_CACHE_WRITE;
            cckdblk.writepending++;
        }

    /* Schedule the writer if any writes are pending */
    if (cckdblk.writepending)
    {
        if (cckdblk.writerswaiting)
            signal_condition (&cckdblk.writercond);
        else if (cckdblk.writers < cckdblk.writermax)
            create_thread (&tid, NULL, cckd_writer, NULL);
    }
}


/*-------------------------------------------------------------------*/
/* Purge cache entries for a device                                  */
/*-------------------------------------------------------------------*/
void cckd_purge_cache(DEVBLK *dev)
{
int             i;                      /* Index                     */

    /* Purge cache entries */
    for (i = 0; i < cckdblk.cachenbr; i++)
        if (dev == NULL || dev == cckdblk.cache[i].dev)
        {
            cckdblk.cache[i].dev = NULL;
            cckdblk.cache[i].trk = -1;
            cckdblk.cache[i].age = cckdblk.cache[i].flags = 0;
            if (dev == NULL && cckdblk.cache[i].buf)
            {
                free (cckdblk.cache[i].buf);
                cckdblk.cache[i].buf = NULL;
            }
            if (dev) cckdtrc ("cckddasd: purge cache[%d]\n", i);
        }

    /* Signal any cache waiters */
    if (cckdblk.cachewaiting) signal_condition (&cckdblk.cachecond);
}

/*-------------------------------------------------------------------*/
/* Writer thread                                                     */
/*-------------------------------------------------------------------*/
void cckd_writer()
{
DEVBLK         *dev;                    /* Device block              */
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             writer;                 /* Writer identifier         */
int             i, o;                   /* Indexes                   */
BYTE           *buf;                    /* Buffer                    */
#if defined(CCKD_COMPRESS_ZLIB) || defined(CCKD_COMPRESS_BZIP2)
BYTE            buf2[65536];            /* Compress buffer           */
unsigned long   len2;                   /* Buffer 2 length           */
#endif
BYTE           *bufp;                   /* Buffer to be written      */
unsigned long   len, bufl;              /* Buffer lengths            */
int             trk;                    /* Track number              */
BYTE            compress;               /* Compression algorithm     */
int             parm;                   /* Compression parameter     */
TID             tid;                    /* Writer thead id           */
int             rc;                     /* Return code               */
BYTE            stressed;               /* Trk written under stress  */
BYTE           *comp[] = {"none", "zlib", "bzip2"};

    cckd = dev->cckd_ext;

#ifndef WIN32
    /* Set writer priority just below cpu priority to mimimize the
       compression effect */
    if(cckdblk.writerprio >= 0)
        setpriority (PRIO_PROCESS, 0, cckdblk.writerprio);
#endif

    obtain_lock (&cckdblk.cachelock);
    writer = ++cckdblk.writers;

    /* Return without messages if too many already started */
    if (writer > cckdblk.writermax
     && !(cckdblk.writermax == 0 && cckdblk.writepending))
    {
        --cckdblk.writers;
        release_lock (&cckdblk.cachelock);
        return;
    }

    if (!cckdblk.batch)
    cckdmsg ("cckddasd: writer thread %d started: tid="TIDPAT", pid=%d\n",
            writer, thread_id(), getpid());

    while (writer <= cckdblk.writermax || cckdblk.writepending)
    {
        /* Wait for work */
        if (!cckdblk.writepending)
        {
            /* Wake up threads waiting for writes to finish */
            if (cckdblk.writewaiting
             && cckdblk.writerswaiting + 1 >= cckdblk.writers)
                broadcast_condition (&cckdblk.writecond);

            /* Wait for new work */
            cckdblk.writerswaiting++;
            wait_condition (&cckdblk.writercond, &cckdblk.cachelock);
            cckdblk.writerswaiting--;
        }

        /* Scan the cache for the oldest pending entry */
        for (o = -1, i = 0; i < cckdblk.cachenbr; i++)
            if ((cckdblk.cache[i].flags & CCKD_CACHE_WRITE)
             && (o == -1 || cckdblk.cache[i].age < cckdblk.cache[o].age))
                o = i;

        /* Possibly shutting down if no writes pending */
        if (o < 0)
        {
            cckdblk.writepending = 0;
            continue;
        }

        cckdblk.writepending--;
        cckdblk.cache[o].flags &= ~CCKD_CACHE_WRITE;
        cckdblk.cache[o].flags |= CCKD_CACHE_WRITING;

        /* Schedule the other writers if any writes are still pending */
        if (cckdblk.writepending)
        {
            if (cckdblk.writerswaiting)
                signal_condition (&cckdblk.writercond);
            else if (cckdblk.writers < cckdblk.writermax)
                create_thread (&tid, NULL, cckd_writer, NULL);
        }

        release_lock (&cckdblk.cachelock);

        /* Compress the track image */
        buf = cckdblk.cache[o].buf;
        dev = cckdblk.cache[o].dev;
        cckd = dev->cckd_ext;
        len = cckd_trklen (dev, buf);
        trk = cckdblk.cache[o].trk;
        compress = (len < CCKD_COMPRESS_MIN ?
                    CCKD_COMPRESS_NONE : cckd->cdevhdr[cckd->sfn].compress);
        parm = cckd->cdevhdr[cckd->sfn].compress_parm;

        /* Stress adjustments */
        if (((cckdblk.cache[o].flags & CCKD_CACHE_WRITEWAIT)
          || cckdblk.cachewaiting
          || cckdblk.writepending > (cckdblk.cachenbr >> 1))
         && !cckdblk.nostress)
        {
            stressed = 1;
            cckdblk.stats_stresswrites++;
            if (len < CCKD_STRESS_MINLEN)
                compress = CCKD_COMPRESS_NONE;
            else
                compress = CCKD_STRESS_COMP;
            if (cckdblk.writepending < cckdblk.cachenbr - (cckdblk.cachenbr >> 2))
                parm = CCKD_STRESS_PARM1;
            else
                parm = CCKD_STRESS_PARM2;
            if (cckd->cdevhdr[cckd->sfn].compress_parm >= 0
             && cckd->cdevhdr[cckd->sfn].compress_parm < parm)
                parm = cckd->cdevhdr[cckd->sfn].compress_parm;
        }
        else stressed = 0;

        cckdtrc ("cckddasd: %d wrtrk[%d] %d comp %s parm %d\n",
                  writer, o, cckdblk.cache[o].trk, comp[compress], parm);

        switch (compress) {

#ifdef CCKD_COMPRESS_ZLIB
        case CCKD_COMPRESS_ZLIB:
            memcpy (&buf2, buf, CKDDASD_TRKHDR_SIZE);
            buf2[0] = CCKD_COMPRESS_ZLIB;
            len2 = 65535 - CKDDASD_TRKHDR_SIZE;
            rc = compress2 (&buf2[CKDDASD_TRKHDR_SIZE], &len2,
                            &buf[CKDDASD_TRKHDR_SIZE], len - CKDDASD_TRKHDR_SIZE,
                            parm);
            len2 += CKDDASD_TRKHDR_SIZE;
            cckdtrc ("cckddasd: writer[%d] compress trk %d len %lu rc=%d\n",
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
#endif

#ifdef CCKD_COMPRESS_BZIP2
        case CCKD_COMPRESS_BZIP2:
            memcpy (&buf2, buf, CKDDASD_TRKHDR_SIZE);
            buf2[0] = CCKD_COMPRESS_BZIP2;
            len2 = 65535 - CKDDASD_TRKHDR_SIZE;
            rc = BZ2_bzBuffToBuffCompress (
                            &buf2[CKDDASD_TRKHDR_SIZE], (unsigned int *)&len2,
                            &buf[CKDDASD_TRKHDR_SIZE], len - CKDDASD_TRKHDR_SIZE,
                            parm >= 1 && parm <= 9 ? parm : 5, 0, 0);
            len2 += CKDDASD_TRKHDR_SIZE;
            cckdtrc ("cckddasd: %d writer compress trk %d len %lu rc=%d\n",
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
            buf[0] = CCKD_COMPRESS_NONE;
            bufp = buf;
            bufl = len;
            rc = 0;
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
        if (cckdblk.gcols < cckdblk.gcolmax)
            create_thread (&tid, NULL, cckd_gcol, NULL);

        obtain_lock (&cckdblk.cachelock);
        cckdblk.cache[o].flags &= ~CCKD_CACHE_WRITING;
        if (cckdblk.cache[o].flags & CCKD_CACHE_WRITEWAIT)
        {   cckdtrc ("cckddasd: writer[%d] cache[%2.2d] %d signalling write complete\n",
                 writer, o, trk);
            signal_condition (&cckd->writecond);
        }
        else if (cckdblk.cachewaiting && !(cckdblk.cache[o].flags & CCKD_CACHE_BUSY))
        {   cckdtrc ("cckddasd: writer[%d] cache[%2.2d] %d signalling"
            " cache entry available\n", writer, o, trk);
            signal_condition (&cckdblk.cachecond);
        }

        cckdtrc ("cckddasd: %d wrtrk[%2.2d] %d complete flags:%8.8x\n",
                  writer, o, cckdblk.cache[o].trk, cckdblk.cache[o].flags);

    }

    if (!cckdblk.batch)
    cckdmsg ("cckddasd: writer thread %d stopping: tid="TIDPAT", pid=%d\n",
            writer, thread_id(), getpid());
    cckdblk.writers--;
    if (!cckdblk.writers) signal_condition(&cckdblk.termcond);
    release_lock(&cckdblk.cachelock);
} /* end thread cckd_writer */


/*-------------------------------------------------------------------*/
/* Get file space                                                    */
/*-------------------------------------------------------------------*/
off_t cckd_get_space(DEVBLK *dev, unsigned int len)
{
int             rc;                     /* Return code               */
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             i,p,n;                  /* Free space indices        */
off_t           fpos;                   /* Free space offset         */
unsigned int    flen;                   /* Free space size           */
int             sfx;                    /* Shadow file index         */
struct stat     st;                     /* File status area          */

    cckd = dev->cckd_ext;
    sfx = cckd->sfn;

    cckdtrc ("cckddasd: get_space len %d\n", len);

    if (len <= 1 || len >= 0xffff) return 0;
    if (!cckd->free) cckd_read_fsp (dev);

    if (!(len == cckd->cdevhdr[sfx].free_largest ||
          len + CCKD_FREEBLK_SIZE <= cckd->cdevhdr[sfx].free_largest))
    { /* no free space big enough; add space to end of the file */
        fpos = cckd->cdevhdr[sfx].size;
        if ((U64)(fpos + len) > 4294967295ULL)
        {
            devmsg("%4.4X:cckddasd file[%d] get space error, size exceeds 4G\n",
                   dev->devnum, sfx);
            return -1;
        }

        rc = fstat (cckd->fd[sfx], &st);
        if (rc < 0)
        {
            devmsg("%4.4X:cckddasd file[%d] get space fstat error, size %llx: %s\n",
                   dev->devnum, sfx, (long long)cckd->cdevhdr[sfx].size, strerror(errno));
            return -1;
        }

        if (fpos + len > st.st_size)
        {
            int sz = fpos + len;
            /* FIXME - workaround for ftruncate() linux problem */
            if (cckdblk.ftruncwa)
            {
                BYTE sfn[1024];
                sz = (sz + 1024*1024) & 0xfff00000;
                fsync (cckd->fd[sfx]);
                close (cckd->fd[sfx]);
                usleep(10000 * cckdblk.ftruncwa);
                cckd_sf_name (dev, sfx, (char *)&sfn);
                cckd->fd[sfx] = open (sfn, O_RDWR|O_BINARY);
                if (cckd->fd[sfx] < 0)
                {
                    devmsg ("%4.4X:cckddasd truncate re-open error: %s\n",
                            dev->devnum, strerror(errno));
                    return -1;
                }
            }
            rc = ftruncate (cckd->fd[sfx], sz);
            if (rc < 0)
            {
                devmsg("%4.4X:cckddasd file[%d] get space ftruncate error, size %llx: %s\n",
                   dev->devnum, sfx, (long long)cckd->cdevhdr[sfx].size, strerror(errno));
                return -1;
            }
        }

        cckd->cdevhdr[sfx].size += len;
        cckd->cdevhdr[sfx].used += len;
        cckdtrc ("cckddasd: get_space atend pos 0x%llx len %d\n",
                 (long long)fpos, len);
        return fpos;
    }

    /* scan free space chain */
    for (i = cckd->free1st; i >= 0; i = cckd->free[i].next)
        if ((cckd->free[i].len == len ||
             cckd->free[i].len >= len + CCKD_FREEBLK_SIZE)
         && !cckd->free[i].pending)
            break;

    p = cckd->free[i].prev;
    n = cckd->free[i].next;

    /* found a free space, obtain its file position and length */
    fpos = p >= 0 ? cckd->free[p].pos : cckd->cdevhdr[sfx].free;
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
        else cckd->freelast = p;

        /* make this free space entry available */
        cckd->free[i].next = cckd->freeavail;
        cckd->freeavail = i;
    }

    /* find the largest free space if we got the largest */
    if (flen >= cckd->cdevhdr[sfx].free_largest)
    {
        cckd->cdevhdr[sfx].free_largest = 0;
        for (i = cckd->free1st; i >= 0; i = cckd->free[i].next)
            if (cckd->free[i].len > cckd->cdevhdr[sfx].free_largest
             && !cckd->free[i].pending)
                cckd->cdevhdr[sfx].free_largest = cckd->free[i].len;
    }

    /* update free space stats */
    cckd->cdevhdr[sfx].used += len;
    cckd->cdevhdr[sfx].free_total -= len;

    cckdtrc ("cckddasd: get_space found pos 0x%llx len %d\n",
             (long long)fpos, len);

    return fpos;

} /* end function cckd_get_space */


/*-------------------------------------------------------------------*/
/* Release file space                                                */
/*-------------------------------------------------------------------*/
void cckd_rel_space(DEVBLK *dev, off_t pos, int len)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
off_t           ppos,npos;              /* Prev/next offsets         */
int             p2,p,i,n,n2;            /* Free space indices        */
int             sfx;                    /* Shadow file index         */

    if (len <= 1 || len >= 0xffff) return;

    cckd = dev->cckd_ext;
    sfx = cckd->sfn;

    cckdtrc ("cckddasd: rel_space pos %llx len %d nbr %d %s\n",
            (long long)pos, len, cckd->cdevhdr[sfx].free_number,
            pos + len == (off_t)cckd->cdevhdr[sfx].size ? "at end" : "");

    if (!cckd->free) cckd_read_fsp (dev);

    /* Increase the size of the free space array if necessary */
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

    /* Get a free space entry */
    i = cckd->freeavail;
    cckd->freeavail = cckd->free[i].next;
    cckd->free[i].len = len;
    if (cckdblk.freepend >= 0)
        cckd->free[i].pending = cckdblk.freepend;
    else
        cckd->free[i].pending = 1 + (1 - cckdblk.fsync);

    /* Update the free space statistics */
    cckd->cdevhdr[sfx].free_number++;
    cckd->cdevhdr[sfx].used -= len;
    cckd->cdevhdr[sfx].free_total += len;

    /* Scan free space chain */
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

    /* Insert the new entry into the chain */
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

    /* If the new free space is adjacent to the previous free
       space then combine the two if the pending values match */
    if (p >= 0 && (off_t)(ppos + cckd->free[p].len) == pos
     && cckd->free[p].pending == cckd->free[i].pending)
    {
        cckd->cdevhdr[sfx].free_number--;
        cckd->free[p].pos = cckd->free[i].pos;
        cckd->free[p].len += cckd->free[i].len;
        cckd->free[p].next = cckd->free[i].next;
        if (cckd->free[p].len > cckd->cdevhdr[sfx].free_largest
         && !cckd->free[p].pending)
            cckd->cdevhdr[sfx].free_largest = cckd->free[p].len;
        cckd->free[i].next = cckd->freeavail;
        cckd->freeavail = i;
        if (n >= 0) cckd->free[n].prev = p;
        i = p;
        pos = ppos;
        p = p2;
    }

    /* If the new free space is adjacent to the following free
       space then combine the two if the pending values match */
    if (n >= 0 && (off_t)(pos + cckd->free[i].len) == npos
     && cckd->free[n].pending == cckd->free[i].pending)
    {
        cckd->cdevhdr[sfx].free_number--;
        cckd->free[i].pos = cckd->free[n].pos;
        cckd->free[i].len += cckd->free[n].len;
        cckd->free[i].next = cckd->free[n].next;
        if (cckd->free[i].len > cckd->cdevhdr[sfx].free_largest
         && !cckd->free[i].pending)
            cckd->cdevhdr[sfx].free_largest = cckd->free[i].len;
        cckd->free[n].next = cckd->freeavail;
        cckd->freeavail = n;
        n = n2;
        if (n >= 0) cckd->free[n].prev = i;
    }

    /* Update if the last free space entry */
    if (cckd->free[i].next < 0)
        cckd->freelast = i;

    /* Release the free space if it's at the end */
    if (pos + cckd->free[i].len == cckd->cdevhdr[sfx].size
     && !cckd->free[i].pending)
        cckd_rel_free_atend (dev, pos, cckd->free[i].len, i);

} /* end function cckd_rel_space */


/*-------------------------------------------------------------------*/
/* Release free space at end of file                                 */
/*-------------------------------------------------------------------*/
void cckd_rel_free_atend(DEVBLK *dev, unsigned int pos, int len, int i)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             p;                      /* Prev free space index     */
int             sfx;                    /* Shadow file index         */

    cckd = dev->cckd_ext;
    sfx = cckd->sfn;

    cckdtrc ("cckddasd: rel_free_atend ix %d pos %x len %d sz %d\n",
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
    cckd->freelast = p;
    cckd->free[i].next = cckd->freeavail;
    cckd->freeavail = i;

    if (cckd->free[i].len >= cckd->cdevhdr[sfx].free_largest
     && !cckd->free[i].pending)
    {   /* find the next largest free space */
        cckd->cdevhdr[sfx].free_largest = 0;
        for (i = cckd->free1st; i >= 0; i = cckd->free[i].next)
            if (cckd->free[i].len > cckd->cdevhdr[sfx].free_largest
             && !cckd->free[i].pending)
                cckd->cdevhdr[sfx].free_largest = cckd->free[i].len;
    }

} /* end function cckd_rel_free_atend */


/*-------------------------------------------------------------------*/
/* Flush pending free space                                          */
/*-------------------------------------------------------------------*/
void cckd_flush_space(DEVBLK *dev)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             p,i,n;                  /* Free free space indexes   */
int             sfx;                    /* Shadow file index         */
U32             ppos, pos;              /* Free space offsets        */

    cckd = dev->cckd_ext;
    sfx = cckd->sfn;

    cckdtrc ("cckddasd: rel_flush_space nbr %d\n",
             cckd->cdevhdr[sfx].free_number);

    pos = cckd->cdevhdr[sfx].free;
    ppos = p = -1;
    cckd->cdevhdr[sfx].free_number = cckd->cdevhdr[sfx].free_largest = 0;
    for (i = cckd->free1st; i >= 0; i = cckd->free[i].next)
    {
        /* Decrement the pending count */
        if (cckd->free[i].pending)
            --cckd->free[i].pending;

        /* Combine adjacent free spaces */
        while (pos + cckd->free[i].len == cckd->free[i].pos)
        {
            n = cckd->free[i].next;
            if (cckd->free[n].pending > cckd->free[i].pending + 1
             || cckd->free[n].pending < cckd->free[i].pending)
                break;
            cckd->free[i].pos = cckd->free[n].pos;
            cckd->free[i].len += cckd->free[n].len;
            cckd->free[i].next = cckd->free[n].next;
            cckd->free[n].next = cckd->freeavail;
            cckd->freeavail = n;
            n = cckd->free[i].next;
            if (n >= 0)
                cckd->free[n].prev = i;

        }
        ppos = pos;
        pos = cckd->free[i].pos;
        cckd->cdevhdr[sfx].free_number++;
        if (cckd->free[i].len > cckd->cdevhdr[sfx].free_largest
         && !cckd->free[i].pending)
            cckd->cdevhdr[sfx].free_largest = cckd->free[i].len;
        p = i;
    }
    cckd->freelast = p;

    cckdtrc ("cckddasd: rel_flush_space nbr %d (after merge)\n",
             cckd->cdevhdr[sfx].free_number);

    /* If the last free space is at the end of the file then release it */
    if (p >= 0 && ppos + cckd->free[p].len == cckd->cdevhdr[sfx].size
     && !cckd->free[p].pending)
        cckd_rel_free_atend (dev, ppos, cckd->free[p].len, p);

} /* end function cckd_flush_space */


/*-------------------------------------------------------------------*/
/* Read compressed dasd header                                       */
/*-------------------------------------------------------------------*/
int cckd_read_chdr (DEVBLK *dev)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             rc;                     /* Return code               */
off_t           rcoff;                  /* lseek() return value      */
int             sfx;                    /* File index                */
int             fend,mend;              /* Byte order indicators     */

    cckd = dev->cckd_ext;
    sfx = cckd->sfn;

    memset (&cckd->cdevhdr[sfx], 0, CCKDDASD_DEVHDR_SIZE);

    /* read the device header */
    rcoff = lseek(cckd->fd[sfx], CKDDASD_DEVHDR_SIZE, SEEK_SET);
    if (rcoff < 0)
    {
        devmsg ("%4.4X:cckddasd file[%d] hdr lseek error, offset %llx: %s\n",
                dev->devnum, sfx, (long long)CKDDASD_DEVHDR_SIZE, strerror(errno));
        return -1;
    }

    /* read the compressed device header */
    rc = read (cckd->fd[sfx], &cckd->cdevhdr[sfx], CCKDDASD_DEVHDR_SIZE);
    if (rc != CCKDDASD_DEVHDR_SIZE)
    {
        devmsg ("%4.4X:cckddasd file[%d] chdr read error, offset %llx: %s\n",
                dev->devnum, sfx, (long long)CKDDASD_DEVHDR_SIZE, strerror(errno));
        return -1;
    }

    /* check endian format */
    cckd->swapend[sfx] = 0;
    fend = ((cckd->cdevhdr[sfx].options & CCKD_BIGENDIAN) != 0);
    mend = cckd_endian ();
    if (fend != mend)
    {
        if (cckd->open[sfx] == CCKD_OPEN_RW)
        {
            rc = cckd_swapend (cckd->fd[sfx], dev->msgpipew);
            if (rc < 0) return -1;
            return cckd_read_chdr (dev);
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
int cckd_write_chdr (DEVBLK *dev)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             rc;                     /* Return code               */
off_t           rcoff;                  /* lseek() return value      */
int             sfx;                    /* File index                */

    cckd = dev->cckd_ext;
    sfx = cckd->sfn;

    rcoff = lseek(cckd->fd[sfx], CKDDASD_DEVHDR_SIZE, SEEK_SET);
    if (rcoff < 0)
    {
        devmsg ("%4.4X:cckddasd file[%d] chdr lseek error, offset %llx: %s\n",
                dev->devnum, sfx, (long long)CKDDASD_DEVHDR_SIZE, strerror(errno));
        return -1;
    }
    rc = write (cckd->fd[sfx], &cckd->cdevhdr[sfx], CCKDDASD_DEVHDR_SIZE);
    if (rc < CCKDDASD_DEVHDR_SIZE)
    {
        devmsg ("%4.4X:cckddasd file[%d] chdr write error, offset %llx: %s\n",
                dev->devnum, sfx, (long long)CKDDASD_DEVHDR_SIZE, strerror(errno));
        return -1;
    }

    return 0;

} /* end function cckd_write_chdr */


/*-------------------------------------------------------------------*/
/* Read the level 1 table                                            */
/*-------------------------------------------------------------------*/
int cckd_read_l1 (DEVBLK *dev)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             rc;                     /* Return code               */
off_t           rcoff;                  /* lseek() return value      */
int             sfx;                    /* File index                */
int             len;                    /* Length of level 1 table   */

    cckd = dev->cckd_ext;
    sfx = cckd->sfn;

    /* Free the old level 1 table if it exists */
    if (cckd->l1[sfx] != NULL) free (cckd->l1[sfx]);

    /* get the level 1 table */
    len = cckd->cdevhdr[sfx].numl1tab * CCKD_L1ENT_SIZE;
    cckd->l1[sfx] = malloc (len);
    if (cckd->l1[sfx] == NULL)
    {
        devmsg ("%4.4X:cckddasd l1 table malloc error: %s\n",
                dev->devnum, strerror(errno));
        return -1;
    }

    /* read the level 1 table */
    rcoff = lseek (cckd->fd[sfx], CCKD_L1TAB_POS, SEEK_SET);
    if (rcoff < 0)
    {
        devmsg ("%4.4X:cckddasd file[%d] l1 lseek error, offset %llx: %s\n",
                dev->devnum, sfx, (long long)CCKD_L1TAB_POS, strerror(errno));
        return -1;
    }
    rc = read(cckd->fd[sfx], cckd->l1[sfx], len);
    if (rc != len)
    {
        devmsg ("%4.4X:cckddasd file[%d] l1 read error, offset %llx: %s\n",
                dev->devnum, sfx, (long long)CCKD_L1TAB_POS, strerror(errno));
        return -1;
    }
    if (cckd->swapend[sfx])
        cckd_swapend_l1 (cckd->l1[sfx], cckd->cdevhdr[sfx].numl1tab);

    cckdtrc ("cckddasd: file[%d] l1 read offset 0x%llx\n",
              sfx, (long long) CCKD_L1TAB_POS);

    return 0;

} /* end function cckd_read_l1 */


/*-------------------------------------------------------------------*/
/* Write the level 1 table                                           */
/*-------------------------------------------------------------------*/
int cckd_write_l1 (DEVBLK *dev)
{
CCKDDASD_EXT    *cckd;                  /* -> cckd extension         */
int             rc;                     /* Return code               */
off_t           rcoff;                  /* lseek() return value      */
int             sfx;                    /* File index                */
int             len;                    /* Length of level 1 table   */

    cckd = dev->cckd_ext;
    sfx = cckd->sfn;
    len = cckd->cdevhdr[sfx].numl1tab * CCKD_L1ENT_SIZE;

    rcoff = lseek (cckd->fd[sfx], CCKD_L1TAB_POS, SEEK_SET);
    if (rcoff < 0)
    {
        devmsg ("%4.4X:cckddasd file[%d] l1 lseek error, offset %llx: %s\n",
                dev->devnum, sfx, (long long)CCKD_L1TAB_POS, strerror(errno));
        return -1;
    }
    rc = write (cckd->fd[sfx], cckd->l1[sfx], len);
    if (rc != len)
    {
        devmsg ("%4.4X:cckddasd file[%d] l1 write error, offset %llx: %s\n",
                dev->devnum, sfx, (long long)CCKD_L1TAB_POS, strerror(errno));
        return -1;
    }

    cckdtrc ("cckddasd: file[%d] l1 written pos 0x%llx\n",
              sfx, (long long) CCKD_L1TAB_POS);

    return 0;

} /* end function cckd_write_l1 */


/*-------------------------------------------------------------------*/
/* Update a level 1 table entry                                      */
/*-------------------------------------------------------------------*/
int cckd_write_l1ent (DEVBLK *dev, int l1x)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             sfx;                    /* File index                */
int             rc;                     /* Return code               */
off_t           rcoff;                  /* lseek() return value      */
off_t           l1pos;                  /* Offset to l1 entry        */

    cckd = dev->cckd_ext;
    sfx = cckd->sfn;
    l1pos = (off_t)(CCKD_L1TAB_POS + l1x * CCKD_L1ENT_SIZE);

    rcoff = lseek (cckd->fd[sfx], l1pos, SEEK_SET);
    if (rcoff < 0)
    {
        devmsg ("%4.4X:cckddasd file[%d] l1[%d] lseek error, offset %llx: %s\n",
                dev->devnum, sfx, l1x, (long long)l1pos, strerror(errno));
        return -1;
    }
    rc = write (cckd->fd[sfx], &cckd->l1[sfx][l1x], CCKD_L1ENT_SIZE);
    if (rc != CCKD_L1ENT_SIZE)
    {
        devmsg ("%4.4X:cckddasd file[%d] l1[%d] write error, offset %llx: %s\n",
                dev->devnum, sfx, l1x, (long long)l1pos, strerror(errno));
        return -1;
    }

    cckdtrc ("cckddasd: file[%d] l1[%d] updated offset 0x%llx\n",
             sfx, l1x, (long long)l1pos);

    return 0;

} /* end function cckd_write_l1ent */


/*-------------------------------------------------------------------*/
/* Initial read                                                      */
/*-------------------------------------------------------------------*/
int cckd_read_init (DEVBLK *dev)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
CKDDASD_DEVHDR  devhdr;                 /* Device header             */
int             rc;                     /* Return code               */
off_t           rcoff;                  /* lseek() return value      */
int             sfx;                    /* File index                */

    cckd = dev->cckd_ext;
    sfx = cckd->sfn;

    /* Read the device header */
    rcoff = lseek (cckd->fd[sfx], 0, SEEK_SET);
    if (rcoff < 0)
    {
        devmsg ("%4.4X:cckddasd file[%d] devhdr lseek error, offset %llx: %s\n",
                dev->devnum, sfx, (long long)0, strerror(errno));
        return -1;
    }
    rc = read (cckd->fd[sfx], &devhdr, CKDDASD_DEVHDR_SIZE);
    if (rc != CKDDASD_DEVHDR_SIZE)
    {
        devmsg ("%4.4X:cckddasd file[%d] devhdr read error, offset %llx: %s\n",
                dev->devnum, sfx, (long long)0, strerror(errno));
        return -1;
    }

    /* Check the device hdr */
    if (sfx == 0 && memcmp (&devhdr.devid, "CKD_C370", 8) == 0)
        cckd->ckddasd = 1;
    else if (sfx == 0 && memcmp (&devhdr.devid, "FBA_C370", 8) == 0)
        cckd->fbadasd = 1;
    else if (!(sfx && memcmp (&devhdr.devid, "CKD_S370", 8) == 0 && cckd->ckddasd)
          && !(sfx && memcmp (&devhdr.devid, "FBA_S370", 8) == 0 && cckd->fbadasd))
    {
        devmsg ("%4.4X:cckddasd file[%d] devhdr id error\n",
                dev->devnum, sfx);
        return -1;
    }

    /* Read the compressed header */
    rc = cckd_read_chdr (dev);
    if (rc < 0) return -1;

    /* Read the level 1 table */
    rc = cckd_read_l1 (dev);
    if (rc < 0) return -1;

    return 0;
} /* end function cckd_read_init */


/*-------------------------------------------------------------------*/
/* Read  the free space                                              */
/*-------------------------------------------------------------------*/
int cckd_read_fsp (DEVBLK *dev)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             rc;                     /* Return code               */
off_t           rcoff;                  /* lseek() return value      */
off_t           fpos;                   /* Free space offset         */
int             sfx;                    /* File index                */
int             i;                      /* Index                     */

    cckd = dev->cckd_ext;
    sfx = cckd->sfn;

    cckdtrc ("cckddasd: file[%d] read free space, number %d\n",
              sfx, cckd->cdevhdr[sfx].free_number);

    cckd->free1st = cckd->freeavail = -1;

    /* get storage for the internal free space chain;
       get a multiple of 1024 entries. */
    cckd->freenbr = ((cckd->cdevhdr[sfx].free_number << 10) + 1) >> 10;
    cckd->free = calloc (cckd->freenbr, CCKD_FREEBLK_ISIZE);
    if (!cckd->free)
    {
        devmsg ("%4.4X:cckddasd: calloc failed for free space, size %d: %s\n",
                dev->devnum, cckd->freenbr * CCKD_FREEBLK_ISIZE,
                strerror(errno));
        return -1;
    }

    /* if the only free space is at the end of the file,
       then remove it.  this should only happen for a file
       built by the cckddump os/390 utility */
    if (cckd->cdevhdr[sfx].free_number == 1)
    {
        fpos = (off_t)cckd->cdevhdr[sfx].free;
        rcoff = lseek (cckd->fd[sfx], fpos, SEEK_SET);
        if (rcoff < 0)
        {
            devmsg("%4.4X:cckddasd file[%d] free space lseek error, offset %llx: %s\n",
                   dev->devnum, sfx, (long long)fpos, strerror(errno));
            return -1;
        }
        rc = read (cckd->fd[sfx], &cckd->free[0], CCKD_FREEBLK_SIZE);
        if (rc < CCKD_FREEBLK_SIZE)
        {
            devmsg("%4.4X:cckddasd file[%d] free space read error, offset %llx: %s\n",
                   dev->devnum, sfx, (long long)fpos, strerror(errno));
            return -1;
        }
        if (fpos + cckd->free[0].len == cckd->cdevhdr[sfx].size)
        {
            cckd->cdevhdr[sfx].free_number = 
            cckd->cdevhdr[sfx].free_total = 
            cckd->cdevhdr[sfx].free_largest = 0;
            cckd->cdevhdr[sfx].size -= cckd->free[0].len;
            rc = ftruncate (cckd->fd[sfx], (off_t)cckd->cdevhdr[sfx].size);
            if (rc < 0)
            {
                devmsg("%4.4X:cckddasd file[%d] free space ftruncate error, size %llx: %s\n",
                       dev->devnum, sfx, (long long)cckd->cdevhdr[sfx].size, strerror(errno));
                return -1;
            }
        }
    }

    /* build the doubly linked internal free space chain */
    if (cckd->cdevhdr[sfx].free_number)
    {
        cckd->free1st = 0;
        fpos = (off_t)cckd->cdevhdr[sfx].free;
        for (i = 0; i < cckd->cdevhdr[sfx].free_number; i++)
        {
            rcoff = lseek (cckd->fd[sfx], fpos, SEEK_SET);
            if (rcoff < 0)
            {
                devmsg("%4.4X:cckddasd file[%d] free space lseek error, offset %llx: %s\n",
                       dev->devnum, sfx, (long long)fpos, strerror(errno));
                return -1;
            }
            rc = read (cckd->fd[sfx], &cckd->free[i], CCKD_FREEBLK_SIZE);
            if (rc < CCKD_FREEBLK_SIZE)
            {
                devmsg("%4.4X:cckddasd file[%d] free space read error, offset %llx: %s\n",
                       dev->devnum, sfx, (long long)fpos, strerror(errno));
                return -1;
            }
            cckd->free[i].prev = i - 1;
            cckd->free[i].next = i + 1;
            fpos = (off_t)cckd->free[i].pos;
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
int cckd_write_fsp (DEVBLK *dev)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             rc;                     /* Return code               */
off_t           rcoff;                  /* lseek() return value      */
off_t           fpos;                   /* Free space offset         */
int             sfx;                    /* File index                */
int             i;                      /* Index                     */

    cckd = dev->cckd_ext;
    sfx = cckd->sfn;

    if (!cckd->free) return 0;

    for (i = 0; i < CCKD_MAX_FREEPEND; i++)
        cckd_flush_space(dev);

    cckdtrc ("cckddasd: file[%d] write free space, number %d\n",
              sfx, cckd->cdevhdr[sfx].free_number);

    fpos = (off_t)cckd->cdevhdr[sfx].free;
    for (i = cckd->free1st; i >= 0; i = cckd->free[i].next)
    {
        rcoff = lseek (cckd->fd[sfx], fpos, SEEK_SET);
        if (rcoff < 0)
        {
            devmsg("%4.4X:cckddasd file[%d] free space lseek error, offset %llx: %s\n",
                   dev->devnum, sfx, (long long)fpos, strerror(errno));
            return -1;
        }
        rc = write (cckd->fd[sfx], &cckd->free[i], CCKD_FREEBLK_SIZE);
        if (rc < CCKD_FREEBLK_SIZE)
        {
            devmsg("%4.4X:cckddasd file[%d] free space write error, offset %llx: %s\n",
                   dev->devnum, sfx, (long long)fpos, strerror(errno));
            return -1;
        }
        fpos = (off_t)cckd->free[i].pos;
    }

    if (cckd->free) free (cckd->free);
    cckd->free = NULL;
    cckd->freenbr = 0;
    cckd->free1st = cckd->freeavail = -1;

    cckd_truncate (dev, 1);

    return 0;

} /* end function cckd_write_fsp */


/*-------------------------------------------------------------------*/
/* Read a new level 2 table                                          */
/*-------------------------------------------------------------------*/
int cckd_read_l2 (DEVBLK *dev, int sfx, int l1x)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             rc;                     /* Return code               */
off_t           rcoff;                  /* lseek() return value      */
int             i;                      /* Index variable            */
int             fnd=-1;                 /* Cache index for hit       */
int             lru=-1;                 /* Least-Recently-Used cache
                                            index                    */

    cckd = dev->cckd_ext;

    /* return if table is already active */
    if (sfx == cckd->sfx && l1x == cckd->l1x) return 0;

    obtain_lock (&cckdblk.l2cachelock);

    /* Inactivate the previous entry */
    if (cckd->l2active)
        cckd->l2active->flags &= ~CCKD_CACHE_ACTIVE;
    cckd->l2active = (void *)cckd->l2 = NULL;
    cckd->sfx = cckd->l1x = -1;

cckd_read_l2_retry:

    /* scan the cache array for the l2tab */
    for (i = 0; i < cckdblk.l2cachenbr; i++)
    {
        if (sfx == cckdblk.l2cache[i].sfx
         && l1x == cckdblk.l2cache[i].l1x
         && dev == cckdblk.l2cache[i].dev)
        {   fnd = i;
            break;
        }
        /* find the oldest entry */
        if ((lru == - 1 || cckdblk.l2cache[i].age < cckdblk.l2cache[lru].age)
         && !(cckdblk.l2cache[i].flags & CCKD_CACHE_BUSY))
            lru = i;
    }

    /* check for level 2 cache hit */
    if (fnd >= 0)
    {
        cckdtrc ("cckddasd: l2[%d,%d] cache[%d] hit\n", sfx, l1x, fnd);
        cckdblk.l2cache[fnd].age = ++cckdblk.cacheage;
        cckdblk.l2cache[fnd].flags = CCKD_CACHE_ACTIVE;
        cckdblk.stats_l2cachehits++;
        release_lock (&cckdblk.l2cachelock);
        cckd->sfx = sfx;
        cckd->l1x = l1x;
        cckd->l2 = (CCKD_L2ENT *)cckdblk.l2cache[fnd].buf;
        cckd->l2active = &cckdblk.l2cache[fnd];
        return 1;
    }
    cckdtrc ("cckddasd: l2[%d,%d] cache[%d] miss\n", sfx, l1x, lru);

    /* Check if an available entry was found */
    if (lru < 0)
    {
        int j = 8;
        if (cckdblk.l2cachenbr + j > CCKD_MAX_L2CACHE)
            j = CCKD_MAX_L2CACHE - cckdblk.l2cachenbr;
        if (j > 0)
        {
            cckdblk.l2cachenbr += j;
            devmsg ("%4.4X:cckddasd: expanding l2 cache to %d\n",
                    dev->devnum, cckdblk.l2cachenbr);
            goto cckd_read_l2_retry;
        }
        devmsg ("%4.4X:cckddasd: No available l2 cache entry !!\n",
                dev->devnum);
        return -1;
    }

    /* get buffer for level 2 table if there isn't one */
    if (!cckdblk.l2cache[lru].buf)
    {
        cckdblk.l2cache[lru].buf = malloc (CCKD_L2TAB_SIZE);
        if (!cckdblk.l2cache[lru].buf)
        {
            devmsg ("cckddasd: malloc failed for l2 buffer, size %d: %s\n",
                    CCKD_L2TAB_SIZE, strerror(errno));
            cckdblk.l2cache[lru].flags = CCKD_CACHE_INVALID;
            goto cckd_read_l2_retry;
        }
    }

    cckdblk.l2cache[lru].dev = dev;
    cckdblk.l2cache[lru].sfx = sfx;
    cckdblk.l2cache[lru].l1x = l1x;
    cckdblk.l2cache[lru].age = ++cckdblk.cacheage;
    cckdblk.l2cache[lru].flags = CCKD_CACHE_ACTIVE;
    cckdblk.stats_l2cachemisses++;
    release_lock (&cckdblk.l2cachelock);

    /* check for null table */
    if (!cckd->l1[sfx][l1x] || cckd->l1[sfx][l1x] == 0xffffffff)
    {
        memset (cckdblk.l2cache[lru].buf, cckd->l1[sfx][l1x] & 0xff, CCKD_L2TAB_SIZE);
        cckdtrc ("cckddasd: l2[%d,%d] cache[%d] null\n", sfx, l1x, lru);
    }
    /* read the new level 2 table */
    else
    {
        rcoff = lseek (cckd->fd[sfx], (off_t)cckd->l1[sfx][l1x], SEEK_SET);
        if (rcoff < 0)
        {
            devmsg ("%4.4X:cckddasd: file[%d] l2[%d] lseek error offset %lld: %s\n",
                    dev->devnum, sfx, l1x, (long long)cckd->l1[sfx][l1x],
                    strerror(errno));
            cckdblk.l2cache[lru].flags = 0;
            return -1;
        }
        rc = read (cckd->fd[sfx], cckdblk.l2cache[lru].buf, CCKD_L2TAB_SIZE);
        if (rc < CCKD_L2TAB_SIZE)
        {
            devmsg ("%4.4X:cckddasd: file[%d] l2[%d] read error offset %lld: %s\n",
                    dev->devnum, sfx, l1x, (long long)cckd->l1[sfx][l1x],
                    strerror(errno));
            cckdblk.l2cache[lru].flags = 0;
            return -1;
        }
        if (cckd->swapend[sfx])
            cckd_swapend_l2 ((CCKD_L2ENT *)cckdblk.l2cache[lru].buf);
        cckdtrc ("cckddasd: file[%d] cache[%d] l2[%d] read offset 0x%llx\n",
                 sfx, lru, l1x, (long long)cckd->l1[sfx][l1x]);
        cckd->l2reads[sfx]++;
        cckd->totl2reads++;
        cckdblk.stats_l2reads++;
    }

    cckd->sfx = sfx;
    cckd->l1x = l1x;
    cckd->l2 = (CCKD_L2ENT *)cckdblk.l2cache[lru].buf;
    cckd->l2active = &cckdblk.l2cache[lru];

    return 0;

} /* end function cckd_read_l2 */


/*-------------------------------------------------------------------*/
/* Purge all l2tab cache entries for a given index                   */
/*-------------------------------------------------------------------*/
void cckd_purge_l2 (DEVBLK *dev)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             i;                      /* Index variable            */

    /* scan the cache array for l2tabs matching this index */
    obtain_lock (&cckdblk.l2cachelock);

    /* Invalidate the active entry */
    if (dev != NULL)
    {
        cckd = dev->cckd_ext;
        cckd->sfx = cckd->l1x = -1;
        cckd->l2 = (void *)cckd->l2active = NULL;
    }

    for (i = 0; i < cckdblk.l2cachenbr && cckdblk.l2cache; i++)
    {
        if (dev == NULL || dev == cckdblk.l2cache[i].dev)
        {
            /* Invalidate the cache entry */
            cckdblk.l2cache[i].dev = NULL;
            cckdblk.l2cache[i].sfx = cckdblk.l2cache[i].l1x = -1;
            cckdblk.l2cache[i].age = cckdblk.l2cache[i].flags = 0;
            if (dev == NULL && cckdblk.l2cache[i].buf)
            {
                free (cckdblk.l2cache[i].buf);
                cckdblk.l2cache[i].buf = NULL;
            }
        }
    }
    release_lock (&cckdblk.l2cachelock);

} /* end function cckd_purge_l2 */


/*-------------------------------------------------------------------*/
/* Write the current level 2 table                                   */
/*-------------------------------------------------------------------*/
int cckd_write_l2 (DEVBLK *dev)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             sfx,l1x;                /* Lookup table indices      */
off_t           l2pos;                  /* Level 2 table file offset */
int             rc;                     /* Return code               */
off_t           rcoff;                  /* lseek() return value      */

    cckd = dev->cckd_ext;
    sfx = cckd->sfn;
    l1x = cckd->l1x;

    if (sfx < 0 || l1x < 0) return -1;

    if (!cckd->l1[sfx][l1x] || cckd->l1[sfx][l1x] == 0xffffffff)
    {
        l2pos = cckd_get_space (dev, CCKD_L2TAB_SIZE);
        cckdtrc ("cckddasd: file[%d] l2[%d] new, offset 0x%llx\n",
                  sfx, l1x, (long long)l2pos);
        if (l2pos == -1) return -1;
    }
    else l2pos = (off_t)cckd->l1[sfx][l1x];

    /* write the level 2 table */
    rcoff = lseek (cckd->fd[sfx], l2pos, SEEK_SET);
    if (rcoff < 0)
    {
        devmsg ("%4.4X:cckddasd: file[%d] l2[%d] lseek error offset %lld: %s\n",
                dev->devnum, sfx, l1x, (long long)l2pos, strerror(errno));
        return -1;
    }
    rc = write (cckd->fd[sfx], cckd->l2, CCKD_L2TAB_SIZE);
    if (rc < CCKD_L2TAB_SIZE)
    {
        devmsg ("%4.4X:cckddasd: file[%d] l2[%d] write error offset %lld: %s\n",
                dev->devnum, sfx, l1x, (long long)l2pos, strerror(errno));
        return -1;
    }
    cckdtrc ("cckddasd: file[%d] l2[%d] written offset 0x%llx\n",
             sfx, l1x, (long long unsigned int)l2pos);

    /* Check if level 1 table needs to be updated */
    if ((U32)l2pos != cckd->l1[sfx][l1x])
    {
        cckd->l1[sfx][l1x] = (U32)l2pos;
        rc = cckd_write_l1ent (dev, l1x);
        if (rc < 0) return -1;
    }

    return 0;

} /* end function cckd_write_l2 */


/*-------------------------------------------------------------------*/
/* Return a level 2 entry                                            */
/*-------------------------------------------------------------------*/
int cckd_read_l2ent (DEVBLK *dev, CCKD_L2ENT *l2, int trk)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             rc=0;                   /* Return code               */
int             sfx,l1x,l2x;            /* Lookup table indices      */

    cckd = dev->cckd_ext;

    l1x = trk >> 8;
    l2x = trk & 0xff;

    if (l2) memset (l2, 0, CCKD_L2ENT_SIZE);

    for (sfx = cckd->sfn; sfx >= 0; sfx--)
    {
        cckdtrc ("cckddasd: rdl2ent trk %d l2[%d,%d] offset 0x%x\n",
                 trk, sfx, l1x, cckd->l1[sfx][l1x]);
        if (cckd->l1[sfx][l1x] == 0xffffffff) continue;
        rc = cckd_read_l2 (dev, sfx, l1x);
        if (rc < 0) return -1;
        if (cckd->l2[l2x].pos != 0xffffffff) break;
    }

    if (l2) memcpy (l2, &cckd->l2[l2x], CCKD_L2ENT_SIZE);

    cckdtrc ("cckddasd: file[%d] l2[%d,%d] entry %s trk %d pos 0x%llx len %d\n",
              sfx, l1x, l2x, rc ? "cached" : "read", trk,
             (long long)cckd->l2[l2x].pos, cckd->l2[l2x].len);

    return sfx;

} /* end function cckd_read_l2ent */


/*-------------------------------------------------------------------*/
/* Update a level 2 entry                                            */
/*-------------------------------------------------------------------*/
int cckd_write_l2ent (DEVBLK *dev,  CCKD_L2ENT *l2, int trk)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             sfx,l1x,l2x;            /* Lookup table indices      */
off_t           l2pos;                  /* Level 2 table file offset */
int             rc;                     /* Return code               */
off_t           rcoff;                  /* lseek() return value      */

    cckd = dev->cckd_ext;

    /* Error return if no available level 2 table */
    if (!cckd->l2) return -1;

    sfx = cckd->sfn;
    l1x = trk >> 8;
    l2x = trk & 0xff;

    /* Copy the new entry if passed */
    if (l2) memcpy (&cckd->l2[l2x], l2, CCKD_L2ENT_SIZE);

    /* If no level 2 table for this file, then write a new one */
    if (!cckd->l1[sfx][l1x] || cckd->l1[sfx][l1x] == 0xffffffff)
        return cckd_write_l2 (dev);

    l2pos = (off_t)(cckd->l1[sfx][l1x] + l2x * CCKD_L2ENT_SIZE);
    rcoff = lseek (cckd->fd[sfx], l2pos, SEEK_SET);
    if (rcoff < 0)
    {
        devmsg ("%4.4X:cckddasd: file[%d] l2[%d,%d] lseek error offset %lld: %s\n",
                dev->devnum, sfx, l1x, l2x, (long long)l2pos, strerror(errno));
        return -1;
    }
    rc = write (cckd->fd[sfx], &cckd->l2[l2x], CCKD_L2ENT_SIZE);
    if (rc < CCKD_L2ENT_SIZE)
    {
        devmsg ("%4.4X:cckddasd: file[%d] l2[%d,%d] write error offset %lld: %s\n",
                dev->devnum, sfx, l1x, l2x, (long long)l2pos, strerror(errno));
        return -1;
    }
    cckdtrc ("cckddasd: file[%d] l2[%d,%d] updated offset 0x%llx\n",
              sfx, l1x, l2x, (long long)l2pos);

    return 0;
} /* end function cckd_write_l2ent */


/*-------------------------------------------------------------------*/
/* Read a track image                                                */
/*-------------------------------------------------------------------*/
int cckd_read_trkimg (DEVBLK *dev, BYTE *buf, int trk, BYTE *unitstat)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             rc;                     /* Return code               */
off_t           rcoff;                  /* lseek() return value      */
int             sfx,l1x,l2x;            /* Lookup table indices      */
CCKD_L2ENT      l2;                     /* Level 2 entry             */

    cckd = dev->cckd_ext;

    sfx = cckd_read_l2ent (dev, &l2, trk);
    if (sfx < 0) goto cckd_read_trkimg_error;
    l1x = trk >> 8;
    l2x = trk & 0xff;

    if (sfx >= 0 && l2.pos && l2.pos != 0xffffffff)
    {
        rcoff = lseek (cckd->fd[sfx], (off_t)l2.pos, SEEK_SET);
        if (rcoff < 0)
        {
            devmsg ("%4.4X:cckddasd: file[%d] trk %d lseek error offset %llx: %s\n",
                    dev->devnum, sfx, trk, (long long)l2.pos, strerror(errno));
            goto cckd_read_trkimg_error;
        }
        rc = read (cckd->fd[sfx], buf, l2.len);
        if (rc < l2.len)
        {
            devmsg ("%4.4X:cckddasd: file[%d] trk %d read error offset %llx: %s\n",
                    dev->devnum, sfx, trk, (long long)l2.pos, strerror(errno));
            goto cckd_read_trkimg_error;
        }
        cckd->reads[sfx]++;
        cckd->totreads++;
        cckdblk.stats_reads++;
        cckdblk.stats_readbytes += rc;
    }
    else rc = cckd_null_trk (dev, buf, trk, l2.len);

    cckdtrc ("cckddasd: trkimg %d read sfx %d pos 0x%x len %d "
              "%2.2x%2.2x%2.2x%2.2x%2.2x\n",
              trk, sfx, l2.pos, rc,
              buf[0], buf[1], buf[2], buf[3], buf[4]);

    if (cckd_cchh (dev, buf, trk) < 0) goto cckd_read_trkimg_error;

    return rc;

cckd_read_trkimg_error:

    if (unitstat)
    {
        ckd_build_sense (dev, SENSE_EC, 0, 0, FORMAT_1, MESSAGE_0);
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
    }

    return cckd_null_trk (dev, buf, trk, 0);

} /* end function cckd_read_trkimg */


/*-------------------------------------------------------------------*/
/* Write a track image                                               */
/*-------------------------------------------------------------------*/
int cckd_write_trkimg (DEVBLK *dev, BYTE *buf, int len, int trk)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             rc;                     /* Return code               */
off_t           rcoff;                  /* lseek() return value      */
CCKD_L2ENT      l2, oldl2;              /* Level 2 entries           */
int             sfx,l1x,l2x;            /* Lookup table indices      */
int             after = 0;              /* 1=New track after old     */

    cckd = dev->cckd_ext;

    sfx = cckd->sfn;
    l1x = trk >> 8;
    l2x = trk & 0xff;

    cckdtrc ("cckddasd: file[%d] trk %d write trkimg len %d\n",
             sfx, trk, len);

    /* Validate the new track image */
    rc = cckd_cchh (dev, buf, trk);
    if (rc < 0)
    {
        devmsg ("%4.4X:cckddasd: file[%d] trk %d not written, invalid format\n",
                dev->devnum, sfx, trk);
        return -1;
    }

    /* get the level 2 entry for the track in the active file */
    rc = cckd_read_l2 (dev, sfx, l1x);
    memcpy (&oldl2, &cckd->l2[l2x], CCKD_L2ENT_SIZE);

    /* get offset and length for the track image */
    if (len == CCKD_NULLTRK_SIZE0 || len == CCKD_NULLTRK_SIZE1)
    {
        l2.pos = 0;
        l2.len = l2.size = (len == CCKD_NULLTRK_SIZE0);
    }
    else
    {
        l2.pos = cckd_get_space (dev, len);
        l2.len = l2.size = len;
        if (l2.pos == 0) return -1;
        if (oldl2.pos != 0 && oldl2.pos != 0xffffffff && oldl2.pos < l2.pos)
            after = 1;
    }

    /* write the track image */
    if (l2.pos)
    {
        rcoff = lseek (cckd->fd[sfx], (off_t)l2.pos, SEEK_SET);
        if (rcoff < 0)
        {
            devmsg ("%4.4X:cckddasd: file[%d] trk %d lseek error offset %llx: %s\n",
                    dev->devnum, sfx, trk, (long long)l2.pos, strerror(errno));
            return -1;
        }
        rc = write (cckd->fd[sfx], buf, len);
        if (rc < len)
        {
            devmsg ("%4.4X:cckddasd: file[%d] trk %d write error offset %llx len %d rc %d: %s\n",
                    dev->devnum, sfx, trk, (long long)l2.pos, len, rc, strerror(errno));
            return -1;
        }
        cckdtrc ("cckddasd: file[%d] trk %d written offset %llx len %d"
                 " %2.2x%2.2x%2.2x%2.2x%2.2x\n",
                 sfx, trk, (long long)l2.pos, len,
                 buf[0],buf[1],buf[2],buf[3],buf[4]);
        cckd->writes[sfx]++;
        cckd->totwrites++;
        cckdblk.stats_writes++;
        cckdblk.stats_writebytes += rc;
    }

    /* update the level 2 entry */
    rc = cckd_write_l2ent (dev, &l2, trk);
    if (rc < 0) return -1;

    /* release the previous space */
    cckd_rel_space (dev, (off_t)oldl2.pos, oldl2.len);

    cckdtrc ("cckddasd: file[%d] trk %d write complete offset 0x%llx len %d\n",
              sfx, trk, (long long)l2.pos, l2.len);

    return after;

} /* end function cckd_write_trkimg */


/*-------------------------------------------------------------------*/
/* Harden the file                                                   */
/*-------------------------------------------------------------------*/
int cckd_harden(DEVBLK *dev)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             rc, hrc=0;              /* Return codes              */

    cckd = dev->cckd_ext;
    if (dev->ckdrdonly) return 0;

    /* Write the compressed device header */
    rc = cckd_write_chdr (dev);
    if (rc < hrc) hrc = rc;

    /* Write the level 1 table */
    rc = cckd_write_l1 (dev);
    if (rc < hrc) hrc = rc;

    /* Write the free space chain */
    rc = cckd_write_fsp (dev);
    if (rc < hrc) hrc = rc;

    /* Re-write the compressed device header */
    cckd->cdevhdr[cckd->sfn].options &= ~CCKD_OPENED;
    rc = cckd_write_chdr (dev);
    if (rc < hrc) hrc = rc;

    if (cckdblk.fsync)
        rc = fdatasync (cckd->fd[cckd->sfn]);

    return hrc;
} /* cckd_harden */


/*-------------------------------------------------------------------*/
/* Return length of an uncompressed track image                      */
/*-------------------------------------------------------------------*/
int cckd_trklen (DEVBLK *dev, BYTE *buf)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             size;                   /* Track size                */

    cckd = dev->cckd_ext;

    if (cckd->fbadasd)
        return CKDDASD_TRKHDR_SIZE + CFBA_BLOCK_SIZE;

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
/* Truncate the file                                                 */
/*-------------------------------------------------------------------*/
void cckd_truncate (DEVBLK *dev, int now)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             rc;                     /* Return code               */
int             sfx;                    /* File index                */
struct stat     st;                     /* File status area          */
off_t           sz;                     /* Change for ftruncate      */

    cckd = dev->cckd_ext;
    sfx = cckd->sfn;

    /* FIXME - workaround for ftruncate() linux problem */
    if (cckdblk.ftruncwa) sz = 2 * 1024 * 1024;
    else sz = 0;

    rc = fstat (cckd->fd[sfx], &st);
    if (rc < 0)
    {
        cckdmsg ("%4.4X:cckddasd truncate fstat error: %s\n",
                dev->devnum, strerror(errno));
        return;
    }

    cckdtrc("cckddasd: truncate st_size=%lld, chdr_size=%d\n",
            (long long)st.st_size, cckd->cdevhdr[sfx].size);

    if (now || (off_t)(st.st_size - cckd->cdevhdr[sfx].size) > sz)
    {
        /* FIXME - workaround for ftruncate() linux problem */
        if (cckdblk.ftruncwa)
        {
            BYTE sfn[1024];
            fsync (cckd->fd[sfx]);
            close (cckd->fd[sfx]);
            usleep(10000 * cckdblk.ftruncwa);
            cckd_sf_name (dev, sfx, (char *)&sfn);
            cckd->fd[sfx] = open (sfn, O_RDWR|O_BINARY);
            if (cckd->fd[sfx] < 0)
            {
                devmsg ("%4.4X:cckddasd truncate re-open error: %s\n",
                        dev->devnum, strerror(errno));
                return;
            }
        }

        rc = ftruncate (cckd->fd[sfx], (off_t)cckd->cdevhdr[sfx].size);
        if (rc < 0)
        {
            devmsg ("%4.4X:cckddasd truncate ftruncate error: %s\n",
                    dev->devnum, strerror(errno));
            return;
        }
    }

} /* end function cckd_truncate */


/*-------------------------------------------------------------------*/
/* Build a null track                                                */
/*-------------------------------------------------------------------*/
int cckd_null_trk(DEVBLK *dev, BYTE *buf, int trk, int sz0)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
U16             cyl;                    /* Cylinder                  */
U16             head;                   /* Head                      */
FWORD           cchh;                   /* Cyl, head big-endian      */
int             size;                   /* Size of null record       */

    cckd = dev->cckd_ext;

    cckdtrc ("cckddasd: null_trk trk %d\n", trk);

    if (cckd->ckddasd)
    {
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
        if (sz0)
        {
            memcpy (&buf[21], eighthexFF, 8);
            size = CCKD_NULLTRK_SIZE0;
        }
        else
        {
            memcpy (&buf[21], cchh, sizeof(cchh));
            buf[25] = 1;
            memcpy (&buf[29], eighthexFF, 8);
            size = CCKD_NULLTRK_SIZE1;
        }
    }
    else
    {
        memset (buf, 0, CFBA_BLOCK_SIZE + CKDDASD_TRKHDR_SIZE);
        buf[1] = (trk >> 24) & 0xff;
        buf[2] = (trk >> 16) & 0xff;
        buf[3] = (trk >>  8) & 0xff;
        buf[4] = trk & 0xff;
        size = CFBA_BLOCK_SIZE + CKDDASD_TRKHDR_SIZE;
    }

    return size;

} /* end function cckd_null_trk */


/*-------------------------------------------------------------------*/
/* Verify a track/block header and return track/block number         */
/*-------------------------------------------------------------------*/
int cckd_cchh (DEVBLK *dev, BYTE *buf, int trk)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
U16             cyl;                    /* Cylinder                  */
U16             head;                   /* Head                      */
int             t;                      /* Calculated track          */

    cckd = dev->cckd_ext;

    /* CKD dasd header verification */
    if (cckd->ckddasd)
    {
        cyl = (buf[1] << 8) + buf[2];
        head = (buf[3] << 8) + buf[4];
        t = cyl * dev->ckdheads + head;

        if (buf[0] <= CCKD_COMPRESS_MAX
         && cyl < dev->ckdcyls
         && head < dev->ckdheads
         && (trk == -1 || t == trk))
            return t;
    }
    /* FBA dasd header verification */
    else
    {
        t = (buf[1] << 24) + (buf[2] << 16) + (buf[3] << 8) + buf[4];
        if (buf[0] <= CCKD_COMPRESS_MAX
         && t < dev->fbanumblk
         && (trk == -1 || t == trk))
            return t;
    }

    devmsg ("%4.4X:cckddasd: invalid %s hdr %s %d "
            "buf %2.2x%2.2x%2.2x%2.2x%2.2x\n",
            dev->devnum, cckd->ckddasd ? "trk" : "blk",
            cckd->ckddasd ? "trk" : "blk", trk,
            buf[0], buf[1], buf[2], buf[3], buf[4]);

    cckd_print_itrace ();

    return -1;
} /* end function cckd_cchh */

/*-------------------------------------------------------------------*/
/* Validate a track image                                            */
/*-------------------------------------------------------------------*/
int cckd_validate (DEVBLK *dev, BYTE *buf, int trk, int len)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             cyl;                    /* Cylinder                  */
int             head;                   /* Head                      */
char            cchh[4],cchh2[4];       /* Cyl, head big-endian      */
int             r;                      /* Record number             */
int             sz;                     /* Track size                */
int             kl,dl;                  /* Key/Data lengths          */

    cckd = dev->cckd_ext;

    cckdtrc ("cckddasd: validating %s %d len %d %2.2x%2.2x%2.2x%2.2x%2.2x "
             "%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x\n",
             cckd->ckddasd ? "trk" : "blkgrp", trk, len,
             buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6],
             buf[7], buf[8], buf[9], buf[10], buf[11], buf[12]);

    /* FBA dasd check */
    if (cckd->fbadasd)
    {
        if (len == CFBA_BLOCK_SIZE + CKDDASD_TRKHDR_SIZE) return 0;
        cckdtrc ("cckddasd: validation failed: bad length%s\n","");
        return -1;
    }

    /* cylinder and head calculations */
    cyl = trk / dev->ckdheads;
    head = trk % dev->ckdheads;
    cchh[0] = cyl >> 8;
    cchh[1] = cyl & 0xFF;
    cchh[2] = head >> 8;
    cchh[3] = head & 0xFF;

    /* validate record 0 */
    memcpy (cchh2, &buf[5], 4); cchh2[0] &= 0x7f; /* fix for ovflow */
    if (memcmp (cchh, cchh2, 4) != 0 || buf[9]  != 0 ||
        buf[10] != 0 || buf[11] != 0 || buf[12] != 8)
    {
        cckdtrc ("cckddasd: validation failed: bad r0%s\n","");
        return -1;
    }

    /* validate records 1 thru n */
    for (r = 1, sz = 21; sz + 8 <= len; sz += 8 + kl + dl, r++)
    {
        if (memcmp (&buf[sz], eighthexFF, 8) == 0) break;
        kl = buf[sz+5];
        dl = buf[sz+6] * 256 + buf[sz+7];
        /* fix for track overflow bit */
        memcpy (cchh2, &buf[sz], 4); cchh2[0] &= 0x7f;

        /* fix for funny formatted vm disks */
        if (r == 1) memcpy (cchh, cchh2, 4);

        if (memcmp (cchh, cchh2, 4) != 0 || buf[sz+4] == 0 ||
            sz + 8 + kl + dl >= len)
        {
            cckdtrc ("cckddasd: validation failed: bad r%d "
                 "%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x\n",
                 r, buf[sz], buf[sz+1], buf[sz+2], buf[sz+3],
                 buf[sz+4], buf[sz+5], buf[sz+6], buf[sz+7]);
             return -1;
        }
    }
    sz += 8;

    if (sz != len)
    {
        cckdtrc ("cckddasd: validation failed: no eot%s\n","");
        return -1;
    }

    return 0;

} /* end function cckd_validate */



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
    if (dev->dasdsfn[0] == '\0')
    {
        devmsg ("%4.4X:cckddasd: no shadow file name specified\n",
                dev->devnum);
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
    strcpy (sfn, (const char *)&dev->dasdsfn);
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
    if (dev->dasdsfn[0] == '\0') return 0;

#if 1
    /* Check for shadow file name collision */
    for (i = 1; i <= CCKD_MAX_SF; i++)
    {
     DEVBLK       *dev2;
     CCKDDASD_EXT *cckd2;
     char          sfn2[256];
     int           j;

        rc = cckd_sf_name (dev, i, (char *)&sfn);
        if (rc < 0) continue;
        for (dev2 = cckdblk.dev1st; dev2; dev2 = cckd2->devnext)
        {
            cckd2 = dev2->cckd_ext;
            if (dev2 == dev) continue;
            for (j = 0; j <= CCKD_MAX_SF; j++)
            {
                if (j > 0 && dev2->dasdsfn[0] == '\0') break;
                rc = cckd_sf_name (dev2, j, (char *)&sfn2);
                if (rc < 0) continue;
                if (strcmp ((char *)&sfn, (char *)&sfn2) == 0)
                {
                    devmsg ("%4.4X:cckddasd shadow file[%d] name %s\n"
                            "      collides with %4.4X file[%d] name %s\n",
                            dev->devnum, i, sfn, dev2->devnum, j, sfn2);
                    return -1;
                }
            }
        }
    }
#endif

    /* open all existing shadow files */
    for (cckd->sfn = 1; cckd->sfn <= CCKD_MAX_SF; cckd->sfn++)
    {
        /* get the shadow file name */
        rc = cckd_sf_name (dev, cckd->sfn, (char *)&sfn);
        if (rc < 0) return -1;

        /* try to open the shadow file read-write then read-only */
        if (!dev->ckdrdonly)
            cckd->fd[cckd->sfn] = open (sfn, O_RDWR|O_BINARY);
        if (dev->ckdrdonly || cckd->fd[cckd->sfn] < 0)
        {
            cckd->fd[cckd->sfn] = open (sfn, O_RDONLY|O_BINARY);
            if (cckd->fd[cckd->sfn] < 0) break;
            cckd->open[cckd->sfn] = CCKD_OPEN_RO;
        }
        else cckd->open[cckd->sfn] = CCKD_OPEN_RW;

        /* Call the chkdsk function */
        rc = cckd_chkdsk (cckd->fd[cckd->sfn], dev->msgpipew, 0);
        if (rc < 0) return -1;

        /* Perform initial read */
        rc = cckd_read_init (dev);
    }

    /* Backup to the last opened file number */
    cckd->sfn--;

    /* If the last file was opened read-only then create a new one   */
    if (cckd->open[cckd->sfn] == CCKD_OPEN_RO && !dev->ckdrdonly)
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
int             rc;                     /* Return code               */
off_t           rcoff;                  /* lseek() return value      */
CKDDASD_DEVHDR  devhdr;                 /* Device header             */
BYTE            sfn[256];               /* Shadow file name          */
int             sfx;                    /* Shadow file index         */
int             sfd;                    /* Shadow file descriptor    */
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
    {    devmsg ("%4.4X:cckddasd: shadow file[%d] open error: %s\n",
                 dev->devnum, sfx, strerror (errno));
         return -1;
    }

    /* build the device header */
    rcoff = lseek (cckd->fd[sfx-1], 0, SEEK_SET);
    if (rcoff < 0)
    {
        devmsg ("%4.4X:cckddasd: file[%d] lseek error offset %d: %s\n",
                dev->devnum, sfx-1, 0, strerror(errno));
        close (sfd);
        return -1;
    }
    rc = read (cckd->fd[sfx-1], &devhdr, CKDDASD_DEVHDR_SIZE);
    if (rc < CKDDASD_DEVHDR_SIZE)
    {
        devmsg ("%4.4X:cckddasd: file[%d] read error offset %d: %s\n",
                dev->devnum, sfx-1, 0, strerror(errno));
        close (sfd);
        return -1;
    }
    if (cckd->ckddasd) memcpy (&devhdr.devid, "CKD_S370", 8);
    else memcpy (&devhdr.devid, "FBA_S370", 8);
    rc = write (sfd, &devhdr, CKDDASD_DEVHDR_SIZE);
    if (rc < CKDDASD_DEVHDR_SIZE)
    {
        devmsg ("%4.4X:cckddasd: shadow file[%d] write error offset %d: %s\n",
                dev->devnum, sfx, 0, strerror(errno));
        close (sfd);
        return -1;
    }

    /* build the compressed device header */
    memcpy (&cckd->cdevhdr[sfx], &cckd->cdevhdr[sfx-1], CCKDDASD_DEVHDR_SIZE);
    memset (&cckd->cdevhdr[sfx].CCKD_FREEHDR, 0, CCKD_FREEHDR_SIZE);
    l1size = cckd->cdevhdr[sfx].numl1tab * CCKD_L1ENT_SIZE;
    cckd->cdevhdr[sfx].size = cckd->cdevhdr[sfx].used = 
          CKDDASD_DEVHDR_SIZE + CCKDDASD_DEVHDR_SIZE + l1size;

    /* Init the level 1 table */
    cckd->l1[sfx] = malloc (l1size);
    if (!cckd->l1[sfx])
    {
        devmsg ("%4.4X:cckddasd file[%d] l1 malloc failed: %s\n",
                dev->devnum, sfx, strerror(errno));
        close (sfd);
        return -1;
    }
    memset (cckd->l1[sfx], 0xff, l1size);

    /* Make the new file active */
    cckd->sfn = sfx;
    cckd->fd[sfx] = sfd;
    cckd->open[sfx] = CCKD_OPEN_RW;

    /* Harden the file */
    rc = cckd_harden (dev);
    if (rc < 0)
    {
        cckd->sfn--;
        free (cckd->l1[sfx]);
        cckd->l1[sfx] = NULL;
        close (sfd);
        return -1;
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
        devmsg ("%4.4X:cckddasd: not a cckd device\n", dev->devnum);
        return;
    }

    /* schedule updated track entries to be written */
    obtain_lock (&cckdblk.cachelock);
    cckd_flush_cache (dev);
    while (cckdblk.writepending || cckd->ioactive)
    {
        cckdblk.writewaiting++;
        wait_condition (&cckdblk.writecond, &cckdblk.cachelock);
        cckdblk.writewaiting--;
        cckd_flush_cache (dev);
    }
    cckd_purge_cache (dev); cckd_purge_l2 (dev);
    dev->dasdcur = -1; cckd->active = NULL;
    cckd->merging = 1;
    release_lock (&cckdblk.cachelock);

    /* obtain control of the file */
    obtain_lock (&cckd->filelock);

    /* harden the current file */
    cckd_harden (dev);

    /* create a new shadow file */
    rc = cckd_sf_new (dev);
    if (rc < 0)
    {
        devmsg ("%4.4X:cckddasd: file[%d] error adding shadow file: %s\n",
                dev->devnum, cckd->sfn  + 1, strerror(errno));
        release_lock (&cckd->filelock);
        obtain_lock (&cckdblk.cachelock);
        cckd->merging = 0;
        if (cckdblk.writewaiting)
            broadcast_condition (&cckdblk.writecond);
        release_lock (&cckdblk.cachelock);
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
    devmsg ("%4.4X:cckddasd: file[%d] %s added\n",dev->devnum, cckd->sfn, sfn);
    release_lock (&cckd->filelock);

    obtain_lock (&cckdblk.cachelock);
    cckd->merging = 0;
    if (cckdblk.writewaiting)
        broadcast_condition (&cckdblk.writecond);
    release_lock (&cckdblk.cachelock);

    cckd_sf_stats (dev);

} /* end function cckd_sf_add */


/*-------------------------------------------------------------------*/
/* Remove a shadow file  (sf-)                                       */
/*-------------------------------------------------------------------*/
void cckd_sf_remove (DEVBLK *dev, int merge)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             rc;                     /* Return code               */
off_t           rcoff;                  /* lseek() return value      */
int             add = 0;                /* 1=Add shadow file back    */
int             err = 0;                /* 1=I/O error occurred      */
int             l2updated;              /* 1=L2 table was updated    */
int             i,j;                    /* Loop indexes              */
off_t           pos;                    /* File offset               */
int             sfx[2];                 /* Shadow file indexes       */
BYTE            sfn[256];               /* Shadow file name          */
CCKD_L2ENT      l2[2][256];             /* Level 2 tables            */
BYTE            buf[65536];             /* Buffer                    */

    cckd = dev->cckd_ext;
    if (!cckd)
    {
        devmsg ("%4.4X:cckddasd: not a cckd device\n",dev->devnum);
        return;
    }

    if (!cckd->sfn)
    {
        devmsg ("%4.4X:cckddasd: cannot remove base file\n",dev->devnum);
        return;
    }

    /* Schedule updated track entries to be written */
    obtain_lock (&cckdblk.cachelock);
    cckd_flush_cache (dev);
    while (cckdblk.writepending || cckd->ioactive)
    {
        cckdblk.writewaiting++;
        wait_condition (&cckdblk.writecond, &cckdblk.cachelock);
        cckdblk.writewaiting--;
        cckd_flush_cache (dev);
    }
    cckd_purge_cache (dev); cckd_purge_l2 (dev);
    dev->dasdcur = -1; cckd->active = NULL;
    cckd->merging = 1;
    release_lock (&cckdblk.cachelock);

    obtain_lock (&cckd->filelock);
    sfx[0] = cckd->sfn;
    sfx[1] = cckd->sfn - 1;

    /* Attempt to re-open the `to' file read-write */
    close (cckd->fd[sfx[1]]);
    cckd_sf_name (dev, sfx[1], (char *)&sfn);
    cckd->fd[sfx[1]] = open (sfn, O_RDWR|O_BINARY);
    if (cckd->fd[sfx[1]] < 0)
    {
        cckd->fd[sfx[1]] = open (sfn, O_RDONLY|O_BINARY);
        if (merge)
        {
            devmsg ("%4.4X:cckddasd: file[%d] not merged, "
                    "file[%d] cannot be opened read-write\n",
                    dev->devnum, sfx[0], sfx[1]);
            goto sf_remove_exit;
        }
        else add = 1;
    }
    else
    {
        rc = cckd_chkdsk (cckd->fd[sfx[1]], dev->msgpipew, 0);
        if (rc < 0)
        {
            devmsg ("%4.4X:cckddasd: file[%d] not merged, "
                    "file[%d] check failed\n",
                    dev->devnum, sfx[0], sfx[1]);
            goto sf_remove_exit;
        }
        cckd->open[sfx[1]] = CCKD_OPEN_RW;
    }

    /* Harden the current file */
    if (merge)
    {
        rc = cckd_harden (dev);
        if (rc < 0)
        {
            devmsg ("%4.4X:cckddasd: file[%d] not merged, "
                    "file[%d] not hardened\n",
                    dev->devnum, sfx[0], sfx[0]);
            goto sf_remove_exit;
        }
    }

    /* make the previous file active */
    cckd->sfn--;

    /* perform backwards merge */
    if (merge)
    {
        cckdtrc ("cckddasd: merging to file[%d] %s\n", sfx[1], sfn);
        cckd->cdevhdr[sfx[1]].options |= (CCKD_OPENED | CCKD_ORDWR);

        /* Loop for each level 1 table entry */
        for (i = 0; i < cckd->cdevhdr[sfx[0]].numl1tab; i++)
        {
            cckdtrc ("cckddasd: merging l1[%d]: from %llx, to %llx\n",
                     i, (long long)cckd->l1[sfx[0]][i],
                     (long long)cckd->l1[sfx[1]][i]);

            if (cckd->l1[sfx[0]][i] == 0xffffffff
             || (cckd->l1[sfx[0]][i] == 0 && cckd->l1[sfx[1]] == 0))
                continue;

            /* Read `from' l2 table */
            if (cckd->l1[sfx[0]][i] == 0)
                memset (&l2[0], 0, CCKD_L2TAB_SIZE);
            else if (cckd->l1[sfx[0]][i] == 0xffffffff)
                memset (&l2[0], 0xff, CCKD_L2TAB_SIZE);
            else
            {
                rcoff = lseek (cckd->fd[sfx[0]], (off_t)cckd->l1[sfx[0]][i], SEEK_SET);
                if (rcoff < 0)
                {
                    devmsg ("%4.4X:cckddasd: file[%d] l2[%d] lseek error offset %lld: %s\n",
                            dev->devnum, sfx[0], i, (long long)cckd->l1[sfx[0]][i],
                            strerror(errno));
                    err = 1;
                    continue;
                }
                rc = read (cckd->fd[sfx[0]], &l2[0], CCKD_L2TAB_SIZE);
                if (rc < CCKD_L2TAB_SIZE)
                {
                    devmsg ("%4.4X:cckddasd: file[%d] l2[%d] read error offset %lld: %s\n",
                            dev->devnum, sfx[0], i, (long long)cckd->l1[sfx[0]][i],
                            strerror(errno));
                    err = 1;
                    continue;
                }
            }

            /* Read `to' l2 table */
            if (cckd->l1[sfx[1]][i] == 0)
                memset (&l2[1], 0, CCKD_L2TAB_SIZE);
            else if (cckd->l1[sfx[1]][i] == 0xffffffff)
                memset (&l2[1], 0xff, CCKD_L2TAB_SIZE);
            else
            {
                rcoff = lseek (cckd->fd[sfx[1]], (off_t)cckd->l1[sfx[1]][i], SEEK_SET);
                if (rcoff < 0)
                {
                    devmsg ("%4.4X:cckddasd: file[%d] l2[%d] lseek error offset %lld: %s\n",
                            dev->devnum, sfx[1], i, (long long)cckd->l1[sfx[1]][i],
                            strerror(errno));
                    err = 1;
                    continue;
                }
                rc = read (cckd->fd[sfx[1]], &l2[1], CCKD_L2TAB_SIZE);
                if (rc < CCKD_L2TAB_SIZE)
                {
                    devmsg ("%4.4X:cckddasd: file[%d] l2[%d] read error offset %lld: %s\n",
                            dev->devnum, sfx[1], i, (long long)cckd->l1[sfx[1]][i],
                            strerror(errno));
                    err = 1;
                    continue;
                }
            }

            /* Loop for each level 2 table entry */
            l2updated = 0;
            for (j = 0; j < 256; j++)
            {
                cckdtrc ("cckddasd: merging l2[%d]: from %llx:%d, to %llx:%d\n",
                         j, (long long)l2[0][j].pos, (int)l2[0][j].len,
                         (long long)l2[1][j].pos, (int)l2[1][j].len);

                if (l2[0][j].pos == 0xffffffff
                 || (l2[0][j].pos == 0 && l2[1][j].pos == 0))
                    continue;

                /* Read the `from' track/blkgrp image*/
                if (l2[0][j].len > 1)
                {
                    rcoff = lseek (cckd->fd[sfx[0]], (off_t)l2[0][j].pos, SEEK_SET);
                    if (rcoff < 0)
                    {
                        devmsg ("%4.4X:cckddasd: file[%d] %s[%d] lseek error "
                                " offset %lld: %s\n",
                                dev->devnum, sfx[0], cckd->ckddasd ? "trk" : "blkgrp",
                                i * 256 + j, (long long)l2[0][j].pos,
                                strerror(errno));
                        devmsg ("%4.4X:cckddasd: file[%d] %s[%d] not merged\n",
                                dev->devnum, sfx[0], cckd->ckddasd ? "trk" : "blkgrp",
                                i * 256 + j);
                        err = 1;
                        continue;
                    }
                    rc = read (cckd->fd[sfx[0]], &buf, (size_t)l2[0][j].len);
                    if (rc != (int)l2[0][j].len)
                    {
                        devmsg ("%4.4X:cckddasd: file[%d] %s[%d] read error "
                                " offset %lld: %s\n",
                                dev->devnum, sfx[0], cckd->ckddasd ? "trk" : "blkgrp",
                                i * 256 + j, (long long)l2[0][j].pos,
                                strerror(errno));
                        devmsg ("%4.4X:cckddasd: file[%d] %s[%d] not merged\n",
                                dev->devnum, sfx[0], cckd->ckddasd ? "trk" : "blkgrp",
                                i * 256 + j);
                        err = 1;
                        continue;
                    }
                }

                /* Get space for new `to' entry */
                pos = cckd_get_space (dev, (int)l2[0][j].len);

                /* Write the `to' track/blkgrp image */
                if (l2[0][j].len > 1)
                {
                    cckdtrc ("cckddasd: merging trk[%d] to %llx\n",
                         i * 256 + j, (long long)pos);

                    rcoff = lseek (cckd->fd[sfx[1]], pos, SEEK_SET);
                    if (rcoff < 0)
                    {
                        devmsg ("%4.4X:cckddasd: file[%d] %s[%d] lseek error "
                                " offset %lld: %s\n",
                                dev->devnum, sfx[1], cckd->ckddasd ? "trk" : "blkgrp",
                                i * 256 + j, (long long)pos, strerror(errno));
                        devmsg ("%4.4X:cckddasd: file[%d] %s[%d] not merged\n",
                                dev->devnum, sfx[0], cckd->ckddasd ? "trk" : "blkgrp",
                                i * 256 + j);
                        err = 1;
                        continue;
                    }
                    rc = write (cckd->fd[sfx[1]], &buf, (size_t)l2[0][j].len);
                    if (rc != (int)l2[0][j].len)
                    {
                        devmsg ("%4.4X:cckddasd: file[%d] %s[%d] write error "
                                " offset %lld: %s\n",
                                dev->devnum, sfx[1], cckd->ckddasd ? "trk" : "blkgrp",
                                i * 256, (long long)pos, strerror(errno));
                        devmsg ("%4.4X:cckddasd: file[%d] %s[%d] not merged\n",
                                dev->devnum, sfx[0], cckd->ckddasd ? "trk" : "blkgrp",
                                i * 256 + j);
                        err = 1;
                        continue;
                    }
                }

                /* Release space occupied by old `to' entry */
                cckd_rel_space (dev, (off_t)l2[1][j].pos, (int)l2[1][j].len);

                /* Update `to' l2 table entry */
                l2updated = 1;
                l2[1][j].pos = (U32)pos;
                l2[1][j].len = l2[1][j].size = l2[0][j].len;
            } /* for each level 2 table entry */

            /* Update the `to' level 2 table */
            if (l2updated)
            {
                if (memcmp (&l2[1], &cckd_empty_l2tab, CCKD_L2TAB_SIZE) == 0)
                {
                    cckd_rel_space (dev, (off_t)cckd->l1[sfx[1]][i], CCKD_L2TAB_SIZE);
                    cckd->l1[sfx[1]][i] = 0;
                }
                else
                {
                    pos = (off_t)cckd->l1[sfx[1]][i];
                    if (pos == 0 || pos == 0xffffffff)
                        pos = cckd_get_space (dev, CCKD_L2TAB_SIZE);

                    cckdtrc ("cckddasd: merging l2[%d] to %llx\n",
                             i, (long long)pos);

                    rcoff = lseek (cckd->fd[sfx[1]], pos, SEEK_SET);
                    if (rcoff < 0)
                    {
                        devmsg ("%4.4X:cckddasd: file[%d] l1[%d] lseek error "
                                " offset %lld: %s\n",
                                dev->devnum, sfx[1], i, (long long)pos,
                                strerror(errno));
                        devmsg ("%4.4X:cckddasd: file[%d] %s[%d-%d] not merged\n",
                                dev->devnum, sfx[0], cckd->ckddasd ? "trks" : "blkgrps",
                                i * 256, i * 256 +255);
                        err = 1;
                        continue;
                    }
                    rc = write (cckd->fd[sfx[1]], &l2[1], CCKD_L2TAB_SIZE);
                    if (rc != CCKD_L2TAB_SIZE)
                    {
                        devmsg ("%4.4X:cckddasd: file[%d] l1[%d] write error "
                                " offset %lld: %s\n",
                                dev->devnum, sfx[1], i, (long long)pos,
                                strerror(errno));
                        devmsg ("%4.4X:cckddasd: file[%d] %s[%d-%d] not merged\n",
                                dev->devnum, sfx[0], cckd->ckddasd ? "trks" : "blkgrps",
                                i * 256, i * 256 + 255);
                        err = 1;
                        continue;
                    }
                    cckd->l1[sfx[1]][i] = (U32)pos;
                } /* `to' level 2 table not null */
            } /* Update level 2 table */
        } /* for each level 1 table entry */

        /* Validate the merge */
        cckd_harden (dev);
        if (err)
            cckd_chkdsk (cckd->fd[sfx[1]], dev->msgpipew, 2);
        cckd_read_init (dev);

    } /* if merge */

    /* Remove the old file */
    close (cckd->fd[sfx[0]]);
    free (cckd->l1[sfx[0]]);
    cckd->l1[sfx[0]] = NULL;
    memset (&cckd->cdevhdr[sfx[0]], 0, CCKDDASD_DEVHDR_SIZE); 
    cckd_sf_name (dev, sfx[0], (char *)&sfn);
    rc = unlink ((char *)&sfn);

    /* Add the file back if necessary */
    if (add) rc = cckd_sf_new (dev) ;

    devmsg ("cckddasd: shadow file [%d] successfully %s%s\n",
            sfx[0], merge ? "merged" : add ? "re-added" : "removed",
            err ? " with errors" : "");

sf_remove_exit:
    release_lock (&cckd->filelock);

    obtain_lock (&cckdblk.cachelock);
    cckd->merging = 0;
    if (cckdblk.writewaiting)
        broadcast_condition (&cckdblk.writecond);
    release_lock (&cckdblk.cachelock);

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

    strcpy ((char *)&dev->dasdsfn, (const char *)sfn);
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
    obtain_lock (&cckdblk.cachelock);
    cckd_flush_cache (dev);
    while (cckdblk.writepending || cckd->ioactive)
    {
        cckdblk.writewaiting++;
        wait_condition (&cckdblk.writecond, &cckdblk.cachelock);
        cckdblk.writewaiting--;
        cckd_flush_cache (dev);
    }
    cckd_purge_cache (dev); cckd_purge_l2 (dev);
    dev->dasdcur = -1; cckd->active = NULL;
    cckd->merging = 1;
    release_lock (&cckdblk.cachelock);

    /* obtain control of the file */
    obtain_lock (&cckd->filelock);

    /* harden the current file */
    cckd_harden (dev);

    /* Call the compress function */
    rc = cckd_comp (cckd->fd[cckd->sfn], dev->msgpipew);

    /* Perform initial read */
    rc = cckd_read_init (dev);

    release_lock (&cckd->filelock);

    obtain_lock (&cckdblk.cachelock);
    cckd->merging = 0;
    if (cckdblk.writewaiting)
        broadcast_condition (&cckdblk.writecond);
    release_lock (&cckdblk.cachelock);

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

//  obtain_lock (&cckd->filelock);

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

    if (dev->dasdsfn[0] && CCKD_MAX_SF > 0)
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
//  release_lock (&cckd->filelock);
} /* end function cckd_sf_stats */


/*-------------------------------------------------------------------*/
/* Garbage Collection thread                                         */
/*-------------------------------------------------------------------*/
void cckd_gcol()
{
int             gcol;                   /* Identifier                */
int             rc;                     /* Return code               */
DEVBLK         *dev;                    /* -> device block           */
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
long long       size, free;             /* File size, free           */
struct timeval  now;                    /* Time-of-day               */
struct timespec tm;                     /* Time-of-day to wait       */
int             gc;                     /* Garbage collection state  */
int             gctab[5]= {             /* default gcol parameters   */
                           4096,        /* critical  50%   - 100%    */
                           2048,        /* severe    25%   -  50%    */
                           1024,        /* moderate  12.5% -  25%    */
                            512,        /* light      6.3% -  12.5%  */
                            256};       /* none       0%   -   6.3%  */
//char *gcstates[] = {"critical","severe","moderate","light","none"}; 

    obtain_lock (&cckdblk.gclock);
    gcol = ++cckdblk.gcols;
    
    /* Return without messages if too many already started */
    if (gcol > cckdblk.gcolmax)
    {
        --cckdblk.gcols;
        release_lock (&cckdblk.gclock);
        return;
    }

    if (!cckdblk.batch)
    cckdmsg ("cckddasd: garbage collector thread started: tid="TIDPAT" pid=%d \n",
              thread_id(), getpid());

    while (gcol <= cckdblk.gcolmax)
    {
        /* Perform collection on each device */
        for (dev = cckdblk.dev1st; dev; dev = cckd->devnext)
        {
            cckd = dev->cckd_ext;
            if (!cckd || cckd->stopping || cckd->merging)
                continue;

            if (!(cckd->cdevhdr[cckd->sfn].options & CCKD_OPENED))
            {
                if (cckd->updated)
                {
                    obtain_lock (&cckdblk.cachelock);
                    cckd_flush_cache (dev);
                    release_lock (&cckdblk.cachelock);
                }
                continue;
            }

            /* Determine garbage state */
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
            if (cckd->cdevhdr[cckd->sfn].free_number > 3000)           gc = 0;

            /* Set the size */
            if (cckdblk.gcolparm > 0) size = gctab[gc] << cckdblk.gcolparm;
            else if (cckdblk.gcolparm < 0) size = gctab[gc] >> abs(cckdblk.gcolparm);
            else size = gctab[gc];
            if (size > cckd->cdevhdr[cckd->sfn].used >> 10)
                size = cckd->cdevhdr[cckd->sfn].used >> 10;
            if (size < 64) size = 64;

            /* Call the garbage collector */
            rc = cckd_gc_percolate (dev, size);

            /* Schedule any updated tracks to be written */
            obtain_lock (&cckdblk.cachelock);
            cckd_flush_cache (dev);

            /* Wait for pending writes to complete */
            while (cckdblk.writepending || cckd->ioactive)
            {
                cckdblk.writewaiting++;
                wait_condition (&cckdblk.writecond, &cckdblk.cachelock);
                cckdblk.writewaiting--;
                cckd_flush_cache (dev);
            }
            release_lock (&cckdblk.cachelock);

            /* Sync the file */
            if (cckdblk.gcolwait >= 5 || cckd->lastsync + 5 <= now.tv_sec)
            {
                obtain_lock (&cckd->filelock);
                if (cckdblk.fsync)
                    rc = fdatasync (cckd->fd[cckd->sfn]);
                cckd_flush_space (dev);
                cckd_truncate (dev, 0);
                release_lock (&cckd->filelock);
                cckd->lastsync = now.tv_sec;
            }

        } /* for each cckd device */

        /* wait a bit */
        gettimeofday (&now, NULL);
        tm.tv_sec = now.tv_sec + cckdblk.gcolwait;
        tm.tv_nsec = now.tv_usec * 1000;
        cckdtrc ("cckddasd: gcol wait %d seconds at %s",
                 cckdblk.gcolwait, ctime (&now.tv_sec));
        timed_wait_condition (&cckdblk.gccond, &cckdblk.gclock, &tm);
    }

    if (!cckdblk.batch)
    cckdmsg ("cckddasd: garbage collector thread stopping: tid="TIDPAT", pid=%d\n",
            thread_id(), getpid());

    cckdblk.gcols--;
    if (!cckdblk.gcols) signal_condition (&cckdblk.termcond);
    release_lock (&cckdblk.gclock);
} /* end thread cckd_gcol */


/*-------------------------------------------------------------------*/
/* Garbage Collection -- Percolate algorithm                         */
/*                                                                   */
/* A kinder gentler algorithm                                        */
/*                                                                   */
/*-------------------------------------------------------------------*/
int cckd_gc_percolate(DEVBLK *dev, unsigned int size)
{
int             rc;                     /* Return code               */
off_t           rcoff;                  /* lseek() return value      */
int             i;                      /* Loop Index                */
int             fd;                     /* Current file descriptor   */
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             sfx,l1x,l2x;            /* Table Indexes             */
int             trk;                    /* Track number              */
off_t           fpos, bpos, upos;       /* File offsets              */
unsigned int    blen, ulen, len = 0;    /* Lengths                   */
int             b, u;                   /* Space indexes             */
int             moved = 0;              /* Space moved               */
CCKD_L2ENT      l2;                     /* Copied level 2 entry      */
int             after = 0;              /* New trk after old trk     */
BYTE            buf[65536];             /* Buffer                    */
int             asw;                    /* New spc after current spc */

    cckd = dev->cckd_ext;
    sfx = cckd->sfn;
    size = size << 10;

    cckdtrc ("cckddasd: gcperc size %d 1st 0x%x nbr %d largest %u\n",
             size, cckd->cdevhdr[sfx].free, cckd->cdevhdr[sfx].free_number,
             cckd->cdevhdr[sfx].free_largest);
    if (cckdblk.itracen)
    {
        fpos = (off_t)cckd->cdevhdr[sfx].free;
        for (i = cckd->free1st; i >= 0; i = cckd->free[i].next)
        {
            cckdtrc ("cckddasd: gcperc free[%4d]:%8.8x end %8.8x len %10d%cpend %d\n",
             i,(int)fpos,(int)(fpos+cckd->free[i].len),(int)cckd->free[i].len,
             fpos+(int)cckd->free[i].len == (int)cckd->free[i].pos ? '*' : ' ',cckd->free[i].pending);
            fpos = cckd->free[i].pos;
        }
    }

    while (moved + len < size && after < 3)
    {
        /* get the file lock */
        obtain_lock (&cckd->filelock);
        sfx = cckd->sfn;
        fd = cckd->fd[sfx];

        /* Exit if no more free space */
        if (cckd->cdevhdr[sfx].free_total == 0)
        {
            release_lock (&cckd->filelock);
            return moved;
        }

        /* Make sure the free space chain is built */
        if (!cckd->free) cckd_read_fsp (dev);

        /* Find a space to start with ...

           We find the first used space after the first non-pending
           free space unless `after' is non-zero.  `after' is a count
           of how many times space was obtained for an image where the
           new offset is after the current offset.  If `after' is
           non-zero then we find the first used space after the largest
           free space.  We do allow after-type allocations, but we
           try to limit them.

           This algorithm is subject to change ;-)  */

        upos = ulen = len = 0;
        fpos = cckd->cdevhdr[sfx].free;
        for (i = cckd->free1st; i >= 0; i = cckd->free[i].next)
        {
            if (!cckd->free[i].pending) break;
            fpos = cckd->free[i].pos;
        }
        for ( ; i >= 0 && after > 0; i = cckd->free[i].next)
        {
            if (cckd->free[i].len == cckd->cdevhdr[sfx].free_largest)
                break;
            fpos = cckd->free[i].pos;
        }
        for ( ; i >= 0; i = cckd->free[i].next)
        {
            if (fpos + cckd->free[i].len != cckd->free[i].pos) break;
            fpos = cckd->free[i].pos;
        }

        /* Calculate the offset/length of the used space.
           If only embedded free space is left, then start
           with the first used space */
        if (i >= 0)
        {
            upos = fpos + cckd->free[i].len;
            ulen = (cckd->free[i].pos ? cckd->free[i].pos : cckd->cdevhdr[sfx].size) - upos;
        }
        else if (!cckd->cdevhdr[sfx].free_number && cckd->cdevhdr[sfx].free_imbed)
        {
            upos = (off_t)(CCKD_L1TAB_POS + cckd->cdevhdr[sfx].numl1tab * CCKD_L1ENT_SIZE);
            ulen = cckd->cdevhdr[sfx].size - upos;
        }

        /* Return if no applicable used space */
        if (!ulen)
        {
            release_lock (&cckd->filelock);
            return moved;
        }

        if (ulen > size - moved && ulen > 65536)
            ulen = size - moved > 65536 ? size - moved : 65536;

        cckdtrc ("cckddasd: selected space 0x%llx len %d\n",
                 (long long)upos, ulen);

        for (len = u = b = asw = 0; u + len <= ulen && !asw; u += b)
        {
            bpos = upos + u;
            blen = ulen - u < 65536 ? ulen - u : 65536;

            /* Read used space into the buffer */
            cckdtrc ("cckddasd: gcperc buf read file[%d] offset 0x%llx len %d\n",
                     sfx, (long long)bpos, blen);
            rcoff = lseek (fd, (off_t)bpos, SEEK_SET);
            if (rcoff < 0)
            {
                devmsg ("%4.4X cckddasd: gcperc lseek error file[%d] offset 0x%llx: %s\n",
                        dev->devnum, sfx, (long long)bpos, strerror(errno));
                goto cckd_gc_perc_error;
            }
            rc = read (fd, &buf, blen);
            if (rc < (int)blen)
            {
                devmsg ("%4.4X cckddasd: gcperc read error file[%d] offset 0x%llx: %s\n",
                        dev->devnum, sfx, (long long)bpos, strerror(errno));
                goto cckd_gc_perc_error;
            }

            /* Process each space in the buffer */
            for (b = 0; b + CKDDASD_TRKHDR_SIZE <= (int)blen; b += len)
            {
                /* Check for level 2 table */
                for (i = 0; i < cckd->cdevhdr[sfx].numl1tab; i++)
                    if (cckd->l1[sfx][i] == (U32)(bpos + b)) break;

                if (i < cckd->cdevhdr[sfx].numl1tab)
                {
                    /* Moving a level 2 table */
                    len = CCKD_L2TAB_SIZE;
                    if (b + len > blen) break;
                    cckdtrc ("cckddasd: gcperc move l2tab[%d] at pos 0x%llx len %d\n",
                             i, (unsigned long long)(bpos + b), len);

                    /* Make the level 2 table active */
                    rc = cckd_read_l2 (dev, sfx, i);
                    if (rc < 0) goto cckd_gc_perc_error;

                    /* Relocate the level 2 table somewhere else.  When the l1
                       entry is zero, cckd_write_l2 will obtain a new space and
                       update the l1 entry */
                    fpos = cckd->l1[sfx][i];
                    cckd->l1[sfx][i] = 0;
                    rc = cckd_write_l2 (dev);
                    if (rc < 0)
                    {
                        cckd->l1[sfx][i] = fpos;
                        goto cckd_gc_perc_error;
                    }

                    /* Release the space occupied by the l2tab */
                    cckd_rel_space (dev, (off_t)(bpos + b), len);
                }
                else
                {
                    /* Moving a track image */
                    trk = cckd_cchh (dev, &buf[b], -1);
                    if (trk < 0) goto cckd_gc_perc_error;

                    l1x = trk >> 8;
                    l2x = trk & 0xff;

                    /* Read the lookup entry for the track */
                    rc = cckd_read_l2ent (dev, &l2, trk);
                    if (rc < 0) goto cckd_gc_perc_error;
                    if (l2.pos != (U32)(bpos + b))
                    {
                        devmsg ("%4.4X:cckddasd: unknown space at offset 0x%llx\n",
                                dev->devnum, (long long)(bpos + b));
                        devmsg ("%4.4X:cckddasd: %2.2x%2.2x%2.2x%2.2x%2.2x\n",
                                dev->devnum, buf[b+0], buf[b+1],buf[b+2], buf[b+3], buf[b+4]);
                        cckd_print_itrace ();
                        goto cckd_gc_perc_error;
                    }
                    len = l2.len;
                    if (b + len > blen) break;

                    cckdtrc ("cckddasd: gcperc move trk %d at pos 0x%llx len %d\n",
                              trk, (long long)(bpos + b), len);

                    /* Relocate the track image somewhere else */
                    rc = cckd_write_trkimg (dev, &buf[b], len, trk);
                    if (rc < 0) goto cckd_gc_perc_error;
                    if (rc == 1) asw = 1;
                }
            } /* for each space in the buffer */
        } /* for each space in the used space */
        moved += u;
        cckdblk.stats_gcolmoves++;
        cckdblk.stats_gcolbytes += b;
        after += asw;
 
        release_lock (&cckd->filelock);
    } /* while (moved < size) */

    sfx = cckd->sfn;
    cckdtrc ("cckddasd: gcperc moved %d 1st 0x%x nbr %d\n",
             moved, cckd->cdevhdr[sfx].free, cckd->cdevhdr[sfx].free_number);
    return moved;

cckd_gc_perc_error:

    cckdtrc ("cckddasd: gcperc exiting due to error, moved %d\n", moved);
    release_lock (&cckd->filelock);
    return moved;

} /* end function cckd_gc_percolate */


/*-------------------------------------------------------------------*/
/* cckd command help                                                 */
/*-------------------------------------------------------------------*/
void cckd_command_help()
{
    cckdmsg ("cckd command parameters:\n"
             "help\t\tDisplay help message\n"
             "stats\t\tDisplay cckd statistics\n"
             "opts\t\tDisplay cckd options\n"
             "cache=<n>\tSet cache size (M)\t\t\t(1 .. 64)\n"
             "l2cache=<n>\tSet l2cache size (K)\t\t\t(256 .. 2048)\n"
             "ra=<n>\t\tSet number readahead threads\t\t(1 .. 9)\n"
             "raq=<n>\t\tSet readahead queue size\t\t(0 .. 16)\n"
             "rat=<n>\t\tSet number tracks to read ahead\t\t(0 .. 16)\n"
             "wr=<n>\t\tSet number writer threads\t\t(1 .. 9)\n"
             "gcint=<n>\tSet garbage collector interval (sec)\t(1 .. 60)\n"
             "gcparm=<n>\tSet garbage collector parameter\t\t(-8 .. 8)\n"
             "\t\t    (least agressive ... most aggressive)\n"
             "nostress=<n>\t1=Disable stress writes\n"
             "freepend=<n>\tSet free pending cycles\t\t\t(-1 .. 4)\n"
             "fsync=<n>\t1=Enable fsync()\n"
             "ftruncwa=<n>\t1=Enable ftruncate() workaround fix\t(0 .. 1000)\n"
             "trace=<n>\tSet trace table size\t\t\t(0 .. 200000)\n"
            );
} /* end function cckd_command_help */


/*-------------------------------------------------------------------*/
/* cckd command stats                                                */
/*-------------------------------------------------------------------*/
void cckd_command_opts()
{
    cckdmsg ("cache=%d,l2cache=%d,ra=%d,raq=%d,rat=%d,"
             "wr=%d,gcint=%d,gcparm=%d,nostress=%d,\n"
             "\tfreepend=%d,fsync=%d,ftruncwa=%d,trace=%d\n",
             (cckdblk.cachenbr + 8) >> 4,
             cckdblk.l2cachenbr << 1, cckdblk.ramax,
             cckdblk.ranbr, cckdblk.readaheads,
             cckdblk.writermax, cckdblk.gcolwait,
             cckdblk.gcolparm, cckdblk.nostress, cckdblk.freepend,
             cckdblk.fsync, cckdblk.ftruncwa,cckdblk.itracen);
} /* end function cckd_command_opts */


/*-------------------------------------------------------------------*/
/* cckd command stats                                                */
/*-------------------------------------------------------------------*/
void cckd_command_stats()
{
    cckdmsg("reads....%10lld Kbytes...%10lld writes...%10lld Kbytes...%10lld\n"
            "readaheads%9lld misses...%10lld syncios..%10lld misses...%10lld\n"
            "switches.%10lld l2 reads.%10lld              stress writes...%10lld\n"
            "cachehits%10lld misses...%10lld l2 hits..%10lld misses...%10lld\n"
            "waits               write....%10lld read.....%10lld cache....%10lld\n"
            "garbage collector   moves....%10lld Kbytes...%10lld\n",
            cckdblk.stats_reads, cckdblk.stats_readbytes >> 10,
            cckdblk.stats_writes, cckdblk.stats_writebytes >> 10,
            cckdblk.stats_readaheads, cckdblk.stats_readaheadmisses,
            cckdblk.stats_syncios, cckdblk.stats_synciomisses,
            cckdblk.stats_switches, cckdblk.stats_l2reads,
            cckdblk.stats_stresswrites,
            cckdblk.stats_cachehits, cckdblk.stats_cachemisses,
            cckdblk.stats_l2cachehits, cckdblk.stats_l2cachemisses,
            cckdblk.stats_writewaits, cckdblk.stats_readwaits,
            cckdblk.stats_cachewaits,
            cckdblk.stats_gcolmoves, cckdblk.stats_gcolbytes >> 10);
} /* end function cckd_command_stats */


/*-------------------------------------------------------------------*/
/* cckd command debug                                                */
/*-------------------------------------------------------------------*/
void cckd_command_debug()
{
    int i;

    cckdmsg ("cache:\n");
    for (i = 0; i < cckdblk.cachenbr; i++)
    {
        cckdmsg ("[%3d] %4.4X:%6.6d %8.8x %p %lld\n",
            i, cckdblk.cache[i].dev ? cckdblk.cache[i].dev->devnum : 0,
            cckdblk.cache[i].trk, cckdblk.cache[i].flags,
            cckdblk.cache[i].buf, cckdblk.cache[i].age); 
    }

    cckdmsg ("l2cache:\n");
    for (i = 0; i < cckdblk.l2cachenbr; i++)
    {
        cckdmsg ("[%3d] %4.4X:%d.%3.3d  %8.8x %p %lld\n",
            i, cckdblk.l2cache[i].dev ? cckdblk.l2cache[i].dev->devnum : 0,
            cckdblk.l2cache[i].sfx, cckdblk.l2cache[i].l1x,
            cckdblk.l2cache[i].flags, cckdblk.l2cache[i].buf,
            cckdblk.l2cache[i].age); 
    }

}


/*-------------------------------------------------------------------*/
/* cckd command processor                                            */
/*-------------------------------------------------------------------*/
int cckd_command(BYTE *op, int cmd)
{
BYTE *kw, *p, c = '\0';
int   val, i, opts = 0;

    /* Display help for null operand */
    if (op == NULL)
    {
        if (memcmp (&cckdblk.id, "CCKDBLK ", sizeof(cckdblk.id)) == 0 && cmd)
            cckd_command_help();
        return 0;
    }

    /* Initialize the global cckd block if necessary */
    if (memcmp (&cckdblk.id, "CCKDBLK ", sizeof(cckdblk.id)))
        cckddasd_init (0, NULL);

    while (op)
    {
        /* Operands are delimited by commas */
        kw = op;
        op = strchr (op, ',');
        if (op) *op++ = '\0';

        /* Check for keyword = value */
        if ((p = strchr (kw, '=')))
        {
            *p++ = '\0';
            sscanf (p, "%d%c", &val, &c);
        }
        else val = -77;

        /* Parse the keyword */
        if (strcasecmp (kw, "stats") == 0)
        {
            if (!cmd) return 0;
            cckd_command_stats ();
        }
        else if (strcasecmp (kw, "opts") == 0)
        {
            if (!cmd) return 0;
            cckd_command_opts();
        }
        else if (strcasecmp (kw, "debug") == 0)
        {
            if (!cmd) return 0;
            cckd_command_debug();
        }
        else if (strcasecmp (kw, "cache") == 0)
        {
            val = val << 4;
            if (val < CCKD_MIN_CACHE || val > CCKD_MAX_CACHE || c != '\0')
            {
                cckdmsg ("Invalid value for cache= (%d)\n", val);
                return -1;
            }
            else
            {
                obtain_lock (&cckdblk.cachelock);

                /* Free cache entries if the number is decreasing */
                if (val < cckdblk.cachenbr)
                {
                    for (i = cckdblk.cachenbr - 1; i >= val; i--);
                    {
                        if (cckdblk.cache[i].flags & CCKD_CACHE_ACTIVE)
                            break;
                        else if (cckdblk.cache[i].buf)
                            free (cckdblk.cache[i].buf);
                        memset (&cckdblk.cache[i], 0, CCKD_CACHE_SIZE);
                    }
                    val = i + 1;
                }

                cckdblk.cachenbr = val;
                release_lock (&cckdblk.cachelock);
                opts = 1;
            }
        }
        else if (strcasecmp (kw, "l2cache") == 0)
        {
            val = (val + 1) >> 1;
            if (val < CCKD_MIN_L2CACHE || val > CCKD_MAX_L2CACHE || c != '\0')
            {
                cckdmsg ("Invalid value for l2cache=\n");
                return -1;
            }
            else
            {
                obtain_lock (&cckdblk.l2cachelock);

                /* Free cache entries if the number is decreasing */
                if (val < cckdblk.l2cachenbr)
                {
                    for (i = cckdblk.l2cachenbr - 1; i >= val; i--);
                    {
                        if (cckdblk.l2cache[i].flags & CCKD_CACHE_ACTIVE)
                            break;
                        else if (cckdblk.l2cache[i].buf)
                            free (cckdblk.l2cache[i].buf);
                        memset (&cckdblk.l2cache[i], 0, CCKD_CACHE_SIZE);
                    }
                    val = i + 1;
                }

                cckdblk.l2cachenbr = val;
                release_lock (&cckdblk.l2cachelock);
                opts = 1;
            }
        }
        else if (strcasecmp (kw, "ra") == 0)
        {
            if (val < CCKD_MIN_RA || val > CCKD_MAX_RA || c != '\0')
            {
                cckdmsg ("Invalid value for ra=\n");
                return -1;
            }
            else
            {
                cckdblk.ramax = val;
                opts = 1;
            }
        }
        else if (strcasecmp (kw, "raq") == 0)
        {
            if (val < 0 || val > CCKD_MAX_RA_SIZE || c != '\0')
            {
                cckdmsg ("Invalid value for raq=\n");
                return -1;
            }
            else
            {
                cckdblk.ranbr = val;
                opts = 1;
            }
        }
        else if (strcasecmp (kw, "rat") == 0)
        {
            if (val < 0 || val > CCKD_MAX_RA_SIZE || c != '\0')
            {
                cckdmsg ("Invalid value for rat=\n");
                return -1;
            }
            else
            {
                cckdblk.readaheads = val;
                opts = 1;
            }
        }
        else if (strcasecmp (kw, "wr") == 0)
        {
            if (val < CCKD_MIN_WRITER || val > CCKD_MAX_WRITER || c != '\0')
            {
                cckdmsg ("Invalid value for wr=\n");
                return -1;
            }
            else
            {
                cckdblk.writermax = val;
                opts = 1;
            }
        }
        else if (strcasecmp (kw, "gcint") == 0)
        {
            if (val < 1 || val > 60 || c != '\0')
            {
                cckdmsg ("Invalid value for gcint=\n");
                return -1;
            }
            else
            {
                cckdblk.gcolwait = val;
                opts = 1;
            }
        }
        else if (strcasecmp (kw, "gcparm") == 0)
        {
            if (val < -8 || val > 8 || c != '\0')
            {
                cckdmsg ("Invalid value for gcparm=\n");
                return -1;
            }
            else
            {
                cckdblk.gcolparm = val;
                opts = 1;
            }
        }
        else if (strcasecmp (kw, "nostress") == 0)
        {
            if (val < 0 || val > 1 || c != '\0')
            {
                cckdmsg ("Invalid value for nostress=\n");
                return -1;
            }
            else
            {
                cckdblk.nostress = val;
                opts = 1;
            }
        }
        else if (strcasecmp (kw, "freepend") == 0)
        {
            if (val < -1 || val > CCKD_MAX_FREEPEND || c != '\0')
            {
                cckdmsg ("Invalid value for freepend=\n");
                return -1;
            }
            else
            {
                cckdblk.freepend = val;
                opts = 1;
            }
        }
        else if (strcasecmp (kw, "fsync") == 0)
        {
            if (val < 0 || val > 1 || c != '\0')
            {
                cckdmsg ("Invalid value for fsync=\n");
                return -1;
            }
            else
            {
                cckdblk.fsync = val;
                opts = 1;
            }
        }
        else if (strcasecmp (kw, "ftruncwa") == 0)
        {
            if (val < 0 || val > 1000 || c != '\0')
            {
                cckdmsg ("Invalid value for ftruncwa=\n");
                return -1;
            }
            else
            {
                cckdblk.ftruncwa = val;
                opts = 1;
            }
        }
        else if (strcasecmp (kw, "trace") == 0)
        {
            if (val < 0 || val > CCKD_MAX_TRACE || c != '\0')
            {
                cckdmsg ("Invalid value for trace=\n");
                return -1;
            }
            else
            {
                /* Disable tracing in case it's already active */
                cckdblk.itracex = cckdblk.itracen = 0;
                if (cckdblk.itrace)
                {
                    sleep (1);
                    free (cckdblk.itrace);
                    cckdblk.itrace = NULL;
                }

                /* Get a new trace table */
                if (val > 0)
                {
                    cckdblk.itrace = calloc ( val, 128);
                    if (cckdblk.itrace)
                    {
                        cckdblk.itracex = 0;
                        cckdblk.itracen = val;
                    }
                    else
                        cckdmsg ("calloc() failed for trace table: %s\n",
                                 strerror(errno));
                }
                opts = 1;
            }
        }
        else
        {
            cckdmsg ("cckd invalid keyword: %s\n",kw);
            if (!cmd) return -1;
            cckd_command_help ();
            op = NULL;
        }
    }

    if (cmd && opts) cckd_command_opts();
    return 0;
} /* end function cckd_command */


/*-------------------------------------------------------------------*/
/* Print internal trace                                              */
/*-------------------------------------------------------------------*/
void cckd_print_itrace()
{
int             start, i, n;            /* Indexes                   */

    if (cckdblk.itracen == 0) return;
    cckdmsg ("cckddasd: print_itrace\n");
    n = cckdblk.itracen;
    cckdblk.itracen = 0;
    sleep (1);
    i = start = cckdblk.itracex;
    do
    {
        if (i >= 128 * n) i = 0;
        if (cckdblk.itrace[i] != '\0')
            cckdmsg ("%s", &cckdblk.itrace[i]);
        i+=128;
    } while (i != start);
    memset (cckdblk.itrace, 0, n * 128);
    cckdblk.itracex = 0;
    cckdblk.itracen = n;
    sleep (2);
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



DEVHND cfbadasd_device_hndinfo = {
        &fbadasd_init_handler,
        &fbadasd_execute_ccw,
        &cckddasd_close_device,
        &fbadasd_query_device,
        &cckddasd_start,
        &cckddasd_end,
        &cckddasd_start,
        &cckddasd_end
};

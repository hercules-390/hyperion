/* CCKDDASD.C   (c) Copyright Roger Bowler, 1999-2004                */
/*       ESA/390 Compressed CKD Direct Access Storage Device Handler */

/*-------------------------------------------------------------------*/
/* This module contains device functions for compressed emulated     */
/* count-key-data direct access storage devices.                     */
/*-------------------------------------------------------------------*/

#include "hercules.h"
#include "devtype.h"
#include "opcode.h"

#define cckdtrc(format, a...) \
do { \
 if (dev && (dev->ccwtrace||dev->ccwstep)) \
  logmsg("%4.4X:" format, dev->devnum, a); \
 if (cckdblk.itrace) { \
    struct timeval tv; \
    CCKD_TRACE *p; \
    p = cckdblk.itracep++; \
    if (p >= cckdblk.itracex) { \
        p = cckdblk.itrace; \
        cckdblk.itracep = p + 1; \
    } \
    gettimeofday(&tv, NULL); \
    if (p) sprintf ((char *)p, "%6.6ld" "." "%6.6ld %4.4X:" format, \
                    tv.tv_sec, tv.tv_usec, dev ? dev->devnum : 0, a); \
 } \
} while (0)

/*-------------------------------------------------------------------*/
/* Internal functions                                                */
/*-------------------------------------------------------------------*/
int     cckddasd_init(int argc, BYTE *argv[]);
int     cckddasd_term();
int     cckddasd_init_handler( DEVBLK *dev, int argc, BYTE *argv[] );
int     cckddasd_close_device(DEVBLK *dev);
void    cckddasd_start(DEVBLK *dev);
void    cckddasd_end(DEVBLK *dev);
int     cckd_read_track(DEVBLK *dev, int trk, BYTE *unitstat);
int     cckd_update_track(DEVBLK *dev, int trk, int off, 
                         BYTE *buf, int len, BYTE *unitstat);
int     cckd_used(DEVBLK *dev);
int     cfba_read_block(DEVBLK *dev, int blkgrp, BYTE *unitstat);
int     cfba_write_block(DEVBLK *dev, int blkgrp, int off, 
                         BYTE *buf, int wrlen, BYTE *unitstat);
int     cfba_used(DEVBLK *dev);
int     cckd_read_trk(DEVBLK *dev, int trk, int ra, BYTE *unitstat);
void    cckd_readahead(DEVBLK *dev, int trk);
int     cckd_readahead_scan(int *answer, int ix, int i, void *data);
void    cckd_ra();
void    cckd_flush_cache(DEVBLK *dev);
int     cckd_flush_cache_scan(int *answer, int ix, int i, void *data);
void    cckd_flush_cache_all();
void    cckd_purge_cache(DEVBLK *dev);
int     cckd_purge_cache_scan(int *answer, int ix, int i, void *data);
void    cckd_writer();
int     cckd_writer_scan(int *o, int ix, int i, void *data);
off_t   cckd_get_space(DEVBLK *dev, unsigned int len);
void    cckd_rel_space(DEVBLK *dev, off_t pos, int len);
void    cckd_rel_free_atend(DEVBLK *dev, unsigned int pos, int len, int i);
void    cckd_flush_space(DEVBLK *dev);
int     cckd_read_chdr(DEVBLK *dev);
int     cckd_write_chdr(DEVBLK *dev);
int     cckd_read_l1(DEVBLK *dev);
int     cckd_write_l1(DEVBLK *dev);
int     cckd_write_l1ent(DEVBLK *dev, int l1x);
int     cckd_read_init(DEVBLK *dev);
int     cckd_read_fsp(DEVBLK *dev);
int     cckd_write_fsp(DEVBLK *dev);
int     cckd_read_l2(DEVBLK *dev, int sfx, int l1x);
void    cckd_purge_l2(DEVBLK *dev);
int     cckd_purge_l2_scan(int *answer, int ix, int i, void *data);
int     cckd_steal_l2();
int     cckd_steal_l2_scan(int *answer, int ix, int i, void *data);
int     cckd_write_l2(DEVBLK *dev);
int     cckd_read_l2ent(DEVBLK *dev, CCKD_L2ENT *l2, int trk);
int     cckd_write_l2ent(DEVBLK *dev,   CCKD_L2ENT *l2, int trk);
int     cckd_read_trkimg(DEVBLK *dev, BYTE *buf, int trk, BYTE *unitstat);
int     cckd_write_trkimg(DEVBLK *dev, BYTE *buf, int len, int trk);
int     cckd_harden(DEVBLK *dev);
int     cckd_trklen(DEVBLK *dev, BYTE *buf);
void    cckd_truncate(DEVBLK *dev, int now);
int     cckd_null_trk(DEVBLK *dev, BYTE *buf, int trk, int sz0);
int     cckd_cchh(DEVBLK *dev, BYTE *buf, int trk);
int     cckd_validate(DEVBLK *dev, BYTE *buf, int trk, int len);
int     cckd_sf_name(DEVBLK *dev, int sfx, char *sfn);
int     cckd_sf_init(DEVBLK *dev);
int     cckd_sf_new(DEVBLK *dev);
void    cckd_sf_add(DEVBLK *dev);
void    cckd_sf_remove(DEVBLK *dev, int merge);
void    cckd_sf_newname(DEVBLK *dev, BYTE *sfn);
void    cckd_sf_comp(DEVBLK *dev);
void    cckd_sf_stats(DEVBLK *dev);
int     cckd_disable_syncio(DEVBLK *dev);
void    cckd_lock_devchain(int flag);
void    cckd_unlock_devchain();
void    cckd_gcol();
int     cckd_gc_percolate(DEVBLK *dev, unsigned int size);
DEVBLK *cckd_find_device_by_devnum (U16 devnum);
BYTE   *cckd_uncompress(DEVBLK *dev, BYTE *from, int len, int maxlen, int trk);
int     cckd_uncompress_zlib(DEVBLK *dev, BYTE *to, BYTE *from, int len, int maxlen);
int     cckd_uncompress_bzip2(DEVBLK *dev, BYTE *to, BYTE *from, int len, int maxlen);
int     cckd_compress(DEVBLK *dev, BYTE **to, BYTE *from, int len, int comp, int parm);
int     cckd_compress_none(DEVBLK *dev, BYTE **to, BYTE *from, int len, int parm);
int     cckd_compress_zlib(DEVBLK *dev, BYTE **to, BYTE *from, int len, int parm);
int     cckd_compress_bzip2(DEVBLK *dev, BYTE **to, BYTE *from, int len, int parm);
void    cckd_command_help();
void    cckd_command_opts();
void    cckd_command_stats();
void    cckd_command_debug();
int     cckd_command(BYTE *op, int cmd);
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
    initialize_lock (&cckdblk.gclock);
    initialize_lock (&cckdblk.ralock);
    initialize_lock (&cckdblk.wrlock);
    initialize_lock (&cckdblk.devlock);
    initialize_condition (&cckdblk.gccond);
    initialize_condition (&cckdblk.racond);
    initialize_condition (&cckdblk.wrcond);
    initialize_condition (&cckdblk.devcond);
    initialize_condition (&cckdblk.termcond);

    /* Initialize some variables */
    cckdblk.wrprio     = 16;
    cckdblk.ranbr      = CCKD_DEFAULT_RA_SIZE;
    cckdblk.ramax      = CCKD_DEFAULT_RA;
    cckdblk.wrmax      = CCKD_DEFAULT_WRITER;
    cckdblk.gcmax      = CCKD_DEFAULT_GCOL;
    cckdblk.gcwait     = CCKD_DEFAULT_GCOLWAIT;
    cckdblk.gcparm     = CCKD_DEFAULT_GCOLPARM;
    cckdblk.readaheads = CCKD_DEFAULT_READAHEADS;
    cckdblk.freepend   = CCKD_DEFAULT_FREEPEND;
#ifdef HAVE_LIBZ
    cckdblk.comps     |= CCKD_COMPRESS_ZLIB;
#endif
#ifdef CCKD_BZIP2
    cckdblk.comps     |= CCKD_COMPRESS_BZIP2;
#endif
    cckdblk.comp       = 0xff;
    cckdblk.compparm   = -1;

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
    cckdblk.gcmax = 0;
    if (cckdblk.gcs)
    {
        broadcast_condition (&cckdblk.gccond);
        wait_condition (&cckdblk.termcond, &cckdblk.gclock);
    }
    release_lock (&cckdblk.gclock);

    /* Terminate the writer threads */
    obtain_lock (&cckdblk.wrlock);
    cckdblk.wrmax = 0;
    if (cckdblk.wrs)
    {
        broadcast_condition (&cckdblk.wrcond);
        wait_condition (&cckdblk.termcond, &cckdblk.wrlock);
    }
    release_lock (&cckdblk.wrlock);

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
        logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD100E malloc failed for cckd extension: %s\n"),
                strerror(errno));
        return -1;
    }
    memset(cckd, 0, sizeof(CCKDDASD_EXT));
    memset(&cckd_empty_l2tab, 0, CCKD_L2TAB_SIZE);

    /* Initialize the global cckd block if necessary */
    if (memcmp (&cckdblk.id, "CCKDBLK ", sizeof(cckdblk.id)))
        cckddasd_init (0, NULL);

    /* Initialize locks and conditions */
    initialize_lock (&cckd->iolock);
    initialize_lock (&cckd->filelock);
    initialize_condition (&cckd->iocond);

    /* Initialize some variables */
    obtain_lock (&cckd->filelock);
    cckd->l1x = cckd->sfx = dev->cache = cckd->free1st = -1;
    cckd->fd[0] = dev->fd;
    fdflags = fcntl (dev->fd, F_GETFL);
    cckd->open[0] = (fdflags & O_RDWR) ? CCKD_OPEN_RW : CCKD_OPEN_RO;

    /* call the chkdsk function */
    rc = cckd_chkdsk (cckd->fd[0], stdout, 0);
    if (rc < 0) return -1;

    /* Perform initial read */
    rc = cckd_read_init (dev);
    if (rc < 0) return -1;
    if (cckd->fbadasd) dev->ckdtrksz = CFBA_BLOCK_SIZE;

    /* open the shadow files */
    rc = cckd_sf_init (dev);
    if (rc < 0)
    {   logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD101E error initializing shadow files\n"));
        return -1;
    }

    /* Update the device handler routines */
    if (cckd->ckddasd)
        dev->hnd = &cckddasd_device_hndinfo;
    else
        dev->hnd = &cfbadasd_device_hndinfo;
    release_lock (&cckd->filelock);

    /* Insert the device into the cckd device queue */
    cckd_lock_devchain(1);
    for (cckd = NULL, dev2 = cckdblk.dev1st; dev2; dev2 = cckd->devnext)
        cckd = dev2->cckd_ext;
    if (cckd) cckd->devnext = dev;
    else cckdblk.dev1st = dev;
    cckd_unlock_devchain();

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

    /* Wait for readaheads to finish */
    obtain_lock(&cckdblk.ralock);
    cckd->stopping = 1;
    while (cckd->ras)
    {
        release_lock(&cckdblk.ralock);
        usleep(1);
        obtain_lock(&cckdblk.ralock);
    }
    release_lock(&cckdblk.ralock);

    /* Flush the cache and wait for the writes to complete */
    obtain_lock (&cckd->iolock);
    cckd->stopping = 1;
    cckd_flush_cache (dev);
    while (cckd->wrpending || cckd->ioactive)
    {
        cckd->iowaiters++;
        wait_condition (&cckd->iocond, &cckd->iolock);
        cckd->iowaiters--;
        cckd_flush_cache (dev);
    }
    broadcast_condition (&cckd->iocond);
    cckd_purge_cache (dev); cckd_purge_l2 (dev);
    dev->bufcur = dev->cache = -1;
    if (cckd->newbuf) free (cckd->newbuf);
    release_lock (&cckd->iolock);

    /* Remove the device from the cckd queue */
    cckd_lock_devchain(1);
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
    cckd_unlock_devchain();

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
U16             devnum = 0;             /* Last active device number */
int             trk = 0;                /* Last active track         */

    cckd = dev->cckd_ext;

    cckdtrc ("cckddasd: start i/o bufcur %d cache[%d]\n",
             dev->bufcur, dev->cache);

    /* Reset buffer offsets */
    dev->bufoff = 0;
    dev->bufoffhi = cckd->ckddasd ? dev->ckdtrksz : CFBA_BLOCK_SIZE;

    /* Check for merge - synchronous i/o should be disabled */
    obtain_lock(&cckd->iolock);
    if (cckd->merging)
    {
        cckdtrc("cckddasd: start i/o waiting for merge%s\n","");
        while (cckd->merging)
        {
            cckd->iowaiters++;
            wait_condition (&cckd->iocond, &cckd->iolock);
            cckd->iowaiters--;
        }
        dev->bufcur = dev->cache = -1;
    }
    cckd->ioactive = 1;

    cache_lock(CACHE_DEVBUF);

    if (dev->cache >= 0)
        CCKD_CACHE_GETKEY(dev->cache, devnum, trk);

    /* Check if previous active entry is still valid and not busy */
    if (dev->cache >= 0 && dev->devnum == devnum && dev->bufcur == trk
     && !(cache_getflag(CACHE_DEVBUF, dev->cache) & CCKD_CACHE_IOBUSY))
    {
        /* Make the entry active again */
        cache_setflag (CACHE_DEVBUF, dev->cache, ~0, CCKD_CACHE_ACTIVE);

        /* If the entry is pending write then change it to `updated' */
        if (cache_getflag(CACHE_DEVBUF, dev->cache) & CCKD_CACHE_WRITE)
        {
            cache_setflag (CACHE_DEVBUF, dev->cache, ~CCKD_CACHE_WRITE, CCKD_CACHE_UPDATED);
            cckd->wrpending--;
            if ((cckd->iowaiters || cckd->gcwaiting) && !cckd->wrpending)
                broadcast_condition (&cckd->iocond);
        }
    }
    else
        dev->bufcur = dev->cache = -1;

    cache_unlock (CACHE_DEVBUF);

    release_lock (&cckd->iolock);

    return;

} /* end function cckddasd_start */

/*-------------------------------------------------------------------*/
/* Compressed ckd end/suspend channel program                        */
/*-------------------------------------------------------------------*/
void cckddasd_end (DEVBLK *dev)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */

    cckd = dev->cckd_ext;

    /* Update length if previous image was updated */
    if (dev->bufupd && dev->bufcur >= 0 && dev->cache >= 0)
    {
        dev->buflen = cckd_trklen (dev, dev->buf);
        cache_setval (CACHE_DEVBUF, dev->cache, dev->buflen);
    }

    dev->bufupd = 0;

    cckdtrc ("cckddasd: end i/o bufcur %d cache[%d] waiters %d\n",
             dev->bufcur, dev->cache, cckd->iowaiters);

    obtain_lock (&cckd->iolock);

    cckd->ioactive = 0;

    /* Make the current entry inactive */
    if (dev->cache >= 0)
    {
        cache_lock (CACHE_DEVBUF);
        cache_setflag (CACHE_DEVBUF, dev->cache, ~CCKD_CACHE_ACTIVE, 0);
        cache_unlock (CACHE_DEVBUF);
    }

    /* Cause writers to start after first update */
    if (cckd->updated && (cckdblk.wrs == 0 || cckd->iowaiters != 0))
        cckd_flush_cache (dev);
    else if (cckd->iowaiters)
        broadcast_condition (&cckd->iocond);

    release_lock (&cckd->iolock); 

} /* end function cckddasd_end */

/*-------------------------------------------------------------------*/
/* Compressed ckd read track image                                   */
/*-------------------------------------------------------------------*/
int cckd_read_track (DEVBLK *dev, int trk, BYTE *unitstat)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             rc;                     /* Return code               */
int             len;                    /* Compressed length         */
BYTE           *newbuf;                 /* Uncompressed buffer       */
int             cache;                  /* New active cache entry    */
int             syncio;                 /* Syncio indicator          */

    cckd = dev->cckd_ext;

    /* Update length if previous image was updated */
    if (dev->bufupd && dev->bufcur >= 0 && dev->cache >= 0)
    {
        dev->buflen = cckd_trklen (dev, dev->buf);
        cache_setval (CACHE_DEVBUF, dev->cache, dev->buflen);
    }

    /* Turn off the synchronous I/O bit if trk overflow or trk 0 */
    syncio = dev->syncio_active;
    if (dev->ckdtrkof || trk == 0)
        dev->syncio_active = 0;

    /* Reset buffer offsets */
    dev->bufoff = 0;
    dev->bufoffhi = dev->ckdtrksz;

    /* Check if reading the same track image */
    if (trk == dev->bufcur && dev->cache >= 0)
    {
        /* Track image may be compressed */
        if ((dev->buf[0] & CCKD_COMPRESS_MASK) != 0
         && (dev->buf[0] & dev->comps) == 0)
        {
            /* Return if synchronous i/o */
            if (dev->syncio_active)
            {
                cckdtrc ("cckddasd: read  trk   %d syncio compressed\n", trk);
                cckdblk.stats_synciomisses++;
                dev->syncio_retry = 1;
                return -1;
            }
            len = cache_getval(CACHE_DEVBUF, dev->cache);
            newbuf = cckd_uncompress (dev, dev->buf, len, dev->ckdtrksz, trk);
            if (newbuf == NULL) {
                ckd_build_sense (dev, SENSE_EC, 0, 0, FORMAT_1, MESSAGE_0);
                *unitstat = CSW_CE | CSW_DE | CSW_UC;
                dev->bufcur = dev->cache = -1;
                dev->syncio_active = syncio;
                return -1;
            }
            cache_setbuf (CACHE_DEVBUF, dev->cache, newbuf, dev->ckdtrksz);
            dev->buf     = newbuf;
            dev->buflen  = cckd_trklen (dev, newbuf);
            cache_setval (CACHE_DEVBUF, dev->cache, dev->buflen);
            dev->bufsize = cache_getlen (CACHE_DEVBUF, dev->cache);
            dev->bufupd  = 0;
            cckdtrc ("cckddasd: read  trk   %d uncompressed len %d\n",
                     trk, dev->buflen);
        }

        dev->comp = dev->buf[0] & CCKD_COMPRESS_MASK;
        if (dev->comp != 0) dev->compoff = CKDDASD_TRKHDR_SIZE;

        return 0;
    }

    cckdtrc ("cckddasd: read  trk   %d (%s)\n", trk,
              dev->syncio_active ? "synchronous" : "asynchronous");

    /* read the new track */
    dev->bufupd = 0;
    *unitstat = 0;
    cache = cckd_read_trk (dev, trk, 0, unitstat);
    if (cache < 0)
    {
        dev->bufcur = dev->cache = -1;
        return -1;
    }

    dev->cache    = cache;
    dev->buf      = cache_getbuf (CACHE_DEVBUF, dev->cache, 0);
    dev->bufcur   = trk;
    dev->bufoff   = 0;
    dev->bufoffhi = dev->ckdtrksz;
    dev->buflen   = cache_getval (CACHE_DEVBUF, dev->cache);
    dev->bufsize  = cache_getlen (CACHE_DEVBUF, dev->cache);

    dev->comp = dev->buf[0] & CCKD_COMPRESS_MASK;
    if (dev->comp != 0) dev->compoff = CKDDASD_TRKHDR_SIZE;

    /* If the image is compressed then call ourself recursively
       to cause the image to get uncompressed */
    if (dev->comp != 0 && (dev->comp & dev->comps) == 0)
        rc = cckd_read_track (dev, trk, unitstat);
    else
        rc = 0;

    dev->syncio_active = syncio;
    return rc;
} /* end function cckd_read_track */

/*-------------------------------------------------------------------*/
/* Compressed ckd update track image                                 */
/*-------------------------------------------------------------------*/
int cckd_update_track (DEVBLK *dev, int trk, int off,
                       BYTE *buf, int len, BYTE *unitstat)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             rc;                     /* Return code               */

    cckd = dev->cckd_ext;

    /* Immediately return if fake writing */
    if (dev->ckdfakewr)
        return len;

    /* Error if opened read-only */
    if (dev->ckdrdonly)
    {
        ckd_build_sense (dev, SENSE_EC, SENSE1_WRI, 0,FORMAT_1, MESSAGE_0);
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        dev->bufcur = dev->cache = -1;
        return -1;
    }

    /* If the track is not current or compressed then read it.
       `dev->comps' is set to zero forcing the read routine to
       uncompress the image.                                     */
    if (trk != dev->bufcur || (dev->buf[0] & CCKD_COMPRESS_MASK) != 0)
    {
        dev->comps = 0;
        rc = (dev->hnd->read) (dev, trk, unitstat);
        if (rc < 0)
        {
            dev->bufcur = dev->cache = -1;
            return -1;
        }
    }

    /* Invalid track format if going past buffer end */
    if (off + len > dev->ckdtrksz)
    {
        ckd_build_sense (dev, 0, SENSE1_ITF, 0, 0, 0);
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        dev->bufcur = dev->cache = -1;
        return -1;
    }

    /* Copy the data into the buffer */
    if (buf && len > 0) memcpy (dev->buf + off, buf, len);

    cckdtrc ("cckddasd: updt  trk   %d offset %d length %d\n",
             trk, off, len);

    /* Update the cache entry */
    cache_setflag (CACHE_DEVBUF, dev->cache, ~0, CCKD_CACHE_UPDATED | CCKD_CACHE_USED);
    cckd->updated = 1;

    /* Notify the shared server of the update */
    if (!dev->bufupd)
    {
        dev->bufupd = 1;
        shared_update_notify (dev, trk);
    }

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
int cfba_read_block (DEVBLK *dev, int blkgrp, BYTE *unitstat)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             rc;                     /* Return code               */
int             cache;                  /* New active cache entry    */
BYTE           *cbuf;                   /* -> cache buffer           */
BYTE           *newbuf;                 /* Uncompressed buffer       */
int             len;                    /* Compressed length         */
int             maxlen;                 /* Size for cache entry      */

    cckd = dev->cckd_ext;

    if (dev->cache >= 0)
        cbuf = cache_getbuf (CACHE_DEVBUF, dev->cache, 0);
    else
        cbuf = NULL;
    maxlen = CFBA_BLOCK_SIZE + CKDDASD_TRKHDR_SIZE;

    /* Return if reading the same track image */
    if (blkgrp == dev->bufcur && dev->cache >= 0)
    {
        /* Block group image may be compressed */
        if ((cbuf[0] & CCKD_COMPRESS_MASK) != 0
         && (cbuf[0] & dev->comps) == 0)
        {
            /* Return if synchronous i/o */
            if (dev->syncio_active)
            {
                cckdtrc ("cckddasd: read blkgrp  %d syncio compressed\n",
                         blkgrp);
                cckdblk.stats_synciomisses++;
                dev->syncio_retry = 1;
                return -1;
            }
            len = cache_getval(CACHE_DEVBUF, dev->cache);
            newbuf = cckd_uncompress (dev, cbuf, len, maxlen, blkgrp);
            if (newbuf == NULL) {
                dev->sense[0] = SENSE_EC;
                *unitstat = CSW_CE | CSW_DE | CSW_UC;
                dev->bufcur = dev->cache = -1;
                return -1;
            }
            cache_setbuf (CACHE_DEVBUF, dev->cache, newbuf, maxlen);
            cbuf = newbuf;
            dev->buf     = newbuf + CKDDASD_TRKHDR_SIZE;
            dev->buflen  = CFBA_BLOCK_SIZE;
            cache_setval (CACHE_DEVBUF, dev->cache, dev->buflen);
            dev->bufsize = cache_getlen (CACHE_DEVBUF, dev->cache);
            dev->bufupd  = 0;
            cckdtrc ("cckddasd: read bkgrp  %d uncompressed len %d\n",
                     blkgrp, dev->buflen);
        }

        dev->comp = cbuf[0] & CCKD_COMPRESS_MASK;

        return 0;
    }

    cckdtrc ("cckddasd: read blkgrp  %d (%s)\n", blkgrp,
              dev->syncio_active ? "synchronous" : "asynchronous");

    /* Read the new blkgrp */
    dev->bufupd = 0;
    *unitstat = 0;
    cache = cckd_read_trk (dev, blkgrp, 0, unitstat);
    if (cache < 0)
    {
        dev->bufcur = dev->cache = -1;
        return -1;
    }
    dev->cache    = cache;
    cbuf          = cache_getbuf (CACHE_DEVBUF, dev->cache, 0);
    dev->buf      = cbuf + CKDDASD_TRKHDR_SIZE;
    dev->bufcur   = blkgrp;
    dev->bufoff   = 0;
    dev->bufoffhi = CFBA_BLOCK_SIZE;
    dev->buflen   = cache_getval (CACHE_DEVBUF, dev->cache);
    dev->bufsize  = cache_getlen (CACHE_DEVBUF, dev->cache);
    dev->comp     = cbuf[0] & CCKD_COMPRESS_MASK;

    /* If the image is compressed then call ourself recursively
       to cause the image to get uncompressed.  This is because
      `bufcur' will match blkgrp and `comps' won't match `comp' */
    if (dev->comp != 0 && (dev->comp & dev->comps) == 0)
        rc = cfba_read_block (dev, blkgrp, unitstat);
    else
        rc = 0;

    return rc;
} /* end function cfba_read_block */

/*-------------------------------------------------------------------*/
/* Compressed fba write block(s)                                     */
/*-------------------------------------------------------------------*/
int cfba_write_block (DEVBLK *dev, int blkgrp, int off,
                      BYTE *buf, int len, BYTE *unitstat)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             rc;                     /* Return code               */
BYTE           *cbuf;                   /* -> cache buffer           */

    cckd = dev->cckd_ext;

    if (dev->cache >= 0)
        cbuf = cache_getbuf (CACHE_DEVBUF, dev->cache, 0);
    else
        cbuf = NULL;

    /* Read the block group if it's not current or compressed.
       `dev->comps' is set to zero forcing the read routine to
       uncompress the image.                                   */
    if (blkgrp != dev->bufcur || (cbuf[0] & CCKD_COMPRESS_MASK) != 0)
    {
        dev->comps = 0;
        rc = (dev->hnd->read) (dev, blkgrp, unitstat);
        if (rc < 0)
        {
            dev->bufcur = dev->cache = -1;
            return -1;
        }
    }

    /* Copy the data into the buffer */
    if (buf) memcpy (dev->buf + off, buf, len);

    /* Update the cache entry */
    cache_setflag (CACHE_DEVBUF, dev->cache, ~0, CCKD_CACHE_UPDATED|CCKD_CACHE_USED);
    cckd->updated = 1;

    /* Notify the shared server of the update */
    if (!dev->bufupd)
    {
        dev->bufupd = 1;
        shared_update_notify (dev, blkgrp);
    }

    return len;

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
int cckd_read_trk(DEVBLK *dev, int trk, int ra, BYTE *unitstat)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             fnd;                    /* Cache index for hit       */
int             lru;                    /* Oldest unused cache index */
int             len;                    /* Length of track image     */
int             maxlen;                 /* Length for buffer         */
int             curtrk = -1;            /* Current track (at entry)  */
U16             devnum;                 /* Device number             */
U32             oldtrk;                 /* Stolen track number       */
U32             flag;                   /* Cache flag                */
BYTE           *buf;                    /* Read buffer               */

    cckd = dev->cckd_ext;

    cckdtrc ("cckddasd: %d rdtrk     %d\n", ra, trk);

    maxlen = cckd->ckddasd ? dev->ckdtrksz
                           : CFBA_BLOCK_SIZE + CKDDASD_TRKHDR_SIZE;

    if (!ra) obtain_lock (&cckd->iolock);

    cache_lock (CACHE_DEVBUF);

    /* Inactivate the old entry */
    if (!ra)
    {
        curtrk = dev->bufcur;
        if (dev->cache >= 0)
            cache_setflag(CACHE_DEVBUF, dev->cache, ~CCKD_CACHE_ACTIVE, 0);
        dev->bufcur = dev->cache = -1;
    }

cckd_read_trk_retry:

    /* scan the cache array for the track */
    fnd = cache_lookup (CACHE_DEVBUF, CCKD_CACHE_SETKEY(dev->devnum, trk), &lru);

    /* check for cache hit */
    if (fnd >= 0)
    {
        if (ra) /* readahead doesn't care about a cache hit */
        {   cache_unlock (CACHE_DEVBUF);
            return fnd;
        }

        /* If synchronous I/O and I/O is active then return
           with syncio_retry bit on */
        if (dev->syncio_active)
        {
            if (cache_getflag(CACHE_DEVBUF, fnd) & CCKD_CACHE_IOBUSY)
            {
                cckdtrc ("cckddasd: %d rdtrk[%d] %d syncio %s\n", ra, fnd, trk,
                         cache_getflag(CACHE_DEVBUF, fnd) & CCKD_CACHE_READING ?
                         "reading" : "writing");
                cckdblk.stats_synciomisses++;
                dev->syncio_retry = 1;
                cache_unlock (CACHE_DEVBUF);
                release_lock (&cckd->iolock);
                return -1;
            }
            else cckdblk.stats_syncios++;
        }    

        /* Mark the new entry active */
        cache_setflag(CACHE_DEVBUF, fnd, ~0, CCKD_CACHE_ACTIVE | CCKD_CACHE_USED);
        cache_setage(CACHE_DEVBUF, fnd);

        /* If the entry is pending write then change it to `updated' */
        if (cache_getflag(CACHE_DEVBUF, fnd) & CCKD_CACHE_WRITE)
        {
            cache_setflag(CACHE_DEVBUF, fnd, ~CCKD_CACHE_WRITE, CCKD_CACHE_UPDATED);
            cckd->wrpending--;
            if ((cckd->iowaiters || cckd->gcwaiting) && !cckd->wrpending)
                broadcast_condition (&cckd->iocond);
        }

        cache_unlock (CACHE_DEVBUF);

        cckdtrc ("cckddasd: %d rdtrk[%d] %d cache hit\n", ra, fnd, trk);

        cckdblk.stats_switches++;  cckd->switches++;
        cckdblk.stats_cachehits++; cckd->cachehits++;

        /* if read/write is in progress then wait for it to finish */
        while (cache_getflag(CACHE_DEVBUF, fnd) & CCKD_CACHE_IOBUSY)
        {
            cckdblk.stats_iowaits++;
            cckdtrc ("cckddasd: %d rdtrk[%d] %d waiting for %s\n", ra, fnd, trk,
                      cache_getflag(CACHE_DEVBUF, fnd) & CCKD_CACHE_READING ?
                      "read" : "write");
            cache_setflag (CACHE_DEVBUF, fnd, ~0, CCKD_CACHE_IOWAIT);
            cckd->iowaiters++;
            wait_condition (&cckd->iocond, &cckd->iolock);
            cckd->iowaiters--;
            cache_setflag (CACHE_DEVBUF, fnd, ~CCKD_CACHE_IOWAIT, 0); 
            cckdtrc ("cckddasd: %d rdtrk[%d] %d io wait complete\n",
                      ra, fnd, trk);
        }

        release_lock (&cckd->iolock);

        /* Asynchrously schedule readaheads */
        if (curtrk > 0 && trk > curtrk && trk <= curtrk + 2)
            cckd_readahead (dev, trk);

        return fnd;

    } /* cache hit */

    /* If not readahead and synchronous I/O then retry */
    if (!ra && dev->syncio_active)
    {
        cache_unlock(CACHE_DEVBUF);
        release_lock (&cckd->iolock);
        cckdtrc ("cckddasd: %d rdtrk[%d] %d syncio cache miss\n", ra, lru, trk);
        cckdblk.stats_synciomisses++;
        dev->syncio_retry = 1;
        return -1;
    }

    cckdtrc ("cckddasd: %d rdtrk[%d] %d cache miss\n", ra, lru, trk);

    /* If no cache entry was stolen, then flush all outstanding writes.
       This requires us to release our locks.  cache_wait should be
       called with only the cache_lock held.  Fortunately, cache waits
       occur very rarely. */
    if (lru < 0) /* No available entry to be stolen */
    {
        cckdtrc ("cckddasd: %d rdtrk[%d] %d no available cache entry\n",
                 ra, lru, trk);
        cache_unlock (CACHE_DEVBUF);
        if (!ra) release_lock (&cckd->iolock);
        cckd_flush_cache_all();
        cache_lock (CACHE_DEVBUF);
        cckdblk.stats_cachewaits++;
        cache_wait (CACHE_DEVBUF);
        if (!ra)
        {
            cache_unlock (CACHE_DEVBUF);
            obtain_lock (&cckd->iolock);
            cache_lock (CACHE_DEVBUF);
        }
        goto cckd_read_trk_retry;
    }

    CCKD_CACHE_GETKEY(lru, devnum, oldtrk);
    if (devnum != 0)
    {
        cckdtrc ("cckddasd: %d rdtrk[%d] %d dropping %4.4X:%d from cache\n",
                 ra, lru, trk, devnum, oldtrk);
        if (!(cache_getflag(CACHE_DEVBUF, lru) & CCKD_CACHE_USED))
        {
            cckdblk.stats_readaheadmisses++;  cckd->misses++;
        }
    }

    /* Initialize the entry */
    cache_setkey(CACHE_DEVBUF, lru, CCKD_CACHE_SETKEY(dev->devnum, trk));
    cache_setflag(CACHE_DEVBUF, lru, 0, CCKD_CACHE_READING);
    cache_setage(CACHE_DEVBUF, lru);
    cache_setval(CACHE_DEVBUF, lru, 0);
    if (!ra)
    {
        cckdblk.stats_switches++; cckd->switches++;
        cckdblk.stats_cachemisses++;
        cache_setflag(CACHE_DEVBUF, lru, ~0, CCKD_CACHE_ACTIVE|CCKD_CACHE_USED);
    }
    cache_setflag(CACHE_DEVBUF, lru, ~CACHE_TYPE,
                  cckd->ckddasd ? DEVBUF_TYPE_CCKD : DEVBUF_TYPE_CFBA);
    buf = cache_getbuf(CACHE_DEVBUF, lru, maxlen);

    cckdtrc ("cckddasd: %d rdtrk[%d] %d buf %p len %d\n",
             ra, lru, trk, buf, cache_getlen(CACHE_DEVBUF, lru));

    cache_unlock (CACHE_DEVBUF);

    if (!ra) release_lock (&cckd->iolock);

    /* Asynchronously schedule readaheads */
    if (!ra && curtrk > 0 && trk > curtrk && trk <= curtrk + 2)
        cckd_readahead (dev, trk);

    /* Clear the buffer if batch mode */
    if (dev->batch) memset(buf, 0, maxlen);

    /* Read the track image */
    obtain_lock (&cckd->filelock);
    len = cckd_read_trkimg (dev, buf, trk, unitstat);
    release_lock (&cckd->filelock);
    cache_setval (CACHE_DEVBUF, lru, len);

    obtain_lock (&cckd->iolock);

    /* Turn off the READING bit */
    cache_lock (CACHE_DEVBUF);
    flag = cache_setflag(CACHE_DEVBUF, lru, ~CCKD_CACHE_READING, 0);
    cache_unlock (CACHE_DEVBUF);

    /* Wakeup other thread waiting for this read */
    if (cckd->iowaiters && (flag & CCKD_CACHE_IOWAIT))
    {   cckdtrc ("cckddasd: %d rdtrk[%d] %d signalling read complete\n",
                 ra, lru, trk);
        broadcast_condition (&cckd->iocond);
    }

    release_lock (&cckd->iolock);

    if (ra)
    {
        cckdblk.stats_readaheads++; cckd->readaheads++;
    }

    cckdtrc ("cckddasd: %d rdtrk[%d] %d complete\n", ra, lru, trk);

    if (cache_busy_percent(CACHE_DEVBUF) > 80) cckd_flush_cache_all();

    return lru;

} /* end function cckd_read_trk */

/*-------------------------------------------------------------------*/
/* Schedule asynchronous readaheads                                  */
/*-------------------------------------------------------------------*/
void cckd_readahead (DEVBLK *dev, int trk)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             i, r;                   /* Indexes                   */
TID             tid;                    /* Readahead thread id       */

    cckd = dev->cckd_ext;

    if (cckdblk.ramax < 1 || cckdblk.readaheads < 1)
        return;

    obtain_lock (&cckdblk.ralock);

    /* Scan the cache to see if the tracks are already there */
    memset(cckd->ralkup, 0, sizeof(cckd->ralkup));
    cckd->ratrk = trk;
    cache_lock(CACHE_DEVBUF);
    cache_scan(CACHE_DEVBUF, cckd_readahead_scan, dev);
    cache_unlock(CACHE_DEVBUF);

    /* Scan the queue to see if the tracks are already there */
    for (r = cckdblk.ra1st; r >= 0; r = cckdblk.ra[r].next)
        if (cckdblk.ra[r].dev == dev)
        {
            i = cckdblk.ra[r].trk - trk;
            if (i > 0 && i <= cckdblk.readaheads)
                cckd->ralkup[i-1] = 1;
        }

    /* Queue the tracks to the readahead queue */
    for (i = 1; i <= cckdblk.readaheads && cckdblk.rafree >= 0; i++)
    {
        if (cckd->ralkup[i-1]) continue;
        if (trk + i >= dev->ckdtrks) break;
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
        cckdblk.ra[r].trk = trk + i;
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

int cckd_readahead_scan (int *answer, int ix, int i, void *data)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
U16             devnum;                 /* Cached device number      */
U32             trk;                    /* Cached track              */
DEVBLK         *dev = data;             /* -> device block           */
int             k;                      /* Index                     */

    UNREFERENCED(answer);
    UNREFERENCED(ix);
    cckd = dev->cckd_ext;
    CCKD_CACHE_GETKEY(i, devnum, trk);
    if (devnum == dev->devnum)
    {
        k = (int)trk - cckd->ratrk;
        if (k > 0 && k <= cckdblk.readaheads)
            cckd->ralkup[k-1] = 1;
    }
    return 0;
}

/*-------------------------------------------------------------------*/
/* Asynchronous readahead thread                                     */
/*-------------------------------------------------------------------*/
void cckd_ra ()
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
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
    logmsg (_("HHCCD001I Readahead thread %d started: tid="TIDPAT", pid=%d\n"),
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

        r = cckdblk.ra1st;
        trk = cckdblk.ra[r].trk;
        dev = cckdblk.ra[r].dev;
        cckd = dev->cckd_ext;

        /* Requeue the 1st entry to the readahead free queue */
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

        if (!cckd || cckd->stopping || cckd->merging) continue;

        cckd->ras++;
        release_lock (&cckdblk.ralock);

        /* Read the readahead track */
        cckd_read_trk (dev, trk, ra, NULL);

        obtain_lock (&cckdblk.ralock);
        cckd->ras--;
    }

    if (!cckdblk.batch)
    logmsg (_("HHCCD011I Readahead thread %d stopping: tid="TIDPAT", pid=%d\n"),
            ra, thread_id(), getpid());
    --cckdblk.ras;
    if (!cckdblk.ras) signal_condition(&cckdblk.termcond);
    release_lock(&cckdblk.ralock);

} /* end thread cckd_ra_thread */

/*-------------------------------------------------------------------*/
/* Flush updated cache entries for a device                          */
/*                                                                   */
/* Caller holds the cckd->iolock                                     */
/* cckdblk.wrlock then cache_lock is obtained and released           */
/*-------------------------------------------------------------------*/
void cckd_flush_cache(DEVBLK *dev)
{
int             rc;                     /* Return code               */
TID             tid;                    /* Writer thread id          */

    /* Scan cache for updated cache entries */
    obtain_lock (&cckdblk.wrlock);
    cache_lock (CACHE_DEVBUF);
    rc = cache_scan (CACHE_DEVBUF, cckd_flush_cache_scan, dev);
    cache_unlock (CACHE_DEVBUF);

    /* Schedule the writer if any writes are pending */
    if (cckdblk.wrpending)
    {
        if (cckdblk.wrwaiting)
            signal_condition (&cckdblk.wrcond);
        else if (cckdblk.wrs < cckdblk.wrmax)
            create_thread (&tid, NULL, cckd_writer, NULL);
    }
    release_lock (&cckdblk.wrlock);
}
int cckd_flush_cache_scan (int *answer, int ix, int i, void *data)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
U16             devnum;                 /* Cached device number      */
U32             trk;                    /* Cached track              */
DEVBLK         *dev = data;             /* -> device block           */

    UNREFERENCED(answer);
    cckd = dev->cckd_ext;
    CCKD_CACHE_GETKEY(i, devnum, trk);
    if ((cache_getflag(ix,i) & CACHE_BUSY) == CCKD_CACHE_UPDATED
     && dev->devnum == devnum)
    {
        cache_setflag (ix, i, ~CCKD_CACHE_UPDATED, CCKD_CACHE_WRITE);
        ++cckd->wrpending;
        ++cckdblk.wrpending;
        cckdtrc ("cckddasd: flush cache[%d] %4.4X trk %d\n",
                 i, devnum, trk);
    }
    return 0;
}
void cckd_flush_cache_all()
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
DEVBLK         *dev;                    /* -> device block           */

    cckd_lock_devchain(0);
    for (dev = cckdblk.dev1st; dev; dev = cckd->devnext)
    {
        cckd = dev->cckd_ext;
        obtain_lock (&cckd->iolock);
        if (!cckd->merging && !cckd->stopping)
            cckd_flush_cache(dev);
        release_lock (&cckd->iolock);
    }
    cckd_unlock_devchain();
}

/*-------------------------------------------------------------------*/
/* Purge cache entries for a device                                  */
/*                                                                   */
/* Caller holds the iolock                                           */
/* cache_lock is obtained and released                               */
/*-------------------------------------------------------------------*/
void cckd_purge_cache(DEVBLK *dev)
{

    /* Scan cache and purge entries */
    cache_lock (CACHE_DEVBUF);
    cache_scan (CACHE_DEVBUF, cckd_purge_cache_scan, dev);
    cache_unlock (CACHE_DEVBUF);
}
int cckd_purge_cache_scan (int *answer, int ix, int i, void *data)
{
U16             devnum;                 /* Cached device number      */
U32             trk;                    /* Cached track              */
DEVBLK         *dev = data;             /* -> device block           */

    UNREFERENCED(answer);
    CCKD_CACHE_GETKEY(i, devnum, trk);
    if (dev->devnum == devnum)
    {
        cache_release (ix, i, 0);
        cckdtrc ("cckddasd: purge cache[%d] %4.4X trk %d purged\n",
                 i, devnum, trk);
    }
    return 0;
}

/*-------------------------------------------------------------------*/
/* Writer thread                                                     */
/*-------------------------------------------------------------------*/
void cckd_writer()
{
DEVBLK         *dev;                    /* Device block              */
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             writer;                 /* Writer identifier         */
int             o;                      /* Cache entry found         */
U16             devnum;                 /* Device number             */
BYTE           *buf;                    /* Buffer                    */
BYTE           *bufp;                   /* Buffer to be written      */
int             len, bufl;              /* Buffer lengths            */
int             trk;                    /* Track number              */
int             comp;                   /* Compression algorithm     */
int             parm;                   /* Compression parameter     */
TID             tid;                    /* Writer thead id           */
U32             flag;                   /* Cache flag                */
static char    *compress[] = {"none", "zlib", "bzip2"};
BYTE            buf2[65536];            /* Compress buffer           */

#ifndef WIN32
    /* Set writer priority just below cpu priority to mimimize the
       compression effect */
    if(cckdblk.wrprio >= 0)
        setpriority (PRIO_PROCESS, 0, cckdblk.wrprio);
#endif

    obtain_lock (&cckdblk.wrlock);
    writer = ++cckdblk.wrs;

    /* Return without messages if too many already started */
    if (writer > cckdblk.wrmax && cckdblk.wrpending == 0)
    {
        --cckdblk.wrs;
        release_lock (&cckdblk.wrlock);
        return;
    }

    if (!cckdblk.batch)
    logmsg (_("HHCCD002I Writer thread %d started: tid="TIDPAT", pid=%d\n"),
            writer, thread_id(), getpid());

    while (writer <= cckdblk.wrmax || cckdblk.wrpending)
    {
        /* Wait for work */
        if (cckdblk.wrpending == 0)
        {
            cckdblk.wrwaiting++;
            wait_condition (&cckdblk.wrcond, &cckdblk.wrlock);
            cckdblk.wrwaiting--;
        }

        /* Scan the cache for the oldest pending write */
        cache_lock (CACHE_DEVBUF);
        o = cache_scan (CACHE_DEVBUF, cckd_writer_scan, NULL);

        /* Possibly shutting down if no writes pending */
        if (o < 0)
        {
            cache_unlock (CACHE_DEVBUF);
            cckdblk.wrpending = 0;
            continue;
        }
        cache_setflag (CACHE_DEVBUF, o, ~CCKD_CACHE_WRITE, CCKD_CACHE_WRITING);
        cache_unlock (CACHE_DEVBUF);

        /* Schedule the other writers if any writes are still pending */
        cckdblk.wrpending--;
        if (cckdblk.wrpending)
        {
            if (cckdblk.wrwaiting)
                signal_condition (&cckdblk.wrcond);
            else if (cckdblk.wrs < cckdblk.wrmax)
                create_thread (&tid, NULL, cckd_writer, NULL);
        }
        release_lock (&cckdblk.wrlock);

        /* Prepare to compress */
        CCKD_CACHE_GETKEY(o, devnum, trk);
        dev = cckd_find_device_by_devnum (devnum);
        cckd = dev->cckd_ext;
        buf = cache_getbuf(CACHE_DEVBUF, o, 0);
        len = cckd_trklen (dev, buf);
        comp = len < CCKD_COMPRESS_MIN ? CCKD_COMPRESS_NONE
             : cckdblk.comp == 0xff ? cckd->cdevhdr[cckd->sfn].compress
             : cckdblk.comp;
        parm = cckdblk.compparm < 0
             ? cckd->cdevhdr[cckd->sfn].compress_parm
             : cckdblk.compparm;

        /* Stress adjustments */
        if ((cache_waiters(CACHE_DEVBUF) || cache_busy(CACHE_DEVBUF) > 90)
         && !cckdblk.nostress)
        {
            cckdblk.stats_stresswrites++;
            comp = len < CCKD_STRESS_MINLEN ? 
                   CCKD_COMPRESS_NONE : CCKD_STRESS_COMP;
            parm = cache_busy(CACHE_DEVBUF) <= 95 ?
                   CCKD_STRESS_PARM1 : CCKD_STRESS_PARM2;
        }

        /* Compress the track image */
        cckdtrc ("cckddasd: %d wrtrk[%d] %d comp %s parm %d\n",
                  writer, o, trk, compress[comp], parm);
        bufp = (BYTE *)&buf2;
        bufl = cckd_compress(dev, &bufp, buf, len, comp, parm);
        cckdtrc ("cckddasd: %d wrtrk[%d] %d compressed length %d\n",
                  writer, o, trk, bufl);

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
        if (cckdblk.gcs < cckdblk.gcmax)
            create_thread (&tid, NULL, cckd_gcol, NULL);

        obtain_lock (&cckd->iolock);
        cache_lock (CACHE_DEVBUF);
        flag = cache_setflag (CACHE_DEVBUF, o, ~CCKD_CACHE_WRITING, 0);
        cache_unlock (CACHE_DEVBUF);
        cckd->wrpending--;
        if ((cckd->iowaiters || cckd-> gcwaiting) && ((flag & CCKD_CACHE_IOWAIT) || !cckd->wrpending))
        {   cckdtrc ("cckddasd: writer[%d] cache[%2.2d] %d signalling write complete\n",
                 writer, o, trk);
            broadcast_condition (&cckd->iocond);
        }
        release_lock(&cckd->iolock);

        cckdtrc ("cckddasd: %d wrtrk[%2.2d] %d complete flags:%8.8x\n",
                  writer, o, trk, cache_getflag(CACHE_DEVBUF,o));

        obtain_lock(&cckdblk.wrlock);
    }

    if (!cckdblk.batch)
    logmsg (_("HHCCD012I Writer thread %d stopping: tid="TIDPAT", pid=%d\n"),
            writer, thread_id(), getpid());
    cckdblk.wrs--;
    if (cckdblk.wrs == 0) signal_condition(&cckdblk.termcond);
    release_lock(&cckdblk.wrlock);
} /* end thread cckd_writer */

int cckd_writer_scan (int *o, int ix, int i, void *data)
{
    UNREFERENCED(data);
    if ((cache_getflag(ix,i) & DEVBUF_TYPE_COMP)
     && (cache_getflag(ix,i) & CCKD_CACHE_WRITE)
     && (*o == -1 || cache_getage(ix, i) < cache_getage(ix, *o)))
        *o = i;
    return 0;
}

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
        if (((U64)((U64)fpos + (U64)len)) > ((U64)4294967295ULL))
        {
            logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD102E file[%d] get space error, size exceeds 4G\n"), sfx);
            return -1;
        }

        rc = fstat (cckd->fd[sfx], &st);
        if (rc < 0)
        {
            logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD103E file[%d] get space fstat error, size %llx: %s\n"),
                   sfx, (long long)cckd->cdevhdr[sfx].size, strerror(errno));
            return -1;
        }

        if ((off_t)(fpos + len) > (off_t)st.st_size)
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
                    logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD104E truncate re-open error: %s\n"),
                            strerror(errno));
                    return -1;
                }
            }
            rc = ftruncate (cckd->fd[sfx], sz);

            /* Turns out certain linux file-systems (eg FAT) don't allow
               us to *increase* the size of the file using ftruncate
               (which implies `sparse' blocks or whatever).  So try to
               write zeroes if an error occurred. */
            if (rc < 0)
            {
                BYTE zbuf[1024];
                size_t l = sz - st.st_size;
                off_t rcoff = lseek (cckd->fd[sfx], (off_t)st.st_size, SEEK_SET);
                if (rcoff >= 0)
                {
                    memset(zbuf, 0, sizeof(zbuf));
                    do {
                        rc = write (cckd->fd[sfx], zbuf, l < sizeof(zbuf) ? l : sizeof(zbuf));
                        l -= l < sizeof(zbuf) ? l : sizeof(zbuf);
                    } while (rc > 0 && l > 0);
                }
            }

            if (rc < 0)
            {
                logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD105E file[%d] get space ftruncate error, size %llx: %s\n"),
                   sfx, (long long)cckd->cdevhdr[sfx].size, strerror(errno));
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
    if (cckd->cdevhdr[sfx].free_number == 0) return;

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
        logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD106E file[%d] hdr lseek error, offset %llx: %s\n"),
                sfx, (long long)CKDDASD_DEVHDR_SIZE, strerror(errno));
        return -1;
    }

    /* read the compressed device header */
    rc = read (cckd->fd[sfx], &cckd->cdevhdr[sfx], CCKDDASD_DEVHDR_SIZE);
    if (rc != CCKDDASD_DEVHDR_SIZE)
    {
        logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD107E file[%d] chdr read error, offset %llx: %s\n"),
                sfx, (long long)CKDDASD_DEVHDR_SIZE, strerror(errno));
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
            rc = cckd_swapend (cckd->fd[sfx], stdout);
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
        logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD108E file[%d] chdr lseek error, offset %llx: %s\n"),
                sfx, (long long)CKDDASD_DEVHDR_SIZE, strerror(errno));
        return -1;
    }
    rc = write (cckd->fd[sfx], &cckd->cdevhdr[sfx], CCKDDASD_DEVHDR_SIZE);
    if (rc < CCKDDASD_DEVHDR_SIZE)
    {
        logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD109E file[%d] chdr write error, offset %llx: %s\n"),
                sfx, (long long)CKDDASD_DEVHDR_SIZE, strerror(errno));
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
        logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD110E l1 table malloc error: %s\n"),
                strerror(errno));
        return -1;
    }

    /* read the level 1 table */
    rcoff = lseek (cckd->fd[sfx], CCKD_L1TAB_POS, SEEK_SET);
    if (rcoff < 0)
    {
        logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD111E file[%d] l1 lseek error, offset %llx: %s\n"),
                sfx, (long long)CCKD_L1TAB_POS, strerror(errno));
        return -1;
    }
    rc = read(cckd->fd[sfx], cckd->l1[sfx], len);
    if (rc != len)
    {
        logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD112E file[%d] l1 read error, offset %llx: %s\n"),
                sfx, (long long)CCKD_L1TAB_POS, strerror(errno));
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
        logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD113E file[%d] l1 lseek error, offset %llx: %s\n"),
                sfx, (long long)CCKD_L1TAB_POS, strerror(errno));
        return -1;
    }
    rc = write (cckd->fd[sfx], cckd->l1[sfx], len);
    if (rc != len)
    {
        logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD114E file[%d] l1 write error, offset %llx: %s\n"),
                sfx, (long long)CCKD_L1TAB_POS, strerror(errno));
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
        logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD115E file[%d] l1[%d] lseek error, offset %llx: %s\n"),
                sfx, l1x, (long long)l1pos, strerror(errno));
        return -1;
    }
    rc = write (cckd->fd[sfx], &cckd->l1[sfx][l1x], CCKD_L1ENT_SIZE);
    if (rc != CCKD_L1ENT_SIZE)
    {
        logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD116E file[%d] l1[%d] write error, offset %llx: %s\n"),
                sfx, l1x, (long long)l1pos, strerror(errno));
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
        logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD117E file[%d] devhdr lseek error, offset %llx: %s\n"),
                sfx, (long long)0, strerror(errno));
        return -1;
    }
    rc = read (cckd->fd[sfx], &devhdr, CKDDASD_DEVHDR_SIZE);
    if (rc != CKDDASD_DEVHDR_SIZE)
    {
        logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD118E file[%d] devhdr read error, offset %llx: %s\n"),
                sfx, (long long)0, strerror(errno));
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
        logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD119E file[%d] devhdr id error\n"),
                sfx);
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
        logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD120E calloc failed for free space, size %d: %s\n"),
                cckd->freenbr * CCKD_FREEBLK_ISIZE,
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
            logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD121E file[%d] free space lseek error, offset %llx: %s\n"),
                   sfx, (long long)fpos, strerror(errno));
            return -1;
        }
        rc = read (cckd->fd[sfx], &cckd->free[0], CCKD_FREEBLK_SIZE);
        if (rc < CCKD_FREEBLK_SIZE)
        {
            logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD122E file[%d] free space read error, offset %llx: %d,%d %s\n"),
                   sfx, (long long)fpos, rc, CCKD_FREEBLK_SIZE, strerror(errno));
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
                logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD123E file[%d] free space ftruncate error, size %llx: %s\n"),
                       sfx, (long long)cckd->cdevhdr[sfx].size, strerror(errno));
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
                logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD124E file[%d] free space lseek error, offset %llx: %s\n"),
                       sfx, (long long)fpos, strerror(errno));
                return -1;
            }
            rc = read (cckd->fd[sfx], &cckd->free[i], CCKD_FREEBLK_SIZE);
            if (rc < CCKD_FREEBLK_SIZE)
            {
                logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD125E file[%d] free space read error, offset %llx: %d,%d,%d %s\n"),
                       sfx, (long long)fpos, rc, CCKD_FREEBLK_SIZE, errno, strerror(errno));
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
            logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD126E file[%d] free space lseek error, offset %llx: %s\n"),
                   sfx, (long long)fpos, strerror(errno));
            return -1;
        }
        rc = write (cckd->fd[sfx], &cckd->free[i], CCKD_FREEBLK_SIZE);
        if (rc < CCKD_FREEBLK_SIZE)
        {
            logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD127E file[%d] free space write error, offset %llx: %s\n"),
                   sfx, (long long)fpos, strerror(errno));
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
int             fnd;                    /* Found cache               */
int             lru;                    /* Oldest available cache    */
CCKD_L2ENT     *buf;                    /* -> Cache buffer           */

    cckd = dev->cckd_ext;

    /* return if table is already active */
    if (sfx == cckd->sfx && l1x == cckd->l1x) return 0;

    cache_lock(CACHE_L2);

    /* Inactivate the previous entry */
    if (cckd->l2active >= 0)
        cache_setflag(CACHE_L2, cckd->l2active, ~L2_CACHE_ACTIVE, 0);
    cckd->l2 = NULL;
    cckd->l2active = cckd->sfx = cckd->l1x = -1;

    /* scan the cache array for the l2tab */
    fnd = cache_lookup (CACHE_L2, L2_CACHE_SETKEY(sfx, dev->devnum, l1x), &lru);

    /* check for level 2 cache hit */
    if (fnd >= 0)
    {
        cckdtrc ("cckddasd: l2[%d,%d] cache[%d] hit\n", sfx, l1x, fnd);
        cache_setflag (CACHE_L2, fnd, 0, L2_CACHE_ACTIVE);
        cache_setage (CACHE_L2, fnd);
        cckdblk.stats_l2cachehits++;
        cache_unlock (CACHE_L2);
        cckd->sfx = sfx;
        cckd->l1x = l1x;
        cckd->l2 = cache_getbuf(CACHE_L2, fnd, 0);
        cckd->l2active = fnd;
        return 1;
    }

    cckdtrc ("cckddasd: l2[%d,%d] cache[%d] miss\n", sfx, l1x, lru);

    /* Steal an entry if all are busy */
    if (lru < 0) lru = cckd_steal_l2();

    /* Make the entry active */
    cache_setkey (CACHE_L2, lru, L2_CACHE_SETKEY(sfx, dev->devnum, l1x));
    cache_setflag (CACHE_L2, lru, 0, L2_CACHE_ACTIVE);
    cache_setage (CACHE_L2, lru);
    buf = cache_getbuf(CACHE_L2, lru, CCKD_L2TAB_SIZE);
    cckdblk.stats_l2cachemisses++;
    cache_unlock (CACHE_L2);
    if (buf == NULL) return -1;

    /* check for null table */
    if (!cckd->l1[sfx][l1x] || cckd->l1[sfx][l1x] == 0xffffffff)
    {
        memset (buf, cckd->l1[sfx][l1x] & 0xff, CCKD_L2TAB_SIZE);
        cckdtrc ("cckddasd: l2[%d,%d] cache[%d] null\n", sfx, l1x, lru);
    }
    /* read the new level 2 table */
    else
    {
        rcoff = lseek (cckd->fd[sfx], (off_t)cckd->l1[sfx][l1x], SEEK_SET);
        if (rcoff < 0)
        {
            logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD128E file[%d] l2[%d] lseek error offset %lld: %s\n"),
                    sfx, l1x, (long long)cckd->l1[sfx][l1x],
                    strerror(errno));
            cache_lock(CACHE_L2);
            cache_setflag(CACHE_L2, lru, 0, 0);
            cache_unlock(CACHE_L2);
            return -1;
        }
        rc = read (cckd->fd[sfx], buf, CCKD_L2TAB_SIZE);
        if (rc < CCKD_L2TAB_SIZE)
        {
            logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD129E file[%d] l2[%d] read error offset %lld: %s\n"),
                    sfx, l1x, (long long)cckd->l1[sfx][l1x],
                    strerror(errno));
            cache_lock(CACHE_L2);
            cache_setflag(CACHE_L2, lru, 0, 0);
            cache_unlock(CACHE_L2);
            return -1;
        }
        if (cckd->swapend[sfx])
            cckd_swapend_l2 (buf);
        cckdtrc ("cckddasd: file[%d] cache[%d] l2[%d] read offset 0x%llx\n",
                 sfx, lru, l1x, (long long)cckd->l1[sfx][l1x]);
        cckd->l2reads[sfx]++;
        cckd->totl2reads++;
        cckdblk.stats_l2reads++;
    }

    cckd->sfx = sfx;
    cckd->l1x = l1x;
    cckd->l2 = buf;
    cckd->l2active = lru;

    return 0;

} /* end function cckd_read_l2 */

/*-------------------------------------------------------------------*/
/* Purge all l2tab cache entries for a given device                  */
/*-------------------------------------------------------------------*/
void cckd_purge_l2 (DEVBLK *dev)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */

    cckd = dev->cckd_ext;

    cache_lock (CACHE_L2);
    cckd->l2active = cckd->sfx = cckd->l1x = -1;
    cckd->l2 = NULL;
    cache_scan (CACHE_L2, cckd_purge_l2_scan, dev);
    cache_unlock (CACHE_L2);
}
int cckd_purge_l2_scan (int *answer, int ix, int i, void *data)
{
U16             sfx;                    /* Cached suffix             */
U16             devnum;                 /* Cached device number      */
U32             l1x;                    /* Cached level 1 index      */
DEVBLK         *dev = data;             /* -> device block           */

    UNREFERENCED(answer);
    L2_CACHE_GETKEY(i, sfx, devnum, l1x);
    if (dev == NULL || devnum == dev->devnum)
    {
        cckdtrc ("cckddasd: purge l2cache[%d] %4.4X sfx %d ix %d purged\n",
                 i, devnum, sfx, l1x);
        cache_release(ix, i, 0);
    }
    return 0;
}

/*-------------------------------------------------------------------*/
/* Steal an l2tab cache entry                                        */
/*-------------------------------------------------------------------*/
int cckd_steal_l2 ()
{
DEVBLK         *dev;                    /* -> device block           */
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             i;                      /* Stolen cache index        */
U16             sfx;                    /* Cached suffix             */
U16             devnum;                 /* Cached device number      */
U32             l1x;                    /* Cached level 1 index      */

    i = cache_scan (CACHE_L2, cckd_steal_l2_scan, NULL);
    L2_CACHE_GETKEY(i, sfx, devnum, l1x);
    dev = cckd_find_device_by_devnum(devnum);
    cckd = dev->cckd_ext;
    cckd->l2active = cckd->sfx = cckd->l1x = -1;
    cckd->l2 = NULL;
    cache_release(CACHE_L2, i, 0);
    return i;
}
int cckd_steal_l2_scan (int *answer, int ix, int i, void *data)
{
    UNREFERENCED(data);
    if (*answer < 0) *answer = i;
    else if (cache_getage(ix, i) < cache_getage(ix, *answer))
        *answer = i;
    return 0;
}

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
        logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD130E file[%d] l2[%d] lseek error offset %lld: %s\n"),
                sfx, l1x, (long long)l2pos, strerror(errno));
        return -1;
    }
    rc = write (cckd->fd[sfx], cckd->l2, CCKD_L2TAB_SIZE);
    if (rc < CCKD_L2TAB_SIZE)
    {
        logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD131E file[%d] l2[%d] write error offset %lld: %s\n"),
                sfx, l1x, (long long)l2pos, strerror(errno));
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
        logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD132E file[%d] l2[%d,%d] lseek error offset %lld: %s\n"),
                sfx, l1x, l2x, (long long)l2pos, strerror(errno));
        return -1;
    }
    rc = write (cckd->fd[sfx], &cckd->l2[l2x], CCKD_L2ENT_SIZE);
    if (rc < CCKD_L2ENT_SIZE)
    {
        logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD133E file[%d] l2[%d,%d] write error offset %lld: %s\n"),
                sfx, l1x, l2x, (long long)l2pos, strerror(errno));
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
            logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD134E file[%d] trk %d lseek error offset %llx: %s\n"),
                    sfx, trk, (long long)l2.pos, strerror(errno));
            goto cckd_read_trkimg_error;
        }
        rc = read (cckd->fd[sfx], buf, l2.len);
        if (rc < l2.len)
        {
            logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD135E file[%d] trk %d read error offset %llx: %s\n"),
                    sfx, trk, (long long)l2.pos, strerror(errno));
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
        logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD136E file[%d] trk %d not written, invalid format\n"),
                sfx, trk);
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
            logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD137E file[%d] trk %d lseek error offset %llx: %s\n"),
                    sfx, trk, (long long)l2.pos, strerror(errno));
            return -1;
        }
        rc = write (cckd->fd[sfx], buf, len);
        if (rc < len)
        {
            logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD138E file[%d] trk %d write error offset %llx len %d rc %d: %s\n"),
                    sfx, trk, (long long)l2.pos, len, rc, strerror(errno));
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
        logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD139E trklen err for %2.2x%2.2x%2.2x%2.2x%2.2x\n"),
                buf[0], buf[1], buf[2], buf[3], buf[4]);
        size = -1;
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
        logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD140E truncate fstat error: %s\n"),
                strerror(errno));
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
                logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD141E truncate re-open error: %s\n"),
                        strerror(errno));
                return;
            }
        }

        rc = ftruncate (cckd->fd[sfx], (off_t)cckd->cdevhdr[sfx].size);
        if (rc < 0)
        {
            logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD142E truncate ftruncate error: %s\n"),
                    strerror(errno));
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
BYTE            badcomp=0;              /* 1=Unsupported compression */
static char    *comp[] = {"none", "zlib", "bzip2"};

    cckd = dev->cckd_ext;

    /* CKD dasd header verification */
    if (cckd->ckddasd)
    {
        cyl = fetch_hw (buf + 1);
        head = fetch_hw (buf + 3);
        t = cyl * dev->ckdheads + head;

        if (cyl < dev->ckdcyls && head < dev->ckdheads
         && (trk == -1 || t == trk))
        {
            if (buf[0] & ~cckdblk.comps)
            {
                if (buf[0] & ~CCKD_COMPRESS_MASK)
                {
                    if (cckdblk.bytemsgs++ < 10)
                        logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD143E invalid byte 0 trk %d: "
                            "buf %2.2x%2.2x%2.2x%2.2x%2.2x\n"), dev->devnum,
                            t, buf[0],buf[1],buf[2],buf[3],buf[4]);
                    buf[0] &= CCKD_COMPRESS_MASK;
                } 
            }
            if (buf[0] & ~cckdblk.comps)
                badcomp = 1;
            else
                return t;
        }
    }
    /* FBA dasd header verification */
    else
    {
        t = fetch_fw (buf + 1);
        if (t < dev->fbanumblk && (trk == -1 || t == trk))
        {
            if (buf[0] & ~cckdblk.comps)
            {
                if (buf[0] & ~CCKD_COMPRESS_MASK)
                {
                    logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD144E invalid byte 0 blkgrp %d: "
                            "buf %2.2x%2.2x%2.2x%2.2x%2.2x\n"), dev->devnum,
                            t, buf[0],buf[1],buf[2],buf[3],buf[4]);
                    buf[0] &= CCKD_COMPRESS_MASK;
                } 
            }
            if (buf[0] & ~cckdblk.comps)
                badcomp = 1;
            else
                return t;
        }
    }

    if (badcomp)
    {
        logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD145E invalid %s hdr %s %d: "
                "%s compression unsupported\n"),
                cckd->ckddasd ? "trk" : "blk",
                cckd->ckddasd ? "trk" : "blk", t, comp[buf[0]]);
    }
    else
    {
        logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD146E invalid %s hdr %s %d "
                "buf %2.2x%2.2x%2.2x%2.2x%2.2x\n"),
                cckd->ckddasd ? "trk" : "blk",
                cckd->ckddasd ? "trk" : "blk", trk,
                buf[0], buf[1], buf[2], buf[3], buf[4]);
        cckd_print_itrace ();
    }

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
int             vlen;                   /* Validation length         */
int             kl,dl;                  /* Key/Data lengths          */

    cckd = dev->cckd_ext;

    if (buf == NULL || len < 0) return -1;

    cckdtrc ("cckddasd: validating %s %d len %d %2.2x%2.2x%2.2x%2.2x%2.2x "
             "%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x\n",
             cckd->ckddasd ? "trk" : "blkgrp", trk, len,
             buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6],
             buf[7], buf[8], buf[9], buf[10], buf[11], buf[12]);

    /* FBA dasd check */
    if (cckd->fbadasd)
    {
        if (len == CFBA_BLOCK_SIZE + CKDDASD_TRKHDR_SIZE || len == 0)
            return 0;
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
    if (/* memcmp (cchh, cchh2, 4) != 0 || */ buf[9]  != 0 ||
        buf[10] != 0 || buf[11] != 0 || buf[12] != 8)
    {
        cckdtrc ("cckddasd: validation failed: bad r0%s\n","");
        return -1;
    }

    /* validate records 1 thru n */
    vlen = len > 0 ? len : dev->ckdtrksz;
    for (r = 1, sz = 21; sz + 8 <= vlen; sz += 8 + kl + dl, r++)
    {
        if (memcmp (&buf[sz], eighthexFF, 8) == 0) break;
        kl = buf[sz+5];
        dl = buf[sz+6] * 256 + buf[sz+7];
        /* fix for track overflow bit */
        memcpy (cchh2, &buf[sz], 4); cchh2[0] &= 0x7f;

        /* fix for funny formatted vm disks */
        /*
        if (r == 1) memcpy (cchh, cchh2, 4);
        */

        if (/*memcmp (cchh, cchh2, 4) != 0 ||*/ buf[sz+4] == 0 ||
            sz + 8 + kl + dl >= vlen)
        {
            cckdtrc ("cckddasd: validation failed: bad r%d "
                 "%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x\n",
                 r, buf[sz], buf[sz+1], buf[sz+2], buf[sz+3],
                 buf[sz+4], buf[sz+5], buf[sz+6], buf[sz+7]);
             return -1;
        }
    }
    sz += 8;

    if ((sz != len && len > 0) || sz > vlen)
    {
        cckdtrc ("cckddasd: validation failed: no eot%s\n","");
        return -1;
    }

    return sz;

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
        logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD147E no shadow file name specified\n"));
        return -1;
    }

    /* Error if number shadow files exceeded */
    if (sfx > CCKD_MAX_SF)
    {
        logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD148E [%d] number of shadow files exceeded: %d\n"),
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
                    logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD149E shadow file[%d] name %s\n"
                            "      collides with %4.4X file[%d] name %s\n"),
                            i, sfn, dev2->devnum, j, sfn2);
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
        rc = cckd_chkdsk (cckd->fd[cckd->sfn], stdout, 0);
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
            logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD150E error re-opening %s readonly\n  %s\n"),
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
    {    logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD151E shadow file[%d] open error: %s\n"),
                 sfx, strerror (errno));
         return -1;
    }

    /* build the device header */
    rcoff = lseek (cckd->fd[sfx-1], 0, SEEK_SET);
    if (rcoff < 0)
    {
        logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD152E file[%d] lseek error offset %d: %s\n"),
                sfx-1, 0, strerror(errno));
        close (sfd);
        return -1;
    }
    rc = read (cckd->fd[sfx-1], &devhdr, CKDDASD_DEVHDR_SIZE);
    if (rc < CKDDASD_DEVHDR_SIZE)
    {
        logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD153E file[%d] read error offset %d: %s\n"),
                sfx-1, 0, strerror(errno));
        close (sfd);
        return -1;
    }
    if (cckd->ckddasd) memcpy (&devhdr.devid, "CKD_S370", 8);
    else memcpy (&devhdr.devid, "FBA_S370", 8);
    rc = write (sfd, &devhdr, CKDDASD_DEVHDR_SIZE);
    if (rc < CKDDASD_DEVHDR_SIZE)
    {
        logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD154E shadow file[%d] write error offset %d: %s\n"),
                sfx, 0, strerror(errno));
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
        logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD155E file[%d] l1 malloc failed: %s\n"),
                sfx, strerror(errno));
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
int             syncio;                 /* Saved syncio bit          */
int             rc;                     /* Return code               */
BYTE            sfn[256];               /* Shadow file name          */

    cckd = dev->cckd_ext;
    if (!cckd)
    {
        logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD156E not a cckd device\n"), dev->devnum);
        return;
    }
    
    /* Disable synchronous I/O for the device */
    syncio = cckd_disable_syncio(dev);

    /* schedule updated track entries to be written */
    obtain_lock (&cckd->iolock);
    cckd_flush_cache (dev);
    while (cckd->wrpending || cckd->ioactive)
    {
        cckd->iowaiters++;
        wait_condition (&cckd->iocond, &cckd->iolock);
        cckd->iowaiters--;
        cckd_flush_cache (dev);
    }
    cckd_purge_cache (dev); cckd_purge_l2 (dev);
    dev->bufcur = dev->cache = -1;
    cckd->merging = 1;
    release_lock (&cckd->iolock);

    /* obtain control of the file */
    obtain_lock (&cckd->filelock);

    /* harden the current file */
    cckd_harden (dev);

    /* create a new shadow file */
    rc = cckd_sf_new (dev);
    if (rc < 0)
    {
        logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD157E file[%d] error adding shadow file: %s\n"),
                cckd->sfn  + 1, strerror(errno));
        release_lock (&cckd->filelock);
        obtain_lock (&cckd->iolock);
        cckd->merging = 0;
        if (cckd->iowaiters)
            broadcast_condition (&cckd->iocond);
        dev->syncio = syncio;
        release_lock (&cckd->iolock);
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
    logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD158E file[%d] %s added\n"), cckd->sfn, sfn);
    release_lock (&cckd->filelock);

    obtain_lock (&cckd->iolock);
    cckd->merging = 0;
    if (cckd->iowaiters)
        broadcast_condition (&cckd->iocond);
    dev->syncio = syncio;
    release_lock (&cckd->iolock);

    cckd_sf_stats (dev);

} /* end function cckd_sf_add */

/*-------------------------------------------------------------------*/
/* Remove a shadow file  (sf-)                                       */
/*-------------------------------------------------------------------*/
void cckd_sf_remove (DEVBLK *dev, int merge)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             syncio;                 /* Saved syncio bit          */
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
        logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD159E not a cckd device\n"),dev->devnum);
        return;
    }

    if (!cckd->sfn)
    {
        logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD160E cannot remove base file\n"),dev->devnum);
        return;
    }

    cckdtrc("cckddasd: merge starting%s\n","");
    
    /* Disable synchronous I/O for the device */
    syncio = cckd_disable_syncio(dev);

    /* Schedule updated track entries to be written */
    obtain_lock (&cckd->iolock);
    cckd_flush_cache (dev);
    while (cckd->wrpending || cckd->ioactive)
    {
        cckd->iowaiters++;
        wait_condition (&cckd->iocond, &cckd->iolock);
        cckd->iowaiters--;
        cckd_flush_cache (dev);
    }
    cckd_purge_cache (dev); cckd_purge_l2 (dev);
    dev->bufcur = dev->cache = -1;
    cckd->merging = 1;
    release_lock (&cckd->iolock);

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
            logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD161E file[%d] not merged, "
                    "file[%d] cannot be opened read-write\n"),
                    sfx[0], sfx[1]);
            goto sf_remove_exit;
        }
        else add = 1;
    }
    else
    {
        rc = cckd_chkdsk (cckd->fd[sfx[1]], stdout, 0);
        if (rc < 0)
        {
            logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD162E file[%d] not merged, "
                    "file[%d] check failed\n"),
                    sfx[0], sfx[1]);
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
            logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD163E file[%d] not merged, "
                    "file[%d] not hardened\n"),
                    sfx[0], sfx[0]);
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
                    logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD164E file[%d] l2[%d] lseek error offset %lld: %s\n"),
                            sfx[0], i, (long long)cckd->l1[sfx[0]][i],
                            strerror(errno));
                    err = 1;
                    continue;
                }
                rc = read (cckd->fd[sfx[0]], &l2[0], CCKD_L2TAB_SIZE);
                if (rc < CCKD_L2TAB_SIZE)
                {
                    logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD165E file[%d] l2[%d] read error offset %lld: %s\n"),
                            sfx[0], i, (long long)cckd->l1[sfx[0]][i],
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
                    logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD166E file[%d] l2[%d] lseek error offset %lld: %s\n"),
                            sfx[1], i, (long long)cckd->l1[sfx[1]][i],
                            strerror(errno));
                    err = 1;
                    continue;
                }
                rc = read (cckd->fd[sfx[1]], &l2[1], CCKD_L2TAB_SIZE);
                if (rc < CCKD_L2TAB_SIZE)
                {
                    logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD167E file[%d] l2[%d] read error offset %lld: %s\n"),
                            sfx[1], i, (long long)cckd->l1[sfx[1]][i],
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
                        logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD168E file[%d] %s[%d] lseek error "
                                " offset %lld: %s\n"),
                                sfx[0], cckd->ckddasd ? "trk" : "blkgrp",
                                i * 256 + j, (long long)l2[0][j].pos,
                                strerror(errno));
                        logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD169E file[%d] %s[%d] not merged\n"),
                                sfx[0], cckd->ckddasd ? "trk" : "blkgrp",
                                i * 256 + j);
                        err = 1;
                        continue;
                    }
                    rc = read (cckd->fd[sfx[0]], &buf, (size_t)l2[0][j].len);
                    if (rc != (int)l2[0][j].len)
                    {
                        logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD170E file[%d] %s[%d] read error "
                                " offset %lld: %s\n"),
                                sfx[0], cckd->ckddasd ? "trk" : "blkgrp",
                                i * 256 + j, (long long)l2[0][j].pos,
                                strerror(errno));
                        logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD171E file[%d] %s[%d] not merged\n"),
                                sfx[0], cckd->ckddasd ? "trk" : "blkgrp",
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
                    cckdtrc ("cckddasd: merging trk[%d] to 0x%llx\n",
                         i * 256 + j, (long long)pos);

                    rcoff = lseek (cckd->fd[sfx[1]], pos, SEEK_SET);
                    if (rcoff < 0)
                    {
                        logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD172E file[%d] %s[%d] lseek error "
                                " offset %lld: %s\n"),
                                sfx[1], cckd->ckddasd ? "trk" : "blkgrp",
                                i * 256 + j, (long long)pos, strerror(errno));
                        logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD173E file[%d] %s[%d] not merged\n"),
                                sfx[0], cckd->ckddasd ? "trk" : "blkgrp",
                                i * 256 + j);
                        err = 1;
                        continue;
                    }
                    rc = write (cckd->fd[sfx[1]], &buf, (size_t)l2[0][j].len);
                    if (rc != (int)l2[0][j].len)
                    {
                        logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD174E file[%d] %s[%d] write error "
                                " offset %lld: %s\n"),
                                sfx[1], cckd->ckddasd ? "trk" : "blkgrp",
                                i * 256, (long long)pos, strerror(errno));
                        logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD175E file[%d] %s[%d] not merged\n"),
                                sfx[0], cckd->ckddasd ? "trk" : "blkgrp",
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
                    if (pos == 0 || pos == (off_t)0xffffffff)
                        pos = cckd_get_space (dev, CCKD_L2TAB_SIZE);

                    cckdtrc ("cckddasd: merging l2[%d] to %llx\n",
                             i, (long long)pos);

                    rcoff = lseek (cckd->fd[sfx[1]], pos, SEEK_SET);
                    if (rcoff < 0)
                    {
                        logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD176E file[%d] l1[%d] lseek error "
                                " offset %lld: %s\n"),
                                sfx[1], i, (long long)pos,
                                strerror(errno));
                        logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD177E file[%d] %s[%d-%d] not merged\n"),
                                sfx[0], cckd->ckddasd ? "trks" : "blkgrps",
                                i * 256, i * 256 +255);
                        err = 1;
                        continue;
                    }
                    rc = write (cckd->fd[sfx[1]], &l2[1], CCKD_L2TAB_SIZE);
                    if (rc != CCKD_L2TAB_SIZE)
                    {
                        logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD178E file[%d] l1[%d] write error "
                                " offset %lld: %s\n"),
                                sfx[1], i, (long long)pos,
                                strerror(errno));
                        logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD179E file[%d] %s[%d-%d] not merged\n"),
                                sfx[0], cckd->ckddasd ? "trks" : "blkgrps",
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
            cckd_chkdsk (cckd->fd[sfx[1]], stdout, 2);
        cckd_read_init (dev);

    } /* if merge */

    /* Remove the old file */
//FIXME: unlink doesn't free space ??
    if (!cckdblk.ftruncwa) ftruncate(cckd->fd[sfx[0]], 0);
    close (cckd->fd[sfx[0]]);
    free (cckd->l1[sfx[0]]);
    cckd->l1[sfx[0]] = NULL;
    memset (&cckd->cdevhdr[sfx[0]], 0, CCKDDASD_DEVHDR_SIZE); 
    cckd_sf_name (dev, sfx[0], (char *)&sfn);
    rc = unlink ((char *)&sfn);

    /* Add the file back if necessary */
    if (add) rc = cckd_sf_new (dev) ;

    logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD200I shadow file [%d] successfully %s%s\n"),
            sfx[0], merge ? "merged" : add ? "re-added" : "removed",
            err ? " with errors" : "");

sf_remove_exit:
    release_lock (&cckd->filelock);

    obtain_lock (&cckd->iolock);
    cckd->merging = 0;
    if (cckd->iowaiters)
        broadcast_condition (&cckd->iocond);
    dev->syncio = syncio;
    cckdtrc("cckddasd: merge complete%s\n","");
    release_lock (&cckd->iolock);

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
        logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD201W device is not a shadow file\n"));
        return;
    }
    if (CCKD_MAX_SF == 0)
    {
        logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD202W file shadowing not activated\n"));
        return;
    }

    obtain_lock (&cckd->filelock);

    if (cckd->sfn)
    {
        logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD203W shadowing is already active\n"));
        release_lock (&cckd->filelock);
        return;
    }

    strcpy ((char *)&dev->dasdsfn, (const char *)sfn);
    logmsg (_("HHCCD204I shadow file name set to %s\n"), sfn);
    release_lock (&cckd->filelock);

} /* end function cckd_sf_newname */

/*-------------------------------------------------------------------*/
/* Check and compress a shadow file  (sfc)                           */
/*-------------------------------------------------------------------*/
void cckd_sf_comp (DEVBLK *dev)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             syncio;                 /* Saved syncio bit          */
int             rc;                     /* Return code               */

    cckd = dev->cckd_ext;
    if (!cckd)
    {
        logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD205W device is not a shadow file\n"));
        return;
    }
    
    /* Disable synchronous I/O for the device */
    syncio = cckd_disable_syncio(dev);

    /* schedule updated track entries to be written */
    obtain_lock (&cckd->iolock);
    cckd_flush_cache (dev);
    while (cckd->wrpending || cckd->ioactive)
    {
        cckd->iowaiters++;
        wait_condition (&cckd->iocond, &cckd->iolock);
        cckd->iowaiters--;
        cckd_flush_cache (dev);
    }
    cckd_purge_cache (dev); cckd_purge_l2 (dev);
    dev->bufcur = dev->cache = -1;
    cckd->merging = 1;
    release_lock (&cckd->iolock);

    /* obtain control of the file */
    obtain_lock (&cckd->filelock);

    /* harden the current file */
    cckd_harden (dev);

    /* Call the compress function */
    rc = cckd_comp (cckd->fd[cckd->sfn], stdout);

    /* Perform initial read */
    rc = cckd_read_init (dev);

    release_lock (&cckd->filelock);

    obtain_lock (&cckd->iolock);
    cckd->merging = 0;
    if (cckd->iowaiters)
        broadcast_condition (&cckd->iocond);
    dev->syncio = syncio;
    release_lock (&cckd->iolock);

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
        logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD206W device is not a shadow file\n"));
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
    logmsg (_("HHCCD210I           size free  nbr st  reads  writes l2reads    hits switches\n"));
    if (cckd->readaheads || cckd->misses)
    logmsg (_("HHCCD211I                                                 readaheads   misses\n"));
    logmsg (_("HHCCD212I -------------------------------------------------------------------\n"));

    /* total statistics */
    logmsg (_("HHCCD213I [*] %10lld %3lld%% %4d   %7d %7d %7d %7d  %7d\n"),
            size, (free * 100) / size, freenbr,
            cckd->totreads, cckd->totwrites, cckd->totl2reads,
            cckd->cachehits, cckd->switches);
    if (cckd->readaheads || cckd->misses)
    logmsg (_("HHCCD214I                                                    %7d  %7d\n"),
            cckd->readaheads, cckd->misses);

    /* base file statistics */
    logmsg (_("HHCCD215I %s\n"), dev->filename);
    logmsg (_("HHCCD216I [0] %10lld %3lld%% %4d %s %7d %7d %7d\n"),
            (long long)st.st_size,
            (long long)((long long)((long long)cckd->cdevhdr[0].free_total * 100) / st.st_size),
            cckd->cdevhdr[0].free_number, ost[cckd->open[0]],
            cckd->reads[0], cckd->writes[0], cckd->l2reads[0]);

    if (dev->dasdsfn[0] && CCKD_MAX_SF > 0)
    {
        cckd_sf_name ( dev, -1, (char *)&sfn);
        logmsg (_("HHCCD217I %s\n"), sfn);
    }

    /* shadow file statistics */
    for (i = 1; i <= cckd->sfn; i++)
    {
        logmsg (_("HHCCD218I [%d] %10lld %3lld%% %4d %s %7d %7d %7d\n"),
                i, (long long)cckd->cdevhdr[i].size,
                (long long)((long long)((long long)cckd->cdevhdr[i].free_total * 100) / cckd->cdevhdr[i].size),
                cckd->cdevhdr[i].free_number, ost[cckd->open[i]],
                cckd->reads[i], cckd->writes[i], cckd->l2reads[i]);
    }
//  release_lock (&cckd->filelock);
} /* end function cckd_sf_stats */

/*-------------------------------------------------------------------*/
/* Disable synchronous I/O for a device                              */
/*-------------------------------------------------------------------*/
int cckd_disable_syncio(DEVBLK *dev)
{
    if (!dev->syncio) return 0;
    obtain_lock(&dev->lock);
    while (dev->syncio_active)
    {
        release_lock(&dev->lock);
        usleep(1);
        obtain_lock(&dev->lock);
    }
    dev->syncio = 0;
    release_lock(&dev->lock);
    cckdtrc ("cckddasd: syncio disabled%s\n","");
    return 1;
}

/*-------------------------------------------------------------------*/
/* Lock/unlock the device chain                                      */
/*-------------------------------------------------------------------*/
void cckd_lock_devchain(int flag)
{
//struct timespec tm;
//struct timeval  now;
//int             timeout;

    obtain_lock(&cckdblk.devlock);
    while ((flag && cckdblk.devusers != 0)
        || (!flag && cckdblk.devusers < 0))
    {
//gettimeofday(&now,NULL);
//tm.tv_sec = now.tv_sec + 2;
//tm.tv_nsec = now.tv_usec * 1000;
        cckdblk.devwaiters++;
        wait_condition(&cckdblk.devcond, &cckdblk.devlock);
//timeout = timed_wait_condition(&cckdblk.devcond, &cckdblk.devlock, &tm);
//if (timeout) cckd_print_itrace();
        cckdblk.devwaiters--;
    }
    if (flag) cckdblk.devusers--;
    else cckdblk.devusers++;
    release_lock(&cckdblk.devlock);
}
void cckd_unlock_devchain()
{
    obtain_lock(&cckdblk.devlock);
    if (cckdblk.devusers < 0) cckdblk.devusers++;
    else cckdblk.devusers--;
    if (cckdblk.devusers == 0 && cckdblk.devwaiters)
        signal_condition(&cckdblk.devcond);
    release_lock(&cckdblk.devlock);
}

/*-------------------------------------------------------------------*/
/* Garbage Collection thread                                         */
/*-------------------------------------------------------------------*/
void cckd_gcol()
{
int             gcol;                   /* Identifier                */
int             rc;                     /* Return code               */
DEVBLK         *dev;                    /* -> device block           */
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
long long       size, fsiz;             /* File size, free size      */
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
    gcol = ++cckdblk.gcs;
    
    /* Return without messages if too many already started */
    if (gcol > cckdblk.gcmax)
    {
        --cckdblk.gcs;
        release_lock (&cckdblk.gclock);
        return;
    }

    if (!cckdblk.batch)
    logmsg (_("HHCCD003I Garbage collector thread started: tid="TIDPAT", pid=%d \n"),
              thread_id(), getpid());

    while (gcol <= cckdblk.gcmax)
    {
        cckd_lock_devchain(0);
        /* Perform collection on each device */
        for (dev = cckdblk.dev1st; dev; dev = cckd->devnext)
        {
            cckd = dev->cckd_ext;
            obtain_lock (&cckd->iolock);

            /* Bypass if merging or stopping */
            if (cckd->merging || cckd->stopping)
            {
                release_lock (&cckd->iolock);
                continue;
            }

            /* Bypass if not opened read-write */
            if (cckd->open[cckd->sfn] != CCKD_OPEN_RW)
            {
                release_lock (&cckd->iolock);
                continue;
            }

            /* Free newbuf if it hasn't been used */
            if (!cckd->ioactive && !cckd->bufused && cckd->newbuf)
            {
                free (cckd->newbuf);
                cckd->newbuf = NULL;
            }
            cckd->bufused = 0;

            /* If OPENED bit not on then flush if updated */
            if (!(cckd->cdevhdr[cckd->sfn].options & CCKD_OPENED))
            {
                if (cckd->updated) cckd_flush_cache (dev);
                release_lock (&cckd->iolock);
                continue;
            }

            /* Determine garbage state */
            size = (long long)cckd->cdevhdr[cckd->sfn].size;
            fsiz = (long long)cckd->cdevhdr[cckd->sfn].free_total;
            if      (fsiz >= (size = size/2)) gc = 0;
            else if (fsiz >= (size = size/2)) gc = 1;
            else if (fsiz >= (size = size/2)) gc = 2;
            else if (fsiz >= (size = size/2)) gc = 3;
            else gc = 4;

            /* Adjust the state based on the number of free spaces */
            if (cckd->cdevhdr[cckd->sfn].free_number >  800 && gc > 0) gc--;
            if (cckd->cdevhdr[cckd->sfn].free_number > 1800 && gc > 0) gc--;
            if (cckd->cdevhdr[cckd->sfn].free_number > 3000)           gc = 0;

            /* Set the size */
            if (cckdblk.gcparm > 0) size = gctab[gc] << cckdblk.gcparm;
            else if (cckdblk.gcparm < 0) size = gctab[gc] >> abs(cckdblk.gcparm);
            else size = gctab[gc];
            if (size > cckd->cdevhdr[cckd->sfn].used >> 10)
                size = cckd->cdevhdr[cckd->sfn].used >> 10;
            if (size < 64) size = 64;

            release_lock (&cckd->iolock);

            /* Call the garbage collector */
            rc = cckd_gc_percolate (dev, size);

            /* Schedule any updated tracks to be written */
            obtain_lock (&cckd->iolock);
            cckd_flush_cache (dev);
            while (cckd->wrpending)
            {
                cckd->gcwaiting = 1;
                wait_condition (&cckd->iocond, &cckd->iolock);
                cckd->gcwaiting = 0;
            }
            release_lock (&cckd->iolock);

            /* Sync the file */
            if (cckdblk.gcwait >= 5 || cckd->lastsync + 5 <= now.tv_sec)
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
        cckd_unlock_devchain();

        /* wait a bit */
        gettimeofday (&now, NULL);
        tm.tv_sec = now.tv_sec + cckdblk.gcwait;
        tm.tv_nsec = now.tv_usec * 1000;
        cckdtrc ("cckddasd: gcol wait %d seconds at %s",
                 cckdblk.gcwait, ctime (&now.tv_sec));
        timed_wait_condition (&cckdblk.gccond, &cckdblk.gclock, &tm);
    }

    if (!cckdblk.batch)
    logmsg (_("HHCCD013I Garbage collector thread stopping: tid="TIDPAT", pid=%d\n"),
            thread_id(), getpid());

    cckdblk.gcs--;
    if (!cckdblk.gcs) signal_condition (&cckdblk.termcond);
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
                logmsg ("%4.4X:",dev->devnum);
                logmsg (_("HHCCD180E gcperc lseek error file[%d] "
                          "offset 0x%llx: %s\n"),
                        sfx, (long long)bpos, strerror(errno));
                goto cckd_gc_perc_error;
            }
            rc = read (fd, &buf, blen);
            if (rc < 0)
            {
                logmsg ("%4.4X:",dev->devnum);
                logmsg (_("HHCCD181E gcperc read error file[%d] "
                          "offset 0x%llx (expected %d bytes)\n"
                           "         error: %s\n"),
                        sfx, (long long)bpos, blen, strerror(errno));
                goto cckd_gc_perc_error;
            }
            if (rc < (int)blen)
            {
                logmsg ("%4.4X:",dev->devnum);
                logmsg (_("HHCCD184E gcperc read too few bytes in file[%d] "
                          "offset 0x%llx: read %d, expected %d\n"),
                        sfx, (long long)bpos, rc, blen, strerror(errno));
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
                        logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD182E unknown space at offset 0x%llx\n"),
                                (long long)(bpos + b));
                        logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD183E %2.2x%2.2x%2.2x%2.2x%2.2x\n"),
                                buf[b+0], buf[b+1],buf[b+2], buf[b+3], buf[b+4]);
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
/* Find device by devnum                                             */
/*-------------------------------------------------------------------*/
DEVBLK *cckd_find_device_by_devnum (U16 devnum)
{
DEVBLK       *dev;
CCKDDASD_EXT *cckd;

    cckd_lock_devchain (0);
    for (dev = cckdblk.dev1st; dev; dev = cckd->devnext)
    {
        if (dev->devnum == devnum) break;
        cckd = dev->cckd_ext;
    }
    cckd_unlock_devchain ();
    return dev;
} /* end function cckd_find_device_by_devnum */

/*-------------------------------------------------------------------*/
/* Uncompress a track image                                          */
/*-------------------------------------------------------------------*/
BYTE *cckd_uncompress (DEVBLK *dev, BYTE *from, int len, int maxlen,
                       int trk)
{
CCKDDASD_EXT   *cckd;
BYTE           *to;                       /* Uncompressed buffer     */
int             newlen;                   /* Uncompressed length     */
BYTE            comp;                     /* Compression type        */
static char    *compress[] = {"none", "zlib", "bzip2"};

    cckd = dev->cckd_ext;

    cckdtrc ("cckddasd: uncompress comp %d len %d maxlen %d trk %d\n",
             from[0] & CCKD_COMPRESS_MASK, len, maxlen, trk);

    /* Extract compression type */
    comp = (from[0] & CCKD_COMPRESS_MASK);

    /* Get a buffer to uncompress into */
    if (comp != CCKD_COMPRESS_NONE && cckd->newbuf == NULL)
    {
        cckd->newbuf = malloc (maxlen);
        if (cckd->newbuf == NULL)
        {
            logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD190E uncompress %d malloc() error: %s\n"),
                   trk, strerror(errno));
            return NULL;
        }
    }

    /* Uncompress the track image */
    switch (comp) {

    case CCKD_COMPRESS_NONE:
        newlen = cckd_trklen (dev, from);
        to = from;
        break;
    case CCKD_COMPRESS_ZLIB:
        to = cckd->newbuf;
        newlen = cckd_uncompress_zlib (dev, to, from, len, maxlen);
        break;
    case CCKD_COMPRESS_BZIP2:
        to = cckd->newbuf;
        newlen = cckd_uncompress_bzip2 (dev, to, from, len, maxlen);
        break;
    default:
        newlen = -1;
        break;
    }

    /* Validate the uncompressed track image */
    newlen = cckd_validate (dev, to, trk, newlen);

    /* Return if successful */
    if (newlen > 0)
    {
        if (to != from)
        {
            cckd->newbuf = from;
            cckd->bufused = 1;
        }
        return to;
    }

    /* Get a buffer now if we haven't gotten one */
    if (cckd->newbuf == NULL)
    {
        cckd->newbuf = malloc (maxlen);
        if (cckd->newbuf == NULL)
        {
            logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD192E uncompress %d malloc() error: %s\n"),
                   trk, strerror(errno));
            return NULL;
        }
    }

    /* Try each uncompression routine in turn */

    /* uncompressed */
    newlen = cckd_trklen (dev, from);
    newlen = cckd_validate (dev, from, trk, newlen);
    if (newlen > 0)
        return from;

    /* zlib compression */
    to = cckd->newbuf;
    newlen = cckd_uncompress_zlib (dev, to, from, len, maxlen);
    newlen = cckd_validate (dev, to, trk, newlen);
    if (newlen > 0)
    {
        cckd->newbuf = from;
        cckd->bufused = 1;
        return to;
    }

    /* bzip2 compression */
    to = cckd->newbuf;
    newlen = cckd_uncompress_bzip2 (dev, to, from, len, maxlen);
    newlen = cckd_validate (dev, to, trk, newlen);
    if (newlen > 0)
    {
        cckd->newbuf = from;
        cckd->bufused = 1;
        return to;
    }

    /* Unable to uncompress */
    logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD193E uncompress error trk %d: %2.2x%2.2x%2.2x%2.2x%2.2x\n"),
            trk, from[0], from[1], from[2], from[3], from[4]);
    if (comp & ~cckdblk.comps)
        logmsg ("%4.4X:",dev->devnum); logmsg (_("HHCCD194E %s compression not supported\n"),
                compress[comp]);
    return NULL;
}

int cckd_uncompress_zlib (DEVBLK *dev, BYTE *to, BYTE *from, int len, int maxlen)
{
#if defined(HAVE_LIBZ)
unsigned long newlen;
int rc;

    UNREFERENCED(dev);
    memcpy (to, from, CKDDASD_TRKHDR_SIZE);
    newlen = maxlen - CKDDASD_TRKHDR_SIZE;
    rc = uncompress(&to[CKDDASD_TRKHDR_SIZE], &newlen,
                &from[CKDDASD_TRKHDR_SIZE], len - CKDDASD_TRKHDR_SIZE);
    if (rc == Z_OK)
    {
        newlen += CKDDASD_TRKHDR_SIZE;
        to[0] = 0;
    }
    else
        newlen = -1;

    cckdtrc ("cckddasd: uncompress zlib newlen %d rc %d\n",(int)newlen,rc);

    return (int)newlen;
#else
    UNREFERENCED(dev);
    UNREFERENCED(to);
    UNREFERENCED(from);
    UNREFERENCED(len);
    UNREFERENCED(maxlen);
    return -1;
#endif
}
int cckd_uncompress_bzip2 (DEVBLK *dev, BYTE *to, BYTE *from, int len, int maxlen)
{
#if defined(CCKD_BZIP2)
unsigned int newlen;
int rc;

    UNREFERENCED(dev);
    memcpy (to, from, CKDDASD_TRKHDR_SIZE);
    newlen = maxlen - CKDDASD_TRKHDR_SIZE;
    rc = BZ2_bzBuffToBuffDecompress (
                &to[CKDDASD_TRKHDR_SIZE], &newlen,
                &from[CKDDASD_TRKHDR_SIZE], len - CKDDASD_TRKHDR_SIZE,
                0, 0);
    if (rc == BZ_OK)
    {
        newlen += CKDDASD_TRKHDR_SIZE;
        to[0] = 0;
    }
    else
        newlen = -1;

    cckdtrc ("cckddasd: uncompress bz2 newlen %d rc %d\n",newlen,rc);

    return (int)newlen;
#else
    UNREFERENCED(dev);
    UNREFERENCED(to);
    UNREFERENCED(from);
    UNREFERENCED(len);
    UNREFERENCED(maxlen);
    return -1;
#endif
}

/*-------------------------------------------------------------------*/
/* Compress a track image                                            */
/*-------------------------------------------------------------------*/
int cckd_compress (DEVBLK *dev, BYTE **to, BYTE *from, int len,
                   int comp, int parm)
{
int newlen;

    switch (comp) {
    case CCKD_COMPRESS_NONE:
        newlen = cckd_compress_none (dev, to, from, len, parm);
        break;
    case CCKD_COMPRESS_ZLIB:
        newlen = cckd_compress_zlib (dev, to, from, len, parm);
        break;
    case CCKD_COMPRESS_BZIP2:
        newlen = cckd_compress_bzip2 (dev, to, from, len, parm);
        break;
    default:
        newlen = cckd_compress_bzip2 (dev, to, from, len, parm);
        break;
    }
    return newlen;
}
int cckd_compress_none (DEVBLK *dev, BYTE **to, BYTE *from, int len, int parm)
{
    UNREFERENCED(dev);
    UNREFERENCED(parm);
    *to = from;
    from[0] = CCKD_COMPRESS_NONE;
    return len;
}
int cckd_compress_zlib (DEVBLK *dev, BYTE **to, BYTE *from, int len, int parm)
{
#if defined(HAVE_LIBZ)
unsigned long newlen;
int rc;
BYTE *buf;

    UNREFERENCED(dev);
    buf = *to;
    from[0] = CCKD_COMPRESS_NONE;
    memcpy (buf, from, CKDDASD_TRKHDR_SIZE);
    buf[0] = CCKD_COMPRESS_ZLIB;
    newlen = 65535 - CKDDASD_TRKHDR_SIZE;
    rc = compress2 (&buf[CKDDASD_TRKHDR_SIZE], &newlen,
                    &from[CKDDASD_TRKHDR_SIZE], len - CKDDASD_TRKHDR_SIZE,
                    parm);
    newlen += CKDDASD_TRKHDR_SIZE;
    if (rc != Z_OK || (int)newlen >= len)
    {
        *to = from;
        newlen = len;
    }
    return (int)newlen;
#else

#if defined(CCKD_BZIP2)
    return cckd_compress_bzip2 (dev, to, from, len, parm);
#else
    return cckd_compress_none (dev, to, from, len, parm);
#endif

#endif
}
int cckd_compress_bzip2 (DEVBLK *dev, BYTE **to, BYTE *from, int len, int parm)
{
#if defined(CCKD_BZIP2)
unsigned int newlen;
int rc;
BYTE *buf;

    UNREFERENCED(dev);
    buf = *to;
    from[0] = CCKD_COMPRESS_NONE;
    memcpy (buf, from, CKDDASD_TRKHDR_SIZE);
    buf[0] = CCKD_COMPRESS_BZIP2;
    newlen = 65535 - CKDDASD_TRKHDR_SIZE;
    rc = BZ2_bzBuffToBuffCompress (
                    &buf[CKDDASD_TRKHDR_SIZE], &newlen,
                    &from[CKDDASD_TRKHDR_SIZE], len - CKDDASD_TRKHDR_SIZE,
                    parm >= 1 && parm <= 9 ? parm : 5, 0, 0);
    newlen += CKDDASD_TRKHDR_SIZE;
    if (rc != BZ_OK || newlen >= (unsigned int)len)
    {
        *to = from;
        newlen = len;
    }
    return newlen;
#else
    return cckd_compress_zlib (dev, to, from, len, parm);
#endif
}

/*-------------------------------------------------------------------*/
/* cckd command help                                                 */
/*-------------------------------------------------------------------*/
void cckd_command_help()
{
    logmsg ("cckd command parameters:\n"
             "help\t\tDisplay help message\n"
             "stats\t\tDisplay cckd statistics\n"
             "opts\t\tDisplay cckd options\n"
             "comp=<n>\t\tOverride compression\t\t(-1 .. 2)\n"
             "compparm=<n>\tOverride compression parm\t\t(-1 .. 9)\n"
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
    logmsg ("comp=%d,compparm=%d,ra=%d,raq=%d,rat=%d,"
             "wr=%d,gcint=%d,gcparm=%d,nostress=%d,\n"
             "\tfreepend=%d,fsync=%d,ftruncwa=%d,trace=%d\n",
             cckdblk.comp == 0xff ? -1 : cckdblk.comp,
             cckdblk.compparm, cckdblk.ramax,
             cckdblk.ranbr, cckdblk.readaheads,
             cckdblk.wrmax, cckdblk.gcwait,
             cckdblk.gcparm, cckdblk.nostress, cckdblk.freepend,
             cckdblk.fsync, cckdblk.ftruncwa,cckdblk.itracen);
} /* end function cckd_command_opts */

/*-------------------------------------------------------------------*/
/* cckd command stats                                                */
/*-------------------------------------------------------------------*/
void cckd_command_stats()
{
    logmsg("reads....%10lld Kbytes...%10lld writes...%10lld Kbytes...%10lld\n"
            "readaheads%9lld misses...%10lld syncios..%10lld misses...%10lld\n"
            "switches.%10lld l2 reads.%10lld              stress writes...%10lld\n"
            "cachehits%10lld misses...%10lld l2 hits..%10lld misses...%10lld\n"
            "waits                                   i/o......%10lld cache....%10lld\n"
            "garbage collector   moves....%10lld Kbytes...%10lld\n",
            cckdblk.stats_reads, cckdblk.stats_readbytes >> 10,
            cckdblk.stats_writes, cckdblk.stats_writebytes >> 10,
            cckdblk.stats_readaheads, cckdblk.stats_readaheadmisses,
            cckdblk.stats_syncios, cckdblk.stats_synciomisses,
            cckdblk.stats_switches, cckdblk.stats_l2reads,
            cckdblk.stats_stresswrites,
            cckdblk.stats_cachehits, cckdblk.stats_cachemisses,
            cckdblk.stats_l2cachehits, cckdblk.stats_l2cachemisses,
            cckdblk.stats_iowaits, cckdblk.stats_cachewaits,
            cckdblk.stats_gcolmoves, cckdblk.stats_gcolbytes >> 10);
} /* end function cckd_command_stats */

/*-------------------------------------------------------------------*/
/* cckd command debug                                                */
/*-------------------------------------------------------------------*/
void cckd_command_debug()
{
}

/*-------------------------------------------------------------------*/
/* cckd command processor                                            */
/*-------------------------------------------------------------------*/
int cckd_command(BYTE *op, int cmd)
{
BYTE *kw, *p, c = '\0', buf[256];
int   val, opts = 0;

    /* Display help for null operand */
    if (op == NULL)
    {
        if (memcmp (&cckdblk.id, "CCKDBLK ", sizeof(cckdblk.id)) == 0 && cmd)
            cckd_command_help();
        return 0;
    }

    strcpy(buf, op);
    op = buf;

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
        else if (strcasecmp (kw, "comp") == 0)
        {
            if (val < -1 || (val & ~cckdblk.comps) || c != '\0')
            {
                logmsg ("Invalid value for comp=\n");
                return -1;
            }
            else
            {
                cckdblk.comp = val < 0 ? 0xff : val;
                opts = 1;
            }
        }
        else if (strcasecmp (kw, "compparm") == 0)
        {
            if (val < -1 || val > 9 || c != '\0')
            {
                logmsg ("Invalid value for compparm=\n");
                return -1;
            }
            else
            {
                cckdblk.compparm = val;
                opts = 1;
            }
        }
        else if (strcasecmp (kw, "ra") == 0)
        {
            if (val < CCKD_MIN_RA || val > CCKD_MAX_RA || c != '\0')
            {
                logmsg ("Invalid value for ra=\n");
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
                logmsg ("Invalid value for raq=\n");
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
                logmsg ("Invalid value for rat=\n");
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
                logmsg ("Invalid value for wr=\n");
                return -1;
            }
            else
            {
                cckdblk.wrmax = val;
                opts = 1;
            }
        }
        else if (strcasecmp (kw, "gcint") == 0)
        {
            if (val < 1 || val > 60 || c != '\0')
            {
                logmsg ("Invalid value for gcint=\n");
                return -1;
            }
            else
            {
                cckdblk.gcwait = val;
                opts = 1;
            }
        }
        else if (strcasecmp (kw, "gcparm") == 0)
        {
            if (val < -8 || val > 8 || c != '\0')
            {
                logmsg ("Invalid value for gcparm=\n");
                return -1;
            }
            else
            {
                cckdblk.gcparm = val;
                opts = 1;
            }
        }
        else if (strcasecmp (kw, "nostress") == 0)
        {
            if (val < 0 || val > 1 || c != '\0')
            {
                logmsg ("Invalid value for nostress=\n");
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
                logmsg ("Invalid value for freepend=\n");
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
                logmsg ("Invalid value for fsync=\n");
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
                logmsg ("Invalid value for ftruncwa=\n");
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
                logmsg ("Invalid value for trace=\n");
                return -1;
            }
            else
            {
                /* Disable tracing in case it's already active */
                CCKD_TRACE *p = cckdblk.itrace;
                cckdblk.itrace = NULL;
                if (p)
                {
                    sleep (1);
                    cckdblk.itrace = cckdblk.itracep = cckdblk.itracex = NULL;
                    cckdblk.itracen = 0;
                    free (p);
                }

                /* Get a new trace table */
                if (val > 0)
                {
                    p = calloc ( val, sizeof(CCKD_TRACE));
                    if (p)
                    {
                        cckdblk.itracen = val;
                        cckdblk.itracex = p + val;
                        cckdblk.itracep = p;
                        cckdblk.itrace  = p;
                    }
                    else
                        logmsg ("calloc() failed for trace table: %s\n",
                                 strerror(errno));
                }
                opts = 1;
            }
        }
        else
        {
            logmsg ("cckd invalid keyword: %s\n",kw);
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
CCKD_TRACE     *i, *p;                  /* Trace table pointers      */

    if (!cckdblk.itrace) return;
    logmsg (_("HHCCD900I print_itrace\n"));
    i = cckdblk.itrace;
    cckdblk.itrace = NULL;
    sleep (1);
    p = cckdblk.itracep;
    if (p >= cckdblk.itracex) p = i;
    do
    {
        if (p[0] != '\0')
            logmsg ("%s", (char *)p);
        if (++p >= cckdblk.itracex) p = i;
    } while (p != cckdblk.itracep);
    memset (i, 0, cckdblk.itracen * sizeof(CCKD_TRACE));
    cckdblk.itracep = i;
    cckdblk.itrace  = i;
}

DEVHND cckddasd_device_hndinfo = {
        &ckddasd_init_handler,
        &ckddasd_execute_ccw,
        &cckddasd_close_device,
        &ckddasd_query_device,
        &cckddasd_start,
        &cckddasd_end,
        &cckddasd_start,
        &cckddasd_end,
        &cckd_read_track,
        &cckd_update_track,
        &cckd_used,
        NULL,
        NULL,
        NULL
};

DEVHND cfbadasd_device_hndinfo = {
        &fbadasd_init_handler,
        &fbadasd_execute_ccw,
        &cckddasd_close_device,
        &fbadasd_query_device,
        &cckddasd_start,
        &cckddasd_end,
        &cckddasd_start,
        &cckddasd_end,
        &cfba_read_block,
        &cfba_write_block,
        &cfba_used,
        NULL,
        NULL,
        NULL
};

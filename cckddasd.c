/* CCKDDASD.C   (c) Copyright Roger Bowler, 1999-2001                */
/*       ESA/390 Compressed CKD Direct Access Storage Device Handler */

/*-------------------------------------------------------------------*/
/* This module contains device functions for compressed emulated     */
/* count-key-data direct access storage devices.                     */
/*-------------------------------------------------------------------*/

//#define CCKD_ITRACEMAX 100000

#ifdef NOTHREAD
#ifndef CCKD_NOTHREAD
#define CCKD_NOTHREAD
#endif
#endif

#include "hercules.h"

#ifndef NO_CCKD

#include "zlib.h"
#ifdef CCKD_BZIP2
#include "bzlib.h"
#endif

#ifdef CCKD_ITRACEMAX
#undef DEVTRACE
#define DEVTRACE(format, a...) \
  {int n; \
   if (cckd->itracex >= 128 * CCKD_ITRACEMAX) cckd->itracex = 0; \
   n = cckd->itracex; cckd->itracex += 128; \
   sprintf(&cckd->itrace[n], "%4.4X:" format, dev->devnum, a); }
#endif

/* external functions */
int     cckd_chkdsk(int, FILE *, int);

/* internal functions */
int     cckddasd_init_handler (DEVBLK *, int, BYTE **);
int     cckddasd_close_device (DEVBLK *);
off_t   cckd_lseek (DEVBLK *, int, off_t, int);
ssize_t cckd_read (DEVBLK *, int, char *, size_t);
ssize_t cckd_write (DEVBLK *, int, const void *, size_t);
CCKD_CACHE *cckd_read_trk (DEVBLK *, int, int);
void    cckd_readahead (DEVBLK *, int);
void    cckd_ra (DEVBLK *);
void    cckd_write_trk (DEVBLK *, BYTE *);
CCKD_DFWQE *cckd_scan_dfwq (DEVBLK *, int);
void    cckd_dfw (DEVBLK *);
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
void    cckd_write_l2 (DEVBLK *);
int     cckd_read_l2ent (DEVBLK *, CCKD_L2ENT *, int);
void    cckd_write_l2ent (DEVBLK *, CCKD_L2ENT *, int);
int     cckd_read_trkimg (DEVBLK *, BYTE *, int);
int     cckd_write_trkimg (DEVBLK *, BYTE *, int, int);
void    cckd_harden (DEVBLK *);
int     cckd_trklen (DEVBLK *, BYTE *);
int     cckd_null_trk (DEVBLK *, BYTE *, int);
int     cckd_cchh (DEVBLK *, BYTE *, int);
int     cckd_sf_name (DEVBLK *, int, char *);
int     cckd_sf_init (DEVBLK *);
int     cckd_sf_new (DEVBLK *);
void    cckd_sf_add (DEVBLK *);
void    cckd_sf_remove (DEVBLK *, int);
void    cckd_sf_newname (DEVBLK *, BYTE *);
void    cckd_sf_stats (DEVBLK *);
void    cckd_gcol (DEVBLK *);
void    cckd_gc_combine (DEVBLK *, int, int, int);
int     cckd_gc_len (DEVBLK *, BYTE *, off_t, int, int);
void    cckd_swapend (DEVBLK *);
void    cckd_swapend_chdr (CCKDDASD_DEVHDR *);
void    cckd_swapend_l1 (CCKD_L1ENT *, int);
void    cckd_swapend_l2 (CCKD_L2ENT *);
void    cckd_swapend_free (CCKD_FREEBLK *);
void    cckd_swapend4 (char *);
void    cckd_swapend2 (char *);
int     cckd_endian ();
void    cckd_print_itrace(DEVBLK *);

extern  char eighthexFF[];
BYTE    cckd_empty_l2tab[CCKD_L2TAB_SIZE];  /* Empty Level 2 table   */

/*-------------------------------------------------------------------*/
/* Initialize the compressed device handler                          */
/*-------------------------------------------------------------------*/
int cckddasd_init_handler ( DEVBLK *dev, int argc, BYTE *argv[] )
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             rc;                     /* Return code               */
int             fdflags;                /* File flags                */
int             i;                      /* Loop index                */
char           *kw, *op;                /* Argument keyword/option   */

    /* Obtain area for cckd extension */
    dev->cckd_ext = cckd = malloc(sizeof(CCKDDASD_EXT));
    memset(cckd, 0, sizeof(CCKDDASD_EXT));
    memset(&cckd_empty_l2tab, 0, CCKD_L2TAB_SIZE);

#ifndef CCKD_NOTHREAD
    /* Set threading indicator */
    cckd->threading = 1;
#endif

#ifdef CCKD_ITRACEMAX
    /* get internal trace table */
    cckd->itrace = calloc (CCKD_ITRACEMAX, 128);
#endif

    /* process arguments starting with arg 1 */
    cckd->l2cachenbr = cckd->max_dfwq = cckd->max_ra =
                       cckd->max_dfw  = cckd->max_wt = -1;
    for (i = 1; i < argc; i++)
    {
        /* `l2cache=' specifies size of the level 2 cache */
        if (strlen (argv[i]) > 8 &&
            memcmp ("l2cache=", argv[i], 8) == 0)
        {
            kw = strtok (argv[i], "=");
            op = strtok (NULL, " \t");
            if (op) cckd->l2cachenbr = atoi (op);
            continue;
        }

        /* `dfwq=' specifies max size of the deferred write queue
           before throttling occurs */
        if (strlen (argv[i]) > 5 &&
            memcmp ("dfwq=", argv[i], 5) == 0)
        {
            kw = strtok (argv[i], "=");
            op = strtok (NULL, " \t");
            if (op) cckd->max_dfwq = atoi (op);
            continue;
        }

        /* `wt=' specifies idle time for an updated thread before the
           garbage collector will write it */
        if (strlen (argv[i]) > 3 &&
            memcmp ("wt=", argv[i], 3) == 0)
        {
            kw = strtok (argv[i], "=");
            op = strtok (NULL, " \t");
            if (op) cckd->max_wt = atoi (op);
            continue;
        }

        /* `ra=' specifies number of readahead threads */
        if (strlen (argv[i]) == 4
         && memcmp ("ra=", argv[i], 3) == 0
         && argv[i][3] >= '0'
         && argv[i][3] <= '0' + CCKD_MAX_RA)
        {
            cckd->max_ra = argv[i][3] - '0';
            continue;
        }

        /* `dfw=' specifies number of deferred write threads */
        if (strlen (argv[i]) == 5
         && memcmp ("dfw=", argv[i], 4) == 0
         && argv[i][4] >= '0'
         && argv[i][4] <= '0' + CCKD_MAX_DFW)
        {
            cckd->max_dfw = argv[i][4] - '0';
            continue;
        }
    }
    if (cckd->l2cachenbr < 1) cckd->l2cachenbr = CCKD_L2CACHE_NBR;
    if (cckd->max_dfwq < 1) cckd->max_dfwq = CCKD_MAX_DFWQ_DEPTH;
#ifndef WIN32
    if (cckd->max_ra < 0) cckd->max_ra = 2;
#else
    if (cckd->max_ra < 0) cckd->max_ra = 0;
#endif
    if (cckd->max_dfw < 1) cckd->max_dfw = 1;
    if (cckd->max_wt < 1) cckd->max_wt = CCKD_MAX_WRITE_TIME;

    cckd->l1x = cckd->sfx = -1;

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
    if ((cckd->cdevhdr[0].options & CCKD_OPENED) == 0)
        rc = cckd_chkdsk (cckd->fd[0], sysblk.msgpipew, 0);
    else
    {
        logmsg("cckddasd: forcing chkdsk -1, %s not closed\n",
               dev->filename);
        rc = cckd_chkdsk (cckd->fd[0], sysblk.msgpipew, 1);
    }
    if (rc < 0) return -1;

    /* re-read the compressed device header if file was repaired */
    if (rc > 0)
    {
        rc = cckd_read_chdr (dev);
        if (rc < 0) return -1;
    }

    /* open the shadow files */
    rc = cckd_sf_init (dev);
    if (rc < 0)
    {   logmsg ("cckddasd: error initializing shadow files: %s\n",
                strerror (errno));
        return -1;
    }

    /* display statistics */
    cckd_sf_stats (dev);

    /* Set current ckddasd position */
    cckd->curpos = 512;

    /* Initialize locks, conditions and attributes */
    initialize_lock (&cckd->filelock);
    initialize_lock (&cckd->dfwlock);
    initialize_lock (&cckd->gclock);
    initialize_lock (&cckd->termlock);
    initialize_condition (&cckd->dfwcond);
    initialize_condition (&cckd->gccond);
    initialize_condition (&cckd->rtcond);
    initialize_condition (&cckd->termcond);
    initialize_detach_attr (&cckd->gcattr);
    initialize_detach_attr (&cckd->dfwattr);
    for (i = 0; i < cckd->max_ra; i++)
    {   /* read-ahead locks, conditions and attributes */
        initialize_lock (&cckd->ralock[i]);
        initialize_condition (&cckd->racond[i]);
        initialize_detach_attr (&cckd->raattr[i]);
    }

    /* set default cache limit for cckd_read_trk */
    if (dev->ckdcachenbr < 1)
        dev->ckdcachenbr = dev->ckdheads + cckd->max_ra;

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

    /* write any updated track images */
    obtain_lock (&cckd->cachelock);
    obtain_lock (&cckd->dfwlock);
    for (i = 0; i < dev->ckdcachenbr && cckd->cache; i++)
        if (cckd->cache[i].updated)
        {
            cckd->cache[i].updated = 0;
            cckd_write_trk (dev, cckd->cache[i].buf);
        }
    if (cckd->dfwq && cckd->dfwaiting)
        signal_condition (&cckd->dfwcond);
    release_lock (&cckd->dfwlock);
    release_lock (&cckd->cachelock);
    if (cckd->dfwq) sleep (1); /* not sure why this is necessary */

    /* terminate threads */
    if (cckd->threading)
    {
        cckd->threading = 0;

        /* I'm using a termination lock and condition because for
           some reason I can't get pthread_join to work correctly */
        obtain_lock (&cckd->termlock);

        /* Terminate the readahead threads */
        for (i = 0; i < cckd->max_ra; i++)
            if (cckd->rainit[i])
            {
                obtain_lock (&cckd->ralock[i]);
                signal_condition (&cckd->racond[i]);
                release_lock (&cckd->ralock[i]);
                wait_condition (&cckd->termcond, &cckd->termlock);
            }

        /* Terminate the deferred-write threads */
        if (cckd->writeinit)
            for (i = 0; i < cckd->max_dfw; i++)
            {
                obtain_lock (&cckd->dfwlock);
                signal_condition (&cckd->dfwcond);
                release_lock (&cckd->dfwlock);
                wait_condition (&cckd->termcond, &cckd->termlock);
            } while (cckd->dfwaiting);

        /* Terminate the garbage-collection thread */
        if (cckd->gcinit)
        {
            obtain_lock (&cckd->gclock);
            signal_condition (&cckd->gccond);
            release_lock (&cckd->gclock);
            wait_condition (&cckd->termcond, &cckd->termlock);
        }

        release_lock (&cckd->termlock);
        }

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
        for (i = 0; i <= cckd->max_ra; i++)
            free (cckd->cachebuf[i]);
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
    cckd_sf_stats (dev);

    /* free the cckd extension */
    dev->cckd_ext= NULL;
    memset (&dev->ckdsfn, 0, sizeof(dev->ckdsfn));
    free (cckd);

    return 0;
}

/*-------------------------------------------------------------------*/
/* Compressed ckddasd lseek                                          */
/*-------------------------------------------------------------------*/
off_t cckd_lseek(DEVBLK *dev, int fd, off_t offset, int pos)
{
unsigned int    newpos=0;               /* New position              */
off_t           oldpos;                 /* Old position              */
int             newtrk;                 /* New track                 */
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */

    cckd = dev->cckd_ext;

    /* calculate new file position */
    switch (pos) {

        case SEEK_SET:
            newpos = 0;
            break;

        case SEEK_CUR:
            newpos = cckd->curpos;
            break;

        case SEEK_END:
            newpos = dev->ckdtrks*dev->ckdtrksz + CKDDASD_DEVHDR_SIZE;
            break;
    }
    newpos = newpos + offset;

    /* calculate new track number from the new file position */
    newtrk = (newpos - CKDDASD_DEVHDR_SIZE) / dev->ckdtrksz;

    /* check for new track */
    if (newtrk != cckd->curtrk || cckd->active == NULL)
    {
        /* read the new track */
        DEVTRACE ("cckddasd: lseek trk   %d\n", newtrk);
        cckd->switches++;
        cckd->active = cckd_read_trk (dev, newtrk, 0);
        cckd->curtrk = newtrk;
        cckd->trkpos = newtrk * dev->ckdtrksz + CKDDASD_DEVHDR_SIZE;
    }

    /* set new file position */
    oldpos = cckd->curpos;
    cckd->curpos = newpos;
    return oldpos;

} /* end function cckd_lseek */


/*-------------------------------------------------------------------*/
/* Compressed ckddasd read                                           */
/*-------------------------------------------------------------------*/
ssize_t cckd_read(DEVBLK *dev, int fd, char *buf, size_t N)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */

    cckd = dev->cckd_ext;
    cckd->active->used = 1;
    memcpy (buf, (char *)&cckd->active->buf[cckd->curpos - cckd->trkpos], N);
    cckd->curpos += N;
    return N;

} /* end function cckd_read */


/*-------------------------------------------------------------------*/
/* Compressed ckddasd write                                          */
/*-------------------------------------------------------------------*/
ssize_t cckd_write(DEVBLK *dev, int fd, const void *buf, size_t N)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */

    cckd = dev->cckd_ext;
    cckd->active->updated = cckd->active->used = 1;
    memcpy ((char *)&cckd->active->buf[cckd->curpos - cckd->trkpos], buf, N);
    cckd->curpos += N;
    return N;

} /* end function cckd_write */


/*-------------------------------------------------------------------*/
/* Read a track image                                                */
/*                                                                   */
/* This routine can be called by the i/o thread (`ra' == 0) or       */
/* by readahead threads (0 < `ra' <= CCKD_MAX_RA).                   */
/*                                                                   */
/*-------------------------------------------------------------------*/
CCKD_CACHE *cckd_read_trk(DEVBLK *dev, int trk, int ra)
{
int             rc;                     /* Return code               */
int             i;                      /* Index variable            */
int             fnd;                    /* Cache index for hit       */
int             lru;                    /* Least-Recently-Used cache
                                            index                    */
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
CCKD_DFWQE     *dfw;                    /* -> deferred write q elem  */
unsigned long   len,len2;               /* Lengths                   */
BYTE           *buf,*buf2;              /* Buffers                   */

    cckd = dev->cckd_ext;

    DEVTRACE("cckddasd: %d rdtrk     %d\n", ra, trk);

    obtain_lock (&cckd->cachelock);

    /* get the cache array if it doesn't exist yet */
    if (cckd->cache == NULL)
    {
        cckd->cache = calloc (dev->ckdcachenbr, CCKD_CACHE_SIZE);
        for (i = 0; i <= cckd->max_ra; i++)
            cckd->cachebuf[i] = malloc (dev->ckdtrksz);
    }

    /* scan the cache array for the track */
    for (fnd = lru = -1, i = 0; i < dev->ckdcachenbr; i++)
    {
        if (trk == cckd->cache[i].trk && cckd->cache[i].buf)
        {   fnd = i;
            break;
        }
        /* find the oldest entry that doesn't have an active read */
        if (!cckd->cache[i].reading &&
            (lru == - 1 ||
            (cckd->cache[i].tv.tv_sec < cckd->cache[lru].tv.tv_sec ||
            (cckd->cache[i].tv.tv_sec == cckd->cache[lru].tv.tv_sec &&
             cckd->cache[i].tv.tv_usec < cckd->cache[lru].tv.tv_usec))))
                lru = i;
    }

    /* check for cache hit */
    if (fnd >= 0)
    {
        if (ra) /* readahead doesn't care about a cache hit */
        {   release_lock (&cckd->cachelock);
             return NULL;
        }

        /* if read is in progress then wait for it to finish */
        if (cckd->cache[fnd].reading)
        {   DEVTRACE ("cckddasd: %d rdtrk[%2.2d] %d waiting for read\n",
                  ra, fnd, trk);
            cckd->cache[fnd].waiting = 1;
            wait_condition (&cckd->rtcond, &cckd->cachelock);
            cckd->cache[fnd].waiting = 0;
            DEVTRACE ("cckddasd: %d rdtrk[%2.2d] %d wait complete buf %p\n",
                      ra, fnd, trk, cckd->cache[fnd].buf);
        }
        else gettimeofday (&cckd->cache[fnd].tv, NULL);
        DEVTRACE ("cckddasd: %d rdtrk[%2.2d] %d cache hit buf %p\n",
                  ra, fnd, trk, cckd->cache[fnd].buf);
        cckd->cachehits++;
        if (trk == cckd->curtrk + 1) cckd_readahead (dev, trk);
        release_lock (&cckd->cachelock);
        return &cckd->cache[fnd];
    }

    /* if readahead, return if the lru entry active */
    if (ra && &cckd->cache[lru] == cckd->active)
    {   release_lock (&cckd->cachelock);
        return NULL;
    }

    if (cckd->writeinit || cckd->cache[lru].updated)
    {
        /* note the nested locking */
        obtain_lock (&cckd->dfwlock);

        /* queue the the old track for write if it has been updated */
        if (cckd->cache[lru].updated)
        {
            DEVTRACE ("cckddasd: %d rdtrk[%2.2d] %d upd old trk %d buf %p\n",
                      ra, lru, trk, cckd->cache[lru].trk, cckd->cache[lru].buf);
            cckd_write_trk (dev, cckd->cache[lru].buf);
            cckd->cache[lru].buf = NULL;
            cckd->cache[lru].updated = 0;
        }

        /* see if the track is in the deferred write queue */
        dfw = cckd_scan_dfwq (dev, trk);
        if (dfw != NULL)
        {
            buf = dfw->buf;

            /* wakeup dfw if old track was updated */
            if (cckd->dfwaiting)
                signal_condition (&cckd->dfwcond);

            release_lock (&cckd->dfwlock);

            /* check for readahead miss */
            if (cckd->cache[lru].buf && !cckd->cache[lru].used)
                cckd->misses++;

            /* free old cache buffer if it isn't writing */
            if (cckd->cache[lru].buf && !cckd->cache[lru].writing)
            {   free (cckd->cache[lru].buf);
                DEVTRACE ("cckddasd: %d rdtrk[%2.2d] %d free buf %p\n",
                          ra, lru, trk, cckd->cache[lru].buf);
            }

            /* update the cache entry */
            cckd->cache[lru].trk = trk;
            cckd->cache[lru].buf = buf;
            cckd->cache[lru].used = 0;
            cckd->cache[lru].writing = 1;
            gettimeofday (&cckd->cache[lru].tv, NULL);

            if (!ra)
            {   cckd->cachehits++;
                if (trk == cckd->curtrk + 1) cckd_readahead (dev, trk);
            }
            release_lock (&cckd->cachelock);

            DEVTRACE ("cckddasd: %d rdtrk[%2.2d] %d in dfwq %p buf %p\n",
                      ra, lru, trk, dfw, cckd->cache[lru].buf);

            return &cckd->cache[lru];
        }
        release_lock (&cckd->dfwlock);
    }

    /* if the current cache buf is writing, then NULLify the buf */
    if (cckd->cache[lru].writing)
    {
        DEVTRACE ("cckddasd: %d rdtrk[%2.2d] %d old trk %d writing"
                  " buf %p released\n", ra, lru, trk,
                  cckd->cache[lru].trk, cckd->cache[lru].buf);
        cckd->cache[lru].buf = NULL;
        cckd->cache[lru].writing = 0;
    }

    /* get buffer if there isn't one */
    if (cckd->cache[lru].buf == NULL)
    {
        cckd->cache[lru].buf = malloc (dev->ckdtrksz);
        DEVTRACE ("cckddasd: %d rdtrk[%2.2d] %d get buf %p\n",
                  ra, lru, trk, cckd->cache[lru].buf);
    }
    else
    {
        DEVTRACE ("cckddasd: %d rdtrk[%2.2d] %d dropping %d from cache buf %p\n",
                  ra, lru, trk, cckd->cache[lru].trk, cckd->cache[lru].buf);
        if (!cckd->cache[lru].used) cckd->misses++;
    }

    buf = cckd->cache[lru].buf;
    buf2 = cckd->cachebuf[ra];
    cckd->cache[lru].trk = trk;
    if (ra) cckd->readaheads++;
    cckd->cache[lru].reading = 1;
    cckd->cache[lru].used = 0;
    gettimeofday (&cckd->cache[lru].tv, NULL);

    /* asynchrously schedule readaheads */
    if (!ra && trk == cckd->curtrk + 1)
        cckd_readahead (dev, trk);

    release_lock (&cckd->cachelock);

    /* read the track image */
    obtain_lock (&cckd->filelock);
    len2 = cckd_read_trkimg (dev, buf2, trk);
    release_lock (&cckd->filelock);
    DEVTRACE ("cckddasd: %d rdtrk[%2.2d] %d read buf %p len %ld\n",
              ra, lru, trk, buf2, len2);

    /* uncompress the track image */
    switch (buf2[0]) {

    case CCKD_COMPRESS_NONE:
        DEVTRACE("cckddasd: %d rdtrk[%2.2d] %d not compressed\n",
                 ra, lru, trk);
        break;

    case CCKD_COMPRESS_ZLIB:
        /* Uncompress the track image using zlib */
        memcpy (buf, buf2, CKDDASD_TRKHDR_SIZE);
        len = dev->ckdtrksz - CKDDASD_TRKHDR_SIZE;
        rc = uncompress(&buf[CKDDASD_TRKHDR_SIZE],
                        &len, &buf2[CKDDASD_TRKHDR_SIZE],
                        len2 - CKDDASD_TRKHDR_SIZE);
        len += CKDDASD_TRKHDR_SIZE;
        DEVTRACE("cckddasd: %d rdtrk[%2.2d] %d uncompressed len %ld code %d\n",
                 ra, lru, trk, len, rc);

        if (rc != Z_OK)
        {   logmsg ("%4.4X cckddasd: rdtrk %d uncompress error: %d\n",
                        dev->devnum, trk, rc);
            cckd_null_trk (dev, buf, trk);
        }
        break;

#ifdef CCKD_BZIP2
    case CCKD_COMPRESS_BZIP2:
        /* Decompress the track image using bzip2 */
        memcpy (buf, buf2, CKDDASD_TRKHDR_SIZE);
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
        {   logmsg ("cckddasd: decompress error for trk %d: %d\n",
                    trk, rc);
            cckd_null_trk (dev, buf, trk);
        }
        break;
#endif

    default:
        logmsg ("cckddasd: %4.4x unknown compression for trk %d: %d\n",
                dev->devnum, trk, buf2[0]);
        cckd_null_trk (dev, buf2, trk);
        break;
    }

    obtain_lock (&cckd->cachelock);

    cckd->cache[lru].reading = cckd->cache[lru].updated = 0;

    if (buf2[0] == CCKD_COMPRESS_NONE)
    {   /* need to flip/flop buffers */
        cckd->cache[lru].buf = buf2;
        cckd->cachebuf[ra] = buf;
    }
    else buf[0] = CCKD_COMPRESS_NONE;


    /* wakeup other thread waiting for this read */
    if (cckd->cache[lru].waiting)
    {   DEVTRACE("cckddasd: %d rdtrk[%2.2d] %d signalling read complete buf %p\n",
                 ra, lru, trk, cckd->cache[lru].buf);
        signal_condition (&cckd->rtcond);
    }

    DEVTRACE("cckddasd: %d rdtrk[%2.2d] %d complete buf %p\n",
              ra, lru, trk, buf);

    release_lock (&cckd->cachelock);

    /* schedule any outstanding writes */
    if (cckd->dfwq)
    {   obtain_lock (&cckd->dfwlock);
        if (cckd->dfwq && cckd->dfwaiting)
            signal_condition (&cckd->dfwcond);
        release_lock (&cckd->dfwlock);
    }

    return &cckd->cache[lru];

} /* end function cckd_read_trk */


/*-------------------------------------------------------------------*/
/* Schedule asynchronous readaheads                                  */
/*-------------------------------------------------------------------*/
void cckd_readahead (DEVBLK *dev, int trk)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             ra[CCKD_MAX_RA];        /* Thread create switches    */
int             i;                      /* Loop index                */

//  return; /*DEBUG*/
    cckd = dev->cckd_ext;

    if (!cckd->threading || dev->ckdcachenbr < cckd->max_ra + 2 ||
        cckd->max_ra < 1) return;

    /* initialize */
    memset ( ra, 0, sizeof (ra));

    /* make sure readahead tracks aren't already cached */
    for (i = 0; i < dev->ckdcachenbr; i++)
        if (cckd->cache[i].trk > trk
         && cckd->cache[i].trk <= trk + cckd->max_ra)
            ra[cckd->cache[i].trk - (trk+1)] = 1;

    /* wakeup readahead threads for uncached tracks */
    for (i = 0; i < cckd->max_ra; i++)
    {
        if (ra[i] || trk + 1 +i >= dev->ckdtrks) continue;
        cckd->ratrk[i] = trk + 1 + i;
        if (cckd->rainit[i])
        {
            obtain_lock (&cckd->ralock[i]);
            signal_condition (&cckd->racond[i]);
            release_lock (&cckd->ralock[i]);
        }
        else
           create_thread (&cckd->ratid[i], &cckd->raattr[i],
                          cckd_ra, dev);
    }

} /* end function cckd_readahead */


/*-------------------------------------------------------------------*/
/* Asynchronous readahead thread                                     */
/*-------------------------------------------------------------------*/
void cckd_ra (DEVBLK *dev)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             trk;                    /* Readahead track           */
int             ra;                     /* Readahead index           */

    cckd = dev->cckd_ext;

    obtain_lock (&cckd->cachelock);
    ra = cckd->ra++;
    release_lock (&cckd->cachelock);

    cckd->rainit[ra] = 1;

    do /* while (cckd->threading) */
    {
        /* call readahead while `trk' isn't zero */
        while ((trk = cckd->ratrk[ra]) > 0)
        {   cckd->ratrk[ra] = 0;
            cckd_read_trk (dev, trk, ra + 1);
        }

        /* wait for the next track */
        obtain_lock (&cckd->ralock[ra]);
        wait_condition (&cckd->racond[ra], &cckd->ralock[ra]);
        release_lock (&cckd->ralock[ra]);

    } while (cckd->threading);

    cckd->rainit[ra] = 0;
    obtain_lock (&cckd->termlock);
    signal_condition (&cckd->termcond);
    release_lock (&cckd->termlock);

} /* end thread cckd_ra_thread */


/*-------------------------------------------------------------------*/
/* Schedule a track image to be written                              */
/*-------------------------------------------------------------------*/
void cckd_write_trk(DEVBLK *dev, BYTE *buf)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             trk;                    /* Track number              */
CCKD_DFWQE     *dfw;                    /* -> deferred write q elem  */
int             i;                      /* Loop index                */

    cckd = dev->cckd_ext;

    trk = cckd_cchh (dev, buf, -1);
    if (trk < 0)
    {
        free (buf);
        return;
    }

    dfw = cckd_scan_dfwq (dev, trk);
    if (dfw)
    {   /* reuse the existing entry */
        DEVTRACE("cckddasd: wrtrk reusing%s dfw %p\n",
                 dfw->busy ? " busy" : "", dfw);
        if (dfw->busy) dfw->retry = 1;
    }
    else
    {   /* build a new deferred write queue element */
        dfw = malloc (CCKD_DFWQE_SIZE);
        dfw->busy = dfw->retry = 0;
        dfw->next = cckd->dfwq;
        cckd->dfwq = dfw;
    }
    dfw->buf = buf;
    dfw->trk = trk;

    DEVTRACE("cckddasd: wrtrk dfw %p trk %u buf %p\n",
             dfw, trk, buf);

    if (!(cckd->cdevhdr[cckd->sfn].options & CCKD_OPENED))
    {
        obtain_lock (&cckd->filelock);
        cckd->cdevhdr[cckd->sfn].options |= CCKD_OPENED;
        cckd_write_chdr (dev);
        if (cckd->threading)
        {
            if (!cckd->writeinit)
              for (i = 0; i < cckd->max_dfw; i++)
                create_thread (&cckd->dfwtid, &cckd->dfwattr,
                               cckd_dfw, dev);
            if (!cckd->gcinit)
                create_thread (&cckd->gctid, &cckd->gcattr,
                               cckd_gcol, dev);
        }
        release_lock (&cckd->filelock);
    }

#ifdef CCKD_NOTHREAD
    cckd_dfw (dev);
#endif

} /* end function cckd_write_trk */


/*-------------------------------------------------------------------*/
/* Deferred Write queue scan                                         */
/*-------------------------------------------------------------------*/
CCKD_DFWQE *cckd_scan_dfwq(DEVBLK *dev, int trk)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
CCKD_DFWQE     *dfw;                    /* -> deferred write q elem  */
int             q;                      /* Queue depth               */

    cckd = dev->cckd_ext;

    for (dfw = cckd->dfwq, q = 0; dfw; dfw = dfw->next, q++)
        if (trk == dfw->trk) break;
    if (!dfw) cckd->dfwqdepth = q;

    DEVTRACE("cckddasd: dfwqscan trk %d: %s depth %d\n", trk,
             dfw ? "found" :
             cckd->dfwq ? "not found" : "null queue", q);

    return dfw;

} /* end function cckd_scan_dfwq */


/*-------------------------------------------------------------------*/
/* Deferred Write thread                                             */
/*-------------------------------------------------------------------*/
void cckd_dfw(DEVBLK *dev)
{
int             rc;                     /* Return code               */
int             i;                      /* Index                     */
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
CCKD_DFWQE     *dfw;                    /* -> deferred write q elem  */
CCKD_DFWQE     *prevdfw;                /* -> previous write q elem  */
BYTE           *buf;                    /* Buffer                    */
BYTE           *cbuf;                   /* Compress buffer           */
int             len;                    /* Buffer length             */
unsigned long   clen;                   /* Compressed buffer length  */
int             trk;                    /* Track number              */
BYTE            compress;               /* Compression algorithm     */
int             d;                      /* Thread id                 */
int             dfwqovflow=0;           /* 1=dfw queue overflowed    */

    cckd = dev->cckd_ext;
    cbuf = malloc (dev->ckdtrksz);
    cckd->writeinit = 1;

    /* start off holding the deferred-write lock */
    obtain_lock (&cckd->dfwlock);
    d = cckd->dfwid++;

    do /* while(cckd->threading) */
    {
        while (cckd->dfwq != NULL)
        {   /* get a queue element */
            dfw = cckd->dfwq;
            while (dfw && dfw->busy) dfw = dfw->next;
            if (!dfw) goto dfw_wait;
        dfw_retry:
            dfw->busy = 1;
            dfw->retry = 0;
            buf = dfw->buf;
            trk = dfw->trk;
            len = cckd_trklen (dev, buf);
            DEVTRACE("cckddasd: %d dfw %p nxt %p buf %p trk %u len %d\n",
                     d, dfw, dfw->next, buf, trk, len);
            /* signal other thread if more work to do */
            if (cckd->dfwaiting && dfw->next)
                signal_condition (&cckd->dfwcond);

            release_lock (&cckd->dfwlock);

            /* Compress the track image */
            compress = (len < CCKD_COMPRESS_MIN ?
                        CCKD_COMPRESS_NONE : cckd->cdevhdr[cckd->sfn].compress);
            switch (compress) {

            case CCKD_COMPRESS_ZLIB:
                /* Compress the track image using zlib. Note
                   that the track header is not compressed. */
                memcpy (cbuf, buf, CKDDASD_TRKHDR_SIZE);
                cbuf[0] = CCKD_COMPRESS_ZLIB;
                clen = dev->ckdtrksz - CKDDASD_TRKHDR_SIZE;
                rc = compress2 (&cbuf[CKDDASD_TRKHDR_SIZE], &clen,
                                &buf[CKDDASD_TRKHDR_SIZE],
                                len - CKDDASD_TRKHDR_SIZE,
                                cckd->cdevhdr[cckd->sfn].compress_parm);
                clen += CKDDASD_TRKHDR_SIZE;
                DEVTRACE("cckddasd: %d dfw compress trk %d len %lu rc=%d\n",
                         d, trk, clen, rc);
                if (rc != Z_OK)
                {   /* compression error */
                    memcpy (cbuf, buf, len);
                    clen = len;
                    cbuf[0] = CCKD_COMPRESS_NONE;
                }
                break;

#ifdef CCKD_BZIP2
            case CCKD_COMPRESS_BZIP2:
                /* Compress the track image using bzip2. Note
                   that the track header is not compressed. */
                memcpy (cbuf, buf, CKDDASD_TRKHDR_SIZE);
                cbuf[0] = CCKD_COMPRESS_BZIP2;
                clen = dev->ckdtrksz - CKDDASD_TRKHDR_SIZE;
                rc = BZ2_bzBuffToBuffCompress (
                                &cbuf[CKDDASD_TRKHDR_SIZE],
                                (unsigned int *)&clen,
                                &buf[CKDDASD_TRKHDR_SIZE],
                                len - CKDDASD_TRKHDR_SIZE,
                                cckd->cdevhdr[cckd->sfn].compress_parm >= 1 &&
                                cckd->cdevhdr[cckd->sfn].compress_parm <= 9 ?
                                cckd->cdevhdr[cckd->sfn].compress_parm : 5, 0, 0);
                clen += CKDDASD_TRKHDR_SIZE;
                DEVTRACE("cckddasd: %d dfw compress trk %d len %lu rc=%d\n",
                         d, trk, clen, rc);
                if (rc != BZ_OK)
                {   /* compression error */
                    memcpy (cbuf, buf, len);
                    clen = len;
                    cbuf[0] = CCKD_COMPRESS_NONE;
                }
                break;
#endif

            default:
            case CCKD_COMPRESS_NONE:
                memcpy (cbuf, buf, len);
                clen = (len == CCKD_NULLTRK_SIZE) ? 0 : len;
                cbuf[0] = CCKD_COMPRESS_NONE;
                break;
            }

            /* if the same track was updated while we were
               compressing then try again */
            if (dfw->retry)
            {   DEVTRACE ("cckddasd: %d dfw %p retry trk %d\n", d, dfw, trk);
                obtain_lock (&cckd->dfwlock);
                goto dfw_retry;
            }

            /* write the track image */
            obtain_lock (&cckd->filelock);
            cckd_write_trkimg (dev, cbuf, clen, trk);
            release_lock (&cckd->filelock);

            obtain_lock (&cckd->dfwlock);

            /* if the same track was updated while we were
               writing it then try again */
            if (dfw->retry)
            {   DEVTRACE ("cckddasd: %d dfw %p retry trk %d\n", d, dfw, trk);
                goto dfw_retry;
            }

            /* remove the dfw entry from the queue */
            prevdfw = (CCKD_DFWQE *)&cckd->dfwq;
            while (prevdfw->next != dfw)
                prevdfw = prevdfw->next;
            prevdfw->next = dfw->next;
            release_lock (&cckd->dfwlock);

            /* free the queue entry */
            free (dfw);
            DEVTRACE ("cckddasd: %d dfw %p freed\n", d, dfw);

            /* free the buffer unless in use in the cache --  if the
               overflow flag is 1 then we already have the cache lock */
            if (!dfwqovflow) obtain_lock (&cckd->cachelock);
            for (i = 0; i < dev->ckdcachenbr; i++)
                if (cckd->cache[i].trk == trk)
                {
                    cckd->cache[i].writing = 0;
                    break;
                }
            if (!dfwqovflow) release_lock (&cckd->cachelock);

            if (i >= dev->ckdcachenbr)
            {   DEVTRACE ("cckddasd: %d dfw trk %d free buf %p\n",
                          d, trk, buf);
                free (buf);
            }

            /* if the dfw queue has exceeded its maximum depth,
               obtain the cache lock to prevent cckd_write_trk()
               from being called.  note that this will also lock
               out any other dfw threads for the time being. */
            if (!dfwqovflow && cckd->dfwqdepth >= cckd->max_dfwq)
            {
                DEVTRACE ("cckddasd: dfw q overflow size %d\n",
                          cckd->dfwqdepth);
                obtain_lock (&cckd->cachelock);
                dfwqovflow = 1;
            }

            obtain_lock (&cckd->dfwlock);

            /* if the dfw queue has overflowed, check its status */
            if (dfwqovflow)
            {
                cckd_scan_dfwq (dev, -1); /* sets cckd->dfwqdepth */
                if (cckd->dfwqdepth < cckd->max_dfwq)
                {
                    dfwqovflow = 0;
                    release_lock (&cckd->cachelock);
                    DEVTRACE ("cckddasd: dfw q overflow relieved size %d\n",
                          cckd->dfwqdepth);
                }
            }
        } /* while (cckd->dfwq != NULL) */

    dfw_wait:
        /* wait for another track to be written */
        if (cckd->threading)
        {   DEVTRACE ("cckddasd: %d dfw waiting\n", d);
            cckd->dfwaiting++;
            wait_condition (&cckd->dfwcond, &cckd->dfwlock);
            cckd->dfwaiting--;
        }

    } while (cckd->threading);

    release_lock (&cckd->dfwlock);
    free (cbuf) ;
#ifdef CCKD_NOTHREAD
    cckd_gcol(dev);
#endif
    obtain_lock (&cckd->termlock);
    signal_condition (&cckd->termcond);
    release_lock (&cckd->termlock);
} /* end thread cckd_dfw */


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
        DEVTRACE("cckddasd: get_space_at_end pos 0x%lx len %d\n",
                 fpos, len);
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

    DEVTRACE("cckddasd: get_space found pos 0x%lx len %d\n",
             fpos, len);

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
        logmsg ("file [%d] is not a shadow file\n", cckd->sfn);
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
            cckd_swapend (dev);
            rc = lseek (cckd->fd[sfx], CKDDASD_DEVHDR_SIZE, SEEK_SET);
            rc = read (cckd->fd[sfx], &cckd->cdevhdr[sfx], CCKDDASD_DEVHDR_SIZE);
        }
        else cckd->swapend[sfx] = 1;
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

    rc = lseek (cckd->fd[sfx], CCKD_L1TAB_POS + l1x * CCKD_L1ENT_SIZE,
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
unsigned int    fpos;                   /* Free space offset         */
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
        fpos = cckd->cdevhdr[sfx].free;
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
        fpos = cckd->cdevhdr[sfx].free;
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
unsigned int    fpos;                   /* Free space offset         */
int             sfx;                    /* File index                */
int             i;                      /* Index                     */

    cckd = dev->cckd_ext;
    sfx = cckd->sfn;

    if (!cckd->free) return;
    DEVTRACE ("cckddasd: writing free space, number %d\n",
              cckd->cdevhdr[sfx].free_number);

    fpos = cckd->cdevhdr[sfx].free;
    for (i = cckd->free1st; i >= 0; i = cckd->free[i].next)
    {
        rc = lseek (cckd->fd[sfx], fpos, SEEK_SET);
        rc = write (cckd->fd[sfx], &cckd->free[i], CCKD_FREEBLK_SIZE);
        fpos = cckd->free[i].pos;
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
        if  (lru == - 1 ||
             cckd->l2cache[i].tv.tv_sec < cckd->l2cache[lru].tv.tv_sec ||
            (cckd->l2cache[i].tv.tv_sec == cckd->l2cache[lru].tv.tv_sec &&
             cckd->l2cache[i].tv.tv_usec < cckd->l2cache[lru].tv.tv_usec))
            lru = i;
    }

    /* check for level 2 cache hit */
    if (fnd >= 0)
    {
        DEVTRACE ("cckddasd: l2[%d,%d] cache[%d] hit\n", sfx, l1x, fnd);
        cckd->l2 = (CCKD_L2ENT *)cckd->l2cache[fnd].buf;
        gettimeofday (&cckd->l2cache[fnd].tv, NULL);
        return 0;
    }

    /* get buffer for level 2 table if there isn't one */
    if (!cckd->l2cache[lru].buf)
        cckd->l2cache[lru].buf = malloc (CCKD_L2TAB_SIZE);

    cckd->l2cache[lru].sfx = sfx;
    cckd->l2cache[lru].l1x = l1x;
    gettimeofday (&cckd->l2cache[lru].tv, NULL);

    cckd->l2 = (CCKD_L2ENT *)cckd->l2cache[lru].buf;


    /* check for null table */
    if (!cckd->l1[sfx][l1x] || cckd->l1[sfx][l1x] == 0xffffffff)
    {
        if (cckd->l1[sfx][l1x])
            memset (cckd->l2, 0xff, CCKD_L2TAB_SIZE);
        else memset (cckd->l2, 0, CCKD_L2TAB_SIZE);
        DEVTRACE("cckddasd: l2[%d,%d], null cache[%d]\n", sfx, l1x, lru);
        rc = 0;
    }
    /* read the new level 2 table */
    else
    {
        rc = lseek (cckd->fd[sfx], cckd->l1[sfx][cckd->l1x], SEEK_SET);
        rc = read (cckd->fd[sfx], cckd->l2, CCKD_L2TAB_SIZE);
        DEVTRACE("cckddasd: l2[%d,%d] read pos 0x%x cache[%d]\n",
                 sfx, l1x, cckd->l1[sfx][l1x], lru);
        cckd->l2reads[sfx]++;
        cckd->totl2reads++;
    }

    /* get fields in correct byte order (read-only files only) */
    if (cckd->swapend[sfx]) cckd_swapend_l2 (cckd->l2);

    return rc;

} /* end function cckd_read_l2 */


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
        rc = lseek (cckd->fd[sfx], cckd->l1[sfx][l1x], SEEK_SET);
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
                    cckd->l1[sfx][l1x] + l2x * CCKD_L2ENT_SIZE,
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
int cckd_read_trkimg (DEVBLK *dev, BYTE *buf, int trk)
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
        rc = lseek (cckd->fd[sfx], l2.pos, SEEK_SET);
        rc = read (cckd->fd[sfx], buf, l2.len);
        cckd->reads[sfx]++;
        cckd->totreads++;
    }
    else rc = cckd_null_trk (dev, buf, trk);

    /* if sfx is zero and l2.pos is all 0xff's, then we are reading
       from a regular ckd file (ie not a compressed one) */
    if (!sfx && l2.pos == 0xffffffff)
    {
        if (trk < dev->ckdtrks)
        {
            rc = lseek (cckd->fd[sfx],
                        trk * dev->ckdtrksz + CKDDASD_DEVHDR_SIZE,
                        SEEK_SET);
            rc = read (cckd->fd[sfx], buf, dev->ckdtrksz);
            rc = cckd_trklen (dev, buf);
            cckd->reads[sfx]++;
            cckd->totreads++;
        }
        else
        {
            logmsg ("%4.4x: cckddasd: trkimg %d beyond end of "
                    "regular ckd file: trks %d\n",
                    dev->devnum, trk, dev->ckdtrks);
            rc = cckd_null_trk (dev, buf, trk);
        }
    }

    DEVTRACE ("cckddasd: trkimg %d read sfx %d pos 0x%x len %d "
              "%2.2x%2.2x%2.2x%2.2x%2.2x\n",
              trk, sfx, l2.pos, rc,
              buf[0], buf[1], buf[2], buf[3], buf[4]);

    if (cckd_cchh (dev, buf, trk) < 0)
        rc = cckd_null_trk (dev, buf, trk);

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
                        trk * dev->ckdtrksz + CKDDASD_DEVHDR_SIZE,
                        SEEK_SET);
            rc = write (cckd->fd[sfx], buf, len ? len : CCKD_NULLTRK_SIZE);
            cckd->writes[sfx]++;
            cckd->totwrites++;
            DEVTRACE ("cckddasd: trkimg %d write regular len %d "
              "%2.2x%2.2x%2.2x%2.2x%2.2x\n",
              trk, len, buf[0], buf[1], buf[2], buf[3], buf[4]);
        }
        else
        {   logmsg ("%4.4x: cckddasd: wrtrk %d beyond end of file "
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
        {   rc = lseek (cckd->fd[sfx], l2.pos, SEEK_SET);
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
        logmsg ("%4.4X:cckddasd trklen err for %2.2x%2.2x%2.2x%2.2x%2.2x\n",
                dev->devnum, buf[0], buf[1], buf[2], buf[3], buf[4]);
        size = dev->ckdtrksz;
    }

    return size;
}


/*-------------------------------------------------------------------*/
/* Build a null track                                                */
/*-------------------------------------------------------------------*/
int cckd_null_trk(DEVBLK *dev, BYTE *buf, int trk)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
U16             cyl;                    /* Cylinder                  */
U16             head;                   /* Head                      */
FWORD           cchh;                   /* Cyl, head big-endian      */

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
    memcpy (&buf[21], cchh, sizeof(cchh));
    buf[25] = 1;
    memcpy (&buf[29], eighthexFF, 8);

    return CCKD_NULLTRK_SIZE;

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

    logmsg ("%4.4X:cckddasd: invalid trk hdr trk %d "
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
        logmsg ("cckddasd: no shadow file name specified%s\n", "");
        return -1;
    }

    /* Error if number shadow files exceeded */
    if (sfx > CCKD_MAX_SF)
    {
        logmsg ("cckddasd: [%d] number of shadow files exceeded: %d\n",
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
        if ((cckd->cdevhdr[cckd->sfn].options & CCKD_OPENED) == 0)
            rc = cckd_chkdsk (cckd->fd[cckd->sfn], sysblk.msgpipew, 0);
        else
        {
            logmsg("cckddasd: forcing chkdsk -1, %s not closed\n",
                   dev->ckdsfn);
            rc = cckd_chkdsk (cckd->fd[cckd->sfn], sysblk.msgpipew, 1);
        }
        if (rc < 0) return -1;

        /* re-read the compressed header if the file was repaired */
        if (rc > 0)
        {
            rc = cckd_read_chdr (dev);
            if (rc < 0) return -1;
        }

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
            logmsg ("cckddasd: error re-opening %s readonly\n   %s\n",
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
int            *cyltab;                 /* Possible cyls for device  */
int             i;                      /* Loop index                */
int             cyls;                   /* Number cylinders for devt */
int             l1size;                 /* Size of level 1 table     */

int             cyls2311[]   = {200, 0};
int             cyls2314[]   = {200, 0};
int             cyls3330[]   = {404, 808, 0};
int             cyls3340[]   = {348, 696, 0};
int             cyls3350[]   = {555, 0};
int             cyls3375[]   = {959, 0};
int             cyls3380[]   = {885, 1770, 2655, 0};
int             cyls3390[]   = {1113, 2226, 3339, 10017, 0};

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
    {    logmsg ("cckddasd: shadow file open error: %s\n",
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
    switch (dev->devtype) {
    case 0x2311: cyltab = cyls2311;
                 break;
    case 0x2314: cyltab = cyls2314;
                 break;
    case 0x3330: cyltab = cyls3330;
                 break;
    case 0x3340: cyltab = cyls3340;
                 break;
    case 0x3350: cyltab = cyls3350;
                 break;
    case 0x3375: cyltab = cyls3375;
                 break;
    case 0x3380: cyltab = cyls3380;
                 break;
    case 0x3390: cyltab = cyls3390;
                 break;
    default:     logmsg ("Unsupported device type %4.4x\n",
                       dev->devtype);
               return -1;
    } /* end switch(dev->devtype) */
    for (i = 0; cyltab[i] != 0; i++)
        if (dev->ckdcyls <= cyltab[i]) break;
    cyls = cyltab[i];
    if (!cyls)
    {    logmsg ("Unsupported number of cylinders (%d) for %4.4x device\n",
                 dev->ckdcyls, dev->devtype);
         return -1;
    }
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
int             i;                      /* Loop index                */
int             dfw_locked;             /* Flag byte                 */
BYTE            sfn[256];               /* Shadow file name          */

    cckd = dev->cckd_ext;
    if (!cckd)
    {
        logmsg ("%4.4X: cckddasd: device is not a shadow file\n",
                dev->devnum);
        return;
    }

    /* schedule updated track entries to be written */
    dfw_locked = 0;
    obtain_lock (&cckd->cachelock);
    for (i = 0; i < dev->ckdcachenbr && cckd->cache; i++)
    {
        if (cckd->cache[i].updated)
        {   cckd->cache[i].updated = 0;
            cckd->cache[i].writing = 1;
            if (!dfw_locked)
            {   obtain_lock (&cckd->dfwlock);
                dfw_locked = 1;
            }
            cckd_write_trk (dev, cckd->cache[i].buf);
        }
    }

    /* signal the deferred write thread */
    if (cckd->dfwq && cckd->dfwaiting)
    {
        if (!dfw_locked) obtain_lock (&cckd->dfwlock);
        if (cckd->dfwq && cckd->dfwaiting)
            signal_condition (&cckd->dfwcond);
        release_lock (&cckd->dfwlock);
    }
    release_lock (&cckd->cachelock);

    /* wait a bit */
    while (cckd->dfwq) sleep (1);

    /* obtain control of the file */
    obtain_lock (&cckd->filelock);

    /* harden the current file */
    cckd_harden (dev);

    /* create a new shadow file */
    rc = cckd_sf_new (dev);
    if (rc < 0)
    {
        logmsg ("cckddasd: error adding shadow file: %s\n",
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
    logmsg ("cckddasd: shadow file [%d] %s added\n",
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

    cckd = dev->cckd_ext;
    if (!cckd)
    {
        logmsg ("%4.4X: cckddasd: device is not a shadow file\n",
                dev->devnum);
        return;
    }

    if (!cckd->sfn)
    {
        logmsg ("cckddasd: cannot remove base file\n");
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
        logmsg ("cckddasd: cannot remove shadow file [%d], "
                "file [%d] cannot be opened read-write\n",
                sfx, sfx-1);
        release_lock (&cckd->filelock);
        return;
    }
    cckd->open[sfx-1] = CCKD_OPEN_RW;
    DEVTRACE ("cckddasd: sfrem [%d] %s opened r/w\n", sfx-1, sfn);

    /* harden the current file */
    cckd_harden (dev);

    /* make the previous file active */
    cckd->sfn--;

    /* perform backwards merge to a compressed file */
    if (merge && cckd->cdevhdr[sfx-1].size)
    {
        buf = malloc (dev->ckdtrksz);
        for (i = 0; i < cckd->cdevhdr[sfx].numl1tab; i++)
        {
            DEVTRACE ("cckddasd: sfrem merging to compressed file [%d] %s\n", sfx-1, sfn);

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
                rc = lseek (cckd->fd[sfx], l2[j].pos, SEEK_SET);
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
             logmsg ("cckddasd: cannot merge, last used track %d exceeds "
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
                if (!cckd->l2[j].pos) cckd_null_trk (dev, buf, i * 256 + j);
                else
                {   rc = lseek (cckd->fd[sfx], cckd->l2[j].pos, SEEK_SET);
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
                        len = cckd_null_trk (dev, buf2, i * 256 + j);
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
                        len = cckd_null_trk (dev, buf2, i * 256 + j);
                    break;
#endif
                default:
                    len = cckd_null_trk (dev, buf2, i * 256 + j);
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
    close (cckd->fd[sfx]);
    free (cckd->l1[sfx]);
    cckd_sf_name (dev, sfx, (char *)&sfn);
    rc = unlink ((char *)&sfn);

    /* finished successful */
    logmsg ("cckddasd: shadow file [%d] successfully %s\n",
            sfx, merge ? "merged" : "removed");
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
        logmsg ("%4.4X: cckddasd: device is not a shadow file\n",
                dev->devnum);
        return;
    }
    if (CCKD_MAX_SF == 0)
    {
        logmsg ("%4.4X: cckddasd: file shadowing not activated\n",
                dev->devnum);
        return;
    }

    obtain_lock (&cckd->filelock);

    if (cckd->sfn)
    {
        logmsg ("%4.4X: cckddasd: shadowing is already active\n",
                dev->devnum);
        release_lock (&cckd->filelock);
        return;
    }

    strcpy ((char *)&dev->ckdsfn, (const char *)sfn);
    logmsg ("cckddasd: shadow file name set to %s\n", sfn);
    release_lock (&cckd->filelock);

} /* end function cckd_sf_newname */


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
unsigned int    size=0,free=0;          /* Total size, free space    */
int             freenbr=0;              /* Total number free spaces  */
BYTE            sfn[256];               /* Shadow file name          */


    cckd = dev->cckd_ext;
    if (!cckd)
    {
        logmsg ("%4.4X: cckddasd: device is not a shadow file\n",
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
    logmsg ("cckddasd:           size free  nbr st   reads  writes l2reads    hits switches\n");
    if (cckd->readaheads || cckd->misses)
    logmsg ("cckddasd:                                                  readaheads   misses\n");
    logmsg ("cckddasd: --------------------------------------------------------------------\n");

    /* total statistics */
    logmsg ("cckddasd: [*] %10u %3d%% %4d    %7d %7d %7d %7d  %7d\n",
            size, (free * 100) / size, freenbr,
            cckd->totreads, cckd->totwrites, cckd->totl2reads,
            cckd->cachehits, cckd->switches);
    if (cckd->readaheads || cckd->misses)
    logmsg ("cckddasd:                                                     %7d  %7d\n",
            cckd->readaheads, cckd->misses);

    /* base file statistics */
    logmsg ("cckddasd: %s\n", dev->filename);
    logmsg ("cckddasd: [0] %10ld %3ld%% %4d %s %7d %7d %7d\n",
            st.st_size, (cckd->cdevhdr[0].free_total * 100) / st.st_size,
            cckd->cdevhdr[0].free_number, ost[cckd->open[0]],
            cckd->reads[0], cckd->writes[0], cckd->l2reads[0]);

    if (dev->ckdsfn[0] && CCKD_MAX_SF > 0)
    {
        cckd_sf_name ( dev, -1, (char *)&sfn);
        logmsg ("cckddasd: %s\n", sfn);
    }

    /* shadow file statistics */
    for (i = 1; i <= cckd->sfn; i++)
    {
        logmsg ("cckddasd: [%d] %10d %3d%% %4d %s %7d %7d %7d\n",
                i, cckd->cdevhdr[i].size,
                (cckd->cdevhdr[i].free_total * 100) / cckd->cdevhdr[i].size,
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
int             i,sfx;                  /* Indices                   */
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             dfw_locked;             /* Dfw lock obtained         */
int             size;                   /* File size                 */
int             wait;                   /* Seconds to wait           */
struct timespec tm;                     /* Time-of-day to wait       */
int             gc;                     /* Garbage states            */
struct          CCKD_GCOL {             /* Garbage collection parms  */
int             algorithm;              /* Algorithm                 */
int             interval;               /* Collection interval (sec) */
int             iterations;             /* Iterations per interval   */
int             size;                   /* Bytes per iteration       */
    }           gctab[5]= {             /* default gcol parameters   */
               {GC_COMBINE_HI,  8, 8, -4},   /* critical  50% - 100% */
               {GC_COMBINE_HI,  8, 4, -2},   /* severe    25% - 50%  */
               {GC_COMBINE_HI,  8, 4, -1},   /* moderate 12.5%- 25%  */
               {GC_COMBINE_LO, 10, 2, -1},   /* light     6.3%- 12.5%*/
               {GC_COMBINE_LO, 20, 1, -1}};  /* none       0% -  6.3%*/
char           *gc_state[]=             /* Garbage states            */
                   {"critical", "severe", "moderate", "light", "none"};

    cckd = dev->cckd_ext;
    sfx = cckd->sfn;

    /* first-time initialization */
    if (!cckd->gcinit) cckd->gcinit=1;

#ifdef CCKD_NOTHREAD
    if (time(NULL) < cckd->gctime) return;
#endif
    time (&cckd->gctime);

    do /* while cckd->threading */
    {
        sfx = cckd->sfn;

        /* find updated tracks if enough time has elapsed */
        dfw_locked = 0;
        obtain_lock (&cckd->cachelock);
        for (i = 0; i < dev->ckdcachenbr && cckd->cache; i++)
        {
            if (cckd->cache[i].updated && cckd->cache[i].tv.tv_sec +
                cckd->max_wt < cckd->gctime)
            {   cckd->cache[i].updated = 0;
                cckd->cache[i].writing = 1;
                if (!dfw_locked)
                {   obtain_lock (&cckd->dfwlock);
                    dfw_locked = 1;
                }
                cckd_write_trk (dev, cckd->cache[i].buf);
            }
        }

        /* signal dfw thread if any writes were scheduled */
        if (dfw_locked)
        {
            if (cckd->dfwq && cckd->dfwaiting)
                signal_condition (&cckd->dfwcond);
            release_lock (&cckd->dfwlock);
        }
        release_lock (&cckd->cachelock);

        /* go wait if no free space */
        if (!cckd->cdevhdr[sfx].free_number)
        {
            wait = cckd->max_wt;
            goto gc_wait;
        }

        /* determine garbage state */
        size = cckd->cdevhdr[sfx].size;
        if      (cckd->cdevhdr[sfx].free_total >= (size = size /2)) gc = 0;
        else if (cckd->cdevhdr[sfx].free_total >= (size = size /2)) gc = 1;
        else if (cckd->cdevhdr[sfx].free_total >= (size = size /2)) gc = 2;
        else if (cckd->cdevhdr[sfx].free_total >= (size = size /2)) gc = 3;
        else gc = 4;

        /* reduce state for `smallish' files */
        if (gc < 2 && cckd->cdevhdr[sfx].size < 10 * 1024*1024) gc = 2;

        if (gc < 3)
            logmsg ("cckddasd: %4.4x garbage collection state is %s\n",
                    dev->devnum, gc_state[gc]);

        /* call the garbage collector */
        switch (gctab[gc].algorithm) {
        default:
        case GC_COMBINE_LO:
             cckd_gc_combine(dev, gctab[gc].iterations,
                                  gctab[gc].size, 0);
             break;

        case GC_COMBINE_HI:
             cckd_gc_combine(dev, gctab[gc].iterations,
                                  gctab[gc].size, 1);
             break;
        }

        /* wait a bit */
        wait = gctab[gc].interval;
        if (wait < 1) wait = 10;

    gc_wait:
#ifndef CCKD_NOTHREAD
        time (&cckd->gctime);
        DEVTRACE ( "cckddasd: gcol waiting %d seconds at %s",
                   wait, ctime(&cckd->gctime));

        tm.tv_sec = cckd->gctime + wait;
        tm.tv_nsec = 0;
        obtain_lock (&cckd->gclock);
        wait_timed_condition ( &cckd->gccond, &cckd->gclock, &tm);
        release_lock (&cckd->gclock);
        time (&cckd->gctime);
        DEVTRACE ( "cckddasd: gcol waking up at %s", ctime(&cckd->gctime));
#endif
    } while (cckd->threading);

    obtain_lock (&cckd->termlock);
    cckd->gcinit = 0;
    signal_condition (&cckd->termcond);
    release_lock (&cckd->termlock);
} /* end thread cckd_gcol */

/*-------------------------------------------------------------------*/
/* Garbage Collection -- Combine algorithm                           */
/*                                                                   */
/* Selects the free space block that will combine with the most      */
/* other free space blocks within `size' bytes.                      */
/* In the event of a tie selects the free space block closest to the */
/* beginning of the file (hi == 0) or the end of the file (hi == 1). */
/*                                                                   */
/*-------------------------------------------------------------------*/
void cckd_gc_combine(DEVBLK *dev, int iterations, int size, int hi)
{
int             rc;                     /* Return code               */
int             f,i,j,n,p;              /* Indices                   */
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             sfx,l1x,l2x;            /* Table Indices             */
int             read_len;               /* Read length               */
int             len;                    /* Length                    */
off_t           pos;                    /* Buffer offset             */
off_t           off;                    /* Offset into buffer        */
off_t           fpos,npos;              /* Free space offsets        */
int             moved;                  /* Amount of space moved     */

    cckd = dev->cckd_ext;
    sfx = cckd->sfn;

    if (iterations == 0) iterations = 1;
    if (size == 0) size = dev->ckdtrksz;
    if (size < 0) size = abs (size) * dev->ckdtrksz;

    DEVTRACE("cckddasd: gccomb iter %d sz %d 1st 0x%x nbr %d\n",
             iterations, size, cckd->cdevhdr[sfx].free, cckd->cdevhdr[sfx].free_number);

    if (cckd->cdevhdr[sfx].free == 0 || cckd->cdevhdr[sfx].free_number == 0)
        return;

    /* free the old buffer if it's too short */
    if (cckd->gcbuf != NULL && size > cckd->gcbuflen)
    {
        free (cckd->gcbuf);
        cckd->gcbuf = NULL;
    }

    /* get a buffer if we don't have one */
    if (cckd->gcbuf == NULL)
    {
       cckd->gcbuflen = size > dev->ckdtrksz ?
                        size : dev->ckdtrksz;
       cckd->gcbuf = malloc (cckd->gcbuflen);
    }

    for ( ; iterations && cckd->threading; iterations--)
    {
        /* get the file lock */
        obtain_lock (&cckd->filelock);
        sfx = cckd->sfn;

        /* exit if no more free space */
        if (cckd->cdevhdr[sfx].free == 0)
        {   release_lock (&cckd->filelock);
            break;
        }

        /* make sure the free space chain is built */
        if (!cckd->free) cckd_read_fsp (dev);

        /* find the free space that can combine with the most */
        fpos = cckd->cdevhdr[sfx].free;
        for (f = i = cckd->free1st; i >= 0; i = cckd->free[i].next)
        {
            pos = fpos + cckd->free[i].len + size * iterations;
            cckd->free[i].cnt = (pos >= cckd->cdevhdr[sfx].size);
            npos = cckd->free[i].pos;
            for (j = cckd->free[i].next; j >= 0; j = cckd->free[j].next)
            {
                if (pos < npos) break;
                cckd->free[i].cnt++;
                npos = cckd->free[j].pos;
            }
            fpos = cckd->free[i].pos;
            if (cckd->free[i].cnt > cckd->free[f].cnt) f = i;
            else
                if (hi && cckd->free[i].cnt == cckd->free[f].cnt) f = i;
        }

        if (cckd->free[f].prev >= 0)
            fpos = cckd->free[cckd->free[f].prev].pos;
        else fpos = cckd->cdevhdr[sfx].free;
        pos = fpos + cckd->free[f].len;

        DEVTRACE ("cckddasd: gccomb free pos 0x%lx len %d cnt %d\n",
                  fpos, cckd->free[f].len, cckd->free[f].cnt);

        /* read to end of file or `size' */
        if (pos + size <= cckd->cdevhdr[sfx].size)
            read_len = size;
        else
            read_len = cckd->cdevhdr[sfx].size - pos;

        rc = lseek (cckd->fd[sfx], pos, SEEK_SET);
        rc = read (cckd->fd[sfx], cckd->gcbuf, read_len);

        /* make sure we read at least 1 complete img */
        len = cckd_gc_len(dev, cckd->gcbuf, pos, 0, read_len);
        if (len > read_len)
        {
            rc = read(cckd->fd[sfx], &cckd->gcbuf[read_len],
                      len - read_len);
            read_len = len;
        }

        DEVTRACE("cckddasd: gccomb read pos 0x%lx len %u\n",
                 pos, read_len);

        /* relocate each track image or level 2 table in the buffer */
        off = moved = 0;
        while (len !=0 && off + len <= read_len)
        {
                l1x = cckd->gcl1x;
            l2x = cckd->gcl2x;

            if (cckd->gcl2x == -1)
            {   /* reposition a level 2 table */
                if (memcmp (&cckd->gcbuf[off], &cckd_empty_l2tab, len))
                {
                    DEVTRACE("cckddasd: gccomb repo l2[%u]: "
                             "0x%x to 0x%x\n", l1x, cckd->l1[sfx][l1x],
                             cckd->l1[sfx][l1x] - cckd->free[f].len);
                    cckd->l1[sfx][l1x] -= cckd->free[f].len;
                    cckd->l2updated = 1;
                    moved += len;
                }
                else
                { /* empty level 2 table */
                    DEVTRACE("cckddasd: gccomb empty l2[%d]\n",l1x);
                    cckd->l1[sfx][l1x] = 0;
                    cckd->cdevhdr[sfx].used -= len;
                    cckd->cdevhdr[sfx].free_total += len;
                    cckd->free[f].len += len;
                }
                cckd->l1updated = 1;
            }
            else
            {   /* repositioning a track image */
                cckd->l2[l2x].pos -= cckd->free[f].len;
                rc = lseek (cckd->fd[sfx], cckd->l2[l2x].pos, SEEK_SET);
                rc = write (cckd->fd[sfx], &cckd->gcbuf[off], len);
                moved += len;
                DEVTRACE("cckddasd: gccomb repo trk %u: 0x%x(%u) to 0x%x\n",
                         l1x * 256 + l2x, cckd->l2[l2x].pos + cckd->free[f].len,
                         cckd->l2[l2x].len, cckd->l2[l2x].pos);
                cckd->l2updated = 1;
            }
            off += len;

            /* check if we are now at a free space */
            if (pos + off == cckd->free[f].pos)
            {
                n = cckd->free[f].next;
                DEVTRACE("cckddasd: gccomb combine free 0x%lx len %d\n",
                         pos + off, cckd->free[n].len);
                off += cckd->free[n].len;
                cckd->free[f].pos = cckd->free[n].pos;
                cckd->free[f].len += cckd->free[n].len;
                cckd->free[f].next = cckd->free[n].next;
                cckd->free[n].next = cckd->freeavail;
                cckd->freeavail = n;
                n = cckd->free[f].next;
                if (n >= 0) cckd->free[n].prev = f;
                cckd->cdevhdr[sfx].free_number--;
            }

            /* get length of next space */
            len = cckd_gc_len(dev, cckd->gcbuf, pos, off, read_len);
        }

        /* write the lookup tables */
        cckd_write_l2 (dev);
        cckd_write_l1 (dev);

        DEVTRACE("cckddasd: gccomb space moved %d\n", moved);

        /* relocate the free space */
        fpos += moved;
        DEVTRACE("cckddasd: gccomb new free 0x%lx len %d nbr %d\n",
                 fpos, cckd->free[f].len, cckd->cdevhdr[sfx].free_number);
        p = cckd->free[f].prev;
        if (p >= 0)
            cckd->free[p].pos = fpos;
        else cckd->cdevhdr[sfx].free = fpos;
        if (cckd->free[f].len > cckd->cdevhdr[sfx].free_largest)
            cckd->cdevhdr[sfx].free_largest = cckd->free[f].len;

        /* if the free space is at the end then release it */
        if (fpos + cckd->free[f].len == cckd->cdevhdr[sfx].size)
            cckd_rel_free_atend (dev, fpos, cckd->free[f].len, f);

        release_lock (&cckd->filelock);
    }

    /* free the buffer if it's not standard */
    if (cckd->gcbuflen > dev->ckdtrksz)
    {
        free (cckd->gcbuf);
        cckd->gcbuf = NULL;
    }

    return;

} /* end function cckd_gc_combine */


/*-------------------------------------------------------------------*/
/* Garbage Collection -- Return length of space                      */
/*                                                                   */
/* Space should either be a track image or a level 2 table.          */
/* Sets the following fields in the CCKD extension:                  */
/*                                                                   */
/*   gcl1x   -  level 1 table index for the space                    */
/*   gcl2x   -  level 2 table index for a track image else -1        */
/*                                                                   */
/*-------------------------------------------------------------------*/
int cckd_gc_len(DEVBLK *dev, BYTE *buf, off_t pos, int off, int len)
{
int             rc;                     /* Return code               */
int             sfx;                    /* File index                */
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             cyl;                    /* Cylinder  (from trk hdr)  */
int             hd;                     /* Head      (from trk hdr)  */
int             trk;                    /* Track number              */

    if (off + CKDDASD_TRKHDR_SIZE > len) return 0;
    cckd = dev->cckd_ext;
    sfx = cckd->sfn;

    /* check for level 2 table */
    cckd->gcl2x = -1;
    for (cckd->gcl1x = 0; cckd->gcl1x < cckd->cdevhdr[sfx].numl1tab;
         cckd->gcl1x++)
        if (cckd->l1[sfx][cckd->gcl1x] == pos + off)
        {   /* space is a level 2 table */
            DEVTRACE("cckddasd: gclen off 0x%lx is l2tab[%d] len %ld\n",
                     pos + off, cckd->gcl1x, (long) CCKD_L2TAB_SIZE);
            rc = cckd_read_l2 (dev, sfx, cckd->gcl1x);
            return CCKD_L2TAB_SIZE;
        }

    cyl = (buf[off+1] << 8) + buf[off+2];
    hd  = (buf[off+3] << 8) + buf[off+4];
    trk = cyl * dev->ckdheads + hd;
    cckd->gcl1x = trk >> 8;
    cckd->gcl2x = trk & 0xff;

    /* check for track image */
    if (cyl < dev->ckdcyls
     && hd  < dev->ckdheads
     && cckd->l1[sfx][cckd->gcl1x]
     && cckd->l1[sfx][cckd->gcl1x] != 0xffffffff)
    {   /* get the level 2 table entry */
        rc = cckd_read_l2 (dev, sfx, cckd->gcl1x);
        if (cckd->l2[cckd->gcl2x].pos == pos + off)
        {   /* space is a track image */
            DEVTRACE("cckddasd: gclen off 0x%lx is trk %d len %d\n",
                     pos + off, trk, cckd->l2[cckd->gcl2x].len);
            return cckd->l2[cckd->gcl2x].len;
        }
    }

    /* unknown space */
    logmsg ("cckddasd: %4.4x unknown space at offset 0x%lx\n",
            dev->devnum, pos + off);
    logmsg ("cckddasd: %4.4x %2.2x%2.2x%2.2x%2.2x %2.2x%2.2x%2.2x%2.2x\n",
            dev->devnum, buf[off], buf[off+1], buf[off+2], buf[off+3],
            buf[off+4], buf[off+5], buf[off+6], buf[off+7]);

    cckd->gcl1x = cckd->gcl2x = -1;
    return 0;

} /* end function cckd_gc_len */


/*-------------------------------------------------------------------*/
/* Swap endian                                                       */
/*-------------------------------------------------------------------*/
void cckd_swapend (DEVBLK *dev)
{
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int               i;                    /* Index                     */
int               sfx;                  /* File index                */
CCKDDASD_DEVHDR   cdevhdr;              /* Compressed ckd header     */
CCKD_L1ENT       *l1;                   /* Level 1 table             */
CCKD_L2ENT        l2[256];              /* Level 2 table             */
CCKD_FREEBLK      free1;                /* Free block                */

    cckd = dev->cckd_ext;
    sfx = cckd->sfn;

    /* fix the compressed ckd header */

    lseek (cckd->fd[sfx], CKDDASD_DEVHDR_SIZE, SEEK_SET);
    read (cckd->fd[sfx], &cdevhdr, CCKDDASD_DEVHDR_SIZE);
    cckd_swapend_chdr (&cdevhdr);
    lseek (cckd->fd[sfx], CKDDASD_DEVHDR_SIZE, SEEK_SET);
    write (cckd->fd[sfx], &cdevhdr, CCKDDASD_DEVHDR_SIZE);

    /* fix the level 1 table */

    l1 = malloc (cdevhdr.numl1tab * CCKD_L1ENT_SIZE);
    lseek (cckd->fd[sfx], CCKD_L1TAB_POS, SEEK_SET);
    read (cckd->fd[sfx], l1, cdevhdr.numl1tab * CCKD_L1ENT_SIZE);
    cckd_swapend_l1 (l1, cdevhdr.numl1tab);
    lseek (cckd->fd[sfx], CCKD_L1TAB_POS, SEEK_SET);
    write (cckd->fd[sfx], l1, cdevhdr.numl1tab * CCKD_L1ENT_SIZE);

    /* fix the level 2 tables */

    for (i=0; i<cdevhdr.numl1tab; i++)
    {
        if (l1[i] && l1[i] != 0xffffffff)
        {
            lseek (cckd->fd[sfx], l1[i], SEEK_SET);
            read (cckd->fd[sfx], &l2, CCKD_L2TAB_SIZE);
            cckd_swapend_l2 ((CCKD_L2ENT *)&l2);
            lseek (cckd->fd[sfx], l1[i], SEEK_SET);
            write (cckd->fd[sfx], &l2, CCKD_L2TAB_SIZE);
        }
    }
    free (l1);

    /* fix the free chain */
    for (i = cdevhdr.free; i; i = free1.pos)
    {
        lseek (cckd->fd[sfx], i, SEEK_SET);
        read (cckd->fd[sfx], &free1, CCKD_FREEBLK_SIZE);
        cckd_swapend_free (&free1);
        lseek (cckd->fd[sfx], i, SEEK_SET);
        write (cckd->fd[sfx], &free1, CCKD_FREEBLK_SIZE);
    }
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

    for (i=0; i<n; i++)
        cckd_swapend4 ((char *) &l1[i]);
}


/*-------------------------------------------------------------------*/
/* Swap endian - level 2 table                                       */
/*-------------------------------------------------------------------*/
void cckd_swapend_l2 (CCKD_L2ENT *l2)
{
int i;                                  /* Index                     */

    for (i=0; i<256; i++)
    {
        cckd_swapend4 ((char *) &l2[i].pos);
        cckd_swapend2 ((char *) &l2[i].len);
        cckd_swapend2 ((char *) &l2[i].size);
    }
}


/*-------------------------------------------------------------------*/
/* Swap endian - free space entry                                    */
/*-------------------------------------------------------------------*/
void cckd_swapend_free (CCKD_FREEBLK *free)
{
    cckd_swapend4 ((char *) &free->pos);
    cckd_swapend4 ((char *) &free->len);
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
/* Print internal trace                                              */
/*-------------------------------------------------------------------*/
void cckd_print_itrace(DEVBLK *dev)
{
#ifdef CCKD_ITRACEMAX
CCKDDASD_EXT   *cckd;                   /* -> cckd extension         */
int             start, i;               /* Start index, index        */

logmsg ("%4.4X:cckddasd: print_itrace\n", dev->devnum);
    cckd = dev->cckd_ext;
    i = start = cckd->itracex;
    do
    {
        if (i >= 128*CCKD_ITRACEMAX) i = 0;
        if (cckd->itrace[i] != '\0')
            logmsg ("%s", &cckd->itrace[i]);
        i+=128;
    } while (i != start);
    fflush (sysblk.msgpipew);
    sleep (2);
#endif
}

#else /* NO_CCKD */

int cckddasd_init_handler ( DEVBLK *dev, int argc, BYTE *argv[] )
{
    logmsg ("%4.4X cckddasd support not generated\n", dev->devnum);
    return -1;
}
int cckddasd_close_device (DEVBLK *dev)
{
    logmsg ("%4.4X cckddasd support not generated\n", dev->devnum);
    return -1;
}
off_t cckd_lseek(DEVBLK *dev, int fd, off_t offset, int pos)
{
    logmsg ("%4.4X cckddasd support not generated\n", dev->devnum);
    return -1;
}
ssize_t cckd_read(DEVBLK *dev, int fd, char *buf, size_t N)
{
    logmsg ("%4.4X cckddasd support not generated\n", dev->devnum);
    return -1;
}
ssize_t cckd_write(DEVBLK *dev, int fd, const void *buf, size_t N)
{
    logmsg ("%4.4X cckddasd support not generated\n", dev->devnum);
    return -1;
}
void cckd_print_itrace(DEVBLK *dev)
{
    logmsg ("%4.4X cckddasd support not generated\n", dev->devnum);
}
void cckd_sf_add(DEVBLK *dev)
{
    logmsg ("%4.4X cckddasd support not generated\n", dev->devnum);
}
void cckd_sf_remove(DEVBLK *dev, int merge)
{
    logmsg ("%4.4X cckddasd support not generated\n", dev->devnum);
}
void cckd_sf_newname(DEVBLK *dev, BYTE *sfn)
{
    logmsg ("%4.4X cckddasd support not generated\n", dev->devnum);
}
void cckd_sf_stats(DEVBLK *dev)
{
    logmsg ("%4.4X cckddasd support not generated\n", dev->devnum);
}

#endif

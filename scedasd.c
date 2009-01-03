/* SCEDASD.C    (c) Copyright Jan Jaeger, 1999-2009                  */
/*              Service Control Element DASD I/O functions           */

// $Id$

// $Log$
// Revision 1.5  2009/01/02 23:03:08  rbowler
// scedasd.c(221) : error C2065 : 'S_IROTH' : undeclared identifier
//
// Revision 1.4  2009/01/02 22:44:06  rbowler
// scedasd.c(115) : warning C4013: 'assert' undefined
//
// Revision 1.3  2009/01/02 19:21:52  jj
// DVD-RAM IPL
// RAMSAVE
// SYSG Integrated 3270 console fixes
//
// Revision 1.2  2008/12/29 16:13:37  jj
// Make sce_base_dir externally available
//
// Revision 1.1  2008/12/29 11:03:11  jj
// Move HMC disk I/O functions to scedasd.c
//

#include "hstdinc.h"

#define _HENGINE_DLL_

#include "hercules.h"
#include "opcode.h"

#if !defined(_SCEDASD_C)
#define _SCEDASD_C

static TID     scedio_tid;             /* Thread id of the i/o driver
                                                                     */
static char sce_base_dir[1024];


static char *set_base_dir(char *path)
{
char *base_dir;

    strlcpy(sce_base_dir,path,sizeof(sce_base_dir));

    if((base_dir = strrchr(sce_base_dir,'/')))
    {
        *(++base_dir) = '\0';
        return strrchr(path,'/') + 1;
    }
    else
    {
        *sce_base_dir = '\0';
        return path;
    }
}


#endif /* !defined(_SCEDASD_C) */


/*-------------------------------------------------------------------*/
/* function load_hmc simulates the load from the service processor   */
/*   the filename pointed to is a descriptor file which has the      */
/*   following format:                                               */
/*                                                                   */
/*   '*' in col 1 is comment                                         */
/*   core image file followed by address where it should be loaded   */
/*                                                                   */
/* For example:                                                      */
/*                                                                   */
/* * Linux/390 cdrom boot image                                      */
/* boot_images/tapeipl.ikr 0x00000000                                */
/* boot_images/initrd 0x00800000                                     */
/* boot_images/parmfile 0x00010480                                   */
/*                                                                   */
/* The location of the image files is relative to the location of    */
/* the descriptor file.                         Jan Jaeger 10-11-01  */
/*                                                                   */
/*-------------------------------------------------------------------*/
int ARCH_DEP(load_hmc) (char *fname, int cpu, int clear)
{
REGS   *regs;                           /* -> Regs                   */
FILE   *fp;
char    inputline[1024];
char    filename[1024];                 /* filename of image file    */
char    pathname[1024];                 /* pathname of image file    */
U32     fileaddr;
int     rc, rx;                         /* Return codes (work)       */

    /* Get started */
    if (ARCH_DEP(common_load_begin) (cpu, clear) != 0)
        return -1;

    /* The actual IPL proper starts here... */

    regs = sysblk.regs[cpu];    /* Point to IPL CPU's registers */

    if(fname == NULL)                   /* Default ipl from DASD     */
        fname = "HERCULES.ins";         /*   from HERCULES.ins       */

    hostpath(pathname, fname, sizeof(filename));

    fname = set_base_dir(pathname);

    /* reconstruct full name */
    strlcpy(filename,sce_base_dir,sizeof(filename));
    strlcat(filename,fname,sizeof(filename));

    hostpath(pathname, filename, sizeof(filename));
    fp = fopen(pathname, "r");
    if(fp == NULL)
    {
        logmsg(_("HHCCP031E Load from %s failed: %s\n"),fname,strerror(errno));
        return -1;
    }

    do
    {
        rc = fgets(inputline,sizeof(inputline),fp) != NULL;
        ASSERT(sizeof(pathname) == 1024);
        rx = sscanf(inputline,"%1024s %i",pathname,&fileaddr);
        hostpath(filename, pathname, sizeof(filename));

        /* If no load address was found load to location zero */
        if(rc && rx < 2)
            fileaddr = 0;

        if(rc && rx > 0 && *filename != '*' && *filename != '#')
        {
            /* Prepend the directory name if one was found
               and if no full pathname was specified */
            if(
#ifndef WIN32
                filename[0] != '/'
#else // WIN32
                filename[1] != ':'
#endif // !WIN32
            )
            {
                strlcpy(pathname,sce_base_dir,sizeof(pathname));
                strlcat(pathname,filename,sizeof(pathname));
            }
            else
                strlcpy(pathname,filename,sizeof(pathname));

            if( ARCH_DEP(load_main) (pathname, fileaddr) < 0 )
            {
                fclose(fp);
                HDC1(debug_cpu_state, regs);
                return -1;
            }
            sysblk.main_clear = sysblk.xpnd_clear = 0;
        }
    } while(rc);
    fclose(fp);

    /* Finish up... */
    return ARCH_DEP(common_load_finish) (regs);

} /* end function load_hmc */


/*-------------------------------------------------------------------*/
/* Function to Load (read) specified file into absolute main storage */
/*-------------------------------------------------------------------*/
int ARCH_DEP(load_main) (char *fname, RADR startloc)
{
int fd;
int len;
int rc = 0;
RADR pageaddr;
U32  pagesize;
char pathname[MAX_PATH];

    hostpath(pathname, fname, sizeof(pathname));

    fd = open (pathname, O_RDONLY|O_BINARY);
    if (fd < 0)
    {
        logmsg(_("HHCCP033E load_main: %s: %s\n"), fname, strerror(errno));
        return fd;
    }

    pagesize = PAGEFRAME_PAGESIZE - (startloc & PAGEFRAME_BYTEMASK);
    pageaddr = startloc;

    for( ; ; ) {
        if (pageaddr >= sysblk.mainsize)
        {
            logmsg(_("HHCCP034W load_main: terminated at end of mainstor\n"));
            close(fd);
            return rc;
        }

        len = read(fd, sysblk.mainstor + pageaddr, pagesize);
        if (len > 0)
        {
            STORAGE_KEY(pageaddr, &sysblk) |= STORKEY_REF|STORKEY_CHANGE;
            rc += len;
        }

        if (len < (int)pagesize)
        {
            close(fd);
            return rc;
        }

        pageaddr += PAGEFRAME_PAGESIZE;
        pageaddr &= PAGEFRAME_PAGEMASK;
        pagesize  = PAGEFRAME_PAGESIZE;
    }

} /* end function load_main */


#if defined(FEATURE_SCEDIO)

/*-------------------------------------------------------------------*/
/* Function to write to a file on the service processor disk         */
/*-------------------------------------------------------------------*/
static S64 ARCH_DEP(write_file)(char *fname, int mode, CREG sto, S64 size)
{
int fd, nwrite;
U64 totwrite = 0;

    fd = open (fname, mode |O_WRONLY|O_BINARY,
#if defined(_MSVC_)
            S_IREAD|S_IWRITE);
#else
            S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
#endif
    if (fd < 0)
    {
        logmsg ("HHCRD001I %s open error: %s\n", fname, strerror(errno));
        return -1;
    }

#if defined(FEATURE_ESAME)
    sto &= ASCE_TO;
#else /*!defined(FEATURE_ESAME)*/
    sto &= STD_STO;
#endif /*!defined(FEATURE_ESAME)*/

    for( ; size > 0 ; sto += sizeof(sto))
    {
#if defined(FEATURE_ESAME)
    DBLWRD *ste;
#else /*!defined(FEATURE_ESAME)*/
    FWORD *ste;
#endif /*!defined(FEATURE_ESAME)*/
    CREG pto, pti;

        /* Fetch segment table entry and calc Page Table Origin */
        if( sto >= sysblk.mainsize)
            goto eof;
#if defined(FEATURE_ESAME)
        ste = (DBLWRD*)(sysblk.mainstor + sto);
#else /*!defined(FEATURE_ESAME)*/
        ste = (FWORD*)(sysblk.mainstor + sto);
#endif /*!defined(FEATURE_ESAME)*/
        FETCH_W(pto, ste);
        STORAGE_KEY(sto, &sysblk) |= (STORKEY_REF);
        if( pto & SEGTAB_INVALID )
            goto eof;
#if defined(FEATURE_ESAME)
        pto &= ZSEGTAB_PTO;
#else /*!defined(FEATURE_ESAME)*/
        pto &= SEGTAB_PTO;
#endif /*!defined(FEATURE_ESAME)*/

        for(pti = 0; pti < 256 && size > 0; pti++, pto += sizeof(pto))
        {
#if defined(FEATURE_ESAME)
        DBLWRD *pte;
#else /*!defined(FEATURE_ESAME)*/
        FWORD *pte;
#endif /*!defined(FEATURE_ESAME)*/
        CREG pgo;
        BYTE *page;

            /* Fetch Page Table Entry to get page origin */
            if( pto >= sysblk.mainsize)
                goto eof;

#if defined(FEATURE_ESAME)
            pte = (DBLWRD*)(sysblk.mainstor + pto);
#else /*!defined(FEATURE_ESAME)*/
            pte = (FWORD*)(sysblk.mainstor + pto);
#endif /*!defined(FEATURE_ESAME)*/
            FETCH_W(pgo, pte);
            STORAGE_KEY(pto, &sysblk) |= (STORKEY_REF);
            if( !(pgo & PAGETAB_INVALID) )
            {
#if defined(FEATURE_ESAME)
                pgo &= ZPGETAB_PFRA;
#else /*!defined(FEATURE_ESAME)*/
                pgo &= PAGETAB_PFRA;
#endif /*!defined(FEATURE_ESAME)*/

                /* Write page to SCE disk */
                if( pgo >= sysblk.mainsize)
                    goto eof;

                page = sysblk.mainstor + pgo;
                nwrite = write(fd, page, STORAGE_KEY_PAGESIZE);
                totwrite += nwrite;
                if( nwrite != STORAGE_KEY_PAGESIZE )
                    goto eof;

                STORAGE_KEY(pgo, &sysblk) |= (STORKEY_REF);
            }
            size -= STORAGE_KEY_PAGESIZE;
        }
    }
eof:
    close(fd);
    return totwrite;
}


/*-------------------------------------------------------------------*/
/* Function to read from a file on the service processor disk        */
/*-------------------------------------------------------------------*/
static S64 ARCH_DEP(read_file)(char *fname, CREG sto, S64 seek, S64 size)
{
int fd, nread;
U64 totread = 0;

    fd = open (fname, O_RDONLY|O_BINARY);
    if (fd < 0)
    {
        if(errno != ENOENT)
            logmsg ("HHCRD002I %s open error: %s\n", fname, strerror(errno));
        return -1;
    }

    if(lseek(fd, (off_t)seek, SEEK_SET) == (off_t)seek)
    {
#if defined(FEATURE_ESAME)
        sto &= ASCE_TO;
#else /*!defined(FEATURE_ESAME)*/
        sto &= STD_STO;
#endif /*!defined(FEATURE_ESAME)*/

        for( ; size > 0 ; sto += sizeof(sto))
        {
#if defined(FEATURE_ESAME)
        DBLWRD *ste;
#else /*!defined(FEATURE_ESAME)*/
        FWORD *ste;
#endif /*!defined(FEATURE_ESAME)*/
        CREG pto, pti;

            /* Fetch segment table entry and calc Page Table Origin */
            if( sto >= sysblk.mainsize)
                goto eof;
#if defined(FEATURE_ESAME)
            ste = (DBLWRD*)(sysblk.mainstor + sto);
#else /*!defined(FEATURE_ESAME)*/
            ste = (FWORD*)(sysblk.mainstor + sto);
#endif /*!defined(FEATURE_ESAME)*/
            FETCH_W(pto, ste);
            STORAGE_KEY(sto, &sysblk) |= (STORKEY_REF);
            if( pto & SEGTAB_INVALID )
                goto eof;
#if defined(FEATURE_ESAME)
            pto &= ZSEGTAB_PTO;
#else /*!defined(FEATURE_ESAME)*/
            pto &= SEGTAB_PTO;
#endif /*!defined(FEATURE_ESAME)*/

            for(pti = 0; pti < 256 && size > 0; pti++, pto += sizeof(pto))
            {
#if defined(FEATURE_ESAME)
            DBLWRD *pte;
#else /*!defined(FEATURE_ESAME)*/
            FWORD *pte;
#endif /*!defined(FEATURE_ESAME)*/
            CREG pgo;
            BYTE *page;

                /* Fetch Page Table Entry to get page origin */
                if( pto >= sysblk.mainsize)
                    goto eof;
#if defined(FEATURE_ESAME)
                pte = (DBLWRD*)(sysblk.mainstor + pto);
#else /*!defined(FEATURE_ESAME)*/
                pte = (FWORD*)(sysblk.mainstor + pto);
#endif /*!defined(FEATURE_ESAME)*/
                FETCH_W(pgo, pte);
                STORAGE_KEY(pto, &sysblk) |= (STORKEY_REF);
                if( pgo & PAGETAB_INVALID )
                    goto eof;
#if defined(FEATURE_ESAME)
                pgo &= ZPGETAB_PFRA;
#else /*!defined(FEATURE_ESAME)*/
                pgo &= PAGETAB_PFRA;
#endif /*!defined(FEATURE_ESAME)*/

                /* Read page into main storage */
                if( pgo >= sysblk.mainsize)
                    goto eof;
                page = sysblk.mainstor + pgo;
                nread = read(fd, page, STORAGE_KEY_PAGESIZE);
                totread += nread;
                size -= nread;
                if( nread != STORAGE_KEY_PAGESIZE )
                    goto eof;
                STORAGE_KEY(pgo, &sysblk) |= (STORKEY_REF|STORKEY_CHANGE);
            }
        }
    }
eof:
    close(fd);
    return totread;
}


/*-------------------------------------------------------------------*/
/* Thread to perform service processor I/O functions                 */
/*-------------------------------------------------------------------*/
static void ARCH_DEP(scedio_thread)(SCCB_SCEDIO_BK *scedio_bk)
{
S64     seek;
S64     length;
S64     totread, totwrite;
U64     sto;
char    fname[1024];

    switch(scedio_bk->type) {

    case SCCB_SCEDIO_TYPE_INIT:
        scedio_bk->flag3 |= SCCB_SCEDIO_FLG3_COMPLETE;
        break;


    case SCCB_SCEDIO_TYPE_READ:
        strlcpy(fname,sce_base_dir,sizeof(fname));
        strlcat(fname,(char *)scedio_bk->filename,sizeof(fname));
        FETCH_DW(sto,scedio_bk->sto);
        FETCH_DW(seek,scedio_bk->seek);
        FETCH_DW(length,scedio_bk->length);

        totread = ARCH_DEP(read_file)(fname, sto, seek, length);

        if(totread > 0)
        {
            STORE_DW(scedio_bk->length,totread);

            if(totread == length)
                STORE_DW(scedio_bk->ncomp,0);
            else
                STORE_DW(scedio_bk->ncomp,seek+totread);

            scedio_bk->flag3 |= SCCB_SCEDIO_FLG3_COMPLETE;
        }
        else
            scedio_bk->flag3 &= ~SCCB_SCEDIO_FLG3_COMPLETE;

        break;


    case SCCB_SCEDIO_TYPE_CREATE:
    case SCCB_SCEDIO_TYPE_APPEND:
        strlcpy(fname,sce_base_dir,sizeof(fname));
        strlcat(fname,(char *)scedio_bk->filename,sizeof(fname));
        FETCH_DW(sto,scedio_bk->sto);
        FETCH_DW(seek,scedio_bk->seek);
        FETCH_DW(length,scedio_bk->length);

        totwrite = ARCH_DEP(write_file)(fname,
          ((scedio_bk->type == SCCB_SCEDIO_TYPE_CREATE) ? O_CREAT : O_APPEND), sto, length);

        STORE_DW(scedio_bk->ncomp,totwrite);
        scedio_bk->flag3 |= SCCB_SCEDIO_FLG3_COMPLETE;
        break;

    default:
        scedio_bk->flag3 &= ~SCCB_SCEDIO_FLG3_COMPLETE;
    }

    OBTAIN_INTLOCK(NULL);

    // The VM boys appear to have made an error in not
    // allowing for asyncronous attentions to be merged
    // with pending interrupts as such we will wait here
    // until a pending interrupt has been cleared. *JJ
    while(IS_IC_SERVSIG)
    {
        RELEASE_INTLOCK(NULL);
        sched_yield();
        OBTAIN_INTLOCK(NULL);
    }

    sclp_attention(SCCB_EVD_TYPE_SCEDIO);

    scedio_tid = 0;

    RELEASE_INTLOCK(NULL);
}


/*-------------------------------------------------------------------*/
/* Function to interface with the service processor I/O thread       */
/*-------------------------------------------------------------------*/
static int ARCH_DEP(scedio_request)(U32 sclp_command, SCCB_SCEDIO_BK *scedio_bk)
{
static SCCB_SCEDIO_BK static_scedio_bk;
static int scedio_pending;

    if(sclp_command == SCLP_READ_EVENT_DATA)
    {
    int pending_req = scedio_pending;

        /* Return no data if the scedio thread is still active */
        if(scedio_tid)
            return 0;

        /* Update the scedio_bk copy in the SCCB */
        if(scedio_pending)
            *scedio_bk = static_scedio_bk;

        /* Reset the pending flag */
        scedio_pending = 0;

        /* Return true if a request was pending */
        return pending_req;

    }
    else
    {
#if !defined(NO_SIGABEND_HANDLER)
        switch(scedio_bk->type) {

            case SCCB_SCEDIO_TYPE_INIT:
                /* Kill the scedio thread if it is active */
                if( scedio_tid )
                {
                    OBTAIN_INTLOCK(NULL);
                    signal_thread(scedio_tid, SIGKILL);
                    scedio_tid = 0;
                    scedio_pending = 0;
                    RELEASE_INTLOCK(NULL);
                }
                break;
        }
#endif

        /* Take a copy of the scedio_bk in the SCCB */
        static_scedio_bk = *scedio_bk;

        /* Create the scedio thread */
        if( create_thread(&scedio_tid, &sysblk.detattr,
            ARCH_DEP(scedio_thread), &static_scedio_bk, "scedio_thread") )
            return -1;

        scedio_pending = 1;

    }

    return 0;
}


/*-------------------------------------------------------------------*/
/* Function to request service processor I/O                         */
/*-------------------------------------------------------------------*/
void ARCH_DEP(sclp_scedio_request) (SCCB_HEADER *sccb)
{
SCCB_EVD_HDR    *evd_hdr = (SCCB_EVD_HDR*)(sccb + 1);
SCCB_SCEDIO_BK  *scedio_bk = (SCCB_SCEDIO_BK*)(evd_hdr + 1);

    if( ARCH_DEP(scedio_request)(SCLP_WRITE_EVENT_DATA, scedio_bk) )
    {
        /* Set response code X'0040' in SCCB header */
        sccb->reas = SCCB_REAS_NONE;
        sccb->resp = SCCB_RESP_BACKOUT;
    }
    else
    {
        /* Set response code X'0020' in SCCB header */
        sccb->reas = SCCB_REAS_NONE;
        sccb->resp = SCCB_RESP_COMPLETE;
    }

    /* Indicate Event Processed */
    evd_hdr->flag |= SCCB_EVD_FLAG_PROC;

}


/*-------------------------------------------------------------------*/
/* Function to read service processor I/O event data                 */
/*-------------------------------------------------------------------*/
void ARCH_DEP(sclp_scedio_event) (SCCB_HEADER *sccb)
{
SCCB_EVD_HDR    *evd_hdr = (SCCB_EVD_HDR*)(sccb + 1);
SCCB_SCEDIO_BK  *scedio_bk = (SCCB_SCEDIO_BK*)(evd_hdr+1);
U32 sccb_len;
U32 evd_len;

    if( ARCH_DEP(scedio_request)(SCLP_READ_EVENT_DATA, scedio_bk) )
    {
        /* Zero all fields */
        memset (evd_hdr, 0, sizeof(SCCB_EVD_HDR));

        /* Set length in event header */
        evd_len = sizeof(SCCB_EVD_HDR) + sizeof(SCCB_SCEDIO_BK);
        STORE_HW(evd_hdr->totlen, evd_len);

        /* Set type in event header */
        evd_hdr->type = SCCB_EVD_TYPE_SCEDIO;

        /* Update SCCB length field if variable request */
        if (sccb->type & SCCB_TYPE_VARIABLE)
        {
            /* Set new SCCB length */
            sccb_len = evd_len + sizeof(SCCB_HEADER);
            STORE_HW(sccb->length, sccb_len);
            sccb->type &= ~SCCB_TYPE_VARIABLE;
        }

        /* Set response code X'0020' in SCCB header */
        sccb->reas = SCCB_REAS_NONE;
        sccb->resp = SCCB_RESP_COMPLETE;
    }

}

#endif /*defined(FEATURE_SCEDIO)*/


#if !defined(_GEN_ARCH)

#if defined(_ARCHMODE2)
 #define  _GEN_ARCH _ARCHMODE2
 #include "scedasd.c"
#endif

#if defined(_ARCHMODE3)
 #undef   _GEN_ARCH
 #define  _GEN_ARCH _ARCHMODE3
 #include "scedasd.c"
#endif


/*-------------------------------------------------------------------*/
/*  Service Processor Load    (load/ipl from the specified file)     */
/*-------------------------------------------------------------------*/

int load_hmc (char *fname, int cpu, int clear)
{
    switch(sysblk.arch_mode) {
#if defined(_370)
        case ARCH_370:
            return s370_load_hmc (fname, cpu, clear);
#endif
#if defined(_390)
        case ARCH_390:
            return s390_load_hmc (fname, cpu, clear);
#endif
#if defined(_900)
        case ARCH_900:
            /* z/Arch always starts out in ESA390 mode */
            return s390_load_hmc (fname, cpu, clear);
#endif
    }
    return -1;
}


/*-------------------------------------------------------------------*/
/* Load/Read specified file into absolute main storage               */
/*-------------------------------------------------------------------*/
int load_main (char *fname, RADR startloc)
{
    switch(sysblk.arch_mode) {
#if defined(_370)
        case ARCH_370:
            return s370_load_main (fname, startloc);
#endif
#if defined(_390)
        case ARCH_390:
            return s390_load_main (fname, startloc);
#endif
#if defined(_900)
        case ARCH_900:
            return z900_load_main (fname, startloc);
#endif
    }
    return -1;
}

#endif /*!defined(_GEN_ARCH)*/

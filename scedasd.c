/* SCEDASD.C    (c) Copyright Jan Jaeger, 1999-2008                  */
/*              Service Control Element DASD I/O functions           */

// $Id$

// $Log$

#include "hstdinc.h"

#define _HENGINE_DLL_

#include "hercules.h"
#include "opcode.h"
#include "inline.h"
#include <assert.h>
#if defined(OPTION_FISHIO)
#include "w32chan.h"
#endif // defined(OPTION_FISHIO)

#if !defined(_SCEDASD_C)
#define _SCEDASD_C


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
char    dirname[1024];                  /* dirname of ins file       */
char   *dirbase;
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

    /* remove filename from pathname */
    hostpath(pathname, fname, sizeof(filename));
    strlcpy(dirname,pathname,sizeof(dirname));
    dirbase = strrchr(dirname,'/');
    if(dirbase) *(++dirbase) = '\0';

    fp = fopen(pathname, "r");
    if(fp == NULL)
    {
        logmsg(_("HHCCP031E Load from %s failed: %s\n"),fname,strerror(errno));
        return -1;
    }

    do
    {
        rc = fgets(inputline,sizeof(inputline),fp) != NULL;
        assert(sizeof(pathname) == 1024);
        rx = sscanf(inputline,"%1024s %i",pathname,&fileaddr);
        hostpath(filename, pathname, sizeof(filename));

        /* If no load address was found load to location zero */
        if(rc && rx < 2)
            fileaddr = 0;

        if(rc && rx > 0 && *filename != '*' && *filename != '#')
        {
            /* Prepend the directory name if one was found
               and if no full pathname was specified */
            if(dirbase &&
#ifndef WIN32
                filename[0] != '/'
#else // WIN32
                filename[1] != ':'
#endif // !WIN32
            )
            {
                strlcpy(pathname,dirname,sizeof(pathname));
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

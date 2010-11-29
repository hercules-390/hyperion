/* DYN76   (c) Copyright Jason Paul Winter, 2010                      */
/*             See license_dyn76.txt for licensing                    */
/* DYN76.C (c) Copyright Harold Grovesteen, 2010                      */ 
/*   Released under "The Q Public License Version 1"                  */
/*   (http://www.hercules-390.org/herclic.html) as modifications to   */
/*   Hercules.                                                        */

// $Id$

/*
*** For *nix type systems: (be warned: this information is quite old!)
*** compile by: (Update "i686" to "i586" if required.) ***

gcc -DHAVE_CONFIG_H -I. -fomit-frame-pointer -O3 -march=i686 -W -Wall -shared -export-dynamic -o dyn76.dll dyn76.c .libs/libherc.dll.a .libs/libhercs.dll.a

  *** Or ***
  
gcc -DHAVE_CONFIG_H -I. -fomit-frame-pointer -O3 -march=i686 -W -Wall -shared -export-dynamic -o dyn76.dll dyn76.c hercules.a

libherc*.dll.a - is windows only, it's not required on unix.
It provides back-link support under cygwin.

If hercules is built without libtool, then hercules.a should
be used to include backlink support.

You can then issue ldmod dyninst dyn76 from the hercules panel,
or add the same statement to the hercules.cnf file.
*/

#include "hstdinc.h"
 
#if !defined(_HENGINE_DLL_)
#define _HENGINE_DLL_
#endif /*_HENGINE_DLL_*/

#if !defined(_DYN76_C)
#define _DYN76_C
#endif /* !defined(_DYN76_C) */

#include "hercules.h"
#include "opcode.h"
#include "inline.h"
#include "hdiagf18.h"

#if !defined(_DYN76_H)
#define _DYN76_H
/*-------------------------------------------------------------------*/
/* Internal macro definitions                                        */
/*-------------------------------------------------------------------*/

/* These macros allow retrieving and setting of register contents from/to
   the Compatibility Mode Parameter Block, which is in essence a R0-R15
   register save area.  These macros ensure storage keys and accesses are
   checked for the parameter block.
*/
#define set_reg(_r, _v) \
    ARCH_DEP(vstore4)(_v ,cmpb+(_r * 4), USE_REAL_ADDR, regs)
#define get_reg(_v, _r) \
    _v = ARCH_DEP(vfetch4)(cmpb+(_r * 4), USE_REAL_ADDR, regs)

#if 0
 #if defined(WIN32) && !defined(HDL_USE_LIBTOOL)
/* We need to do some special tricks for cygwin here, since cygwin   */
/* does not support backlink and we need to resolve symbols during   */
/* dll initialisation (REGISTER/RESOLVER). Opcode tables are renamed */
/* such that no naming conflicts occur.                              */
  #define opcode_table opcode_table_r
  #define opcode_01xx  opcode_01xx_r
  #define opcode_a5xx  opcode_a5xx_r
  #define opcode_a4xx  opcode_a4xx_r
  #define opcode_a7xx  opcode_a1xx_r
  #define opcode_b2xx  opcode_b2xx_r
  #define opcode_b3xx  opcode_b3xx_r
  #define opcode_b9xx  opcode_b9xx_r
  #define opcode_c0xx  opcode_c0xx_r
  #define opcode_e3xx  opcode_e3xx_r
  #define opcode_e5xx  opcode_e5xx_r
  #define opcode_e6xx  opcode_e6xx_r
  #define opcode_ebxx  opcode_ebxx_r
  #define opcode_ecxx  opcode_ecxx_r
  #define opcode_edxx  opcode_edxx_r
  #define s370_opcode_table s370_opcode_table_r
  #define s390_opcode_table s390_opcode_table_r
  #define z900_opcode_table z900_opcode_table_r
 #endif /* defined(WIN32) && !defined(HDL_USE_LIBTOOL) */
 
 #if defined(WIN32) && !defined(HDL_USE_LIBTOOL)
  #undef opcode_table
  #undef opcode_01xx
  #undef opcode_a5xx
  #undef opcode_a4xx
  #undef opcode_a7xx
  #undef opcode_b2xx
  #undef opcode_b3xx
  #undef opcode_b9xx
  #undef opcode_c0xx
  #undef opcode_e3xx
  #undef opcode_e5xx
  #undef opcode_e6xx
  #undef opcode_ebxx
  #undef opcode_ecxx
  #undef opcode_edxx
  #undef s370_opcode_table
  #undef s390_opcode_table
  #undef z900_opcode_table
 #endif /* defined(WIN32) && !defined(HDL_USE_LIBTOOL) */
#endif

/* Keep track of open files, a utility can then be used to close them if needed (fn3) */

//typedef struct fkeeper_tag * fkeeper_ptr;
struct fkeeper 
{
    struct fkeeper *next; /* May or may not end up in the global list */
    U32  SaveArea;        /* Space to save a work register */
    U32  id;              /* Used to identify this fkeeper to the guest */
	int  handle;          /* linked list handle for clean-ups */
    int  mode;            /* Text/Binary-Mode */
    int  data;            /* filenames index and for read/write */
	char oldname  [260];
	char filename [260];  /* Also used for 256 byte read/write buffer */
};

//static fkeeper_ptr fkpr_head = NULL;
static struct fkeeper *fkpr_head = NULL;

static unsigned char DCCascii_to_ebcdic[] = {
    "\x00\x01\x02\x03\x37\x2D\x2E\x2F\x16\x05\x15\x0B\x0C\x0D\x0E\x0F"
    "\x10\x11\x12\x13\x3C\x3D\x32\x26\x18\x19\x3F\x27\x1C\x1D\x1E\x1F"
    "\x40\x5A\x7F\x7B\x5B\x6C\x50\x7D\x4D\x5D\x5C\x4E\x6B\x60\x4B\x61"
    "\xF0\xF1\xF2\xF3\xF4\xF5\xF6\xF7\xF8\xF9\x7A\x5E\x4C\x7E\x6E\x6F"
    "\x7C\xC1\xC2\xC3\xC4\xC5\xC6\xC7\xC8\xC9\xD1\xD2\xD3\xD4\xD5\xD6"
    "\xD7\xD8\xD9\xE2\xE3\xE4\xE5\xE6\xE7\xE8\xE9\xAD\xE0\xBD\x5F\x6D"
    "\x79\x81\x82\x83\x84\x85\x86\x87\x88\x89\x91\x92\x93\x94\x95\x96"
    "\x97\x98\x99\xA2\xA3\xA4\xA5\xA6\xA7\xA8\xA9\xC0\x4F\xD0\xA1\x07"
    "\x20\x21\x22\x23\x24\x25\x06\x17\x28\x29\x2A\x2B\x2C\x09\x0A\x1B"
    "\x30\x31\x1A\x33\x34\x35\x36\x08\x38\x39\x3A\x3B\x04\x14\x3E\xFF"
    "\x41\xAA\x4A\xB1\x9F\xB2\x6A\xB5\xBB\xB4\x9A\x8A\xB0\xCA\xAF\xBC"
    "\x90\x8F\xEA\xFA\xBE\xA0\xB6\xB3\x9D\xDA\x9B\x8B\xB7\xB8\xB9\xAB"
    "\x64\x65\x62\x66\x63\x67\x9E\x68\x74\x71\x72\x73\x78\x75\x76\x77"
    "\xAC\x69\xED\xEE\xEB\xEF\xEC\xBF\x80\xFD\xFE\xFB\xFC\xBA\xAE\x59"
    "\x44\x45\x42\x46\x43\x47\x9C\x48\x54\x51\x52\x53\x58\x55\x56\x57"
    "\x8C\x49\xCD\xCE\xCB\xCF\xCC\xE1\x70\xDD\xDE\xDB\xDC\x8D\x8E\xDF"
};

static unsigned char DCCebcdic_to_ascii[] = {
    "\x00\x01\x02\x03\x9C\x09\x86\x7F\x97\x8D\x8E\x0B\x0C\x0D\x0E\x0F"
    "\x10\x11\x12\x13\x9D\x0A\x08\x87\x18\x19\x92\x8F\x1C\x1D\x1E\x1F"
    "\x80\x81\x82\x83\x84\x85\x17\x1B\x88\x89\x8A\x8B\x8C\x05\x06\x07"
    "\x90\x91\x16\x93\x94\x95\x96\x04\x98\x99\x9A\x9B\x14\x15\x9E\x1A"
    "\x20\xA0\xE2\xE4\xE0\xE1\xE3\xE5\xE7\xF1\xA2\x2E\x3C\x28\x2B\x7C"
    "\x26\xE9\xEA\xEB\xE8\xED\xEE\xEF\xEC\xDF\x21\x24\x2A\x29\x3B\x5E"
    "\x2D\x2F\xC2\xC4\xC0\xC1\xC3\xC5\xC7\xD1\xA6\x2C\x25\x5F\x3E\x3F"
    "\xF8\xC9\xCA\xCB\xC8\xCD\xCE\xCF\xCC\x60\x3A\x23\x40\x27\x3D\x22"
    "\xD8\x61\x62\x63\x64\x65\x66\x67\x68\x69\xAB\xBB\xF0\xFD\xFE\xB1"
    "\xB0\x6A\x6B\x6C\x6D\x6E\x6F\x70\x71\x72\xAA\xBA\xE6\xB8\xC6\xA4"
    "\xB5\x7E\x73\x74\x75\x76\x77\x78\x79\x7A\xA1\xBF\xD0\x5B\xDE\xAE"
    "\xAC\xA3\xA5\xB7\xA9\xA7\xB6\xBC\xBD\xBE\xDD\xA8\xAF\x5D\xB4\xD7"
    "\x7B\x41\x42\x43\x44\x45\x46\x47\x48\x49\xAD\xF4\xF6\xF2\xF3\xF5"
    "\x7D\x4A\x4B\x4C\x4D\x4E\x4F\x50\x51\x52\xB9\xFB\xFC\xF9\xFA\xFF"
    "\x5C\xF7\x53\x54\x55\x56\x57\x58\x59\x5A\xB2\xD4\xD6\xD2\xD3\xD5"
    "\x30\x31\x32\x33\x34\x35\x36\x37\x38\x39\xB3\xDB\xDC\xD9\xDA\x9F"
};

//static void StrConverter (unsigned char * s, unsigned char * cct)
static void StrConverter (char * s, unsigned char * cct) 
{
    int i = 0;
    while (s [i]) 
    {
        s [i] = (signed char)cct [(unsigned char)s [i]];
        i++;
    }
}

static void MemConverter (char * s, unsigned char * cct, int len) 
{
    int i = 0;
    while (i < len) 
    {
        s [i] = (signed char)cct [(unsigned char)s [i]];
        i++;
    }
}

#ifdef _MSVC_

/* When using MSVC, just use the normal critical section functions: */

#undef initialize_lock
#undef LOCK
#undef obtain_lock
#undef release_lock

#define initialize_lock InitializeCriticalSection
#define LOCK CRITICAL_SECTION
#define obtain_lock EnterCriticalSection
#define release_lock LeaveCriticalSection

#endif

static LOCK nfile_lock;
static long nfile_init_req = 1;
static U32 nfile_id = 0;

static void nfile_init () 
{
    initialize_lock(&nfile_lock);
}
#define dolock(l) obtain_lock (&l); /* This ensures we use the correct fn */
#define unlock(l) release_lock (&l);

#ifdef _MSVC_
#include <io.h>
#else
#include <fcntl.h>
#include <unistd.h>

#define _open open
#define _read read
#define _write write
#define _lseek lseek
#define _close close

#define _O_TEXT   0x04
#define _O_BINARY 0x08

#define _O_WRONLY O_WRONLY
#define _O_RDWR   O_RDWR
#define _O_RDONLY O_RDONLY
#define _O_APPEND O_APPEND
#define _O_TRUNC  O_TRUNC
#define _O_CREAT  O_CREAT
#define _O_EXCL   O_EXCL
#endif

static int GetMode (int h) 
{
    int mode = 0;
    struct fkeeper *fk;
    //fkeeper_ptr fk;

    dolock (nfile_lock); /* Take ownership of the list */
    fk = fkpr_head; /* Search the list */
    while (fk) {
        if (fk->handle == h) { /* Found the entry? */
            mode = fk->mode;
            break;
        }
        fk = fk->next;
    }
    unlock (nfile_lock); /* Release ownership of the list */
    return (mode);
}

static int SetMode (int h, int m) 
{
    int mode = 0;
    struct fkeeper *fk;

    dolock (nfile_lock); /* Take ownership of the list */
    fk = fkpr_head; /* Search the list */
    while (fk) {
        if (fk->handle == h) 
        { /* Found the entry? */
            mode = fk->mode;
            fk->mode = m;
            break;
        }
        fk = fk->next;
    }
    unlock (nfile_lock); /* Release ownership of the list */
    return (mode);
}

static struct fkeeper * FindFK (U32 id)
{
    struct fkeeper *fk;

    dolock (nfile_lock); /* Take ownership of the list */
    fk = fkpr_head; /* Search the list */
    while (fk) 
    {
        if (fk->id == id) 
        {   /* Found the entry */
            return fk;
        }
        fk = fk->next;
    }
    unlock (nfile_lock); /* Release ownership of the list */
    /* Reached the end of the list without finding the id */
    return 0;
}


static void RemoveFK (int h) 
{
    struct fkeeper *pfk;
    struct fkeeper *fk;

    dolock (nfile_lock); /* Take ownership of the list */
    pfk = NULL;
    fk = fkpr_head; /* Search the list */
    while (fk) {
        if (fk->handle == h) { /* Found the entry? */
            if (pfk) /* Wasn't the head entry */
                pfk->next = fk->next;
            else     /* Was the head entry */
                fkpr_head = fk->next;
            free (fk);
            break;
        }
        pfk = fk;
        fk = fk->next;
    }
    unlock (nfile_lock); /* Release ownership of the list */
}

static void RemoveFKByID (U32 id) 
{
    struct fkeeper *pfk;
    struct fkeeper *fk;

    dolock (nfile_lock); /* Take ownership of the list */
    pfk = NULL;
    fk = fkpr_head; /* Search the list */
    while (fk) 
    {
        if (fk->id == id) 
        {   /* Found the entry? */
            if (pfk) /* Wasn't the head entry */
                pfk->next = fk->next;
            else     /* Was the head entry */
                fkpr_head = fk->next;
            free (fk);
            break;
        }
        pfk = fk;
        fk = fk->next;
    }
    unlock (nfile_lock); /* Release ownership of the list */
}

static int RemoveFKByName (char * filename) {
    int i = -1 * EBADF;
    struct fkeeper *pfk;
    struct fkeeper *fk;

    dolock (nfile_lock); /* Take ownership of the list */
    pfk = NULL;
    fk = fkpr_head; /* Search the list */
    while (fk) {
        if (strcmp (fk->filename, filename) == 0) { /* Found the entry? */
            i = _close (fk->handle); /* Additional step here is to close the file too */
            if (i == -1)
                i = -errno;
            else {
                if (pfk) /* Wasn't the head entry */
                    pfk->next = fk->next;
                else     /* Was the head entry */
                    fkpr_head = fk->next;
                free (fk);
            }
            break;
        }
        pfk = fk;
        fk = fk->next;
    }
    unlock (nfile_lock); /* Release ownership of the list */
    return (i);
}

#endif /* !defined(_DYN76_H) */
/*-------------------------------------------------------------------*/
/* 76xx NFL  - NFILE Ra,yyy(Rb,Rc)  Ra=0,yyy=0,Rb=0,Rc=R2            */
/*-------------------------------------------------------------------*/

/*
  R0  = Set to 0 prior to call, 
        but due to possible instruction restart, >=0 on return
  R1  = Function number:
		0/ int rename (char * oldname - char * newname); 
		       -special 1 parm points to dual nul-term-strings
		1/ int unlink (char * filename);
        2/ int open   (char * filename, int oflg, int pmode);
        3/ int close  (char * filename); 
               -special version to allow abended pgm cleanups "by hand"
		4/ int read   (char * buf, int h, int count);
		5/ int write  (char * buf, int h, int count);
		6/ int seek   (int h, int offset, int origin);
		7/ int commit (int h);
		8/ int close  (int h);
        9/ int setmode (int h, int mode);
  R2..R4 = Parameters, depending on function number  
           All may be destroyed on return
  R5  = Work structure pointer (not set prior to call and safely restored on return)
  R15 = Return Code (0 = ok/handle, +ve = handle, -ve = errno)
*/

// void ARCH_DEP(hdiagf18_FC) (U32 options, CMPB *cmpb, REGS * regs)
void ARCH_DEP(hdiagf18_FC) (U32 options, VADR cmpb, REGS *regs)
{
    int    space_ctl;   /* This is used to control address space selection */
    int    i;
    struct fkeeper *fk = NULL;
    /* Pseudo register contents are cached here once retrieved */
    U32     R0;
    U32     R1;
    U32     R2;
    U32     R3;
    U32     R4;
    U32     R5;
    U32     R15;

#if 0
DLL_EXPORT DEF_INST(dyninst_opcode_76) {
    int     r1;              /* Value of R field        */
    int     b2;              /* Base of effective addr  */
    VADR    effective_addr2; /* Effective address       */

    /*  vv---vv---------------- input variables to NFILE */
    RX(inst, regs, r1, b2, effective_addr2);
    /*                     ^^-- becomes yyy+gr[b]+gr[c] */
    /*                 ^^------ becomes access register c */
    /*             ^^---------- becomes to-store register a */
#endif

    /* Initialise the LOCK on our first use */
    if (nfile_init_req) 
    {
        nfile_init_req = 0;
        nfile_init ();
    }

    /* The following 4 functions are always ready to attempt 
       and they are not interruptible
    */
    
    /*------------------------*/
    /* SETMODE File Operation */
    /*------------------------*/

    get_reg(R1, 1);
    
    if (R1 > 9)
    {
        {   /* Invalid Operation - generate an exception */
            R0 = 0;
            set_reg(0,R0); /* don't restart this */
            ARCH_DEP(program_interrupt) (regs, PGM_OPERAND_EXCEPTION);
        }
    get_reg(R2, 2);
    if (R1 == 9) 
    { 
        get_reg(R3, 3);
        if (R3 & 0x04)
            i = _O_TEXT;
        else
            i = _O_BINARY;
#ifdef _MSVC_
        // get_reg(R2, 2);
        i = _setmode (R2, i); /* Alter the Windows mode */
        if (i == -1) 
        {
            // regs->GR_L(15) = -errno;
            R15 = -errno;
            set_reg(15,R15);
            return;
        }
#else
        get_reg(R3, 3);
        if (GetMode (R3)) /* *nix doesn't need handle updates */
            i = _O_TEXT;
        else
            i = _O_BINARY;
#endif
        if (i == _O_TEXT) /* Previous mode */
            R15 = 0x04;
        else
            R15 = 0x08;
        set_reg(15,R15);

        if (R3 & 0x04) /* Update translation details: */
            SetMode (R2, 1); /* yes, translate */
        else
            SetMode (R2, 0); /* no, dont */
        return;
    } else
     
    /*------------------------*/
    /* CLOSE File Operation   */
    /*------------------------*/
        
    if (R1 == 8) 
    {
        get_reg(R2,2);
        R15 = _close (R2);
        set_reg(15,R15);
        if (R15 == 0)
            RemoveFK (R2);
        else
        {
            // regs->GR_L(15) = -errno;
            R15 = -errno;
            set_reg(15,R15);
        }
        return;
    } else
        
    /*------------------------*/
    /* COMMIT File Operation  */
    /*------------------------*/        
        
    if (R1 == 7) 
    {
        get_reg(R2,2);
#ifdef _MSVC_
        // regs->GR_L(15) = _commit (regs->GR_L(2));
        R15 =  _commit (R2);
#else
        // regs->GR_L(15) = fsync (regs->GR_L(2));
        R15 = fsync (R2);
#endif
        if (R15 != 0)
            // regs->GR_L(15) = -errno;
            R15 = -errno;
        set_reg(15,R15);
        return;
    } else
        
    /*------------------------*/
    /* SEEK File Operation    */
    /*------------------------*/            
        
    if (R1 == 6)
    {
        // regs->GR_L(15) = _lseek (regs->GR_L(2), regs->GR_L(3), regs->GR_L(4));
        get_reg(R2,2);
        get_reg(R3,3);
        get_reg(R4,4);
        R15 = _lseek (R2, R3, R4);
        if (R15 == (U32)-1)
            // regs->GR_L(15) = -errno;
            R15 = -errno;
        set_reg(15,R15);
        return;
    }

    /*-------------------------------------------------------------*/
    /* Interruptible operation initilization                       */
    /*-------------------------------------------------------------*/
    
    if ( (options & SPACE_MASK) == DF18_REAL_SPACE)
        space_ctl = USE_REAL_ADDR;
    else
        space_ctl = USE_PRIMARY_SPACE;

    get_reg(R0,0);
    get_reg(R2,2);
    
    if (R0 == 0) 
    {   /* New operation, not a restart.  Establish new state */
        fk = malloc (sizeof (struct fkeeper));
        if (fk == NULL) 
        {
            //regs->GR_L(15) = -1 * ENOMEM; /* Error */
            R15 = -1 * ENOMEM; /* Error */
            set_reg(15,R15);
            return;
        }
        fk->mode = -1;  /* mode is not initially set */
        fk->data = 0;   /* Nothing in the buffer yet */
        //fk->SaveArea = regs->GR_L(5);
        get_reg(R5,5);
        fk->SaveArea = R5;
        
        /* Set this state's id for the guest and increment 
           and link the structure to the list even if not yet open.
           This is required to allow open to be restarted based upon the
           id.  If the file does not successfully open, then the linked entry
           must be removed.
        */
        // /* This does not work on 64-bit hosts and is replaced by nfile_id */
        // regs->GR_L(5) = (unsigned int)fk;
        
        /* Because the actual host pointer is no longer returned to the guest,
           the fkeeper structure must actually be linked to the list now, whereas
           in the past the structure hung unlinked in the host process address
           space and just freed if the file did not open.
        */
        dolock (nfile_lock); /* Take ownership of the list */
        R5 = nfile_id++;
        fk->id = R5;
        fk->next = fkpr_head;      /* Add to the list */
        fkpr_head = fk;
        unlock (nfile_lock); /* Release ownership of the list */
        set_reg(5,R5);
        
        // regs->GR_L(15) = 0; /* For read/write we need this cleared here */
        R15 = 0;
        set_reg(15,R15);
        
        /* Set the restart state in case the next stage is interrupted */
        // regs->GR_L(0) = 1;
        R0 = 1;                /* Set work stage 1 */
        set_reg(0,R0);
    } else
        
    /*-------------------------------------------------------------*/
    /* Interruptible operation restart                             */
    /*-------------------------------------------------------------*/
    
        /* Must have restarted, refresh fk */
        //fk = (struct fkeeper *)(regs->GR_L(5));
        get_reg(R5,5);    /* R5 contains the previous restart id */
        fk = FindFK(R5);
        if (!fk)
        {   /* Did not find the restart - generate an exception */
            R0 = 0;
            set_reg(0,R0); /* restart failed, so don't do it again */
            ARCH_DEP(program_interrupt) (regs, PGM_SPECIAL_OPERATION_EXCEPTION);
        }

    /*-------------------------------------------------------------*/
    /* Work Stage 1 Initial or Restart                             */
    /*-------------------------------------------------------------*/  
        
    if (R0 == 1) 
    {
        if (R1 == 0) 
        { /* rename - need to get 2 filenames */
            while ((fk->data == 0) ||                    /* Started? */
                   (fk->oldname [fk->data - 1] != 0)) 
            {   /* Not Finished */

                // ARCH_DEP(vfetchc) 
                //    (&(fk->oldname [fk->data]), 0, effective_addr2, b2, regs);

                ARCH_DEP(wfetchc) 
                    (&(fk->oldname [fk->data]), 
                     0, 
                     R2, 
                     space_ctl, 
                     regs);

                /* Exception, can recalculate if/when restart */
                // (regs->GR_L(b2)) += 1;
                R2 += 1;
                set_reg(2,R2);
                fk->data += 1;  /* Next host byte location */
                if (fk->data >= 259) 
                {
                    fk->oldname [fk->data] = 0;
                    break;
                }
            }
            StrConverter (fk->oldname, DCCebcdic_to_ascii);
            fk->data = 0; /* Get ready for newname */
        }
        // regs->GR_L(0) = 2;
        R0 = 2;               /* Set work stage 2 */
        set_reg(0,R0);
    }
    
    /*-------------------------------------------------------------*/
    /* Work Stage 2 Initial or Restart                             */
    /*-------------------------------------------------------------*/  
    
    if (R0 == 2) 
    {
        if (R1 <= 3) 
        {   /* unlink/open/close - need to get 1 filename */
            /* rename - needs to get 2nd filename */
            while ((fk->data == 0)                        /* Starting */ 
                   || (fk->filename [fk->data - 1] != 0)) /* Not Finished */
            {
                ARCH_DEP(wfetchc) 
                    (&(fk->filename [fk->data]), 
                     0, 
                     R2, 
                     space_ctl, 
                     regs);

                /* Exception, can recalculate if/when restart */
                //(regs->GR_L(b2)) += 1;
                R2 += 1;
                set_reg(2,R2);
                fk->data += 1;  /* Next PC byte location */
                if (fk->data >= 259) 
                {
                    fk->filename [fk->data] = 0;
                    break;
                }
            }
            StrConverter (fk->filename, DCCebcdic_to_ascii);
        }
        // regs->GR_L(0) = 3;
        R0 = 3;               /* Set work stage */
        set_reg(0,R0);
    }
    
    /*---------------------------------------------------------------------*/
    /* Work Stage 3 Initial or Restart - Interruptible Operation & Results */
    /*---------------------------------------------------------------------*/  
    
    /* Final stage - if we are here, 
       R0==3 which prevents reentering the routines above 
    */
    
    /*------------------------*/
    /* RENAME File Operation  */
    /*------------------------*/

    if (R1 == 0) 
    { 
        /* This is safe to restore early here */
        // regs->GR_L(5) = fk->SaveArea;
        R5 = fk->SaveArea;
        set_reg(5,R5);
        
        // regs->GR_L(15) = rename (fk->oldname, fk->filename);
        R15 = rename (fk->oldname, fk->filename);
        
        if (R15 != 0)
            // regs->GR_L(15) = -errno;
            R15 = -errno;
        set_reg(15,R15);
    } else
       
    /*------------------------*/
    /* UNLINK File Operation  */
    /*------------------------*/       
        
    if (R1 == 1) 
    {
        /* This is safe to restore early here */
        // regs->GR_L(5) = fk->SaveArea;
        R5 = fk->SaveArea;
        set_reg(5,R5);
        
#ifdef _MSVC_
        // regs->GR_L(15) = _unlink (fk->filename);
        R15 = _unlink (fk->filename);
#else
        // regs->GR_L(15) = remove (fk->filename);
        R15 = remove (fk->filename);
#endif
        if (R15 != 0)
            // regs->GR_L(15) = -errno;
            R15 = -errno;
        set_reg(15,R15);
    } else
        
    /*------------------------*/
    /* OPEN File Operation    */
    /*------------------------*/               
        
    if (R1 == 2) 
    {
        /* This is safe to restore early here */
        // regs->GR_L(5) = fk->SaveArea;
        R5 = fk->SaveArea;
        set_reg(5,R5);
        
        /* Convert from JCC to Native open flags */
        i = 0;
        get_reg(R3,3);
        if (R3 & 0x01)
            i |= _O_WRONLY;
        if (R3 & 0x02)
            i |= _O_RDWR;
        if ((R3 & 0x03) == 0)
            i |= _O_RDONLY;
        if (R3 & 0x04) 
        {
            fk->mode = 1; /* Record the desire to translate codepages */
#ifdef _MSVC_
            i |= _O_TEXT; /* Windows can translate newlines to CRLF pairs */
#endif
        } 
        else
        {  
            fk->mode = 0; /* Binary mode (untranslated) */
        }
        if (R3 & 0x10)
            i |= _O_APPEND;
        if (R3 & 0x20)
            i |= _O_TRUNC;
        if (R3 & 0x40) 
        {
            i |= _O_CREAT;
#ifdef _MSVC_
            get_reg(R4,4);
            if (R4 == 0) /* Pass 0x100 in Windows for readonly */
            {
                /* Set a Windows default */
                // regs->GR_L(4) = _S_IWRITE;
                R4 = _S_IWRITE;
                set_reg(4,R4);
            }
#endif
        }
        if (R3 & 0x80)
            i |= _O_EXCL;
#ifndef _MSVC_
        get_reg(R4,4);
        if (R4 == 0)
        {   /* Set a *nix default */
            // regs->GR_L(4) = 0666;
            R4 = 0666;
            set_reg(4,R4);
        }
#endif
        // regs->GR_L(15) = _open (fk->filename, i, regs->GR_L(4));
        R15 = _open (fk->filename, i, R4);
        set_reg(15,R15);
        if (R15 != (unsigned int)-1) 
        {
            /* Save the handle for clean-up later */
            // fk->handle = regs->GR_L(15);
            fk->handle = R15;
#if 0
            dolock (nfile_lock);         /* Take ownership of the list */
            fk->next = fkpr_head;        /* Add to the list */
            fkpr_head = fk;
            unlock (nfile_lock);         /* Release ownership of the list */
#endif
            fk = NULL;                   /* Don't free the structure now */
        } 
        else
        {
            // regs->GR_L(15) = -errno;
            R15 = -errno;
            set_reg(15,R15);
            RemoveFKByID( fk->id );
            /* Note: during start of open operation the fkeeper structure was
               linked to the fkeeper list to allow restart of the open from the 
               id.  On a failure to actually open the file, the structure 
               needs to be removed from the list.
            */
        }
    } else
        
    /*-----------------------------*/
    /* ORPHAN CLOSE File Operation */
    /*-----------------------------*/        
        
    if (R1 == 3) 
    { /* close (char * filename) */
        /* This is safe to restore early here */
        // regs->GR_L(5) = fk->SaveArea;
        R5 = fk->SaveArea;
        set_reg(5,R5);
        
        // regs->GR_L(15) = RemoveFKByName (fk->filename);
        R15 = RemoveFKByName (fk->filename);
        set_reg(15,R15);
    } else
        
    /*-----------------------------*/
    /* READ File Operation         */
    /*-----------------------------*/         
        
    if (R1 == 4) 
    {
        if (fk->mode == -1)
        {
            // fk->mode = GetMode (regs->GR_L(3));
            get_reg(R3,3);
            fk->mode = GetMode (R3);
        }
        get_reg(R4,4);
        get_reg(R2,2);
        /* Note: R15 has been set to zero above during operation initialization */
        while (R4 != 0) 
        {
            if (fk->data == 0) { /* Need to fill our buffer with some data? */
                // i = regs->GR_L(4);
                i = R4;
                
                if (i > 256) 
                    i = 256;
                
                // fk->data = _read (regs->GR_L(3), fk->filename, i);
                get_reg(R3,3);
                fk->data = _read (R3, fk->filename, i);
                
                if (fk->mode)
                    MemConverter (fk->filename, DCCascii_to_ebcdic, fk->data);
            }
            if (fk->data < 0)
            {
                // regs->GR_L(15) = -errno;
                R15 = -errno;
                set_reg(15,R15);
            }
            if (fk->data <= 0)
                /* All done, we reached EOF (or an error occured) */
                break;

            /* Move the data read to the guest's storage */
            i = fk->data - 1;
            ARCH_DEP(wstorec) 
                (fk->filename,  /* This member is used a buffer */
                 (unsigned char)i, 
                 R2, 
                 space_ctl, 
                 regs);
            i++;
            
            /* Exception, can recalculate if/when restart */
            // (regs->GR_L(b2)) += i;
            R2 += i;
            set_reg(2,R2);
            
            /* Remember to stop when enough is read */
            //(regs->GR_L(4)) -= i;
            R4 -= i;
            set_reg(4,R4);
            
            /* Return accumulated total */
            // (regs->GR_L(15)) += i;
            R15 += i;
            set_reg(15,R15);
            
            fk->data = 0;
        }
        // regs->GR_L(5) = fk->SaveArea;
        R5 = fk->SaveArea;
        set_reg(5,R5);
    } else
        
    /*-----------------------------*/
    /* WRITE File Operation        */
    /*-----------------------------*/             
        
        
    if (R1 == 5) 
    {
        if (fk->mode == -1)
        {
            get_reg(R3,3);
            fk->mode = GetMode (R3);
        }
        get_reg(R4,4);
        get_reg(R2,2);
        get_reg(R3,3);
        /* Note: R15 has been set to zero above during operation initialization */

        while (R4 != 0) 
        {
            i = R4 - 1;

            /* Move guest's write data to the internal buffer */
            if (i > 255) 
                i = 255;
            ARCH_DEP(wfetchc) 
                (fk->filename, 
                 (unsigned char)i, 
                 R2, 
                 space_ctl, 
                 regs);   
            i++;
            
            /* Exception, can recalculate if/when restart */
            // (regs->GR_L(b2)) += i;
            R2 += i;
            set_reg(2,R2);
            
            /* Remember to stop when enough is written */
            //(regs->GR_L(4)) -= i;
            R4 -= i;
            set_reg(4,R4);

            if (fk->mode)
                MemConverter (fk->filename, DCCebcdic_to_ascii, i);
            
            /* Write to the host file from the internal buffer */
            // i = _write (regs->GR_L(3), fk->filename, i);
            i = _write (R3, fk->filename, i);
            
            if (i < 0)
            {
                // regs->GR_L(15) = -errno;
                R15 = -errno;
                set_reg(15,R15);
            }
            
            if (i <= 0)
                /* We ran out of space (or an error occured) */
                break;
            
            /* Return accumulated total */
            // (regs->GR_L(15)) += i;
            R15 += i;
            set_reg(15,R15);
        }
        // regs->GR_L(5) = fk->SaveArea;
        R5 = fk->SaveArea;
        set_reg(5,R5);
    }
    
    /* Make sure we do not restart this operation */
    R0 = 0;
    set_reg(5,R5);
    
    if (fk) /* clean up, unless open out this into the global list */
        free (fk);
}

#if !defined(_GEN_ARCH)

#if defined(_ARCHMODE2)
#define _GEN_ARCH _ARCHMODE2
#include "dyn76.c"
#endif 

#if defined(_ARCHMODE3)
#undef _GEN_ARCH
#define _GEN_ARCH _ARCHMODE3
#include "dyn76.c"
#endif 

#endif /*!defined(_GEN_ARCH)*/

#if 0
/* Libtool static name colision resolution */
/* note : lt_dlopen will look for symbol & modulename_LTX_symbol */
#if !defined(HDL_BUILD_SHARED) && defined(HDL_USE_LIBTOOL)
#define hdl_ddev dyn76_LTX_hdl_ddev
#define hdl_depc dyn76_LTX_hdl_depc
#define hdl_reso dyn76_LTX_hdl_reso
#define hdl_init dyn76_LTX_hdl_init
#define hdl_fini dyn76_LTX_hdl_fini
#endif

HDL_DEPENDENCY_SECTION;

/*HDL_DEPENDENCY (HERCULES);*/
HDL_DEPENDENCY (REGS);

END_DEPENDENCY_SECTION;

HDL_REGISTER_SECTION;

HDL_REGISTER (s370_dyninst_opcode_76, s370_dyninst_opcode_76);
HDL_REGISTER (s390_dyninst_opcode_76, s390_dyninst_opcode_76);
HDL_REGISTER (z900_dyninst_opcode_76, z900_dyninst_opcode_76);

END_REGISTER_SECTION;
#endif


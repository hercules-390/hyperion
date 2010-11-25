/* HDIAGF18.C   (c) Copyright, Harold Grovesteen, 2010                */
/*              Hercules DIAGNOSE code X'F18'                         */
/*                                                                    */
/*   Released under "The Q Public License Version 1"                  */
/*   (http://www.hercules-390.org/herclic.html) as modifications to   */
/*   Hercules.                                                        */

/*--------------------------------------------------------------------*/
/* This module implements Hercules DIAGNOSE code X'F18'               */
/* described in ?                                                     */
/*--------------------------------------------------------------------*/


/* The following structure exists between the different functions     */
/* AD == ARCH_DEP                                                     */
/*                                                                    */
/* From diagnose.c                                                    */
/*                                                                    */
/* AD:host_rsc_acc                                                    */
/*    |                                                               */
/*   QUERY:                                                           */
/*    +---> d250_init32---+                                           */
/*    |                   +---> d250_init                             */
/*    +---> d250_init64---+                                           */
/*    |                                                               */
/*   CSOCKET:                                                         */
/*    +-> AD:d250_iorq32--+---SYNC----> d250_list32--+                */
/*    |                   V                   ^      |                */
/*    |               ASYNC Thread            |      |    d250_read   */
/*    |                   +-> AD:d250_async32-+      +--> d250_write  */
/*    |                       d250_bio_interrupt     |    (calls      */
/*    |                                              |    drivers)    */
/*    +-> AD:d250_iorq64--+----SYNC---> d250_list64--+                */
/*    |                   V                   ^                       */
/*    |               ASYNC Thread            |                       */
/*    |                   +-> AD:d250_async64-+                       */
/*    |                       d250_bio_interrupt                      */
/*   CFILE:                                                           */
/*    +---> d250_remove                                               */
/*    |                                                               */
/*   NSOCKET:                                                         */
/*    +--->                                                           */
/*    |                                                               */
/*   NFILE:                                                           */
/*    +--->                                                           */
/*                                                                    */
/*  Function         ARCH_DEP   On CPU   On Async    Owns             */
/*                              thread    thread     device           */
/*                                                                    */
/*  host_rsc_acc        Yes      Yes        No         No             */


#include "hstdinc.h"

#if !defined(_HENGINE_DLL_)
#define _HENGINE_DLL_
#endif /*_HENGINE_DLL_*/

#if !defined(_HDIAGF18_C_)
#define _HDIAGF18_C_
#endif /* !defined(_HDIAGF18_C_) */

#include "hercules.h"
#include "opcode.h"
#include "inline.h"
#include "hdiagf18.h"

#if defined(_FEATURE_HOST_RESOURCE_ACCESS_FACILITY)

#if !defined(_HDIAGF18_C)
#define _HDIAGF18_C

#if !defined(_HDIAGF18_H)
#define _HDIAGF18_H
/*-------------------------------------------------------------------*/
/* Internal macro definitions                                        */
/*-------------------------------------------------------------------*/
/* Function Subcodes                                                 */
#define QUERY   0x00000000   /* Query Operation */
//#define CSOCKET 0x00000001 /* Socket Function Compatibility Mode */
//#define CFILE   0x00000002 /* File Operation Compatibility Mode  */
//#define NSOCKET 0x00000003 /* Socket Function Native Mode */ 
//#define NFILE   0x00000004 /* File Operation Native Mode  */

#define DF18_VER 0x01      /* Version of DIAGNOSE X'F18'    */
#define VER_MASK 0x0F      /* Mask for version in options   */
#define NATIVE_OPTIONS (0x0000+DF18_VER)
#define COMPAT_OPTIONS (0xC000+DF18_VER)



/*-------------------------------------------------------------------*/
/* Capability Parameter Block                                        */
/*-------------------------------------------------------------------*/
CPB cap =
{
        0x00,      /* IPv6 Socket Types not supported */
        0x00,      /* IPv4 Socket Types not supported */
        0x00       /* Resources and modes supported   */
#if defined(CSOCKET)
              | 0x80
#endif /* defined(CSOCKET) */
#if defined(CFILE)
              | 0x40
#endif /* defined(CFILE) */
#if defined(NSOCKET)
              | 0x20
#endif /* defined(NSOCKET) */
#if defined(NFILE)
              | 0x10
#endif /* defined(NFILE) */
        ,
                   /*  Indicate if large files supported */
        ( 0x80 * ( SIZEOF_OFF_T == 8 ) )
#if   defined(__gnu_linux__)
          + 0x01
#elif defined(WIN32)
          + 0x02
#elif defined(SOLARIS)
          + 0x03
#elif defined(__APPLE__)
          + 0x04
#elif defined(__FreeBSD__)
          + 0x05
#elif defined(_AIX)
          + 0x06
#endif
        ,
        /* Compatibility Operation Options in big endian format */
        {COMPAT_OPTIONS>>8,COMPAT_OPTIONS&0xFF},
        /* Native Operation Options in big endian format */
        {NATIVE_OPTIONS>>8,NATIVE_OPTIONS&0xFF}
};
    
#endif /* !defined(_HDIAGF18_H) */

/*-------------------------------------------------------------------*/
/* Internal Architecture Independent Function Prototypes             */
/*-------------------------------------------------------------------*/
/* Initialization Functions */





/*-------------------------------------------------------------------*/
/*  Trigger Block I/O External Interrupt                             */
/*-------------------------------------------------------------------*/


/*-------------------------------------------------------------------*/
/*  Initialize Environment - 32-bit Addressing                       */
/*-------------------------------------------------------------------*/


/*-------------------------------------------------------------------*/
/*  Initialize Environment - 64-bit Addressing                       */
/*-------------------------------------------------------------------*/


/*-------------------------------------------------------------------*/
/*  Initialize Environment - Addressing Independent                  */
/*-------------------------------------------------------------------*/


#endif /*!defined(_HDIAGF18_C)*/

/*-------------------------------------------------------------------*/
/* Internal Architecture Dependent Function Prototypes               */
/*-------------------------------------------------------------------*/
/* Input/Output Request Functions */
//int   ARCH_DEP(d250_iorq32)(DEVBLK *, int *, BIOPL_IORQ32 *, REGS *);

/*-------------------------------------------------------------------*/
/* Access Host Resource (Function code 0xF18)                        */
/*-------------------------------------------------------------------*/
/* r1   - Function subcode                                           */
/*        0x00000000 - Query Capability                              */
/*        0x00000001 - Socket Function Compatibility Mode            */
/*        0x00000002 - File Operation Compatibility Mode             */
/*        0x00000003 - Socket Function Native Mode                   */
/*        0x00000004 - File Operation Native Mode                    */
/* r1+1 - Operation options                                          */
/* r2   - Parameter block real address                               */

void ARCH_DEP(diagf18_call) (int r1, int r2, REGS *regs)
{
/* Guest related paramters and values                                */
RADR    pbaddr;                      /* Parameter block real address */
U16     options;                     /* supplied options             */

#if 0
union   parmlist                     /* Parmater block formats that  */
        {                            /* May be supplied by the guest */        
            BIOPL biopl;
            BIOPL_INIT32 init32;
            BIOPL_IORQ32 iorq32;
            BIOPL_REMOVE remove;
        };
union   parmlist plin;               /* Parm block from/to guest     */
#endif
    
    /* Retrieve the Parameter Block address from Ry */
    pbaddr = regs->GR(r2);

    /* Specification exception if the Rx is 15 */
    if ( (!FACILITY_ENABLED(HOST_RESOURCE_ACCESS,regs)) || (r1 == 15) ) 
    {
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIFICATION_EXCEPTION);
    }
    
    /* Get the options from Rx+1 */
    options = regs->GR_L(r1+1) & 0x0000FFFF;

#if 0
    /* Specification exception if version missing or greater than supported */
    if ( ((options & VER_MASK)==0) || ((options & VER_MASK)>=DF18_VER) )
    {
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIFICATION_EXCEPTION);
    }
    /* This needs to go under the individual subcodes that use options */
#endif
    
    /* Retrieve the Parameter Block address from Ry */
    pbaddr = regs->GR(r2);

    switch(regs->GR_L(r1))
    {

/*--------------------------------------------------------*/
/* Perform the Query Operation                            */
/*--------------------------------------------------------*/
    case QUERY:
            

         break;

#if defined(CSOCKET)
/*--------------------------------------------------------*/
/* Perform Socket Function in Compatibility Mode          */
/*--------------------------------------------------------*/
    case CSOCKET:
         break;

#endif /* defined(CSOCKET) */

#if defined(CFILE)
/*--------------------------------------------------------*/
/* Perform File Operation in Compatibility Mode           */
/*--------------------------------------------------------*/
    case CFILE:
        break;

#endif /* defined(CFILE) */

#if defined(NSOCKET)
/*--------------------------------------------------------*/
/* Perform Socket Function in Native Mode                 */
/*--------------------------------------------------------*/
    case NSOCKET:
        break;
        
#endif /* defined(NSOCKET) */

#if defined(NFILE)
/*--------------------------------------------------------*/
/* Perform File Operation in Native Mode                  */
/*--------------------------------------------------------*/
    case NFILE:
        break;

#endif /* defined(NFILE) */ 

    default:
        ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

    } /* end switch(regs->GR_L(r1)) */
} /* end function ARCH_DEP(host_rsc_acc) */


#endif /* defined(_FEATURE_HOST_RESOURCE_ACCESS_FACILITY) */

#if !defined(_GEN_ARCH)

#if defined(_ARCHMODE2)
 #define  _GEN_ARCH _ARCHMODE2
 #include "hdiagf18.c"
#endif

#if defined(_ARCHMODE3)
 #undef   _GEN_ARCH
 #define  _GEN_ARCH _ARCHMODE3
 #include "hdiagf18.c"
#endif

#endif /*!defined(_GEN_ARCH)*/

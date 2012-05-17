/* CMPSCPUT.H   (c) Copyright "Fish" (David B. Trout), 2012          */
/*              Compression Call Put Next Index Functions            */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id: cmpscput.h 2462 2012-04-29 17:57:15Z Fish $

#ifndef _CMPSCPUT_H_
#define _CMPSCPUT_H_    // Code to be compiled ONLY ONCE goes after here

///////////////////////////////////////////////////////////////////////////////
// COMPRESSION: Put Next output DST Index
//
// Returns:  TRUE if there is no more room remaining in the DST buffer for
//           any more index values (i.e. it's TRUE that we HAVE reached the
//           end of our buffer), otherwise FALSE (i.e. there is still room
//           remaining in the DST buffer for additional index values).

#ifndef _MEMBLK_TYPEDEFFED_
#define _MEMBLK_TYPEDEFFED_
typedef struct MEMBLK MEMBLK;
#endif

struct PIBLK                // PutIndex parameters block
{
    CMPSCBLK*  pCMPSCBLK;   // Ptr to CMPSC instruction control block.
                            // pOp1, nLen1, and cbn updated on success.
    MEMBLK*    pMEMBLK;     // Ptr to operand-1 memory control block
    void**     ppPutIndex;  // Ptr to PutIndex function pointer.
    U16        index;       // U16 index value to be written to DST.
};
typedef struct PIBLK PIBLK; // Returns TRUE/FALSE (no more room/room remains)

typedef U8 (CMPSC_FASTCALL PutIndex)( PIBLK* pPIBLK );

///////////////////////////////////////////////////////////////////////////////
#endif // _CMPSCPUT_H_     // Place all 'ARCH_DEP' code after this statement

extern PutIndex** ARCH_DEP( PutIndexCDSSTab )[5];  // (ptrs to CBN jump tables by CDSS)
extern PutIndex*  ARCH_DEP( PutIndexCBNTab  )[8];  // (ptrs to PutIndex functions by CBN)

///////////////////////////////////////////////////////////////////////////////

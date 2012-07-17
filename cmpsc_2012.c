/* CMPSC_2012.C (c) Copyright "Fish" (David B. Trout), 2012          */
/*              (c) Bernard van der Helm, 2000-2012                  */
/*              S/390 Compression Call Instruction Functions         */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

/*-------------------------------------------------------------------*/
/* This module implements ESA/390 Compression Call instruction as    */
/* described in SA22-7832-08 zArchitecture Principles of Operation   */
/* and in great detail by "SA22-7208-01 ESA/390 Data Compression".   */
/* Please also note that it was designed as written for use by both  */
/* the primary Hercules instruction engine as well as by a separate  */
/* stand alone testing tool utility from outside of Hercules.        */
/*-------------------------------------------------------------------*/

/*-------------------------------------------------------------------*/
/*              Additional credits and copyrights:                   */
/*                                                                   */
/* The technique of expanding symbols until CBN# zero is reached     */
/* and then expanding 8 symbols at a time was borrowed from the      */
/* previous version of Hercules and is thus Bernard van der Helm's   */
/* idea and not mine. Bernard gets all the credit for this technique */
/* and not me. The technique of using a cache of previously expanded */
/* symbols is not my own. It is the exact same technique that the    */
/* previous version of Hercules was using and thus was Bernard's     */
/* idea as well and not my own. Bernard gets all the credit for this */
/* technique as well. Both techniques are copyrighted by him.        */
/*                                                                   */
/* The basic compression/expansion algorithms which are implemented  */
/* however, are the exact algorithms documented by IBM in the manual */
/* SA22-7832-08 zArchitecture Principles of Operation.               */
/*                                                                   */
/* All *OTHER* techniques (bit extraction macros, the index get/put  */
/* functions, the dictionary extraction functions and their accom-   */
/* panying memory access functions to save a call to MADDR) are all  */
/* techniques copyrighted by me and licensed to Hercules.            */
/*                                                                   */
/*                          --  Fish (David B. Trout),  Feb. 2012    */
/*-------------------------------------------------------------------*/

#include "hstdinc.h"    // (MUST be first #include in EVERY source file)

#if !defined(_HENGINE_DLL_)
#define _HENGINE_DLL_
#endif

#if !defined( NOT_HERC )                          // (building Hercules?)
#include "hercules.h"
#include "opcode.h"
#include "inline.h"
#else                                             // (building utility)
#define alt_cmpsc           cmpsc_2012
#endif
#include "cmpsc.h"                                // (Master header for both)

#ifdef FEATURE_COMPRESSION
///////////////////////////////////////////////////////////////////////////////
// Separate return functions for easier debugging...

#ifndef RETFUNCS_ONCE
#define RETFUNCS_ONCE

static CMPSC_INLINE U8 (CMPSC_FASTCALL ERR)( CMPSCBLK* pCMPSCBLK );
static CMPSC_INLINE U8 (CMPSC_FASTCALL CC3)( CMPSCBLK* pCMPSCBLK );
static CMPSC_INLINE U8 (CMPSC_FASTCALL CC1)( CMPSCBLK* pCMPSCBLK );
static CMPSC_INLINE U8 (CMPSC_FASTCALL CC0)( CMPSCBLK* pCMPSCBLK );

#define RETERR() return ERR( pCMPSCBLK ) // (failure)
#define RETCC3() return CC3( pCMPSCBLK ) // (stop)
#define RETCC1() return CC1( pCMPSCBLK ) // (stop)
#define RETCC0() return CC0( pCMPSCBLK ) // (stop)

typedef struct EXPBLK EXPBLK; // (fwd ref)

static CMPSC_INLINE U8 (CMPSC_FASTCALL EXPOK )( CMPSCBLK* pCMPSCBLK, EXPBLK* pEXPBLK );
static CMPSC_INLINE U8 (CMPSC_FASTCALL EXPERR)( CMPSCBLK* pCMPSCBLK, EXPBLK* pEXPBLK );
static CMPSC_INLINE U8 (CMPSC_FASTCALL EXPCC3)( CMPSCBLK* pCMPSCBLK, EXPBLK* pEXPBLK );
static CMPSC_INLINE U8 (CMPSC_FASTCALL EXPCC1)( CMPSCBLK* pCMPSCBLK, EXPBLK* pEXPBLK );
static CMPSC_INLINE U8 (CMPSC_FASTCALL EXPCC0)( CMPSCBLK* pCMPSCBLK, EXPBLK* pEXPBLK );

#define EXP_RETOK()  return EXPOK ( pCMPSCBLK, pEXPBLK ) // (success; keep going)
#define EXP_RETERR() return EXPERR( pCMPSCBLK, pEXPBLK ) // (break)
#define EXP_RETCC3() return EXPCC3( pCMPSCBLK, pEXPBLK ) // (break)
#define EXP_RETCC1() return EXPCC1( pCMPSCBLK, pEXPBLK ) // (break)
#define EXP_RETCC0() return EXPCC0( pCMPSCBLK, pEXPBLK ) // (break)

#endif // RETFUNCS_ONCE

///////////////////////////////////////////////////////////////////////////////
// Symbols Cache Control Entry

#ifndef EXP_ONCE
#define EXP_ONCE

#ifdef CMPSC_SYMCACHE

struct SYMCTL           // Symbol Cache Control Entry
{
    U16   idx;          // Cache index      (symbol's location within cache)
    U16   len;          // Symbol length    (symbol's total expanded length)
};
typedef struct SYMCTL SYMCTL;

#endif // CMPSC_SYMCACHE

///////////////////////////////////////////////////////////////////////////////
// EXPAND Index Symbol parameters block

struct EXPBLK                 // EXPAND Index Symbol parameters block
{
#ifdef CMPSC_SYMCACHE
    SYMCTL  symcctl[ MAX_DICT_ENTRIES ];     // Symbols cache control entries
    U8      symcache[ CMPSC_SYMCACHE_SIZE ]; // Previously expanded symbols
    U16     symindex;                        // Next available cache location
#endif // CMPSC_SYMCACHE
    DCTBLK      dctblk;       // GetDCT parameters block
    ECEBLK      eceblk;       // GetECE parameters block
    MEMBLK      op1blk;       // Operand-1 memory access control block
    MEMBLK      op2blk;       // Operand-2 memory access control block
    ECE         ece;          // Expansion Character Entry data
    U16         symlen;       // Working symbol length value
    U16         index;        // SRC Index value
    U8          SRC_bytes;    // Number of bytes to adjust the SRC ptr/len by
    U8          rc;           // TRUE == success (cc), FALSE == failure (pic)
};

#endif // EXP_ONCE
///////////////////////////////////////////////////////////////////////////////
// EXPAND Index Symbol; TRUE == success, FALSE == error; rc == return code

U8 (CMPSC_FASTCALL ARCH_DEP( cmpsc_Expand_Index ))( CMPSCBLK* pCMPSCBLK, EXPBLK* pEXPBLK );

///////////////////////////////////////////////////////////////////////////////
// EXPANSION: TRUE == success (cc), FALSE == failure (pic)

U8 (CMPSC_FASTCALL ARCH_DEP( cmpsc_Expand ))( CMPSCBLK* pCMPSCBLK )
{
    U64         pCPULimit;      // CPU Limit SRC pointer
    GetIndex**  ppGetIndex;     // Ptr to GetNextIndex table for this CDSS-1
    GetIndex*   pGetIndex;      // Ptr to GetNextIndex function for this CBN
    GIBLK       giblk;          // GetIndex parameters block
    EXPBLK      expblk;         // EXPAND Index Symbol parameters block
    U16         index[8];       // SRC Index values
    U8          bits;           // Number of bits per index

    pCPULimit   = pCMPSCBLK->pOp2 + max( MIN_CMPSC_CPU_AMT, min( MAX_CMPSC_CPU_AMT, pCMPSCBLK->nCPUAmt ));
    ppGetIndex  = ARCH_DEP( GetIndexCDSSTab  )[ pCMPSCBLK->cdss - 1 ];
    pGetIndex   = ppGetIndex [ pCMPSCBLK->cbn ];
    bits        = pCMPSCBLK->cdss + 8;

    // Initialize GetIndex parameters block

    memset( &giblk, 0, sizeof( giblk ));

    giblk.pCMPSCBLK   =  pCMPSCBLK;
    giblk.pMEMBLK     =  &expblk.op2blk;    // (we get indexes from op-2)
    giblk.pIndex      =  &index[0];
    giblk.ppGetIndex  =  (void**) &pGetIndex;

    // Initialize EXPAND Index Symbol parameters block

    memset( &expblk, 0, sizeof( expblk ));

    expblk.dctblk.regs      = pCMPSCBLK->regs;
    expblk.dctblk.arn       = pCMPSCBLK->r2;
    expblk.dctblk.pkey      = pCMPSCBLK->regs->psw.pkey;
    expblk.dctblk.pDict     = pCMPSCBLK->pDict;

    expblk.eceblk.pDCTBLK   = &expblk.dctblk;
    expblk.eceblk.max_index = 0xFFFF >> (16 - bits);
    expblk.eceblk.pECE      = &expblk.ece;

    expblk.op1blk.arn       = pCMPSCBLK->r1;
    expblk.op1blk.regs      = pCMPSCBLK->regs;
    expblk.op1blk.pkey      = pCMPSCBLK->regs->psw.pkey;

    expblk.op2blk.arn       = pCMPSCBLK->r2;
    expblk.op2blk.regs      = pCMPSCBLK->regs;
    expblk.op2blk.pkey      = pCMPSCBLK->regs->psw.pkey;

#ifdef CMPSC_EXPAND8

    // Expand individual index symbols until CBN==0...

    if ((pCMPSCBLK->cbn & 7) != 0)
    {
        while ((pCMPSCBLK->pOp2 < pCPULimit) && (pCMPSCBLK->cbn & 7) != 0)
        {
            // Get index symbol...

            if (unlikely( !(expblk.SRC_bytes = pGetIndex( &giblk ))))
                RETCC0();

            // Expand it...

            expblk.index = index[0];

            if (unlikely( !ARCH_DEP( cmpsc_Expand_Index )( pCMPSCBLK, &expblk )))
                return expblk.rc;

            // Bump source...

            pCMPSCBLK->pOp2  += expblk.SRC_bytes;
            pCMPSCBLK->nLen2 -= expblk.SRC_bytes;
            pCMPSCBLK->cbn   += bits;

            MEMBLK_BUMP( &expblk.op2blk, pCMPSCBLK->pOp2 );

            // Bump destination...

            pCMPSCBLK->pOp1  += expblk.symlen;
            pCMPSCBLK->nLen1 -= expblk.symlen;

            MEMBLK_BUMP( &expblk.op1blk, pCMPSCBLK->pOp1 );
        }
    }

    // Now expand eight (8) index symbols at a time...

    if ((pCMPSCBLK->cbn & 7) == 0)
    {
        GetIndex**  ppGet8Index;    // Ptr to GetNext8Index table for this CDSS-1
        GetIndex*   pGet8Index;     // Ptr to GetNext8Index function for this CBN
        CMPSCBLK    save_cmpsc;     // Work context
        MEMBLK      save_op1blk;    // Work context
        MEMBLK      save_op2blk;    // Work context
        U8          i;              // (work)

        ppGet8Index = ARCH_DEP( Get8IndexCDSSTab )[ pCMPSCBLK->cdss - 1 ];
        pGet8Index  = ppGet8Index[ 0 ]; // (always CBN==0)

        // Save context

        memcpy( &save_cmpsc,  pCMPSCBLK,      sizeof( CMPSCBLK ));
        memcpy( &save_op1blk, &expblk.op1blk, sizeof( MEMBLK   ));
        memcpy( &save_op2blk, &expblk.op2blk, sizeof( MEMBLK   ));

        while (pCMPSCBLK->pOp2 < pCPULimit)
        {
            // Retrieve 8 index symbols from operand-2...

            if (unlikely( !(expblk.SRC_bytes = pGet8Index( &giblk ))))
                break;

            // Bump source...

            pCMPSCBLK->pOp2  += expblk.SRC_bytes;
            pCMPSCBLK->nLen2 -= expblk.SRC_bytes;

            MEMBLK_BUMP( &expblk.op2blk, pCMPSCBLK->pOp2 );

            // Expand each of the 8 individually into operand-1...

            for (i=0; i < 8; i++)
            {
                expblk.index = index[i];

                if (unlikely( !ARCH_DEP( cmpsc_Expand_Index )( pCMPSCBLK, &expblk )))
                {
                    // Restore context

                    memcpy( pCMPSCBLK,      &save_cmpsc,  sizeof( CMPSCBLK ));
                    memcpy( &expblk.op1blk, &save_op1blk, sizeof( MEMBLK   ));
                    memcpy( &expblk.op2blk, &save_op2blk, sizeof( MEMBLK   ));
                    break; // (i < 8)
                }

                // Bump destination...

                pCMPSCBLK->pOp1  += expblk.symlen;
                pCMPSCBLK->nLen1 -= expblk.symlen;
                pCMPSCBLK->cbn   += bits;

                MEMBLK_BUMP( &expblk.op1blk, pCMPSCBLK->pOp1 );
            }

            if (i < 8)
                break;

            // Save context

            memcpy( &save_cmpsc,  pCMPSCBLK, sizeof( CMPSCBLK ));
            memcpy( &save_op1blk, &expblk.op1blk, sizeof( MEMBLK   ));
            memcpy( &save_op2blk, &expblk.op2blk, sizeof( MEMBLK   ));
        }
    }

#endif // CMPSC_EXPAND8

    // Finish up any remainder...

    while (pCMPSCBLK->pOp2 < pCPULimit)
    {
        // Get index symbol...

        if (unlikely( !(expblk.SRC_bytes = pGetIndex( &giblk ))))
            RETCC0();

        // Expand it...

        expblk.index = index[0];

        if (unlikely( !ARCH_DEP( cmpsc_Expand_Index )( pCMPSCBLK, &expblk )))
            return expblk.rc;

        // Bump source...

        pCMPSCBLK->pOp2  += expblk.SRC_bytes;
        pCMPSCBLK->nLen2 -= expblk.SRC_bytes;
        pCMPSCBLK->cbn   += bits;

        MEMBLK_BUMP( &expblk.op2blk, pCMPSCBLK->pOp2 );

        // Bump destination...

        pCMPSCBLK->pOp1  += expblk.symlen;
        pCMPSCBLK->nLen1 -= expblk.symlen;

        MEMBLK_BUMP( &expblk.op1blk, pCMPSCBLK->pOp1 );
    }

    RETCC3();
}

///////////////////////////////////////////////////////////////////////////////
// EXPAND Index Symbol; TRUE == success, FALSE == error; rc == return code

U8 (CMPSC_FASTCALL ARCH_DEP( cmpsc_Expand_Index ))( CMPSCBLK* pCMPSCBLK, EXPBLK* pEXPBLK )
{
    U8  dicts;  // Counts dictionary entries processed

    if (unlikely( !pCMPSCBLK->nLen1 ))
        EXP_RETCC1();

    if (unlikely( pEXPBLK->index >= 256 ))
    {
#ifdef CMPSC_SYMCACHE
        // Check our cache of previously expanded index symbols
        // to see if we've already expanded this symbol before
        // and if we have room in the o/p buffer to expand it.

        if (1
            && (pEXPBLK->symlen  = pEXPBLK->symcctl[ pEXPBLK->index ].len) > 0
            &&  pEXPBLK->symlen <= pCMPSCBLK->nLen1
        )
        {
            store_op_str( &pEXPBLK->symcache[ pEXPBLK->symcctl[ pEXPBLK->index ].idx ], pEXPBLK->symlen-1, pCMPSCBLK->pOp1, &pEXPBLK->op1blk );
        }
        else
#endif // CMPSC_SYMCACHE
        {
            // We need to expand this index symbol: fetch the ECE...

            if (unlikely( !ARCH_DEP( GetECE )( pEXPBLK->index, &pEXPBLK->eceblk )))
                EXP_RETERR();

            if (pEXPBLK->ece.psl)
            {
                // Preceded (i.e. partial symbol)...
                // Do we have room for the complete symbol?

                if (unlikely( pCMPSCBLK->nLen1 < (pEXPBLK->symlen = pEXPBLK->ece.psl + pEXPBLK->ece.ofst)))
                    EXP_RETCC1();

                dicts = 1;

                do
                {
                    // Expand this partial ("preceded") chunk of this index symbol...

                    store_op_str( pEXPBLK->ece.ec, pEXPBLK->ece.psl-1, pCMPSCBLK->pOp1 + pEXPBLK->ece.ofst, &pEXPBLK->op1blk );

                    // Get the ECE for the next chunk...

                    if (unlikely( !ARCH_DEP( GetECE )( pEXPBLK->ece.pptr, &pEXPBLK->eceblk )))
                        EXP_RETERR();

                    if (unlikely( ++dicts > 127 ))
                        EXP_RETERR();
                }
                while (pEXPBLK->ece.psl);

                // we're done with the partial ("preceded") part of this
                // symbol's expansion. Fall through to the "complete" symbol
                // expansion logic to finish up this symbol's expansion...
            }
            else
            {
                // Unpreceded (i.e. complete symbol)...
                // Do we have room for the complete symbol?

                if (unlikely( pCMPSCBLK->nLen1 < (pEXPBLK->symlen = pEXPBLK->ece.csl)))
                    EXP_RETCC1();
            }

            // Complete the expansion of this index symbol...

            store_op_str( pEXPBLK->ece.ec, pEXPBLK->ece.csl-1, pCMPSCBLK->pOp1, &pEXPBLK->op1blk );

#ifdef CMPSC_SYMCACHE
            // If there's room for it, add this symbol to our expanded symbols cache

            if (pEXPBLK->symlen <= (sizeof( pEXPBLK->symcache ) - pEXPBLK->symindex))
            {
                pEXPBLK->symcctl[ pEXPBLK->index ].len = pEXPBLK->symlen;
                pEXPBLK->symcctl[ pEXPBLK->index ].idx = pEXPBLK->symindex;

                // (add this symbol to our previously expanded symbols cache)

                fetch_op_str( &pEXPBLK->symcache[ pEXPBLK->symindex ], pEXPBLK->symlen-1, pCMPSCBLK->pOp1, &pEXPBLK->op1blk );

                pEXPBLK->symindex += pEXPBLK->symlen;
            }
#endif // CMPSC_SYMCACHE
        }
    }
    else // (pEXPBLK->index < 256)
    {
        // The index symbol is an alphabet entry (i.e. an index symbol
        // corresponding to a one-byte symbol). The expanded symbol it
        // represents is the value of the alphabet index symbol itself.

        store_op_b( (U8)pEXPBLK->index, pCMPSCBLK->pOp1, &pEXPBLK->op1blk );
        pEXPBLK->symlen = 1;
    }

    EXP_RETOK();
}

///////////////////////////////////////////////////////////////////////////////
// COMPRESSION: TRUE == success (cc), FALSE == failure (pgmck).

U8 (CMPSC_FASTCALL ARCH_DEP( cmpsc_Compress ))( CMPSCBLK* pCMPSCBLK )
{
    U64         pCPULimit;          // CPU Limit SRC pointer
    U64         pEndMaxSym;         // Ptr to end of maximum length symbol
    PutIndex**  ppPutIndex;         // Ptr to PutNextIndex table for this CDSS-1
    PutIndex*   pPutIndex;          // Ptr to PutNextIndex function for this CBN
    U64         pSymTab;            // Symbol-Translation Table
    GETSD*      pGetSD;             // Pointer to Get-Sibling-Descriptor function
    CCE         parent;             // Parent Compression Character Entry data
    CCE         child;              // Child  Compression Character Entry data
    SDE         sibling;            // Sibling Descriptor Entry data
    MEMBLK      op1blk;             // Operand-1 memory access control block
    MEMBLK      op2blk;             // Operand-2 memory access control block
    DCTBLK      dctblk;             // GetDCT parameters block  (cmp dict)
    DCTBLK      dctblk2;            // GetDCT parameters block  (exp dict)
    CCEBLK      cceblk;             // GetCCE parameters block
    SDEBLK      sdeblk;             // GetSDn parameters block
    PIBLK       piblk;              // PutIndex parameters block
    U16         parent_index;       // Parent's CE Index value
    U16         child_index;        // Child's CE Index value
    U16         sibling_index;      // Sibling's SDE Index value
    U16         max_index;          // Maximum Index value
    U16         children;           // Counts children
    U8          ccnum, scnum, byt;  // (work variables)
    U8          eodst, equal;       // (work flags)
    U8          bits;               // Number of bits per index
    U8          wrk[ MAX_SYMLEN ];  // (work buffer)

    bits       = 8 + pCMPSCBLK->cdss;
    max_index  = (0xFFFF >> (16 - bits));
    ppPutIndex = ARCH_DEP( PutIndexCDSSTab )[ pCMPSCBLK->cdss - 1 ];
    pPutIndex  = ppPutIndex[ pCMPSCBLK->cbn ];
    pCPULimit  = pCMPSCBLK->pOp2 + max( MIN_CMPSC_CPU_AMT, min( MAX_CMPSC_CPU_AMT, pCMPSCBLK->nCPUAmt ));
    pSymTab    = pCMPSCBLK->st ? pCMPSCBLK->pDict + ((U32)pCMPSCBLK->stt << 7) : 0;
    pGetSD     = pCMPSCBLK->f1 ? ARCH_DEP( GetSD1 ) : ARCH_DEP( GetSD0 );

    memset( &dctblk,  0, sizeof( dctblk ) );
    memset( &dctblk2, 0, sizeof( dctblk ) );

    dctblk.regs       = pCMPSCBLK->regs;
    dctblk.arn        = pCMPSCBLK->r2;
    dctblk.pkey       = pCMPSCBLK->regs->psw.pkey;
    dctblk.pDict      = pCMPSCBLK->pDict;

    dctblk2.regs      = pCMPSCBLK->regs;
    dctblk2.arn       = pCMPSCBLK->r2;
    dctblk2.pkey      = pCMPSCBLK->regs->psw.pkey;
    dctblk2.pDict     = pCMPSCBLK->pDict + g_nDictSize[ pCMPSCBLK->cdss - 1 ];

    cceblk.pDCTBLK    = &dctblk;
    cceblk.max_index  = max_index;
    cceblk.pCCE       = NULL;           // (filled in before each call)

    sdeblk.pDCTBLK    = &dctblk;
    sdeblk.pDCTBLK2   = &dctblk2;
    sdeblk.pSDE       = &sibling;
    sdeblk.pCCE       = NULL;           // (depends if first sibling)

    piblk.ppPutIndex  = (void**) &pPutIndex;
    piblk.pCMPSCBLK   = pCMPSCBLK;
    piblk.pMEMBLK     = &op1blk;        // (we put indexes into op-1)
    piblk.index       = 0;              // (filled in before each call)

    op1blk.arn       = pCMPSCBLK->r1;
    op1blk.regs      = pCMPSCBLK->regs;
    op1blk.pkey      = pCMPSCBLK->regs->psw.pkey;
    op1blk.vpagebeg  = 0;
    op1blk.maddr[0]  = 0;
    op1blk.maddr[1]  = 0;

    op2blk.arn       = pCMPSCBLK->r2;
    op2blk.regs      = pCMPSCBLK->regs;
    op2blk.pkey      = pCMPSCBLK->regs->psw.pkey;
    op2blk.vpagebeg  = 0;
    op2blk.maddr[0]  = 0;
    op2blk.maddr[1]  = 0;

    // GET STARTED...

    eodst = (pCMPSCBLK->nLen1 < (2 + ((pCMPSCBLK->cbn > (16 - bits)) ? 1 : 0))) ? TRUE : FALSE;

    //-------------------------------------------------------------------------
    // PROGRAMMING NOTE: the following compression algorithm follows exactly
    // the algorithm documented by IBM in their Principles of Operation manual.
    // Most labels and all primary comments match the algorithm's flowchart.
    //-------------------------------------------------------------------------

cmp1:

    // Another SRC char exists?
    // No, set CC0 and endop.

    if (unlikely( !pCMPSCBLK->nLen2 ))
        RETCC0();

cmp2:

    // Another DST index position exists?
    // No, set CC1 and endop.

    if (unlikely( eodst ))
        RETCC1();

    if (unlikely( pCMPSCBLK->pOp2 >= pCPULimit ))   // (max bytes processed?)
        RETCC3();                                   // (return cc3 to caller)

    pEndMaxSym = pCMPSCBLK->pOp2 + MAX_SYMLEN;
    children   = 0;

    // Use next SRC char as index of alphabet entry.
    // Call this entry the parent.
    // Advance 1 byte in SRC.

    parent_index = (U16) fetch_op_b( pCMPSCBLK->pOp2, &op2blk );

    cceblk.pCCE = &parent;
    if (unlikely( !ARCH_DEP( GetCCE )( parent_index, &cceblk )))
        RETERR();

    pCMPSCBLK->pOp2++;
    pCMPSCBLK->nLen2--;

cmp3:

    MEMBLK_BUMP( &op1blk, pCMPSCBLK->pOp1 );
    MEMBLK_BUMP( &op2blk, pCMPSCBLK->pOp2 );

    // CCT=0?
    // Yes, goto cmp9;

    if (!parent.cct)
        goto cmp9;

//cmp4:

    ccnum = 0;

    // Set flag=1.

    equal = 1;

    // Another SRC char exists?
    // No, goto cmp13;

    if (unlikely( !pCMPSCBLK->nLen2 ))
        goto cmp13;

    // (REPEAT FOR EACH CC)...

cmp5:

    // ---------------- UNROLL #1 ----------------

    // Next SRC char = CC?
    // Yes, goto cmp10;

    byt = fetch_op_b( pCMPSCBLK->pOp2, &op2blk );

    if (byt == parent.cc[ ccnum ])
        goto cmp10;

    // Set flag=0;
    equal = 0;

    // Another CC?
    // Yes, goto cmp5;

    if (++ccnum >= parent.cct)
        goto cmp5E;

    // ---------------- UNROLL #2 ----------------

    // Next SRC char = CC?
    // Yes, goto cmp10;

    if (byt == parent.cc[ ccnum ])
        goto cmp10;

    // Set flag=0;
//  equal = 0;      // (already done by UNROLL #1)

    // Another CC?
    // Yes, goto cmp5;

    if (++ccnum >= parent.cct)
        goto cmp5E;

    // ---------------- UNROLL #3 ----------------

    // Next SRC char = CC?
    // Yes, goto cmp10;

    if (byt == parent.cc[ ccnum ])
        goto cmp10;

    // Set flag=0;
//  equal = 0;      // (already done by UNROLL #1)

    // Another CC?
    // Yes, goto cmp5;

    if (++ccnum >= parent.cct)
        goto cmp5E;

    // ---------------- UNROLL #4 ----------------

    // Next SRC char = CC?
    // Yes, goto cmp10;

    if (byt == parent.cc[ ccnum ])
        goto cmp10;

    // Set flag=0;
//  equal = 0;      // (already done by UNROLL #1)

    // Another CC?
    // Yes, goto cmp5;

    if (++ccnum >= parent.cct)
        goto cmp5E;

    // ---------------- UNROLL #5 ----------------

    // Next SRC char = CC?
    // Yes, goto cmp10;

    if (byt == parent.cc[ ccnum ])
        goto cmp10;

    // Set flag=0;
//  equal = 0;      // (already done by UNROLL #1)

    // Another CC?
    // Yes, goto cmp5;

    if (++ccnum < parent.cct)   // (** last UNROLL **)
        goto cmp5;

cmp5E:

    // CCT indicates more children?
    // No, goto cmp8;

    if (!parent.mc)
        goto cmp8;

//cmp6:

    // Set SD index = CPTR + #of CC's.

    sibling_index = (parent.cptr + parent.cct);

    sdeblk.pCCE = &parent;
    if (unlikely( !pGetSD( sibling_index, &sdeblk )))
        RETERR();
    scnum = 0;

    // (REPEAT FOR EACH SC IN SD)...

cmp7:

    // ---------------- UNROLL #1 ----------------

    // Next SRC char = SC?
    // Yes, goto cmp12;

    byt = fetch_op_b( pCMPSCBLK->pOp2, &op2blk );

    if (byt == sibling.sc[ scnum ])
        goto cmp12;

    // Another SC?
    // Yes, goto cmp7;

    if (++scnum >= sibling.sct)
        goto cmp7E;

    // ---------------- UNROLL #2 ----------------

    // Next SRC char = SC?
    // Yes, goto cmp12;

    if (byt == sibling.sc[ scnum ])
        goto cmp12;

    // Another SC?
    // Yes, goto cmp7;

    if (++scnum >= sibling.sct)
        goto cmp7E;

    // ---------------- UNROLL #3 ----------------

    // Next SRC char = SC?
    // Yes, goto cmp12;

    if (byt == sibling.sc[ scnum ])
        goto cmp12;

    // Another SC?
    // Yes, goto cmp7;

    if (++scnum >= sibling.sct)
        goto cmp7E;

    // ---------------- UNROLL #4 ----------------

    // Next SRC char = SC?
    // Yes, goto cmp12;

    if (byt == sibling.sc[ scnum ])
        goto cmp12;

    // Another SC?
    // Yes, goto cmp7;

    if (++scnum >= sibling.sct)
        goto cmp7E;

    // ---------------- UNROLL #5 ----------------

    // Next SRC char = SC?
    // Yes, goto cmp12;

    if (byt == sibling.sc[ scnum ])
        goto cmp12;

    // Another SC?
    // Yes, goto cmp7;

    if (++scnum >= sibling.sct)
        goto cmp7E;

    // ---------------- UNROLL #6 ----------------

    // Next SRC char = SC?
    // Yes, goto cmp12;

    if (byt == sibling.sc[ scnum ])
        goto cmp12;

    // Another SC?
    // Yes, goto cmp7;

    if (++scnum >= sibling.sct)
        goto cmp7E;

    // ---------------- UNROLL #7 ----------------

    // Next SRC char = SC?
    // Yes, goto cmp12;

    if (byt == sibling.sc[ scnum ])
        goto cmp12;

    // Another SC?
    // Yes, goto cmp7;

    if (++scnum < sibling.sct)  // (** last UNROLL **)
        goto cmp7;

cmp7E:

    // SCT indicates more children?
    // No, goto cmp8;

    if (!sibling.ms)
        goto cmp8;

    // Set SD index = current SD index + #of SC's + 1.

    sibling_index += sibling.sct + 1;

    sdeblk.pCCE = NULL;
    if (unlikely( !pGetSD( sibling_index, &sdeblk )))
        RETERR();
    scnum = 0;

    // goto cmp7;

    goto cmp7;

cmp8:

    // Store parent index in DST.
    // Advance 1 index in DST.
    // goto cmp2;

    piblk.index = (!pCMPSCBLK->st) ? parent_index
          : fetch_dct_hw( pSymTab + (parent_index << 1), pCMPSCBLK );
    eodst = pPutIndex( &piblk );
    goto cmp2;

cmp9:

    // Store parent index in DST.
    // Advance 1 index in DST.
    // goto cmp1;

    piblk.index = (!pCMPSCBLK->st) ? parent_index
          : fetch_dct_hw( pSymTab + (parent_index << 1), pCMPSCBLK );
    eodst = pPutIndex( &piblk );
    goto cmp1;

cmp10:

    if (unlikely( ++children > MAX_CHILDREN ))
        RETERR();

    // Set child index = CPTR + CC number (0-origin numbering).

    child_index = parent.cptr + ccnum;

    // X=1 for child?
    // No, goto cmp14;

    if (!(parent.ecb & (0x8000 >> ccnum)))
        goto cmp14;

    // ACT=0 or D=0 in child?
    // Yes, goto cmp15;

    cceblk.pCCE = &child;
    if (unlikely( !ARCH_DEP( GetCCE )( child_index, &cceblk )))
        RETERR();

    if (!child.act)
        goto cmp15;

    // (compare SRC chars after next char to AEC's in child)...

    // Enough SRC chars for comparison?
    // No, goto cmp11;

    if (unlikely( pCMPSCBLK->nLen2 < (U64)(1 + child.act)))
        goto cmp11;

    // Chars equal?
    // Yes, goto cmp16;

    fetch_op_str( wrk, child.act-1, pCMPSCBLK->pOp2 + 1, &op2blk );

    if (memcmp( wrk, &child.ec[0], child.act ) == 0)
        goto cmp16;

cmp11:

    // Flag=1?
    // No, goto cmp8;

    if (equal != 1)
        goto cmp8;

    // Another CC?
    // No, goto cmp8;

    if (++ccnum >= parent.cct)
        goto cmp8;

    // goto cmp5;

    goto cmp5;

cmp12:

    if (unlikely( ++children > MAX_CHILDREN ))
        RETERR();

    // Set child index = SD index + SC number (1-origin numbering).

    child_index = sibling_index + (scnum + 1);

    // Y=1 for child or no Y?
    // No, goto cmp14;

    if (!(sibling.ecb & (0x8000 >> scnum)))
        goto cmp14;

    // ACT=0 or D=0 in child?
    // Yes, goto cmp15;

    cceblk.pCCE = &child;
    if (unlikely( !ARCH_DEP( GetCCE )( child_index, &cceblk )))
        RETERR();

    if (!child.act)
        goto cmp15;

    // (compare SRC chars after next char to AEC's in child)...

    // Enough SRC chars for comparison?
    // No, goto cmp8;

    if (pCMPSCBLK->nLen2 < (U64)(1 + child.act))
        goto cmp8;

    // Chars equal?
    // Yes, goto cmp16;

    fetch_op_str( wrk, child.act-1, pCMPSCBLK->pOp2 + 1, &op2blk );

    if (memcmp( wrk, &child.ec[0], child.act ) == 0)
        goto cmp16;

    // goto cmp8;

    goto cmp8;

cmp13:

    // Store parent index in DST.
    // Advance 1 index in DST;
    // Set CC0 and endop.

    piblk.index = (!pCMPSCBLK->st) ? parent_index
          : fetch_dct_hw( pSymTab + (parent_index << 1), pCMPSCBLK );
    pPutIndex( &piblk );
    RETCC0();

cmp14:

    // Store child index in DST.
    // Advance 1 index in DST.
    // Advance 1 byte in SRC.
    // goto cmp1;

    pCMPSCBLK->pOp2++;
    pCMPSCBLK->nLen2--;

    piblk.index = (!pCMPSCBLK->st) ? child_index
          : fetch_dct_hw( pSymTab + (child_index << 1), pCMPSCBLK );
    eodst = pPutIndex( &piblk );
    goto cmp1;

cmp15:

    // Call child the parent.
    // Advance 1 byte in SRC.
    // goto cmp3;

    parent       = child;
    parent_index = child_index;

    pCMPSCBLK->pOp2++;
    pCMPSCBLK->nLen2--;

    if (unlikely( pCMPSCBLK->pOp2 > pEndMaxSym ))
        RETERR();

    goto cmp3;

cmp16:

    // Call child the parent.
    // Advance in SRC by 1 + #of AEC bytes.
    // goto cmp3;

    parent       = child;
    parent_index = child_index;

    pCMPSCBLK->pOp2   += 1 + child.act;
    pCMPSCBLK->nLen2  -= 1 + child.act;

    if (unlikely( pCMPSCBLK->pOp2 > pEndMaxSym ))
        RETERR();

    goto cmp3;
}

/*---------------------------------------------------------------------------*/
/* B263 CMPSC - Compression Call                                       [RRE] */
/*---------------------------------------------------------------------------*/
DEF_INST( alt_cmpsc )
{
    CMPSCBLK cmpsc;                     /* Compression Call parameters block  */
    int  r1, r2, rc;                    /* Operand reg numbers, return code  */
    RRE( inst, regs, r1, r2 );          /* Decode the instruction...         */

    /* Build our internal Compression Call parameters block */

    ARCH_DEP( cmpsc_SetCMPSC )( &cmpsc, regs, r1, r2 );

    /* Verify that an even-odd register pair was specified
       and that the compressed-data symbol size is valid. */

    if (likely( 1
        && !(r1 & 0x01)       /* (even register number?) */
        && !(r2 & 0x01)       /* (even register number?) */
        && cmpsc.cdss >= 1    /* (is symbol size valid?) */
        && cmpsc.cdss <= 5    /* (is symbol size valid?) */
    ))
    {
        /* Perform the Compression or Expansion */

        rc = (regs->GR_L(0) & 0x100)
            ? ARCH_DEP( cmpsc_Expand   )( &cmpsc )
            : ARCH_DEP( cmpsc_Compress )( &cmpsc );

        /* Update register context with results */

        cmpsc.cbn &= 0x07;
        ARCH_DEP( cmpsc_SetREGS )( &cmpsc, regs, r1, r2 );

        /* Program Check Interrupt if we failed */

        if (unlikely( !rc ))
        {
            regs->dxc = DXC_DECIMAL; /* Set Data Exception code */
            ARCH_DEP( program_interrupt )( regs, PGM_DATA_EXCEPTION );
        }
    }
    else
        ARCH_DEP( program_interrupt )( regs, PGM_SPECIFICATION_EXCEPTION );
}

///////////////////////////////////////////////////////////////////////////////

#ifndef _CMPSC_C_
#define _CMPSC_C_           // Code to be compiled ONLY ONCE goes after here

///////////////////////////////////////////////////////////////////////////////
// Dictionary sizes in bytes by CDSS

static const U32 g_nDictSize[ MAX_CDSS ] =
{                 // ------------- Dictionary sizes by CDSS -------------
      512 * 8,    // cdss 1:   512  8-byte entries =    4K   (4096 bytes)
     1024 * 8,    // cdss 2:  1024  8-byte entries =    8K   (2048 bytes)
     2048 * 8,    // cdss 3:  2048  8-byte entries =   16K  (16384 bytes)
     4096 * 8,    // cdss 4:  4096  8-byte entries =   32K  (32768 bytes)
     8192 * 8,    // cdss 5:  8192  8-byte entries =   64K  (65536 bytes)
};

///////////////////////////////////////////////////////////////////////////////
// Separate return functions for easier debugging... (it's much easier to
// set a breakpoint here and then examine the stack to see where you came
// from than it is to set breakpoints everywhere each functions is called)

static CMPSC_INLINE U8 (CMPSC_FASTCALL ERR)( CMPSCBLK* pCMPSCBLK )
{
    pCMPSCBLK->pic = 7;
    return FALSE;
}
static CMPSC_INLINE U8 (CMPSC_FASTCALL CC3)( CMPSCBLK* pCMPSCBLK )
{
    pCMPSCBLK->pic = 0;
    pCMPSCBLK->cc = 3;
    return TRUE;
}
static CMPSC_INLINE U8 (CMPSC_FASTCALL CC1)( CMPSCBLK* pCMPSCBLK )
{
    pCMPSCBLK->pic = 0;
    pCMPSCBLK->cc = 1;
    return TRUE;
}
static CMPSC_INLINE U8 (CMPSC_FASTCALL CC0)( CMPSCBLK* pCMPSCBLK )
{
    pCMPSCBLK->pic = 0;
    pCMPSCBLK->cc = 0;
    return TRUE;
}
//-----------------------------------------------------------------------------

static CMPSC_INLINE U8 (CMPSC_FASTCALL EXPOK)( CMPSCBLK* pCMPSCBLK, EXPBLK* pEXPBLK )
{
    UNREFERENCED( pCMPSCBLK );
    UNREFERENCED( pEXPBLK   );
    return TRUE; // (success; keep going)
}
static CMPSC_INLINE U8 (CMPSC_FASTCALL EXPERR)( CMPSCBLK* pCMPSCBLK, EXPBLK* pEXPBLK )
{
    pEXPBLK->rc = ERR( pCMPSCBLK ); return FALSE; // (break)
}
static CMPSC_INLINE U8 (CMPSC_FASTCALL EXPCC3)( CMPSCBLK* pCMPSCBLK, EXPBLK* pEXPBLK )
{
    pEXPBLK->rc = CC3( pCMPSCBLK ); return FALSE; // (break)
}
static CMPSC_INLINE U8 (CMPSC_FASTCALL EXPCC1)( CMPSCBLK* pCMPSCBLK, EXPBLK* pEXPBLK )
{
    pEXPBLK->rc = CC1( pCMPSCBLK ); return FALSE; // (break)
}
static CMPSC_INLINE U8 (CMPSC_FASTCALL EXPCC0)( CMPSCBLK* pCMPSCBLK, EXPBLK* pEXPBLK )
{
    pEXPBLK->rc = CC0( pCMPSCBLK ); return FALSE; // (break)
}
#endif // _CMPSC_C_         // End of compile ONLY ONCE code...
///////////////////////////////////////////////////////////////////////////////
#endif /* FEATURE_COMPRESSION */

#ifndef _GEN_ARCH
  #ifdef _ARCHMODE2
    #define _GEN_ARCH _ARCHMODE2
    #include "cmpsc_2012.c"
  #endif /* #ifdef _ARCHMODE2 */
  #ifdef _ARCHMODE3
    #undef _GEN_ARCH
    #define _GEN_ARCH _ARCHMODE3
    #include "cmpsc_2012.c"
  #endif /* #ifdef _ARCHMODE3 */

#if !defined( NOT_HERC )        // (building Hercules?)

HDL_DEPENDENCY_SECTION;
{
    HDL_DEPENDENCY( HERCULES );
    HDL_DEPENDENCY( REGS );
}
END_DEPENDENCY_SECTION;

HDL_INSTRUCTION_SECTION;
{
    char*    fn = NULL, info[256];
    if (!fn) fn = strrchr( __FILE__, '\\' );
    if (!fn) fn = strrchr( __FILE__ , '/' );
    if (!fn) fn =          __FILE__        ; else fn++;
#ifdef __TIMESTAMP__
    MSGBUF( info, "%s version %s last updated on %s", fn, VERSION, __TIMESTAMP__ );
#else
    MSGBUF( info, "%s version %s compiled on %s at %s", fn, VERSION, __DATE__, __TIME__ );
#endif
    WRMSG( HHC01417, "I", info );

    HDL_DEFINST( HDL_INSTARCH_390 | HDL_INSTARCH_900, 0xB263, alt_cmpsc );
}
END_INSTRUCTION_SECTION;

#endif // !defined( NOT_HERC )  // (building Hercules?)

#endif /* #ifndef _GEN_ARCH */

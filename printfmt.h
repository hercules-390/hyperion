/* PRINTFMT.H   (C) Copyright "Fish" (David B. Trout), 2013          */
/*              printf/sscanf format specifier strings               */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

#ifndef _PRINTFMT_H_
#define _PRINTFMT_H_

/*-------------------------------------------------------------------*/
/*  Length modifiers only                                            */
/*-------------------------------------------------------------------*/

#if defined( _MSVC_ )
#define  I16_FMT                    "h"         // length modifier only
#define  I32_FMT                    "I32"       // length modifier only
#define  I64_FMT                    "I64"       // length modifier only
#elif defined( __PRI_64_LENGTH_MODIFIER__ )     // (MAC)
#define  I16_FMT                    "h"         // length modifier only
#define  I32_FMT                    ""          // length modifier only
#define  I64_FMT                    __PRI_64_LENGTH_MODIFIER__
#elif defined( SIZEOF_LONG ) && SIZEOF_LONG >= 8
#define  I16_FMT                    "h"         // length modifier only
#define  I32_FMT                    ""          // length modifier only
#define  I64_FMT                    "l"         // length modifier only
#else
#define  I16_FMT                    "h"         // length modifier only
#define  I32_FMT                    ""          // length modifier only
#define  I64_FMT                    "ll"        // length modifier only
#endif

#if defined( SIZEOF_INT_P ) && SIZEOF_INT_P >= 8
#define  PTR_FMT                    I64_FMT     // length modifier only
#else
#define  PTR_FMT                    I32_FMT     // length modifier only
#endif

/*-------------------------------------------------------------------*/
/* Hercules fixed-width, zero padded hex values                      */
/*-------------------------------------------------------------------*/

#define  I16_FMTx           "%4.4" I16_FMT "x"  // complete format spec
#define  I32_FMTx           "%8.8" I32_FMT "x"  // complete format spec
#define  I64_FMTx         "%16.16" I64_FMT "x"  // complete format spec

#define  I16_FMTX           "%4.4" I16_FMT "X"  // complete format spec
#define  I32_FMTX           "%8.8" I32_FMT "X"  // complete format spec
#define  I64_FMTX         "%16.16" I64_FMT "X"  // complete format spec

/*-------------------------------------------------------------------*/
/* U16/U32/U64 typedef names might be easier to remember             */
/*-------------------------------------------------------------------*/

#define  U16_FMT                   I16_FMT      // length modifier only
#define  U32_FMT                   I32_FMT      // length modifier only
#define  U64_FMT                   I64_FMT      // length modifier only

#define  U16_FMTx                  I16_FMTx     // complete format spec
#define  U32_FMTx                  I32_FMTx     // complete format spec
#define  U64_FMTx                  I64_FMTx     // complete format spec

#define  U16_FMTX                  I16_FMTX     // complete format spec
#define  U32_FMTX                  I32_FMTX     // complete format spec
#define  U64_FMTX                  I64_FMTX     // complete format spec

/*-------------------------------------------------------------------*/
/* Hercules pointer format                                           */
/*-------------------------------------------------------------------*/

#if defined( SIZEOF_INT_P ) && SIZEOF_INT_P >= 8
 #define UINT_PTR_FMT              I64_FMT      // length modifier only
 #define      PTR_FMTx             I64_FMTx     // complete format spec
 #define      PTR_FMTX             I64_FMTX     // complete format spec
#else
 #define UINT_PTR_FMT              I32_FMT      // length modifier only
 #define      PTR_FMTx             I32_FMTx     // complete format spec
 #define      PTR_FMTX             I32_FMTX     // complete format spec
#endif

/*-------------------------------------------------------------------*/
/* Hercules size_t format                                            */
/*-------------------------------------------------------------------*/

#if defined( SIZEOF_SIZE_T ) && SIZEOF_SIZE_T >= 8
  #define  SIZE_T_FMT              I64_FMT      // length modifier only
  #define  SIZE_T_FMTx             I64_FMTx     // complete format spec
  #define  SIZE_T_FMTX             I64_FMTX     // complete format spec
#else
  #define  SIZE_T_FMT              I32_FMT      // length modifier only
  #define  SIZE_T_FMTx             I32_FMTx     // complete format spec
  #define  SIZE_T_FMTX             I32_FMTX     // complete format spec
#endif

/*-------------------------------------------------------------------*/
/* Hercules thread-id format                                         */
/*-------------------------------------------------------------------*/

#if defined( _MSVC_ )
  #define  TID_FMT                 U32_FMT      // length modifier only
  #define  TID_FMTx                U32_FMTx     // complete format spec
  #define  TID_FMTX                U32_FMTX     // complete format spec
#elif defined( SIZEOF_PTHREAD_T ) && SIZEOF_PTHREAD_T >= 8
  #define  TID_FMT                 U64_FMT      // length modifier only
  #define  TID_FMTx                U64_FMTx     // complete format spec
  #define  TID_FMTX                U64_FMTX     // complete format spec
#else
  #define  TID_FMT                 U32_FMT      // length modifier only
  #define  TID_FMTx                U32_FMTx     // complete format spec
  #define  TID_FMTX                U32_FMTX     // complete format spec
#endif

#endif // _PRINTFMT_H_

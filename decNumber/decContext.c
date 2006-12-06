/* ------------------------------------------------------------------ */
/* Decimal Context module                                             */
/* ------------------------------------------------------------------ */
/* Copyright (c) IBM Corporation, 2000, 2005.  All rights reserved.   */
/*                                                                    */
/* This software is made available under the terms of the             */
/* ICU License -- ICU 1.8.1 and later.                                */
/*                                                                    */
/* The description and User's Guide ("The decNumber C Library") for   */
/* this software is called decNumber.pdf.  This document is           */
/* available, together with arithmetic and format specifications,     */
/* testcases, and Web links, at: http://www2.hursley.ibm.com/decimal  */
/*                                                                    */
/* Please send comments, suggestions, and corrections to the author:  */
/*   mfc@uk.ibm.com                                                   */
/*   Mike Cowlishaw, IBM Fellow                                       */
/*   IBM UK, PO Box 31, Birmingham Road, Warwick CV34 5JL, UK         */
/* ------------------------------------------------------------------ */
/* This module comprises the routines for handling arithmetic         */
/* context structures.                                                */
/* ------------------------------------------------------------------ */

#include <string.h>           // for strcmp
#include "decContext.h"       // context and base types
#include "decNumberLocal.h"   // decNumber local types, etc.

/* ------------------------------------------------------------------ */
/* decContextDefault -- initialize a context structure                */
/*                                                                    */
/*  context is the structure to be initialized                        */
/*  kind selects the required set of default values, one of:          */
/*      DEC_INIT_BASE       -- select ANSI X3-274 defaults            */
/*      DEC_INIT_DECIMAL32  -- select IEEE 754r defaults, 32-bit      */
/*      DEC_INIT_DECIMAL64  -- select IEEE 754r defaults, 64-bit      */
/*      DEC_INIT_DECIMAL128 -- select IEEE 754r defaults, 128-bit     */
/*      For any other value a valid context is returned, but with     */
/*      Invalid_operation set in the status field.                    */
/*  returns a context structure with the appropriate initial values.  */
/* ------------------------------------------------------------------ */
decContext * decContextDefault(decContext *context, Int kind) {
  // set defaults...
  context->digits=9;                         // 9 digits
  context->emax=DEC_MAX_EMAX;                // 9-digit exponents
  context->emin=DEC_MIN_EMIN;                // .. balanced
  context->round=DEC_ROUND_HALF_UP;          // 0.5 rises
  context->traps=DEC_Errors;                 // all but informational
  context->status=0;                         // cleared
  context->clamp=0;                          // no clamping
  #if DECSUBSET
  context->extended=0;                       // cleared
  #endif
  switch (kind) {
    case DEC_INIT_BASE:
      // [use defaults]
      break;
    case DEC_INIT_DECIMAL32:
      context->digits=7;                     // digits
      context->emax=96;                      // Emax
      context->emin=-95;                     // Emin
      context->round=DEC_ROUND_HALF_EVEN;    // 0.5 to nearest even
      context->traps=0;                      // no traps set
      context->clamp=1;                      // clamp exponents
      #if DECSUBSET
      context->extended=1;                   // set
      #endif
      break;
    case DEC_INIT_DECIMAL64:
      context->digits=16;                    // digits
      context->emax=384;                     // Emax
      context->emin=-383;                    // Emin
      context->round=DEC_ROUND_HALF_EVEN;    // 0.5 to nearest even
      context->traps=0;                      // no traps set
      context->clamp=1;                      // clamp exponents
      #if DECSUBSET
      context->extended=1;                   // set
      #endif
      break;
    case DEC_INIT_DECIMAL128:
      context->digits=34;                    // digits
      context->emax=6144;                    // Emax
      context->emin=-6143;                   // Emin
      context->round=DEC_ROUND_HALF_EVEN;    // 0.5 to nearest even
      context->traps=0;                      // no traps set
      context->clamp=1;                      // clamp exponents
      #if DECSUBSET
      context->extended=1;                   // set
      #endif
      break;

    default:                                 // invalid Kind
      // use defaults, and ..
      decContextSetStatus(context, DEC_Invalid_operation); // trap
    }
  return context;} // decContextDefault

/* ------------------------------------------------------------------ */
/* decContextStatusToString -- convert status flags to a string       */
/*                                                                    */
/*  context is a context with valid status field                      */
/*                                                                    */
/*  returns a constant string describing the condition.  If multiple  */
/*    (or no) flags are set, a generic constant message is returned.  */
/* ------------------------------------------------------------------ */
const char *decContextStatusToString(const decContext *context) {
  Int status=context->status;
  if (status==DEC_Conversion_syntax    ) return DEC_Condition_CS;
  if (status==DEC_Division_by_zero     ) return DEC_Condition_DZ;
  if (status==DEC_Division_impossible  ) return DEC_Condition_DI;
  if (status==DEC_Division_undefined   ) return DEC_Condition_DU;
  if (status==DEC_Inexact              ) return DEC_Condition_IE;
  if (status==DEC_Insufficient_storage ) return DEC_Condition_IS;
  if (status==DEC_Invalid_context      ) return DEC_Condition_IC;
  if (status==DEC_Invalid_operation    ) return DEC_Condition_IO;
  #if DECSUBSET
  if (status==DEC_Lost_digits          ) return DEC_Condition_LD;
  #endif
  if (status==DEC_Overflow             ) return DEC_Condition_OV;
  if (status==DEC_Clamped              ) return DEC_Condition_PA;
  if (status==DEC_Rounded              ) return DEC_Condition_RO;
  if (status==DEC_Subnormal            ) return DEC_Condition_SU;
  if (status==DEC_Underflow            ) return DEC_Condition_UN;
  if (status==0                        ) return DEC_Condition_ZE;
  return DEC_Condition_MU;  // Multiple errors
  } // decContextStatusToString

/* ------------------------------------------------------------------ */
/* decContextSetStatusFromString -- set status from a string          */
/*                                                                    */
/*  context is the controlling context                                */
/*  string is a string exactly equal to one that might be returned    */
/*            by decContextStatusToString                             */
/*                                                                    */
/*  The status bit corresponding to the string is set, and a trap     */
/*  is raised if appropriate.                                         */
/*                                                                    */
/*  returns the context structure, unless the string is equal to      */
/*    DEC_Condition_MU or is not recognized.  In these cases NULL is  */
/*    returned.                                                       */
/* ------------------------------------------------------------------ */
decContext * decContextSetStatusFromString(decContext *context,
                                           const char *string) {
  if (strcmp(string, DEC_Condition_CS)==0)
    return decContextSetStatus(context, DEC_Conversion_syntax);
  if (strcmp(string, DEC_Condition_DZ)==0)
    return decContextSetStatus(context, DEC_Division_by_zero);
  if (strcmp(string, DEC_Condition_DI)==0)
    return decContextSetStatus(context, DEC_Division_impossible);
  if (strcmp(string, DEC_Condition_DU)==0)
    return decContextSetStatus(context, DEC_Division_undefined);
  if (strcmp(string, DEC_Condition_IE)==0)
    return decContextSetStatus(context, DEC_Inexact);
  if (strcmp(string, DEC_Condition_IS)==0)
    return decContextSetStatus(context, DEC_Insufficient_storage);
  if (strcmp(string, DEC_Condition_IC)==0)
    return decContextSetStatus(context, DEC_Invalid_context);
  if (strcmp(string, DEC_Condition_IO)==0)
    return decContextSetStatus(context, DEC_Invalid_operation);
  #if DECSUBSET
  if (strcmp(string, DEC_Condition_LD)==0)
    return decContextSetStatus(context, DEC_Lost_digits);
  #endif
  if (strcmp(string, DEC_Condition_OV)==0)
    return decContextSetStatus(context, DEC_Overflow);
  if (strcmp(string, DEC_Condition_PA)==0)
    return decContextSetStatus(context, DEC_Clamped);
  if (strcmp(string, DEC_Condition_RO)==0)
    return decContextSetStatus(context, DEC_Rounded);
  if (strcmp(string, DEC_Condition_SU)==0)
    return decContextSetStatus(context, DEC_Subnormal);
  if (strcmp(string, DEC_Condition_UN)==0)
    return decContextSetStatus(context, DEC_Underflow);
  if (strcmp(string, DEC_Condition_ZE)==0)
    return context;
  return NULL;  // Multiple status, or unknown
  } // decContextSetStatusFromString

/* ------------------------------------------------------------------ */
/* decContextSetStatus -- set status and raise trap if appropriate    */
/*                                                                    */
/*  context is the controlling context                                */
/*  status  is the DEC_ exception code                                */
/*  returns the context structure                                     */
/*                                                                    */
/* Control may never return from this routine, if there is a signal   */
/* handler and it takes a long jump.                                  */
/* ------------------------------------------------------------------ */
decContext * decContextSetStatus(decContext *context, uInt status) {
  context->status|=status;
  if (status & context->traps) raise(SIGFPE);
  return context;} // decContextSetStatus


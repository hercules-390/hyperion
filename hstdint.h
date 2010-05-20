/* HSTDINT.H    (c)Copyright Roger Bowler, 2006-2010                 */
/*              Hercules standard integer definitions                */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

/*-------------------------------------------------------------------*/
/* The purpose of this file is to define the following typedefs:     */
/*      uint8_t, uint16_t, uint32_t, uint64_t                        */
/*      int8_t, int16_t, int32_t, int64_t                            */
/* For Windows, the MSVC-specific C sized integer types are used.    */
/* For Unix, definitions are included from stdint.h if it exists,    */
/* otherwise from inttypes.h (if it exists), otherwise unistd.h      */
/*-------------------------------------------------------------------*/

#ifndef _HSTDINT_H
#define _HSTDINT_H

#ifdef HAVE_CONFIG_H
  #include <config.h>           /* Hercules build configuration      */
#endif

#if defined(_MSVC_)
  typedef unsigned __int8  uint8_t;
  typedef unsigned __int16 uint16_t;
  typedef unsigned __int32 uint32_t;
  typedef unsigned __int64 uint64_t;
  typedef signed __int8  int8_t;
  typedef signed __int16 int16_t;
  typedef signed __int32 int32_t;
  typedef signed __int64 int64_t;
#elif defined(HAVE_STDINT_H)
  #include <stdint.h>           /* Standard integer definitions      */
#elif defined(HAVE_INTTYPES_H)
  #include <inttypes.h>         /* Fixed size integral types         */
#else
  #include <unistd.h>           /* Unix standard definitions         */
#endif

#endif /*_HSTDINT_H*/

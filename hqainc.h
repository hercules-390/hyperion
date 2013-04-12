/* HQAINC.H     (C) "Fish" (David B. Trout), 2013                    */
/*              Include user override header HQA.H if needed         */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

#ifndef _HQAINC_H_
#define _HQAINC_H_

/*
**  The "hqa.h" header is an OPTIONAL header the user can create
**  that contains overrides for any of Hercules's default values
**  to provide for easier Quality Assurance testing of different
**  build configurations without the need to modify any Hercules
**  source code (header) files.
*/

#ifdef HAVE_HQA_H
  #include "hqa.h"                      /* user override to select   */
  #include "hqadefs.h"                  /* predefined QA scenarios   */
#endif

#endif /*_HQAINC_H_*/

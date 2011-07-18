/* HERCULES.H   (c) Copyright Roger Bowler, 1999-2011                */
/*              Hercules Header Files                                */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

#ifndef _HINLINES_H
#define _HINLINES_H

#if !defined(round_to_hostpagesize)
INLINE U64 round_to_hostpagesize(U64 n)
{
    register U64 factor = (U64)hostinfo.hostpagesz - 1;
    return ((n + factor) & ~factor);
}
#endif

#endif // _HINLINES_H

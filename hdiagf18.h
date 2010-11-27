/* HDIAGF18.H   (c) Copyright, Harold Grovesteen, 2010               */
/*              Hercules DIAGNOSE code X'F18'                        */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

#if !defined(__HDIAGF18_H__)
#define __HDIAGF18_H__

/*-------------------------------------------------------------------*/
/*  DIAGNOSE X'F18' Capability Parameter Block                       */
/*-------------------------------------------------------------------*/
typedef struct _CPB 
    {   
        BYTE    ipv6;         /* IPv6 Native Mode Socket Types       */
        BYTE    ipv4;         /* IPv4 Native Mode Socket Types       */
        BYTE    resources;    /* Resources and Modes Supported       */
        BYTE    platform;     /* Platform and Large File Support     */
        HWORD   comptopt;     /* Compatibility Operation Options     */
        HWORD   nativopt;     /* Native Operation Options            */
    } CPB;

/* Socket Type Masks */
#define STREAM_SOCKET   0x01
#define DATAGRAM_SOCKET 0x02

#endif /* !defined(__HDIAGF18_H__) */

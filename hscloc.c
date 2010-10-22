/* HSCLOC.C     (c) Copyright TurboHercules, SAS 2010                */
/*              locate debugging functions                           */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

#include "hstdinc.h"

#define _HSCLOC_C_

#include "hercules.h"

/*-------------------------------------------------------------------*/
/* locate - display control blocks by name                           */
/*-------------------------------------------------------------------*/
int locate_cmd(int argc, char *argv[], char *cmdline)
{
    int rc = 0;
    UNREFERENCED(argc);
    UNREFERENCED(argv);
    UNREFERENCED(cmdline);

    if (argc > 1 && CMD(argv[1],sysblk,6))
    {
        char    msgbuf[256];
        int     i, j, k, l, o;
        char    hbuf[128];
        char    cbuf[64];
        int     start = 0; 
        int     start_adj = 0;
        int     length = 512;
        BYTE    c;
        unsigned char   *tbl = (unsigned char *)&sysblk;

        if ( argc == 2 )
        {
            int ok = TRUE;
            U64 loc = swap_byte_U64(sysblk.blkloc);

            /* verify head, tail, length and address */
            if ( loc != (U64)&sysblk )
            {
                MSGBUF( msgbuf, "SYSBLK moved; was 0x"I64_FMTX", is 0x%p", loc, &sysblk );
                WRMSG( HHC90000, "D", msgbuf );
                ok = FALSE;
            }
            if ( swap_byte_U32(sysblk.blksiz) != (U32)sizeof(SYSBLK) )
            {
                MSGBUF( msgbuf, "SYSBLK size wrong; is %u, should be %u", swap_byte_U32(sysblk.blksiz), (U32)sizeof(SYSBLK));
                WRMSG( HHC90000, "D", msgbuf );
                ok = FALSE;
            }
            { /* verify header */
                char str[32];

                memset( str, SPACE, sizeof(str) );

                memcpy( str, HDL_NAME_SYSBLK, strlen(HDL_NAME_SYSBLK) );
                
                if ( memcmp( sysblk.blknam, str, sizeof(sysblk.blknam) ) != 0 )
                {
                    char sstr[32];

                    bzero( sstr,sizeof(sstr) );
                    memcpy( sstr, sysblk.blknam, sizeof(sysblk.blknam) );

                    MSGBUF( msgbuf, "SYSBLK header wrong; is %s, should be %s", sstr, str);
                    WRMSG( HHC90000, "D", msgbuf );
                    ok = FALSE;
                }
            }
            {   /* verify version */
                char str[32];

                memset( str, SPACE, sizeof(str) );
                memcpy( str, HDL_VERS_SYSBLK, strlen(HDL_VERS_SYSBLK) );

                if ( memcmp( sysblk.blkver, str, sizeof(sysblk.blkver) ) != 0 )
                {
                    char sstr[32];
                    bzero( sstr,sizeof(sstr) );
                    memcpy( sstr, sysblk.blkver, sizeof(sysblk.blkver) );

                    MSGBUF( msgbuf, "SYSBLK version wrong; is %s, should be %s", sstr, str);
                    WRMSG( HHC90000, "D", msgbuf );
                    ok = FALSE;
                }
            }
            {   /* verify trailer */
                char str[32];
                char trailer[32];
                
                MSGBUF( trailer, "END%13.13s", HDL_NAME_SYSBLK );
                memset( str, SPACE, sizeof(str) );
                memcpy( str, trailer, strlen(trailer) );
                
                if ( memcmp(sysblk.blkend, str, sizeof(sysblk.blkend)) != 0 )
                {
                    char sstr[32];
                    bzero( sstr, sizeof(sstr) );

                    memcpy( sstr, sysblk.blkend, sizeof(sysblk.blkend) );

                    MSGBUF( msgbuf, "SYSBLK trailer wrong; is %s, should be %s", sstr, trailer);
                    WRMSG( HHC90000, "D", msgbuf );
                    ok = FALSE;
                }
            }

            MSGBUF( msgbuf, "SYSBLK @ 0x%p - %sVerified", &sysblk, ok ? "" : "Not " );
            WRMSG( HHC90000, "D", msgbuf );
        }

        if ( argc > 2 )
        {
            /* start offset */
            int     x;
            BYTE    c;

            if ( sscanf(argv[2], "%x%c", &x, &c) != 1  )
            {
                return -1;
            }
            if ( x > (int)sizeof(SYSBLK) )
            {
                return -1;
            }
            start_adj = x % 16;
            start = x - ( x % 16 );      /* round to a 16 byte boundry */       
            if ( start + length + start_adj > (int)sizeof(SYSBLK) )
                length = (int)sizeof(SYSBLK) - start;
        }

        if ( argc > 3 )
        {
            /* length */
            int     x;
            BYTE    c;

            if ( sscanf(argv[3], "%x%c", &x, &c) != 1  )
            {
                return -1;
            }
            if ( x > 4096 )
            {
                return -1;
            }
            length = x;
        }
        length += start_adj;
        if ( start + length > (int)sizeof(SYSBLK) )
            length = (int)sizeof(SYSBLK) - start;

        l = length < 32 ? length : 32;
        for( o = start; o < (start+length); o += l )
        {
            bzero( hbuf, sizeof(hbuf) );
            bzero( cbuf, sizeof(cbuf) );
            
            for (i = 0, j = 0, k = 0; i < l; i++)
            {
                c = tbl[o+i];
                if ( (i & 0x3) == 0x0 ) hbuf[j++] = SPACE;
                if ( (i & 0xf) == 0x0 ) { hbuf[j++] = SPACE; cbuf[k++] = SPACE; }

                j += snprintf( hbuf+j, sizeof(hbuf)-j, "%2.2X", c );
                cbuf[k++] = ( !isprint(c) ? '.' : c );

            } /* end for(i) */
            MSGBUF( msgbuf, "sysblk+0x%04x%-73.73s %-33.33s", o, hbuf, cbuf );
            WRMSG( HHC90000, "D", msgbuf );
        }
    }
    return rc;
}

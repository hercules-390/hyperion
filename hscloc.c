/* HSCLOC.C     (c) Copyright TurboHercules, SAS 2010-2011           */
/*              locate debugging functions                           */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

#include "hstdinc.h"

#define _HSCLOC_C_
#define _HENGINE_DLL_

#include "hercules.h"

/*-------------------------------------------------------------------*/
/* fmt_memsize routine for qstor                                     */
/*-------------------------------------------------------------------*/
static char *fmt_memsize( const U64 memsize )
{
    // Mainframe memory and DASD amounts are reported in 2**(10*n)
    // values, (x_iB international format, and shown as x_ or x_B, when
    // x >= 1024; x when x < 1024). Open Systems and Windows report
    // memory in the same format, but report DASD storage in 10**(3*n)
    // values. (Thank you, various marketing groups and international
    // standards committees...)

    // For Hercules, mainframe oriented reporting characteristics will
    // be formatted and shown as x_, when x >= 1024, and as x when x <
    // 1024. Reporting of Open Systems and Windows specifics should
    // follow the international format, shown as x_iB, when x >= 1024,
    // and x or xB when x < 1024. Reporting is done at the highest
    // integral boundary.

    // Format storage in 2**(10*n) values at the highest integral
    // integer boundary.

    const  char suffix[9] = {0x00, 'K', 'M', 'G', 'T', 'P', 'E', 'Z', 'Y'};
    static char fmt_mem[128];    // Max of 21 bytes used for U64
    U64 mem = memsize;
    u_int   i = 0;

    if (mem)
        for (i = 0; i < sizeof(suffix); i++)
            {
/*                    if ( mem > ONE_YOBIBYTE )
                    mem &= 0xFFFFFFFFC0000000ULL;
                     if ( mem > ONE_ZEBIBYTE )
                    mem &= 0xFFFFFFFFC0000000ULL;
                else 
*/
                     if ( mem > ONE_EXBIBYTE )
                    mem &= 0xFFFC000000000000ULL;
                else if ( mem > ONE_PEBIBYTE )
                    mem &= 0xFFFFFF0000000000ULL;
                else if ( mem > ONE_TEBIBYTE )
                    mem &= 0xFFFFFFFFC0000000ULL;
                else if ( mem > ONE_GIBIBYTE )
                    mem &= 0xFFFFFFFFFFF00000ULL;
                else if ( mem > ONE_MEBIBYTE )
                    mem &= 0xFFFFFFFFFFFFFC00ULL;
                if (mem & 0x03FF) 
                    break;
                mem >>= 10;
            }

    MSGBUF( fmt_mem, "%5"I64_FMT"u %c", mem, suffix[i]);

    return fmt_mem;
}


static char *fmt_decimal( const U64 number )
{
    static  char    fmt_dec[64];
    double  num  = (double)number;
    BYTE    size;
    int     i;

    bzero(fmt_dec, sizeof(fmt_dec));

    if ( num > 0 )
    {
        if ( num >= (double)ONE_TRILLION )
        {
            num /= (double)ONE_TRILLION;
            size = 'T';
        }
        else if ( num >= ONE_BILLION )
        {
            num /= (double)ONE_BILLION;
            size = 'G';
        }
        else if ( num >= (double)ONE_MILLION )
        {
            num /= (double)ONE_MILLION;
            size = 'M';
        }
        else 
        {
            num /= (double)ONE_THOUSAND;
            size = 'K';
        }

        MSGBUF( fmt_dec, "%7.3f", num );
    
        i = (int)strlen(fmt_dec);

        if ( i > 0 )
        {
            for ( i--; i > 0; i-- )
            {
                if      ( fmt_dec[i] == '0' ) fmt_dec[i] = '\0';
                else if ( fmt_dec[i] == '.' ) { fmt_dec[i] = '\0'; break; }
                else break;
            }
        }

        fmt_dec[strlen(fmt_dec)] = '\0';
        fmt_dec[strlen(fmt_dec)+1] = '\0';
        fmt_dec[strlen(fmt_dec)+2] = '\0';
        fmt_dec[strlen(fmt_dec)] = ' ';
        fmt_dec[strlen(fmt_dec)] = size;
    }
    else
    {
        MSGBUF( fmt_dec, "%3d ", 0 );
        size = ' ';
    }

    return fmt_dec;
}

void fmt_line( unsigned char *tbl, char *name, int start, int length)
{
    int     i, j, k, l, o;
    char    hbuf[128];
    char    cbuf[64];
    char    fmtline[256];
    BYTE    c;

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
        MSGBUF( fmtline, "%s+0x%04x%-74.74s %-34.34s", name, o, hbuf, cbuf );
        WRMSG( HHC90000, "D", fmtline );
    }

}


/*-------------------------------------------------------------------*/
/* locate - display sysblk [offset [ length ] ]                      */
/*-------------------------------------------------------------------*/
int locate_sysblk(int argc, char *argv[], char *cmdline)
{
    int         rc = 0;
    char        msgbuf[256];
    int         start = 0; 
    int         start_adj = 0;
    int         length = 512;
    u_char     *tbl = (u_char *)&sysblk;

    UNREFERENCED(cmdline);

    if ( argc == 2 )
    {
        int ok = TRUE;
        U64 loc = swap_byte_U64(sysblk.blkloc);

        /* verify head, tail, length and address */
        if ( loc != (U64)((uintptr_t)&sysblk) )
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
        
    fmt_line( tbl, "sysblk", start, length);

    return rc;
}


/*-------------------------------------------------------------------*/
/* locate - display regs engine [offset [ length ] ] ]              */
/*-------------------------------------------------------------------*/
int locate_regs(int argc, char *argv[], char *cmdline)
{
    int         rc = 0;
    int         cpu = sysblk.pcpu;
    char        msgbuf[256];
    int         start = 0; 
    int         start_adj = 0;
    int         length = 512;
    REGS       *regs = sysblk.regs[cpu];
    u_char     *tbl = (u_char *)regs;

    UNREFERENCED(cmdline);
    
    if ( argc == 2 )
    {
        int ok = TRUE;
        U64 loc = swap_byte_U64(sysblk.regs[cpu]->blkloc);
        char hdr[32];
        char tlr[32];
        char blknam[32];

        MSGBUF( blknam, "%-4.4s_%2.2s%2.2X", HDL_NAME_REGS, PTYPSTR( cpu ), cpu );
        MSGBUF( hdr, "%-16.16s", blknam );
        MSGBUF( tlr, "END%13.13s", blknam );
        
        /* verify head, tail, length and address */
        if ( loc != (U64)((uintptr_t)regs) )
        {
            MSGBUF( msgbuf, "REGS[%2.2X] moved; was 0x"I64_FMTX", is 0x%p", 
                            cpu, loc, regs );
            WRMSG( HHC90000, "D", msgbuf );
            ok = FALSE;
        }
        if ( swap_byte_U32(sysblk.regs[cpu]->blksiz) != (U32)sizeof(REGS) )
        {
            MSGBUF( msgbuf, "REGS[%2.2X] size wrong; is %u, should be %u", 
                            cpu, 
                            swap_byte_U32(sysblk.regs[cpu]->blksiz), 
                            (U32)sizeof(REGS));
            WRMSG( HHC90000, "D", msgbuf );
            ok = FALSE;
        }
        { /* verify header */
            if ( memcmp( sysblk.regs[cpu]->blknam, 
                         hdr, 
                         sizeof(sysblk.regs[cpu]->blknam) ) != 0 )
            {
                char sstr[32];

                bzero( sstr, sizeof(sstr) );
                memcpy( sstr, sysblk.regs[cpu]->blknam, sizeof(sysblk.regs[cpu]->blknam) );

                MSGBUF( msgbuf, "REGS[%2.2X] header wrong; is %s, should be %s", 
                                cpu, sstr, hdr);
                WRMSG( HHC90000, "D", msgbuf );
                ok = FALSE;
            }
        }
        {   /* verify version */
            char str[32];

            memset( str, SPACE, sizeof(str) );
            memcpy( str, HDL_VERS_REGS, strlen(HDL_VERS_REGS) );

            if ( memcmp( sysblk.regs[cpu]->blkver, 
                         str, 
                         sizeof(sysblk.regs[cpu]->blkver) ) != 0 )
            {
                char sstr[32];
                bzero( sstr, sizeof(sstr) );
                memcpy( sstr, sysblk.regs[cpu]->blkver, sizeof(sysblk.regs[cpu]->blkver) );

                MSGBUF( msgbuf, "REGS[%2.2X] version wrong; is %s, should be %s", cpu, sstr, str);
                WRMSG( HHC90000, "D", msgbuf );
                ok = FALSE;
            }
        }
        {   /* verify trailer */
               
            if ( memcmp(sysblk.regs[cpu]->blkend, tlr, sizeof(sysblk.regs[cpu]->blkend)) != 0 )
            {
                char sstr[32];
                bzero( sstr, sizeof(sstr) );

                memcpy( sstr, sysblk.regs[cpu]->blkend, sizeof(sysblk.regs[cpu]->blkend) );

                MSGBUF( msgbuf, "REGS[%2.2X] trailer wrong; is %s, should be %s", 
                                cpu, sstr, tlr);
                WRMSG( HHC90000, "D", msgbuf );
                ok = FALSE;
            }
        }

        MSGBUF( msgbuf, "REGS[%2.2X] @ 0x%p - %sVerified", cpu, regs, ok ? "" : "Not " );
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
        if ( x > (int)sizeof(REGS) )
        {
            return -1;
        }
        start_adj = x % 16;
        start = x - ( x % 16 );      /* round to a 16 byte boundry */       
        if ( start + length + start_adj > (int)sizeof(REGS) )
            length = (int)sizeof(REGS) - start;
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
    if ( start + length > (int)sizeof(REGS) )
        length = (int)sizeof(REGS) - start;
        
    fmt_line( tbl, "regs", start, length);

    return rc;
}

/*-------------------------------------------------------------------*/
/* locate - display hostinfo block                                   */
/*-------------------------------------------------------------------*/
int locate_hostinfo(int argc, char *argv[], char *cmdline)
{
    int         rc = 0;
    char        msgbuf[256];
    HOST_INFO  *pHostInfo = &hostinfo;
    int         ok = TRUE;
    U64         loc = swap_byte_U64(hostinfo.blkloc);

    UNREFERENCED(argc);
    UNREFERENCED(argv);
    UNREFERENCED(cmdline);
    init_hostinfo(NULL);                    // refresh information
    
    /* verify head, tail, length and address */
        if ( loc != (U64)((uintptr_t)&hostinfo) )
        {
            MSGBUF( msgbuf, "HOSTINFO moved; was 0x"I64_FMTX", is 0x%p", loc, &hostinfo );
            WRMSG( HHC90000, "D", msgbuf );
            ok = FALSE;
        }
        if ( swap_byte_U32(hostinfo.blksiz) != (U32)sizeof(HOST_INFO) )
        {
            MSGBUF( msgbuf, "HOSTINFO size wrong; is %u, should be %u", swap_byte_U32(hostinfo.blksiz), (U32)sizeof(HOST_INFO));
            WRMSG( HHC90000, "D", msgbuf );
            ok = FALSE;
        }
        { /* verify header */
            char str[32];

            memset( str, SPACE, sizeof(str) );

            memcpy( str, HDL_NAME_HOST_INFO, strlen(HDL_NAME_HOST_INFO) );
                
            if ( memcmp( hostinfo.blknam, str, sizeof(hostinfo.blknam) ) != 0 )
            {
                char sstr[32];

                bzero( sstr,sizeof(sstr) );
                memcpy( sstr, hostinfo.blknam, sizeof(hostinfo.blknam) );

                MSGBUF( msgbuf, "HOSTINFO header wrong; is %s, should be %s", sstr, str);
                WRMSG( HHC90000, "D", msgbuf );
                ok = FALSE;
            }
        }
        {   /* verify version */
            char str[32];

            memset( str, SPACE, sizeof(str) );
            memcpy( str, HDL_VERS_HOST_INFO, strlen(HDL_VERS_HOST_INFO) );

            if ( memcmp( hostinfo.blkver, str, sizeof(hostinfo.blkver) ) != 0 )
            {
                char sstr[32];
                bzero( sstr,sizeof(sstr) );
                memcpy( sstr, hostinfo.blkver, sizeof(hostinfo.blkver) );

                MSGBUF( msgbuf, "HOSTINFO version wrong; is %s, should be %s", sstr, str);
                WRMSG( HHC90000, "D", msgbuf );
                ok = FALSE;
            }
        }
        {   /* verify trailer */
            char str[32];
            char trailer[32];
            
            MSGBUF( trailer, "END%13.13s", HDL_NAME_HOST_INFO );
            memset( str, SPACE, sizeof(str) );
            memcpy( str, trailer, strlen(trailer) );
               
            if ( memcmp(hostinfo.blkend, str, sizeof(hostinfo.blkend)) != 0 )
            {
                char sstr[32];
                bzero( sstr, sizeof(sstr) );

                memcpy( sstr, hostinfo.blkend, sizeof(hostinfo.blkend) );

                MSGBUF( msgbuf, "HOSTINFO trailer wrong; is %s, should be %s", sstr, trailer);
                WRMSG( HHC90000, "D", msgbuf );
                ok = FALSE;
            }
        }

        MSGBUF( msgbuf, "HOSTINFO @ 0x%p - %sVerified", &hostinfo, ok ? "" : "Not " );
        WRMSG( HHC90000, "D", msgbuf );

        MSGBUF( msgbuf, "%-17s = %s", "sysname", pHostInfo->sysname );
        WRMSG( HHC90000, "D", msgbuf );

        MSGBUF( msgbuf, "%-17s = %s", "nodename", pHostInfo->nodename );
        WRMSG( HHC90000, "D", msgbuf );

        MSGBUF( msgbuf, "%-17s = %s", "release", pHostInfo->release );
        WRMSG( HHC90000, "D", msgbuf );

        MSGBUF( msgbuf, "%-17s = %s", "version", pHostInfo->version );
        WRMSG( HHC90000, "D", msgbuf );

        MSGBUF( msgbuf, "%-17s = %s", "machine", pHostInfo->machine );
        WRMSG( HHC90000, "D", msgbuf );

        MSGBUF( msgbuf, "%-17s = %s", "cpu_brand", pHostInfo->cpu_brand );
        WRMSG( HHC90000, "D", msgbuf );

        MSGBUF( msgbuf, "%-17s = %s", "trycritsec_avail", pHostInfo->trycritsec_avail ? "YES" : "NO" );
        WRMSG( HHC90000, "D", msgbuf );

        MSGBUF( msgbuf, "%-17s = %d", "maxfilesopen", pHostInfo->maxfilesopen );
        WRMSG( HHC90000, "D", msgbuf );

        WRMSG( HHC90000, "D", "" );

        MSGBUF( msgbuf, "%-17s = %3d", "num_procs", pHostInfo->num_procs );
        WRMSG( HHC90000, "D", msgbuf );

        MSGBUF( msgbuf, "%-17s = %3d", "num_packages", pHostInfo->num_packages );
        WRMSG( HHC90000, "D", msgbuf );

        MSGBUF( msgbuf, "%-17s = %3d", "num_physical_cpu", pHostInfo->num_physical_cpu );
        WRMSG( HHC90000, "D", msgbuf );

        MSGBUF( msgbuf, "%-17s = %3d", "num_logical_cpu", pHostInfo->num_logical_cpu );
        WRMSG( HHC90000, "D", msgbuf );

        MSGBUF( msgbuf, "%-17s = %sHz", "bus_speed", fmt_decimal((U64)pHostInfo->bus_speed) );
        WRMSG( HHC90000, "D", msgbuf );

        MSGBUF( msgbuf, "%-17s = %sHz", "cpu_speed", fmt_decimal((U64)pHostInfo->cpu_speed) );
        WRMSG( HHC90000, "D", msgbuf );

        MSGBUF( msgbuf, "%-17s = %s", "vector_unit", pHostInfo->vector_unit ? "YES" : " NO" );
        WRMSG( HHC90000, "D", msgbuf );

        MSGBUF( msgbuf, "%-17s = %s", "fp_unit", pHostInfo->fp_unit ? "YES" : " NO" );
        WRMSG( HHC90000, "D", msgbuf );

        MSGBUF( msgbuf, "%-17s = %s", "cpu_64bits", pHostInfo->cpu_64bits ? "YES" : " NO" );
        WRMSG( HHC90000, "D", msgbuf );

        MSGBUF( msgbuf, "%-17s = %s", "cpu_aes_extns", pHostInfo->cpu_aes_extns ? "YES" : " NO" );
        WRMSG( HHC90000, "D", msgbuf );

        WRMSG( HHC90000, "D", "" );

        MSGBUF( msgbuf, "%-17s = %s", "valid_cache_nums", pHostInfo->valid_cache_nums ? "YES" : " NO" );
        WRMSG( HHC90000, "D", msgbuf );

        MSGBUF( msgbuf, "%-17s = %5"FRADR"u B", "cachelinesz", pHostInfo->cachelinesz );
        WRMSG( HHC90000, "D", msgbuf );

        if ( pHostInfo->L1Dcachesz != 0 )
        {
            MSGBUF( msgbuf, "%-17s = %siB", "L1Dcachesz", fmt_memsize((U64)pHostInfo->L1Dcachesz) );
            WRMSG( HHC90000, "D", msgbuf );
        }

        if ( pHostInfo->L1Icachesz != 0 )
        {
            MSGBUF( msgbuf, "%-17s = %siB", "L1Icachesz", fmt_memsize((U64)pHostInfo->L1Icachesz) );
            WRMSG( HHC90000, "D", msgbuf );
        }

        if ( pHostInfo->L1Ucachesz != 0 )
        {
            MSGBUF( msgbuf, "%-17s = %siB", "L1Ucachesz", fmt_memsize((U64)pHostInfo->L1Ucachesz) );
            WRMSG( HHC90000, "D", msgbuf );
        }

        MSGBUF( msgbuf, "%-17s = %siB", "L2cachesz", fmt_memsize((U64)pHostInfo->L2cachesz) );
        WRMSG( HHC90000, "D", msgbuf );

        MSGBUF( msgbuf, "%-17s = %siB", "L3cachesz", fmt_memsize((U64)pHostInfo->L3cachesz) );
        WRMSG( HHC90000, "D", msgbuf );

        WRMSG( HHC90000, "D", "" );

        MSGBUF( msgbuf, "%-17s = %siB", "hostpagesz", fmt_memsize((U64)pHostInfo->hostpagesz) );
        WRMSG( HHC90000, "D", msgbuf );

        MSGBUF( msgbuf, "%-17s = %siB", "AllocGran", fmt_memsize((U64)pHostInfo->AllocationGranularity) );
        WRMSG( HHC90000, "D", msgbuf );

        WRMSG( HHC90000, "D", "" );

        MSGBUF( msgbuf, "%-17s = %siB", "TotalPhys", fmt_memsize((U64)pHostInfo->TotalPhys) );
        WRMSG( HHC90000, "D", msgbuf );

        MSGBUF( msgbuf, "%-17s = %siB", "AvailPhys", fmt_memsize((U64)pHostInfo->AvailPhys) );
        WRMSG( HHC90000, "D", msgbuf );

        MSGBUF( msgbuf, "%-17s = %siB", "TotalPageFile", fmt_memsize((U64)pHostInfo->TotalPageFile) );
        WRMSG( HHC90000, "D", msgbuf );

        MSGBUF( msgbuf, "%-17s = %siB", "AvailPageFile", fmt_memsize((U64)pHostInfo->AvailPageFile) );
        WRMSG( HHC90000, "D", msgbuf );

        MSGBUF( msgbuf, "%-17s = %siB", "TotalVirtual", fmt_memsize((U64)pHostInfo->TotalVirtual) );
        WRMSG( HHC90000, "D", msgbuf );

        MSGBUF( msgbuf, "%-17s = %siB", "AvailVirtual", fmt_memsize((U64)pHostInfo->AvailVirtual) );
        WRMSG( HHC90000, "D", msgbuf );

    return rc;
}

/*-------------------------------------------------------------------*/
/* locate   display control blocks by name                           */
/*-------------------------------------------------------------------*/
int locate_cmd(int argc, char *argv[], char *cmdline)
{
    int rc = 0;

    UNREFERENCED(cmdline);

    if (argc > 1 && CMD(argv[1],sysblk,6))
    {
        rc = locate_sysblk(argc, argv, cmdline);
    }
    else 
    if (argc > 1 && CMD(argv[1],regs,4))
    {
        rc = locate_regs(argc, argv, cmdline);
    }
    else 
    if (argc > 1 && CMD(argv[1],hostinfo,4))
    {
        rc = locate_hostinfo(argc, argv, cmdline);
    }
    else
    {
        WRMSG( HHC02299, "E", argv[0] );
        rc = -1;
    }

    return rc;
}
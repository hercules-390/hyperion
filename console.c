/* CONSOLE.C    (c)Copyright Roger Bowler, 1999-2002                 */
/*              ESA/390 Console Device Handler                       */

/*-------------------------------------------------------------------*/
/* This module contains device handling functions for console        */
/* devices for the Hercules ESA/390 emulator.                        */
/*                                                                   */
/* Telnet support is provided for two classes of console device:     */
/* - local non-SNA 3270 display consoles via tn3270                  */
/* - local non-SNA 3270 printers         via tn3270                  */
/* - 1052 and 3215 console printer keyboards via regular telnet      */
/*-------------------------------------------------------------------*/

/*-------------------------------------------------------------------*/
/* This module also takes care of the differences between the        */
/* remote 3270 and local non-SNA 3270 devices.  In particular        */
/* the support of command chaining, which is not supported on        */
/* the remote 3270 implementation on which telnet 3270 is based.     */
/* In the local non-SNA environment a chained read or write will     */
/* continue at the buffer address where the previous command ended.  */
/* In order to achieve this, this module will keep track of the      */
/* buffer location, and adjust the buffer address on chained read    */
/* and write operations.                                             */
/*                                           03/06/00 Jan Jaeger.    */
/*-------------------------------------------------------------------*/

/*-------------------------------------------------------------------*/
/* Add code to bypass bug in telnet client from TCP/IP for OS/390    */
/* where telnet responds with the server response rather then the    */
/* client response as documented in RFC1576.                         */
/*                                           20/06/00 Jan Jaeger.    */
/*-------------------------------------------------------------------*/

/*-------------------------------------------------------------------*/
/* Corrections to buffer position calculations in find_buffer_pos    */
/* and get_screen_pos subroutines (symptom: message IEE305I)         */
/*                                           09/12/00 Roger Bowler.  */
/*-------------------------------------------------------------------*/

/*-------------------------------------------------------------------*/
/* When using VTAM with local non-SNA 3270 devices, ensure that      */
/* enough bufferspace is available when doing IND$FILE type          */
/* filetransfers.  Code IOBUF=(,3992) in ATCSTRxx, and/or BUFNUM=xxx */
/* on the LBUILD LOCAL statement defining the 3270 device.    JJ     */
/*                                                                   */
/*-------------------------------------------------------------------*/

#include "hercules.h"

#include "devtype.h"

#include "opcode.h"

/*-------------------------------------------------------------------*/
/* Telnet command definitions                                        */
/*-------------------------------------------------------------------*/
#define BINARY          0       /* Binary Transmission */
#define IS              0       /* Used by terminal-type negotiation */
#define SEND            1       /* Used by terminal-type negotiation */
#define ECHO_OPTION     1       /* Echo option */
#define SUPPRESS_GA     3       /* Suppress go-ahead option */
#define TIMING_MARK     6       /* Timing mark option */
#define TERMINAL_TYPE   24      /* Terminal type option */
#define EOR             25      /* End of record option */
#define EOR_MARK        239     /* End of record marker */
#define SE              240     /* End of subnegotiation parameters */
#define NOP             241     /* No operation */
#define DATA_MARK       242     /* The data stream portion of a Synch.
                                   This should always be accompanied
                                   by a TCP Urgent notification */
#define BRK             243     /* Break character */
#define IP              244     /* Interrupt Process */
#define AO              245     /* Abort Output */
#define AYT             246     /* Are You There */
#define EC              247     /* Erase character */
#define EL              248     /* Erase Line */
#define GA              249     /* Go ahead */
#define SB              250     /* Subnegotiation of indicated option */
#define WILL            251     /* Indicates the desire to begin
                                   performing, or confirmation that
                                   you are now performing, the
                                   indicated option */
#define WONT            252     /* Indicates the refusal to perform,
                                   or continue performing, the
                                   indicated option */
#define DO              253     /* Indicates the request that the
                                   other party perform, or
                                   confirmation that you are expecting
                                   the other party to perform, the
                                   indicated option */
#define DONT            254     /* Indicates the demand that the
                                   other party stop performing,
                                   or confirmation that you are no
                                   longer expecting the other party
                                   to perform, the indicated option */
#define IAC             255     /* Interpret as Command */

/*-------------------------------------------------------------------*/
/* 3270 definitions                                                  */
/*-------------------------------------------------------------------*/

/* 3270 local commands (CCWs) */
#define L3270_EAU       0x0F            /* Erase All Unprotected     */
#define L3270_EW        0x05            /* Erase/Write               */
#define L3270_EWA       0x0D            /* Erase/Write Alternate     */
#define L3270_RB        0x02            /* Read Buffer               */
#define L3270_RM        0x06            /* Read Modified             */
#define L3270_WRT       0x01            /* Write                     */
#define L3270_WSF       0x11            /* Write Structured Field    */

#define L3270_NOP       0x03            /* No Operation              */
#define L3270_SELRM     0x0B            /* Select RM                 */
#define L3270_SELRB     0x1B            /* Select RB                 */
#define L3270_SELRMP    0x2B            /* Select RMP                */
#define L3270_SELRBP    0x3B            /* Select RBP                */
#define L3270_SELWRT    0x4B            /* Select WRT                */
#define L3270_SENSE     0x04            /* Sense                     */
#define L3270_SENSEID   0xE4            /* Sense ID                  */

/* 3270 remote commands */
#define R3270_EAU       0x6F            /* Erase All Unprotected     */
#define R3270_EW        0xF5            /* Erase/Write               */
#define R3270_EWA       0x7E            /* Erase/Write Alternate     */
#define R3270_RB        0xF2            /* Read Buffer               */
#define R3270_RM        0xF6            /* Read Modified             */
#define R3270_RMA       0x6E            /* Read Modified All         */
#define R3270_WRT       0xF1            /* Write                     */
#define R3270_WSF       0xF3            /* Write Structured Field    */

/* 3270 orders */
#define O3270_SBA       0x11            /* Set Buffer Address        */
#define O3270_SF        0x1D            /* Start Field               */
#define O3270_SFE       0x29            /* Start Field Extended      */
#define O3270_SA        0x28            /* Set Attribute             */
#define O3270_IC        0x13            /* Insert Cursor             */
#define O3270_MF        0x2C            /* Modify Field              */
#define O3270_PT        0x05            /* Program Tab               */
#define O3270_RA        0x3C            /* Repeat to Address         */
#define O3270_EUA       0x12            /* Erase Unprotected to Addr */
#define O3270_GE        0x08            /* Graphic Escape            */

/* Inbound structured fields */
#define SF3270_AID      0x88            /* Aid value of inbound SF   */
#define SF3270_3270DS   0x80            /* SFID of 3270 datastream SF*/

/* 12 bit 3270 buffer address code conversion table                  */
static BYTE sba_code[] = { "\x40\xC1\xC2\xC3\xC4\xC5\xC6\xC7"
                           "\xC8\xC9\x4A\x4B\x4C\x4D\x4E\x4F"
                           "\x50\xD1\xD2\xD3\xD4\xD5\xD6\xD7"
                           "\xD8\xD9\x5A\x5B\x5C\x5D\x5E\x5F"
                           "\x60\x61\xE2\xE3\xE4\xE5\xE6\xE7"
                           "\xE8\xE9\x6A\x6B\x6C\x6D\x6E\x6F"
                           "\xF0\xF1\xF2\xF3\xF4\xF5\xF6\xF7"
                           "\xF8\xF9\x7A\x7B\x7C\x7D\x7E\x7F" };

/*-------------------------------------------------------------------*/
/* Internal macro definitions                                        */
/*-------------------------------------------------------------------*/
#define DEBUG_LVL       0               /* 1 = status
                                           2 = headers
                                           3 = buffers               */
#if DEBUG_LVL == 0
 #define TNSDEBUG1(_format, _args...)
 #define TNSDEBUG2(_format, _args...)
 #define TNSDEBUG3(_format, _args...)
#endif
#if DEBUG_LVL == 1
#define TNSDEBUG1(_format, _args...) \
        logmsg("console: " _format, ## _args)
#define TNSDEBUG2(_format, _args...)
#define TNSDEBUG3(_format, _args...)
#endif
#if DEBUG_LVL == 2
#define TNSDEBUG1(_format, _args...) \
        logmsg("console: " _format, ## _args)
#define TNSDEBUG2(_format, _args...) \
        logmsg("console: " _format, ## _args) 
#define TNSDEBUG3(_format, _args...) 
#endif
#if DEBUG_LVL == 3
#define TNSDEBUG1(_format, _args...) \
        logmsg("console: " _format, ## _args)
#define TNSDEBUG2(_format, _args...) \
        logmsg("console: " _format, ## _args)
#define TNSDEBUG3(_format, _args...) \
        logmsg("console: " _format, ## _args)
#endif

#define TNSERROR(_format,_args...) \
        logmsg("console: " _format, ## _args)

#define BUFLEN_3270     65536           /* 3270 Send/Receive buffer  */
#define BUFLEN_1052     150             /* 1052 Send/Receive buffer  */
#define SPACE           ((BYTE)' ')


#undef  FIX_QWS_BUG_FOR_MCS_CONSOLES

/*-------------------------------------------------------------------*/
/* Static data areas                                                 */
/*-------------------------------------------------------------------*/
static struct utsname hostinfo;         /* Host info for this system */


/*-------------------------------------------------------------------*/
/* SUBROUTINE TO TRACE THE CONTENTS OF AN ASCII MESSAGE PACKET       */
/*-------------------------------------------------------------------*/
#if DEBUG_LVL == 3
static void
packet_trace(BYTE *addr, int len)
{
unsigned int  i, offset;
unsigned char c;
unsigned char print_chars[17];

    for (offset=0; offset < len; )
    {
        memset(print_chars,0,sizeof(print_chars));
        logmsg("+%4.4X  ", offset);
        for (i=0; i < 16; i++)
        {
            c = *addr++;
            if (offset < len) {
                logmsg("%2.2X", c);
                print_chars[i] = '.';
                if (isprint(c)) print_chars[i] = c;
                c = guest_to_host(c);
                if (isprint(c)) print_chars[i] = c;
            }
            else {
                logmsg("  ");
            }
            offset++;
            if ((offset & 3) == 0) {
                logmsg(" ");
            }
        } /* end for(i) */
        logmsg(" %s\n", print_chars);
    } /* end for(offset) */

} /* end function packet_trace */
#else
 #define packet_trace( _addr, _len)
#endif


/*-------------------------------------------------------------------*/
/* SUBROUTINE TO REMOVE ANY IAC SEQUENCES FROM THE DATA STREAM       */
/* Returns the new length after deleting IAC commands                */
/*-------------------------------------------------------------------*/
static int
remove_iac (BYTE *buf, int len)
{
int     m, n, c;

    for (m=0, n=0; m < len; ) {
        /* Interpret IAC commands */
        if (buf[m] == IAC) {
            /* Treat IAC in last byte of buffer as IAC NOP */
            c = (++m < len)? buf[m++] : NOP;
            /* Process IAC command */
            switch (c) {
            case IAC: /* Insert single IAC in buffer */
                buf[n++] = IAC;
                break;
            case BRK: /* Set ATTN indicator */
                break;
            case IP: /* Set SYSREQ indicator */
                break;
            case WILL: /* Skip option negotiation command */
            case WONT:
            case DO:
            case DONT:
                m++;
                break;
            case SB: /* Skip until IAC SE sequence found */
                for (; m < len; m++) {
                    if (buf[m] != IAC) continue;
                    if (++m >= len) break;
                    if (buf[m] == SE) { m++; break; }
                } /* end for */
            default: /* Ignore NOP or unknown command */
                break;
            } /* end switch(c) */
        } else {
            /* Copy data bytes */
            if (n < m) buf[n] = buf[m];
            m++; n++;
        }
    } /* end for */

    if (n < m) {
        TNSDEBUG3("%d IAC bytes removed, newlen=%d\n", m-n, n);
        packet_trace (buf, n);
    }

    return n;

} /* end function remove_iac */


/*-------------------------------------------------------------------*/
/* SUBROUTINE TO DOUBLE UP ANY IAC BYTES IN THE DATA STREAM          */
/* Returns the new length after inserting extra IAC bytes            */
/*-------------------------------------------------------------------*/
static int
double_up_iac (BYTE *buf, int len)
{
int     m, n, x, newlen;

    /* Count the number of IAC bytes in the data */
    for (x=0, n=0; n < len; n++)
        if (buf[n] == IAC) x++;

    /* Exit if nothing to do */
    if (x == 0) return len;

    /* Insert extra IAC bytes backwards from the end of the buffer */
    newlen = len + x;
    TNSDEBUG3("%d IAC bytes added, newlen=%d\n", x, newlen);
    for (n=newlen, m=len; n > m; ) {
        buf[--n] = buf[--m];
        if (buf[n] == IAC) buf[--n] = IAC;
    }
    packet_trace (buf, newlen);
    return newlen;

} /* end function double_up_iac */


/*-------------------------------------------------------------------*/
/* SUBROUTINE TO TRANSLATE A NULL-TERMINATED STRING TO EBCDIC        */
/*-------------------------------------------------------------------*/
static BYTE *
translate_to_ebcdic (BYTE *str)
{
int     i;                              /* Array subscript           */
BYTE    c;                              /* Character work area       */

    for (i = 0; str[i] != '\0'; i++)
    {
        c = str[i];
        str[i] = (isprint(c) ? host_to_guest(c) : SPACE);
    }

    return str;
} /* end function translate_to_ebcdic */


/*-------------------------------------------------------------------*/
/* SUBROUTINE TO SEND A DATA PACKET TO THE CLIENT                    */
/*-------------------------------------------------------------------*/
static int
send_packet (int csock, BYTE *buf, int len, char *caption)
{
int     rc;                             /* Return code               */

    if (caption != NULL) {
        TNSDEBUG2("Sending %s\n", caption);
        packet_trace (buf, len);
    }

    rc = send (csock, buf, len, 0);

    if (rc < 0) {
        TNSERROR("send: %s\n", strerror(errno));
        return -1;
    } /* end if(rc) */

    return 0;

} /* end function send_packet */


/*-------------------------------------------------------------------*/
/* SUBROUTINE TO RECEIVE A DATA PACKET FROM THE CLIENT               */
/* This subroutine receives bytes from the client.  It stops when    */
/* the receive buffer is full, or when the last two bytes received   */
/* consist of the IAC character followed by a specified delimiter.   */
/* If zero bytes are received, this means the client has closed the  */
/* connection, and this is treated as an error.                      */
/* Input:                                                            */
/*      csock is the socket number                                   */
/*      buf points to area to receive data                           */
/*      reqlen is the number of bytes requested                      */
/*      delim is the delimiter character (0=no delimiter)            */
/* Output:                                                           */
/*      buf is updated with data received                            */
/*      The return value is the number of bytes received, or         */
/*      -1 if an error occurred.                                     */
/*-------------------------------------------------------------------*/
static int
recv_packet (int csock, BYTE *buf, int reqlen, BYTE delim)
{
int     rc=0;                           /* Return code               */
int     rcvlen=0;                       /* Length of data received   */

    while (rcvlen < reqlen) {

        rc = recv (csock, buf + rcvlen, reqlen - rcvlen, 0);

        if (rc < 0) {
            TNSERROR("recv: %s\n", strerror(errno));
            return -1;
        }

        if (rc == 0) {
            TNSDEBUG1("Connection closed by client\n");
            return -1;
        }

        rcvlen += rc;

        if (delim != '\0' && rcvlen >= 2
            && buf[rcvlen-2] == IAC && buf[rcvlen-1] == delim)
            break;
    }

    TNSDEBUG2("Packet received length=%d\n", rcvlen);
    packet_trace (buf, rcvlen);

    return rcvlen;

} /* end function recv_packet */


/*-------------------------------------------------------------------*/
/* SUBROUTINE TO RECEIVE A PACKET AND COMPARE WITH EXPECTED VALUE    */
/*-------------------------------------------------------------------*/
static int
expect (int csock, BYTE *expected, int len, char *caption)
{
int     rc;                             /* Return code               */
BYTE    buf[512];                       /* Receive buffer            */
#if 1
/* TCP/IP for MVS returns the server sequence rather then 
   the client sequence during bin negotiation    19/06/00 Jan Jaeger */
static BYTE do_bin[] = { IAC, DO, BINARY, IAC, WILL, BINARY };
static BYTE will_bin[] = { IAC, WILL, BINARY, IAC, DO, BINARY };
#endif

    UNREFERENCED(caption);

    rc = recv_packet (csock, buf, len, 0);
    if (rc < 0) return -1;

#if 1
        /* TCP/IP FOR MVS DOES NOT COMPLY TO RFC 1576 THIS IS A BYPASS */
        if(memcmp(buf, expected, len) != 0
          && !(len == sizeof(will_bin) 
              && memcmp(expected, will_bin, len) == 0
              && memcmp(buf, do_bin, len) == 0) )
#else 
    if (memcmp(buf, expected, len) != 0)
#endif
    {
        TNSDEBUG2("Expected %s\n", caption);
        return -1;
    }
    TNSDEBUG2("Received %s\n", caption);

    return 0;

} /* end function expect */


/*-------------------------------------------------------------------*/
/* SUBROUTINE TO NEGOTIATE TELNET PARAMETERS                         */
/* This subroutine negotiates the terminal type with the client      */
/* and uses the terminal type to determine whether the client        */
/* is to be supported as a 3270 display console or as a 1052/3215    */
/* printer-keyboard console.                                         */
/*                                                                   */
/* Valid display terminal types are "IBM-NNNN", "IBM-NNNN-M", and    */
/* "IBM-NNNN-M-E", where NNNN is 3270, 3277, 3278, 3279, 3178, 3179, */
/* or 3180, M indicates the screen size (2=25x80, 3=32x80, 4=43x80,  */
/* 5=27x132, X=determined by Read Partition Query command), and      */
/* -E is an optional suffix indicating that the terminal supports    */
/* extended attributes. Displays are negotiated into tn3270 mode.    */
/* An optional device number suffix (example: IBM-3270@01F) may      */
/* be specified to request allocation to a specific device number.   */
/* Valid 3270 printer type is "IBM-3287-1"                           */
/*                                                                   */
/* Terminal types whose first four characters are not "IBM-" are     */
/* handled as printer-keyboard consoles using telnet line mode.      */
/*                                                                   */
/* Input:                                                            */
/*      csock   Socket number for client connection                  */
/* Output:                                                           */
/*      class   D=3270 display console, K=printer-keyboard console   */
/*              P=3270 printer                                       */
/*      model   3270 model indicator (2,3,4,5,X)                     */
/*      extatr  3270 extended attributes (Y,N)                       */
/*      devn    Requested device number, or FFFF=any device number   */
/* Return value:                                                     */
/*      0=negotiation successful, -1=negotiation error               */
/*-------------------------------------------------------------------*/
static int
negotiate(int csock, BYTE *class, BYTE *model, BYTE *extatr, U16 *devn)
{
int    rc;                              /* Return code               */
BYTE  *termtype;                        /* Pointer to terminal type  */
BYTE  *s;                               /* String pointer            */
U16    devnum;                          /* Requested device number   */
BYTE   buf[512];                        /* Telnet negotiation buffer */
static BYTE do_term[] = { IAC, DO, TERMINAL_TYPE };
static BYTE will_term[] = { IAC, WILL, TERMINAL_TYPE };
static BYTE req_type[] = { IAC, SB, TERMINAL_TYPE, SEND, IAC, SE };
static BYTE type_is[] = { IAC, SB, TERMINAL_TYPE, IS };
static BYTE do_eor[] = { IAC, DO, EOR, IAC, WILL, EOR };
static BYTE will_eor[] = { IAC, WILL, EOR, IAC, DO, EOR };
static BYTE do_bin[] = { IAC, DO, BINARY, IAC, WILL, BINARY };
static BYTE will_bin[] = { IAC, WILL, BINARY, IAC, DO, BINARY };
#if 0
static BYTE do_tmark[] = { IAC, DO, TIMING_MARK };
static BYTE will_tmark[] = { IAC, WILL, TIMING_MARK };
static BYTE wont_sga[] = { IAC, WONT, SUPPRESS_GA };
static BYTE dont_sga[] = { IAC, DONT, SUPPRESS_GA };
#endif
static BYTE wont_echo[] = { IAC, WONT, ECHO_OPTION };
static BYTE dont_echo[] = { IAC, DONT, ECHO_OPTION };

    /* Perform terminal-type negotiation */
    rc = send_packet (csock, do_term, sizeof(do_term),
                        "IAC DO TERMINAL_TYPE");
    if (rc < 0) return -1;

    rc = expect (csock, will_term, sizeof(will_term),
                        "IAC WILL TERMINAL_TYPE");
    if (rc < 0) return -1;

    /* Request terminal type */
    rc = send_packet (csock, req_type, sizeof(req_type),
                        "IAC SB TERMINAL_TYPE SEND IAC SE");
    if (rc < 0) return -1;

    rc = recv_packet (csock, buf, sizeof(buf)-2, SE);
    if (rc < 0) return -1;

    if (rc < (int)(sizeof(type_is) + 2)
        || memcmp(buf, type_is, sizeof(type_is)) != 0
        || buf[rc-2] != IAC || buf[rc-1] != SE) {
        TNSDEBUG2("Expected IAC SB TERMINAL_TYPE IS\n");
        return -1;
    }
    buf[rc-2] = '\0';
    termtype = buf + sizeof(type_is);
    TNSDEBUG2("Received IAC SB TERMINAL_TYPE IS %s IAC SE\n",
            termtype);

    /* Check terminal type string for device name suffix */
    s = strchr (termtype, '@');
    if (s != NULL && sscanf (s, "@%hx", &devnum) == 1)
        *devn = devnum;
    else
        *devn = 0xFFFF;

    /* Test for non-display terminal type */
    if (memcmp(termtype, "IBM-", 4) != 0)
    {
#if 0
        /* Perform line mode negotiation */
        rc = send_packet (csock, do_tmark, sizeof(do_tmark),
                            "IAC DO TIMING_MARK");
        if (rc < 0) return -1;

        rc = expect (csock, will_tmark, sizeof(will_tmark),
                            "IAC WILL TIMING_MARK");
        if (rc < 0) return 0;

        rc = send_packet (csock, wont_sga, sizeof(wont_sga),
                            "IAC WONT SUPPRESS_GA");
        if (rc < 0) return -1;

        rc = expect (csock, dont_sga, sizeof(dont_sga),
                            "IAC DONT SUPPRESS_GA");
        if (rc < 0) return -1;
#endif

        if (memcmp(termtype, "ANSI", 4) == 0)
        {
            rc = send_packet (csock, wont_echo, sizeof(wont_echo),
                                "IAC WONT ECHO");
            if (rc < 0) return -1;

            rc = expect (csock, dont_echo, sizeof(dont_echo),
                                "IAC DONT ECHO");
            if (rc < 0) return -1;
        }

        /* Return printer-keyboard terminal class */
        *class = 'K';
        *model = '-';
        *extatr = '-';
        return 0;
    }

    /* Determine display terminal model */
    if (memcmp(termtype+4,"DYNAMIC",7) == 0) {
        *model = 'X';
        *extatr = 'Y';
    } else {
        if (!(memcmp(termtype+4, "3277", 4) == 0
              || memcmp(termtype+4, "3270", 4) == 0
              || memcmp(termtype+4, "3178", 4) == 0
              || memcmp(termtype+4, "3278", 4) == 0
              || memcmp(termtype+4, "3179", 4) == 0
              || memcmp(termtype+4, "3180", 4) == 0
              || memcmp(termtype+4, "3287", 4) == 0
              || memcmp(termtype+4, "3279", 4) == 0))
            return -1;

        *model = '2';
        *extatr = 'N';

        if (termtype[8]=='-') {
            if (termtype[9] < '1' || termtype[9] > '5')
                return -1;
            *model = termtype[9];
            if (memcmp(termtype+4, "328",3) == 0) *model = '2';
            if (memcmp(termtype+10, "-E", 2) == 0)
                *extatr = 'Y';
        }
    }

    /* Perform end-of-record negotiation */
    rc = send_packet (csock, do_eor, sizeof(do_eor),
                        "IAC DO EOR IAC WILL EOR");
    if (rc < 0) return -1;

    rc = expect (csock, will_eor, sizeof(will_eor),
                        "IAC WILL EOR IAC DO EOR");
    if (rc < 0) return -1;

    /* Perform binary negotiation */
    rc = send_packet (csock, do_bin, sizeof(do_bin),
                        "IAC DO BINARY IAC WILL BINARY");
    if (rc < 0) return -1;

    rc = expect (csock, will_bin, sizeof(will_bin),
                        "IAC WILL BINARY IAC DO BINARY");
    if (rc < 0) return -1;

    /* Return display terminal class */
    if (memcmp(termtype+4,"3287",4)==0) *class='P';
    else *class = 'D';
    return 0;

} /* end function negotiate */


/*-------------------------------------------------------------------*/
/* SUBROUTINE TO RECEIVE 3270 DATA FROM THE CLIENT                   */
/* This subroutine receives bytes from the client and appends them   */
/* to any data already in the 3270 receive buffer.                   */
/* If zero bytes are received, this means the client has closed the  */
/* connection, and attention and unit check status is returned.      */
/* If the buffer is filled before receiving end of record, then      */
/* attention and unit check status is returned.                      */
/* If the data ends with IAC followed by EOR_MARK, then the data     */
/* is scanned to remove any IAC sequences, attention status is       */
/* returned, and the read pending indicator is set.                  */
/* If the data accumulated in the buffer does not yet constitute a   */
/* complete record, then zero status is returned, and a further      */
/* call must be made to this subroutine when more data is available. */
/*-------------------------------------------------------------------*/
static BYTE
recv_3270_data (DEVBLK *dev)
{
int     rc;                             /* Return code               */
int     eor = 0;                        /* 1=End of record received  */

    /* If there is a complete data record already in the buffer
       then discard it before reading more data */
    if (dev->readpending)
    {
        dev->rlen3270 = 0;
        dev->readpending = 0;
    }

    /* Receive bytes from client */
    rc = recv (dev->fd, dev->buf + dev->rlen3270,
               BUFLEN_3270 - dev->rlen3270, 0);

    if (rc < 0) {
        TNSERROR("recv: %s\n", strerror(errno));
        dev->sense[0] = SENSE_EC;
        return (CSW_ATTN | CSW_UC);
    }

    /* If zero bytes were received then client has closed connection */
    if (rc == 0) {
        logmsg (_("HHC603I Device %4.4X connection closed by client %s\n"),
                dev->devnum, inet_ntoa(dev->ipaddr));
        dev->sense[0] = SENSE_IR;
        return (CSW_ATTN | CSW_UC | CSW_DE);
    }

    /* Update number of bytes in receive buffer */
    dev->rlen3270 += rc;

    /* Check whether Attn indicator was received */
    if (dev->rlen3270 >= 2
        && dev->buf[dev->rlen3270 - 2] == IAC
        && dev->buf[dev->rlen3270 - 1] == BRK)
        eor = 1;

    /* Check whether SysRq indicator was received */
    if (dev->rlen3270 >= 2
        && dev->buf[dev->rlen3270 - 2] == IAC
        && dev->buf[dev->rlen3270 - 1] == IP)
        eor = 1;

    /* Check whether end of record marker was received */
    if (dev->rlen3270 >= 2
        && dev->buf[dev->rlen3270 - 2] == IAC
        && dev->buf[dev->rlen3270 - 1] == EOR_MARK)
        eor = 1;

    /* If record is incomplete, test for buffer full */
    if (eor == 0 && dev->rlen3270 >= BUFLEN_3270)
    {
        TNSDEBUG1("3270 buffer overflow\n");
        dev->sense[0] = SENSE_DC;
        return (CSW_ATTN | CSW_UC);
    }

    /* Return zero status if record is incomplete */
    if (eor == 0) return 0;

    /* Trace the complete 3270 data packet */
    TNSDEBUG2("Packet received length=%d\n", dev->rlen3270);
    packet_trace (dev->buf, dev->rlen3270);

    /* Strip off the telnet EOR marker */
    dev->rlen3270 -= 2;

    /* Remove any embedded IAC commands */
    dev->rlen3270 = remove_iac (dev->buf, dev->rlen3270);

    /* Set the read pending indicator and return attention status */
    dev->readpending = 1;
    return (CSW_ATTN);

} /* end function recv_3270_data */


/*-------------------------------------------------------------------*/
/* SUBROUTINE TO SOLICIT 3270 DATA FROM THE CLIENT                   */
/* This subroutine sends a Read or Read Modified command to the      */
/* client and then receives the data into the 3270 receive buffer.   */
/* This subroutine is called by loc3270_execute_ccw as a result of   */
/* processing a Read Buffer CCW, or a Read Modified CCW when no      */
/* data is waiting in the 3270 read buffer.  It waits until the      */
/* client sends end of record.  Certain tn3270 clients fail to       */
/* flush their buffer until the user presses an attention key;       */
/* these clients cause this routine to hang and are not supported.   */
/* Since this routine is only called while a channel program is      */
/* active on the device, we can rely on the dev->busy flag to        */
/* prevent the connection thread from issuing a read and capturing   */
/* the incoming data intended for this routine.                      */
/* The caller MUST hold the device lock.                             */
/* Returns zero status if successful, or unit check if error.        */
/*-------------------------------------------------------------------*/
static BYTE
solicit_3270_data (DEVBLK *dev, BYTE cmd)
{
int             rc;                     /* Return code               */
int             len;                    /* Data length               */
BYTE            buf[32];                /* tn3270 write buffer       */

    /* Clear the inbound buffer of any unsolicited
       data accumulated by the connection thread */
    dev->rlen3270 = 0;
    dev->readpending = 0;

    /* Construct a 3270 read command in the outbound buffer */
    len = 0;
    buf[len++] = cmd;

    /* Append telnet EOR marker to outbound buffer */
    buf[len++] = IAC;
    buf[len++] = EOR_MARK;

    /* Send the 3270 read command to the client */
    rc = send_packet(dev->fd, buf, len, "3270 Read Command");
    if (rc < 0)
    {
        dev->sense[0] = SENSE_DC;
        return (CSW_UC);
    }

    /* Receive response data from the client */
    do {
        len = dev->rlen3270;
        rc = recv_3270_data (dev);
        TNSDEBUG2("read buffer: %d bytes received\n",
                dev->rlen3270 - len);
    } while(rc == 0);

    /* Close the connection if an error occurred */
    if (rc & CSW_UC)
    {
        close (dev->fd);
        dev->fd = -1;
        dev->connected = 0;
        dev->sense[0] = SENSE_DC;
        return (CSW_UC);
    }

    /* Return zero status to indicate response received */
    return 0;

} /* end function solicit_3270_data */


/*-------------------------------------------------------------------*/
/* SUBROUTINE TO RECEIVE 1052/3215 DATA FROM THE CLIENT              */
/* This subroutine receives keyboard input characters from the       */
/* client, and appends the characters to any data already in the     */
/* keyboard buffer.                                                  */
/* If zero bytes are received, this means the client has closed the  */
/* connection, and attention and unit check status is returned.      */
/* If the buffer is filled before receiving end of record, then      */
/* attention and unit check status is returned.                      */
/* If a break indication (control-C, IAC BRK, or IAC IP) is          */
/* received, the attention and unit exception status is returned.    */
/* When carriage return and line feed (CRLF) is received, then       */
/* the CRLF is discarded, the data in the keyboard buffer is         */
/* translated to EBCDIC, the read pending indicator is set, and      */
/* attention status is returned.                                     */
/* If CRLF has not yet been received, then zero status is returned,  */
/* and a further call must be made to this subroutine when more      */
/* data is available.                                                */
/*-------------------------------------------------------------------*/
static BYTE
recv_1052_data (DEVBLK *dev)
{
int     num;                            /* Number of bytes received  */
int     i;                              /* Array subscript           */
BYTE    buf[BUFLEN_1052];               /* Receive buffer            */
BYTE    c;                              /* Character work area       */

    /* Receive bytes from client */
    num = recv (dev->fd, buf, BUFLEN_1052, 0);

    /* Return unit check if error on receive */
    if (num < 0) {
        TNSERROR("recv: %s\n", strerror(errno));
        dev->sense[0] = SENSE_EC;
        return (CSW_ATTN | CSW_UC);
    }

    /* If zero bytes were received then client has closed connection */
    if (num == 0) {
        logmsg (_("HHC606I Device %4.4X connection closed by client %s\n"),
                dev->devnum, inet_ntoa(dev->ipaddr));
        dev->sense[0] = SENSE_IR;
        return (CSW_ATTN | CSW_UC);
    }

    /* Trace the bytes received */
    TNSDEBUG2("Bytes received length=%d\n", num);
    packet_trace (buf, num);

    /* Copy received bytes to keyboard buffer */
    for (i = 0; i < num; i++)
    {
        /* Decrement keyboard buffer pointer if backspace received */
        if (buf[i] == 0x08)
        {
            if (dev->keybdrem > 0) dev->keybdrem--;
            continue;
        }

        /* Return unit exception if control-C received */
        if (buf[i] == 0x03)
        {
            dev->keybdrem = 0;
            return (CSW_ATTN | CSW_UX);
        }

        /* Return unit check if buffer is full */
        if (dev->keybdrem >= BUFLEN_1052)
        {
            TNSDEBUG1("Console keyboard buffer overflow\n");
            dev->keybdrem = 0;
            dev->sense[0] = SENSE_EC;
            return (CSW_ATTN | CSW_UC);
        }

        /* Copy character to keyboard buffer */
        dev->buf[dev->keybdrem++] = buf[i];

        /* Decrement keyboard buffer pointer if telnet
           erase character sequence received */
        if (dev->keybdrem >= 2
            && dev->buf[dev->keybdrem - 2] == IAC
            && dev->buf[dev->keybdrem - 1] == EC)
        {
            dev->keybdrem -= 2;
            if (dev->keybdrem > 0) dev->keybdrem--;
            continue;
        }

        /* Zeroize keyboard buffer pointer if telnet
           erase line sequence received */
        if (dev->keybdrem >= 2
            && dev->buf[dev->keybdrem - 2] == IAC
            && dev->buf[dev->keybdrem - 1] == EL)
        {
            dev->keybdrem = 0;
            continue;
        }

        /* Zeroize keyboard buffer pointer if telnet
           carriage return sequence received */
        if (dev->keybdrem >= 2
            && dev->buf[dev->keybdrem - 2] == '\r'
            && dev->buf[dev->keybdrem - 1] == '\0')
        {
            dev->keybdrem = 0;
            continue;
        }

        /* Return unit exception if telnet break sequence received */
        if (dev->keybdrem >= 2
            && dev->buf[dev->keybdrem - 2] == IAC
            && (dev->buf[dev->keybdrem - 1] == BRK
                || dev->buf[dev->keybdrem - 1] == IP))
        {
            dev->keybdrem = 0;
            return (CSW_ATTN | CSW_UX);
        }

        /* Return unit check with overrun if telnet CRLF
           sequence received and more data follows the CRLF */
        if (dev->keybdrem >= 2
            && dev->buf[dev->keybdrem - 2] == '\r'
            && dev->buf[dev->keybdrem - 1] == '\n'
            && i < num - 1)
        {
            TNSDEBUG1("Console keyboard buffer overrun\n");
            dev->keybdrem = 0;
            dev->sense[0] = SENSE_OR;
            return (CSW_ATTN | CSW_UC);
        }

    } /* end for(i) */

    /* Return zero status if CRLF was not yet received */
    if (dev->keybdrem < 2
        || dev->buf[dev->keybdrem - 2] != '\r'
        || dev->buf[dev->keybdrem - 1] != '\n')
        return 0;

    /* Trace the complete keyboard data packet */
    TNSDEBUG2("Packet received length=%d\n", dev->keybdrem);
    packet_trace (dev->buf, dev->keybdrem);

    /* Strip off the CRLF sequence */
    dev->keybdrem -= 2;

    /* Translate the keyboard buffer to EBCDIC */
    for (i = 0; i < dev->keybdrem; i++)
    {
        c = dev->buf[i];
        dev->buf[i] = (isprint(c) ? host_to_guest(c) : SPACE);
    } /* end for(i) */

    /* Trace the EBCDIC input data */
    TNSDEBUG2("Input data line length=%d\n", dev->keybdrem);
    packet_trace (dev->buf, dev->keybdrem);

    /* Return attention status */
    return (CSW_ATTN);

} /* end function recv_1052_data */


/*-------------------------------------------------------------------*/
/* NEW CLIENT CONNECTION THREAD                                      */
/*-------------------------------------------------------------------*/
static void *
connect_client (int *csockp)
{
int                     rc;             /* Return code               */
DEVBLK                 *dev;            /* -> Device block           */
int                     len;            /* Data length               */
int                     csock;          /* Socket for conversation   */
struct sockaddr_in      client;         /* Client address structure  */
socklen_t               namelen;        /* Length of client structure*/
struct hostent         *pHE;            /* Addr of hostent structure */
char                   *clientip;       /* Addr of client ip address */
char                   *clientname;     /* Addr of client hostname   */
U16                     devnum;         /* Requested device number   */
BYTE                    class;          /* D=3270, P=3287, K=3215/1052 */
BYTE                    model;          /* 3270 model (2,3,4,5,X)    */
BYTE                    extended;       /* Extended attributes (Y,N) */
BYTE                    buf[256];       /* Message buffer            */
BYTE                    conmsg[80];     /* Connection message        */
BYTE                    hostmsg[80];    /* Host ID message           */
BYTE                    rejmsg[80];     /* Rejection message         */

    /* Load the socket address from the thread parameter */
    csock = *csockp;

    /* Obtain the client's IP address */
    namelen = sizeof(client);
    rc = getpeername (csock, (struct sockaddr *)&client, &namelen);

    /* Log the client's IP address and hostname */
    clientip = strdup(inet_ntoa(client.sin_addr));

    pHE = gethostbyaddr ((unsigned char*)(&client.sin_addr),
                         sizeof(client.sin_addr), AF_INET);

    if (pHE != NULL && pHE->h_name != NULL
     && pHE->h_name[0] != '\0') {
        clientname = (char*) pHE->h_name;
    } else {
        clientname = "host name unknown";
    }

    TNSDEBUG1("Received connection from %s (%s)\n",
            clientip, clientname);

    /* Negotiate telnet parameters */
    rc = negotiate (csock, &class, &model, &extended, &devnum);
    if (rc != 0)
    {
        close (csock);
        if (clientip) free(clientip);
        return NULL;
    }

    /* Look for an available console device */
    for (dev = sysblk.firstdev; dev != NULL; dev = dev->nextdev)
    {
        /* Loop if the device is invalid */
        if(!(dev->pmcw.flag5 & PMCW5_V))
            continue;

        /* Loop if non-matching device type */
        if (class == 'D' && dev->devtype != 0x3270)
            continue;

        if (class == 'P' && dev->devtype != 0x3287)
            continue;

        if (class == 'K' && dev->devtype != 0x1052
            && dev->devtype != 0x3215)
            continue;

        /* Loop if a specific device number was requested and
           this device is not the requested device number */
        if (devnum != 0xFFFF && dev->devnum != devnum)
            continue;

        /* Obtain the device lock */
        obtain_lock (&dev->lock);

        /* Test for available device */
        if (dev->connected == 0)
        {
            /* Claim this device for the client */
            dev->connected = 1;
            dev->fd = csock;
            dev->ipaddr = client.sin_addr;
            dev->mod3270 = model;
            dev->eab3270 = (extended == 'Y' ? 1 : 0);

            /* Reset the console device */
            dev->readpending = 0;
            dev->rlen3270 = 0;
            dev->keybdrem = 0;
            dev->busy = 0;
            dev->pending = 0;
            dev->pcipending = 0;
            memset (&dev->scsw, 0, sizeof(SCSW));
            memset (&dev->pciscsw, 0, sizeof(SCSW));

            release_lock (&dev->lock);
            break;
        }

        /* Release the device lock */
        release_lock (&dev->lock);

    } /* end for(dev) */

    /* Build connection message for client */
    len = snprintf (hostmsg, sizeof(hostmsg),
                "running on %s (%s %s)",
                hostinfo.nodename, hostinfo.sysname,
                hostinfo.release);
    len = snprintf (conmsg, sizeof(conmsg),
                "Hercules version %s built at %s %s",
                VERSION, __DATE__, __TIME__);

    if (dev != NULL)
        len += sprintf (conmsg + len, " device %4.4X", dev->devnum);

    /* Reject the connection if no available console device */
    if (dev == NULL)
    {
        /* Build the rejection message */
        if (devnum == 0xFFFF)
            len = sprintf (rejmsg,
                    "Connection rejected, no available %s device",
       (class=='D' ? "3270" : (class=='P' ? "3287" : "1052 or 3215")));
        else
            len = sprintf (rejmsg,
                    "Connection rejected, device %4.4X unavailable",
                    devnum);

        TNSDEBUG1( "%s\n", rejmsg);

        /* Send connection rejection message to client */
        if (class != 'K')
        {
            len = sprintf (buf,
                        "\xF5\x40\x11\x40\x40\x1D\x60%s"
                        "\x11\xC1\x50\x1D\x60%s"
                        "\x11\xC2\x60\x1D\x60%s",
                        translate_to_ebcdic(conmsg),
                        translate_to_ebcdic(hostmsg),
                        translate_to_ebcdic(rejmsg));
            buf[len++] = IAC;
            buf[len++] = EOR_MARK;
        }
        else
        {
            len = sprintf (buf, "%s\r\n%s\r\n%s\r\n", conmsg, hostmsg, rejmsg);
        }
        if (class != 'P')  /* do not write connection resp on 3287 */
        rc = send_packet (csock, buf, len, "CONNECTION RESPONSE");

        /* Close the connection and terminate the thread */
        sleep (5);
        close (csock);
        if (clientip) free(clientip);
        return NULL;
    }

    logmsg (_("HHC604I Client %s connected to %4.4X device %4.4X\n"),
            clientip, dev->devtype, dev->devnum);

    /* Send connection message to client */
    if (class != 'K')
    {
        len = sprintf (buf,
                    "\xF5\x40\x11\x40\x40\x1D\x60%s"
                    "\x11\xC1\x50\x1D\x60%s",
                    translate_to_ebcdic(conmsg),
                    translate_to_ebcdic(hostmsg));
        buf[len++] = IAC;
        buf[len++] = EOR_MARK;
    }
    else
    {
        len = sprintf (buf, "%s\r\n%s\r\n", conmsg, hostmsg);
    }
    if (class != 'P')  /* do not write connection resp on 3287 */
    rc = send_packet (csock, buf, len, "CONNECTION RESPONSE");

    /* Raise attention interrupt for the device */
    if (class != 'P')  /* do not raise attention for  3287 */
    rc = device_attention (dev, CSW_ATTN);

    /* Signal connection thread to redrive its select loop */
    signal_thread (sysblk.cnsltid, SIGUSR2);

    if (clientip) free(clientip);
    return NULL;

} /* end function connect_client */


/*-------------------------------------------------------------------*/
/* CONSOLE CONNECTION AND ATTENTION HANDLER THREAD                   */
/*-------------------------------------------------------------------*/
static void *
console_connection_handler (void *arg)
{
int                     rc = 0;         /* Return code               */
int                     lsock;          /* Socket for listening      */
int                     csock;          /* Socket for conversation   */
struct sockaddr_in      server;         /* Server address structure  */
fd_set                  readset;        /* Read bit map for select   */
int                     maxfd;          /* Highest fd for select     */
int                     optval;         /* Argument for setsockopt   */
TID                     tidneg;         /* Negotiation thread id     */
DEVBLK                 *dev;            /* -> Device block           */
BYTE                    unitstat;       /* Status after receive data */

    UNREFERENCED(arg);

    /* Display thread started message on control panel */
    logmsg (_("HHC600I Console connection thread started: "
            "tid="TIDPAT", pid=%d\n"),
            thread_id(), getpid());

    /* Get information about this system */
    uname (&hostinfo);

    /* Obtain a socket */
    lsock = socket (AF_INET, SOCK_STREAM, 0);

    if (lsock < 0)
    {
        TNSERROR("socket: %s\n", strerror(errno));
        return NULL;
    }

    /* Allow previous instance of socket to be reused */
    optval = 1;
    setsockopt (lsock, SOL_SOCKET, SO_REUSEADDR,
                &optval, sizeof(optval));

    /* Prepare the sockaddr structure for the bind */
    memset (&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = sysblk.cnslport;
    server.sin_port = htons(server.sin_port);

    /* Attempt to bind the socket to the port */
    while (sysblk.cnslcnt)
    {
        rc = bind (lsock, (struct sockaddr *)&server, sizeof(server));

        if (rc == 0 || errno != EADDRINUSE) break;

        logmsg (_("HHC601I Waiting for port %u to become free\n"),
                sysblk.cnslport);
        sleep(10);
    } /* end while */

    if (rc != 0)
    {
        TNSERROR("bind: %s\n", strerror(errno));
        return NULL;
    }

    /* Put the socket into listening state */
    rc = listen (lsock, 10);

    if (rc < 0)
    {
        TNSERROR("listen: %s\n", strerror(errno));
        return NULL;
    }

    logmsg (_("HHC602I Waiting for console connection on port %u\n"),
            sysblk.cnslport);

    /* Handle connection requests and attention interrupts */
    while (sysblk.cnslcnt) {

        /* Initialize the select parameters */
        maxfd = lsock;
        FD_ZERO (&readset);
        FD_SET (lsock, &readset);

        /* Include the socket for each connected console */
        for (dev = sysblk.firstdev; dev != NULL; dev = dev->nextdev)
        {
            if (dev->console
                && dev->connected
                && (dev->busy == 0 || (dev->scsw.flag3 & SCSW3_AC_SUSP))
                && dev->pending == 0
                && (dev->pmcw.flag5 & PMCW5_V)
// NOT S/370    && (dev->pmcw.flag5 & PMCW5_E)
                && (dev->scsw.flag3 & SCSW3_SC_PEND) == 0)
            {
                FD_SET (dev->fd, &readset);
                if (dev->fd > maxfd) maxfd = dev->fd;
            }
        } /* end for(dev) */

        /* Wait for a file descriptor to become ready */
#ifdef WIN32
    {
        struct timeval tv={0,500000};   /* half a second */
        rc = select ( maxfd+1, &readset, NULL, NULL, &tv );
    }
#else /* WIN32 */
        rc = select ( maxfd+1, &readset, NULL, NULL, NULL );
#endif /* WIN32 */

        if (rc == 0) continue;

        if (rc < 0 )
        {
            if (errno == EINTR) continue;
            TNSERROR("select: %s\n", strerror(errno));
            break;
        }

        /* If a client connection request has arrived then accept it */
        if (FD_ISSET(lsock, &readset))
        {
            /* Accept a connection and create conversation socket */
            csock = accept (lsock, NULL, NULL);

            if (csock < 0)
            {
                TNSERROR("accept: %s\n", strerror(errno));
                continue;
            }

            /* Create a thread to complete the client connection */
            if ( create_thread (&tidneg, &sysblk.detattr,
                                connect_client, &csock) )
            {
                TNSERROR("connect_client create_thread: %s\n",
                        strerror(errno));
                close (csock);
            }

        } /* end if(lsock) */

        /* Check if any connected client has data ready to send */
        for (dev = sysblk.firstdev; dev != NULL; dev = dev->nextdev)
        {
            /* Obtain the device lock */
            obtain_lock (&dev->lock);

            /* Test for connected console with data available */
            if (dev->console
                && dev->connected
                && FD_ISSET (dev->fd, &readset)
                && (dev->busy == 0 || (dev->scsw.flag3 & SCSW3_AC_SUSP))
                && dev->pending == 0
                && (dev->pmcw.flag5 & PMCW5_V)
// NOT S/370    && (dev->pmcw.flag5 & PMCW5_E)
                && (dev->scsw.flag3 & SCSW3_SC_PEND) == 0)
            {
                /* Receive console input data from the client */
                if ((dev->devtype == 0x3270) || (dev->devtype == 0x3287))
                    unitstat = recv_3270_data (dev);
                else
                    unitstat = recv_1052_data (dev);

                /* Nothing more to do if incomplete record received */
                if (unitstat == 0)
                {
                    release_lock (&dev->lock);
                    continue;
                }

                /* Close the connection if an error occurred */
                if (unitstat & CSW_UC)
                {
                    close (dev->fd);
                    dev->fd = -1;
                    dev->connected = 0;
                }

                /* Indicate that data is available at the device */
                if(dev->rlen3270)
                    dev->readpending = 1;
  
                /* Release the device lock */
                release_lock (&dev->lock);

                /* Raise attention interrupt for the device */
                /* Do not raise attention interrupt for 3287  */
                /* Otherwise zVM loops after ENABLE ccuu     */
                /* Following 5 lines are repeated on Hercules console: */
                /* console: sending 3270 data */
                /* +0000   F5C2FFEF     */
                /* console: Packet received length=7 */
                /* +0000   016CD902 00FFEF */   
                /*         I do not know what is this */
                /* console: CCUU attention requests raised */
                if (dev->devtype != 0x3287)
                {
                rc = device_attention (dev, unitstat);

                /* Trace the attention request */
                TNSDEBUG2("%4.4X attention request %s\n",
                        dev->devnum,
                        (rc == 0 ? "raised" : "rejected"));
                }
                continue;
            } /* end if(data available) */

            /* Release the device lock */
            release_lock (&dev->lock);

        } /* end for(dev) */

    } /* end while */

    /* Close the listening socket */
    close (lsock);

    logmsg (_("HHC609I Console connection thread terminated\n"));

    return NULL;

} /* end function console_connection_handler */


static int
console_initialise()
{
    if(!(sysblk.cnslcnt++))
    {
        if ( create_thread (&sysblk.cnsltid, &sysblk.detattr,
                            console_connection_handler, NULL) )
        {
            logmsg (_("HHC135I Cannot create console thread: %s\n"),
                    strerror(errno));
            return 1;
        }
    }
    return 0;
}


static void
console_remove(DEVBLK *dev)
{
    if(dev->fd > 2)
        close (dev->fd);
    dev->fd = -1;

    /* Reset device dependent flags */
    dev->connected = 0;
    dev->console = 0;

    if(!sysblk.cnslcnt--)
        logmsg(_("console_remove() error\n"));

    signal_thread (sysblk.cnsltid, SIGUSR2);
}


/*-------------------------------------------------------------------*/
/* INITIALIZE THE 3270 DEVICE HANDLER                                */
/*-------------------------------------------------------------------*/
static int
loc3270_init_handler ( DEVBLK *dev, int argc, BYTE *argv[] )
{
    UNREFERENCED(argc);
    UNREFERENCED(argv);

    /* Indicate that this is a console device */
    dev->console = 1;

    /* Reset device dependent flags */
    dev->connected = 0;

    /* Set number of sense bytes */
    dev->numsense = 1;

    /* Set the size of the device buffer */
    dev->bufsize = BUFLEN_3270;

    /* Initialize the device identifier bytes */
    dev->devid[0] = 0xFF;
    dev->devid[1] = 0x32; /* Control unit type is 3274-1D */
    dev->devid[2] = 0x74;
    dev->devid[3] = 0x1D;
    dev->devid[4] = 0x32; /* Device type is 3278-2 */
    if ((dev->devtype & 0xFF)==0x70)
     {
    dev->devid[5] = 0x78;
    dev->devid[6] = 0x02;
     }
    else
     {
     dev->devid[5] = dev->devtype & 0xFF; /* device type is 3287-1 */
     dev->devid[6] = 0x01;
     }
    dev->numdevid = 7;

    return console_initialise();
} /* end function loc3270_init_handler */


/*-------------------------------------------------------------------*/
/* QUERY THE 3270 DEVICE DEFINITION                                  */
/*-------------------------------------------------------------------*/
static void
loc3270_query_device (DEVBLK *dev, BYTE **class,
                int buflen, BYTE *buffer)
{

    *class = "DSP";

    if (dev->connected)
        snprintf (buffer, buflen, "%s",
                (BYTE*)inet_ntoa(dev->ipaddr));
    else
        buffer[0] = '\0';

} /* end function loc3270_query_device */


/*-------------------------------------------------------------------*/
/* CLOSE THE 3270 DEVICE HANDLER                                     */
/*-------------------------------------------------------------------*/
static int
loc3270_close_device ( DEVBLK *dev )
{
    console_remove(dev);

    return 0;
} /* end function loc3270_close_device */


/*-------------------------------------------------------------------*/
/* INITIALIZE THE 1052/3215 DEVICE HANDLER                           */
/*-------------------------------------------------------------------*/
static int
constty_init_handler ( DEVBLK *dev, int argc, BYTE *argv[] )
{
    UNREFERENCED(argc);
    UNREFERENCED(argv);

    /* Indicate that this is a console device */
    dev->console = 1;

    /* Set number of sense bytes */
    dev->numsense = 1;

    /* Initialize device dependent fields */
    dev->keybdrem = 0;

    /* Set length of print buffer */
    dev->bufsize = BUFLEN_1052;

    /* Initialize the device identifier bytes */
    dev->devid[0] = 0xFF;
    dev->devid[1] = dev->devtype >> 8;
    dev->devid[2] = dev->devtype & 0xFF;
    dev->devid[3] = 0x00;
    dev->devid[4] = dev->devtype >> 8;
    dev->devid[5] = dev->devtype & 0xFF;
    dev->devid[6] = 0x00;
    dev->numdevid = 7;

    return console_initialise();

} /* end function constty_init_handler */


/*-------------------------------------------------------------------*/
/* QUERY THE 1052/3215 DEVICE DEFINITION                             */
/*-------------------------------------------------------------------*/
static void
constty_query_device (DEVBLK *dev, BYTE **class,
                int buflen, BYTE *buffer)
{

    *class = "CON";

    if (dev->connected)
        snprintf (buffer, buflen, "%s",
                (BYTE*)inet_ntoa(dev->ipaddr));
    else
        buffer[0] = '\0';

} /* end function constty_query_device */


/*-------------------------------------------------------------------*/
/* CLOSE THE 1052/3215 DEVICE HANDLER                                */
/*-------------------------------------------------------------------*/
static int
constty_close_device ( DEVBLK *dev )
{
    console_remove(dev);

    return 0;
} /* end function constty_close_device */


/*-------------------------------------------------------------------*/
/* SUBROUTINE TO ADVANCE TO NEXT CHAR OR ORDER IN A 3270 DATA STREAM */
/* Input:                                                            */
/*      buf     Buffer containing 3270 data stream                   */
/*      off     Offset in buffer of current character or order       */
/*      pos     Position on screen of current character or order     */
/* Output:                                                           */
/*      off     Offset in buffer of next character or order          */
/*      pos     Position on screen of next character or order        */
/*-------------------------------------------------------------------*/
static void
next_3270_pos (BYTE *buf, int *off, int *pos)
{
int     i;

    /* Copy the offset and advance the offset by 1 byte */
    i = (*off)++;

    /* Advance the offset past the argument bytes and set position */
    switch (buf[i]) {

    /* The Repeat to Address order has 3 argument bytes (or in case
       of a Graphics Escape 4 bytes) and sets the screen position */
            case O3270_RA:

        *off += (buf[i+3] == O3270_GE) ? 4 : 3;
                if ((buf[i+1] & 0xC0) == 0x00)
                    *pos = (buf[i+1] << 8) | buf[i+2];
                else
                    *pos = ((buf[i+1] & 0x3F) << 6)
                                 | (buf[i+2] & 0x3F);
        break;
            
    /* The Start Field Extended and Modify Field orders have
       a count byte followed by a variable number of type-
       attribute pairs, and advance the screen position by 1 */
            case O3270_SFE:
            case O3270_MF:

        *off += (1 + 2*buf[i+1]);
        (*pos)++;
                break;

    /* The Set Buffer Address and Erase Unprotected to Address
       orders have 2 argument bytes and set the screen position */
            case O3270_SBA:
            case O3270_EUA:

        *off += 2;
                if ((buf[i+1] & 0xC0) == 0x00)
                    *pos = (buf[i+1] << 8) | buf[i+2];
                else
                    *pos = ((buf[i+1] & 0x3F) << 6)
                                 | (buf[i+2] & 0x3F);
        break;

    /* The Set Attribute order has 2 argument bytes and
       does not change the screen position */
            case O3270_SA:

        *off += 2;
                break;

    /* Insert Cursor and Program Tab have no argument
       bytes and do not change the screen position */
            case O3270_IC:
            case O3270_PT:

                break;

    /* The Start Field and Graphics Escape orders have one
       argument byte, and advance the screen position by 1 */
            case O3270_SF:
            case O3270_GE:

        (*off)++;
        (*pos)++;
        break;

    /* All other characters advance the screen position by 1 */
            default:

                (*pos)++;
                break;

    } /* end switch */

} /* end function next_3270_pos */


/*-------------------------------------------------------------------*/
/* SUBROUTINE TO FIND A GIVEN SCREEN POSITION IN A 3270 READ BUFFER  */
/* Input:                                                            */
/*      buf     Buffer containing an inbound 3270 data stream        */
/*      size    Number of bytes in buffer                            */
/*      pos     Screen position whose offset in buffer is desired    */
/* Return value:                                                     */
/*      Offset in buffer of the character or order corresponding to  */
/*      the given screen position, or zero if position not found.    */
/*-------------------------------------------------------------------*/
static int
find_buffer_pos (BYTE *buf, int size, int pos)
{
int     wpos;                           /* Current screen position   */
int     woff;                           /* Current offset in buffer  */

    /* Screen position 0 is at offset 3 in the device buffer,
       following the AID and cursor address bytes */
    wpos = 0;
    woff = 3;

    while (woff < size)
    {
        /* Exit if desired screen position has been reached */
        if (wpos >= pos)
        {
//          logmsg (_("console: Pos %4.4X reached at %4.4X\n"),
//                  wpos, woff);

#ifdef FIX_QWS_BUG_FOR_MCS_CONSOLES
            /* There is a bug in QWS3270 when used to emulate an
               MCS console with EAB.  At position 1680 the Read
               Buffer contains two 6-byte SFE orders (12 bytes)
               preceding the entry area, whereas MCS expects the
               entry area to start 4 bytes after screen position
               1680 in the buffer.  The bypass is to add 8 to the
               calculated buffer offset if this appears to be an
               MCS console read buffer command */
            if (pos == 0x0690 && buf[woff] == O3270_SFE
                && buf[woff+6] == O3270_SFE)
            {
                woff += 8;
//              logmsg (_("console: Pos %4.4X adjusted to %4.4X\n"),
//                      wpos, woff);
        }
#endif /*FIX_QWS_BUG_FOR_MCS_CONSOLES*/

            return woff;
        }

        /* Process next character or order, update screen position */
        next_3270_pos (buf, &woff, &wpos);

    } /* end while */

    /* Return offset zero if the position cannot be determined */
    return 0;

} /* end function find_buffer_pos */


/*-------------------------------------------------------------------*/
/* SUBROUTINE TO UPDATE THE CURRENT SCREEN POSITION                  */
/* Input:                                                            */
/*      pos     Current screen position                              */
/*      buf     Pointer to the byte in the 3270 data stream          */
/*              corresponding to the current screen position         */
/*      size    Number of bytes remaining in buffer                  */
/* Output:                                                           */
/*      pos     Updated screen position after end of buffer          */
/*-------------------------------------------------------------------*/
static void
get_screen_pos (int *pos, BYTE *buf, int size)
{
int     woff = 0;                       /* Current offset in buffer  */

    while (woff < size)
    {
        /* Process next character or order, update screen position */
        next_3270_pos (buf, &woff, pos);

    } /* end while */

} /* end function get_screen_pos */


/*-------------------------------------------------------------------*/
/* EXECUTE A 3270 CHANNEL COMMAND WORD                               */
/*-------------------------------------------------------------------*/
static void
loc3270_execute_ccw ( DEVBLK *dev, BYTE code, BYTE flags,
        BYTE chained, U16 count, BYTE prevcode, int ccwseq,
        BYTE *iobuf, BYTE *more, BYTE *unitstat, U16 *residual )
{
int             rc;                     /* Return code               */
int             num;                    /* Number of bytes to copy   */
int             len;                    /* Data length               */
int             aid;                    /* First read: AID present   */
U32             off;                    /* Offset in device buffer   */
BYTE            cmd;                    /* tn3270 command code       */
BYTE            buf[BUFLEN_3270];       /* tn3270 write buffer       */

    UNREFERENCED(prevcode);
    UNREFERENCED(ccwseq);

    /* Clear the current screen position at start of CCW chain */
    if (!chained)
        dev->pos3270 = 0;

    /* Unit check with intervention required if no client connected */
    if (!dev->connected && !IS_CCW_SENSE(code))
    {
        dev->sense[0] = SENSE_IR;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return;
    }

    /* Process depending on CCW opcode */
    switch (code) {

    case L3270_NOP:
    /*---------------------------------------------------------------*/
    /* CONTROL NO-OPERATION                                          */
    /*---------------------------------------------------------------*/
        /* Reset the buffer address */
        dev->pos3270 = 0;

        *unitstat = CSW_CE | CSW_DE;
        break;

    case L3270_SELRM:
    case L3270_SELRB:
    case L3270_SELRMP:
    case L3270_SELRBP:
    case L3270_SELWRT:
    /*---------------------------------------------------------------*/
    /* SELECT                                                        */
    /*---------------------------------------------------------------*/
        /* Reset the buffer address */
        dev->pos3270 = 0;

        *residual = 0;
        *unitstat = CSW_CE | CSW_DE;
        break;

    case L3270_EAU:
    /*---------------------------------------------------------------*/
    /* ERASE ALL UNPROTECTED                                         */
    /*---------------------------------------------------------------*/
        dev->pos3270 = 0;
        cmd = R3270_EAU;
        goto write;

    case L3270_WRT:
    /*---------------------------------------------------------------*/
    /* WRITE                                                         */
    /*---------------------------------------------------------------*/
        cmd = R3270_WRT;
        goto write;

    case L3270_EW:
    /*---------------------------------------------------------------*/
    /* ERASE/WRITE                                                   */
    /*---------------------------------------------------------------*/
        dev->pos3270 = 0;
        cmd = R3270_EW;
        goto write;

    case L3270_EWA:
    /*---------------------------------------------------------------*/
    /* ERASE/WRITE ALTERNATE                                         */
    /*---------------------------------------------------------------*/
        dev->pos3270 = 0;
        cmd = R3270_EWA;
        goto write;

    case L3270_WSF:
    /*---------------------------------------------------------------*/
    /* WRITE STRUCTURED FIELD                                        */
    /*---------------------------------------------------------------*/
        /* Process WSF command if device has extended attributes */
        if (dev->eab3270)
        {
        dev->pos3270 = 0;
        cmd = R3270_WSF;
            goto write;
        }

        /* Operation check, device does not have extended attributes */
        dev->sense[0] = SENSE_OC;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        break;

    write:
    /*---------------------------------------------------------------*/
    /* All write commands, and the EAU control command, come here    */
    /*---------------------------------------------------------------*/
        /* Initialize the data length */
        len = 0;

        /* Calculate number of bytes to move and residual byte count */
        num = sizeof(buf) / 2;
        num = (count < num) ? count : num;
        if(cmd == R3270_EAU) 
           num = 0;
        *residual = count - num;

        /* Move the 3270 command code to the first byte of the buffer
           unless data-chained from previous CCW */
        if ((chained & CCW_FLAGS_CD) == 0)
        {
            buf[len++] = cmd;
            /* If this is a chained write then we start at the
               current buffer address rather then the cursor address.
               If the first action the datastream takes is not a
               positioning action then insert a SBA to position to
               the current buffer address */
            if(chained
              && cmd == R3270_WRT
              && dev->pos3270 != 0
              && iobuf[1] != O3270_SBA
              && iobuf[1] != O3270_RA
              && iobuf[1] != O3270_EUA)
            {
                /* Copy the write control character and ajust buffer */
                buf[len++] = *iobuf++; num--;
                /* Insert the SBA order */
                buf[len++] = O3270_SBA;
                if(dev->pos3270 < 4096)
                {
                    buf[len++] = sba_code[dev->pos3270 >> 6];
                    buf[len++] = sba_code[dev->pos3270 & 0x3F];
                }
                else
                {
                    buf[len++] = dev->pos3270 >> 8;
                    buf[len++] = dev->pos3270 & 0xFF;
                }
            } /* if(iobuf[0] != SBA, RA or EUA) */

            /* Save the screen position at completion of the write.
               This is necessary in case a Read Buffer command is chained
               from another write or read, this does not apply for the 
               write structured field command */
            if(cmd != R3270_WSF)
                get_screen_pos (&dev->pos3270, iobuf+1, num-1);

        } /* if(!data_chained) */
        else /* if(data_chained) */
            if(cmd != R3270_WSF)
                get_screen_pos (&dev->pos3270, iobuf, num);

        /* Copy data from channel buffer to device buffer */
        memcpy (buf + len, iobuf, num);
        len += num;

        /* Double up any IAC bytes in the data */
        len = double_up_iac (buf, len);

        /* Append telnet EOR marker at end of data */
        if ((flags & CCW_FLAGS_CD) == 0) {
            buf[len++] = IAC;
            buf[len++] = EOR_MARK;
        }

        /* Send the data to the client */
        rc = send_packet(dev->fd, buf, len, "3270 data");
        if (rc < 0)
        {
            dev->sense[0] = SENSE_DC;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Return normal status */
        *unitstat = CSW_CE | CSW_DE;
        break;

    case L3270_RB:
    /*---------------------------------------------------------------*/
    /* READ BUFFER                                                   */
    /*---------------------------------------------------------------*/
        /* Obtain the device lock */
        obtain_lock (&dev->lock);

        /* AID is only present during the first read */
        aid = dev->readpending != 2;

        /* Receive buffer data from client if not data chained */
        if ((chained & CCW_FLAGS_CD) == 0)
        {
            /* Send read buffer command to client and await response */
            rc = solicit_3270_data (dev, R3270_RB);
            if (rc & CSW_UC)
            {
                *unitstat = CSW_CE | CSW_DE | CSW_UC;
                release_lock (&dev->lock);
                break;
            }

            /* Set AID in buffer flag */
            aid = 1;

            /* Save the AID of the current inbound transmission */
            dev->aid3270 = dev->buf[0];
            if(dev->pos3270 != 0 && dev->aid3270 != SF3270_AID)
            {
                /* Find offset in buffer of current screen position */
                off = find_buffer_pos (dev->buf, dev->rlen3270,
                                        dev->pos3270);

                /* Shift out unwanted characters from buffer */
                num = (dev->rlen3270 > off ? dev->rlen3270 - off : 0);
                memmove (dev->buf + 3, dev->buf + off, num);
                dev->rlen3270 = 3 + num;
            }

        } /* end if(!CCW_FLAGS_CD) */

        /* Calculate number of bytes to move and residual byte count */
        len = dev->rlen3270;
        num = (count < len) ? count : len;
        *residual = count - num;
        if (count < len) *more = 1;

        /* Save the screen position at completion of the read.
           This is necessary in case a Read Buffer command is chained
           from another write or read. */
        if(dev->aid3270 != SF3270_AID)
        {
            if(aid)
                get_screen_pos(&dev->pos3270, dev->buf+3, num-3);
            else
                get_screen_pos(&dev->pos3270, dev->buf, num);
        }

        /* Indicate that the AID bytes have been skipped */
        if(dev->readpending == 1)
            dev->readpending = 2;

        /* Copy data from device buffer to channel buffer */
        memcpy (iobuf, dev->buf, num);

        /* If data chaining is specified, save remaining data */
        if ((flags & CCW_FLAGS_CD) && len > count)
        {
            memmove (dev->buf, dev->buf + count, len - count);
            dev->rlen3270 = len - count;
        }
        else
        {
            dev->rlen3270 = 0;
            dev->readpending = 0;
        }

        /* Return normal status */
        *unitstat = CSW_CE | CSW_DE;

        /* Release the device lock */
        release_lock (&dev->lock);
        break;

    case L3270_RM:
    /*---------------------------------------------------------------*/
    /* READ MODIFIED                                                 */
    /*---------------------------------------------------------------*/
        /* Obtain the device lock */
        obtain_lock (&dev->lock);

        /* AID is only present during the first read */
        aid = dev->readpending != 2;

        /* If not data chained from previous Read Modified CCW,
           and if the connection thread has not already accumulated
           a complete Read Modified record in the inbound buffer,
           then solicit a Read Modified operation at the client */
        if ((chained & CCW_FLAGS_CD) == 0
            && !dev->readpending)
        {
            /* Send read modified command to client, await response */
            rc = solicit_3270_data (dev, R3270_RM);
            if (rc & CSW_UC)
            {
                *unitstat = CSW_CE | CSW_DE | CSW_UC;
                release_lock (&dev->lock);
                break;
            }

            dev->aid3270 = dev->buf[0];
            if(dev->pos3270 != 0 && dev->aid3270 != SF3270_AID)
            {
                /* Find offset in buffer of current screen position */
                off = find_buffer_pos (dev->buf, dev->rlen3270,
                                        dev->pos3270);

                /* Shift out unwanted characters from buffer */
                num = (dev->rlen3270 > off ? dev->rlen3270 - off : 0);
                memmove (dev->buf + 3, dev->buf + off, num);
                dev->rlen3270 = 3 + num;
            }

        } /* end if(!CCW_FLAGS_CD) */

        /* Calculate number of bytes to move and residual byte count */
        len = dev->rlen3270;
        num = (count < len) ? count : len;
        *residual = count - num;
        if (count < len) *more = 1;

        /* Save the screen position at completion of the read.
           This is necessary in case a Read Buffer command is chained
           from another write or read. */
        if(dev->aid3270 != SF3270_AID)
        {
            if(aid)
                get_screen_pos(&dev->pos3270, dev->buf+3, num-3);
            else
                get_screen_pos(&dev->pos3270, dev->buf, num);
        }

        /* Indicate that the AID bytes have been skipped */
        if(dev->readpending == 1)
            dev->readpending = 2;

        /* Copy data from device buffer to channel buffer */
        memcpy (iobuf, dev->buf, num);

        /* If data chaining is specified, save remaining data */
        if ((flags & CCW_FLAGS_CD) && len > count)
        {
            memmove (dev->buf, dev->buf + count, len - count);
            dev->rlen3270 = len - count;
        }
        else
        {
            dev->rlen3270 = 0;
            dev->readpending = 0;
        }

        /* Set normal status */
        *unitstat = CSW_CE | CSW_DE;

        /* Release the device lock */
        release_lock (&dev->lock);

        /* Signal connection thread to redrive its select loop */
        signal_thread (sysblk.cnsltid, SIGUSR2);

        break;

    case L3270_SENSE:
    /*---------------------------------------------------------------*/
    /* SENSE                                                         */
    /*---------------------------------------------------------------*/
        /* Calculate residual byte count */
        num = (count < dev->numsense) ? count : dev->numsense;
        *residual = count - num;
        if (count < dev->numsense) *more = 1;

        /* Copy device sense bytes to channel I/O buffer */
        memcpy (iobuf, dev->sense, num);

        /* Clear the device sense bytes */
        memset (dev->sense, 0, sizeof(dev->sense));

        /* Reset the buffer address */
        dev->pos3270 = 0;

        /* Return unit status */
        *unitstat = CSW_CE | CSW_DE;
        break;

    case L3270_SENSEID:
    /*---------------------------------------------------------------*/
    /* SENSE ID                                                      */
    /*---------------------------------------------------------------*/
        /* Calculate residual byte count */
        num = (count < dev->numdevid) ? count : dev->numdevid;
        *residual = count - num;
        if (count < dev->numdevid) *more = 1;

        /* Copy device identifier bytes to channel I/O buffer */
        memcpy (iobuf, dev->devid, num);

        /* Reset the buffer address */
        dev->pos3270 = 0;

        /* Return unit status */
        *unitstat = CSW_CE | CSW_DE;
        break;

    default:
    /*---------------------------------------------------------------*/
    /* INVALID OPERATION                                             */
    /*---------------------------------------------------------------*/
        /* Set command reject sense byte, and unit check status */
        dev->sense[0] = SENSE_CR;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;

    } /* end switch(code) */

} /* end function loc3270_execute_ccw */


/*-------------------------------------------------------------------*/
/* EXECUTE A 1052/3215 CHANNEL COMMAND WORD                          */
/*-------------------------------------------------------------------*/
static void
constty_execute_ccw ( DEVBLK *dev, BYTE code, BYTE flags,
        BYTE chained, U16 count, BYTE prevcode, int ccwseq,
        BYTE *iobuf, BYTE *more, BYTE *unitstat, U16 *residual )
{
int     rc;                             /* Return code               */
int     len;                            /* Length of data            */
int     num;                            /* Number of bytes to move   */
BYTE    c;                              /* Print character           */
BYTE    stat;                           /* Unit status               */

    UNREFERENCED(chained);
    UNREFERENCED(prevcode);
    UNREFERENCED(ccwseq);

    /* Unit check with intervention required if no client connected */
    if (dev->connected == 0 && !IS_CCW_SENSE(code))
    {
        dev->sense[0] = SENSE_IR;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;
        return;
    }

    /* Process depending on CCW opcode */
    switch (code) {

    case 0x01:
    /*---------------------------------------------------------------*/
    /* WRITE NO CARRIER RETURN                                       */
    /*---------------------------------------------------------------*/

    case 0x09:
    /*---------------------------------------------------------------*/
    /* WRITE AUTO CARRIER RETURN                                     */
    /*---------------------------------------------------------------*/

        /* Calculate number of bytes to write and set residual count */
        num = (count < BUFLEN_1052) ? count : BUFLEN_1052;
        *residual = count - num;

        /* Translate data in channel buffer to ASCII */
        for (len = 0; len < num; len++)
        {
            c = guest_to_host(iobuf[len]);
            if (!isprint(c) && c != 0x0a && c != 0x0d) c = SPACE;
            iobuf[len] = c;
        } /* end for(len) */

        /* Perform end of record processing if not data-chaining */
        if ((flags & CCW_FLAGS_CD) == 0)
        {
            /* Append carriage return and newline if required */
            if (code == 0x09)
            {
                iobuf[len++] = '\r';
                iobuf[len++] = '\n';
            }

        } /* end if(!data-chaining) */

        /* Send the data to the client */
        rc = send_packet (dev->fd, iobuf, len, NULL);
        if (rc < 0)
        {
            dev->sense[0] = SENSE_EC;
            *unitstat = CSW_CE | CSW_DE | CSW_UC;
            break;
        }

        /* Return normal status */
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x03:
    /*---------------------------------------------------------------*/
    /* CONTROL NO-OPERATION                                          */
    /*---------------------------------------------------------------*/
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x0A:
    /*---------------------------------------------------------------*/
    /* READ INQUIRY                                                  */
    /*---------------------------------------------------------------*/

        /* Solicit console input if no data in the device buffer */
        if (dev->keybdrem == 0)
        {
            /* Display prompting message on console */
            len = sprintf (dev->buf,
                    "HHC901I Enter input for console device %4.4X\r\n",
                    dev->devnum);
            rc = send_packet (dev->fd, dev->buf, len, NULL);
            if (rc < 0)
            {
                dev->sense[0] = SENSE_EC;
                *unitstat = CSW_CE | CSW_DE | CSW_UC;
                break;
            }

            /* Accumulate client input data into device buffer */
            while (1) {

                /* Receive client data and increment dev->keybdrem */
                stat = recv_1052_data (dev);

                /* Exit if error or end of line */
                if (stat != 0)
                    break;

            } /* end while */

            /* Exit if error status */
            if (stat != CSW_ATTN)
            {
                *unitstat = (CSW_CE | CSW_DE) | (stat & ~CSW_ATTN);
                break;
            }

        }

        /* Calculate number of bytes to move and residual byte count */
        len = dev->keybdrem;
        num = (count < len) ? count : len;
        *residual = count - num;
        if (count < len) *more = 1;

        /* Copy data from device buffer to channel buffer */
        memcpy (iobuf, dev->buf, num);

        /* If data chaining is specified, save remaining data */
        if ((flags & CCW_FLAGS_CD) && len > count)
        {
            memmove (dev->buf, dev->buf + count, len - count);
            dev->keybdrem = len - count;
        }
        else
        {
            dev->keybdrem = 0;
        }

        /* Return normal status */
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x0B:
    /*---------------------------------------------------------------*/
    /* AUDIBLE ALARM                                                 */
    /*---------------------------------------------------------------*/
        rc = send_packet (dev->fd, "\a", 1, NULL);
        *residual = 0;
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0x04:
    /*---------------------------------------------------------------*/
    /* SENSE                                                         */
    /*---------------------------------------------------------------*/
        /* Calculate residual byte count */
        num = (count < dev->numsense) ? count : dev->numsense;
        *residual = count - num;
        if (count < dev->numsense) *more = 1;

        /* Copy device sense bytes to channel I/O buffer */
        memcpy (iobuf, dev->sense, num);

        /* Clear the device sense bytes */
        memset (dev->sense, 0, sizeof(dev->sense));

        /* Return unit status */
        *unitstat = CSW_CE | CSW_DE;
        break;

    case 0xE4:
    /*---------------------------------------------------------------*/
    /* SENSE ID                                                      */
    /*---------------------------------------------------------------*/
        /* Calculate residual byte count */
        num = (count < dev->numdevid) ? count : dev->numdevid;
        *residual = count - num;
        if (count < dev->numdevid) *more = 1;

        /* Copy device identifier bytes to channel I/O buffer */
        memcpy (iobuf, dev->devid, num);

        /* Return unit status */
        *unitstat = CSW_CE | CSW_DE;
        break;

    default:
    /*---------------------------------------------------------------*/
    /* INVALID OPERATION                                             */
    /*---------------------------------------------------------------*/
        /* Set command reject sense byte, and unit check status */
        dev->sense[0] = SENSE_CR;
        *unitstat = CSW_CE | CSW_DE | CSW_UC;

    } /* end switch(code) */

} /* end function constty_execute_ccw */


DEVHND constty_device_hndinfo = {
        &constty_init_handler,
        &constty_execute_ccw,
        &constty_close_device,
        &constty_query_device,
        NULL, NULL, NULL, NULL
};


DEVHND loc3270_device_hndinfo = {
        &loc3270_init_handler,
        &loc3270_execute_ccw,
        &loc3270_close_device,
        &loc3270_query_device,
        NULL, NULL, NULL, NULL
};

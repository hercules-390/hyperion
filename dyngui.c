/*********************************************************************/
/* DYNGUI.C     Hercules External GUI Interface DLL                  */
/*              (c) Copyright "Fish" (David B. Trout), 2003-2006     */
/*                                                                   */
/* Primary contact:   Fish   [fish@infidels.com]                     */
/*                                                                   */
/*********************************************************************/
/*                                                                   */
/* Change log:                                                       */
/*                                                                   */
/* mm/dd/yy  Description...                                          */
/* --------  ------------------------------------------------------- */
/*                                                                   */
/* 06/22/03  Created.                                                */
/* 05/29/04  Minor fix to UpdateTargetCPU, remove max MIPS rate      */
/*           check in UpdateCPUStatus (trust Herc to be correct),    */
/*           use Herc-calculate MIPS/SIOS rate.                      */
/* 06/01/04  Minor fix to detect switching to a different displayed  */
/*           (target) CPU when no other status has otherwise changed.*/
/* 06/09/04  Minor fix to UpdateDeviceStatus for terminal devices.   */
/* 07/28/04  Minor fix to UpdateDeviceStatus for non-terminal devices*/
/* 10/04/04  Change default maxrates interval to 1440 mins (1 day).  */
/* 03/12/05  Win32 port...                                           */
/* 04/22/05  Win32 port continued...                                 */
/* 04/24/05  Move "maxrates" command and variables to panel.c        */
/* 05/05/05  Fix streams collision issue via locking (gui_fprintf)   */
/* 05/13/05  Fix crash @ shutdown when initiated externally.         */
/* 08/17/05  Moved "high water mark" code to panel.c.                */
/* 09/20/05  sysblk.mipsrate now per second instead of millisecond   */
/* 11/18/05  Fix crash in UpdateTargetCPU when NUMCPU=0 (as it might */
/*           be when, e.g., running as a shared device server mode)  */
/* 11/18/05  Show offline CPUs as OFFLINE.                           */
/* 12/01/05  SetCurrentDirectory support for shell commands          */
/*                                                                   */
/*********************************************************************/

#include "hstdinc.h"
#include "hercules.h"       // (#includes "config." w/#define for VERSION)

#ifdef EXTERNALGUI

#if defined(OPTION_DYNAMIC_LOAD)

#include "devtype.h"
#include "opcode.h"

///////////////////////////////////////////////////////////////////////////////
// Some handy macros...       (feel free to add these to hercules.h)

#ifndef  min
#define  min(a,b)              (((a) <= (b)) ? (a) : (b))
#endif
#ifndef  max
#define  max(a,b)              (((a) >= (b)) ? (a) : (b))
#endif
#define  MINMAX(var,low,high)  ((var) = min(max((var),(low)),(high)))

#ifndef BOOL
#define BOOL  BYTE
#endif
#ifndef FALSE
#define FALSE   0
#endif
#ifndef TRUE
#define TRUE    1
#endif

///////////////////////////////////////////////////////////////////////////////
// Our global variables...    (initialized by our "Initialize" function)

#define  INPUT_STREAM_FILE_PTR    ( stdin  )
#define  OUTPUT_STREAM_FILE_PTR   ( stdout )
#define  STATUS_STREAM_FILE_PTR   ( stderr )
#define  MAX_COMMAND_LEN          (  1024  )
#define  DEF_MAXRATES_RPT_INTVL   (  1440  )

#if defined( WIN32 ) && !defined( HDL_USE_LIBTOOL )
#if !defined( _MSVC_ )
  SYSBLK            *psysblk;                  // (ptr to Herc's SYSBLK structure)
  #define  sysblk  (*psysblk)
#endif
#endif


static FILE*    fOutputStream        = NULL;   // (stdout stream)
static FILE*    fStatusStream        = NULL;   // (stderr stream)
static int      nInputStreamFileNum  =  -1;    // (file descriptor for stdin stream)
static int      gui_nounload         =   1;    // (nounload indicator)

// The device query buffer SHOULD be the maximum device filename length
// plus the maximum descriptive length of any/all options for the device,
// but there is no #define for either so we have no choice but to impose
// our own maximum.

#define  MAX_DEVICEQUERY_LEN      ( 1024 + 256 )

///////////////////////////////////////////////////////////////////////////////
// Some forward references...   (our own functions that we call)

void  Initialize         ();
void  ProcessingLoop     ();
void  Cleanup            ();
void  UpdateTargetCPU    ();
void  ReadInputData      (size_t nTimeoutMillsecs);
void  ProcessInputData   ();
void* gui_panel_command  (char* pszCommand);
void  UpdateStatus       ();
void  UpdateCPUStatus    ();
void  UpdateRegisters    ();
void  UpdateDeviceStatus ();
void  NewUpdateDevStats  ();

void  gui_fprintf( FILE* stream, const char* pszFormat, ... );

///////////////////////////////////////////////////////////////////////////////
// Our main processing loop...

BOOL bDoneProcessing = FALSE;       // (set to TRUE to exit)

void ProcessingLoop()
{
    // Our main purpose in life: read input stream and process
    // any commands that may be entered, and send periodic status
    // information back to the external gui via its status stream.

    // Note we only exit whenever our bDoneProcessing flag is set
    // which is normally not done until just before Herc unloads
    // us which is normally not done until immediately before it
    // terminates.

    // Also note we re-retrieve sysblk.panrate each iteration
    // since it could change from one iteration to the next as a result
    // of the Hercules "panrate" command being entered and processed.

    while (!bDoneProcessing)
    {
        UpdateTargetCPU();      // ("cpu" command could have changed it)
        UpdateStatus();         // (keep sending status back to gui...)

        ReadInputData( sysblk.panrate );

        ProcessInputData();     // (if there even is any of course...)
    }
}

///////////////////////////////////////////////////////////////////////////////

REGS*   pTargetCPU_REGS     = NULL; // target CPU for commands and displays
REGS*   pPrevTargetCPU_REGS = NULL; // target CPU for commands and displays
int     pcpu = 0, prev_pcpu = 0;    // target cpu#

///////////////////////////////////////////////////////////////////////////////

void  UpdateTargetCPU ()
{
    if (sysblk.shutdown) return;

    // Use the requested CPU for our status information
    // unless it's no longer online (enabled), in which case
    // we'll default to the first one we find that's online

    //     sysblk.cpus      =   number online cpu's
    //     sysblk.pcpu      =   panel target cpu
    //     pTargetCPU_REGS  ->  panel target cpu

    obtain_lock (&sysblk.intlock);

    if (pcpu != sysblk.pcpu)
        pcpu  = sysblk.pcpu;

    if (pcpu >= MAX_CPU)
        pcpu = sysblk.pcpu = 0;

    if (sysblk.cpus && pcpu >= 0 && IS_CPU_ONLINE(pcpu))
        pTargetCPU_REGS = sysblk.regs[pcpu];
    else
    {
        // We *MUST* have a cpu and registers to work with!

        pTargetCPU_REGS = &sysblk.dummyregs;

        if (pcpu >= 0)
            pcpu = -pcpu; // (indicates dummy/offline cpu)
    }

    // If SIE is active, use the guest regs rather than the host regs...

#if defined(_FEATURE_SIE)
    if (pTargetCPU_REGS->sie_active)
        pTargetCPU_REGS = pTargetCPU_REGS->guestregs;
#endif

    release_lock(&sysblk.intlock);
}

///////////////////////////////////////////////////////////////////////////////

char*  pszInputBuff    = NULL;                  // ptr to buffer
int    nInputBuffSize  = (MAX_COMMAND_LEN+1);   // how big the buffer is
int    nInputLen       = 0;                     // amount of data it's holding

///////////////////////////////////////////////////////////////////////////////

void ReadInputData ( size_t  nTimeoutMillsecs )
{
    size_t  nMaxBytesToRead;
    int     nBytesRead;
    char*   pReadBuffer;

    // Wait for keyboard input data to arrive...

#if !defined( _MSVC_ )

    fd_set          input_fd_set;
    struct timeval  wait_interval_timeval;
    int             rc;

    FD_ZERO ( &input_fd_set );
    FD_SET  ( nInputStreamFileNum, &input_fd_set );

    wait_interval_timeval.tv_sec  =  nTimeoutMillsecs / 1000;
    wait_interval_timeval.tv_usec = (nTimeoutMillsecs % 1000) * 1000;

    if ((rc = select( nInputStreamFileNum+1, &input_fd_set, NULL, NULL, &wait_interval_timeval )) < 0)
    {
        if (HSO_EINTR == HSO_errno)
            return;             // (we were interrupted by a signal)

        // A bona fide error occurred; abort...

        logmsg
        (
            _("HHCDG003S select failed on input stream: %s\n")

            ,strerror(HSO_errno)
        );

        bDoneProcessing = TRUE;     // (force main loop to exit)
        return;
    }

    // Has keyboard input data indeed arrived yet?

    if (!FD_ISSET( nInputStreamFileNum, &input_fd_set ))
        return;     // (nothing for us to do...)

#endif // !defined( _MSVC_ )

    // Ensure our buffer never overflows... (-2 because
    // we need room for at least 1 byte + NULL terminator)

    MINMAX(nInputLen,0,(nInputBuffSize-2));

    // Read input data into next available buffer location...
    // (nMaxBytesToRead-1 == room for NULL terminator)

    pReadBuffer     = (pszInputBuff   + nInputLen);
    nMaxBytesToRead = (nInputBuffSize - nInputLen) - 1;

#if !defined( _MSVC_ )

    if ((nBytesRead = read( nInputStreamFileNum, pReadBuffer, nMaxBytesToRead )) < 0)
    {
        if (EINTR == errno)
            return;             // (we were interrupted by a signal)

        // A bona fide error occurred; abort...

        logmsg
        (
            _("HHCDG004S read failed on input stream: %s\n")

            ,strerror(errno)
        );

        bDoneProcessing = TRUE;     // (force main loop to exit)
        return;
    }

#else // defined( _MSVC_ )

    if ( ( nBytesRead = w32_get_stdin_char( pReadBuffer, nTimeoutMillsecs ) ) <= 0 )
        return;

#endif // !defined( _MSVC_ )

    // Update amount of input data we have and
    // ensure that it's always NULL terminated...

    MINMAX(nBytesRead,0,nInputBuffSize);
    nInputLen += nBytesRead;
    MINMAX(nInputLen,0,(nInputBuffSize-1));
    *(pszInputBuff + nInputLen) = 0;
}

///////////////////////////////////////////////////////////////////////////////

char*  pszCommandBuff    = NULL;                // ptr to buffer
int    nCommandBuffSize  = (MAX_COMMAND_LEN+1); // how big the buffer is
int    nCommandLen       = 0;                   // amount of data it's holding

///////////////////////////////////////////////////////////////////////////////
// Process the data we just read from the input stream...

void  ProcessInputData ()
{
    char*  pNewLineChar;

    // Ensure our buffer is NULL terminated...

    MINMAX(nInputLen,0,(nInputBuffSize-1));
    *(pszInputBuff + nInputLen) = 0;

    // Input commands are delimited by newline characters...

    while (nInputLen && (pNewLineChar = strchr(pszInputBuff,'\n')) != NULL)
    {
        // Extract command from input buffer
        // into our command processing buffer...

        nCommandLen = (pNewLineChar - pszInputBuff);
        MINMAX(nCommandLen,0,(nCommandBuffSize-1));
        memcpy(pszCommandBuff, pszInputBuff, nCommandLen);
        *(pszCommandBuff + nCommandLen) = 0;

        // Process the extracted command...

        // Note that we always call the registered "panel_command" function
        // rather than call our "gui_panel_command" function directly. This
        // is in case some other DLL has overridden OUR command handler...

        panel_command ( pszCommandBuff );   // (call registered handler)

        // Shift remaining data back to beginning of input buffer...

        nInputLen = ((pszInputBuff + nInputLen) - (pNewLineChar+1));
        MINMAX(nInputLen,0,(nInputBuffSize-1));
        memmove(pszInputBuff,pNewLineChar+1,nInputLen);
        *(pszInputBuff + nInputLen) = 0;
    }
}

///////////////////////////////////////////////////////////////////////////////
// (These are actually boolean flags..)

BYTE   gui_wants_gregs       = 1;
BYTE   gui_wants_cregs       = 1;
BYTE   gui_wants_aregs       = 1;
BYTE   gui_wants_fregs       = 1;
BYTE   gui_wants_devlist     = 1;
BYTE   gui_wants_new_devlist = 0;
#if defined(OPTION_MIPS_COUNTING)
BYTE   gui_wants_cpupct      = 0;
#endif

#ifdef OPTION_MIPS_COUNTING
U32    prev_mips_rate  = 0;
U32    prev_sios_rate  = 0;
#endif

///////////////////////////////////////////////////////////////////////////////
// Our Hercules "panel_command" override...

void*  gui_panel_command (char* pszCommand)
{
    void* (*next_panel_command_handler)(char* pszCommand);

    // Special GUI commands start with ']'. At the moment, all these special
    // gui commands tell us is what status information it's interested in...

    if ( ']' != *pszCommand )
        goto NotSpecialGUICommand;

    pszCommand++;                                    // (bump past ']')

    if (strncasecmp(pszCommand,"SCD=",4) == 0)
    {
        SetCurrentDirectory(pszCommand+4);
        return NULL;
    }

    if (strncasecmp(pszCommand,"GREGS=",6) == 0)
    {
        gui_wants_gregs = atoi(pszCommand+6);
        return NULL;
    }

    if (strncasecmp(pszCommand,"CREGS=",6) == 0)
    {
        gui_wants_cregs = atoi(pszCommand+6);
        return NULL;
    }

    if (strncasecmp(pszCommand,"AREGS=",6) == 0)
    {
        gui_wants_aregs = atoi(pszCommand+6);
        return NULL;
    }

    if (strncasecmp(pszCommand,"FREGS=",6) == 0)
    {
        gui_wants_fregs = atoi(pszCommand+6);
        return NULL;
    }

    if (strncasecmp(pszCommand,"DEVLIST=",8) == 0)
    {
        gui_wants_devlist = atoi(pszCommand+8);
        if ( gui_wants_devlist )
            gui_wants_new_devlist = 0;
        return NULL;
    }

    if (strncasecmp(pszCommand,"NEWDEVLIST=",11) == 0)
    {
        gui_wants_new_devlist = atoi(pszCommand+11);
        if ( gui_wants_new_devlist )
            gui_wants_devlist = 0;
        return NULL;
    }

    if (strncasecmp(pszCommand,"MAINSTOR=",9) == 0)
    {
        gui_fprintf(fStatusStream,"MAINSTOR=%"UINT_PTR_FMT"d\n",(uintptr_t)pTargetCPU_REGS->mainstor);

        // Here's a trick! Hercules reports its version number to the GUI
        // by means of the MAINSIZE value! Later releases of HercGUI know
        // to interpret mainsizes less than 1000 as Hercule's version number.
        // Earlier versions of HercGUI will simply try to interpret it as
        // the actual mainsize, but no real harm is done since we immediately
        // send it the CORRECT mainsize immediately afterwards. This allows
        // future versions of HercGUI to know whether the version of Hercules
        // that it's talking to supports a given feature or not. Slick, eh? :)
        gui_fprintf(fStatusStream,"MAINSIZE=%s\n",VERSION);
        gui_fprintf(fStatusStream,"MAINSIZE=%d\n",(U32)sysblk.mainsize);
        return NULL;
    }

#if defined(OPTION_MIPS_COUNTING)
    if (strncasecmp(pszCommand,"CPUPCT=",7) == 0)
    {
        gui_wants_cpupct = atoi(pszCommand+7);
        return NULL;
    }
#endif

    // Silently ignore any unrecognized special GUI commands...

    return NULL;        // (silently ignore it)

NotSpecialGUICommand:

    // Ignore "commands" that are actually just comments (start with '*' or '#')

    if ('*' == pszCommand[0] || '#' == pszCommand[0])
    {
        logmsg("%s\n",pszCommand);      // (log comment to console)
        return NULL;                    // (and otherwise ignore it)
    }

    // Otherwise it's not a command that we handle. Call the next higher
    // level command handler which, under normal circumstances SHOULD be
    // Hercules's "panel_command" function, but which MAY have been over-
    // ridden by yet some OTHER dynamically loaded command handler...

    next_panel_command_handler = HDL_FINDNXT( gui_panel_command );

    if (!next_panel_command_handler)    // (extremely unlikely!)
        return NULL;                    // (extremely unlikely!)

    return  next_panel_command_handler( pszCommand );
}

///////////////////////////////////////////////////////////////////////////////

QWORD  psw, prev_psw;
BYTE   wait_bit;
BYTE   prev_cpustate       = 0xFF;
U64    prev_instcount      = 0;

///////////////////////////////////////////////////////////////////////////////
// Send status information messages back to the gui...

void  UpdateStatus ()
{
    BOOL  bStatusChanged = FALSE;   // (whether or not anything has changed)

    if (sysblk.shutdown) return;

    copy_psw(pTargetCPU_REGS, psw);
    wait_bit = (psw[1] & 0x02);

    // The SYS light and %CPU-Utilization
    // information we send *ALL* the time...

    if (!(0
        || CPUSTATE_STOPPING == pTargetCPU_REGS->cpustate
        || CPUSTATE_STOPPED  == pTargetCPU_REGS->cpustate
    ))
    {
        gui_fprintf(fStatusStream,

            "SYS=%c\n"

            ,wait_bit ? '0' : '1'
        );
    }

#if defined(OPTION_MIPS_COUNTING)
    if (gui_wants_cpupct)
    {
        char  cpupct[10];

        if (CPUSTATE_STOPPED == pTargetCPU_REGS->cpustate)
            strcpy(cpupct,"0");
        else
            snprintf(cpupct,sizeof(cpupct),
                "%1.0f",(100.0 * pTargetCPU_REGS->cpupct));

        if (isdigit(cpupct[0]))
        {
            gui_fprintf(fStatusStream,

                "CPUPCT=%s\n"

                ,cpupct
            );
        }
    }
#endif

    // Determine if we need to inform the GUI of anything...

    bStatusChanged = FALSE;     // (whether or not anything has changed)

    if (0
        || pTargetCPU_REGS != pPrevTargetCPU_REGS
        || pcpu != prev_pcpu
        || memcmp(prev_psw, psw, sizeof(prev_psw)) != 0
        || prev_cpustate   != pTargetCPU_REGS->cpustate
        || (prev_instcount != (
#if defined(_FEATURE_SIE)
                SIE_MODE(pTargetCPU_REGS) ?  pTargetCPU_REGS->hostregs->instcount :
#endif
                pTargetCPU_REGS->instcount)
           )
    )
    {
        bStatusChanged = TRUE;      // (something has indeed changed...)

        // Save new values for next time...

        pPrevTargetCPU_REGS = pTargetCPU_REGS;
        prev_pcpu = pcpu;
        memcpy(prev_psw, psw, sizeof(prev_psw));
        prev_cpustate = pTargetCPU_REGS->cpustate;
        prev_instcount = (
#if defined(_FEATURE_SIE)
            SIE_MODE(pTargetCPU_REGS) ? pTargetCPU_REGS->hostregs->instcount :
#endif
            pTargetCPU_REGS->instcount);
    }

    // If anything has changed, inform the GUI...

    if (bStatusChanged)
    {
        UpdateCPUStatus();      // (update the status line info...)
        UpdateRegisters();      // (update the registers display...)
    }

    // PROGRAMMING NOTE: my original [rather poorly designed I admit] logic
    // sent device status messages to the GUI *continuously* (i.e. all the
    // time), even when both Herc and the channel subsystem was idle. This
    // proved to be terribly inefficient, causing the GUI to consume *FAR*
    // too much valuable CPU cycles parsing all of those messages.

    // Thus, starting with this version of dyngui, we now only send device
    // status messages to the GUI only whenever the device's status actually
    // changes, but only if it (the GUI) specifically requests such notifi-
    // cations of course (via the new "]NEWDEVLIST=" special message).

    // The new(er) version of HercGUI understands (and thus requests) these
    // newer format device status messages, but older versions of HercGUI
    // of course do not. Thus in order to remain compatible with the current
    // (older) version of the GUI, we still need to support the inefficient
    // technique of constantly sending a constant stream of device status
    // messages.

    // Eventually at some point this existing original inefficient technique
    // logic will be removed (once everyone has had time to upgrade to the
    // newer version of HercGUI), but for now, at least for the next couple
    // of HercGUI release cycles, we need to keep it.

    if (gui_wants_devlist)      // (if the device list is visible)
        UpdateDeviceStatus();   // (update the list of devices...)

    else // (the two options are mutually exclusive from one another)

    if (gui_wants_new_devlist)  // (if the device list is visible)
        NewUpdateDevStats();    // (update the list of devices...)
}

///////////////////////////////////////////////////////////////////////////////
// Send status information messages back to the gui...

void  UpdateCPUStatus ()
{
    if (sysblk.shutdown) return;

    if (pTargetCPU_REGS == &sysblk.dummyregs || pcpu < 0)
    {
        // pTargetCPU_REGS == &sysblk.dummyregs; cpu is offline

        gui_fprintf(fStatusStream, "STATUS="

            "CPU%4.4X (((((((((((((((((((((((( OFFLINE ))))))))))))))))))))))))\n"

            ,pcpu < 0 ? -pcpu : pcpu);
    }
    else // pTargetCPU_REGS != &sysblk.dummyregs; cpu is online
    {

    // CPU status line...  (PSW, status indicators, and instruction count)

    gui_fprintf(fStatusStream, "STATUS="

        "CPU%4.4X "

        "PSW=%2.2X%2.2X%2.2X%2.2X "
            "%2.2X%2.2X%2.2X%2.2X "
            "%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X "

        "%c%c%c%c%c%c%c%c "

        "instcount=%" I64_FMT "u\n"

        ,pTargetCPU_REGS->cpuad

        ,psw[0], psw[1], psw[2],  psw[3]
        ,psw[4], psw[5], psw[6],  psw[7]
        ,psw[8], psw[9], psw[10], psw[11], psw[12], psw[13], psw[14], psw[15]

        ,CPUSTATE_STOPPED == pTargetCPU_REGS->cpustate ? 'M' : '.'
        ,sysblk.inststep                       ? 'T' : '.'
        ,wait_bit                                      ? 'W' : '.'
        ,pTargetCPU_REGS->loadstate                    ? 'L' : '.'
        ,pTargetCPU_REGS->checkstop                    ? 'C' : '.'
        ,PROBSTATE(&pTargetCPU_REGS->psw)              ? 'P' : '.'
        ,
#if        defined(_FEATURE_SIE)
        SIE_MODE(pTargetCPU_REGS)                      ? 'S' : '.'
#else  // !defined(_FEATURE_SIE)
                                                               '.'
#endif //  defined(_FEATURE_SIE)
        ,
#if        defined(_900)
        ARCH_900 == pTargetCPU_REGS->arch_mode         ? 'Z' : '.'
#else  // !defined(_900)
                                                               '.'
#endif //  defined(_900)
        ,(long long)(
#if       defined(_FEATURE_SIE)
        SIE_MODE(pTargetCPU_REGS) ? pTargetCPU_REGS->hostregs->instcount :
#endif // defined(_FEATURE_SIE)
        pTargetCPU_REGS->instcount)
    );

    } // endif cpu is online/offline

    // MIPS rate and SIOS rate...

#if defined(OPTION_MIPS_COUNTING)

    // MIPS rate...

    if (sysblk.mipsrate != prev_mips_rate)
    {
        gui_fprintf(fStatusStream,

            "MIPS=%2.1d.%2.2d\n"

            , sysblk.mipsrate / 1000000
            ,(sysblk.mipsrate % 1000000) / 10000
        );

        prev_mips_rate = sysblk.mipsrate;
    }

    // SIOS rate...

    if (sysblk.siosrate != prev_sios_rate)
    {
        gui_fprintf(fStatusStream,

            "SIOS=%5d\n"

            ,sysblk.siosrate
        );

        prev_sios_rate = sysblk.siosrate;
    }

    update_maxrates_hwm(); // (update high-water-mark values)

#endif // defined(OPTION_MIPS_COUNTING)
}

///////////////////////////////////////////////////////////////////////////////
// Send status information messages back to the gui...

void  UpdateRegisters ()
{
    if (sysblk.shutdown) return;

    if (gui_wants_gregs)
    {
        gui_fprintf(fStatusStream,

            "GR0-3=%8.8X %8.8X %8.8X %8.8X\n"
            "GR4-7=%8.8X %8.8X %8.8X %8.8X\n"
            "GR8-B=%8.8X %8.8X %8.8X %8.8X\n"
            "GRC-F=%8.8X %8.8X %8.8X %8.8X\n"

            ,pTargetCPU_REGS->GR_L(0)
            ,pTargetCPU_REGS->GR_L(1)
            ,pTargetCPU_REGS->GR_L(2)
            ,pTargetCPU_REGS->GR_L(3)
            ,pTargetCPU_REGS->GR_L(4)
            ,pTargetCPU_REGS->GR_L(5)
            ,pTargetCPU_REGS->GR_L(6)
            ,pTargetCPU_REGS->GR_L(7)
            ,pTargetCPU_REGS->GR_L(8)
            ,pTargetCPU_REGS->GR_L(9)
            ,pTargetCPU_REGS->GR_L(10)
            ,pTargetCPU_REGS->GR_L(11)
            ,pTargetCPU_REGS->GR_L(12)
            ,pTargetCPU_REGS->GR_L(13)
            ,pTargetCPU_REGS->GR_L(14)
            ,pTargetCPU_REGS->GR_L(15)
        );
    }

    if (gui_wants_cregs)
    {
        gui_fprintf(fStatusStream,

            "CR0-3=%8.8X %8.8X %8.8X %8.8X\n"
            "CR4-7=%8.8X %8.8X %8.8X %8.8X\n"
            "CR8-B=%8.8X %8.8X %8.8X %8.8X\n"
            "CRC-F=%8.8X %8.8X %8.8X %8.8X\n"

            ,pTargetCPU_REGS->CR_L(0)
            ,pTargetCPU_REGS->CR_L(1)
            ,pTargetCPU_REGS->CR_L(2)
            ,pTargetCPU_REGS->CR_L(3)
            ,pTargetCPU_REGS->CR_L(4)
            ,pTargetCPU_REGS->CR_L(5)
            ,pTargetCPU_REGS->CR_L(6)
            ,pTargetCPU_REGS->CR_L(7)
            ,pTargetCPU_REGS->CR_L(8)
            ,pTargetCPU_REGS->CR_L(9)
            ,pTargetCPU_REGS->CR_L(10)
            ,pTargetCPU_REGS->CR_L(11)
            ,pTargetCPU_REGS->CR_L(12)
            ,pTargetCPU_REGS->CR_L(13)
            ,pTargetCPU_REGS->CR_L(14)
            ,pTargetCPU_REGS->CR_L(15)
        );
    }

    if (gui_wants_aregs)
    {
        gui_fprintf(fStatusStream,

            "AR0-3=%8.8X %8.8X %8.8X %8.8X\n"
            "AR4-7=%8.8X %8.8X %8.8X %8.8X\n"
            "AR8-B=%8.8X %8.8X %8.8X %8.8X\n"
            "ARC-F=%8.8X %8.8X %8.8X %8.8X\n"

            ,pTargetCPU_REGS->AR(0)
            ,pTargetCPU_REGS->AR(1)
            ,pTargetCPU_REGS->AR(2)
            ,pTargetCPU_REGS->AR(3)
            ,pTargetCPU_REGS->AR(4)
            ,pTargetCPU_REGS->AR(5)
            ,pTargetCPU_REGS->AR(6)
            ,pTargetCPU_REGS->AR(7)
            ,pTargetCPU_REGS->AR(8)
            ,pTargetCPU_REGS->AR(9)
            ,pTargetCPU_REGS->AR(10)
            ,pTargetCPU_REGS->AR(11)
            ,pTargetCPU_REGS->AR(12)
            ,pTargetCPU_REGS->AR(13)
            ,pTargetCPU_REGS->AR(14)
            ,pTargetCPU_REGS->AR(15)
        );
    }

    if (gui_wants_fregs)
    {
        gui_fprintf(fStatusStream,

            "FR0-2=%8.8X %8.8X %8.8X %8.8X\n"
            "FR4-6=%8.8X %8.8X %8.8X %8.8X\n"

            ,pTargetCPU_REGS->fpr[0]
            ,pTargetCPU_REGS->fpr[1]
            ,pTargetCPU_REGS->fpr[2]
            ,pTargetCPU_REGS->fpr[3]
            ,pTargetCPU_REGS->fpr[4]
            ,pTargetCPU_REGS->fpr[5]
            ,pTargetCPU_REGS->fpr[6]
            ,pTargetCPU_REGS->fpr[7]
        );
    }
}

///////////////////////////////////////////////////////////////////////////////

char  szQueryDeviceBuff[ MAX_DEVICEQUERY_LEN + 1 ]; // (always +1 for safety!)

///////////////////////////////////////////////////////////////////////////////
// Send status information messages back to the gui...  (VERY inefficient!)

void  UpdateDeviceStatus ()
{
    DEVBLK* pDEVBLK;
    char*   pDEVClass;
    BYTE    chOnlineStat, chBusyStat, chPendingStat, chOpenStat;

    if (sysblk.shutdown) return;

    // Process ALL the devices in the entire configuration each time...

    for (pDEVBLK = sysblk.firstdev; pDEVBLK != NULL; pDEVBLK = pDEVBLK->nextdev)
    {
        // Does this device actually exist in the configuration?

        if (!pDEVBLK->allocated || !(pDEVBLK->pmcw.flag5 & PMCW5_V))
            continue;   // (no, skip)

        // Retrieve this device's filename and optional settings parameter values...

        szQueryDeviceBuff[MAX_DEVICEQUERY_LEN] = 0; // (buffer allows room for 1 extra)

        (pDEVBLK->hnd->query)(pDEVBLK, &pDEVClass, MAX_DEVICEQUERY_LEN, szQueryDeviceBuff);

        if (0 != szQueryDeviceBuff[MAX_DEVICEQUERY_LEN])    // (buffer overflow?)
        {
            logmsg
            (
                _("HHCDG005E Device query buffer overflow! (device=%4.4X)\n")

                ,pDEVBLK->devnum

            );
        }

        szQueryDeviceBuff[MAX_DEVICEQUERY_LEN] = 0;   // (enforce NULL termination)

        // Device status flags...
                                                                              chOnlineStat  =
                                                                              chBusyStat    =
                                                                              chPendingStat =
                                                                              chOpenStat    = '0';

        if ((!pDEVBLK->console && pDEVBLK->fd >= 0) ||
            ( pDEVBLK->console && pDEVBLK->connected))                        chOnlineStat  = '1';
        if (pDEVBLK->busy)                                                    chBusyStat    = '1';
        if (IOPENDING(pDEVBLK))                                               chPendingStat = '1';
        if (pDEVBLK->fd > max(STDIN_FILENO,max(STDOUT_FILENO,STDERR_FILENO))) chOpenStat    = '1';

        // Send status message back to gui...

        gui_fprintf(fStatusStream,

            "DEV=%4.4X %4.4X %-4.4s %c%c%c%c %s\n"

            ,pDEVBLK->devnum
            ,pDEVBLK->devtype
            ,pDEVClass

            ,chOnlineStat
            ,chBusyStat
            ,chPendingStat
            ,chOpenStat

            ,szQueryDeviceBuff
        );
    }

    // Since the device list can be in any order and devices can be added
    // and/or removed at any time, the GUI needs to know "That's all the
    // devices there are" so that it can detect when devices are removed...

    gui_fprintf(fStatusStream, "DEV=X\n");    // (indicates end of list)
}

///////////////////////////////////////////////////////////////////////////////
// Send device status msgs to the gui IF NEEDED...  (slightly more efficient)

#ifdef EXTERNALGUI
void  NewUpdateDevStats ()
{
    DEVBLK*   pDEVBLK;
    GUISTAT*  pGUIStat;
    char*     pDEVClass;
    BYTE      chOnlineStat, chBusyStat, chPendingStat, chOpenStat;
    BOOL      bUpdatesSent = FALSE;

    if (sysblk.shutdown) return;

    // Process ALL the devices in the entire configuration each time...

    // (But only send device status messages to the GUI only when the
    // device's status actually changes and not continuously like before)

    for (pDEVBLK = sysblk.firstdev; pDEVBLK != NULL; pDEVBLK = pDEVBLK->nextdev)
    {
        pGUIStat = pDEVBLK->pGUIStat;

        // Does this device exist in the configuration?

        if (!pDEVBLK->allocated || !(pDEVBLK->pmcw.flag5 & PMCW5_V))
        {
            // This device no longer exists in the configuration...
            // If we haven't yet notified the GUI about this device
            // being deleted from the configuration, then do so at
            // this time...

            if (*pGUIStat->pszNewStatStr)
            {
                // Send "device deleted" message...

                gui_fprintf ( fStatusStream, "DEVD=%4.4X\n", pDEVBLK->devnum );
                bUpdatesSent = TRUE;

                *pGUIStat->pszNewStatStr = 0;   // (prevent re-reporting it)
                *pGUIStat->pszOldStatStr = 0;   // (prevent re-reporting it)
            }

            continue;   // (go on to next device)
        }

        // Retrieve this device's filename and optional settings parameter values...

        szQueryDeviceBuff[MAX_DEVICEQUERY_LEN] = 0; // (buffer allows room for 1 extra)

        (pDEVBLK->hnd->query)(pDEVBLK, &pDEVClass, MAX_DEVICEQUERY_LEN, szQueryDeviceBuff);

        if (0 != szQueryDeviceBuff[MAX_DEVICEQUERY_LEN])    // (buffer overflow?)
        {
            logmsg
            (
                _("HHCDG005E Device query buffer overflow! (device=%4.4X)\n")

                ,pDEVBLK->devnum

            );
        }

        szQueryDeviceBuff[MAX_DEVICEQUERY_LEN] = 0;   // (enforce NULL termination)

        // Device status flags...
                                                                              chOnlineStat  =
                                                                              chBusyStat    =
                                                                              chPendingStat =
                                                                              chOpenStat    = '0';

        if ((!pDEVBLK->console && pDEVBLK->fd >= 0) ||
            ( pDEVBLK->console && pDEVBLK->connected))                        chOnlineStat  = '1';
        if (pDEVBLK->busy)                                                    chBusyStat    = '1';
        if (IOPENDING(pDEVBLK))                                               chPendingStat = '1';
        if (pDEVBLK->fd > max(STDIN_FILENO,max(STDOUT_FILENO,STDERR_FILENO))) chOpenStat    = '1';

        // Build a new "device added" or "device changed"
        // status string for this device...

        snprintf( pGUIStat->pszNewStatStr, GUI_STATSTR_BUFSIZ,

            "DEV%c=%4.4X %4.4X %-4.4s %c%c%c%c %s"

            ,*pGUIStat->pszOldStatStr ? 'C' : 'A'
            ,pDEVBLK->devnum
            ,pDEVBLK->devtype
            ,pDEVClass

            ,chOnlineStat
            ,chBusyStat
            ,chPendingStat
            ,chOpenStat

            ,szQueryDeviceBuff
        );

        *(pGUIStat->pszNewStatStr + GUI_STATSTR_BUFSIZ - 1) = 0;

        // If the new status string is different from the old one,
        // then send the new one to the GUI and swap buffer ptrs
        // for next time. In this way we only send device status
        // msgs to the GUI only when the status actually changes...

        if ( strcmp( pGUIStat->pszNewStatStr, pGUIStat->pszOldStatStr ) )
        {
            gui_fprintf ( fStatusStream, "%s\n", pGUIStat->pszNewStatStr );
            bUpdatesSent = TRUE;
            {
                register char*
                          pszSavStatStr = pGUIStat->pszNewStatStr;
                pGUIStat->pszNewStatStr = pGUIStat->pszOldStatStr;
                pGUIStat->pszOldStatStr =           pszSavStatStr;
            }
        }
    }

    if ( bUpdatesSent )
        gui_fprintf(fStatusStream, "DEVX=\n");  // (send end-of-batch indicator)
}
#endif

///////////////////////////////////////////////////////////////////////////////
// Our Hercules "debug_cpu_state" override...

// The following function fixes an unintentional problem caused by the new
// logger mechanism (wherein stdout and stderr now point to the same stream)
// due to a oversight (bug) on my part wherein the 'LOAD' and 'MAN' messages
// are being [mistakenly] written to stdout instead of stderr (where they
// normally should be). The current version of the gui expects both messages
// to come in on the stdout stream, but due to the recent logger changes, they
// now come in on the stderr stream instead (because stdout was duped to stderr
// by the new logger logic) thus causing the gui to miss seeing them without
// the below fix. The below fix simply corrects for this by simply writing the
// two messages to the stdout stream where the current gui expects to see them.

void*  gui_debug_cpu_state ( REGS* pREGS )
{
void *(*next_debug_call)(REGS *);

    static BOOL bLoading = FALSE;
    static BOOL bStopped = FALSE;

    if (sysblk.shutdown) return NULL;

    if (pTargetCPU_REGS && pREGS != pTargetCPU_REGS)
        return NULL;

    if (bLoading != (pREGS->loadstate ? TRUE : FALSE))
    {
        bLoading  = (pREGS->loadstate ? TRUE : FALSE);
        gui_fprintf(stdout,"LOAD=%c\n", bLoading ? '1' : '0');
    }

    if (bStopped != ((CPUSTATE_STOPPED == pREGS->cpustate) ? TRUE : FALSE))
    {
        bStopped  = ((CPUSTATE_STOPPED == pREGS->cpustate) ? TRUE : FALSE);
        gui_fprintf(stdout,"MAN=%c\n", bStopped ? '1' : '0');
    }

    if((next_debug_call = HDL_FINDNXT( gui_debug_cpu_state )))
        return next_debug_call( pREGS );

    return NULL;    // (I have no idea why this is a void* func)
}

///////////////////////////////////////////////////////////////////////////////
// Streams 'fprintf' function to prevent interleaving collision problem...

LOCK gui_fprintf_lock;

void gui_fprintf( FILE* stream, const char* pszFormat, ... )
{
    va_list vl;
    va_start( vl, pszFormat );
    obtain_lock ( &gui_fprintf_lock );
    vfprintf( stream, pszFormat, vl ); 
    fflush( stream );  
    release_lock( &gui_fprintf_lock );
}

///////////////////////////////////////////////////////////////////////////////
// Acquire any resources we need in order to operate...
// (called by 'gui_panel_display' before main loop initiates...)

void  Initialize ()
{
    initialize_lock( &gui_fprintf_lock );

    // reject any unload attempt

    gui_nounload = 1;

    // Initialize streams...

    fOutputStream = OUTPUT_STREAM_FILE_PTR;
    fStatusStream = STATUS_STREAM_FILE_PTR;

    nInputStreamFileNum = fileno( INPUT_STREAM_FILE_PTR );

    // Allocate input stream buffer...

    if (!(pszInputBuff = (char *) malloc( nInputBuffSize )))
    {
        fprintf(stderr,
            _("HHCDG006S malloc pszInputBuff failed: %s\n")
            ,strerror(errno));
        exit(0);
    }

    memset(pszInputBuff,0,nInputBuffSize);
    nInputLen = 0;

    // Allocate command processing buffer...

    if (!(pszCommandBuff = (char *) malloc( nCommandBuffSize )))
    {
        fprintf(stderr,
            _("HHCDG007S malloc pszCommandBuff failed: %s\n")
            ,strerror(errno));
        exit(0);
    }

    memset(pszCommandBuff,0,nCommandBuffSize);
    nCommandLen = 0;
}

///////////////////////////////////////////////////////////////////////////////
// Release any resources we acquired in order to operate...
// (called by 'gui_panel_display' when main loop terminates...)

void Cleanup()
{
    if  (pszInputBuff)
    free(pszInputBuff);

    if  (pszCommandBuff)
    free(pszCommandBuff);
}

///////////////////////////////////////////////////////////////////////////////
// Hercules  "daemon_task"  -or-  "panel_display"  override...

void gui_panel_display ()
{
    if ( !bDoneProcessing )
    {
        logmsg(_("HHCDG001I dyngui.dll initiated\n"));
        Initialize();               // (allocate buffers, etc)
        ProcessingLoop();           // (primary processing loop)
        logmsg(_("HHCDG002I dyngui.dll terminated\n"));
        Cleanup();                  // (de-allocate resources)
    }
}

/*****************************************************************************\
                 Hercules Dynamic Loader control sections...
\*****************************************************************************/
/*
   Note that ALL of the below "sections" are actually just simple functions
   that Hercules's dynamic loader logic calls, thus allowing you to insert
   whatever 'C' code you may need directly into any of the below sections.
*/
///////////////////////////////////////////////////////////////////////////////
//                        HDL_DEPENDENCY_SECTION
// The following are the various Hercules structures whose layout this module
// depends on. The layout of the following structures (size and version) MUST
// match the layout that was used to build Hercules with. If the size/version
// of any of the following structures changes (and a new version of Hercules
// is built using the new layout), then THIS module must also be built with
// the new layout as well. The layout (size/version) of the structure as it
// was when Hercules was built MUST MATCH the layout as it was when THIS DLL
// was built)

/* Libtool static name colision resolution */
/* note : lt_dlopen will look for symbol & modulename_LTX_symbol */
#if !defined(HDL_BUILD_SHARED) && defined(HDL_USE_LIBTOOL)
#define hdl_ddev dyngui_LTX_hdl_ddev
#define hdl_depc dyngui_LTX_hdl_depc
#define hdl_reso dyngui_LTX_hdl_reso
#define hdl_init dyngui_LTX_hdl_init
#define hdl_fini dyngui_LTX_hdl_fini
#endif

HDL_DEPENDENCY_SECTION;

HDL_DEPENDENCY ( HERCULES );        // hercules itself
HDL_DEPENDENCY ( SYSBLK   );        // master control block
HDL_DEPENDENCY ( REGS     );        // cpu regs and such
HDL_DEPENDENCY ( DEVBLK   );        // device info block

END_DEPENDENCY_SECTION;

///////////////////////////////////////////////////////////////////////////////
//                        HDL_REGISTER_SECTION
// The following section defines the entry points within Hercules that THIS
// module is overriding (replacing). That is to say, THIS module's functions
// will be called by Hercules instead of the normal Hercules function (if any).
// The functions defined below thus provide additional/different functionality
// above/beyond the functionality normally provided by Hercules. (Of course,
// yet OTHER dlls may have further overridden whatever overrides we register
// here, such as would likely be the case for panel command overrides).

HDL_REGISTER_SECTION;       // ("Register" our entry-points)

//             Hercules's       Our
//             registered       overriding
//             entry-point      entry-point
//             name             value

HDL_REGISTER ( panel_display,   gui_panel_display   );// (Yep! We override EITHER!)
HDL_REGISTER ( daemon_task,     gui_panel_display   );// (Yep! We override EITHER!)
HDL_REGISTER ( debug_cpu_state, gui_debug_cpu_state );
HDL_REGISTER ( panel_command,   gui_panel_command   );

END_REGISTER_SECTION;

#if defined( WIN32 ) && !defined( HDL_USE_LIBTOOL )
#if !defined( _MSVC_ )
  #undef sysblk
#endif
  ///////////////////////////////////////////////////////////////////////////////
  //                        HDL_RESOLVER_SECTION
  // The following section "resolves" entry-points that this module needs. The
  // below HDL_RESOLVE entries define the names of Hercules's registered entry-
  // points that we need "imported" to us (so that we may call those functions
  // directly ourselves). The HDL_RESOLVE_PTRVAR entries set the named pointer
  // variable value (i.e. the name of OUR pointer variable) to the registered
  // entry-point value that was registered by Hercules or some other DLL.

  HDL_RESOLVER_SECTION;       // ("Resolve" needed entry-points)

  //            Registered
  //            entry-points
  //            that we call
  HDL_RESOLVE ( panel_command );

#if !defined( _MSVC_ )
  //                    Our pointer-     Registered entry-
  //                    variable name    point value name
  HDL_RESOLVE_PTRVAR (  psysblk,           sysblk         );
#endif

  END_RESOLVER_SECTION;
#endif

///////////////////////////////////////////////////////////////////////////////
//                        HDL_FINAL_SECTION
// The following section defines what should be done immediately before this
// module is unloaded. It is nothing more than a function that is called by
// Hercules just before your module is unloaded. You can do anything you want
// but the normal thing to do is release any resources that were acquired when
// your module was loaded (e.g. release memory that was malloc'ed, etc).

HDL_FINAL_SECTION;
{
    bDoneProcessing = TRUE;     // (tell main loop to stop processing)

    usleep(100000);             // (brief delay to give GUI time
                                //  to display ALL shutdown msgs)
    return gui_nounload;        // (reject unloads when activated)
}
END_FINAL_SECTION;

///////////////////////////////////////////////////////////////////////////////

#endif /*defined(OPTION_DYNAMIC_LOAD)*/

#endif // EXTERNALGUI

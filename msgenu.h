/*----------------------------------------------------------------------------*/
/* (c) Copyright Roger Bowler and others 1999-2010                            */
/* Hercules-390 mainframe emulator                                            */
/*                                                                            */
/* file: msgenu.h                                                             */
/*                                                                            */
/* (c) Copyright Bernard van der Helm 2010                                    */
/* Main header file for Hercules messages.                                    */
/*----------------------------------------------------------------------------*/

#define MSG(id, ...)      #id " " id "\n", ## __VA_ARGS__
#define WRITEMSG(id, ...) logmsg(#id " " id "\n", ## __VA_ARGS__)

/* crypto/dyncrypt.c */
#define HHCRY001I "Crypto module loaded (c) Copyright Bernard van der Helm, 2003-2010"
#define HHCRY002I "Active: %s"

/* hdl.c */
#define HHCHD001E "Registration malloc failed for %s"
#define HHCHD005E "Module %s already loaded\n"
#define HHCHD006S "Cannot allocate memory for DLL descriptor: %s"
#define HHCHD007E "Unable to open DLL %s: %s"
#define HHCHD008E "Device %4.4X bound to %s"
#define HHCHD009E "Module %s not found"
#define HHCHD010I "Dependency check failed for %s, version(%s) expected(%s)"
#define HHCHD011I "Dependency check failed for %s, size(%d) expected(%d)"
#define HHCHD013E "No dependency section in %s: %s"
#define HHCHD014E "Dependency check failed for module %s"
#define HHCHD015E "Unloading of %s not allowed"
#define HHCHD016E "DLL %s is duplicate of %s"
#define HHCHD017E "Unload of %s rejected by final section"
#define HHCHD018I "Loadable module directory is %s"
#define HHCHD900I "Begin shutdown sequence"
#define HHCHD901I "Calling %s"
#define HHCHD902I "%s complete"
#define HHCHD909I "Shutdown sequence complete"
#define HHCHD950I "Begin HDL termination sequence"
#define HHCHD951I "Calling module %s cleanup routine"
#define HHCHD952I "Module %s cleanup complete"
#define HHCHD959I "HDL Termination sequence complete"

/* hostinfo.c */
#define HHCIN015I "%s"

/* httpserv.c */
#define HHCHT001I "Thread started: tid " TIDPAT ", pid %d, prio %d, name %s"
#define HHCHT002E "Socket error: %s"
#define HHCHT003W "Waiting for port %u to become free"
#define HHCHT004E "Bind error: %s"
#define HHCHT005E "Listen error: %s"
#define HHCHT006I "Waiting for HTTP requests on port %u"
#define HHCHT007E "Select error: %s"
#define HHCHT008E "Accept error: %s"
#define HHCHT009I "Thread ended: tid " TIDPAT ", pid %d, prio %d, name %s"
#define HHCHT010E "Http_request create_thread error: %s"
#define HHCHT011E "Html_include: Cannot open %s: %s"
#define HHCHT013I "Using HTTPROOT directory \"%s\""
#define HHCCF066E "Invalid HTTPROOT: \"%s\": %s"

/* version.c */
#define HHCIN010I "%sversion %s"
#define HHCIN011I "%s"
#define HHCIN012I "Built on %s at %s"
#define HHCIN013I "Build information:"
#define HHCIN014I "  %s"

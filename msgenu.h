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

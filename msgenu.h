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

/* version.c */
#define HHCIN010I "%sversion %s"
#define HHCIN011I "%s"
#define HHCIN012I "Built on %s at %s"
#define HHCIN013I "Build information:"
#define HHCIN014I "  %s"

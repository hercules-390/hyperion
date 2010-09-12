/* MSGENU.H     (c) Copyright Bernard van der Helm, 2010             */
/*              (c) Copyright TurboHercules, SAS 2010                */
/*              Header file for Hercules messages (US English)       */
/* Message text (c) Copyright Roger Bowler and others, 1999-2010     */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$ 

/*-------------------------------------------------------------------*/
/* This file contains the text of all of the messages issued by      */
/* the various components of the Hercules mainframe emulator.        */
/*-------------------------------------------------------------------*/

/*-------------------------------------------------------------------
Message principles:
-----------------------------------------------------------------------
HHCnnnnns Message text

-----------------------------------------------------------------------
Prefix principles
-----------------------------------------------------------------------
HHC:   'HHC' is the standard prefix for all Hercules messages.
nnnnn: Five numeric digit code that unifies the Hercules message.
s:     The capital letter that denotes the severity.
' ':   One space character between the message prefix and Text.

-----------------------------------------------------------------------
Severity principles
-----------------------------------------------------------------------
S: Severe error, this could terminate Hercules.
E: Error, hercules continues.
W: Warning message.
I: Informatonal message.
A: Action, Hercules needs input.
D: Debug message.

-----------------------------------------------------------------------
Text principles
-----------------------------------------------------------------------
Text: 
1. The text is written in lower characters and starts with a 
   capital and does not end with a '.', '!' or something else.
2. Added strings are rounded with apostrophes to delimiter it from
   the text and to see the start and endpoint of the string.

Examples:
"HHC01234I This is a correct message"
"HHC12345E This is wrong!"
"HHC23456E THIS IS ALSO WRONG"
"HHC34567E Do not end with a dot or somehting else!"
"HHC45678E This string %s cannot be seen and is therefore wrong", ""
"HHC56789I This is a '%s' better example", "much"

-----------------------------------------------------------------------
Message format principles:
-----------------------------------------------------------------------
Device:     "%1d:%04X", lcssnum, devnum
Processor:  "%s%02X", PTYPSTR(procnum), procnum
32bit reg:  "%08X", r32
64bit reg:  "%016X", r64 (in 64bit mode)
64bit reg:  "--------%08X", r32 (under SIE in 32 bit)
64bit psw:  "%016X", psw64
64bit psw:  "%016X ----------------", psw32 (under SIE in 32 bit)
128bit psw: '%016X %016X", psw64h, psw64l
Regs:       "GR%02d CR%02d AR%02d FP%02d", grnum, crnum, arnum, fpnum
Strings:    '%s', ""
-----------------------------------------------------------------------
DEBUG
-----------------------------------------------------------------------
If DEBUG is defined, either by #define or configure --enable-debug
and OPTION_DEBUG_MESSAGES is enabled in featall.h then messages
will be prefixed by sourcefile.c:lineno: where the message originates
cpu.c:123:HABC1234I This is a message
---------------------------------------------------------------------*/

// I'm not sure if this is needed, but __FUNCTION__ wasn't part of the standard until C99
#if __STDC_VERSION__ < 199901L
#if __GNUC__ >= 2
#define __func__ __FUNCTION__
#else
#define __func__ "<unknown>"
#endif
#endif

/* Use these macro's */
#define MSGBUF(buf, ...)             snprintf(buf, sizeof(buf)-1, ## __VA_ARGS__)
#define MSG(id, s, ...)              #id s " " id "\n", ## __VA_ARGS__
#define MSG_C(id, s, ...)            #id s " " id "", ## __VA_ARGS__
#define WRMSG(id, s, ...)            writemsg(__FILE__, __LINE__, __FUNCTION__, 0, MLVL(DEBUG), "", _(#id s " " id "\n"), ## __VA_ARGS__)
#define WRMSG_C(id, s, ...)          writemsg(__FILE__, __LINE__, __FUNCTION__, 0, MLVL(DEBUG), "", _(#id s " " id ""), ## __VA_ARGS__)
#define WRCMSG(color, id, s, ...)    writemsg(__FILE__, __LINE__, __FUNCTION__, 0, MLVL(DEBUG), color, _(#id s " " id "\n"), ## __VA_ARGS__)
#define WRCMSG_C(color, id, s, ...)  writemsg(__FILE__, __LINE__, __FUNCTION__, 0, MLVL(DEBUG), color, _(#id s " " id ""), ## __VA_ARGS__)

#define WRGMSG_ON \
{ \
  while(try_obtain_lock(&sysblk.msglock)) \
  { \
    usleep(100); \
    if(host_tod() - sysblk.msglocktime > 1000000) \
    { \
      release_lock(&sysblk.msglock); \
      WRMSG(HHC00016, "E"); \
    } \
  } \
  sysblk.msglocktime = host_tod(); \
  sysblk.msggrp = 1; \
}
#define WRGMSG(id, s, ...)           writemsg(__FILE__, __LINE__, __FUNCTION__, 1, MLVL(DEBUG), "", _(#id s " " id "\n"), ## __VA_ARGS__)
#define WRGMSG_C(id, s, ...)         writemsg(__FILE__, __LINE__, __FUNCTION__, 1, MLVL(DEBUG), "", _(#id s " " id ""), ## __VA_ARGS__)
#define WRGCMSG(color, id, s, ...)   writemsg(__FILE__, __LINE__, __FUNCTION__, 1, MLVL(DEBUG), color, _(#id s " " id "\n"), ## __VA_ARGS__)
#define WRGCMSG_C(color, id, s, ...) writemsg(__FILE__, __LINE__, __FUNCTION__, 1, MLVL(DEBUG), color, _(#id s " " id ""), ## __VA_ARGS__)
#define WRGMSG_OFF                   { sysblk.msggrp = 0; release_lock(&sysblk.msglock); }

/* Hercules messages */
#define HHC00001 "%s"
#define HHC00002 "SCLP console not receiving '%s'"
#define HHC00003 "Empty SCP command issued"
#define HHC00004 "Control program identification: type '%s', name '%s', sysplex '%s', level %"I64_FMT"X"
#define HHC00005 "The configuration has been placed into a system check-stop state because of an incompatible service call"
#define HHC00006 "SCLP console interface '%s'"
#define HHC00007 "Previous message from function '%s()' in '%s' line %d" 
#define HHC00008 "%s%s"
#define HHC00009 "RRR...RING...GGG!\a"
#define HHC00010 "Enter input for console %1d:%04X"
#define HHC00011 "Function '%s' failed; cache '%d' size '%d': '[%02d] %s'"
#define HHC00012 "Releasing inactive buffer storage"
#define HHC00013 "Herc command: '%s'"
#define HHC00014 "select: %s"
#define HHC00015 "keyboard read: %s"
#define HHC00016 "Message lock hold longer than 1 second, release forced"

// reserve 20-39 for file related

// reserve 43-58 for option related

#define HHC00069 "There %s %d CPU%s still active; confirmation required"
// HHC0007xx, HHC0008xx and HHC0009xx reserved for hao.c. (to recognize own messages)
#define HHC00070 "Unknown hao command, valid commands are:\n" \
       "          hao tgt <tgt> : define target rule (pattern) to react on\n" \
       "          hao cmd <cmd> : define command for previously defined rule\n" \
       "          hao list <n>  : list all rules/commands or only at index <n>\n" \
       "          hao del <n>   : delete the rule at index <n>\n" \
       "          hao clear     : delete all rules (stops automatic operator)"
#define HHC00071 "The '%s' was not added because table is full; table size is %02d"
#define HHC00072 "The command '%s' given, but the command '%s' was expected"
#define HHC00073 "Empty '%s' specified"
#define HHC00074 "The target was not added because a duplicate was found in the table at %02d"
#define HHC00075 "Error in function '%s': '%s'"
#define HHC00076 "The '%s' was not added because it causes a loop with the '%s' at index %02d"
#define HHC00077 "The '%s' was placed at index %d"
#define HHC00078 "The command was not added because it may cause dead locks"
#define HHC00079 "No rule defined at index %02d"
#define HHC00080 "All HAO rules are cleared"
#define HHC00081 "Match at index %02d, executing command '%s'"
#define HHC00082 "%d rule(s) displayed"
#define HHC00083 "The command 'del' was given without a valid index"
#define HHC00084 "Invalid index; index must be between 0 and %02d"
#define HHC00085 "Rule at index %d not deleted, already empty"
#define HHC00086 "Rule at index %d successfully deleted"
#define HHC00087 "The defined Hercules Automatic Operator rule(s) are:"
#define HHC00088 "Index %02d: target '%s' -> command '%s'"
#define HHC00089 "The are no HAO rules defined"
// reserve 90-99 for hao.c

#define HHC00100 "Thread id "TIDPAT", prio %2d, name '%s' started"
#define HHC00101 "Thread id "TIDPAT", prio %2d, name '%s' ended"
#define HHC00102 "Error in function 'create_thread()': '%s'"
// reserve 102-129 thread related
#define HHC00130 "PGMPRDOS LICENSED specified and a licenced program product operating system is running"
#define HHC00131 "A licensed program product operating system detected, all processors have been stopped"

#define HHC00135 "Timeout for module '%s', possible older version"
#define HHC00136 "Error in function '%s': '%s'"
#define HHC00137 "Error opening TUN/TAP device '%s': '%s'"
#define HHC00138 "Error setting TUN/TAP mode '%s': '%s'"
#define HHC00139 "Invalid TUN/TAP device name '%s'"
#define HHC00140 "Invalid net device name '%s'"
#define HHC00141 "Net device '%s': Invalid ip '%s'"
#define HHC00142 "Net device '%s': Invalid destination address '%s'"
#define HHC00143 "Net device '%s': Invalid net mask '%s'"
#define HHC00144 "Net device '%s': Invalid MTU '%s'"
#define HHC00145 "Net device '%s': Invalid MAC address '%s'"
#define HHC00146 "Net device '%s': Invalid gateway address '%s'"
#define HHC00147 "Executing '%s' to configure interface"
#define HHC00148 "Closing %" I64_FMT "d files"
#define HHC00149 "IFC_IOCtl called for %s on FDs %d %d"
#define HHC00150 "%s module loaded%s"
#define HHC00151 "Activated facility: '%s'"
// 00152 - 00159 unused
#define HHC00160 "SCP %scommand: '%s'"
#define HHC00161 "Function %s failed: '[%02d] %s'"

// reserve 002xx for tape device related
#define HHC00201 "%1d:%04X Tape file '%s', type '%s': tape closed"
#define HHC00202 "%1d:%04X Tape file '%s', type '%s': block length %d exceeds maximum at offset "I64_FMTX
#define HHC00203 "%1d:%04X Tape file '%s', type '%s': invalid tapemark at offset "I64_FMTX
#define HHC00204 "%1d:%04X Tape file '%s', type '%s': error in function '%s', offset "I64_FMTX": '%s'"
#define HHC00205 "%1d:%04X Tape file '%s', type '%s': error in function '%s': '%s'"
#define HHC00206 "%1d:%04X Tape file '%s', type '%s': not a valid file"
#define HHC00207 "%1d:%04X Tape file '%s', type '%s': line %d: '%s'"
#define HHC00208 "%1d:%04X Tape file '%s', type '%s': maximum tape capacity exceeded"
#define HHC00209 "%1d:%04X Tape file '%s', type '%s': maximum tape capacity enforced"
#define HHC00210 "%1d:%04X Tape file '%s', type '%s': tape unloaded"
#define HHC00211 "%1d:%04X Tape file '%s', type 'scsi' status '%s', sstat 0x%8.8lX: %s %s%s%s%s%s%s%s"
#define HHC00212 "%1d:%04X Tape file '%s', type '%s': data chaining not supported for CCW %2.2X"
#define HHC00214 "%1d:%04X Tape file '%s', type '%s': auto-mount rejected: drive not empty"
#define HHC00215 "%1d:%04X Tape file '%s', type '%s': auto-mounted"
#define HHC00216 "%1d:%04X Tape file '%s', type '%s': auto-unmounted"
#define HHC00217 "%1d:%04X Tape file '%s', type '%s': locate block 0x%8.8"I32_FMT"X"
#define HHC00218 "%1d:%04X Tape file '%s', type '%s': display '%s' until unmounted"
#define HHC00219 "%1d:%04X Tape file '%s', type '%s': display '%s' until unmounted, then '%s' until mounted"
#define HHC00220 "%1d:%04X Tape file '%s', type '%s': format type is not determinable, presumed '%s'"
#define HHC00221 "%1d:%04X Tape file '%s', type '%s': format type '%s'"
#define HHC00222 "%1d:%04X Tape file '%s', type '%s': option '%s' accepted"
#define HHC00223 "%1d:%04X Tape file '%s', type '%s': option '%s' rejected: '%s'"
#define HHC00224 "%1d:%04X Tape file '%s', type '%s': display '%s'"
#define HHC00225 "Unsupported tape device type '%04X' specified"
#define HHC00226 "%1d:%04X Tape file '%s', type '%s': '%s' tape volume '%s' being auto unloaded"
#define HHC00227 "%1d:%04X Tape file '%s', type '%s': '%s' tape volume '%s' being auto loaded"
#define HHC00228 "Tape autoloader: file request fn '%s'"
#define HHC00229 "Tape autoloader: adding '%s' value '%s'"
#define HHC00230 "%1d:%04X Tape file '%s', '%s' volume '%s': not loaded; file not found" 
#define HHC00231 "%1d:%04X Tape file '%s', '%s' volume '%s': not loaded; manual mount pending" 
#define HHC00232 "%1d:%04X Tape file '%s', '%s' volume '%s': not loaded; duplicate file found" 
#define HHC00233 "%1d:%04X Tape file '%s', '%s' volume '%s': not loaded; rename failed" 
#define HHC00234 "%1d:%04X Tape file '%s', '%s' volume '%s': not loaded; not enough space on volume" 
#define HHC00235 "%1d:%04X Tape file '%s', type '%s': tape created"

// reserve 003xx for compressed dasd device related
#define HHC00300 "%1d:%04X CCKD file: error initializing shadow files"
#define HHC00301 "%1d:%04X CCKD file[%d] '%s': error in function '%s': '%s'"
#define HHC00302 "%1d:%04X CCKD file[%d] '%s': error in function '%s' at offset "I64_FMTX": '%s'"
#define HHC00303 "%1d:%04X CCKD file: error in function '%s': '%s'"
#define HHC00304 "%1d:%04X CCKD file[%d] '%s': get space error, size exceeds %lldM"
#define HHC00305 "%1d:%04X CCKD file[%d] '%s': device header id error"
#define HHC00306 "%1d:%04X CCKD file[%d] '%s': trklen error for %2.2x%2.2x%2.2x%2.2x%2.2x"
#define HHC00307 "%1d:%04X CCKD file[%d] '%s': invalid byte 0 trk %d, buf %2.2x%2.2x%2.2x%2.2x%2.2x"
#define HHC00308 "%1d:%04X CCKD file[%d] '%s': invalid byte 0 blkgrp %d, buf %2.2x%2.2x%2.2x%2.2x%2.2x"
#define HHC00309 "%1d:%04X CCKD file[%d] '%s': invalid %s hdr %s %d: %s compression unsupported"
#define HHC00310 "%1d:%04X CCKD file[%d] '%s': invalid %s hdr %s %d buf %p:%2.2x%2.2x%2.2x%2.2x%2.2x"
#define HHC00311 "%1d:%04X CCKD file[%d] '%s': shadow file name collides with %1d:%04X file[%d] '%s'"
#define HHC00312 "%1d:%04X CCKD file[%d] '%s': error re-opening readonly: '%s'"
#define HHC00313 "%1d:%04X CCKD file[%d]: no shadow file name"
#define HHC00314 "%1d:%04X CCKD file[%d] '%s':  max shadow files exceeded"
#define HHC00315 "%1d:%04X CCKD file: adding shadow files..."
#define HHC00316 "CCKD file number of devices processed: %d"
#define HHC00317 "%1d:%04X CCKD file: device is not a cckd device"
#define HHC00318 "%1d:%04X CCKD file[%d] '%s': error adding shadow file, sf command busy on device"
#define HHC00319 "%1d:%04X CCKD file[%d] '%s': error adding shadow file"
#define HHC00320 "%1d:%04X CCKD file[%d] '%s': shadow file succesfully added"
#define HHC00321 "%1d:%04X CCKD file: merging shadow files..."
#define HHC00322 "%1d:%04X CCKD file[%d] '%s': error merging shadow file, sf command busy on device"
#define HHC00323 "%1d:%04X CCKD file[%d] '%s': cannot remove base file"
#define HHC00324 "%1d:%04X CCKD file[%d] '%s': shadow file not merged: 'file[%d] %s%s'"
#define HHC00325 "%1d:%04X CCKD file[%d] '%s': shadow file successfully %s"
#define HHC00326 "%1d:%04X CCKD file[%d] '%s': shadow file not merged, error during merge"
#define HHC00327 "%1d:%04X CCKD file[%d] '%s': shadow file not merged, error processing trk(%d)"
#define HHC00328 "%1d:%04X CCKD file: compressing shadow files..."
#define HHC00329 "%1d:%04X CCKD file[%d] '%s': error compressing shadow file, sf command busy on device"
#define HHC00330 "%1d:%04X CCKD file: checking level %d..."
#define HHC00331 "%1d:%04X CCKD file[%d] '%s': shadow file check failed, sf command busy on device"
#define HHC00332 "%1d:%04X CCKD file: display cckd statistics"
#define HHC00333 "%1d:%04X           size free  nbr st   reads  writes l2reads    hits switches"
#define HHC00334 "%1d:%04X                                                  readaheads   misses"
#define HHC00335 "%1d:%04X --------------------------------------------------------------------"
#define HHC00336 "%1d:%04X [*] %10" I64_FMT "d %3" I64_FMT "d%% %4d    %7d %7d %7d %7d  %7d"
#define HHC00337 "%1d:%04X                                                     %7d  %7d"
#define HHC00338 "%1d:%04X %s"
#define HHC00339 "%1d:%04X [0] %10" I64_FMT "d %3" I64_FMT "d%% %4d %s %7d %7d %7d"
#define HHC00340 "%1d:%04X %s"
#define HHC00341 "%1d:%04X [%d] %10" I64_FMT "d %3" I64_FMT "d%% %4d %s %7d %7d %7d"
#define HHC00342 "%1d:%04X CCKD file[%d] '%s': offset 0x%" I64_FMT "x unknown space %2.2x%2.2x%2.2x%2.2x%2.2x"
#define HHC00343 "%1d:%04X CCKD file[%d] '%s': uncompress error trk %d: %2.2x%2.2x%2.2x%2.2x%2.2x"
#define HHC00344 "%1d:%04X CCKD file[%d] '%s': compression '%s' not supported"
#define HHC00345 "Command parameters for cckd:\n" \
                 "\thelp\t\tDisplay help message\n" \
                 "\tstats\t\tDisplay cckd statistics\n" \
                 "\topts\t\tDisplay cckd options\n" \
                 "\tcomp=<n>\tOverride compression\t\t\t(-1 .. 2)\n" \
                 "\tcompparm=<n>\tOverride compression parm\t\t(-1 .. 9)\n" \
                 "\tra=<n>\t\tSet number readahead threads\t\t(1 .. 9)\n" \
                 "\traq=<n>\t\tSet readahead queue size\t\t(0 .. 16)\n" \
                 "\trat=<n>\t\tSet number tracks to read ahead\t\t(0 .. 16)\n" \
                 "\twr=<n>\t\tSet number writer threads\t\t(1 .. 9)\n" \
                 "\tgcint=<n>\tSet garbage collector interval (sec)\t(1 .. 60)\n" \
                 "\tgcparm=<n>\tSet garbage collector parameter\t\t(-8 .. 8)\n" \
                 "\tgcstart\t\tStart garbage collector\n" \
                 "\tnostress=<n>\t1=Disable stress writes\n" \
                 "\tfreepend=<n>\tSet free pending cycles\t\t\t(-1 .. 4)\n" \
                 "\tfsync=<n>\t1=Enable fsync\n" \
                 "\tlinuxnull=<n>\t1=Check for null linux tracks\n" \
                 "\ttrace=<n>\tSet trace table size\t\t\t(0 .. 200000)"
#define HHC00346 "cckd opts: comp=%d,compparm=%d,ra=%d,raq=%d,rat=%d," \
                 "wr=%d,gcint=%d,gcparm=%d,nostress=%d," \
                 "freepend=%d,fsync=%d,linuxnull=%d,trace=%d"
#define HHC00347 "cckd stats:\n" \
                 "\t  reads....%10" I64_FMT "d Kbytes...%10" I64_FMT "d writes...%10" I64_FMT "d Kbytes...%10" I64_FMT "d\n" \
                 "\t  readaheads%9" I64_FMT "d misses...%10" I64_FMT "d syncios..%10" I64_FMT "d misses...%10" I64_FMT "d\n" \
                 "\t  switches.%10" I64_FMT "d l2 reads.%10" I64_FMT "d strs wrt.%10" I64_FMT "d\n" \
                 "\t  cachehits%10" I64_FMT "d misses...%10" I64_FMT "d l2 hits..%10" I64_FMT "d misses...%10" I64_FMT "d\n" \
                 "\t  waits               i/o......%10" I64_FMT "d cache....%10" I64_FMT "d\n" \
                 "\t  garbage collector   moves....%10" I64_FMT "d Kbytes...%10" I64_FMT "d"
#define HHC00348 "CCKD file: invalid value '%d' for '%s='"
#define HHC00349 "CCKD file: invalid cckd keyword: '%s'"

#define HHC00352 "%1d:%04X CCKD file '%s': 'opened' bit is on, use -f"
#define HHC00353 "%1d:%04X CCKD file '%s': check disk errors"
#define HHC00354 "%1d:%04X CCKD file '%s': error in function '%s': '%s'"
#define HHC00355 "%1d:%04X CCKD file '%s': error in function '%s' at offset 0x%"I64_FMT"X: '%s'"
#define HHC00356 "%1d:%04X CCKD file '%s': not a compressed dasd file"
#define HHC00357 "%1d:%04X CCKD file '%s': converting to '%s'"
#define HHC00358 "%1d:%04X CCKD file '%s': file already compressed"
#define HHC00359 "%1d:%04X CCKD file '%s': compress succesful, %lld bytes released"
#define HHC00360 "%1d:%04X CCKD file '%s': compress succesful, L2 tables relocated"
#define HHC00361 "%1d:%04X CCKD file '%s': dasd lookup error type %02X cylinders %d"
#define HHC00362 "%1d:%04X CCKD file '%s': bad '%s' %d, expecting %d"
#define HHC00363 "%1d:%04X CCKD file '%s': cdevhdr inconsistencies found, code %4.4X"
#define HHC00364 "%1d:%04X CCKD file '%s': forcing check level %d"
#define HHC00365 "%1d:%04X CCKD file '%s': %s offset 0x%" I32_FMT "X len %d is out of bounds"
#define HHC00366 "%1d:%04X CCKD file '%s': %s offset 0x%" I32_FMT "X len %d overlaps %s offset 0x%" I32_FMT "X"
#define HHC00367 "%1d:%04X CCKD file '%s': %s[%d] l2 inconsistency: len %d, size %d"
#define HHC00368 "%1d:%04X CCKD file '%s': free space errors detected"
#define HHC00369 "%1d:%04X CCKD file '%s': %s[%d] hdr error offset 0x%"I64_FMT"X: %2.2X%2.2X%2.2X%2.2X%2.2X"
#define HHC00370 "%1d:%04X CCKD file '%s': %s[%d] compressed using %s, not supported"
#define HHC00371 "%1d:%04X CCKD file '%s': %s[%d] offset 0x%"I64_FMT"X len %d validation error"
#define HHC00372 "%1d:%04X CCKD file '%s': %s[%d] recovered offset 0x%" I64_FMT "X len %d"
#define HHC00373 "%1d:%04X CCKD file '%s': %d %s images recovered"
#define HHC00374 "%1d:%04X CCKD file '%s': not enough file space for recovery"
#define HHC00375 "%1d:%04X CCKD file '%s': recovery not completed: '%s'"
#define HHC00376 "%1d:%04X CCKD file '%s': free space not rebuilt, file opened read-only"
#define HHC00377 "%1d:%04X CCKD file '%s': free space rebuilt"
#define HHC00378 "%1d:%04X CCKD file '%s': error during swap"

#define HHC00396 "%1d:%04X %s"
#define HHC00397 "CCKD file: internal cckd trace table is empty"
#define HHC00398 "%s"
#define HHC00399 "CCKD file: internal cckd trace"

// reserve 004xx for dasd device related
#define HHC00400 "%1d:%04X CKD file: name missing or invalid filename length"
#define HHC00401 "%1d:%04X CKD file '%s': open error: not found"
#define HHC00402 "%1d:%04X CKD file: parameter '%s' in argument %d is invalid"
#define HHC00403 "%1d:%04X CKD file '%s': opening as r/o%s"
#define HHC00404 "%1d:%04X CKD file '%s': error in function '%s': %s'"
#define HHC00405 "%1d:%04X CKD file '%s': only one base file is allowed"
#define HHC00406 "%1d:%04X CKD file '%s': ckd header invalid"
#define HHC00407 "%1d:%04X CKD file '%s': only 1 CCKD file allowed"
#define HHC00408 "%1d:%04X CKD file '%s': ckd file out of sequence"
#define HHC00409 "%1d:%04X CKD file '%s': seq %d cyls %d-%d"
#define HHC00410 "%1d:%04X CKD file '%s': found heads %d trklen %d, expected heads %d trklen %d"
#define HHC00411 "%1d:%04X CKD file '%s': ckd header inconsistent with file size"
#define HHC00412 "%1d:%04X CKD file '%s': ckd header high cylinder incorrect"
#define HHC00413 "%1d:%04X CKD file '%s': maximum CKD files exceeded: %d"
#define HHC00414 "%1d:%04X CKD file '%s': cyls %d heads %d tracks %d trklen %d"
#define HHC00415 "%1d:%04X CKD file '%s': device type %4.4X not found in dasd table"
#define HHC00416 "%1d:%04X CKD file '%s': control unit '%s' not found in dasd table"
#define HHC00417 "%1d:%04X CKD file '%s': cache hits %d, misses %d, waits %d"
#define HHC00418 "%1d:%04X CKD file '%s': invalid track header for cyl %d head %d %02X%02X%02X%02X%%02X"
#define HHC00419 "%1d:%04X CKD file '%s': error attempting to read past end of track '%d %d'"
#define HHC00420 "%1d:%04X CKD file '%s': error write kd orientation"
#define HHC00421 "%1d:%04X CKD file '%s': error write data orientation"
#define HHC00422 "%1d:%04X CKD file '%s': data chaining not supported for CCW %02X"
#define HHC00423 "%1d:%04X CKD file '%s': search key '%s'"
#define HHC00424 "%1d:%04X CKD file '%s': read trk %d cur trk %d"
#define HHC00425 "%1d:%04X CKD file '%s': read track updating track %d"
#define HHC00426 "%1d:%04X CKD file '%s': read trk %d cache hit, using cache[%d]"
#define HHC00427 "%1d:%04X CKD file '%s': read trk %d no available cache entry, waiting"
#define HHC00428 "%1d:%04X CKD file '%s': read trk %d cache miss, using cache[%d]"
#define HHC00429 "%1d:%04X CKD file '%s': read trk %d reading file %d offset %" I64_FMT "d len %d"
#define HHC00430 "%1d:%04X CKD file '%s': read trk %d trkhdr %02X %02X%02X %02X%02X"
#define HHC00431 "%1d:%04X CKD file '%s': seeking to cyl %d head %d"
#define HHC00432 "%1d:%04X CKD file '%s': error: MT advance: locate record %d file mask %02X"
#define HHC00433 "%1d:%04X CKD file '%s': MT advance to cyl(%d) head(%d)"
#define HHC00434 "%1d:%04X CKD file '%s': read count orientation '%s'"
#define HHC00435 "%1d:%04X CKD file '%s': cyl %d head %d record %d kl %d dl %d of %d"
#define HHC00436 "%1d:%04X CKD file '%s': read key %d bytes"
#define HHC00437 "%1d:%04X CKD file '%s': read data %d bytes"
#define HHC00438 "%1d:%04X CKD file '%s': writing cyl %d head %d record %d kl %d dl %d"
#define HHC00439 "%1d:%04X CKD file '%s': setting track overflow flag for cyl %d head %d record %d"
#define HHC00440 "%1d:%04X CKD file '%s': updating cyl %d head %d record %d kl %d dl %d"
#define HHC00441 "%1d:%04X CKD file '%s': ipdating cyl %d head %d record %d dl %d"
#define HHC00442 "%1d:%04X CKD file '%s': set file mask %02X"
#define HHC00443 "%1d:%04X Error: wrong recordheader: cc hh r=%d %d %d, should be:cc hh r=%d %d %d"

/* dasdutil.c */
#define HHC00445 "%1d:%04X CKD file '%s': updating cyl %d head %d"
#define HHC00446 "%1d:%04X CKD file '%s': write track error: stat %2.2X"
#define HHC00447 "%1d:%04X CKD file '%s': reading cyl %d head %d"
#define HHC00448 "%1d:%04X CKD file '%s': read track error: stat %2.2X"
#define HHC00449 "%1d:%04X CKD file '%s': searching extent %d begin %d,%d end %d,%d"
#define HHC00450 "Track %d not found in extent table"
#define HHC00451 "%1d:%04X CKD file '%s': DASD table entry not found for devtype 0x%2.2X"
#define HHC00452 "%1d:%04X CKD file '%s': initialization failed"
#define HHC00453 "%1d:%04X CKD file '%s': heads %d trklen %d"
#define HHC00454 "%1d:%04X CKD file '%s': sectors %d size %d"
#define HHC00455 "%1d:%04X CKD file '%s': '%s' record not found"
#define HHC00456 "%1d:%04X CKD file '%s': VOLSER '%s' VTOC %4.4X%4.4X%2.2X"
#define HHC00457 "%1d:%04X CKD file '%s': VTOC start %2.2X%2.2X%2.2X%2.2X end %2.2X%2.2X%2.2X%2.2X"
#define HHC00458 "%1d:%04X CKD file '%s': dataset '%s' not found in VTOC"
#define HHC00459 "%1d:%04X CKD file '%s': DSNAME '%s' F1DSCB CCHHR=%4.4X%4.4X%2.2X"
#define HHC00460 "%1d:%04X CKD file '%s': %u '%s' successfully written"
#define HHC00461 "%1d:%04X CKD file '%s': '%s' count %u is outside range %u-%u"
#define HHC00462 "%1d:%04X CKD file '%s': creating %4.4X volume %s: %u cyls, %u trks/cyl, %u bytes/track"
#define HHC00463 "%1d:%04X CKD file '%s': creating %4.4X volume %s: %u sectors, %u bytes/sector"
#define HHC00464 "%1d:%04X CKD file '%s': file size too large: %"I64_FMT"ud [%d]"
#define HHC00465 "%1d:%04X CKD file '%s': creating %4.4X compressed volume %s: %u sectors, %u bytes/sector"

// reserve 005xx for fba dasd device related messages 
#define HHC00500 "%1d:%04X FBA file: name missing or invalid filename length"
#define HHC00501 "%1d:%04X FBA file '%s' not found or invalid"
#define HHC00502 "%1d:%04X FBA file '%s': error in function '%s': '%s'"
#define HHC00503 "%1d:%04X FBA file: parameter '%s' in argument %d is invalid"
#define HHC00504 "%1d:%04X FBA file '%s': REAL FBA opened"                   
#define HHC00505 "%1d:%04X FBA file '%s': invalid device origin block number '%s'"
#define HHC00506 "%1d:%04X FBA file '%s': invalid device block count '%s'"
#define HHC00507 "%1d:%04X FBA file '%s': origin %lld, blks %d"
#define HHC00508 "%1d:%04X FBA file '%s': device type %4.4X not found in dasd table"
#define HHC00509 "%1d:%04X FBA file '%s': define extent data too short: %d bytes"
#define HHC00510 "%1d:%04X FBA file '%s': second define extent in chain"
#define HHC00511 "%1d:%04X FBA file '%s': invalid file mask %2.2X"
#define HHC00512 "%1d:%04X FBA file '%s': invalid extent: first block %d last block %d numblks %d device size %d"
#define HHC00513 "%1d:%04X FBA file '%s': FBA origin mismatch: %d, expected %d,"
#define HHC00514 "%1d:%04X FBA file '%s': FBA numblk mismatch: %d, expected %d,"
#define HHC00515 "%1d:%04X FBA file '%s': FBA blksiz mismatch: %d, expected %d,"
#define HHC00516 "%1d:%04X FBA file '%s': read blkgrp %d cache hit, using cache[%d]"
#define HHC00517 "%1d:%04X FBA file '%s': read blkgrp %d no available cache entry, waiting"
#define HHC00518 "%1d:%04X FBA file '%s': read blkgrp %d cache miss, using cache[%d]"
#define HHC00519 "%1d:%04X FBA file '%s': read blkgrp %d offset %" I64_FMT "d len %d"
#define HHC00520 "%1d:%04X FBA file '%s': positioning to %8.8" I64_FMT "X %" I64_FMT "u"

// reserve 006xx for sce dasd device related messages 
#define HHC00600 "SCE file '%s': error in function '%s': '%s'"
#define HHC00601 "SCE file '%s': load from file failed: '%s'"
#define HHC00602 "SCE file '%s': load from path failed: '%s'"
#define HHC00603 "SCE file '%s': load main terminated at end of mainstor"
#define HHC00604 "SCE file '%s': access error on image '%s': '%s'"
#define HHC00605 "SCE file '%s': access error: %s"

// reserve 007xx for shared device related messages 
#define HHC00700 "Shared: parameter '%s' in argument %d is invalid"
#define HHC00701 "%1d:%04X Shared: connect pending to file '%s'"
#define HHC00702 "%1d:%04X Shared: error retrieving cylinders"
#define HHC00703 "%1d:%04X Shared: error retrieving device characteristics"
#define HHC00704 "%1d:%04X Shared: remote device %04X is a %04X"
#define HHC00705 "%1d:%04X Shared: error retrieving device id"
#define HHC00706 "%1d:%04X Shared: device type %4.4X not found in dasd table"
#define HHC00707 "%1d:%04X Shared: control unit '%s' not found in dasd table"
#define HHC00708 "%1d:%04X Shared: file '%s' cyls %d heads %d tracks %d trklen %d"
#define HHC00709 "%1d:%04X Shared: error retrieving fba origin"
#define HHC00710 "%1d:%04X Shared: error retrieving fba number blocks"
#define HHC00711 "%1d:%04X Shared: error retrieving fba block size"
#define HHC00712 "%1d:%04X Shared: file '%s' origin %d blks %d"
#define HHC00713 "%1d:%04X Shared: error during channel program start"
#define HHC00714 "%1d:%04X Shared: error during channel program end"
#define HHC00715 "%1d:%04X Shared: error reading track %d"
#define HHC00716 "%1d:%04X Shared: error reading block group %d"
#define HHC00717 "%1d:%04X Shared: error retrieving usage information"
#define HHC00718 "%1d:%04X Shared: error writing track %d"
#define HHC00719 "%1d:%04X Shared: remote error writing track %d %2.2X-%2.2X"
#define HHC00720 "%1d:%04X Shared: error in function '%s': %s"
#define HHC00721 "%1d:%04X Shared: connected to file '%s'"
#define HHC00722 "%1d:%04X Shared: error in connect to file '%s': '%s'"
#define HHC00723 "%1d:%04X Shared: error in send for %2.2X-%2.2X: '%s'"
#define HHC00724 "%1d:%04X Shared: not connected to file '%s'"
#define HHC00725 "%1d:%04X Shared: error in receive: %s"
#define HHC00726 "%1d:%04X Shared: remote error %2.2X-%2.2X: '%s'"
#define HHC00727 "Shared: decompress error %d offset %d length %d"
#define HHC00728 "Shared: data compressed using method '%s' is unsupported"
#define HHC00729 "%1d:%04X Shared: error in send id %d: '%s'"
#define HHC00730 "%1d:%04X Shared: busy client being removed id %d '%s'"
#define HHC00731 "%1d:%04X Shared: '%s' disconnected id %d"
#define HHC00732 "Shared: connect to ip '%s' failed"
#define HHC00733 "%1d:%04X Shared: '%s' connected id %d"
#define HHC00734 "%1d:%04X Shared: error in receive from '%s' id %d"
#define HHC00735 "Shared: error in function '%s': '%s'"
#define HHC00736 "Shared: waiting for port %u to become free"
#define HHC00737 "Shared: waiting for shared device requests on port %u"
#define HHC00738 "Shared: invalid or missing argument"
#define HHC00739 "Shared: invalid or missing keyword"
#define HHC00740 "Shared: invalid or missing value '%s'"
#define HHC00741 "Shared: invalid or missing keyword '%s'"
#define HHC00742 "Shared: OPTION_SHARED_DEVICES not defined"
#define HHC00743 "Shared: %s"

// reserve 008xx for processor related messages 
#define HHC00800 "Processor %s%02X: loaded wait state PSW %s"
#define HHC00801 "Processor %s%02X: %s%s '%s' code %4.4X  ilc %d%s"
#define HHC00802 "Processor %s%02X: PER event: code %4.4X perc %2.2X addr "F_VADR
#define HHC00803 "Processor %s%02X: program interrupt loop PSW %s"
#define HHC00804 "Processor %s%02X: I/O interrupt code %4.4X CSW %2.2X%2.2X%2.2X%2.2X %2.2X%2.2X%2.2X%2.2X"
#define HHC00805 "Processor %s%02X: I/O interrupt code %8.8X parm %8.8X"
#define HHC00806 "Processor %s%02X: I/O interrupt code %8.8X parm %8.8X id %8.8X"
#define HHC00807 "Processor %s%02X: machine check code %16.16"I64_FMT"u"
#define HHC00808 "Processor %s%02X: store status completed"
#define HHC00809 "Processor %s%02X: disabled wait state %s"
#define HHC00811 "Processor %s%02X: architecture mode '%s'"
#define HHC00812 "Processor %s%02X: vector facility online"
#define HHC00813 "Processor %s%02X: error in function '%s': '%s'"
#define HHC00814 "Processor %s: CC %d%s"
#define HHC00815 "Processors %s%02X through %s%02X are offline"
#define HHC00816 "Processor %s%02X: processor is not '%s'"
#define HHC00817 "Processor %s%02X: store status completed"
#define HHC00818 "Processor %s%02X: not eligible for ipl nor restart"
#define HHC00819 "Processor %s%02X: online" 
#define HHC00820 "Processor %s%02X: offline"
#define HHC00821 "Processor %s%02X: vector facility configured '%s'"
#define HHC00822 "Processor %s%02X: machine check due to host error: '%s'"
#define HHC00823 "Processor %s%02X: check-stop due to host error: '%s'"
#define HHC00824 "Processor %s%02X: machine check code %16.16"I64_FMT"u"
#define HHC00825 "USR2 signal received for undetermined device"
#define HHC00826 "%1d:%04X: USR2 signal received"
#define HHC00827 "Processor %s%02X: engine %02X type %1d set: '%s'"
#define HHC00828 "Processor %s%02X: ipl failed: %s"
#define HHC00829 "IPL failure is usually as a result of ipl text missing or invalid."
#define HHC00830 "Capping active, specialty engines excluded from MIPS calculation"
#define HHC00831 "No central processors found, capping disabled"
#define HHC00832 "Central processors will be capped to %u MIPS"
#define HHC00833 "Invalid capping argument '%s', capping disabled"

/* external.c */
#define HHC00840 "External interrupt: interrupt key"
#define HHC00841 "External interrupt: clock comparator"
#define HHC00842 "External interrupt: CPU timer=%16.16" I64_FMT "X"
#define HHC00843 "External interrupt: interval timer"
#define HHC00844 "%1d:%04X: processing block I/O interrupt: code %4.4X parm %16.16X status %2.2X subcode %2.2X"
#define HHC00845 "External interrupt: block I/O %s"
#define HHC00846 "External interrupt: service signal %8.8X"

#define HHC00850 "Processor %s%02X: CPUint=%8.8X (State:%8.8X)&(Mask:%8.8X)"
#define HHC00851 "Processor %s%02X: interrupt %spending"
#define HHC00852 "Processor %s%02X: I/O interrupt %spending"
#define HHC00853 "Processor %s%02X: clock comparator %spending"
#define HHC00854 "Processor %s%02X: CPU timer %spending"
#define HHC00855 "Processor %s%02X: interval timer %spending"
#define HHC00856 "Processor %s%02X: ECPS vtimer %spending"
#define HHC00857 "Processor %s%02X: external call %spending"
#define HHC00858 "Processor %s%02X: emergency signal %spending"
#define HHC00859 "Processor %s%02X: machine check interrupt %spending"
#define HHC00860 "Processor %s%02X: service signal %spending"
#define HHC00861 "Processor %s%02X: mainlock held: %s"
#define HHC00862 "Processor %s%02X: intlock held: %s"
#define HHC00863 "Processor %s%02X: waiting for intlock: %s"
#define HHC00864 "Processor %s%02X: lock %sheld"
#define HHC00865 "Processor %s%02X: connected to channelset %s"
#define HHC00866 "Processor %s%02X: state %s"
#define HHC00867 "Processor %s%02X: instcount %" I64_FMT "d"
#define HHC00868 "Processor %s%02X: siocount %" I64_FMT "d"
#define HHC00869 "Processor %s%02X: psw %s"
#define HHC00870 "config mask "F_CPU_BITMAP" started mask "F_CPU_BITMAP" waiting mask "F_CPU_BITMAP""
#define HHC00871 "syncbc mask "F_CPU_BITMAP" %s"
#define HHC00872 "signaling facility %sbusy"
#define HHC00873 "TOD lock %sheld"
#define HHC00874 "mainlock %sheld; owner %4.4x"
#define HHC00875 "intlock %sheld; owner %4.4x"
#define HHC00876 "ioq lock %sheld"
#define HHC00877 "Central processors are capped at %u MIPS"
#define HHC00878 "No central processors found, capping disabled"

#define HHC00880 "device %1d:%04X: status %s"
#define HHC00881 "I/O interrupt queue:%s" 
#define HHC00882 "device %1d:%04X: %s%s%s%s, pri %d" 

#define HHC00890 "Facility(%-20s) %sabled"
#define HHC00891 "Facility name not found"
#define HHC00892 "Facility name not specified"
#define HHC00893 "Facility(%s) does not exist"
#define HHC00895 "Archmode %s is invalid"
#define HHC00896 "Facility(%s) is only supported for archmode%s %s"
#define HHC00897 "Facility(%s) is mandatory for archmode%s %s"
#define HHC00898 "Facility(%s) %sabled for archmode %s"
#define HHC00899 "Facility(%s) not supported for archmode %s"

// reserve 009xx for ctc related messages 
/*ctc_ctci.c*/
#define HHC00900 "%1d:%04X CTC: error in function '%s': '%s'"
#define HHC00901 "%1d:%04X CTC: device '%s', type '%s' opened"
#define HHC00902 "%1d:%04X CTC: ioctl '%s' failed for device '%s': '%s'"
#define HHC00904 "%1d:%04X CTC: halt or clear recognized"
#define HHC00905 "%1d:%04X CTC: received %d bytes size frame"
#define HHC00906 "%1d:%04X CTC: write CCW count %u is invalid"
#define HHC00907 "%1d:%04X CTC: interface command: '%s' %8.8X"
#define HHC00908 "%1d:%04X CTC: incomplete write buffer segment header at offset %4.4X"
#define HHC00909 "%1d:%04X CTC: invalid write buffer segment length %u at offset %4.4X"
#define HHC00910 "%1d:%04X CTC: sending packet to device '%s'"
#define HHC00911 "%1d:%04X CTC: error writing to device '%s': '%s'"
#define HHC00912 "%1d:%04X CTC: error reading from device '%s': '%s'"
#define HHC00913 "%1d:%04X CTC: received %d bytes size packet from device '%s'"
#define HHC00914 "%1d:%04X CTC: packet frame too big, dropped"
#define HHC00915 "%1d:%04X CTC: incorrect number of parameters"
#define HHC00916 "%1d:%04X CTC: option '%s' value '%s' invalid"
#define HHC00917 "%1d:%04X CTC: default value '%s' is used for option '%s'"

/* ctc_lcs.c */
#define HHC00920 "%1d:%04X CTC: lcs device %04X not in configuration"
#define HHC00921 "%1d:%04X CTC: lcs read"
#define HHC00922 "%1d:%04X CTC: lcs command packet received"
#define HHC00933 "%1d:%04X CTC: executing command '%s'"
#define HHC00934 "%1d:%04X CTC: sending packet to file '%s'"
#define HHC00936 "%1d:%04X CTC: error writing to file '%s': '%s'"
#define HHC00937 "%1d:%04X CTC: lcs write: unsupported frame type 0x%2.2X"
#define HHC00938 "%1d:%04X CTC: lcs triggering event"
#define HHC00939 "%1d:%04X CTC: lcs startup: frame buffer size 0x%4.4X '%s' compiled size 0x%4.4X: ignored"
#define HHC00940 "CTC: error in function '%s': '%s'"
#define HHC00941 "CTC: ioctl '%s' failed for device '%s': '%s'"
#define HHC00942 "CTC: lcs device '%s' using mac %2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X"
#define HHC00943 "CTC: lcs device '%s' not using mac %2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X"
#define HHC00944 "CTC: lcs device read error from port %2.2X: '%s'"
#define HHC00945 "CTC: lcs device port %2.2X: read buffer"
#define HHC00946 "CTC: lcs device port %2.2X: IPv4 frame received for %s"
#define HHC00947 "CTC: lcs device port %2.2X: ARP frame received for %s"
#define HHC00948 "CTC: lcs device port %2.2X: RARP frame received for %2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X"
#define HHC00949 "CTC: lcs device port %2.2X: SNA frame received"
#define HHC00950 "CTC: lcs device port %2.2X: no match found, selecting '%s' %4.4X"
#define HHC00951 "CTC: lcs device port %2.2X: no match found, discarding frame"
#define HHC00952 "CTC: lcs device port %2.2X: enqueing frame to device %4.4X '%s'"
#define HHC00953 "CTC: lcs device port %2.2X: packet frame too big, dropped"
#define HHC00954 "CTC: invalid statement '%s' in file '%s': '%s'"
#define HHC00955 "CTC: invalid '%s' '%s' in statement '%s' in file '%s': %s"
#define HHC00956 "CTC: error in file '%s': missing device number or mode"
#define HHC00957 "CTC: error in file '%s': invalid '%s' value '%s'"
#define HHC00958 "CTC: error in file '%s': '%s': missing port number"
#define HHC00959 "CTC: error in file '%s': '%s': invalid entry starting at '%s'"
#define HHC00960 "CTC: error in file '%s': '%s': SNA does not accept any arguments"
#define HHC00961 "CTC: error in file '%s': '%s': invalid mode"
#define HHC00962 "CTC: error in file '%s': reading line %d: '%s'"
#define HHC00963 "CTC: error in file '%s': line %d is too long"
#define HHC00964 "CTC: packet trace: %s %s %s"

/*ctcadpt.c */
#define HHC00970 "%1d:%04X CTC: unrecognized emulation type '%s'"
#define HHC00971 "%1d:%04X CTC: connect to %s:%s failed, starting server"
#define HHC00972 "%1d:%04X CTC: connected to %s:%s"
#define HHC00973 "%1d:%04X CTC: error reading from file '%s': '%s'"
#define HHC00974 "%1d:%04X CTC: incorrect client or config error: config file '%s' connecting client '%s'"
#define HHC00975 "%1d:%04X CTC: invalid '%s' length: %d < %d"
#define HHC00976 "%1d:%04X CTC: EOF on read, CTC network down"

/* ndis_lcs.c */
#define HHC00980 "%1d:%04X NDIS Error: IPv4 address and --mac are mutually exclusive with --oat"
#define HHC00981 "%1d:%04X NDIS open failed"
#define HHC00982 "Error %s: [%04X]:%s"
#define HHC00988 "NDIS RB Statistics for Port %d\n" \
       "          \n" \
       "          Ringbuffer size    = %d bytes\n" \
       "          Numbers of slots   = %d\n" \
       "          \n" \
       "          Packet Read Count  = %d\n" \
       "                 Write Count = %d\n" \
       "          \n" \
       "          Bytes Read         = %d\n" \
       "                Written      = %d\n" \
       "          \nLast Error: '%s'"
#define HHC00989 "NDIS Statistics for Port %d\n" \
       "          \n" \
       "          Reads:  Cnt    %12" I64_FMT "d\n" \
       "                  Busy   %12" I64_FMT "d\n" \
       "                  Wait   %12" I64_FMT "d\n" \
       "                  Errors %12" I64_FMT "d\n" \
       "          \n" \
       "          Writes: Cnt    %12" I64_FMT "d\n" \
       "                  Busy   %12" I64_FMT "d\n" \
       "                  Wait   %12" I64_FMT "d\n" \
       "                  Errors %12" I64_FMT "d"

// reserve 010xx for communication adapter specific component messages
/* comm3705.c and commadpt.c console.c */
#define HHC01000 "%1d:%04X CA: error in function '%s': '%s'"
#define HHC01001 "%1d:%04X CA: connect out to %s:%d failed during initial status: '%s'"
#define HHC01002 "%1d:%04X CA: cannot obtain socket for incoming calls: '%s'"
#define HHC01003 "%1d:%04X CA: waiting 5 seconds for port %d to become available"
#define HHC01004 "%1d:%04X CA: listening on port %d for incoming TCP connections"
#define HHC01005 "%1d:%04X CA: outgoing call failed during '%s' command: '%s'"
#define HHC01006 "%1d:%04X CA: incoming call"
#define HHC01007 "%1d:%04X CA: option '%s' value '%s' invalid"
#define HHC01008 "%1d:%04X CA: missing parameter: DIAL(%s) and '%s' not specified"
#define HHC01009 "%1d:%04X CA: conflicting parameter: DIAL(%s) and %s=%s specified"
#define HHC01010 "%1d:%04X CA: RPORT parameter ignored"
#define HHC01011 "%1d:%04X CA: initialization not performed"
#define HHC01012 "%1d:%04X CA: error parsing '%s'"
#define HHC01013 "%1d:%04X CA: incorrect switched/dial specification '%s': defaulting to DIAL=OUT"
#define HHC01014 "%1d:%04X CA: initialization failed due to previous errors"
#define HHC01015 "%1d:%04X CA: BSC communication thread did not initialize"
#define HHC01016 "CA: unable to determine '%s' from '%s'"
#define HHC01017 "CA: invalid parameter '%s'"
#define HHC01018 "%1d:%04X CA: client '%s' connected to %4.4X device"
#define HHC01019 "%1d:%04X CA: unrecognized parameter '%s'"
#define HHC01020 "%1d:%04X CA: no buffers trying to send '%s'"
#define HHC01021 "%1d:%04X CA: client '%s' devtype %4.4X: '%s'"
#define HHC01022 "%1d:%04X CA: connection closed by client '%s'"
#define HHC01023 "Waiting for port %u to become free for console connections"
#define HHC01024 "Waiting for console connections on port %u"
#define HHC01025 "%1d:%04X CA: duplicate SYSG console definition"
#define HHC01026 "%1d:%04X CA: enter console input"
#define HHC01027 "Hercules version %s, built on %s %s"
#define HHC01028 "Connection rejected, no available %s device"
#define HHC01029 "Connection rejected, no available %s device in the %s group"
#define HHC01030 "Connection rejected, device %04X unavailable"
#define HHC01031 "Running on %s (%s-%s.%s %s %s)"
#define HHC01032 "CA: this hercules build does not support unix domain sockets"
#define HHC01033 "CA: error: socket pathname '%s' exceeds limit %d"
#define HHC01034 "CA: error in function '%s': '%s'"
#define HHC01035 "CA: failed to determine IP address from node '%s'"
#define HHC01036 "CA: failed to determine port number from service '%s'"
#define HHC01037 "%1d:%04X CA: client '%s', ip '%s' connection to device '%s' rejected: device busy or interrupt pending"
#define HHC01038 "%1d:%04X CA: client '%s', ip '%s' connection to device '%s' rejected: client '%s' ip '%s' still connected"
#define HHC01039 "%1d:%04X CA: client '%s', ip '%s' connection to device '%s' rejected: by onconnect callback"
#define HHC01040 "%1d:%04X CA: client '%s', ip '%s' connected to device '%s'"
#define HHC01041 "%1d:%04X CA: error: device already bound to socket '%s'"
#define HHC01042 "%1d:%04X CA: device bound to socket '%s'"
#define HHC01043 "%1d:%04X CA: device not bound to any socket"
#define HHC01044 "%1d:%04X CA: client '%s', ip '%s' disconnected from device '%s'"
#define HHC01045 "%1d:%04X CA: client '%s', ip '%s' still connected to device '%s'"
#define HHC01046 "%1d:%04X CA: device unbound from socket '%s'"
#define HHC01047 "CA: connect message sent: '%s'"
#define HHC01048 "%1d:%04X CA: '%s'"
#define HHC01049 "%1d:%04X CA: '%s': dump of %d (%x) byte(s)"
#define HHC01050 "%1d:%04X CA: '%s': %s"
#define HHC01051 "%1d:%04X CA: %s"
#define HHC01052 "%1d:%04X CA: clean: '%s'"
#define HHC01053 "%1d:%04X CA: received telnet command 0x%02x 0x%02x"
#define HHC01054 "%1d:%04X CA: sending telnet command 0x%02x 0x%02x"
#define HHC01055 "%1d:%04X CA: received telnet IAC 0x%02x"
#define HHC01056 "%1d:%04X CA: posted %d input bytes"
#define HHC01057 "%1d:%04X CA: raised attention, return code %d"
#define HHC01058 "%1d:%04X CA: initialization starting"
#define HHC01059 "%1d:%04X CA: initialization: control block allocated"
#define HHC01060 "%1d:%04X CA: closing down"
#define HHC01061 "%1d:%04X CA: closed down"
#define HHC01062 "%1d:%04X CA: %s: %s %s %-6.6s %s"
#define HHC01063 "%1d:%04X CA: CCW exec - entry code %x"
#define HHC01064 "%1d:%04X CA: %s: status text=%s, trans=%s, tws=%s"
#define HHC01065 "Ring buffer for ring %p at %p '%s'"
#define HHC01066 "%1d:%04X CA: found data beyond EON"
#define HHC01067 "%1d:%04X CA: found incorrect ip address section at position %d"
#define HHC01068 "%1d:%04X CA: %d (0x%04X) greater than 255 (0xFE)"
#define HHC01069 "%1d:%04X CA: too many separators in dial data"
#define HHC01070 "%1d:%04X CA: incorrect dial data byte %02X"
#define HHC01071 "%1d:%04X CA: not enough separator in dial data, %d found"
#define HHC01072 "%1d:%04X CA: destination tcp port %d exceeds maximum of 65535"
#define HHC01073 "%s:%d terminal connected cua %04X term %s"
#define HHC01074 "%1d:%04X CA: cthread - entry - devexec '%s'"
#define HHC01075 "%1d:%04X CA: poll command abort, poll address > 7 bytes"
#define HHC01076 "%1d:%04X CA: writing byte %02X in socket"
#define HHC01077 "%1d:%04X CA: cthread - select in maxfd %d / devexec '%s'"
#define HHC01078 "%1d:%04X CA: cthread - select out return code %d"
#define HHC01079 "%1d:%04X CA: cthread - select time out"
#define HHC01080 "%1d:%04X CA: cthread - ipc pipe closed"
#define HHC01081 "%1d:%04X CA: cthread - ipc pipe data: code %d"
#define HHC01082 "%1d:%04X CA: cthread - inbound socket data"
#define HHC01083 "%1d:%04X CA: cthread - socket write available"
#define HHC01084 "%1d:%04X CA: set mode '%s'" 

// reserve 011xx for printer specific component messages
#define HHC01100 "%1d:%04X Printer: client '%s', ip '%s' disconnected from device '%s'"
#define HHC01101 "%1d:%04X Printer: file name missing or invalid"
#define HHC01102 "%1d:%04X Printer: parameter '%s' in argument %d is invalid"
#define HHC01103 "%1d:%04X Printer: parameter '%s' in argument %d at position %d is invalid"
#define HHC01104 "%1d:%04X Printer: option '%s' is incompatible"
#define HHC01105 "%1d:%04X Printer: error in function '%s': '%s'"
#define HHC01106 "%1d:%04X Printer: pipe receiver with pid %d starting"
#define HHC01107 "%1d:%04X Printer: pipe receiver with pid %d terminating"
#define HHC01108 "%1d:%04X Printer: unable to execute file '%s': '%s'"

// reserve 012xx for card devices
#define HHC01200 "%1d:%04X Card: error in function '%s': '%s'"
#define HHC01201 "%1d:%04X Card: filename '%s' too long, maximum length is %ud"
#define HHC01202 "%1d:%04X Card: options 'ascii' and 'ebcdic' are mutually exclusive"
#define HHC01203 "%1d:%04X Card: only one filename (sock_spec) allowed for socket device"
#define HHC01204 "%1d:%04X Card: option 'ascii' is default for socket device"
#define HHC01205 "%1d:%04X Card: option 'multifile' ignored: only one file specified"
#define HHC01206 "%1d:%04X Card: client '%s', ip '%s' disconnected from device '%s'"
#define HHC01207 "%1d:%04X Card: file '%s': card image exceeds maximum %d bytes"
#define HHC01208 "%1d:%04X Card: filename is missing"
#define HHC01209 "%1d:%04X Card: parameter '%s' in argument %d is invalid"

// reserve 013xx for channel related messages
/* channel.c */
#define HHC01300 "%1d:%04X CH: halt subchannel: cc=%d"
#define HHC01301 "%1d:%04X CH: midaw %2.2X %4.4"I16_FMT"X %16.16"I64_FMT"X: '%s'"
#define HHC01302 "%1d:%04X CH: idaw %8.8"I32_FMT"X, len %3.3"I16_FMT"X: '%s'"
#define HHC01303 "%1d:%04X CH: idaw %16.16"I64_FMT"X, len %4.4"I16_FMT"X: '%s'"
#define HHC01304 "%1d:%04X CH: attention signaled"
#define HHC01305 "%1d:%04X CH: attention"
#define HHC01306 "%1d:%04X CH: initial status interrupt"
#define HHC01307 "%1d:%04X CH: attention completed"
#define HHC01308 "%1d:%04X CH: clear completed"
#define HHC01309 "%1d:%04X CH: halt completed"
#define HHC01310 "%1d:%04X CH: suspended"
#define HHC01311 "%1d:%04X CH: resumed"
#define HHC01312 "%1d:%04X CH: stat %2.2X%2.2X, count %4.4X '%s'"
#define HHC01313 "%1d:%04X CH: sense %2.2X%2.2X%2.2X%2.2X %2.2X%2.2X%2.2X%2.2X %2.2X%2.2X%2.2X%2.2X %2.2X%2.2X%2.2X%2.2X %2.2X%2.2X%2.2X%2.2X %2.2X%2.2X%2.2X%2.2X"
#define HHC01314 "%1d:%04X CH: sense %s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s"
#define HHC01315 "%1d:%04X CH: ccw %2.2X%2.2X%2.2X%2.2X %2.2X%2.2X%2.2X%2.2X%s"
#define HHC01316 "%1d:%04X CH: stat %2.2X%2.2X, count %2.2X%2.2X, ccw %2.2X%2.2X%2.2X"
#define HHC01317 "%1d:%04X CH: scsw %2.2X%2.2X%2.2X%2.2X, stat %2.2X%2.2X, count %2.2X%2.2X, ccw %2.2X%2.2X%2.2X%2.2X"
#define HHC01318 "%1d:%04X CH: test I/O"
#define HHC01319 "%1d:%04X CH: TIO modification executed: cc=1"
#define HHC01329 "%1d:%04X CH: halt I/O"
#define HHC01330 "%1d:%04X CH: HIO modification executed: cc=1"
#define HHC01331 "%1d:%04X CH: clear subchannel"
#define HHC01332 "%1d:%04X CH: halt subchannel"
#define HHC01333 "%1d:%04X CH: resume subchannel: cc=%d"
#define HHC01334 "%1d:%04X CH: asynchronous I/O ccw addr %8.8x"
#define HHC01335 "%1d:%04X CH: synchronous  I/O ccw addr %8.8x"

/* hchan.c */
#define HHC01350 "%1d:%04X CH: missing generic channel method"
#define HHC01351 "%1d:%04X CH: incorrect generic channel method '%s'"
#define HHC01352 "%1d:%04X CH: generic channel initialisation failed"
#define HHC01353 "%1d:%04X CH: generic channel is currently in development"

// reserve 014xx for initialization and shutdown
/* impl.c */
#define HHC01400 "Ctrl-break intercepted: interrupt key pressed simulated"
#define HHC01401 "Ctrl-c intercepted"
#define HHC01402 "'%s' event received: shutdown %sstarting..."
#define HHC01403 "'%s' event received, shutdown previously requested..."
#define HHC01404 "Cannot create the 'Hercules Automatic Operator' thread"
#define HHC01405 "Run commands file '%s' not found"
#define HHC01406 "Startup parm '-l': maximum loadable modules %d exceeded; remainder not loaded"
#define HHC01407 "Usage: %s [-f config-filename] [-d] [-b logo-filename] [-s sym=val]%s [> logfile]"
#define HHC01408 "Hercules terminating, see previous messages for reason"
#define HHC01409 "Load of 'dyngui.dll' failed, hercules terminated"
#define HHC01410 "Cannot register '%s' handler: %s"
#define HHC01411 "Cannot suppress SIGPIPE signal: %s"
#define HHC01412 "Hercules terminated"

/* version.c */
#define HHC01413 "%s version %s"
#define HHC01414 "%s"
#define HHC01415 "Built on %s at %s"
#define HHC01416 "Build information:"
#define HHC01417 "%s"

#define HHC01419 "Symbol and/or Value is invalid; ignored"

/* hscmisc.c */
#define HHC01420 "Begin Hercules shutdown"
#define HHC01421 "Releasing configuration"
#define HHC01422 "Configuration release complete"
#define HHC01423 "Calling termination routines"
#define HHC01424 "All termination routines complete"
#define HHC01425 "Hercules shutdown complete"
#define HHC01426 "Shutdown initiated"

/* bldcfg.c */
#define HHC01430 "Error in function '%s': '%s'"
#define HHC01431 "Expanded storage support not installed"
#define HHC01432 "Config file[%d] '%s': error in function '%s': '%s'"
#define HHC01433 "Config file[%d] '%s': line is too long"
#define HHC01434 "Config file '%s': no device statements in file"
#define HHC01435 "Config file '%s': will ignore include errors"
#define HHC01436 "Config file[%d] '%s': maximum nesting level %d reached"
#define HHC01437 "Config file[%d] '%s': including file '%s'"
#define HHC01438 "Config file '%s': open error ignored file '%s': '%s'"
#define HHC01439 "Config file '%s': open error file '%s': '%s'"
#define HHC01440 "Config file[%d] '%s': statement '%s' deprecated, use '%s' instead"
#define HHC01441 "Config file[%d] '%s': syntax error: '%s'"
#define HHC01442 "Config file[%d] '%s': incorrect number of operands"
#define HHC01443 "Config file[%d] '%s': '%s' is not a valid '%s'"
#define HHC01444 "Hercules is not running as root, cannot raise '%s'"
#define HHC01445 "Vector Facility support not configured"
#define HHC01446 "Config file[%d] '%s': '%s' is not a valid '%s', assumed '%s'"
#define HHC01447 "Default allowed AUTOMOUNT directory: '%s'"
#define HHC01448 "Config file[%d] '%s': missing device number or device type"
#define HHC01449 "Config file '%s': NUMCPU %d exceeds MAXCPU %d; MAXCPU set to NUMCPU"
#define HHC01450 "Config file[%d] '%s': '%s' not supported in this build; see option '%s'"
#define HHC01451 "Invalid value '%s' specified for '%s'"
#define HHC01452 "Default port '%s' being used for '%s'"
#define HHC01453 "Config file '%s': lock failure:'%s'; multiple instances restricted"
#define HHC01454 "Config file '%s': error in function '%s': '%s'"
#define HHC01455 "Invalid number of arguments for '%s'"
#define HHC01456 "Invalid syntax '%s' for '%s'"
#define HHC01457 "Valid years for '%s' are '%s'; other values no longer supported"

/* config.c */
#define HHC01458 "%1d:%04X: only devices on CSS 0 are usable in S/370 mode"
#define HHC01459 "Device %1d:%04X defined as %1d:%04X"
#define HHC01460 "%1d:%04X error in function '%s': '%s'"
#define HHC01461 "%1d:%04X device already exists"
#define HHC01462 "%1d:%04X devtype '%s' not recognized"
#define HHC01463 "%1d:%04X device initialization failed"
#define HHC01464 "%1d:%04X %s does not exist"
#define HHC01465 "%1d:%04X %s detached"
#define HHC01466 "Unspecified error occured while parsing 'Logical Channel Subsystem Identification'"
#define HHC01467 "No more than 1 'Logical Channel Subsystem Identification' may be specified"
#define HHC01468 "Non-numeric 'Logical Channel Subsystem Identification' '%s'"
#define HHC01469 "'Logical Channel Subsystem Identification' %d exceeds maximum of %d"
#define HHC01470 "Incorrect '%s' near character '%c'"
#define HHC01471 "Incorrect device address range '%04X < %04X'"
#define HHC01472 "%1d:%04X is on wrong channel, 1st device defined on channel %02X"
#define HHC01473 "Some or all devices in %04X-%04X duplicate already defined devices"

/* codepage.c c */
#define HHC01474 "Using '%s' codepage conversion table '%s'"
#define HHC01475 "Codepage conversion table '%s' is not defined"
#define HHC01476 "Codepage is '%s'"
#define HHC01477 "Valid codepage conversion tables are:"
#define HHC01478 "      %s"

// reserve 015xx for Hercules dynamic loader      
/* hdl.c */
#define HHC01500 "HDL: begin shutdown sequence"
#define HHC01501 "HDL: calling '%s'"
#define HHC01502 "HDL: calling '%s' complete"
#define HHC01503 "HDL: calling '%s' skipped during windows shutdown immediate"
#define HHC01504 "HDL: shutdown sequence complete"
#define HHC01505 "HDL: path name length %d exceeds maximum of %d"
#define HHC01506 "HDL: change request of directory to '%s' is ignored"
#define HHC01507 "HDL: directory remains '%s'; taken from startup"
#define HHC01508 "HDL: loadable module directory is '%s'"
#define HHC01509 "HDL: dependency check failed for '%s', version '%s' expected '%s'"
#define HHC01510 "HDL: dependency check failed for '%s', size %d expected %d"
#define HHC01511 "HDL: error in function '%s': '%s'"
#define HHC01512 "HDL: begin termination sequence"
#define HHC01513 "HDL: calling module cleanup routine '%s'"
#define HHC01514 "HDL: module cleanup routine '%s' complete"
#define HHC01515 "HDL: termination sequence complete"
#define HHC01516 "HDL: unable to open dll '%s': '%s'"
#define HHC01517 "HDL: no dependency section in '%s': '%s'"
#define HHC01518 "HDL: dependency check failed for module '%s'"
#define HHC01519 "HDL: module '%s' already loaded"
#define HHC01520 "HDL: dll '%s' is duplicate of '%s'"
#define HHC01521 "HDL: unloading of module '%s' not allowed"
#define HHC01522 "HDL: module '%s' bound to device %1d:%04X"
#define HHC01523 "HDL: unload of module '%s' rejected by final section"
#define HHC01524 "HDL: module '%s' not found"
#define HHC01525 "HDL: usage: %s <module>"
#define HHC01526 "HDL: loading module '%s'..."
#define HHC01527 "HDL: module '%s' loaded"
#define HHC01528 "HDL: unloading module '%s'..."
#define HHC01529 "HDL: module '%s' unloaded"
#define HHC01530 "HDL: usage: %s <path>"
#define HHC01531 "HDL: dll type = %s, name = %s, flags = (%s, %s)"
#define HHC01532 "HDL:  symbol = %s, loadcount = %d%s, owner = %s"
#define HHC01533 "HDL:  devtype(s) =%s"
#define HHC01534 "HDL:  instruction = %s, opcode = %4.4X"
#define HHC01535 "HDL: dependency '%s' version '%s' size %d"

/* dyngui.c */
#define HHC01540 "HDL: query buffer overflow for device %1d:%04X"
#define HHC01541 "HDL: dyngui.dll initiated"
#define HHC01542 "HDL: dyngui.dll terminated"

// reserve 016xx for panel communication
#define HHC01600 "Unknown herc command '%s', enter 'help' for a list of valid commands"
#define HHC01602 "%-9.9s    %s"
#define HHC01603 "%s"
#define HHC01604 "Unknown herc command '%s', no help available"
#define HHC01605 "Invalid 'cmdlevel' option: '%s'"
#define HHC01606 "cmdlevel[%2.2X] is '%s'"
#define HHC01607 "No help available yet for message '%s'"
#define HHC01608 "PF KEY SUBSTitution results would exceed command line maximum size of %d; truncation occurred"

/* ecpsvm.c */
// reserve 017xx for ecps:vm support
#define HHC01700 "Abend condition detected in DISP2 instruction"
#define HHC01701 "| %-9s | %8d | %8d |  %3d%% |"
#define HHC01702 "+-----------+----------+----------+-------+"
#define HHC01703 "* : Unsupported, - : Disabled, %% - Debug"
#define HHC01704 "%d entry/entries not shown and never invoked"
#define HHC01705 "%d call(s), was/where made to unsupported functions"
#define HHC01706 "| %-9s | %-8s | %-8s | %-5s |"
#define HHC01707 "ECPS:VM %s feature %s%s%s"
#define HHC01708 "All ECPS:VM %s features %s%s"
#define HHC01709 "ECPS:VM global debug %s"
#define HHC01710 "ECPS:VM %s feature %s %s%s"
#define HHC01711 "Unknown ECPS:VM feature %s; ignored"
#define HHC01712 "Current reported ECPS:VM level is %d"
#define HHC01713 "But ECPS:VM is currently disabled"
#define HHC01714 "Level reported to guest program is now %d"
#define HHC01715 "ECP:VM level %d is not supported, unpredictable results may occur"
#define HHC01716 "The microcode support level is 20"
#define HHC01717 "%s: %s"
#define HHC01718 "Unknown subcommand %s, valid subcommands are :"
#define HHC01719 "ECPS:VM Command processor invoked"
#define HHC01720 "NO EVM subcommand. Type \"evm help\" for a list of valid subcommands"
#define HHC01721 "Unknown EVM subcommand '%s'"
#define HHC01722 "ECPS:VM Command processor complete"

// reserve 018xx for http server
#define HHC01800 "HTTP server: error in function '%s': '%s'"
#define HHC01801 "Invalid HTTPROOT: '%s': '%s'"
#define HHC01802 "Using HTTPROOT directory '%s'"
#define HHC01803 "Waiting for HTTP requests on port %u"
#define HHC01804 "Waiting for port %u to become free for HTTP requests"

// reserve 019xx for diagnose calls
/* diagnose.c */
#define HHC01900 "Diagnose 0x308 called: System is re-ipled"
#define HHC01901 "Checking processor %s%02X"
#define HHC01902 "Waiting 1 second for cpu's to stop"

/* vmd250.c */
#define HHC01905 "%04X triggered block I/O interrupt: code %4.4X parm %16.16X status %2.2X subcode %2.2X"
#define HHC01906 "%04X d250_init32 s %i o %i b %i e %i"
#define HHC01907 "%04X d250_init BLKTAB: type %4.4X arch %i 512 %i 1024 %i 2048 %i 4096 %i"
#define HHC01908 "Error in function '%s': '%s'"
#define HHC01909 "%04X d250_preserve pending sense preserved"
#define HHC01920 "%04X d250_restore pending sense restored"
#define HHC01921 "%04X d250_remove block I/O environment removed"
#define HHC01922 "%04X d250_read %d-byte block (rel. to 0): %d"
#define HHC01923 "%04X d250_read FBA unit status %2.2X residual %d"
#define HHC01924 "%04X async biopl %8.8X entries %d key %2.2X intp %8.8X"
#define HHC01925 "%04X d250_iorq32 sync bioel %8.8X entries %d key %2.2X"
#define HHC01926 "%04X d250_iorq32 psc %d succeeded %d failed %d"
#define HHC01927 "d250_list32 error: psc %i"
#define HHC01928 "%04X d250_list32 bios %i addr "F_RADR" I/O key %2.2X"
#define HHC01929 "%04X d250_list32 xcode %4.4X bioe32 %8.8X-%8.8X fetch key %2.2X"
#define HHC01930 "%04X d250_list32 bioe %8.8X oper %2.2X block %i buffer %8.8X"
#define HHC01931 "%04X d250_list32 xcode %4.4X rdbuf %8.8X-%8.8X fetch key %2.2X"
#define HHC01932 "%04X d250_list32 xcode %4.4X wrbuf %8.8X-%8.8X store key %2.2X"
#define HHC01933 "%04X d250_list32 xcode %4.4X status %8.8X-%8.8X  store key %2.2X"
#define HHC01934 "%04X d250_list32 bioe %8.8X status %2.2X"
#define HHC01935 "%04X async bioel %16.16X entries %d key %2.2X intp %16.16X"
#define HHC01936 "%04X d250_iorq64 sync bioel %16.16X entries %d key %2.2X"
#define HHC01937 "%04X d250_iorq64 psc %d succeeded %d failed %d"
#define HHC01938 "d250_list64 error: psc %i"
#define HHC01939 "%04X d250_list64 bioes %i addr "F_RADR" I/O key %2.2X"
#define HHC01940 "%04X d250_list64 xcode %4.4X bioe64 %8.8X-%8.8X fetch key %2.2X"
#define HHC01941 "%04X d250_list64 bioe %16.16X oper %2.2X block %i buffer %16.16X"
#define HHC01942 "%04X d250_list64 xcode %4.4X readbuf %16.16X-%16.16X fetch key %2.2X"
#define HHC01943 "%04X d250_list64 xcode %4.4X writebuf %16.16X-%16.16X  store key %2.2X"
#define HHC01944 "%04X d250_list64 xcode %4.4X status %16.16X-%16.16X store key %2.2X"
#define HHC01945 "%04X d250_list64 bioe %16.16X status %2.2X"

/* vm.c */
#define HHC01950 "Panel command '%s' issued by guest %s"
#define HHC01951 "Panel command '%s' issued by guest not processed, disabled in configuration"
#define HHC01952 "%1d:%04X:Diagnose X\'0A4\':%s blk=%8.8X adr=%8.8X len=%8.8X"
#define HHC01953 "Host command processing disabled by configuration statement"
#define HHC01954 "Host command processing not included in engine build"


// reserve 020xx for sr.c 
#define HHC02000 "SR: too many arguments"
#define HHC02001 "SR: error in function '%s': '%s'"
#define HHC02002 "SR: waiting for device %04X"
#define HHC02003 "SR: device %04X still busy, proceeding anyway"
#define HHC02004 "SR: error processing file '%s'"
#define HHC02005 "SR: all processors must be stopped to resume"
#define HHC02006 "SR: file identifier error"
#define HHC02007 "SR: resuming suspended file '%s' created"
#define HHC02008 "SR: archmode '%s' not supported"
#define HHC02009 "SR: mismatch in '%s': '%s' found, '%s' expected"
#define HHC02010 "SR: processor CP%02X exceeds max allowed CP%02X"
#define HHC02011 "SR: processor %s%02X already configured"
#define HHC02012 "SR: processor %s%02X unable to configure online"
#define HHC02013 "SR: processor %s%02X invalid psw length %d"
#define HHC02014 "SR: processor %s%02X error loading psw, rc %d"
#define HHC02015 "SR: %04X: device initialization failed"
#define HHC02016 "SR: %04X: device type mismatch; '%s' found, '%s' expected"
#define HHC02017 "SR: %04X: '%s' size mismatch: %d found, %d expected"
#define HHC02018 "SR: invalid key %8.8X"
#define HHC02019 "SR: CPU key %8.8X found but no active CPU"
#define HHC02020 "SR: value error, incorrect length"
#define HHC02021 "SR: string error, incorrect length"

// reserve 021xx for logger.c
#define HHC02100 "Logger: log not active"
#define HHC02101 "Logger: log closed"
#define HHC02102 "Logger: error in function '%s': '%s'"
#define HHC02103 "Logger: logger thread terminating"
#define HHC02104 "Logger: log switched to '%s'"

#define HHC02198 "Device attached"
#define HHC02199 "Symbol %-12s '%s'"
// reserve 022xx for command processing */
#define HHC02200 "%1d:%04X device not found"
#define HHC02201 "Device number missing"
#define HHC02202 "Missing argument(s)"
#define HHC02203 "Value '%s': '%s'"
#define HHC02204 "Value '%s' set to '%s'"
#define HHC02205 "Invalid argument '%s'%s"
#define HHC02206 "Uptime %u week%s, %u day%s, %02u:%02u:%02u"
#define HHC02207 "Uptime %u day%s, %02u:%02u:%02u"
#define HHC02208 "Uptime %02u:%02u:%02u"
#define HHC02209 "%1d:%04X device is not a '%s'"
#define HHC02210 "%1d:%04X %s"
#define HHC02211 "%1d:%04X device not stopped"
#define HHC02212 "%1d:%04X device started"
#define HHC02213 "%1d:%04X device not started%s"
#define HHC02214 "%1d:%04X device stopped"
#define HHC02215 "Command 'quiet' ignored: external GUI active"
#define HHC02216 "Empty list"
#define HHC02217 "'%c%s'"
#define HHC02218 "Logic error"
#define HHC02219 "Error in function '%s': '%s'"
#define HHC02220 "Entry deleted%s"
#define HHC02221 "Entry not found"
#define HHC02222 "Unsupported function"
#define HHC02223 "%s of %s-labeled volume '%s' pending for drive %u:%4.4X '%s'"
#define HHC02224 "Store status rejected: CPU not stopped"
#define HHC02225 "HTTP server already active"
#define HHC02226 "Held messages cleared"
#define HHC02227 "Shell commands are disabled"
#define HHC02228 "Key '%s' pressed"
#define HHC02229 "Instruction %s %s %s"
#define HHC02230 "%1d:%04X attention request raised"
#define HHC02231 "%1d:%04X busy or interrupt pending"
#define HHC02232 "%1d:%04X attention request rejected"
#define HHC02233 "%1d:%04X subchannel not enabled"
#define HHC02234 "Are you sure you didn't mean 'ipl %04X'"
#define HHC02235 "System reset/clear rejected: all CPU's must be stopped"
#define HHC02236 "IPL rejected: All CPU's must be stopped"
#define HHC02237 "Not all devices shown (max %d)"
#define HHC02238 "Device numbers can only be redefined within the same Logical Channel SubSystem"
#define HHC02239 "%1d:%04X synchronous: %12" I64_FMT "d asynchronous: %12" I64_FMT "d"
#define HHC02240 "Total synchronous: %13" I64_FMT "d asynchronous: %12" I64_FMT "d  %3" I64_FMT "d%%"
#define HHC02241 "Max device threads: %d, current: %d, most: %d, waiting: %d, max exceeded: %d"
#define HHC02242 "Max device threads: %d, current: %d, most: %d, waiting: %d, total I/Os queued: %d"
#define HHC02243 "%1d:%04X reinit rejected; drive not empty"
#define HHC02244 "%1d:%04X device initialization failed"
#define HHC02245 "%1d:%04X device initialized"
#define HHC02246 "Savecore: no modified storage found"
#define HHC02247 "Operation rejected: CPU not stopped"
#define HHC02248 "Saving locations %08X-%08X to file '%s'"
#define HHC02249 "Operation complete"
#define HHC02250 "Loading file '%s' to location %x"
#define HHC02251 "Address exceeds main storage size"
#define HHC02252 "Sorry, too many instructions"
#define HHC02253 "All CPU's must be stopped to change architecture"
#define HHC02254 " i: %12" I64_FMT "d"
#define HHC02255 "%3d: %12" I64_FMT "d"
#define HHC02256 "Command '%s' is deprecated, use '%s' instead"
#define HHC02257 "%s%7d"
#define HHC02258 "Only 1 script may be invoked from the panel at any time"
#define HHC02259 "Script aborted: '%s'"
#define HHC02260 "Script file processing started using file '%s'"
#define HHC02261 "Invalid script file pause statement: '%s', ignored"
#define HHC02262 "Pausing script file processing for %d seconds..."
#define HHC02263 "Resuming script file processing..."
#define HHC02264 "Script file processing complete"
#define HHC02265 "Script file '%s' aborted due to previous conditions"
#define HHC02266 "Confirm command by entering '%s' again within %d seconds"
//efine HHC02267 "%s" /* Used for instruction tracing */
//efine HHC02268 "%s" /* Used for ds command */
#define HHC02269 "%s" 
#define HHC02270 "%s" 
#define HHC02271 "%s"
#define HHC02272 "%s"
#define HHC02273 "Index %2d: '%s'"
#define HHC02274 "%s"
#define HHC02275 "%s"
#define HHC02276 "Floating point control register: %08"I32_FMT"X"
#define HHC02277 "Prefix register: %s"
#define HHC02278 "Program status word: %s\n" \
       "          sm=%2.2X pk=%d cmwp=%X as=%s cc=%d pm=%X am=%s ia=%"I64_FMT"X" 
#define HHC02279 "%s"
#define HHC02280 "%s"
#define HHC02281 "%s"
#define HHC02282 "%s"
#define HHC02283 "%s"
#define HHC02284 "%s"
#define HHC02285 "Counted %5u '%s' events"
#define HHC02286 "Average instructions / SIE invocation: %5u"
#define HHC02287 "No SIE performance data"
#define HHC02288 "Commands are sent to '%s'"
#define HHC02289 "%s"
#define HHC02290 "%s"
#define HHC02291 "%s"
#define HHC02292 "%s"
#define HHC02293 "%s"
#define HHC02294 "%s"
#define HHC02295 "CP group capping rate is %d MIPS"
#define HHC02296 "Capping rate for each non-CP CPU is %d MIPS"
#define HHC02297 "MIP capping is not enabled"
#define HHC02298 "%1d:%04X drive is empty"
#define HHC02299 "Invalid number of arguments. Type 'help %s' for assistance."

// reserve 023xx for ieee.c
#define HHC02300 "Function '%s': unexpectedly converting to '%s'"
#define HHC02301 "Inexact"

// reserve 024xx for dasd* utilities
//dasdcat.c
#define HHC02400 "Directory block byte count is invalid"
#define HHC02401 "Non-PDS-members not yet supported"
#define HHC02402 "Unknown option '%s' value '%s'"
#define HHC02403 "Failed opening '%s'"
#define HHC02404 "Can't make 80 column card images from block length %d"
#define HHC02405 "Usage: %s [-i dasd_image [sf=shadow-file-name] dsname...]...\n" \
       "          dsname can (currently must) be pdsname/spec\n" \
       "          spec is memname[:flags], * (all) or ? (list)\n" \
       "          flags can include (c)ard images, (a)scii"
//dasdconv.c
#define HHC02410 "Usage: %s [options] infile outfile\n" \
       "          infile:  name of input HDR-30 CKD image file ('-' means stdin)\n" \
       "          outfile: name of AWSCKD image file to be created\n" \
       "          options:\n" \
       "            -r     replace existing output file\n" \
       "            -q     suppress progress messages%s"

#define HHC02411 "Usage: %s [-v] [-f] [-level] [-ro] file1 [file2 ...]\n" \
       "          options:\n" \
       "            -v      display version and exit\n" \
       "\n" \
       "            -f      force check even if OPENED bit is on\n" \
       "\n" \
       "        level is a digit 0 - 4:\n" \
       "            -0  --  minimal checking (hdr, chdr, l1tab, l2tabs)\n" \
       "            -1  --  normal  checking (hdr, chdr, l1tab, l2tabs, free spaces)\n" \
       "            -2  --  extra   checking (hdr, chdr, l1tab, l2tabs, free spaces, trkhdrs)\n" \
       "            -3  --  maximal checking (hdr, chdr, l1tab, l2tabs, free spaces, trkimgs)\n" \
       "            -4  --  recover everything without using meta-data\n" \
       "\n" \
       "            -ro     open file readonly, no repairs"
#define HHC02412 "Error in function '%s': '%s'"
#define HHC02413 "Dasdconv is compiled without compress support and input is compressed"
#define HHC02414 "Input file is already in CKD format, use dasdcopy"
#define HHC02415 "Unknown device type %04X at offset 00000000 in input file"
#define HHC02416 "Unknown device type %04X"
#define HHC02417 "Invalid track header at offset %08X"
#define HHC02418 "Expected CCHH %04X%04X, found CCHH %04X%04X"
#define HHC02419 "Invalid record header (rc %d) at offset %04X in trk at CCHH %04X%04X at offset %08X in file '%s'"
#define HHC02420 "%u cylinders succesfully written to file '%s'"
#define HHC02421 "Cylinder count %u is outside range %u-%u"
#define HHC02422 "Converting %04X volume '%s': %u cyls, %u trks/cyl, %u bytes/trk"
#define HHC02423 "DASD operation completed"
//dasdcopy.c
#define HHC02430 "CKD lookup failed: device type %04X cyls %d"
#define HHC02431 "FBA lookup failed: blks %d"
#define HHC02432 "Failed creating '%s'"
#define HHC02433 "Read error on file '%s': %s %d stat=%2.2X, null %s substituted"
#define HHC02434 "Write error on file '%s': %s %d stat=%2.2X"
#define HHC02435 "Usage: ckd2cckd [-options] ifile ofile\n" \
       "          Copy a ckd dasd file to a compressed ckd dasd file\n" \
       "            ifile  input ckd dasd file\n" \
       "            ofile  output compressed ckd dasd file\n" \
       "          options:\n" \
       "            -v     display program version and quit\n" \
       "            -h     display this help and quit\n" \
       "            -q     quiet mode, don't display status\n" \
       "            -r     replace the output file if it exists\n" \
       "%s" \
       "%s" \
       "            -0     don't compress track images\n" \
       "            -cyls n  size of output file\n" \
       "            -a     output file will have alt cyls"
#define HHC02436 "Usage: cckd2ckd [-options] ifile [sf=sfile] ofile\n" \
       "          Copy a compressed ckd file to a ckd file\n" \
       "            ifile  input compressed ckd dasd file\n" \
       "            sfile  input compressed ckd shadow file\n" \
       "                   (optional)\n" \
       "            ofile  output ckd dasd file\n" \
       "          options:\n" \
       "            -v     display program version and quit\n" \
       "            -h     display this help and quit\n" \
       "            -q     quiet mode, don't display status\n" \
       "            -r     replace the output file if it exists\n" \
       "%s" \
       "            -cyls n size of output file\n" \
       "            -a     output file will have alt cyls"
#define HHC02437 "Usage: fba2cfba [-options] ifile ofile\n" \
       "          Copy a fba dasd file to a compressed fba dasd file\n" \
       "            ifile  input fba dasd file\n" \
       "            ofile  output compressed fba dasd file\n" \
       "          options:\n" \
       "            -v     display program version and quit\n" \
       "            -h     display this help and quit\n" \
       "            -q     quiet mode, don't display status\n" \
       "            -r     replace the output file if it exists\n" \
       "%s" \
       "%s" \
       "            -0     don't compress track images\n" \
       "            -blks n size of output file"
#define HHC02438 "Usage: cfba2fba [-options] ifile [sf=sfile] ofile\n" \
       "          Copy a compressed fba file to a fba file\n" \
       "            ifile  input compressed fba dasd file\n" \
       "            sfile  input compressed fba shadow file\n" \
       "                   (optional)\n" \
       "            ofile  output fba dasd file\n" \
       "          options:\n" \
       "            -v     display program version and quit\n" \
       "            -h     display this help and quit\n" \
       "            -q     quiet mode, don't display status\n" \
       "            -r     replace the output file if it exists\n" \
       "%s" \
       "            -blks n  size of output file"
#define HHC02439 "Usage: %s [-options] ifile [sf=sfile] ofile\n" \
       "          Copy a dasd file to another dasd file\n" \
       "            ifile  input dasd file\n" \
       "            sfile  input shadow file [optional]\n" \
       "            ofile  output dasd file\n" \
       "          options:\n" \
       "            -v     display program version and quit\n" \
       "            -h     display this help and quit\n" \
       "            -q     quiet mode, don't display status\n" \
       "            -r     replace the output file if it exists\n" \
       "%s" \
       "%s" \
       "            -0     don't compress output\n" \
       "            -blks n  size of output fba file\n" \
       "            -cyls n  size of output ckd file\n" \
       "            -a     output ckd file will have alt cyls\n" \
       "%s" \
       "                   even if it exceeds 2G in size\n" \
       "            -o type  output file type (CKD, CCKD, FBA, CFBA)"
//dasdinit.c
#define HHC02444 "Member '%s' is not a single text record"
#define HHC02445 "Invalid, unsupported or missing '%s': '%s'"
#define HHC02446 "Invalid number of arguments"
#define HHC02447 "Option '-linux' is only supported fo device type 3390"
#define HHC02448 "Usage: dasdinit [-options] filename devtype[-model] [volser] [size]\n" \
       "          Builds an empty dasd image file\n" \
       "          options:\n" \
       "            -v     display version info and help\n" \
       "%s" \
       "%s" \
       "            -0     build compressed dasd image file with no compression\n" \
       "%s" \
       "            -a     build dasd image file that includes alternate cylinders\n" \
       "                   (option ignored if size is manually specified)\n" \
       "            -r     build 'raw' dasd image file  (no VOL1 or IPL track)\n" \
       "            -linux  null track images will look like linux dasdfmt'ed images\n" \
       "                   (3390 device type only)\n\n" \
       "            filename  name of dasd image file to be created\n" \
       "            devtype  CKD: 2305, 2311, 2314, 3330, 3340, 3350, 3375, 3380, 3390, 9345\n" \
       "                   FBA: 0671, 3310, 3370, 9313, 9332, 9335, 9336\n" \
       "            model  device model (implies size) (opt)\n" \
       "            volser  volume serial number (1-6 characters)\n" \
       "                   (specified only if '-r' option not used)\n" \
       "            size   number of CKD cylinders or 512-byte FBA sectors\n" \
       "                   (required if model not specified else optional)"
#define HHC02449 "DASD operation failed"
#define HHC02450 "Member '%s' type '%s' skipped"
#define HHC02451 "Too many members"
#define HHC02452 "Member '%s' has TTR count zero"
#define HHC02453 "Member '%s' is not a single text record"
#define HHC02454 "Member '%s' size %04X exceeds limit 07F8"
#define HHC02455 "Member '%s' size %04X is not a multiple of 8"
#define HHC02456 "Member '%s' has invalid TTR %04X%02X"
#define HHC02457 "Member '%s' text record TTR %04X%02X CCHHR %04X%04X%02X in progress"
#define HHC02458 "Member '%s' error reading TTR %04X%02X"
#define HHC02459 "Member '%s' TTR %04X%02X text record length %X is invalid"
#define HHC02460 "Member '%s' TTR %04X%02X text record length %X does not match %X in directory"
#define HHC02461 "Member '%s' TTR %04X%02X XCTL table improperly terminated"
#define HHC02462 "Member '%s' '%s' TTRL %02X%02X%02X%02X: %s"
#define HHC02463 "Usage: %s ckdfile [sf=shadow-file-name]%s"
#define HHC02464 "End of directory, %d members selected"
#define HHC02465 "Invalid argument: '%s'"
#define HHC02466 "Reading directory block at CCHHR %04X%04X%02X"
#define HHC02467 "End of directory"
#define HHC02468 "File '%s'; %s error: '%s'"
#define HHC02469 "Member '%s' TTR %04X%02X"
#define HHC02470 "Invalid block length %d at CCHHR %04X%04X%02X"
#define HHC02471 "%s record not found"
#define HHC02472 "Dataset is not RECFM %s; utility ends"
#define HHC02473 "Dataset is not DSORG %s; utility ends"
#define HHC02474 "Error processing '%s'"
#define HHC02475 "Records written to '%s': %d"
#define HHC02476 "Dataset '%s' not found"
#define HHC02477 "In '%s': function '%s' rc %d%s"
#define HHC02478 "Length invalid for KEY '%d' or DATA '%d'%s"
#define HHC02479 "In '%s': x'F%s' expected for DS%sIDFMT, received x'%02X'"
#define HHC02480 "In '%s': extent slots exhausted; maximum supported is %d"
#define HHC02481 "     EXTENT --begin-- ---end---"
#define HHC02482 "TYPE NUMBER CCCC HHHH CCCC HHHH"
#define HHC02483 "  %02X   %02X   %04X %04X %04X %04X"
#define HHC02484 "Extent Information:"
#define HHC02485 "Dataset '%s' on volume '%s' sequence '%d'"
#define HHC02486 "Creation Date: '%s'; Expiration Date: '%s'"
#define HHC02487 "Dsorg=%s recfm=%s lrecl=%d blksize=%d"
#define HHC02488 "System code %s"
#define HHC02489 "%s: unable to allocate ASCII buffer"
#define HHC02490 "%s: convert_tt() track %5.5d/x'%04X', rc %d"
#define HHC02491 "%s: extent number parameter invalid '%d'; utility ends"
#define HHC02492 "Option '%s' specified"
#define HHC02493 "Filename '%s' specified for %s"
#define HHC02494 "Requested number of extents '%d' exceeds maximum '%d'; utility ends"
#define HHC02495 "Usage: %s [-v] [-f] [-n] file1 [file2 ...]\n" \
       "\n" \
       "          -v      display version and exit\n" \
       "\n" \
       "          -f      force check even if OPENED bit is on\n" \
       "\n" \
       "          -n      chkdsk level 'n' is a digit 0 - 3:\n" \
       "              -0  --  minimal checking\n" \
       "              -1  --  normal  checking\n" \
       "              -2  --  intermediate checking\n" \
       "              -3  --  maximal checking\n" \
       "                  default  -0"
#define HHC02496 "Usage: %s [options] ctlfile outfile [n]\n" \
       "          options:\n" \
       "            -0     no compression (default)\n" \
       "            -a     output disk will include alternate cylinders\n" \
       "%s%s%s" \
       "\n" \
       "          ctlfile  name of input control file\n" \
       "          outfile  name of DASD image file to be created\n" \
       "\n" \
       "          n        msglevel 'n' is a digit 0 - 5 re: output verbosity"


#define HHC02499 "Hercules utility '%s' - %s;"

// reserve 025xx for dasd* utilities
#define HHC02500 "Volume serial statement missing from '%s'"
#define HHC02501 "Volume serial '%s' is invalid; '%s' line %d"
#define HHC02502 "Device type '%s' is not recognized; '%s' line %d"
#define HHC02503 "Cylinder value '%s' is invalid; '%s' line %d"
#define HHC02504 "Cannot create '%s'; '%s' failed"
#define HHC02505 "CCHH[%04X%04X] not found in extent table"
#define HHC02506 "Cannot '%s' file '%s'; error '%s'"
#define HHC02507 "File '%s': invalid object file"
#define HHC02508 "File '%s': TXT record has invalid count of '%d'"
#define HHC02509 "File '%s': IPL text exceeds '%d' bytes"
#define HHC02510 "Input record CCHHR[%02X%02X%02X%02X%02X] exceeds output device track size"
#define HHC02511 "Dataset exceeds extent size: reltrk='%d', maxtrk='%d'"
#define HHC02512 "Input record CCHHR[%02X%02X%02X%02X%02X] exceeds virtual device track size"
#define HHC02513 "File '%s': read error at cyl[%04X/%d] head[%04X/%d]"
#define HHC02514 "File '%s': cyl[%04X/%d] head[%04X/%d]; invalid track header %02X%02X%02X%02X%02X"
#define HHC02515 "File '%s': cyl[%04X/%d] head[%04X/%d] rec[%02X/%d] record not found"
#define HHC02516 "File '%s': cyl[%04X/%d] head[%04X/%d] rec[%02X/%d] unmatched KL/DL"
#define HHC02517 "Error obtaining storage for %sDSCB: %s"
#define HHC02518 "Internal error: DSCB count exceeds MAXDSCB of '%d'"
#define HHC02519 "VTOC requires %d track%s; current allocation is too small"
#define HHC02520 "Creating %4.4X volume %s: %u trks/cyl, %u bytes/track"
#define HHC02521 "Loading %4.4X volume %s"
#define HHC02522 "IPL text address=%06X length=%04X"
#define HHC02523 "Updating cyl[%04X/%d] head[%%04X/%d] rec[%02X/%d]; kl[%d] dl[%d]"
#define HHC02524 "VTOC starts at cyl[%04X/%d] head[%%04X/%d] and is %d track%s"
#define HHC02525 "Format %d DSCB CCHHR[%04X%04X%02X] (TTR[%04X%02X]) %s"
#define HHC02526 "\t+%04X %-8.8s %04X %04X"
#define HHC02527 "File number: %d"
#define HHC02528 "File[%04u]: DSNAME=%s"
#define HHC02529 "            DSORG=%s RECFM=%s LRECL=%d BLKSIZE=%d KEYLEN=%d DIRBLKS=%d"
#define HHC02530 "Error reading %s at cyl[%04X/%d] head[%04X/%d]"
#define HHC02531 "Incomplete text unit"
#define HHC02532 "Too many fields in text unit"
#define HHC02533 "File '%s': read error: %s"
#define HHC02534 "File '%s': invalid segment header: %02X%02X"
#define HHC02535 "File '%s': first segment indicator expected"
#define HHC02536 "File '%s': first segment indicator not expected"
#define HHC02537 "File '%s': control record indicator mismatch"
#define HHC02538 "Invalid text unit at offset %04X"
#define HHC02539 "COPYR%d record length is invalid"
#define HHC02540 "COPYR%d header identifier not correct"
#define HHC02541 "COPYR%d unload format is unsupported"
#define HHC02542 "Invalid number of extents %d"
#define HHC02543 "Invalid directory block record length"
#define HHC02544 "Storage allocation for '%s' using '%s' failed: '%s'" 
#define HHC02545 "Internal error: Number of directory blocks exceeds MAXDBLK of %d"
#define HHC02546 "Invalid directory block byte count"
#define HHC02547 "File '%s': read error at cyl[%04X/%d] head[%04X/%d]"
#define HHC02548 "File '%s': cyl[%04X/%d] head[%04X/%d] invalid track header %02X%02X%02X%02X%02X"
#define HHC02549 "File '%s': cyl[%04X/%d] head[%04X/%d] rec[%02X/%d] note list record not found"
#define HHC02550 "Original dataset: DSORG=%s RECFM=%s LRECL=%d BLKSIZE=%d KEYLEN=%d"
#define HHC02551 "Dataset was unloaded from device type %02X%02X%02X%02X (%s)"
#define HHC02552 "Original device was cyls[%04X/%d], heads[%04X/%d]"
#define HHC02553 "Extent %d: Begin CCHH[%04X%04X] End CCHH[%04X%04X] Tracks=[%04X/%d]"
#define HHC02554 "End of directory"
#define HHC02555 "%s %-8.8s TTR[%02X%02X%02X] "
#define HHC02556 "Member '%s' TTR[%02X%02X%02X] replaced by TTR[%02X%02X%02X]"
#define HHC02557 "Member '%s' TTR[%02X%02X%02X] not found in dataset"
#define HHC02558 "Member '%s' Updating note list for member at TTR[%04X%02X] CCHHR[%04X%04X%02X]"
#define HHC02559 "Member '%s' Note list at cyl[%04X/%d] head[%04X/%d] rec[%02X/%d] dlen[%d] is too short for %d TTRs"
#define HHC02560 "File '%s': Processing"
#define HHC02561 "Control record: '%s' len[%d]"
#define HHC02562 "File number:    '%d' '%s'"
#define HHC02563 "Data record:    len[%d]"
#define HHC02564 "Input record:   CCHHR[%04X%04X%02X] (TTR[%04X%02X]) kl[%d] dl[%d]\n" \
       "          relocated to:   CCHHR[%04X%04X%02X] (TTR[%04X%02X])"
#define HHC02565 "Catalog block:  cyl[%04X/%d] head[%04X/%d] rec[%02X/%d]"
#define HHC02566 "DIP complete:   cyl[%04X/%d] head[%04X/%d] rec[%02X/%d]"
#define HHC02567 "File '%s'[%04d]: '%s'"
#define HHC02568 "Creating dataset %-44s at cyl[%04X/%d] head[%04X/%d]"
#define HHC02569 "File '%s': line[%04d] error: statement is invalid"
#define HHC02570 "File '%s': '%s' error: input record CCHHR[%04X%04X%02X] (TTR[%04X%02X]) kl[%d] dl[%d]" 
#define HHC02571 "Internal error: TTR count exceeds MAXTTR of %d"
#define HHC02572 "File '%s': XMIT utility file is not in IEBCOPY format; file is not loaded"
#define HHC02573 "File '%s': invalid dsorg[0x%02X] for SEQ processing; dsorg must be PS or DA"
#define HHC02574 "File '%s': invalid recfm[0x%02X] for SEQ processing; recfm must be F or FB"
#define HHC02575 "File '%s': invalid lrecl[%d] or blksize[%d] for SEQ processing"
#define HHC02576 "File '%s': invalid keyln[%d] for SEQ processing; keyln must be 0 for blocked"
#define HHC02577 "File '%s': line[%04d] error: line length is invalid"
#define HHC02578 "%s missing"
#define HHC02579 "Initialization method '%s' is invalid"
#define HHC02580 "Initialization file name missing"
#define HHC02581 "Unit of allocation '%s' is invalid"
#define HHC02582 "%s allocation of '%s' %sS is invalid"
#define HHC02583 "Directory space '%s' is invalid"
#define HHC02584 "Dataset organization (DSORG) '%s' is invalid"
#define HHC02585 "Record format (RECFM) '%s' is invalid"
#define HHC02586 "Logical record length (LRECL) '%s' is invalid"
#define HHC02587 "Block size (BLKSIZE) '%s' is invalid"
#define HHC02588 "Key Length (KEYLEN) '%s' is invalid"
#define HHC02589 "Dataset %-44s contains %d track%s"
#define HHC02590 "Volume exceeds '%d' cylinders"
#define HHC02591 "File '%s': total cylinders written was '%d'"
#define HHC02592 "VTOC pointer [%02X%02X%02X%02X%02X] updated"
#define HHC02593 "VOL1 record not readable or locatable"

// reserve 026xx for utilites
#define HHC02600 "Usage: %s [options] file-name\n" \
       "\n" \
       "          -v      display version and exit\n" \
       "          -d      display DEVHDR and exit\n" \
       "          -c      display CDEVHDR and exit\n" \
       "          -1      display L1TAB (numeric one)\n" \
       "          -g      enable debug output\n" \
       "\n" \
       "           CKD track related options:\n" \
       "           -a cc hh  display absolute CCHH data\n" \
       "           -r tt     display relative TT data\n" \
       "           -2        display L2TAB related to -a or -r\n" \
       "           -t        display track data\n" \
       "           -x        hex display track key/data\n" \
       "\n" \
       "           Offset option:\n" \
       "           -o oo ll  hex display data at offset oo of length ll" 
#define HHC02601 "Press enter to continue"
#define HHC02602 "From '%s': Storage allocation of size '%d' using '%s' failed"
#define HHC02603 "In '%s': lseek() to pos 0x%08x error: '%s'"
#define HHC02604 "In '%s': read() error: '%s'"
#define HHC02605 "Track %d COUNT cyl[%04X/%d] head[%04X/%d] rec[%02X/%d] kl[%d] dl[%d]"
#define HHC02606 "Track %d rec[%02X/%d] kl[%d]"
#define HHC02607 "Track %d rec[%02X/%d] dl[%d]"
#define HHC02608 "End of track"

#define HHC02700 "SCSI tapes are not supported with this build"
#define HHC02701 "Abnormal termination"
#define HHC02702 "Tape '%s': Status %8.8lX%s%s%s%s%s%s%s%s%s%s%s"
#define HHC02703 "Tape '%s': Error reading status: rc=%d, errno=%d: %s"
#define HHC02704 "End of tape"
#define HHC02705 "Tape '%s': Error reading tape: errno=%d: %s"
#define HHC02706 "File '%s': End of %s input"
#define HHC02707 "File '%s': Error reading %s header: rc=%d, errno=%d: %s"
#define HHC02708 "File '%s': Block too large for %s tape: block size=%d, maximum=%d"
#define HHC02709 "File '%s': Error reading %s data block: rc=%d, errno=%d: %s"
#define HHC02710 "Tape '%s': Error writing data block: rc=%d, errno=%d: %s"
#define HHC02711 "File '%s': Error writing %s header: rc=%d, errno=%d: %s"
#define HHC02712 "File '%s': Error writing %s data block: rc=%d, errno=%d: %s"
#define HHC02713 "Tape '%s': Error writing tapemark: rc=%d, errno=%d: %s"
#define HHC02714 "File '%s': Error writing %s tapemark: rc=%d, errno=%d: %s"
#define HHC02715 "Tape '%s': Error opening: errno=%d: %s"
#define HHC02716 "Tape '%s': Error setting attributes: rc=%d, errno=%d: %s"
#define HHC02717 "Tape '%s': Error rewinding: rc=%d, errno=%d: %s"
#define HHC02718 "Tape '%s': Device type%s"
#define HHC02719 "Tape '%s': Device density%s"
#define HHC02720 "File '%s': Error opening: errno=%d: %s"
#define HHC02721 "File No. %u: Blocks=%u, Bytes=%"I64_FMT"d, Block size min=%u, max=%u, avg=%u"
#define HHC02722 "Tape Label: %s"
#define HHC02723 "File No. %u: Block %u"
#define HHC02724 "Successful completion\n" \
       "          Bytes read:    %"I64_FMT"d (%3.1f MB), Blocks=%u, avg=%u\n" \
       "          Bytes written: %"I64_FMT"d (%3.1f MB)"
#define HHC02725 "Usage: %s infilename outfilename"
#define HHC02726 "Usage: %s filename"
#define HHC02727 "Usage: %s [-f n] infilename outfilename"
#define HHC02728 "Usage: %s [options] hetfile outfile fileno [recfm lrecl blksize]\n" \
       "\n" \
       "            fileno  range 1 to 9999; default 1\n" \
       "\n" \
       "            Options:\n" \
       "                -a  convert to ASCII (implies -u)\n" \
       "                -h  display usage summary\n" \
       "                -n  file is an NL (or BLP like) tape\n" \
       "                -u  unblock (removes BDWs and RDWs if RECFM=V)\n" \
       "                -s  strip trailing blanks (requires -a)"
#define HHC02729 "Usage: %s [options] filename [volser] [owner]\n" \
       "\n" \
       "            Options:\n" \
       "                -d  disable compression\n" \
       "                -h  display usage summary\n" \
       "                -i  create an IEHINITT formatted tape (default: on)\n" \
       "                -n  create an NL tape"
#define HHC02730 "Usage: %s [options] source [dest]\n" \
       "\n" \
       "            Options:\n" \
       "                -1   compress fast\n" \
       "                     ...\n" \
       "                -9   compress best\n" \
       "%s" \
       "                -c n set chunk size to \"n\"\n" \
       "                -d   decompress source tape\n" \
       "                -h   display usage summary\n" \
       "                -r   rechunk\n" \
       "                -s   strict AWSTAPE specification (chunksize=4096,no compression)\n" \
       "                -v   verbose (debug) information\n" \
       "                -z   use ZLIB compression\n"

#define HHC02740 "File '%s': writing output file"
#define HHC02741 "File '%s': Error, incomplete %s header"
#define HHC02742 "File '%s': Error, incomplete final data block: expected %d bytes, read %d"
#define HHC02743 "File '%s': Error, %s header block without data"
#define HHC02744 "File '%s': Error renaming file to '%s' - manual intervention required" 
#define HHC02745 "File '%s': Error removing file - manual intervention required"

#define HHC02750 "DCB attributes required for NL tape"
#define HHC02751 "Valid record formats are:"
#define HHC02752 "%s"
#define HHC02753 "%s label missing"
#define HHC02754 "File Info:  DSN=%-17.17s"
#define HHC02755 "HET: Setting option '%s' to %s"
#define HHC02756 "HET: HETLIB reported error %s files; %s"
#define HHC02757 "HET: %s"

#define HHC02800 "%1d:%04X %s complete"
#define HHC02801 "%1d:%04X %s failed; %s"
#define HHC02802 "%1d:%04X Current file number %d"
#define HHC02803 "%1d:%04X Current block number %d"
#define HHC02804 "%1d:%04X File protect enabled"


// reserve 04xxx for host os specific component messages
// reserve 041xx for windows specific component messages (w32xxxx.c)
#define HHC04100 "%s version %s initiated"
#define HHC04101 "%s Statistics:\n" \
       "          \n" \
       "          Size of Kernel Hold Buffer:      %5luK\n" \
       "          Size of DLL I/O Buffer:          %5luK\n" \
       "          Maximum DLL I/O Bytes Received:  %5luK\n" \
       "          \n" \
       "          %12" I64_FMT "d  Total Write Calls\n" \
       "          %12" I64_FMT "d  Total Write I/Os\n" \
       "          %12" I64_FMT "d  Packets To All Zeroes MAC Written\n" \
       "          %12" I64_FMT "d  Total Packets Written\n" \
       "          %12" I64_FMT "d  Total Bytes Written\n" \
       "          \n" \
       "          %12" I64_FMT "d  Total Read Calls\n" \
       "          %12" I64_FMT "d  Total Read I/Os\n" \
       "          %12" I64_FMT "d  Internally Handled ARP Packets\n" \
       "          %12" I64_FMT "d  Packets From Ourself\n" \
       "          %12" I64_FMT "d  Total Ignored Packets\n" \
       "          %12" I64_FMT "d  Packets To All Zeroes MAC Read\n" \
       "          %12" I64_FMT "d  Total Packets Read\n" \
       "          %12" I64_FMT "d  Total Bytes Read\n\n"
#define HHC04102 "One of the GetProcAddress calls failed" 
#define HHC04109 "%s"
#define HHC04110 "Maximum device threads (devtmax) of %d exceeded by %d"
#define HHC04111 "%1d:%04X Function %s failed: '[%02d] %s'"

// reserve 17000-17099 messages for QUERY and SET functions
#define HHC17000 "Missing or invalid argument(s)"
#define HHC17001 "Server '%12s' is listening %s"
#define HHC17002 "Server '%12s' is inactive"
#define HHC17003 "%-8s storage is %sBytes '%ssize' "
#define HHC17004 "CPUID  = "I64_FMTX""
#define HHC17005 "CPC SI = %4.4X.%s.%s.%s.%16.16X"
#define HHC17006 "LPARNAME[%2.2X] = %s"
#define HHC17007 "NumCPU = %02d, NumVEC = %02d, MaxCPU = %02d"
#define HHC17008 "Avgproc  %03d%% %02d; MIPS[%4d.%02d]; SIOS[%6d]"
#define HHC17009 "PROC %s%02X %c %03d%%; MIPS[%4d.%02d]; SIOS[%6d]"
#define HHC17010 " - Started        : Stopping        * Stopped"
#define HHC17011 "Avg CP   %03d%% %02d; MIPS[%4d.%02d];"
#define HHC17012 "EMSG = %s"
#define HHC17013 "Process ID = %d"

#define HHC17100 "Timeout value for 'quit' and 'ssd' is '%d' seconds"
#define HHC17199 "%.4s '%s'"

// Reserve 17500-17999 REXX Message
#define HHC17500 "REXX(%s) Initialization failed"
#define HHC17501 "REXX(%s) Script file name missing"
#define HHC17502 "REXX(%s) Error: %s"

// reserve 90000 messages for debugging
#define HHC90000 "DBG: %s"
#define HHC90001 " *** Assertion Failed! *** %s(%d); function: %s()"

/* pttrace.c */
#define HHC90010 "Pttrace: trace is busy"
#define HHC90011 "Pttrace: invalid argument '%s'"
#define HHC90012 "Pttrace: %s%s%s%s%s%s%s%s%s%s%s %s %s to %d %d"

/* from crypto/dyncrypt.c when compiled with debug on*/
#define HHC90100 "%s"
#define HHC90101 "  r%d        : GR%02d"
#define HHC90102 "    address : " F_VADR
#define HHC90103 "    length  : " F_GREG
#define HHC90104 "  GR%02d      : " F_GREG
#define HHC90105 "    bit 56  : %s"
#define HHC90106 "    fc      : %d"
#define HHC90107 "    m       : %s"
#define HHC90108 "  GR%02d  : " F_GREG
#define HHC90109 "  %s %s"
#define HHC90110 "      %s"

/* from scsitape.c trace */
#define HHC90205 "%1d:%04X Tape file '%s', type '%s': error in function '%s': '%s'"

/* from cmpsc.c when compiled with debug on */
#define HHC90300 "CMPSC: compression call"
#define HHC90301 " r%d      : GR%02d"
#define HHC90302 " address : " F_VADR
#define HHC90303 " length  : " F_GREG
#define HHC90304 " GR%02d    : " F_GREG
#define HHC90305 "   st    : %s"
#define HHC90306 "   cdss  : %d"
#define HHC90307 "   f1    : %s"
#define HHC90308 "   e     : %s"
#define HHC90309 "   dictor: " F_GREG
#define HHC90310 "   sttoff: %08X"
#define HHC90311 "   cbn   : %d"
#define HHC90312 "*** Registers committed"
#define HHC90313 "cbn not zero, process individual index symbols"
#define HHC90314 "compress : is %04X (%d)"
#define HHC90315 "*** Interrupt pending, commit and return with cc3"
#define HHC90316 "fetch_cce: index %04X"
#define HHC90317 "fetch_ch : reached end of source"
#define HHC90318 "fetch_ch : %02X at " F_VADR
#define HHC90319 "  cce    : %s"
#define HHC90320 "  cct    : %d"
#define HHC90321 "  act    : %d"
#define HHC90322 "  ec(s)  :%s"
#define HHC90323 "  x1     : %c"
#define HHC90324 "  act    : %d"
#define HHC90325 "  cptr   : %04X"
#define HHC90326 "  cc     : %02X"
#define HHC90327 "  x1..x5 : %s"
#define HHC90328 "  y1..y2 : %s"
#define HHC90329 "  d      : %s"
#define HHC90330 "  ec     : %02X"
#define HHC90331 "  ccs    :%s"
#define HHC90332 "  sd1    : %s"
#define HHC90333 "  sct    : %d"
#define HHC90334 "  y1..y12: %s"
#define HHC90335 "  sc(s)  :%s"
#define HHC90336 "  sd0    : %s"
#define HHC90337 "  y1..y5 : %s"
#define HHC90338 "search_cce index %04X parent"
#define HHC90339 "fetch_sd1: index %04X"
#define HHC90340 "fetch_sd0: index %04X"
#define HHC90341 "search_sd: index %04X parent"
#define HHC90342 "store_is : end of output buffer"
#define HHC90343 "store_is : %04X -> %02X%02X"
#define HHC90344 "store_is : %04X, cbn=%d, GR%02d=" F_VADR ", GR%02d=" F_GREG
#define HHC90345 "store_iss: %04X -> %02X%02X"
#define HHC90346 "store_iss:%s, GR%02d=" F_VADR ", GR%02d=" F_GREG
#define HHC90347 "expand   : is %04X (%d)"
#define HHC90348 "expand   : reached CPU determined amount of data"
#define HHC90349 "fetch_ece: index %04X"
#define HHC90350 "fetch_is : reached end of source"
#define HHC90351 "fetch_is : %04X, cbn=%d, GR%02d=" F_VADR ", GR%02d=" F_GREG
#define HHC90352 "fetch_iss: GR%02d=" F_VADR ", GR%02d=" F_GREG
#define HHC90353 "  ece    : %s"
#define HHC90354 "  psl    : %d"
#define HHC90355 "  pptr   : %04X"
#define HHC90356 "  ecs    :%s"
#define HHC90357 "  ofst   : %02X"
#define HHC90358 "  bit34  : %s"
#define HHC90359 "  csl    : %d"
#define HHC90360 "vstore   : Reached end of destination"
#define HHC90361 F_GREG " - " F_GREG " Same buffer as previously shown"
#define HHC90362 "%s - " F_GREG " Same line as above"
#define HHC90363 "%s"
#define HHC90364 "   zp    : %s"
 
/* tapeccws tapedev */
#define HHC93480 "%1d:%04X TDSPSTAT[%02X] msg1[%-8s] msg2[%-8s] msg[%-8s] mnt[%s] unmnt[%s] TDSPFLAG[%02X]"

/* ctc/lcs/ndis */
#define HHC90900 "DBG: CTC: %s device port %2.2X: %s"
#define HHC90901 "DBG: CTC: %s: %s"

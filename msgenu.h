/* MSGENU.H     (c) Copyright Bernard van der Helm, 2010             */
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

/* These macro's will be deleted when all messages are std */
#define WRITEMSG(id, ...)           writemsg(__FILE__, __LINE__, __FUNCTION__, sysblk.msglvl, "", _(#id " " id "\n"), ## __VA_ARGS__)
#define WRITEMSG_C(id, ...)         writemsg(__FILE__, __LINE__, __FUNCTION__, sysblk.msglvl, "", _(#id " " id ""), ## __VA_ARGS__)
#define WRITECMSG(color, id, ...)   writemsg(__FILE__, __LINE__, __FUNCTION__, sysblk.msglvl, color, _(#id " " id "\n"), ## __VA_ARGS__)
#define WRITECMSG_C(color, id, ...) writemsg(__FILE__, __LINE__, __FUNCTION__, sysblk.msglvl, color, _(#id " " id ""), ## __VA_ARGS__)

/* Use these macro's */
#define MSG(id, s, ...)             #id s " " id "\n", ## __VA_ARGS__
#define MSG_C(id, s, ...)           #id s " " id "", ## __VA_ARGS__
#define WRMSG(id, s, ...)           writemsg(__FILE__, __LINE__, __FUNCTION__, sysblk.msglvl, "", _(#id s " " id "\n"), ## __VA_ARGS__)
#define WRMSG_C(id, s, ...)         writemsg(__FILE__, __LINE__, __FUNCTION__, sysblk.msglvl, "", _(#id s " " id ""), ## __VA_ARGS__)
#define WRCMSG(color, id, s, ...)   writemsg(__FILE__, __LINE__, __FUNCTION__, sysblk.msglvl, color, _(#id s " " id "\n"), ## __VA_ARGS__)
#define WRCMSG_C(color, id, s, ...) writemsg(__FILE__, __LINE__, __FUNCTION__, sysblk.msglvl, color, _(#id s " " id ""), ## __VA_ARGS__)

/* Hercules messages */
#define HHC00001 "%s"
#define HHC00002 "SCLP console not receiving '%s'"
#define HHC00003 "Empty SCP command issued"
#define HHC00004 "Control program identification: type '%s', name '%s', sysplex '%s', level %16.16"I64_FMT"X"
#define HHC00005 "The configuration has been placed into a system check-stop state because of an incompatible service call"
#define HHC00006 "SCLP console interface '%s'"
#define HHC00007 "Previous message fired from function '%s()' in '%s' line %d" 
#define HHC00008 "%s%s"
#define HHC00009 "RRR...RING...GGG!\a"
#define HHC00010 "Enter input for console %1d:%04X"
#define HHC00011 "Function '%s' failed; cache '%d' size '%d': '[%02d] %s'"
#define HHC00012 "Releasing inactive buffer storage"
#define HHC00013 "Herc command: '%s'"

// reserve 20-39 for file related

// reserve 43-58 for option related

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
#define HHC00081 "Match at index %02d, performing command '%s'"
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
#define HHC00102 "Error in fucntion 'create_thread()': '%s'"
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
#define HHC00152 "Server '%12s' is listening %s"
#define HHC00153 "Server '%12s' is inactive"
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
#define HHC00211 "%s"
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
#define HHC00320 "%1d:%04X CCKD file[%d] '%s': shaddow file succesfully added"
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
#define HHC00815 "Processor %s%02X: processer '%s'"
#define HHC00816 "Processor %s%02X: processor is not '%s'"
#define HHC00817 "Processor %s%02X: store status completed"
#define HHC00818 "Processor %s%02X: not eligible for ipl nor restart"
#define HHC00819 "Processor %s%02X: %s"
#define HHC00820 "%s"
#define HHC00821 "Processor %s%02X: vector facility configured '%s'"
#define HHC00822 "Processor %s%02X: machine check due to host error: '%s'"
#define HHC00823 "Processor %s%02X: check-stop due to host error: '%s'"
#define HHC00824 "Processor %s%02X: machine check code %16.16"I64_FMT"u"
#define HHC00825 "USR2 signal received for undetermined device"
#define HHC00826 "%1d:%04X: USR2 signal received"
#define HHC00827 "Processor %s%02X: engine %02X type %1d set: '%s'"
#define HHC00828 "Processor %s%02X: ipl failed: %s"

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
#define HHC01400 "Ctrl-break intercepted: interrupt key depressed simulated"
#define HHC01401 "Ctrl-c intercepted"
#define HHC01402 "'%s' event received: shutdown %sstarting..."
#define HHC01403 "'%s' event received, shutdown previously requested..."
#define HHC01404 "Cannot create the 'Hercules Automatic Operator' thread"
#define HHC01405 "Run commands file '%s' not found"
#define HHC01406 "Startup parm '-l': maximum loadable modules %d exceeded; remainder not loaded"
#define HHC01407 "Usage: %s [-f config-filename] [-d] [-b logo-filename]%s [> logfile]"
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

/* hscmisc.c */
#define HHC01420 "Begin Hercules shutdown"
#define HHC01421 "Releasing configuration"
#define HHC01422 "Configuration release complete"
#define HHC01423 "Calling termination routines"
#define HHC01424 "All termination routines complete"
#define HHC01425 "Hercules shutdown complete"
#define HHC01426 "Shutdown initiated"

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

// reserve 90000 messages for debugging
#define HHC90000 "DBG: %s"
#define HHC90001 " *** Assertion Failed! *** %s(%d); function: %s"

/*
 *                                  N E W   M E S S A G E   F O R M A T
 *=========================================================================================================================================
 *                                  O L D   M E S S A G E   F O R M A T
 */
 
/* bldcfg.c cmdtab.c codepage.c config.c */
#define HHCMD001S "File '%s[%d]': error reading line: '%s'"
#define HHCMD002S "File '%s[%d]': line is too long"
#define HHCMD003S "File '%s': open error: '%s'"
#define HHCMD004S "File '%s': no device statements in file"
#define HHCMD008E "File '%s[%d]': syntax error: '%s'"
#define HHCMD009E "File '%s[%d]': incorrect number of operands"
#define HHCMD012S "File '%s[%d]': '%s' is not a valid CPU '%s'"
#define HHCMD013S "File '%s[%d]': invalid main storage size '%s'"
#define HHCMD014S "File '%s[%d]': invalid expanded storage size '%s'"
#define HHCMD016S "File '%s[%d]': invalid '%s' thread priority '%s'"
#define HHCMD017W "Hercules is not running as root, cannot raise '%s' thread priority"
#define HHCMD018S "File '%s[%d]': invalid number of CPUs '%s'"
#define HHCMD019S "File '%s[%d]': invalid number of VFs '%s'"
#define HHCMD020I "Vector Facility support not configured"
#define HHCMD022S "File '%s[%d]': invalid system epoch '%s'; valid range is 1801-2099"
#define HHCMD023S "File '%s[%d]': invalid timezone offset '%s'"
#define HHCMD029S "File '%s[%d]': invalid SHRDPORT port number '%s'"
#define HHCMD031S "Cannot obtain (%d)MB main storage: '%s'"
#define HHCMD032S "Cannot obtain storage key array: '%s'"
#define HHCMD033S "Cannot obtain (%d)MB expanded storage: '%s'"
#define HHCMD034I "Expanded storage support not installed"
#define HHCMD035S "File '%s[%d]': missing device number or device type"
#define HHCMD036S "File '%s[%d]': '%s' is not a valid device number(s) specification"
#define HHCMD041E "%1d:%04X: device already exists"
#define HHCMD042E "%1d:%04X: devtype '%s' not recognized"
#define HHCMD043E "%1d:%04X: cannot obtain device block, function malloc() failed: '%s'"
#define HHCMD044E "%1d:%04X: device initialization failed"
#define HHCMD045E "%1d:%04X: cannot obtain buffer for device: '%s'"
#define HHCMD046E "%1d:%04X: %s does not exist"
#define HHCMD047I "%1d:%04X: %s detached"
#define HHCMD048E "%1d:%04X: device does not exist"
#define HHCMD049E "%1d:%04X: device already exists"
#define HHCMD051S "File '%s[%d]': invalid serial number: '%s'"
#define HHCMD052W "File '%s[%d]': invalid ECPSVM level value: '%s', 20 Assumed"
#define HHCMD053W "File '%s[%d]': invalid ECPSVM keyword: '%s', NO Assumed"
#define HHCMD054E "Incorrect device count near character(%c)"
#define HHCMD055E "Incorrect device address specification near character(%c)"
#define HHCMD056E "Incorrect device address range(%04X < %04X)"
#define HHCMD057E "%1d:%04X is on wrong channel (1st device defined on channel(%02X))"
#define HHCMD058E "Some or all devices in %04X-%04X duplicate devices already defined"
#define HHCMD059E "Incorrect second device number in device range near character(%c)"
#define HHCMD061W "File '%s[%d]': '%s' statement deprecated. Use '%s' instead"
#define HHCMD062W "File '%s[%d]': missing ECPSVM level value, 20 Assumed"
#define HHCMD063W "File '%s[%d]': specifying ECPSVM level directly is deprecated. Use the 'LEVEL' keyword instead"
#define HHCMD064W "Hercules set priority '%d' failed: '%s'"
#define HHCMD070S "File '%s[%d]': invalid year offset: '%s'"
#define HHCMD072W "SYSEPOCH '%04d' is deprecated, please specify \"SYSEPOCH 1900 '%s%d'\""
#define HHCMD073W "SYSEPOCH '%04d' is deprecated, please specify \"SYSEPOCH 1960 '%s%d'\""
#define HHCMD074S "File '%s[%d]': invalid engine syntax '%s'"
#define HHCMD075S "File '%s[%d]': invalid engine type '%s'"

#define HHCMD081I "File '%s': will ignore include errors"
#define HHCMD082S "File '%s[%d]': maximum nesting level '%d' reached"
#define HHCMD083I "File '%s': including file '%s' at '%d'"
#define HHCMD084W "File '%s': open error ignored file '%s': '%s'"
#define HHCMD085S "File '%s': open error file '%s': '%s'"
#define HHCMD086S "File '%s': NUMCPU '%d' must not exceed MAXCPU '%d'"
#define HHCMD090I "Default Allowed AUTOMOUNT directory = '%s'"
#define HHCMD139E "Unknown herc command: '%s', enter 'HELP' for a list of valid commands"
#define HHCMD140I "Valid panel commands are\n  %-9.9s    %s \n  %-9.9s    %s "
#define HHCMD142I "No HELP available. Unknown Hercules command: '%s'"
#define HHCMD151E "Codepage conversion table '%s' is not defined"
#define HHCMD152I "Using '%s' codepage conversion table '%s'"
#define HHCMD174E "Unspecified error occured while parsing Logical Channel Subsystem Identification"
#define HHCMD175E "No more than 1 Logical Channel Subsystem Identification may be specified"
#define HHCMD176E "Non-numeric Logical Channel Subsystem Identification '%s'"
#define HHCMD177E "Logical Channel Subsystem Identification '%d' exceeds maximum of '%d'"
#define HHCMD900S "Out of memory"

/* cmdtab.c */
#define HHCMD853I "CMDLEVEL invalid option: %s"
#define HHCMD854I "cmdlevel[%2.2X] is %s"

/* diagnose.c */
#define HHCDN001I "Diagnose 0x308 called: System is re-ipled"
#define HHCDN002I "Checking cpu(%d)"
#define HHCDN003I "Waiting 1 second for cpu's to stop"

/* dynguic. */
#define HHCDG001I "Dyngui.dll initiated"
#define HHCDG002I "Dyngui.dll terminated"
#define HHCDG003S "Select failed on input stream: %s"
#define HHCDG004S "Read failed on input stream: %s"
#define HHCDG005E "Device(%4.4X) query buffer overflow"

/* ecpsvm.c */
#define HHCEV001I "| %-9s | %8d | %8d |  %3d%% |"
#define HHCEV002I "| %-9s | %-8s | %-8s | %-5s |"
#define HHCEV003I "+-----------+----------+----------+-------+"
#define HHCEV004W "Abend condition detected in DISP2 instr"
#define HHCEV005I "%d Entr%s not shown (never invoked)"
#define HHCEV006I "%d call(s) was/where made to unsupported functions"
#define HHCEV008E "NO EVM subcommand. Type \"evm help\" for a list of valid subcommands"
#define HHCEV010I "%s : %s"
#define HHCEV011E "Unknown subcommand %s - valid subcommands are :"
#define HHCEV012I "%s : %s"
#define HHCEV013I "ECPS:VM Global Debug %s"
#define HHCEV014I "ECPS:VM %s feature %s %s%s"
#define HHCEV015I "ECPS:VM %s feature %s%s%s"
#define HHCEV016I "All ECPS:VM %s features %s%s"
#define HHCEV017I "But ECPS:VM is currently disabled"
#define HHCEV018W "WARNING ! Unpredictable results may occur"
#define HHCEV019I "The microcode support level is 20"
#define HHCEV024I "* : Unsupported, - : Disabled, %% - Debug"
#define HHCEV025I "Unknown ECPS:VM feature %s; Ignored"
#define HHCEV026I "Current reported ECPS:VM Level is %d"
#define HHCEV027I "Level reported to guest program is now %d"
#define HHCEV028W "WARNING ! current level (%d) is not supported"
#define HHCEV029I "ECPS:VM Command processor invoked"
#define HHCEV030E "Unknown EVM subcommand %s"
#define HHCEV031I "ECPS:VM Command processor complete"
//#define HHCEV300D in ecpsvm.c

/* external.c */
#define HHCCP023I "External interrupt: Interrupt key"
#define HHCCP024I "External interrupt: Clock comparator"
#define HHCCP025I "External interrupt: CPU timer=%16.16" I64_FMT "X"
#define HHCCP026I "External interrupt: Interval timer"
#define HHCCP027I "External interrupt: Service signal %8.8X"
#define HHCCP028I "External interrupt: Block I/O %s"
#define HHCCP031I "%1d:%04X Processing Block I/O interrupt: code(%4.4X) parm(%16.16X) status(%2.2X) subcode(%2.2X)"

/* hsccmd.c */
#define HHCMD001W "Ignoring invalid SCRIPT file pause statement: %s"
#define HHCMD002I "EOF reached on SCRIPT file. Processing complete"
#define HHCMD003E "I/O error reading SCRIPT file: %s"
#define HHCMD004I "Message level %s"
#define HHCMD007E "Script file \"%s\" open failed: %s"
#define HHCMD008I "Script file processing started using file \"%s\""
#define HHCMD011I "Pausing SCRIPT file processing for %d seconds..."
#define HHCMD012I "Resuming SCRIPT file processing..."
#define HHCMD013I "SHCMDOPT %sabled%s"
#define HHCMD015S "Invalid program product OS license setting %s"
#define HHCMD016I "Panel refresh rate = %d millisecond(s)"
#define HHCMD017E "%1d:%04X is not a printer"
#define HHCMD018I "%1d:%04X Printer started"
#define HHCMD019E "%1d:%04X Printer not started: %s"
#define HHCMD021E "Device address is missing"
#define HHCMD025I "%1d:%04X Printer stopped"
#define HHCMD026W "Quiet command ignored: external GUI active"
#define HHCMD027I "Automatic refresh %s"
#define HHCMD028I "ASN and LX reuse is %s"
#define HHCMD029E "Invalid I/O delay value: %s"
#define HHCMD030I "I/O delay = %d"
#define HHCMD031E "Missing device number"
#define HHCMD032I "CTC debugging now %s for %s device %1d:%04X group"
#define HHCMD033I "CTC debugging now %s for all CTCI/LCS device groups"
#define HHCMD034E "%1d:%04X is not a supported CTCI or LCS"
#define HHCMD035E "store status rejected: CPU not stopped"
#define HHCMD036I "TOD clock drag factor = %lf"
#define HHCMD037I "Timer update interval = %d microsecond(s)"
#define HHCMD038I "Restart key depressed"
#define HHCMD039E "Invalid arguments"
#define HHCMD040I "Instruction %s %s %s"
#define HHCMD045I "%1d:%04X attention request raised"
#define HHCMD047E "%1d:%04X attention request rejected"
#define HHCMD049W "Are you sure you didn't mean 'ipl %04X' instead?"
#define HHCMD050I "Interrupt key depressed"
#define HHCMD051I "LOADPARM=%s"

#define HHCMD053I "SHCMDOPT invalid option: %s"
#define HHCMD054S "DIAG8CMD: %sable, %secho"
#define HHCMD056I "LPAR name = %s"
//#define HHCMD059E in hsccmd.c
#define HHCMD060I "LPAR number = %"I16_FMT"X"
#define HHCMD061E "LPARNUM %02X invalid; must be 00 to 3F (hex)"
#define HHCMD062E "Missing argument(s)"
#define HHCMD066E "Program interrupt number %s is invalid"
#define HHCMD067S "Incorrect keyword %s for the ASN_AND_LX_REUSE statement"
#define HHCMD068E "Invalid value: %s; Enter \"help scsimount\" for assistance"
#define HHCMD069I "%s of %s-labeled volume \"%s\" pending for drive %u:%4.4X = %s"
#define HHCMD072I "Device(%4.4X) synchronous: %12" I64_FMT "d asynchronous: %12" I64_FMT "d"
#define HHCMD073I "No synchronous I/O devices found"
#define HHCMD074I "TOTAL synchronous: %12" I64_FMT "d asynchronous: %12" I64_FMT "d  %3" I64_FMT "d%%"
#define HHCMD075E "Invalid max device threads value (must be -1 to n)"
#define HHCMD076I "Max device threads: %d, current: %d, most: %d, waiting: %d, max exceeded: %d"
#define HHCMD078E "Max device threads %d current %d most %d waiting %d total I/Os queued %d"
#define HHCMD079W "%1d:%04X Only devices on CSS 0 are usable in S/370 mode"
#define HHCMD081E "No cckd devices found"
#define HHCMD084E "%1d:%04X is not a cckd device"
#define HHCMD087E "Operand must be `merge', `nomerge' or `force'"
#define HHCMD088E "Operand must be a number -1 .. 4"
#define HHCMD089E "Unexpected operand: %s"	
#define HHCMD091E "Command must be 'sf+', 'sf-', 'sfc', 'sfk' or 'sfd'"
#define HHCMD093E "Missing argument(s)"
#define HHCMD096E "%1d:%04X busy or interrupt pending"
#define HHCMD098I "%1d:%04X initialized"
#define HHCMD099E "savecore rejected: filename missing"
#define HHCMD100I "pantitle = %s"
#define HHCMD101I "Current message held time is %d seconds"
#define HHCMD102I "Held messages cleared"
#define HHCMD103I "The message held time is set to %d seconds"
#define HHCMD104I "Saving locations %8.8X-%8.8X to %s"
#define HHCMD105E "savecore error creating %s: %s"
#define HHCMD106E "savecore error writing to %s: %s"
#define HHCMD107E "savecore: unable to save %d bytes"
#define HHCMD108E "loadcore rejected: filename missing"
#define HHCMD109E "Cannot open %s: %s"
#define HHCMD110E "Legacysenseid invalid option: %s"
#define HHCMD111I "Legacysenseid %sabled"
#define HHCMD112I "Loading %s to location %x"
#define HHCMD113E "MODEL: no model code"
#define HHCMD114E "PLANT: no plant code"
#define HHCMD115E "MANUFACTURER: no manufacturer code"
#define HHCMD116E "Address greater than mainstore size"
#define HHCMD117E "loadtext rejected: CPU not stopped"
#define HHCMD118E "Cannot open %s: %s"
#define HHCMD119E "Cannot read %s: %s"
#define HHCMD120I "Finished loading TEXT deck file, last 'TXT' record had address: %3.3X"
//#define HHCMD123I in hsccmd.c
#define HHCMD126I "Architecture mode = %s"
#define HHCMD127E "All CPU's must be stopped to change architecture"
#define HHCMD128E "Invalid architecture mode %s"
#define HHCMD130E "Invalid frame address %8.8X"
#define HHCMD131I "Frame %8.8X marked %s"
#define HHCMD134I "CKD KEY trace is now %s"
#define HHCMD136I "CCW tracing is now %s for device %1d:%04X"
#define HHCMD137I "CCW stepping is now %s for device %1d:%04X"
#define HHCMD138E "Unrecognized +/- command"
#define HHCMD146E "Work buffer malloc() failed: %s"
#define HHCMD147W "Warning: not all devices shown (max %d)"
#define HHCMD148E "savecore: no modified storage found"
#define HHCMD149W "No dasd devices"
#define HHCMD150W "evm command is deprecated. Use \"ecpsvm\" instead"

#define HHCMD156E "CPUIDFMT must be either 0 or 1"
#define HHCMD157I "CPUIDFMT = %d"

#define HHCMD161I "%s%7d"
#define HHCMD162E "Invalid format. Enter \"help gpr\" for assistance"
#define HHCMD163E "Invalid format. Command does not support any arguments"
#define HHCMD164E "Invalid format. .Enter \"help cr\" for assistance"
#define HHCMD165E "Invalid operand %s"
#define HHCMD166E "No argument"
#define HHCMD170I "savecore command complete"
#define HHCMD180E "shell commands are disabled"
#define HHCMD181E "%1d:%04X not found"
#define HHCMD182E "Device numbers can only be redefined within the same Logical Channel SubSystem"
#define HHCMD183E "Reinit rejected for drive %u:%4.4X; drive not empty"
#define HHCMD184I "'%s'"
#define HHCMD187E "Invalid argument"
#define HHCMD188E "Missing arguments; enter 'help %s' for assistance"
#define HHCMD189I "TT32 debug tracing messages %s"
#define HHCMD190I "Keep-alive = (%d,%d,%d)"
#define HHCMD192E "Invalid format. Enter 'help conkpalv' for assistance"
#define HHCMD195I "Log options: %s"
#define HHCMD196E "Invalid logopt value %s"
#define HHCMD197I "Log option set: %s"
#define HHCMD200E "Missing operand; enter 'HELP AUTOMOUNT' for assistance"
#define HHCMD202E "Empty list"
#define HHCMD203I "\"%c%s\""
#define HHCMD205E "**LOGIC ERROR** file \"%s\", line %d"
#define HHCMD206E "Out of memory"
#define HHCMD207E "Invalid AUTOMOUNT directory: \"%s\": %s"
#define HHCMD208E "AUTOMOUNT directory \"%s\" conflicts with previous specification"
#define HHCMD209E "AUTOMOUNT directory \"%s\" duplicates previous specification"
#define HHCMD210I "%s%s AUTOMOUNT directory = \"%s\""
#define HHCMD211E "Out of memory"
#define HHCMD212I "Default Allowed AUTOMOUNT directory = \"%s\""
#define HHCMD214I "Ok.%s"
#define HHCMD215E "Out of memory"
#define HHCMD216I "Default Allowed AUTOMOUNT directory = \"%s\""
#define HHCMD217E "Empty list"
#define HHCMD218E "Entry not found"
#define HHCMD219E "Unsupported function; enter 'HELP AUTOMOUNT' for assistance"
#define HHCMD320I "%1d:%04X fcb %s/"
#define HHCMD322E "%1d:%04X is not a 1403 device"
#define HHCMD323E "%1d:%04X not stopped"
#define HHCMD324E "%1d:%04X invalid fcb image ==>%s<=="
//#define HHCMD422I in hscmisc.c
#define HHCMD423E "Script file buffer malloc failed: %s"

#define HHCMD425S "DIAG8CMD invalid option: %s"
#define HHCMD426E "%1d:%04X busy or interrupt pending"
#define HHCMD427E "%1d:%04X subchannel not enabled"
#define HHCMD428E "Target processor %s is invalid"
#define HHCMD429E "Missing argument(s)"
#define HHCMD430E "LPARNUM must be one or two hex digits"
#define HHCMD431E "LCSS id %s is invalid"

#define HHCMD433E "Script file \"%s\" not found"
#define HHCMD434I "Hercules instruction trace displayed in %s mode"
#define HHCMD441S "DEFSYM requires a single value (use quotes if necessary)"
#define HHCMD442E "invalid address: %s"
#define HHCMD443E "loadtext rejected: filename missing"
#define HHCMD444I "%d bytes read from %s"
#define HHCMD445E "loadcore rejected: CPU not stopped"
#define HHCMD446E "invalid address: %s"
#define HHCMD447E "invalid range: %8.8X-%8.8X"
#define HHCMD448E "savecore rejected: CPU not stopped"
#define HHCMD449E "savecore: invalid ending address: %s"
#define HHCMD450E "savecore: invalid starting address: %s"
#define HHCMD451S "%s: %s invalid argument"
#define HHCMD452E "ipl rejected: All CPU's must be stopped"
#define HHCMD453E "System reset/clear rejected: All CPU's must be stopped"
#define HHCMD454E "Missing argument"
#define HHCMD467E "Program interrupt number out of range (%4.4X)"
#define HHCMD497E "Initialization failed for device %1d:%04X"
#define HHCMD800I "Hercules has been up for %u week%s, %u day%s, %02u:%02u:%02u"
#define HHCMD801I "Hercules has been up for %u day%s, %02u:%02u:%02u"
#define HHCMD802I "Hercules has been up for %02u:%02u:%02u"
#define HHCMD820I "SCLPROOT %s"
#define HHCMD821I "SCLP DISK I/O Disabled"
#define HHCMD830I "HTTPROOT %s"
#define HHCMD831I "HTTPROOT not specified"
#define HHCMD832S "HTTP server already active"
#define HHCMD833S "Invalid HTTP port number %s"
#define HHCMD834S "Unrecognized argument %s"
#define HHCMD836I "HTTPPORT %d"
#define HHCMD837S "Invalid HTTP_SERVER_CONNECT_KLUDGE value: %s"
#define HHCMD838S "HTTP_SERVER_CONNECT_KLUDGE value: %s"
#define HHCMD850I "Tape mount reinit %sallowed"
#define HHCMD851I "Auto SCSI mount %d seconds"
#define HHCMD860I "SCP '.' and PRIORITY SCP '!' input is %sechoed to hercules console"
#define HHCMD870I "Instruction counts reset to zero"
#define HHCMD871E "Sorry, not enough memory"
#define HHCMD872E "Sorry, too many instructions"
//#define HHCMD875I in hsccmd.c
//#define HHCMD876I in hsccmd.c
#define HHCMD877I  " i: %12" I64_FMT "d"
#define HHCMD878I "%3d: %12" I64_FMT "d"
#define HHCMD994E "Script aborted : user cancel request"
#define HHCMD996E "The script command requires a filename"
#define HHCMD997E "Only 1 script may be invoked from the panel at any time"
#define HHCMD998E "Script aborted : Script recursion level exceeded"
#define HHCMD999I "Script \"%s\" aborted due to previous conditions"

/* hscutl.c */
#define HHCUT001I "SO_KEEPALIVE rc=%d %s"
#define HHCUT002I "TCP_KEEPALIVE rc=%d %s"
#define HHCUT003I "TCP_KEEPIDLE rc=%d %s"
#define HHCUT004I "TCP_KEEPINTVL rc=%d %s"
#define HHCUT005I "TCP_KEEPCNT rc=%d %s"
#define HHCMD042I "%s=%s"

/* httpserv.c */
#define HHCHT002E "Socket error: (%s)"
#define HHCHT003W "Waiting for port(%u) to become free"
#define HHCHT004E "Bind error: (%s)"
#define HHCHT005E "Listen error: (%s)"
#define HHCHT006I "Waiting for HTTP requests on port(%u)"
#define HHCHT007E "Select error: (%s)"
#define HHCHT008E "Accept error: (%s)"
#define HHCHT011E "Html_include: Cannot open file(%s): (%s)"
#define HHCHT013I "Using HTTPROOT directory (%s)"
#define HHCHT014E "Invalid HTTPROOT: (%s): (%s)"

/* hscmisc.c */
#define HHCMD143E "Invalid value: %s"
#define HHCMD144E "Invalid operand: %s"
#define HHCMD145E "Invalid range: %s"

/* logger.c */
#define HHCLG014E "Log not active"
#define HHCLG015I "Log closed"
#define HHCLG016E "Error opening logfile(%s) %s"
#define HHCLG017S "Log file(%s) fdopen failed %s"

/* pttrace.c */
#define HHCPT001E "Invalid argument(%s)"
#define HHCPT002E "Trace is busy"
#define HHCPT003I "Ptt(%s%s%s%s%s%s%s%s%s%s%s %s %s) to(%d %d)"


/* sr.c */
#define HHCSR001I "Resuming suspended file(%s) created"
#define HHCSR010E "Write error(%s)"
#define HHCSR011E "Read error(%s)"
#define HHCSR012E "Seek error(%s)"
#define HHCSR013E "Value error, incorrect length"
#define HHCSR014E "String error, incorrect length"
#define HHCSR015E "Error processing file(%s)"
#define HHCSR016E "CPU key(%8.8X) found but no active CPU"
#define HHCSR101E "Too many arguments"
#define HHCSR102E "File(%s) open error(%s)"
#define HHCSR103W "Waiting for device(%4.4X)"
#define HHCSR104W "Device(%4.4X) still busy, proceeding anyway"
#define HHCSR105E "Archmode(%s) not supported"
#define HHCSR106E "Mainsize mismatch: %dM expected %dM"
#define HHCSR108E "Storkey size mismatch: %d expected %d"
#define HHCSR110E "Xpndsize mismatch: %dM expected %dM"
#define HHCSR113E "Processor(%s%02X) exceeds max allowed cpu(%02d)"
#define HHCSR114E "Processor(%s%02X) already configured"
#define HHCSR115E "Processor(%s%02X) unable to configure online"
#define HHCSR116E "Processor(%s%02X) invalid psw length(%d)"
#define HHCSR117E "Processor(%s%02X) error loading psw - rc(%d)"
#define HHCSR118W "Device(%4.4X) initialization failed"
#define HHCSR119W "Device(%4.4X) type mismatch; %s expected %s"
#define HHCSR120E "Device(%4.4X) %s size mismatch: %d expected %d"
#define HHCSR132E "Device(%4.4X) type mismatch: %4.4X expected %4.4X"
#define HHCSR133E "Device(%4.4X) Unable to resume suspended device: %s"
#define HHCSR203E "All CPU's must be stopped to resume"
#define HHCSR204E "File identifier error"
#define HHCSR999E "Invalid key %8.8x"

/* vm.c */
#define HHCVM001I "Panel command *%s* issued by guest %s"
#define HHCVM005W "Panel command *%s* issued by guest not processed, disabled in configuration"

/* vmd250.c */
#define HHCVM006E "VM BLOCK I/O environment malloc failed"
#define HHCVM007I "Device(%4.4X) d250_init BLKTAB: type(%4.4X) arch(%i) 512(%i) 1024(%i) 2048(%i) 4096(%i)"
#define HHCVM008I "Device(%4.4X) d250_init32 s(%i) o(%i) b(%i) e(%i)"
#define HHCVM009E "d250_list32 error: PSC(%i)"
#define HHCVM011E "VM BLOCK I/O request malloc failed"
#define HHCVM012I "Device(%4.4X) d250_preserve pending sense preserved"
#define HHCVM013I "Device(%4.4X) d250_restore pending sense restored"
#define HHCVM014I "Device(%4.4X) d250_list32 BIOE(%8.8X) status(%2.2X)"
#define HHCVM015I "Device(%4.4X) d250_list32 BIOEs(%i) A("F_RADR") I/O key(%2.2X)"
#define HHCVM016I "Device(%4.4X) d250_list32 BIOE(%8.8X) oper(%2.2X) block(%i) buffer(%8.8X)"
#define HHCVM017I "Device(%4.4X) d250_iorq32 PSC(%d) succeeded(%d) failed(%d)"
#define HHCVM018I "Device(%4.4X) d250_read %d-byte block (rel. to 0): %d"
#define HHCVM019I "Device(%4.4X) ASYNC BIOEL(%8.8X) Entries(%d) Key(%2.2X) Intp(%8.8X)"
#define HHCVM020I "Device(%4.4X) d250_list32 xcode(%4.4X) BIOE32(%8.8X-%8.8X) FETCH key(%2.2X)"
#define HHCVM021I "Device(%4.4X) d250_read FBA unit status(%2.2X) residual(%d)"
#define HHCVM022I "Device(%4.4X) d250_remove Block I/O environment removed"
#define HHCVM023I "Device(%4.4X) Triggered Block I/O interrupt: code(%4.4X) parm(%16.16X) status(%2.2X) subcode(%2.2X)"
#define HHCVM109E "d250_list64 error: PSC(%i)"
#define HHCVM114I "Device(%4.4X) d250_list64 BIOE(%16.16X) status(%2.2X)"
#define HHCVM115I "Device(%4.4X) d250_list64 BIOEs(%i) A("F_RADR" I/O key(%2.2X)"
#define HHCVM116I "Device(%4.4X) d250_list64 BIOE(%16.16X) oper(%2.2X) block(%i) buffer(%16.16X)"
#define HHCVM117I "Device(%4.4X) d250_iorq64 PSC(%d) succeeded(%d) failed(%d)"
#define HHCVM119I "Device(%4.4X) d250_iorq32 SYNC BIOEL(%8.8X) Entries(%d) Key(%2.2X)"
#define HHCVM120I "Device(%4.4X) d250_list32 xcode(%4.4X) Rd Buf(%8.8X-%8.8X) FETCH key(%2.2X)"
#define HHCVM219I "Device(%4.4X) ASYNC BIOEL(%16.16X) Entries(%d) Key(%2.2X) Intp(%16.16X)"
#define HHCVM220I "Device(%4.4X) d250_list32 xcode(%4.4X) Wr Buf(%8.8X-%8.8X) STORE key(%2.2X)"
#define HHCVM319I "Device(%4.4X) d250_iorq64 SYNC BIOEL(%16.16X) Entries9%d) Key(%2.2X)"
#define HHCVM320I "Device(%4.4X) d250_list32 xcode(%4.4X) Status(%8.8X-%8.8X) STORE key(%2.2X)"
#define HHCVM420I "Device(%4.4X) d250_list64 xcode(%4.4X) BIOE64(%8.8X-%8.8X) FETCH key(%2.2X)"
#define HHCVM520I "Device(%4.4X) d250_list64 xcode(%4.4X) Rd Buf(%16.16X-%16.16X) FETCH key(%2.2X)"
#define HHCVM620I "Device(%4.4X) d250_list64 xcode(%4.4X) Wr Buf(%16.16X-%16.16X) STORE key(%2.2X)"
#define HHCVM720I "Device(%4.4X) d250_list64 xcode(%4.4X) Status(%16.16X-%16.16X) STORE key(%2.2X)"



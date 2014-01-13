/* CMDTAB.H     (c) Copyright Roger Bowler, 1999-2012                */
/*              (c) Copyright "Fish" (David B. Trout), 2002-2012     */
/*              (c) Copyright Jan Jaeger, 2003-2012                  */
/*              (c) Copyright TurboHercules, SAS 2010-2011           */
/*              Defines all Hercules Configuration statements        */
/*              and panel commands                                   */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

/*-------------------------------------------------------------------*/
/*              Command descriptions and help text                   */
/*-------------------------------------------------------------------*/

#if defined( _FW_REF )          /* (pre-table build pass) */

//  -------------- (template for new commands) ----------------
//  -------------- (template for new commands) ----------------
//
//#define xxx_cmd_desc            "Short XXX description"
//#define xxx_cmd_help            \n
//                                \n
//  "Much longer, more detailed xxx command description...\n"                     \
//  "Use other commands as reference for typical formatting and wording.\n"       \
//
//  -------------- (template for new commands) ----------------
//  -------------- (template for new commands) ----------------


#define $test_cmd_desc           "Your custom command (*DANGEROUS!*)"
#define $test_cmd_help          \
                                \
  "Performs whatever test function *YOU* specifically coded it to do.\n\n"      \
                                                                                \
  "                  * * * *  WARNING!  * * * *\n\n"                            \
                                                                                \
  "DO NOT RUN THIS COMMAND UNLESS *YOU* SPECIFICALLY CODED THE FUNCTION\n"      \
  "THAT THIS COMMAND INVOKES! Unless you wrote it yourself you probably\n"      \
  "don't know it does. It could perform any function at all from crashing\n"    \
  "Hercules to launching a nuclear strike. You have been warned!\n"

#define $zapcmd_cmd_desc        "Enable/disable command (*CAREFUL!*)"
#define $zapcmd_cmd_help        \
                                \
  "Format:\n\n"                                                                 \
                                                                                \
  "    $zapcmd  cmdname  CFG|NOCFG|CMD|NOCMD\n\n"                               \
                                                                                \
  "For normal non-Debug production release builds, use the sequence:\n\n"       \
                                                                                \
    "    msglvl   VERBOSE      (optional)\n"                                    \
    "    msglvl   DEBUG        (optional)\n"                                    \
    "    cmdlvl   DEBUG        (*required!*) (because not DEBUG build,\n"       \
    "    $zapcmd  cmdname  CMD                and $zapcmd is SYSDEBUG)\n\n"     \
                                                                                \
    "In other words, the $zapcmd is itself a 'debug' level command, and\n"      \
    "thus in order to use it, the debug cmdlvl must be set first (which\n"      \
    "is the default for Debug builds but not normal production builds).\n"      \
    "Note: it is possible to disable the $zapcmd itself so BE CAREFUL!\n"

#define $locate_cmd_desc        "Display sysblk, regs or hostinfo"
#define bangmsg_cmd_desc        "SCP priority message"
#define bangmsg_cmd_help        \
                                \
  "To enter a system control program (i.e. guest operating system)\n"           \
  "priority command on the hercules console, simply prefix the command\n"       \
  "with an exclamation point '!'.\n"

#define hash_cmd_desc           "Silent comment"
#define star_cmd_desc           "Loud comment"
#define reply_cmd_desc          "SCP command"
#define reply_cmd_help          \
                                \
  "To reply to a system control program (i.e. guest operating system)\n"        \
  "message that gets issued to the hercules console, prefix the reply\n"        \
  "with a period.\n"

#define quest_cmd_desc          "alias for help"
#define aea_cmd_desc            "Display AEA tables"
#define aia_cmd_desc            "Display AIA fields"
#define alrf_cmd_desc           "Command deprecated: Use \"archlvl enable|disable|query asn_lx_reuse\" instead"
#define ar_cmd_desc             "Display access registers"
#define archlvl_cmd_desc        "Set Architecture Level"
#define archlvl_cmd_help        \
                                \
  "Format: archlvl s/370|als0 | esa/390|als1 | esame|als2 | z/arch|als3\n"      \
  "                enable|disable <facility> [s/370|esa/390|z/arch]\n"          \
  "                query [<facility> | all]\n"                                  \
  "command without any argument simply displays the current architecture\n"     \
  "mode. Entering the command with an argument sets the architecture mode\n"    \
  "to the specified value.\n"

#define archmode_cmd_desc       "Alias for archlvl"
#define asnlx_cmd_desc          "Command deprecated: Use \"archlvl enable|disable|query asn_lx_reuse\" instead"
#define attach_cmd_desc         "Configure device"
#define attach_cmd_help         \
                                \
  "Format: \"attach devn type [arg...]\n"

#define autoscsi_cmd_desc       "Command deprecated - Use \"SCSIMOUNT\""
#define autoscsi_cmd_help       \
                                \
  "This command is deprecated. Use \"scsimount\" instead.\n"

#define autoinit_cmd_desc       "Display/Set automatic create empty tape file switch"
#define autoinit_cmd_help       \
                                \
  "Format: \"autoinit [on|off]\"\n"                                             \
  "Default for autoinit is off.\n"                                              \
  "When autoinit is off, devinit will return a file not found error\n"          \
  "if the specified file is not found.\n"                                       \
  "When autoinit is on, devinit will initialize a blank, non-labeled\n"         \
  "tape if the requested file is not found. Tape will have two tapemarks\n"     \
  "and be positioned at the beginning of the tape."

#define automount_cmd_desc      "Display/Update allowable tape automount directories"
#define automount_cmd_help      \
                                \
  "Format: \"automount  { add <dir> | del <dir> | list }\"\n"                   \
  "\n"                                                                          \
  "Adds or deletes entries from the list of allowable/unallowable tape\n"       \
  "automount directories, or lists all currently defined list entries,\n"       \
  "if any.\n"                                                                   \
  "\n"                                                                          \
  "The format of the <dir> directory operand for add/del operations is\n"       \
  "identical to that as described in the documentation for the AUTOMOUNT\n"     \
  "configuration file statement (i.e. prefix with '+' or '-' as needed).\n"     \
  "\n"                                                                          \
  "The automount feature is appropriately enabled or disabled for all\n"        \
  "tape devices as needed depending on the updated empty/non-empty list\n"      \
  "state.\n"

#define bminus_cm_desc          "Delete breakpoint"
#define bminus_cm_help          \
                                \
  "Format: \"b-\"  This command is the same as \"s-\"\n"

#define b_cmd_desc              "Set breakpoint"
#define b_cmd_help              \
                                \
  "Format: \"b addr\" or \"b addr-addr\" where 'addr' is the instruction\n"     \
  "address or range of addresses where you wish to halt execution. This\n"      \
  "command is synonymous with the \"s+\" command.\n"

#define bplus_cmd_desc          "Set breakpoint"
#define cache_cmd_desc          "Execute cache related commands"
#define cache_cmd_help          \
                                \
  "Format: \"cache [dasd system [on|off]]\"\n"                                  \
  "\n"                                                                          \
  "dasd system on|off         will enable(on) or disable(off) caching for\n"    \
  "                           all dasd devices\n"                               \
  "dasd system                will present status of system dasd caching\n"     \
  "\n"                                                                          \
  "Command without arguments will present cache stats.\n"

#define cachestats_cmd_desc     "Cache stats command"
#define capping_cmd_desc        "Set/display capping value"
#define capping_cmd_help        \
                                \
  "Format: capping [ n | off ]\n"                                                       \
  "         If no operands are specified, the current capping value is displayed,\n"    \
  "         the value represents the maximum total number of MIPS for all of the\n"     \
  "         'CP' type processors.\n"                                                    \
  "\n"                                                                                  \
  "     n    Maximum total number of MIPS for all of the 'CP' type processors.\n"       \
  "          A zero value will turn off MIP capping\n"                                  \
  "     off  Turn off capping\n"

#define cckd_cmd_desc           "cckd command"
#define cckd_cmd_help           \
                                \
  "The cckd statement is used to display current cckd processing\n"             \
  "options and statistics, and to set new cckd options.\n"                      \
  "Type \"cckd help\" for additional information.\n"

#define cd_cmd_desc             "Change directory"
#define cf_cmd_desc             "Configure current CPU online or offline"
#define cf_cmd_help             \
                                \
  "Configure current CPU online or offline:  Format->  \"cf [on|off]\"\n"       \
  "Where the 'current' CPU is defined as whatever CPU was defined as\n"         \
  "the panel command target cpu via the \"cpu\" panel command. (Refer\n"        \
  "to the 'cpu' command for further information) Entering 'cf' by itself\n"     \
  "simply displays the current online/offline status of the current cpu.\n"     \
  "Otherwise the current cpu is configured online or offline as\n"              \
  "specified.\n"                                                                \
  "Use 'cfall' to configure/display all CPUs online/offline state.\n"

#define cfall_cmd_desc          "Configure all CPU's online or offline"
#define clocks_cmd_desc         "Display tod clkc and cpu timer"
#define cmdlevel_cmd_desc       "Display/Set current command group"
#define cmdlevel_cmd_help       \
                                \
  "Format: cmdlevel [{+/-}{ALL|MAINT|PROGrammer|OPERator|DEVELoper}]\n"

#define cmdlvl_cmd_desc         "(alias for cmdlevel)"
#define cmdsep_cmd_desc         "Display/Set command line separator"
#define cmdsep_cmd_help         \
                                \
  "A command line separator character allows multiple commands\n"               \
  "to be entered on a single line.\n"                                           \
  "\n"                                                                          \
  "Format: cmdsep [c | off ]\n"                                                 \
  "        c       a single character used for command separation. Must\n"      \
  "                not be '.', '!', or '-'.  Note: using '#' may prevent\n"     \
  "                lines with comments from being processed correctly.\n"       \
  "        off     disables command separation.\n"

#define cmdtgt_cmd_desc         "Specify the command target"
#define cmdtgt_cmd_help         \
                                \
  "Format: \"cmdtgt [herc | scp | pscp | ?]\". Specify the command target.\n"

#define cnslport_cmd_desc       "Set console port"
#if defined(_FEATURE_CMPSC_ENHANCEMENT_FACILITY)
#define cmpscpad_cmd_desc       "Set/display the CMPSC zero padding value."
#define cmpscpad_cmd_help       \
                                \
  "The CMPSCPAD command defines the zero padding storage alignment boundary\n"  \
  "for the CMPSC-Enhancement Facility. It must be a power of 2 value ranging\n" \
  "anywhere from " QSTR( MIN_CMPSC_ZP_BITS ) " to " QSTR( MAX_CMPSC_ZP_BITS ) ". Enter the command with no arguments to display the\n" \
  "current value.\n"

#endif /* defined(_FEATURE_CMPSC_ENHANCEMENT_FACILITY) */
#define codepage_cmd_desc       "Set/display code page conversion table"
#define codepage_cmd_help       \
                                \
  "Format: 'codepage [cp]'\n"                                                   \
  "        'codepage maint cmd [operands]' - see cp_updt command for\n"         \
  "                                          help\n"                            \
  "If no operand is specified, the current codepage is displayed.\n"            \
  "If 'cp' is specified, then code page is set to the specified page\n"         \
  "if the page is valid.\n"

#define conkpalv_cmd_desc       "Display/alter console TCP keep-alive settings"
#define conkpalv_cmd_help       \
                                \
  "Format: \"conkpalv (idle, intv, count)\" where 'idle', 'intv' and 'count'\n" \
  "are the new values for the TCP keep-alive settings for console\n"            \
  "connections:\n"                                                              \
  "- send probe when connection goes idle for 'idle' seconds\n"                 \
  "- wait maximum of 'intv' seconds for a response to probe\n"                  \
  "- disconnect after 'count' consecutive failed probes\n"                      \
  "The format must be exactly as shown, with each value separated from\n"       \
  "the next by a single comma, no intervening spaces between them,\n"           \
  "surrounded by parenthesis. The command \"conkpalv\" without any operand\n"   \
  "displays the current values.\n"

#define count_cmd_desc          "Display/clear overall instruction count"
#define cp_updt_cmd_desc        "Create/Modify user character conversion table"
#define cp_updt_cmd_help        \
                                \
  "Format: 'cp_updt cmd [operands]'\n"                                          \
  "\n"                                                                          \
  "  altER e|g2h|a|h2g (pos, val, pos, val,...)\n"                              \
  "                              - alter the user eBCDIC/g2h or aSCII/h2g\n"    \
  "                                table value at hex POSition to hex\n"        \
  "                                VALue 16 pairs of hex digits may be\n"       \
  "                                specified within the parens\n"               \
  "\n"                                                                          \
  "  dsp|disPLAY e|g2h|a|h2g     - display user eBCDIC|aSCII table\n"           \
  "\n"                                                                          \
  "  expORT e|g2h|a|h2g filename - export contents of user table to file\n"     \
  "\n"                                                                          \
  "  impORT e|g2h|a|h2g filename - import file contents into user table\n"      \
  "\n"                                                                          \
  "  refERENCE [cp]              - copy codepage to user tables\n"              \
  "                                if cp is not specified, a list of\n"         \
  "                                valid codepage tables is generated\n"        \
  "\n"                                                                          \
  "  reset                       - reset the internal user tables to\n"         \
  "                                binary zero\n"                               \
  "\n"                                                                          \
  "  test                        - verify that user tables are\n"               \
  "                                transparent, i.e. the value at\n"            \
  "                                position n in g2h used as an index\n"        \
  "                                into h2g will return a value equal n\n"      \
  "                                ( g2h<=>h2g, h2g<=>g2h )\n"                  \
  "\n"                                                                          \
  " *e|g2h represent ebcdic; a|h2g represent ascii\n"                           \
  " **lower case part of the cmd name represent the minimum abbreviation\n"     \
  "   for the command\n"                                                        \
  "\n"                                                                          \
  "To activate the user table, enter the command 'codepage user'\n"             \
  "\n"                                                                          \
  "Note: ebcdic refers to guest to host translation\n"                          \
  "      ascii refers to host to guest translation\n"                           \
  "      These terms are used for historical purposes and do not\n"             \
  "      represent the literal term.\n"

#define cpu_cmd_desc            "Define target cpu for panel display and commands"
#define cpu_cmd_help            \
                                \
  "Format: \"cpu xx\" where 'xx' is the hexadecimal cpu address of the cpu\n"   \
  "in your multiprocessor configuration which you wish all panel commands\n"    \
  "to apply to. If command text follows the cpu address, the command will\n"    \
  "execute on cpu xx and the target cpu will not be permanently changed.\n"     \
  "For example, entering 'cpu 1F' followed by \"gpr\" will change the\n"        \
  "target cpu for the panel display and commands and then display the\n"        \
  "general purpose registers for cpu 31 of your configuration. Entering\n"      \
  "'cpu 14 gpr' will execute the 'gpr' command on cpu 20, but will not\n"       \
  "change the target cpu for subsequent panel displays and commands.\n"

#define cpuidfmt_cmd_desc       "Set format BASIC/0/1 STIDP generation"
#define cpumodel_cmd_desc       "Set CPU model number"
#define cpuprio_cmd_desc        "Set/Display cpuprio parameter"
#define cpuserial_cmd_desc      "Set CPU serial number"
#define cpuverid_cmd_desc       "Set CPU verion number"
#define cr_cmd_desc             "Display or alter control registers"
#define cr_cmd_help             \
                                \
  "Format: \"cr [nn=xxxxxxxxxxxxxxxx]\" where 'nn' is the optional\n"           \
  "control register number (0 to 15) and 'xxxxxxxxxxxxxxxx' is the\n"           \
  "control register value in hex (1-8 hex digits for 32-bit registers\n"        \
  "or 1-16 hex digits for 64-bit registers). Enter \"cr\" by itself to\n"       \
  "display the control registers without altering them.\n"

#define cscript_cmd_desc        "Cancels a running script thread"
#define cscript_cmd_help        \
                                \
  "Format: \"cscript  ['*' | 'ALL' | id]\".  This command cancels a running\n"  \
  "script or all scripts. If '*' or 'ALL' is given then all running scripts\n"  \
  "are canceled. If no arguments are given only the first running script is\n"  \
  "cancelled. Otherwise the specific script 'id' is canceled. The 'script'\n"   \
  "command may be used to display a list of all currently running scripts.\n"

#define ctc_cmd_desc            "Enable/Disable CTC debugging"
#define ctc_cmd_help            \
                                \
  "Format:  \"ctc  debug  { on | off }  [ <devnum> | ALL ]\".\n\n"              \
  "Enables/disables debug packet tracing for the specified CTCI/LCS/PTP\n"          \
  "device group(s) identified by <devnum> or for all CTCI/LCS/PTP device\n"         \
  "groups if <devnum> is not specified or specified as 'ALL'.\n"

#define define_cmd_desc         "Rename device"
#define define_cmd_help         \
                                \
  "Format: \"define olddevn newdevn\"\n"

#define defstore_cmd_desc       "Define/Display main and expanded storage values"
#define defstore_cmd_help       \
                                \
  "Format: defstorE [ mAIN [ ssss[S] [ lOCK | unlOCK ] ] ]\n"                   \
  "                 [ xSTORE | eXPANDED [ ssss[S] [ lOCK | unlOCK ] ] ]\n"      \
  "        Main and Expanded Storage may be specified on the same command\n"    \
  "        line.\n"                                                             \
  "\n"                                                                          \
  "                   Without any options, display current settings for both\n" \
  "                   types of storage\n"                                       \
  "\n"                                                                          \
  "        mAIN     - define/display main storage allocations\n"                \
  "        xSTORE   - define/display expanded storage allocations\n"            \
  "        eXPANDED\n"                                                          \
  "\n"                                                                          \
  "        ssss     - specify amount of storate in M units\n"                   \
  "        ssssS    - specify amount of storage in 'S' units where 'S' is\n"    \
  "                   B = no multiplier             - main only\n"              \
  "                   K = 2**10 (kilo/kibi)         - main only\n"              \
  "                   M = 2**20 (mega/mebi)\n"                                  \
  "                   G = 2**30 (giga/gibi)\n"                                  \
  "                   T = 2**40 (tera/tebi)         - not 32-bit\n"             \
  "                   P = 2**50 (peta/pebi)         - not 32-bit\n"             \
  "                   E = 2**60 (exa/exbi)          - not 32-bit\n"             \
  "\n"                                                                          \
  "        lOCK    - attempt to lock storage (pages lock by host OS)\n"         \
  "        unlOCK  - leave storage unlocked (pagable by host OS)\n"             \
  "\n"                                                                          \
  "      (none)    - display current value(s)\n"                                \
  "\n"                                                                          \
  " Note: Multipliers 'T', 'P', and 'E' are not available on 32bit machines\n"  \
  "       Expanded Storage is allocated in minimum of 1M units\n"

#define defsym_cmd_desc         "Define symbol"
#define defsym_cmd_help         \
                                \
  "Format: \"defsym symbol [value]\". Defines symbol 'symbol' to contain\n"      \
  "value 'value'. The symbol can then be the object of a substitution for\n"     \
  "later panel commands. If 'value' contains blanks or spaces, then it\n"        \
  "must be enclosed within quotes or apostrophes. For more detailed\n"           \
  "information regarding symbol substitution refer to the 'DEFSYM'\n"            \
  "configuration file statement in Hercules documentation.\n"                    \
  "Enter \"defsym\" by itself to display the values of all defined\n"            \
  "symbols.\n"

#define delsym_cmd_desc         "Delete a symbol"
#define delsym_cmd_help         \
                                \
  "Format: \"delsym symbol\". Deletes symbol 'symbol'.\n"

#define detach_cmd_desc         "Remove device"
#define devinit_cmd_desc        "Reinitialize device"
#define devinit_cmd_help        \
                                \
  "Format: \"devinit devn [arg...]\"\n"                                          \
  "If no arguments are given then the same arguments are used\n"                 \
  "as were used the last time the device was created/initialized.\n"

#define devlist_cmd_desc        "List device, device class, or all devices"
#define devlist_cmd_help        \
                                \
  "Format: \"devlist [devn | devc]\"\n"                                          \
  "    devn       is a single device address\n"                                  \
  "    devc       is a single device class. Device classes are CON,\n"           \
  "               CTCA, DASD, DSP, LINE, PCH, PRT, QETH, RDR, and TAPE.\n"       \
  "\n"                                                                           \
  "If no arguments are given then all devices will be listed.\n"

#define devprio_cmd_desc        "Set/Display devprio parameter"
#define devtmax_cmd_desc        "Display or set max device threads"
#define devtmax_cmd_help        \
                                \
  "Specifies the maximum number of device threads allowed.\n"                    \
  "\n"                                                                           \
  "Specify -1 to cause 'one time only' temporary threads to be created\n"        \
  "to service each I/O request to a device. Once the I/O request is\n"           \
  "complete, the thread exits. Subsequent I/O to the same device will\n"         \
  "cause another worker thread to be created again.\n"                           \
  "\n"                                                                           \
  "Specify 0 to cause an unlimited number of 'semi-permanent' threads\n"         \
  "to be created on an 'as-needed' basis. With this option, a thread\n"          \
  "is created to service an I/O request for a device if one doesn't\n"           \
  "already exist, but once the I/O is complete, the thread enters an\n"          \
  "idle state waiting for new work. If a new I/O request for the device\n"       \
  "arrives before the timeout period expires, the existing thread will\n"        \
  "be reused. The timeout value is currently hard coded at 5 minutes.\n"         \
  "Note that this option can cause one thread (or possibly more) to be\n"        \
  "created for each device defined in your configuration. Specifying 0\n"        \
  "means there is no limit to the number of threads that can be created.\n"      \
  "\n"                                                                           \
  "Specify a value from 1 to nnn  to set an upper limit to the number of\n"      \
  "threads that can be created to service any I/O request to any device.\n"      \
  "Like the 0 option, each thread, once done servicing an I/O request,\n"        \
  "enters an idle state. If a new request arrives before the timeout\n"          \
  "period expires, the thread is reused. If all threads are busy when a\n"       \
  "new I/O request arrives however, a new thread is created only if the\n"       \
  "specified maximum has not yet been reached. If the specified maximum\n"       \
  "number of threads has already been reached, then the I/O request is\n"        \
  "placed in a queue and will be serviced by the first available thread\n"       \
  "(i.e. by whichever thread becomes idle first). This option was created\n"     \
  "to address a threading issue (possibly related to the cygwin Pthreads\n"      \
  "implementation) on Windows systems.\n"                                        \
  "\n"                                                                           \
  "The default for Windows is 8. The default for all other systems is 0.\n"

#define diag8_cmd_desc          "Set diag8 command option"
#define dir_cmd_desc            "Displays a list of files and subdirs in a directory"
#define ds_cmd_desc             "Display subchannel"
#define ecps_cmd_desc           "Command deprecated - Use \"ECPSVM\""
#define ecps_cmd_help           \
                                \
  "This command is deprecated. Use \"ecpsvm\" instead.\n"

#define ecpsvm_cmd_desc         "ECPS:VM Commands"
#define ecpsvm_cmd_help         \
                                \
  "Format: \"ecpsvm\". This command invokes ECPS:VM Subcommands.\n"              \
  "Type \"ecpsvm help\" to see a list of available commands\n"

#define engines_cmd_desc        "Set engines parameter"
#define evm_cmd_desc            "Command deprecated - Use \"ECPSVM\""
#define evm_cmd_help            \
                                \
  "This command is deprecated. Use \"ecpsvm\" instead.\n"

#if defined(ENABLE_OBJECT_REXX) || defined(ENABLE_REGINA_REXX)
#define exec_cmd_desc           "Execute a Rexx script"
#define exec_cmd_help           \
                                \
  "Format: \"exec [mode] rexx_exec [args...]\" where 'rexx_exec' \n"             \
  "is the name of the Rexx script, \n"                                           \
  "and 'args' are arguments (separated by spaces) to be passed to the script.\n" \
  "the arguments passing style is determined by the REXX Mode settings\n"        \
  "it can be overridden for the current exec invocation specifying the mode\n"   \
  "as ... \"exec com rexx_exec [args...]\" for command style arguments\n"        \
  "or ... \"exec sub rexx_exec [args...]\" for subroutine style arguments\n"
#endif /* defined(ENABLE_OBJECT_REXX) || defined(ENABLE_REGINA_REXX) */

#define exit_cmd_desc           "(Synonym for 'quit')"
#define ext_cmd_desc            "Generate external interrupt"
#define f_cmd_desc              "Mark frames unusable/usable"
#define fcb_cmd_desc            "Display the current FCB (if only the printer is given)"
#define fcb_cmd_help            \
                                \
  "Reset the fcb to the standard one\n"                                          \
  "Load a fcb image\n"

#define fpc_cmd_desc            "Display or alter floating point control register"
#define fpc_cmd_help            \
                                \
  "Format: \"fpc [xxxxxxxxxxxxxxxx]\" where 'xxxxxxxxxxxxxxxx' is the\n"         \
  "register value in hexadecimal (1-8 hex digits). Enter \"fpc\" by itself\n"    \
  "to display the register value without altering it.\n"

#define fpr_cmd_desc            "Display or alter floating point registers"
#define fpr_cmd_help            \
                                \
  "Format: \"fpr [nn=xxxxxxxxxxxxxxxx]\" where 'nn' is the register number\n"    \
  "(0 to 15 or 0, 2, 4 or 6 depending on the Control Register 0 AFP bit) and\n"  \
  "'xxxxxxxxxxxxxxxx' is the register value in hexadecimal (1-16 hex digits\n"   \
  "for 64-bit registers). Enter \"fpr\" by itself to display the register\n"     \
  "values without altering them.\n"

#define g_cmd_desc              "Turn off instruction stepping and start all CPUs"
#define gpr_cmd_desc            "Display or alter general purpose registers"
#define gpr_cmd_help            \
                                \
  "Format: \"gpr [nn=xxxxxxxxxxxxxxxx]\" where 'nn' is the optional\n"           \
  "register number (0 to 15) and 'xxxxxxxxxxxxxxxx' is the register\n"           \
  "value in hexadecimal (1-8 hex digits for 32-bit registers or 1-16 hex\n"      \
  "digits for 64-bit registers). Enter \"gpr\" by itself to display the\n"       \
  "register values without altering them.\n"

#define hao_cmd_desc            "Hercules Automatic Operator"
#define hao_cmd_help            \
                                \
  "Format: \"hao  tgt <tgt> | cmd <cmd> | list <n> | del <n> | clear \".\n"      \
  "  hao tgt <tgt> : define target rule (regex pattern) to react on\n"           \
  "  hao cmd <cmd> : define command for previously defined rule\n"               \
  "  hao list <n>  : list all rules/commands or only at index <n>\n"             \
  "  hao del <n>   : delete the rule at index <n>\n"                             \
  "  hao clear     : delete all rules (stops automatic operator)\n"

#define help_cmd_desc           "list all commands / command specific help"
#define help_cmd_help           \
                                \
  "Format: \"help [cmd|c*]\".\n"                                                 \
  "\n"                                                                           \
  "The command without any options will display a short description\n"           \
  "of all of the commands available matching the current cmdlevel. You\n"        \
  "may specify a partial command name followed by an '*' to get a\n"             \
  "list matching the partial command name. For example 'help msg*'\n"            \
  "will list all commands beginning with 'msg' and matching the current\n"       \
  "cmdlevel.\n"                                                                  \
  "\n"                                                                           \
  "This command with the 'cmd' option will display a long form of help\n"        \
  "information associated with that command if the command is available\n"       \
  "for the current cmdlevel.\n"                                                  \
  "\n"                                                                           \
  "Help text may be limited to explaining the general format of the\n"           \
  "command and its various required or optional parameters and is not\n"         \
  "meant to replace the appropriate manual.\n"

#define herc_cmd_desc           "Hercules command"
#define herc_cmd_help           \
                                \
  "Format: \"herc [cmd]\". Send hercules cmd in any cmdtgt mode.\n"

#define herclogo_cmd_desc       "Read a new hercules logo file"
#define herclogo_cmd_help       \
                                \
  "Format: \"herclogo [<filename>]\". Load a new logo file for 3270\n"           \
  "terminal sessions. If no filename is specified, the built-in logo\n"          \
  "is used instead.\n"

#define hercprio_cmd_desc       "Set/Display hercprio parameter"
#define hst_cmd_desc            "History of commands"
#define hst_cmd_help            \
                                \
  "Format: \"hst | hst n | hst l\". Command \"hst l\" or \"hst 0\" displays\n"   \
  "list of last ten commands entered from command line\n"                        \
  "hst n, where n is a positive number retrieves n-th command from list\n"       \
  "hst n, where n is a negative number retrieves n-th last command\n"            \
  "hst without an argument works exactly as hst -1, it retrieves the\n"          \
  "last command\n"

#define http_cmd_desc           "Start/Stop/Modify/Display HTTP Server"
#define http_cmd_help           \
                                \
  "Format: 'http [start|stop|port nnnn [[noauth]|[auth user pass]]|root path]'\n"   \
  "\n"                                                                              \
  "start                                 - starts HTTP server if stopped\n"         \
  "stop                                  - stops HTTP server if started\n"          \
  "port nnnn [[noauth]|[auth user pass]] - set port and optional authorization.\n"  \
  "                                        Default is noauthorization needed.\n"    \
  "                                        'auth' requires a user and password\n"   \
  "\n"                                                                              \
  "root path                             - set the root file path name\n"           \
  "\n"                                                                              \
  "<none>                                - display status of HTTP server\n"

#define httpport_cmd_desc       "Command deprecated - Use \"HTTP PORT ...\""
#define httpport_cmd_help       \
                                \
  "This command is deprecated. Use \"http port ...\" instead.\n"

#define httproot_cmd_desc       "Command deprecated - Use \"HTTP ROOT fn\""
#define httproot_cmd_help       \
                                \
  "This command is deprecated. Use \"http root fn\" instead.\n"

#define i_cmd_desc              "Generate I/O attention interrupt for device"
#define icount_cmd_desc         "Display individual instruction counts"
#define iodelay_cmd_desc        "Display or set I/O delay value"
#define iodelay_cmd_help        \
                                \
  "Format:  \"iodelay  n\".\n\n"                                                                                                \
  "Specifies the amount of time (in microseconds) to wait after an\n"            \
  "I/O interrupt is ready to be set pending. This value can also be\n"           \
  "set using the Hercules console. The purpose of this parameter is\n"           \
  "to bypass a bug in the Linux/390 and zLinux dasd.c device driver.\n"          \
  "The problem is more apt to happen under Hercules than on a real\n"            \
  "machine because we may present an I/O interrupt sooner than a\n"              \
  "real machine.\n"

#define ipending_cmd_desc       "Display pending interrupts"
#define ipl_cmd_desc            "IPL from device or file"
#define ipl_cmd_help            \
                                \
  "Format: \"ipl xxxx | cccc [loadparm xxxxnnnn | parm xxxxxxxxxxxxxx] [clear]\""\
  "\n\n"                                                                         \
  "Performs the Initial Program Load manual control function. If the\n"          \
  "first operand 'xxxx' is a 1- to 4-digit hexadecimal number, a\n"              \
  "CCW-type IPL is initiated from the indicated device number, and\n"            \
  "SCLP disk I/O is disabled. Otherwise a list-directed IPL is performed\n"      \
  "from the .ins file named 'cccc', and SCLP disk I/O is enabled for the\n"      \
  "directory path where the .ins file is located.\n"                             \
  "\n"                                                                           \
  "An optional 'loadparm' keyword followed by a 1-8 character string can\n"      \
  "be used to set the LOADPARM prior to the IPL.\n"                              \
  "\n"                                                                           \
  "An optional 'parm' keyword followed by a string can also be passed to\n"      \
  "the IPL command processor. The string will be loaded into the\n"              \
  "low-order 32 bits of the general purpose registers (4 characters per\n"       \
  "register for up to 64 bytes). The PARM option behaves similarly to\n"         \
  "the VM IPL command.\n"                                                        \
  "\n"                                                                           \
  "An optional 'clear' keyword will initiate a Load Clear manual control\n"      \
  "function, prior to starting an IPL.\n"

#define iplc_cmd_desc           "Command deprecated - use IPL with clear option"
#define iplc_cmd_help           \
                                \
  "This command is deprecated. Use \"ipl\" with clear option specified instead.\n"

#define k_cmd_desc              "Display cckd internal trace"
#define kd_cmd_desc             "Short form of 'msghld clear'"
#define ldmod_cmd_desc          "Load a module"
#define ldmod_cmd_help          \
                                \
  "Format: \"ldmod module ...\"\n"                                               \
  "Specifies additional modules that are to be loaded by the\n"                  \
  "Hercules dynamic loader.\n"

#define legacy_cmd_desc         "Set legacysenseid setting"
#define loadcore_cmd_desc       "Load a core image file"
#define loadcore_cmd_help       \
                                \
  "Format: \"loadcore filename [address]\" where 'address' is the storage\n"     \
  "address of where to begin loading memory. The file 'filename' is\n"           \
  "presumed to be a pure binary image file previously created via the\n"         \
  "'savecore' command. The default for 'address' is 0 (beginning of\n"           \
  "storage).\n"

#define loadparm_cmd_desc       "Set IPL parameter"
#define loadparm_cmd_help       \
                                \
  "Specifies the eight-character IPL parameter which is used by\n"               \
  "some operating systems to select system parameters."

#define loadtext_cmd_desc       "Load a text deck file"
#define loadtext_cmd_help       \
                                \
  "Format: \"loadtext filename [address]\". This command is essentially\n"       \
  "identical to the 'loadcore' command except that it loads a text deck\n"       \
  "file with \"TXT\" and \"END\" 80 byte records (i.e. an object deck).\n"

#define locks_cmd_desc          "Display internal locks list"
#define locks_cmd_help          \
                                \
  "Format: \"locks [HELD|tid|ALL] [SORT [TIME|TOD]|[OWNER|TID]|NAME|LOC]\"\n"

#define log_cmd_desc            "Direct logger output"
#define log_cmd_help            \
                                \
  "Format: \"log [ OFF | newfile ]\".   Sets log filename or stops\n"            \
  "log file output with the \"OFF\" option."

#define logopt_cmd_desc         "Set/Display logging options"
#define logopt_cmd_help         \
                                \
  "Format: \"logopt [timestamp | notimestamp]\".   Sets logging options.\n"      \
  "\"timestamp\" inserts a time stamp in front of each log message.\n"           \
  "\"notimestamp\" displays log messages with no time stamps.  Entering\n"       \
  "the command with no arguments displays current logging options.\n"            \
  "\"timestamp\" and \"notimestamp\" may be abbreviated as \"time\"\n"           \
  "and \"notime\" respectively.\n"

#define lparname_cmd_desc       "Set LPAR name"
#define lparname_cmd_help       \
                                \
  "Specifies the eight-character LPAR name returned by\n"                        \
  "DIAG X'204'. The default is HERCULES"

#define lparnum_cmd_desc        "Set LPAR identification number"
#define lparnum_cmd_help        \
                                \
   "Specifies the one- or two-digit hexadecimal LPAR identification\n"           \
   "number stored by the STIDP instruction, or BASIC. If a one-digit\n"          \
   "hexadecimal number from 1 to F is specified, then STIDP stores a\n"          \
   "format-0 CPU ID. If a two-digit hexadecimal number is specified,\n"          \
   "except 10, then STIDP stores a format-1 CPU ID. For LPARNUM 10, \n"          \
   "STIDP uses the current CPUIDFMT setting. If LPARNUM is BASIC, then\n"        \
   "STIDP stores a basic-mode CPU ID. The default LPAR identification\n"         \
   "number is 1.\n"

#define ls_cmd_desc             "List directory contents"
#define lsdep_cmd_desc          "List module dependencies"
#define lsmod_cmd_desc          "List dynamic modules"
#define mainsize_cmd_desc       "Define/Display mainsize parameter"
#define mainsize_cmd_help       \
                                \
  "Format: mainsize [ mmmm | nnnS [ lOCK | unlOCK ] ]\n"                         \
  "        mmmm    - define main storage size mmmm Megabytes\n"                  \
  "\n"                                                                           \
  "        nnnS    - define main storage size nnn S where S is the\n"            \
  "                  multipler:\n"                                               \
  "                  B = no multiplier\n"                                        \
  "                  K = 2**10 (kilo/kibi)\n"                                    \
  "                  M = 2**20 (mega/mebi)\n"                                    \
  "                  G = 2**30 (giga/gibi)\n"                                    \
  "                  T = 2**40 (tera/tebi)\n"                                    \
  "                  P = 2**50 (peta/pebi)\n"                                    \
  "                  E = 2**60 (exa/exbi)\n"                                     \
  "\n"                                                                           \
  "        lOCK    - attempt to lock storage (pages lock by host OS)\n"          \
  "        unlOCK  - leave storage unlocked (pagable by host OS)\n"              \
  "\n"                                                                           \
  "      (none)    - display current mainsize value\n"                           \
  "\n"                                                                           \
  " Note: Multipliers 'T', 'P', and 'E' are not available on 32bit machines\n"

#define manuf_cmd_desc          "Set STSI manufacturer code"
#define maxcpu_cmd_desc         "Set maxcpu parameter"
#define maxrates_cmd_desc       "Display highest MIPS/SIOS rate or set interval"
#define maxrates_cmd_help       \
                                \
  "Format: \"maxrates [nnnn]\" where 'nnnn' is the desired reporting\n"          \
  "interval in minutes or 'midnight'. Acceptable values are from\n"              \
  "1 to 1440. The default is 1440 minutes (one day).\n"                          \
  "The interval 'midnight' sets the interval to 1440 and aligns the\n"           \
  "start of the current interval to midnight.\n"                                 \
  "Entering \"maxrates\" by itself displays the current highest\n"               \
  "rates observed during the defined intervals.\n"

#define message_cmd_desc        "Display message on console a la VM"
#define message_cmd_help        \
                                \
  "Format: \"message * text\". The 'text' field is variable in size.\n"          \
  "A 'VM' formatted similar to \"13:02:41  * MSG FROM HERCULES: hello\" is\n"    \
  "diplayed on the console panel as a result of the panel command\n"             \
  "'message * hello'.\n"

#define model_cmd_desc          "Set/Query STSI model code"
#define model_cmd_help          \
                                \
  "Format:\n"                                                                    \
  "\n"                                                                           \
  "     model [hardware [capacity [permanent [temporary]]]]\n"                   \
  "\n"                                                                           \
  "where:\n"                                                                     \
  "\n"                                                                           \
  "<null>       specifies a query of the current model code settings.\n"         \
  "\n"                                                                           \
  "hardware     specifies the hardware model setting. Specifying an \"=\"\n"     \
  "             resets the hardware model to \"EMULATOR\"; specifying an\n"      \
  "             \"*\" leaves the current hardware model setting intact.\n"       \
  "             The default hardware model is \"EMULATOR\".\n"                   \
  "\n"                                                                           \
  "capacity     specifies the capacity model setting. Specifying an \"=\"\n"     \
  "             copies the current hardware model; specifying an \"*\" \n"       \
  "             leaves the current capacity model setting intact. The\n"         \
  "             default capacity model is \"EMULATOR\".\n"                       \
  "\n"                                                                           \
  "permanent    specifies the permanent model setting. Specifying an\n"          \
  "             \"=\" copies the current capacity model; specifying an\n"        \
  "             \"*\" leaves the current permanent model setting intact.\n"      \
  "             The default permanent model is \"\" (null string).\n"            \
  "\n"                                                                           \
  "temporary    specifies the temporary model setting. Specifying an\n"          \
  "             \"=\" copies the current permanent model; specifying an\n"       \
  "             \"*\" leaves the current temporary model setting intact.\n"      \
  "             The default temporary model is \"\" (null string).\n"

#define modpath_cmd_desc        "Set module load path"
#define mtapeinit_cmd_desc      "Control tape initialization"
#define mtapeinit_cmd_help      \
                                \
  "Format: \"mounted_tape_reinit [disallow|disable | allow|enable]\"\n"          \
  "Specifies whether reinitialization of tape drive devices\n"                   \
  "(via the devinit command, in order to mount a new tape)\n"                    \
  "should be allowed if there is already a tape mounted on\n"                    \
  "the drive. The current value is displayed if no operand is\n"                 \
  "specified\n"                                                                  \
  "Specifying ALLOW or ENABLE indicates new tapes may be\n"                      \
  "mounted (via 'devinit nnnn new-tape-filename') irrespective\n"                \
  "of whether or not there is already a tape mounted on the drive.\n"            \
  "This is the default state.\n"                                                 \
  "Specifying DISALLOW or DISABLE prevents new tapes from being\n"               \
  "mounted if one is already mounted. When DISALLOW or DISABLE has\n"            \
  "been specified and a tape is already mounted on the drive, it\n"              \
  "must first be unmounted (via the command 'devinit nnnn *') before\n"          \
  "the new tape can be mounted. Otherwise the devinit attempt to\n"              \
  "mount the new tape is rejected.\n"

#define msg_cmd_desc            "Alias for message"
#define msghld_cmd_desc         "Display or set the timeout of held messages"
#define msghld_cmd_help         \
                                \
  "Format: \"msghld [value | info | clear]\".\n"                                 \
  "value: timeout value of held message in seconds\n"                            \
  "info:  displays the timeout value\n"                                          \
  "clear: releases the held messages\n"

#define msglevel_cmd_desc       "Display/Set current Message Display output"
#define msglevel_cmd_help       \
                                \
  "Format: msglevel [on|off|text|time|debug|nodebug|verbose|terse|{device}]\n"   \
  "  on      Normal message display\n"                                           \
  "  off     No messages are displayed\n"                                        \
  "  text    Text portion only of message is display\n"                          \
  "  time    Timestamp is prefixed to message\n"                                 \
  "  debug   Messages prefixed with source and linenumber\n"                     \
  "  nodebug Turn off debug\n"                                                   \
  "  tape    Tape related messages\n"                                            \
  "  dasd    DASD related messages\n"                                            \
  "  comm    Communications related messages\n"                                  \
  "  ur      Unit Record related messages\n"                                     \
  "  scsi    SCSI related messages\n"                                            \
  "  ctca    CTCA, LCS related messages\n"                                       \
  "  graf    Graphics (3270) related messages\n"                                 \
  "  thread  Threading related messages\n"                                       \
  "  channel Channel related messages\n"                                         \
  "  verbose Display messages during configuration file processing\n"            \
  "  terse   Turn off verbose"

#define msglvl_cmd_desc         "Alias for msglevel"
#define msgnoh_cmd_desc         "Similar to \"message\" but no header"
#define mt_cmd_desc             "Control magnetic tape operation"
#define mt_cmd_help             \
                                \
  "Format:     \"mt device operation [ 1-9999 ]\".\n"                            \
  "  Operations below can be used on a valid tape device. The device\n"          \
  "  must not have any I/O operation in process or pending.\n"                   \
  "     operation   description\n"                                               \
  "       rew       rewind tape to the beginning\n"                              \
  "       asf n     position tape at 'n' file  (default = 1)\n"                  \
  "       fsf n     forward space 'n' files    (default = 1)\n"                  \
  "       bsf n     backward space 'n' files   (default = 1)\n"                  \
  "       fsr n     forward space 'n' records  (default = 1)\n"                  \
  "       bsr n     backward space 'n' records (default = 1)\n"                  \
  "       wtm n     write 'n' tapemarks        (default = 1)\n"                  \
  "       dse       data secure erase\n"                                         \
  "       dvol1     display VOL1 header\n"

#define numcpu_cmd_desc         "Set numcpu parameter"
#define numvec_cmd_desc         "Set numvec parameter"
#define ostailor_cmd_desc       "Tailor trace information for specific OS"
#define ostailor_cmd_help       \
                                \
  "Format: \"ostailor [quiet|os/390|z/os|vm|vse|linux|opensolaris|null]\".\n"    \
  "Specifies the intended operating system. The effect is to reduce\n"           \
  "control panel message traffic by selectively suppressing program\n"           \
  "check trace messages which are considered normal in the specified\n"          \
  "environment. The option 'quiet' suppresses all exception messages,\n"         \
  "whereas 'null' suppresses none of them. The other options suppress\n"         \
  "some messages and not others depending on the specified o/s. Prefix\n"        \
  "values with '+' to combine them with existing values or '-' to exclude\n"     \
  "them. SEE ALSO the 'pgmtrace' command which allows you to further fine\n"     \
  "tune the tracing of program interrupt exceptions.\n"

#define panrate_cmd_desc        "Display or set rate at which console refreshes"
#define panrate_cmd_help        \
                                \
  "Format: \"panrate [nnn | fast | slow]\".\n"                                                  \
  "Sets or displays the panel refresh rate.\n"                                                  \
  "panrate nnn sets the refresh rate to nnn milliseconds.\n"                                    \
  "panrate fast sets the refresh rate to " QSTR(PANEL_REFRESH_RATE_FAST) " milliseconds.\n"  \
  "panrate slow sets the refresh rate to " QSTR(PANEL_REFRESH_RATE_SLOW) " milliseconds.\n"  \
  "If no operand is specified, panrate displays the current refresh rate.\n"

#define pantitle_cmd_desc       "Display or set console title"
#define pantitle_cmd_help       \
                                \
  "Format: pantitle [\"title string\"]\n"                                        \
  "        pantitle \"\"\n"                                                      \
  "\n"                                                                           \
  "Sets or displays the optional console window title-bar\n"                     \
  "string to be used in place of the default supplied by\n"                      \
  "the windowing system. The value should be enclosed within\n"                  \
  "double quotes if there are embedded blanks.\n"                                \
  "\n"                                                                           \
  "An empty string (\"\") will remove the existing console title.\n"             \
  "\n"                                                                           \
  "The default console title will be a string consisting of\n"                   \
  "LPARNAME - SYSTYPE * SYSNAME * SYSPLEX - System Status: color\n"              \
  "\n"                                                                           \
  "SYSTYPE, SYSNAME, and SYSPLEX are populated by the system call\n"             \
  "SCLP Control Program Identification. If a value is blank, then\n"             \
  "that field is not presented.\n"                                               \
  "\n"                                                                           \
  "System Status colors: GREEN  - is every thing working correctly\n"            \
  "                      YELLOW - one or more CPUs are not running\n"            \
  "                      RED    - one or more CPUs are in a disabled\n"          \
  "                               wait state\n"

#define pgmprdos_cmd_desc       "Set LPP license setting"
#define pgmprdos_cmd_help       \
  "Format: \"pgmprdos restricted | licensed\"\n\n"                               \
  "Note: It is YOUR responsibility to comply with the terms of the license for\n"\
  "      the operating system you intend to run on Hercules. If you specify\n"   \
  "      LICENSED and run a licensed operating system in violation of that\n"    \
  "      license, then don't come after the Hercules developers when the vendor\n"\
  "      sends his lawyers after you.\n"


#define pgmtrace_cmd_desc       "Trace program interrupts"
#define pgmtrace_cmd_help       \
                                \
  "Format: \"pgmtrace [-]intcode\" where 'intcode' is any valid program\n"       \
  "interruption code in the range 0x01 to 0x40. Precede the interrupt\n"         \
  "code with a '-' to stop tracing of that particular program\n"                 \
  "interruption.\n"

#define plant_cmd_desc          "Set STSI plant code"
#define pr_cmd_desc             "Display prefix register"
#define pscp_cmd_desc           "Send prio message scp command"
#define pscp_cmd_help           \
                                \
  "Format: \"pscp [cmd]\". Send priority message cmd to scp in any cmdtgt mode.\n"

#define psw_cmd_desc            "Display or alter program status word"
#define psw_cmd_help            \
                                \
  "Format: \"psw [operand ...]\" where 'operand ...' is one or more\n"           \
  "optional parameters which modify the contents of the Program Status\n"        \
  "Word:\n\n"                                                                    \
  "  am=24|31|64           addressing mode\n"                                    \
  "  as=ar|home|pri|sec    address-space\n"                                      \
  "  cc=n                  condition code       (decimal 0 to 3)\n"              \
  "  cmwp=x                C/M/W/P bits         (one hex digit)\n"               \
  "  ia=xxx                instruction address  (1 to 16 hex digits)\n"          \
  "  pk=n                  protection key       (decimal 0 to 15)\n"             \
  "  pm=x                  program mask         (one hex digit)\n"               \
  "  sm=xx                 system mask          (2 hex digits)\n"                \
  "\n"                                                                           \
  "Enter \"psw\" by itself to display the current PSW without altering it.\n"

#define ptp_cmd_desc            "Enable/Disable PTP debugging"
#define ptp_cmd_help            \
                                \
  "Format:  \"ptp  debug  { on | off } [ [ <devnum> | ALL ] [ mask ] ]\".\n\n"   \
  "Enables/disables debug tracing for the PTP device group\n"                    \
  "identified by <devnum>, or for all PTP device groups if\n"                    \
  "<devnum> is not specified or specified as 'ALL'.\n"

#define ptt_cmd_desc            "Set or display internal trace"
#define ptt_cmd_help            \
                                \
  "Format: \"ptt [options] [nnn]\"\n"                                               \
  "When specified with no operands, the ptt command displays the trace options\n"   \
  "and the contents of the internal trace table.\n"                                 \
  "When specified with operands, the ptt command sets the trace options and/or\n"   \
  "specifies which events are to be traced. If the last operand is numeric, it\n"   \
  "sets the size of the trace table and activates the trace.\n"                     \
  "options:\n"                                                                      \
  "  (no)control - control trace\n"                                                 \
  "  (no)error   - error trace\n"                                                   \
  "  (no)inter   - interlock failure trace\n"                                       \
  "  (no)io      - io trace\n"                                                      \
  "  (no)lock    - lock trace buffer\n"                                             \
  "  (no)logger  - logger trace\n"                                                  \
  "  (no)prog    - program interrupt trace\n"                                       \
  "  (no)sie     - sie trace\n"                                                     \
  "  (no)signal  - signaling trace\n"                                               \
  "  (no)threads - thread trace\n"                                                  \
  "  (no)timer   - timer trace\n"                                                   \
  "  (no)tod     - timestamp trace entries\n"                                       \
  "  (no)wrap    - wrap trace buffer\n"                                             \
  "  to=nnn      - trace buffer display timeout\n"                                  \
  "  nnn         - trace buffer size\n"

#define pwd_cmd_desc            "Print working directory"
#define qcpuid_cmd_desc         "Display cpuid"
#define qcpuid_cmd_help         \
                                \
  "Display cpuid and STSI results presented to the SCP\n"

#define qd_cmd_desc             "Query dasd"
#define qpfkeys_cmd_desc        "Display the current PF Key settings"
#define qpid_cmd_desc           "Display Process ID of Hercules"
#define qports_cmd_desc         "Display TCP/IP ports in use"
#define qproc_cmd_desc          "Display processors type and utilization"
#define qstor_cmd_desc          "Display main and expanded storage values"
#define quiet_cmd_desc          "Toggle automatic refresh of panel display data"
#define quiet_cmd_help          \
                                \
  "'quiet' either disables automatic screen refreshing if it is\n"               \
  "currently enabled or enables it if it is currently disabled.\n"               \
  "When disabled you will no be able to see the response of any\n"               \
  "entered commands nor any messages issued by the system nor be\n"              \
  "able to scroll the display, etc. Basically all screen updating\n"             \
  "is disabled. Entering 'quiet' again re-enables screen updating.\n"

#define quit_ssd_cmd_desc       "Terminate the emulator"
#define quit_ssd_cmd_help       \
                                \
  "Format: \"quit [force]\"  Terminates the emulator. If the guest OS\n"         \
  "                        has enabled Signal Shutdown, then a\n"                \
  "                        signal shutdown request is sent to the\n"             \
  "                        guest OS and termination will begin\n"                \
  "                        after guest OS has shutdown.\n"                       \
  "              force     This option will terminate the emulator\n"            \
  "                        immediately.\n"

#define hwldr_cmd_desc          "Specify boot loader filename"
#define hwldr_cmd_help          \
                                \
  "Format: \"hwldr scsiboot [filename]\"  Specifies the bootstrap loader\n"      \
  "                                     to be used for FCP attached SCSI\n"      \
  "                                     devices.\n"

#define loaddev_cmd_desc        "Specify bootstrap loader IPL parameters"
#define loaddev_cmd_help        \
                                \
  "Format: \"loaddev [options]\"  Specifies optional parameters to be\n"         \
  "                             passed to be bootstrap loader.\n"                \
  "  Valid options are:\n\n"                          \
  "  \"portname [16 digit WWPN]\" Fibre Channel Portname of the FCP device\n"    \
  "  \"lun      [16 digit LUN]\"  Fibre Channel Logical Unit Number\n"           \
  "  \"bootprog [number]\"        The boot program number to be loaded\n"        \
  "  \"br_lba   [16 digit LBA]\"  Logical Block Address of the boot record\n"    \
  "  \"scpdata  [data]\"          Information to be passed to the OS\n"

#define dumpdev_cmd_desc        "Specify bootstrap loader DUMP parameters"
#define dumpdev_cmd_help        \
                                \
  "Format: \"dumpdev [options]\"  Specifies optional parameters to be\n"         \
  "                             passed to be bootstrap loader.\n"                \
  "  Valid options are:\n\n"                          \
  "  \"portname [16 digit WWPN]\" Fibre Channel Portname of the FCP device\n"    \
  "  \"lun      [16 digit LUN]\"  Fibre Channel Logical Unit Number\n"           \
  "  \"bootprog [number]\"        The boot program number to be loaded\n"        \
  "  \"br_lba   [16 digit LBA]\"  Logical Block Address of the boot record\n"    \
  "  \"scpdata  [data]\"          Information to be passed to the OS\n"

#define quit_cmd_desc           "Terminate the emulator"
#define quit_cmd_help           \
                                \
  "Format: \"quit [force]\"  Terminates the emulator. The \"quit\"\n"            \
  "                        command will first check that all online\n"           \
  "                        CPUs are stopped. If any CPU is not in a\n"           \
  "                        stopped state, a message is displayed\n"              \
  "                        indicating the number of CPUs running. A\n"           \
  "                        second message follows that prompts for\n"            \
  "                        confirmation by entering a second \"quit\"\n"         \
  "                        command to start termination of the\n"                \
  "                        emulator.\n"                                          \
  "              force     This option will terminate hercules\n"                \
  "                        immediately.\n"

#define quitmout_cmd_desc       "Set/Display the quit timeout value"
#define quitmout_cmd_help       \
                                \
  "Format: \"quitmount [n]\" Sets or displays the elasped time that\n"           \
  "                        a confirming quit/ssd command must be\n"              \
  "                        entered.\n"                                           \
  "               n        This is a value between 2 and 60,\n"                  \
  "                        representing seconds.\n"

#define r_cmd_desc              "Display or alter real storage"
#define r_cmd_help              \
                                \
  "Format: \"r addr[.len]\" or \"r addr[-addr2]\" to display real\n"             \
  "storage, or \"r addr=value\" to alter real storage, where 'value'\n"          \
  "is a hex string of up to 32 pairs of digits.\n"

#define restart_cmd_desc        "Generate restart interrupt"
#define resume_cmd_desc         "Resume hercules"

#if defined(ENABLE_OBJECT_REXX) || defined(ENABLE_REGINA_REXX)
#if defined(ENABLE_OBJECT_REXX) && defined(ENABLE_REGINA_REXX)
#define rexx_cmd_desc           "Modify/Display Rexx interpreter settings"
#define rexx_cmd_help           \
                                \
  "Format: 'rexx [option [parms]]'\n"                                            \
  "<none>             - display rexx status\n"                                   \
  "ena[ble]/sta[rt]   - enable/start rexx \n"                                    \
  "                   - package name oorexx/regina\n"                            \
  "                   - <none> will enable/start the default Rexx interpreter\n" \
  "disa[ble]/sto[p]   - disable/stop rexx support\n"                             \
  "\n"                                                                           \
  "Path[s]/Rexxp[aths]- where to find rexx scripts\n"                            \
  "Sysp[ath]          - extend the search to the System paths\n"                 \
  "                   - on/off\n"                                                \
  "Ext[ensions]       - what extensions to use for rexx scripts autodetect \n"   \
  "Suf[fixes]         - same as above\n"                                         \
  "                   - a search for no extension will ALWAYS be done \n"        \
  "Resolv[er]         - on, hercules will resolve the script full path\n"        \
  "                   - off, the script name will be passed as is\n"             \
  "Msgl[evel]         - 0/1 disable/enable HHC17503I and HHC17504I messages \n"  \
  "Msgp[refix]        - set the prefix for normal messages\n"                    \
  "Errp[refix]        - set the prefix for trace/error messages\n"               \
  "Mode               - define the argument passing style\n"                     \
  "                   - command/subroutine\n"                                    \
  "\n"                                                                           \
  "using reset as parameter will reset the above settings to the defaults\n"
#else /* !defined(ENABLE_OBJECT_REXX) || !defined(ENABLE_REGINA_REXX) */
#define rexx_cmd_desc           "display Rexx interpreter settings"
#define rexx_cmd_help           \
                                \
  "Format: 'rexx [option [parms]]'\n"                                            \
  "<none>             - display rexx status\n"                                   \
  "\n"                                                                           \
  "Path[s]/Rexxp[aths]- where to find rexx scripts\n"                            \
  "Sysp[ath]          - extend the search to the System paths\n"                 \
  "                   - on/off\n"                                                \
  "Ext[ensions]       - what extensions to use for rexx scripts autodetect \n"   \
  "Suf[fixes]         - same as above\n"                                         \
  "                   - a search for no extension will ALWAYS be done \n"        \
  "Resolv[er]         - on, hercules will resolve the script full path\n"        \
  "                   - off, the script name will be passed as is\n"             \
  "Msgl[evel]         - 0/1 disable/enable HHC17503I and HHC17504I messages \n"  \
  "Msgp[refix]        - set the prefix for normal messages\n"                    \
  "Errp[refix]        - set the prefix for trace/error messages\n"               \
  "Mode               - define the argument passing style\n"                     \
  "                   - command/subroutine\n"                                    \
  "\n"                                                                           \
  "using reset as parameter will reset the above settings to the defaults\n"
#endif /* defined(ENABLE_OBJECT_REXX) && defined(ENABLE_REGINA_REXX) */
#endif /* defined(ENABLE_OBJECT_REXX) || defined(ENABLE_REGINA_REXX) */

#define rmmod_cmd_desc          "Delete a module"
#define sminus_cmd_desc         "Turn off instruction stepping"
#define s_cmd_desc              "Instruction stepping"
#define s_cmd_help              \
                                \
  "Format: \"s addr-addr\" or \"s addr:addr\" or \"s addr.length\"\n"            \
  "sets the instruction stepping and instruction breaking range,\n"              \
  "(which is totally separate from the instruction tracing range).\n"            \
  "With or without a range, the s command displays whether instruction\n"        \
  "stepping is on or off and the range if any.\n"                                \
  "The s command by itself does not activate instruction stepping.\n"            \
  "Use the s+ command to activate instruction stepping.\n"                       \
  "\"s 0\" eliminates the range (all addresses will be stepped).\n"

#define squest_cmd_desc         "Instruction stepping query"
#define squest_cmd_help         \
                                \
  "Format: \"s?\" displays whether instruction stepping is on or off\n"          \
  "and the range if any.\n"

#define sdev_cmd_desc           "Turn CCW stepping on/off"
#define splus_cmd_desc          "Instruction stepping on"
#define splus_cmd_help          \
                                \
  "Format: \"s+\" turns on instruction stepping. A range can be specified\n"     \
  "as for the \"s\" command, otherwise the existing range is used. If\n"         \
  "there is no range (or range was specified as 0) then the range\n"             \
  "includes all addresses. When an instruction within the range is about\n"      \
  "to be executed, the CPU is temporarily stopped and the next instruction\n"    \
  "is displayed. You may then examine registers and/or storage, etc,\n"          \
  "before pressing 'Enter' to execute the instruction and stop at the next\n"    \
  "instruction. To turn off instruction stepping and continue execution,\n"      \
  "enter the \"g\" command.\n"

#define savecore_cmd_desc       "Save a core image to file"
#define savecore_cmd_help       \
                                \
  "Format: \"savecore filename [{start|*}] [{end|*}]\" where 'start' and\n"      \
  "'end' define the starting and ending addresss of the range of real\n"         \
  "storage to be saved to file 'filename'. An '*' for either the start\n"        \
  "address or end address (the default) means: \"the first/last byte of\n"       \
  "the first/last modified page as determined by the storage-key\n"              \
  "'changed' bit\".\n"

#define sclproot_cmd_desc       "Set SCLP base directory"
#define sclproot_cmd_help       \
                                \
  "Format: \"sclproot [path|NONE]\"\n"                                           \
  "Enables SCLP disk I/O for the specified directory path, or disables\n"        \
  "SCLP disk I/O if NONE is specified. A subsequent list-directed IPL\n"         \
  "resets the path to the location of the .ins file, and a CCW-type IPL\n"       \
  "disables SCLP disk I/O. If no operand is specified, sclproot displays\n"      \
  "the current setting.\n"

#define scp_cmd_desc            "Send scp command"
#define scp_cmd_help            \
                                \
  "Format: \"scp [cmd]\". Send scp cmd in any cmdtgt mode.\n"

#define scpecho_cmd_desc        "Set/Display option to echo to console and history of scp replys"
#define scpecho_cmd_help        \
                                \
  "Format: \"scpecho [ on | off ]\"\n"                                           \
  "When scpecho is set ON, scp commands entered on the console are\n"            \
  "echoed to console and recording in command history.\n"                        \
  "The default is on. When scpecho is entered without any options,\n"            \
  "the current state is displayed. This is to help manage passwords\n"           \
  "sent to the scp from being displayed and journaled.\n"

#define scpimply_cmd_desc       "Set/Display option to pass non-hercules commands to the scp"
#define scpimply_cmd_help       \
                                \
  "Format: \"scpimply [ on | off ]\"\n"                                          \
  "When scpimply is set ON, non-hercules commands are passed to\n"               \
  "the scp if the scp has enabled receipt of scp commands. The\n"                \
  "default is off. When scpimply is entered without any options,\n"              \
  "the current state is displayed.\n"

#define script_cmd_desc         "Run a sequence of panel commands contained in a file"
#define script_cmd_help         \
                                \
  "Format: \"script [filename [filename] ...]\". Sequentially executes\n"        \
  "the commands contained within the file 'filename'. The script file\n"         \
  "may also contain \"script\" commands, but the system ensures that no\n"       \
  "more than " QSTR( MAX_SCRIPT_DEPTH ) " levels of script are invoked at any one time.\n\n"  \
                                                                                 \
  "Enter the command with no arguments to list all running scripts.\n"

#define scsimount_cmd_desc      "Automatic SCSI tape mounts"
#define scsimount_cmd_help      \
                                \
  "Format:    \"scsimount [ no | yes | 0-99 ]\".\n"                              \
  "\n"                                                                           \
  "Displays or modifies the automatic SCSI tape mounts option.\n\n"              \
  "When entered without any operands, it displays the current interval\n"        \
  "and any pending tape mount requests. Entering 'no' (or 0 seconds)\n"          \
  "disables automount detection.\n"                                              \
  "\n"                                                                           \
  "Entering a value between 1-99 seconds (or 'yes') enables the option\n"        \
  "and specifies how often to query SCSI tape drives to automatically\n"         \
  "detect when a tape has been mounted (upon which an unsolicited\n"             \
  "device-attention interrupt will be presented to the guest operating\n"        \
  "system). 'yes' is equivalent to specifying a 5 second interval.\n"

#define sfminus_cmd_desc        "Delete shadow file"
#define sfplus_cmd_desc         "Add shadow file"
#define sfc_cmd_desc            "Compress shadow files"
#define sfd_cmd_desc            "Display shadow file stats"
#define sfk_cmd_desc            "Check shadow files"
#define sfk_cmd_help            \
                                \
  "Format: \"sfk{*|xxxx} [n]\". Performs a chkdsk on the active shadow file\n"   \
  "where xxxx is the device number (*=all cckd devices)\n"                       \
  "and n is the optional check level (default is 2):\n"                          \
  " -1 devhdr, cdevhdr, l1 table\n"                                              \
  "  0 devhdr, cdevhdr, l1 table, l2 tables\n"                                   \
  "  1 devhdr, cdevhdr, l1 table, l2 tables, free spaces\n"                      \
  "  2 devhdr, cdevhdr, l1 table, l2 tables, free spaces, trkhdrs\n"             \
  "  3 devhdr, cdevhdr, l1 table, l2 tables, free spaces, trkimgs\n"             \
  "  4 devhdr, cdevhdr. Build everything else from recovery\n"                   \
  "You probably don't want to use `4' unless you have a backup and are\n"        \
  "prepared to wait a long time.\n"

#define sh_cmd_desc             "Shell command"
#if defined( _MSVC_ )
#define sh_cmd_help             \
                                \
  "Format: \"sh command [args...]\" where 'command' is any valid shell\n"        \
  "command or the special command 'startgui'. The entered command and any\n"     \
  "arguments are passed as-is to the shell for processing and the results\n"     \
  "are displayed on the Hercules console.\n\n"                                   \
                                                                                 \
  "The special startgui command MUST be used if the command being started\n"     \
  "either directly or indirectly starts a Windows graphical user interface\n"    \
  "(i.e. non-command-line) program such as notepad. Failure to use startgui\n"   \
  "in such cases will hang Hercules until you close/exit notepad. Note that\n"   \
  "starting a batch file which starts notepad still requires using startgui.\n"  \
  "If 'foo.bat' does: \"start notepad\", then doing \"sh foo.bat\" will hang\n"   \
  "Hercules until notepad exits just as doing \"sh start foo.bat\" will too.\n"  \
  "Use startgui to invoke foo.bat instead: \"sh startgui foo.bat\".\n"

#else /* !defined( _MSVC ) */

#define sh_cmd_help             \
                                \
  "Format: \"sh command [args...]\" where 'command' is any valid shell\n"        \
  "command. The entered command and any arguments are passed as-is to the\n"     \
  "shell for processing and the results are displayed on the Hercules console.\n"

#endif /* defined( _MSVC ) */

#define showdvol1_cmd_desc      "Enable showing of dasd volsers in device list"
#define showdvol1_cmd_help      \
                                \
  "Format:   showdvol1  [ NO | YES | ONLY ]\n\n"                                      \
                                                                                      \
  "Indicates whether to show the dasd VOL1 labels (volser) in the device list\n"      \
  "display. 'YES' shows the volser in addition to the usual filename, whereas\n"      \
  "'NO' shows the device list in a traditional filename only format. The 'ONLY'\n"    \
  "option shows only the volser; the filename is not shown at all. The default\n"     \
  "is 'NO', which results in a traditional device list display. Enter the command\n"  \
  "without any operands to echo the current settings to the console.\n"

#define shcmdopt_cmd_desc       "Set diag8 sh option"
#define shrd_cmd_desc           "shrd command"
#define shrdport_cmd_desc       "Set shrdport value"
#define sizeof_cmd_desc         "Display size of structures"
#define spm_cmd_desc            "SIE performance monitor"
#define srvprio_cmd_desc        "Set/Display srvprio parameter"
#define ssd_cmd_desc            "Signal shutdown"
#define ssd_cmd_help            \
                                \
  "The SSD (signal shutdown) command signals an imminent hypervisor\n"           \
  "shutdown to the guest.  Guests who support this are supposed to\n"            \
  "perform a shutdown upon receiving this request.\n"                            \
  "An implicit ssd command is given on a hercules \"quit\" command\n"            \
  "if the guest supports ssd.  In that case hercules shutdown will\n"            \
  "be delayed until the guest has shutdown or a 2nd quit command is\n"           \
  "given. \"ssd now\" will signal the guest immediately, without\n"              \
  "asking for confirmation.\n"

#define start_cmd_desc          "Start CPU (or printer/punch device if argument given)"
#define start_cmd_help          \
                                \
  "Entering the 'start' command by itself simply starts a stopped\n"             \
  "CPU, whereas 'start <devn>' presses the virtual start button on\n"            \
  "printer/punch device <devn>.\n"

#define startall_cmd_desc       "Start all CPU's"
#define stop_cmd_desc           "Stop CPU (or printer/punch device if argument given)"
#define stop_cmd_help           \
                                \
  "Entering the 'stop' command by itself simply stops a running\n"               \
  "CPU, whereas 'stop <devn>' presses the virtual stop button on\n"              \
  "printer/punch device <devn>, usually causing an INTREQ.\n"

#define stopall_cmd_desc        "Stop all CPU's"
#define store_cmd_desc          "Store CPU status at absolute zero"
#define suspend_cmd_desc        "Suspend hercules"
#define symptom_cmd_desc        "Alias for traceopt"
#define syncio_cmd_desc         "Display syncio devices statistics"
#define sysclear_cmd_desc       "System Clear Reset manual operation"
#define sysclear_cmd_help       \
                                \
  "Performs the System Reset Clear manual control function. Same as\n"           \
  "the \"sysreset clear\" command. Clears main storage to 0, and all\n"          \
  "registers, control registers, etc.. are reset to their initial value.\n"      \
  "At this point, the system is essentially in the same state as it was\n"       \
  "when it was first started.\n"

#define sysepoch_cmd_desc       "Set sysepoch parameter"
#define sysreset_cmd_desc       "System Reset manual operation"
#define sysreset_cmd_help       \
                                \
  "Performs the System Reset manual control function. Without any arguments\n"   \
  "or with the \"normal\" argument then only a CPU and I/O subsystem reset\n"    \
  "are performed. When the \"clear\" argument is given then this command is\n"   \
  "identical in functionality to the \"sysclear\" command.\n"

#define tminus_cmd_desc         "Turn off instruction tracing"
#define t_cmd_desc              "Instruction trace"
#define t_cmd_help              \
                                \
  "Format: \"t addr-addr\" or \"t addr:addr\" or \"t addr.length\"\n"            \
  "sets the instruction tracing range (which is totally separate from\n"         \
  "the instruction stepping and breaking range).\n"                              \
  "With or without a range, the t command displays whether instruction\n"        \
  "tracing is on or off and the range if any.\n"                                 \
  "The t command by itself does not activate instruction tracing.\n"             \
  "Use the t+ command to activate instruction tracing.\n"                        \
  "\"t 0\" eliminates the range (all addresses will be traced).\n"

#define tquest_cmd_desc         "Instruction trace query"
#define tquest_cmd_help         \
                                \
  "Format: \"t?\" displays whether instruction tracing is on or off\n"           \
  "and the range if any.\n"

#define tckd_cmd_desc           "Turn CKD_KEY tracing on/off"
#define tdev_cmd_desc           "Turn CCW tracing on/off"
#define tplus_cmd_desc          "Instruction trace on"
#define tplus_cmd_help          \
                                \
  "Format: \"t+\" turns on instruction tracing. A range can be specified\n"      \
  "as for the \"t\" command, otherwise the existing range is used. If there\n"   \
  "is no range (or range was specified as 0) then all instructions will\n"       \
  "be traced.\n"

#define timerint_cmd_desc       "Display or set timers update interval"
#define timerint_cmd_help       \
                                \
  "Specifies the internal timers update interval, in microseconds.\n"            \
  "This parameter specifies how frequently Hercules's internal\n"                \
  "timers-update thread updates the TOD Clock, CPU Timer, and other\n"           \
  "architectural related clock/timer values. The default interval\n"             \
  "is 50 microseconds, which strikes a reasonable balance between\n"             \
  "clock accuracy and overall host performance. The minimum allowed\n"           \
  "value is 1 microsecond and the maximum is 1000000 microseconds\n"             \
  "(i.e. one second).\n"

#define tlb_cmd_desc            "Display TLB tables"
#define toddrag_cmd_desc        "Display or set TOD clock drag factor"
#define todprio_cmd_desc        "Set/Display todprio parameter"
#define traceopt_cmd_desc       "Instruction trace display options"
#define traceopt_cmd_help       \
                                \
  "Format: \"traceopt [regsfirst | noregs | traditional]\". Determines how\n"    \
  "the registers are displayed during instruction tracing and stepping.\n"       \
  "Entering the command without any argument simply displays the current\n"      \
  "mode.\n"

#define tt32_cmd_desc           "Control/query CTCI-W32 functionality"
#define tt32_cmd_help           \
                                \
  "Format:  \"tt32   debug | nodebug | stats <devnum>\".\n"                      \
  "\n"                                                                           \
  "Enables or disables global CTCI-W32 debug tracing\n"                          \
  "or displays TunTap32 stats for the specified CTC device.\n"

#define tzoffset_cmd_desc       "Set tzoffset parameter"
#define u_cmd_desc              "Disassemble storage"
#define u_cmd_help              \
                                \
  "Format: \"u [R|V|P|H]addr[.len]\" or \"u [R|V|P|H]addr[-addr2]\" to\n"       \
  "disassemble storage beginning at address 'addr' for length 'len' or\n"       \
  "to address 'addr2'. The optional 'R', 'V', 'P' or 'H' address prefix\n"      \
  "forces Real, Virtual, Primary Space, or Home Space address translation\n"    \
  "mode instead of using the current PSW mode, which is the default.\n"

#define uptime_cmd_desc         "Display how long Hercules has been running"
#define v_cmd_desc              "Display or alter virtual storage"
#define v_cmd_help              \
                                \
  "Format: \"v [P|S|H]addr[.len]\" or \"v [P|S|H]addr[-addr2]\" to display\n"   \
  "virtual storage, or \"v [P|S|H]addr=value\" to alter virtual storage,\n"     \
  "where 'value' is a hex string of up to 32 pairs of digits. The optional\n"   \
  "'P', 'S' or 'H' address prefix character forces Primary Space, Secondary\n"  \
  "Space or Home Space address translation mode instead of current PSW mode.\n"

#define version_cmd_desc        "Display version information"
#define xpndsize_cmd_desc       "Define/Display xpndsize parameter"
#define xpndsize_cmd_help       \
                                \
  "Format: xpndsize [ mmmm | nnnS [ lOCK | unlOCK ] ]\n"                                \
  "        mmmm    - define expanded storage size mmmm Megabytes\n"                     \
  "\n"                                                                                  \
  "        nnnS    - define expanded storage size nnn S where S is the multiplier\n"    \
  "                  M = 2**20 (mega/mebi)\n"                                           \
  "                  G = 2**30 (giga/gibi)\n"                                           \
  "                  T = 2**40 (tera/tebi)\n"                                           \
  "\n"                                                                                  \
  "      (none)    - display current mainsize value\n"                                  \
  "\n"                                                                                  \
  "        lOCK    - attempt to lock storage (pages lock by host OS)\n"                 \
  "        unlOCK  - leave storage unlocked (pagable by host OS)\n"                     \
  "\n"                                                                                  \
  " Note: Multiplier 'T' is not available on 32bit machines\n"                          \
  "       Expanded storage is limited to 1G on 32bit machines\n"

#define yroffset_cmd_desc       "Set yroffset parameter"

#endif // defined( _FW_REF )        /* (pre-table build pass) */

/*-------------------------------------------------------------------*/
/*                      Standard commands                            */
/*-------------------------------------------------------------------*/

//MMAND( "sample",   "sample command",   sample_cmd,  SYSOPER,    "detailed long help text" )
//MMAND( "sample2",  "sample cmd two",   sample2_cmd, SYSCMD,      NULL )  // No long help provided
//MMAND( "sample3",   NULL,              sample3_cmd, SYSCMD,      NULL )  // No help provided at all
//DABBR( "common", 3, NULL,              sample4_cmd, SYSDEBUG,    NULL )  // Disabled command - generally debugging only
//MMAND( "sss",       "shadow file cmd", NULL,        SYSCMDNOPER, NULL )  // 'ShadowFile_cmd' (special handling)
//MMAND( "x{+/-}zz",  "flag on/off cmd", NULL,        SYSCMDNOPER, NULL )  // 'OnOffCommand'   (special handling)

//       "1...5...9",               function                type flags          description             long help
COMMAND( "$test",                   test_cmd,               SYSPROGDEVELDEBUG,  $test_cmd_desc,         $test_cmd_help      )
CMDABBR( "$zapcmd",        4,       zapcmd_cmd,             SYSPROGDEVELDEBUG,  $zapcmd_cmd_desc,       $zapcmd_cmd_help    )
CMDABBR( "$locate",        4,       locate_cmd,             SYSPROGDEVELDEBUG,  $locate_cmd_desc,       NULL                )

COMMAND( "cache",                   cache_cmd,              SYSCONFIG,          cache_cmd_desc,         cache_cmd_help      )
COMMAND( "cckd",                    cckd_cmd,               SYSCONFIG,          cckd_cmd_desc,          cckd_cmd_help       )
COMMAND( "devtmax",                 devtmax_cmd,            SYSCONFIG,          devtmax_cmd_desc,       devtmax_cmd_help    )
CMDABBR( "legacysenseid",  9,       legacysenseid_cmd,      SYSCONFIG,          legacy_cmd_desc,        NULL                )
COMMAND( "sclproot",                sclproot_cmd,           SYSCONFIG,          sclproot_cmd_desc,      sclproot_cmd_help   )

COMMAND( "#",                       comment_cmd,            SYSALL,             hash_cmd_desc,          NULL                )
COMMAND( "*",                       comment_cmd,            SYSALL,             star_cmd_desc,          NULL                )
COMMAND( "?",                       HelpCommand,            SYSALL,             quest_cmd_desc,         NULL                )
COMMAND( "cmdlevel",                CmdLevel,               SYSALL,             cmdlevel_cmd_desc,      cmdlevel_cmd_help   )
COMMAND( "cmdlvl",                  CmdLevel,               SYSALL,             cmdlvl_cmd_desc,        NULL                )
COMMAND( "cmdsep",                  cmdsep_cmd,             SYSALL,             cmdsep_cmd_desc,        cmdsep_cmd_help     )
COMMAND( "help",                    HelpCommand,            SYSALL,             help_cmd_desc,          help_cmd_help       )
COMMAND( "hst",                     History,                SYSALL,             hst_cmd_desc,           hst_cmd_help        )
CMDABBR( "message",  1,             msg_cmd,                SYSALL,             message_cmd_desc,       message_cmd_help    )
COMMAND( "msg",                     msg_cmd,                SYSALL,             msg_cmd_desc,           NULL                )
COMMAND( "msglevel",                msglevel_cmd,           SYSALL,             msglevel_cmd_desc,      msglevel_cmd_help   )
COMMAND( "msglvl",                  msglevel_cmd,           SYSALL,             msglvl_cmd_desc,        NULL                )
COMMAND( "msgnoh",                  msg_cmd,                SYSALL,             msgnoh_cmd_desc,        NULL                )
COMMAND( "uptime",                  uptime_cmd,             SYSALL,             uptime_cmd_desc,        NULL                )
COMMAND( "version",                 version_cmd,            SYSALL,             version_cmd_desc,       NULL                )

COMMAND( "attach",                  attach_cmd,             SYSCMD,             attach_cmd_desc,        attach_cmd_help     )
COMMAND( "cpu",                     cpu_cmd,                SYSCMD,             cpu_cmd_desc,           cpu_cmd_help        )
COMMAND( "define",                  define_cmd,             SYSCMD,             define_cmd_desc,        define_cmd_help     )
COMMAND( "detach",                  detach_cmd,             SYSCMD,             detach_cmd_desc,        NULL                )
COMMAND( "devinit",                 devinit_cmd,            SYSCMD,             devinit_cmd_desc,       devinit_cmd_help    )
COMMAND( "devlist",                 devlist_cmd,            SYSCMD,             devlist_cmd_desc,       devlist_cmd_help    )
COMMAND( "fcb",                     fcb_cmd,                SYSCMD,             fcb_cmd_desc,           fcb_cmd_help        )
COMMAND( "loadparm",                loadparm_cmd,           SYSCMD,             loadparm_cmd_desc,      loadparm_cmd_help   )
COMMAND( "log",                     log_cmd,                SYSCMD,             log_cmd_desc,           log_cmd_help        )
CMDABBR( "logopt",  6,              logopt_cmd,             SYSCMD,             logopt_cmd_desc,        logopt_cmd_help     )
COMMAND( "mt",                      mt_cmd,                 SYSCMD,             mt_cmd_desc,            mt_cmd_help         )
COMMAND( "pantitle",                pantitle_cmd,           SYSCMD,             pantitle_cmd_desc,      pantitle_cmd_help   )
CMDABBR( "qcpuid",  5,              qcpuid_cmd,             SYSCMD,             qcpuid_cmd_desc,        qcpuid_cmd_help     )
CMDABBR( "qpid",    4,              qpid_cmd,               SYSCMD,             qpid_cmd_desc,          NULL                )
CMDABBR( "qports",  5,              qports_cmd,             SYSCMD,             qports_cmd_desc,        NULL                )
CMDABBR( "qproc",   5,              qproc_cmd,              SYSCMD,             qproc_cmd_desc,         NULL                )
CMDABBR( "qstor",   5,              qstor_cmd,              SYSCMD,             qstor_cmd_desc,         NULL                )
COMMAND( "start",                   start_cmd,              SYSCMD,             start_cmd_desc,         start_cmd_help      )
COMMAND( "stop",                    stop_cmd,               SYSCMD,             stop_cmd_desc,          stop_cmd_help       )

COMMAND( "aea",                     aea_cmd,                SYSCMDNOPER,        aea_cmd_desc,           NULL                )
COMMAND( "aia",                     aia_cmd,                SYSCMDNOPER,        aia_cmd_desc,           NULL                )
COMMAND( "ar",                      ar_cmd,                 SYSCMDNOPER,        ar_cmd_desc,            NULL                )
COMMAND( "autoinit",                autoinit_cmd,           SYSCMDNOPER,        autoinit_cmd_desc,      autoinit_cmd_help   )
COMMAND( "automount",               automount_cmd,          SYSCMDNOPER,        automount_cmd_desc,     automount_cmd_help  )
COMMAND( "b-",                      trace_cmd,              SYSCMDNOPER,        bminus_cm_desc,         bminus_cm_help      )
COMMAND( "b",                       trace_cmd,              SYSCMDNOPER,        b_cmd_desc,             b_cmd_help          )
COMMAND( "b+",                      trace_cmd,              SYSCMDNOPER,        bplus_cmd_desc,         NULL                )
COMMAND( "cachestats",              EXTCMD(cachestats_cmd), SYSCMDNOPER,        cachestats_cmd_desc,    NULL                )
COMMAND( "clocks",                  clocks_cmd,             SYSCMDNOPER,        clocks_cmd_desc,        NULL                )
COMMAND( "codepage",                codepage_cmd,           SYSCMDNOPER,        codepage_cmd_desc,      codepage_cmd_help   )
COMMAND( "conkpalv",                conkpalv_cmd,           SYSCMDNOPER,        conkpalv_cmd_desc,      conkpalv_cmd_help   )
COMMAND( "cp_updt",                 cp_updt_cmd,            SYSCMDNOPER,        cp_updt_cmd_desc,       cp_updt_cmd_help    )
COMMAND( "cr",                      cr_cmd,                 SYSCMDNOPER,        cr_cmd_desc,            cr_cmd_help         )
COMMAND( "cscript",                 cscript_cmd,            SYSCMDNOPER,        cscript_cmd_desc,       cscript_cmd_help    )
COMMAND( "ctc",                     ctc_cmd,                SYSCMDNOPER,        ctc_cmd_desc,           ctc_cmd_help        )
COMMAND( "ds",                      ds_cmd,                 SYSCMDNOPER,        ds_cmd_desc,            NULL                )
COMMAND( "fpc",                     fpc_cmd,                SYSCMDNOPER,        fpc_cmd_desc,           fpc_cmd_help        )
COMMAND( "fpr",                     fpr_cmd,                SYSCMDNOPER,        fpr_cmd_desc,           fpr_cmd_help        )
COMMAND( "g",                       g_cmd,                  SYSCMDNOPER,        g_cmd_desc,             NULL                )
COMMAND( "gpr",                     gpr_cmd,                SYSCMDNOPER,        gpr_cmd_desc,           gpr_cmd_help        )
COMMAND( "herclogo",                herclogo_cmd,           SYSCMDNOPER,        herclogo_cmd_desc,      herclogo_cmd_help   )
COMMAND( "ipending",                ipending_cmd,           SYSCMDNOPER,        ipending_cmd_desc,      NULL                )
COMMAND( "k",                       k_cmd,                  SYSCMDNOPER,        k_cmd_desc,             NULL                )
COMMAND( "loadcore",                loadcore_cmd,           SYSCMDNOPER,        loadcore_cmd_desc,      loadcore_cmd_help   )
COMMAND( "loadtext",                loadtext_cmd,           SYSCMDNOPER,        loadtext_cmd_desc,      loadtext_cmd_help   )
COMMAND( "maxcpu",                  maxcpu_cmd,             SYSCMDNOPER,        maxcpu_cmd_desc,        NULL                )
CMDABBR( "mounted_tape_reinit",  9, mounted_tape_reinit_cmd,SYSCMDNOPER,        mtapeinit_cmd_desc,     mtapeinit_cmd_help  )
COMMAND( "numcpu",                  numcpu_cmd,             SYSCMDNOPER,        numcpu_cmd_desc,        NULL                )
COMMAND( "numvec",                  numvec_cmd,             SYSCMDNOPER,        numvec_cmd_desc,        NULL                )
COMMAND( "ostailor",                ostailor_cmd,           SYSCMDNOPER,        ostailor_cmd_desc,      ostailor_cmd_help   )
COMMAND( "pgmtrace",                pgmtrace_cmd,           SYSCMDNOPER,        pgmtrace_cmd_desc,      pgmtrace_cmd_help   )
COMMAND( "pr",                      pr_cmd,                 SYSCMDNOPER,        pr_cmd_desc,            NULL                )
COMMAND( "psw",                     psw_cmd,                SYSCMDNOPER,        psw_cmd_desc,           psw_cmd_help        )
COMMAND( "ptp",                     ptp_cmd,                SYSCMDNOPER,        ptp_cmd_desc,           ptp_cmd_help        )
COMMAND( "ptt",                     EXTCMD( ptt_cmd ),      SYSCMDNOPER,        ptt_cmd_desc,           ptt_cmd_help        )
COMMAND( "qd",                      qd_cmd,                 SYSCMDNOPER,        qd_cmd_desc,            NULL                )
COMMAND( "quiet",                   quiet_cmd,              SYSCMDNOPER,        quiet_cmd_desc,         quiet_cmd_help      )
COMMAND( "r",                       r_cmd,                  SYSCMDNOPER,        r_cmd_desc,             r_cmd_help          )
COMMAND( "resume",                  resume_cmd,             SYSCMDNOPER,        resume_cmd_desc,        NULL                )
COMMAND( "s-",                      trace_cmd,              SYSCMDNOPER,        sminus_cmd_desc,        NULL                )
COMMAND( "s",                       trace_cmd,              SYSCMDNOPER,        s_cmd_desc,             s_cmd_help          )
COMMAND( "s?",                      trace_cmd,              SYSCMDNOPER,        squest_cmd_desc,        squest_cmd_help     )
COMMAND( "s+",                      trace_cmd,              SYSCMDNOPER,        splus_cmd_desc,         splus_cmd_help      )
COMMAND( "savecore",                savecore_cmd,           SYSCMDNOPER,        savecore_cmd_desc,      savecore_cmd_help   )
COMMAND( "script",                  script_cmd,             SYSCMDNOPER,        script_cmd_desc,        script_cmd_help     )
COMMAND( "sh",                      sh_cmd,                 SYSCMDNOPER,        sh_cmd_desc,            sh_cmd_help         )
COMMAND( "shrd",                    EXTCMD(shared_cmd),     SYSCMDNOPER,        shrd_cmd_desc,          NULL                )
COMMAND( "suspend",                 suspend_cmd,            SYSCMDNOPER,        suspend_cmd_desc,       NULL                )
COMMAND( "symptom",                 traceopt_cmd,           SYSCMDNOPER,        symptom_cmd_desc,       NULL                )
COMMAND( "t-",                      trace_cmd,              SYSCMDNOPER,        tminus_cmd_desc,        NULL                )
COMMAND( "t",                       trace_cmd,              SYSCMDNOPER,        t_cmd_desc,             t_cmd_help          )
COMMAND( "t?",                      trace_cmd,              SYSCMDNOPER,        tquest_cmd_desc,        tquest_cmd_help     )
COMMAND( "t+",                      trace_cmd,              SYSCMDNOPER,        tplus_cmd_desc,         tplus_cmd_help      )
COMMAND( "timerint",                timerint_cmd,           SYSCMDNOPER,        timerint_cmd_desc,      timerint_cmd_help   )
COMMAND( "tlb",                     tlb_cmd,                SYSCMDNOPER,        tlb_cmd_desc,           NULL                )
COMMAND( "toddrag",                 toddrag_cmd,            SYSCMDNOPER,        toddrag_cmd_desc,       NULL                )
COMMAND( "traceopt",                traceopt_cmd,           SYSCMDNOPER,        traceopt_cmd_desc,      traceopt_cmd_help   )
COMMAND( "u",                       u_cmd,                  SYSCMDNOPER,        u_cmd_desc,             u_cmd_help          )
COMMAND( "v",                       v_cmd,                  SYSCMDNOPER,        v_cmd_desc,             v_cmd_help          )

#if 0 // Changing directory on the fly will invalidate all relative filenames in the configuration that have not yet been opened
COMMAND( "cd",                      cd_cmd,                 SYSCMDNDIAG8,       cd_cmd_desc,            NULL                )
#endif
COMMAND( "i",                       i_cmd,                  SYSCMDNDIAG8,       i_cmd_desc,             NULL                )
COMMAND( "ipl",                     ipl_cmd,                SYSCMDNDIAG8,       ipl_cmd_desc,           ipl_cmd_help        )
COMMAND( "iplc",                    iplc_cmd,               SYSCMDNDIAG8,       iplc_cmd_desc,          iplc_cmd_help       )
#if 0
COMMAND( "pwd",                     pwd_cmd,                SYSCMDNDIAG8,       pwd_cmd_desc,           NULL                )
#endif
COMMAND( "restart",                 restart_cmd,            SYSCMDNDIAG8,       restart_cmd_desc,       NULL                )
COMMAND( "ext",                     ext_cmd,                SYSCMDNDIAG8,       ext_cmd_desc,           NULL                )
COMMAND( "startall",                startall_cmd,           SYSCMDNDIAG8,       startall_cmd_desc,      NULL                )
COMMAND( "stopall",                 stopall_cmd,            SYSCMDNDIAG8,       stopall_cmd_desc,       NULL                )
COMMAND( "store",                   store_cmd,              SYSCMDNDIAG8,       store_cmd_desc,         NULL                )
COMMAND( "sysclear",                sysclear_cmd,           SYSCMDNDIAG8,       sysclear_cmd_desc,      sysclear_cmd_help   )
COMMAND( "sysreset",                sysreset_cmd,           SYSCMDNDIAG8,       sysreset_cmd_desc,      sysreset_cmd_help   )

COMMAND( "capping",                 capping_cmd,            SYSCFGNDIAG8,       capping_cmd_desc,       capping_cmd_help    )
COMMAND( "cnslport",                cnslport_cmd,           SYSCFGNDIAG8,       cnslport_cmd_desc,      NULL                )
COMMAND( "cpuidfmt",                cpuidfmt_cmd,           SYSCFGNDIAG8,       cpuidfmt_cmd_desc,      NULL                )
COMMAND( "cpumodel",                cpumodel_cmd,           SYSCFGNDIAG8,       cpumodel_cmd_desc,      NULL                )
COMMAND( "cpuprio",                 cpuprio_cmd,            SYSCFGNDIAG8,       cpuprio_cmd_desc,       NULL                )
COMMAND( "cpuserial",               cpuserial_cmd,          SYSCFGNDIAG8,       cpuserial_cmd_desc,     NULL                )
COMMAND( "cpuverid",                cpuverid_cmd,           SYSCFGNDIAG8,       cpuverid_cmd_desc,      NULL                )
CMDABBR( "defstore",  7,            defstore_cmd,           SYSCFGNDIAG8,       defstore_cmd_desc,      defstore_cmd_help   )
COMMAND( "devprio",                 devprio_cmd,            SYSCFGNDIAG8,       devprio_cmd_desc,       NULL                )
COMMAND( "diag8cmd",                diag8_cmd,              SYSCFGNDIAG8,       diag8_cmd_desc,         NULL                )
COMMAND( "engines",                 engines_cmd,            SYSCFGNDIAG8,       engines_cmd_desc,       NULL                )
COMMAND( "hercprio",                hercprio_cmd,           SYSCFGNDIAG8,       hercprio_cmd_desc,      NULL                )
COMMAND( "lparname",                lparname_cmd,           SYSCFGNDIAG8,       lparname_cmd_desc,      lparname_cmd_help   )
COMMAND( "lparnum",                 lparnum_cmd,            SYSCFGNDIAG8,       lparnum_cmd_desc,       lparnum_cmd_help    )
COMMAND( "mainsize",                mainsize_cmd,           SYSCFGNDIAG8,       mainsize_cmd_desc,      mainsize_cmd_help   )
CMDABBR( "manufacturer",  8,        stsi_manufacturer_cmd,  SYSCFGNDIAG8,       manuf_cmd_desc,         NULL                )
COMMAND( "model",                   stsi_model_cmd,         SYSCFGNDIAG8,       model_cmd_desc,         model_cmd_help      )
COMMAND( "plant",                   stsi_plant_cmd,         SYSCFGNDIAG8,       plant_cmd_desc,         NULL                )
COMMAND( "shcmdopt",                shcmdopt_cmd,           SYSCFGNDIAG8,       shcmdopt_cmd_desc,      NULL                )
COMMAND( "srvprio",                 srvprio_cmd,            SYSCFGNDIAG8,       srvprio_cmd_desc,       NULL                )
COMMAND( "sysepoch",                sysepoch_cmd,           SYSCFGNDIAG8,       sysepoch_cmd_desc,      NULL                )
COMMAND( "todprio",                 todprio_cmd,            SYSCFGNDIAG8,       todprio_cmd_desc,       NULL                )
COMMAND( "tzoffset",                tzoffset_cmd,           SYSCFGNDIAG8,       tzoffset_cmd_desc,      NULL                )
COMMAND( "xpndsize",                xpndsize_cmd,           SYSCFGNDIAG8,       xpndsize_cmd_desc,      xpndsize_cmd_help   )
COMMAND( "yroffset",                yroffset_cmd,           SYSCFGNDIAG8,       yroffset_cmd_desc,      NULL                )

COMMAND( "archlvl",                 archlvl_cmd,            SYSCMDNOPERNDIAG8,  archlvl_cmd_desc,       archlvl_cmd_help    )
COMMAND( "archmode",                archlvl_cmd,            SYSCMDNOPERNDIAG8,  archmode_cmd_desc,      NULL                )

COMMAND( "exit",                    quit_cmd,               SYSALLNDIAG8,       exit_cmd_desc,          NULL                )

COMMAND( "sizeof",                  sizeof_cmd,             SYSCMDNOPERNPROG,   sizeof_cmd_desc,        NULL                )

COMMAND( "locks",                   EXTCMD( locks_cmd ),    SYSPROGDEVEL,       locks_cmd_desc,         locks_cmd_help      )

/*-------------------------------------------------------------------*/
/*             Commands optional by build option                     */
/*-------------------------------------------------------------------*/

//       "1...5...9",               function                type flags          description             long help
#if defined(_FEATURE_CMPSC_ENHANCEMENT_FACILITY)
COMMAND( "cmpscpad",                cmpscpad_cmd,           SYSCFGNDIAG8,       cmpscpad_cmd_desc,      cmpscpad_cmd_help   )
#endif /* defined(_FEATURE_CMPSC_ENHANCEMENT_FACILITY) */
#if defined( _FEATURE_ASN_AND_LX_REUSE )
COMMAND( "alrf",                    alrf_cmd,               SYSCMDNOPER,        alrf_cmd_desc,          NULL                )
COMMAND( "asn_and_lx_reuse",        alrf_cmd,               SYSCMDNOPER,        asnlx_cmd_desc,         NULL                )
#endif
#if defined( _FEATURE_CPU_RECONFIG )
COMMAND( "cf",                      cf_cmd,                 SYSCMDNDIAG8,       cf_cmd_desc,            cf_cmd_help         )
COMMAND( "cfall",                   cfall_cmd,              SYSCMDNDIAG8,       cfall_cmd_desc,         NULL                )
#else
COMMAND( "cf",                      cf_cmd,                 SYSCMDNDIAG8,       NULL,                   NULL                )
COMMAND( "cfall",                   cfall_cmd,              SYSCMDNDIAG8,       NULL,                   NULL                )
#endif
#if defined( FEATURE_ECPSVM )
COMMAND( "ecps:vm",                 ecpsvm_cmd,             SYSCMDNOPER,        ecps_cmd_desc,          ecps_cmd_help       )
COMMAND( "ecpsvm",                  ecpsvm_cmd,             SYSCMDNOPER,        ecpsvm_cmd_desc,        ecpsvm_cmd_help     )
COMMAND( "evm",                     ecpsvm_cmd,             SYSCMDNOPER,        evm_cmd_desc,           evm_cmd_help        )
#endif
#if defined( _FEATURE_SYSTEM_CONSOLE )
COMMAND( "!message",                g_cmd,                  SYSCMD,             bangmsg_cmd_desc,       bangmsg_cmd_help    )
COMMAND( ".reply",                  g_cmd,                  SYSCMD,             reply_cmd_desc,         reply_cmd_help      )
COMMAND( "scpecho",                 scpecho_cmd,            SYSCMD,             scpecho_cmd_desc,       scpecho_cmd_help    )
COMMAND( "scpimply",                scpimply_cmd,           SYSCMD,             scpimply_cmd_desc,      scpimply_cmd_help   )
COMMAND( "ssd",                     ssd_cmd,                SYSCMD,             ssd_cmd_desc,           ssd_cmd_help        )
#endif
#if defined( _FEATURE_SCSI_IPL )
COMMAND( "hwldr",                   hwldr_cmd,              SYSCMDNOPER,        hwldr_cmd_desc,         hwldr_cmd_help      )
COMMAND( "loaddev",                 lddev_cmd,              SYSCMD,             loaddev_cmd_desc,       loaddev_cmd_help    )
COMMAND( "dumpdev",                 lddev_cmd,              SYSCMD,             dumpdev_cmd_desc,       dumpdev_cmd_help    )
#endif
#if !defined( _FW_REF )
COMMAND( "f{+/-}adr",               NULL,                   SYSCMDNOPER,        f_cmd_desc,             NULL                )
COMMAND( "s{+/-}dev",               NULL,                   SYSCMDNOPER,        sdev_cmd_desc,          NULL                )
COMMAND( "sf-dev",                  NULL,                   SYSCMDNOPER,        sfminus_cmd_desc,       NULL                )
COMMAND( "sf+dev",                  NULL,                   SYSCMDNOPER,        sfplus_cmd_desc,        NULL                )
COMMAND( "sfc",                     NULL,                   SYSCMDNOPER,        sfc_cmd_desc,           NULL                )
COMMAND( "sfd",                     NULL,                   SYSCMDNOPER,        sfd_cmd_desc,           NULL                )
COMMAND( "sfk",                     NULL,                   SYSCMDNOPER,        sfk_cmd_desc,           sfk_cmd_help        )
COMMAND( "t{+/-}dev",               NULL,                   SYSCMDNOPER,        tdev_cmd_desc,          NULL                )
#if defined( OPTION_CKD_KEY_TRACING )
COMMAND( "t{+/-}CKD",               NULL,                   SYSCMDNOPER,        tckd_cmd_desc,          NULL )
#endif
#endif
#if defined(ENABLE_OBJECT_REXX) || defined(ENABLE_REGINA_REXX)
COMMAND( "rexx",                    rexx_cmd,               SYSCONFIG,          rexx_cmd_desc,          rexx_cmd_help       )
COMMAND( "exec",                    exec_cmd,               SYSCMD,             exec_cmd_desc,          exec_cmd_help       )
#endif /* defined(ENABLE_OBJECT_REXX) || defined(ENABLE_REGINA_REXX) */
#if 0
#if defined( _MSVC_ )
COMMAND( "dir",                     dir_cmd,                SYSCMDNDIAG8,       dir_cmd_desc,           NULL                )
#else
COMMAND( "ls",                      ls_cmd,                 SYSCMDNDIAG8,       ls_cmd_desc,            NULL                )
#endif
#endif
#if defined( OPTION_CMDTGT )
COMMAND( "cmdtgt",                  cmdtgt_cmd,             SYSCMD,             cmdtgt_cmd_desc,        cmdtgt_cmd_help     )
COMMAND( "herc",                    herc_cmd,               SYSCMD,             herc_cmd_desc,          herc_cmd_help       )
COMMAND( "pscp",                    prioscp_cmd,            SYSCMD,             pscp_cmd_desc,          pscp_cmd_help       )
COMMAND( "scp",                     scp_cmd,                SYSCMD,             scp_cmd_desc,           scp_cmd_help        )
#endif
#if defined( OPTION_CONFIG_SYMBOLS )
CMDABBR( "qpfkeys",  3,             qpfkeys_cmd,            SYSCMD,             qpfkeys_cmd_desc,       NULL                )
COMMAND( "defsym",                  defsym_cmd,             SYSCMDNOPER,        defsym_cmd_desc,        defsym_cmd_help     )
COMMAND( "delsym",                  delsym_cmd,             SYSCMDNOPER,        delsym_cmd_desc,        delsym_cmd_help     )
#endif
#if defined(HAVE_MLOCKALL)
COMMAND( "memlock",                 memlock_cmd,            SYSCONFIG,          NULL,                   NULL                )
COMMAND( "memfree",                 memfree_cmd,            SYSCONFIG,          NULL,                   NULL                )
#endif /*defined(HAVE_MLOCKALL)*/
#if defined( OPTION_COUNTING )
COMMAND( "count",                   count_cmd,              SYSCMDNOPER,        count_cmd_desc,         NULL                )
#endif
#if defined( OPTION_DYNAMIC_LOAD )
COMMAND( "modpath",                 modpath_cmd,            SYSCONFIG,          modpath_cmd_desc,       NULL                )
COMMAND( "ldmod",                   ldmod_cmd,              SYSCMDNOPER,        ldmod_cmd_desc,         ldmod_cmd_help      )
COMMAND( "lsdep",                   lsdep_cmd,              SYSCMDNOPER,        lsdep_cmd_desc,         NULL                )
COMMAND( "lsmod",                   lsmod_cmd,              SYSCMDNOPER,        lsmod_cmd_desc,         NULL                )
COMMAND( "rmmod",                   rmmod_cmd,              SYSCMDNOPER,        rmmod_cmd_desc,         NULL                )
#endif
#if defined( OPTION_HAO )
COMMAND( "hao",                     hao_cmd,                SYSPROGDEVEL,       hao_cmd_desc,           hao_cmd_help        )
#endif
#if defined( OPTION_HTTP_SERVER )
COMMAND( "http",                    http_cmd,               SYSCONFIG,          http_cmd_desc,          http_cmd_help       )
COMMAND( "httpport",                httpport_cmd,           SYSCONFIG,          httpport_cmd_desc,      httpport_cmd_help   )
COMMAND( "httproot",                httproot_cmd,           SYSCONFIG,          httproot_cmd_desc,      httproot_cmd_help   )
#endif
#if defined( OPTION_INSTRUCTION_COUNTING )
COMMAND( "icount",                  icount_cmd,             SYSCMDNOPER,        icount_cmd_desc,        NULL                )
#endif
#if defined( OPTION_IODELAY_KLUDGE )
COMMAND( "iodelay",                 iodelay_cmd,            SYSCMDNOPER,        iodelay_cmd_desc,       iodelay_cmd_help    )
#endif
#if defined( OPTION_LPP_RESTRICT )
COMMAND( "pgmprdos",                pgmprdos_cmd,           SYSCFGNDIAG8,       pgmprdos_cmd_desc,      pgmprdos_cmd_help   )
#else
COMMAND( "pgmprdos",                pgmprdos_cmd,           SYSCFGNDIAG8,       NULL,                   NULL                )
#endif
#if defined( OPTION_MIPS_COUNTING )
COMMAND( "maxrates",                maxrates_cmd,           SYSCMD,             maxrates_cmd_desc,      maxrates_cmd_help   )
#endif
#if defined( OPTION_MSGHLD )
COMMAND( "kd",                      msghld_cmd,             SYSCMD,             kd_cmd_desc,            NULL                )
COMMAND( "msghld",                  msghld_cmd,             SYSCMD,             msghld_cmd_desc,        msghld_cmd_help     )
#endif
#if defined( OPTION_SCSI_TAPE )
COMMAND( "auto_scsi_mount",         scsimount_cmd,          SYSCMDNOPER,        autoscsi_cmd_desc,      autoscsi_cmd_help   )
COMMAND( "scsimount",               scsimount_cmd,          SYSCMDNOPER,        scsimount_cmd_desc,     scsimount_cmd_help  )
#endif
#if defined( OPTION_SHARED_DEVICES )
COMMAND( "shrdport",                shrdport_cmd,           SYSCFGNDIAG8,       shrdport_cmd_desc,      NULL                )
#endif
#if defined ( OPTION_SHUTDOWN_CONFIRMATION )
COMMAND( "quit",                    quit_cmd,               SYSALLNDIAG8,       quit_cmd_desc,          quit_cmd_help       )
COMMAND( "quitmout",                quitmout_cmd,           SYSCMDNOPER,        quitmout_cmd_desc,      quitmout_cmd_help   )
#else
COMMAND( "quit",                    quit_cmd,               SYSALLNDIAG8,       quit_ssd_cmd_desc,      quit_ssd_cmd_help   )
#endif
#if defined( OPTION_W32_CTCI )
COMMAND( "tt32",                    tt32_cmd,               SYSCMDNOPER,        tt32_cmd_desc,          tt32_cmd_help       )
#endif
#if defined( PANEL_REFRESH_RATE )
COMMAND( "panrate",                 panrate_cmd,            SYSCMD,             panrate_cmd_desc,       panrate_cmd_help    )
#endif
#if defined( SIE_DEBUG_PERFMON )
COMMAND( "spm",                     spm_cmd,                SYSCMDNOPER,        spm_cmd_desc,           NULL                )
#endif
#if defined( OPTION_SHOWDVOL1 )
COMMAND( "showdvol1",               showdvol1_cmd,          SYSCMD,             showdvol1_cmd_desc,     showdvol1_cmd_help  )
#endif /* defined( OPTION_SHOWDVOL1 ) */
#ifdef OPTION_SYNCIO
COMMAND( "syncio",                  syncio_cmd,             SYSCMDNOPER,        syncio_cmd_desc,        NULL                )
#endif // OPTION_SYNCIO

/*------------------------------(EOF)--------------------------------*/

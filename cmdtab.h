/* CMDTAB.H     (c) Copyright Roger Bowler, 1999-2010                */
/*              (c) Copyright "Fish" (David B. Trout), 2002-2009     */
/*              (c) Copyright Jan Jaeger, 2003-2009                  */
/*              (c) Copyright TurboHercules, SAS 2010                */
/*              Defines all Hercules Configuration statements        */
/*              and panel commands                                   */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$


//          command    type       function      one-line description...
// COMMAND ("sample"   SYSOPER,   sample_cmd,   "short help text", "long help text" )
// COMMAND ("sample2"  SYSCMDALL, sample2_cmd,  "short help text", NULL )  // No long help provided
// COMMAND ("sample3"  SYSCMDALL, sample3_cmd,  NULL, NULL ) // No help provided at all
// COMMAND ("sample4"  SYSDEBUG,  sample4_cmd,  NULL, NULL ) // Disabled command - generally debugging only

COMMAND("help",      SYSALL,             HelpCommand,
  "list all commands / command specific help",
  "Format: \"help [cmd|c*]\".\n"
  "\n"
  "The command without any options will display a short description\n"
  "of all of the commands available matching the current cmdlevel. You\n"
  "may specify a partial command name followed by an '*' to get a\n"
  "list matching the partial command name. For example 'help msg*'\n"
  "will list all commands beginning with 'msg' and matching the current\n"
  "cmdlevel.\n"
  "\n"
  "This command with the 'cmd' option will display a long form of help\n"
  "information associated with that command if the command is available\n"
  "for the current cmdlevel.\n"
  "\n"
  "Help text may be limited to explaining the general format of the\n"
  "command and its various required or optional parameters and is not\n"
  "meant to replace the appropriate manual.\n")

COMMAND("?",         SYSALL,             HelpCommand,
  "alias for help",
    NULL)

COMMAND("cmdlevel",  SYSCONFIG,          CmdLevel,
  "Display/Set current command group",
    "display/set the current command group set(s)\n"
    "Format: cmdlevel [{+/-}{ALL, MAINT, PROGrammer, OPERator,\n"
    "                        and/or DEVELoper}]\n")

COMMAND("cmdlvl",    SYSCONFIG,          CmdLevel,
  "Alias for cmdlevel",
    NULL)

COMMAND("cmdsep",    SYSALL,             cmdsep_cmd,
  "Display/Set current command line separator",
    "display/set the current command line separator for multiple\n"
    "commands on a single line\n"
    "Format: cmdsep [c | off ]\n"
    "        c       a single character used for command separation\n"
    "                must not be '.', '!', or '-'\n"
    "                Although it maybe '#', this could have an affect\n"
    "                processing command lines that contain comments\n"
    "        off     disables command separation\n")

COMMAND("msglevel", SYSALL,             msglevel_cmd,
  "Display/Set current Message Display output",
  "Format: msglevel [on|off|text|time|debug|nodebug|verbose|terse]\n"
  "  on      Normal message display\n"
  "  off     No messages are displayed\n"
  "  text    Text portion only of message is display\n"
  "  time    Timestamp is prefixed to message\n"
  "  debug   Messages prefixed with source and linenumber\n"
  "  nodebug Turn off debug\n"
  "  verbose Display messages during configuration file processing\n"
  "  terse   Turn off verbose")

COMMAND("msglvl",    SYSALL,             msglevel_cmd,
  "Alias for msglevel", NULL)

COMMAND("*",         SYSALL,             comment_cmd,
  "Comment",
    NULL)

COMMAND("#",         SYSALL,             comment_cmd,
  "Comment",
    NULL)

CMDABBR("message",1, SYSALL,             msg_cmd,
  "Display message on console a la VM",
  "Format: \"message * text\". The 'text' field is variable in size.\n"
  "A 'VM' formatted similar to \"13:02:41  * MSG FROM HERCULES: hello\" is\n"
  "diplayed on the console panel as a result of the panel command\n"
  "'message * hello'.\n")

COMMAND("msg",       SYSALL,             msg_cmd,
  "Alias for message",
    NULL)

COMMAND("msgnoh",    SYSALL,             msg_cmd,
  "Similar to \"message\" but no header",
    NULL)

COMMAND("hst",       SYSALL,             History,
  "History of commands",
    "Format: \"hst | hst n | hst l\". Command \"hst l\" or \"hst 0\" displays\n"
    "list of last ten commands entered from command line\n"
    "hst n, where n is a positive number retrieves n-th command from list\n"
    "hst n, where n is a negative number retrieves n-th last command\n"
    "hst without an argument works exactly as hst -1, it retrieves the\n"
    "last command\n")

#if defined(OPTION_HAO)
COMMAND("hao",       SYSPROG+SYSDEVEL,   hao_cmd,
  "Hercules Automatic Operator",
    "Format: \"hao  tgt <tgt> | cmd <cmd> | list <n> | del <n> | clear \".\n"
    "  hao tgt <tgt> : define target rule (regex pattern) to react on\n"
    "  hao cmd <cmd> : define command for previously defined rule\n"
    "  hao list <n>  : list all rules/commands or only at index <n>\n"
    "  hao del <n>   : delete the rule at index <n>\n"
    "  hao clear     : delete all rules (stops automatic operator)\n")
#endif /* defined(OPTION_HAO) */

COMMAND("log",       SYSCMDALL,          log_cmd,
  "Direct logger output",
    "Format: \"log [ OFF | newfile ]\".   Sets log filename or stops\n"
    "log file output with the \"OFF\" option." )

CMDABBR("logopts",6, SYSCMDALL,          logopt_cmd,
  "Set/Display logging options",
    "Format: \"logopt [timestamp | notimestamp]\".   Sets logging options.\n"
    "\"timestamp\" inserts a time stamp in front of each log message.\n"
    "\"notimestamp\" displays log messages with no time stamps.  Entering\n"
    "the command with no arguments displays current logging options.\n"
    "\"timestamp\" and \"notimestamp\" may be abbreviated as \"time\"\n"
    "and \"notime\" respectively.\n")

COMMAND("uptime",    SYSALL,             uptime_cmd,
  "Display how long Hercules has been running",
    NULL)

COMMAND("version",   SYSALL,             version_cmd,
  "Display version information",
    NULL)

#if defined (OPTION_SHUTDOWN_CONFIRMATION)
COMMAND("quit",      SYSALL,             quit_cmd,
  "Terminate the emulator",
  "Format: \"quit [force]\"  Terminates the emulator. The \"quit\"\n"
    "                        command will first check that all online\n"
    "                        CPUs are stopped. If any CPU is not in a\n"
    "                        stopped state, a message is displayed\n"
    "                        indicating the number of CPUs running. A\n"
    "                        second message follows that prompts for\n"
    "                        confirmation by entering a second \"quit\"\n"
    "                        command to start termination of the\n"
    "                        emulator.\n"
    "              force     This option will terminate hercules\n"
    "                        immediately.\n")

COMMAND("quitmout",  SYSCMDALL-SYSOPER,  quitmout_cmd,
  "Set/Display the quit timeout value",
  "Format: \"quitmount [n]\" Sets or displays the elasped time that\n"
    "                        a confirming quit/ssd command must be\n"
    "                        entered.\n"
    "               n        This is a value between 2 and 60,\n"
    "                        representing seconds.\n")
#else
COMMAND("quit",      SYSALL,             quit_cmd,
  "Terminate the emulator",
  "Format: \"quit [force]\"  Terminates the emulator. If the guest OS\n"
    "                        has enabled Signal Shutdown, then a\n"
    "                        signal shutdown request is sent to the\n"
    "                        guest OS and termination will begin\n"
    "                        after guest OS has shutdown.\n"
    "              force     This option will terminate the emulator\n"
    "                        immediately.\n" )

#endif
COMMAND("exit",      SYSALL,             quit_cmd,
  "(Synonym for 'quit')",
    NULL)

COMMAND("cpu",       SYSCMDALL,          cpu_cmd,
  "Define target cpu for panel display and commands",
    "Format: \"cpu xx\" where 'xx' is the hexadecimal cpu address of the cpu\n"
    "in your multiprocessor configuration which you wish all panel commands\n"
    "to apply to. If command text follows the cpu address, the command will\n"
    "execute on cpu xx and the target cpu will not be permanently changed.\n"
    "For example, entering 'cpu 1F' followed by \"gpr\" will change the\n"
    "target cpu for the panel display and commands and then display the\n"
    "general purpose registers for cpu 31 of your configuration. Entering\n"
    "'cpu 14 gpr' will execute the 'gpr' command on cpu 20, but will not\n"
    "change the target cpu for subsequent panel displays and commands.\n")

COMMAND("start",     SYSCMDALL,          start_cmd,
  "Start CPU (or printer device if argument given)",
    "Entering the 'start' command by itself simply starts a stopped\n"
    "CPU, whereas 'start <devn>' presses the virtual start button on\n"
    "printer device <devn>.\n")

COMMAND("stop",      SYSCMDALL,          stop_cmd,
  "Stop CPU (or printer device if argument given)",
    "Entering the 'stop' command by itself simply stops a running\n"
    "CPU, whereas 'stop <devn>' presses the virtual stop button on\n"
    "printer device <devn>, usually causing an INTREQ.\n")

COMMAND("startall",  SYSCMDALL,          startall_cmd,
  "Start all CPU's",
    NULL)

COMMAND("stopall",   SYSCMDALL,          stopall_cmd,
  "Stop all CPU's",
    NULL)

#ifdef _FEATURE_CPU_RECONFIG
COMMAND("cf",        SYSCMDALL,          cf_cmd,
  "Configure current CPU online or offline",
    "Configure current CPU online or offline:  Format->  \"cf [on|off]\"\n"
    "Where the 'current' CPU is defined as whatever CPU was defined as\n"
    "the panel command target cpu via the \"cpu\" panel command. (Refer\n"
    "to the 'cpu' command for further information) Entering 'cf' by itself\n"
    "simply displays the current online/offline status of the current cpu.\n"
    "Otherwise the current cpu is configured online or offline as\n"
    "specified.\n"
    "Use 'cfall' to configure/display all CPUs online/offline state.\n")

COMMAND("cfall",     SYSCMDALL,          cfall_cmd,
  "Configure all CPU's online or offline",
     NULL)
#else
COMMAND("cf",        SYSCMDALL,          cf_cmd, NULL, NULL )
COMMAND("cfall",     SYSCMDALL,          cfall_cmd, NULL, NULL )
#endif

#ifdef _FEATURE_SYSTEM_CONSOLE
COMMAND("scpimply",   SYSCMDALL,          scpimply_cmd,
  "Set/Display option to pass non-hercules commands to the scp",
  "Format: \"scpimply [ on | off ]\"\n"
    "When scpimply is set ON, non-hercules commands are passed to\n"
    "the scp if the scp has enabled receipt of scp commands. The\n"
    "default is off. When scpimply is entered without any options,\n"
    "the current state is displayed.\n")

COMMAND("scpecho",   SYSCMDALL,          scpecho_cmd,
  "Set/Display option to echo to console and history of scp replys",
  "Format: \"scpecho [ on | off ]\"\n"
    "When scpecho is set ON, scp commands entered on the console are\n"
    "echoed to console and recording in command history.\n"
    "The default is off. When scpecho is entered without any options,\n"
    "the current state is displayed. This is to help manage passwords\n"
    "sent to the scp from being displayed and journaled.\n")

COMMAND(".reply",    SYSCMDALL,          g_cmd,
  "SCP command",
    "To reply to a system control program (i.e. guest operating system)\n"
    "message that gets issued to the hercules console, prefix the reply\n"
    "with a period.\n")

COMMAND("!message",  SYSCMDALL,          g_cmd,
  "SCP priority messsage",
    "To enter a system control program (i.e. guest operating system)\n"
    "priority command on the hercules console, simply prefix the command\n"
    "with an exclamation point '!'.\n")

COMMAND("ssd",       SYSCMDALL,          ssd_cmd,
  "Signal shutdown",
    "The SSD (signal shutdown) command signals an imminent hypervisor\n"
    "shutdown to the guest.  Guests who support this are supposed to\n"
    "perform a shutdown upon receiving this request.\n"
    "An implicit ssd command is given on a hercules \"quit\" command\n"
    "if the guest supports ssd.  In that case hercules shutdown will\n"
    "be delayed until the guest has shutdown or a 2nd quit command is\n"
    "given. \"ssd now\" will signal the guest immediately, without\n"
    "asking for confirmation.\n")
#endif

#ifdef OPTION_PTTRACE
COMMAND("ptt",       SYSCMDALL-SYSOPER, EXT_CMD(ptt_cmd),
  "Set or display internal trace",
    "Format: \"ptt [options] [nnn]\"\n"
    "When specified with no operands, the ptt command displays the trace options\n"
    "and the contents of the internal trace table.\n"
    "When specified with operands, the ptt command sets the trace options and/or\n"
    "specifies which events are to be traced. If the last operand is numeric, it\n"
    "sets the size of the trace table and activates the trace.\n"
     "options:\n"
     "  (no)control - control trace\n"
     "  (no)error   - error trace\n"
     "  (no)inter   - interlock failure trace\n"
     "  (no)io      - io trace\n"
     "  (no)lock    - lock trace buffer\n"
     "  (no)logger  - logger trace\n"
     "  (no)prog    - program interrupt trace\n"
     "  (no)sie     - sie trace\n"
     "  (no)signal  - signaling trace\n"
     "  (no)threads - thread trace\n"
     "  (no)timer   - timer trace\n"
     "  (no)tod     - timestamp trace entries\n"
     "  (no)wrap    - wrap trace buffer\n"
     "  to=nnn      - trace buffer display timeout\n"
     "  nnn         - trace buffer size\n")
#endif

COMMAND("i",         SYSCMDALL,          i_cmd,
  "Generate I/O attention interrupt for device",
    NULL)

COMMAND("ext",       SYSCMDALL,          ext_cmd,
  "Generate external interrupt",
    NULL)

COMMAND("restart",   SYSCMDALL,          restart_cmd,
  "Generate restart interrupt",
    NULL)

COMMAND("archlvl",   SYSCMDALL-SYSOPER,  archlvl_cmd,
  "Set Architecture Level",
    "Format: archlvl s/370|als0 | esa/390|als1 | esame|als2 | z/arch|als3\n"
    "                enable|disable <facility> [s/370|esa/390|z/arch]\n"
    "                query [<facility> | all]\n"
    "command without any argument simply displays the current architecture\n"
    "mode. Entering the command with an argument sets the architecture mode\n"
    "to the specified value.\n")

COMMAND("archmode",  SYSCMDALL-SYSOPER,  archlvl_cmd,
  "Alias for archlvl",
    NULL)

COMMAND("engines",   SYSCONFIG|SYSNDIAG, engines_cmd,
  "Set engines parameter",
    NULL)

COMMAND("sysepoch",  SYSCONFIG|SYSNDIAG, sysepoch_cmd,
  "Set sysepoch parameter",
    NULL)

COMMAND("tzoffset",  SYSCONFIG|SYSNDIAG, tzoffset_cmd,
  "Set tzoffset parameter",
    NULL)

COMMAND("yroffset",  SYSCONFIG|SYSNDIAG, yroffset_cmd,
  "Set yroffset parameter",
    NULL)

COMMAND("mainsize",  SYSCONFIG|SYSNDIAG, mainsize_cmd,
  "Set mainsize parameter",
    NULL)

COMMAND("xpndsize",  SYSCONFIG|SYSNDIAG, xpndsize_cmd,
  "Set xpndsize parameter",
    NULL)

COMMAND("hercprio",  SYSCONFIG|SYSNDIAG, hercprio_cmd,
  "Set/Display hercprio parameter",
    NULL)

COMMAND("cpuprio",   SYSCONFIG|SYSNDIAG, cpuprio_cmd,
  "Set/Display cpuprio parameter",
    NULL)

COMMAND("devprio",   SYSCONFIG|SYSNDIAG, devprio_cmd,
  "Set/Display devprio parameter",
    NULL)

COMMAND("todprio",   SYSCONFIG|SYSNDIAG, todprio_cmd,
  "Set/Display todprio parameter",
    NULL)

COMMAND("srvprio",   SYSCONFIG|SYSNDIAG, srvprio_cmd,
  "Set/Display srvprio parameter",
    NULL)

COMMAND("numvec",    SYSCMDALL-SYSOPER,  numvec_cmd,
  "Set numvec parameter",
    NULL)

COMMAND("numcpu",    SYSCMDALL-SYSOPER,  numcpu_cmd,
  "Set numcpu parameter",
    NULL)

COMMAND("maxcpu",    SYSCMDALL-SYSOPER,  maxcpu_cmd,
  "Set maxcpu parameter",
    NULL)

COMMAND("loadparm",  SYSCMDALL,          loadparm_cmd,
  "Set IPL parameter",
  "Specifies the eight-character IPL parameter which is used by\n"
  "some operating systems to select system parameters.")

COMMAND("lparname",  SYSCONFIG|SYSNDIAG, lparname_cmd,
  "Set LPAR name",
    "Specifies the eight-character LPAR name returned by\n"
    "DIAG X'204'. The default is HERCULES")

COMMAND("cpuverid",  SYSCONFIG|SYSNDIAG, cpuverid_cmd,
  "Set CPU verion number",
    NULL)

COMMAND("cpumodel",  SYSCONFIG|SYSNDIAG, cpumodel_cmd,
  "Set CPU model number",
    NULL)

COMMAND("cpuserial", SYSCONFIG|SYSNDIAG, cpuserial_cmd,
  "Set CPU serial number",
    NULL)

COMMAND("lparnum",   SYSCONFIG|SYSNDIAG, lparnum_cmd,
  "Set LPAR identification number",
  "Specifies the one- or two-digit hexadecimal LPAR identification\n"
  "number stored by the STIDP instruction. If a one-digit number\n"
  "is specified then STIDP stores a format-0 CPU ID. If a two-digit\n"
  "number is specified then STIDP stores a format-1 CPU ID. If\n"
  "LPARNUM is not specified, then STIDP stores a basic-mode CPUID")

COMMAND("cpuidfmt",  SYSCONFIG|SYSNDIAG, cpuidfmt_cmd,
  "Set format 0/1 STIDP generation",
    NULL)

COMMAND("cnslport",  SYSCONFIG|SYSNDIAG, cnslport_cmd,
  "Set console port",
    NULL)

#ifdef OPTION_CAPPING
COMMAND("capping",   SYSCONFIG|SYSNDIAG, capping_cmd,
  "Set capping value",
    NULL)
#endif // OPTION_CAPPING

#if defined(OPTION_SHARED_DEVICES)
COMMAND("shrdport",  SYSCONFIG|SYSNDIAG, shrdport_cmd,
  "Set shrdport value",
    NULL)
#endif /*defined(OPTION_SHARED_DEVICES)*/

#if defined(OPTION_SET_STSI_INFO)
COMMAND("model",     SYSCONFIG|SYSNDIAG, stsi_model_cmd,
  "Set/Query STSI model code",
  "\n"
  "Format:\n"
  "\n"
  "model [hardware [capacity [permanent [temporary]]]]\n"
  "\n"
  "where:\n"
  "\n"
  "<null>       specifies a query of the current model code settings.\n"
  "\n"
  "hardware     specifies the hardware model setting. Specifying an \"=\"\n"
  "             resets the hardware model to \"EMULATOR\"; specifying an\n"
  "             \"*\" leaves the current hardware model setting intact.\n"
  "             The default hardware model is \"EMULATOR\".\n"
  "\n"
  "capacity     specifies the capacity model setting. Specifying an \"=\"\n"
  "             copies the current hardware model; specifying an \"*\" \n"
  "             leaves the current capacity model setting intact. The\n"
  "             default capacity model is \"EMULATOR\".\n"
  "\n"
  "permanent    specifies the permanent model setting. Specifying an\n"
  "             \"=\" copies the current capacity model; specifying an\n"
  "             \"*\" leaves the current permanent model setting intact.\n"
  "             The default permanent model is \"\" (null string).\n"
  "\n"
  "temporary    specifies the temporary model setting. Specifying an\n"
  "             \"=\" copies the current permanent model; specifying an\n"
  "             \"*\" leaves the current temporary model setting intact.\n"
  "             The default temporary model is \"\" (null string).\n"
  "\n")

COMMAND("plant",           SYSCONFIG|SYSNDIAG,   stsi_plant_cmd,
  "Set STSI plant code",
    NULL)

CMDABBR("manufacturer",8,  SYSCONFIG|SYSNDIAG,   stsi_manufacturer_cmd,
  "Set STSI manufacturer code",
    NULL)
#endif /* defined(OPTION_SET_STSI_INFO) */

#if defined(OPTION_LPP_RESTRICT)
COMMAND("pgmprdos",  SYSCONFIG|SYSNDIAG, pgmprdos_cmd,
  "Set LPP license setting",
    NULL)
#endif /*defined(OPTION_LPP_RESTRICT)*/

COMMAND("codepage",  SYSCMDALL-SYSOPER,  codepage_cmd,
  "Set/display code page conversion table",
    "Format: 'codepage [cp]'\n"
    "        'codepage maint cmd [operands]' - see cp_updt command for\n"
    "                                          help\n"
    "If no operand is specified, the current codepage is displayed.\n"
    "If 'cp' is specified, then code page is set to the specified page\n"
    "if the page is valid.\n")

COMMAND("cp_updt",    SYSCMDALL-SYSOPER, cp_updt_cmd,
  "Create/Modify user character conversion table",
    "Format: 'cp_updt cmd [operands]'\n"
    "\n"
    "  altER e|g2h|a|h2g (pos,val,pos,val,...)\n"
    "                              - alter the user eBCDIC/g2h or aSCII/h2g\n"
    "                                table value at hex POSition to hex\n"
    "                                VALue 16 pairs of hex digits may be\n"
    "                                specified within the parens\n"
    "\n"
    "  dsp|disPLAY e|g2h|a|h2g     - display user eBCDIC|aSCII table\n"
    "\n"
    "  expORT e|g2h|a|h2g filename - export contents of user table to file\n"
    "\n"
    "  impORT e|g2h|a|h2g filename - import file contents into user table\n"
    "\n"
    "  refERENCE [cp]              - copy codepage to user tables\n"
    "                                if cp is not specified, a list of\n"
    "                                valid codepage tables is generated\n"
    "\n"
    "  reset                       - reset the internal user tables to\n"
    "                                binary zero\n"
    "\n"
    "  test                        - verify that user tables are\n"
    "                                transparent, i.e. the value at\n"
    "                                position n in g2h used as an index\n"
    "                                into h2g will return a value equal n\n"
    "                                ( g2h<=>h2g, h2g<=>g2h )\n"
    "\n"
    " *e|g2h represent ebcdic; a|h2g represent ascii\n"
    " **lower case part of the cmd name represent the minimum abbreviation\n"
    "   for the command\n"
    "\n"
    "To activate the user table, enter the command 'codepage user'\n"
    "\n"
    "Note: ebcdic refers to guest to host translation\n"
    "      ascii refers to host to guest translation\n"
    "      These terms are used for historical purposes and do not\n"
    "      represent the literal term.\n")

COMMAND("diag8cmd",  SYSCONFIG|SYSNDIAG, diag8_cmd,
  "Set diag8 command option",
    NULL)

COMMAND("shcmdopt",  SYSCONFIG|SYSNDIAG, shcmdopt_cmd,
  "Set diag8 sh option",
    NULL)

CMDABBR("legacysenseid",9,SYSCONFIG,          legacysenseid_cmd,
  "Set legacysenseid setting",
    NULL)

COMMAND("ipl",       SYSCMDALL,          ipl_cmd,
  "IPL Normal from device xxxx",
    "Format: \"ipl xxxx | cccc [loadparm xxxxnnnn | parm xxxxxxxxxxxxxx]\"\n"
    "\n"
    "Performs the Initial Program Load manual control function. If the\n"
    "first operand 'xxxx' is a 1- to 4-digit hexadecimal number, a\n"
    "CCW-type IPL is initiated from the indicated device number, and\n"
    "SCLP disk I/O is disabled. Otherwise a list-directed IPL is performed\n"
    "from the .ins file named 'cccc', and SCLP disk I/O is enabled for the\n"
    "directory path where the .ins file is located.\n"
    "\n"
    "An optional 'loadparm' keyword followed by a 1-8 character string can\n"
    "be used to set the LOADPARM prior to the IPL.\n"
    "\n"
    "An optional 'parm' keyword followed by a string can also be passed to\n"
    "the IPL command processor. The string will be loaded into the\n"
    "low-order 32 bits of the general purpose registers (4 characters per\n"
    "register for up to 64 bytes). The PARM option behaves similarly to\n"
    "the VM IPL command.\n")

COMMAND("iplc",      SYSCMDALL,          iplc_cmd,
  "IPL Clear from device xxxx",
    "Performs the Load Clear manual control function. See \"ipl\".\n")

COMMAND("sysreset",  SYSCMDALL,          sysreset_cmd,
  "SYSTEM Reset manual operation",
    "Performs the System Reset manual control function. A CPU and I/O\n"
    "subsystem reset are performed.\n")

COMMAND("sysclear",  SYSCMDALL,          sysclear_cmd,
  "SYSTEM Clear Reset manual operation",
    "Performs the System Reset Clear manual control function. Same as\n"
    "the \"sysreset\" command but also clears main storage to 0. Also,\n"
    "registers control registers, etc.. are reset to their initial value.\n"
    "At this point, the system is essentially in the same state as it was\n"
    "just after having been started\n")

COMMAND("store",     SYSCMDALL,          store_cmd,
  "Store CPU status at absolute zero",
    NULL)

COMMAND("sclproot",  SYSCONFIG,          sclproot_cmd,
  "Set SCLP base directory",
    "Format: \"sclproot [path|NONE]\"\n"
    "Enables SCLP disk I/O for the specified directory path, or disables\n"
    "SCLP disk I/O if NONE is specified. A subsequent list-directed IPL\n"
    "resets the path to the location of the .ins file, and a CCW-type IPL\n"
    "disables SCLP disk I/O. If no operand is specified, sclproot displays\n"
    "the current setting.\n")

#if defined(OPTION_HTTP_SERVER)
COMMAND("httproot",  SYSCONFIG,          httproot_cmd,
  "Command deprecated - Use \"HTTP ROOT fn\"",
    "This command is deprecated. Use \"http root fn\" instead.\n")

COMMAND("httpport",  SYSCONFIG,          httpport_cmd,
  "Command deprecated - Use \"HTTP PORT ...\"",
    "This command is deprecated. Use \"http port ...\" instead.\n")

COMMAND("http",      SYSCONFIG,          http_cmd,
  "Start/Stop/Modify/Display HTTP Server",
  "Format: 'http [start|stop|port nnnn [[noauth]|[auth user pass]]|root path]'\n"
  "\n"
  "start                                 - starts HTTP server if stopped\n"
  "stop                                  - stops HTTP server if started\n"
  "port nnnn [[noauth]|[auth user pass]] - set port and optional authorization.\n"
  "                                        Default is noauthorization needed.\n"
  "                                        'auth' requires a user and password\n"
  "\n"
  "root path                             - set the root file path name\n"
  "\n"
  "<none>                                - display status of HTTP server\n")

#endif /*defined(OPTION_HTTP_SERVER)*/

COMMAND("psw",       SYSCMDALL-SYSOPER,  psw_cmd,
  "Display or alter program status word",
    "Format: \"psw [operand ...]\" where 'operand ...' is one or more\n"
    "optional parameters which modify the contents of the Program Status\n"
    "Word:\n\n"

    "  am=24|31|64           addressing mode\n"
    "  as=ar|home|pri|sec    address-space\n"
    "  cc=n                  condition code       (decimal 0 to 3)\n"
    "  cmwp=x                C/M/W/P bits         (one hex digit)\n"
    "  ia=xxx                instruction address  (1 to 16 hex digits)\n"
    "  pk=n                  protection key       (decimal 0 to 15)\n"
    "  pm=x                  program mask         (one hex digit)\n"
    "  sm=xx                 system mask          (2 hex digits)\n"
    "\n"
    "Enter \"psw\" by itself to display the current PSW without altering it.\n")

COMMAND("gpr",       SYSCMDALL-SYSOPER,  gpr_cmd,
  "Display or alter general purpose registers",
    "Format: \"gpr [nn=xxxxxxxxxxxxxxxx]\" where 'nn' is the optional\n"
    "register number (0 to 15) and 'xxxxxxxxxxxxxxxx' is the register\n"
    "value in hexadecimal (1-8 hex digits for 32-bit registers or 1-16 hex\n"
    "digits for 64-bit registers). Enter \"gpr\" by itself to display the\n"
    "register values without altering them.\n")

COMMAND("fpr",       SYSCMDALL-SYSOPER,  fpr_cmd,
  "Display floating point registers",
    NULL)

COMMAND("fpc",       SYSCMDALL-SYSOPER,  fpc_cmd,
  "Display floating point control register",
    NULL)

COMMAND("cr",        SYSCMDALL-SYSOPER,  cr_cmd,
  "Display or alter control registers",
    "Format: \"cr [nn=xxxxxxxxxxxxxxxx]\" where 'nn' is the optional\n"
    "control register number (0 to 15) and 'xxxxxxxxxxxxxxxx' is the\n"
    "control register value in hex (1-8 hex digits for 32-bit registers\n"
    "or 1-16 hex digits for 64-bit registers). Enter \"cr\" by itself to\n"
    "display the control registers without altering them.\n")

COMMAND("ar",        SYSCMDALL-SYSOPER,  ar_cmd,
  "Display access registers",
    NULL)

COMMAND("pr",        SYSCMDALL-SYSOPER,  pr_cmd,
  "Display prefix register",
    NULL)

COMMAND("timerint",  SYSCMDALL-SYSOPER,  timerint_cmd,
  "Display or set timers update interval",
  "Specifies the internal timers update interval, in microseconds.\n"
  "This parameter specifies how frequently Hercules's internal\n"
  "timers-update thread updates the TOD Clock, CPU Timer, and other\n"
  "architectural related clock/timer values. The default interval\n"
  "is 50 microseconds, which strikes a reasonable balance between\n"
  "clock accuracy and overall host performance. The minimum allowed\n"
  "value is 1 microsecond and the maximum is 1000000 microseconds\n"
  "(i.e. one second).\n")

COMMAND("clocks",    SYSCMDALL-SYSOPER,  clocks_cmd,
  "Display tod clkc and cpu timer",
    NULL)

COMMAND("ipending",  SYSCMDALL-SYSOPER,  ipending_cmd,
  "Display pending interrupts",
    NULL )

COMMAND("ds",        SYSCMDALL-SYSOPER,  ds_cmd,
  "Display subchannel",
    NULL)

COMMAND("r",         SYSCMDALL-SYSOPER,  r_cmd,
  "Display or alter real storage",
    "Format: \"r addr[.len]\" or \"r addr-addr\" to display real\n"
    "storage, or \"r addr=value\" to alter real storage, where 'value'\n"
    "is a hex string of up to 32 pairs of digits.\n")

COMMAND("v",         SYSCMDALL-SYSOPER,  v_cmd,
  "Display or alter virtual storage",
    "Format: \"v [P|S|H] addr[.len]\" or \"v [P|S|H] addr-addr\" to display\n"
    "virtual storage, or \"v [P|S|H] addr=value\" to alter virtual storage,\n"
    "where 'value' is a hex string of up to 32 pairs of digits. The\n"
    "optional 'P' or 'S' or 'H' will force Primary, Secondary, or Home\n"
    "translation instead of current PSW mode.\n")

COMMAND("u",         SYSCMDALL-SYSOPER,  u_cmd,
  "Disassemble storage",
    NULL)

COMMAND("devtmax",   SYSCONFIG,          devtmax_cmd,
  "Display or set max device threads",
    "Specifies the maximum number of device threads allowed.\n"
    "\n"
    "Specify -1 to cause 'one time only' temporary threads to be created\n"
    "to service each I/O request to a device. Once the I/O request is\n"
    "complete, the thread exits. Subsequent I/O to the same device will\n"
    "cause another worker thread to be created again.\n"
    "\n"
    "Specify 0 to cause an unlimited number of 'semi-permanent' threads\n"
    "to be created on an 'as-needed' basis. With this option, a thread\n"
    "is created to service an I/O request for a device if one doesn't\n"
    "already exist, but once the I/O is complete, the thread enters an\n"
    "idle state waiting for new work. If a new I/O request for the device\n"
    "arrives before the timeout period expires, the existing thread will\n"
    "be reused. The timeout value is currently hard coded at 5 minutes.\n"
    "Note that this option can cause one thread (or possibly more) to be\n"
    "created for each device defined in your configuration. Specifying 0\n"
    "means there is no limit to the number of threads that can be created.\n"
    "\n"
    "Specify a value from 1 to nnn  to set an upper limit to the number of\n"
    "threads that can be created to service any I/O request to any device.\n"
    "Like the 0 option, each thread, once done servicing an I/O request,\n"
    "enters an idle state. If a new request arrives before the timeout\n"
    "period expires, the thread is reused. If all threads are busy when a\n"
    "new I/O request arrives however, a new thread is created only if the\n"
    "specified maximum has not yet been reached. If the specified maximum\n"
    "number of threads has already been reached, then the I/O request is\n"
    "placed in a queue and will be serviced by the first available thread\n"
    "(i.e. by whichever thread becomes idle first). This option was created\n"
    "to address a threading issue (possibly related to the cygwin Pthreads\n"
    "implementation) on Windows systems.\n"
    "\n"
    "The default for Windows is 8. The default for all other systems is 0.\n")

COMMAND("k",         SYSCMDALL-SYSOPER,  k_cmd,
  "Display cckd internal trace",
    NULL)

COMMAND("attach",    SYSCMDALL,          attach_cmd,
  "Configure device",
    "Format: \"attach devn type [arg...]\n")

COMMAND("detach",    SYSCMDALL,          detach_cmd,
  "Remove device",
    NULL)

COMMAND("define",    SYSCMDALL,          define_cmd,
  "Rename device",
    "Format: \"define olddevn newdevn\"\n")

COMMAND("devinit",   SYSCMDALL,          devinit_cmd,
  "Reinitialize device",
    "Format: \"devinit devn [arg...]\"\n"
    "If no arguments are given then the same arguments are used\n"
    "as were used the last time the device was created/initialized.\n")

COMMAND("devlist",   SYSCMDALL,          devlist_cmd,
  "List device, device class, or all devices",
    "Format: \"devlist [devn | devc]\"\n"
    "    devn       is a single device address\n"
    "    devc       is a single device class. Device classes are CON,\n"
    "               CTCA, DASD, DSP, LINE, PCH, PRT, QETH, RDR, and TAPE.\n"
    "\n"
    "If no arguments are given then all devices will be listed.\n" )

COMMAND("qd",        SYSCMDALL-SYSOPER,  qd_cmd,
  "Query dasd",
    NULL )

COMMAND("fcb",       SYSCMDALL,          fcb_cmd,
  "Display the current FCB (if only the printer is given)",
   "Reset the fcb to the standard one \n"
   "Load a fcb image \n")

CMDABBR("qcpuid",5,  SYSCMDALL,          qcpuid_cmd,
  "Display cpuid",
  "Display cpuid and STSI results presented to the SCP\n")

#if        defined( OPTION_CONFIG_SYMBOLS )
CMDABBR("qpfkeys",3, SYSCMDALL,          qpfkeys_cmd,
  "Display pf keys",
  "Display the current PF Key settings\n")
#endif  // defined( OPTION_CONFIG_SYMBOLS )

CMDABBR("qpid",4,    SYSCMDALL,          qpid_cmd,
  "Display PID",
  "Display Process ID of Hercules\n")

CMDABBR("qports",5,  SYSCMDALL,          qports_cmd,
  "Display ports",
  "Display TCP/IP ports in use\n")

CMDABBR("qproc",5,   SYSCMDALL,          qproc_cmd,
  "Display processors",
  "Display processors type and utilization\n")

CMDABBR("qstor",5,   SYSCMDALL,          qstor_cmd,
  "Display storage",
  "Display main and expanded storage values\n")

CMDABBR("mounted_tape_reinit",9, SYSCMDALL-SYSOPER, mounted_tape_reinit_cmd,
  "Control tape initialization",
  "Format: \"mounted_tape_reinit [disallow|disable | allow|enable]\"\n"
  "Specifies whether reinitialization of tape drive devices\n"
  "(via the devinit command, in order to mount a new tape)\n"
  "should be allowed if there is already a tape mounted on\n"
  "the drive. The current value is displayed if no operand is\n"
  "specified\n"
  "Specifying ALLOW or ENABLE indicates new tapes may be\n"
  "mounted (via 'devinit nnnn new-tape-filename') irrespective\n"
  "of whether or not there is already a tape mounted on the drive.\n"
  "This is the default state.\n"
  "Specifying DISALLOW or DISABLE prevents new tapes from being\n"
  "mounted if one is already mounted. When DISALLOW or DISABLE has\n"
  "been specified and a tape is already mounted on the drive, it\n"
  "must first be unmounted (via the command 'devinit nnnn *') before\n"
  "the new tape can be mounted. Otherwise the devinit attempt to\n"
  "mount the new tape is rejected.\n")

COMMAND("autoinit",  SYSCMDALL-SYSOPER,  autoinit_cmd,
  "Display/Set automatic create empty tape file switch",
  "Format: \"autoinit [on|off]\"\n"
  "Default for autoinit is off.\n"
  "When autoinit is off, devinit will return a file not found error\n"
  "if the specified file is not found.\n"
  "When autoinit is on, devinit will initialize a blank, non-labeled\n"
  "tape if the requested file is not found. Tape will have two tapemarks\n"
  "and be positioned at the beginning of the tape.")

#if defined( OPTION_TAPE_AUTOMOUNT )
COMMAND("automount", SYSCMDALL-SYSOPER,  automount_cmd,
  "Display/Update allowable tape automount directories",
    "Format: \"automount  { add <dir> | del <dir> | list }\"\n"
    "\n"
    "Adds or deletes entries from the list of allowable/unallowable tape\n"
    "automount directories, or lists all currently defined list entries,\n"
    "if any.\n"
    "\n"
    "The format of the <dir> directory operand for add/del operations is\n"
    "identical to that as described in the documentation for the AUTOMOUNT\n"
    "configuration file statement (i.e. prefix with '+' or '-' as needed).\n"
    "\n"
    "The automount feature is appropriately enabled or disabled for all\n"
    "tape devices as needed depending on the updated empty/non-empty list\n"
    "state.\n")
#endif /* OPTION_TAPE_AUTOMOUNT */

#if defined( OPTION_SCSI_TAPE )
COMMAND("auto_scsi_mount", SYSCMDALL-SYSOPER, scsimount_cmd,
  "Command deprecated - Use \"SCSIMOUNT\"",
    "This command is deprecated. Use \"scsimount\" instead.\n")

COMMAND("scsimount",       SYSCMDALL-SYSOPER, scsimount_cmd,
  "Automatic SCSI tape mounts",
    "Format:    \"scsimount [ no | yes | 0-99 ]\".\n"
    "\n"
    "Displays or modifies the automatic SCSI tape mounts option.\n\n"
    "When entered without any operands, it displays the current interval\n"
    "and any pending tape mount requests. Entering 'no' (or 0 seconds)\n"
    "disables automount detection.\n"
    "\n"
    "Entering a value between 1-99 seconds (or 'yes') enables the option\n"
    "and specifies how often to query SCSI tape drives to automatically\n"
    "detect when a tape has been mounted (upon which an unsolicited\n"
    "device-attention interrupt will be presented to the guest operating\n"
    "system). 'yes' is equivalent to specifying a 5 second interval.\n")
#endif /* defined( OPTION_SCSI_TAPE ) */

COMMAND("mt",          SYSCMDALL,          mt_cmd,
  "Control magnetic tape operation",
  "Format:     \"mt device operation [ 1-9999 ]\".\n"
  "  Operations below can be used on a valid tape device. The device\n"
  "  must not have any I/O operation in process or pending.\n"
  "     operation   description\n"
  "       rew       rewind tape to the beginning\n"
  "       asf n     position tape at 'n' file  (default = 1)\n"
  "       fsf n     forward space 'n' files    (default = 1)\n"
  "       bsf n     backward space 'n' files   (default = 1)\n"
  "       fsr n     forward space 'n' records  (default = 1)\n"
  "       bsr n     backward space 'n' records (default = 1)\n"
  "       wtm n     write 'n' tapemarks        (default = 1)\n")

COMMAND("cd",        SYSCMDALL|SYSNDIAG, cd_cmd,
  "Change directory",
    NULL)

COMMAND("pwd",       SYSCMDALL|SYSNDIAG, pwd_cmd,
  "Print working directory",
    NULL)

#if defined( _MSVC_ )
COMMAND("dir",       SYSCMDALL|SYSNDIAG, dir_cmd,
  "Displays a list of files and subdirs in a directory",
    NULL)
#else
COMMAND("ls",        SYSCMDALL|SYSNDIAG, ls_cmd,
  "List directory contents",
    NULL)
#endif
COMMAND("sh",        (SYSCMDALL-SYSOPER)|SYSNDIAG,  sh_cmd,
  "Shell command",
    "Format: \"sh command [args...]\" where 'command' is any valid shell\n"
    "command. The entered command and any arguments are passed as-is to the\n"
    "shell for processing and the results are displayed on the console.\n")

#if defined(HAVE_REGINA_REXXSAA_H)
COMMAND("exec",      SYSCMDALL,          exec_cmd,
  "Execute a REXX script",
    "Format: \"exec rexx_exec [args...]\" where 'rexx_exec' is the name of\n"
    "the REXX script, and 'args' are arguments (separated by spaces) to be\n"
    "passed to the script.\n")
#endif /*defined(HAVE_REGINA_REXXSAA_H)*/

COMMAND("cache",       SYSCONFIG,           cache_cmd,
  "Execute cache related commands",
    "Format: \"cache [dasd system [on|off]]\"\n"
    "\n"
    "dasd system on|off         will enable(on) or disable(off) caching for\n"
    "                           all dasd devices\n"
    "dasd system                will present status of system dasd caching\n"
    "\n"
    "Command without arguments will present cache stats.\n")

COMMAND("cachestats",     SYSCMDALL-SYSOPER,  EXT_CMD(cachestats_cmd),
  "Cache stats command",
    NULL)

COMMAND("cckd",      SYSCONFIG,          cckd_cmd,
  "cckd command",
    "The cckd statement is used to display current cckd processing\n"
    "options and statistics, and to set new cckd options.\n"
    "Type \"cckd help\" for additional information.\n")

COMMAND("shrd",      SYSCMDALL-SYSOPER,  EXT_CMD(shared_cmd),
  "shrd command",
    NULL)

COMMAND("conkpalv",  SYSCMDALL-SYSOPER,  conkpalv_cmd,
  "Display/alter console TCP keep-alive settings",
    "Format: \"conkpalv (idle,intv,count)\" where 'idle', 'intv' and 'count'\n"
    "are the new values for the TCP keep-alive settings for console\n"
    "connections:\n"
    "- send probe when connection goes idle for 'idle' seconds\n"
    "- wait maximum of 'intv' seconds for a response to probe\n"
    "- disconnect after 'count' consecutive failed probes\n"
    "The format must be exactly as shown, with each value separated from\n"
    "the next by a single comma, no intervening spaces between them,\n"
    "surrounded by parenthesis. The command \"conkpalv\" without any operand\n"
    "displays the current values.\n")

COMMAND("quiet",     SYSCMDALL-SYSOPER,  quiet_cmd,
  "Toggle automatic refresh of panel display data",
    "'quiet' either disables automatic screen refreshing if it is\n"
    "currently enabled or enables it if it is currently disabled.\n"
    "When disabled you will no be able to see the response of any\n"
    "entered commands nor any messages issued by the system nor be\n"
    "able to scroll the display, etc. Basically all screen updating\n"
    "is disabled. Entering 'quiet' again re-enables screen updating.\n")

COMMAND("t",         SYSCMDALL-SYSOPER,  trace_cmd,
  "Instruction trace",
    "Format: \"t addr-addr\" or \"t addr:addr\" or \"t addr.length\"\n"
    "sets the instruction tracing range (which is totally separate from\n"
    "the instruction stepping and breaking range).\n"
    "With or without a range, the t command displays whether instruction\n"
    "tracing is on or off and the range if any.\n"
    "The t command by itself does not activate instruction tracing.\n"
    "Use the t+ command to activate instruction tracing.\n"
    "\"t 0\" eliminates the range (all addresses will be traced).\n")

COMMAND("t+",        SYSCMDALL-SYSOPER,  trace_cmd,
  "Instruction trace on",
    "Format: \"t+\" turns on instruction tracing. A range can be specified\n"
    "as for the \"t\" command, otherwise the existing range is used. If there\n"
    "is no range (or range was specified as 0) then all instructions will\n"
    "be traced.\n")

COMMAND("t-",        SYSCMDALL-SYSOPER,  trace_cmd,
  "Instruction trace off",
    "Format: \"t-\" turns off instruction tracing.\n")

COMMAND("t?",        SYSCMDALL-SYSOPER,  trace_cmd,
  "Instruction trace query",
    "Format: \"t?\" displays whether instruction tracing is on or off\n"
    "and the range if any.\n")

COMMAND("s",         SYSCMDALL-SYSOPER,  trace_cmd,
  "Instruction stepping",
    "Format: \"s addr-addr\" or \"s addr:addr\" or \"s addr.length\"\n"
    "sets the instruction stepping and instruction breaking range,\n"
    "(which is totally separate from the instruction tracing range).\n"
    "With or without a range, the s command displays whether instruction\n"
    "stepping is on or off and the range if any.\n"
    "The s command by itself does not activate instruction stepping.\n"
    "Use the s+ command to activate instruction stepping.\n"
    "\"s 0\" eliminates the range (all addresses will be stepped).\n")

COMMAND("s+",        SYSCMDALL-SYSOPER,  trace_cmd,
  "Instruction stepping on",
    "Format: \"s+\" turns on instruction stepping. A range can be specified\n"
    "as for the \"s\" command, otherwise the existing range is used. If\n"
    "there is no range (or range was specified as 0) then the range\n"
    "includes all addresses. When an instruction within the range is about\n"
    "to be executed, the CPU is temporarily stopped and the next instruction\n"
    "is displayed. You may then examine registers and/or storage, etc,\n"
    "before pressing 'Enter' to execute the instruction and stop at the next\n"
    "instruction. To turn off instruction stepping and continue execution,\n"
    "enter the \"g\" command.\n")

COMMAND("s-",        SYSCMDALL-SYSOPER,  trace_cmd,
  "Instruction stepping off",
    "Format: \"s-\" turns off instruction stepping.\n")

COMMAND("s?",        SYSCMDALL-SYSOPER,  trace_cmd,
  "Instruction stepping query",
    "Format: \"s?\" displays whether instruction stepping is on or off\n"
    "and the range if any.\n")

COMMAND("b",         SYSCMDALL-SYSOPER,  trace_cmd,
  "Set breakpoint",
    "Format: \"b addr\" or \"b addr-addr\" where 'addr' is the instruction\n"
    "address or range of addresses where you wish to halt execution. This\n"
    "command is synonymous with the \"s+\" command.\n")

COMMAND("b+",        SYSCMDALL-SYSOPER,  trace_cmd,
  "Set breakpoint",
    NULL)

COMMAND("b-",        SYSCMDALL-SYSOPER,  trace_cmd,
  "Delete breakpoint",
    "Format: \"b-\"  This command is the same as \"s-\"\n")

COMMAND("g",         SYSCMDALL-SYSOPER,  g_cmd,
  "Turn off instruction stepping and start all CPUs",
    NULL)

COMMAND("ostailor",  SYSCMDALL-SYSOPER,  ostailor_cmd,
  "Tailor trace information for specific OS",
  "Format: \"ostailor [quiet|os/390|z/os|vm|vse|linux|opensolaris|null]\".\n"
    "Specifies the intended operating system. The effect is to reduce\n"
    "control panel message traffic by selectively suppressing program\n"
    "check trace messages which are considered normal in the specified\n"
    "environment. The option 'quiet' suppresses all exception messages,\n"
    "whereas 'null' suppresses none of them. The other options suppress\n"
    "some messages and not others depending on the specified o/s. Prefix\n"
    "values with '+' to combine them with existing values or '-' to exclude\n"
    "them. SEE ALSO the 'pgmtrace' command which allows you to further fine\n"
    "tune the tracing of program interrupt exceptions.\n")

COMMAND("pgmtrace",  SYSCMDALL-SYSOPER,  pgmtrace_cmd,
  "Trace program interrupts",
    "Format: \"pgmtrace [-]intcode\" where 'intcode' is any valid program\n"
    "interruption code in the range 0x01 to 0x40. Precede the interrupt\n"
    "code with a '-' to stop tracing of that particular program\n"
    "interruption.\n")

COMMAND("savecore",  SYSCMDALL-SYSOPER,  savecore_cmd,
  "Save a core image to file",
    "Format: \"savecore filename [{start|*}] [{end|*}]\" where 'start' and\n"
    "'end' define the starting and ending addresss of the range of real\n"
    "storage to be saved to file 'filename'. An '*' for either the start\n"
    "address or end address (the default) means: \"the first/last byte of\n"
    "the first/last modified page as determined by the storage-key\n"
    "'changed' bit\".\n")

COMMAND("loadcore",  SYSCMDALL-SYSOPER,  loadcore_cmd,
  "Load a core image file",
    "Format: \"loadcore filename [address]\" where 'address' is the storage\n"
    "address of where to begin loading memory. The file 'filename' is\n"
    "presumed to be a pure binary image file previously created via the\n"
    "'savecore' command. The default for 'address' is 0 (beginning of\n"
    "storage).\n")

COMMAND("loadtext",  SYSCMDALL-SYSOPER,  loadtext_cmd,
  "Load a text deck file",
    "Format: \"loadtext filename [address]\". This command is essentially\n"
    "identical to the 'loadcore' command except that it loads a text deck\n"
    "file with \"TXT\" and \"END\" 80 byte records (i.e. an object deck).\n")

#if defined(OPTION_DYNAMIC_LOAD)
COMMAND("modpath",   SYSCONFIG,          modpath_cmd,
  "Set module load path",
    NULL)

COMMAND("ldmod",     SYSCMDALL-SYSOPER,  ldmod_cmd,
  "Load a module",
  "Format: \"ldmod module ...\"\n"
  "Specifies additional modules that are to be loaded by the\n"
  "Hercules dynamic loader.\n")

COMMAND("rmmod",     SYSCMDALL-SYSOPER,  rmmod_cmd,
  "Delete a module",
    NULL)

COMMAND("lsmod",     SYSCMDALL-SYSOPER,  lsmod_cmd,
  "List dynamic modules",
    NULL)

COMMAND("lsdep",     SYSCMDALL-SYSOPER,  lsdep_cmd,
  "List module dependencies",
    NULL)
#endif /*defined(OPTION_DYNAMIC_LOAD)*/

#ifdef OPTION_IODELAY_KLUDGE
COMMAND("iodelay",   SYSCMDALL-SYSOPER,  iodelay_cmd,
  "Display or set I/O delay value",
  "Format:  \"iodelay  n\".\n\n"
  "Specifies the amount of time (in microseconds) to wait after an\n"
  "I/O interrupt is ready to be set pending. This value can also be\n"
  "set using the Hercules console. The purpose of this parameter is\n"
  "to bypass a bug in the Linux/390 and zLinux dasd.c device driver.\n"
  "The problem is more apt to happen under Hercules than on a real\n"
  "machine because we may present an I/O interrupt sooner than a\n"
  "real machine.\n")
#endif

COMMAND("ctc",       SYSCMDALL-SYSOPER,  ctc_cmd,
  "Enable/Disable CTC debugging",
    "Format:  \"ctc  debug  { on | off }  [ <devnum> | ALL ]\".\n\n"
    "Enables/disables debug packet tracing for the specified CTCI/LCS\n"
    "device group(s) identified by <devnum> or for all CTCI/LCS device\n"
    "groups if <devnum> is not specified or specified as 'ALL'.\n")

#if defined(OPTION_W32_CTCI)
COMMAND("tt32",      SYSCMDALL-SYSOPER,  tt32_cmd,
  "Control/query CTCI-W32 functionality",
    "Format:  \"tt32   debug | nodebug | stats <devnum>\".\n"
    "\n"
    "Enables or disables global CTCI-W32 debug tracing\n"
    "or displays TunTap32 stats for the specified CTC device.\n")
#endif

COMMAND("toddrag",   SYSCMDALL-SYSOPER,  toddrag_cmd,
  "Display or set TOD clock drag factor",
    NULL)

#ifdef PANEL_REFRESH_RATE
COMMAND("panrate",   SYSCMDALL,          panrate_cmd,
  "Display or set rate at which console refreshes",
    "Format: \"panrate [nnn | fast | slow]\".\n"
    "Sets or displays the panel refresh rate.\n"
    "panrate nnn sets the refresh rate to nnn milliseconds.\n"
    "panrate fast sets the refresh rate to " MSTRING(PANEL_REFRESH_RATE_FAST) " milliseconds.\n"
    "panrate slow sets the refresh rate to " MSTRING(PANEL_REFRESH_RATE_SLOW) " milliseconds.\n"
    "If no operand is specified, panrate displays the current refresh rate.\n")
#endif

COMMAND("pantitle",        SYSCMDALL,          pantitle_cmd,
  "Display or set console title",
    "Format: pantitle [\"title string\"]\n"
    "        pantitle \"\"\n"
    "\n"
    "Sets or displays the optional console window title-bar\n"
    "string to be used in place of the default supplied by\n"
    "the windowing system. The value should be enclosed within\n"
    "double quotes if there are embedded blanks.\n"
    "\n"
    "An empty string (\"\") will remove the existing console title.\n"
    "\n"
    "The default console title will be a string consisting of\n"
    "LPARNAME - SYSTYPE * SYSNAME * SYSPLEX - System Status: color\n"
    "\n"
    "SYSTYPE, SYSNAME, and SYSPLEX are populated by the system call\n"
    "SCLP Control Program Identification. If anyone of the values is\n"
    "blank, then that field is not presented.\n"
    "\n"
    "System Status colors: GREEN  - is every thing working correctly\n"
    "                      YELLOW - one or more CPUs are not running\n"
    "                      RED    - one or more CPUs are in a disabled\n"
    "                               wait state\n"
    "\n")

#ifdef OPTION_MSGHLD
COMMAND("kd",        SYSCMDALL,          msghld_cmd,
  "Short form of 'msghld clear'",
    NULL)

COMMAND("msghld",    SYSCMDALL,          msghld_cmd,
  "Display or set the timeout of held messages",
    "Format: \"msghld [value | info | clear]\".\n"
    "value: timeout value of held message in seconds\n"
    "info:  displays the timeout value\n"
    "clear: releases the held messages\n")
#endif

COMMAND("syncio",    SYSCMDALL-SYSOPER,  syncio_cmd,
  "Display syncio devices statistics",
    NULL)

#if defined(OPTION_INSTRUCTION_COUNTING)
COMMAND("icount",    SYSCMDALL-SYSOPER,  icount_cmd,
  "Display individual instruction counts",
    NULL)
#endif

#ifdef OPTION_MIPS_COUNTING
COMMAND("maxrates",  SYSCMDALL,          maxrates_cmd,
  "Display max MIPS and IO/s rate or set reporting interval",
    "Format: \"maxrates [nnnn]\" where 'nnnn' is the desired reporting\n"
    "interval in minutes or 'midnight'. Acceptable values are from\n"
    "1 to 1440. The default is 1440 minutes (one day).\n"
    "The interval 'midnight' sets the interval to 1440 and aligns the\n"
    "start of the current interval to midnight.\n"
    "Entering \"maxrates\" by itself displays the current highest\n"
    "rates observed during the defined intervals.\n")
#endif // OPTION_MIPS_COUNTING

#if defined(_FEATURE_ASN_AND_LX_REUSE)
COMMAND("asn_and_lx_reuse",SYSCMDALL-SYSOPER,  alrf_cmd,
  "Command deprecated:"
  "Use \"archlvl enable|disable|query asn_lx_reuse\" instead",
    NULL)

COMMAND("alrf",      SYSCMDALL-SYSOPER,  alrf_cmd,
  "Command deprecated:"
  "Use \"archlvl enable|disable|query asn_lx_reuse\" instead",
    NULL)
#endif /* defined(_FEATURE_ASN_AND_LX_REUSE) */

#if defined(FISH_HANG)
COMMAND("FishHangReport", SYSCMDALL-SYSOPER,  FishHangReport_cmd,
  "Display thread/lock/event objects (DEBUG)",
    "When built with --enable-fthreads --enable-fishhang, a detailed record of\n"
    "every thread, lock and event that is created is maintained for debugging purposes.\n"
    "If a lock is accessed before it has been initialized or if a thread exits while\n"
    "still holding a lock, etc (including deadlock situations), the FishHang logic will\n"
    "detect and report it. If you suspect one of hercules's threads is hung waiting for\n"
    "a condition to be signalled for example, entering \"FishHangReport\" will display\n"
    "the internal list of thread, locks and events to possibly help you determine where\n"
    "it's hanging and what event (condition) it's hung on.\n")
#endif

#if defined(OPTION_CONFIG_SYMBOLS)
COMMAND("defsym",    SYSCMDALL-SYSOPER,  defsym_cmd,
  "Define symbol",
    "Format: \"defsym symbol [value]\". Defines symbol 'symbol' to contain\n"
    "value 'value'. The symbol can then be the object of a substitution for\n"
    "later panel commands. If 'value' contains blanks or spaces, then it\n"
    "must be enclosed within quotes or apostrophes. For more detailed\n"
    "information regarding symbol substitution refer to the 'DEFSYM'\n"
    "configuration file statement in Hercules documentation.\n"
    "Enter \"defsym\" by itself to display the values of all defined\n"
    "symbols.\n")

COMMAND("delsym",    SYSCMDALL-SYSOPER,  delsym_cmd,
  "Delete a symbol",
    "Format: \"delsym symbol\". Deletes symbol 'symbol'.\n")

#endif

COMMAND("script",    SYSCMDALL-SYSOPER,  script_cmd,
  "Run a sequence of panel commands contained in a file",
    "Format: \"script filename [...filename...]\". Sequentially executes\n"
    "the commands contained within the file 'filename'. The script file may\n"
    "also contain \"script\" commands, but the system ensures that no more\n"
    "than 10 levels of script are invoked at any one time.\n")

COMMAND("cscript",   SYSCMDALL-SYSOPER,  cscript_cmd,
  "Cancels a running script thread",
    "Format: \"cscript\". This command will cancel the currently running\n"
    "script. If no script is running, no action is taken.\n")

#if defined(FEATURE_ECPSVM)
COMMAND("ecps:vm",   SYSCMDALL-SYSOPER,  ecpsvm_cmd,
  "Command deprecated - Use \"ECPSVM\"",
    "This command is deprecated. Use \"ecpsvm\" instead.\n")

COMMAND("evm",       SYSCMDALL-SYSOPER,  ecpsvm_cmd,
  "Command deprecated - Use \"ECPSVM\"",
    "This command is deprecated. Use \"ecpsvm\" instead.\n")

COMMAND ( "ecpsvm",  SYSCMDALL-SYSOPER,  ecpsvm_cmd,
  "ECPS:VM Commands",
    "Format: \"ecpsvm\". This command invokes ECPS:VM Subcommands.\n"
    "Type \"ecpsvm help\" to see a list of available commands\n")
#endif

COMMAND("aea",       SYSCMDALL-SYSOPER,  aea_cmd,
  "Display AEA tables",
    NULL)

COMMAND("aia",       SYSCMDALL-SYSOPER,  aia_cmd,
  "Display AIA fields",
    NULL)
COMMAND("tlb",       SYSCMDALL-SYSOPER,  tlb_cmd,
  "Display TLB tables",
    NULL)

#if defined(SIE_DEBUG_PERFMON)
COMMAND("spm",       SYSCMDALL-SYSOPER,  spm_cmd,
  "SIE performance monitor",
    NULL)
#endif
#if defined(OPTION_COUNTING)
COMMAND("count",     SYSCMDALL-SYSOPER,  count_cmd,
  "Display/clear overall instruction count",
    NULL)
#endif
COMMAND("sizeof",    SYSCMDALL-SYSOPER-SYSPROG, sizeof_cmd,
  "Display size of structures",
    NULL)

COMMAND("suspend",   SYSCMDALL-SYSOPER,      suspend_cmd,
        "Suspend hercules", NULL )

COMMAND("resume",    SYSCMDALL-SYSOPER,      resume_cmd,
        "Resume hercules", NULL )

COMMAND("herclogo",  SYSCMDALL-SYSOPER,      herclogo_cmd,
  "Read a new hercules logo file",
    "Format: \"herclogo [<filename>]\". Load a new logo file for 3270\n"
    "terminal sessions. If no filename is specified, the built-in logo\n"
    "is used instead.\n"    )

COMMAND("traceopt",  SYSCMDALL-SYSOPER,  traceopt_cmd,
  "Instruction trace display options",
    "Format: \"traceopt [regsfirst | noregs | traditional]\". Determines how\n"
    "the registers are displayed during instruction tracing and stepping.\n"
    "Entering the command without any argument simply displays the current\n"
    "mode.\n" )

COMMAND("symptom",   SYSCMDALL-SYSOPER,  traceopt_cmd,
  "Alias for traceopt",
    NULL)

COMMAND("$zapcmd",   SYSDEBUG,           zapcmd_cmd,
  NULL,
    NULL)     // enable/disable commands and config statements

COMMAND("$test",     SYSDEBUG,           test_cmd,
  NULL,
    NULL)     // enable in config with: $zapcmd $test cmd

#ifdef OPTION_CMDTGT
COMMAND("cmdtgt",    SYSCMDALL,          cmdtgt_cmd,
  "Specify the command target",
    "Format: \"cmdtgt [herc | scp | pscp | ?]\". Specify the command target.\n")

COMMAND("herc",      SYSCMDALL,          herc_cmd,
  "Hercules command",
    "Format: \"herc [cmd]\". Send hercules cmd in any cmdtgt mode.\n")

COMMAND("scp",       SYSCMDALL,          scp_cmd,
  "Send scp command",
    "Format: \"scp [cmd]\". Send scp cmd in any cmdtgt mode.\n")

COMMAND("pscp",      SYSCMDALL,          prioscp_cmd,
  "Send prio message scp command",
    "Format: \"pscp [cmd]\". Send priority message cmd to scp in any\n"
    "cmdtgt mode.\n")
#endif // OPTION_CMDTGT

// The actual command table ends here, the next entries are just for help
// as the associated command are processed as part of commandline parsing
// and there are no forward references to be created

#if !defined(_FW_REF)
COMMAND("sf+dev",    SYSCMDALL-SYSOPER,  NULL,
  "Add shadow file",
    NULL)

COMMAND("sf-dev",    SYSCMDALL-SYSOPER,  NULL,
  "Delete shadow file",
    NULL)

COMMAND("sfc",       SYSCMDALL-SYSOPER,  NULL,
  "Compress shadow files",
    NULL)

COMMAND("sfk",       SYSCMDALL-SYSOPER,  NULL,
  "Check shadow files",
    "Format: \"sfk{*|xxxx} [n]\". Performs a chkdsk on the active shadow file\n"
    "where xxxx is the device number (*=all cckd devices)\n"
    "and n is the optional check level (default is 2):\n"
    " -1 devhdr, cdevhdr, l1 table\n"
    "  0 devhdr, cdevhdr, l1 table, l2 tables\n"
    "  1 devhdr, cdevhdr, l1 table, l2 tables, free spaces\n"
    "  2 devhdr, cdevhdr, l1 table, l2 tables, free spaces, trkhdrs\n"
    "  3 devhdr, cdevhdr, l1 table, l2 tables, free spaces, trkimgs\n"
    "  4 devhdr, cdevhdr. Build everything else from recovery\n"
    "You probably don't want to use `4' unless you have a backup and are\n"
    "prepared to wait a long time.\n"                                     )

COMMAND("sfd",       SYSCMDALL-SYSOPER,  NULL,
  "Display shadow file stats",
    NULL)

COMMAND("t{+/-}dev", SYSCMDALL-SYSOPER,  NULL,
  "Turn CCW tracing on/off",
    NULL)

COMMAND("s{+/-}dev", SYSCMDALL-SYSOPER,  NULL,
  "Turn CCW stepping on/off",
    NULL)

#ifdef OPTION_CKD_KEY_TRACING
COMMAND("t{+/-}CKD", SYSCMDALL-SYSOPER,  NULL,
  "Turn CKD_KEY tracing on/off",
    NULL)
#endif

COMMAND("f{+/-}adr", SYSCMDALL-SYSOPER,  NULL,
  "Mark frames unusable/usable",
    NULL)
#endif /*!defined(_FW_REF)*/

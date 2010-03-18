/* CMDTAB.H     (c) Copyright Roger Bowler, 1999-2009                */
/*              (c) Copyright "Fish" (David B. Trout), 2002-2009     */
/*              (c) Copyright Jan Jaeger, 2003-2009                  */
/*              Defines all Hercules Configuration statements        */
/*              and panel commands                                   */

// $Id$


//          command    type          function      one-line description...
// COMMAND ("sample"   PANEL,        sample_cmd,   "short help text", "long help text" )
// COMMAND ("sample2"  PANEL+CONFIG, sample2_cmd,  "short help text", NULL )  // No long help provided
// COMMAND ("sample3"  CONFIG,       sample3_cmd,  NULL, NULL ) // No help provided at all
// COMMAND ("sample4"  DISABLED,     sample4_cmd,  NULL, NULL ) // Disabled command - generally debugging only

COMMAND ( "help",      PANEL,        HelpCommand,
  "list all commands / command specific help",
    "Enter \"help cmd\" where cmd is the command you need help\n"
    "with. If the command has additional help text defined for it,\n"
    "it will be displayed. Help text is usually limited to explaining\n"
    "the format of the command and its various required or optional\n"
    "parameters and is not meant to replace reading the documentation.\n" )

COMMAND ( "?",         PANEL,        HelpCommand,  "alias for help\n", NULL )

COMMAND ( "*",         CONFIG+PANEL, comment_cmd,  "Comment", NULL )

COMMAND ( "#",         CONFIG+PANEL, comment_cmd,  "Comment\n", NULL )

CMDABBR ( "message",1, PANEL,        msg_cmd,      "Display message on console a la VM", NULL )
CMDABBR ( "msg",1,     PANEL,        msg_cmd,      "Alias for message", NULL )
COMMAND ( "msgnoh",    PANEL,        msgnoh_cmd,   "Similar to \"message\" but no header\n", NULL )

COMMAND ( "hst",       PANEL,        History,
  "history of commands",
    "Format: \"hst | hst n | hst l\". Command \"hst l\" or \"hst 0\" displays\n"
    "list of last ten commands entered from command line\n"
    "hst n, where n is a positive number retrieves n-th command from list\n"
    "hst n, where n is a negative number retrieves n-th last command\n"
    "hst without an argument works exactly as hst -1, it retrieves last command\n" )

#if defined(OPTION_HAO)
COMMAND ( "hao",       PANEL,        hao_cmd,
  "Hercules Automatic Operator",
    "Format: \"hao  tgt <tgt> | cmd <cmd> | list <n> | del <n> | clear \".\n"
    "  hao tgt <tgt> : define target rule (regex pattern) to react on\n"
    "  hao cmd <cmd> : define command for previously defined rule\n"
    "  hao list <n>  : list all rules/commands or only at index <n>\n"
    "  hao del <n>   : delete the rule at index <n>\n"
    "  hao clear     : delete all rules (stops automatic operator)\n" )
#endif /* defined(OPTION_HAO) */

COMMAND ( "log",       PANEL,        log_cmd,      "direct log output", NULL )

COMMAND ( "logopt",    PANEL+CONFIG, logopt_cmd,
  "change log options",
    "Format: \"logopt [timestamp | notimestamp]\".   Sets logging options.\n"
    "\"timestamp\" inserts a time stamp in front of each log message.\n"
    "\"notimestamp\" displays log messages with no time stamps.  Entering\n"
    "the command with no arguments displays current logging options.\n"
    "\"timestamp\" and \"notimestamp\" may be abbreviated as \"time\"\n"
    "and \"notime\" respectively.\n"                                      )

COMMAND ( "uptime",    PANEL,        uptime_cmd,   "display how long Hercules has been running\n", NULL )
COMMAND ( "version",   PANEL,        version_cmd,  "display version information\n", NULL )

COMMAND ( "quit",      PANEL,        quit_cmd,     "terminate the emulator", NULL )
COMMAND ( "exit",      PANEL,        quit_cmd,     "(synonym for 'quit')\n", NULL )

COMMAND ( "cpu",       PANEL,        cpu_cmd,
  "Define target cpu for panel display and commands\n",
    "Format: \"cpu hh\" where 'hh' is the hexadecimal cpu address of the cpu\n"
    "in your multiprocessor configuration which you wish all panel commands\n"
    "to apply to. For example, entering 'cpu 1F' followed by \"gpr\" will\n"
    "display the general purpose registers for cpu 31 of your configuration.\n" )

COMMAND ( "fcb",     PANEL,        fcb_cmd,
  "display the current FCB (if only the printer is given)\n",
   "Reset the fcb to the standard one \n" 
   "Load a fcb image \n" )

COMMAND ( "start",     PANEL,        start_cmd,
  "start CPU (or printer device if argument given)",
    "Entering the 'start' command by itself simply starts a stopped\n"
    "CPU, whereas 'start <devn>' presses the virtual start button on\n"
    "printer device <devn>.\n"                                            )

COMMAND ( "stop",      PANEL,        stop_cmd,
  "stop CPU (or printer device if argument given)\n",
    "Entering the 'stop' command by itself simply stops a running\n"
    "CPU, whereas 'stop <devn>' presses the virtual stop button on\n"
    "printer device <devn>, usually causing an INTREQ.\n"                 )

COMMAND ( "startall",  PANEL,        startall_cmd, "start all CPU's", NULL )

COMMAND ( "stopall",   PANEL,        stopall_cmd,  "stop all CPU's\n", NULL )

#ifdef _FEATURE_CPU_RECONFIG
COMMAND ( "cf",        PANEL,        cf_cmd,
  "Configure current CPU online or offline",
    "Configure current CPU online or offline:  Format->  \"cf [on|off]\"\n"
    "Where the 'current' CPU is defined as whatever CPU was defined as\n"
    "the panel command target cpu via the \"cpu\" panel command. (Refer\n"
    "to the 'cpu' command for further information) Entering 'cf' by itself\n"
    "simply displays the current online/offline status of the current cpu.\n"
    "Otherwise the current cpu is configured online or offline as specified.\n"
    "Use 'cfall' to configure/display all CPUs online/offline state.\n"   )

COMMAND ( "cfall",     PANEL,        cfall_cmd,    "configure all CPU's online or offline\n", NULL )
#endif

#ifdef _FEATURE_SYSTEM_CONSOLE
COMMAND ( ".reply",    PANEL,        g_cmd,
  "scp command",
    "To reply to a system control program (i.e. guest operating system)\n"
    "message that gets issued to the hercules console, prefix the reply\n"
    "with a period.\n"                                                    )

COMMAND ( "!message",  PANEL,        g_cmd,
  "scp priority messsage",
    "To enter a system control program (i.e. guest operating system)\n"
    "priority command on the hercules console, simply prefix the command\n"
    "with an exclamation point '!'.\n"                                    )

COMMAND ( "ssd",       PANEL,        ssd_cmd,
  "signal shutdown\n",
    "The SSD (signal shutdown) command signals an imminent hypervisor shutdown to\n"
    "the guest.  Guests who support this are supposed to perform a shutdown upon\n"
    "receiving this request.\n"
    "An implicit ssd command is given on a hercules \"quit\" command if the guest\n"
    "supports ssd.  In that case hercules shutdown will be delayed until the guest\n"
    "has shutdown or a 2nd quit command is given.\n"                      )
#endif

#ifdef OPTION_PTTRACE
COMMAND ( "ptt",       PANEL+CONFIG, EXT_CMD(ptt_cmd),
  "Set or display internal trace\n",

    "Format: \"ptt [options] [nnn]\"\n"
    "When specified with no operands, the ptt command displays the trace options\n"
    "and the contents of the internal trace table.\n"
    "When specified with operands, the ptt command sets the trace options and/or\n"
    "specifies which events are to be traced. If the last operand is numeric, it\n"
    "sets the size of the trace table and activates the trace.\n"
     "options:\n"
     "  (no)error   - error trace\n"
     "  (no)control - control trace\n"
     "  (no)prog    - program interrupt trace\n"
     "  (no)inter   - interlock failure trace\n"
     "  (no)sie     - sie trace\n"
     "  (no)signal  - signaling trace\n"
     "  (no)io      - io trace\n"
     "  (no)timer   - timer trace\n"
     "  (no)threads - thread trace\n"
     "  (no)logger  - logger trace\n"
     "  (no)lock    - lock trace buffer\n"
     "  (no)tod     - timestamp trace entries\n"
     "  (no)wrap    - wrap trace buffer\n"
     "  to=nnn      - trace buffer display timeout\n"
     "  nnn         - trace buffer size\n"                                  )
#endif

COMMAND ( "i",         PANEL,        i_cmd,        "generate I/O attention interrupt for device", NULL )

COMMAND ( "ext",       PANEL,        ext_cmd,      "generate external interrupt", NULL )

COMMAND ( "restart",   PANEL,        restart_cmd,  "generate restart interrupt", NULL )

COMMAND ( "archmode",  PANEL+CONFIG, archmode_cmd,
  "Set architecture mode",
    "Format: \"archmode [S/370 | ESA/390 | z/Arch | ESAME]\". Entering the command\n"
    "without any argument simply displays the current architecture mode. Entering\n"
    "the command with an argument sets the architecture mode to the specified value.\n"
    "Note: \"ESAME\" (Enterprise System Architecture, Modal Extensions) is simply a\n"
    "synonym for \"z/Arch\". (they are identical to each other and mean the same thing)\n" )

COMMAND ( "loadparm",  PANEL+CONFIG, loadparm_cmd, "set IPL parameter\n", NULL )

COMMAND ( "lparname",  PANEL+CONFIG, lparname_cmd, "set LPAR name\n", NULL )

COMMAND ( "lparnum",   PANEL+CONFIG, lparnum_cmd,  "set LPAR identification number\n", NULL )

#if defined(OPTION_SET_STSI_INFO)
COMMAND ( "model",     CONFIG,       stsi_model_cmd,"Set STSI model code", NULL )

COMMAND ( "plant",     CONFIG,       stsi_plant_cmd,"Set STSI plant code", NULL )

COMMAND ( "manufacturer",CONFIG,     stsi_mfct_cmd,"Set STSI manufacturer code\n", NULL )
#endif /* defined(OPTION_SET_STSI_INFO) */

COMMAND ( "pgmprdos",  CONFIG,        pgmprdos_cmd,"set LPP license setting\n", NULL )

COMMAND ( "codepage",  CONFIG,        codepage_cmd,"set codepage conversion table\n", NULL )

COMMAND ( "diag8cmd",  CONFIG,        diag8_cmd,   "Set diag8 command option\n", NULL )

// The shcmdopt config statement should never be a command as it will introduce a possible integrity exposure *JJ
COMMAND ( "shcmdopt",  CONFIG,        shcmdopt_cmd,"Set diag8 sh option\n", NULL )

COMMAND ( "legacysenseid",CONFIG,     lsid_cmd,    "set legacysenseid setting\n", NULL )

COMMAND ( "ipl",       PANEL,         ipl_cmd,
  "IPL Normal from device xxxx",
    "Format: \"ipl nnnn [parm xxxxxxxxxxxxxx]\"\n"
    "Performs the Initial Program Load manual control function. If the first operand\n"
    "'nnnn' is a 1- to 4-digit hexadecimal number, a CCW-type IPL is initiated from\n"
    "the indicated device number, and SCLP disk I/O is disabled.\n"
    "Otherwise a list-directed IPL is performed from the .ins file named 'nnnn', and\n"
    "SCLP disk I/O is enabled for the directory path where the .ins file is located.\n"
    "An optional 'parm' keyword followed by a string can also be passed to the IPL\n"
    "command processor. The string will be loaded into the low-order 32 bits of the\n"
    "general purpose registers (4 characters per register for up to 64 bytes).\n"
    "The PARM option behaves similarly to the VM IPL command.\n"          )

COMMAND ( "iplc",      PANEL,        iplc_cmd,
  "IPL Clear from device xxxx",
    "Performs the Load Clear manual control function. See \"ipl\".\n"     )

COMMAND ( "sysreset",  PANEL,        sysr_cmd,
  "issue SYSTEM Reset manual operation",
    "Performs the System Reset manual control function. A CPU and I/O\n"
    "subsystem reset are performed.\n"                                    )

COMMAND ( "sysclear",  PANEL,        sysc_cmd,
  "issue SYSTEM Clear Reset manual operation",
    "Performs the System Reset Clear manual control function. Same as\n"
    "the \"sysreset\" command but also clears main storage to 0. Also, registers\n"
    "control registers, etc.. are reset to their initial value. At this\n"
    "point, the system is essentially in the same state as it was just after\n"
    "having been started\n"                                               )

COMMAND ( "store",     PANEL,        store_cmd,    "store CPU status at absolute zero\n", NULL )

COMMAND ( "sclproot",  CONFIG+PANEL, sclproot_cmd,
  "set SCLP base directory",
    "Format: \"sclproot [path|NONE]\"\n"
    "Enables SCLP disk I/O for the specified directory path, or disables SCLP disk\n"
    "I/O if NONE is specified. A subsequent list-directed IPL resets the path to\n"
    "the location of the .ins file, and a CCW-type IPL disables SCLP disk I/O.\n"
    "If no operand is specified, sclproot displays the current setting.\n")

#if defined(OPTION_HTTP_SERVER)
COMMAND ( "httproot",  CONFIG,       httproot_cmd, "Set HTTP server root directory", NULL )

COMMAND ( "httpport",  CONFIG,       httpport_cmd, "Set HTTP server port\n", NULL )

#if defined( HTTP_SERVER_CONNECT_KLUDGE )
COMMAND ( "HTTP_SERVER_CONNECT_KLUDGE", CONFIG, httpskm_cmd, "HTTP_SERVER_CONNECT_KLUDGE", NULL )
#endif // defined( HTTP_SERVER_CONNECT_KLUDGE )
#endif /*defined(OPTION_HTTP_SERVER)*/

COMMAND ( "psw",       PANEL,        psw_cmd,
  "display or alter program status word",
    "Format: \"psw [operand ...]\" where 'operand ...' is one or more optional\n"
    "parameters which modify the contents of the Program Status Word:\n\n"

    "  sm=xx                 system mask          (2 hex digits)\n"
    "  pk=n                  protection key       (decimal 0 to 15)\n"
    "  cmwp=x                C/M/W/P bits         (one hex digit)\n"
    "  as=pri|sec|ar|home    address-space\n"
    "  cc=n                  condition code       (decimal 0 to 3)\n"
    "  pm=x                  program mask         (one hex digit)\n"
    "  am=24|31|64           addressing mode\n"
    "  ia=xxx                instruction address  (1 to 16 hex digits)\n\n"

    "Enter \"psw\" by itself to display the current PSW without altering it.\n" )

COMMAND ( "gpr",       PANEL,        gpr_cmd,
  "display or alter general purpose registers",
    "Format: \"gpr [nn=xxxxxxxxxxxxxxxx]\" where 'nn' is the optional register\n"
    "number (0 to 15) and 'xxxxxxxxxxxxxxxx' is the register value in hexadecimal\n"
    "(1-8 hex digits for 32-bit registers or 1-16 hex digits for 64-bit registers).\n"
    "Enter \"gpr\" by itself to display the register values without altering them.\n" )

COMMAND ( "fpr",       PANEL,        fpr_cmd,       "display floating point registers", NULL )

COMMAND ( "fpc",       PANEL,        fpc_cmd,       "display floating point control register", NULL )

COMMAND ( "cr",        PANEL,        cr_cmd,
  "display or alter control registers",
    "Format: \"cr [nn=xxxxxxxxxxxxxxxx]\" where 'nn' is the optional control register\n"
    "number (0 to 15) and 'xxxxxxxxxxxxxxxx' is the control register value in hex\n"
    "(1-8 hex digits for 32-bit registers or 1-16 hex digits for 64-bit registers).\n"
    "Enter \"cr\" by itself to display the control registers without altering them.\n" )

COMMAND ( "ar",        PANEL,        ar_cmd,        "display access registers", NULL )

COMMAND ( "pr",        PANEL,        pr_cmd,        "display prefix register", NULL )

COMMAND ( "timerint",  PANEL+CONFIG, timerint_cmd,"display or set timers update interval", NULL )

COMMAND ( "clocks",    PANEL,        clocks_cmd,    "display tod clkc and cpu timer", NULL )

COMMAND ( "ipending",  PANEL,        ipending_cmd,  "display pending interrupts", NULL )

COMMAND ( "ds",        PANEL,        ds_cmd,        "display subchannel", NULL )

COMMAND ( "r",         PANEL,        r_cmd,
  "display or alter real storage",
    "Format: \"r addr[.len]\" or \"r addr-addr\" to display real\n"
    "storage, or \"r addr=value\" to alter real storage, where 'value'\n"
    "is a hex string of up to 32 pairs of digits.\n"                      )

COMMAND ( "v",         PANEL,        v_cmd,
  "display or alter virtual storage",
    "Format: \"v [P|S|H] addr[.len]\" or \"v [P|S|H] addr-addr\" to display virtual\n"
    "storage, or \"v [P|S|H] addr=value\" to alter virtual storage, where 'value'\n"
    "is a hex string of up to 32 pairs of digits. The optional 'P' or 'S' or 'H'\n"
    "will force Primary, Secondary, or Home translation instead of current PSW mode.\n" )

COMMAND ( "u",         PANEL,        u_cmd,         "disassemble storage", NULL )

COMMAND ( "devtmax",   PANEL+CONFIG, devtmax_cmd,   "display or set max device threads", NULL )

COMMAND ( "k",         PANEL,        k_cmd,         "display cckd internal trace\n", NULL )

COMMAND ( "attach",    PANEL,        attach_cmd,
  "configure device",
    "Format: \"attach devn type [arg...]\n"                               )

COMMAND ( "detach",    PANEL,        detach_cmd,    "remove device", NULL )

COMMAND ( "define",    PANEL,        define_cmd,
  "rename device",
    "Format: \"define olddevn newdevn\"\n"                                )

COMMAND ( "devinit",   PANEL,        devinit_cmd,
  "reinitialize device",
    "Format: \"devinit devn [arg...]\"\n"
    "If no arguments are given then the same arguments are used\n"
    "as were used the last time the device was created/initialized.\n"    )

COMMAND ( "devlist",   PANEL,        devlist_cmd,   "list device or all devices\n", NULL )

COMMAND ( "qd",        PANEL,        qd_cmd,        "query dasd\n", NULL )

COMMAND ( "mounted_tape_reinit", PANEL+CONFIG, mnttapri_cmd,  "Control tape initilisation", NULL )

#if defined( OPTION_TAPE_AUTOMOUNT )
COMMAND ( "automount", PANEL+CONFIG, automount_cmd,
  "Show/Update allowable tape automount directories\n",
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
    "The automount feature is appropriately enabled or disabled for all tape\n"
    "devices as needed depending on the updated empty/non-empty list state.\n" )
#endif /* OPTION_TAPE_AUTOMOUNT */

#if defined( OPTION_SCSI_TAPE )
COMMAND ( "auto_scsi_mount", PANEL+CONFIG, ascsimnt_cmd,  "Control SCSI tape mount", NULL )

COMMAND ( "scsimount", PANEL,        scsimount_cmd,
  "automatic SCSI tape mounts\n",
    "Format:    \"scsimount  [ no | yes | 0-99 ]\".\n"
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
    "system). 'yes' is equivalent to specifying a 5 second interval.\n"   )
#endif /* defined( OPTION_SCSI_TAPE ) */

COMMAND ( "cd",        PANEL,        cd_cmd,        "change directory", NULL )

COMMAND ( "pwd",       PANEL,        pwd_cmd,       "print working directory", NULL )

COMMAND ( "sh",        PANEL,        sh_cmd,
  "shell command\n",
    "Format: \"sh command [args...]\" where 'command' is any valid shell\n"
    "command. The entered command and any arguments are passed as-is to the\n"
    "shell for processing and the results are displayed on the console.\n" )

COMMAND ( "cache",     PANEL,        EXT_CMD(cache_cmd), "cache command", NULL )

COMMAND ( "cckd",      PANEL+CONFIG, cckd_cmd,       "cckd command", NULL )

COMMAND ( "shrd",      PANEL,        EXT_CMD(shared_cmd), "shrd command", NULL )

COMMAND ( "conkpalv",  PANEL+CONFIG, conkpalv_cmd,
  "Display/alter console TCP keep-alive settings",
    "Format: \"conkpalv (idle,intv,count)\" where 'idle', 'intv' and 'count' are the\n"
    "new values for the TCP keep-alive settings for console connections:\n"
    "- send probe when connection goes idle for 'idle' seconds\n"
    "- wait maximum of 'intv' seconds for a response to probe\n"
    "- disconnect after 'count' consecutive failed probes\n"
    "The format must be exactly as shown, with each value separated from the next by\n"
    "a single comma, no intervening spaces between them, surrounded by parenthesis.\n"
    "The command \"conkpalv\" without any operand displays the current values.\n" )

COMMAND ( "quiet",     PANEL,        quiet_cmd,
  "Toggle automatic refresh of panel display data\n",
    "'quiet' either disables automatic screen refreshing if it is\n"
    "currently enabled or enables it if it is currently disabled.\n"
    "When disabled you will no be able to see the response of any\n"
    "entered commands nor any messages issued by the system nor be\n"
    "able to scroll the display, etc. Basically all screen updating\n"
    "is disabled. Entering 'quiet' again re-enables screen updating.\n"   )

COMMAND ( "t",         PANEL,        trace_cmd,
  "instruction trace",
    "Format: \"t addr-addr\" or \"t addr:addr\" or \"t addr.length\"\n"
    "sets the instruction tracing range (which is totally separate from\n"
    "the instruction stepping and breaking range).\n"
    "With or without a range, the t command displays whether instruction\n"
    "tracing is on or off and the range if any.\n"
    "The t command by itself does not activate instruction tracing.\n"
    "Use the t+ command to activate instruction tracing.\n"
    "\"t 0\" eliminates the range (all addresses will be traced).\n"      )

COMMAND ( "t+",        PANEL,        trace_cmd,
  "instruction trace on",
    "Format: \"t+\" turns on instruction tracing. A range can be specified\n"
    "as for the \"t\" command, otherwise the existing range is used. If there\n"
    "is no range (or range was specified as 0) then all instructions will be\n"
    "traced.\n"                                                           )

COMMAND ( "t-",        PANEL,        trace_cmd,
  "instruction trace off",
    "Format: \"t-\" turns off instruction tracing.\n"                     )

COMMAND ( "t?",        PANEL,        trace_cmd,
  "instruction trace query",
    "Format: \"t?\" displays whether instruction tracing is on or off\n"
    "and the range if any.\n"                                             )

COMMAND ( "s",         PANEL,        trace_cmd,
  "instruction stepping",
    "Format: \"s addr-addr\" or \"s addr:addr\" or \"s addr.length\"\n"
    "sets the instruction stepping and instruction breaking range,\n"
    "(which is totally separate from the instruction tracing range).\n"
    "With or without a range, the s command displays whether instruction\n"
    "stepping is on or off and the range if any.\n"
    "The s command by itself does not activate instruction stepping.\n"
    "Use the s+ command to activate instruction stepping.\n"
    "\"s 0\" eliminates the range (all addresses will be stepped).\n"     )

COMMAND ( "s+",        PANEL,        trace_cmd,
  "instruction stepping on",
    "Format: \"s+\" turns on instruction stepping. A range can be specified\n"
    "as for the \"s\" command, otherwise the existing range is used. If there\n"
    "is no range (or range was specified as 0) then the range includes all\n"
    "addresses. When an instruction within the range is about to be executed,\n"
    "the CPU is temporarily stopped and the next instruction is displayed.\n"
    "You may then examine registers and/or storage, etc, before pressing Enter\n"
    "to execute the instruction and stop at the next instruction. To turn\n"
    "off instruction stepping and continue execution, enter the \"g\" command.\n" )

COMMAND ( "s-",        PANEL,        trace_cmd,
  "instruction stepping off",
    "Format: \"s-\" turns off instruction stepping.\n"                    )

COMMAND ( "s?",        PANEL,        trace_cmd,
  "instruction stepping query",
    "Format: \"s?\" displays whether instruction stepping is on or off\n"
    "and the range if any.\n"                                             )

COMMAND ( "b",         PANEL,        trace_cmd,
  "set breakpoint",
    "Format: \"b addr\" or \"b addr-addr\" where 'addr' is the instruction\n"
    "address or range of addresses where you wish to halt execution. This\n"
    "command is synonymous with the \"s+\" command.\n"                    )

COMMAND ( "b+",        PANEL,        trace_cmd,    "set breakpoint", NULL )

COMMAND ( "b-",        PANEL,        trace_cmd,
  "delete breakpoint",
    "Format: \"b-\"  This command is the same as \"s-\"\n"                )

COMMAND ( "g",         PANEL,        g_cmd,        "turn off instruction stepping and start all CPUs\n", NULL )

COMMAND ( "ostailor",  PANEL+CONFIG, ostailor_cmd,
  "trace program interrupts",
    "Format: \"ostailor quiet | os/390 | z/os | vm | vse | linux | null\". Specifies\n"
    "the intended operating system. The effect is to reduce control panel message\n"
    "traffic by selectively suppressing program check trace messages which are\n"
    "considered normal in the specified environment. 'quiet' suppresses all\n"
    "exception messages, whereas 'null' suppresses none of them. The other options\n"
    "suppress some messages and not others depending on the specified o/s. Prefix\n"
    "values with '+' to combine them with existing values or '-' to exclude them.\n"
    "SEE ALSO the 'pgmtrace' command which allows you to further fine tune\n"
    "the tracing of program interrupt exceptions.\n"                      )

COMMAND ( "pgmtrace",  PANEL,        pgmtrace_cmd,
  "trace program interrupts",
    "Format: \"pgmtrace [-]intcode\" where 'intcode' is any valid program\n"
    "interruption code in the range 0x01 to 0x40. Precede the interrupt code\n"
    "with a '-' to stop tracing of that particular program interruption.\n" )

COMMAND ( "savecore",  PANEL,        savecore_cmd,
  "save a core image to file",
    "Format: \"savecore filename [{start|*}] [{end|*}]\" where 'start' and 'end'\n"
    "define the starting and ending addresss of the range of real storage to be\n"
    "saved to file 'filename'.  '*' for either the start address or end address\n"
    "(the default) means: \"the first/last byte of the first/last modified page\n"
    "as determined by the storage-key 'changed' bit\".\n"                 )

COMMAND ( "loadcore",  PANEL,        loadcore_cmd,
  "load a core image file",
    "Format: \"loadcore filename [address]\" where 'address' is the storage address\n"
    "of where to begin loading memory. The file 'filename' is presumed to be a pure\n"
    "binary image file previously created via the 'savecore' command. The default for\n"
    "'address' is 0 (begining of storage).\n"                             )

COMMAND ( "loadtext",  PANEL,        loadtext_cmd,
  "load a text deck file\n",
    "Format: \"loadtext filename [address]\". This command is essentially identical\n"
    "to the 'loadcore' command except that it loads a text deck file with \"TXT\"\n"
    "and \"END\" 80 byte records (i.e. an object deck).\n"                )

#if defined(OPTION_DYNAMIC_LOAD)
COMMAND ( "modpath",   CONFIG,       modpath_cmd,  "set module load path", NULL )
COMMAND ( "ldmod",     CONFIG+PANEL, ldmod_cmd,    "load a module", NULL )
COMMAND ( "rmmod",     PANEL,        rmmod_cmd,    "delete a module", NULL )
COMMAND ( "lsmod",     PANEL,        lsmod_cmd,    "list dynamic modules", NULL )
COMMAND ( "lsdep",     PANEL,        lsdep_cmd,    "list module dependencies\n", NULL )
#endif /*defined(OPTION_DYNAMIC_LOAD)*/

#ifdef OPTION_IODELAY_KLUDGE
COMMAND ( "iodelay",   PANEL+CONFIG, iodelay_cmd,   "display or set I/O delay value", NULL )
#endif

COMMAND ( "ctc",       PANEL,        ctc_cmd,
  "Enable/Disable CTC debugging",
    "Format:  \"ctc  debug  { on | off }  [ <devnum> | ALL ]\".\n\n"
    "Enables/disables debug packet tracing for the specified CTCI/LCS\n"
    "device group(s) identified by <devnum> or for all CTCI/LCS device\n"
    "groups if <devnum> is not specified or specified as 'ALL'.\n"        )

#if defined(OPTION_W32_CTCI)
COMMAND ( "tt32",      PANEL,        tt32_cmd,
  "control/query CTCI-W32 functionality",
    "Format:  \"tt32   debug | nodebug | stats <devnum>\".\n"
    "\n"
    "Enables or disables global CTCI-W32 debug tracing\n"
    "or displays TunTap32 stats for the specified CTC device.\n"          )
#endif

COMMAND ( "toddrag",   PANEL+CONFIG, toddrag_cmd,  "display or set TOD clock drag factor", NULL )

#ifdef PANEL_REFRESH_RATE
COMMAND ( "panrate",   PANEL+CONFIG, panrate_cmd,
  "Display or set rate at which console refreshes",
    "Format: \"panrate [nnn | fast | slow]\". Sets or displays the panel refresh rate.\n"
    "panrate nnn sets the refresh rate to nnn milliseconds.\n"
    "panrate fast sets the refresh rate to " MSTRING(PANEL_REFRESH_RATE_FAST) " milliseconds.\n"
    "panrate slow sets the refresh rate to " MSTRING(PANEL_REFRESH_RATE_SLOW) " milliseconds.\n"
    "If no operand is specified, panrate displays the current refresh rate.\n")
#endif

COMMAND ( "pantitle",  CONFIG,       pantitle_cmd, "display or set console title", NULL )

#ifdef OPTION_MSGHLD
COMMAND ( "msghld",    PANEL,        msghld_cmd,
  "Display or set the timeout of held messages",
    "Format: \"msghld [value | info | clear]\".\n"
    "value: timeout value of held message in seconds\n"
    "info:  displays the timeout value\n"
    "clear: releases the held messages\n"                                 )
#endif

COMMAND ( "syncio",    PANEL,        syncio_cmd,    "display syncio devices statistics", NULL )

#if defined(OPTION_INSTRUCTION_COUNTING)
COMMAND ( "icount",    PANEL,        icount_cmd,    "display individual instruction counts", NULL )
#endif

#ifdef OPTION_MIPS_COUNTING
COMMAND ( "maxrates",  PANEL,        maxrates_cmd,
  "display maximum observed MIPS/SIOS rate for the\n"
  "               defined interval or define a new reporting interval\n",
    "Format: \"maxrates [nnnn]\" where 'nnnn' is the desired reporting\n"
    "interval in minutes. Acceptable values are from 1 to 1440. The default\n"
    "is 1440 minutes (one day). Entering \"maxrates\" by itself displays\n"
    "the current highest rates observed during the defined intervals.\n"  )
#endif // OPTION_MIPS_COUNTING

#if defined(_FEATURE_ASN_AND_LX_REUSE)
COMMAND ( "asn_and_lx_reuse", CONFIG, alrf_cmd, "Enable/Disable ASN and LX reuse facility", NULL )
COMMAND ( "alrf"            , CONFIG, alrf_cmd, "Alias for asn_and_lx_reuse\n", NULL )
#endif /* defined(_FEATURE_ASN_AND_LX_REUSE) */

#if defined(FISH_HANG)
COMMAND ( "FishHangReport",   PANEL, FishHangReport_cmd,
  "Display thread/lock/event objects (DEBUG)\n",
    "When built with --enable-fthreads --enable-fishhang, a detailed record of\n"
    "every thread, lock and event that is created is maintained for debugging purposes.\n"
    "If a lock is accessed before it has been initialized or if a thread exits while\n"
    "still holding a lock, etc (including deadlock situations), the FishHang logic will\n"
    "detect and report it. If you suspect one of hercules's threads is hung waiting for\n"
    "a condition to be signalled for example, entering \"FishHangReport\" will display\n"
    "the internal list of thread, locks and events to possibly help you determine where\n"
    "it's hanging and what event (condition) it's hung on.\n"             )
#endif

#if defined(OPTION_CONFIG_SYMBOLS)
COMMAND ( "defsym",    PANEL+CONFIG, defsym_cmd,
  "Define symbol",
    "Format: \"defsym symbol [value]\". Defines symbol 'symbol' to contain value 'value'.\n"
    "The symbol can then be the object of a substitution for later panel commands.\n"
    "If 'value' contains blanks or spaces, then it must be enclosed within quotes\n"
    "or apostrophes. For more detailed information regarding symbol substitution\n"
    "refer to the 'DEFSYM' configuration file statement in Hercules documentation.\n"
    "Enter \"defsym\" by itself to display the values of all defined symbols.\n" )
#endif

COMMAND ( "script",    PANEL,        script_cmd,
  "Run a sequence of panel commands contained in a file",
    "Format: \"script filename [...filename...]\". Sequentially executes the commands contained\n"
    "within the file -filename-. The script file may also contain \"script\" commands,\n"
    "but the system ensures that no more than 10 levels of script are invoked at any\n"
    "one time (to avoid a recursion loop)\n"                              )

COMMAND ( "cscript",   PANEL,        cscript_cmd,
  "Cancels a running script thread\n",
    "Format: \"cscript\". This command will cancel the currently running script.\n"
    "if no script is running, no action is taken\n"                       )

#if defined(FEATURE_ECPSVM)
COMMAND ( "evm",       PANEL,        evm_cmd_1,
  "ECPS:VM Commands (Deprecated)",
    "Format: \"evm\". This command is deprecated.\n"
    "use \"ecpsvm\" instead\n"                                            )

COMMAND ( "ecpsvm",    PANEL,        evm_cmd,
  "ECPS:VM Commands\n",
    "Format: \"ecpsvm\". This command invokes ECPS:VM Subcommands.\n"
    "Type \"ecpsvm help\" to see a list of available commands\n"          )
#endif

COMMAND ( "aea",       PANEL,        aea_cmd,       "Display AEA tables", NULL )
COMMAND ( "aia",       PANEL,        aia_cmd,       "Display AIA fields", NULL )
COMMAND ( "tlb",       PANEL,        tlb_cmd,       "Display TLB tables\n", NULL )

#if defined(SIE_DEBUG_PERFMON)
COMMAND ( "spm",       PANEL,        spm_cmd,       "SIE performance monitor\n", NULL )
#endif
#if defined(OPTION_COUNTING)
COMMAND ( "count",     PANEL,        count_cmd,     "Display/clear overall instruction count\n", NULL )
#endif
COMMAND ( "sizeof",    PANEL,        sizeof_cmd,    "Display size of structures\n", NULL )

COMMAND ( "suspend",   PANEL,        suspend_cmd,   "Suspend hercules", NULL )

COMMAND ( "resume",    PANEL,        resume_cmd,    "Resume hercules\n", NULL )

COMMAND ( "herclogo",  PANEL,        herclogo_cmd,
  "Read a new hercules logo file\n",
    "Format: \"herclogo [<filename>]\". Load a new logo file for 3270 terminal sessions\n"
    "If no filename is specified, the built-in logo is used instead\n"    )

COMMAND ( "traceopt",  CONFIG+PANEL, traceopt_cmd,
  "Instruction trace display options\n",
    "Format: \"traceopt [regsfirst | noregs | traditional]\". Determines how the\n"
    "registers are displayed during instruction tracing and stepping. Entering\n"
    "the command without any argument simply displays the current mode.\n" )

COMMAND ( "symptom",   CONFIG,       traceopt_cmd, "Alias for traceopt\n", NULL )

COMMAND ( "$zapcmd",   CONFIG,       zapcmd_cmd,   NULL, NULL )     // enable/disable commands and config statements

COMMAND ( "$test",     DISABLED,     test_cmd,     NULL, NULL )     // enable in config with: $zapcmd $test cmd

#ifdef OPTION_CMDTGT
COMMAND ( "cmdtgt",    PANEL,        cmdtgt_cmd,
  "Specify the command target",
    "Format: \"cmdtgt [herc | scp | pscp | ?]\". Specify the command target.\n" )

COMMAND ( "herc",      PANEL,        herc_cmd,
  "Hercules command",
    "Format: \"herc [cmd]\". Send hercules cmd in any cmdtgt mode.\n"     )

COMMAND ( "scp",       PANEL,        scp_cmd,
  "Send scp command",
    "Format: \"scp [cmd]\". Send scp cmd in any cmdtgt mode.\n"           )

COMMAND ( "pscp",      PANEL,        prioscp_cmd,
  "Send prio message scp command\n",
    "Format: \"pscp [cmd]\". Send priority message cmd to scp in any cmdtgt mode.\n" )
#endif // OPTION_CMDTGT

// The actual command table ends here, the next entries are just for help
// as the associated command are processed as part of commandline parsing
// and there are no forward references to be created

#if !defined(_FW_REF)
COMMAND ( "sf+dev",    PANEL,        NULL,         "add shadow file", NULL )

COMMAND ( "sf-dev",    PANEL,        NULL,         "delete shadow file", NULL )

COMMAND ( "sfc",       PANEL,        NULL,         "compress shadow files", NULL )

COMMAND ( "sfk",       PANEL,        NULL,
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

COMMAND ( "sfd",       PANEL,        NULL,         "display shadow file stats\n", NULL )

COMMAND ( "t{+/-}dev", PANEL,        NULL,         "turn CCW tracing on/off", NULL )

COMMAND ( "s{+/-}dev", PANEL,        NULL,         "turn CCW stepping on/off\n", NULL )

#ifdef OPTION_CKD_KEY_TRACING
COMMAND ( "t{+/-}CKD", PANEL,        NULL,         "turn CKD_KEY tracing on/off\n", NULL )
#endif

COMMAND ( "f{+/-}adr", PANEL,        NULL,         "mark frames unusable/usable\n", NULL )
#endif /*!defined(_FW_REF)*/

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


//          command    type          group      function      one-line description...
// COMMAND ("sample"   PANEL,        SYSOPER,   sample_cmd,   "short help text", "long help text" )
// COMMAND ("sample2"  PANEL+CONFIG, SYSCMDALL, sample2_cmd,  "short help text", NULL )  // No long help provided
// COMMAND ("sample3"  CONFIG,       SYSCMDALL, sample3_cmd,  NULL, NULL ) // No help provided at all
// COMMAND ("sample4"  DISABLED,     SYSDEBUG,  sample4_cmd,  NULL, NULL ) // Disabled command - generally debugging only

COMMAND("help",      PANEL,         SYSNONE,            HelpCommand, 
  "list all commands / command specific help",
    "Enter \"help cmd\" where cmd is the command you need help\n"
    "with. If the command has additional help text defined for it,\n"
    "it will be displayed. Help text is usually limited to explaining\n"
    "the format of the command and its various required or optional\n"
    "parameters and is not meant to replace reading the documentation.\n")
    
COMMAND("?",         PANEL,         SYSNONE,            HelpCommand,  
  "alias for help", 
    NULL)

COMMAND("cmdlevel",  PANEL+CONFIG,  SYSNONE,            CmdLevel,     
  "Display/Set current command group",
    "display/set the current command group set(s)\n"
    "Format: cmdlevel [{+/-}{ALL, MAINT, PROGrammer, OPERator,\n"
    "                        and/or DEVELoper}]\n")
    
COMMAND("cmdlvl",    PANEL+CONFIG,  SYSNONE,            CmdLevel,     
  "Alias for cmdlevel", 
    NULL)

COMMAND("cmdsep",    PANEL,         SYSNONE,            cmdsep_cmd,
  "Display/Set current command line separator",
    "display/set the current command line separator for multiple\n"
    "commands on a single line\n"
    "Format: cmdsep [c | off ]\n"
    "        c       a single character used for command separation\n"
    "                must not be '.', '!', or '-'\n"
    "                Although it maybe '#', this could have an affect\n"
    "                processing command lines that contain comments\n"
    "        off     disables command separation\n")

COMMAND("msglevel",  PANEL+CONFIG,  SYSNONE,            msglvl_cmd,   
  "Display or set the message level",
    "Format: msglevel [normal | debug | info]\n"
    "normal: default messages\n"
    "debug:  messages prefixed with source and linenumber\n"
    "info:   displays the message level\n")
    
COMMAND("msglvl",    PANEL+CONFIG,  SYSNONE,            msglvl_cmd,   
  "Alias for msglevel", 
    NULL)

COMMAND("emsg",      PANEL+CONFIG,  SYSNONE,            emsg_cmd,
  "Display/Set current Error Message display",
  "Format: emsg [ on | off | text | time ]\n"
  "  on    Normal message display\n"
  "  off   No messages are displayed\n"
  "  text  Text portion of message is display\n"
  "  time  Timestamp is prefixed to message\n" )

COMMAND("*",         PANEL+CONFIG,  SYSNONE,            comment_cmd,  
  "Comment", 
    NULL)

COMMAND("#",         PANEL+CONFIG,  SYSNONE,            comment_cmd,  
  "Comment", 
    NULL)

CMDABBR("message",1, PANEL,         SYSNONE,            msg_cmd,      
  "Display message on console a la VM", 
    NULL)
    
CMDABBR("msg",1,     PANEL,         SYSNONE,            msg_cmd,      
  "Alias for message", 
    NULL)
    
COMMAND("msgnoh",    PANEL,         SYSNONE,            msgnoh_cmd,   
  "Similar to \"message\" but no header", 
    NULL)

COMMAND("hst",       PANEL,         SYSNONE,            History,      
  "History of commands",
    "Format: \"hst | hst n | hst l\". Command \"hst l\" or \"hst 0\" displays\n"
    "list of last ten commands entered from command line\n"
    "hst n, where n is a positive number retrieves n-th command from list\n"
    "hst n, where n is a negative number retrieves n-th last command\n"
    "hst without an argument works exactly as hst -1, it retrieves the\n"
    "last command\n")

#if defined(OPTION_HAO)
COMMAND("hao",       PANEL,         SYSPROG+SYSDEVEL,   hao_cmd,      
  "Hercules Automatic Operator",
    "Format: \"hao  tgt <tgt> | cmd <cmd> | list <n> | del <n> | clear \".\n"
    "  hao tgt <tgt> : define target rule (regex pattern) to react on\n"
    "  hao cmd <cmd> : define command for previously defined rule\n"
    "  hao list <n>  : list all rules/commands or only at index <n>\n"
    "  hao del <n>   : delete the rule at index <n>\n"
    "  hao clear     : delete all rules (stops automatic operator)\n")
#endif /* defined(OPTION_HAO) */

COMMAND("log",       PANEL,         SYSCMDALL,          log_cmd,      
  "Direct log output", 
    NULL)

COMMAND("logopt",    PANEL+CONFIG,  SYSCMDALL,          logopt_cmd,   
  "Change log options",
    "Format: \"logopt [timestamp | notimestamp]\".   Sets logging options.\n"
    "\"timestamp\" inserts a time stamp in front of each log message.\n"
    "\"notimestamp\" displays log messages with no time stamps.  Entering\n"
    "the command with no arguments displays current logging options.\n"
    "\"timestamp\" and \"notimestamp\" may be abbreviated as \"time\"\n"
    "and \"notime\" respectively.\n")

COMMAND("uptime",    PANEL,         SYSCMDALL,          uptime_cmd,   
  "Display how long Hercules has been running", 
    NULL)
    
COMMAND("version",   PANEL,         SYSNONE,            version_cmd,  
  "Display version information", 
    NULL)

COMMAND("abort",     CONFIG,        SYSNONE,            abort_cmd,     
  "Abort initialisation",          
    NULL)

#if defined (OPTION_SHUTDOWN_CONFIRMATION)
COMMAND("quit",      PANEL,         SYSNONE,            quit_cmd,     
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

COMMAND("quitmout",  PANEL+CONFIG,  SYSNONE,            quitmout_cmd,
  "Set/Display the quit timeout value",
  "Format: \"quitmount [n]\" Sets or displays the elasped time that\n"
    "                        a confirming quit/ssd command must be\n"
    "                        entered.\n"
    "               n        This is a value between 2 and 60,\n"
    "                        representing seconds.\n")
#else
COMMAND("quit",      PANEL,         SYSNONE,            quit_cmd,     
  "Terminate the emulator", 
  "Format: \"quit [force]\"  Terminates the emulator. If the guest OS\n" 
    "                        has enabled Signal Shutdown, then a\n" 
    "                        signal shutdown request is sent to the\n"
    "                        guest OS and termination will begin\n" 
    "                        after guest OS has shutdown.\n"   
    "              force     This option will terminate the emulator\n"
    "                        immediately.\n" )
   
#endif
COMMAND("exit",      PANEL,         SYSNONE,            quit_cmd,     
  "(Synonym for 'quit')", 
    NULL)

COMMAND("cpu",       PANEL,         SYSCMDALL,          cpu_cmd,      
  "Define target cpu for panel display and commands",
    "Format: \"cpu xx\" where 'xx' is the hexadecimal cpu address of the cpu\n"
    "in your multiprocessor configuration which you wish all panel commands\n"
    "to apply to. For example, entering 'cpu 1F' followed by \"gpr\" will\n"
    "display the general purpose registers for cpu 31 of your\n"
    "configuration.\n")

COMMAND("start",     PANEL,         SYSCMDALL,          start_cmd,    
  "Start CPU (or printer device if argument given)",
    "Entering the 'start' command by itself simply starts a stopped\n"
    "CPU, whereas 'start <devn>' presses the virtual start button on\n"
    "printer device <devn>.\n")

COMMAND("stop",      PANEL,         SYSCMDALL,          stop_cmd,
  "Stop CPU (or printer device if argument given)",
    "Entering the 'stop' command by itself simply stops a running\n"
    "CPU, whereas 'stop <devn>' presses the virtual stop button on\n"
    "printer device <devn>, usually causing an INTREQ.\n")

COMMAND("startall",  PANEL,         SYSCMDALL,          startall_cmd, 
  "Start all CPU's", 
    NULL)

COMMAND("stopall",   PANEL,         SYSCMDALL,          stopall_cmd,  
  "Stop all CPU's", 
    NULL)

#ifdef _FEATURE_CPU_RECONFIG
COMMAND("cf",        PANEL,         SYSCMDALL,          cf_cmd,
  "Configure current CPU online or offline",
    "Configure current CPU online or offline:  Format->  \"cf [on|off]\"\n"
    "Where the 'current' CPU is defined as whatever CPU was defined as\n"
    "the panel command target cpu via the \"cpu\" panel command. (Refer\n"
    "to the 'cpu' command for further information) Entering 'cf' by itself\n"
    "simply displays the current online/offline status of the current cpu.\n"
    "Otherwise the current cpu is configured online or offline as\n"
    "specified.\n"
    "Use 'cfall' to configure/display all CPUs online/offline state.\n")

COMMAND("cfall",     PANEL,         SYSCMDALL,          cfall_cmd,    
  "Configure all CPU's online or offline", 
     NULL)
#endif

#ifdef _FEATURE_SYSTEM_CONSOLE
COMMAND("scpimply",   PANEL+CONFIG,  SYSCMDALL,          scpimply_cmd,  
  "Toggle on/off passing non-hercules commands to the scp",
    "scpimply toggles passing non-hercules commands to the scp if the scp\n"
    "has enabled receipt of scp commands. The default is off. If on,\n"
    "'invalid' hercules commands are passed to the scp.\n")

COMMAND("scpecho",   PANEL+CONFIG,  SYSCMDALL,          scpecho_cmd,  
  "Toggle on/off echo to console and history of scp replys",
    "scpecho toggles the '.' (scp) and '!' (priority scp) replys and\n"
    "responses to the hercules console. The default is off. This is to\n"
    "manage passwords being displayed and journaled.\n")
    
COMMAND(".reply",    PANEL,         SYSCMDALL,          g_cmd,
  "SCP command",
    "To reply to a system control program (i.e. guest operating system)\n"
    "message that gets issued to the hercules console, prefix the reply\n"
    "with a period.\n")

COMMAND("!message",  PANEL,         SYSCMDALL,          g_cmd,
  "SCP priority messsage",
    "To enter a system control program (i.e. guest operating system)\n"
    "priority command on the hercules console, simply prefix the command\n"
    "with an exclamation point '!'.\n")

COMMAND("ssd",       PANEL,         SYSCMDALL,          ssd_cmd,
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
COMMAND("ptt",       PANEL+CONFIG,  SYSCMDALL-SYSOPER, EXT_CMD(ptt_cmd),
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

COMMAND("i",         PANEL,         SYSCMDALL,          i_cmd,        
  "Generate I/O attention interrupt for device", 
    NULL)

COMMAND("ext",       PANEL,         SYSCMDALL,          ext_cmd,      
  "Generate external interrupt", 
    NULL)

COMMAND("restart",   PANEL,         SYSCMDALL,          restart_cmd,  
  "Generate restart interrupt", 
    NULL)

COMMAND("archlvl",  PANEL+CONFIG,  SYSCMDALL-SYSOPER, archlvl_cmd,
  "Set Architecture Level",
    "Format: archlvl s/370|als0 | esa/390|als1 | esame|als2 | z/arch|als3\n"
    "                enable|disable <facility> [s/370|esa/390|z/arch]\n"
    "                query [<facility> | all]\n"
    "command without any argument simply displays the current architecture\n"
    "mode. Entering the command with an argument sets the architecture mode\n"
    "to the specified value.\n")

COMMAND("archmode",  PANEL+CONFIG,  SYSCMDALL-SYSOPER, archlvl_cmd, 
  "Alias for archlvl", 
    NULL)

COMMAND("engines",   CONFIG,        SYSCMDALL,          engines_cmd, 
  "Set engines parameter",
    NULL)

COMMAND("sysepoch",  CONFIG,        SYSCMDALL,          sysepoch_cmd, 
  "Set sysepoch parameter",
    NULL)

COMMAND("tzoffset",  CONFIG,        SYSCMDALL,          tzoffset_cmd, 
  "Set tzoffset parameter",
    NULL)

COMMAND("yroffset",  CONFIG,        SYSCMDALL,          yroffset_cmd, 
  "Set yroffset parameter",
    NULL)

COMMAND("mainsize",  CONFIG,        SYSCMDALL,          mainsize_cmd, 
  "Set mainsize parameter",
    NULL)

COMMAND("xpndsize",  CONFIG,        SYSCMDALL,          xpndsize_cmd, 
  "Set xpndsize parameter",
    NULL)

COMMAND("hercprio",  CONFIG,        SYSCMDALL,          hercprio_cmd, 
  "Set hercprio parameter",
    NULL)

COMMAND("cpuprio",   CONFIG,        SYSCMDALL,          cpuprio_cmd, 
  "Set cpuprio parameter",
    NULL)

COMMAND("devprio",   CONFIG,        SYSCMDALL,          devprio_cmd, 
  "Set devprio parameter",
    NULL)

COMMAND("todprio",   CONFIG,        SYSCMDALL,          todprio_cmd, 
  "Set todprio parameter",
    NULL)

COMMAND("numvec",    CONFIG,        SYSCMDALL,          numvec_cmd, 
  "Set numvec parameter",
    NULL)

COMMAND("numcpu",    CONFIG,        SYSCMDALL,          numcpu_cmd, 
  "Set numcpu parameter",
    NULL)

COMMAND("maxcpu",    CONFIG,        SYSCMDALL,          maxcpu_cmd, 
  "Set maxcpu parameter",
    NULL)

COMMAND("loadparm",  PANEL+CONFIG,  SYSCMDALL,          loadparm_cmd, 
  "Set IPL parameter", 
    NULL)

COMMAND("lparname",  PANEL+CONFIG,  SYSCMDALL,          lparname_cmd, 
  "Set LPAR name", 
    NULL)

COMMAND("cpuverid",  CONFIG,        SYSCMDALL,          cpuverid_cmd,    
  "Set CPU verion number",
    NULL)

COMMAND("cpumodel",  CONFIG,        SYSCMDALL,          cpumodel_cmd,    
  "Set CPU model number",
    NULL)

COMMAND("cpuserial", CONFIG,        SYSCMDALL,          cpuserial_cmd,    
  "Set CPU serial number",
    NULL)

COMMAND("lparnum",   PANEL+CONFIG,  SYSCMDALL,          lparnum_cmd,  
  "Set LPAR identification number", 
    NULL)

COMMAND("cpuidfmt",  PANEL+CONFIG,  SYSCMDALL,          cpuidfmt_cmd, 
  "Set format 0/1 STIDP generation", 
    NULL)

COMMAND("cnslport",  CONFIG,        SYSNONE,            cnslport_cmd,
  "Set console port", 
    NULL)

#ifdef OPTION_CAPPING
COMMAND("capping",   CONFIG,        SYSNONE,            capping_cmd,
  "Set capping value", 
    NULL)
#endif // OPTION_CAPPING

#if defined(OPTION_SHARED_DEVICES)
COMMAND("shrdport",  CONFIG,        SYSNONE,            shrdport_cmd,
  "Set shrdport value", 
    NULL)
#endif /*defined(OPTION_SHARED_DEVICES)*/

#if defined(OPTION_SET_STSI_INFO)
COMMAND("model",     CONFIG,        SYSCMDALL,          stsi_model_cmd,
  "Set STSI model code", 
    NULL)

COMMAND("plant",     CONFIG,        SYSCMDALL,          stsi_plant_cmd,
  "Set STSI plant code", 
    NULL)

COMMAND("manufacturer",CONFIG,      SYSCMDALL,          stsi_mfct_cmd,
  "Set STSI manufacturer code", 
    NULL)
#endif /* defined(OPTION_SET_STSI_INFO) */

COMMAND("pgmprdos",  CONFIG,        SYSCMDALL,          pgmprdos_cmd, 
  "Set LPP license setting", 
    NULL)

COMMAND("codepage",  PANEL+CONFIG,  SYSCMDALL,          codepage_cmd, 
  "Set/display codepage conversion table", 
    "Format: \"codepage [cp]\"\n"
    "If no operand is specified, the current codepage is displayed.\n"
    "If 'cp' is specified, then codepage is set to the specified page\n"
    "if the page is valid. Type 'qcodepages' for a list of valid 'cp'\n"
    "operands.\n")

COMMAND("diag8cmd",  CONFIG,        SYSCMDALL,          diag8_cmd,    
  "Set diag8 command option", 
    NULL)

// The shcmdopt config statement should never be a command as 
// it will introduce a possible integrity exposure *JJ
COMMAND("shcmdopt",  CONFIG,        SYSCMDALL,          shcmdopt_cmd,
  "Set diag8 sh option", 
    NULL)

CMDABBR("legacysenseid",9,CONFIG,   SYSCMDALL,          lsid_cmd,    
  "Set legacysenseid setting", 
    NULL)

COMMAND("ipl",       PANEL,         SYSCMDALL,          ipl_cmd,
  "IPL Normal from device xxxx",
    "Format: \"ipl xxxx | cccc [loadparm xxxxnnnn | parm xxxxxxxxxxxxxx]\"\n"
    "Performs the Initial Program Load manual control function. If the\n" 
    "first operand 'xxxx' is a 1- to 4-digit hexadecimal number, a\n"
    "CCW-type IPL is initiated from the indicated device number, and\n"
    "SCLP disk I/O is disabled. Otherwise a list-directed IPL is performed\n"
    "from the .ins file named 'cccc', and SCLP disk I/O is enabled for the\n"
    "directory path where the .ins file is located.\n\n"
    "An optional 'loadparm' keyword followed by a 1-8 character string can\n"
    "be used to set the LOADPARM prior to the IPL.\n\n"
    "An optional 'parm' keyword followed by a string can also be passed to\n"
    "the IPL command processor. The string will be loaded into the\n"
    "low-order 32 bits of the general purpose registers (4 characters per\n"
    "register for up to 64 bytes). The PARM option behaves similarly to\n"
    "the VM IPL command.\n")

COMMAND("iplc",      PANEL,         SYSCMDALL,          iplc_cmd,
  "IPL Clear from device xxxx",
    "Performs the Load Clear manual control function. See \"ipl\".\n")

COMMAND("sysreset",  PANEL,         SYSCMDALL,          sysr_cmd,
  "SYSTEM Reset manual operation",
    "Performs the System Reset manual control function. A CPU and I/O\n"
    "subsystem reset are performed.\n")

COMMAND("sysclear",  PANEL,         SYSCMDALL,          sysc_cmd,
  "SYSTEM Clear Reset manual operation",
    "Performs the System Reset Clear manual control function. Same as\n"
    "the \"sysreset\" command but also clears main storage to 0. Also,\n"
    "registers control registers, etc.. are reset to their initial value.\n"
    "At this point, the system is essentially in the same state as it was\n"
    "just after having been started\n")

COMMAND("store",     PANEL,         SYSCMDALL,          store_cmd,    
  "Store CPU status at absolute zero", 
    NULL)

COMMAND("sclproot",  PANEL+CONFIG,  SYSCMDALL,          sclproot_cmd,
  "Set SCLP base directory",
    "Format: \"sclproot [path|NONE]\"\n"
    "Enables SCLP disk I/O for the specified directory path, or disables\n"
    "SCLP disk I/O if NONE is specified. A subsequent list-directed IPL\n"
    "resets the path to the location of the .ins file, and a CCW-type IPL\n"
    "disables SCLP disk I/O. If no operand is specified, sclproot displays\n"
    "the current setting.\n")

#if defined(OPTION_HTTP_SERVER)
COMMAND("httproot",  CONFIG,        SYSCMDALL,          httproot_cmd, 
  "Set HTTP server root directory", 
    NULL)

COMMAND("httpport",  CONFIG,        SYSCMDALL,          httpport_cmd, 
  "Set HTTP server port", 
    NULL)

#if defined( HTTP_SERVER_CONNECT_KLUDGE )
COMMAND("HTTP_SERVER_CONNECT_KLUDGE", CONFIG, SYSCMDALL, httpskm_cmd, 
  "HTTP_SERVER_CONNECT_KLUDGE", 
    NULL)
#endif // defined( HTTP_SERVER_CONNECT_KLUDGE )
#endif /*defined(OPTION_HTTP_SERVER)*/

COMMAND("psw",       PANEL,         SYSCMDALL-SYSOPER,  psw_cmd,
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

COMMAND("gpr",       PANEL,         SYSCMDALL-SYSOPER,  gpr_cmd,
  "Display or alter general purpose registers",
    "Format: \"gpr [nn=xxxxxxxxxxxxxxxx]\" where 'nn' is the optional\n"
    "register number (0 to 15) and 'xxxxxxxxxxxxxxxx' is the register\n"
    "value in hexadecimal (1-8 hex digits for 32-bit registers or 1-16 hex\n"
    "digits for 64-bit registers). Enter \"gpr\" by itself to display the\n"
    "register values without altering them.\n")

COMMAND("fpr",       PANEL,         SYSCMDALL-SYSOPER,  fpr_cmd,       
  "Display floating point registers", 
    NULL)

COMMAND("fpc",       PANEL,         SYSCMDALL-SYSOPER,  fpc_cmd,       
  "Display floating point control register", 
    NULL)

COMMAND("cr",        PANEL,         SYSCMDALL-SYSOPER,  cr_cmd,
  "Display or alter control registers",
    "Format: \"cr [nn=xxxxxxxxxxxxxxxx]\" where 'nn' is the optional\n"
    "control register number (0 to 15) and 'xxxxxxxxxxxxxxxx' is the\n"
    "control register value in hex (1-8 hex digits for 32-bit registers\n"
    "or 1-16 hex digits for 64-bit registers). Enter \"cr\" by itself to\n"
    "display the control registers without altering them.\n")

COMMAND("ar",        PANEL,         SYSCMDALL-SYSOPER,  ar_cmd,        
  "Display access registers", 
    NULL)

COMMAND("pr",        PANEL,         SYSCMDALL-SYSOPER,  pr_cmd,        
  "Display prefix register", 
    NULL)

COMMAND("timerint",  PANEL+CONFIG,  SYSCMDALL-SYSOPER,  timerint_cmd,  
  "Display or set timers update interval", 
    NULL)

COMMAND("clocks",    PANEL,         SYSCMDALL-SYSOPER,  clocks_cmd,    
  "Display tod clkc and cpu timer", 
    NULL)

COMMAND("ipending",  PANEL,         SYSCMDALL-SYSOPER,  ipending_cmd,  
  "Display pending interrupts", 
    NULL )

COMMAND("ds",        PANEL,         SYSCMDALL-SYSOPER,  ds_cmd,        
  "Display subchannel", 
    NULL)

COMMAND("r",         PANEL,         SYSCMDALL-SYSOPER,  r_cmd,
  "Display or alter real storage",
    "Format: \"r addr[.len]\" or \"r addr-addr\" to display real\n"
    "storage, or \"r addr=value\" to alter real storage, where 'value'\n"
    "is a hex string of up to 32 pairs of digits.\n")

COMMAND("v",         PANEL,         SYSCMDALL-SYSOPER,  v_cmd,
  "Display or alter virtual storage",
    "Format: \"v [P|S|H] addr[.len]\" or \"v [P|S|H] addr-addr\" to display\n"
    "virtual storage, or \"v [P|S|H] addr=value\" to alter virtual storage,\n"
    "where 'value' is a hex string of up to 32 pairs of digits. The\n"
    "optional 'P' or 'S' or 'H' will force Primary, Secondary, or Home\n"
    "translation instead of current PSW mode.\n")

COMMAND("u",         PANEL,         SYSCMDALL-SYSOPER,  u_cmd,         
  "Disassemble storage", 
    NULL)

COMMAND("devtmax",   PANEL+CONFIG,  SYSCMDALL-SYSOPER,  devtmax_cmd,   
  "Display or set max device threads", 
    NULL)

COMMAND("k",         PANEL,         SYSCMDALL-SYSOPER,  k_cmd,         
  "Display cckd internal trace", 
    NULL)

COMMAND("attach",    PANEL+CONFIG,  SYSCMDALL,          attach_cmd,
  "Configure device",
    "Format: \"attach devn type [arg...]\n")

COMMAND("detach",    PANEL,         SYSCMDALL,          detach_cmd,    
  "Remove device", 
    NULL)

COMMAND("define",    PANEL,         SYSCMDALL,          define_cmd,
  "Rename device",
    "Format: \"define olddevn newdevn\"\n")

COMMAND("devinit",   PANEL,         SYSCMDALL,          devinit_cmd,
  "Reinitialize device",
    "Format: \"devinit devn [arg...]\"\n"
    "If no arguments are given then the same arguments are used\n"
    "as were used the last time the device was created/initialized.\n")

COMMAND("devlist",   PANEL,         SYSCMDALL,          devlist_cmd,   
  "List device, device class, or all devices", 
    "Format: \"devlist [devn | devc]\"\n"
    "    devn       is a single device address\n"
    "    devc       is a single device class. Device classes are CON,\n"
    "               CTCA, DASD, DSP, LINE, PCH, PRT, QETH, RDR, and TAPE.\n"
    "\n"
    "If no arguments are given then all devices will be listed.\n" )

COMMAND("qd",        PANEL,         SYSCMDALL-SYSOPER,  qd_cmd,        
  "Query dasd", 
    NULL )

COMMAND("fcb",       PANEL,         SYSCMDALL,          fcb_cmd,      
  "Display the current FCB (if only the printer is given)",
   "Reset the fcb to the standard one \n" 
   "Load a fcb image \n")

CMDABBR("qcodepages",5,PANEL,       SYSCMDALL,          qcodepage_cmd,
  "Display list of valid codepages",
  "Display codepages currently available for selection\n")

CMDABBR("qcpuid",5,  PANEL,         SYSCMDALL,          qcpuid_cmd,
  "Display cpuid",
  "Display cpuid and STSI results presented to the SCP\n")

CMDABBR("qlpar",5,   PANEL,         SYSCMDALL,          qlpar_cmd,
  "Display lpar name and number",
  "Display LPAR name and number presented to the SCP\n")

#if        defined( OPTION_CONFIG_SYMBOLS )
CMDABBR("qpfkeys",3, PANEL,         SYSCMDALL,          qpfkeys_cmd,
  "Display pf keys",
  "Display the current PF Key settings\n")
#endif  // defined( OPTION_CONFIG_SYMBOLS )

CMDABBR("qpid",4,    PANEL,         SYSCMDALL,          qpid_cmd,
  "Display PID",
  "Display Process ID of Hercules\n")

CMDABBR("qports",5,  PANEL,         SYSCMDALL,          qports_cmd,
  "Display ports",
  "Display TCP/IP ports in use\n")

CMDABBR("qproc",5,   PANEL,         SYSCMDALL,          qproc_cmd,
  "Display processors",
  "Display processors type and utilization\n")

CMDABBR("qstor",5,   PANEL,         SYSCMDALL,          qstor_cmd,
  "Display storage",
  "Display main and expanded storage values\n")

CMDABBR("mounted_tape_reinit",9, PANEL+CONFIG, SYSCMDALL-SYSOPER, mnttapri_cmd,  
  "Control tape initialization", 
  "Format: \"mounted_tape_reinit [disallow|allow]\"\n"
  "Specifies whether reinitialization of tape drive devices\n"
  "(via the devinit command, in order to mount a new tape)\n"
  "should be allowed if there is already a tape mounted on\n"
  "the drive. The current value is displayed if no operand is\n"
  "specified\n" 
  "Specifying ALLOW (the default) indicates new tapes may be\n"
  "mounted (via 'devinit nnnn new-tape-filename') irrespective\n"
  "of whether or not there is already a tape mounted on the drive.\n" 
  "Specifying DISALLOW prevents new tapes from being mounted if\n"
  "one is already mounted. When DISALLOW is specified and a tape\n"
  "is already mounted on the drive, it must first be unmounted\n"
  "(via the command 'devinit nnnn *') before the new tape can be\n"
  "mounted. Otherwise the devinit attempt to mount the new tape\n"
  "is rejected.\n")

CMDABBR("autoinit",8, PANEL+CONFIG, SYSCMDALL-SYSOPER, autoinit_cmd,  
  "Display/Set automatic create empty tape file switch", 
  "Format: \"autoinit [on|off]\"\n"
  "Default for autoinit is off.\n"
  "When autoinit is off, devinit will return a file not found error\n"
  "if the specified file is not found.\n"
  "When autoinit is on, devinit will initialize a blank, non-labeled\n"
  "tape if the requested file is not found. Tape will have two tapemarks\n"
  "and be positioned at the beginning of the tape.")

#if defined( OPTION_TAPE_AUTOMOUNT )
COMMAND("automount", PANEL+CONFIG,  SYSCMDALL-SYSOPER,  automount_cmd,
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
COMMAND("auto_scsi_mount", PANEL+CONFIG, SYSCMDALL-SYSOPER, ascsimnt_cmd,  
  "Control SCSI tape mount", 
    NULL)

COMMAND("scsimount",       PANEL,   SYSCMDALL-SYSOPER,  scsimount_cmd,
  "Automatic SCSI tape mounts",
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
    "system). 'yes' is equivalent to specifying a 5 second interval.\n")
#endif /* defined( OPTION_SCSI_TAPE ) */

COMMAND("mt",          PANEL,      SYSCMDALL,          mt_cmd,
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

COMMAND("cd",        PANEL,         SYSCMDALL,          cd_cmd,        
  "Change directory", 
    NULL)

COMMAND("pwd",       PANEL,         SYSCMDALL,          pwd_cmd,       
  "Print working directory", 
    NULL)

#if defined( _MSVC_ )
COMMAND("dir",       PANEL,         SYSCMDALL,          dir_cmd,       
  "Displays a list of files and subdirs in a directory", 
    NULL)
#else
COMMAND("ls",        PANEL,         SYSCMDALL,          ls_cmd,       
  "List directory contents", 
    NULL)
#endif
COMMAND("sh",        PANEL,         SYSCMDALL-SYSOPER,  sh_cmd,
  "Shell command",
    "Format: \"sh command [args...]\" where 'command' is any valid shell\n"
    "command. The entered command and any arguments are passed as-is to the\n"
    "shell for processing and the results are displayed on the console.\n")

#if defined(HAVE_REGINA_REXXSAA_H)
COMMAND("exec",      PANEL,         SYSCMDALL,          exec_cmd,       
  "Execute a REXX script", 
    "Format: \"exec rexx_exec [args...]\" where 'rexx_exec' is the name of\n"
    "the REXX script, and 'args' are arguments (separated by spaces) to be\n"
    "passed to the script.\n")
#endif /*defined(HAVE_REGINA_REXXSAA_H)*/

COMMAND("cache",     PANEL,         SYSCMDALL-SYSOPER,  EXT_CMD(cache_cmd), 
  "Cache command", 
    NULL)

COMMAND("cckd",      PANEL+CONFIG,  SYSCMDALL-SYSOPER,  cckd_cmd,       
  "cckd command", 
    NULL)

COMMAND("shrd",      PANEL,         SYSCMDALL-SYSOPER,  EXT_CMD(shared_cmd), 
  "shrd command", 
    NULL)

COMMAND("conkpalv",  PANEL+CONFIG,  SYSCMDALL-SYSOPER,  conkpalv_cmd,
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

COMMAND("quiet",     PANEL,         SYSCMDALL-SYSOPER,  quiet_cmd,
  "Toggle automatic refresh of panel display data",
    "'quiet' either disables automatic screen refreshing if it is\n"
    "currently enabled or enables it if it is currently disabled.\n"
    "When disabled you will no be able to see the response of any\n"
    "entered commands nor any messages issued by the system nor be\n"
    "able to scroll the display, etc. Basically all screen updating\n"
    "is disabled. Entering 'quiet' again re-enables screen updating.\n")

COMMAND("t",         PANEL,         SYSCMDALL-SYSOPER,  trace_cmd,
  "Instruction trace",
    "Format: \"t addr-addr\" or \"t addr:addr\" or \"t addr.length\"\n"
    "sets the instruction tracing range (which is totally separate from\n"
    "the instruction stepping and breaking range).\n"
    "With or without a range, the t command displays whether instruction\n"
    "tracing is on or off and the range if any.\n"
    "The t command by itself does not activate instruction tracing.\n"
    "Use the t+ command to activate instruction tracing.\n"
    "\"t 0\" eliminates the range (all addresses will be traced).\n")

COMMAND("t+",        PANEL,         SYSCMDALL-SYSOPER,  trace_cmd,
  "Instruction trace on",
    "Format: \"t+\" turns on instruction tracing. A range can be specified\n"
    "as for the \"t\" command, otherwise the existing range is used. If there\n"
    "is no range (or range was specified as 0) then all instructions will\n"
    "be traced.\n")

COMMAND("t-",        PANEL,         SYSCMDALL-SYSOPER,  trace_cmd,
  "Instruction trace off",
    "Format: \"t-\" turns off instruction tracing.\n")

COMMAND("t?",        PANEL,         SYSCMDALL-SYSOPER,  trace_cmd,
  "Instruction trace query",
    "Format: \"t?\" displays whether instruction tracing is on or off\n"
    "and the range if any.\n")

COMMAND("s",         PANEL,         SYSCMDALL-SYSOPER,  trace_cmd,
  "Instruction stepping",
    "Format: \"s addr-addr\" or \"s addr:addr\" or \"s addr.length\"\n"
    "sets the instruction stepping and instruction breaking range,\n"
    "(which is totally separate from the instruction tracing range).\n"
    "With or without a range, the s command displays whether instruction\n"
    "stepping is on or off and the range if any.\n"
    "The s command by itself does not activate instruction stepping.\n"
    "Use the s+ command to activate instruction stepping.\n"
    "\"s 0\" eliminates the range (all addresses will be stepped).\n")

COMMAND("s+",        PANEL,         SYSCMDALL-SYSOPER,  trace_cmd,
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

COMMAND("s-",        PANEL,         SYSCMDALL-SYSOPER,  trace_cmd,
  "Instruction stepping off",
    "Format: \"s-\" turns off instruction stepping.\n")

COMMAND("s?",        PANEL,         SYSCMDALL-SYSOPER,  trace_cmd,
  "Instruction stepping query",
    "Format: \"s?\" displays whether instruction stepping is on or off\n"
    "and the range if any.\n")

COMMAND("b",         PANEL,         SYSCMDALL-SYSOPER,  trace_cmd,
  "Set breakpoint",
    "Format: \"b addr\" or \"b addr-addr\" where 'addr' is the instruction\n"
    "address or range of addresses where you wish to halt execution. This\n"
    "command is synonymous with the \"s+\" command.\n")

COMMAND("b+",        PANEL,         SYSCMDALL-SYSOPER,  trace_cmd,    
  "Set breakpoint", 
    NULL)

COMMAND("b-",        PANEL,         SYSCMDALL-SYSOPER,  trace_cmd,
  "Delete breakpoint",
    "Format: \"b-\"  This command is the same as \"s-\"\n")

COMMAND("g",         PANEL,         SYSCMDALL-SYSOPER,  g_cmd,        
  "Turn off instruction stepping and start all CPUs", 
    NULL)

COMMAND("ostailor",  PANEL+CONFIG,  SYSCMDALL-SYSOPER,  ostailor_cmd,
  "Tailor trace information for specific OS",
  "Format: \"ostailor [quiet | os/390 | z/os | vm | vse | linux | null]\".\n"
    "Specifies the intended operating system. The effect is to reduce\n"
    "control panel message traffic by selectively suppressing program\n"
    "check trace messages which are considered normal in the specified\n"
    "environment. The option 'quiet' suppresses all exception messages,\n"
    "whereas 'null' suppresses none of them. The other options suppress\n"
    "some messages and not others depending on the specified o/s. Prefix\n"
    "values with '+' to combine them with existing values or '-' to exclude\n"
    "them. SEE ALSO the 'pgmtrace' command which allows you to further fine\n"
    "tune the tracing of program interrupt exceptions.\n")

COMMAND("pgmtrace",  PANEL,         SYSCMDALL-SYSOPER,  pgmtrace_cmd,
  "Trace program interrupts",
    "Format: \"pgmtrace [-]intcode\" where 'intcode' is any valid program\n"
    "interruption code in the range 0x01 to 0x40. Precede the interrupt\n"
    "code with a '-' to stop tracing of that particular program\n"
    "interruption.\n")

COMMAND("savecore",  PANEL,         SYSCMDALL-SYSOPER,  savecore_cmd,
  "Save a core image to file",
    "Format: \"savecore filename [{start|*}] [{end|*}]\" where 'start' and\n"
    "'end' define the starting and ending addresss of the range of real\n"
    "storage to be saved to file 'filename'. An '*' for either the start\n"
    "address or end address (the default) means: \"the first/last byte of\n"
    "the first/last modified page as determined by the storage-key\n"
    "'changed' bit\".\n")

COMMAND("loadcore",  PANEL,         SYSCMDALL-SYSOPER,  loadcore_cmd,
  "Load a core image file",
    "Format: \"loadcore filename [address]\" where 'address' is the storage\n"
    "address of where to begin loading memory. The file 'filename' is\n"
    "presumed to be a pure binary image file previously created via the\n"
    "'savecore' command. The default for 'address' is 0 (beginning of\n"
    "storage).\n")

COMMAND("loadtext",  PANEL,         SYSCMDALL-SYSOPER,  loadtext_cmd,
  "Load a text deck file",
    "Format: \"loadtext filename [address]\". This command is essentially\n"
    "identical to the 'loadcore' command except that it loads a text deck\n"
    "file with \"TXT\" and \"END\" 80 byte records (i.e. an object deck).\n")

#if defined(OPTION_DYNAMIC_LOAD)
COMMAND("modpath",   CONFIG,        SYSCMDALL-SYSOPER,  modpath_cmd,  
  "Set module load path", 
    NULL)

COMMAND("ldmod",     PANEL+CONFIG,  SYSCMDALL-SYSOPER,  ldmod_cmd,    
  "Load a module", 
    NULL)

COMMAND("rmmod",     PANEL,         SYSCMDALL-SYSOPER,  rmmod_cmd,    
  "Delete a module", 
    NULL)

COMMAND("lsmod",     PANEL,         SYSCMDALL-SYSOPER,  lsmod_cmd,    
  "List dynamic modules", 
    NULL)

COMMAND("lsdep",     PANEL,         SYSCMDALL-SYSOPER,  lsdep_cmd,    
  "List module dependencies", 
    NULL)
#endif /*defined(OPTION_DYNAMIC_LOAD)*/

#ifdef OPTION_IODELAY_KLUDGE
COMMAND("iodelay",   PANEL+CONFIG,  SYSCMDALL-SYSOPER,  iodelay_cmd,   
  "Display or set I/O delay value", 
    NULL)
#endif

COMMAND("ctc",       PANEL,         SYSCMDALL-SYSOPER,  ctc_cmd,
  "Enable/Disable CTC debugging",
    "Format:  \"ctc  debug  { on | off }  [ <devnum> | ALL ]\".\n\n"
    "Enables/disables debug packet tracing for the specified CTCI/LCS\n"
    "device group(s) identified by <devnum> or for all CTCI/LCS device\n"
    "groups if <devnum> is not specified or specified as 'ALL'.\n")

#if defined(OPTION_W32_CTCI)
COMMAND("tt32",      PANEL,         SYSCMDALL-SYSOPER,  tt32_cmd,
  "Control/query CTCI-W32 functionality",
    "Format:  \"tt32   debug | nodebug | stats <devnum>\".\n"
    "\n"
    "Enables or disables global CTCI-W32 debug tracing\n"
    "or displays TunTap32 stats for the specified CTC device.\n")
#endif

COMMAND("toddrag",   PANEL+CONFIG,  SYSCMDALL-SYSOPER,  toddrag_cmd,  
  "Display or set TOD clock drag factor", 
    NULL)

#ifdef PANEL_REFRESH_RATE
COMMAND("panrate",   PANEL+CONFIG,  SYSCMDALL,          panrate_cmd,
  "Display or set rate at which console refreshes",
    "Format: \"panrate [nnn | fast | slow]\".\n"
    "Sets or displays the panel refresh rate.\n"
    "panrate nnn sets the refresh rate to nnn milliseconds.\n"
    "panrate fast sets the refresh rate to " MSTRING(PANEL_REFRESH_RATE_FAST) " milliseconds.\n"
    "panrate slow sets the refresh rate to " MSTRING(PANEL_REFRESH_RATE_SLOW) " milliseconds.\n"
    "If no operand is specified, panrate displays the current refresh rate.\n")
#endif

COMMAND("pantitle",  PANEL+CONFIG,        SYSCMDALL,          pantitle_cmd, 
  "Display or set console title", 
    "Format: pantitle [\"title string\"]\n"
    "Sets or displays the optional console window title-bar\n"
    "string to be used in place of the default supplied by\n"
    "the windowing system. The value should be enclosed within\n"
    "double quotes if there are embedded blanks.\n")
 
#ifdef OPTION_MSGHLD
COMMAND("kd",        PANEL,         SYSCMDALL,          msghld_cmd, 
  "Short form of 'msghld clear'", 
    NULL)

COMMAND("msghld",    PANEL,         SYSCMDALL,          msghld_cmd,
  "Display or set the timeout of held messages",
    "Format: \"msghld [value | info | clear]\".\n"
    "value: timeout value of held message in seconds\n"
    "info:  displays the timeout value\n"
    "clear: releases the held messages\n")
#endif

COMMAND("syncio",    PANEL,         SYSCMDALL-SYSOPER,  syncio_cmd,    
  "Display syncio devices statistics", 
    NULL)

#if defined(OPTION_INSTRUCTION_COUNTING)
COMMAND("icount",    PANEL,         SYSCMDALL-SYSOPER,  icount_cmd,    
  "Display individual instruction counts", 
    NULL)
#endif

#ifdef OPTION_MIPS_COUNTING
COMMAND("maxrates",  PANEL,         SYSCMDALL,          maxrates_cmd,
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
COMMAND("asn_and_lx_reuse", CONFIG, SYSCMDALL-SYSOPER,  alrf_cmd, 
  "Command depricated:" 
  "Use \"archlvl enable|disable|query asn_lx_reuse\" instead\n",
    NULL)
    
COMMAND("alrf",      CONFIG,        SYSCMDALL-SYSOPER,  alrf_cmd, 
  "Alias for asn_and_lx_reuse\n", 
    NULL)
#endif /* defined(_FEATURE_ASN_AND_LX_REUSE) */

#if defined(FISH_HANG)
COMMAND("FishHangReport", PANEL,    SYSCMDALL-SYSOPER,  FishHangReport_cmd,
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
COMMAND("defsym",    PANEL+CONFIG,  SYSCMDALL-SYSOPER,  defsym_cmd,
  "Define symbol",
    "Format: \"defsym symbol [value]\". Defines symbol 'symbol' to contain\n"
    "value 'value'. The symbol can then be the object of a substitution for\n"
    "later panel commands. If 'value' contains blanks or spaces, then it\n"
    "must be enclosed within quotes or apostrophes. For more detailed\n"
    "information regarding symbol substitution refer to the 'DEFSYM'\n"
    "configuration file statement in Hercules documentation.\n"
    "Enter \"defsym\" by itself to display the values of all defined\n"
    "symbols.\n")
#endif

COMMAND("script",    PANEL,         SYSCMDALL-SYSOPER,  script_cmd,
  "Run a sequence of panel commands contained in a file",
    "Format: \"script filename [...filename...]\". Sequentially executes\n"
    "the commands contained within the file 'filename'. The script file may\n"
    "also contain \"script\" commands, but the system ensures that no more\n"
    "than 10 levels of script are invoked at any one time.\n")

COMMAND("cscript",   PANEL,         SYSCMDALL-SYSOPER,  cscript_cmd,
  "Cancels a running script thread",
    "Format: \"cscript\". This command will cancel the currently running\n"
    "script. If no script is running, no action is taken.\n")

#if defined(FEATURE_ECPSVM)
COMMAND("evm",       PANEL+CONFIG,  SYSCMDALL-SYSOPER,  evm_cmd_1,
  "ECPS:VM Commands (Deprecated)",
    "Format: \"evm\". This command is deprecated.\n"
    "use \"ecpsvm\" instead\n")

COMMAND ( "ecpsvm",  PANEL+CONFIG,  SYSCMDALL-SYSOPER,  evm_cmd,
  "ECPS:VM Commands",
    "Format: \"ecpsvm\". This command invokes ECPS:VM Subcommands.\n"
    "Type \"ecpsvm help\" to see a list of available commands\n")
#endif

COMMAND("aea",       PANEL,         SYSCMDALL-SYSOPER,  aea_cmd,       
  "Display AEA tables", 
    NULL)
    
COMMAND("aia",       PANEL,         SYSCMDALL-SYSOPER,  aia_cmd,       
  "Display AIA fields", 
    NULL)
COMMAND("tlb",       PANEL,         SYSCMDALL-SYSOPER,  tlb_cmd,       
  "Display TLB tables", 
    NULL)

#if defined(SIE_DEBUG_PERFMON)
COMMAND("spm",       PANEL,         SYSCMDALL-SYSOPER,  spm_cmd,       
  "SIE performance monitor", 
    NULL)
#endif
#if defined(OPTION_COUNTING)
COMMAND("count",     PANEL,         SYSCMDALL-SYSOPER,  count_cmd,     
  "Display/clear overall instruction count", 
    NULL)
#endif
COMMAND("sizeof",    PANEL,         SYSCMDALL-SYSOPER-SYSPROG, sizeof_cmd,    
  "Display size of structures", 
    NULL)

COMMAND("suspend",   PANEL,         SYSCMDALL-SYSOPER,      suspend_cmd,   
        "Suspend hercules", NULL )

COMMAND("resume",    PANEL,         SYSCMDALL-SYSOPER,      resume_cmd,    
        "Resume hercules", NULL )

COMMAND("herclogo",  PANEL+CONFIG,  SYSCMDALL-SYSOPER,      herclogo_cmd,
  "Read a new hercules logo file",
    "Format: \"herclogo [<filename>]\". Load a new logo file for 3270\n"
    "terminal sessions. If no filename is specified, the built-in logo\n"
    "is used instead.\n"    )

COMMAND("traceopt",  PANEL+CONFIG,  SYSCMDALL-SYSOPER,  traceopt_cmd,
  "Instruction trace display options",
    "Format: \"traceopt [regsfirst | noregs | traditional]\". Determines how\n"
    "the registers are displayed during instruction tracing and stepping.\n"
    "Entering the command without any argument simply displays the current\n"
    "mode.\n" )

COMMAND("symptom",   CONFIG,        SYSCMDALL-SYSOPER,  traceopt_cmd, 
  "Alias for traceopt", 
    NULL)

COMMAND("$zapcmd",   CONFIG,        SYSDEBUG,           zapcmd_cmd,   
  NULL, 
    NULL)     // enable/disable commands and config statements

COMMAND("$test",     DISABLED,      SYSDEBUG,           test_cmd,     
  NULL, 
    NULL)     // enable in config with: $zapcmd $test cmd

#ifdef OPTION_CMDTGT
COMMAND("cmdtgt",    PANEL,         SYSCMDALL,          cmdtgt_cmd,
  "Specify the command target",
    "Format: \"cmdtgt [herc | scp | pscp | ?]\". Specify the command target.\n")

COMMAND("herc",      PANEL,         SYSCMDALL,          herc_cmd,
  "Hercules command",
    "Format: \"herc [cmd]\". Send hercules cmd in any cmdtgt mode.\n")

COMMAND("scp",       PANEL,         SYSCMDALL,          scp_cmd,
  "Send scp command",
    "Format: \"scp [cmd]\". Send scp cmd in any cmdtgt mode.\n")

COMMAND("pscp",      PANEL,         SYSCMDALL,          prioscp_cmd,
  "Send prio message scp command",
    "Format: \"pscp [cmd]\". Send priority message cmd to scp in any\n"
    "cmdtgt mode.\n")
#endif // OPTION_CMDTGT

// The actual command table ends here, the next entries are just for help
// as the associated command are processed as part of commandline parsing
// and there are no forward references to be created

#if !defined(_FW_REF)
COMMAND("sf+dev",    PANEL,         SYSCMDALL-SYSOPER,  NULL,         
  "Add shadow file", 
    NULL)

COMMAND("sf-dev",    PANEL,         SYSCMDALL-SYSOPER,  NULL,         
  "Delete shadow file", 
    NULL)

COMMAND("sfc",       PANEL,         SYSCMDALL-SYSOPER,  NULL,         
  "Compress shadow files", 
    NULL)

COMMAND("sfk",       PANEL,         SYSCMDALL-SYSOPER,  NULL,
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

COMMAND("sfd",       PANEL,         SYSCMDALL-SYSOPER,  NULL,         
  "Display shadow file stats", 
    NULL)

COMMAND("t{+/-}dev", PANEL,         SYSCMDALL-SYSOPER,  NULL,         
  "Turn CCW tracing on/off", 
    NULL)

COMMAND("s{+/-}dev", PANEL,         SYSCMDALL-SYSOPER,  NULL,         
  "Turn CCW stepping on/off", 
    NULL)

#ifdef OPTION_CKD_KEY_TRACING
COMMAND("t{+/-}CKD", PANEL,         SYSCMDALL-SYSOPER,  NULL,         
  "Turn CKD_KEY tracing on/off", 
    NULL)
#endif

COMMAND("f{+/-}adr", PANEL,         SYSCMDALL-SYSOPER,  NULL,         
  "Mark frames unusable/usable", 
    NULL)
#endif /*!defined(_FW_REF)*/

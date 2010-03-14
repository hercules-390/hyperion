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

/* bldcfg.c */
#define HHCCF001S "Error reading file %s line %d: %s"
#define HHCCF002S "File %s line %d is too long"
#define HHCCF003S "Open error file %s: %s"
#define HHCCF004S "No device records in file %s"
#define HHCCF008E "Error in %s line %d: Syntax error: %s"
#define HHCCF009E "Error in %s line %d: Incorrect number of operands"
#define HHCCF012S "Error in %s line %d: %s is not a valid CPU %s"
#define HHCCF013S "Error in %s line %d: Invalid main storage size %s"
#define HHCCF014S "Error in %s line %d: Invalid expanded storage size %s"
#define HHCCF016S "Error in %s line %d: Invalid %s thread priority %s"
#define HHCCF017W "Hercules is not running as setuid root, cannot raise %s thread priority"
#define HHCCF018S "Error in %s line %d: Invalid number of CPUs %s"
#define HHCCF019S "Error in %s line %d: Invalid number of VFs %s"
#define HHCCF020W "Vector Facility support not configured"
#define HHCCF022S "Error in %s line %d: %s is not a valid system epoch. The only valid values are 1801-2099"
#define HHCCF023S "Error in %s line %d: %s is not a valid timezone offset"
#define HHCCF029S "Error in %s line %d: Invalid SHRDPORT port number %s"
#define HHCCF031S "Cannot obtain %dMB main storage: %s"
#define HHCCF032S "Cannot obtain storage key array: %s"
#define HHCCF033S "Cannot obtain %dMB expanded storage: %s"
#define HHCCF034W "Expanded storage support not installed"
#define HHCCF035S "Error in %s line %d: Missing device number or device type"
#define HHCCF036S "Error in %s line %d: %s is not a valid device number(s) specification"
#define HHCCF051S "Error in %s line %d: %s is not a valid serial number"
#define HHCCF052W "Warning in %s line %d: Invalid ECPSVM level value: %s. 20 Assumed"
#define HHCCF053W "Error in %s line %d: Invalid ECPSVM keyword: %s NO Assumed"
#define HHCCF061W "Warning in %s line %d: %s statement deprecated. Use %s instead"
#define HHCCF062W "Warning in %s line %d: Missing ECPSVM level value, 20 Assumed"
#define HHCCF063W "Warning in %s line %d: Specifying ECPSVM level directly is deprecated. Use the 'LEVEL' keyword instead"
#define HHCCF064W "Hercules set priority %d failed: %s"
#define HHCCF065I "Thread started tid " TIDPAT ", pid %d, prio %d, name %s"
#define HHCCF070S "Error in %s line %d: %s is not a valid year offset"
#define HHCCF072W "SYSEPOCH %04d is deprecated, please specify \"SYSEPOCH 1900 %s%d\""
#define HHCCF073W "SYSEPOCH %04d is deprecated, please specify \"SYSEPOCH 1960 %s%d\""
#define HHCCF074S "Error in %s line %d: Invalid engine syntax %s"
#define HHCCF075S "Error in %s line %d: Invalid engine type %s"
#define HHCCF077I "Engine %d set to type %d (%s)"
#define HHCCF081I "%s will ignore include errors"
#define HHCCF082S "Error in %s line %d: Maximum nesting level (%d) reached"
#define HHCCF083I "%s including %s at %d"
#define HHCCF084W "%s open error ignored file %s: %s"
#define HHCCF085S "%s open error file %s: %s"
#define HHCCF086S "Error in %s: NUMCPU %d must not exceed MAXCPU %d"
#define HHCCF090I "Default Allowed AUTOMOUNT directory = \"%s\""
#define HHCCF900S "Out of memory"

/* console.c */
#define HHCGI001I "Unable to determine IP address from %s"
#define HHCGI002I "Unable to determine port number from %s"
#define HHCGI003E "Invalid parameter: %s"

#define HHCTE001I "Thread started tid " TIDPAT ", pid %d, prio %d, name %s"
#define HHCTE002W "Waiting for port %u to become free"
#define HHCTE003I "Waiting for console connection on port %u"
#define HHCTE004I "Thread ended tid " TIDPAT ", pid %d, prio %d, name %s" 
#define HHCTE005E "Cannot create console thread: %s"
#define HHCTE007I "%4.4X device %4.4X client %s connection closed"
#define HHCTE008I "Device %4.4X connection closed by client %s"
#define HHCTE009I "Client %s connected to %4.4X device %d:%4.4X"
#define HHCTE010E "CNSLPORT statement invalid: %s"
#define HHCTE011E "Device %4.4X: Invalid IP address: %s"
#define HHCTE012E "Device %4.4X: Invalid mask value: %s"
#define HHCTE013E "Device %4.4X: Extraneous argument(s): %s..."
#define HHCTE014I "%4.4X device %4.4X client %s connection reset"
#define HHCTE017E "Device %4.4X: Duplicate SYSG console definition"
#define HHCTE090E "%4.4X malloc() failed for resume buf: %s"

/* cpu.c */
#define HHCCP001W "%s%02X thread set priority %d failed: %s"
#define HHCCP002I "Thread started tid " TIDPAT ", pid %d, prio %d, name %s%02X"
#define HHCCP003I "%s%02X architecture mode %s"
#define HHCCP004I "%s%02X Vector Facility online"
#define HHCCP006S "Cannot create timer thread: %s"
#define HHCCP007I "%s%02X architecture mode set to %s"
#define HHCCP008I "Thread ended tid " TIDPAT ", pid %d, prio %d, name %s%02X"
#define HHCCP010I "%s%02X store status completed"
#define HHCCP011I "%s%02X disabled wait state %s"
#define HHCCP014I "%s%s%s02X: %s CODE %4.4X ILC %d%s"
#define HHCCP015I "%s%02X PER event: code %4.4X perc %2.2X addr " F_VADR
#define HHCCP016I "%s%02X: Program interrupt loop: PSW %s"
#define HHCCP022I "Machine Check code %16.16" I64_FMT "u"
#define HHCCP043I "Wait state PSW loaded: %s"
#define HHCCP044I "I/O interrupt code %4.4X CSW %2.2X%2.2X%2.2X%2.2X %2.2X%2.2X%2.2X%2.2X"
#define HHCCP045I "I/O interrupt code %8.8X parm %8.8X"
#define HHCCP046I "I/O interrupt code %8.8X parm %8.8X id %8.8X"
#define HHCCP080E "%s%02X malloc failed for archjmp regs: %s"

/* crypto/dyncrypt.c */
#define HHCRY001I "Crypto module loaded (c) Copyright Bernard van der Helm, 2003-2010"
#define HHCRY002I "Active: %s"

/* hao.c */
#define HHCIN004S "Cannot create HAO thread: %s"
#define HHCAO001I "Thread started tid " TIDPAT ", pid %d, prio %d, name %s"
#define HHCAO002I "Thread ended tid " TIDPAT ", pid %d, prio %d, name %s"
#define HHCAO003I "Firing command: '%s'"
#define HHCAO004I "The defined Automatic Operator rule(s) are:"
#define HHCAO005I "%02d: '%s' -> '%s'"
#define HHCAO006I "%d rule(s) displayed"
#define HHCAO007E "Unknown hao command, valid commands are:\n" \
                  "          hao tgt <tgt> : define target rule (pattern) to react on\n" \
                  "          hao cmd <cmd> : define command for previously defined rule\n" \
                  "          hao list <n>  : list all rules/commands or only at index <n>\n" \
                  "          hao del <n>   : delete the rule at index <n>\n" \
                  "          hao clear     : delete all rules (stops automatic operator)"
#define HHCAO008E "No rule defined at index %d"
#define HHCAO009E "Invalid index, index must be between 0 and %d"
#define HHCAO010E "Target not added, table full"
#define HHCAO011E "Tgt command given, but cmd command expected"
#define HHCAO012E "Empty target specified"
#define HHCAO013E "Target not added, duplicate found in table"
#define HHCAO014E "%s"
#define HHCAO015E "%s"
#define HHCAO016I "Target placed at index %d"
#define HHCAO017E "Cmd command given, but tgt command expected"
#define HHCAO018E "Empty command specified"
#define HHCAO019E "Command not added; causes loop with target at index %d"
#define HHCAO020I "Command placed at index %d"
#define HHCAO021E "Target not added, causes loop with command at index %d"
#define HHCAO022I "All automatic operation rules cleared"
#define HHCAO023E "Hao del command given without a valid index"
#define HHCAO024E "Rule at index %d not deleted, already empty"
#define HHCAO025I "Rule at index %d succesfully deleted"
#define HHCA0026E "Command not added, may cause dead locks"

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
#define HHCHT001I "Thread started tid " TIDPAT ", pid %d, prio %d, name %s"
#define HHCHT002E "Socket error: %s"
#define HHCHT003W "Waiting for port %u to become free"
#define HHCHT004E "Bind error: %s"
#define HHCHT005E "Listen error: %s"
#define HHCHT006I "Waiting for HTTP requests on port %u"
#define HHCHT007E "Select error: %s"
#define HHCHT008E "Accept error: %s"
#define HHCHT009I "Thread ended tid " TIDPAT ", pid %d, prio %d, name %s"
#define HHCHT010E "Http_request create_thread error: %s"
#define HHCHT011E "Html_include: Cannot open %s: %s"
#define HHCHT013I "Using HTTPROOT directory \"%s\""
#define HHCCF066E "Invalid HTTPROOT: \"%s\": %s"

/* panel.c */
#define HHCPN001I "Thread started tid " TIDPAT ", pid %d, prio %d, name %s"
#define HHCPN002S "Cannot obtain keyboard buffer: %s"
#define HHCPN003S "Cannot obtain message buffer: %s"

/* timer.c */
#define HHCTT001W "Timer thread set priority %d failed: %s"
#define HHCTT002I "Thread started tid " TIDPAT ", pid %d, prio %d, name %s"
#define HHCTT003I "Thread ended tid " TIDPAT ", pid %d, prio %d, name %s"

/* version.c */
#define HHCIN010I "%sversion %s"
#define HHCIN011I "%s"
#define HHCIN012I "Built on %s at %s"
#define HHCIN013I "Build information:"
#define HHCIN014I "  %s"

/* CGIBIN.C     (c)Copyright Jan Jaeger, 2002-2004                   */
/*              HTTP cgi-bin routines                                */

/* This file contains all cgi routines that may be executed on the   */
/* server (ie under control of a hercules thread)                    */
/*                                                                   */
/*                                                                   */
/* All cgi-bin routines are identified in the directory at the end   */
/* of this file                                                      */
/*                                                                   */
/*                                                                   */
/* The cgi-bin routines may call the following HTTP service routines */
/*                                                                   */
/* char *cgi_variable(WEBBLK *webblk, char *name);                   */
/*   This call returns a pointer to the cgi variable requested       */
/*   or a NULL pointer if the variable is not found                  */
/*                                                                   */
/* char *cgi_cookie(WEBBLK *webblk, char *name);                     */
/*   This call returns a pointer to the cookie requested             */
/*   or a NULL pointer if the cookie is not found                    */
/*                                                                   */
/* char *cgi_username(WEBBLK *webblk);                               */
/*   Returns the username for which the user has been authenticated  */
/*   or NULL if not authenticated (refer to auth/noauth parameter    */
/*   on the HTTPPORT configuration statement)                        */
/*                                                                   */
/* char *cgi_baseurl(WEBBLK *webblk);                                */
/*   Returns the url as requested by the user                        */
/*                                                                   */
/*                                                                   */
/* void html_header(WEBBLK *webblk);                                 */
/*   Sets up the standard html header, and includes the              */
/*   html/header.htmlpart file.                                      */
/*                                                                   */
/* void html_footer(WEBBLK *webblk);                                 */
/*   Sets up the standard html footer, and includes the              */
/*   html/footer.htmlpart file.                                      */
/*                                                                   */
/* int html_include(WEBBLK *webblk, char *filename);                 */
/*   Includes an html file                                           */
/*                                                                   */
/*                                                                   */
/*                                           Jan Jaeger - 28/03/2002 */
 

#include "hercules.h"
#include "devtype.h"
#include "opcode.h"
#include "httpmisc.h"

#if defined(OPTION_HTTP_SERVER)


void cgibin_reg_control(WEBBLK *webblk)
{
int i;

    REGS *regs = sysblk.regs + sysblk.pcpu;

    html_header(webblk);

    fprintf(webblk->hsock, "<H2>Control Registers</H2>\n");
    fprintf(webblk->hsock, "<PRE>\n");
    if(regs->arch_mode != ARCH_900)
        for (i = 0; i < 16; i++)
            fprintf(webblk->hsock, "CR%2.2d=%8.8X%s", i, regs->CR_L(i),
                ((i & 0x03) == 0x03) ? "\n" : "\t");
    else
        for (i = 0; i < 16; i++)
            fprintf(webblk->hsock, "CR%1.1X=%16.16llX%s", i,
                (long long)regs->CR_G(i), ((i & 0x03) == 0x03) ? "\n" : " ");           

    fprintf(webblk->hsock, "</PRE>\n");

    html_footer(webblk);

}


void cgibin_reg_general(WEBBLK *webblk)
{
int i;

    REGS *regs = sysblk.regs + sysblk.pcpu;

    html_header(webblk);

    fprintf(webblk->hsock, "<H2>General Registers</H2>\n");
    fprintf(webblk->hsock, "<PRE>\n");
    if(regs->arch_mode != ARCH_900)
        for (i = 0; i < 16; i++)
            fprintf(webblk->hsock, "GR%2.2d=%8.8X%s", i, regs->GR_L(i),
                ((i & 0x03) == 0x03) ? "\n" : "\t");
    else
        for (i = 0; i < 16; i++)
            fprintf(webblk->hsock, "GR%1.1X=%16.16llX%s", i,
                (long long)regs->GR_G(i), ((i & 0x03) == 0x03) ? "\n" : " ");           

    fprintf(webblk->hsock, "</PRE>\n");

    html_footer(webblk);

}


void store_psw (REGS *regs, BYTE *addr);

void cgibin_psw(WEBBLK *webblk)
{
    REGS *regs = sysblk.regs + sysblk.pcpu;
    QWORD   qword;                            /* quadword work area      */  

    char *value;
    int autorefresh=0;
    int refresh_interval=5;

    html_header(webblk);


    if (cgi_variable(webblk,"autorefresh"))
        autorefresh = 1;
    else if (cgi_variable(webblk,"norefresh"))
        autorefresh = 0;
    else if (cgi_variable(webblk,"refresh"))
        autorefresh = 1;

    if ((value = cgi_variable(webblk,"refresh_interval")))
        refresh_interval = atoi(value);

    fprintf(webblk->hsock, "<H2>Program Status Word</H2>\n");

    fprintf(webblk->hsock, "<FORM method=post>\n");

    if (!autorefresh)
    {
        fprintf(webblk->hsock, "<INPUT type=submit value=\"Auto Refresh\" name=autorefresh>\n");
        fprintf(webblk->hsock, "Refresh Interval: ");
        fprintf(webblk->hsock, "<INPUT type=text size=2 name=\"refresh_interval\" value=%d>\n", 
           refresh_interval);
    }
    else
    {
        fprintf(webblk->hsock, "<INPUT type=submit value=\"Stop Refreshing\" name=norefresh>\n");
        fprintf(webblk->hsock, "Refresh Interval: %d\n", refresh_interval);
        fprintf(webblk->hsock, "<INPUT type=hidden name=\"refresh_interval\" value=%d>\n",refresh_interval);
    }

    fprintf(webblk->hsock, "</FORM>\n");

    fprintf(webblk->hsock, "<P>\n");

    if( regs->arch_mode != ARCH_900 )
    {
        store_psw (regs, qword);
        fprintf(webblk->hsock, "PSW=%2.2X%2.2X%2.2X%2.2X %2.2X%2.2X%2.2X%2.2X\n",
                qword[0], qword[1], qword[2], qword[3],
                qword[4], qword[5], qword[6], qword[7]);
    }
    else
    {
        store_psw (regs, qword);
        fprintf(webblk->hsock, "PSW=%2.2X%2.2X%2.2X%2.2X %2.2X%2.2X%2.2X%2.2X "
                "%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X\n",
                qword[0], qword[1], qword[2], qword[3],
                qword[4], qword[5], qword[6], qword[7],
                qword[8], qword[9], qword[10], qword[11],
                qword[12], qword[13], qword[14], qword[15]);
    }
 
    if (autorefresh)
    {
        /* JavaScript to cause automatic page refresh */
        fprintf(webblk->hsock, "<script language=\"JavaScript\">\n");
        fprintf(webblk->hsock, "<!--\nsetTimeout('window.location.replace(\"%s?refresh_interval=%d&refresh=1\")', %d)\n", 
               cgi_baseurl(webblk),
               refresh_interval,
               refresh_interval*1000);
        fprintf(webblk->hsock, "//-->\n</script>\n");
    }

    html_footer(webblk);

}


void cgibin_syslog(WEBBLK *webblk)
{
int     msgcnt;
int     msgnum;
char   *msgbuf;

char   *command;

char   *value;
int     autorefresh = 0;
int     refresh_interval = 5;
int     msgcount = 22;

    if ((command = cgi_variable(webblk,"command")))
        panel_command(command);

    if((value = cgi_variable(webblk,"msgcount")))
        msgcount = atoi(value);
    else
        if((value = cgi_cookie(webblk,"msgcount")))
            msgcount = atoi(value);

    if ((value = cgi_variable(webblk,"refresh_interval")))
        refresh_interval = atoi(value);

    if (cgi_variable(webblk,"autorefresh"))
        autorefresh = 1;
    else if (cgi_variable(webblk,"norefresh"))
        autorefresh = 0;
    else if (cgi_variable(webblk,"refresh"))
        autorefresh = 1;

    html_header(webblk);

    fprintf(webblk->hsock,"<script language=\"JavaScript\">\n"
                          "<!--\n"
                          "document.cookie = \"msgcount=%d\";\n"
                          "//-->\n"
                          "</script>\n",
                          msgcount);

    fprintf(webblk->hsock, "<H2>Hercules System Log</H2>\n");
    fprintf(webblk->hsock, "<PRE>\n");

    msgnum = msgcount ? log_line(msgcount) : -1;
    while((msgcnt = log_read(&msgbuf, &msgnum, LOG_NOBLOCK)))
        fwrite(msgbuf,msgcnt,1,webblk->hsock);

    fprintf(webblk->hsock, "</PRE>\n");

    fprintf(webblk->hsock, "<FORM method=post>Command:\n");
    fprintf(webblk->hsock, "<INPUT type=text name=command size=80>\n");
    fprintf(webblk->hsock, "<INPUT type=submit name=send value=\"Send\">\n");
    fprintf(webblk->hsock, "<INPUT type=hidden name=%srefresh value=1>\n",autorefresh ? "auto" : "no");
    fprintf(webblk->hsock, "<INPUT type=hidden name=refresh_interval value=%d>\n",refresh_interval);
    fprintf(webblk->hsock, "<INPUT type=hidden name=msgcount value=%d>\n",msgcount);
    fprintf(webblk->hsock, "</FORM>\n<BR>\n");

    fprintf(webblk->hsock, "<A name=bottom>\n");

    fprintf(webblk->hsock, "<FORM method=post>\n");
    if(!autorefresh)
    {
        fprintf(webblk->hsock, "<INPUT type=submit value=\"Auto Refresh\" name=autorefresh>\n");
        fprintf(webblk->hsock, "Refresh Interval: ");
        fprintf(webblk->hsock, "<INPUT type=text name=\"refresh_interval\" size=2 value=%d>\n", 
           refresh_interval);
    }
    else
    {
        fprintf(webblk->hsock, "<INPUT type=submit name=norefresh value=\"Stop Refreshing\">\n");
        fprintf(webblk->hsock, "<INPUT type=hidden name=refresh_interval value=%d>\n",refresh_interval);
        fprintf(webblk->hsock, " Refresh Interval: %2d \n", refresh_interval);
    }
    fprintf(webblk->hsock, "<INPUT type=hidden name=msgcount value=%d>\n",msgcount);
    fprintf(webblk->hsock, "</FORM>\n");

    fprintf(webblk->hsock, "<FORM method=post>\n");
    fprintf(webblk->hsock, "Only show last ");
    fprintf(webblk->hsock, "<INPUT type=text name=msgcount size=3 value=%d>",msgcount);
    fprintf(webblk->hsock, " lines (zero for all loglines)\n");
    fprintf(webblk->hsock, "<INPUT type=hidden name=%srefresh value=1>\n",autorefresh ? "auto" : "no");
    fprintf(webblk->hsock, "<INPUT type=hidden name=refresh_interval value=%d>\n",refresh_interval);
    fprintf(webblk->hsock, "</FORM>\n");
    
    if (autorefresh)
    {
        /* JavaScript to cause automatic page refresh */
        fprintf(webblk->hsock, "<script language=\"JavaScript\">\n");
        fprintf(webblk->hsock, "<!--\nsetTimeout('window.location.replace(\"%s"
               "?refresh_interval=%d"
               "&refresh=1"
               "&msgcount=%d"
               "\")', %d)\n", 
               cgi_baseurl(webblk),
               refresh_interval,
               msgcount,
               refresh_interval*1000);
        fprintf(webblk->hsock, "//-->\n</script>\n");
    }

    html_footer(webblk);

}


void cgibin_debug_registers(WEBBLK *webblk)
{
int i, cpu = 0;
int select_gr, select_cr, select_ar;
char *value;
REGS *regs;

    if((value = cgi_variable(webblk,"cpu")))
        cpu = atoi(value);

    if((value = cgi_variable(webblk,"select_gr")) && *value == 'S')
        select_gr = 1;
    else
        select_gr = 0;

    if((value = cgi_variable(webblk,"select_cr")) && *value == 'S')
        select_cr = 1;
    else
        select_cr = 0;

    if((value = cgi_variable(webblk,"select_ar")) && *value == 'S')
        select_ar = 1;
    else
        select_ar = 0;

    /* Validate cpu number */
#if defined(_FEATURE_CPU_RECONFIG)
    if(cpu < 0 || cpu > MAX_CPU_ENGINES
       || !sysblk.regs[cpu].cpuonline)
#else
    if(cpu < 0 || cpu > sysblk.numcpu)
#endif
        for(cpu = 0;
#if defined(_FEATURE_CPU_RECONFIG)
            cpu < MAX_CPU_ENGINES;
#else
            cpu < sysblk.numcpu;
#endif
                cpu++)
            if(sysblk.regs[cpu].cpuonline)
                break;

    regs = sysblk.regs + cpu;

    if((value = cgi_variable(webblk,"alter_gr")) && *value == 'A')
    {
        for(i = 0; i < 16; i++)
        {
        char regname[16];
            sprintf(regname,"alter_gr%d",i);
            if((value = cgi_variable(webblk,regname)))
            {
                if(regs->arch_mode != ARCH_900)
                    sscanf(value,"%x",&(regs->GR_L(i)));
                else
                    sscanf(value,"%llx",&(regs->GR_G(i)));
            }
        }
    }

    if((value = cgi_variable(webblk,"alter_cr")) && *value == 'A')
    {
        for(i = 0; i < 16; i++)
        {
        char regname[16];
            sprintf(regname,"alter_cr%d",i);
            if((value = cgi_variable(webblk,regname)))
            {
                if(regs->arch_mode != ARCH_900)
                    sscanf(value,"%x",&(regs->CR_L(i)));
                else
                    sscanf(value,"%llx",&(regs->CR_G(i)));
            }
        }
    }

    if((value = cgi_variable(webblk,"alter_ar")) && *value == 'A')
    {
        for(i = 0; i < 16; i++)
        {
        char regname[16];
            sprintf(regname,"alter_ar%d",i);
            if((value = cgi_variable(webblk,regname)))
                sscanf(value,"%x",&(regs->AR(i)));
        }
    }

    html_header(webblk);

    fprintf(webblk->hsock,"<form method=post>\n"
                          "<select type=submit name=cpu>\n");

    for(i = 0;
#if defined(_FEATURE_CPU_RECONFIG)
        i < MAX_CPU_ENGINES;
#else
        i < sysblk.numcpu;
#endif
        i++)
        if(sysblk.regs[i].cpuonline)
            fprintf(webblk->hsock,"<option value=%d%s>CPU%4.4X</option>\n",
              i,i==cpu?" selected":"",i);

    fprintf(webblk->hsock,"</select>\n"
                          "<input type=submit name=selcpu value=\"Select\">\n"
                          "<input type=hidden name=cpu value=%d>\n" 
                          "<input type=hidden name=select_gr value=%c>\n" 
                          "<input type=hidden name=select_cr value=%c>\n" 
                          "<input type=hidden name=select_ar value=%c>\n",
                          cpu, select_gr?'S':'H',select_cr?'S':'H',select_ar?'S':'H');
    fprintf(webblk->hsock,"Mode: %s\n",get_arch_mode_string(regs));
    fprintf(webblk->hsock,"</form>\n");

    if(!select_gr)
    {
        fprintf(webblk->hsock,"<form method=post>\n"
                              "<input type=submit name=select_gr "
                              "value=\"Select General Registers\">\n"
                              "<input type=hidden name=cpu value=%d>\n" 
                              "<input type=hidden name=select_cr value=%c>\n" 
                              "<input type=hidden name=select_ar value=%c>\n" 
                              "</form>\n",cpu,select_cr?'S':'H',select_ar?'S':'H');
    }
    else
    {
        fprintf(webblk->hsock,"<form method=post>\n"
                              "<input type=submit name=select_gr "
                              "value=\"Hide General Registers\">\n"
                              "<input type=hidden name=cpu value=%d>\n" 
                              "<input type=hidden name=select_cr value=%c>\n" 
                              "<input type=hidden name=select_ar value=%c>\n" 
                              "</form>\n",cpu,select_cr?'S':'H',select_ar?'S':'H');

        fprintf(webblk->hsock,"<form method=post>\n"
                              "<table>\n");
        for(i = 0; i < 16; i++)
        {
            if(regs->arch_mode != ARCH_900)
                fprintf(webblk->hsock,"%s<td>GR%d</td><td><input type=text name=alter_gr%d size=8 "
                  "value=%8.8X></td>\n%s",
                  (i&3)==0?"<tr>\n":"",i,i,regs->GR_L(i),((i&3)==3)?"</tr>\n":"");
            else
                fprintf(webblk->hsock,"%s<td>GR%d</td><td><input type=text name=alter_gr%d size=16 "
                  "value=%16.16llX></td>\n%s",
                  (i&3)==0?"<tr>\n":"",i,i,(long long)regs->GR_G(i),((i&3)==3)?"</tr>\n":"");
        }
        fprintf(webblk->hsock,"</table>\n"
                              "<input type=submit name=refresh value=\"Refresh\">\n"
                              "<input type=submit name=alter_gr value=\"Alter\">\n"
                              "<input type=hidden name=cpu value=%d>\n" 
                              "<input type=hidden name=select_gr value=S>\n" 
                              "<input type=hidden name=select_cr value=%c>\n" 
                              "<input type=hidden name=select_ar value=%c>\n" 
                              "</form>\n",cpu,select_cr?'S':'H',select_ar?'S':'H');
    }


    if(!select_cr)
    {
        fprintf(webblk->hsock,"<form method=post>\n"
                              "<input type=submit name=select_cr "
                              "value=\"Select Control Registers\">\n"
                              "<input type=hidden name=cpu value=%d>\n" 
                              "<input type=hidden name=select_gr value=%c>\n" 
                              "<input type=hidden name=select_ar value=%c>\n" 
                              "</form>\n",cpu,select_gr?'S':'H',select_ar?'S':'H');
    }
    else
    {
        fprintf(webblk->hsock,"<form method=post>\n"
                              "<input type=submit name=select_cr "
                              "value=\"Hide Control Registers\">\n"
                              "<input type=hidden name=cpu value=%d>\n" 
                              "<input type=hidden name=select_gr value=%c>\n" 
                              "<input type=hidden name=select_ar value=%c>\n" 
                              "</form>\n",cpu,select_gr?'S':'H',select_ar?'S':'H');

        fprintf(webblk->hsock,"<form method=post>\n"
                              "<table>\n");
        for(i = 0; i < 16; i++)
        {
            if(regs->arch_mode != ARCH_900)
                fprintf(webblk->hsock,"%s<td>CR%d</td><td><input type=text name=alter_cr%d size=8 "
                  "value=%8.8X></td>\n%s",
                  (i&3)==0?"<tr>\n":"",i,i,regs->CR_L(i),((i&3)==3)?"</tr>\n":"");
            else
                fprintf(webblk->hsock,"%s<td>CR%d</td><td><input type=text name=alter_cr%d size=16 "
                  "value=%16.16llX></td>\n%s",
                  (i&3)==0?"<tr>\n":"",i,i,(long long)regs->CR_G(i),((i&3)==3)?"</tr>\n":"");
        }
        fprintf(webblk->hsock,"</table>\n"
                              "<input type=submit name=refresh value=\"Refresh\">\n"
                              "<input type=submit name=alter_cr value=\"Alter\">\n"
                              "<input type=hidden name=cpu value=%d>\n" 
                              "<input type=hidden name=select_cr value=S>\n" 
                              "<input type=hidden name=select_gr value=%c>\n" 
                              "<input type=hidden name=select_ar value=%c>\n" 
                              "</form>\n",cpu,select_gr?'S':'H',select_ar?'S':'H');
    }


    if(regs->arch_mode != ARCH_370)
    {
        if(!select_ar)
        {
            fprintf(webblk->hsock,"<form method=post>\n"
                                  "<input type=submit name=select_ar "
                                  "value=\"Select Access Registers\">\n"
                                  "<input type=hidden name=cpu value=%d>\n" 
                                  "<input type=hidden name=select_gr value=%c>\n" 
                                  "<input type=hidden name=select_cr value=%c>\n" 
                                  "</form>\n",cpu,select_gr?'S':'H',select_cr?'S':'H');
        }
        else
        {
            fprintf(webblk->hsock,"<form method=post>\n"
                                  "<input type=submit name=select_ar "
                                  "value=\"Hide Access Registers\">\n"
                                  "<input type=hidden name=cpu value=%d>\n" 
                                  "<input type=hidden name=select_gr value=%c>\n" 
                                  "<input type=hidden name=select_cr value=%c>\n" 
                                  "</form>\n",cpu,select_gr?'S':'H',select_cr?'S':'H');
    
            fprintf(webblk->hsock,"<form method=post>\n"
                                  "<table>\n");
            for(i = 0; i < 16; i++)
            {
                fprintf(webblk->hsock,"%s<td>AR%d</td><td><input type=text name=alter_ar%d size=8 "
                  "value=%8.8X></td>\n%s",
                  (i&3)==0?"<tr>\n":"",i,i,regs->AR(i),((i&3)==3)?"</tr>\n":"");
            }
            fprintf(webblk->hsock,"</table>\n"
                                  "<input type=submit name=refresh value=\"Refresh\">\n"
                                  "<input type=submit name=alter_ar value=\"Alter\">\n"
                                  "<input type=hidden name=cpu value=%d>\n" 
                                  "<input type=hidden name=select_gr value=%c>\n" 
                                  "<input type=hidden name=select_cr value=%c>\n" 
                                  "<input type=hidden name=select_ar value=S>\n" 
                                  "</form>\n",cpu,select_gr?'S':'H',select_cr?'S':'H');
        }
    }

    html_footer(webblk);

}


void cgibin_debug_storage(WEBBLK *webblk)
{
int i, j;
char *value;
U32 addr = 0;

    /* INCOMPLETE 
     * no storage alter
     * no storage type (abs/real/prim virt/sec virt/access reg virt)
     * no cpu selection for storage other then abs 
     */

    if((value = cgi_variable(webblk,"alter_a0")))
        sscanf(value,"%x",&addr);

    addr &= ~0x0F;

    html_header(webblk);


    fprintf(webblk->hsock,"<form method=post>\n"
                          "<table>\n");

    if(addr > sysblk.mainsize || (addr + 128) > sysblk.mainsize)
        addr = sysblk.mainsize - 128;

    for(i = 0; i < 128;)
    {
        if(i == 0)
            fprintf(webblk->hsock,"<tr>\n"
                                  "<td><input type=text name=alter_a0 size=8 value=%8.8X>"
                                  "<input type=hidden name=alter_a1 value=%8.8X></td>\n"
                                  "<td><input type=submit name=refresh value=\"Refresh\"></td>\n",
                                  i + addr, i + addr);
        else
            fprintf(webblk->hsock,"<tr>\n"
                                  "<td align=center>%8.8X</td>\n"
                                  "<td></td>\n",
                                  i + addr);

    for(j = 0; j < 4; i += 4, j++)
        {
        U32 m;
            FETCH_FW(m,sysblk.mainstor + i + addr);
            fprintf(webblk->hsock,"<td><input type=text name=alter_m%d size=8 value=%8.8X></td>\n",i,m);
        }

        fprintf(webblk->hsock,"</tr>\n");
    }

    fprintf(webblk->hsock,"</table>\n"
                          "</form>\n");
    html_footer(webblk);

}


void cgibin_ipl(WEBBLK *webblk)
{
U32 i;
char *value;
DEVBLK *dev;
U32 ipldev;
U32 iplcpu;
U32 doipl;

    html_header(webblk);

    fprintf(webblk->hsock,"<h1>Perform Initial Program Load</h1>\n");

    if(cgi_variable(webblk,"doipl"))
        doipl = 1;
    else
        doipl = 0;

    if((value = cgi_variable(webblk,"device")))
        sscanf(value,"%x",&ipldev);
    else
        ipldev = sysblk.ipldev;

    if((value = cgi_variable(webblk,"cpu")))
        sscanf(value,"%x",&iplcpu);
    else
        iplcpu = sysblk.iplcpu;

    if((value = cgi_variable(webblk,"loadparm")))
    {
        for(i = 0; i < strlen(value); i++)
            sysblk.loadparm[i] = host_to_guest((int)value[i]);
        for(; i < 8; i++)
            sysblk.loadparm[i] = host_to_guest(' ');
    }

    /* Validate CPU number */
    if(
#if defined(_FEATURE_CPU_RECONFIG)
            iplcpu >= MAX_CPU_ENGINES
#else
            iplcpu >= sysblk.numcpu
#endif
      || !sysblk.regs[iplcpu].cpuonline)
        doipl = 0;
  
    if(!doipl)
    {
        /* Present IPL parameters */
        fprintf(webblk->hsock,"<form method=post>\n"
                              "<select type=submit name=cpu>\n");

        for(i = 0;
#if defined(_FEATURE_CPU_RECONFIG)
            i < MAX_CPU_ENGINES;
#else
            i < sysblk.numcpu;
#endif
            i++)
            if(sysblk.regs[i].cpuonline)
                fprintf(webblk->hsock,"<option value=%4.4X%s>CPU%4.4X</option>\n",
                  i, ((sysblk.regs[i].cpuad == iplcpu) ? " selected" : ""), i);

        fprintf(webblk->hsock,"</select>\n"
                              "<select type=submit name=device>\n");

        for(dev = sysblk.firstdev; dev; dev = dev->nextdev)
            if(dev->pmcw.flag5 & PMCW5_V)
                fprintf(webblk->hsock,"<option value=%4.4X%s>DEV%4.4X</option>\n",
                  dev->devnum, ((dev->devnum == ipldev) ? " selected" : ""), dev->devnum);

        fprintf(webblk->hsock,"</select>\n");

        fprintf(webblk->hsock,"Loadparm:<input type=text name=loadparm size=8 value=\"%c%c%c%c%c%c%c%c\">\n",
          guest_to_host(sysblk.loadparm[0]),
          guest_to_host(sysblk.loadparm[1]),
          guest_to_host(sysblk.loadparm[2]),
          guest_to_host(sysblk.loadparm[3]),
          guest_to_host(sysblk.loadparm[4]),
          guest_to_host(sysblk.loadparm[5]),
          guest_to_host(sysblk.loadparm[6]),
          guest_to_host(sysblk.loadparm[7]));

        fprintf(webblk->hsock,"<input type=submit name=doipl value=\"IPL\">\n"
                          "</form>\n");

    }
    else
    {
        /* Perform IPL function */
        if( load_ipl(ipldev, sysblk.regs + iplcpu) )
        {
            fprintf(webblk->hsock,"<h3>IPL failed, see the "
                                  "<a href=\"syslog#bottom\">system log</a> "
                                  "for details</h3>\n");
        }
        else
        {
            fprintf(webblk->hsock,"<h3>IPL completed</h3>\n");
        }
    }

    html_footer(webblk);

}


void cgibin_debug_device_list(WEBBLK *webblk)
{
DEVBLK *dev;
BYTE   *class;
BYTE   buf[80];

    html_header(webblk);

    fprintf(webblk->hsock,"<h2>Attached Device List</h2>\n"
                          "<table>\n"
                          "<tr><th>Number</th>"
                          "<th>Subchannel</th>"
                          "<th>Class</th>"
                          "<th>Type</th>"
                          "<th>Status</th></tr>\n");

    for(dev = sysblk.firstdev; dev; dev = dev->nextdev)
        if(dev->pmcw.flag5 & PMCW5_V)
        {
             (dev->hnd->query)(dev, &class, sizeof(buf), buf);

             fprintf(webblk->hsock,"<tr>"
                                   "<td>%4.4X</td>"
                                   "<td><a href=\"detail?subchan=%4.4X\">%4.4X</a></td>"
                                   "<td>%s</td>"
                                   "<td>%4.4X</td>"
                                   "<td>%s%s%s</td>"
                                   "</tr>\n",
                                   dev->devnum,
                                   dev->subchan,dev->subchan,
                                   class,
                                   dev->devtype,
                                   (dev->fd > 2 ? "open " : ""),
                                   (dev->busy ? "busy " : ""),
                                   (IOPENDING(dev) ? "pending " : ""));
        }

    fprintf(webblk->hsock,"</table>\n");

    html_footer(webblk);

}


void cgibin_debug_device_detail(WEBBLK *webblk)
{
DEVBLK *sel, *dev = NULL;
char *value;
int subchan;

    html_header(webblk);

    if((value = cgi_variable(webblk,"subchan"))
      && sscanf(value,"%x",&subchan) == 1)
        for(dev = sysblk.firstdev; dev; dev = dev->nextdev)
            if(dev->subchan == subchan)
                break;

    fprintf(webblk->hsock,"<h3>Subchannel Details</h3>\n");

    fprintf(webblk->hsock,"<form method=post>\n"
                          "<select type=submit name=subchan>\n");

    for(sel = sysblk.firstdev; sel; sel = sel->nextdev)
    {
        fprintf(webblk->hsock,"<option value=%4.4X%s>Subchannel %4.4X",
          sel->subchan, ((sel == dev) ? " selected" : ""), sel->subchan);
        if(sel->pmcw.flag5 & PMCW5_V)
            fprintf(webblk->hsock," Device %4.4X</option>\n",sel->devnum);
        else
            fprintf(webblk->hsock,"</option>\n");
    }

    fprintf(webblk->hsock,"</select>\n"
                          "<input type=submit value=\"Select / Refresh\">\n"
                          "</form>\n");

    if(dev)
    {

        fprintf(webblk->hsock,"<table border>\n"
                              "<caption align=left>"
                              "<h3>Path Management Control Word</h3>"
                              "</caption>\n");

        fprintf(webblk->hsock,"<tr><th colspan=32>Interruption Parameter</th></tr>\n");

        fprintf(webblk->hsock,"<tr><td colspan=32>%2.2X%2.2X%2.2X%2.2X</td></tr>\n",
                              dev->pmcw.intparm[0], dev->pmcw.intparm[1],
                              dev->pmcw.intparm[2], dev->pmcw.intparm[3]);

        fprintf(webblk->hsock,"<tr><th>Q</th>"
                              "<th>0</th>"
                              "<th colspan=3>ISC</th>"
                              "<th colspan=2>00</th>"
                              "<th>A</th>"
                              "<th>E</th>"
                              "<th colspan=2>LM</th>"
                              "<th colspan=2>MM</th>"
                              "<th>D</th>"
                              "<th>T</th>"
                              "<th>V</th>"
                              "<th colspan=16>DEVNUM</th></tr>\n");

        fprintf(webblk->hsock,"<tr><td>%d</td>"
                              "<td></td>"
                              "<td colspan=3>%d</td>"
                              "<td colspan=2></td>"
                              "<td>%d</td>"
                              "<td>%d</td>"
                              "<td colspan=2>%d%d</td>"
                              "<td colspan=2>%d%d</td>"
                              "<td>%d</td>"
                              "<td>%d</td>"
                              "<td>%d</td>"
                              "<td colspan=16>%2.2X%2.2X</td></tr>\n",
                              ((dev->pmcw.flag4 & PMCW4_Q) >> 7),
                              ((dev->pmcw.flag4 & PMCW4_ISC) >> 3),
                              (dev->pmcw.flag4 & 1),
                              ((dev->pmcw.flag5 >> 7) & 1),
                              ((dev->pmcw.flag5 >> 6) & 1),
                              ((dev->pmcw.flag5 >> 5) & 1),
                              ((dev->pmcw.flag5 >> 4) & 1),
                              ((dev->pmcw.flag5 >> 3) & 1),
                              ((dev->pmcw.flag5 >> 2) & 1),
                              ((dev->pmcw.flag5 >> 1) & 1),
                              (dev->pmcw.flag5 & 1),
                              dev->pmcw.devnum[0],
                              dev->pmcw.devnum[1]);

        fprintf(webblk->hsock,"<tr><th colspan=8>LPM</th>"
                              "<th colspan=8>PNOM</th>"
                              "<th colspan=8>LPUM</th>"
                              "<th colspan=8>PIM</th></tr>\n");

        fprintf(webblk->hsock,"<tr><td colspan=8>%2.2X</td>"
                              "<td colspan=8>%2.2X</td>"
                              "<td colspan=8>%2.2X</td>"
                              "<td colspan=8>%2.2X</td></tr>\n",
                              dev->pmcw.lpm,
                              dev->pmcw.pnom,
                              dev->pmcw.lpum,
                              dev->pmcw.pim);

        fprintf(webblk->hsock,"<tr><th colspan=16>MBI</th>"
                              "<th colspan=8>POM</th>"
                              "<th colspan=8>PAM</th></tr>\n");

        fprintf(webblk->hsock,"<tr><td colspan=16>%2.2X%2.2X</td>"
                              "<td colspan=8>%2.2X</td>"
                              "<td colspan=8>%2.2X</td></tr>\n",
                              dev->pmcw.mbi[0],
                              dev->pmcw.mbi[1],
                              dev->pmcw.pom,
                              dev->pmcw.pam);

        fprintf(webblk->hsock,"<tr><th colspan=8>CHPID=0</th>"
                              "<th colspan=8>CHPID=1</th>"
                              "<th colspan=8>CHPID=2</th>"
                              "<th colspan=8>CHPID=3</th></tr>\n");

        fprintf(webblk->hsock,"<tr><td colspan=8>%2.2X</td>"
                              "<td colspan=8>%2.2X</td>"
                              "<td colspan=8>%2.2X</td>"
                              "<td colspan=8>%2.2X</td></tr>\n",
                              dev->pmcw.chpid[0],
                              dev->pmcw.chpid[1],
                              dev->pmcw.chpid[2],
                              dev->pmcw.chpid[3]);

        fprintf(webblk->hsock,"<tr><th colspan=8>CHPID=4</th>"
                              "<th colspan=8>CHPID=5</th>"
                              "<th colspan=8>CHPID=6</th>"
                              "<th colspan=8>CHPID=7</th></tr>\n");

        fprintf(webblk->hsock,"<tr><td colspan=8>%2.2X</td>"
                              "<td colspan=8>%2.2X</td>"
                              "<td colspan=8>%2.2X</td>"
                              "<td colspan=8>%2.2X</td></tr>\n",
                              dev->pmcw.chpid[4],
                              dev->pmcw.chpid[5],
                              dev->pmcw.chpid[6],
                              dev->pmcw.chpid[7]);

        fprintf(webblk->hsock,"<tr><th colspan=8>ZONE</th>"
                              "<th colspan=5>00000</th>"
                              "<th colspan=3>VISC</th>"
                              "<th colspan=8>00000000</th>"
                              "<th>I</th>"
                              "<th colspan=6>000000</th>"
                              "<th>S</th></tr>\n");

        fprintf(webblk->hsock,"<tr><td colspan=8>%2.2X</td>"
                              "<td colspan=5></td>"
                              "<td colspan=3>%d</td>"
                              "<td colspan=8></td>"
                              "<td>%d</td>"
                              "<td colspan=6></td>"
                              "<td>%d</td></tr>\n",
                              dev->pmcw.zone,
                              (dev->pmcw.flag25 & PMCW25_VISC),
                              (dev->pmcw.flag27 & PMCW27_I) >> 7,
                              (dev->pmcw.flag27 & PMCW27_S));

        fprintf(webblk->hsock,"</table>\n");

    }

    html_footer(webblk);

}


void cgibin_debug_misc(WEBBLK *webblk)
{
int zone;

    html_header(webblk);

    fprintf(webblk->hsock,"<h2>Miscellaneous Registers<h2>\n");


    fprintf(webblk->hsock,"<table border>\n"
                          "<caption align=left>"
                          "<h3>Zone related Registers</h3>"
                          "</caption>\n");

    fprintf(webblk->hsock,"<tr><th>Zone</th>"
                          "<th>CS Origin</th>"
                          "<th>CS Limit</th>"
                          "<th>ES Origin</th>"
                          "<th>ES Limit</th>"
                          "<th>Measurement Block</th>"
                          "<th>Key</th></tr>\n");

    for(zone = 0; zone < FEATURE_SIE_MAXZONES; zone++)
    {
        fprintf(webblk->hsock,"<tr><td>%2.2X</td>"
                              "<td>%8.8X</td>"
                              "<td>%8.8X</td>"
                              "<td>%8.8X</td>"
                              "<td>%8.8X</td>"
                              "<td>%8.8X</td>"
                              "<td>%2.2X</td></tr>\n",
                              zone,
                              (U32)sysblk.zpb[zone].mso << 20,
                              ((U32)sysblk.zpb[zone].msl << 20) | 0xFFFFF,
                              (U32)sysblk.zpb[zone].eso << 20,
                              ((U32)sysblk.zpb[zone].esl << 20) | 0xFFFFF,
                              (U32)sysblk.zpb[zone].mbo,
                              sysblk.zpb[zone].mbk);

    }

    fprintf(webblk->hsock,"</table>\n");


    fprintf(webblk->hsock,"<table border>\n"
                          "<caption align=left>"
                          "<h3>Alternate Measurement</h3>"
                          "</caption>\n");

    fprintf(webblk->hsock,"<tr><th>Measurement Block</th>"
                          "<th>Key</th></tr>\n");

    fprintf(webblk->hsock,"<tr><td>%8.8X</td>"
                          "<td>%2.2X</td></tr>\n",
                          (U32)sysblk.mbo,
                          sysblk.mbk);

    fprintf(webblk->hsock,"</table>\n");


    fprintf(webblk->hsock,"<table border>\n"
                          "<caption align=left>"
                          "<h3>Address Limit Register</h3>"
                          "</caption>\n");

    fprintf(webblk->hsock,"<tr><td>%8.8X</td></tr>\n",
                              (U32)sysblk.addrlimval);

    fprintf(webblk->hsock,"</table>\n");

    html_footer(webblk);

}



void cgibin_debug_version_info(WEBBLK *webblk)
{
    html_header(webblk);

    fprintf(webblk->hsock,"<h1>Hercules Version Information</h1>\n"
                          "<pre>\n");
    display_version(webblk->hsock,"Hercules HTTP Server ", TRUE);
    fprintf(webblk->hsock,"</pre>\n");

    html_footer(webblk);

}


/* The following table is the cgi-bin directory, which               */
/* associates directory filenames with cgibin routines               */

CGITAB cgidir[] = {
    { "tasks/syslog", &cgibin_syslog },
    { "tasks/ipl", &cgibin_ipl },
    { "debug/registers", &cgibin_debug_registers },
    { "debug/storage", &cgibin_debug_storage },
    { "debug/misc", &cgibin_debug_misc },
    { "debug/version_info", &cgibin_debug_version_info },
    { "debug/device/list", &cgibin_debug_device_list },
    { "debug/device/detail", &cgibin_debug_device_detail },
    { "registers/general", &cgibin_reg_general },
    { "registers/control", &cgibin_reg_control },
    { "registers/psw", &cgibin_psw },
    { NULL, NULL } };

#endif /*defined(OPTION_HTTP_SERVER)*/

/* CGIBIN.C     (c)Copyright Jan Jaeger, 2002                        */
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
#include "opcode.h"
#include "httpmisc.h"

#if defined(OPTION_HTTP_SERVER)


zz_cgibin cgibin_reg_control(WEBBLK *webblk)
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
            fprintf(webblk->hsock, "CR%1.1X=%16.16llX%s", i, regs->CR_G(i),
                ((i & 0x03) == 0x03) ? "\n" : " ");           

    fprintf(webblk->hsock, "</PRE>\n");

    html_footer(webblk);

    return NULL;
}


zz_cgibin cgibin_reg_general(WEBBLK *webblk)
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
            fprintf(webblk->hsock, "GR%1.1X=%16.16llX%s", i, regs->GR_G(i),
                ((i & 0x03) == 0x03) ? "\n" : " ");           

    fprintf(webblk->hsock, "</PRE>\n");

    html_footer(webblk);

    return NULL;
}


void store_psw (REGS *regs, BYTE *addr);

zz_cgibin cgibin_psw(WEBBLK *webblk)
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

    return NULL;
}


void get_msgbuf(BYTE **msgbuf, int *msgslot, int *nummsgs, int *msg_size, int *max_msgs);

zz_cgibin cgibin_syslog(WEBBLK *webblk)
{
BYTE   *msgbuf;                         /* Circular message buffer   */
int     msgslot;                        /* Next available buffer slot*/
int     nummsgs;                        /* Number of msgs in buffer  */
int     msg_size;
int     max_msgs;
BYTE   *pointer;
int     currmsgn;
char   *command;

char *value;
int autorefresh = 0;
int refresh_interval = 5;
int msgcount = 22;

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

    get_msgbuf(&msgbuf, &msgslot, &nummsgs, &msg_size, &max_msgs);

    for(currmsgn = 0; currmsgn < nummsgs; currmsgn++) {
        if(!msgcount || currmsgn >= (nummsgs - msgcount))
        {
            if(nummsgs < max_msgs)
                pointer = msgbuf + (currmsgn * msg_size);
            else
                if(currmsgn + msgslot < max_msgs) 
                    pointer = msgbuf + ((currmsgn + msgslot) * msg_size);
                else
                    pointer = msgbuf + ((currmsgn + msgslot - max_msgs) * msg_size);
    
            fprintf(webblk->hsock,"%80.80s\n",pointer);
        }
    }

    fprintf(webblk->hsock, "</PRE>\n");

    fprintf(webblk->hsock, "<FORM method=post>Command:\n");
    fprintf(webblk->hsock, "<INPUT type=text name=command size=80>\n");
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

    return NULL;
}


zz_cgibin cgibin_debug_registers(WEBBLK *webblk)
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
                  (i&3)==0?"<tr>\n":"",i,i,regs->GR_G(i),((i&3)==3)?"</tr>\n":"");
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
                  (i&3)==0?"<tr>\n":"",i,i,regs->CR_G(i),((i&3)==3)?"</tr>\n":"");
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

    return NULL;
}


zz_cgibin cgibin_debug_storage(WEBBLK *webblk)
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

    return NULL;
}


zz_cgibin cgibin_ipl(WEBBLK *webblk)
{
int i;
DEVBLK *dev;

    html_header(webblk);

    fprintf(webblk->hsock,"<h1>Function not yet implemented</h1>\n");

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
            fprintf(webblk->hsock,"<option value=%d>CPU%4.4X</option>\n",i,i);

    fprintf(webblk->hsock,"</select>\n"
                          "<select type=submit name=cpu>\n");

    for(dev = sysblk.firstdev; dev; dev = dev->nextdev)
        if(dev->pmcw.flag5 & PMCW5_V)
            fprintf(webblk->hsock,"<option value=%d>DEV%4.4X</option>\n",
              dev->devnum, dev->devnum);

    fprintf(webblk->hsock,"</select>\n");

    fprintf(webblk->hsock,"Loadparm:<input type=text size=8 value=\"%c%c%c%c%c%c%c%c\">\n",
      ebcdic_to_ascii[sysblk.loadparm[0]],
      ebcdic_to_ascii[sysblk.loadparm[1]],
      ebcdic_to_ascii[sysblk.loadparm[2]],
      ebcdic_to_ascii[sysblk.loadparm[3]],
      ebcdic_to_ascii[sysblk.loadparm[4]],
      ebcdic_to_ascii[sysblk.loadparm[5]],
      ebcdic_to_ascii[sysblk.loadparm[6]],
      ebcdic_to_ascii[sysblk.loadparm[7]]);

    fprintf(webblk->hsock,"<input type=submit name=doipl value=\"IPL\">\n"
                          "</form>\n");

    html_footer(webblk);

    return NULL;
}


zz_cgibin cgibin_debug_version_info(WEBBLK *webblk)
{
    html_header(webblk);

    fprintf(webblk->hsock,"<h3>Hercules Version Information</h3>\n"
                          "<pre>\n");
    display_version(webblk->hsock,"Hercules HTTP Server ");
    fprintf(webblk->hsock,"</pre>\n");

    html_footer(webblk);

    return NULL;
}


/* The following table is the cgi-bin directory, which               */
/* associates directory filenames with cgibin routines               */

CGITAB cgidir[] = {
    { "syslog", (void*)&cgibin_syslog },
    { "ipl", (void*)&cgibin_ipl },
    { "debug/registers", (void*)&cgibin_debug_registers },
    { "debug/storage", (void*)&cgibin_debug_storage },
    { "debug/version_info", (void*)&cgibin_debug_version_info },
    { "registers/general", (void*)&cgibin_reg_general },
    { "registers/control", (void*)&cgibin_reg_control },
    { "registers/psw", (void*)&cgibin_psw },
    { NULL, NULL } };

#endif /*defined(OPTION_HTTP_SERVER)*/

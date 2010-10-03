/* LOADMEM.C    (c) Copyright TurboHercules, SAS 2010                */
/*              load memory functions                                */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id: loadmem.c 589 2010-10-03 06:12:47Z paulgorlinsky $

#include "hstdinc.h"

#define _LOADMEM_C_

#include "hercules.h"

/*-------------------------------------------------------------------*/
/* loadcore filename command - load a core image file                */
/*-------------------------------------------------------------------*/
int loadcore_cmd(int argc, char *argv[], char *cmdline)
{
REGS *regs;

    char   *fname;                      /* -> File name (ASCIIZ)     */
    struct stat statbuff;               /* Buffer for file status    */
    char   *loadaddr;                   /* loadcore memory address   */
    U32     aaddr;                      /* Absolute storage address  */
    int     len;                        /* Number of bytes read      */
    char    pathname[MAX_PATH];         /* file in host path format  */

    UNREFERENCED(cmdline);

    if (argc < 2)
    {
        WRMSG(HHC02202, "E");
        return -1;
    }

    fname = argv[1];
    hostpath(pathname, fname, sizeof(pathname));

    if (stat(pathname, &statbuff) < 0)
    {
        WRMSG(HHC02219, "E", "stat()", strerror(errno));
        return -1;
    }

    if (argc < 3) aaddr = 0;
    else
    {
        loadaddr = argv[2];

        if (sscanf(loadaddr, "%x", &aaddr) !=1)
        {
            WRMSG(HHC02205, "E", loadaddr, ": invalid address" );
            return -1;
        }
    }

    obtain_lock(&sysblk.cpulock[sysblk.pcpu]);

    if (!IS_CPU_ONLINE(sysblk.pcpu))
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        WRMSG(HHC00816, "W", PTYPSTR(sysblk.pcpu), sysblk.pcpu, "online");
        return 0;
    }
    regs = sysblk.regs[sysblk.pcpu];

    /* Command is valid only when CPU is stopped */
    if (CPUSTATE_STOPPED != regs->cpustate)
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        WRMSG(HHC02247, "E");
        return -1;
    }

    /* Read the file into absolute storage */
    WRMSG(HHC02250, "I", fname, aaddr );

    len = load_main(fname, aaddr);

    release_lock(&sysblk.cpulock[sysblk.pcpu]);

    WRMSG(HHC02249, "I");

    return 0;
}


/*-------------------------------------------------------------------*/
/* loadtext filename command - load a text deck file                 */
/*-------------------------------------------------------------------*/
int loadtext_cmd(int argc, char *argv[], char *cmdline)
{
    const char blanks[8]     = {"\x40\x40\x40\x40\x40\x40\x40\x40"};

    /* Object deck record format headers                             */
    /* (names chosen not to conflict with future GOFF support)       */

    const char ObjectESD[10] = {"\x02\xC5\xE2\xC4\x40\x40\x40\x40\x40\x40"};    /* ESD card */
    const char ObjectTXT[5]  = {"\x02\xE3\xE7\xE3\x40"};                        /* TXT card */
    const char ObjectRLD[10] = {"\x02\xD9\xD3\xC4\x40\x40\x40\x40\x40\x40"};    /* RLD card */
    const char ObjectEND[5]  = {"\x02\xC5\xD5\xC4\x40"};                        /* END card */
    const char ObjectSYM[10] = {"\x02\xE2\xE8\xD4\x40\x40\x40\x40\x40\x40"};    /* SYM card */

    /* Special SPB (Set Page Boundary) directive from VM             */

    const char ObjectSPB[5]  = {"\x02\xE1\xD7\xC2\x40"};                        /* SPB card */
    const char ObjectComment[1] = { "\x5C" };                                   /* comment card */

    char   *fname;                      /* -> File name (ASCIIZ)     */
    char   *loadaddr;                   /* loadcore memory address   */
    U32     aaddr;                      /* Absolute storage address  */
    U32     ahighaddr;                  /* Absolute high address     */
    int     fd;                         /* File descriptor           */
    BYTE    buf[80];                    /* Read buffer               */
    int     recno;                      /* Record number             */
    int     rc = 0;                     /* Return code               */
    REGS   *regs;
    char    pathname[MAX_PATH];

    UNREFERENCED(cmdline);

    if (argc < 2)
    {
        WRMSG(HHC02202, "E");
        return -1;
    }

    fname = argv[1];

    if (argc < 3) 
        aaddr = 0;
    else
    {
        loadaddr = argv[2];

        if (sscanf(loadaddr, "%x", &aaddr) !=1)
        {
            WRMSG(HHC02205, "E", loadaddr, ": invalid address" );
            return -1;
        }
        
        if ( aaddr > sysblk.mainsize )
        {
            WRMSG( HHC02251, "E" );
        }
    }

    ahighaddr = aaddr;

    obtain_lock(&sysblk.cpulock[sysblk.pcpu]);

    if (!IS_CPU_ONLINE(sysblk.pcpu))
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        WRMSG(HHC00816, "W", PTYPSTR(sysblk.pcpu), sysblk.pcpu, "online");
        return 0;
    }
    regs = sysblk.regs[sysblk.pcpu];

    if (aaddr > regs->mainlim)
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        WRMSG(HHC02251, "E");
        return -1;
    }

    /* Command is valid only when CPU is stopped */
    if (CPUSTATE_STOPPED != regs->cpustate)
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        WRMSG(HHC02247, "E");
        return -1;
    }

    /* Open the specified file name */
    hostpath(pathname, fname, sizeof(pathname));
    if ((fd = open (pathname, O_RDONLY | O_BINARY)) < 0)
    {
        release_lock(&sysblk.cpulock[sysblk.pcpu]);
        WRMSG(HHC02219,"E", "open()", strerror(errno));
        return -1;
    }

    for ( recno = 1; ; recno++ )
    {
        int rlen;                               /* Record Length */

        /* Read 80 bytes into buffer */
        rlen = read( fd, buf, sizeof(buf) );
        if ( rlen < 0 )
        {
            WRMSG(HHC02219,"E", "read()", strerror(errno));
            rc = -1;
            break;
        }   
        /* if record length is not 80; leave */
        else if ( rlen != sizeof(buf) )
        {
            WRMSG( HHC02301, "E", "loadtext", sizeof(buf) );
            rc = -1;
            break;
        }
        /* Process valid Object Deck Records; priority sorted        */

        /* - Unknown cards such as comments and link-edit/binder     */
        /*   commands are ignored.                                   */
        /* - Cards that are determined to be in error either raise   */
        /*   an error condition to the operator, or are ignored.     */
        /* - Columns 73-80 (Deck ID, sequence number, or both) are   */
        /*   ignored.                                                */

        /* if record is "TXT" then copy bytes to mainstore */

        if ( NCMP(ObjectTXT, buf, sizeof(ObjectTXT)) && // TXT Object identifier
             NCMP(buf+8, blanks, 2) &&                  // Required blanks
             NCMP(buf+12, blanks, 2) )                  // ...
        {
            int n   = ((((buf[5] << 8) | buf[6])<<8) | buf[7]); // Relative address (positive)
            int len = (buf[10] << 8) | buf[11];                 // Byte count
            if (len <= 56)                                      // Process if useable count
            {
               RADR lastbyte = aaddr + n + len - 1;
               if (lastbyte > sysblk.mainsize)                  // Bytes must fit into storage
               {
                  WRMSG(HHC02251,"E");
                  rc = -1;
                  break;
               }
               ahighaddr = MAX(ahighaddr, lastbyte);            // Keep track of highsest byte used
               memcpy(regs->mainstor + aaddr + n, &buf[16], len);
               STORAGE_KEY(aaddr + n, regs) |= (STORKEY_REF | STORKEY_CHANGE);
               STORAGE_KEY(aaddr + n + len - 1, regs) |= (STORKEY_REF | STORKEY_CHANGE);
            }
        }

        /* if record is "RLD" then process */
        else if (NCMP(ObjectRLD, buf, sizeof(ObjectRLD)) && // RLD Object identifier
                 NCMP(buf+12, blanks, 4))                   // Required blanks
        {

            /* Define local macros for relocation tests              */

#define RLD_RELOC_A     ((reloc[4] & 0x30) == 0x00)             // A-constant
#define RLD_RELOC_V     ((reloc[4] & 0x30) == 0x10)             // V-constant
#define RLD_RELOC_Q     ((reloc[4] & 0x30) == 0x20)             // Q-constant
#define RLD_RELOC_CXD   ((reloc[4] & 0x30) == 0x30)             // CXD-constant
#define RLD_RELOC_RI2   ((reloc[4] & 0x7C) == 0x70)             // 2-byte Relative-Immediate reference
#define RLD_RELOC_RI4   ((reloc[4] & 0x7C) == 0x78)             // 4-byte Relative-Immediate reference
#define RLD_RELOC_RI    (RLD_RELOC_RI2 || RLD_RELOC_RI4)        // Relative-Immediate reference


            char *reloc;
            U16 count = (buf[10] << 8) | buf[11];
            int seglength = 8;                  // Set initial segment length
            U16 resdid;                         // Relocation ESDID
            U16 pesdid;                         // Position ESDID

            if (count > 56)
               continue;                        // Skip if bad count

            /* Process Relocation Dictionary Entries                          */

            /* Note: All relocations are presently considered to only have    */
            /*       relocation within a single object deck (ESDIDs ignored). */

            for (reloc = (char *)buf+16; reloc < (char *)buf+16+count; )
            {
               int acl;                        // Address constant length
               int aarel;                      // Load relocation address
               int n;                          // Working length
               char *vc;                       // Pointer to relocation constant
               U64 v = 0;                      // Variable contents for relocation


               /* Load new Relocation ESDID and Position ESDID */

               if (seglength > 4)
               {
                  resdid = (reloc[0] << 8) | reloc[1];
                  pesdid = (reloc[2] << 8) | reloc[3];
               }


               /* Determine address constant length - 1 */

               if (RLD_RELOC_RI)                        // Relative-Immediate?
                  acl = ((reloc[4] & 0x08) >> 2) + 1;   // Set 2/4-byte Relative-Immediate length
               else                                     // Else set address constant length
                  acl = ((reloc[4] & 0x40) >> 4) + ((reloc[4] & 0x0C) >> 2);


               /* Load relocation address */

               aarel = (((reloc[5] << 8) | reloc[6]) << 8) | reloc[7];
               if ((aaddr + aarel + acl) > sysblk.mainsize)
               {
                  WRMSG(HHC02251,"E");
                  rc = -1;
                  break;
               }


               if (!(RLD_RELOC_Q || RLD_RELOC_CXD))     // Relocate all except Q/CXD-constants
               {

                  /* Fetch variable length constant                     */

                  vc = (char *)regs->mainstor + aaddr + aarel;     // Point to constant
                  v = 0;
                  for (n = 0;;)                                    // Load constant
                  {
                     v |=  vc[n];
                     if (n++ >= acl)
                        break;
                     v <<= 8;
                  }

                  /* Relocate value */

                  if (reloc[4] & 0x02)
                     v = aaddr - v;        /* Negative relocation */
                  else
                     v += aaddr;           /* Positive relocation */


                  /* Store variable length constant */

                  for (n = acl; n >= 0; n--)               // Store constant
                  {
                     vc[n] = v & 0xFF;
                     v >>= 8;
                  }
               }


               /* Set length based on using last ESDID entries */

               reloc += seglength = (reloc[4] & 0x01) ? 4 : 8;
            }

            /* Undefine local macros for relocation tests            */

#undef RLD_RELOC_A      // A-constant
#undef RLD_RELOC_V      // V-constant
#undef RLD_RELOC_Q      // Q-constant
#undef RLD_RELOC_CXD    // CXD-constant
#undef RLD_RELOC_RI2    // 2-byte Relative-Immediate reference
#undef RLD_RELOC_RI4    // 4-byte Relative-Immediate reference
#undef RLD_RELOC_RI     // Relative-Immediate reference

        }

        /* if record is "ESD" then process */
        else if (NCMP(ObjectESD, buf, sizeof(ObjectESD)) && // ESD Object identifier
                 NCMP(buf+12, blanks, 2) &&                 // Required blanks
                 NCMP(buf+64, blanks, 8))                   // ...
        {
            continue;    // For now, fill in later
        }

        /* if record is "END" then break out of loop */
        else if (NCMP(ObjectEND, buf, sizeof(ObjectEND)) && // END Object identifier
                 NCMP(buf+8, blanks, 6) &&                  // Required blanks
                 NCMP(buf+24, blanks, 4) &&                 // ...
                 NCMP(buf+71, blanks, 1 ) )                 // ...
        {
            break;
        }

        /* if record is "SYM" then process */
        else if (NCMP(ObjectSYM, buf, sizeof(ObjectSYM)) && // SYM Object identifier
                 NCMP(buf+12, blanks, 4))                   // Required blanks
        {
            continue;    // For now, fill in later
        }

        /* if record is "SPB" then process */
        else if (NCMP(ObjectSPB, buf, sizeof(ObjectSPB)))   // SPB Object identifier
        {
            aaddr = ahighaddr = (ahighaddr + 4093) && 0xFFFFF000;
        }

        /* if comment record inserted by a VM process, ignore */
        else if (NCMP(ObjectComment, buf, sizeof(ObjectComment)))
        {
            continue;    // Ignore record
        }

        /* If invalid object record, complain */
        else if (buf[0] == 0x02)
        {
            char msgbuf[4];

            bzero(msgbuf,sizeof(msgbuf));
            msgbuf[0] = guest_to_host(buf[1]);
            msgbuf[1] = guest_to_host(buf[2]);
            msgbuf[2] = guest_to_host(buf[3]);
            WRMSG( HHC02302, "W", argv[0], msgbuf );
        }

        /* Error if GOFF object (not handled yet) */
        else if (buf[0] == 0x03)
        {
            char msgbuf[4];

            bzero(msgbuf,sizeof(msgbuf));
            msgbuf[0] = guest_to_host(buf[1]);
            msgbuf[1] = guest_to_host(buf[2]);
            msgbuf[2] = guest_to_host(buf[3]);
            WRMSG( HHC02303, "E", "GOFF", argv[0], msgbuf );
            rc = -1;
            break;
        }

        /* Error if unrecognized record, but we'll ignore it (probable linkage editor command) */
        else
        {
            WRMSG( HHC02304, "W", argv[0], recno );
        }
    }

    /* Close file and issue status message */
    close (fd);

    if ( rc == 0 )
        WRMSG(HHC02249, "I" );

    release_lock(&sysblk.cpulock[sysblk.pcpu]);

    return rc;
}

/* VMFPLC2.C  (c) Copyright Ivan S. Warre, 2010                      */
/*            Utility to create VMFPLC2 formatted tapes              */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

#include "hstdinc.h"

#include "hercules.h"
#include "hetlib.h"
#include "ftlib.h"
#include "sllib.h"
#include "herc_getopt.h"

#define TAPE_BLOCKSIZE	4000
#define TAPE_BLKSIZE_MODULO	800

struct FST
{
/* Basic (short) FST */
	unsigned char fn[8];    /* File name */
	unsigned char ft[8];    /* File Type */
	unsigned char dt[4];    /* date : MMDDHHmm (In DCB) */
	unsigned char wp[2];    /* Write ptr */
	unsigned char rp[2];    /* Read ptr */
	unsigned char fm[2];    /* File Mode */
	unsigned char reccount[2];      /* 16 bit record count */
	unsigned char fcl[2];   /* First chain link */
	unsigned char recfm;    /* Record fmt (F or V) */
	unsigned char flag1;    /* Flags */
	unsigned char lrecl[4]; /* 32 bit record length */
	unsigned char db[2];    /* Data Block Count */
	unsigned char year[2];  /* Year (EBCDIC) */
/* The following 2 fields are VMFPLC2 specific */
        unsigned char lastsz[4]; /* Number of 800 byte blocks in last record */
        unsigned char numblk[4]; /* Count of complete 4000 byte records */
/* Extended EDF FST */
        unsigned char afop[4];  /* Alt File Origin Ptr */
        unsigned char adbc[4];  /* Alt Data Block Count */
        unsigned char aic[4];   /* Alt Item Count */
        unsigned char levels;   /* Levels of indirection */
        unsigned char ptrsz;    /* Size of Pointers */
        unsigned char adt[6];   /* Alt Date (YYMMDDHHMMSS) (In DCB) */
        unsigned char extra[4];  /* Reserved */
/* Global length MUST be 72 */
};

struct FST_BLOCK
{
	unsigned char hdr[5];
	struct FST fst;
};

struct options
{
	char	*cmd;
	char	*procfile;
	char	*tapefile;
#define VMFPLC2DUMP	0
#define VMFPLC2LOAD	1
#define VMFPLC2SCAN	2
	char	verb;
#define OPT_FLAG_EDF    0x01
        unsigned int flags;
        HETB    *hetfd;
};

int	usage(char *cmd)
{
	char	*bcmd;
	bcmd=basename(cmd);
	fprintf(stderr,"Usage :\n");
	fprintf(stderr,"\t%s DUMP filelist tapefile\n",bcmd);
	fprintf(stderr,"\t%s LOAD tapefile\n",bcmd);
	fprintf(stderr,"\t%s SCAN tapefile\n",bcmd);
	return 0;
}

int	parse_parms(int ac,char **av,struct options *opts)
{
	opts->cmd=basename(av[0]);
	if(ac<2)
	{
		usage(av[0]);
		return 1;
	}
	do
	{
		if(strcasecmp(av[1],"DUMP")==0)
		{
			opts->verb=VMFPLC2DUMP;
			break;
		}
		if(strcasecmp(av[1],"SCAN")==0)
		{
			opts->verb=VMFPLC2SCAN;
			break;
		}
		if(strcasecmp(av[1],"LOAD")==0)
		{
			opts->verb=VMFPLC2LOAD;
			break;
		}
		fprintf(stderr,"Invalid function %s\n",av[1]);
		usage(av[0]);
		return 1;
	} while(0);
	switch(opts->verb)
	{
		case VMFPLC2SCAN:
		case VMFPLC2LOAD:
		if(ac<3)
		{
			fprintf(stderr,"Tape file not specified\n");
			usage(av[0]);
			return 1;
		}
		opts->tapefile=av[2];
		break;
		case VMFPLC2DUMP:
		if(ac<3)
		{
			fprintf(stderr,"file list not specified\n");
			usage(av[0]);
			return 1;
		}
		if(ac<4)
		{
			fprintf(stderr,"Tape file not specified\n");
			usage(av[0]);
			return 1;
		}
		opts->procfile=av[2];
		opts->tapefile=av[3];
		break;
	}
	return 0;
}

static	char	*validchars="ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789$#@+-:_";
static  char	*validfmnum="0123456";

int	validate_fnft(char *s)
{
	int	i,j,found;
	for(i=0;s[i];i++)
	{
		for(found=0,j=0;validchars[j];j++)
		{
			if(s[i]==validchars[j])
			{
				found=1;
				break;
			}
		}
		if(!found) return i+1;
	}
	return 0;
}

char	*validate_cmsfile(char *fn,char *ft,char *fm)
{
	int	i;
	int	found;
	static	char	msg[1024];
	if(strlen(fn)>8) 
	{
		return "CMS File name too long";
	}
	if(strlen(ft)>8) 
	{
		return "CMS File type too long";
	}
	if(strlen(fm)>2) 
	{
		return "CMS File mode too long";
	}
	if((i=validate_fnft(fn))!=0)
	{
		sprintf(msg,"Invalid character in CMS file name at position %d",i);
		return msg;
	}
	if((i=validate_fnft(ft))!=0)
	{
		sprintf(msg,"Invalid character in CMS file type at position %d",i);
		return msg;
	}
	if(!isalpha(fm[0]))
	{
		return "CMS File mode must start with a letter";
	}
	if(strlen(fm)>1)
	{
		found=0;
		for(i=0;validfmnum[i];i++)
		{
			if(fm[1]==validfmnum[i]) 
			{
				found=1;
				break;
			}
		}
		if(!found)
		{
			return "CMS File mode number must be 1-6";
		}
	}
	return NULL;
}

void	str_toupper(char *s)
{
	int	i;
	for(i=0;s[i];i++)
	{
		if(islower(s[i])) s[i]=toupper(s[i]);
	}
}

struct	TAPE_BLOCK
{
	unsigned char	*data;
	size_t	sz;
	struct TAPE_BLOCK *next;
};

struct	TAPE_BLOCKS
{
	unsigned char *hdr;
	size_t	hdrsz;
	size_t	blksz;
        size_t  blk_modulo;     /* Must be a multiple of this */
	struct TAPE_BLOCK *first;
	struct TAPE_BLOCK *current;
};

int     write_tape_mark(struct options *opts)
{
        het_tapemark(opts->hetfd);
        return 0;
}

int	write_tape_block_data(struct options *opts,unsigned char *bfr,int sz)
{
        het_write(opts->hetfd,bfr,sz);
        return 0;
}

int	write_tape_block(struct options *opts,struct TAPE_BLOCK *tb,size_t mod,size_t hdrsz)
{
        size_t  sz;
        sz=(tb->sz-hdrsz)/mod;
        if((tb->sz-hdrsz)%mod)
        {
                sz++;
        }
        sz*=mod;
        sz+=hdrsz;
	return write_tape_block_data(opts,tb->data,sz);
}

int	write_tape_blocks(struct options *opts,struct TAPE_BLOCKS *tbs)
{
	int	rc;
	struct TAPE_BLOCK *tb;
	tb=tbs->first;
	while(tb)
	{
		rc=write_tape_block(opts,tb,tbs->blk_modulo,tbs->hdrsz);
		if(rc!=0) break;
		tb=tb->next;
	}
	return rc;
}
	
struct TAPE_BLOCKS *init_blocks(size_t blksz,size_t modulo,unsigned char *hdr,int hdrsz)
{
	struct TAPE_BLOCKS *tbs;
	tbs=malloc(sizeof(struct TAPE_BLOCKS));
	tbs->first=NULL;
	tbs->current=NULL;
	tbs->hdr=hdr;
	tbs->hdrsz=hdrsz;
	tbs->blksz=blksz;
        tbs->blk_modulo=modulo;
	return tbs;
}

/*
static void	hexdump(unsigned char *bfr,int sz)
{
	int i;
	for(i=0;i<sz;i++)
	{
		if(i%16==0)
		{
			if(i) printf("\n");
			printf("%4.4X :",i);
		}
		if(i%4==0)
		{
			printf(" ");
		}
		printf("%2.2X",bfr[i]);
	}
	if(i%16) printf("\n");
}
*/

void	append_data(struct TAPE_BLOCKS *tbs,unsigned char *bfr,size_t sz)
{
	int	dsz;
	int	of;
	struct TAPE_BLOCK *tb;
	of=0;
	while(sz)
	{
		if(tbs->current==NULL || tbs->current->sz==(tbs->blksz+tbs->hdrsz))
		{
			tb=malloc(sizeof(struct TAPE_BLOCK));
			tb->next=NULL;
                        tb->sz=0;
			if(tbs->first==NULL) 
			{
				tbs->first=tb;
				tbs->current=tb;
			}
			else
			{
				tbs->current->next=tb;
				tbs->current=tb;
			}
			tb->data=malloc(tbs->blksz+tbs->hdrsz);
                        memset(tb->data,0,tbs->blksz+tbs->hdrsz);
                        memcpy(tb->data,tbs->hdr,tbs->hdrsz);
                        tb->sz=tbs->hdrsz;
		}
		else
		{
			tb=tbs->current;
		}
		dsz=MIN(sz,(tbs->blksz+tbs->hdrsz)-tb->sz);
		memcpy(&tb->data[tb->sz],&bfr[of],dsz);
		tb->sz+=dsz;
		sz-=dsz;
		of+=dsz;
	}
}

void	free_blocks(struct TAPE_BLOCKS *tbs)
{
	struct TAPE_BLOCK *tb,*ntb;
	tb=tbs->first;
	while(tb)
	{
		free(tb->data);
		ntb=tb->next;
		free(tb);
		tb=ntb;
	}
	free(tbs);
}

struct	RECS
{
	size_t	bc;
	size_t	bts;
	off_t	cur_offset;
	struct TAPE_BLOCKS *blocks;
	char	recfm;
	int	reclen;
	int	reccount;
	size_t	filesz;
};


struct RECS *initrecs(char recfm,int recl,unsigned char *hdr,int hdrsz)
{
	struct RECS *recs;
	recs=malloc(sizeof(struct RECS));
	recs->blocks=init_blocks(TAPE_BLOCKSIZE,TAPE_BLKSIZE_MODULO,hdr,hdrsz);
	recs->reccount=0;
	recs->recfm=recfm;
	recs->reclen=recl;
	recs->filesz=0;
	return recs;
}

void addrecs(struct RECS *recs,unsigned char *bfr,int sz)
{
	unsigned char	recd[2];
	recs->filesz+=sz;
	switch(recs->recfm)
	{
		case 'V':
			recd[0]=sz&0xff;
			recd[1]=(sz>>8)&0xff;
			append_data(recs->blocks,recd,2);
			append_data(recs->blocks,bfr,sz);
			recs->reccount++;
			recs->reclen=MAX(recs->reclen,sz);
			break;
		case 'F':
			append_data(recs->blocks,bfr,sz);
			break;
	}
}

struct TAPE_BLOCKS *flushrecs(struct RECS *recs,int *recl,int *recc,size_t *filesz,unsigned char pad)
{
	struct TAPE_BLOCKS *blks;
	int	residual;
	if(recs->filesz==0)
	{
		append_data(recs->blocks,&pad,1);
		recs->filesz=1;
		if(recs->recfm=='V')
		{
			recs->reccount=1;
			recs->reclen=1;
		}
	}
	if(recs->recfm=='F')
	{
		residual=recs->reclen-recs->filesz%recs->reclen;
		residual=residual%recs->reclen;
		if(residual)
		{
			unsigned char *padbfr;
			recs->filesz+=residual;
			padbfr=malloc(residual);
			memset(padbfr,pad,residual);
			append_data(recs->blocks,padbfr,residual);
			free(padbfr);

		}
		recs->reccount=recs->filesz/recs->reclen;
	}
	*recc=recs->reccount;
	*recl=recs->reclen;
        *filesz=recs->filesz;
	blks=recs->blocks;
	free(recs);
	return blks;
}

static unsigned char plcd_hdr[5]={0x02,0xd7,0xd3,0xc3,0xc4};
static unsigned char plch_hdr[5]={0x02,0xd7,0xd3,0xc3,0xc8};

struct TAPE_BLOCKS *load_binary_file(char *infile,char recfm,int *recl,int *recc,size_t *filesz)
{
	int	rsz;
	FILE	*ifile;
	int	maxsize;
	unsigned char bfr[65536];
	struct RECS *recs;

	if(recfm=='V') maxsize=65535;
	else maxsize=65536;
	ifile=fopen(infile,"r");
	if(ifile==NULL)
	{
		perror(infile);
		return NULL;
	}

	recs=initrecs(recfm,*recl,plcd_hdr,5);
	while((rsz=fread(bfr,1,maxsize,ifile))!=0)
	{
		addrecs(recs,bfr,rsz);
	}
	fclose(ifile);
	return flushrecs(recs,recl,recc,filesz,0x40);
}

struct TAPE_BLOCKS *load_text_file(char *infile,char recfm,int *recl,int *recc,size_t *filesz)
{
	return NULL;
}

struct TAPE_BLOCKS *load_structured_file(char *infile,char recfm,int *recl,int *recc,size_t *filesz)
{
	return NULL;
}

struct TAPE_BLOCKS *load_file(char *infile,char *filefmt,char recfm,int *recl,int *recc,size_t *filesz)
{
	struct TAPE_BLOCKS *blks;
	switch(filefmt[0])
	{
		case 'B':
			blks=load_binary_file(infile,recfm,recl,recc,filesz);
			break;
		case 'T':
			blks=load_text_file(infile,recfm,recl,recc,filesz);
			break;
		case 'S':
			blks=load_structured_file(infile,recfm,recl,recc,filesz);
			break;
		default:
			fprintf(stderr,"*** INTERNAL ERROR 0001\n");
			return NULL;
	}
	return blks;
}

void host_to_guest_str(unsigned char *d,char *s)
{
	int	i;
	for(i=0;s[i];i++)
	{
		d[i]=host_to_guest(s[i]);
	}
}

void	big_endian_16(unsigned char *d,int s)
{
	d[0]=(s>>8) & 0xff;
	d[1]=s&0xff;
}

void	big_endian_32(unsigned char *d,int s)
{
	d[0]=(s>>24) & 0xff;
	d[1]=(s>>16) & 0xff;
	d[2]=(s>>8) & 0xff;
	d[3]=s&0xff;
}

unsigned char to_dcb(int v)
{
        return((v/10)*16+v%10);
}

struct FST_BLOCK *format_fst(char *fn,char *ft,char *fm,char recfm,int lrecl,int reccount,int filesz,time_t dt)
{
        int     numfull;
        int     lastsz;
        char    bfr[3];
        struct  tm *ttm;
	struct FST_BLOCK *fstb;

	fstb=malloc(sizeof(struct FST_BLOCK));
	memset(fstb,0,sizeof(struct FST_BLOCK));
	memcpy(fstb->hdr,plch_hdr,5);

        memset(fstb->fst.fn,0x40,8);
        memset(fstb->fst.ft,0x40,8);
        memset(fstb->fst.fm,0x40,2);

	host_to_guest_str(fstb->fst.fn,fn);
	host_to_guest_str(fstb->fst.ft,ft);
	host_to_guest_str(fstb->fst.fm,fm);

        ttm=localtime(&dt);
        fstb->fst.dt[0]=to_dcb(ttm->tm_mon);
        fstb->fst.dt[1]=to_dcb(ttm->tm_mday);
        fstb->fst.dt[2]=to_dcb(ttm->tm_hour);
        fstb->fst.dt[3]=to_dcb(ttm->tm_min);
        snprintf(bfr,3,"%2.2u",ttm->tm_year%100);
        bfr[2]=0;
        host_to_guest_str(fstb->fst.year,bfr);

        fstb->fst.adt[0]=to_dcb(ttm->tm_year%100);
        fstb->fst.adt[1]=to_dcb(ttm->tm_mon);
        fstb->fst.adt[2]=to_dcb(ttm->tm_mday);
        fstb->fst.adt[3]=to_dcb(ttm->tm_hour);
        fstb->fst.adt[4]=to_dcb(ttm->tm_min);
        fstb->fst.adt[5]=to_dcb(ttm->tm_sec);

	fstb->fst.recfm=host_to_guest(recfm);

	big_endian_32(fstb->fst.lrecl,lrecl);
	big_endian_16(fstb->fst.reccount,reccount);
        big_endian_16(fstb->fst.wp,reccount+1);
        big_endian_16(fstb->fst.rp,1);
	big_endian_32(fstb->fst.aic,reccount);

        numfull=filesz/TAPE_BLOCKSIZE;
        lastsz=filesz%TAPE_BLOCKSIZE;
        if(lastsz%TAPE_BLKSIZE_MODULO==0)
        {
                lastsz/=TAPE_BLKSIZE_MODULO;
        }
        else
        {
                lastsz=lastsz/TAPE_BLKSIZE_MODULO+1;
        }

        big_endian_32(fstb->fst.numblk,numfull);
        big_endian_32(fstb->fst.lastsz,lastsz);


	return  fstb;
}

int	process_entry(struct options *opts,char *orec,int recno)
{
	int	ignore;
	int	badentry;
	char	*fn,*ft,*fm;
	char	*recfm,*lrecl;
	char	*filefmt;
	char	*infile;
	int	reclen;
	char	*msg;
	char	*rec;
	char	*endptr;
	int	reccount;
        size_t  filesz;
	int	rc;
	struct TAPE_BLOCKS *blks;
	struct FST_BLOCK *fstb;
        struct stat stt;
	ignore=0;
	rec=strdup(orec);
	do
	{
		badentry=1;
		fn=strtok(rec," ");
		if(fn==NULL)
		{
			/* blank line : ignore */
			ignore=1;
			break;
		}
		if(strcmp(fn,"*")==0)
		{
			/* comment : ignore */
			ignore=1;
			break;
		}
		ft=strtok(NULL," ");
		if(ft==NULL) 
		{
			msg="File type missing";
			break;
		}
		fm=strtok(NULL," ");
		if(fm==NULL) 
		{
			msg="File mode missing";
			break;
		}
		str_toupper(fn);
		str_toupper(ft);
		str_toupper(fm);
		msg=validate_cmsfile(fn,ft,fm);
		if(msg!=NULL) break;
		recfm=strtok(NULL," ");
		if(recfm==NULL)
		{
			msg="Record format missing";
			break;
		}
		str_toupper(recfm);
		if(strlen(recfm)>1 || ( recfm[0]!='F' && recfm[0]!='V'))
		{
			msg="Record format must be F or V";
			break;
		}
		if(recfm[0]=='F')
		{
			lrecl=strtok(NULL," ");
			if(lrecl==NULL)
			{
				msg="Logical Record Length missing for F record format file";
				break;
			}
			errno=0;
			reclen=strtoul(lrecl,&endptr,0);
			if(errno || endptr[0]!=0)
			{
				msg="Logical Record Length must be a numeric value";
				break;
			}
			if(reclen<1 || reclen>65535)
			{
				msg="Logical Record Length must be between 1 and 65535";
				break;
			}
				
		}
		else
		{
			reclen=0;
		}
		filefmt=strtok(NULL," ");
		if(filefmt==NULL)
		{
			msg="File format missing";
			break;
		}
		str_toupper(filefmt);
		if(strlen(filefmt)>1 || ( filefmt[0]!='B' && filefmt[0]!='S' && filefmt[0]!='T'))
		{
			msg="File format must be B (Binary) S (Structured) or T (Text)";
			break;
		}
		infile=strtok(NULL,"");	/* allow spaces here */
		if(infile==NULL)
		{
			msg="Input file name missing";
			break;
		}
		badentry=0;
	} while(0);
	if(ignore) 
	{
		free(rec);
		return 0;
	}
	if(badentry)
	{
		fprintf(stderr,">>> %s\n",orec);
		fprintf(stderr,"*** Bad entry at line %d in file \"%s\"\n",recno,opts->procfile);
		fprintf(stderr,"*** Cause : %s\n",msg);
		free(rec);
		return 1;
	}
	printf(" %-8s %-8s %-2s\n",fn,ft,fm);
	blks=load_file(infile,filefmt,recfm[0],&reclen,&reccount,&filesz);
	if(blks==NULL)
	{
		free(rec);
		return 1;
	}
        stat(infile,&stt);
	fstb=format_fst(fn,ft,fm,recfm[0],reclen,reccount,filesz,stt.st_mtime);
	if(fstb==NULL)
	{
		free(rec);
		free_blocks(blks);
		return 1;
	}
	rc=write_tape_block_data(opts,(unsigned char *)fstb,sizeof(struct FST_BLOCK));
	rc=write_tape_blocks(opts,blks);


	free(rec);
	free(fstb);
	free_blocks(blks);
	return 0;
}

int	process_procfile(struct options *opts)
{
	int	rc;
	FILE	*pfile;
	char	recbfr[65536];
	char	*rec;
	int	recno=0;
	int	errcount=0;
	pfile=fopen(opts->procfile,"r");
	if(pfile==NULL)
	{
		perror(opts->procfile);
		return 1;
	}
	while((rec=fgets(recbfr,sizeof(recbfr),pfile))!=NULL)
	{
		recno++;
		rec[strlen(rec)-1]=0;
		rc=process_entry(opts,rec,recno);
		if(rc!=0) 
		{
			fprintf(stderr,"*** Entry ignored\n\n");
			errcount++;
		}
	}
	fclose(pfile);
	if(errcount>0)
	{
		fprintf(stderr,"%d errors encountered\n",errcount);
	}
        rc=write_tape_mark(opts);
        rc=write_tape_mark(opts);
	return errcount?1:0;
}

int     open_tapefile(struct options *opts,int w)
{
        int     rc;
        if(w)
        {
                rc=het_open(&opts->hetfd,opts->tapefile,HETOPEN_CREATE);
                rc=het_cntl(opts->hetfd,HETCNTL_SET|HETCNTL_COMPRESS,1);
        }
        else
        {
                rc=het_open(&opts->hetfd,opts->tapefile,HETOPEN_READONLY);
                rc=het_cntl(opts->hetfd,HETCNTL_SET|HETCNTL_DECOMPRESS,1);
        }
        return 0;
}

int	dodump(struct options *opts)
{
	int	rc;
	printf(" DUMPING...\n");
        open_tapefile(opts,1);
	rc=process_procfile(opts);
	return rc;
}
int	doload(struct options *opts)
{
	printf("LOAD function not implemented yet\n");
	return 0;
}
int	doscan(struct options *opts)
{
	printf("SCAN function not implemented yet\n");
	return 0;
}

int main(int ac,char **av)
{
	int	rc;
	struct options opts;
	if(parse_parms(ac,av,&opts)!=0)
	{
		return 1;
	}
	switch(opts.verb)
	{
		case VMFPLC2DUMP:
			rc=dodump(&opts);
			break;
		case VMFPLC2LOAD:
			rc=doload(&opts);
			break;
		case VMFPLC2SCAN:
			rc=doscan(&opts);
			break;
                default:
                        rc=0;
                        break;
	}
	return rc;
}

/* load VM image */

#define BIOS		0	/* Can only be used under the BIOS. */
#define nil 		0
#define _POSIX_SOURCE	1
#define _MINIX		1

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <lib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <alloca.h>
#include <a.out.h>
#include <timers.h>

#include <minix/config.h>
#include <minix/ipc.h>
#include <minix/syslib.h>
#include <minix/com.h>
#include <minix/const.h>
#include <minix/type.h>
#include <minix/syslib.h>
#include <minix/sysutil.h>

#define LOADVMIMG 		1
#include <minix/vmachine.h>
#include <minix/endpoint.h>

#include "../boot/rawfs.h"

#ifdef EXTERN
#undef EXTERN
#endif
#define EXTERN

#include "../kernel/config.h"
#include "../servers/vmm/const.h"

#define running_vm 		fl_vmid

#include "../kernel/const.h"
#include "../kernel/config.h"
#include "../kernel/type.h"
#include "../kernel/proc.h"
#include "../kernel/ipc.h"

#include "../servers/vmm/mhmacros.h"

#define click_shift	clck_shft	/* 7 char clash with click_size. */

/* Some kernels have extra features: */
#define K_I386	 0x0001	/* Make the 386 transition before you call me. */
#define K_CLAIM	 0x0002	/* I will acquire my own bss pages, thank you. */
#define K_CHMEM  0x0004	/* This kernel listens to chmem for its stack size. */
#define K_HIGH   0x0008	/* Load mm, fs, etc. in extended memory. */
#define K_HDR	 0x0010	/* No need to patch sizes, kernel uses the headers. */
#define K_RET	 0x0020	/* Returns to the monitor on reboot. */
#define K_INT86	 0x0040	/* Requires generic INT support. */
#define K_MEML	 0x0080	/* Pass a list of free memory. */
#define K_BRET	 0x0100	/* New monitor code on shutdown in boot parameters. */
#define K_ALL	 0x01FF	/* All feature bits this monitor supports. */

int n_procs;			/* Number of processes. */
process_t process[PROCESS_MAX];

/* struc image_header defined en image.h and sub-struct exec defined in a.out.h */
imginfo_t 	img;

#define KERNEL0		0	/* Kernel index into image file */
#define FS2			2	/* The third must be fs. */

/* Magic numbers in process' data space. */
#define MAGIC_OFF	0	/* Offset of magic # in data seg. */
#define CLICK_OFF	2	/* Offset in kernel text to click_shift. */
#define FLAGS_OFF	4	/* Offset in kernel text to flags. */
#define KERNEL_D_MAGIC	0x526F	/* Kernel magic number. */

/* Offsets of sizes to be patched into kernel and fs. */
#define P_SIZ_OFF	0	/* Process' sizes into kernel data. */
#define P_INIT_OFF	4	/* Init cs & sizes into fs data. */

char lcl_ref; 		/* reference local address */
char *rmt_ref; 		/*address of the remote reference variable  */
long diff;			/* difference */
FILE *fp, *fp0, *fp1;
int fl_vmid;
message m;
message *m_ptr;
int px_vmid;			/* VMID of the VM that this proxy represent */
int px_ep;

#define 	NOT_INITIALIZED	(0)
PUBLIC struct proc proc[NR_TASKS + NR_PROCS]; 

unsigned click_shift;
unsigned click_size;	/* click_size = Smallest kernel memory object. */
unsigned k_flags;	/* Not all kernels are created equal. */
long dyn_size;
u32_t vsec;
char boot_params[SECTOR_SIZE + 1]; /* space reserved for boot parameters */
char prtbuf[SECTOR_SIZE];

#define between(a, c, z)	((unsigned) ((c) - (a)) <= ((z) - (a)))
#define align(a, n)	(((u32_t)(a) + ((u32_t)(n) - 1)) & ~((u32_t)(n) - 1))
#define raw_clear		bzero
#define raw_copy		memcpy
#define 	OK		0
#define BITS_BYTE 8
#define NO_DRIVER (-1)

_PROTOTYPE(int main, (int argc, char *argv []));
_PROTOTYPE(int no_driver, (int driver_nr));

_PROTOTYPE(int tty_driver, (int driver_nr));
_PROTOTYPE(int tty_init, (void));

_PROTOTYPE(int floppy_driver, (int driver_nr));
_PROTOTYPE(int floppy_init, (void));


_PROTOTYPE (int (*driver_vec[BITS_BYTE*sizeof(long)]), (int driver_nr) ) = {
	no_driver,	/*  0 = unused	*/
	no_driver,	/*  1 = unused	*/
	no_driver,	/*  2 = unused	*/
	no_driver,	/*  3 = unused	*/
	no_driver,	/*  4 = unused	*/
	no_driver,	/*  5 = TTY_PROC_NR	*/
	no_driver,	/*  6 = unused	*/
	no_driver,	/*  7 = unused	*/
	no_driver,	/*  8 = unused	*/
	no_driver,	/*  9 = unused	*/
	no_driver,	/*  10 = unused	*/
	no_driver,	/*  11 = unused	*/
	no_driver,	/*  12 = unused	*/
	floppy_driver,	/*  13 =  FLOPPY_PROC_NR	*/
	no_driver,	/*  14 = unused	*/
	no_driver,	/*  15 = unused	*/
	no_driver,	/*  16 = unused	*/
	no_driver,	/*  17 = unused	*/
	no_driver,	/*  18 = unused	*/
	no_driver,	/*  19 = unused	*/
	no_driver,	/*  20 = unused	*/
	no_driver,	/*  21 = unused	*/
	no_driver,	/*  22 = unused	*/
	no_driver,	/*  23 = unused	*/
	no_driver,	/*  24 = unused	*/
	no_driver,	/*  25 = unused	*/
	no_driver,	/*  26 = unused	*/
	no_driver,	/*  27 = unused	*/
	no_driver,	/*  28 = unused	*/
	no_driver,	/*  29 = unused	*/
	no_driver,	/*  30 = unused	*/
	no_driver,	/*  31 = unused	*/
};

struct rmt2lcl_s {
	char *ref;
	long diff;
} ; 
typedef struct rmt2lcl_s rmt2lcl_t;
				
rmt2lcl_t r2l[NR_PROCS];
int floppy_nr, floppy_ep;
phys_bytes floppy_phys;
struct priv priv_tab[NR_SYS_PROCS];
unsigned long virtual_bm;	/* bitmap of virtual devices that notifies the proxy */


/*---------------------------------------------------------------------------------------*/
/*					rvirt_lvirt							*/
/* convert the virtual address referred to a process inside VMx 		*/
/* into a virtual address referred to the proxy inside VM0			*/
/*---------------------------------------------------------------------------------------*/
char *rvirt_lvirt(char *rvirt, int p_nr) 
{
	char *rmt_virt; 
	rmt_virt =  (char *) &lcl_ref;
	rmt_virt 	+= r2l[p_nr].diff;
	rmt_virt	+= ( (long) rvirt - (long) r2l[p_nr].ref);
	return(rmt_virt);
}

/*---------------------------------------------------------------------------------------*/
/*					get_refdiff							*/
/* get reference remote address and address difference between  the proxy */
/* and a  remote (in inside VM) process address 						*/
/*---------------------------------------------------------------------------------------*/
int get_refdiff(int rproc, char *raddr, int rbytes ) 
{
	int rcode; 
	phys_bytes rmt_phys, lcl_phys;

	/* Get REMOTE  physical address of the future remote reference variable */
	rcode = sys_umapvm(fl_vmid, rproc, D,  (vir_bytes) raddr, 
				(vir_bytes) rbytes, &rmt_phys);
	if(rcode) return(rcode);

	fprintf(fp0,"floppy_driver: DEV_READ: raddr=%X rmt_phys=%X\n",
				raddr, rmt_phys);
					
	/* Get LOCAL physical address of local reference variable  */
	rcode = sys_umapvm(VM0, SELF, D,  (vir_bytes) &lcl_ref, 
				(vir_bytes) sizeof(char), &lcl_phys);
	if(rcode) return(rcode);
	
	fprintf(fp0,"floppy_driver: DEV_READ: &lcl_ref=%X lcl_phys=%X\n",
				&lcl_ref, lcl_phys);
				
	/* Calculate the difference among both physical addresses */
	r2l[rproc].diff = 	(rmt_phys - lcl_phys);
	/* save the remote reference */
	r2l[rproc].ref = (char *) raddr;
	fprintf(fp0,"floppy_driver: DEV_READ: proc=%d diff=%X ref=%X \n", 
	rproc, r2l[rproc].diff, r2l[rproc].ref);
	return(OK);
}


/*---------------------------------------------------------------------------------------*/
/*					get_imagetab							*/
/*---------------------------------------------------------------------------------------*/
int get_imagetab(void )
{
	int i;
	char *start_addr, *end_addr, *ptr;
	struct bimage *bi_start, *bi_ptr;

	start_addr = (char*) img.hdr[KERNEL0].process.a_data; /* start of KERNEL DATA	*/
	end_addr   = (char*) img.hdr[KERNEL0].process.a_bss;  /* end  of KERNEL DATA	*/
	
	for( ptr = start_addr; ptr < end_addr; ptr++) {
		bi_start = (struct bimage *) ptr;
		if ( !strcmp((char*)&bi_start->proc_name, "idle" ) ){
			MHDEBUG("FIND IMAGE TABLE: idle slot at address=%X \n", ptr);
			/* copy the boot image table into the image struct */
			bi_ptr = bi_start;
			for( i = 0; i < (n_procs+NR_TASKS-1); i++) {
				memcpy((char*) &img.image[i],(char*)bi_ptr,sizeof(struct bimage));
				bi_ptr++; 
			}
			return(OK);
		}
	}

	MHDEBUG("IDLE NOT FOUND\n");
	return(-1);
}

/*---------------------------------------------------------------------------------------*/
/*					selected							*/
/*---------------------------------------------------------------------------------------*/
int selected(char *name)
/* True iff name has no label or the proper label. */
{
	char *colon, *label;
	int cmp;

	if ((colon= strchr(name, ':')) == nil) return 1;
/*	if ((label= b_value("label")) == nil) return 1; */

	*colon= 0;
	cmp= strcmp(label, name);
	*colon= ':';
	return cmp == 0;
}

/*---------------------------------------------------------------------------------------*/
/*					get_sector							*/
/*---------------------------------------------------------------------------------------*/							
char *get_sector(u32_t vsec)
/* Read a sector "vsec" from the image into memory and return its address.
 * Return nil on error.  
  */
{
	int rbytes;
#define SECBUFS 16
	static char buf[SECBUFS * SECTOR_SIZE];

	fseek(fp, (vsec*SECTOR_SIZE), SEEK_SET);
	rbytes = fread(buf, sizeof(char), SECTOR_SIZE, fp);
	if ( rbytes  == 0) {
		errno= 0;
		return NULL;
	}
MHDEBUG(" [%d]",vsec);
	return buf;
}

/*---------------------------------------------------------------------------------------*/
/*					pretty_image							*/
/*---------------------------------------------------------------------------------------*/					
void pretty_image(char *image)
/* Pretty print the name of the image to load.  Translate '/' and '_' to
 * space, first letter goes uppercase.  An 'r' before a digit prints as
 * 'revision'.  E.g. 'minix/1.6.16r10' -> 'Minix 1.6.16 revision 10'.
 * The idea is that the part before the 'r' is the official Minix release
 * and after the 'r' you can put version numbers for your own changes.
 */
{
	int up= 0, c;

	while ((c= *image++) != 0) {
		if (c == '/' || c == '_') c= ' ';

		if (c == 'r' && between('0', *image, '9')) {
			printf(" revision ");
			continue;
		}
		if (!up && between('a', c, 'z')) c= c - 'a' + 'A';

		if (between('A', c, 'Z')) up= 1;

		putchar(c);
	}
}

/*---------------------------------------------------------------------------------------*/
/*					get_segment							*/
/*---------------------------------------------------------------------------------------*/
int get_segment(u32_t *vsec, long *size, u32_t *addr, u32_t limit)
/* Read *size bytes starting at virtual sector *vsec to memory at *addr. */
{
	char *buf;
	size_t cnt, n;

	cnt= 0;
	while (*size > 0) {
		if (cnt == 0) {
			if ((buf= get_sector((*vsec)++)) == nil) return 0;
			cnt= SECTOR_SIZE;
		}
		if (*addr + click_size > limit) { errno= ENOMEM; return 0; }
		n= click_size;
		if (n > cnt) n= cnt;
		raw_copy((void *)*addr, buf, n);
		*addr+= n;
		*size-= n;
		buf+= n;
		cnt-= n;
	}

	/* Zero extend to a click. */
	n= align(*addr, click_size) - *addr;
	raw_clear((void*) *addr, n);
	*addr+= n;
	*size-= n;
	return 1;
}

/*---------------------------------------------------------------------------------------*/
/*					get_clickshift							*/
/*---------------------------------------------------------------------------------------*/
int get_clickshift(u32_t ksec, struct image_header *hdr)
/* Get the click shift and special flags from kernel text. */
{
	char *textp;

	if ((textp= get_sector(ksec)) == NULL) return 0;

	if (hdr->process.a_flags & A_PAL) textp+= hdr->process.a_hdrlen;
	click_shift= * (u16_t *) (textp + CLICK_OFF);
	k_flags= * (u16_t *) (textp + FLAGS_OFF);

	if ((k_flags & ~K_ALL) != 0) {
		printf("%s requires features this monitor doesn't offer\n",
			hdr->name);
		return 0;
	}

	if (click_shift < HCLICK_SHIFT || click_shift > 16) {
		printf("%s click size is bad\n", hdr->name);
		errno= 0;
		return 0;
	}

	click_size= 1 << click_shift;

MHDEBUG("get_clickshift click_size=%d k_flags=%X \n",click_size,k_flags);

	return(1);
}

/*---------------------------------------------------------------------------------------*/
/*					proc_size								*/
/*---------------------------------------------------------------------------------------*/
u32_t proc_size(struct image_header *hdr)
/* Return the size of a process in sectors as found in an image. */
{
	u32_t len= hdr->process.a_text;

	if (hdr->process.a_flags & A_PAL) len+= hdr->process.a_hdrlen;
	if (hdr->process.a_flags & A_SEP) len= align(len, SECTOR_SIZE);
	len= align(len + hdr->process.a_data, SECTOR_SIZE);

	return len >> SECTOR_SHIFT;
} 

/*---------------------------------------------------------------------------------------*/
/*					patch_sizes							*/
/*---------------------------------------------------------------------------------------*/
void patch_sizes(void)
/* Patch sizes of each process into kernel data space, kernel ds into kernel
 * text space, and sizes of init into data space of fs.  All the patched
 * numbers are based on the kernel click size, not hardware segments.
 */
{
	u16_t text_size, data_size;
	int i;
	process_t *procp, *initp;
	u16_t *doff;

MHDEBUG("ENTRANDO patch_sizes\n");
	if (k_flags & K_HDR) return;	/* Uses the headers.*/
	/* Patch text and data sizes of the processes into kernel data space. */
	doff = (u16_t *) (process[KERNEL0].data + P_SIZ_OFF);

	for (i= 0; i < n_procs; i++) {
MHDEBUG("patching process %d:", i);
		procp= &process[i];
		text_size= (procp->ds - procp->cs) >> click_shift;
		data_size= (procp->end - procp->ds) >> click_shift;
MHDEBUG("text_size=%d data_size=%d ",text_size,data_size);

		/* Two words per process, the text and data size: */
		*doff = (u16_t) text_size;  
MHDEBUG(" %d",*doff);
		doff++;
		
		*doff = (u16_t) data_size; 
MHDEBUG(" %d \n",*doff);
		doff++;
		initp= procp;	/* The last process must be init. */
	}


	if (k_flags & (K_HIGH|K_MEML)) return;	/* Doesn't need FS patching. */

	/* Patch cs and sizes of init into fs data. */
 	doff = (u16_t *) (process[FS2].data + P_INIT_OFF);
    *doff = (u16_t) initp->cs >> click_shift; *doff++;
	*doff = (u16_t) text_size;*doff++;
	*doff = (u16_t) data_size;*doff++;
}

/*---------------------------------------------------------------------------------------*/
/*					read_image								*/
/* get image file size	(global variable img.bytes)						*/
/* allocate dynamic memory (global variable img.addr)					*/
/* dump the image into memory									*/
/*---------------------------------------------------------------------------------------*/
int read_image(void)
{
	int rcode, pnr, banner = 0;
	u32_t fcount, hdrpos, maddr;
	int rbytes, rem_bytes;
	long a_text, a_data, a_bss, a_stack;
	u32_t limit, n;
	process_t *procp;		/* Process under construction. */
	char *buf;
	u16_t *pmag;

 fp  = fopen(img.name,"r");
 if (fp == NULL)
 	MHERROR("fopen: rcode=%d\n", -1);
	
 /*--------------- get the image file size  ------*/
 rcode = stat(img.name, &img.stat);
 if (rcode != 0) 
	MHERROR("stat: rcode=%d\n", -1);

 MHDEBUG("img.stat.st_size=%d\n",img.stat.st_size);  	
 img.bytes = img.stat.st_size;
 img.sectors= (img.bytes  + SECTOR_SIZE - 1) >> SECTOR_SHIFT;
 
 /*--------------- load image file into mermory  ------*/
 pnr = 0;						/* process number 	*/
 rem_bytes = img.bytes;			/*rem_bytes: remanining bytes of image file */
								/* img.bytes: total bytes of image file */
								
 maddr = (u32_t)img.addr; /* img.addr: start address of image in memory */  	
							/* fcount: pointer to image bytes in file */
							/* hdrpos: pointer to the header in file */
							/* maddr: pointer to image byte in memory */
 hdrpos = 0; 
 limit = dyn_size;
 vsec= 0;			/* Load this sector from image next. */
MHDEBUG("addr=%d/%X limit= %d\n",maddr, maddr, limit);

 printf("\nLoading ");
 pretty_image(img.name);
 printf(".\n\n");
 
	for (pnr = 0; vsec < img.sectors; pnr++) {
		if (pnr == PROCESS_MAX) {
			errno= 0;
			printf("There are more then %d programs in %s\n",
				PROCESS_MAX, img.name);
			return(-1);
		}

		procp= &process[pnr];

MHDEBUG("Read header \n");
		/* Read header. */
		for (;;) {
			if ((buf= get_sector(vsec++)) == NULL) {
MHDEBUG("ERROR get_sector \n");
				return(-1);
			}
			memcpy(&img.hdr[pnr], buf, sizeof(struct image_header));

			if (BADMAG(img.hdr[pnr].process)) { 
MHDEBUG("ERROR BADMAG \n");
			errno= ENOEXEC; 
			return(-1);
			}
				
			if (selected(img.hdr[pnr].name)) {
MHDEBUG("name[%d]:%s proc_size=%d \n",
					pnr, 
					img.hdr[pnr].name, 
					(proc_size(&img.hdr[pnr])*SECTOR_SIZE));
				break;
			}

			/* Bad label, skip this process. */
MHDEBUG("Bad label, skip this process %s\n",img.hdr[pnr].name); 
			vsec+= proc_size(&img.hdr[pnr]);
		}	

MHDEBUG("Sanity check \n");
		
		/* Sanity check: an 8086 can't run a 386 kernel. */
		if (img.hdr[pnr].process.a_cpu != A_I80386) {
			printf("Image file not for cpu >= 386. Image for cpu=80%ld\n",
				img.hdr[pnr].process.a_cpu);
			errno= 0;
				return(-1);
		}

MHDEBUG("Get the click shift \n");
		
		/* Get the click shift from the kernel text segment. */
		if (pnr == KERNEL0) {  /*kernel */
			if (!get_clickshift(vsec, &img.hdr[pnr])) 
				return(-1);
			maddr= align(maddr, click_size);
		}

		/* Save a copy of the header for the kernel, with a_syms
		 * misused as the address where the process is loaded at.
		 */
		img.hdr[pnr].process.a_syms= maddr;
		raw_copy(&process[pnr], &img.hdr[pnr].process, A_MINHDR); 

		if (!banner) {
			printf("     cs       ds     text     data      bss       sym");
			if (k_flags & K_CHMEM) printf("    stack");
			printf("\n");
			banner= 1;
		}

		/* Segment sizes. */
		a_text= img.hdr[pnr].process.a_text;
		a_data= img.hdr[pnr].process.a_data;
		a_bss= img.hdr[pnr].process.a_bss;
		if (k_flags & K_CHMEM) {
			a_stack= img.hdr[pnr].process.a_total - a_data - a_bss;
			if (!(img.hdr[pnr].process.a_flags & A_SEP)) a_stack-= a_text;
		} else {
			a_stack= 0;
		}

		/* Collect info about the process to be. */
		procp->cs= maddr;

		/* Process may be page aligned so that the text segment contains
		 * the header, or have an unmapped zero page against vaxisms.
		 */
		procp->entry= img.hdr[pnr].process.a_entry;
		if (img.hdr[pnr].process.a_flags & A_PAL) a_text+= img.hdr[pnr].process.a_hdrlen;
		if (img.hdr[pnr].process.a_flags & A_UZP) procp->cs-= click_size;


		/* Separate I&D: two segments.  Common I&D: only one. */
		if (img.hdr[pnr].process.a_flags & A_SEP) {
MHDEBUG("Separate I&D Read the text segment. \n");
			/* Read the text segment. */
			if (!get_segment(&vsec, &a_text, &maddr, limit))
				return(-1);

			/* The data segment follows. */
			procp->ds= maddr;
			if (img.hdr[pnr].process.a_flags & A_UZP) procp->ds-= click_size;
			procp->data= maddr;
		} else {
MHDEBUG("Common I&D Add text to data to form one segment. \n");
			/* Add text to data to form one segment. */
			procp->data= maddr + a_text;
			procp->ds= procp->cs;
			a_data+= a_text;
		}

MHDEBUG("Read the data segment \n");

		/* Read the data segment. */
		if (!get_segment(&vsec, &a_data, &maddr, limit)) 
			return(-1);

		/* Make space for bss and stack unless... */
		if (pnr != KERNEL0 && (k_flags & K_CLAIM)) 
			a_bss= a_stack= 0;

		printf("%d %07lx  %07lx %8ld %8ld %8ld  %8ld",
		    pnr,
			procp->cs, procp->ds,
			img.hdr[pnr].process.a_text, 
			img.hdr[pnr].process.a_data,
			img.hdr[pnr].process.a_bss,
			img.hdr[pnr].process.a_syms	
			);


		if (k_flags & K_CHMEM) 
			printf(" %8ld", a_stack);

		printf("  %s\n", img.hdr[pnr].name);

		/* Note that a_data may be negative now, but we can look at it
		 * as -a_data bss bytes.
		 */

		/* Compute the number of bss clicks left. */
		a_bss+= a_data;
		n= align(a_bss, click_size);
		a_bss-= n;

		/* Zero out bss. */
		if (maddr + n > limit) {
MHDEBUG("maddr=%d + n=%d (%d) > limit(%d) \n",maddr,n,(maddr+n),limit);
			errno= ENOMEM;
			return(-1);
			}
		raw_clear((void*) maddr, n);
		maddr+= n;

		/* And the number of stack clicks. */
		a_stack+= a_bss;
		n= align(a_stack, click_size);
		a_stack-= n;

		/* Add space for the stack. */
		maddr+= n;

		/* Process endpoint. */
		procp->end= maddr;
		}

 MHDEBUG("\n");

 
 /*--------------- close the image file   ------*/
 fclose(fp);
 
 	if ( pnr == 0) {
		printf("There are no programs in %s\n", img.name);
		errno= 0;
		return(-1);
	}
	n_procs = pnr;		/* number of elements in process[] table */

	/* Check the kernel magic number. */
	pmag =(u16_t *) (process[KERNEL0].data + MAGIC_OFF);
	if ( *pmag != KERNEL_D_MAGIC) {
		printf("Kernel magic number is incorrect %d != %d\n", *pmag,KERNEL_D_MAGIC );
		errno= 0;
		return(-1);
	}
		
	/* Patch sizes, etc. into kernel data. */
	patch_sizes();
	
MHDEBUG("read_image Salida Normal \n");
	
 return(0);
}

/*---------------------------------------------------------------------------------------*/
/*					floppy_init					*/
/*---------------------------------------------------------------------------------------*/
int floppy_init(void)
{
	int rcode, i;
  
	_mhdebug("floppy_init: SECTOR_SIZE", SECTOR_SIZE);

	fp1 = fopen("mhyper.flp","r+");
	if( fp1 == NULL )
		_mhdebug("fprintf: fopen errno", errno );
		

	/* get the physical address of the SELF data segment */
	rcode = sys_umap(SELF, D, (vir_bytes) 0x0000, 1, &floppy_phys);
	_mhdebug("floppy_driver: sys_umap rcode", rcode);
	fprintf(fp0,"floppy_phys=%lX\n", floppy_phys);

#ifdef ANULADO
	for( i= 0; i < NR_PROCS; i++)		
		r2l[i].diff = NOT_INITIALIZED;
		
	/*dump the SELF process descriptor to cache  */
	floppy_ep = getprocnr();
	_mhdebug("floppy_driver: endpoint", floppy_ep);
	floppy_nr = _ENDPOINT_P(floppy_ep);
	floppy_ptr = &proc[floppy_nr+NR_TASKS]; 
	rcode = sys_getproc(floppy_ptr, SELF);
	if( rcode ) {
		_mhdebug("floppy_driver: sys_getproc rcode", rcode);
	}else{
		fprintf(fp0,PROC_FORMAT, PROC_FIELDS(floppy_ptr));
	}
	
	register struct proc *rp;
	static struct proc *oldrp;
  
	rp = BEG_PROC_ADDR;
	oldrp = BEG_PROC_ADDR;
  /* First obtain a fresh copy of the current process table. */
	if ((rcode = sys_getproctab(rp)) != OK) {
		fprintf(fp0,"floppy_driver:warning: couldn't get copy of process table:", rcode);
        return(rcode);
	}
#endif /* ANULADO */
	
	return(OK);
}

/*---------------------------------------------------------------------------------------*/
/*					floppy_driver					*/
/*---------------------------------------------------------------------------------------*/
int floppy_driver(int driver_nr)
{
	int i, val, rcode, bytes, count, offset, rqtr_nr;
	int fl_rqst, fl_ep, fl_dev, fl_bytes, fl_nr_req;
	long fl_offset;
	message m_in, m_out;
	message *mi_ptr, *mo_ptr;
	char *fl_address, *rmt_virt; 
	iovec_t *iop, *iov_end;
	struct proc *rqtr_ptr;

static iovec_t iovec[NR_IOREQS];
static  prtbuf[256];
static	devbuf[SECTOR_SIZE*2];
	
	/*
	 * Driver number NO_DRIVER means the command to execute the driver initialization 
	 */

	if( driver_nr == NO_DRIVER) return(floppy_init());

	_mhdebug("floppy_driver: driver_nr", driver_nr);
	
	/* set pointer to message IN and message out */
	mi_ptr = &m_in;
	mo_ptr = &m_out;
	m_ptr = &m;

	rcode = recvas(ANY, mi_ptr, driver_nr);
	_mhdebug("floppy_driver: recvas rcode", rcode);
	if( rcode) {
		return(rcode);
	}

	fprintf(fp0,"floppy_driver:" MSG1_FORMAT, MSG1_FIELDS(mi_ptr));

	
	if(mi_ptr->m_type < DEV_RQ_BASE) {
		_mhdebug("floppy_driver: bad request", mi_ptr->m_type);
		return(EINVAL);
	}
	
	fl_rqst = mi_ptr->m_type;
	switch(fl_rqst) {
		case DEV_OPEN:   
			fl_ep = mi_ptr->IO_ENDPT;
			fl_dev = mi_ptr->DEVICE;
			_mhdebug("floppy_driver: DEV_OPEN", fl_dev);
			fprintf(fp0,"floppy_driver: DEV_OPEN: fl_dev=%d fl_ep=%d\n",
				fl_dev,fl_ep);
			rcode = OK;
			break;			
		case DEV_READ:	
			fl_ep = mi_ptr->IO_ENDPT;
			fl_dev = mi_ptr->DEVICE;
			fl_bytes= mi_ptr->COUNT;
			fl_offset = mi_ptr->POSITION;
			fl_address = mi_ptr->ADDRESS;
			_mhdebug("floppy_driver: DEV_READ", fl_dev);
			_mhdebug("floppy_driver: fl_ep", fl_ep);
			
			/* check if the local process descriptor cache is updated  */
			rqtr_nr = _ENDPOINT_P(fl_ep);
			_mhdebug("floppy_driver: rqtr_nr", rqtr_nr);
			rqtr_ptr = &proc[rqtr_nr + NR_PROCS]; 
			if( rqtr_ptr->p_endpoint != fl_ep) {
				/* process endpoint does not match - get the new descriptor */
				rcode = sys_getproc(rqtr_ptr, fl_ep);
				if( rcode ) {
					_mhdebug("floppy_driver: sys_getproc rcode", rcode);
				}
			}

			fprintf(fp0,PROC_FORMAT, PROC_FIELDS(rqtr_ptr));

			rmt_virt = (fl_address + abs((rqtr_ptr->p_memmap[D].mem_phys << CLICK_SHIFT) - floppy_phys));
			_mhdebug("floppy_driver: fl_address", (int) fl_address);
			_mhdebug("floppy_driver: rmt_phys", (rqtr_ptr->p_memmap[D].mem_phys << CLICK_SHIFT));
			_mhdebug("floppy_driver: rmt_virt", (int) rmt_virt);
			fprintf(fp0,"fl_address=%X rmt_phys=%X rmt_virt=%X \n",
				fl_address,
				(rqtr_ptr->p_memmap[D].mem_phys << CLICK_SHIFT),
				rmt_virt);

#ifdef ANULADO			
	
			if (r2l[fl_ep].diff == NOT_INITIALIZED) 
				get_refdiff(fl_ep, fl_address, fl_bytes);
				
			rmt_virt = rvirt_lvirt(fl_address, fl_ep);
			fprintf(fp0,"floppy_driver: DEV_READ: rmt_virt=%X\n",rmt_virt);
			
			_mhdebug("floppy_driver: DEV_READ", fl_dev);
			fprintf(fp0,"floppy_driver: DEV_READ: fl_dev=%d fl_ep=%d fl_offset=%d fl_bytes=%d\n",
					fl_dev, fl_ep, fl_offset,  fl_bytes);
			
			rcode = fseek(fp1, fl_offset, SEEK_SET); 
			_mhdebug("floppy_driver: fseek", rcode);
			
			rcode = fread(devbuf, 1, fl_bytes, fp1);
			_mhdebug("floppy_driver: fread", rcode);

			rcode = sys_vcopyvm(SELF, VM0, devbuf, fl_ep, fl_vmid, fl_address, fl_bytes);
			_mhdebug("floppy_driver: sys_vcopyvm", rcode);		
			memcpy(rmt_virt, devbuf, fl_bytes); 

#else  /* ANULADO */

			rcode = fseek(fp1, fl_offset, SEEK_SET); 
			_mhdebug("floppy_driver: fseek", rcode);
			
			rcode = fread(rmt_virt, 1, fl_bytes, fp1); /* DIRECT READ to REQUESTER BUFFER !! */
			_mhdebug("floppy_driver: fread", rcode);		
#endif /* ANULADO */			

			rcode = fl_bytes;
			break;
		case DEV_CLOSE:  
			fl_ep = mi_ptr->IO_ENDPT;
			fl_dev = mi_ptr->DEVICE;
			_mhdebug("floppy_driver: DEV_CLOSE", fl_dev);
			fprintf(fp0,"floppy_driver: DEV_CLOSE: fl_dev=%d fl_ep=%d\n",
				fl_dev,fl_ep);
			rcode = OK;
			break;
		case DEV_GATHER:
				fl_ep = mi_ptr->IO_ENDPT;
				fl_dev = mi_ptr->DEVICE;
				fl_offset = mi_ptr->POSITION;
				fl_nr_req = mi_ptr->COUNT;	/* Length of I/O vector */
			    if (fl_nr_req > NR_IOREQS) fl_nr_req = NR_IOREQS;
				_mhdebug("floppy_driver: DEV_GATHER", fl_nr_req);
				fprintf(fp0,"floppy_driver: DEV_GATHER: fl_dev=%d fl_ep=%d fl_offset=%d fl_nr_req=%d\n",
					fl_dev, fl_ep, fl_offset, fl_nr_req);

				/* copy request vector from requester process address space to local address space */	
				rcode = sys_vcopyvm(mi_ptr->m_source, fl_vmid, (vir_bytes) mi_ptr->ADDRESS, 
							SELF, VM0, (vir_bytes) iovec, (phys_bytes) (fl_nr_req * sizeof(iovec[0])));
				_mhdebug("floppy_driver: sys_vcopyvm", rcode);
				
				rcode = fseek(fp1, fl_offset, SEEK_SET); 
				_mhdebug("floppy_driver: fseek", rcode);
					
				for( i = 0; i < fl_nr_req; i++) {
					iop = &iovec[i];			
					fl_address = (char *) iop->iov_addr;
					fl_bytes   = iop->iov_size;		
					fprintf(fp0,"floppy_driver: iovec[%d] fl_address=%X fl_bytes=%d\n",
								i, fl_address, fl_bytes);
					count  = fl_bytes; 
					offset = 0;
					do	{
						bytes = (count > SECTOR_SIZE)?SECTOR_SIZE:count;
						
						/* read data from the virtual device file into the local buffer  */
						bytes = fread(devbuf, 1, bytes, fp1);
						_mhdebug("floppy_driver: fread", bytes);

						/* copy the data from the local buffer to the requester process address space in other VM */
						rcode = sys_vcopyvm(SELF, VM0, devbuf, fl_ep, fl_vmid, fl_address+offset, bytes);
						_mhdebug("floppy_driver: sys_vcopyvm", rcode);
						offset += bytes;
						count -= bytes;
					} while(count > 0);
				}

				/* copy back the request vector from local space to requester process address space in other VM */	
				rcode = sys_vcopyvm(SELF, VM0, (vir_bytes) iovec, 
										mi_ptr->m_source, fl_vmid, (vir_bytes) mi_ptr->ADDRESS, 
										(phys_bytes) (fl_nr_req * sizeof(iovec[0])));
				_mhdebug("floppy_driver: sys_vcopyvm", rcode);
				
			break;
		case DEV_SCATTER:
				fl_ep = mi_ptr->IO_ENDPT;
				fl_dev = mi_ptr->DEVICE;
				fl_offset = mi_ptr->POSITION;
				fl_nr_req = mi_ptr->COUNT;	/* Length of I/O vector */
			    if (fl_nr_req > NR_IOREQS) fl_nr_req = NR_IOREQS;
				_mhdebug("floppy_driver: DEV_SCATTER", fl_nr_req);
				fprintf(fp0,"floppy_driver: DEV_SCATTER: fl_dev=%d fl_ep=%d fl_offset=%d fl_nr_req=%d\n",
					fl_dev, fl_ep, fl_offset, fl_nr_req);
					
				/* copy request vector from requester process address space to local address space */	
				rcode = sys_vcopyvm(mi_ptr->m_source, fl_vmid, (vir_bytes) mi_ptr->ADDRESS, 
							SELF, VM0, (vir_bytes) iovec, (phys_bytes) (fl_nr_req * sizeof(iovec[0])));
				_mhdebug("floppy_driver: sys_vcopyvm", rcode);
				
				rcode = fseek(fp1, fl_offset, SEEK_SET); 
				_mhdebug("floppy_driver: fseek", rcode);
					
				for( i = 0; i < fl_nr_req; i++) {
					iop = &iovec[i];			
					fl_address = (char*)iop->iov_addr;
					fl_bytes   = iop->iov_size;		
					fprintf(fp0,"floppy_driver: iovec[%d] fl_address=%X fl_bytes=%d\n",
								i, fl_address, fl_bytes);
					count  = fl_bytes; 
					offset = 0;
					do	{
						bytes = (count > SECTOR_SIZE)?SECTOR_SIZE:count;
						
						/* copy the data from the requester process address space in other VM  to the local buffer */
						rcode = sys_vcopyvm( fl_ep, fl_vmid, fl_address+offset,SELF, VM0, devbuf, bytes);
						_mhdebug("floppy_driver: sys_vcopyvm", rcode);
						
						/* write data from local buffer to the  virtual device file */
						bytes = fwrite(devbuf, 1, bytes, fp1);
						_mhdebug("floppy_driver: fwrite", bytes);

						offset += bytes;
						count -= bytes;
					} while(count > 0);
				}
				
				/* copy back the request vector from local space to requester process address space */	
				rcode = sys_vcopyvm(SELF, VM0, (vir_bytes) iovec, 
										mi_ptr->m_source, fl_vmid, (vir_bytes) mi_ptr->ADDRESS, 
										(phys_bytes) (fl_nr_req * sizeof(iovec[0])));
				_mhdebug("floppy_driver: sys_vcopyvm", rcode);
				
			break;
		case CANCEL:     
		case DEV_WRITE:  
		case DEV_IOCTL: 
		case DEV_SELECT:	
		case DEV_STATUS: 
			_mhdebug("floppy_driver: EINVAL fl_rqst", fl_rqst);
			rcode = EINVAL;
		default:
			_mhdebug("floppy_driver: ERROR fl_rqst", fl_rqst);
			rcode = EINVAL;
			break;
	}

	mo_ptr->m_type = TASK_REPLY;
	mo_ptr->REP_ENDPT = fl_ep;
	mo_ptr->REP_STATUS = rcode;
	rcode = sendas(mi_ptr->m_source, mo_ptr, driver_nr);
	_mhdebug("floppy_driver: sendas rcode", rcode);
	if( rcode) {
		return(rcode);
	}

	return(OK);
}


/*---------------------------------------------------------------------------------------*/
/*					tty_init					*/
/*---------------------------------------------------------------------------------------*/
int tty_init(void)
{
	_mhdebug("tty_init: ", 0);

	return(OK);
}

/*---------------------------------------------------------------------------------------*/
/*					tty_driver					*/
/*---------------------------------------------------------------------------------------*/
int tty_driver(int driver_nr)
{
	int i, val, rcode;
	message m_in, m_out;
	message *mi_ptr, *mo_ptr;
static  prtbuf[256];
	
	/*
	 * Driver number NO_DRIVER means the command to execute the driver initialization 
	 */

	if( driver_nr == NO_DRIVER) return(tty_init());

	_mhdebug("tty_driver: driver_nr", driver_nr);
	
	mi_ptr = &m_in;
	mo_ptr = &m_out;
	m_ptr = &m;

	rcode = recvas(ANY, mi_ptr, driver_nr);
	_mhdebug("do_proxy: recvas rcode", rcode);
	if( rcode) {
		return(rcode);
	}

	fprintf(fp0,"tty_driver:" MSG1_FORMAT, MSG1_FIELDS(mi_ptr));

	switch(mi_ptr->m_type) {
		case DIAGNOSTICS:
			fprintf(fp0,"tty_driver: DIAGNOSTICS: ep=%d bytes=%d\n",
				mi_ptr->DIAG_ENDPT, mi_ptr->DIAG_BUF_COUNT);
			if(mi_ptr->DIAG_ENDPT == SELF)
				mi_ptr->DIAG_ENDPT = mi_ptr->m_source;
			rcode = sys_vcopyvm(mi_ptr->DIAG_ENDPT, px_vmid, (vir_bytes) mi_ptr->DIAG_PRINT_BUF, 
				px_ep, VM0, (vir_bytes) prtbuf, mi_ptr->DIAG_BUF_COUNT);
			if(rcode)
				_mhdebug("do_proxy: sys_vcopyvm rcode", rcode);
			else 
				fprintf(fp0,"tty_driver: DIAGNOSTICS: %s\n", prtbuf);	
			break;
		default:
			mo_ptr->m_type = OK;
			break;
	}

	rcode = sendas(mi_ptr->m_source, mo_ptr, driver_nr);
	_mhdebug("do_proxy: sendas rcode", rcode);
	if( rcode) {
		return(rcode);
	}

	return(OK);
}

/*---------------------------------------------------------------------------------------*/
/*					no_driver					*/
/*---------------------------------------------------------------------------------------*/
int no_driver(int driver_nr)
{
	int i, val;
	
	/*
	 * Driver number (-1) means the command to execute the driver initialization
	 */
	if( driver_nr == (-1)) return(OK);

	_mhdebug("no_driver: driver_nr", driver_nr);

	return(ENOSYS);
}

/*---------------------------------------------------------------------------------------*/
/*					get_driver_nr					*/
/*---------------------------------------------------------------------------------------*/
int get_driver_nr(void)
{
	int i, val, rcode;
	struct priv *priv_ptr;
    message m;
	
	_mhdebug("get_driver_nr: INPUT virtual_bm",virtual_bm);
	
	for(i = 1, val = 1; i <= sizeof(long) * BITS_BYTE; i++){
		if( val & virtual_bm) {
			priv_ptr = &priv_tab[(i-1)];
			if( priv_ptr->s_id != (i-1))
				fprintf(fp0,"get_driver_nr: s_id=%d bit=%d\n", priv_ptr->s_id,(i-1));
			fprintf(fp0,PRIV_FORMAT, PRIV_FIELDS(priv_ptr));
			rcode = priv_ptr->s_proc_nr;
			_mhdebug("get_driver_nr: driver_nr",priv_ptr->s_proc_nr);
			virtual_bm &= ~(val);
			_mhdebug("get_driver_nr: OUTPUT virtual_bm",virtual_bm);
			return(rcode);
		}else{
			val = (1 << i);
		}
	}
	return(-1);
}	

/*---------------------------------------------------------------------------------------*/
/*					proxy_init					 */	
/*---------------------------------------------------------------------------------------*/
int proxy_init(int vmid)
{
	int rcode, i; 
	int new_vm;
	
	_mhdebug("proxy_init: vmid", vmid);
	m_ptr = &m;

	virtual_bm = 0; /* initialize virtual drivers notification bitmap */
	
	rcode = sys_getprivtab(priv_tab);
	if( rcode) 
		_mhdebug("proxy_init: sys_getprivtab", rcode);

	fp0 = fopen("proxy.txt","w");
	if( fp0 == NULL )
		_mhdebug("fprintf: fopen errno", errno );
		
	fprintf(fp0,"proxy_init: vmid=%d\n",vmid);
	fflush(fp0);

	/*
	 * Initializa all the drivers calling the drivers functions with parameter NO_DRIVER
	 */
	for( i = 0; i < (BITS_BYTE*sizeof(long)); i++){
		_mhdebug("proxy_init: initializing driver", i);
		rcode = (*driver_vec[i])(NO_DRIVER);
		if( rcode)
			_mhdebug("proxy_init: driver", i);
	}
		
	px_ep = getprocnr();
	_mhdebug("proxy_init: px_ep", px_ep);

	return(OK);
}
/*---------------------------------------------------------------------------------------*/
/*					do_proxy					*/
/*---------------------------------------------------------------------------------------*/
void do_proxy(int vmid)
{
	int rcode, driver_nr;
	
	_mhdebug("do_proxy: starting proxy vmid", vmid);
	rcode = proxy_init(vmid);
	fl_vmid = vmid;
	
	while (1) {
		_mhdebug("do_proxy: receive from", ANY);
		rcode = receive(ANY, &m);
		if(rcode) {
			_mhdebug("do_proxy: receive rcode", rcode);
		} else {
			_mhdebug("do_proxy: receive m_source", m_ptr->m_source);
			/*
 			 * A notify from HARDWARE means that a request is pending in a front end
			 * driver in VMx (VIRTUAL_TASK)
			 */
			if(m_ptr->m_source == HARDWARE) {
				/*
				 * Gets the driver number in VMx
				 */
				_mhdebug("do_proxy: receive NOTIFY_ARG",(m_ptr)->NOTIFY_ARG);
				virtual_bm = (m_ptr)->NOTIFY_ARG;
				while(virtual_bm) {
					driver_nr = get_driver_nr();
					_mhdebug("do_proxy: driver_nr", driver_nr);
					if(driver_nr < 0) {
						_mhdebug("do_proxy: error virtual_bm", virtual_bm);
						continue;
					}
					rcode = (*driver_vec[driver_nr])(driver_nr);
					fflush(fp0);
					if( rcode)
						_mhdebug("do_proxy: driver_vec rcode", rcode);
				};
			}else {
			/*
 			 * A message from a source != HARDWARE means that it is a reply from VM0
			 * driver or server
			 */
				_mhdebug("do_proxy: message from VM", 0);
			}
		}
	}
}

/******************************************************************************/
/*						main							*/ 
/******************************************************************************/
int main(argc, argv)
int argc;
char *argv[];
{
 int rcode, i , vmid;
 struct image_header hdr;
 char *heap_ptr;
 char *stk_ptr;
 pid_t vm_pid;

 n_procs = 0;

  /*--------------- check arguments -----------------------------*/
	if (argc != 3) {
	fprintf(stderr,"usage: loadvmimg <VM_name> <VM_image_pathname>\n");
	exit(1);
	}

	if( strlen(argv[1]) > PROC_NAME_LEN-1) {
		fprintf(stderr, "VM name %s is too long\n",argv[1]);
		exit(1);
	}

	if( strlen(argv[2]) > IM_NAME_MAX-1) {
		fprintf(stderr, "VM image %s is too long\n",argv[2]);
		exit(1);
	}

	strncpy(img.name, argv[2],IM_NAME_MAX);
	MHDEBUG("VM image pathname= %s\n", img.name);

 /* get the  maximum size for dynamic memory => (stack + heap) */
  stk_ptr = (char *) &stk_ptr;
  heap_ptr = sbrk(0); 
  dyn_size = (long)stk_ptr - (long)heap_ptr;
  dyn_size /= 2; 				/* not admite VM size with small memory sizes */
    
 /*--------------- alloc memory from heap for the image  ------*/
  heap_ptr = malloc(dyn_size/2);

  if(heap_ptr == NULL) {
	fprintf(stderr,"malloc error = %d\n",errno);  	
	return(-1);
	}
  MHDEBUG("malloc(%d) = %X\n",dyn_size, heap_ptr); 

 /*---------------------- allign image memory pointer -----------------------*/ 
  for (img.addr = heap_ptr;  
	((long)img.addr != (((long)img.addr>>CLICK_SHIFT)<<CLICK_SHIFT));
	img.addr++) 
	;			/* now img.addr is aligned */
  MHDEBUG("img.addr is click aligned = %X\n",img.addr); 
  MHDEBUG("SECTOR_SIZE= %d \n",SECTOR_SIZE); 
  	
 /*--------------- read the image file, build process headers, and path  ------*/
 if((rcode = read_image()) == -1) {
	printf("read_image errno=%d \n", errno);
	exit(1);
	}
 
 /*-----searchs for the image[] table (kernel/table.c) into de image in memory ---*/
 if((rcode = get_imagetab()) == -1) {
	fprintf(stderr,"get_imagetab errno=%d \n", errno);
	exit(1);
	}

 /* ---------- inform the VMM ---------------------*/
MHDEBUG("image: \n\t addr=%X bytes=%d sectors=%d name=%s\n",
		img.addr,img.bytes,img.sectors,img.name);
MHDEBUG("\t stat.st_size=%d\n",img.stat.st_size);  	
MHDEBUG("\t n_procs=%d\n",n_procs);

MHDEBUG("\t\t [#] a_text a_data a_bss_ a_total a_syms-   -name- \n");

  for( i = 0; i < n_procs; i++) {
	MHDEBUG("\t\t %0.3d %0.6X %0.6X %0.6X %0.7X %0.7X %s\n",
		i,
		img.hdr[i].process.a_text, 
		img.hdr[i].process.a_data, 
		img.hdr[i].process.a_bss,
		img.hdr[i].process.a_total,
		img.hdr[i].process.a_syms,
		img.hdr[i].name);
		};

MHDEBUG("\n\t\t [#] entry- --cs-- --ds-- -data- --end- \n");
  for( i = 0; i < n_procs; i++) {
	MHDEBUG("\t\t %0.3d %0.6X %0.6X %0.6X %0.6X %0.6X\n",
		i,
		process[i].entry,
		process[i].cs,
		process[i].ds,
		process[i].data,
		process[i].end);
	}

MHDEBUG("\t\t [#] p_nr qtum prio endpoint -name- \n");
  for( i = 0; i < (n_procs+NR_TASKS-1); i++) {
	MHDEBUG("\t\t %0.3d %0.4d %0.4d %0.4d %0.7d %s\n",
		i,
		img.image[i].proc_nr,
		img.image[i].quantum,
		img.image[i].priority,
		img.image[i].endpoint,
		img.image[i].proc_name);
  }

 vm_pid = getpid();
 MHDEBUG("VM image loaded. Inform SYSTASK through VMM. My PID is %d\n", vm_pid); 
 
   /* El VMM informa de todo a la SYSTASK la que marca al proceso actual como NO DESTRUIBLE y NO EJECUTABLE
      hasta tanto el VMM se lo indique para no liberar la memoria 
      El VMM le pasa los datos de la imagen a la SYSTASK para que ponga en marcha la nueva VM.
      La información que le interesa a VMM y SYSTASK es:
	  - struct img 
	  - stuct proc 
	  - PID de este proceso (que corresponde a la VM)
 
      El VMM necesita la info sobre la direccion de la VM y el tamaño. Esta info la tiene el PM.
      Entonces  loadvmimg---> PID ---> VMM 
	           VMM-->pide datos a --> PM y ademas le dice que este proceso es indestructible
	           VMM tiene todo lo que necesita, le pasa toda esta info a SYSTASK para que construya la VM.

 */
   img.stk_addr = (stk_ptr-CLICK_SIZE);
   
   img.params_addr = boot_params;
   img.params_len  = SECTOR_SIZE;
   
   img.loadPID	= vm_pid;
   img.vmsize	= 0; /* unknown until here for this process, must be filled by VMM */
   img.nr_bprocs= (n_procs+NR_TASKS-1);
   
  /* send command to VMM 	*/
  /* VMM_LOAD 	= m1_i1 	*/
  /* vm_pid		= m1_i2	*/
  /* n_procs	= m1_i3	*/
  /* &process 	= m1_p1	*/
  /* &img	 	= m1_p2	*/
  /* stk_ptr	= m1_p3	*/
  rcode = vmmcmd(VMM_LOAD, vm_pid, n_procs, &process, &img, (stk_ptr-CLICK_SIZE) );
  MHDEBUG("LOADVMIMG: vm_pid=%d n_procs=%d stk_ptr=%X \n",vm_pid,n_procs,(stk_ptr-CLICK_SIZE));
  if( rcode < 0) {
	fprintf(stderr,"vmmcmd: errno = %d\n",errno);
	exit(2);
  }
 close(STDOUT_FILENO);

	/* copy the VM name and  set the status WAITING to START the VM before starting the proxy   */
	px_vmid = vmmcmd(VMM_WAITVM, vm_pid, strlen(argv[1]), argv[1], NULL, 0);
	if(px_vmid < 0) {
		printf("VMM_WAITVM rcode=%d\n", px_vmid);
		exit(1);
	}
	
	do_proxy(px_vmid);

	
 exit(0);	
}


#ifdef ANULADO
void do_proxy(int px_vmid)
{			
	while(TRUE) {
			rcode = receive(ANY, m_ptr);
			_mhdebug("do_proxy: receive m_source", m_ptr->m_source);
			if(m_ptr->m_source == HARDWARE) {
				_mhdebug("do_proxy: receive NOTIFY_ARG",(m_ptr)->NOTIFY_ARG);
				driver_nr = get_driver_nr((m_ptr)->NOTIFY_ARG);
				if(driver_nr == (-1)) {
					_mhdebug("do_proxy: error NOTIFY_ARG", (m_ptr)->NOTIFY_ARG);
				}else {
					_mhdebug("do_proxy: driver_nr", driver_nr);
				}
				
				rcode = recvas(ANY, m_ptr, driver_nr);
				if( rcode) {
					_mhdebug("do_proxy: recvas rcode", rcode);
				}else {
					_mhdebug("do_proxy: recvas m_source", m_ptr->m_source);				
					_mhdebug("do_proxy: recvas m1_i1", m_ptr->m1_i1);
					_mhdebug("do_proxy: recvas m1_i2", m_ptr->m1_i2);
					_mhdebug("do_proxy: recvas m1_i3", m_ptr->m1_i3);
				}
				m_ptr->m1_i1 = 0x07;
				m_ptr->m1_i2 = 0x08;
				m_ptr->m1_i3 = 0x09;
				rcode = sendas(m_ptr->m_source, m_ptr, driver_nr);
				/*
				 * Here select the apropiated proxy backend
				 */
				fp  = fopen("borrar.txt","w");
 				if (fp == NULL) {
 					_mhdebug("fopen: errno\n", errno);
				}else{
					rcode = fwrite("0123456789", 1,10,fp);
					if( rcode < 0)
	  					_mhdebug("fwrite: errno\n", errno);
					else
	  					_mhdebug("fwrite: rcode\n", rcode);
					fclose(fp);
				}

				/*
				 * Gets the message that a process in VMx sent to the front end driver
				 * (VIRTUAL TASK)
				 */
				rcode = recvas(ANY, m_ptr, driver_nr);
	}	
}

#endif /* ANULADO */






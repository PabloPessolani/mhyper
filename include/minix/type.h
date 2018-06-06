#ifndef _TYPE_H
#define _TYPE_H

#ifndef _MINIX_SYS_CONFIG_H
#include <minix/sys_config.h>
#endif

#ifndef _TYPES_H
#include <sys/types.h>
#endif

/* Type definitions. */
typedef unsigned int vir_clicks; 	/*  virtual addr/length in clicks */
typedef unsigned long phys_bytes;	/* physical addr/length in bytes */
typedef unsigned int phys_clicks;	/* physical addr/length in clicks */

#if (_MINIX_CHIP == _CHIP_INTEL)
typedef unsigned int vir_bytes;	/* virtual addresses and lengths in bytes */
#endif

#if (_MINIX_CHIP == _CHIP_M68000)
typedef unsigned long vir_bytes;/* virtual addresses and lengths in bytes */
#endif

#if (_MINIX_CHIP == _CHIP_SPARC)
typedef unsigned long vir_bytes;/* virtual addresses and lengths in bytes */
#endif

/* Memory map for local text, stack, data segments. */
struct mem_map {
  vir_clicks mem_vir;		/* virtual address */
  phys_clicks mem_phys;		/* physical address */
  vir_clicks mem_len;		/* length */
};
#ifdef MHYPER
typedef struct mem_map mem_map_t;
#define MMAP_FORMAT "mem_vir=%X mem_phys=%X mem_len=%d\n"
#define MMAP_FIELDS(mp) 	mp->mem_vir, mp->mem_phys, mp->mem_len
#endif /*  MHYPER */

/* Metric struct to measure scheduler behavior */
#ifdef MHYPER
struct vm_metric{
  clock_t timestamp;
  int kernel[NR_VMS];
  int task[NR_VMS];
  int proc[NR_VMS];
  int idle;
};
#endif /* MHYPER */

/* Memory map for remote memory areas, e.g., for the RAM disk. */
struct far_mem {
  int in_use;			/* entry in use, unless zero */
  phys_clicks mem_phys;		/* physical address */
  vir_clicks mem_len;		/* length */
};

/* Structure for virtual copying by means of a vector with requests. */
struct vir_addr {
  int proc_nr_e;
  int segment;
  vir_bytes offset;
};

#ifdef MHYPER
/* Structure for virtual copying between VMs */
struct vir_addr_vm {
  int proc_nr_e;
  int segment;
  vir_bytes offset;
  int vmid;
};
#endif /*  MHYPER */


/* Memory allocation by PM. */
struct hole {
  struct hole *h_next;          /* pointer to next entry on the list */
  phys_clicks h_base;           /* where does the hole begin? */
  phys_clicks h_len;            /* how big is the hole? */
};

/* Memory info from PM. */
struct pm_mem_info {
	struct hole pmi_holes[_NR_HOLES];/* memory (un)allocations */
	u32_t pmi_hi_watermark;		 /* highest ever-used click + 1 */
};

#define phys_cp_req vir_cp_req 
struct vir_cp_req {
  struct vir_addr src;
  struct vir_addr dst;
  phys_bytes count;
};

typedef struct {
  vir_bytes iov_addr;		/* address of an I/O buffer */
  vir_bytes iov_size;		/* sizeof an I/O buffer */
} iovec_t;

/* PM passes the address of a structure of this type to KERNEL when
 * sys_sendsig() is invoked as part of the signal catching mechanism.
 * The structure contain all the information that KERNEL needs to build
 * the signal stack.
 */
struct sigmsg {
  int sm_signo;			/* signal number being caught */
  unsigned long sm_mask;	/* mask to restore when handler returns */
  vir_bytes sm_sighandler;	/* address of handler */
  vir_bytes sm_sigreturn;	/* address of _sigreturn in C library */
  vir_bytes sm_stkptr;		/* user stack pointer */
};

/* This is used to obtain system information through SYS_GETINFO. */
struct kinfo {
  phys_bytes code_base;		/* base of kernel code */
  phys_bytes code_size;		
  phys_bytes data_base;		/* base of kernel data */
  phys_bytes data_size;
  vir_bytes proc_addr;		/* virtual address of process table */
  
  phys_bytes kmem_base;		/* kernel memory layout (/dev/kmem) */
  phys_bytes kmem_size;
  phys_bytes bootdev_base;	/* boot device from boot image (/dev/boot) */
  phys_bytes bootdev_size;
  
  phys_bytes ramdev_base;	/* boot device from boot image (/dev/boot) */
  phys_bytes ramdev_size;
  phys_bytes params_base;	/* parameters passed by boot monitor */
  phys_bytes params_size;
  
  int nr_procs;			/* number of user processes */
  int nr_tasks;			/* number of kernel tasks */
  char release[6];		/* kernel release number */
  char version[6];		/* kernel version number */
#if DEBUG_LOCK_CHECK
  int relocking;		/* interrupt locking depth (should be 0) */
#endif
  int nr_bprocs;			/* number of boot process */
  int vmid;
  int nr_vms;
  int vm_bitmap;
  int metric_addr;
};

#ifdef MHYPER
typedef struct kinfo kinfo_t;
#define KINFO_FORMAT1 "code_base=%X code_size=%d data_base=%X data_size=%d proc_addr=%p\n"
#define KINFO_FIELDS1(kp) 	kp->code_base, kp->code_size, kp->data_base, kp->data_size, kp->proc_addr
#define KINFO_FORMAT2 "kmem_base=%X kmem_size=%d bootdev_base=%X bootdev_size=%d\n"
#define KINFO_FIELDS2(kp) 	kp->kmem_base, kp->kmem_size, kp->bootdev_base, kp->bootdev_size
#define KINFO_FORMAT3 "ramdev_base=%X ramdev_size=%d params_base=%X params_size=%d\n"
#define KINFO_FIELDS3(kp) 	kp->ramdev_base, kp->ramdev_size, kp->params_base, kp->params_size
#define KINFO_FORMAT4 "nr_procs=%d nr_tasks=%d nr_vms=%d release=%s nr_bprocs=%d version=%s vmid=%d \n"
#define KINFO_FIELDS4(kp) 	kp->nr_procs, kp->nr_tasks, kp->nr_vms, kp->release, kp->version, kp->vmid, kp->nr_bprocs
#endif /*  MHYPER */


/* Load data accounted every this no. of seconds. */
#define _LOAD_UNIT_SECS		 6 

/* Load data history is kept for this long. */
#define _LOAD_HISTORY_MINUTES	15
#define _LOAD_HISTORY_SECONDS	(60*_LOAD_HISTORY_MINUTES)

/* We need this many slots to store the load history. */
#define _LOAD_HISTORY	(_LOAD_HISTORY_SECONDS/_LOAD_UNIT_SECS)

/* Runnable processes and other load-average information. */
struct loadinfo {
  u16_t proc_load_history[_LOAD_HISTORY];	/* history of proc_s_cur */
  u16_t proc_last_slot;
  clock_t last_clock;
};
#ifdef MHYPER
typedef struct loadinfo loadinfo_t;
#endif /*  MHYPER */

struct machine {
  int pc_at;
  int ps_mca;
  int processor;
  int prot;
  int vdu_ega;
  int vdu_vga;
};
#ifdef MHYPER
typedef struct machine machine_t;
#define MACH_FORMAT "pc_at=%d ps_mca=%d processor=%d prot=%d vdu_ega=%d vdu_vga=%d\n"
#define MACH_FIELDS(mp) 	mp->pc_at, mp->ps_mca, mp->processor, mp->prot, mp->vdu_ega, mp->vdu_vga
#endif /*  MHYPER */

struct io_range
{
	unsigned ior_base;	/* Lowest I/O port in range */
	unsigned ior_limit;	/* Highest I/O port in range */
};

struct mem_range
{
	phys_bytes mr_base;	/* Lowest memory address in range */
	phys_bytes mr_limit;	/* Highest memory address in range */
};

#ifdef MHYPER
struct vm_mmap_s {
	struct mem_map		vmmap_start;		/* The virtual address 0 of the loader  */
	struct mem_map		vmmap_image;		/* The address of the 1rst boot process */
	struct mem_map		vmmap_bottom;		/* The first free address after laoded all boot process */
	struct mem_map		vmmap_top;			/* The last free address of the VM = minimun stack of loader */
};
typedef struct vm_mmap_s vm_mmap_t;



#endif /*  MHYPER */

#endif /* _TYPE_H */

#ifndef _VMIMAGE_H
#define _VMIMAGE_H

#ifndef _STAT_H
#include <sys/stat.h>
#endif

#ifndef _IMAGE_H
#include <minix/image.h>
#endif

#define PROCESS_MAX	16	/* Must match the space in kernel/mpx.x */

#ifdef LOADVMIMG /* NEEDED ONLY BY LOADVMIMG				*/
/**********************************************************************/
/* THIS FIELDS MUST BE THE SAME AS IN KERNEL boot_image		*/
/**********************************************************************/
struct bimage {
  proc_nr_t proc_nr;			/* process number to use */
  task_t *initial_pc;			/* start function for tasks */
  int flags;				/* process flags */
  unsigned char quantum;		/* quantum (tick count) */
  int priority;				/* scheduling priority */
  int stksize;				/* stack size for tasks */
  short trap_mask;			/* allowed system call traps */
  bitchunk_t ipc_to;			/* send mask protection */
  long call_mask;			/* system call protection */
  char proc_name[P_NAME_LEN];		/* name in process table */
  int endpoint;				/* endpoint number when started */
};
#endif 

typedef  struct {
		char 		*addr;		/* virtual address of the image 		*/
		char 		*stk_addr;	/* top address that PM of VM can use 	*/
		char 		*params_addr;	/* virtual address of boot parameters  	*/
		int			params_len;	/* lenght of boot parameters			*/
		int			loadep;		/* laader endpoint			*/
		pid_t		loadPID;	/* loader PID				*/
		u32_t		vmsize;		/* VM size in clicks 	*/
		int			nr_bprocs;/* number of boot processes		*/
		u32_t 		bytes;		/* image size in bytes			*/
		off_t 		sectors;	/* image size in disk sectors		*/
		struct 		stat stat;
		char 		name[IM_NAME_MAX + 1]; /* image name	*/
		struct  	image_header hdr[PROCESS_MAX]; 

/* The system image table lists all programs that are part of the boot image. 
 * The order of the entries here MUST agree with the order of the programs
 * in the boot image and all kernel tasks must come first.
*/
#ifdef LOADVMIMG
		struct bimage image[PROCESS_MAX+NR_TASKS];
#else
		struct boot_image image[PROCESS_MAX+NR_TASKS];
#endif
		} imginfo_t ;

#define IMGI_FORMAT "addr=%X stk_addr=%X loadep=%d loadPID=%d vmsize=%d nr_bprocs=%d bytes=%d name=%s\n"
#define IMGI_FIELDS(ip) 	ip->addr, ip->stk_addr, ip->loadep, ip->loadPID, ip->vmsize, ip->nr_bprocs, ip->bytes, ip->name
		
typedef struct {	/* Per-process memory adresses. */
	u32_t	entry;	/* Entry point. */
	u32_t	cs;		/* Code segment. */
	u32_t	ds;		/* Data segment. */
	u32_t	data;		/* To access the data segment. */
	u32_t	end;		/* End of this process, size = (end - cs). */
} process_t;

struct  vm_drivers_s {
  char drv_name[P_NAME_LEN];		
  unsigned int drv_type; 
  union {
	unsigned long drv_bitmap;
	}drv_u; 
};
typedef struct vm_drivers_s vm_drivers_t;
#define DRV_FORMAT "drv_name=%s drv_type=%X drv_bitmap=%X \n "
#define DRV_FIELDS(dp) 	dp->drv_name, dp->drv_type, dp->drv_u.drv_bitmap

#endif  /* _VMIMAGE_H */

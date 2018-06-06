/* This table has one slot per VM.  It contains all the VM management
 * information for each VM.  The kernel have tables that are also 
 * indexed by VM, with the contents of corresponding slots 
 * referring to the same VM.
 */
#ifndef _VMACHINE_H
#define _VMACHINE_H

#ifndef _VMIMAGE_H
#include <minix/vmimage.h>
#endif

#define VM_NAME_LEN 	PROC_NAME_LEN

struct vmm_desc_s {	/* VM Descriptor structure		*/
 
  vmid_t	vmm_id;		/* VM id  DEFINIR vmid_t en include/sys/types.h 	*/
  unsigned 	vmm_flags;		/* VM status flag bits */
  signed int vmm_nice;		/* nice is PRIO_MIN..PRIO_MAX, standard 0. */
  int		vmm_endpoint; 	/* endpoint of the VM loader		*/
  int		vmm_pid; 		/* PID  of the VM loader		*/
  int		vmm_size;		/* size in clicks of VM */
  char 		*vmm_stkptr;	/* highest address (click aligned) that VMx's PM can manage */

  /*Token Bucket*/
  int		vmm_bsize; /* Max Tokens assigned to the VM */
  int		vmm_tokens; /* Tokens assigned to the VM, using for resting */
	
  /* VM Processes user and system times. Accounting done on process exit. */
  clock_t 	vmm_proc_utime;	/* cumulative user time of all processes of the VM */
  clock_t 	vmm_proc_stime;	/* cumulative sys time of all processes of the VM  */

  imginfo_t vmm_img;				/* VM boot image info */
  process_t vmm_bootprocs[PROCESS_MAX];	/* per boot process memory address */


/* major-minor numbers of main devices (on VM0 filesystem) for this VM	
*   dev_t	main_dev[NR_MAIN_DEV];  major-minor numbre of devices	
* The Main devices are:		 DEFINIR en minix/com.h		
* VM_CONSOLE	  0	
* VM_ROOT	 	  1
* VM_TTY	 	  2	
* VM_INET	 	  3  
* NR_MAIN_DEV	  4 DEFINIR en minix/config.h
*/
  char vmm_name[VM_NAME_LEN];	/* VM name */
} ;
typedef struct vmm_desc_s vmm_desc_t;

#define VMM_FORMAT "id=%d flags=%X endp=%d pid=%d size=%d tokens=%d/%d name=%s\n"
#define VMM_FIELDS(vmmp) vmmp->vmm_id,vmmp->vmm_flags, vmmp->vmm_endpoint, vmmp->vmm_pid, vmmp->vmm_size, vmmp->vmm_tokens, vmmp->vmm_bsize, vmmp->vmm_name


/* VM Status Flags */
#define VM_FREE         0x0000	/* the VM descriptor is FREE		*/
#define VM_LOADED 	    0x0001	/* the VM is loaded in memory		*/
#define VM_RUNNING      0x0002	/* the VM is running			*/
#define VM_STOPPED      0x0004	/* the VM has been stopped 		*/
#define VM_SUSPENDED    0x0008	/* the VM is suspended 			*/
#define VM_WAITING      0x0010	/* the Loader of the VM is wainting for START */

/* All processes of this VM are set as P_STOP and unqueued of READY queues */
#define NIL_VMMDESC ((struct vmm_desc_t *) 0)

 

#endif /* _VMACHINE_H */



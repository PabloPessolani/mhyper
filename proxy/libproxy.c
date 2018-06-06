
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

#include "../kernel/config.h"
#include "../servers/vmm/const.h"

#define running_vm 		px_vmid

#include "../kernel/const.h"
#include "../kernel/config.h"
#include "../kernel/type.h"
#include "../kernel/proc.h"
#include "../kernel/ipc.h"

#include "../servers/vmm/mhmacros.h"


#include "libproxy.h"
#include "glo.h"


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
	rcode = sys_umapvm(px_vmid, rproc, D,  (vir_bytes) raddr, 
				(vir_bytes) rbytes, &rmt_phys);
	if(rcode) return(rcode);

	fprintf(fp_log,"floppy_driver: DEV_READ: raddr=%X rmt_phys=%X\n",
				raddr, rmt_phys);
					
	/* Get LOCAL physical address of local reference variable  */
	rcode = sys_umapvm(VM0, SELF, D,  (vir_bytes) &lcl_ref, 
				(vir_bytes) sizeof(char), &lcl_phys);
	if(rcode) return(rcode);
	
	fprintf(fp_log,"floppy_driver: DEV_READ: &lcl_ref=%X lcl_phys=%X\n",
				&lcl_ref, lcl_phys);
				
	/* Calculate the difference among both physical addresses */
	r2l[rproc].diff = 	(rmt_phys - lcl_phys);
	/* save the remote reference */
	r2l[rproc].ref = (char *) raddr;
	fprintf(fp_log,"floppy_driver: DEV_READ: proc=%d diff=%X ref=%X \n", 
	rproc, r2l[rproc].diff, r2l[rproc].ref);
	return(OK);
}
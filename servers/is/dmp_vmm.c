/* This file contains procedures to dump VMM  data structures.
 *
 * The entry points into this file are
 *   vmm_dmp:   	display VMM  table	  
 *
 * Created:
 *   Nov 5, 2013:	by Pablo Pessolani
 #include <minix/config.h>
#include <unistd.h>
#include "is.h"
 */

#include "is.h"

#include <signal.h>
#include <stdlib.h>
#include <fcntl.h>
#include <a.out.h>

#include <minix/keymap.h>
#include <minix/com.h>
#include <minix/endpoint.h>
#include <minix/ipc.h>
#include <minix/syslib.h>
#include <minix/com.h>

#ifdef MHYPER
#include <minix/com.h>
#include <minix/vmachine.h>
#endif /*  MHYPER */

#include <sys/resource.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <string.h>
#include <limits.h>
#include <timers.h>
#include <string.h>

#include "../../kernel/const.h"
#include "../../kernel/config.h"
#include "../../kernel/type.h"
#include "../../kernel/proc.h" 

#ifdef MHYPER

#include "../vmm/const.h"
#include "../vmm/mhmacros.h"
#include <minix/vmimage.h>
#include <minix/vmachine.h>
#include "../vmm/param.h"

FORWARD _PROTOTYPE( int  do_getvmid, (void)			); 

PUBLIC vmm_desc_t vmm_desc[NR_VMS]; 

PUBLIC void vmm_dmp()
{
	vmm_desc_t *vmmp;
	int i, vmid;
	message m;
	int rcode;

	if( (vmid = do_getvmid()) != VM0)	{
		printf("This is VM%d\n", vmid);
		return;
	}

	vmmp = vmm_desc;
	printf("Virtual Machine Manager table dump\n");

	rcode = do_tabdump();
	if(rcode) return;
	
/*	printf(VMM_FORMAT, VMM_FIELDS(vmmp)); */
	
	printf("vm_id vm_flags vm_endpoint vm_pid vm_size tokens/bucket vm_name\n");
	for (i=0; i<NR_VMS; i++) {
		vmmp = &vmm_desc[i];
		if (vmmp->vmm_flags == VM_FREE) continue;
		printf("%5d %8X %7d %6d %7d %6d/%6d %-10.10s\n", VMM_FIELDS(vmmp));
	}

}


/*===========================================================================*
 *				switch_vm				     *
 *===========================================================================*/
PUBLIC void switch_vm(void)
{
    struct kinfo kinfo;
	vmm_desc_t  *vmm_ptr;
	int ret, s;
	
    do_tabdump();
	do {
		active_vm = (active_vm+1)% NR_VMS;
		vmm_ptr = &vmm_desc[active_vm];	
	} while( (vmm_ptr->vmm_flags & VM_LOADED) != VM_LOADED);
	ret = sys_setISvm(active_vm);
	printf("IS: active VM is VM%d\n", active_vm);

	if ((s = sys_getkinfo(&kinfo)) != OK) {
    	_mhdebug("IS warning: getkinfo", s);
    	return;
	}

	_mhdebug("IS kinfo.nr_bprocs", kinfo.nr_bprocs);
	nrbprocs = kinfo.nr_bprocs;
}

/*===========================================================================*
 *				do_tabdump				     *
 *===========================================================================*/
PUBLIC int do_tabdump(void)
{  
  message m;
  int rcode;

	MHDEBUG("do_tabdump\n");
	m.VMM_cmd= VMM_TABDUMP;
	m.VMM_p1= (char*) vmm_desc;
	rcode = _taskcall(VMM_PROC_NR, VMMCMD, &m);
    	if (rcode != 0) 
        	MHERROR("do_tabdump taskcall rcode=%d\n",rcode);

  return(rcode);
}

/*===========================================================================*
 *				do_getvmid				     *
 *===========================================================================*/
PRIVATE int do_getvmid(void)
{  
	message m;
	int rcode;

	MHDEBUG("do_getvmid\n");
	rcode = _taskcall(SYSTEM, SYS_GETVMID, &m);
  
	if (rcode < 0) {
        MHERROR("do_getvmid taskcall rcode=%d\n",rcode);
	}else{
        MHDEBUG("do_getvmid taskcall vmid=%d\n",rcode);
	}  
 
	return(rcode);
}

#endif /* MHYPER */



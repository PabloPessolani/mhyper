/* This file contains the main program of the VM Manager and some related
 * procedures.  
 */

#include "vmm.h"
#ifdef  MHYPER 

#include <minix/keymap.h>
#include <minix/callnr.h>
#include <minix/com.h>
#include <minix/endpoint.h>
#include <minix/ipc.h>
#include <minix/syslib.h>
#include <minix/com.h>
#include <minix/minlib.h>
#include <minix/vmconfig.h>
#include <string.h>
#include <sys/resource.h>
#include <limits.h>
#include <signal.h>

#include "const.h"
#include "param.h"
#include "glo.h"
#include "../ds/store.h"

#include "mhmacros.h"

#define MB_SHIFT	20	/*bits to right shift to convert into Mb */ 

FORWARD _PROTOTYPE( int get_work, (void)				);
FORWARD _PROTOTYPE( int vmm_init, (void)				);
FORWARD _PROTOTYPE( int send_reply, (int result)		);

FORWARD _PROTOTYPE( int  do_getvmid, (void)			); 
FORWARD _PROTOTYPE( int alloc_vmdesc,	(void)				);
FORWARD _PROTOTYPE( int free_vmdesc,	(int vmid)			);
#define  DS_BOOT_NR	 (DS_PROC_NR+NR_TASKS-3) 	/* ONLY FOR TESTING (DS_PROC_NR=6 + NR_TASKS=4)=> 10-3 = 7 !!*/

int send_result;

/*===========================================================================*
 *				main					     *
 * Main routine of the Virtual Machine Manager Server 	*
 *===========================================================================*/
PUBLIC int main()
{
/* Main routine of the process manager. */
    int result;

  result = vmm_init();			/* initialize Virtual Machine Manager tables */
  if(result != OK) panic("VMM","vmm_init failed!", result);

  /* This is VMM's main loop-  get work and do it, forever and forever. */
  while (TRUE) {

	get_work();		/* wait for an VMM system call */

	/* Check for valid, perform the call. */
	if ( call_nr >= VMM_NCALLS || call_nr < 0 ) {
		result = ENOSYS;
	} else {
		result = (*call_vec[call_nr])();
	}
	if( send_result ) 
		send_reply(result);
   }

  return(OK);
}


/*===========================================================================*
 *				get_work				     *
 *===========================================================================*/
PRIVATE int get_work()
{
	message *m_ptr;
/* Wait for the next message and extract useful information from it. */
  send_result = 1;
  
  if (receive(ANY, &m_in) != OK)
	MHERROR("VMM receive error", NO_NUM);

  who_e = m_in.m_source;	/* who sent the message */
  call_nr = VMM_NCALLS;

  if(vmm_isokendpt(who_e, &who_p) != OK)
	MHERROR("VMM got message from invalid endpoint: \n", EBADSRCDST);

 MHDEBUG("VMM mtype=%d\n",m_in.m_type); 
 if (NOTIFY_FROM(PM_PROC_NR) ==  m_in.m_type ) {
	send_result = 0;
	return(OK);
	}
 
 if ( m_in.m_type != VMMCMD){	
	m_ptr = &m_in;
	MHDEBUG("VMM " MSG1_FORMAT, MSG1_FIELDS(m_ptr));
	MHERROR("VMM got invalid call\n", EBADCALL);
 }
 call_nr = (m_in.VMM_cmd - VMM_RQ_BASE);	/* VMM call number */
 
 MHDEBUG("VMM: get_work: who=%d call_nr=%d i2=%d i3=%d p1=%X p2=%X p3=%X\n"
		,who_e	
		,call_nr
		,m_in.VMM_i2
		,m_in.VMM_i3
		,m_in.VMM_p1
		,m_in.VMM_p2
		,m_in.VMM_p3
		);

  return(OK);
}

/*===========================================================================*
 *				vmm_init				     	*
 * Initialize the VM manager. 						*
 *===========================================================================*/
PRIVATE int vmm_init()
{

  register int i, j, s;
  vmm_desc_t *vmm_ptr;
  vmid_t vmid;

  	vmid = _mhdebug("VMM Running!", vmid);
	vmid = _mhdebug("VMM: vmid=", vmid);
 
	if( vmid != VM0) { 
		vmm_test(vmid);
		while(1) receive(ANY, &m_in); /* To protect an never return to main loop */
   	}
	

 /* VMM can only run on VM0	*/	
	while( (vmid = do_getvmid()) != VM0)	{
		printf("VM Manager Server not in VM0\n");	
		if (receive(ANY, &m_in) != OK) {
			printf("VMM: request from %d\n", m_in.m_source);
		}else{
			m_out.m_type = EBADVMID;
			if ((s=send(m_in.m_source, &m_out)) != OK) 
				MHERROR("VMM can't reply to %d\n",m_in.m_source);
		}		
	}

  /* Initialize VM descriptor table */

  printf("\n VM Manager Server: Initializing VMs: ");
  for (i = 0;  i < NR_VMS; i++)	{
	free_vmdesc(i);
	printf("%2d ",i);
  }
  MHDEBUG("\n CLICK_SIZE = %d\n",CLICK_SIZE);

  /* patch VM0  fields */
  vmm_ptr=&vmm_desc[0];
  vmm_ptr->vmm_flags = (VM_LOADED | VM_RUNNING);
  vmm_ptr->vmm_nice	= PRIO_MAX;
  vmm_ptr->vmm_tokens 	= MAXTOKENS;
  strcpy( vmm_ptr->vmm_name, "VM0");	/* VM name */


  return(OK);
}


/*===========================================================================*
 *				send_reply				     *
 *===========================================================================*/
PRIVATE int send_reply(result)
int result;			/* result of call (usually OK or error #) */
{
	int s;
	m_out.VMM_rcode = result;
	if ((s=send(who_e, &m_out)) != OK) 
		MHERROR("VMM can't reply to %d (%s)\n",who_e);
	return(OK);
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

/*===========================================================================*
 *				alloc_vmdesc						     *
 * Searches for FREE VM descriptor							     *
 *===========================================================================*/
PRIVATE int alloc_vmdesc(void)
{
	int i;
  	struct vmm_desc_s *vmm_ptr;

	for (i = 1;  i < NR_VMS; i++) {
		vmm_ptr	=&vmm_desc[i];
		if( vmm_ptr->vmm_flags == VM_FREE) 
			return(i);
	}
	return(i);
}


/*===========================================================================*
 *				free_vmdesc					     *
 *===========================================================================*/
PRIVATE int free_vmdesc(int vmid)
{
  	struct vmm_desc_s *vmm_ptr;

	if( vmid < 1 || vmid >= NR_VMS)
		return(-1);

	vmm_ptr			=&vmm_desc[vmid];
	vmm_ptr->vmm_id 		= vmid;
	vmm_ptr->vmm_flags		= VM_FREE;
	vmm_ptr->vmm_nice		= PRIO_PROCESS;
	vmm_ptr->vmm_endpoint	= 0;
	vmm_ptr->vmm_pid		= 0;
	vmm_ptr->vmm_size		= 0;
 	vmm_ptr->vmm_stkptr		= NULL;
	vmm_ptr->vmm_proc_utime	= 0;
	vmm_ptr->vmm_proc_stime	= 0;

	vmm_desc[VM0].vmm_tokens += vmm_ptr->vmm_tokens;
	vmm_ptr->vmm_tokens	 = 0;

	strcpy( vmm_ptr->vmm_name, "VM FREE");	/* VM name */
}


/*===========================================================================*
 *				do_vm_info				     *
 *===========================================================================*/
PUBLIC int do_vm_info(void)
{
	int i;
  	struct vmm_desc_s *vmm_ptr;

	MHDEBUG("VMM: do_vm_info\n");
	printf("VM# flags nice endpoint -pid- -vmsize- stk-ptr- vm_utime vm_stime vmm_name\n");
	for (i = 0;  i < NR_VMS; i++) {
		vmm_ptr	=&vmm_desc[i];
		if( vmm_ptr->vmm_flags != VM_FREE)
			printf("%3d %5X %4d %8d %5d %8d %8X %8d %8d %s\n",
 				vmm_ptr->vmm_id,
				vmm_ptr->vmm_flags,
				vmm_ptr->vmm_nice,
				vmm_ptr->vmm_endpoint,
				vmm_ptr->vmm_pid,
				vmm_ptr->vmm_size,
			 	vmm_ptr->vmm_stkptr,
				vmm_ptr->vmm_proc_utime,
				vmm_ptr->vmm_proc_stime,
				vmm_ptr->vmm_name);
	}
	
	printf("\n\nVM# vmm_name bucket tokens\n");
	for (i = 0;  i < NR_VMS; i++) {
		vmm_ptr	=&vmm_desc[i];
		if( vmm_ptr->vmm_flags != VM_FREE) {
			printf("%3d %8s %6d %6d\n",
 				vmm_ptr->vmm_id,
				vmm_ptr->vmm_name,
				vmm_ptr->vmm_bsize,
				vmm_ptr->vmm_tokens);
		}
	}
 

	printf("\n Maximun number of Tokens: %d\n", MAXTOKENS);

	return(OK);
}
	
/*===========================================================================*
 *				do_vm_load				     *
 *===========================================================================*/
PUBLIC int do_vm_load(void)
{
  message m;
  int rcode, i;
  vmm_desc_t *vmm_ptr;
  int vmsize, vmid, vmloadep;
  pid_t vmpid;
  char *vmstkptr, *ptr;
  phys_bytes imgaddr, procaddr;


  MHDEBUG("VMM: do_vm_load: \n");
	/*-------------- Alloc a VM Descriptor ---------*/
  if( (vmid = alloc_vmdesc()) == NR_VMS) 
	MHERROR("VMM: No free vm struct for new VM rcode=%d\n",ENOMEM);
	
  MHDEBUG("VMM: alloc_vmdesc: vmid = %d: \n", vmid);

  /* Input Parameters from loadvmimg 	*/
  /* VMM_LOAD 	= m1_i1 		*/
  /* vmm_pid	= m1_i2			*/
  /* n_procs	= m1_i3			*/
  /* &process 	= m1_p1			*/
  /* &img	= m1_p2			*/
  /* stk_ptr	= m1_p3			*/

  vmpid 	= m_in.m1_i2;			/* save the PID of the loader 	*/
  nbprocs 	= m_in.m1_i3;  			/* save number of boot processes 	*/
  procaddr 	= (phys_bytes) m_in.m1_p1;	/* process table addr	*/
  imgaddr 	= (phys_bytes) m_in.m1_p2;	/* image table addr	*/
  vmstkptr 	= m_in.m1_p3;			/* stack pointer		*/
  MHDEBUG("VMM LOAD: vmpid=%d nbprocs=%d procaddr=%X imgaddr=%X vmstkptr=%X \n",
		vmpid,
		nbprocs,
		procaddr,
		imgaddr,
		vmstkptr);

  /* Request PM info about VM loader and VM size 	*/
  /* PM set the flag of the loader process as VM_PROC*/ 
  /* This flags avoid all signals that kill it		*/
  /* Output Parameters to PM		 				*/
  /* VMM_LOAD 	= m1_i1 							*/
  /* vmpid		= m1_i2						*/
  /* vmid		= m1_i3							*/
    m.m1_i1 = VMM_LOAD; 
    m.m1_i2 = vmpid;
    m.m1_i3 = vmid;
  rcode = _syscall(PM_PROC_NR, VMMCMD, &m);
  if(rcode != 0) 
	MHERROR("VMM: VMMCMD to PM failed. rcode =%d\n", rcode);

  /* Results from PM			*/
  /* VMM_LOAD   = m1_i1 		*/
  /* vmloadep	= m1_i2			*/
  /* vmsize	= m1_i3			*/
  MHDEBUG("VMM: PM return VM size = %d clicks(%d Mb) loadep=%d vmm_desc=%X \n", 
	m.m1_i3,
	((m.m1_i3<< CLICK_SHIFT) >> MB_SHIFT),
	m.m1_i2,
	&vmm_desc
	);

  /*----- checks the endpoint of the loader among VMM and PM -*/
  	if (m.m1_i2 != who_e) 
		MHERROR("PM has a different endpoint than VMM\n",EBADSRCDST);	

  	vmloadep = m.m1_i2;
  	vmsize   = m.m1_i3;

	/* Request SYSTASK to copy image data from VM loader into local VM descriptor */
	rcode = sys_vircopy(vmloadep, D, imgaddr,			/* Source 	*/
			SELF, D, (phys_bytes)&vmm_desc[vmid].vmm_img,	/* Destination 	*/
 			(phys_bytes) sizeof(imginfo_t));		/* Count 	*/
	if( rcode != OK)
		MHERROR("VMM: LOAD: sys_vircopy image error rcode=%d\n",rcode);
		/* ATENCION, SI ALGO FALLA HAY QUE NOTIFICAR AL PM PARA QUE VUELVA TODO ATRAS */

	/*------------- PRINT only for testing ---------------------*/
	vmm_ptr =&vmm_desc[vmid];
#if MHDBG
	for (i = 0; i < nbprocs; i++) 
		printf("image boot process %d: %s %s\n",
		i,vmm_ptr->vmm_img.hdr[i].name,
		vmm_ptr->vmm_img.image[i+NR_TASKS-1].proc_name);
#endif

	/* Request SYSTASK to copy boot processes' data from VM loader into local VM descriptor*/
	rcode = sys_vircopy(vmloadep, D, procaddr,			/* Source 		*/
			SELF, D, (phys_bytes)&vmm_desc[vmid].vmm_bootprocs[0],/* Destination*/
 			(phys_bytes) (sizeof(process_t)*PROCESS_MAX));	/* Count 		*/
	if( rcode != OK)
		MHERROR("VMM: LOAD: sys_vircopy boot processes error rcode=%d\n",rcode);
		/* ATENCION, SI ALGO FALLA HAY QUE NOTIFICAR AL PM PARA QUE VUELVA TODO ATRAS */

      /*---------- Fills de VM descriptor ----------*/
 	vmm_ptr->vmm_id	= vmid;
 	vmm_ptr->vmm_flags	= VM_LOADED;
	vmm_ptr->vmm_endpoint	= vmloadep;
	vmm_ptr->vmm_pid		= vmpid;
	vmm_ptr->vmm_size		= vmsize;
 	vmm_ptr->vmm_stkptr	= vmstkptr; /* highest memory address for the VM */

#ifdef ANULADO	
    ptr	= vmm_ptr->vmm_name;
	ptr	+=2;
	*ptr++ = vmid+'0';	/* VM name */
	*ptr   = '\0';		/* VM name */
#endif /* ANULADO */	

	/* Inform SYSTASK about the image loaded for the VM 		*/
	/* SYSTASK dump the complete VMM descriptor into its descriptor	*/	
  	/* outparams: i1=LOAD,i2=bootprocs, i3=vmid			*/
  	/*            p1=image_table,p2=bootproc_tab,p3=&vmdesc[vmid]	*/
	/* Output Parameters to SYSTASK		 	*/
	m.m1_i1		= VMM_LOAD;
	m.m1_i2		= vmid;
	m.m1_i3		= nbprocs;
  	m.m1_p1	 	= (char *) &vmm_desc[vmid];
  	rcode = _taskcall(SYSTEM, SYS_VMMREQ, &m);
  	if(rcode != 0) 
		MHERROR("VMM: VMM_LOAD to SYSTASK failed. rcode =%d\n", rcode);

#ifdef ANULADO	
	/******************* Publish VMM Endpoint **********************/
	if(ds_publish_vm(vmm_ptr)!=OK) {
		printf("VM Manager Server can not register its endpoint into DS\n");	
		/* exit(1); */
	}
#endif /* ANULADO */	
	
	
/* ATENCION, SI ALGO FALLA HAY QUE NOTIFICAR AL PM PARA QUE VUELVA TODO ATRAS */

  return(OK);
  }
/*===========================================================================*
 *				do_vm_start				     *
 *===========================================================================*/
PUBLIC int do_vm_start(void)
{
  /* Request SYSTASK to patch the image. VM_memory_size in vmm_i2 */

  message m;
  int vmid;
  int vm_tokens, free_tokens;
  pid_t pid;
  int rcode, i, new_p, new_endp;
  struct vmm_desc_s *vmm_ptr;
  pid_t new_pid;

  vmid = m_in.VMM_i2;
 vm_tokens = (int) m_in.VMM_p1;
MHDEBUG("do_vm_start: vmid=%d tokens=%d  \n",vmid, vm_tokens); 

  /* Checks the vmid */
  if( vmid < 1 || vmid >= NR_VMS){
	printf("VMM: START - bad VMID =%d\n", vmid);
	return(rcode);
  }

  
    free_tokens = vmm_desc[VM0].vmm_tokens;
   
   /* The VM can not has more tokens than are available */
   if(vm_tokens <= 0 || vm_tokens > free_tokens){ 
	printf("VMM: START - bad tokens=%d\n", vm_tokens);
	printf("VMM: START - only have %d tokens\n", free_tokens);
	return(rcode);
  }
  
  /* Gets the VM descriptor */
  vmm_ptr	=&vmm_desc[vmid];
 
  /* check the correct status of the VM */
  switch(vmm_ptr->vmm_flags) {
	case VM_RUNNING:
		MHERROR("VMM: Can not kill a running VM - STOP it first rcode=%d\n", -1);
		break;
	case VM_FREE:
		MHERROR("VMM: The VM is unused\n", -1);
		break;
    case VM_STOPPED:
		MHERROR("VMM: The VM is STOPPED\n", -1);
		break;
	case VM_SUSPENDED:
		MHERROR("VMM: The VM is SUSPENDED\n", -1);
		break;
	case (VM_LOADED | VM_WAITING):
		MHDEBUG("VMM in LOADED state - starting it!\n");
		break;
	default:
		MHERROR("VMM: The VM is in invalid state !\n", -1);
		break;	
	}	
  
	/*------------- PRINT only for testing ---------------------*/
#if MHDBG
	for (i = 0; i < nbprocs; i++) 
		printf("START image boot process %d: %s %s\n",
		i,vmm_ptr->vmm_img.hdr[i].name,
		vmm_ptr->vmm_img.image[i+NR_TASKS-1].proc_name);
#endif

	/*----------------------------------------------------------------------*/
	/*	Tell SYSTASK to boot the processes in the boot image 		*/
	/*----------------------------------------------------------------------*/
	/* Output Parameters to SYSTASK	 	*/
	m.m1_i1		= VMM_START;
	m.m1_i2		= vmid;
	m.m1_i3		= vm_tokens;
	MHDEBUG("VMM_START: Tell SYSTASK to boot the processes in the vmid=%d boot image. tokens=%d\n",
		vmid, vm_tokens);

  	rcode = _taskcall(SYSTEM, SYS_VMMREQ, &m);
	
  	if(rcode != 0) { 
		m.m_type = rcode; 
		rcode = send(vmm_ptr->vmm_endpoint, &m);   /* Wakeup the loader , it is waiting for this reply */
		MHERROR("VMM: VMM_START to SYSTASK failed. rcode =%d\n", rcode);
	} else {
		m.m_type = vmid; 
		rcode = send(vmm_ptr->vmm_endpoint, &m);   /* Wakeup the loader, it is waiting for this reply */
	}

	MHDEBUG("VMM_START: rcode=%d\n",rcode);
 	vmm_ptr->vmm_flags = (VM_RUNNING | VM_LOADED);
	vmm_ptr->vmm_bsize = vm_tokens;
	vmm_ptr->vmm_tokens = vm_tokens;
	vmm_desc[VM0].vmm_tokens -= vm_tokens;

return(OK);
}

/*===========================================================================*
 *				do_vm_stop				     *
 *===========================================================================*/
PUBLIC int do_vm_stop(void)
{
MHDEBUG("do_vm_stop\n");
return(OK);

}
/*===========================================================================*
 *				do_vm_suspend				     *
 *===========================================================================*/
PUBLIC int do_vm_suspend(void)
{
MHDEBUG("do_vm_suspend\n");
return(OK);

}
/*===========================================================================*
 *				do_vm_resume				     *
 *===========================================================================*/
PUBLIC int do_vm_resume(void)
{
MHDEBUG("do_vm_resume\n");
return(OK);

}

/*===========================================================================*
 *				do_vm_kill						     *
 *===========================================================================*/
PUBLIC int do_vm_kill(void)
{
  message m;
  int vmid;
  int rcode, i;
  struct vmm_desc_s *vmm_ptr;

MHDEBUG("do_vm_kill\n");
  vmid = m_in.VMM_vmid;
  if( vmid < 1 || vmid >= NR_VMS){
	printf("VMM: KILL - bad VMID =%d\n", vmid);
	return(rcode);
  }

  for (i = 1;  i < NR_VMS; i++) {
	vmm_ptr	=&vmm_desc[i];
	if ( vmm_ptr->vmm_id == vmid) break;
  }

  if( vmid == NR_VMS)
	MHERROR("VMM: KILL - VM does not exist rcode=%d\n", -1);
 
  switch(vmm_ptr->vmm_flags) {
	case VM_RUNNING:
		MHERROR("VMM: Can not kill a running VM - STOP it first rcode=%d\n", -1);
		break;
	case VM_FREE:
		MHERROR("VMM: The VM is unused\n", -1);
		break;
	case VM_LOADED:
      case VM_STOPPED:
	case VM_SUSPENDED:
		break;   
	}	
  	
  m.m1_i1 = VMM_KILL;
  m.m1_i2 = vmm_ptr->vmm_endpoint;
  rcode = _syscall(PM_PROC_NR, VMMCMD, &m);
  if(rcode != 0) 
	MHERROR("VMM: VMMCMD to PM failed. rcode =%d\n", rcode);

  /* Output Parameters to SYSTASK		 	*/
  m.m1_i1		= VMM_KILL;
  m.m1_i2		= vmid;
  rcode = _taskcall(SYSTEM, SYS_VMMREQ, &m);
  if(rcode != 0) 
	MHERROR("VMM: VMMCMD to SYSTASK failed. rcode =%d\n", rcode);

  kill(vmm_desc[vmid].vmm_pid, SIGKILL);
  
  free_vmdesc(vmid);
  
return(OK);

}
/*===========================================================================*
 *				do_vm_save				     *
 *===========================================================================*/
PUBLIC int do_vm_save(void)
{
MHDEBUG("do_vm_save\n");
return(OK);
}

/*===========================================================================*
 *				do_vm_tabdmp				 			    *
 *===========================================================================*/
PUBLIC int do_vm_tabdmp(void)
{
	vir_bytes dst_addr, src_addr;
	int rcode, len;
	
	dst_addr = (vir_bytes) m_in.m1_p1;	/* IS buffer for the vmm_struct table */
	src_addr = (vir_bytes) vmm_desc;
	len = sizeof(struct vmm_desc_s) * NR_VMS;
printf("do_vm_tabdmp copying len=%d to %d\n", len, who_e);
	rcode =sys_datacopy(SELF, src_addr, who_e, dst_addr, len);
	if (rcode != OK)
		MHERROR("sys_datacopy error:",rcode);
	return(OK);
}

/*===========================================================================*
 *				do_vm_vmdmp				 			    *
 *===========================================================================*/
PUBLIC int do_vm_vmdmp(void)
{
	vir_bytes dst_addr, src_addr;
	int rcode, len, vmid;
	
MHDEBUG("do_vm_vmdmp\n");
	vmid = (vir_bytes) m_in.m1_i2;	
	if(vmid < 0 || vmid > NR_VMS) return(EINVAL);
	
	dst_addr = (vir_bytes) m_in.m1_p1;	/* IS buffer for the vmm_struct table */
	src_addr = (vir_bytes) &vmm_desc[vmid];
	len = sizeof(struct vmm_desc_s);
	rcode =sys_datacopy(SELF, src_addr, who_e, dst_addr, len);
	if (rcode != OK)
		MHERROR("sys_datacopy error:",rcode);
	return(OK);
}

/*===========================================================================*
 *				do_vm_waitvm				 			    *
 *===========================================================================*/
PUBLIC int do_vm_waitvm(void)
{
	int rcode, vmid, vm_pid, i, vm_namelen;
	  struct vmm_desc_s *vmm_ptr;
static vm_name[PROC_NAME_LEN];

	
	vm_pid	= (vir_bytes) m_in.m1_i2;
    vm_namelen = m_in.m1_i3;
	
	if( vm_namelen > PROC_NAME_LEN-1)
		return(ENAMETOOLONG);
	rcode =sys_datacopy(who_e, m_in.m1_p1, SELF, vm_name,  vm_namelen+1);
	if (rcode != OK)
		MHERROR("sys_datacopy error:",rcode);

	MHDEBUG("do_vm_waitvm vm_pid=%d vm_namelen=%d vm_name=%s\n", 
		vm_pid, vm_namelen, vm_name);
	
	for( i=0; i < NR_VMS; i++){
		vmm_ptr = &vmm_desc[i];
		if( vmm_ptr->vmm_flags == VM_FREE) continue;
		if( vmm_ptr->vmm_pid == vm_pid && vmm_ptr->vmm_flags == VM_LOADED ) {
			MHDEBUG("do_vm_waitvm FIND VM!\n");
			send_result = 0; 					/*Dont reply !!*/
			vmm_ptr->vmm_flags |= VM_WAITING;	/* Remember that is waiting */
			strncpy(vmm_ptr->vmm_name, vm_name, vm_namelen);
			return(OK);
		}
	}	
	
	return(EPROXYSVR);
}

/*===========================================================================*
 *				do_vm_restore				     *
 *===========================================================================*/
PUBLIC int do_vm_restore(void)
{
MHDEBUG("do_vm_restore\n");
return(OK);

}

/*===========================================================================*
 *				ds_publish_vm								     *
 *===========================================================================*/
PRIVATE int  ds_publish_vm(  vmm_desc_t *vmm_ptr)
{
	static vm_cfg_t	vmc;
	int rcode;
	unsigned long key;

	vm_cfg_t	 *vmc_ptr;
    
	vmc_ptr =&vmc;

	vmc_ptr->v_vmid = vmm_ptr->vmm_id;
	vmc_ptr->v_size = vmm_ptr->vmm_size;
	strcpy(vmc_ptr->v_name,vmm_ptr->vmm_name);
	strcpy(vmc_ptr->v_bootprog,"bootprog_path");
	strncpy(vmc_ptr->v_bootimage,vmm_ptr->vmm_img.name,IM_NAME_MAX);
	vmc_ptr->v_proc_bm = 0x55555555;
	key =  ( (KEY_VM_CFG << KEY_SHIFT_TYPE) |  (vmm_ptr->vmm_id));

	if (rcode = ds_publish( key , DS_VMM_CFG, 
			vmm_ptr->vmm_id, vmm_ptr->vmm_endpoint, (char *) vmc_ptr)) {
      	printf("Couldn't Publish at DS.: rcode=%d errno=%d\n",rcode, errno);
		return(rcode);
	}
	
return (OK);
}

/*===========================================================================*
 *				do_vm_proc						     *
 *===========================================================================*/
PUBLIC int do_vm_proc(void)
{
  message m, *m_ptr;
  int vmid;
  int rcode, i;
  struct vmm_desc_s *vmm_ptr;

	MHDEBUG("do_vm_proc\n");

	m_ptr = &m_in;
	printf(MSG1_FORMAT, MSG1_FIELDS(m_ptr));

	if( m_in.VMM_vmid < 1 || m_in.VMM_vmid >= NR_VMS){
		printf("do_vm_proc - bad VMID =%d\n", m_in.VMM_vmid);
		return(rcode);
	}

	for (i = 1;  i < NR_VMS; i++) {
		vmm_ptr	=&vmm_desc[i];
		if ( vmm_ptr->vmm_id == m_in.VMM_vmid) break;
	}

	if( m_in.VMM_vmid == NR_VMS)
		MHERROR("VMM: PROC - VM does not exist rcode=%d\n", EBADVMID);
 
	if( vmm_ptr->vmm_flags != (VM_LOADED | VM_WAITING) )
		MHERROR("VMM: Bad vm status \n", EVMSTATUS);
  	
	m = m_in;
	m_ptr = &m;
	m_ptr->m1_i3 = m_in.m_source; /* endpoint of vmmcmd */
	
	MHDEBUG(MSG1_FORMAT, MSG1_FIELDS(m_ptr));
	rcode = _taskcall(SYSTEM, SYS_VMMREQ, &m);
	if(rcode != 0) 
		MHERROR("VMM: VMMCMD to SYSTASK failed. rcode =%d\n", rcode);

	return(OK);
}

#endif /*  MHYPER */


	

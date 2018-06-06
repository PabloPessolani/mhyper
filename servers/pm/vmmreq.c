/* This file deals with VMM Requests */

#include "pm.h" 

#define BIOS		0	/* Can only be used under the BIOS. */
#define nil 		0
#define _POSIX_SOURCE	1
#define _MINIX		1

#ifdef MHYPER

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/resource.h>

#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <lib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <alloca.h>
#include <a.out.h>

#include <minix/ipc.h>
#include <minix/syslib.h>
#include <minix/com.h>
#include <minix/const.h>
#include <minix/callnr.h>
#define LOADVMIMG 	1 	
#include <minix/vmachine.h>

#include "mproc.h"
#include "../vmm/mhmacros.h"
#include "../../kernel/proc.h"

#define LAST_FEW            2	/* last few slots reserved for superuser */

struct bimage bproc, *iptr;

/*===========================================================================*
 *				do_vmmreq							     *
 *===========================================================================*/
PUBLIC int do_vmmreq()
{
  phys_clicks vm_base;
  phys_bytes  vm_clicks;
  int p_nr, req, rcode;
  pid_t vmpid;
  int vmid,i;
  char *ptr;
  int vmloadep, vm_proc, bootp_nr, parent_p;
  register struct mproc *rmp;	/* pointer to parent */
  register struct mproc *rmc;	/* pointer to child */
  pid_t new_pid;
  static int next_child, new_p;
  static char mess_sigs[] = { SIGTERM, SIGHUP, SIGABRT, SIGQUIT };
  register char *sig_ptr;
  message m;


  int n = 0;

  rmp = mp;
  req =  m_in.m1_i1;
  
  MHDEBUG("PM: do_vmmreq: who=%d req=%X i2=%d i3=%d p1=%X p2=%X p3=%X \n"
		,who_e	
		,req
		,m_in.m1_i2
		,m_in.m1_i3
		,m_in.m1_p1
		,m_in.m1_p2
		,m_in.m1_p3
		);
		
  if( who_e != VMM_PROC_NR) 
  	MHERROR("PM: a process != VMM trying to make a VMM request. rcode=%d\n", EPERM);

  if (pm_vmid != 0) 
  	MHERROR("PM: VMMREQ can only can be executed on VM0. rcode=%d\n", EPERM);

  switch(req) {
		case VMM_LOAD:
			/* Input Parameters from VMM		 	*/
  			/* VMM_LOAD 	= m1_i1 			*/
  			/* vmpid		= m1_i2			*/
  			/* vmid		= m1_i3			*/
			vmpid =  m_in.m1_i2;	/* VM Loader PID 	*/
			vmid  =  m_in.m1_i3;  	/* VM id to change the name of the loader */
			p_nr = proc_from_pid(vmpid);
			if( p_nr < 0) {
				printf("PM: Process number for PID %d not running!\n", vmpid);
				rcode = ESRCH;
			}
			mproc[p_nr].mp_flags |= VM_PROC;	/* This process can not be killed */

			/* change the name of the loader for PM */
			strcpy(mproc[p_nr].mp_name, "VMx");
		        ptr	= mproc[p_nr].mp_name;
			ptr+=2;
			*ptr++ = vmid+'0';	/* VM name */
			*ptr   = '\0';		/* VM name */

			/* Return to VMM the VM size in clicks and the loader endpoint */
 			/* vmloadep	= m1_i2			*/
  			/* vmsize	= m1_i3				*/
			rmp->mp_reply.m1_i2 = mproc[p_nr].mp_endpoint;
			rmp->mp_reply.m1_i3 = mproc[p_nr].mp_vmsize;
			rcode = OK;
			break;
		case VMM_KILL:
			vmloadep = m_in.m1_i2;
  			if(pm_isokendpt(vmloadep, &vm_proc) != OK) {
				printf("PM: Bad endpoint=%d !\n", vmloadep);
				rcode = ENOENT;
			} else {
				if( (mproc[vm_proc].mp_flags & VM_PROC)) {
					mproc[vm_proc].mp_flags &= ~VM_PROC;	/* This process can not be killed */			
					rcode = OK;
				} else {
					printf("PM: That endpoint is not a VM process =%d !\n", vmloadep);
					rcode = ENOENT;	
				}
			}
			break;
 		default:
			rcode = EBADREQUEST;
			break;
  }
  
  return(rcode);
}

#endif /*  MHYPER */


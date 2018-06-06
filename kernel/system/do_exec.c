/* The kernel call implemented in this file:
 *   m_type:	SYS_EXEC
 *
 * The parameters for this kernel call are:
 *    m1_i1:	PR_ENDPT  		(process that did exec call)
 *    m1_p1:	PR_STACK_PTR		(new stack pointer)
 *    m1_p2:	PR_NAME_PTR		(pointer to program name)
 *    m1_p3:	PR_IP_PTR		(new instruction pointer)
 */
#include "../system.h"
#include <string.h>
#include <signal.h>
#include <minix/endpoint.h>

#define IPC_DBG 1

#if USE_EXEC

/*===========================================================================*
 *				do_exec					     *
 *===========================================================================*/
PUBLIC int do_exec(m_ptr)
register message *m_ptr;	/* pointer to request message */
{
/* Handle sys_exec().  A process has done a successful EXEC. Patch it up. */
  register struct proc *rp, *rp0, *lp, *st;
  reg_t sp;			/* new sp */
  phys_bytes phys_name;
  char *np;
  int proc, p,vmid, lp_ep, lp_nr, rp_nr, rcode, i ,proc_type ;
  	vmm_desc_t *vmm_ptr;
	struct vm_struct  *vm_ptr;
	struct priv *priv_ptr;

	if(!isokendpt(m_ptr->PR_ENDPT, &proc))
		return EINVAL;

	rp = proc_addr(proc);
#ifdef MHYPER
	vmid = running_vm;
	vmm_ptr =&vmm_desc(vmid);
	vm_ptr = &vm_hyper[vmid];

	/* SYSTASK has received a EXEC request from VMx ? */
	rcode = OK;
	if( vmid > VM0) {
#ifdef IPC_DBG
		MHDEBUG("do_exec: in vmid=%d proc=%d\n",vmid, proc);
#endif /* IPC_DBG */		
		phys_name = numap_local(who_p, (vir_bytes) m_ptr->PR_NAME_PTR,
					(vir_bytes) P_NAME_LEN - 1);
		if (phys_name != 0) {
			/* get the process name from PM */
			phys_copy(phys_name, VIR2PHYS(rp->p_name), (phys_bytes) P_NAME_LEN - 1);
			for (np = rp->p_name; (*np & BYTE) >= ' '; np++) {}
					*np = 0;					/* mark end */
				
			proc_type = REAL_PROC_TYPE;
			for( i = 0; i < NR_SYS_PROCS; i++){
				if( MH_LOADED(vm_ptr->vm_driver[i].drv_type) == EXEC_LOADED) {
					if(!strcmp(rp->p_name, vm_ptr->vm_driver[i].drv_name)) {
						proc_type = MH_TYPE(vm_ptr->vm_driver[i].drv_type);
						MHDEBUG("do_exec: driver found %s type=%X\n", 
							vm_ptr->vm_driver[i].drv_name, vm_ptr->vm_driver[i].drv_type);
						break;
					}
				}
			}
			
			/* HERE WE SUPPOSE THAT THE PROCESS ON VMx USES THE SAME SLOT THAT ON VM0 */
			/* OTHERWISE   IT MUST SEARCH THE NAME FROM HARDWARE TO ( NR_SYS_PROCS - NR_TASKS )*/
			/* OF A PROMISCUOUS PROCESS ON VMx WITH THE SAME NAME */
			
			switch(proc_type) {
				case PROMISCUOUS_PROC_TYPE:
					/* Search the process on VM0 with the same name */
					for( p = PM_PROC_NR; p < NR_PROCS; p++) {
						rp0 = proc_addr_vm(VM0, p);
						if( rp0->p_rts_flags != SLOT_FREE) {
							if(!strcmp(rp->p_name, rp0->p_name)) {
								MHDEBUG("do_exec: process %s MATCH on VM0 p_nr0=%d p_nrX=%d\n"
									,rp0->p_name, rp0->p_nr, rp->p_nr );
								break; /* NAME  MATCH */	
							}
						}
					}
  
					if( p == NR_PROCS ) {
						MHDEBUG("do_exec: PROMICUOUS_TASK %s (VM%d) is dead on VM0 \n",
							rp->p_name, vmid);
						rcode = EPROCSTATUS;
						break;
					}

					MHDEBUG("do_exec: in %s(VM0) exec:%s p_nrX=%d\n", 
						rp0->p_name, rp->p_name, _ENDPOINT_P(m_ptr->PR_ENDPT));

					/* The process is a promiscous driver */
					/* rp_nr = rp->p_nr; */
					/* *rp = *rp0;	*/
					rp->p_endpoint= m_ptr->PR_ENDPT; 
					rp->p_misc_flags = PROMICUOUS_TASK;
					rp->p_rts_flags = NO_MAP;
					rp->p_promiscuous = rp0; 

					/* SETs the bit map of the VMs to serve by a  BOOT LOADED PROMISCUOUS  */
					priv(rp0)->s_vm_bitmap |= (1 << vmid);
					/* Store which process number that the promiscuous process will represent on VMx */
					priv(rp0)->s_nr[vmid] = rp->p_nr;

					/* NOTIFIES EXEC loaded PROMISCUOUS  processes to serve this vm */
					if( MH_NTFY(vm_ptr->vm_driver[i].drv_type) == EXEC_NTFY) {
						MHDEBUG("do_exec: notifying %s %d type=%X\n", 
							vm_ptr->vm_driver[i].drv_name, rp0->p_nr, vm_ptr->vm_driver[i].drv_type);
						lock(0,"exec_ntfy");
						running_vm = VM0;
						systask_ptr->p_vmid = VM0;
						raw_notify(VMM_PROC_NR,rp0->p_nr);
						running_vm = vmid;
						systask_ptr->p_vmid = vmid;
						unlock(0);
					}
					rp->p_priv  = NULL;
					return(OK);
					break;
				case VIRTUAL_PROC_TYPE:
					rp0 = proc_addr_vm(VM0,proc);
					MHDEBUG("do_exec: VIRTUAL %s vmm_endpoint=%d\n", rp->p_name,vmm_ptr->vmm_endpoint);
					lp_ep = vmm_ptr->vmm_endpoint; /* loader endpoint */
					if( isokendpt_vm(VM0, lp_ep, &lp_nr)) {
						lp = proc_addr_vm(VM0, lp_nr);
						if(strcmp(rp0->p_name, rp->p_name) == 0) {
							/* The process is a virtual driver, setup the kernel slot in VMx */						
							vmid = rp->p_vmid;
							*rp = *rp0;
							rp->p_misc_flags = VIRTUAL_TASK | MH_LEVEL(vm_ptr->vm_driver[i].drv_type);
							rp->p_rts_flags = 0;
							rp->p_vmid = vmid;
							rp->p_endpoint= m_ptr->PR_ENDPT;
							if (priv(rp)->s_id < NR_SYS_PROCS) {
								priv(lp)->s_dr_bitmap |= (1 << priv(rp)->s_id);
								MHDEBUG(PROC_FORMAT, PROC_FIELDS(rp0));				
								MHDEBUG(PROC_FORMAT, PROC_FIELDS(rp));	
								priv_ptr = lp->p_priv;
								MHDEBUG(PRIV_FORMAT, PRIV_FIELDS(priv_ptr));				
								return(OK);
							} else {
								MHERROR("SYSTEM: do_exec: s_id out of range:%d\n",ERANGE);
								rcode = ERANGE;
							}
						}else{
							MHDEBUG("do_exec: VIRTUAL_TASK %s!=%s \n",rp0->p_name, rp->p_name);
							rcode = EVIRTUALTASK;
						}
					}else{
						MHDEBUG("do_exec: BAD ENDPOINT for VIRTUAL_TASK %s \n",rp->p_name);
						rcode = EPROCSTATUS;
					}
					break;
				case REAL_PROC_TYPE:
					if( vm_ptr->vm_driver[i].drv_type & PARAVIRTUAL){
						rp->p_misc_flags = PARAVIR_TASK;
						priv(rp)->s_call2proxy = vm_ptr->vm_driver[i].drv_u.drv_bitmap;
						st = &vm_hyper[vmid].vm_proc[SYSTEM+NR_TASKS];
						lp_ep = vmm_ptr->vmm_endpoint; /* loader endpoint */
						if(!isokendpt_vm(VM0, lp_ep, &lp_nr)) {
							MHDEBUG("do_exec: BAD ENDPOINT for PARAVIRTUAL %s \n",rp->p_name);
							rcode = EPROCSTATUS;						
						}
						lp = proc_addr_vm(VM0, lp_nr);
						priv(lp)->s_dr_bitmap |= (1 << priv(st)->s_id);					
						MHDEBUG("do_exec: PARAVIRTUAL %s s_call2proxy=%X s_dr_bitmap=%X\n", 
							rp->p_name, priv(rp)->s_call2proxy, priv(lp)->s_dr_bitmap);
					}else{
						MHDEBUG("do_exec: REAL %s\n", rp->p_name);
					}
					break;
				case DISABLED_PROC_TYPE:
					MHDEBUG("do_exec: DISABLED %s\n", rp->p_name);
					rp->p_rts_flags = SENDING;
					break;
				default:
					break;
			}
		}else{
			MHDEBUG("do_exec: numap_local error\n");
			rcode = EFAULT;
		}
		if(rcode)
			MHERROR("do_exec: error %d\n", rcode);
	}
#endif /*  MHYPER */  
  sp = (reg_t) m_ptr->PR_STACK_PTR;
  rp->p_reg.sp = sp;		/* set the stack pointer */
#if (CHIP == M68000)
  rp->p_splow = sp;		/* set the stack pointer low water */
#ifdef FPP
  /* Initialize fpp for this process */
  fpp_new_state(rp);
#endif
#endif
#if (CHIP == INTEL)		/* wipe extra LDT entries */
  phys_memset(VIR2PHYS(&rp->p_ldt[EXTRA_LDT_INDEX]), 0,
	(LDT_SIZE - EXTRA_LDT_INDEX) * sizeof(rp->p_ldt[0]));
#endif
  rp->p_reg.pc = (reg_t) m_ptr->PR_IP_PTR;	/* set pc */
  rp->p_rts_flags &= ~RECEIVING;	/* PM does not reply to EXEC call */
  if (rp->p_rts_flags == 0) lock_enqueue(rp);
  /* Save command name for debugging, ps(1) output, etc. */
  phys_name = numap_local(who_p, (vir_bytes) m_ptr->PR_NAME_PTR,
					(vir_bytes) P_NAME_LEN - 1);
  if (phys_name != 0) {
	phys_copy(phys_name, VIR2PHYS(rp->p_name), (phys_bytes) P_NAME_LEN - 1);
	for (np = rp->p_name; (*np & BYTE) >= ' '; np++) {}
	*np = 0;					/* mark end */
  } else {
  	strncpy(rp->p_name, "<unset>", P_NAME_LEN);
  }
  return(OK);
}
#endif /* USE_EXEC */


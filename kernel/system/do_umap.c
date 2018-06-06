/* The kernel call implemented in this file:
 *   m_type:	SYS_UMAP
 *
 * The parameters for this kernel call are:
 *    m5_i1:	CP_SRC_PROC_NR	(process number)	
 *    m5_c1:	CP_SRC_SPACE	(segment where address is: T, D, or S)
 *    m5_l1:	CP_SRC_ADDR	(virtual address)	
 *    m5_l2:	CP_DST_ADDR	(returns physical address)	
 *    m5_l3:	CP_NR_BYTES	(size of datastructure) 	
 */

#include "../system.h"

#if USE_UMAP

/*==========================================================================*
 *				do_umap					    *
 *==========================================================================*/
PUBLIC int do_umap(m_ptr)
register message *m_ptr;	/* pointer to request message */
{
/* Map virtual address to physical, for non-kernel processes. */
	int seg_type = m_ptr->CP_SRC_SPACE & SEGMENT_TYPE;
	int seg_index = m_ptr->CP_SRC_SPACE & SEGMENT_INDEX;
	vir_bytes offset = m_ptr->CP_SRC_ADDR;
	int count = m_ptr->CP_NR_BYTES;
	int endpt = (int) m_ptr->CP_SRC_ENDPT;
	int proc_nr;
	phys_bytes phys_addr;
	struct proc *rp;
	struct vm_struct *vm_ptr;
	vmm_desc_t *vmm_ptr;

  /* Verify process number. */
  if (endpt == SELF)
	proc_nr = who_p;
  else
	if (! isokendpt(endpt, &proc_nr))
		return(EINVAL);

#ifdef MHYPER
	rp = proc_addr(proc_nr);
	if( running_vm != VM0) {	
#ifdef IPC_DBG
		MHDEBUG("SYSTEM:do_umap: m_source=%d seg_type=%d running_vm:%d\n",m_ptr->m_source, seg_type, running_vm);
#endif /* IPC_DBG */	
		if(rp->p_misc_flags & PROMICUOUS_TASK){
			rp = proc_addr_vm(VM0, proc_nr);
		}else  if(rp->p_misc_flags & VIRTUAL_TASK){
			vm_ptr = &vm_hyper[running_vm];
			if(vm_ptr->vm_flags != VM_RUNNING) 
				MHERROR("SYSTEM: do_umap: the VM is not VM_RUNNING:%d\n",EVMSTATUS);
			vmm_ptr =&vmm_desc(running_vm);
			if (! isokendpt(vmm_ptr->vmm_endpoint, &proc_nr))
				MHERROR("SYSTEM: do_umap: bad endpoint:%d\n",EINVAL);
			rp = proc_addr_vm(VM0, proc_nr);
	 		MHDEBUG(PROC_FORMAT, PROC_FIELDS(rp));				
		}
	}
		  /* See which mapping should be made. */
  switch(seg_type) {
  case LOCAL_SEG:
      phys_addr = umap_local(rp, seg_index, offset, count); 
      break;
  case REMOTE_SEG:
      phys_addr = umap_remote(rp, seg_index, offset, count); 
      break;
  case BIOS_SEG:
      phys_addr = umap_bios(rp, offset, count); 
      break;
  default:
      return(EINVAL);
  }	
		
#else /*  MHYPER */		
		
  /* See which mapping should be made. */
  switch(seg_type) {
  case LOCAL_SEG:
      phys_addr = umap_local(proc_addr(proc_nr), seg_index, offset, count); 
      break;
  case REMOTE_SEG:
      phys_addr = umap_remote(proc_addr(proc_nr), seg_index, offset, count); 
      break;
  case BIOS_SEG:
      phys_addr = umap_bios(proc_addr(proc_nr), offset, count); 
      break;
  default:
      return(EINVAL);
  }
#endif /*  MHYPER */		

  m_ptr->CP_DST_ADDR = phys_addr;
  return (phys_addr == 0) ? EFAULT: OK;
}

#endif /* USE_UMAP */

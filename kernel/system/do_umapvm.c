/* The kernel call implemented in this file:
 *   m_type:	SYS_UMAPVM
 *
 * The parameters for this kernel call are:
 *    m5_i1:	CP_SRC_PROC_NR	(process number)	
 *    m5_c1:	CP_SRC_SPACE	(segment where address is: T, D, or S)
 *   m5_c2:	CP_DST_VMID		destination VMID
 *    m5_l1:	CP_SRC_ADDR	(virtual address)	
 *    m5_l2:	CP_DST_ADDR	(returns physical address)	
 *    m5_l3:	CP_NR_BYTES	(size of datastructure) 	
 */

#include "../system.h"

#if USE_UMAPVM

/*==========================================================================*
 *				do_umapvm					    *
 *==========================================================================*/
PUBLIC int do_umapvm(m_ptr)
register message *m_ptr;	/* pointer to request message */
{
/* Map virtual address to physical, for non-kernel processes. */
  int seg_type = m_ptr->CP_SRC_SPACE & SEGMENT_TYPE;
  int seg_index = m_ptr->CP_SRC_SPACE & SEGMENT_INDEX;
  vir_bytes offset = m_ptr->CP_SRC_ADDR;
  int count = m_ptr->CP_NR_BYTES;
  int endpt = (int) m_ptr->CP_SRC_ENDPT;
  int vmid = (int) m_ptr->CP_DST_VMID;
  int proc_nr;
  phys_bytes phys_addr;

#ifdef IPC_DBG
	if(vmid != VM0)
		MHDEBUG("SYSTEM:do_umapvm: m_source=%d seg_type=%d endpt=%d vmid=%d \n"
			,m_ptr->m_source, seg_type, endpt, vmid);
#endif /* IPC_DBG */  

	if(vmid < 0 || vmid > NR_VMS)  return(EINVAL);
	if( running_vm != VM0 ) return(EPERM);

  /* Verify process number. */
  if (endpt == SELF)
	proc_nr = who_p;
  else
  	if( !isokendpt_vm(vmid, endpt, &proc_nr))
		return(EINVAL);

  /* See which mapping should be made. */
  switch(seg_type) {
  case LOCAL_SEG:
      phys_addr = umap_local(proc_addr_vm(vmid, proc_nr), seg_index, offset, count); 
      break;
  case REMOTE_SEG:
      phys_addr = umap_remote(proc_addr_vm(vmid, proc_nr), seg_index, offset, count); 
      break;
  case BIOS_SEG:
      phys_addr = umap_bios(proc_addr_vm(vmid, proc_nr), offset, count); 
      break;
  default:
      return(EINVAL);
  }
  m_ptr->CP_DST_ADDR = phys_addr;
  return (phys_addr == 0) ? EFAULT: OK;
}

#endif /* USE_UMAP */

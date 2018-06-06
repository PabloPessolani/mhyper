/* The kernel call implemented in this file:
 *   m_type:	SYS_VCOPYVM
 *
 * The parameters for this kernel call are:
 *    m5_c1:	CP_SRC_VMID		source VMID
 *    m5_l1:	CP_SRC_ADDR		source offset within segment
 *    m5_i1:	CP_SRC_PROC_NR	source process number
 *    m5_c2:	CP_DST_VMID		destination VMID
 *    m5_l2:	CP_DST_ADDR		destination offset within segment
 *    m5_i2:	CP_DST_PROC_NR	destination process number
 *    m5_l3:	CP_NR_BYTES		number of bytes to copy
 */


#include "../system.h"
#include <minix/type.h>

#ifdef MHYPER 
#if (USE_VCOPYVM)

/*===========================================================================*
 *				do_vcopyvm					     *
 *===========================================================================*/
PUBLIC int do_vcopyvm(m_ptr)
register message *m_ptr;	/* pointer to request message */
{
/* Handle sys_vcopyvm() and sys_pcopyvm().  Copy data using virtual or
 * physical addressing. Although a single handler function is used, there 
 * are two different kernel calls so that permissions can be checked. 
 */
  struct vir_addr_vm vir_addr[2];	/* virtual source and destination address */
  phys_bytes bytes;			/* number of bytes to copy */
  int i, rcode;

	if( running_vm != VM0 ) return(EPERM);

	
  /* Dismember the command message. */
  vir_addr[_SRC_].proc_nr_e 	= m_ptr->CP_SRC_ENDPT;
  vir_addr[_SRC_].segment     = 'D';
  vir_addr[_SRC_].offset 	= (vir_bytes) m_ptr->CP_SRC_ADDR;
  vir_addr[_SRC_].vmid   	= m_ptr->CP_SRC_VMID;

  vir_addr[_DST_].proc_nr_e 	= m_ptr->CP_DST_ENDPT;
  vir_addr[_DST_].segment     = 'D';
  vir_addr[_DST_].offset 	= (vir_bytes) m_ptr->CP_DST_ADDR;
  vir_addr[_DST_].vmid 		= m_ptr->CP_DST_VMID;

  bytes = (phys_bytes) m_ptr->CP_NR_BYTES;

#ifdef IPC_DBG
  		MHDEBUG("do_vcopyvm: src=%d running_vm=%d\n",  m_ptr->m_source,running_vm);
#endif /* IPC_DBG */	
		
  /* Now do some checks for both the source and destination virtual address.
   * This is done once for _SRC_, then once for _DST_. 
   */
  for (i=_SRC_; i<=_DST_; i++) {
	int p;

/* #define IPC_DBG 1  */
#ifdef IPC_DBG
		MHDEBUG("do_vcopyvm[%d] endp=%d, offse=%X, vmid=%d\n", i,
			vir_addr[i].proc_nr_e, vir_addr[i].offset, vir_addr[i].vmid);
#endif /* IPC_DBG */	

      /* Check if process number was given implictly with SELF and is valid. */
		if (vir_addr[i].proc_nr_e == SELF)
			vir_addr[i].proc_nr_e = m_ptr->m_source;
		if (vir_addr[i].segment != PHYS_SEG &&
			!isokendpt_vm(vir_addr[i].vmid, vir_addr[i].proc_nr_e, &p)) {
			MHDEBUG("do_vcopyvm[%d] source=%d endp=%d, offset=%X, vmid=%d\n", i, m_ptr->m_source,
			vir_addr[i].proc_nr_e, vir_addr[i].offset, vir_addr[i].vmid);
			MHRETURN(EINVAL);
		}
		/* Check if physical addressing is used without SYS_PHYSCOPY. */
		if ((vir_addr[i].segment & PHYS_SEG) &&
			m_ptr->m_type != SYS_PHYSCOPY) return(EPERM);
  }  

  /* Check for overflow. This would happen for 64K segments and 16-bit 
   * vir_bytes. Especially copying by the PM on do_fork() is affected. 
   */
  if (bytes != (vir_bytes) bytes) return(E2BIG);

	rcode = virtual_copy_vm(&vir_addr[_SRC_], &vir_addr[_DST_], bytes);
	if(rcode) {
		MHDEBUG(MSG1_FORMAT, MSG1_FIELDS(m_ptr));
		MHRETURN(rcode);
	}
  /* Now try to make the actual virtual copy. */
  return(OK);
}
#endif /* (USE_VIRCOPY || USE_PHYSCOPY) */

#endif /* MHYPER */

/* The kernel call implemented in this file:
 *   m_type:	SYS_GETVMID
 *
 */


#include "../system.h"

#if USE_GETVMID

/*===========================================================================*
 *			        do_getvmid				     *
 *===========================================================================*/
PUBLIC int do_getvmid(m_ptr)
register message *m_ptr;	/* pointer to request message */
{
  int proc_nr;
  struct proc *rp;			/* process pointer */

  if(!isokendpt(m_ptr->m_source, &proc_nr)) 
	panic("bogus source", m_ptr->m_source);

  rp = proc_addr(proc_nr);   /* aqui deberia estar la VM de SYSTEM incluida en el calculo */
 
  MHDEBUG("SYSTEM: do_getvmid: proc_nr=%d, p_vmid=%d\n", proc_nr, rp->p_vmid); 
  return(rp->p_vmid);
}

#endif /* USE_GETVMID */


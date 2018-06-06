/*===========================================================================*
 *			        fill_proctab						     *
 *===========================================================================*/
PRIVATE int fill_proctab(int vmid, int nbprocs)
{
	int i;
	struct vm_desc_s *vm_ptr;

	vm_ptr =&vm_desc[vmid];
	ip = vm_ptr->hdr[i];

  for (i=0; i < nbprocs ; ++i) {
	ip = &vm_ptr->hdr[i];				/* process' attributes 	*/
	rp = proc_addr(ip->proc_nr);		/* get process pointer 	*/ /* ESTO DEPENDE DE LA VM */

	ip->endpoint = rp->p_endpoint;		/* ipc endpoint 		*/
	rp->p_max_priority = ip->priority;		/* max scheduling priority */
	rp->p_priority = ip->priority;		/* current priority 	*/
	rp->p_quantum_size = ip->quantum;		/* quantum size in ticks */
	rp->p_ticks_left = ip->quantum;		/* current credit */
	strncpy(rp->p_name, ip->proc_name, P_NAME_LEN); /* set process name */
	(void) get_priv(rp, (ip->flags & SYS_PROC));    /* assign structure */
	priv(rp)->s_flags = ip->flags;			/* process flags */
	priv(rp)->s_trap_mask = ip->trap_mask;		/* allowed traps */
	priv(rp)->s_call_mask = ip->call_mask;		/* kernel call mask */
	priv(rp)->s_ipc_to.chunk[0] = ip->ipc_to;	/* restrict targets */

	if (iskerneln(proc_nr(rp))) {		/* part of the kernel? */ 
		if (ip->stksize > 0) {		/* HARDWARE stack size is 0 */
			rp->p_priv->s_stack_guard = (reg_t *) ktsb;			/* ESTO DEPENDE DE LA VM */
			*rp->p_priv->s_stack_guard = STACK_GUARD;				/* OJO QUE SON DIRECCIONES VIRTUALES */
		}
		ktsb += ip->stksize;	/* point to high end of stack */		
		rp->p_reg.sp = ktsb;	/* this task's initial stack ptr */ 	/* OJO QUE SON DIRECCIONES VIRTUALES */
		text_base = kinfo.code_base >> CLICK_SHIFT;
					/* processes that are in the kernel */
		hdrindex = 0;		/* all use the first a.out header */
	} else {
		hdrindex = 1 + i-NR_TASKS;	/* servers, drivers, INIT */
	}

	/* The bootstrap loader created an array of the a.out headers at	*/
	 * absolute address 'aout'. Get one element to e_hdr.
	 */
	phys_copy(aout + hdrindex * A_MINHDR, vir2phys(&e_hdr),			/* OJO QUE SON DIRECCIONES VIRTUALES */
						(phys_bytes) A_MINHDR);
	/* Convert addresses to clicks and build process memory map */
	text_base = e_hdr.a_syms >> CLICK_SHIFT;
	text_clicks = (e_hdr.a_text + CLICK_SIZE-1) >> CLICK_SHIFT;
	if (!(e_hdr.a_flags & A_SEP)) text_clicks = 0;	   /* common I&D */
	data_clicks = (e_hdr.a_total + CLICK_SIZE-1) >> CLICK_SHIFT;
	rp->p_memmap[T].mem_phys = text_base;
	rp->p_memmap[T].mem_len  = text_clicks;
	rp->p_memmap[D].mem_phys = text_base + text_clicks;
	rp->p_memmap[D].mem_len  = data_clicks;
	rp->p_memmap[S].mem_phys = text_base + text_clicks + data_clicks;
	rp->p_memmap[S].mem_vir  = data_clicks;	/* empty - stack is in data */

	/* Set initial register values.  The processor status word for tasks 
	 * is different from that of other processes because tasks can
	 * access I/O; this is not allowed to less-privileged processes 
	 */
	rp->p_reg.pc = (reg_t) ip->initial_pc;
	rp->p_reg.psw = (iskernelp(rp)) ? INIT_TASK_PSW : INIT_PSW;

	/* Initialize the server stack pointer. Take it down one word
	 * to give crtso.s something to use as "argc".
	 */
	if (isusern(proc_nr(rp))) {		/* user-space process? */ 
		rp->p_reg.sp = (rp->p_memmap[S].mem_vir +
				rp->p_memmap[S].mem_len) << CLICK_SHIFT;
		rp->p_reg.sp -= sizeof(reg_t);
	}
	
	/* Set ready. The HARDWARE task is never ready. */
	if (rp->p_nr != HARDWARE) {
		rp->p_rts_flags = 0;		/* runnable if no flags */
		lock_enqueue(rp);		/* add to scheduling queues */
	} else {
		rp->p_rts_flags = NO_MAP;	/* prevent from running */
	}

	/* Code and data segments must be allocated in protected mode. */
	alloc_segments(rp);
  }


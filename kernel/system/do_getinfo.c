/* The kernel call implemented in this file:
 *   m_type:	SYS_GETINFO
 *
 * The parameters for this kernel call are:
 *    m1_i3:	I_REQUEST	(what info to get)	
 *    m1_p1:	I_VAL_PTR 	(where to put it)	
 *    m1_i1:	I_VAL_LEN 	(maximum length expected, optional)	
 *    m1_p2:	I_VAL_PTR2	(second, optional pointer)	
 *    m1_i2:	I_VAL_LEN2_E	(second length or process nr)	
 */

#include "../system.h"

static unsigned long bios_buf[1024];	/* 4K, what about alignment */
static vir_bytes bios_buf_vir, bios_buf_len;

#if USE_GETINFO

/*===========================================================================*
 *			        do_getinfo				     *
 *===========================================================================*/
PUBLIC int do_getinfo(m_ptr)
register message *m_ptr;	/* pointer to request message */
{
/* Request system information to be copied to caller's address space. This
 * call simply copies entire data structures to the caller.
 */
  size_t length;
  phys_bytes src_phys; 
  phys_bytes dst_phys; 
  int proc_nr, nr_e, nr, p_id, vmid;

#ifdef MHYPER
	register struct proc *caller_ptr;

    caller_ptr = proc_addr(who_p);
#endif /*  MHYPER */

  /* Set source address and length based on request type. */
  switch (m_ptr->I_REQUEST) {	
#ifdef MHYPER
    case SET_IS_VM:
	 MHDEBUG("SET_IS_VM: vmid=%d\n", m_ptr->I_VAL_LEN );
	if( m_ptr->I_VAL_LEN < 0 || m_ptr->I_VAL_LEN >= NR_VMS) return(EINVAL);
	if( !(vm_hyper[m_ptr->I_VAL_LEN].vm_flags & VM_LOADED) ) return(EVMSTATUS);
	IS_active_vm = m_ptr->I_VAL_LEN;
	return(OK);
	break;
#endif /*  MHYPER */
    case GET_MACHINE: {
#ifdef MHYPER
		if( strcmp(caller_ptr->p_name,"is") == 0 && caller_ptr->p_vmid == VM0) {
			MHDEBUG("IS request machine info on vm=%d\n", IS_active_vm);
			src_phys = VIR2PHYS(&vm_hyper[IS_active_vm].vm_machine);		
		}else {
			src_phys = VIR2PHYS(&machine);
		}
        length = sizeof(machine_t);
#else /*  MHYPER */
        length = sizeof(struct machine);
        src_phys = vir2phys(&machine);
#endif /*  MHYPER */
        break;
    }
    case GET_KINFO: {
#ifdef MHYPER
		kinfo.metric_addr = (int) &metric;
		if( strcmp(caller_ptr->p_name,"is") == 0 && caller_ptr->p_vmid == VM0) {
			MHDEBUG("IS request kernel info on vm=%d\n", IS_active_vm);
			src_phys = VIR2PHYS(&vm_hyper[IS_active_vm].vm_kinfo);		
		}else {
			src_phys = VIR2PHYS(&kinfo);
		}
		length = sizeof(kinfo_t);
#else /*  MHYPER */
        length = sizeof(struct kinfo);
        src_phys = vir2phys(&kinfo);
#endif /*  MHYPER */
        break;
    }
    case GET_LOADINFO: {
#ifdef MHYPER
		if( strcmp(caller_ptr->p_name,"is") == 0 && caller_ptr->p_vmid == VM0) {
			MHDEBUG("IS request load info on vm=%d\n", IS_active_vm);
			src_phys = VIR2PHYS(&vm_hyper[IS_active_vm].vm_kloadinfo);		
		}else {
			src_phys = VIR2PHYS(&kloadinfo);
		}
        length = sizeof(loadinfo_t);
#else /*  MHYPER */
        length = sizeof(struct loadinfo);
        src_phys = vir2phys(&kloadinfo);
#endif /*  MHYPER */
        break; 
    }
    case GET_IMAGE: {
#ifdef MHYPER
		if( strcmp(caller_ptr->p_name,"is") == 0 && caller_ptr->p_vmid == VM0) {
	        	length = sizeof(struct boot_image) * vm_hyper[IS_active_vm].vm_VMM.vmm_img.nr_bprocs;
			MHDEBUG("IS request boot image on vm=%d nr_bprocs=%d\n", 
				IS_active_vm,  vm_hyper[IS_active_vm].vm_VMM.vmm_img.nr_bprocs);
			src_phys = VIR2PHYS(&vmm_desc(IS_active_vm).vmm_img.image);		
		}else {
			MHDEBUG("%d request boot image on vm=%d nr_bprocs=%d NR_BOOT_PROCS=%d\n", 
				m_ptr->m_source , running_vm , 
				vm_hyper[running_vm].vm_VMM.vmm_img.nr_bprocs,
				NR_BOOT_PROCS);
	        length = sizeof(struct boot_image) *  vm_hyper[running_vm].vm_VMM.vmm_img.nr_bprocs;
			src_phys = VIR2PHYS(&vmm_desc(running_vm).vmm_img.image);
		}
#else /*  MHYPER */
        length = sizeof(struct boot_image) * NR_BOOT_PROCS;
        src_phys = vir2phys(image);
#endif /*  MHYPER */
        break;
    }
    case GET_IRQHOOKS: {
        length = sizeof(struct irq_hook) * NR_IRQ_HOOKS;
#ifndef MHYPER
        src_phys = VIR2PHYS(&irq_hooks);
#else /*  MHYPER */
        src_phys = vir2phys(irq_hooks);
#endif /*  MHYPER */
        break;
    }
    case GET_SCHEDINFO: {
    /* This is slightly complicated because we need two data structures
         * at once, otherwise the scheduling information may be incorrect.
         * Copy the queue heads and fall through to copy the process table. 
         */
		 
		length = sizeof(struct proc *) * NR_SCHED_QUEUES;
        src_phys = vir2phys(rdy_head);
#ifdef MHYPER
		if( strcmp(caller_ptr->p_name,"is") == 0 && caller_ptr->p_vmid == VM0) {
			MHDEBUG("IS request scheduling information on vm=%d dst=%X\n", IS_active_vm, m_ptr->I_VAL_PTR2);
		}
		isokendpt_vm(running_vm, m_ptr->m_source, &proc_nr);
		dst_phys = numap_local(proc_nr, (vir_bytes) m_ptr->I_VAL_PTR2,
				length); 
		if (src_phys == 0 || dst_phys == 0) return(EFAULT);
		phys_copy(src_phys, dst_phys, length);
		if( strcmp(caller_ptr->p_name,"is") == 0 && caller_ptr->p_vmid == VM0) {
			length = sizeof(struct proc) * (NR_PROCS + NR_TASKS) * NR_VMS;
			src_phys = VIR2PHYS(&vm_hyper[VM0]);
			break;
		}
#else /* MHYPER */
		okendpt(m_ptr->m_source, &proc_nr);
        dst_phys = numap_local(proc_nr, (vir_bytes) m_ptr->I_VAL_PTR2,
                length); 
        if (src_phys == 0 || dst_phys == 0) return(EFAULT);
        phys_copy(src_phys, dst_phys, length);
#endif /* MHYPER */
       /* fall through */
    }
    case GET_PROCTAB: {
        length = sizeof(struct proc) * (NR_PROCS + NR_TASKS);
#ifdef MHYPER
		if( strcmp(caller_ptr->p_name,"is") == 0 && caller_ptr->p_vmid == VM0) {
			MHDEBUG("IS request process table on vm=%d\n", IS_active_vm);
			src_phys = VIR2PHYS(&vm_hyper[IS_active_vm].vm_proc[0]);
			if( IS_active_vm != VM0) {
				vm_hyper[IS_active_vm].vm_proc[IDLE+NR_TASKS].p_user_time = 	get_uptime() - vm_hyper[IS_active_vm].vm_uptime;
				vm_hyper[IS_active_vm].vm_proc[IDLE+NR_TASKS].p_user_time -=	vm_hyper[IS_active_vm].vm_time;	
				MHDEBUG("uptime=%ld vm_uptime=%ld vm_time=%ld idle_time=%ld\n", 
					get_uptime(), vm_hyper[IS_active_vm].vm_uptime, 
					vm_hyper[IS_active_vm].vm_time, 
					vm_hyper[IS_active_vm].vm_proc[IDLE+NR_TASKS].p_user_time );
			}		
		} else if( caller_ptr->p_misc_flags&PROXY_SERVER  && caller_ptr->p_vmid == VM0 ) {
			MHDEBUG("Proxy Server request process table on vm=%d\n", caller_ptr->p_proxy);
			src_phys = VIR2PHYS(&vm_hyper[caller_ptr->p_proxy].vm_proc[0]);
		} else {
			src_phys = VIR2PHYS(BEG_PROC_ADDR); 
		}
#else  /*  MHYPER */
        src_phys = vir2phys(proc);
#endif /*  MHYPER */
        break;
    }
    case GET_PRIVTAB: {
        length = sizeof(struct priv) * (NR_SYS_PROCS);
#ifdef MHYPER
		if( strcmp(caller_ptr->p_name,"is") == 0 && caller_ptr->p_vmid == VM0) {
			MHDEBUG("IS request process table on vm=%d\n", IS_active_vm);
			src_phys = VIR2PHYS(&vm_hyper[IS_active_vm].vm_priv[0]);		
		}else {
			src_phys = VIR2PHYS(BEG_PRIV_ADDR);  
		}
#else /*  MHYPER */
        src_phys = vir2phys(priv);
#endif /*  MHYPER */
        break;
    }
    case GET_PROC: {
        nr_e = (m_ptr->I_VAL_LEN2_E == SELF) ?
		m_ptr->m_source : m_ptr->I_VAL_LEN2_E;
		if(!isokendpt(nr_e, &nr)) return EINVAL; /* validate request */
        length = sizeof(struct proc);
		if( caller_ptr->p_misc_flags&PROXY_SERVER  && caller_ptr->p_vmid == VM0 ) {
			MHDEBUG("Proxy Server request process desc=%d on vm=%d\n", nr_e, caller_ptr->p_proxy);
			src_phys = VIR2PHYS(&vm_hyper[caller_ptr->p_proxy].vm_proc[nr+NR_TASKS]);
		}else{
			src_phys = VIR2PHYS(proc_addr(nr));
		}
        break;
    }
	case GET_PRIV_VM: {
        p_id = m_ptr->I_VAL_PRIV;
		if( p_id < 0 || p_id >= NR_SYS_PROCS) return EINVAL; /* validate request */
		vmid = m_ptr->I_VAL_VMID;
		if( vmid < 0 || vmid >= NR_VMS) return EBADVMID; /* validate request */
        length = sizeof(struct priv);
		if( (caller_ptr->p_misc_flags & PROXY_SERVER)  && (caller_ptr->p_vmid == VM0) ) {
			if(vmid != caller_ptr->p_proxy) return EPROCVMID;
			MHDEBUG("Proxy Server request privilege p_id=%d on vmid=%d\n", p_id, vmid);
		}
		src_phys = VIR2PHYS(&vm_hyper[vmid].vm_priv[p_id]);
        break;
    }
    case GET_MONPARAMS: {
#ifdef MHYPER
		if( strcmp(caller_ptr->p_name,"is")==0 && caller_ptr->p_vmid == VM0) {
			MHDEBUG("IS request monitor parms on vm=%d\n", IS_active_vm);
			if( IS_active_vm != VM0) {
				src_phys = VIR2PHYS(vm_hyper[IS_active_vm].vm_kinfo.params_base);	
			} else {		
				src_phys = kinfo.params_base;		/* already is a physical */
			}
		}else {
			MHDEBUG("do_getinfo GET_MONPARAMS %s p_vmid=%d\n", 
				caller_ptr->p_name,caller_ptr->p_vmid);
			if( running_vm != VM0) {
				src_phys = VIR2PHYS(vm_hyper[caller_ptr->p_vmid].vm_kinfo.params_base);	
			} else {		
				src_phys = kinfo.params_base;		/* already is a physical */
			}
		}
		length = kinfo.params_size;
		break;
    }
#else /*  MHYPER */
        src_phys = kinfo.params_base;		/* already is a physical */
        length = kinfo.params_size;
        break;
    }
#endif /*  MHYPER */
    case GET_RANDOMNESS: {		
        static struct randomness copy;		/* copy to keep counters */
		int i;

        copy = krandom;
        for (i= 0; i<RANDOM_SOURCES; i++) {
			krandom.bin[i].r_size = 0;	/* invalidate random data */
			krandom.bin[i].r_next = 0;
		}
    	length = sizeof(struct randomness);
#ifdef MHYPER
    	src_phys = VIR2PHYS(&copy);
#else /*  MHYPER */
    	src_phys = vir2phys(&copy);
#endif /*  MHYPER */		
    	break;
    }
    case GET_KMESSAGES: {
        length = sizeof(struct kmessages);
#ifndef MHYPER
		if( strcmp(caller_ptr->p_name,"is") == 0 && caller_ptr->p_vmid == VM0) {
			MHDEBUG("IS request kernel messages\n", IS_active_vm);
			src_phys = VIR2PHYS(vm_hyper[IS_active_vm].vm_kmess);		
		}else {
			src_phys = VIR2PHYS(&kmess);
		}
#else /*  MHYPER */
        src_phys = vir2phys(&kmess);
#endif /*  MHYPER */		
        break;
    }
#if DEBUG_TIME_LOCKS
    case GET_LOCKTIMING: {
    length = sizeof(timingdata);
#ifndef MHYPER
    src_phys = VIR2PHYS(timingdata);
#else /*  MHYPER */
    src_phys = vir2phys(timingdata);
#endif /*  MHYPER */	
    break;
    }
#endif
    case GET_BIOSBUFFER:
    	bios_buf_vir = (vir_bytes)bios_buf;
    	bios_buf_len = sizeof(bios_buf);

    	length = sizeof(bios_buf_len);
    	src_phys = VIR2PHYS(&bios_buf_len);
		if (length != m_ptr->I_VAL_LEN2_E) return (EINVAL);
		if(!isokendpt(m_ptr->m_source, &proc_nr))
			panic("bogus source", m_ptr->m_source);
		dst_phys = numap_local(proc_nr, (vir_bytes) m_ptr->I_VAL_PTR2, length); 
		if (src_phys == 0 || dst_phys == 0) return(EFAULT);
		phys_copy(src_phys, dst_phys, length);
    	length = sizeof(bios_buf_vir);
#ifndef MHYPER
    	src_phys = VIR2PHYS(&bios_buf_vir);
#else /*  MHYPER */
    	src_phys = vir2phys(&bios_buf_vir);
#endif /*  MHYPER */
    	break;

    case GET_IRQACTIDS: {
        length = sizeof(irq_actids);
#ifndef MHYPER
      src_phys = VIR2PHYS(irq_actids);
#else /*  MHYPER */
      src_phys = vir2phys(irq_actids);
#endif /*  MHYPER */  
        break;
    }

    default:
        return(EINVAL);
  }

  /* Try to make the actual copy for the requested data. */
  if (m_ptr->I_VAL_LEN > 0 && length > m_ptr->I_VAL_LEN) return (E2BIG);
  if(!isokendpt(m_ptr->m_source, &proc_nr)) 
	panic("bogus source", m_ptr->m_source);
  dst_phys = numap_local(proc_nr, (vir_bytes) m_ptr->I_VAL_PTR, length); 
  if (src_phys == 0 || dst_phys == 0) return(EFAULT);
  phys_copy(src_phys, dst_phys, length);
  return(OK);
}

#endif /* USE_GETINFO */


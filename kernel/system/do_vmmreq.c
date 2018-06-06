#include <a.out.h>
#include <sys/stat.h>
#include <string.h>
#include "../kernel.h"
#include "../system.h"
#include <signal.h>
#include <minix/endpoint.h>
#include <sys/sigcontext.h>
#include <minix/vmconfig.h>
#include "../drivers.h"

#ifdef MHYPER


#include "../../servers/vmm/const.h"
#include "../../servers/vmm/param.h"

#define FILLED_MASK	(~0)

FORWARD _PROTOTYPE(int mh_start, (int vmid));
FORWARD _PROTOTYPE(int mh_main, (vmm_desc_t *vmm_ptr, struct proc *lp));
FORWARD _PROTOTYPE(int fill_vmmap, (int vmid, message *m_ptr));
FORWARD _PROTOTYPE(int set_loader_priv, (struct proc *lp));
FORWARD _PROTOTYPE(int set_boot_parms,(int vmid));
FORWARD _PROTOTYPE(int local_vcopy, (int src_ep, char src_seg, long src_addr, int dst_ep, char dst_seg, long dst_addr, int bytes));
FORWARD _PROTOTYPE(int do_vm_load, (message *m_ptr));
FORWARD _PROTOTYPE(int do_vm_start, (message *m_ptr));
FORWARD _PROTOTYPE(void set_mem_chunks,(int vmid));

#define MAXTOKEN 8
char *token[MAXTOKEN] = {
	"rootdev",
	"ramimagedev",
	"ramsize",
	"processor",
	"bus",
	"video",
	"image",
	"memory"  /* MUST BE THE LAST!! */
};

#ifdef MHDEBUG
#undef MHDEBUG
#define MHDEBUG kprintf
#endif
 
 
/*===========================================================================*
 *			        do_vmmreq				     *
 *===========================================================================*/
PUBLIC int do_vmmreq(m_ptr)
register message *m_ptr;	/* pointer to request message */
{
	int proc_nr, i;
	struct proc *rp;		/* process pointer */
	int vmmcmd, vmid, rcode;
static	message m;
static	proc_cfg_t proc_cfg, *pc_ptr;
	struct vm_struct  *vm_ptr;
	vm_drivers_t *d_ptr;

  
  if(!isokendpt(m_ptr->m_source, &proc_nr)) 
	panic("Bad source endpoint", m_ptr->m_source);

  rp = proc_addr(proc_nr);   

  MHDEBUG("SYSTEM: do_vmmreq: who=%d vmm_cmd=%X i2=%d i3=%d p1=%X p2=%X p3=%X \n"
		,rp->p_endpoint	
		,m_ptr->VMM_cmd
		,m_ptr->VMM_i2
		,m_ptr->VMM_i3
		,m_ptr->VMM_p1
		,m_ptr->VMM_p2
		,m_ptr->VMM_p3
		);
 
	if( proc_nr != VMM_PROC_NR && proc_nr != PM_PROC_NR) 
		MHERROR("SYSTEM: do_vmmcmd bogus process trying to make VMMCMD. rcode=%d\n", EBADSRCDST);

	vmmcmd = m_ptr->VMM_cmd;	/* VMM call number */

	rcode = OK;
	vmid 	  = m_ptr->VMM_vmid;
	MHDEBUG("SYSTEM: cmd=%d  vmid=%d\n", vmmcmd, vmid);
	if( vmid < 0 || vmid >= NR_VMS) 
			MHERROR("SYSTEM: invalid vmid. error:%d\n",EINVAL);
		
  switch(vmmcmd){
	case VMM_LOAD:
		rcode = do_vm_load(m_ptr);
		break;	
	case VMM_START:
		rcode = do_vm_start(m_ptr);
		break;	
	case VMM_KILL:
		MHDEBUG("SYSTEM: VMM_KILL  vmid=%d\n", vmid);
		break;
	case VMM_STOP:
		MHDEBUG("SYSTEM: VMM_STOP  vmid=%d\n", vmid);
		break;
	case VMM_INFO:
		MHDEBUG("SYSTEM: VMM_INFO  vmid=%d\n", vmid);
		break;
	case VMM_SUSPEND:
		MHDEBUG("SYSTEM: VMM_SUSPEND  vmid=%d\n", vmid);
		/* Remove all ready processes of the VM 	*/
		/* from the Ready queues			*/
		/* change the status of ALL processes of  */
		/* the VM to NO_MAP)				*/
		/* Remove any process of the VM from the  */
		/* SYSTEM proc struct field p_caller_q 	*/
		/* change their status to SYSSEND	       */
		/* (new status)				*/
		/* set the VM descriptor to VM_SUSPENDED	*/
		break;
	case VMM_RESUME:
		MHDEBUG("SYSTEM: VMM_RESUME  vmid=%d\n", vmid);
		/* inserts all VM processes with SYSSEND  */
		/* into the SYSTEM proc struct field 	*/
		/* p_caller_q					*/
		/* removes the status flag NO_MAP of ALL  */
		/* processes of the VM 			*/
		/* Insert into the READY queues all 	*/
		/* processes with the READY status		*/
		break;
	case VMM_SAVE:	
		MHDEBUG("SYSTEM: VMM_SAVE  vmid=%d\n", vmid);
		/* Copies the VM struct of a SUSPENDED VM */
		/* to VMM					*/
		break;
	case VMM_RESTORE:
		MHDEBUG("SYSTEM: VMM_RESTORE  vmid=%d\n", vmid);
		/* Copies the VM struct from the VMM into */
		/* a VM struct				*/
		break;
	case VMM_VMDUMP:
		MHDEBUG("SYSTEM: VMM_VMDUMP  vmid=%d\n", vmid);
			/* copy the hypervisor VM descriptor to Userspace */
		rcode = local_vcopy(m_ptr->m_source,D, (long) m_ptr->m1_p1, 
					SYSTEM, D, (long) &vmm_desc(vmid), sizeof(vmm_desc_t));
		if( rcode != OK) 
			MHERROR("SYSTEM: do_vmmreq: VMM_VMDUMP local_vcopy error:%d\n",rcode);
		break;
	case VMM_PROC:
		/* copy from vmmcmd (m1_i3) to kernel */
		MHDEBUG("SYSTEM: VMM_PROC  vmid=%d\n", vmid);
		rcode = local_vcopy(m_ptr->m1_i3,D, (long) m_ptr->m1_p1, 
					SYSTEM, D, (long) &proc_cfg, sizeof(proc_cfg_t));
		if( rcode != OK) 
			MHERROR("SYSTEM: do_vmmreq: VMM_PROC local_vcopy error:%d\n",rcode);
		pc_ptr = &proc_cfg;
		vm_ptr = &vm_hyper[vmid];
		for( i= 0; i < NR_SYS_PROCS; i++){
			if( !strncmp(vm_ptr->vm_driver[i].drv_name,pc_ptr->p_name,P_NAME_LEN-1)){
				MHDEBUG("SYSTEM: VMM_PROC match %s\n", pc_ptr->p_name);
				d_ptr =&vm_ptr->vm_driver[i];
				d_ptr->drv_type = pc_ptr->p_flags;
				d_ptr->drv_u.drv_bitmap = pc_ptr->p_bitmap;
				MHDEBUG(DRV_FORMAT, DRV_FIELDS(d_ptr));
				break;
			} 
		}
		if( i == NR_SYS_PROCS) {
			for( i= 0; i < NR_SYS_PROCS; i++){
				if( strcmp(vm_ptr->vm_driver[i].drv_name,"NO_DRIVER") ){
					MHDEBUG("SYSTEM: VMM_PROC new driver %s\n", pc_ptr->p_name);
					d_ptr =&vm_ptr->vm_driver[i];
					d_ptr->drv_type = pc_ptr->p_flags;
					strcpy(d_ptr->drv_name,pc_ptr->p_name);
					d_ptr->drv_u.drv_bitmap = 0;
					MHDEBUG(DRV_FORMAT, DRV_FIELDS(d_ptr));
					break;
				}
			}
		}
		if( i == NR_SYS_PROCS) {
			MHERROR("SYSTEM: do_vmmreq: VMM_PROC no space for driver:%d\n",ENOSPC);
		}
		rcode = OK;
		break;
	default:
		MHERROR("PM: do_vmmreq: ERROR, must not be here! rcode=%d\n",ENOSYS);
		break;
  }

 return(rcode);
}

/*============================================================================*
 *			        mh_start									*
 * Emulates MINIX kernel/start.c									*
 *===========================================================================*/
PRIVATE int mh_start( int vmid)
{
	int h, i, len, rcode;
	char *tk_ptr;
	kinfo_t *kp;
	machine_t *mp;
	char *src_params, *dst_params;
	vmm_desc_t *vmm_ptr;
	imginfo_t  *img_ptr;
	vm_mmap_t	*vmmap_ptr;
static	message m;
	
	MHDEBUG("mh_start: vmid=%d running_vm=%d\n",vmid, running_vm);
	kp = &vm_hyper[vmid].vm_kinfo;
	mp = &vm_hyper[vmid].vm_machine;
	
	memcpy( (char *)kp , (char *)&vm_hyper[VM0].vm_kinfo  , sizeof(kinfo_t));
	memcpy( (char *)mp , (char *)&vm_hyper[VM0].vm_machine, sizeof(machine_t));

	vmm_ptr =&vmm_desc(vmid);
	img_ptr =&vmm_ptr->vmm_img;
	vmmap_ptr = &vm_hyper[vmid].vm_mmap;

	kp->proc_addr = (vir_bytes) BEG_PROC_ADDR;
  
	/* kernel memory layout (/dev/kmem) */
	kp->kmem_base = (vmmap_ptr->vmmap_start.mem_phys) << CLICK_SHIFT;	
	kp->kmem_size = (vmmap_ptr->vmmap_top.mem_phys - vmmap_ptr->vmmap_start.mem_phys) << CLICK_SHIFT;

	kp->bootdev_base=  0;	/* boot device from boot image (/dev/boot) */
	kp->bootdev_size=  0;
	kp->ramdev_base =  0;	/* ram device from boot image (/dev/ram) */
	kp->ramdev_size =  0;
	
	/* Set VM boot parameters equal to VM0's parameters except MEMORY CHUNKS! */
	len = set_boot_parms(vmid);
	kinfo.params_base = (long) &vm_hyper[vmid].vm_boot_params; 	/* address of boot parameters in VM boot loader */
	kinfo.params_size = len;				/* new parameter lenght */

	kinfo.vmid      = vmid;
	kinfo.proc_addr = (vir_bytes) BEG_PROC_ADDR;
	
	MHDEBUG(KINFO_FORMAT1, KINFO_FIELDS1(kp));
	MHDEBUG(KINFO_FORMAT2, KINFO_FIELDS2(kp));
	MHDEBUG(KINFO_FORMAT3, KINFO_FIELDS3(kp));
	MHDEBUG(KINFO_FORMAT4, KINFO_FIELDS4(kp));
	MHDEBUG(MACH_FORMAT, MACH_FIELDS(mp));
		
  	/* Load average data initialization. */
	kloadinfo.proc_last_slot = 0;
	for(h = 0; h < _LOAD_HISTORY; h++)
		kloadinfo.proc_load_history[h] = 0;

	return(OK);
}

/*============================================================================*
 *			        setup_bproc				*
 * Setup a boot process							*
 *===========================================================================*/
PRIVATE int setup_bproc(int vmid, int imgindex, struct proc *lp)
{
 	struct boot_image *bip;
	struct proc *rp, *rp0, *st;
	int p_nr, ret, q, proc_type, i, p;
    int hdrindex;			/* index to array of a.out headers */
	phys_clicks text_base, text_base_bytes;
    vir_clicks text_clicks, data_clicks;
	struct proc *head_ptr;
	vmm_desc_t *vmm_ptr;
	struct vm_struct  *vm_ptr;
	struct priv *priv_ptr;


	vmm_ptr =&vmm_desc(vmid);
	vm_ptr = &vm_hyper[vmid];

	bip = &vmm_ptr->vmm_img.image[imgindex];
	p_nr = bip->proc_nr;	
	MHDEBUG("setup_bproc: vmid=%d imgindex=%d, name=%s p_nr=%d running_vm=%d\n",
			vmid, imgindex, bip->proc_name, p_nr, running_vm); 

	if( p_nr <= HARDWARE) {
		rp = vect_proc_addr(p_nr); /* need pointers in VMx, NOT in VM0 */
	} else {
		rp = proc_addr(p_nr);
	}

	/* se supone que proc_nr ya esta cargado en el slot al inicializar la VM 	*/
  	/* en el caso de la prueba el p_nr esta para el proceso  MEM 	*/
	rp->p_max_priority  = bip->quantum;	/* bip->priority; TESTING	 max scheduling priority */
	rp->p_priority 	    = bip->priority;	/* bip->priority; TESTING	 current priority 	*/
	rp->p_quantum_size  = bip->quantum;	/* quantum size in ticks */
	rp->p_ticks_left    = bip->quantum;	/* current credit */
	strncpy(rp->p_name, bip->proc_name, P_NAME_LEN); 	/* set process name */

	MHDEBUG("setup_bproc: p_nr=%d load_ep=%d priority=%d\n",p_nr, lp->p_endpoint, rp->p_priority); 

	if( bip->endpoint == 0) {
		rp->p_endpoint = _ENDPOINT(0, rp->p_nr);	/* new endpoint of slot 		*/
		bip->endpoint  = rp->p_endpoint;		/* y si actualizamos el ENDPOINT ??	*/
	}else{
		rp->p_endpoint = bip->endpoint;
	}

	MHDEBUG("setup_bproc: p_nr=%d p_endpoint=%d \n", p_nr, rp->p_endpoint);

	ret = get_priv(rp, (bip->flags & SYS_PROC));   /*   assign priv structure 	*/
    	if( ret != OK) MHERROR("setup_bproc: getpriv error:\n",ret);
	priv(rp)->s_flags = bip->flags;			/* process flags 			*/
	priv(rp)->s_trap_mask = bip->trap_mask;		/* allowed traps 			*/
	priv(rp)->s_call_mask = bip->call_mask;		/* kernel call mask		*/
	priv(rp)->s_ipc_to.chunk[0] = bip->ipc_to;	/*  restrict targets 		*/
	priv(rp)->s_call2proxy = 0; 			/* default bitmap */

  	/* Only one in group should have SIGNALED, child doesn't inherit tracing. */
 	rp->p_rts_flags |= NO_MAP;		/* inhibit process from running */
  	rp->p_rts_flags &= ~(SIGNALED | SIG_PENDING | P_STOP);
  	sigemptyset(&rp->p_pending);
	
	MHDEBUG("setup_bproc: p_nr=%d flags=%X tmask=%X cmask=%X ipc_to=%X \n",
		p_nr, 
		priv(rp)->s_flags,
		priv(rp)->s_trap_mask,
		priv(rp)->s_call_mask,
		priv(rp)->s_ipc_to.chunk[0]
		);
		
	rp->p_reg.retreg = 0;	/* child sees pid = 0 to know it is child */
  	rp->p_user_time = 0;		/* set all the accounting times to 0 */
  	rp->p_sys_time = 0;

	/* Parent and child have to share the quantum that the forked process had,
   	* so that queued processes do not have to wait longer because of the fork.
   	* If the time left is odd, the child gets an extra tick.
   	*/
  	rp->p_ticks_left = (rp->p_ticks_left + 1) / 2;
	rp->p_vmid = vmid;

	if( rp->p_nr <= HARDWARE)
		hdrindex = 0;
	else
		hdrindex = 1 + imgindex-NR_TASKS;	/* servers, drivers, DS */

	/* Convert addresses to clicks and build process memory map */
	text_base = lp->p_memmap[D].mem_phys +  (vmm_ptr->vmm_img.hdr[hdrindex].process.a_syms >> CLICK_SHIFT);
	MHDEBUG("setup_bproc: lp->data_base_phys=%X a_sym=%X text_base=%X\n", 
			lp->p_memmap[D].mem_phys, 
			vmm_ptr->vmm_img.hdr[hdrindex].process.a_syms,
			text_base);
	
	text_clicks = (vmm_ptr->vmm_img.hdr[hdrindex].process.a_text + CLICK_SIZE-1) >> CLICK_SHIFT;
	MHDEBUG("setup_bproc: text_base=%X text_clicks=%X\n", text_base, text_clicks);
 
	if (!(vmm_ptr->vmm_img.hdr[hdrindex].process.a_flags & A_SEP)) 
		text_clicks = 0;	   /* common I&D */
	data_clicks = (vmm_ptr->vmm_img.hdr[hdrindex].process.a_total + CLICK_SIZE-1) >> CLICK_SHIFT;

	rp->p_memmap[T].mem_phys = text_base;					
	rp->p_memmap[T].mem_len  = text_clicks;
	rp->p_memmap[D].mem_phys = text_base + text_clicks;			
	rp->p_memmap[D].mem_len  = data_clicks;
	rp->p_memmap[S].mem_phys = text_base + text_clicks + data_clicks;	
	rp->p_memmap[S].mem_vir  = data_clicks;	/* empty - stack is in data */

 	MHDEBUG("setup_bproc: T_phys=%X T_len=%X D_phys=%X D_len=%X S_phys=%X S_vir=%X S_len=%X\n",
		rp->p_memmap[T].mem_phys,
		rp->p_memmap[T].mem_len,
		rp->p_memmap[D].mem_phys,
		rp->p_memmap[D].mem_len,
		rp->p_memmap[S].mem_phys,
		rp->p_memmap[S].mem_vir,
		rp->p_memmap[S].mem_len);

	rp->p_reg.pc = (reg_t)vmm_ptr->vmm_img.image[imgindex].initial_pc;
	rp->p_reg.psw = (iskernelp(rp)) ? INIT_TASK_PSW : INIT_PSW; 

	MHDEBUG("setup_bproc: PC=%X  PSW=%X proc_name=%s load_ep=%d load_name=%s\n",
		rp->p_reg.pc,rp->p_reg.psw, vmm_ptr->vmm_img.image[imgindex].proc_name, lp->p_endpoint, lp->p_name);

	/* Initialize the server stack pointer. Take it down one word
	 * to give crtso.s something to use as "argc".
	 */
	if (isusern(proc_nr(rp))) {		/* user-space process? */ 
		rp->p_reg.sp = (rp->p_memmap[S].mem_vir +
				rp->p_memmap[S].mem_len) << CLICK_SHIFT;
		rp->p_reg.sp -= sizeof(reg_t);
	}

	if(rp->p_nr == PM_PROC_NR)
		set_mem_chunks(vmid);

	/*
	 * The comparison is made by the process name because the process
	 * number p_nr can vary through versions and dont consider the
	 * drivers and server started via RS
	 */
	proc_type = DISABLED_PROC_TYPE;
	/* check if the boot process will be executed  */
	for( i = 0; i < NR_SYS_PROCS; i++){
		if( MH_LOADED(vm_ptr->vm_driver[i].drv_type) == BOOT_LOADED) {
			if(!strcmp(rp->p_name, vm_ptr->vm_driver[i].drv_name)) {
				proc_type = MH_TYPE(vm_ptr->vm_driver[i].drv_type);
				break;
			}
		}
	}
	
	switch(proc_type) {
		case VIRTUAL_PROC_TYPE:
			MHDEBUG("setup_bproc: process %s is VIRTUAL.\n",rp->p_name);
			rp->p_misc_flags = VIRTUAL_TASK;
			if (priv(rp)->s_id < NR_SYS_PROCS)
				priv(lp)->s_dr_bitmap |= (1 << priv(rp)->s_id);
			else
				MHERROR("setup_bproc: s_id out of range:%d\n",ERANGE);	
			priv_ptr = lp->p_priv;
			MHDEBUG(PRIV_FORMAT, PRIV_FIELDS(priv_ptr));
			break;
		case PROMISCUOUS_PROC_TYPE:
			MHDEBUG("setup_bproc: process %s is PROMISCUOUS.\n",rp->p_name);
			rp->p_misc_flags = PROMICUOUS_TASK;
			rp->p_rts_flags = NO_MAP;

			/* Search the process on VM0 with the same name */
			for( p = IDLE; p < NR_PROCS; p++) {
				rp0 = proc_addr_vm(VM0, p);
				if( rp0->p_rts_flags != SLOT_FREE) {
					if(!strcmp(rp->p_name, rp0->p_name)) {
						MHDEBUG("setup_bproc: process %s MATCH on VM0\n",rp0->p_name);
						break; /* NAME  MATCH */	
					}
				}
			}

			if( p == NR_PROCS ) return(EPROMISCUOUS);

			/* SETs the bit map of the VMs to serve by a  BOOT LOADED PROMISCUOUS  */
			priv(rp0)->s_vm_bitmap |= (1 << vmid);

			/* Store which process number that the promiscuous process will represent on VMx */
			priv(rp0)->s_nr[vmid] = rp->p_nr;

			/* pointer to the promiscuous process on VMx descriptor */
			rp->p_promiscuous = rp0;
	
			/* NOTIFIES all BOOT loaded PROMISCUOUS  processes to serve this vm */
			if( MH_NTFY(vm_ptr->vm_driver[i].drv_type) == BOOT_NTFY) {
				running_vm = VM0;
				systask_ptr->p_vmid = VM0;
				raw_notify(VMM_PROC_NR,rp0->p_nr);
				running_vm = vmid;
				systask_ptr->p_vmid = vmid;
			}
			
			rp->p_priv  = NULL;
			break;
		case REAL_PROC_TYPE:
			MHDEBUG("setup_bproc: process %s is REAL.\n",rp->p_name);
/*			if (rp->p_nr == INIT_PROC_NR) {
				rp->p_rts_flags = SENDING; 		
			} else 
*/
			rp->p_misc_flags = 0;
			/* Set ready. The HARDWARE task is never ready. */
			if (rp->p_nr == HARDWARE) {
				rp->p_rts_flags = NO_MAP; /* prevent from running */
			} else {
				rp->p_rts_flags = 0;	 /* runnable if no flags */
				q = rp->p_priority;
				head_ptr = rdy_head[q];
				if(head_ptr != NULL) {
					rp->p_nextready = head_ptr->p_nextready; 
					head_ptr->p_nextready = rp;
					if( head_ptr == rdy_tail[q]) 
	       				rdy_tail[q] = rp;			/* set new tail */			
				} else {
					rp->p_nextready = NULL;
					rdy_head[q] = rp;
					rdy_tail[q] = rp;
				}
			} 
			alloc_segments(rp);
			/* Paravirtualized REAL processes need kernel call enable bitmap */
			if( vm_ptr->vm_driver[i].drv_type  & PARAVIRTUAL) {
				rp->p_misc_flags = PARAVIR_TASK;
				priv(rp)->s_call2proxy = vm_ptr->vm_driver[i].drv_u.drv_bitmap;
				st = &vm_hyper[rp->p_vmid].vm_proc[SYSTEM+NR_TASKS];
				priv(lp)->s_dr_bitmap |= (1 << priv(st)->s_id);		
				MHDEBUG("setup_bproc: PARAVIRTUAL %s s_call2proxy=%X s_dr_bitmap=%X\n", 
					rp->p_name, priv(rp)->s_call2proxy, priv(lp)->s_dr_bitmap);				
			}
			break;
		default:
			MHDEBUG("setup_bproc: process %s is DISABLED.\n",rp->p_name);
			alloc_segments(rp);
			rp->p_rts_flags = SENDING;
			break;
	}

	rp->p_misc_flags |= MH_LEVEL(vm_ptr->vm_driver[i].drv_type);
	MHDEBUG("setup_bproc: proc_name=%s p_misc_flags=%X\n",
		rp->p_name,rp->p_misc_flags);
		
	return(OK);
}

/*============================================================================*
 *			        clear_VM0_procs									*
* Rollback VM0 proxies and promiscous setup 								*
 *===========================================================================*/
PRIVATE int clear_VM0_procs(int vmid,  int nbprocs, struct proc *lp)
{
	struct proc *rp, *rp0;
	int p_nr, ret, q, proc_type, i, imgindex;
   	int hdrindex;			/* index to array of a.out headers */
	vmm_desc_t *vmm_ptr;
	struct vm_struct  *vm_ptr;
	struct priv *priv_ptr;
	struct boot_image *bip;

	MHDEBUG("clear_VM0_procs: vmid=%d nbprocs=%d imgindex=%d\n",vmid, nbprocs, imgindex); 

	vmm_ptr =&vmm_desc(vmid);
	vm_ptr = &vm_hyper[vmid];

	for( imgindex = 0; i < nbprocs; imgindex++) {
	
		bip = &vmm_ptr->vmm_img.image[imgindex];
		p_nr = bip->proc_nr;	
		MHDEBUG("clear_VM0_procs: vmid=%d imgindex=%d, name=%s p_nr=%d running_vm=%d\n",
			vmid, imgindex, bip->proc_name, p_nr, running_vm); 
		if( p_nr <= HARDWARE) {
			rp = vect_proc_addr(p_nr); /* need pointers in VMx, NOT in VM0 */
		} else {
			rp = proc_addr(p_nr);
		}

		proc_type = DISABLED_PROC_TYPE;
		/* check if the boot process will be executed  */
		for( i = 0; i < NR_SYS_PROCS; i++){
			if( MH_LOADED(vm_ptr->vm_driver[i].drv_type) == BOOT_LOADED) {
				if(!strcmp(rp->p_name, vm_ptr->vm_driver[i].drv_name)) {
					proc_type = MH_TYPE(vm_ptr->vm_driver[i].drv_type);
				break;
				}
			}
		}
		switch(proc_type) {
			case VIRTUAL_PROC_TYPE:
				MHDEBUG("clear_VM0_procs: VIRTUAL %s\n",rp->p_name);
				if (priv(rp)->s_id < NR_SYS_PROCS)
					priv(lp)->s_dr_bitmap &= ~(1 << priv(rp)->s_id);
				else
					MHERROR("clear_VM0_procs: s_id out of range:%d\n",ERANGE);	
				break;
			case PROMISCUOUS_PROC_TYPE:
				MHDEBUG("clear_VM0_procs: PROMISCUOUS %s\n",rp->p_name);
				rp0 = proc_addr_vm(VM0, rp->p_nr);
				if( rp0 == NULL) return(EPROCSTATUS);
				if(strcmp( rp0->p_name, rp->p_name) != 0){
					MHDEBUG("clear_VM0_procs: PROMICUOUS_TASK %s(VM0)!=%s(VM%d) \n",rp0->p_name, rp->p_name, vmid);
					return(EPROMISCUOUS);
				}
				priv(rp0)->s_vm_bitmap &= ~(1 << vmid);
				break;
			default:
				break;
		}
	}
	return(OK);
}

/*============================================================================*
 *			       set_mem_chunks  				*
 * memory chunks will be read by the PM to size the initial memory holes 	* 
 * PM patch the chunks passed by the monitor (here the hypervisor)		*
 * We set only one memory chunk starting in the "PM" TEXT segment		*
 *===========================================================================*/
PRIVATE void set_mem_chunks( int vmid)
{
	int len;
	/* Set VM boot parameters equal to VM0's parameters except MEMORY CHUNKS! */
	len = set_boot_parms(vmid);
	kinfo.params_base = (long) &vm_hyper[vmid].vm_boot_params;  /* address of boot parameters in VM boot loader */
	kinfo.params_size = len;				/* new parameter lenght */	
}
 
/*============================================================================*
 *			        mh_main						*
 * Gets information fields from VM boot process informed by VMM and fills the	*
 * VM process descriptors for them						* 
 * Emulates MINIX kernel/main.c						*
 *===========================================================================*/
PRIVATE int mh_main(vmm_desc_t *vmm_ptr, struct proc *lp)
{
	int i, j, vmid, ret, load_ep,nbprocs, p, imgindex;
	struct proc *rp, *rp0;
	struct boot_image *bip;
	struct vm_struct  *vm_ptr;
    struct priv *sp;	/* privilege structure pointer */

	vmid	= vmm_ptr->vmm_id;
	vm_ptr 	= &vm_hyper[vmid];

    nbprocs 	= vmm_ptr->vmm_img.nr_bprocs; 
	load_ep 	= lp->p_endpoint;

	for( i = 0; i < (NR_PROCS+NR_TASKS); i++) 
		vm_ptr->vm_proc[i].p_rts_flags = SLOT_FREE;
	
	/* Clear variable fields of process privileges*/
	for (sp = (&vm_hyper[vmid].vm_priv[0]), i = 0; 
		sp < (&vm_hyper[vmid].vm_priv[NR_SYS_PROCS]); 
		++sp, ++i) {
		sp->s_proc_nr = NONE;		/* initialize as free */
		sp->s_dr_bitmap = 0;
		sp->s_call2proxy = 0;
		for(j = 0; j < NR_SYS_PROCS; j++) {
			 sp->s_ntfy_cnt[j] = 0;		/* notify pending count */
		}	
  	}
	
	MHDEBUG("mh_main:  vmid=%d nbprocs=%d load_ep=%d\n", vmid, nbprocs,load_ep); 
	
	MHDEBUG("SYSTEM: [#] p_nr qtum prio endpoint -name- \n");
	for( i= 0; i < nbprocs; i++) {
		ret = setup_bproc(vmid, i, lp); 
		if(ret) {
			kprintf("mh_main: error on boot process number %d\n",i);
			clear_VM0_procs(vmid, nbprocs, lp); /* Rollback VM0 proxies and promiscous setup */
			MHERROR("mh_main: setup_bproc error",ret);
		}
		MHDEBUG("SYSTEM: %0.3d %0.4d %0.4d %0.4d %0.7d %s\n",
			i,
			vmm_ptr->vmm_img.image[i].proc_nr,
			vmm_ptr->vmm_img.image[i].quantum,
			vmm_ptr->vmm_img.image[i].priority,
			vmm_ptr->vmm_img.image[i].endpoint,
			vmm_ptr->vmm_img.image[i].proc_name);
	}
#ifdef MHYPERXXXX	
	for( j = HARDWARE+1; j < (NR_SYS_PROCS-NR_TASKS); j++) {
		rp0 = &vm_hyper[VM0].vm_proc[j+NR_TASKS];
		for( i = 0; i < NR_SYS_PROCS; i++){
			if( (MH_LOADED(vm_ptr->vm_driver[i].drv_type) == EXEC_LOADED) && 					
				(MH_TYPE(vm_ptr->vm_driver[i].drv_type) == PROMISCUOUS_PROC_TYPE)) {
				if( !strcmp(rp0->p_name, vm_ptr->vm_driver[i].drv_name)) {
					rp = &vm_hyper[vmid].vm_proc[rp0->p_nr+NR_TASKS];
					*rp = *rp0;
					rp->p_vmid = vmid;
					rp->p_misc_flags = PROMICUOUS_TASK;
					rp->p_rts_flags = NO_MAP;
					MHDEBUG("EXEC time promiscuous process %s\n", rp->p_name);
					break;
				}
			}
		}
	}
#endif /* MHYPERXXXX */

	return(OK);
}

/*===========================================================================*
 *			        fill_vmmap			     					*
 * Gets the info from the VM's imginfo_t and fill the memory areas for the VM				*
 *    ------------------										*
 *     | loader STACK  |vmmap_top.mem_len 								*
 *    ------------------	<<<  vmmap_top								*
 *     |               |  										*
 *     |  PMx free mem | Memory to be used by the PM of VMx						*
 *     |               | 										*
 *     |-------------- |vmmap_bottom.mem_len								*
 *     | boot processes| 										*
 *    ------------------	<<< vmmap_bottom							*
 *     |  boot image   |vmmap_image.mem_len								*
 *    ------------------	<<< vmmap_image								*
 *     |  loader DATA  |vmmap_start.mem_len								*
 *    ------------------	<<< vmmap_start								*
 *===========================================================================*/
PRIVATE int fill_vmmap(int vmid, message *m_ptr)
{
	vmm_desc_t *vmm_ptr;
	imginfo_t  *img_ptr;
	int		vmload_ep, vmload_p;
	struct proc *lp;		/* process pointer */
	vm_mmap_t	*vmmap_ptr;
	mem_map_t	*mp;

	MHDEBUG("SYSTEM: fill_vmmap vmid=%d\n",vmid);

	vmm_ptr =&vmm_desc(vmid);
	img_ptr =&vmm_ptr->vmm_img;
	MHDEBUG(IMGI_FORMAT, IMGI_FIELDS(img_ptr));
	
	vmload_ep = vmm_ptr->vmm_endpoint;
	if( !isokendpt(vmload_ep, &vmload_p))	/* converts into proc number*/
		MHERROR("fill_vmmap: Invalid vmload_ep. rcode=%d\n",EINVAL); 
	
	lp = proc_addr(vmload_p);
	vmmap_ptr = &vm_hyper[vmid].vm_mmap;
	
	/* Fill the START of the VM same as the Data segment of loader */
	memcpy(&vmmap_ptr->vmmap_start,&lp->p_memmap[D], sizeof(mem_map_t)); 

	/* Fill the  address of the BOOT IMAGE  */
	vmmap_ptr->vmmap_image.mem_vir  = (((vir_clicks)img_ptr->addr + CLICK_SIZE) >> CLICK_SHIFT);
	vmmap_ptr->vmmap_start.mem_len =  (phys_clicks) (vmmap_ptr->vmmap_image.mem_vir - vmmap_ptr->vmmap_start.mem_vir); 
	vmmap_ptr->vmmap_image.mem_phys =  (vmmap_ptr->vmmap_start.mem_phys + vmmap_ptr->vmmap_start.mem_len) ;  
	
	/* Fill the  BOTTON address of the VM's free area */
	vmmap_ptr->vmmap_bottom.mem_vir   =  (vmmap_ptr->vmmap_image.mem_vir +  (vir_clicks) ((img_ptr->bytes+ (2*CLICK_SIZE))>>CLICK_SHIFT)) ;
	vmmap_ptr->vmmap_image.mem_len    =  (phys_clicks) (vmmap_ptr->vmmap_bottom.mem_vir - vmmap_ptr->vmmap_image.mem_vir); 
	vmmap_ptr->vmmap_bottom.mem_phys  =  (vmmap_ptr->vmmap_image.mem_phys + vmmap_ptr->vmmap_image.mem_len); 

	/* Fill the  TOP address of the VM's free area */
	vmmap_ptr->vmmap_top.mem_vir   =  (((vir_clicks)img_ptr->stk_addr - CLICK_SIZE) >> CLICK_SHIFT) ;
	vmmap_ptr->vmmap_bottom.mem_len  =  (phys_clicks) (vmmap_ptr->vmmap_top.mem_vir - vmmap_ptr->vmmap_bottom.mem_vir); 
	vmmap_ptr->vmmap_top.mem_phys  =  (vmmap_ptr->vmmap_bottom.mem_phys + vmmap_ptr->vmmap_bottom.mem_len) ; 
	vmmap_ptr->vmmap_top.mem_len   =  1;
	img_ptr->vmsize = vmmap_ptr->vmmap_bottom.mem_len;

#if MHDBG
	mp=&vmmap_ptr->vmmap_start;
	MHDEBUG("vmmap_start:" MMAP_FORMAT, MMAP_FIELDS(mp));
	mp=&vmmap_ptr->vmmap_image;
	MHDEBUG("vmmap_image:" MMAP_FORMAT, MMAP_FIELDS(mp));
	mp=&vmmap_ptr->vmmap_bottom;
	MHDEBUG("vmmap_bottom:" MMAP_FORMAT, MMAP_FIELDS(mp));
	mp=&vmmap_ptr->vmmap_top;
	MHDEBUG("vmmap_top:" MMAP_FORMAT, MMAP_FIELDS(mp));
	MHDEBUG("SYSTEM: vmsize=%d\n",img_ptr->vmsize);
#endif /* MHDBG */

	return(OK);
		
}

/*===========================================================================*
 *			       set_loader_priv				     *
 *===========================================================================*/
PRIVATE int set_loader_priv(lp)
struct proc *lp;		/* process pointer */
{
	int rcode, i;
	struct proc *rs_ptr;
	int priv_id;
	

	rs_ptr = proc_addr(RS_PROC_NR);
	
	/* alloc a privilege structure for the loader */
	rcode=get_priv(lp, SYS_PROC);
	if ( rcode != OK) 
		MHERROR("set_loader_priv: rcode=%d\n",rcode); 
	
	priv_id = priv(lp)->s_id;		/* backup privilege id */
	*priv(lp) = *priv(rs_ptr);		/* copy from RS	 */
	priv(lp)->s_id = priv_id;		/* restore privilege id */
	priv(lp)->s_proc_nr = lp->p_nr;	/* reassociate process nr */

	for (i=0; i< BITMAP_CHUNKS(NR_SYS_PROCS); i++)	/* remove pending: */
	      priv(lp)->s_notify_pending.chunk[i] = 0;	/* - notifications */
	priv(lp)->s_int_pending = 0;			/* - interrupts */
	sigemptyset(&priv(lp)->s_sig_pending);		/* - signals */

	for (i=0; i<NR_SYS_PROCS; i++) {
	    priv(lp)->s_ntfy_cnt[i] = 0; /* itinialize notify pending count */
	}

	/* Now update the process' privileges as requested. */
	lp->p_priv->s_trap_mask = FILLED_MASK;
	for (i=0; i<BITMAP_CHUNKS(NR_SYS_PROCS); i++) {
		lp->p_priv->s_ipc_to.chunk[i] = FILLED_MASK;
	}
	unset_sys_bit(lp->p_priv->s_ipc_to, USER_PRIV_ID);

	/* All process that this process can send to must be able to reply. 
	 * Therefore, their send masks should be updated as well. 
	 */
	for (i=0; i<NR_SYS_PROCS; i++) {
	    if (get_sys_bit(lp->p_priv->s_ipc_to, i)) {
		  set_sys_bit(priv_addr(i)->s_ipc_to, priv_id(lp));
	    }
	}

	/* No I/O resources, no memory resources, no IRQs */
	priv(lp)->s_nr_io_range= 0;
	priv(lp)->s_nr_mem_range= 0;
	priv(lp)->s_nr_irq= 0;

	MHDEBUG(PROC_FORMAT, PROC_FIELDS(lp));
	MHDEBUG(PRIV_FORMAT, PRIV_FIELDS(priv(lp)));

	return(OK);
}

/*===========================================================================*
 *			       set_boot_parms				     	*
 * copy VM0 to VMx boot parameters except "memory"		*
 * and returns the boot parameters lenght					*
 *===========================================================================*/
PRIVATE int set_boot_parms(vmid)
int vmid;
{
	char *src_params, *dst_params,  *tk_ptr;
	imginfo_t  *img_ptr;
	vm_mmap_t	*vmmap_ptr;
	vmm_desc_t *vmm_ptr;
	int i, len, vm_len;
	struct proc *pm_ptr;

	vmm_ptr =&vmm_desc(vmid);
	img_ptr =&vmm_ptr->vmm_img;
	vmmap_ptr = &vm_hyper[vmid].vm_mmap;

	pm_ptr = proc_addr(PM_PROC_NR);
	vm_len = (vmmap_ptr->vmmap_top.mem_phys - pm_ptr->p_memmap[T].mem_phys);
	MHDEBUG("set_boot_parms: PM p_vmid=%d vm_len=%d\n", pm_ptr->p_vmid, vm_len);
	
	src_params = (char*) &vm_hyper[VM0].vm_boot_params;
	dst_params = (char*) &vm_hyper[vmid].vm_boot_params;
    for ( i=0 ; i < MAXTOKEN; i++) {
		tk_ptr = token[i];
		
#ifdef MHYPERXXX
		if( strcmp( tk_ptr, "rootdev") == 0) {     	
			/*copy the value */
			strcpy(dst_params,"rootdev=912"); /* /dev/ram => mayor(3) *256 + minor(0) */
		} else if( strcmp( tk_ptr, "ramimagedev") == 0) {     	
			/*copy the value */
			strcpy(dst_params,"ramimagedev=912");
		} else 
#endif /* MHYPERXXX*/
		
		if( strcmp( tk_ptr, "memory") == 0) {     	
			/*copy the value */
			strcpy(dst_params,"memory=");		
			strcat(dst_params, ul2a((pm_ptr->p_memmap[T].mem_phys << CLICK_SHIFT), 0x10)); 
			strcat(dst_params, ":");
			strcat(dst_params, ul2a((vm_len<< CLICK_SHIFT), 0x10));	
		}else{
			/*copy the token */
			strcpy(dst_params,tk_ptr);
			/* complete with "=" */
			strcat(dst_params,"=");
			strcat(dst_params,get_value(src_params, tk_ptr));
		}
		len = strlen(dst_params)+1;
		dst_params += len;
	}
	*dst_params++ = '\0';

	/* Computes the boot parameters lenght */
	dst_params = (char*) &vm_hyper[vmid].vm_boot_params;
	len = 0;
   	for ( i=0 ; i < MAXTOKEN; i++) {
		tk_ptr = token[i];
		len += (strlen(tk_ptr)+1);
		MHDEBUG("%s=%s\n",  tk_ptr, get_value(dst_params, tk_ptr));
		len += (strlen(get_value(dst_params, tk_ptr))+1);
	}
	len++; /*the end with '\0' */
	MHDEBUG("len=%d\n", len);
	return(len);
}

/*===========================================================================*
 *			       local_vcopy				     	*
 *===========================================================================*/
PRIVATE int local_vcopy(int src_ep, char src_seg, long src_addr, int dst_ep, char dst_seg, long dst_addr, int bytes)
{
	message m;
	
		/* copy the full VMM'S VM descriptor into SYSTEM VM desc */
  		m.CP_SRC_ENDPT = src_ep;	
  		m.CP_SRC_SPACE = src_seg;			
  		m.CP_SRC_ADDR  = src_addr;
  		m.CP_DST_ENDPT = dst_ep;
  		m.CP_DST_SPACE = dst_seg;
 		m.CP_DST_ADDR  = dst_addr;
 		m.CP_NR_BYTES  = bytes;
		return(do_copy(&m));			/* local call to sys_vircopy */
}

/*===========================================================================*
 * 			do_vm_load
 * Input Parameters from VMM		*
 * m.m1_i1		= VMM_LOAD;	*
 * m.m1_i2		= vmid;		*
 * m.m1_i3		= nbprocs;	*
 * m.m1_p1	 	= &vm_desc[vmid]; *
 *===========================================================================*/
PRIVATE int do_vm_load(message *m_ptr)
{
	int vmid, nbprocs, rcode, i, vmload_ep, vmload_p;
	struct vm_struct *vm_ptr;
	vmm_desc_t *vmm_ptr;
	char *ptr;
	struct proc *lp;		/* process pointer */

	vmid 	  	= m_ptr->VMM_vmid;
    nbprocs 	= m_ptr->m1_i3;
	MHDEBUG("SYSTEM: do_vm_load  vmid=%d nbprocs=%d\n", vmid, nbprocs);

	if( vmid < 0 || vmid >= NR_VMS) 
		MHERROR("SYSTEM: do_vm_load: invalid vmid. error:%d\n",EINVAL);

	vm_ptr = &vm_hyper[vmid];
	if(vm_ptr->vm_flags != VM_FREE) 
		MHERROR("SYSTEM: do_vm_load: the VM is not free:%d\n",EVMSTATUS);
			
	/* copy the full VMM'S VM descriptor into SYSTEM VM desc */
	rcode = local_vcopy(VMM_PROC_NR, D, (long) m_ptr->m1_p1, 
				SYSTEM, D, (long) &vmm_desc(vmid),sizeof(vmm_desc_t));
	if( rcode != OK) 
		MHERROR("SYSTEM: do_vmmreq: local_vcopy error:%d\n",rcode);

	/*------------- PRINT only for testing ---------------------*/
	vmm_ptr =&vmm_desc(vmid);
	MHDEBUG("SYSTEM: VM%d boot processes: ",vmid);
#if MHDBG
	for (i = 0; i < nbprocs; i++) 
		MHDEBUG("%s ",vmm_ptr->vmm_img.hdr[i].name);
	MHDEBUG("\n");
#endif
	vmload_ep = vmm_ptr->vmm_endpoint;		/* gets the loader endpoint */
	if( !isokendpt(vmload_ep, &vmload_p))	/* converts into proc number*/
		MHERROR("Invalid vm loader endpoint. rcode=%d\n",EINVAL); 
	MHDEBUG("SYSTEM: vmload_ep=%d vmload_p=%d\n",vmload_ep, vmload_p);

	lp = proc_addr(vmload_p);   		/* pointer to loader process */
	/* change the name of the loader for PM */
	memcpy(lp->p_name, "VMx",4);
	ptr	= (char *) &lp->p_name;
	ptr+=2;
	*ptr++ = vmid+'0';	/* VM name */
	*ptr   = '\0';	/* VM name */
	rcode = fill_vmmap(vmid, m_ptr);
	if( rcode != OK) 
		MHERROR("SYSTEM: fill_vmmap:%d\n",rcode);
	rcode = set_loader_priv(lp);
	if( rcode != OK) 
		MHERROR("SYSTEM: set_loader_priv:%d\n",rcode);
		
	vm_ptr->vm_start = umap_local(lp, D, (vir_bytes)vmm_ptr->vmm_img.addr, 1);			
	vm_ptr->vm_end   = umap_local(lp, D, (vir_bytes)vmm_ptr->vmm_img.stk_addr  , 1);
	MHDEBUG("SYSTEM: vm_start=%X vm_end=%d\n",vm_ptr->vm_start, vm_ptr->vm_end);

	/* Initialize VM's drivers table */
	for( i= 0; i < NR_SYS_PROCS; i++){
		if ( i  < NR_DFT_DRIVERS)
			vm_ptr->vm_driver[i] = dflt_driver[i]; 
		else{
			strcpy(vm_ptr->vm_driver[i].drv_name,"NO_DRIVER"); 
			vm_ptr->vm_driver[i].drv_type = DISABLED_PROC_TYPE;
		}
	}
	
	vm_ptr->vm_flags = VM_LOADED;
	return(OK);
}

/*===========================================================================*
 * 			do_vm_start
 * Input Parameters from VMM			*
 * m.m1_i1		= VMM_START;		*
 * m.m1_i2		= vmid;			*
 * m.m1_i3		= tokens 		*
 *===========================================================================*/
PRIVATE int do_vm_start(message *m_ptr)
{
	int vmid, vmload_ep, vmload_p, saved_vm, rcode;     
	struct vm_struct *vm_ptr;
	vmm_desc_t *vmm_ptr;
	struct proc *lp;		/* process pointer */

	vmid 	  = m_ptr->m1_i2;
	MHDEBUG("SYSTEM: do_vm_start vmid=%d running_vm=%d\n", vmid, running_vm);
	if( vmid < 0 || vmid >= NR_VMS) 
		MHERROR("SYSTEM: do_vm_start: invalid vmid. error:%d\n",EINVAL);
		
	vm_ptr 	= &vm_hyper[vmid];				/* kernel's VM descriptor pointer		*/
	vmm_ptr = &vmm_desc(vmid);				/* VMM's VM descriptor pointer		*/
	if(vm_ptr->vm_flags != VM_LOADED)
		MHERROR("SYSTEM: do_vm_start: the VM is not loaded:%d\n",EVMSTATUS);
	
	vmload_ep = vmm_ptr->vmm_endpoint;
	if( !isokendpt(vmload_ep, &vmload_p))	/* converts into proc number*/
		MHERROR("do_vm_startInvalid vm loader endpoint. rcode=%d\n",EINVAL); 
			
	/* fill the process descriptors with the	*/
	/* info loaded in the image and bootprocs 	*/
	/* tables of the VM as in kernel/main.c		*/
	vmid 	 = m_ptr->m1_i2;
	lp = proc_addr(vmload_p);
	lock(10, "do_vmmreq");
	saved_vm = running_vm;		
	running_vm = vmid;
	systask_ptr->p_vmid = vmid;
	if( (rcode = mh_start(vmid)) != OK) {
		running_vm=saved_vm;
		systask_ptr->p_vmid = saved_vm;
		unlock(10);
		MHERROR("do_vm_start: mh_start error=%d\n", rcode);
	}

	if( (rcode = mh_main(vmm_ptr, lp)) != OK) {
		running_vm = saved_vm;
		systask_ptr->p_vmid = saved_vm;
		unlock(10);
		MHERROR("do_vm_start: mh_main error=%d\n", rcode);
	}
	running_vm = saved_vm;
	systask_ptr->p_vmid = saved_vm;
	lp->p_misc_flags |= PROXY_SERVER;	
	lp->p_proxy = vmid;
	vm_ptr->vm_flags |= VM_RUNNING;
	vm_ptr->vm_time   = 0;
	vm_ptr->vm_uptime = get_uptime();
	
	vm_ptr->vm_bsize = m_ptr->m1_i3; /* token size  */
	vm_ptr->vm_tokens = 0; /* Tokens to start */
	/* Force the scheduler for tokens replanisment */
	vm_hyper[VM0].vm_tokens = MAXTOKENS;
	
	/* Actualizacion del tiempo de refresco */
/*	t_refresh += m_ptr->m1_i3; */

	VM_bitmap |= (1 << vmid);
	if( next_vm == VM0) {
		next_vm = vmid;
		switch_vm = TICKS_TO_SWITCH; 
	}

	unlock(10);

/*
	vm_hyper[VM0].vm_kinfo.vm_bitmap |= (1 << vmid);
	vm_hyper[vmid].vm_kinfo.vm_bitmap = (1 << vmid);
*/	

	MHDEBUG("SYSTEM: do_vm_start: vmid=%d is running. uptime=%ld tokens=%d\n",
		vmid, vm_ptr->vm_uptime,  m_ptr->m1_i3);

	return(OK);

}		
#endif /* MHYPER */

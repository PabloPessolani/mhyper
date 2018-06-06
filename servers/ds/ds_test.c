
#include "inc.h"	/* include master header file */
#include "../pm/mproc.h"	

#ifdef MHYPER
#include <minix/endpoint.h>
#include "../rs/manager.h"

PUBLIC struct rproc rproc[NR_SYS_PROCS];

#define NR_MEMS            8
/* #define NR_PROCS           100 */

struct memory {
  phys_clicks base;			/* start address of chunk */
  phys_clicks size;			/* size of memory chunk */
};

PRIVATE struct kinfo kinfo;		/* kernel information */ 
PRIVATE struct machine machine;		/* machine information */ 
PRIVATE char monitor_params[128*sizeof(char *)];	/* boot monitor parameters */

PRIVATE struct mproc mproc[NR_PROCS];

FORWARD _PROTOTYPE(void get_mem_chunks,(struct memory *mem_chunks));
FORWARD _PROTOTYPE(char *find_param,(char *name));

/*===========================================================================*
 *				ds_test             	                   	*
 * This function is called for every instance of DS that runs on VM!=VM0	*
 *===========================================================================*/
PUBLIC int ds_test(int ds_vmid)
{
	message m;
	int rcode, pid, i , floppy_ep ;
	struct mproc *mp;
	struct memory mem_chunks[NR_MEMS];
    struct rproc *rp;

	ds_vmid = _mhdebug("DS_TEST RUNNING ds_vmid=", ds_vmid);  

	rcode = sendas(MEM_PROC_NR, &m, DS_PROC_NR);
	rcode = recvas(MEM_PROC_NR, &m, DS_PROC_NR);

	rcode = sys_getkinfo(&kinfo);
	if( rcode) { 
      		_mhdebug("ds_test: getkinfo error", rcode);
  	}else{
  		_mhdebug("code_base",kinfo.code_base);		/* base of kernel code */
  		_mhdebug("code_size",kinfo.code_size);		
  		_mhdebug("data_base",kinfo.data_base);		/* base of kernel data */
  		_mhdebug("data_size",kinfo.data_size);
  
 		_mhdebug("kmem_base",kinfo.kmem_base);		/* kernel memory layout (/dev/kmem) */
  		_mhdebug("kmem_size",kinfo.kmem_size);
  		_mhdebug("bootdev_base",kinfo.bootdev_base);	/* boot device from boot image (/dev/boot) */
  		_mhdebug("bootdev_size",kinfo.bootdev_size);
  
  		_mhdebug("ramdev_base",kinfo.ramdev_base);	/* boot device from boot image (/dev/boot) */
  		_mhdebug("ramdev_size",kinfo.ramdev_size);
  		_mhdebug("params_base",kinfo.params_base);	/* parameters passed by boot monitor */
  		_mhdebug("params_size",kinfo.params_size);
  
  		_mhdebug("nr_procs",kinfo.nr_procs);			/* number of user processes */
  		_mhdebug("nr_tasks",kinfo.nr_tasks);			/* number of kernel tasks */
  		_mhdebug("release",kinfo.release);		/* kernel release number */
  		_mhdebug("version",kinfo.version);		/* kernel version number */		
	}

	rcode =sys_getmachine(&machine);
 	if( rcode) { 
      		_mhdebug("ds_test: getmachine error", rcode);
  	}else{
		_mhdebug("pc_at",machine.pc_at);
		_mhdebug("ps_mca",machine.ps_mca);
		_mhdebug("processor",machine.processor);
		_mhdebug("prot",machine.prot);
		_mhdebug("vdu_ega",machine.vdu_ega);
		_mhdebug("vdu_vga",machine.vdu_vga);
	}

	rcode = sys_getmonparams(monitor_params, sizeof(monitor_params));
 	if( rcode) { 
      		_mhdebug("ds_test: getmonparams error", rcode);
  	}else{
		  get_mem_chunks(mem_chunks);
	}

	pid = getpid();
  	_mhdebug("ds_test: getpid pid", pid);

	getsysinfo(PM_PROC_NR, SI_PROC_TAB, mproc);
  	for (i= 0; i<NR_PROCS; i++) {
  		mp = &mproc[i];
  		if (mp->mp_pid == 0 && i != PM_PROC_NR) continue;
	  	_mhdebug("mproc pid", mp->mp_pid);
 	}
    

	/* TEST to send a request to a VIRTUAL_TASK (tty) */
	m.m1_i1  = 1;
	m.m1_i2  = 2;
	m.m1_i3  = 3;
	_mhdebug("ds_test: sendrec tty rcode", rcode);


	/* TEST to send a request to a PROMISCUOUS_TASK (floppy) */
	m.m_type  = CANCEL;
	m.DEVICE  = 0;
	m.IO_ENDPT= DS_PROC_NR;
	floppy_ep = _ENDPOINT(1, FLOPPY_PROC_NR);
	rcode =sendrec(floppy_ep, &m);	
	_mhdebug("ds_test: sendrec floppy rcode", rcode);	

	/* TEST to send a request to a REAL_TASK (RS ) */
	m.m_type  = GETSYSINFO;
	m.m1_i1   = SI_PROC_TAB;
	m.m1_p1   = (long) rproc;
	rcode =sendrec(RS_PROC_NR, &m);	
	_mhdebug("ds_test: sendrec RS rcode", rcode);	

#ifdef ANULADO	
	if(rcode == 0){
		for (i= 0; i<NR_PROCS; i++) {
		rp = &rproc[i];
		if (! rp->r_flags & RS_IN_USE) continue;
		_mhdebug(rp->r_cmd,rp->r_proc_nr_e);
		}
	}
#endif /* ANULADO */	
	
}

/*===========================================================================*
 *				get_mem_chunks				     *
 *===========================================================================*/
PRIVATE void get_mem_chunks(mem_chunks)
struct memory *mem_chunks;			/* store mem chunks here */
{
/* Initialize the free memory list from the 'memory' boot variable.  Translate
 * the byte offsets and sizes in this list to clicks, properly truncated. Also
 * make sure that we don't exceed the maximum address space of the 286 or the
 * 8086, i.e. when running in 16-bit protected mode or real mode.
 */
  long base, size, limit;
  char *s, *end;			/* use to parse boot variable */ 
  int i, done = 0;
  struct memory *memp;


  /* Initialize everything to zero. */
  for (i = 0; i < NR_MEMS; i++) {
	memp = &mem_chunks[i];		/* next mem chunk is stored here */
	memp->base = memp->size = 0;
  }
  
  /* The available memory is determined by MINIX' boot loader as a list of 
   * (base:size)-pairs in boothead.s. The 'memory' boot variable is set in
   * in boot.s.  The format is "b0:s0,b1:s1,b2:s2", where b0:s0 is low mem,
   * b1:s1 is mem between 1M and 16M, b2:s2 is mem above 16M. Pairs b1:s1 
   * and b2:s2 are combined if the memory is adjacent. 
   */
  s = find_param("memory");		/* get memory boot variable */
  for (i = 0; i < NR_MEMS && !done; i++) {
	memp = &mem_chunks[i];		/* next mem chunk is stored here */
	base = size = 0;		/* initialize next base:size pair */
	if (*s != 0) {			/* get fresh data, unless at end */	

	    /* Read fresh base and expect colon as next char. */ 
	    base = strtoul(s, &end, 0x10);		/* get number */
	    if (end != s && *end == ':') s = ++end;	/* skip ':' */ 
	    else *s=0;			/* terminate, should not happen */

	    /* Read fresh size and expect comma or assume end. */ 
	    size = strtoul(s, &end, 0x10);		/* get number */
	    if (end != s && *end == ',') s = ++end;	/* skip ',' */
	    else done = 1;
	}

        _mhdebug("mem_chunks base",base);
        _mhdebug("mem_chunks size",size);
	base = (base + CLICK_SIZE-1) & ~(long)(CLICK_SIZE-1);
	limit &= ~(long)(CLICK_SIZE-1);
	if (limit <= base) continue;
	memp->base = base >> CLICK_SHIFT;
	memp->size = (limit - base) >> CLICK_SHIFT;

  }
}

/*===========================================================================*
 *				find_param				     *
 *===========================================================================*/
PUBLIC char *find_param(name)
const char *name;
{
  register const char *namep;
  register char *envp;
  int i;

  for (i=0 , envp = (char *) monitor_params; *envp != 0; i++) {
	  _mhdebug(envp,i);
	for (namep = name; *namep != 0 && *namep == *envp; namep++, envp++)
		;
	if (*namep == '\0' && *envp == '=') {
		_mhdebug("find_param: find memory!",i);
		_mhdebug(envp+1, i);
		return(envp + 1);
	}
	while (*envp++ != 0)
		;
  }
  _mhdebug("find_param: cant find memory",0);
  return(NULL);
}

#endif /* MHYPER */


#include "vmm.h"	/* include master header file */

#ifdef MHYPER


/*===========================================================================*
 *				vmm_test             	                   	*
 * This function is called for every instance of VMM that runs on VM!=VM0	*
 *===========================================================================*/
PUBLIC int vmm_test(int vmid)
{
	int rcode;
	message m;

	vmid = _mhdebug("VMM_TEST RUNNING vmid", vmid);  

	m.m1_i1 = 0x01;
	m.m1_i2 = 0x02;
	m.m1_i3 = 0x03;
	rcode = send(TTY_PROC_NR, &m); 
	 _mhdebug("vmm_test: send rcode", rcode ); 

	rcode = receive(TTY_PROC_NR, &m); 
	 _mhdebug("vmm_test: receive rcode", rcode ); 
	 _mhdebug("vmm_test: m.m1_i1", m.m1_i1); 
	 _mhdebug("vmm_test: m.m1_i2", m.m1_i2); 
	 _mhdebug("vmm_test: m.m1_i3", m.m1_i3); 

	while(1) receive(ANY, &m);
}


#endif /* MHYPER */

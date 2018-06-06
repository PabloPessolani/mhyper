#include "syslib.h"

/*===========================================================================*
 *                                sys_umapvm				     *
 *===========================================================================*/
PUBLIC int sys_umapvm(vmid, proc_nr, seg, vir_addr, bytes, phys_addr)
int vmid; 				/* vmid of the process to do umap for */
int proc_nr; 				/* process number to do umap for */
int seg;				/* T, D, or S segment */
vir_bytes vir_addr;			/* address in bytes with segment*/
vir_bytes bytes;			/* number of bytes to be copied */
phys_bytes *phys_addr;			/* placeholder for result */
{
    message m;
    int result;

    m.CP_SRC_ENDPT = proc_nr;
    m.CP_SRC_SPACE = seg;
    m.CP_SRC_ADDR = vir_addr;
    m.CP_NR_BYTES = bytes;
	m.CP_DST_VMID = vmid;

    result = _taskcall(SYSTASK, SYS_UMAPVM, &m);
    *phys_addr = m.CP_DST_ADDR;
    return(result);
}


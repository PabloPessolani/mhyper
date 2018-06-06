#include "syslib.h"

PUBLIC int sys_vcopyvm(src_proc, src_vmid, src_vir, 
	dst_proc, dst_vmid, dst_vir, bytes)
int src_proc;			/* source process */
int src_vmid;			/* source VMID*/
vir_bytes src_vir;		/* source virtual address */
int dst_proc;			/* destination process */
int dst_vmid;			/* destination VMID */
vir_bytes dst_vir;		/* destination virtual address */
phys_bytes bytes;		/* how many bytes */
{
/* Transfer a block of data.  The source and destination can each either be a
 * process number or SELF (to indicate own process number). 
 */

  message copy_mess;

  if (bytes == 0L) return(OK);
  copy_mess.CP_SRC_ENDPT = src_proc;
  copy_mess.CP_SRC_VMID  = (char) src_vmid;
  copy_mess.CP_SRC_ADDR  = (long) src_vir;
  copy_mess.CP_DST_ENDPT = dst_proc;
  copy_mess.CP_DST_VMID  = (char) dst_vmid;
  copy_mess.CP_DST_ADDR  = (long) dst_vir;
  copy_mess.CP_NR_BYTES  = (long) bytes;
  return(_taskcall(SYSTASK, SYS_VCOPYVM, &copy_mess));
}

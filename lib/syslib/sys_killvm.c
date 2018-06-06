#include "syslib.h"

PUBLIC int sys_killvm(proc, signr, vmid)
int proc;			/* which proc has exited */
int signr;			/* signal number: 1 - 16 */
int vmid;
{
/* A proc has to be signaled via MM.  Tell the kernel. */
  message m;

  m.SIG_ENDPT = proc;
  m.SIG_NUMBER = signr;
  m.SIG_VMID = vmid;
  return(_taskcall(SYSTASK, SYS_KILLVM, &m));
}


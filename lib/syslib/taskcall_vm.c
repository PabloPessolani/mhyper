
#include <lib.h>
#include <minix/syslib.h>

PUBLIC int _taskcall_vm(who, syscallnr, msgptr, vmid)
int who;
int syscallnr;
register message *msgptr;
int vmid;
{
  int status;

  msgptr->m_type = syscallnr;
  status = _sendrecvm(who, msgptr, vmid);
  if (status != 0) return(status);
  return(msgptr->m_type);
}

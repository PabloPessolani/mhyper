#include <lib.h>

PUBLIC int _syscall_vm(who, syscallnr, msgptr, vmid)
int who;
int syscallnr;
register message *msgptr;
int vmid;
{
  int status;

  msgptr->m_type = syscallnr;
  status = _sendrecvm(who, msgptr, vmid);
  if (status != 0) {
	/* 'sendrecvm' itself failed. */
	/* XXX - strerror doesn't know all the codes */
	msgptr->m_type = status;
  }
  if (msgptr->m_type < 0) {
	errno = -msgptr->m_type;
	return(-1);
  }
  return(msgptr->m_type);
}

#include <lib.h>
#define getsysinfo_vm	_getsysinfo_vm
#include <unistd.h>


PUBLIC int getsysinfo_vm(who, what, where, vmid)
int who;			/* from whom to request info */
int what;			/* what information is requested */
void *where;			/* where to put it */
int vmid;			
{
  message m;
  m.m1_i1 = what;
  m.m1_p1 = where;
  if (_syscall_vm(who, GETSYSINFO, &m, vmid) < 0) return(-1);
  return(0);
}


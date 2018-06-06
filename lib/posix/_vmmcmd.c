#include <lib.h>
#define vmmcmd	_vmmcmd
#include <unistd.h>

PUBLIC int vmmcmd(int cmd, int i2, int i3, void *p1, void *p2, void *p3)
{
  message m;

  m.VMM_CMD = cmd;
  m.VMM_I2  = i2;
  m.VMM_I3  = i3;
  m.VMM_P1  = p1;
  m.VMM_P2  = p2;
  m.VMM_P3  = p3;
  
  return(_syscall(VMM, VMMCMD, &m));
  
}

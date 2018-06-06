#include <lib.h>
#define getsep	_getsep
#include <unistd.h>

PUBLIC int getsep(int key)
{
  message m;

  m.DS_KEY = key;
  return(_syscall(MM, GETSEP, &m));
  
}

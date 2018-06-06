#include <lib.h>
#define getvmid	_getvmid
#include <unistd.h>

PUBLIC pid_t getvmid()
{
  message m;

  return(_syscall(MM, GETVMID, &m));
}

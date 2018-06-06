#include "syslib.h"

#ifdef MHYPER
/*===========================================================================*
 *				sys_getvmid				     *
 *===========================================================================*/
PUBLIC int sys_getvmid(void)
{  
  message m;

  return(_taskcall(SYSTASK, SYS_GETVMID, &m));
}

#endif /*  MHYPER */





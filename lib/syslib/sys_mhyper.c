#include "syslib.h"

/*===========================================================================*
 *                                sys_mhyper				     *
 *===========================================================================*/
PUBLIC int sys_mhyper()
{
	struct kinfo ki;
	sys_getinfo(GET_KINFO, &ki, 0,0,0);
	if(ki.version[3] == 'M' && ki.version[4] == 'H') /* search for "MH" at the end of version */ 
		return(TRUE);
	return(FALSE);
}



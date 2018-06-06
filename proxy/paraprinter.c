
#define _POSIX_SOURCE	1
#define _MINIX		1

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <lib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <alloca.h>
#include <a.out.h>
#include <timers.h>

#include <minix/config.h>
#include <minix/ipc.h>
#include <minix/syslib.h>
#include <minix/com.h>
#include <minix/const.h>
#include <minix/type.h>
#include <minix/syslib.h>
#include <minix/sysutil.h>

#define LOADVMIMG 		1
#include <minix/vmachine.h>
#include <minix/endpoint.h>

#include "../boot/rawfs.h"

#include "../kernel/config.h"
#include "../servers/vmm/const.h"

#define running_vm 		px_vmid

#include "../kernel/const.h"
#include "../kernel/config.h"
#include "../kernel/type.h"
#include "../kernel/proc.h"
#include "../kernel/ipc.h"

#include "../servers/vmm/mhmacros.h"

#include "libproxy.h"
#include "glo.h"

#define  paraprinter_init 		init_2		/* (SYSTASK+NR_TASKS)= -2+4*/
#define  paraprinter_driver 	proxy_2		/* (SYSTASK+NR_TASKS)= -2+4*/


/*---------------------------------------------------------------------------------------*/
/*					paraprinter_init					*/
/*---------------------------------------------------------------------------------------*/
int paraprinter_init(void)
{
	_mhdebug("paraprinter_init", running_vm);
	return(OK);
}

/*---------------------------------------------------------------------------------------*/
/*					paraprinter_driver					*/
/*---------------------------------------------------------------------------------------*/
int paraprinter_driver(int driver_nr)
{
	message m_in, m_out;
	message *mi_ptr, *mo_ptr;
	int rcode;
	
	mi_ptr = &m_in;
	mo_ptr = &m_out;

	_mhdebug("paraprinter_driver", driver_nr);

	rcode = recvas(ANY, mi_ptr, driver_nr);
	_mhdebug("paraprinter_driver: recvas", rcode);
	if( rcode) {
		return(rcode);
	}

	fprintf(fp_log,"paraprinter_driver:" MSG1_FORMAT, MSG1_FIELDS(mi_ptr));
	
	return(OK);
}

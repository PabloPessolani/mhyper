/* **********************************************************************/
/* !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! WARNING !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
/* By privileges reasons it must be executed:							*/
/*  service up /usr/src/test/test_ds_retrieve.c 						*/
/* **********************************************************************/

#include <ansi.h>
#include <sys/types.h>
#include <limits.h>
#include <errno.h>

#include <minix/callnr.h>
#include <minix/config.h>
#include <minix/type.h>
#include <minix/const.h>
#include <minix/com.h>
#include <minix/syslib.h>
#include <minix/sysutil.h>
#include <minix/keymap.h>
#include <minix/bitmap.h>
#include <minix/minlib.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include <minix/vmconfig.h>

#include "../servers/ds/store.h"


int main(int argc, char *argv [])
{
	int rcode, key, flags;
    long val1, val2;
static message m;
	message *m_ptr;
static vm_cfg_t vm_cfg;
	vm_cfg_t *c_ptr;
	
	m_ptr = &m;
	c_ptr = &vm_cfg;
	
	printf("%s: Retrieving from DS.\n", argv[0]);
	
	key  = 0x1234;
	
	flags= (DS_IN_USE | DS_PUBLIC | DS_VMM_CFG);
	
	if (rcode = ds_retrieve(key, &flags, &val1, &val2, (char *) c_ptr)) {
      	printf("Couldn't Retrieve at DS.: rcode=%d errno=%d\n",rcode, errno);
		exit(1);	
	}
	
	if( flags & (DS_IN_USE | DS_PUBLIC | DS_VMM_CFG)) {
		printf(VMCFG_FORMAT, VMCFG_FIELDS(c_ptr));
	}

	
	exit(0);
}


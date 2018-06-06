/* **********************************************************************/
/* !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! WARNING !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
/* By privileges reasons it must be executed:							*/
/*  service up /usr/src/test/test_ds_pusblish.c 						*/
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
	
	printf("%s: Publishing at DS.\n", argv[0]);
	key  = 0x1234;
	flags= (DS_IN_USE | DS_PUBLIC | DS_VMM_CFG);
	

	c_ptr->v_vmid = 0; /* This value could be changed by some application */
	c_ptr->v_endpoint = 0; /* Proxy endpoint This value could be changed by some application */

	c_ptr->v_size = 64;
	val1 = c_ptr->v_size;

	c_ptr->v_tokens = 32; 
	val2 = c_ptr->v_tokens;

	strcpy(c_ptr->v_name, "VM-Minix");			/* name of the VM including \0 					*/
	strcpy(c_ptr->v_bootprog, "/usr/src/test/loadvmimg"); /* pathname of the boot program with the proxy embedded 	*/
	strcpy(c_ptr->v_bootimage, "/boot/image/3.1.2H-MHYPERr1875"); /* pathname of the boot image that will be loaded by boot_prog 	*/
	c_ptr->v_proc_bm = 0xFFFFFFFF;			/* bitmap of process that will be started on boot */
	
	if (rcode = ds_publish(key, flags, val1, val2, (char *) c_ptr)) {
      	printf("Couldn't Publish at DS.: rcode=%d errno=%d\n",rcode, errno);
		exit(1);	
	}
	
	exit(0);
}


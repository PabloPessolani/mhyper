/* This file contains the table used to map system call numbers onto the
 * routines that perform them.
 */

#define _TABLE
#include "vmm.h"

#ifdef  MHYPER 
#include <minix/keymap.h>
#include <minix/callnr.h>
#include <minix/com.h>
#include <minix/endpoint.h>
#include <minix/ipc.h>
#include <minix/syslib.h>
#include <minix/com.h>
#include <string.h>
#include <sys/resource.h>
#include <limits.h>
#include <signal.h>

#include "const.h"
#include "param.h"
#include "glo.h"

_PROTOTYPE (int (*call_vec[VMM_NCALLS]), (void) ) = {
	do_vm_info,		/*  0 = vm_info			*/
	do_vm_load,		/*  1 = vm_load			*/
	do_vm_start,	/*  2 = vm_start			*/
	do_vm_stop,		/*  3 = vm_stop			*/
	do_vm_kill, 	/*  4 = vm_kill			*/
	do_vm_suspend,	/*  5 = vm_suspend		*/
	do_vm_resume,	/*  6 = vm_resume		*/
	do_vm_save,		/*  7 = vm_save			*/
	do_vm_restore,	/*  8 = vm_restore		*/
	do_vm_tabdmp,	/*  9 = vm_tabdmp		*/
	do_vm_vmdmp,	/* 10 = vm_vmdmp		*/
	do_vm_waitvm,	/* 11 = vm_waitvm		*/	
	do_vm_proc,		/* 12 = vm_proc 			*/	
};
/* This should not fail with "array size is negative": */
extern int dummy[sizeof(call_vec) == VMM_NCALLS * sizeof(call_vec[0]) ? 1 : -1];

#endif /*  MHYPER */


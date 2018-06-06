/* Function prototypes. */

#include <timers.h>

/* main.c */
_PROTOTYPE( int main, (void)						);

_PROTOTYPE( int do_vm_info, (void)						);
_PROTOTYPE( int do_vm_load, (void)						);
_PROTOTYPE( int do_vm_start, (void)						);
_PROTOTYPE( int do_vm_stop, (void)						);
_PROTOTYPE( int do_vm_kill, (void)						);
_PROTOTYPE( int do_vm_suspend, (void)					);
_PROTOTYPE( int do_vm_resume, (void)					);
_PROTOTYPE( int do_vm_save, (void)						);
_PROTOTYPE( int do_vm_restore, (void)					);
_PROTOTYPE( int do_vm_vmdmp, (void)					);
_PROTOTYPE( int do_vm_tabdmp, (void)					);
_PROTOTYPE( int do_vm_waitvm, (void)					);
_PROTOTYPE( int do_vm_proc, (void)					);

/* utility.c */
_PROTOTYPE( int vmm_isokendpt, 	(int ep, int *proc));
_PROTOTYPE( void vmm_tell_fs, 	(int what, int p1, int p2, int p3));
_PROTOTYPE( int vmm_allowed, 	(char *name_buf, struct stat *s_buf, int mask)	);
_PROTOTYPE( ssize_t vmm_read, 	(int fd, void *buffer,size_t nbytes));

_PROTOTYPE( int vmm_test, (int vmid));

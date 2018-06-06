/* This file contains some utility routines for VMM.
 *
 * The entry points are:

 */
#include "vmm.h"
#ifdef   MHYPER 
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

#include "const.h"
#include "param.h"



/*===========================================================================*
 *				vmm_isokendpt			 	     *
 *===========================================================================*/
PUBLIC int vmm_isokendpt(int endpoint, int *proc)
{
	return OK;
}

/*===========================================================================*
 *				vmm_tell_fs					     *
 *===========================================================================*/
PUBLIC void vmm_tell_fs(what, p1, p2, p3)
int what, p1, p2, p3;
{
/* This routine is only used by VMM to inform FS of certain events:
 *      tell_fs(CHDIR, slot, dir, 0)
 *      tell_fs(EXEC, proc, 0, 0)
 *      tell_fs(EXIT, proc, 0, 0)
 *      tell_fs(FORK, parent, child, pid)
 *      tell_fs(SETGID, proc, realgid, effgid)
 *      tell_fs(SETSID, proc, 0, 0)
 *      tell_fs(SETUID, proc, realuid, effuid)
 *      tell_fs(UNPAUSE, proc, signr, 0)
 *      tell_fs(STIME, time, 0, 0)
 * Ignore this call if the FS is already dead, e.g. on shutdown.
 */
  message m;

  m.VMM_fs_arg1 = p1;
  m.VMM_fs_arg2 = p2;
  m.VMM_fs_arg3 = p3;
  _taskcall(FS_PROC_NR, what, &m);
}

/*===========================================================================*
 *				vmm_allowed					     *
 *===========================================================================*/
PUBLIC int vmm_allowed(name_buf, s_buf, mask)
char *name_buf;			/* pointer to image file name */
struct stat *s_buf;		/* buffer for doing and returning stat struct*/
int mask;			    /* R_BIT, W_BIT, or X_BIT */
{
/* Check to see if file can be accessed.  Return EACCES or ENOENT if the access
 * is prohibited.  If it is legal open the file and return a file descriptor.
 */
  int fd;
  int save_errno;

  /* Use the fact that mask for access() is the same as the permissions mask.
   * E.g., X_BIT in <minix/const.h> is the same as X_OK in <unistd.h> and
   * S_IXOTH in <sys/stat.h>.  vmm_tell_fs(CHDIR, ...) has set VMM's real ids
   * to the user's effective ids, so access() works right for setuid programs.
   */
  if (access(name_buf, mask) < 0) return(-errno);

  /* The file is accessible but might not be readable.  Make it readable. */
  vmm_tell_fs(SETUID, VMM_PROC_NR, (int) SUPER_USER, (int) SUPER_USER);

  /* Open the file and fstat it.  Restore the ids early to handle errors. */
  fd = open(name_buf, O_RDONLY | O_NONBLOCK);
  save_errno = errno;		/* open might fail, e.g. from ENFILE */
  vmm_tell_fs(SETUID, VMM_PROC_NR, (int) 0, (int) 0);
  if (fd < 0) return(-save_errno);
  if (fstat(fd, s_buf) < 0) {
	printf("VMM: vmm_allowed: fstat failed", NO_NUM);
    return(EGENERIC);
	}
	
  /* Only regular files can be executed. */
  if (mask == X_BIT && (s_buf->st_mode & I_TYPE) != I_REGULAR) {
	close(fd);
	return(EACCES);
  }
  return(fd);
}

/*===========================================================================*
 *				vmm_read				     *
 *===========================================================================*/
PUBLIC ssize_t vmm_read(fd, buffer, nbytes)
int fd;
void *buffer;
size_t nbytes;
{
  message m;
  m.m1_i1 = _PM_SEG_FLAG | fd;
  m.m1_i2 = nbytes;
  m.m1_p1 = (char *) buffer;
  return(_syscall(FS_PROC_NR, READ, &m));
}

#endif /*  MHYPER */



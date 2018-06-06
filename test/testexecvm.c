/* test vm load  */

#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <lib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <minix/ipc.h>
#include <minix/syslib.h>
#include <minix/com.h>


#define OK	0

_PROTOTYPE(int main, (int argc, char *argv []));

extern char *const *environ;

#define VMSIZEMB  64

int main(argc, argv)
int argc;
char *argv[];
{
  int rcode, vmsize;
  int sts;
  pid_t pid;
  char *const parmList[] = {"/usr/src/test/loadvmimg", "/boot/image/3.1.2a", NULL};
  char *const envParms[] = {NULL};
  char prog[] = "loadvmimg";
  
  vmsize = VMSIZEMB*1024*1024/CLICK_SIZE;
  printf("Test execvm: vmsize= %d Mb = %d clicks \n", VMSIZEMB, vmsize);
  
  /* send command to VMM */
  if( pid = fork() )
	{
	printf("FATHER: my son PID is %d\n",pid);
	wait(&sts);
	}
  else
	{
	printf("CHILD: execute VM vmsize %d filename %s argv %s %s\n",
		vmsize, parmList[0], parmList[0], parmList[1] );
	rcode = execvm(vmsize, parmList[0], parmList, envParms);
	printf("Child rcode %d\n", rcode);
    }
}


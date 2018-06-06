/* test execve */

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
 
int main(argc, argv)
int argc;
char *argv[];
{
  int rcode, vmsize;
  int pid, sts;
  char *const parmList[] = {"/usr/bin/uname", "-a", NULL};
  char *const envParms[] = {"MYVAR=mhyper", NULL};
  char prog[] = "uname";
  
  printf("Test execve \n");
  
  if( pid = fork() )
	{
	printf("FATHER: my son PID is %d\n",pid);
	wait(&sts);
	}
  else
	{
	printf("Child: execute filename %s argv %s %s\n",
		parmList[0], parmList[0], parmList[1] );
	rcode = execve( parmList[0], parmList, envParms);
	printf("Child rcode %d\n", rcode);
    }
}


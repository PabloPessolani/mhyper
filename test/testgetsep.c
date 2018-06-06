/* test getvmid */

#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <minix/ipc.h>
#include <minix/syslib.h>
#include <minix/com.h>


#define OK	0

_PROTOTYPE(int main, (int argc, char *argv []));

int main(argc, argv)
int argc;
char *argv[];
{
  message m;
  int rcode, sep;

    printf("Test Get Server EndPoint (getsep)\n");

  sep = getsep(1234);
  
  printf("getsep()= %d\n", sep);
  if( sep != 0)
	printf("errno = %d\n", errno);
  

}


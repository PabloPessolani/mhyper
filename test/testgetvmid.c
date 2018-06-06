/* test getvmid */

#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>



_PROTOTYPE(int main, (int argc, char *argv []));

int main(argc, argv)
int argc;
char *argv[];
{
  int vmid;
  
  printf("Test getvmid\n");
 
  vmid = getvmid();
  
  printf("getvmid()= %d\n", vmid);
  if( vmid != 0)
	printf("errno = %d\n", errno);
}


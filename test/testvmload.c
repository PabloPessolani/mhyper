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

int main(argc, argv)
int argc;
char *argv[];
{
  message m;
  int rcode;
  int cmd, i2, i3;
  void *p1;
  char image_name[] = "/boot/image_small";
  
  printf("Test VM load \n");

  cmd = VMM_LOAD;
  i2 = 1024;
  p1 = &image_name;
  i3 = strlen(image_name);
  
  printf("VMM_LOAD [%d] [%d] [%s in addr %X] \n",i2,i3,p1,p1);
  
  /* send command to VMM */
  rcode = vmmcmd(cmd, i2, i3, p1);
 
  if( rcode < 0)
	printf("TESTVMLOAD: errno = %d\n",errno);
  
  printf("TESTVMLOAD: rcode = %d\n", rcode);
}


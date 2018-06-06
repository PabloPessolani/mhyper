/* test vm info  */

#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <lib.h>
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
  int rcode;
  int cmd, i2, i3;
  void *p1, *p2, *p3;
  
  printf("Test VM info \n");

  cmd = VMM_INFO;
  i2 = 1;
  i3 = 2;
  p1 = (void *) 0x1111;
  p2 = (void *) 0x2222;
  p3 = (void *) 0x3333;
 
  /* send command to VMM */
  rcode = vmmcmd(cmd, i2, i3, p1, p2, p3);
 
  if( rcode < 0)
	printf("TESTVMINFO: errno = %d\n",errno);
  
  printf("TESTVMINFO: rcode = %d\n", rcode);
}


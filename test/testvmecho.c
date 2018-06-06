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
  void *p1;
  
  printf("Test VM info \n");

  cmd = VMM_INFO;
  i2 = 0x2222;
  i3 = 0x3333;
  p1 = (void *) 0x1111;
    
  /* send command to VMM */
  rcode = vmmcmd(cmd, i2, i3, p1);
 
  if( rcode < 0)
	printf("TESTVMINFO: errno = %d\n",errno);
  
  printf("TESTVMINFO: rcode = %d\n", rcode);
}


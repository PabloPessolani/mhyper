#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <lib.h>

_PROTOTYPE(int main, (void));

int main()
{
	int *ptr;
	
  _mhdebug("01234567890012345678900123456789001234567890", 1234);
   ptr = 0xFFFFFFFF;
   *ptr = 1;
  printf("esto no deberia aparecer\n");
  exit(0);
}
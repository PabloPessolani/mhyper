/* test SM */

#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#define	MHYPER 1
#include <minix/minlib.h>
#include <minix/ipc.h>
#include <minix/syslib.h>
#include <minix/com.h>
#include <minix/callnr.h>


void prtusage(char *p_name)
{
	int i;
	printf("Usage: %s <SM_endpoint> \n",  p_name);
	exit(1);
}

int main(int argc, char *argv [])
{
	message  m, *m_ptr;
	int sm_ep, status;
	
	if( argc < 2)
		prtusage(argv[0]);

	sm_ep = atoi(argv[1]);
	m_ptr = &m;
	m_ptr->m_type = GETPID;
	printf(MSG1_FORMAT, MSG1_FIELDS(m_ptr)); 

	status = _sendrec(sm_ep, m_ptr);
	printf("status=%d\n",  status);
	printf(MSG1_FORMAT, MSG1_FIELDS(m_ptr)); 

	exit(0);
}


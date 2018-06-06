#include <lib.h>
#include <unistd.h>


PUBLIC int ds_retrieve(key, flags, val1, val2, ptr)
int key;
int *flags;			
long *val1;
long *val2;
char *ptr;
{
	message m;
	int rcode; 

	m.DS_KEY = key;
	m.DS_ENDPOINT = SELF;
	m.DS_PTR = ptr;		  
	
    rcode = _syscall(DS_PROC_NR, DS_RETRIEVE, &m);
	if( rcode) return (rcode);
	
	*val1 = m.DS_VAL_L1;	  
	*val2 = m.DS_VAL_L2;	  
  	*flags = m.DS_FLAGS;

    return(rcode);
}


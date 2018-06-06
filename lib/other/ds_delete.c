#include <lib.h>
#include <unistd.h>


PUBLIC int ds_delete(key)
int key;
{
	message m;
	int rcode; 

	m.DS_KEY = key;
	m.DS_ENDPOINT = SELF;
	
    rcode = _syscall(DS_PROC_NR, DS_DELETE, &m);
    return(rcode);
}


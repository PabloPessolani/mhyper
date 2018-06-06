#include "../drivers.h"
#include "../libdriver/driver.h"
#include "../libdriver/drvlib.h"

_PROTOTYPE(void main, (void));

#ifdef MHYPER
#ifdef receive
#undef receive 
#define receive(src, msg)    recvvm(src, msg, ANY_VM)
#endif
#endif /* MHYPER */


#ifdef MHYPERxxxxx
#define receive(src, msg)     do {\
	_mhdebug("recvvm: line",__LINE__);\
	_mhdebug("recvvm: vmid",recvvm(src, msg, ANY_VM));\
	} while(0);
#endif /* MHYPER */

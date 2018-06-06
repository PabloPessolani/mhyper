
#define _POSIX_SOURCE	1
#define _MINIX		1

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <lib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <alloca.h>
#include <a.out.h>
#include <timers.h>

#include <minix/config.h>
#include <minix/ipc.h>
#include <minix/syslib.h>
#include <minix/com.h>
#include <minix/const.h>
#include <minix/type.h>
#include <minix/syslib.h>
#include <minix/sysutil.h>

#define LOADVMIMG 		1
#include <minix/vmachine.h>
#include <minix/endpoint.h>

#include "../boot/rawfs.h"

#include "../kernel/config.h"
#include "../servers/vmm/const.h"

#define running_vm 		px_vmid

#include "../kernel/const.h"
#include "../kernel/config.h"
#include "../kernel/type.h"
#include "../kernel/proc.h"
#include "../kernel/ipc.h"

#include "../servers/vmm/mhmacros.h"

#include "libproxy.h"
#include "glo.h"

#define  floppy_init 	init_17		/* (floppy_nr+NR_TASKS)= 13+4*/
#define  floppy_driver 	proxy_17	/* (floppy_nr+NR_TASKS)= 13+4*/

FILE *fp_floppy;
phys_bytes floppy_phys;
int floppy_nr, floppy_ep;

/*---------------------------------------------------------------------------------------*/
/*					floppy_init					*/
/*---------------------------------------------------------------------------------------*/
int floppy_init(void)
{
	int rcode, i;
  
	_mhdebug("floppy_init: SECTOR_SIZE", SECTOR_SIZE);

	fp_floppy = fopen("mhyper.flp","r+");
	if( fp_floppy == NULL )
		_mhdebug("fprintf: fopen errno", errno );
		

	/* get the physical address of the SELF data segment */
	rcode = sys_umap(SELF, D, (vir_bytes) 0x0000, 1, &floppy_phys);
	_mhdebug("floppy_driver: sys_umap rcode", rcode);
	fprintf(fp_log,"floppy_phys=%lX\n", floppy_phys);

#ifdef ANULADO
	for( i= 0; i < NR_PROCS; i++)		
		r2l[i].diff = NOT_INITIALIZED;
		
	/*dump the SELF process descriptor to cache  */
	floppy_ep = getprocnr();
	_mhdebug("floppy_driver: endpoint", floppy_ep);
	floppy_nr = _ENDPOINT_P(floppy_ep);
	floppy_ptr = &proc[floppy_nr+NR_TASKS]; 
	rcode = sys_getproc(floppy_ptr, SELF);
	if( rcode ) {
		_mhdebug("floppy_driver: sys_getproc rcode", rcode);
	}else{
		fprintf(fp_log,PROC_FORMAT, PROC_FIELDS(floppy_ptr));
	}
	
	register struct proc *rp;
	static struct proc *oldrp;
  
	rp = BEG_PROC_ADDR;
	oldrp = BEG_PROC_ADDR;
  /* First obtain a fresh copy of the current process table. */
	if ((rcode = sys_getproctab(rp)) != OK) {
		fprintf(fp_log,"floppy_driver:warning: couldn't get copy of process table:", rcode);
        return(rcode);
	}
#endif /* ANULADO */
	
	return(OK);
}

/*---------------------------------------------------------------------------------------*/
/*					floppy_driver					*/
/*---------------------------------------------------------------------------------------*/
int floppy_driver(int driver_nr)
{
	int i, val, rcode, bytes, count, offset, rqtr_nr;
	int fl_rqst, fl_ep, fl_dev, fl_bytes, fl_nr_req;
	long fl_offset;
	message m_in, m_out;
	message *mi_ptr, *mo_ptr;
	char *fl_address, *rmt_virt; 
	iovec_t *iop, *iov_end;
	struct proc *rqtr_ptr;

static iovec_t iovec[NR_IOREQS];
static  prtbuf[256];
static	devbuf[SECTOR_SIZE*2];

	_mhdebug("floppy_driver: driver_nr", driver_nr);
	
	/* set pointer to message IN and message out */
	mi_ptr = &m_in;
	mo_ptr = &m_out;
	m_ptr = &m;

	rcode = recvas(ANY, mi_ptr, driver_nr);
	_mhdebug("floppy_driver: recvas rcode", rcode);
	if( rcode) {
		return(rcode);
	}

	fprintf(fp_log,"floppy_driver:" MSG1_FORMAT, MSG1_FIELDS(mi_ptr));

	
	if(mi_ptr->m_type < DEV_RQ_BASE) {
		_mhdebug("floppy_driver: bad request", mi_ptr->m_type);
		return(EINVAL);
	}
	
	fl_rqst = mi_ptr->m_type;
	switch(fl_rqst) {
		case DEV_OPEN:   
			fl_ep = mi_ptr->IO_ENDPT;
			fl_dev = mi_ptr->DEVICE;
			_mhdebug("floppy_driver: DEV_OPEN", fl_dev);
			fprintf(fp_log,"floppy_driver: DEV_OPEN: fl_dev=%d fl_ep=%d\n",
				fl_dev,fl_ep);
			rcode = OK;
			break;			
		case DEV_READ:	
			fl_ep = mi_ptr->IO_ENDPT;
			fl_dev = mi_ptr->DEVICE;
			fl_bytes= mi_ptr->COUNT;
			fl_offset = mi_ptr->POSITION;
			fl_address = mi_ptr->ADDRESS;
			_mhdebug("floppy_driver: DEV_READ", fl_dev);
			_mhdebug("floppy_driver: fl_ep", fl_ep);
			
			/* check if the local process descriptor cache is updated  */
			rqtr_nr = _ENDPOINT_P(fl_ep);
			_mhdebug("floppy_driver: rqtr_nr", rqtr_nr);
			rqtr_ptr = &proc[rqtr_nr + NR_PROCS]; 
			if( rqtr_ptr->p_endpoint != fl_ep) {
				/* process endpoint does not match - get the new descriptor */
				rcode = sys_getproc(rqtr_ptr, fl_ep);
				if( rcode ) {
					_mhdebug("floppy_driver: sys_getproc rcode", rcode);
				}
			}

			fprintf(fp_log,PROC_FORMAT, PROC_FIELDS(rqtr_ptr));

			rmt_virt = (fl_address + abs((rqtr_ptr->p_memmap[D].mem_phys << CLICK_SHIFT) - floppy_phys));
			_mhdebug("floppy_driver: fl_address", (int) fl_address);
			_mhdebug("floppy_driver: rmt_phys", (rqtr_ptr->p_memmap[D].mem_phys << CLICK_SHIFT));
			_mhdebug("floppy_driver: rmt_virt", (int) rmt_virt);
			fprintf(fp_log,"fl_address=%X rmt_phys=%X rmt_virt=%X \n",
				fl_address,
				(rqtr_ptr->p_memmap[D].mem_phys << CLICK_SHIFT),
				rmt_virt);

#ifdef ANULADO			
	
			if (r2l[fl_ep].diff == NOT_INITIALIZED) 
				get_refdiff(fl_ep, fl_address, fl_bytes);
				
			rmt_virt = rvirt_lvirt(fl_address, fl_ep);
			fprintf(fp_log,"floppy_driver: DEV_READ: rmt_virt=%X\n",rmt_virt);
			
			_mhdebug("floppy_driver: DEV_READ", fl_dev);
			fprintf(fp_log,"floppy_driver: DEV_READ: fl_dev=%d fl_ep=%d fl_offset=%d fl_bytes=%d\n",
					fl_dev, fl_ep, fl_offset,  fl_bytes);
			
			rcode = fseek(fp_floppy, fl_offset, SEEK_SET); 
			_mhdebug("floppy_driver: fseek", rcode);
			
			rcode = fread(devbuf, 1, fl_bytes, fp_floppy);
			_mhdebug("floppy_driver: fread", rcode);

			rcode = sys_vcopyvm(SELF, VM0, devbuf, fl_ep, px_vmid, fl_address, fl_bytes);
			_mhdebug("floppy_driver: sys_vcopyvm", rcode);		
			memcpy(rmt_virt, devbuf, fl_bytes); 

#else  /* ANULADO */

			rcode = fseek(fp_floppy, fl_offset, SEEK_SET); 
			_mhdebug("floppy_driver: fseek", rcode);
			
			rcode = fread(rmt_virt, 1, fl_bytes, fp_floppy); /* DIRECT READ to REQUESTER BUFFER !! */
			_mhdebug("floppy_driver: fread", rcode);		
#endif /* ANULADO */			

			rcode = fl_bytes;
			break;
		case DEV_CLOSE:  
			fl_ep = mi_ptr->IO_ENDPT;
			fl_dev = mi_ptr->DEVICE;
			_mhdebug("floppy_driver: DEV_CLOSE", fl_dev);
			fprintf(fp_log,"floppy_driver: DEV_CLOSE: fl_dev=%d fl_ep=%d\n",
				fl_dev,fl_ep);
			rcode = OK;
			break;
		case DEV_GATHER:
				fl_ep = mi_ptr->IO_ENDPT;
				fl_dev = mi_ptr->DEVICE;
				fl_offset = mi_ptr->POSITION;
				fl_nr_req = mi_ptr->COUNT;	/* Length of I/O vector */
			    if (fl_nr_req > NR_IOREQS) fl_nr_req = NR_IOREQS;
				_mhdebug("floppy_driver: DEV_GATHER", fl_nr_req);
				fprintf(fp_log,"floppy_driver: DEV_GATHER: fl_dev=%d fl_ep=%d fl_offset=%d fl_nr_req=%d\n",
					fl_dev, fl_ep, fl_offset, fl_nr_req);

				/* copy request vector from requester process address space to local address space */	
				rcode = sys_vcopyvm(mi_ptr->m_source, px_vmid, (vir_bytes) mi_ptr->ADDRESS, 
							SELF, VM0, (vir_bytes) iovec, (phys_bytes) (fl_nr_req * sizeof(iovec[0])));
				_mhdebug("floppy_driver: sys_vcopyvm", rcode);
				
				rcode = fseek(fp_floppy, fl_offset, SEEK_SET); 
				_mhdebug("floppy_driver: fseek", rcode);
					
				for( i = 0; i < fl_nr_req; i++) {
					iop = &iovec[i];			
					fl_address = (char *) iop->iov_addr;
					fl_bytes   = iop->iov_size;		
					fprintf(fp_log,"floppy_driver: iovec[%d] fl_address=%X fl_bytes=%d\n",
								i, fl_address, fl_bytes);
					count  = fl_bytes; 
					offset = 0;
					do	{
						bytes = (count > SECTOR_SIZE)?SECTOR_SIZE:count;
						
						/* read data from the virtual device file into the local buffer  */
						bytes = fread(devbuf, 1, bytes, fp_floppy);
						_mhdebug("floppy_driver: fread", bytes);

						/* copy the data from the local buffer to the requester process address space in other VM */
						rcode = sys_vcopyvm(SELF, VM0, devbuf, fl_ep, px_vmid, fl_address+offset, bytes);
						_mhdebug("floppy_driver: sys_vcopyvm", rcode);
						offset += bytes;
						count -= bytes;
					} while(count > 0);
				}

				/* copy back the request vector from local space to requester process address space in other VM */	
				rcode = sys_vcopyvm(SELF, VM0, (vir_bytes) iovec, 
										mi_ptr->m_source, px_vmid, (vir_bytes) mi_ptr->ADDRESS, 
										(phys_bytes) (fl_nr_req * sizeof(iovec[0])));
				_mhdebug("floppy_driver: sys_vcopyvm", rcode);
				
			break;
		case DEV_SCATTER:
				fl_ep = mi_ptr->IO_ENDPT;
				fl_dev = mi_ptr->DEVICE;
				fl_offset = mi_ptr->POSITION;
				fl_nr_req = mi_ptr->COUNT;	/* Length of I/O vector */
			    if (fl_nr_req > NR_IOREQS) fl_nr_req = NR_IOREQS;
				_mhdebug("floppy_driver: DEV_SCATTER", fl_nr_req);
				fprintf(fp_log,"floppy_driver: DEV_SCATTER: fl_dev=%d fl_ep=%d fl_offset=%d fl_nr_req=%d\n",
					fl_dev, fl_ep, fl_offset, fl_nr_req);
					
				/* copy request vector from requester process address space to local address space */	
				rcode = sys_vcopyvm(mi_ptr->m_source, px_vmid, (vir_bytes) mi_ptr->ADDRESS, 
							SELF, VM0, (vir_bytes) iovec, (phys_bytes) (fl_nr_req * sizeof(iovec[0])));
				_mhdebug("floppy_driver: sys_vcopyvm", rcode);
				
				rcode = fseek(fp_floppy, fl_offset, SEEK_SET); 
				_mhdebug("floppy_driver: fseek", rcode);
					
				for( i = 0; i < fl_nr_req; i++) {
					iop = &iovec[i];			
					fl_address = (char*)iop->iov_addr;
					fl_bytes   = iop->iov_size;		
					fprintf(fp_log,"floppy_driver: iovec[%d] fl_address=%X fl_bytes=%d\n",
								i, fl_address, fl_bytes);
					count  = fl_bytes; 
					offset = 0;
					do	{
						bytes = (count > SECTOR_SIZE)?SECTOR_SIZE:count;
						
						/* copy the data from the requester process address space in other VM  to the local buffer */
						rcode = sys_vcopyvm( fl_ep, px_vmid, fl_address+offset,SELF, VM0, devbuf, bytes);
						_mhdebug("floppy_driver: sys_vcopyvm", rcode);
						
						/* write data from local buffer to the  virtual device file */
						bytes = fwrite(devbuf, 1, bytes, fp_floppy);
						_mhdebug("floppy_driver: fwrite", bytes);

						offset += bytes;
						count -= bytes;
					} while(count > 0);
				}
				
				/* copy back the request vector from local space to requester process address space */	
				rcode = sys_vcopyvm(SELF, VM0, (vir_bytes) iovec, 
										mi_ptr->m_source, px_vmid, (vir_bytes) mi_ptr->ADDRESS, 
										(phys_bytes) (fl_nr_req * sizeof(iovec[0])));
				_mhdebug("floppy_driver: sys_vcopyvm", rcode);
				
			break;
		case CANCEL:     
		case DEV_WRITE:  
		case DEV_IOCTL: 
		case DEV_SELECT:	
		case DEV_STATUS: 
			_mhdebug("floppy_driver: EINVAL fl_rqst", fl_rqst);
			rcode = EINVAL;
		default:
			_mhdebug("floppy_driver: ERROR fl_rqst", fl_rqst);
			rcode = EINVAL;
			break;
	}

	mo_ptr->m_type = TASK_REPLY;
	mo_ptr->REP_ENDPT = fl_ep;
	mo_ptr->REP_STATUS = rcode;
	rcode = sendas(mi_ptr->m_source, mo_ptr, driver_nr);
	_mhdebug("floppy_driver: sendas rcode", rcode);
	if( rcode) {
		return(rcode);
	}

	return(OK);
}

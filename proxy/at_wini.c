
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
#define LOGDBG 		1
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

#define  at_wini_init 		init_20
#define  at_wini_driver 	proxy_20
#define  minor_c0d0 128

FILE *fp_disc[4];
phys_bytes at_wini_phys;
int at_wini_nr, at_wini_ep;

#ifdef MHDBG
#define _MHDEBUG(x,y)	_mhdebug(x,y);
#else
#define _MHDEBUG(x,y)
#endif	

/*---------------------------------------------------------------------------------------*/
/*					at_wini_init					*/
/*---------------------------------------------------------------------------------------*/
int at_wini_init(void)
{
	int rcode, i;
	int result, s;

	_MHDEBUG("at_wini_init: SECTOR_SIZE", SECTOR_SIZE);
	
	/* INICIALIZO EL ARRAY DE DRIVERS */
	fp_disc[0] = fopen("/dev/c0d1p0s0","r+");
	fp_disc[1] = fopen("/dev/c0d1p0s1","r+");
	fp_disc[2] = fopen("/dev/c0d1p0s2","r+");
	fp_disc[3] = fopen("/dev/c0d1p0s3","r+");

	if( fp_disc[0] == NULL )
		_MHDEBUG("fprintf: fopen errno c0d1p0s0", errno );
	if( fp_disc[1] == NULL )
		_MHDEBUG("fprintf: fopen errno c0d1p0s1", errno );
	if( fp_disc[2] == NULL )
		_MHDEBUG("fprintf: fopen errno c0d1p0s2", errno );
	if( fp_disc[3] == NULL )
		_MHDEBUG("fprintf: fopen errno c0d1p0s3", errno );	
	
	/* get the physical address of the SELF data segment */
	rcode = sys_umap(SELF, D, (vir_bytes) 0x0000, 1, &at_wini_phys);
	_MHDEBUG("at_wini_driver: sys_umap rcode", rcode);
#ifdef LOGDBG	
	fprintf(fp_log,"at_wini_phys=%lX\n", at_wini_phys); 
#endif /* LOGDBG*/

#ifdef ANULADO

	for( i= 0; i < NR_PROCS; i++)		
	r2l[i].diff = NOT_INITIALIZED;
	
	/*dump the SELF process descriptor to cache  */
	at_wini_ep = getprocnr();
	_MHDEBUG("at_wini_driver: endpoint", at_wini_ep);
	at_wini_nr = _ENDPOINT_P(at_wini_ep);
	at_wini_ptr = &proc[at_wini_nr+NR_TASKS]; 
	rcode = sys_getproc(at_wini_ptr, SELF);
	if( rcode ) {
		_MHDEBUG("at_wini_driver: sys_getproc rcode", rcode);
	}
#ifdef LOGDBG	
	else{
		fprintf(fp_log,PROC_FORMAT, PROC_FIELDS(at_wini_ptr));
	}
#endif /* LOGDBG*/
	register struct proc *rp;
	static struct proc *oldrp;

	rp = BEG_PROC_ADDR;
	oldrp = BEG_PROC_ADDR;
	/* First obtain a fresh copy of the current process table. */
	if ((rcode = sys_getproctab(rp)) != OK) {
		fprintf(fp_log,"at_wini_driver:warning: couldn't get copy of process table:", rcode);
		return(rcode);
	}
	
#endif /* ANULADO */
	
	return(OK);
}

/*---------------------------------------------------------------------------------------*/
/*					at_wini_driver					*/
/*---------------------------------------------------------------------------------------*/
int at_wini_driver(int driver_nr)
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
	static prtbuf[256];
	static devbuf[SECTOR_SIZE*2];
	
	/* posicion q se debe abrir */
	int dr_index;

	_MHDEBUG("at_wini_driver: driver_nr", driver_nr);
	
	/* set pointer to message IN and message out */
	mi_ptr = &m_in;
	mo_ptr = &m_out;
	m_ptr = &m;

	rcode = recvas(ANY, mi_ptr, driver_nr);
	_MHDEBUG("at_wini_driver: recvas rcode", rcode);
	if( rcode) {
		return(rcode);
	}

#ifdef LOGDBG	
	fprintf(fp_log,"at_wini_driver:" MSG1_FORMAT, MSG1_FIELDS(mi_ptr)); 
#endif /* LOGDBG*/

	
	if(mi_ptr->m_type < DEV_RQ_BASE) {
		_MHDEBUG("at_wini_driver: bad request", mi_ptr->m_type);
		return(EINVAL);
	}
	
	
	fl_rqst = mi_ptr->m_type;
	switch(fl_rqst) {
	case DEV_OPEN: 
		fl_ep = mi_ptr->IO_ENDPT;
		fl_dev = mi_ptr->DEVICE;
		_MHDEBUG("at_wini_driver: DEV_OPEN", fl_dev);
#ifdef LOGDBG	
		fprintf(fp_log,"at_wini_driver: DEV_OPEN: fl_dev=%d fl_ep=%d\n",
		fl_dev,fl_ep);
#endif /* LOGDBG*/	
		rcode = OK;
		break;			
	case DEV_READ:
		fl_ep = mi_ptr->IO_ENDPT;		
		fl_dev = mi_ptr->DEVICE;
		fl_bytes= mi_ptr->COUNT;
		fl_offset = mi_ptr->POSITION;
		fl_address = mi_ptr->ADDRESS;
		_MHDEBUG("at_wini_driver: DEV_READ", fl_dev);
		_MHDEBUG("at_wini_driver: fl_ep", fl_ep);
		
		/* reading disk according minor (mi_ptr->DEVICE) */
		dr_index = fl_dev - minor_c0d0; 
		_MHDEBUG("at_wini_driver: disc to read", fp_disc[dr_index]);
		
		/* check if the local process descriptor cache is updated  */
		rqtr_nr = _ENDPOINT_P(fl_ep);
		_MHDEBUG("at_wini_driver: rqtr_nr", rqtr_nr);
		rqtr_ptr = &proc[rqtr_nr + NR_PROCS]; 
		if( rqtr_ptr->p_endpoint != fl_ep) {
			/* process endpoint does not match - get the new descriptor */
			rcode = sys_getproc(rqtr_ptr, fl_ep);
			if( rcode ) {
				_MHDEBUG("at_wini_driver: sys_getproc rcode", rcode);
			}
		}

#ifdef LOGDBG	
		fprintf(fp_log,PROC_FORMAT, PROC_FIELDS(rqtr_ptr));
#endif /* LOGDBG*/
		rmt_virt = (fl_address + abs((rqtr_ptr->p_memmap[D].mem_phys << CLICK_SHIFT) - at_wini_phys));
		_MHDEBUG("at_wini_driver: fl_address", (int) fl_address);
		_MHDEBUG("at_wini_driver: rmt_phys", (rqtr_ptr->p_memmap[D].mem_phys << CLICK_SHIFT));
		_MHDEBUG("at_wini_driver: rmt_virt", (int) rmt_virt);
#ifdef LOGDBG	
		fprintf(fp_log,"fl_address=%X rmt_phys=%X rmt_virt=%X \n",
		fl_address,
		(rqtr_ptr->p_memmap[D].mem_phys << CLICK_SHIFT),
		rmt_virt);
#endif /* LOGDBG*/
		
#ifdef ANULADO		

		if (r2l[fl_ep].diff == NOT_INITIALIZED) 
		get_refdiff(fl_ep, fl_address, fl_bytes);
		
		rmt_virt = rvirt_lvirt(fl_address, fl_ep);
		fprintf(fp_log,"at_wini_driver: DEV_READ: rmt_virt=%X\n",rmt_virt);
		
		_MHDEBUG("at_wini_driver: DEV_READ", fl_dev);
		fprintf(fp_log,"at_wini_driver: DEV_READ: fl_dev=%d fl_ep=%d fl_offset=%d fl_bytes=%d\n",
		fl_dev, fl_ep, fl_offset,  fl_bytes);
		
		rcode = fseek(fp_disc[dr_index], fl_offset, SEEK_SET); 
		_MHDEBUG("at_wini_driver: fseek", rcode);
		
		rcode = fread(devbuf, 1, fl_bytes, fp_disc[dr_index]);
		_MHDEBUG("at_wini_driver: fread", rcode);

		rcode = sys_vcopyvm(SELF, VM0, devbuf, fl_ep, px_vmid, fl_address, fl_bytes);
		_MHDEBUG("at_wini_driver: sys_vcopyvm", rcode);		
		memcpy(rmt_virt, devbuf, fl_bytes); 

#else  /* ANULADO */

		rcode = fseek(fp_disc[dr_index], fl_offset, SEEK_SET); 
		_MHDEBUG("at_wini_driver: fseek", rcode);
		
		rcode = fread(rmt_virt, 1, fl_bytes, fp_disc[dr_index]); /* DIRECT READ to REQUESTER BUFFER !! */
		_MHDEBUG("at_wini_driver: fread", rcode);	
		
#endif /* ANULADO */	
		
		rcode = fl_bytes;
		break;
	case DEV_CLOSE:  
		fl_ep = mi_ptr->IO_ENDPT;
		fl_dev = mi_ptr->DEVICE;
		_MHDEBUG("at_wini_driver: DEV_CLOSE", fl_dev);
#ifdef LOGDBG	
		fprintf(fp_log,"at_wini_driver: DEV_CLOSE: fl_dev=%d fl_ep=%d\n",
		fl_dev,fl_ep);
#endif /* LOGDBG*/

		rcode = OK;
		break;
	case DEV_GATHER:
		fl_ep = mi_ptr->IO_ENDPT;
		fl_dev = mi_ptr->DEVICE;
		fl_offset = mi_ptr->POSITION;
		fl_nr_req = mi_ptr->COUNT;	/* Length of I/O vector */
		if (fl_nr_req > NR_IOREQS) fl_nr_req = NR_IOREQS;
		_MHDEBUG("at_wini_driver: DEV_GATHER", fl_nr_req);
#ifdef LOGDBG	
		fprintf(fp_log,"at_wini_driver: DEV_GATHER: fl_dev=%d fl_ep=%d fl_offset=%d fl_nr_req=%d\n",
		fl_dev, fl_ep, fl_offset, fl_nr_req);
#endif /* LOGDBG*/

		/* reading disk according minor (mi_ptr->DEVICE) */
		dr_index = fl_dev - minor_c0d0; 
		_MHDEBUG("at_wini_driver: DEV_GATHER", fp_disc[dr_index]);
		
		/* copy request vector from requester process address space to local address space */	
		rcode = sys_vcopyvm(mi_ptr->m_source, px_vmid, (vir_bytes) mi_ptr->ADDRESS, 
		SELF, VM0, (vir_bytes) iovec, (phys_bytes) (fl_nr_req * sizeof(iovec[0])));
		_MHDEBUG("at_wini_driver: sys_vcopyvm", rcode);
		
		rcode = fseek(fp_disc[dr_index], fl_offset, SEEK_SET); 
		_MHDEBUG("at_wini_driver: fseek", rcode);
		
		for( i = 0; i < fl_nr_req; i++) {
			iop = &iovec[i];			
			fl_address = (char *) iop->iov_addr;
			fl_bytes   = iop->iov_size;		
#ifdef LOGDBG	
			fprintf(fp_log,"at_wini_driver: iovec[%d] fl_address=%X fl_bytes=%d\n",
			i, fl_address, fl_bytes);
#endif /* LOGDBG*/

			count  = fl_bytes; 
			offset = 0;
			do	{
				bytes = (count > SECTOR_SIZE)?SECTOR_SIZE:count;
				
				/* read data from the virtual device file into the local buffer  */
				bytes = fread(devbuf, 1, bytes, fp_disc[dr_index]);
				_MHDEBUG("at_wini_driver: fread", bytes);

				/* copy the data from the local buffer to the requester process address space in other VM */
				rcode = sys_vcopyvm(SELF, VM0, devbuf, fl_ep, px_vmid, fl_address+offset, bytes);
				_MHDEBUG("at_wini_driver: sys_vcopyvm", rcode);
				offset += bytes;
				count -= bytes;
			} while(count > 0);
		}

		/* copy back the request vector from local space to requester process address space in other VM */	
		rcode = sys_vcopyvm(SELF, VM0, (vir_bytes) iovec, 
		mi_ptr->m_source, px_vmid, (vir_bytes) mi_ptr->ADDRESS, 
		(phys_bytes) (fl_nr_req * sizeof(iovec[0])));
		_MHDEBUG("at_wini_driver: sys_vcopyvm", rcode);
		
		break;
	case DEV_SCATTER:
		fl_ep = mi_ptr->IO_ENDPT;
		fl_dev = mi_ptr->DEVICE;
		fl_offset = mi_ptr->POSITION;
		fl_nr_req = mi_ptr->COUNT;	/* Length of I/O vector */
		if (fl_nr_req > NR_IOREQS) fl_nr_req = NR_IOREQS;
		_MHDEBUG("at_wini_driver: DEV_SCATTER", fl_nr_req);
#ifdef LOGDBG	
		fprintf(fp_log,"at_wini_driver: DEV_SCATTER: fl_dev=%d fl_ep=%d fl_offset=%d fl_nr_req=%d\n",
		fl_dev, fl_ep, fl_offset, fl_nr_req);
#endif /* LOGDBG*/
		
		/* reading disk according minor (mi_ptr->DEVICE) */
		dr_index = fl_dev - minor_c0d0;
		_MHDEBUG("at_wini_driver: disc to read", fp_disc[dr_index]);
		
		/* copy request vector from requester process address space to local address space */	
		rcode = sys_vcopyvm(mi_ptr->m_source, px_vmid, (vir_bytes) mi_ptr->ADDRESS, 
		SELF, VM0, (vir_bytes) iovec, (phys_bytes) (fl_nr_req * sizeof(iovec[0])));
		_MHDEBUG("at_wini_driver: sys_vcopyvm", rcode);
		
		rcode = fseek(fp_disc[dr_index], fl_offset, SEEK_SET); 
		_MHDEBUG("at_wini_driver: fseek", rcode);
		
		for( i = 0; i < fl_nr_req; i++) {
			iop = &iovec[i];			
			fl_address = (char*)iop->iov_addr;
			fl_bytes   = iop->iov_size;		
#ifdef LOGDBG	
			fprintf(fp_log,"at_wini_driver: iovec[%d] fl_address=%X fl_bytes=%d\n",
			i, fl_address, fl_bytes);
#endif /* LOGDBG*/
			count  = fl_bytes; 
			offset = 0;
			do	{
				bytes = (count > SECTOR_SIZE)?SECTOR_SIZE:count;
				
				/* copy the data from the requester process address space in other VM  to the local buffer */
				rcode = sys_vcopyvm( fl_ep, px_vmid, fl_address+offset,SELF, VM0, devbuf, bytes);
				_MHDEBUG("at_wini_driver: sys_vcopyvm", rcode);
				
				/* write data from local buffer to the  virtual device file */
				bytes = fwrite(devbuf, 1, bytes, fp_disc[dr_index]);
				_MHDEBUG("at_wini_driver: fwrite", bytes);

				offset += bytes;
				count -= bytes;
			} while(count > 0);
		}
		
		/* copy back the request vector from local space to requester process address space */	
		rcode = sys_vcopyvm(SELF, VM0, (vir_bytes) iovec, 
		mi_ptr->m_source, px_vmid, (vir_bytes) mi_ptr->ADDRESS, 
		(phys_bytes) (fl_nr_req * sizeof(iovec[0])));
		_MHDEBUG("at_wini_driver: sys_vcopyvm", rcode);
		fflush(fp_disc[dr_index]);
		sync();
		break;
	case CANCEL:     
	case DEV_WRITE:
		fl_ep = mi_ptr->IO_ENDPT;		
		fl_dev = mi_ptr->DEVICE;
		fl_bytes= mi_ptr->COUNT;
		fl_offset = mi_ptr->POSITION;
		fl_address = mi_ptr->ADDRESS;
		_MHDEBUG("at_wini_driver: DEV_WRITE", fl_dev);
		_MHDEBUG("at_wini_driver: fl_ep", fl_ep);
		
		/* reading disk according minor (mi_ptr->DEVICE) */
		dr_index = fl_dev - minor_c0d0;
		_MHDEBUG("at_wini_driver: disc to write (index))", dr_index);/*fp_disc[dr_index]);*/
		
		/* check if the local process descriptor cache is updated  */
		rqtr_nr = _ENDPOINT_P(fl_ep);
		_MHDEBUG("at_wini_driver: rqtr_nr", rqtr_nr);
		rqtr_ptr = &proc[rqtr_nr + NR_PROCS]; 
		if( rqtr_ptr->p_endpoint != fl_ep) {
			/* process endpoint does not match - get the new descriptor */
			rcode = sys_getproc(rqtr_ptr, fl_ep);
			if( rcode ) {
				_MHDEBUG("at_wini_driver: sys_getproc rcode", rcode);
			}
		}

#ifdef LOGDBG	
		fprintf(fp_log,PROC_FORMAT, PROC_FIELDS(rqtr_ptr));
#endif /* LOGDBG*/

		rmt_virt = (fl_address + abs((rqtr_ptr->p_memmap[D].mem_phys << CLICK_SHIFT) - at_wini_phys));
		_MHDEBUG("at_wini_driver: fl_address", (int) fl_address);
		_MHDEBUG("at_wini_driver: rmt_phys", (rqtr_ptr->p_memmap[D].mem_phys << CLICK_SHIFT));
		_MHDEBUG("at_wini_driver: rmt_virt", (int) rmt_virt);
#ifdef LOGDBG	
		fprintf(fp_log,"fl_address=%X rmt_phys=%X rmt_virt=%X \n",
		fl_address,
		(rqtr_ptr->p_memmap[D].mem_phys << CLICK_SHIFT),
		rmt_virt);
#endif /* LOGDBG*/
		
#ifdef ANULADO	
		
		if (r2l[fl_ep].diff == NOT_INITIALIZED) 
		get_refdiff(fl_ep, fl_address, fl_bytes);
		
		rmt_virt = rvirt_lvirt(fl_address, fl_ep);
		fprintf(fp_log,"at_wini_driver: DEV_WRITE: rmt_virt=%X\n",rmt_virt);
		
		_MHDEBUG("at_wini_driver: DEV_WRITE", fl_dev);
		fprintf(fp_log,"at_wini_driver: DEV_WRITE: fl_dev=%d fl_ep=%d fl_offset=%d fl_bytes=%d\n",
		fl_dev, fl_ep, fl_offset,  fl_bytes);
		
		/*rcode = sys_vcopyvm(fl_ep, px_vmid, fl_address, SELF, VM0, devbuf, fl_bytes);*/
		rcode = sys_vcopyvm( fl_ep, px_vmid, fl_address+offset,SELF, VM0, devbuf, fl_bytes);
		_MHDEBUG("at_wini_driver: sys_vcopyvm", rcode);		
		memcpy(rmt_virt, devbuf, fl_bytes);

		rcode = fseek(fp_disc[dr_index], fl_offset, SEEK_SET); 
		_MHDEBUG("at_wini_driver: fseek", rcode);
		
		/*rcode = fwirte(devbuf, 1, fl_bytes, fp_disc[dr_index]);*/
		_MHDEBUG("at_wini_driver: fwrite", rcode); 

#else  /* ANULADO */

		rcode = fseek(fp_disc[dr_index], fl_offset, SEEK_SET); 
		_MHDEBUG("at_wini_driver: fseek", rcode);
		
		/*rcode = fwrite(rmt_virt, 1, fl_bytes, fp_disc[dr_index]);*/ /* DIRECT READ to REQUESTER BUFFER !! */
		_MHDEBUG("at_wini_driver: fwrite", rcode);
		
#endif /* ANULADO */	
		
		sync();
		rcode = fl_bytes;
		break;
	case DEV_IOCTL: 
	case DEV_SELECT:	
	case DEV_STATUS: 
		_MHDEBUG("at_wini_driver: EINVAL fl_rqst", fl_rqst);
		rcode = EINVAL;
	default:
		_MHDEBUG("at_wini_driver: ERROR fl_rqst", fl_rqst);
		rcode = EINVAL;
		break;
	}

	mo_ptr->m_type = TASK_REPLY;
	mo_ptr->REP_ENDPT = fl_ep;
	mo_ptr->REP_STATUS = rcode;
	rcode = sendas(mi_ptr->m_source, mo_ptr, driver_nr);
	_MHDEBUG("at_wini_driver: sendas rcode", rcode);
	if( rcode) {
		return(rcode);
	}

	return(OK);
}

EXTERN int px_vmid;		/* VMID of the VM that this proxy represent */
EXTERN int px_ep;
EXTERN rmt2lcl_t r2l[NR_PROCS];

EXTERN char lcl_ref; 		/* reference local address */
EXTERN char *rmt_ref; 		/*address of the remote reference variable  */
EXTERN long diff;			/* difference */

EXTERN FILE *fp_log;
EXTERN message m;
EXTERN message *m_ptr;
EXTERN struct proc proc[NR_TASKS + NR_PROCS]; 


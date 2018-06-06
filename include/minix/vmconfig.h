/*
* FORMATS:
* device  <task_name> VDEV_DISK_MINOR <minor_base>
* device  <task_name>  VDEV_DISK_FILE  <file>
* device  <task_name>  VDEV_DISK_NET <ipaddr> [TCP!UDP]  <port>
* device  <task_name> VDEV_DISK_M3IPC <node>  <endpoint>
*/

/*
	<KEY_TYPE(4)> <VMID>(28)
	<KEY_TYPE(4)> 	 <VMID>(4)  <PROC_INDEX(8)>  <PROC_FLAGS(16)>
	<KEY_TYPE(4)> 	 <VMID>(4)  <VDEV_INDEX(8)>  <VDEV_DEV(8)>  <VDEV_VIRT(8)>	

	
*/
#define		KEY_MASK_TYPE	0xF0000000			
#define		KEY_SHIFT_TYPE	28
#define 	KEY_VM_CFG		1
#define 	KEY_PROC_CFG	2
#define 	KEY_VDEV_CFG	3

/* This mask are used to build DS key for a published   */
#define		PROC_MASK_VMID	0x0F000000			
#define		PROC_MASK_INDEX	0x00FF0000
#define		PROC_MASK_FLAGS 0x0000FFFF			/* 16 BITS for process flags - see kernel/proc.h */

/* MODIFY this bit shifts if you modify the mask above */
#define 	PROC_SHIFT_VMID 	24
#define	 	PROC_SHIFT_INDEX	16
#define	 	PROC_SHIFT_FLAGS	0

/* This mask are used to build DS key for a published   */
#define		VDEV_MASK_VMID	0x0F000000	
#define		VDEV_MASK_INDEX	0x00FF0000
#define		VDEV_MASK_DEV	0x0000FF00	
#define		VDEV_MASK_VIRT	0x000000FF	

/* MODIFY this bit shifts if you modify the mask above */
#define 	VDEV_SHIFT_VMID 	24
#define	 	VDEV_SHIFT_INDEX	16
#define	 	VDEV_SHIFT_DEV		8
#define	 	VDEV_SHIFT_VIRT		0

#define		VDEV_MASK_MINOR	0x00000001	
#define		VDEV_MASK_FILE	0x00000002	
#define		VDEV_MASK_NET	0x00000003	
#define		VDEV_MASK_M3IPC	0x00000004	

#define	VDEV_DISK			0x00000100	/* A virtual disk 		*/
#define		VDEV_DISK_MINOR	0x00000101	/* A virtual disk MINOR 	*/
#define		VDEV_DISK_FILE	0x00000102	/* A virtual disk IMAGE FILE	*/
#define		VDEV_DISK_NET	0x00000103	/* A virtual disk through NETWORK*/
#define		VDEV_DISK_M3IPC	0x00000104	/* A virtual disk through M3-IPC*/

#define	VDEV_FLOPPY			0x00000200	/* A virtual floppy		*/
#define		VDEV_FLOP_MINOR	0x00000201	/* A virtual floppy MINOR 	*/
#define		VDEV_FLOP_FILE	0x00000202	/* A virtual floppy IMAGE FILE	*/
#define		VDEV_FLOP_NET	0x00000203	/* A virtual floppy through NETWORK*/
#define		VDEV_FLOP_M3IPC	0x00000204	/* A virtual floppy through M3-IPC */

#define	VDEV_RS232			0x00000300	/* A virtual RS232		*/
#define		VDEV_RS_MINOR	0x00000301	/* A virtual RS232 MINOR	 	*/
#define		VDEV_RS_FILE	0x00000302	/* A virtual RS232 FILE		*/
#define		VDEV_RS_NET		0x00000303	/* A virtual RS232 through NETWORK*/
#define		VDEV_RS_M3IPC	0x00000304	/* A virtual RS232 through M3IPC */

#define	VDEV_PRINTER		0x00000400	/* A virtual PRINTER		*/
#define		VDEV_PRT_MINOR	0x00000401	/* A virtual PRINTER MINOR 	*/
#define		VDEV_PRT_FILE	0x00000402	/* A virtual PRINTER FILE	*/
#define		VDEV_PRT_NET	0x00000403	/* A virtual PRINTER through NETWORK*/
#define		VDEV_PRT_M3IPC	0x00000404	/* A virtual PRINTER through M3IPC*/

#define	VDEV_ETHERNET		0x00000500	/* A virtual ETHERNET		*/
#define		VDEV_ETH_DEV	0x00000501	/* A virtual ETHERNET DEVICE 	*/
#define		VDEV_ETH_VIRT	0x00000502	/* A virtual ETHERNET VIRTUAL DEVICE */

#define HOST_NAME_MAX	255		/* from LINUX kernel */
#define MAXPROARGS	5

struct vdev_cfg_s {			/* VIRTUAL device configuration */
	char 			d_vmname[PROC_NAME_LEN];		/* name of the VM including \0 					*/
	char 			d_name[PROC_NAME_LEN];
	unsigned int  	d_type;							/* Virtual device type 		*/
	union {
		int			d_minor;
		char 		d_filename[_POSIX_PATH_MAX]; 	/* pathname of a filename needed by the device  		*/
	}d_u;
};
typedef struct vdev_cfg_s vdev_cfg_t; 
#define DCFG_FORMAT		"d_vmname=%s d_name=%s d_type=%X\n"
#define DCFG_FIELDS(p)	p->d_vmname, p->d_name, p->d_type

struct proc_cfg_s {						/* structure for virtual, promicous and real processes configuration */
	char p_vmname[PROC_NAME_LEN];			/* name of the VM including \0 					*/
	int  p_nr;
	int  p_endpoint;
	int  p_flags;						/* at (BOOTTIME or EXECTIME) | (PROMISCOUS or VIRTUAL or REAL) 	*/
	char p_name[PROC_NAME_LEN];			/* name of the proces including \0 				*/
	char p_intargs[MAXPROARGS];			/* integer numbers needed by the process as arguments		*/
	char p_chararg[_POSIX_PATH_MAX]; 	/* pathname of a filename needed by the process  		*/
	unsigned int p_bitmap; 				/* bitmap needed by the process  					*/
	char p_next;
};
typedef struct proc_cfg_s proc_cfg_t;  
#define PCFG_FORMAT "p_vmname=%s p_nr=%d p_endpoint=%d p_flags=%X p_name=%s \n\tp_chararg=%s p_bitmap=%X\n"
#define PCFG_FIELDS(p) 	p->p_vmname, p->p_nr, p->p_endpoint, p->p_flags, p->p_name, p->p_chararg,p->p_bitmap 

struct vm_cfg_s {
	char v_name[PROC_NAME_LEN];			/* name of the VM including \0 					*/
	int  v_vmid;						/* this value could be set at runtine, not for configuration */
	int  v_endpoint;					/* this value could be set at runtine, not for configuration */
	int  v_size;						/* runtime or configuration */
	int  v_tokens;						/* MAX TOKEN ALLOCATION - NOT USED YET */
	char v_bootprog[_POSIX_PATH_MAX]; 	/* pathname of the boot program with the proxy embedded 	*/
	char v_bootimage[_POSIX_PATH_MAX]; /* pathname of the boot image that will be loaded by boot_prog 	*/
	unsigned long v_proc_bm;			/* bitmap of process that will be started on boot */
	proc_cfg_t *v_plist;				/* process list head */
};
typedef struct vm_cfg_s vm_cfg_t; 
#define VMCFG_FORMAT " v_name=%s v_vmid=%d size=%d v_endpoint=%d v_tokens=%d \n\tv_bootprog=%s\n\tv_bootimage=%s\n"
#define VMCFG_FIELDS(v) v->v_name, v->v_vmid, v->v_size, v->v_endpoint, v->v_tokens,  v->v_bootprog , v->v_bootimage 

#ifdef TEMPORARY_UNDEFINED
/* virtual device configuration for devices through IP network */
struct vdev_cfg_net_s {			
	char 		vcn_hname[HOST_NAME_MAX+1];  
	ipaddr_t	vcn_ipaddr;	/* Server IP address 		*/
	ipproto_t	vcn_proto; 	/* Server protocol: TCP, UDP 	*/
	u16_t		MINOR;	   	/* Server MINOR 			*/
		
};
typedef struct vdev_cfg_net_s vdev_cfg_net_t; 
#endif


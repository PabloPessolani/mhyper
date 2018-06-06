
union ds_union_u {
	vm_cfg_t 	ds_vm;
	proc_cfg_t	ds_proc;
	vdev_cfg_t  ds_vdev;
};
typedef union ds_union_u ds_union_t;

#define 	ds_vmid		ds_val_l1
#define 	ds_value	ds_val_l2

/* Type definitions for the Data Store Server. */
struct data_store {
  int ds_key;			/* key to lookup information */
  int ds_flags;			/* flags for this store */
  long ds_val_l1;		/* data associated with key */
  long ds_val_l2;		/* data associated with key */
#ifdef ANULADADO  
  long ds_auth;			/* secret given by owner of data */
  int ds_nr_subs;		/* number of subscribers for key */
#endif /* ANULADO */
  int  ds_endpoint;		/* endpoint of the owner	*/
  ds_union_t ds_u; 
  };
#define DS_FORMAT "ds_key=%X ds_flags=%X ds_val_l1=%d ds_val_l2=%d\n"
#define DS_FIELDS(p) 	p->ds_key,p->ds_flags, p->ds_val_l1, p->ds_val_l2

/* Flag values. */
#define DS_FREE			0x00
#define DS_IN_USE		0x01
#define DS_PUBLIC		0x02
#define DS_VMM_CFG		0x04
#define DS_PROC_CFG		0x08
#define DS_VDEV_CFG		0x10

/* Constants for the Data Store Server. */
#define NR_DS_KEYS	  64	/* reserve space for so many items */




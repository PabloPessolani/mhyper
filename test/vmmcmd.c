/* vmmcmd */
/***********************************************
	Virtual Machine Manager Commands
*  Sample File format
*
machine MYMINIX {
	size		64;
#	tokens 	32;   NO MORE !!!
	boot_prog	"/usr/src/test/loadvmimg";
	boot_image  "/boot/image/3.1.2H-MHYPERr1875";
	boot_bitmap 0xFFFFFFFF;
	process pm  REAL_PROC_TYPE  BOOT_LOADED  SERVER_LEVEL  DONOT_NTFY ;
	process fs REAL_PROC_TYPE   BOOT_LOADED  SERVER_LEVEL  DONOT_NTFY;
	process tty PROMISCUOUS_PROC_TYPE  BOOT_LOADED  TASK_LEVEL  BOOT_NTFY;
	process at_wini  PROMISCUOUS_PROC_TYPE   EXEC_LOADED   TASK_LEVEL  DONOT_NTFY;
	process printer  PARAVIRTUAL_PROC_TYPE 0x0F0F0F0F;
};
machine YOURMINIX {
	size	128;
#	tokens 	32;   NO MORE !!!
	boot_prog	"/usr/src/test/loadvmimg";
	boot_image  "/boot/image/3.1.2H-MHYPERr1875";
	boot_bitmap 0x0F0F0F0F;
	process pm  REAL_PROC_TYPE  BOOT_LOADED  SERVER_LEVEL  DONOT_NTFY ;
	process fs REAL_PROC_TYPE   BOOT_LOADED  SERVER_LEVEL  DONOT_NTFY;
	process tty	PROMISCUOUS_PROC_TYPE  BOOT_LOADED  TASK_LEVEL  BOOT_NTFY;
	process at_wini  PROMISCUOUS_PROC_TYPE   EXEC_LOADED   TASK_LEVEL  DONOT_NTFY;
};

**************************************************/
#define _POSIX_SOURCE      1	/* tell headers to include POSIX stuff */
#define _MINIX             1	/* tell headers to include MINIX stuff */
#define _SYSTEM            1	/* tell headers that this is the kernel */

/* The following are so basic, all the *.c files get them automatically. */
#include <minix/config.h>	/* MUST be first */

#include <ansi.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <lib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>

#include <minix/ipc.h>
#include <minix/callnr.h>
#include <minix/type.h>
#include <minix/const.h>
#include <minix/com.h>
#include <minix/syslib.h>
#include <minix/sysutil.h>
#include <minix/keymap.h>
#include <minix/bitmap.h>
#include <minix/minlib.h>
#include <minix/vmachine.h>
#include <minix/vmconfig.h>

#include <sys/stat.h>
#if __minix_vmd
#include <minix/asciictype.h>
#else
#include <ctype.h>
#endif
#include <configfile.h>
#include <unistd.h>
#include <limits.h>

#include "../servers/vmm/param.h"
#include "../servers/vmm/mhmacros.h"
#include "../servers/ds/store.h"
#define _TABLE
#include "../kernel/drivers.h"


#define MAXTOKENSIZE	20
#define OK				0
#define EXIT_CODE		1
#define NEXT_CODE		2

#define TKN_MACHINE		0
#define	TKN_SIZE		1
#define TKN_BOOT_PROG	2
#define TKN_BOOT_IMAGE	3
#define TKN_BIT_MAP		4
#define TKN_PROCESS		5
#define TKN_DEVICE		6

#define NR_IDENT 7

#define VMMCMD_OUT_FILENAME "vmmcmd.out"
	
#define nil ((void*)0)

int vmid;
int proc_idx;
int vdev_idx;

vm_cfg_t vm_cfg, *vmc_ptr;
proc_cfg_t *proc_ptr[NR_SYS_PROCS];
vdev_cfg_t *vdev_ptr[NR_SYS_PROCS];

char vmm_cfg[] = "/etc/vmm.cfg";
char *vmsize_ptr;
vmm_desc_t vmm_desc[NR_VMS];
vmm_desc_t *vmm_ptr;
int	mandatory;
int cmd; 

char *cfg_ident[] = {
	"machine",
	"size",
	"boot_prog",
	"boot_image",
	"boot_bitmap",
	"process",
	"device",
};

#define MAX_FLAG_LEN 30
struct flag_s {
	char f_name[MAX_FLAG_LEN];
	int f_value;
};
typedef struct flag_s flag_t;	

#define NR_PTYPE	5
flag_t proc_type[] = {
	{"DISABLED_PROC_TYPE",DISABLED_PROC_TYPE},
	{"REAL_PROC_TYPE",REAL_PROC_TYPE},	
	{"VIRTUAL_PROC_TYPE",VIRTUAL_PROC_TYPE},
	{"PROMISCUOUS_PROC_TYPE",PROMISCUOUS_PROC_TYPE},
	{"PARAVIRTUAL_PROC_TYPE",REAL_PROC_TYPE | PARAVIRTUAL }	
};

#define NR_LOAD	2
flag_t proc_load[] = {
	{"BOOT_LOADED",BOOT_LOADED},				
	{"EXEC_LOADED",EXEC_LOADED},			
};

#define NR_LEVEL 4
flag_t proc_level[] = {
	{"KERNEL_LEVEL",KERNEL_LEVEL},			
	{"TASK_LEVEL",TASK_LEVEL},				
	{"SERVER_LEVEL",SERVER_LEVEL},			
	{"USER_LEVEL",USER_LEVEL},				
};

#define NR_NTFY 3
flag_t proc_ntfy[] = {
	{"DONOT_NTFY",DONOT_NTFY},				
	{"BOOT_NTFY",BOOT_NTFY},				
	{"EXEC_NTFY",EXEC_NTFY},	
};

#define NR_VDEV 16
flag_t vdev_type[] = {
	{"VDEV_DISK_MINOR",VDEV_DISK_MINOR},
	{"VDEV_DISK_FILE",VDEV_DISK_FILE},	
	{"VDEV_DISK_NET",VDEV_DISK_NET},	
	{"VDEV_DISK_M3IPC",VDEV_DISK_M3IPC},
	{"VDEV_FLOP_MINOR",VDEV_FLOP_MINOR},
	{"VDEV_FLOP_FILE",VDEV_FLOP_FILE},
	{"VDEV_FLOP_NET",VDEV_FLOP_NET},
	{"VDEV_FLOP_M3IPC",VDEV_FLOP_M3IPC},
	{"VDEV_RS_MINOR",VDEV_RS_MINOR},
	{"VDEV_RS_FILE",VDEV_RS_FILE},
	{"VDEV_RS_NET",VDEV_RS_NET},
	{"VDEV_RS_M3IPC",VDEV_RS_M3IPC},
	{"VDEV_PRT_MINOR",VDEV_PRT_MINOR},
	{"VDEV_PRT_FILE",VDEV_PRT_FILE},
	{"VDEV_PRT_NET",VDEV_PRT_NET},
	{"VDEV_PRT_M3IPC",VDEV_PRT_M3IPC},
};	

#define NR_CMD 9
char	*cmdstring[NR_CMD] = {
	"INFO",
	"LOAD",
	"START",
	"STOP",
	"KILL",
	"SUSPEND",
	"RESUME",
	"SAVE",
	"RESTORE",
};

#define 	MANDATORY_SIZE		0x0001
#define 	MANDATORY_LOADER	0x0002
#define 	MANDATORY_IMAGE		0x0004
#define 	MANDATORY_ALL		0x0007
	
_PROTOTYPE(int main, (int argc, char *argv []));
_PROTOTYPE(void prtusage, (void));

extern char *const *environ;

#define MAXPARMLIST	128


/*
*  Publish  VM on DS
*/
int add_VM_DS(vm_cfg_t *vmc_ptr) 
{
	int rcode;
	static struct data_store ds;
	struct data_store *ds_ptr;

	ds_ptr = &ds;
	ds_ptr->ds_key  =  ( (KEY_VM_CFG << KEY_SHIFT_TYPE) |  (vmc_ptr->v_vmid));
	ds_ptr->ds_flags= (DS_IN_USE | DS_PUBLIC | DS_VMM_CFG);
	ds_ptr->ds_val_l1= vmc_ptr->v_vmid;
	ds_ptr->ds_val_l2= vmc_ptr->v_size;
	printf("add_VM_DS: " DS_FORMAT ,DS_FIELDS(ds_ptr));

	if (rcode = ds_publish(ds_ptr->ds_key, ds_ptr->ds_flags, 
		ds_ptr->ds_val_l1, ds_ptr->ds_val_l2, (char *) vmc_ptr)) {
      	fprintf(stderr,"Couldn't Publish VM at DS.: rcode=%d errno=%d\n",rcode, errno);
		return(EXIT_CODE);
	}
	return(OK);
}

/*
*  Publish  Process on DS
*/
int add_proc_DS(proc_cfg_t *p_ptr) 
{
	int rcode, i, driver_index;
	static struct data_store ds;
	struct data_store *ds_ptr;
	
	/* check if the boot process will be executed  */
	for( i = 0; i < NR_DFT_DRIVERS; i++){
		if(!strcmp(p_ptr->p_name, dflt_driver[i].drv_name)) {
			driver_index = i;
			break;
		}
	}	
	if( i == NR_DFT_DRIVERS){
	      	fprintf(stderr,"add_proc_DS: Invaled PROC name: rcode=%d\n",EINVAL);
		return(EINVAL);
	}

	ds_ptr = &ds;
	ds_ptr->ds_key  =  ( (KEY_PROC_CFG << KEY_SHIFT_TYPE) | (vmid << PROC_SHIFT_VMID) | (driver_index << PROC_SHIFT_INDEX)  | (p_ptr->p_flags));
	ds_ptr->ds_flags= (DS_IN_USE | DS_PUBLIC | DS_PROC_CFG);
	ds_ptr->ds_val_l1=vmid;
	ds_ptr->ds_val_l2=driver_index;
	printf("add_proc_DS: " DS_FORMAT ,DS_FIELDS(ds_ptr));

	if (rcode = ds_publish(ds_ptr->ds_key, ds_ptr->ds_flags, 
		ds_ptr->ds_val_l1, ds_ptr->ds_val_l2, (char *) p_ptr)) {
      	fprintf(stderr,"Couldn't Publish PROC at DS.: rcode=%d errno=%d\n",rcode, errno);
		return(EXIT_CODE);
	}
	return(OK);
}

/*
*  Delete a Published Process on DS
*/
int del_proc_DS(int p_index) 
{
	int rcode;
	unsigned long key;

	key  =  ( (KEY_PROC_CFG << KEY_SHIFT_TYPE) | (vmid << PROC_SHIFT_VMID) | (p_index << PROC_SHIFT_INDEX) | (proc_ptr[p_index]->p_flags));
	printf("Deleting from DS: key=%X VM=%s %s p_index=%d\n", 
		key , proc_ptr[p_index]->p_vmname, proc_ptr[p_index]->p_name, p_index);

	if (rcode = ds_delete(key) ){
      	fprintf(stderr,"Couldn't Delete PROC from DS key=%X VM=%s p_name=%s p_index=%d rcode=%d\n",
		key, proc_ptr[p_index]->p_vmname, proc_ptr[p_index]->p_name, p_index, rcode);
		return(EXIT_CODE);
	}
	
	return(OK);
}


/*
*  Delete a Published Virtual Device on DS
*/
int del_vdev_DS(int vd_index) 
{
	int rcode;
	unsigned long key;
	
	key  =  ( (KEY_VDEV_CFG << KEY_SHIFT_TYPE) | (vmid << VDEV_SHIFT_VMID) | (vd_index << VDEV_SHIFT_INDEX)  | (vdev_ptr[vd_index]->d_type));
	printf("Deleting from DS: key=%X VM=%s %s vd_index=%d\n", 
		key, vdev_ptr[vd_index]->d_vmname, vdev_ptr[vd_index]->d_name,  vd_index);
	if (rcode = ds_delete(key) ){
      	fprintf(stderr,"Couldn't Delete VDEV from DS key=%X VM=%s vd_name=%s vd_index=%d rcode=%d\n",
		key, vdev_ptr[vd_index]->d_vmname, vdev_ptr[vd_index]->d_name, vd_index, rcode);
		return(EXIT_CODE);
	}

	
	return(OK);
}

/*
*  Publish Virtual Device on DS
*/
int add_vdev_DS(vdev_cfg_t *d_ptr) 
{
	int rcode, i, driver_index;
	static struct data_store ds;
	struct data_store *ds_ptr;
	
	/* check if the boot process will be executed  */
	for( i = 0; i < NR_DFT_DRIVERS; i++){
		if(!strcmp(d_ptr->d_name, dflt_driver[i].drv_name)) {
			driver_index = i;
			break;
		}
	}
	if( i == NR_DFT_DRIVERS){
	      	fprintf(stderr,"add_vdev_DS: Invaled VDEV name: rcode=%d\n",EINVAL);
		return(EINVAL);
	}
		
	ds_ptr = &ds;
	ds_ptr->ds_key  =  ((KEY_VDEV_CFG << KEY_SHIFT_TYPE) |(vmid << VDEV_SHIFT_VMID) | (driver_index << VDEV_SHIFT_INDEX)  | (d_ptr->d_type));
	ds_ptr->ds_flags= (DS_IN_USE | DS_PUBLIC | DS_VDEV_CFG);
	ds_ptr->ds_val_l1=vmid;
	ds_ptr->ds_val_l2=driver_index;
	printf("add_vdev_DS: " DS_FORMAT ,DS_FIELDS(ds_ptr));

	if (rcode = ds_publish(ds_ptr->ds_key, ds_ptr->ds_flags, 
		ds_ptr->ds_val_l1, ds_ptr->ds_val_l2, (char *) d_ptr)) {
      	fprintf(stderr,"Couldn't VDEV Publish at DS.: rcode=%d errno=%d\n",rcode, errno);
		return(EXIT_CODE);
	}
	return(OK);
}

/*
* Search arguments into a "device" line
*/
int search_device(config_t *cfg)
{
	int i, j, rcode;
	unsigned int vd_type, vd_minor;
	
	if( cfg == nil) {
		fprintf(stderr, "No device name at line %d\n", cfg->line);
		return(EXIT_CODE);
	}
	if (config_isatom(cfg)) {
		printf("device=%s\n", cfg->word); 
		vdev_ptr[vdev_idx] = malloc(sizeof(vdev_cfg_t));
		bzero((void*)vdev_ptr[vdev_idx],sizeof(vdev_cfg_t));
		if( vdev_ptr[vdev_idx] == NULL) {
			fprintf(stderr, "malloc error %d\n",errno);
			return(EXIT_CODE);
		}else {
			if( strlen(cfg->word) > PROC_NAME_LEN-1) {
				fprintf(stderr, "Device name %s is too long at line %d\n",cfg->word, cfg->line);
				return(EXIT_CODE);
			}
			strncpy(vdev_ptr[vdev_idx]->d_name, cfg->word, PROC_NAME_LEN-1);							
		}
		cfg = cfg->next;
		if (! config_isatom(cfg)) {
			fprintf(stderr, "Bad argument type at line %d\n", cfg->line);
			return(EXIT_CODE);
		}
		vd_type = 0;
		/* 
		* Search for type 
		*/
		for( j = 0; j < NR_VDEV; j++) {
			if( !strcmp(cfg->word, vdev_type[j].f_name)) {
				vd_type = vdev_type[j].f_value & ~(VDEV_MASK_DEV);
				vdev_ptr[vdev_idx]->d_type = vdev_type[j].f_value;
				break;
			}
		}
		if( j == NR_VDEV){
			fprintf(stderr, "No device type defined at line %d\n", cfg->line);
			return(EXIT_CODE);
		}
		printf("\t%s vd_type=%X f_value=%X\n",
			vdev_type[j].f_name, vd_type,vdev_type[j].f_value);
			
		cfg = cfg->next;
		if (!config_isatom(cfg)) {
			fprintf(stderr, "Bad argument type at line %d\n", cfg->line);
			return(EXIT_CODE);
		}
		
		switch( vd_type ){
			case VDEV_MASK_MINOR:
				vd_minor = atoi(cfg->word);
				printf("\tminor=%d\n",vd_minor);
				vdev_ptr[vdev_idx]->d_u.d_minor = vd_minor;
				strncpy(vdev_ptr[vdev_idx]->d_vmname, vmc_ptr->v_name,PROC_NAME_LEN-1);				
				break;
			case VDEV_MASK_FILE:
				strncpy(vdev_ptr[vdev_idx]->d_u.d_filename,cfg->word, _POSIX_PATH_MAX-1);
				printf("\tpath=%s\n",vdev_ptr[vdev_idx]->d_u.d_filename);
				strncpy(vdev_ptr[vdev_idx]->d_vmname, vmc_ptr->v_name,PROC_NAME_LEN-1);				
				break;
			default:		
				fprintf(stderr, "Bad Virtual Device type 0x%X at line %d\n", 
						vdev_type[j].f_value, cfg->line);
				return(EXIT_CODE);
		}
	}else {
		fprintf(stderr, "invalid parameter for process at line %d\n", cfg->line);
		return(EXIT_CODE);
	}
	if( cmd == VMM_LOAD) {
		rcode = add_vdev_DS(vdev_ptr[vdev_idx]);
		return(rcode);
	}
	return(OK);
}

/*
* Search arguments into a "process" line
*/
int search_proc(config_t *cfg)
{
	int i, j, rcode;
	unsigned int flags;
	
	if( cfg == nil) {
		fprintf(stderr, "No process name at line %d\n", cfg->line);
		return(EXIT_CODE);
	}
	if (config_isatom(cfg)) {
		printf("process=[%s]\n", cfg->word); 
		proc_ptr[proc_idx] = malloc(sizeof(proc_cfg_t));
		bzero((void*)proc_ptr[proc_idx],sizeof(proc_cfg_t));
		if( proc_ptr[proc_idx] == NULL) {
			fprintf(stderr, "malloc error %d\n",errno);
			return(EXIT_CODE);
		}else {
			if( strlen(cfg->word) > PROC_NAME_LEN-1) {
				fprintf(stderr, "Process name %s is too long at line %d\n",cfg->word, cfg->line);
				return(EXIT_CODE);
			}
			strncpy(proc_ptr[proc_idx]->p_name, cfg->word, PROC_NAME_LEN-1);	
			printf("proc_ptr[proc_idx]->p_name=[%s]\n", proc_ptr[proc_idx]->p_name); 
		}
		cfg = cfg->next;
		if (! config_isatom(cfg)) {
			fprintf(stderr, "Bad argument type at line %d\n", cfg->line);
			return(EXIT_CODE);
		}
		flags = 0;
		/* 
		* Search for type 
		*/
		for( j = 0; j < NR_PTYPE; j++) {
			if( !strcmp(cfg->word, proc_type[j].f_name)) {
				flags |= proc_type[j].f_value;
				break;
			}
		}
		if( j == NR_PTYPE){
			fprintf(stderr, "No process type defined at line %d\n", cfg->line);
			return(EXIT_CODE);
		}
		if( (flags == DISABLED_PROC_TYPE)  ||
			(flags & REAL_PROC_TYPE)) {
			proc_ptr[proc_idx]->p_flags = flags;
			printf("\t%s\n",proc_type[j].f_name);  
			if( flags & (REAL_PROC_TYPE | PARAVIRTUAL) ){
				cfg = cfg->next;
				if (! config_isatom(cfg)) {
					fprintf(stderr, "Missing Kernel call bitmap(hex) in line %d\n", cfg->line);
					return(EXIT_CODE);
				}
				proc_ptr[proc_idx]->p_bitmap = strtol(cfg->word, NULL, 16);
/*
				flags |= (EXEC_LOADED | TASK_LEVEL |EXEC_NTFY )
				printf("proc_ptr[proc_idx]->p_bitmap=[%X] flags=%X\n", 
					proc_ptr[proc_idx]->p_bitmap, flags); 
*/
			}else{
				return(OK);
			}
		}else {
			printf("\t%s\n",proc_type[j].f_name); 	
		}
		
		cfg = cfg->next;
		if (! config_isatom(cfg)) {
			fprintf(stderr, "Bad argument type at line %d\n", cfg->line);
			return(EXIT_CODE);
		}
		/* 
		* Search for load time 
		*/
		for( j = 0; j < NR_LOAD; j++) {
			if( !strcmp(cfg->word, proc_load[j].f_name)) {
				flags |= proc_load[j].f_value;
				break;
			}
		}
		if( j == NR_LOAD){
			fprintf(stderr, "No process loading time defined at line %d\n", cfg->line);
			return(EXIT_CODE);
		}
		printf("\t%s\n",proc_load[j].f_name);  
		cfg = cfg->next;				
		if (! config_isatom(cfg)) {
			fprintf(stderr, "Bad argument type at line %d\n", cfg->line);
			return(EXIT_CODE);
		}
		/* 
		* Search for process level 
		*/
		for( j = 0; j < NR_LEVEL; j++) {
			if( !strcmp(cfg->word, proc_level[j].f_name)) {
				flags |= proc_level[j].f_value;
				break;
			}
		}
		if( j == NR_LEVEL){
			fprintf(stderr, "No process level defined at line %d\n", cfg->line);
			return(EXIT_CODE);
		}
		if( !(flags & PROMISCUOUS_PROC_TYPE) &&
			!(flags & VIRTUAL_PROC_TYPE) &&
			!(flags & PARAVIRTUAL)) {
			proc_ptr[proc_idx]->p_flags = flags;
			printf("\t%s\n",proc_level[j].f_name);  
			return(OK); 
		}

		/* 
		* Search for notify  
		*/
		cfg = cfg->next;
		if (! config_isatom(cfg)) {
			fprintf(stderr, "No notify action selected for virtual/promiscuous process at line %d\n", cfg->line);
			return(EXIT_CODE);
		}
		printf("\t%s\n",proc_level[j].f_name);  
		for( j = 0; j < NR_NTFY; j++) {
			if( !strcmp(cfg->word, proc_ntfy[j].f_name)) {
				flags |= proc_ntfy[j].f_value;
				break;
			}
		}
		if( j == NR_NTFY){
			fprintf(stderr, "Bad Notify action defined at line %d\n", cfg->line);
			return(EXIT_CODE);
		}
		proc_ptr[proc_idx]->p_flags = flags;
		strncpy(proc_ptr[proc_idx]->p_vmname, vmc_ptr->v_name,PROC_NAME_LEN-1);				
		printf("\t%s\n",proc_ntfy[j].f_name);
	}else {
		fprintf(stderr, "invalid parameter for process at line %d\n", cfg->line);
		return(EXIT_CODE);
	}
	printf(PCFG_FORMAT, PCFG_FIELDS(proc_ptr[proc_idx]));
	return(OK);
}

/*
* Search for an identifier: size, tokens, process 
*/
int search_ident(config_t *cfg)
{
	int i, j, rcode;
	
	for( i = 0; cfg!=nil; i++) {
		if (config_isatom(cfg)) {
			printf("search_ident[%d] line=%d word=%s\n",i,cfg->line, cfg->word); 
			for( j = 0; j < NR_IDENT; j++) {
				if( !strcmp(cfg->word, cfg_ident[j])) {
					printf("line[%d] MATCH identifier %s\n", cfg->line, cfg->word);  
					if( cfg->next == nil) {
						fprintf(stderr, "Void value found at line %d\n", cfg->line);
						return(EXIT_CODE);
					}
					cfg = cfg->next;				
					switch(j){
						case TKN_SIZE:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							vmc_ptr->v_size = atoi(cfg->word);
							printf("size=%d\n", vmc_ptr->v_size);
							if( vmc_ptr->v_size < 1 ){
								fprintf(stderr, "size error: %d < 1 \n",vmc_ptr->v_size );
								return(EXIT_CODE);
							}
							mandatory |= MANDATORY_SIZE;
							vmsize_ptr = cfg->word;
							break;				
						case TKN_BOOT_PROG:
							if (!config_isstring(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							if( strlen(cfg->word) > _POSIX_PATH_MAX-1) {
								fprintf(stderr, "Boot program path %s is too long at line %d\n",cfg->word, cfg->line);
								return(EXIT_CODE);
							}
							strncpy(vmc_ptr->v_bootprog, cfg->word, _POSIX_PATH_MAX-1);
							printf("boot_prog=[%s]\n", vmc_ptr->v_bootprog);
							mandatory |= MANDATORY_LOADER;
							break;
						case TKN_BOOT_IMAGE:
							if (!config_isstring(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							printf("boot_image=%s\n", cfg->word);
							if( strlen(cfg->word) > _POSIX_PATH_MAX-1) {
								fprintf(stderr, "Boot image path %s is too long at line %d\n",cfg->word, cfg->line);
								return(EXIT_CODE);
							}						
							strncpy(vmc_ptr->v_bootimage, cfg->word, _POSIX_PATH_MAX-1);	
							mandatory |= MANDATORY_IMAGE;							
							break;
						case TKN_BIT_MAP:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							vmc_ptr->v_proc_bm = strtol(cfg->word, (char **)NULL, 16); 
							printf("boot_bitmap=%X\n", vmc_ptr->v_proc_bm );
							break;
						case TKN_PROCESS:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							rcode = search_proc(cfg);
							if( rcode) return(rcode);
							strncpy(proc_ptr[proc_idx]->p_vmname, vmc_ptr->v_name,PROC_NAME_LEN-1);
							proc_ptr[proc_idx]->p_nr=0;
							proc_ptr[proc_idx]->p_endpoint=0;
							printf(PCFG_FORMAT, PCFG_FIELDS(proc_ptr[proc_idx]));
							if( cmd == VMM_LOAD) {
								rcode = add_proc_DS(proc_ptr[proc_idx]);
								if( rcode) return(rcode);
							}
							proc_idx++;
							break;
						case TKN_DEVICE:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							rcode = search_device(cfg);
							if( rcode) return(rcode);
							vdev_idx++;
							break;	
						default:
							fprintf(stderr, "Programming Error : %s:%d\n", cfg_ident[j], j);
							exit(1);
					}
					return(OK);
				}	
			}
			if(  j == NR_IDENT) {
				fprintf(stderr, "Invaild identifier found at line %d\n", cfg->line);
				return(EXIT_CODE);
			}
		}
		cfg = cfg->next;
	}
	return(OK);
}
/*
* read each line from a configuration file.
*/		
int read_lines(config_t *cfg)
{
	int i, j;
	int rcode;
	for ( i = 0; cfg != nil; i++) {
		printf("read_lines type=%X\n",cfg->flags); 
		rcode = search_ident(cfg->list);
		if( rcode) return(rcode);
		cfg = cfg->next;
	}
	if( mandatory!= MANDATORY_ALL){
		fprintf(stderr, "Some Mandatory attributes were not configured for the VM\n");
		exit(1);
	}
	return(OK);
}	

/*
* Search "machine" identifier and compares against the VM name wanted
*/
int search_machine(config_t *cfg, char vm_name[])
{
	int rcode;
    config_t *name_cfg;
	
    if (cfg != nil) {
		if (config_isatom(cfg)) {
			printf("%s %s",cfg->word,  cfg_ident[TKN_MACHINE]);  
			if( !strcmp(cfg->word, "machine")) {
				printf("TKN_MACHINE ");
				cfg = cfg->next;
				if (cfg != nil) {
					if (config_isatom(cfg)) {
						printf("%s\n", cfg->word);
						if( strlen(cfg->word) > PROC_NAME_LEN-1) {
							fprintf(stderr, "VM name %s is too long at line %d\n",cfg->word, cfg->line);
							return(EXIT_CODE);
						}
						if( strcmp(vm_name, cfg->word)) /* DONT MATCH: Ignore this machine */
							return(NEXT_CODE);
						name_cfg = cfg;
						cfg = cfg->next;
						if (!config_issub(cfg)) {
							fprintf(stderr, "Cell at \"%s\", line %u is not a sublist\n",cfg->word, cfg->line);
							return(EXIT_CODE);
						}
						vmc_ptr = malloc(sizeof(vm_cfg_t));
						if( vmc_ptr == NULL) {
							fprintf(stderr, "malloc error %d\n",errno);
							rcode = EXIT_CODE;
						}else {
							strncpy(vmc_ptr->v_name, name_cfg->word, PROC_NAME_LEN-1);
							printf("VMNAME >%s< >%s< \n", vmc_ptr->v_name, name_cfg->word);
							rcode = read_lines(cfg->list);
						}
						return(rcode);
					}
				}
			}
			fprintf(stderr, "Config error line:%d No machine token found\n", cfg->line);
			return(EXIT_CODE);
		}
		fprintf(stderr, "Config error line:%d No machine name found \n", cfg->line);
		return(EXIT_CODE);
	}
	return(EXIT_CODE);
}

/*
* Search all "machine" lines
*/
int search_vm_config(config_t *cfg,  char vm_name[])
{
	int rcode;
	int i;
	
    for( i=0; cfg != nil; i++) {
		if (!config_issub(cfg)) {
			fprintf(stderr, "Cell at \"%s\", line %u is not a sublist\n", cfg->word, cfg->line);
			return(EXIT_CODE);
		}
		mandatory = 0;
		printf("search_vm_config[%d] line=%d\n",i,cfg->line);
		rcode = search_machine(cfg->list, vm_name);
		printf("search_vm_config search_machine rcode=%d\n", rcode);
		if( rcode == EXIT_CODE) return(rcode);
		if( rcode == OK)		return(OK);
		cfg= cfg->next;
	}
	return(EXIT_CODE);
}

static int redirect_tty(void)
{
	int fd;
	char buffer[PATH_MAX + 1], *home;

	/* redirect stdout to a file if needed */
	if (isatty(STDOUT_FILENO))
	{
		/* first try: current directory */
		fd = open(VMMCMD_OUT_FILENAME, O_WRONLY | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR);
		if (fd < 0)
		{
			/* alternative: home directory */
			home = getenv("HOME");
			if (home)
			{
				snprintf(buffer, sizeof(buffer), "%s/%s", home, VMMCMD_OUT_FILENAME);
				buffer[sizeof(buffer) - 1] = 0;
				fd = open(buffer, O_WRONLY | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR);
			}
		}
				
		if (fd < 0)
		{
			perror("cannot create " VMMCMD_OUT_FILENAME " and $HOME/" VMMCMD_OUT_FILENAME);
			return -1;
		}
		
		/* move the fd to stdout */
		if (dup2(fd, STDOUT_FILENO) < 0 || close(fd) < 0)
		{
			perror("cannot redirect stdout");
			return -1;
		}
	}
	
	/* redirect stderr to stdout if needed */
	if (isatty(STDERR_FILENO))
	{
		if (dup2(STDOUT_FILENO, STDERR_FILENO) < 0)
		{
			perror("cannot redirect stderr");
			return -1;
		}
	}

	return 0;
}


void prtusage()
{
	int i;

	printf("Usage: vmmcmd <VM command> [args] \n");
	printf("\t Valid commands are:\n");
	for( i=0; i < NR_CMD; i++)
		printf("\t\t\t%s \n",cmdstring[i]);
	printf("\n");
	exit(1);
}
/***************************************************************
					MAIN
***************************************************************/
int main(argc, argv)
int argc;
char *argv[];
{
  int rcode, v, child;
  long vmsize, vmsizeMB, tokens;
  int sts, i;
  FILE *fp;
  pid_t pid;
  char *const envParms[] = {NULL};
  char *parmPtr[4];
  char prog[] = "loadvmimg";
  char *ptr;
  config_t *cfg;
static message m;
  struct kinfo kinfo;
	int mhyper;
	struct sigaction sa;

  	mhyper = sys_mhyper();
  	if( !mhyper) {
		printf("\n MHYPER is not Running!!!\n");
		exit(1);
  	}
  
	if( argc < 2)
		prtusage();

	proc_idx = 0;
	vdev_idx = 0;

	for( cmd=0; cmd < NR_CMD; cmd++)
		if( strcmp(argv[1], cmdstring[cmd]) == 0 )
			break;
  
	if( cmd == NR_CMD) 
		prtusage();

	vmc_ptr = &vm_cfg;
	cmd += VMM_RQ_BASE;
	MHDEBUG("VMMCMD: %s cmd=%d \n",
			cmdstring[cmd-VMM_RQ_BASE],	cmd);

 /*
  * Aqui habria que pedir info de todas las VMs al VMM.
  */
   	m.VMM_cmd   = VMM_TABDUMP;
	m.VMM_p1    =  &vmm_desc;
	rcode = vmmcmd(	m.VMM_cmd,
				m.VMM_i2,
				m.VMM_i3,
				m.VMM_p1,
				m.VMM_p2,
				m.VMM_p3);
	MHDEBUG("VMMCMD: VMM_TABDUMP sizeof(vmm_desc)=%d rcode=%d\n",sizeof(vmm_desc) ,rcode);
	
  switch(cmd)
	{
	case VMM_LOAD:
		/* Search the first free VM descriptor and set vmid */
		for (v = 0; v < NR_VMS; v++){
			vmm_ptr = &vmm_desc[v];
			MHDEBUG(VMM_FORMAT, VMM_FIELDS(vmm_ptr));
			if(vmm_ptr->vmm_flags == VM_FREE) {  
				vmid = v;
				break;
			}
		}
		if( v == NR_VMS){
			fprintf(stderr,"No FREE VM descriptor\n");
			exit(1);
		}
		/*checks usage format and argument number */
		if( argc != 3) {
			fprintf(stderr,"usage: vmmcmd LOAD <VM_name>\n");
			exit(1);
			}
		
		if( strlen(argv[2]) > PROC_NAME_LEN-1) {
			fprintf(stderr, "VM name too long (must be < %d)\n", PROC_NAME_LEN);
			exit(1);
		}
		
		/*
		* Verify if the VM is already loaded
		*/
		for (v = 0; v < NR_VMS; v++){
			vmm_ptr = &vmm_desc[v];		
			if( !strcmp(vmm_ptr->vmm_name,argv[2])) {
				fprintf(stderr, "VM %s is already loaded\n", argv[2]);
				exit(1);
			}
		}

		/* find next free VM */
		for (v = 0; v < NR_VMS; v++){
			vmm_ptr = &vmm_desc[v];		
			if( vmm_ptr->vmm_flags == VM_FREE) {
				break;
			}
		}
		
		/*checks VMM configuration file  */
		fp  = fopen(vmm_cfg,"r");
		if (fp == NULL) {
			fprintf(stderr, "Configuration File %s doesn't exist\n", vmm_cfg);
			exit(1);
		}
		fclose(fp);
		
		cfg= nil;
		cfg = config_read(vmm_cfg, CFG_ESCAPED, cfg);
		rcode = search_vm_config(cfg, argv[2]);
		if( rcode != OK)  {
			fprintf(stderr, "Configuration File error for VM %s\n", argv[2]);
			/***************************************************************************/
			/* AQUI HAY QUE SUPRIMIR TODAS LAS CONFIGURACIONES ENVIADAS AL DS */
			/***************************************************************************/
			exit(1);
		}
		
/*printf(VMCFG_FORMAT, VMCFG_FIELDS(vmc_ptr)); */

		/*checks VM memory size */
		vmsizeMB = vmc_ptr->v_size;
		if(vmsizeMB <= 0){
			fprintf(stderr,"Invalid VM memory size: %s\n", argv[3]);
			/***************************************************************************/
			/* AQUI HAY QUE SUPRIMIR TODAS LAS CONFIGURACIONES ENVIADAS AL DS */
			/***************************************************************************/
			exit(1);
			}

		/* Creates the VM image using execvm()	 */
		if( (strlen(vmc_ptr->v_bootprog)+strlen(vmsize_ptr)+ 5) >= MAXPARMLIST) {
			fprintf(stderr,"Boot image pathname too long\n");
			/***************************************************************************/
			/* AQUI HAY QUE SUPRIMIR TODAS LAS CONFIGURACIONES ENVIADAS AL DS */
			/***************************************************************************/
			exit(1);
		}
		
		/***************************************************************************/
		/* AQUI SE deberia chequear la existencia de los archivos de imagen y de proxy */
		/***************************************************************************/
		parmPtr[0] = vmc_ptr->v_bootprog;
		printf("parmPtr[0]=[%s]\n",parmPtr[0]);
		parmPtr[1] = vmc_ptr->v_name;
		printf("parmPtr[1]=[%s]\n",parmPtr[1]);
		parmPtr[2] = vmc_ptr->v_bootimage;
		printf("parmPtr[2]=[%s]\n",parmPtr[2]);
		parmPtr[3] = NULL;

		vmsize = vmsizeMB*1024*1024/CLICK_SIZE;
		
		child = fork();
		if( child < 0 ) {
			fprintf(stderr,"fork error %d errno=%d\n", child, errno);
			exit(rcode);
		}
		if( child > 0) break;
		
		vmc_ptr->v_tokens   = -1;
		vmc_ptr->v_vmid     = v;
		vmc_ptr->v_endpoint = getprocnr();

		rcode = add_VM_DS(vmc_ptr); 
		if(rcode != 0) {
			fprintf(stderr," add_VM_DS rcode:%d errno=%d\n", rcode, errno);
			exit(rcode);
		}
	  	MHDEBUG("vmmcmd LOAD vmsizeMB=%d (%d clicks), boot image file: %s \n", 
			vmsizeMB, 
			vmsize,
			vmc_ptr->v_bootimage);
		
		/* DETACH FROM SHELL */		
		/* ignore SIGHUP */
		sa.sa_handler = SIG_IGN;
		sigemptyset(&sa.sa_mask);
		sa.sa_flags = 0;
		if (sigaction(SIGHUP, &sa, NULL) < 0)	{
			fprintf(stderr, "cannot ignore SIGHUP\n");
			exit(1);
		}
		
		/* redirect tty to vmmcmd.out file */
		if (redirect_tty() < 0) {
			fprintf(stderr, "cannot rediret tty to %s\n", VMMCMD_OUT_FILENAME);
			exit(1);
		}

		/* the child continues */
		rcode = execvm(vmsize, 
				vmc_ptr->v_bootprog, 
				parmPtr, 
				envParms);

		fprintf(stderr,"Child rcode:%d errno=%d\n", rcode, errno);
		/* something has failed . Delete all Virtual devices, processes and VM from DS */
		for( i = 0; i < vdev_idx; i++){
			del_vdev_DS(i);		/* remove from DS */
			free(vdev_ptr[i]);	/* free allocated memory */
		}		
		/***************************************************************************/
		/* AQUI HAY QUE SUPRIMIR TODAS LAS CONFIGURACIONES ENVIADAS AL DS */
		/***************************************************************************/
		break;
	case VMM_START:
		/*checks usage format and argument number */
		if( argc != 4) {
			fprintf(stderr,"usage: vmmcmd START <VM_name> <VM_tokens>\n");
			exit(1);
			}

		if( strlen(argv[2]) > PROC_NAME_LEN-1) {
			fprintf(stderr, "VM name too long (must be < %d)\n", PROC_NAME_LEN);
			exit(1);
		}
		
		/*checks token count  */
		tokens = atoi(argv[3]);
		if(tokens <= 0 || tokens > MAXTOKENS){
			fprintf(stderr,"Invalid token count: %d\n", tokens);
			exit(1);
		}
			
		/*checks VMM configuration file  */
		fp  = fopen(vmm_cfg,"r");
		if (fp == NULL) {
			fprintf(stderr, "Configuration File %s doesn't exist\n", vmm_cfg);
			exit(1);
			}
		fclose(fp);
		
		cfg= nil;
		cfg = config_read(vmm_cfg, CFG_ESCAPED, cfg);
		rcode = search_vm_config(cfg, argv[2]);
		if( rcode != OK) {
			fprintf(stderr, "Configuration File error for VM %s\n", argv[2]);
			exit(1);
		}
		
/* printf(VMCFG_FORMAT, VMCFG_FIELDS(vmc_ptr)); */

		/*
		* Verify if the VM is already loaded
		*/
		for (v = 0; v < NR_VMS; v++){
			vmm_ptr = &vmm_desc[v];		
			if( !strcmp(vmm_ptr->vmm_name,argv[2])) {
				vmid = v;
				break;
			}
		}
		if( (vmid == NR_VMS) || !(vmm_ptr->vmm_flags & VM_LOADED) ){
			fprintf(stderr, "VM %s is not loaded\n", argv[2]);
			exit(1);
		}
		
		/* Verify the correct state to START */
		if( vmm_ptr->vmm_flags != (VM_LOADED | VM_WAITING) ){
			fprintf(stderr, "VM %s is not in correct state to START \n", argv[2]);
			exit(1);
		}	
		
		/*checks VM id */
		if(vmid <= 0 || vmid >= NR_VMS){
			fprintf(stderr,"Invalid VMID: %s\n", vmid);
			exit(1);
		}
		rcode = OK;
		
		/* Inform VMM about process/drivers configuration */
		for( i = 0; i < proc_idx; i++){
			m.VMM_cmd   = VMM_PROC;
			m.VMM_i2    = vmid;
			m.VMM_p1    = proc_ptr[i];	
			rcode = vmmcmd(	m.VMM_cmd,
				m.VMM_i2,
				m.VMM_i3,
				m.VMM_p1,
				m.VMM_p2,
				m.VMM_p3);
			MHDEBUG("VMMCMD VMM_PROC %s: rcode=%d\n",proc_ptr[i]->p_name, rcode);		
		}
	
		m.VMM_cmd   = VMM_START;
		m.VMM_i2    = vmid;
		m.VMM_i3    = getpid();
		MHDEBUG("VMMCMD: %s cmd=%d i2=%d i3=%d\n",
			cmdstring[cmd-VMM_RQ_BASE],
			m.VMM_cmd,
			m.VMM_i2,
			m.VMM_i3);
		m.VMM_p1 = (char *) tokens;		
		rcode = vmmcmd(	m.VMM_cmd,
				m.VMM_i2,
				m.VMM_i3,
				m.VMM_p1,
				m.VMM_p2,
				m.VMM_p3);
		MHDEBUG("VMMCMD VMM_START: rcode=%d\n",rcode);;		
		exit(rcode);
		break;	
	case VMM_KILL:
		if( argc != 3) {
			fprintf(stderr,"usage: vmmcmd %s <VMID> \n",cmdstring[cmd-VMM_RQ_BASE]);
			exit(1);
		}
		break;	
	case VMM_INFO:
		/* Aqui se deberia poder obtener informacion de la VM 	*/
		/* Y de los procesos de una VM 					*/
		break;
	case VMM_STOP:	
	case VMM_SUSPEND:
	case VMM_RESUME:
	case VMM_SAVE:	
	case VMM_RESTORE:
		m.VMM_cmd = cmd;
		m.VMM_i2  = 0x0001;
		m.VMM_i3  = 0x0002;
		m.VMM_p1  = (char *) 0x0003;
		m.VMM_p2  = (char *) 0x0004;
		m.VMM_p3  = (char *) 0x0005;
		MHDEBUG("VMMCMD: %s cmd=%d i2=%d i3=%d p1=%d p2=%d p3=%d \n",
			cmdstring[cmd-VMM_RQ_BASE],
			m.VMM_cmd,
			m.VMM_i2,
			m.VMM_i3,
			m.VMM_p1,
			m.VMM_p2,
			m.VMM_p3);		
		rcode = vmmcmd(	m.VMM_cmd,
						m.VMM_i2,
						m.VMM_i3,
						m.VMM_p1,
						m.VMM_p2,
						m.VMM_p3);
		if(rcode != 0) {
			printf("vmmcmd rcode=%d\n",rcode);
			exit(1);
			}
		break;
	default:
printf("cmd=%d\n",cmd);
		prtusage();
		exit(1);
		break;
	}
exit(0);
}


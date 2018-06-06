/***********************************************
	TEST CONFIGURATION FILE
*  Sample File format
*
machine MYMINIX {
	size		64;
	tokens	32;
	boot_prog	"/usr/src/test/loadvmimg";
	boot_image  "/boot/image/3.1.2H-MHYPERr1875";
	boot_bitmap 0xFFFFFFFF;
	process pm  REAL_PROC_TYPE  BOOT_LOADED  SERVER_LEVEL  DONOT_NTFY ;
	process fs REAL_PROC_TYPE   BOOT_LOADED  SERVER_LEVEL  DONOT_NTFY;
	process tty PROMISCUOUS_PROC_TYPE  BOOT_LOADED  TASK_LEVEL  BOOT_NTFY;
	process at_wini  PROMISCUOUS_PROC_TYPE   EXEC_LOADED   TASK_LEVEL  DONOT_NTFY;
};
machine YOURMINIX {
	size	128;
	tokens	64;
	boot_prog	"/usr/src/test/loadvmimg";
	boot_image  "/boot/image/3.1.2H-MHYPERr1875";
	boot_bitmap 0x0F0F0F0F;
	process pm  REAL_PROC_TYPE  BOOT_LOADED  SERVER_LEVEL  DONOT_NTFY ;
	process fs REAL_PROC_TYPE   BOOT_LOADED  SERVER_LEVEL  DONOT_NTFY;
	process tty	PROMISCUOUS_PROC_TYPE  BOOT_LOADED  TASK_LEVEL  BOOT_NTFY;
	process at_wini  PROMISCUOUS_PROC_TYPE   EXEC_LOADED   TASK_LEVEL  DONOT_NTFY;
};

**************************************************/
#include <sys/types.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#if __minix_vmd
#include <minix/asciictype.h>
#else
#include <ctype.h>
#endif
#include <configfile.h>

#include <unistd.h>
#include <limits.h>
#include <minix/config.h>
#include <minix/com.h>
#include <minix/const.h>
#include <minix/vmconfig.h>

#define MAXTOKENSIZE	20
#define OK				0
#define EXIT_CODE		1
#define NEXT_CODE		2

#define	TKN_SIZE		0
#define TKN_TOKENS		1
#define TKN_BOOT_PROG	2
#define TKN_BOOT_IMAGE	3
#define TKN_BIT_MAP		4
#define TKN_PROCESS		5

#define nil ((void*)0)

int vm_id;
int proc_id;
vm_cfg_t *vmc_ptr[NR_VMS];
proc_cfg_t *proc_ptr[NR_VMS*NR_SYS_PROCS];


char *cfg_ident[] = {
	"size",
	"tokens",
	"boot_prog",
	"boot_image",
	"boot_bitmap",
	"process",
};
#define NR_IDENT 6

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

int search_proc(config_t *cfg)
{
	int i, j, rcode;
	unsigned int flags;
	config_t *cfg_lcl;
	
	if( cfg == nil) {
		fprintf(stderr, "No process name at line %d\n", cfg->line);
		return(EXIT_CODE);
	}
	if (config_isatom(cfg)) {
/*		printf("process=%s\n", cfg->word); */
		proc_ptr[proc_id] = malloc(sizeof(proc_cfg_t));
		if( proc_ptr[vm_id] == NULL) {
			fprintf(stderr, "malloc error %d\n",errno);
			return(EXIT_CODE);
		}else {
			strncpy(proc_ptr[proc_id]->p_name, cfg->word, P_NAME_LEN);							
		}
		cfg = cfg->next;
		if (! config_isatom(cfg)) {
			fprintf(stderr, "Bad argument type at line %d\n", cfg->line);
			return(EXIT_CODE);
		}
		cfg_lcl = cfg;
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
		if( (flags & DISABLED_PROC_TYPE)  ||
			(flags & REAL_PROC_TYPE)) {
			proc_ptr[proc_id]->p_flags = flags;
/*			printf("\t%s\n",proc_type[j].f_name);  */

			if( flags & (REAL_PROC_TYPE | PARAVIRTUAL) ){
				cfg = cfg->next;
				if (! config_isatom(cfg)) {
					fprintf(stderr, "Missing Kernel call bitmap(hex) in line %d\n", cfg->line);
					return(EXIT_CODE);
				}
				proc_ptr[proc_id]->p_bitmap = strtol(cfg->word, NULL, 16);
				printf("proc_ptr[proc_id]->p_bitmap=[%X]\n", proc_ptr[proc_id]->p_bitmap); 
			}
			return(OK);
		}
		cfg = cfg->next;
/*		printf("\t%s\n",proc_type[j].f_name); 	*/
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
/*		printf("\t%s\n",proc_load[j].f_name);  */
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
		if( !(flags & PROMISCUOUS_PROC_TYPE)) {
			proc_ptr[proc_id]->p_flags = flags;
/*			printf("\t%s\n",proc_level[j].f_name);  */
			return(OK);
		}

		/* 
		* Search for notify  
		*/
		cfg = cfg->next;
		if (! config_isatom(cfg)) {
			fprintf(stderr, "No notify action selected for promiscuous process at line %d\n", cfg->line);
			return(EXIT_CODE);
		}
/*		printf("\t%s\n",proc_level[j].f_name);  */
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
		proc_ptr[proc_id]->p_flags = flags;
/*		printf("\t%s\n",proc_ntfy[j].f_name);  */
	}else {
		fprintf(stderr, "invalid parameter for process at line %d\n", cfg->line);
		return(EXIT_CODE);
	}
	return(OK);
}

int search_ident(config_t *cfg)
{
	int i, j, rcode;
	
	for( i = 0; cfg!=nil; i++) {
		if (config_isatom(cfg)) {
/*			printf("search_ident[%d] line=%d word=%s\n",i,cfg->line, cfg->word); */
			for( j = 0; j < NR_IDENT; j++) {
				if( !strcmp(cfg->word, cfg_ident[j])) {
/*					printf("line[%d] MATCH identifier %s\n", cfg->line, cfg->word); */
					if( cfg->next == nil)
						fprintf(stderr, "Void value found at line %d\n", cfg->line);
					cfg = cfg->next;				
					switch(j){
						case TKN_SIZE:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							vmc_ptr[vm_id]->v_size = atoi(cfg->word);
							printf("size=%d\n", vmc_ptr[vm_id]->v_size);
							if( vmc_ptr[vm_id]->v_size < 1 ){
								fprintf(stderr, "size error: %d < 1 \n",vmc_ptr[vm_id]->v_size );
								return(EXIT_CODE);
							}
							break;
						case TKN_TOKENS:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							vmc_ptr[vm_id]->v_tokens = atoi(cfg->word);
							printf("token=%d\n", vmc_ptr[vm_id]->v_tokens);
							if( vmc_ptr[vm_id]->v_tokens < 0 || vmc_ptr[vm_id]->v_tokens > MAXTOKENS ){
								fprintf(stderr, "tokens error: 0 <= %d <= %d \n",vmc_ptr[vm_id]->v_size, MAXTOKENS );
								return(EXIT_CODE);
							}
							break;
						case TKN_BOOT_PROG:
							if (!config_isstring(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							printf("boot_prog=%s\n", cfg->word);
							strncpy(vmc_ptr[vm_id]->v_bootprog, cfg->word, _POSIX_PATH_MAX);
							break;
						case TKN_BOOT_IMAGE:
							if (!config_isstring(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							printf("boot_image=%s\n", cfg->word);
							strncpy(vmc_ptr[vm_id]->v_bootimage, cfg->word, _POSIX_PATH_MAX);							
							break;
						case TKN_BIT_MAP:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							vmc_ptr[vm_id]->v_proc_bm = strtol(cfg->word, (char **)NULL, 16); 
							printf("boot_bitmap=%X\n", vmc_ptr[vm_id]->v_proc_bm );
							break;
						case TKN_PROCESS:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							rcode = search_proc(cfg);
							printf(PCFG_FORMAT, PCFG_FIELDS(proc_ptr[proc_id]));
							if( rcode) return(rcode);
							proc_id++;
							break;
						default:
							fprintf(stderr, "Programming Error\n");
							exit(1);
					}
					return(OK);
				}	
			}
			if(  j == NR_IDENT)
				fprintf(stderr, "Invaild identifier found at line %d\n", cfg->line);
		}
		cfg = cfg->next;
	}
	return(OK);
}
		
int read_lines(config_t *cfg)
{
	int i, j;
	int rcode;
	for ( i = 0; cfg != nil; i++) {
/*		printf("read_lines type=%X\n",cfg->flags); */
		rcode = search_ident(cfg->list);
		if( rcode) return(rcode);
		cfg = cfg->next;
	}
	return(OK);
}	

int search_machine(config_t *cfg)
{
	int rcode;
    config_t *name_cfg;
	
    if (cfg != nil) {
		if (config_isatom(cfg)) {
/*			printf("%s %s",cfg->word,  cfg_ident[TKN_MACHINE]);  */
			if( !strcmp(cfg->word, "machine")) {
				printf("TKN_MACHINE ");
				cfg = cfg->next;
				if (cfg != nil) {
					if (config_isatom(cfg)) {
						printf("%s\n", cfg->word);
						name_cfg = cfg;
						cfg = cfg->next;
						if (!config_issub(cfg)) {
							fprintf(stderr, "Cell at \"%s\", line %u is not a sublist\n",cfg->word, cfg->line);
							return(EXIT_CODE);
						}
						vmc_ptr[vm_id] = malloc(sizeof(vm_cfg_t));
						if( vmc_ptr[vm_id] == NULL) {
							fprintf(stderr, "malloc error %d\n",errno);
							rcode = EXIT_CODE;
						}else {
							strncpy(vmc_ptr[vm_id]->v_name, name_cfg->word, P_NAME_LEN);							
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

int search_vm_config(config_t *cfg)
{
	int rcode;
	int i;
	
    for( i=0; cfg != nil; i++) {
		if (!config_issub(cfg)) {
			fprintf(stderr, "Cell at \"%s\", line %u is not a sublist\n", cfg->word, cfg->line);
			return(EXIT_CODE);
		}
		printf("search_vm_config[%d] line=%d\n",i,cfg->line);
		rcode = search_machine(cfg->list);
		if( rcode == EXIT_CODE)
			return(rcode);
/*		printf("\n\n[%d] " VMCFG_FORMAT, vm_id, VMCFG_FIELDS(vmc_ptr[vm_id])); */
		cfg= cfg->next;
		vm_id++;
	}
	return(OK);
}



int main(int argc, char **argv)
{
    config_t *cfg;
    int rcode;
	
	vm_id = 0;
	proc_id = 0;

    if (argc != 2) {
		fprintf(stderr, "One config file name please\n");
		exit(1);
    }

    cfg= nil;
	rcode  = OK;
	cfg= config_read(argv[1], CFG_ESCAPED, cfg);

	rcode = search_vm_config(cfg);
	
}


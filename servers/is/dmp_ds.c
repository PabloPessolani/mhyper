/* This file contains procedures to dump DS data structures.
 *
 * The entry points into this file are
 *   data_store_dmp:   	display DS data store contents 
 *
 * Created:
 *   Oct 18, 2005:	by Jorrit N. Herder
 */

 
#include "inc.h"
#include <minix/vmconfig.h>
#include <minix/minlib.h>
#include "../ds/store.h"

PUBLIC struct data_store store[NR_DS_KEYS];

FORWARD _PROTOTYPE( char *s_flags_str, (int flags)		);

/***********************************************************************************************/
/*					MHYPER CODE										*/
/***********************************************************************************************/
#ifdef MHYPER
/*===========================================================================*
 *				data_store_dmp				     *
 *===========================================================================*/
PUBLIC void data_store_dmp()
{
  struct data_store *dsp;
  int i,j, n=0;
  static int prev_i=0;


  	vm_cfg_t *vmc_ptr;
	proc_cfg_t *pc_ptr;
	vdev_cfg_t *vd_ptr;
	
	if(mhyper){	
		if( active_vm != VM0) {
			getsysinfo_vm(DS_PROC_NR, SI_DATA_STORE, store, active_vm);
		}else{
			getsysinfo(DS_PROC_NR, SI_DATA_STORE, store);
		}
		printf("Data Store (DS) contents dump. (VM%d)\n", active_vm);
	}else{
		getsysinfo(DS_PROC_NR, SI_DATA_STORE, store);
		printf("Data Store (DS) contents dump.\n");
	}

	printf("slot -key- flags -val_l1- -val_l2- endpoint \n");

	for (i=prev_i; i<NR_DS_KEYS; i++) {
		dsp = &store[i];
		if (! dsp->ds_flags & DS_IN_USE) continue;
		if (++n > 22) break;
		printf("%2d %4X %6s %8d %8d %8d " ,
			i, 
			dsp->ds_key,
			s_flags_str(dsp->ds_flags),
			dsp->ds_val_l1,
			dsp->ds_val_l2,
			dsp->ds_endpoint
			);
		if(mhyper){	
			vmc_ptr = &dsp->ds_u.ds_vm;
			pc_ptr  = &dsp->ds_u.ds_proc;
			vd_ptr  = &dsp->ds_u.ds_vdev;
			if( dsp->ds_flags & DS_VMM_CFG ) {
				printf(VMCFG_FORMAT,  VMCFG_FIELDS(vmc_ptr));
			}else if( dsp->ds_flags & DS_PROC_CFG) {
				printf(PCFG_FORMAT,  PCFG_FIELDS(pc_ptr));
			} if( dsp->ds_flags & DS_VDEV_CFG) {
				printf(DCFG_FORMAT,  DCFG_FIELDS(vd_ptr));
			} else{
				printf("\n");
			}
		} else{
			printf("\n");
		}
  }
  if (i >= NR_DS_KEYS) i = 0;
  else printf("--more--\r");
  prev_i = i;
}

PRIVATE char *s_flags_str(int flags)
{
	static char str[5];
	str[0] = (flags & DS_IN_USE) ? 'U' : '-';
	str[1] = (flags & DS_PUBLIC) ? 'P' : '-';
	str[2] = (flags & DS_VMM_CFG) ? 'V' : '-';
	str[3] = (flags & DS_PROC_CFG) ? 'T' : '-';
	str[4] = (flags & DS_VDEV_CFG) ? 'D' : '-';
	str[5] = '-';
	str[6] = '\0';

	return(str);
}

/***********************************************************************************************/
/*					MINIX CODE										*/
/***********************************************************************************************/
#else /*  MHYPER  */
/*===========================================================================*
 *				data_store_dmp				     *
 *===========================================================================*/
PUBLIC void data_store_dmp()
{
  struct data_store *dsp;
  int i,j, n=0;
  static int prev_i=0;


  printf("Data Store (DS) contents dump\n");

  getsysinfo(DS_PROC_NR, SI_DATA_STORE, store);

  printf("-slot- -key- -flags- -val_l1- -val_l2-\n");

  for (i=prev_i; i<NR_DS_KEYS; i++) {
  	dsp = &store[i];
  	if (! dsp->ds_flags & DS_IN_USE) continue;
  	if (++n > 22) break;
  	printf("%3d %8d %s  [%8d] [%8d] \n",
		i, dsp->ds_key,
		s_flags_str(dsp->ds_flags),
		dsp->ds_val_l1,
		dsp->ds_val_l2
  	);
  }
  if (i >= NR_DS_KEYS) i = 0;
  else printf("--more--\r");
  prev_i = i;
}


PRIVATE char *s_flags_str(int flags)
{
	static char str[5];
	str[0] = (flags & DS_IN_USE) ? 'U' : '-';
	str[1] = (flags & DS_PUBLIC) ? 'P' : '-';
	str[2] = '-';
	str[3] = '\0';

	return(str);
}


#endif /*  MHYPER  */

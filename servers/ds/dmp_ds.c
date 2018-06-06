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

/*===========================================================================*
 *				data_store_dmp				     *
 *===========================================================================*/
PUBLIC void data_store_dmp()
{
  struct data_store *dsp;
  int i,j, n=0;
  static int prev_i=0;

#ifdef MHYPER
 	if( active_vm != VM0) {
		getsysinfo_vm(DS_PROC_NR, SI_DATA_STORE, store, active_vm);
	}else{
		getsysinfo(DS_PROC_NR, SI_DATA_STORE, store);
	}
  printf("Data Store (DS) contents dump. (VM%d)\n", active_vm);
#else /*  MHYPER */
  printf("Data Store (DS) contents dump.\n");
  getsysinfo(DS_PROC_NR, SI_DATA_STORE, store);
#endif /*  MHYPER  4558558*/

  printf("slot -key- flags -val_l1- -val_l2- endpoint \n");

  for (i=prev_i; i<NR_DS_KEYS; i++) {
  	dsp = &store[i];
  	if (! dsp->ds_flags & DS_IN_USE) continue;
  	if (++n > 22) break;
	if( dsp->ds_flags & DS_VMM_CFG ) {
		printf("%2d %4X %6s %8d %8d %8d %4d %4d %4d %8s %8X\n" ,
			i, 
			dsp->ds_key,
			s_flags_str(dsp->ds_flags),
			dsp->ds_val_l1,
			dsp->ds_val_l2,
			dsp->ds_endpoint,
			dsp->ds_u.ds_vm.v_vmid,
			dsp->ds_u.ds_vm.v_size,
			dsp->ds_u.ds_vm.v_tokens,
			dsp->ds_u.ds_vm.v_name,
			dsp->ds_u.ds_vm.v_proc_bm			
		);
	}else if( dsp->ds_flags & DS_PROC_CFG) {
		printf("%2d %4X %6s %8d %8d %8d %4d %8d %8s %8X %s\n" ,
			i, 
			dsp->ds_key,
			s_flags_str(dsp->ds_flags),
			dsp->ds_val_l1,
			dsp->ds_val_l2,
			dsp->ds_endpoint,
			dsp->ds_u.ds_proc.p_nr,
			dsp->ds_u.ds_proc.p_endpoint,
			dsp->ds_u.ds_proc.p_name,
			dsp->ds_u.ds_proc.p_flags,
			dsp->ds_u.ds_proc.p_vmname,			
		);	 
	} if( dsp->ds_flags & DS_VDEV_CFG) {
		printf("%2d %4X %6s %8d %8d %8d %8s %8X %s\n" ,
			i, 
			dsp->ds_key,
			s_flags_str(dsp->ds_flags),
			dsp->ds_val_l1,
			dsp->ds_val_l2,
			dsp->ds_endpoint,
			dsp->ds_u.ds_vdev.d_name,
			dsp->ds_u.ds_vdev.d_type,
			dsp->ds_u.ds_vdev.d_vmname,			
		);	
	} else{
		printf("%4d %5X %6s %8d %8d %8d\n" ,
			i, 
			dsp->ds_key,
			s_flags_str(dsp->ds_flags),
			dsp->ds_val_l1,
			dsp->ds_val_l2,
			dsp->ds_endpoint
		);
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


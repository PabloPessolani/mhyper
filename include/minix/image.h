/*	image.h - Info between installboot and boot.	Author: Kees J. Bot
 */
#ifndef _IMAGE_H
#define _IMAGE_H

#ifndef _AOUT_H
#include <a.out.h>
#endif

#define IM_NAME_MAX	63

struct image_header {
	char		name[IM_NAME_MAX + 1];	/* Null terminated. */
	struct exec	process;
};

#define P_NAME_LEN	   8
typedef int proc_nr_t;			/* process table entry number */
typedef _PROTOTYPE( void task_t, (void) );
struct boot_image {
  proc_nr_t proc_nr;			/* process number to use */
  task_t *initial_pc;			/* start function for tasks */
  int flags;				/* process flags */
  unsigned char quantum;		/* quantum (tick count) */
  int priority;				/* scheduling priority */
  int stksize;				/* stack size for tasks */
  short trap_mask;			/* allowed system call traps */
  bitchunk_t ipc_to;			/* send mask protection */
  long call_mask;				/* system call protection */
  char proc_name[P_NAME_LEN];		/* name in process table */
  int endpoint;				/* endpoint number when started */
};

#ifdef MHYPER
#define BIMG_FORMAT "proc_nr=%d initial_pc=%X flags=%X quantum=%d priority=%d stksize=%d endpoint=%d proc_name=%s\n"
#define BIMG_FIELDS(bip) 	bip->proc_nr, (unsigned long) bip->initial_pc, bip->flags, bip->quantum, bip->priority, bip->stksize, bip->endpoint, bip->proc_name
#endif /*  MHYPER */
/*
 * $PchId: image.h,v 1.4 1995/11/27 22:23:12 philip Exp $
 */

#endif
 
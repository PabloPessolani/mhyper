/* General macros and constants used by the kernel. */
#ifndef CONST_H
#define CONST_H

#include <ibm/interrupt.h>	/* interrupt numbers and hardware vectors */
#include <ibm/ports.h>		/* port addresses and magic numbers */
#include <ibm/bios.h>		/* BIOS addresses, sizes and magic numbers */
#include <ibm/cpu.h>		/* BIOS addresses, sizes and magic numbers */
#include <minix/config.h>
#include "config.h"

/* To translate an address in kernel space to a physical address.  This is
 * the same as umap_local(proc_ptr, D, vir, sizeof(*vir)), but less costly.
 */
#define vir2phys(vir)	(kinfo.data_base + (vir_bytes) (vir))
#ifdef MHYPER
#define CODE2PHYS(vir)	(vm_hyper[VM0].vm_kinfo.code_base + (vir_bytes) (vir))
#define VIR2PHYS(vir)	(vm_hyper[VM0].vm_kinfo.data_base + (vir_bytes) (vir))
#endif /*  MHYPER */
/* Map a process number to a privilege structure id. */
#define s_nr_to_id(n)	(NR_TASKS + (n) + 1)

/* Translate a pointer to a field in a structure to a pointer to the structure
 * itself. So it translates '&struct_ptr->field' back to 'struct_ptr'.
 */
#define structof(type, field, ptr) \
	((type *) (((char *) (ptr)) - offsetof(type, field)))

/* Translate an endpoint number to a process number, return success. */
#define isokendpt(e,p) isokendpt_d((e),(p),0)
#define okendpt(e,p)   isokendpt_d((e),(p),1)

/* Constants used in virtual_copy(). Values must be 0 and 1, respectively. */
#define _SRC_	0
#define _DST_	1

/* Number of random sources */
#define RANDOM_SOURCES	16

/* Constants and macros for bit map manipulation. */
#define BITCHUNK_BITS   (sizeof(bitchunk_t) * CHAR_BIT)
#define BITMAP_CHUNKS(nr_bits) (((nr_bits)+BITCHUNK_BITS-1)/BITCHUNK_BITS)  
#define MAP_CHUNK(map,bit) (map)[((bit)/BITCHUNK_BITS)]
#define CHUNK_OFFSET(bit) ((bit)%BITCHUNK_BITS))
#define GET_BIT(map,bit) ( MAP_CHUNK(map,bit) & (1 << CHUNK_OFFSET(bit) )
#define SET_BIT(map,bit) ( MAP_CHUNK(map,bit) |= (1 << CHUNK_OFFSET(bit) )
#define UNSET_BIT(map,bit) ( MAP_CHUNK(map,bit) &= ~(1 << CHUNK_OFFSET(bit) )

#define get_sys_bit(map,bit) \
	( MAP_CHUNK(map.chunk,bit) & (1 << CHUNK_OFFSET(bit) )
#define set_sys_bit(map,bit) \
	( MAP_CHUNK(map.chunk,bit) |= (1 << CHUNK_OFFSET(bit) )
#define unset_sys_bit(map,bit) \
	( MAP_CHUNK(map.chunk,bit) &= ~(1 << CHUNK_OFFSET(bit) )
#define NR_SYS_CHUNKS	BITMAP_CHUNKS(NR_SYS_PROCS)

#if (CHIP == INTEL)

/* Program stack words and masks. */
#define INIT_PSW      0x0200	/* initial psw */
#define INIT_TASK_PSW 0x1200	/* initial psw for tasks (with IOPL 1) */
#define TRACEBIT      0x0100	/* OR this with psw in proc[] for tracing */
#define SETPSW(rp, new)		/* permits only certain bits to be set */ \
	((rp)->p_reg.psw = (rp)->p_reg.psw & ~0xCD5 | (new) & 0xCD5)
#define IF_MASK 0x00000200
#define IOPL_MASK 0x003000

#if DEBUG_LOCK_CHECK
#define reallock(c, v)	{ if (!(read_cpu_flags() & X86_FLAG_I)) { kinfo.relocking++; } else { intr_disable(); } }
#else
#define reallock(c, v)	intr_disable()
#endif

#define realunlock(c)	intr_enable()

/* Disable/ enable hardware interrupts. The parameters of lock() and unlock()
 * are used when debugging is enabled. See debug.h for more information.
 */
#ifdef MHYPER
#define TICKS_TO_SWITCH		8

#define lock(c, v)	do{\
            lock_flag = 1; \
		if (read_cpu_flags() & X86_FLAG_I) intr_disable();\
		else lock_flag = 0;\
	}while(0);
#define unlock(c)	do{\
		if( (lock_flag != 0) && !(read_cpu_flags() & X86_FLAG_I) ) intr_enable();\
            lock_flag = 0; \
	}while(0);
#else /*  MHYPER */
#define lock(c, v)	reallock(c, v)
#define unlock(c)	realunlock(c) 
#endif /*  MHYPER */


/* Sizes of memory tables. The boot monitor distinguishes three memory areas, 
 * namely low mem below 1M, 1M-16M, and mem after 16M. More chunks are needed
 * for DOS MINIX.
 */
#define NR_MEMS            8	

#endif /* (CHIP == INTEL) */

#if (CHIP == M68000)
/* M68000 specific constants go here. */
#endif /* (CHIP == M68000) */

#ifdef NOT_USED
#define lock(c, v)	do{\
		lock_lvl++;\
		if( k_reenter >= 0){\
			if (read_cpu_flags() & X86_FLAG_I){\
				MHDEBUG("%s:%u: lock k_reenter=%d lock_lvl=%d\n",__FILE__,__LINE__, k_reenter, lock_lvl);\
				intr_disable();\
			}\
		}else{\
			intr_disable();\
		}\
	}while(0);

#define unlock(c)	do{\
		lock_lvl--;\
		if( k_reenter >= 0) {\
			if ( !(read_cpu_flags() & X86_FLAG_I) ){\
				MHDEBUG("%s:%u: unlock k_reenter=%d lock_lvl=%d\n",__FILE__,__LINE__, k_reenter, lock_lvl);\
				intr_enable();\
			}\
		}else{\
			intr_enable();\
		}\
	}while(0);
#endif /*  NOT_USED */

#endif /* CONST_H */
